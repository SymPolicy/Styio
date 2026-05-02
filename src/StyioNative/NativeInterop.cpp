#include "NativeInterop.hpp"

#include <algorithm>
#include <array>
#include <cerrno>
#include <chrono>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <system_error>
#include <unistd.h>
#include <mutex>
#include <unordered_map>
#include <unordered_set>

#include "StyioException/Exception.hpp"
#include "StyioNative/NativeToolchainConfig.hpp"

namespace styio::native {
namespace {

constexpr const char* kCompileFlags = "-shared -fPIC -O2";
constexpr const char* kNativeCacheVersion = "styio-native-cache-v1";

struct CachedModule {
  void* handle = nullptr;
  std::filesystem::path path;
};

class NativeModuleCache
{
public:
  std::mutex mutex;
  std::unordered_map<std::string, CachedModule> modules;

  ~NativeModuleCache() {
    for (auto& entry : modules) {
      if (entry.second.handle != nullptr) {
        ::dlclose(entry.second.handle);
      }
    }
  }
};

NativeModuleCache&
native_module_cache() {
  static NativeModuleCache cache;
  return cache;
}

std::string
trim_copy(const std::string& text) {
  size_t begin = 0;
  while (begin < text.size() && std::isspace(static_cast<unsigned char>(text[begin]))) {
    ++begin;
  }
  size_t end = text.size();
  while (end > begin && std::isspace(static_cast<unsigned char>(text[end - 1]))) {
    --end;
  }
  return text.substr(begin, end - begin);
}

std::string
lower_copy(std::string text) {
  std::transform(
    text.begin(),
    text.end(),
    text.begin(),
    [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
  return text;
}

std::string
collapse_ws_copy(const std::string& text) {
  std::string out;
  out.reserve(text.size());
  bool last_space = false;
  for (char ch : text) {
    if (std::isspace(static_cast<unsigned char>(ch))) {
      if (!last_space) {
        out.push_back(' ');
      }
      last_space = true;
      continue;
    }
    out.push_back(ch);
    last_space = false;
  }
  return trim_copy(out);
}

bool
is_ident_start(char ch) {
  return std::isalpha(static_cast<unsigned char>(ch)) || ch == '_';
}

bool
is_ident_continue(char ch) {
  return std::isalnum(static_cast<unsigned char>(ch)) || ch == '_';
}

std::string
shell_quote(const std::string& value) {
  std::string out = "'";
  for (char ch : value) {
    if (ch == '\'') {
      out += "'\\''";
    }
    else {
      out.push_back(ch);
    }
  }
  out.push_back('\'');
  return out;
}

bool
native_cache_enabled() {
  if (const char* env = std::getenv("STYIO_NATIVE_CACHE")) {
    const std::string mode = lower_copy(trim_copy(env));
    return !(mode == "0" || mode == "false" || mode == "off" || mode == "no");
  }
  return true;
}

std::filesystem::path
native_cache_dir() {
  if (!native_cache_enabled()) {
    return {};
  }
  if (const char* env = std::getenv("STYIO_NATIVE_CACHE_DIR")) {
    if (std::string raw = trim_copy(env); !raw.empty()) {
      return std::filesystem::path(raw) / "v1";
    }
  }
  if (const char* xdg = std::getenv("XDG_CACHE_HOME")) {
    if (std::string raw = trim_copy(xdg); !raw.empty()) {
      return std::filesystem::path(raw) / "styio" / "native" / "v1";
    }
  }
  if (const char* home = std::getenv("HOME")) {
    if (std::string raw = trim_copy(home); !raw.empty()) {
      return std::filesystem::path(raw) / ".cache" / "styio" / "native" / "v1";
    }
  }
  return {};
}

std::string
stable_hash_hex(const std::string& text) {
  uint64_t hash = 1469598103934665603ULL;
  for (unsigned char ch : text) {
    hash ^= static_cast<uint64_t>(ch);
    hash *= 1099511628211ULL;
  }
  std::ostringstream out;
  out << std::hex << std::setw(16) << std::setfill('0') << hash;
  return out.str();
}

std::string
native_cache_key(
  const std::string& normalized_abi,
  const CompilerResolution& compiler,
  const std::string& source_text
) {
  std::string input;
  input.reserve(
    source_text.size()
    + normalized_abi.size()
    + compiler.command.size()
    + compiler.source.size()
    + 96);
  input += kNativeCacheVersion;
  input.push_back('\0');
  input += normalized_abi;
  input.push_back('\0');
  input += compiler.command;
  input.push_back('\0');
  input += compiler.source;
  input.push_back('\0');
  input += kCompileFlags;
  input.push_back('\0');
  input += source_text;
  return normalized_abi + "-" + stable_hash_hex(input);
}

bool
ensure_directory(const std::filesystem::path& dir, std::string& error_message) {
  if (dir.empty()) {
    return false;
  }
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  if (ec) {
    error_message = "cannot create native cache directory: " + dir.string() + ": " + ec.message();
    return false;
  }
  return true;
}

std::filesystem::path
native_cache_path_for_key(const std::string& key, std::string& error_message) {
  const std::filesystem::path dir = native_cache_dir();
  if (dir.empty()) {
    return {};
  }
  if (!ensure_directory(dir, error_message)) {
    return {};
  }
  return dir / ("lib" + key + ".so");
}

std::filesystem::path
native_cache_tmp_path_for_key(const std::filesystem::path& cache_path) {
  const auto now = std::chrono::steady_clock::now().time_since_epoch().count();
  return cache_path.parent_path()
    / (cache_path.filename().string()
       + "."
       + std::to_string(static_cast<long long>(::getpid()))
       + "."
       + std::to_string(static_cast<long long>(now))
       + ".tmp");
}

std::string
native_compile_command(
  const CompilerResolution& compiler,
  const std::filesystem::path& source_path,
  const std::filesystem::path& shared_path,
  const std::filesystem::path& log_path
) {
  return shell_quote(compiler.command)
    + " "
    + kCompileFlags
    + " "
    + shell_quote(source_path.string())
    + " -o "
    + shell_quote(shared_path.string())
    + " 2>"
    + shell_quote(log_path.string());
}

bool
is_exported_name(
  const std::string& name,
  const std::unordered_set<std::string>& exports
) {
  return exports.empty() || exports.find(name) != exports.end();
}

std::string
strip_comments_for_signatures(const std::string& body) {
  std::string out;
  out.reserve(body.size());
  enum class Mode { Normal, LineComment, BlockComment, StringLit, CharLit };
  Mode mode = Mode::Normal;
  bool escaped = false;
  for (size_t i = 0; i < body.size(); ++i) {
    const char ch = body[i];
    const char next = i + 1 < body.size() ? body[i + 1] : '\0';
    switch (mode) {
      case Mode::Normal:
        if (ch == '/' && next == '/') {
          mode = Mode::LineComment;
          out.push_back(' ');
          ++i;
        }
        else if (ch == '/' && next == '*') {
          mode = Mode::BlockComment;
          out.push_back(' ');
          ++i;
        }
        else if (ch == '"') {
          mode = Mode::StringLit;
          out.push_back(ch);
        }
        else if (ch == '\'') {
          mode = Mode::CharLit;
          out.push_back(ch);
        }
        else {
          out.push_back(ch);
        }
        break;
      case Mode::LineComment:
        if (ch == '\n') {
          mode = Mode::Normal;
          out.push_back('\n');
        }
        break;
      case Mode::BlockComment:
        if (ch == '*' && next == '/') {
          mode = Mode::Normal;
          out.push_back(' ');
          ++i;
        }
        else if (ch == '\n') {
          out.push_back('\n');
        }
        break;
      case Mode::StringLit:
        out.push_back(ch);
        if (escaped) {
          escaped = false;
        }
        else if (ch == '\\') {
          escaped = true;
        }
        else if (ch == '"') {
          mode = Mode::Normal;
        }
        break;
      case Mode::CharLit:
        out.push_back(ch);
        if (escaped) {
          escaped = false;
        }
        else if (ch == '\\') {
          escaped = true;
        }
        else if (ch == '\'') {
          mode = Mode::Normal;
        }
        break;
    }
  }
  return out;
}

std::string
top_level_signature_text(const std::string& body) {
  std::string out;
  out.reserve(body.size());
  enum class Mode { Normal, StringLit, CharLit };
  Mode mode = Mode::Normal;
  bool escaped = false;
  int brace_depth = 0;

  auto push_visible = [&](char ch) {
    out.push_back(brace_depth == 0 || ch == '\n' || ch == '\r' ? ch : ' ');
  };

  for (size_t i = 0; i < body.size(); ++i) {
    const char ch = body[i];
    switch (mode) {
      case Mode::Normal:
        if (ch == '"') {
          push_visible(ch);
          mode = Mode::StringLit;
          escaped = false;
        }
        else if (ch == '\'') {
          push_visible(ch);
          mode = Mode::CharLit;
          escaped = false;
        }
        else if (ch == '{') {
          out.push_back(brace_depth == 0 ? ch : ' ');
          brace_depth += 1;
        }
        else if (ch == '}') {
          if (brace_depth > 0) {
            brace_depth -= 1;
          }
          out.push_back(brace_depth == 0 ? ch : ' ');
        }
        else {
          push_visible(ch);
        }
        break;
      case Mode::StringLit:
        push_visible(ch);
        if (escaped) {
          escaped = false;
        }
        else if (ch == '\\') {
          escaped = true;
        }
        else if (ch == '"') {
          mode = Mode::Normal;
        }
        break;
      case Mode::CharLit:
        push_visible(ch);
        if (escaped) {
          escaped = false;
        }
        else if (ch == '\\') {
          escaped = true;
        }
        else if (ch == '\'') {
          mode = Mode::Normal;
        }
        break;
    }
  }
  return out;
}

bool
parse_c_type(const std::string& raw, CType& out) {
  std::string t = collapse_ws_copy(raw);
  if (t.empty()) {
    return false;
  }

  for (const std::string& marker : {"extern \"C\"", "static", "inline", "__attribute__"}) {
    if (t.rfind(marker, 0) == 0) {
      t = trim_copy(t.substr(marker.size()));
    }
  }

  const bool pointer = t.find('*') != std::string::npos;
  std::string normalized;
  normalized.reserve(t.size());
  for (char ch : t) {
    if (ch == '*' || ch == '&') {
      normalized.push_back(' ');
    }
    else {
      normalized.push_back(ch);
    }
  }
  normalized = lower_copy(collapse_ws_copy(normalized));

  for (const std::string& q : {"const ", "volatile ", "restrict ", "signed "}) {
    while (normalized.rfind(q, 0) == 0) {
      normalized = trim_copy(normalized.substr(q.size()));
    }
  }

  bool is_unsigned = false;
  if (normalized.rfind("unsigned ", 0) == 0) {
    is_unsigned = true;
    normalized = trim_copy(normalized.substr(std::string("unsigned ").size()));
  }

  out = CType{};
  out.spelling = t;
  out.is_unsigned = is_unsigned;

  if (pointer) {
    out.kind = CTypeKind::Pointer;
    return true;
  }
  if (normalized == "void") {
    out.kind = CTypeKind::Void;
    return true;
  }
  if (normalized == "bool" || normalized == "_bool") {
    out.kind = CTypeKind::Bool;
    return true;
  }
  if (normalized == "char" || normalized == "int8_t" || normalized == "uint8_t") {
    out.kind = CTypeKind::I8;
    return true;
  }
  if (normalized == "short" || normalized == "short int" || normalized == "int16_t" || normalized == "uint16_t") {
    out.kind = CTypeKind::I16;
    return true;
  }
  if (normalized == "int" || normalized == "int32_t" || normalized == "uint32_t") {
    out.kind = CTypeKind::I32;
    return true;
  }
  if (normalized == "long" || normalized == "long int" || normalized == "long long"
      || normalized == "long long int" || normalized == "int64_t" || normalized == "uint64_t"
      || normalized == "size_t" || normalized == "ssize_t") {
    out.kind = CTypeKind::I64;
    return true;
  }
  if (normalized == "float") {
    out.kind = CTypeKind::F32;
    return true;
  }
  if (normalized == "double") {
    out.kind = CTypeKind::F64;
    return true;
  }
  return false;
}

FunctionParam
parse_param(std::string raw, size_t index) {
  raw = trim_copy(raw);
  if (raw.empty()) {
    throw StyioTypeError("empty native parameter in @extern block");
  }
  if (raw == "...") {
    throw StyioTypeError("variadic native functions are not supported in @extern blocks");
  }

  std::string type_text = raw;
  std::string name;
  size_t end = raw.size();
  while (end > 0 && std::isspace(static_cast<unsigned char>(raw[end - 1]))) {
    --end;
  }
  size_t name_begin = end;
  while (name_begin > 0 && is_ident_continue(raw[name_begin - 1])) {
    --name_begin;
  }
  if (name_begin < end && is_ident_start(raw[name_begin])) {
    const std::string maybe_type = trim_copy(raw.substr(0, name_begin));
    if (!maybe_type.empty()) {
      type_text = maybe_type;
      name = raw.substr(name_begin, end - name_begin);
    }
  }
  if (type_text.empty()) {
    throw StyioTypeError("cannot parse native parameter `" + raw + "`");
  }
  if (type_text.find('*') != std::string::npos && name.empty()) {
    name = "arg" + std::to_string(index);
  }
  if (name.empty()) {
    const size_t last_space = type_text.find_last_of(" \t\r\n");
    if (last_space != std::string::npos) {
      name = trim_copy(type_text.substr(last_space + 1));
      type_text = trim_copy(type_text.substr(0, last_space));
    }
  }
  if (name.empty()) {
    name = "arg" + std::to_string(index);
  }

  CType ctype;
  if (!parse_c_type(type_text, ctype)) {
    throw StyioTypeError("unsupported native parameter type `" + type_text + "`");
  }
  if (ctype.kind == CTypeKind::Void) {
    throw StyioTypeError("native parameter `" + name + "` cannot have type void");
  }
  return FunctionParam{name, ctype};
}

std::vector<FunctionParam>
parse_params(const std::string& raw_params);

bool
find_matching_open_paren(const std::string& text, size_t close, size_t& open) {
  int depth = 0;
  for (size_t i = close + 1; i > 0; --i) {
    const size_t pos = i - 1;
    const char ch = text[pos];
    if (ch == ')') {
      ++depth;
    }
    else if (ch == '(') {
      --depth;
      if (depth == 0) {
        open = pos;
        return true;
      }
    }
  }
  return false;
}

bool
parse_function_signature_candidate(std::string raw, FunctionSignature& out) {
  raw = trim_copy(raw);
  if (raw.empty()) {
    return false;
  }

  const size_t close = raw.rfind(')');
  if (close == std::string::npos) {
    return false;
  }
  for (size_t i = close + 1; i < raw.size(); ++i) {
    if (!std::isspace(static_cast<unsigned char>(raw[i]))) {
      return false;
    }
  }

  size_t open = std::string::npos;
  if (!find_matching_open_paren(raw, close, open)) {
    return false;
  }

  size_t name_end = open;
  while (name_end > 0 && std::isspace(static_cast<unsigned char>(raw[name_end - 1]))) {
    --name_end;
  }
  size_t name_begin = name_end;
  while (name_begin > 0 && is_ident_continue(raw[name_begin - 1])) {
    --name_begin;
  }
  if (name_begin == name_end || !is_ident_start(raw[name_begin])) {
    return false;
  }

  const size_t prior_boundary = raw.find_last_of(";\n\r}", name_begin == 0 ? 0 : name_begin - 1);
  if (prior_boundary != std::string::npos) {
    raw = trim_copy(raw.substr(prior_boundary + 1));
    return parse_function_signature_candidate(std::move(raw), out);
  }

  const std::string raw_ret = trim_copy(raw.substr(0, name_begin));
  const std::string name = raw.substr(name_begin, name_end - name_begin);
  const std::string raw_params = raw.substr(open + 1, close - open - 1);
  const std::string lowered_name = lower_copy(name);
  if (lowered_name == "if" || lowered_name == "for" || lowered_name == "while" || lowered_name == "switch") {
    return false;
  }

  CType ret;
  if (!parse_c_type(raw_ret, ret)) {
    throw StyioTypeError("unsupported native return type `" + raw_ret + "` for `" + name + "`");
  }

  out = FunctionSignature{};
  out.name = name;
  out.return_type = ret;
  out.params = parse_params(raw_params);
  return true;
}

std::vector<FunctionParam>
parse_params(const std::string& raw_params) {
  const std::string trimmed = trim_copy(raw_params);
  if (trimmed.empty() || trimmed == "void") {
    return {};
  }

  std::vector<FunctionParam> params;
  std::string current;
  int paren_depth = 0;
  for (char ch : trimmed) {
    if (ch == '(') {
      ++paren_depth;
    }
    else if (ch == ')' && paren_depth > 0) {
      --paren_depth;
    }

    if (ch == ',' && paren_depth == 0) {
      params.push_back(parse_param(current, params.size()));
      current.clear();
    }
    else {
      current.push_back(ch);
    }
  }
  if (!trim_copy(current).empty()) {
    params.push_back(parse_param(current, params.size()));
  }
  return params;
}

std::filesystem::path
make_native_temp_dir() {
  const std::filesystem::path base = std::filesystem::temp_directory_path();
  std::string tmpl = (base / "styio-native-XXXXXX").string();
  std::vector<char> buffer(tmpl.begin(), tmpl.end());
  buffer.push_back('\0');

  char* created = ::mkdtemp(buffer.data());
  if (created == nullptr) {
    throw StyioTypeError(
      "cannot create native @extern temporary directory: "
      + std::string(std::strerror(errno)));
  }
  return std::filesystem::path(created);
}

bool
write_text_file(const std::filesystem::path& path, const std::string& text, std::string& error_message) {
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  if (!out.is_open()) {
    error_message = "cannot write native source file: " + path.string();
    return false;
  }
  out << text;
  if (!out.good()) {
    error_message = "failed to write native source file: " + path.string();
    return false;
  }
  return true;
}

bool
read_text_file(const std::filesystem::path& path, std::string& out_text) {
  std::ifstream in(path, std::ios::binary);
  if (!in.is_open()) {
    return false;
  }
  std::ostringstream buf;
  buf << in.rdbuf();
  out_text = buf.str();
  return true;
}

bool
is_executable_file(const std::filesystem::path& path) {
  std::error_code ec;
  if (!std::filesystem::is_regular_file(path, ec) && !std::filesystem::is_symlink(path, ec)) {
    return false;
  }
  return ::access(path.c_str(), X_OK) == 0;
}

void
push_unique_path(std::vector<std::filesystem::path>& paths, std::filesystem::path path) {
  if (path.empty()) {
    return;
  }
  path = path.lexically_normal();
  if (std::find(paths.begin(), paths.end(), path) == paths.end()) {
    paths.push_back(std::move(path));
  }
}

std::filesystem::path
current_executable_dir() {
#if defined(__linux__)
  std::array<char, 4096> buf{};
  const ssize_t len = ::readlink("/proc/self/exe", buf.data(), buf.size() - 1);
  if (len > 0) {
    buf[static_cast<size_t>(len)] = '\0';
    return std::filesystem::path(buf.data()).parent_path();
  }
#endif
  return {};
}

std::vector<std::filesystem::path>
candidate_native_toolchain_roots() {
  std::vector<std::filesystem::path> roots;

  if (const char* env_root = std::getenv("STYIO_NATIVE_TOOLCHAIN_ROOT")) {
    push_unique_path(roots, env_root);
  }
  if (std::string configured_root = STYIO_NATIVE_TOOLCHAIN_ROOT; !configured_root.empty()) {
    push_unique_path(roots, configured_root);
  }

  const std::string relative_dir = STYIO_NATIVE_TOOLCHAIN_RELATIVE_DIR;
  const std::filesystem::path exe_dir = current_executable_dir();
  if (!relative_dir.empty() && !exe_dir.empty()) {
    push_unique_path(roots, exe_dir / relative_dir);
    push_unique_path(roots, exe_dir.parent_path() / relative_dir);
    push_unique_path(roots, exe_dir.parent_path() / "lib" / "styio" / relative_dir);
  }

  return roots;
}

std::string
find_bundled_compiler(const std::string& normalized_abi) {
  const auto names = normalized_abi == "c++"
    ? std::vector<std::string>{"clang++", "clang++-18"}
    : std::vector<std::string>{"clang", "clang-18"};

  for (const auto& root : candidate_native_toolchain_roots()) {
    for (const auto& dir : {root / "bin", root}) {
      for (const auto& name : names) {
        const std::filesystem::path candidate = dir / name;
        if (is_executable_file(candidate)) {
          return candidate.string();
        }
      }
    }
  }

  return {};
}

std::string
native_toolchain_mode_or_throw() {
  std::string mode = STYIO_NATIVE_TOOLCHAIN_MODE;
  if (const char* env_mode = std::getenv("STYIO_NATIVE_TOOLCHAIN_MODE")) {
    mode = env_mode;
  }
  mode = lower_copy(trim_copy(mode));
  if (mode == "auto" || mode == "bundled" || mode == "system") {
    return mode;
  }
  throw StyioTypeError(
    "invalid STYIO_NATIVE_TOOLCHAIN_MODE `" + mode + "`; expected auto, bundled, or system");
}

std::string
source_extension_for_abi(const std::string& normalized_abi) {
  return normalized_abi == "c++" ? ".cpp" : ".c";
}

std::string
source_preamble(const std::string& normalized_abi) {
  if (normalized_abi == "c++") {
    return "#include <cstdint>\n#include <cstddef>\n";
  }
  return "#include <stdint.h>\n#include <stddef.h>\n#include <stdbool.h>\n";
}

void*
dlopen_native_module(const std::filesystem::path& shared_path) {
  return ::dlopen(shared_path.c_str(), RTLD_NOW | RTLD_LOCAL);
}

std::vector<LoadedSymbol>
resolve_loaded_symbols(
  void* handle,
  const std::string& normalized_abi,
  const std::vector<FunctionSignature>& selected
) {
  std::vector<LoadedSymbol> symbols;
  symbols.reserve(selected.size());
  for (const auto& sig : selected) {
    ::dlerror();
    void* symbol = ::dlsym(handle, sig.name.c_str());
    const char* err = ::dlerror();
    if (err != nullptr || symbol == nullptr) {
      throw StyioTypeError(
        "native @extern(" + normalized_abi + ") could not resolve exported symbol `"
        + sig.name + "`; C++ blocks must expose callable symbols with extern \"C\"");
    }
    symbols.push_back(LoadedSymbol{sig.name, symbol});
  }
  return symbols;
}

CachedModule*
find_in_process_module(const std::string& cache_key) {
  auto& cache = native_module_cache();
  auto it = cache.modules.find(cache_key);
  if (it == cache.modules.end() || it->second.handle == nullptr) {
    return nullptr;
  }
  return &it->second;
}

LoadedBlock
loaded_block_from_cached_module(
  CachedModule& module,
  const std::string& normalized_abi,
  std::vector<FunctionSignature> selected
) {
  LoadedBlock loaded;
  loaded.handle = nullptr;
  loaded.functions = std::move(selected);
  loaded.symbols = resolve_loaded_symbols(module.handle, normalized_abi, loaded.functions);
  return loaded;
}

}  // namespace

std::string
normalize_abi(std::string abi) {
  abi = lower_copy(trim_copy(abi));
  if (abi == "c") {
    return "c";
  }
  if (abi == "c++") {
    return "c++";
  }
  throw StyioTypeError("unsupported @extern ABI `" + abi + "`");
}

std::string
configured_native_toolchain_mode() {
  return native_toolchain_mode_or_throw();
}

CompilerResolution
resolve_compiler_for_abi(const std::string& abi) {
  const std::string normalized_abi = normalize_abi(abi);
  if (normalized_abi == "c++") {
    if (const char* env = std::getenv("STYIO_NATIVE_CXX")) {
      return CompilerResolution{env, "env:STYIO_NATIVE_CXX"};
    }
  }
  else if (const char* env = std::getenv("STYIO_NATIVE_CC")) {
    return CompilerResolution{env, "env:STYIO_NATIVE_CC"};
  }

  const std::string mode = native_toolchain_mode_or_throw();
  if (mode != "system") {
    const std::string bundled = find_bundled_compiler(normalized_abi);
    if (!bundled.empty()) {
      return CompilerResolution{bundled, "bundled-clang"};
    }
    if (mode == "bundled") {
      throw StyioTypeError(
        "native @extern(" + normalized_abi
        + ") requires a bundled clang toolchain, but no compiler was found. "
          "Set STYIO_NATIVE_TOOLCHAIN_ROOT to a clang+LLVM root containing bin/clang and bin/clang++, "
          "or configure CMake with -DSTYIO_NATIVE_TOOLCHAIN_ROOT=/path/to/llvm.");
    }
  }

  return CompilerResolution{normalized_abi == "c++" ? "c++" : "cc", "system"};
}

std::vector<FunctionSignature>
parse_function_signatures(const std::string& body) {
  const std::string clean = top_level_signature_text(strip_comments_for_signatures(body));
  std::vector<FunctionSignature> functions;
  std::string candidate;
  candidate.reserve(clean.size());
  for (char ch : clean) {
    if (ch == ';' || ch == '{') {
      FunctionSignature sig;
      if (parse_function_signature_candidate(candidate, sig)) {
        functions.push_back(std::move(sig));
      }
      candidate.clear();
    }
    else if (ch == '}') {
      candidate.clear();
    }
    else {
      candidate.push_back(ch);
    }
  }
  return functions;
}

StyioDataType
styio_data_type_for_c_type(const CType& type) {
  switch (type.kind) {
    case CTypeKind::Void:
      return StyioDataType{StyioDataTypeOption::Undefined, "undefined", 0};
    case CTypeKind::Bool:
      return StyioDataType{StyioDataTypeOption::Bool, "bool", 1};
    case CTypeKind::F32:
    case CTypeKind::F64:
      return StyioDataType{StyioDataTypeOption::Float, "f64", 64};
    case CTypeKind::Pointer:
      return StyioDataType{StyioDataTypeOption::String, "string", 0};
    case CTypeKind::I8:
    case CTypeKind::I16:
    case CTypeKind::I32:
    case CTypeKind::I64:
      return StyioDataType{StyioDataTypeOption::Integer, "i64", 64};
  }
  return StyioDataType{StyioDataTypeOption::Undefined, "undefined", 0};
}

LoadedBlock
compile_and_load_block(
  const std::string& abi,
  const std::string& body,
  const std::vector<std::string>& export_symbols
) {
  const std::string normalized_abi = normalize_abi(abi);
  std::vector<FunctionSignature> parsed = parse_function_signatures(body);
  if (parsed.empty()) {
    throw StyioTypeError("@extern(" + normalized_abi + ") block does not declare any callable function");
  }

  std::unordered_set<std::string> exports(export_symbols.begin(), export_symbols.end());
  std::vector<FunctionSignature> selected;
  selected.reserve(parsed.size());
  for (const auto& sig : parsed) {
    if (is_exported_name(sig.name, exports)) {
      selected.push_back(sig);
    }
  }
  if (!exports.empty() && selected.empty()) {
    throw StyioTypeError("@export does not match any @extern(" + normalized_abi + ") function");
  }

  const CompilerResolution compiler = resolve_compiler_for_abi(normalized_abi);
  const std::string source_text = source_preamble(normalized_abi) + body + "\n";
  const std::string cache_key = native_cache_key(normalized_abi, compiler, source_text);

  auto& process_cache = native_module_cache();
  std::lock_guard<std::mutex> cache_lock(process_cache.mutex);
  if (CachedModule* cached = find_in_process_module(cache_key)) {
    return loaded_block_from_cached_module(*cached, normalized_abi, std::move(selected));
  }

  std::string cache_error;
  const std::filesystem::path cache_path = native_cache_path_for_key(cache_key, cache_error);
  if (!cache_path.empty() && std::filesystem::exists(cache_path)) {
    if (void* cached_handle = dlopen_native_module(cache_path)) {
      auto [it, inserted] = process_cache.modules.emplace(
        cache_key,
        CachedModule{cached_handle, cache_path});
      (void)inserted;
      return loaded_block_from_cached_module(it->second, normalized_abi, std::move(selected));
    }
    std::filesystem::remove(cache_path);
  }

  const std::filesystem::path tmp_dir = make_native_temp_dir();
  const std::filesystem::path source_path =
    tmp_dir / ("extern" + source_extension_for_abi(normalized_abi));
  const std::filesystem::path log_path = tmp_dir / "compile.log";
  const std::filesystem::path compile_shared_path =
    cache_path.empty()
      ? tmp_dir / "libstyio_native.so"
      : native_cache_tmp_path_for_key(cache_path);

  std::string write_error;
  if (!write_text_file(source_path, source_text, write_error)) {
    std::filesystem::remove_all(tmp_dir);
    throw StyioTypeError(write_error);
  }

  const std::string command = native_compile_command(compiler, source_path, compile_shared_path, log_path);

  const int rc = std::system(command.c_str());
  if (rc != 0) {
    std::string log;
    (void)read_text_file(log_path, log);
    std::filesystem::remove(compile_shared_path);
    std::filesystem::remove_all(tmp_dir);
    throw StyioTypeError(
      "native @extern(" + normalized_abi + ") compile failed with command `" + command + "`"
      + " using " + compiler.source
      + (log.empty() ? std::string() : "\n" + log));
  }

  std::filesystem::path load_path = compile_shared_path;
  if (!cache_path.empty()) {
    std::error_code ec;
    if (std::filesystem::exists(cache_path, ec)) {
      std::filesystem::remove(compile_shared_path, ec);
      load_path = cache_path;
    }
    else {
      std::filesystem::rename(compile_shared_path, cache_path, ec);
      if (!ec) {
        load_path = cache_path;
      }
      else {
        ec.clear();
        std::filesystem::copy_file(
          compile_shared_path,
          cache_path,
          std::filesystem::copy_options::overwrite_existing,
          ec);
        if (!ec) {
          std::filesystem::remove(compile_shared_path);
          load_path = cache_path;
        }
      }
    }
  }

  void* handle = dlopen_native_module(load_path);
  if (handle == nullptr) {
    const char* err = ::dlerror();
    if (cache_path.empty()) {
      std::filesystem::remove(compile_shared_path);
    }
    else {
      std::filesystem::remove(cache_path);
    }
    std::filesystem::remove_all(tmp_dir);
    throw StyioTypeError(
      "native @extern(" + normalized_abi + ") dlopen failed: "
      + std::string(err != nullptr ? err : "unknown dlopen error"));
  }

  auto [it, inserted] = process_cache.modules.emplace(
    cache_key,
    CachedModule{handle, load_path});
  if (!inserted && it->second.handle != handle) {
    ::dlclose(handle);
  }

  std::filesystem::remove(source_path);
  std::filesystem::remove(log_path);
  if (cache_path.empty()) {
    std::filesystem::remove(compile_shared_path);
  }
  std::filesystem::remove(tmp_dir);
  return loaded_block_from_cached_module(it->second, normalized_abi, std::move(selected));
}

void
close_loaded_block(void* handle) {
  if (handle != nullptr) {
    ::dlclose(handle);
  }
}

}  // namespace styio::native
