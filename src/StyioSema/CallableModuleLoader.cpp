#include "CallableModuleLoader.hpp"

#include <algorithm>
#include <fstream>
#include <limits>
#include <sstream>

#include "../StyioAST/AST.hpp"
#include "../StyioException/Exception.hpp"
#include "../StyioParser/Tokenizer.hpp"

namespace styio::sema {
namespace {

constexpr std::uintmax_t kMaximumModuleSourceBytes =
  UINT64_C(64) * 1024 * 1024;
constexpr std::uintmax_t kMaximumModuleInterfaceBytes =
  UINT64_C(64) * 1024 * 1024;

std::string
read_bounded_file(
  const std::filesystem::path& path,
  std::uintmax_t maximum_bytes,
  std::string_view kind
) {
  std::error_code error;
  const std::uintmax_t size = std::filesystem::file_size(path, error);
  if (error) {
    throw StyioTypeError(
      "cannot read " + std::string(kind) + " `" + path.string()
      + "`: " + error.message());
  }
  if (size > maximum_bytes) {
    throw StyioTypeError(
      std::string(kind) + " exceeds the supported "
      + std::to_string(maximum_bytes) + "-byte limit");
  }

  std::ifstream input(path, std::ios::binary);
  if (!input.is_open()) {
    throw StyioTypeError(
      "cannot open " + std::string(kind) + " `" + path.string() + "`");
  }
  std::ostringstream output;
  output << input.rdbuf();
  if (!input.good() && !input.eof()) {
    throw StyioTypeError(
      "failed while reading " + std::string(kind) + " `"
      + path.string() + "`");
  }
  return output.str();
}

std::vector<std::pair<std::size_t, std::size_t>>
line_separations(std::string_view source) {
  std::vector<std::pair<std::size_t, std::size_t>> lines;
  std::size_t line_start = 0;
  for (std::size_t i = 0; i < source.size(); ++i) {
    if (source[i] != '\n') {
      continue;
    }
    lines.emplace_back(line_start, i - line_start);
    line_start = i + 1;
  }
  if (line_start < source.size() || source.empty()) {
    lines.emplace_back(line_start, source.size() - line_start);
  }
  return lines;
}

std::vector<std::string>
module_imports(MainBlockAST* ast) {
  std::vector<std::string> imports;
  if (ast == nullptr) {
    return imports;
  }
  for (auto* statement : ast->getStmts()) {
    auto* declaration = dynamic_cast<ExtPackAST*>(statement);
    if (declaration == nullptr) {
      continue;
    }
    imports.insert(
      imports.end(),
      declaration->getPaths().begin(),
      declaration->getPaths().end());
  }
  return imports;
}

MainBlockAST*
parse_module(
  const std::filesystem::path& path,
  const std::string& source,
  StyioParserEngine parser_engine,
  bool debug_mode,
  styio::session::SymbolInterner& symbols
) {
  std::vector<StyioToken*> tokens;
  StyioContext* context = nullptr;
  MainBlockAST* ast = nullptr;
  try {
    tokens = StyioTokenizer::tokenize(source);
    context = StyioContext::Create(
      path.string(),
      source,
      line_separations(source),
      tokens,
      debug_mode);
    context->set_symbol_interner(symbols);
    StyioParserRouteStats route_stats;
    ast = parse_main_block_with_engine_latest(
      *context,
      parser_engine,
      parser_engine == StyioParserEngine::Nightly
        ? &route_stats
        : nullptr);
  }
  catch (...) {
    delete ast;
    delete context;
    for (auto* token : tokens) {
      delete token;
    }
    throw;
  }
  delete context;
  for (auto* token : tokens) {
    delete token;
  }
  return ast;
}

std::string
normalized_import_path(std::string path) {
  if (path.find('/') == std::string::npos
      && path.find('.') != std::string::npos) {
    std::replace(path.begin(), path.end(), '.', '/');
  }
  return path;
}

void
validate_import_path(const std::string& path) {
  if (path.empty()
      || path.front() == '/'
      || path.back() == '/'
      || path.find('\\') != std::string::npos) {
    throw StyioTypeError("invalid callable module import path `" + path + "`");
  }
  std::size_t begin = 0;
  while (begin <= path.size()) {
    const std::size_t end = path.find('/', begin);
    const std::string_view segment(
      path.data() + begin,
      (end == std::string::npos ? path.size() : end) - begin);
    if (segment.empty() || segment == "." || segment == "..") {
      throw StyioTypeError(
        "callable module import path may not contain empty, `.` or `..` segments"
      );
    }
    if (end == std::string::npos) {
      break;
    }
    begin = end + 1;
  }
}

std::unordered_map<std::string, StyioAST*>
callable_definitions(MainBlockAST* ast) {
  std::unordered_map<std::string, StyioAST*> definitions;
  for (auto* statement : ast->getStmts()) {
    std::string name;
    if (auto* function = dynamic_cast<FunctionAST*>(statement)) {
      name = function->getNameAsStr();
    }
    else if (auto* function =
               dynamic_cast<SimpleFuncAST*>(statement)) {
      name = function->func_name->getAsStr();
    }
    if (name.empty()) {
      continue;
    }
    if (!definitions.emplace(name, statement).second) {
      throw StyioTypeError(
        "module source contains duplicate callable `" + name + "`"
      );
    }
  }
  return definitions;
}

std::unordered_set<std::string>
exported_symbols(MainBlockAST* ast) {
  std::unordered_set<std::string> symbols;
  for (auto* statement : ast->getStmts()) {
    if (auto* declaration =
          dynamic_cast<ExportDeclAST*>(statement)) {
      symbols.insert(
        declaration->getSymbols().begin(),
        declaration->getSymbols().end());
    }
  }
  return symbols;
}

}  // namespace

void
CallableModuleGraph::load_entry_imports(
  const std::filesystem::path& entry_source_path,
  MainBlockAST* entry_ast,
  StyioParserEngine parser_engine,
  bool debug_mode,
  std::string compiler_abi,
  styio::session::SymbolInterner& symbols
) {
  modules_.clear();
  modules_by_path_.clear();
  load_states_.clear();
  entry_dependencies_.clear();
  parser_engine_ = parser_engine;
  debug_mode_ = debug_mode;
  compiler_abi_ = std::move(compiler_abi);
  symbols_ = &symbols;

  std::unordered_set<std::string> direct_modules;
  for (const auto& raw_import : module_imports(entry_ast)) {
    const std::string import_path =
      normalized_import_path(raw_import);
    if (!direct_modules.insert(import_path).second) {
      throw StyioTypeError(
        "duplicate callable module import `" + import_path + "`"
      );
    }
    LoadedCallableModule* module = load_module(
      entry_source_path.parent_path(),
      import_path,
      "");
    entry_dependencies_.push_back(&module->interface);
  }
  std::sort(
    entry_dependencies_.begin(),
    entry_dependencies_.end(),
    [](const auto* lhs, const auto* rhs)
    {
      return lhs->module_id < rhs->module_id;
    });
}

std::filesystem::path
CallableModuleGraph::resolve_source_path(
  const std::filesystem::path& importer_directory,
  const std::string& raw_import_path
) const {
  const std::string import_path =
    normalized_import_path(raw_import_path);
  validate_import_path(import_path);
  std::filesystem::path candidate =
    importer_directory / std::filesystem::path(import_path);
  if (candidate.extension() != ".styio") {
    candidate += ".styio";
  }
  std::error_code error;
  if (!std::filesystem::is_regular_file(candidate, error) || error) {
    throw StyioTypeError(
      "cannot resolve imported callable module `" + import_path
      + "` relative to its importing source");
  }
  const auto canonical =
    std::filesystem::weakly_canonical(candidate, error);
  if (error) {
    throw StyioTypeError(
      "cannot canonicalize imported callable module `" + import_path
      + "`: " + error.message());
  }
  return canonical;
}

LoadedCallableModule*
CallableModuleGraph::load_module(
  const std::filesystem::path& importer_directory,
  const std::string& raw_import_path,
  const std::string& importer_module
) {
  if (symbols_ == nullptr) {
    throw StyioTypeError("callable module graph has no symbol interner");
  }
  const std::string import_path =
    normalized_import_path(raw_import_path);
  const std::filesystem::path source_path =
    resolve_source_path(importer_directory, import_path);
  const std::string path_key = source_path.generic_string();

  auto state = load_states_.find(path_key);
  if (state != load_states_.end()
      && state->second == LoadState::Visiting) {
    throw StyioTypeError(
      "cross-module recursive dependency cycle is not supported; cycle reaches `"
      + import_path + "`"
    );
  }
  auto existing = modules_by_path_.find(path_key);
  if (existing != modules_by_path_.end()) {
    if (existing->second->module_id != import_path) {
      throw StyioTypeError(
        "one callable module source was imported under both `"
        + existing->second->module_id + "` and `" + import_path + "`"
      );
    }
    existing->second->direct_importers.insert(importer_module);
    return existing->second;
  }

  auto module = std::make_unique<LoadedCallableModule>();
  module->module_id = import_path;
  module->source_path = source_path;
  module->direct_importers.insert(importer_module);
  LoadedCallableModule* result = module.get();
  modules_by_path_[path_key] = result;
  modules_.push_back(std::move(module));
  load_states_[path_key] = LoadState::Visiting;

  result->source_text = read_bounded_file(
    source_path,
    kMaximumModuleSourceBytes,
    "callable module source");
  result->ast.reset(parse_module(
    source_path,
    result->source_text,
    parser_engine_,
    debug_mode_,
    *symbols_));
  result->definitions = callable_definitions(result->ast.get());

  std::vector<const CallableModuleInterface*> dependencies;
  std::unordered_set<std::string> direct_modules;
  for (const auto& raw_dependency : module_imports(result->ast.get())) {
    const std::string dependency_path =
      normalized_import_path(raw_dependency);
    if (!direct_modules.insert(dependency_path).second) {
      throw StyioTypeError(
        "duplicate callable module import `" + dependency_path
        + "` in module `" + import_path + "`"
      );
    }
    LoadedCallableModule* dependency = load_module(
      source_path.parent_path(),
      dependency_path,
      import_path);
    dependencies.push_back(&dependency->interface);
  }
  std::sort(
    dependencies.begin(),
    dependencies.end(),
    [](const auto* lhs, const auto* rhs)
    {
      return lhs->module_id < rhs->module_id;
    });

  std::filesystem::path interface_path = source_path;
  interface_path.replace_extension(".styioi");
  std::error_code error;
  if (!std::filesystem::is_regular_file(interface_path, error) || error) {
    throw StyioTypeError(
      "missing callable module interface for `" + import_path
      + "`; compile the module with --module-id="
      + import_path
      + " --emit-module-interface="
      + interface_path.filename().string());
  }
  const std::string payload = read_bounded_file(
    interface_path,
    kMaximumModuleInterfaceBytes,
    "callable module interface");
  result->interface = parse_callable_module_interface(
    payload,
    import_path,
    result->source_text,
    compiler_abi_,
    dependencies);

  const auto exports = exported_symbols(result->ast.get());
  for (const auto& entry : result->interface.entries) {
    if (result->definitions.count(entry.name) == 0) {
      throw StyioTypeError(
        "callable module interface `" + import_path
        + "` references missing body `" + entry.name + "`"
      );
    }
    const bool source_exports = exports.count(entry.name) != 0;
    if (entry.exported != source_exports) {
      throw StyioTypeError(
        "callable module interface export state drifted for `"
        + entry.name + "`; rebuild its .styioi interface"
      );
    }
  }

  load_states_[path_key] = LoadState::Complete;
  return result;
}

void
CallableModuleGraph::install_into(
  StyioSemaContext& context
) const {
  std::vector<const LoadedCallableModule*> ordered;
  ordered.reserve(modules_.size());
  for (const auto& module : modules_) {
    ordered.push_back(module.get());
  }
  std::sort(
    ordered.begin(),
    ordered.end(),
    [](const auto* lhs, const auto* rhs)
    {
      return lhs->module_id < rhs->module_id;
    });

  for (const auto* module : ordered) {
    std::vector<const CallableInterfaceEntry*> entries;
    entries.reserve(module->interface.entries.size());
    for (const auto& entry : module->interface.entries) {
      entries.push_back(&entry);
    }
    std::sort(
      entries.begin(),
      entries.end(),
      [](const auto* lhs, const auto* rhs)
      {
        return lhs->name < rhs->name;
      });
    for (const auto* entry : entries) {
      std::vector<std::string> visible_from;
      if (entry->exported) {
        visible_from.assign(
          module->direct_importers.begin(),
          module->direct_importers.end());
        std::sort(visible_from.begin(), visible_from.end());
      }
      context.install_imported_callable_definition(
        module->module_id,
        entry->exported,
        entry->has_scheme,
        module->definitions.at(entry->name),
        entry->scheme,
        entry->effects,
        entry->concrete_params,
        entry->concrete_result,
        std::move(visible_from),
        entry->checked_body_digest,
        module->interface.abi_digest);
    }
  }
}

}  // namespace styio::sema
