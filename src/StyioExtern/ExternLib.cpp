#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <cctype>
#include <filesystem>
#include <string>
#include <unordered_set>

#include "ExternLib.hpp"
#include "StyioRuntime/HandleTable.hpp"

namespace {

/* Alternating buffers so two consecutive reads (e.g. zip of two files) keep both lines valid. */
thread_local char g_read_line_bufs[2][65536];
thread_local int g_read_line_buf_which = 0;
thread_local StyioHandleTable g_handle_table;
thread_local std::unordered_set<const void*> g_owned_cstr_ptrs;
thread_local bool g_runtime_error = false;
thread_local std::string g_runtime_error_message;
thread_local std::string g_runtime_error_subcode;

constexpr const char* kRuntimeSubcodeInvalidFileHandle = "STYIO_RUNTIME_INVALID_FILE_HANDLE";
constexpr const char* kRuntimeSubcodeFilePathNull = "STYIO_RUNTIME_FILE_PATH_NULL";
constexpr const char* kRuntimeSubcodeFileOpenRead = "STYIO_RUNTIME_FILE_OPEN_READ";
constexpr const char* kRuntimeSubcodeFileOpenWrite = "STYIO_RUNTIME_FILE_OPEN_WRITE";

thread_local struct HandleTableCleanup {
  ~HandleTableCleanup() {
    g_handle_table.release_all(
      StyioHandleTable::HandleKind::File,
      [](void* raw) { std::fclose(static_cast<FILE*>(raw)); });
  }
} g_handle_table_cleanup;

std::string
resolve_read_path(const char* path) {
  if (path == nullptr) {
    return {};
  }

  std::string p(path);
  std::error_code ec;
  if (std::filesystem::exists(p, ec)) {
    return p;
  }

  constexpr const char* kTestsPrefix = "tests/";
  constexpr const char* kMilestonesPrefix = "tests/milestones/";
  if (p.rfind(kTestsPrefix, 0) != 0 || p.rfind(kMilestonesPrefix, 0) == 0) {
    return p;
  }

  std::string tail = p.substr(std::strlen(kTestsPrefix)); // m5/data/...
  const size_t slash = tail.find('/');
  if (slash == std::string::npos) {
    return p;
  }

  const std::string top = tail.substr(0, slash); // m5
  if (top.size() < 2 || top[0] != 'm') {
    return p;
  }
  for (size_t i = 1; i < top.size(); ++i) {
    if (!std::isdigit(static_cast<unsigned char>(top[i]))) {
      return p;
    }
  }

  std::string candidate = std::string(kMilestonesPrefix) + tail;
  if (std::filesystem::exists(candidate, ec)) {
    return candidate;
  }
  return p;
}

void
set_runtime_error_once(const char* subcode, const std::string& message) {
  g_runtime_error = true;
  if (g_runtime_error_message.empty()) {
    g_runtime_error_message = message;
    g_runtime_error_subcode = (subcode != nullptr) ? subcode : "";
  }
}

int64_t
stash_file(FILE* f) {
  return g_handle_table.acquire(StyioHandleTable::HandleKind::File, f);
}

FILE*
as_file(int64_t h, bool diagnose_if_missing = false) {
  if (h == 0) {
    return nullptr;
  }
  FILE* f = g_handle_table.lookup_as<FILE>(h, StyioHandleTable::HandleKind::File);
  if (f == nullptr && diagnose_if_missing) {
    set_runtime_error_once(
      kRuntimeSubcodeInvalidFileHandle,
      "invalid file handle: " + std::to_string(static_cast<long long>(h)));
  }
  return f;
}

void
close_file(void* raw) {
  if (raw == nullptr) {
    return;
  }
  std::fclose(static_cast<FILE*>(raw));
}

}  // namespace

extern "C" DLLEXPORT int64_t
styio_file_open(const char* path) {
  if (path == nullptr) {
    set_runtime_error_once(kRuntimeSubcodeFilePathNull, "file path is null");
    return 0;
  }
  std::string resolved = resolve_read_path(path);
  FILE* f = std::fopen(resolved.c_str(), "rb");
  if (f == nullptr) {
    set_runtime_error_once(
      kRuntimeSubcodeFileOpenRead,
      std::string("cannot open file for read: ") + path);
    return 0;
  }
  return stash_file(f);
}

extern "C" DLLEXPORT int64_t
styio_file_open_auto(const char* path) {
  /* M5: same as explicit file path for local filesystem paths. */
  return styio_file_open(path);
}

extern "C" DLLEXPORT int64_t
styio_file_open_write(const char* path) {
  if (path == nullptr) {
    set_runtime_error_once(kRuntimeSubcodeFilePathNull, "file path is null");
    return 0;
  }
  /* Append so repeated writes in one program (e.g. per-iteration << file) accumulate. */
  FILE* f = std::fopen(path, "ab");
  if (f == nullptr) {
    set_runtime_error_once(
      kRuntimeSubcodeFileOpenWrite,
      std::string("cannot open file for write: ") + path);
    return 0;
  }
  return stash_file(f);
}

extern "C" DLLEXPORT void
styio_file_close(int64_t h) {
  (void)g_handle_table.release(h, StyioHandleTable::HandleKind::File, close_file);
}

extern "C" DLLEXPORT void
styio_file_rewind(int64_t h) {
  FILE* f = as_file(h, true);
  if (f != nullptr) {
    std::rewind(f);
  }
}

extern "C" DLLEXPORT const char*
styio_file_read_line(int64_t h) {
  FILE* f = as_file(h, true);
  if (f == nullptr) {
    return nullptr;
  }
  int w = g_read_line_buf_which;
  g_read_line_buf_which = 1 - g_read_line_buf_which;
  char* buf = g_read_line_bufs[w];
  if (std::fgets(buf, static_cast<int>(sizeof(g_read_line_bufs[0])), f) == nullptr) {
    return nullptr;
  }
  size_t n = std::strlen(buf);
  while (n > 0 && (buf[n - 1] == '\n' || buf[n - 1] == '\r')) {
    buf[--n] = '\0';
  }
  return buf;
}

extern "C" DLLEXPORT int64_t
styio_cstr_to_i64(const char* s) {
  if (s == nullptr || s[0] == '\0') {
    return 0;
  }
  return static_cast<int64_t>(std::strtoll(s, nullptr, 10));
}

extern "C" DLLEXPORT void
styio_file_write_cstr(int64_t h, const char* data) {
  FILE* f = as_file(h, true);
  if (f == nullptr || data == nullptr) {
    return;
  }
  std::fputs(data, f);
}

extern "C" DLLEXPORT int64_t
styio_read_file_i64line(const char* path) {
  if (path == nullptr) {
    set_runtime_error_once(kRuntimeSubcodeFilePathNull, "file path is null");
    return 0;
  }
  std::string resolved = resolve_read_path(path);
  FILE* f = std::fopen(resolved.c_str(), "rb");
  if (f == nullptr) {
    set_runtime_error_once(
      kRuntimeSubcodeFileOpenRead,
      std::string("cannot open file for read: ") + path);
    return 0;
  }
  char buf[512];
  if (std::fgets(buf, static_cast<int>(sizeof(buf)), f) == nullptr) {
    std::fclose(f);
    return 0;
  }
  std::fclose(f);
  size_t n = std::strlen(buf);
  while (n > 0 && (buf[n - 1] == '\n' || buf[n - 1] == '\r')) {
    buf[--n] = '\0';
  }
  return styio_cstr_to_i64(buf);
}

extern "C" DLLEXPORT const char*
styio_strcat_ab(const char* a, const char* b) {
  const char* ea = a ? a : "";
  const char* eb = b ? b : "";
  size_t na = std::strlen(ea);
  size_t nb = std::strlen(eb);
  char* p = static_cast<char*>(std::malloc(na + nb + 1));
  if (p == nullptr) {
    return "";
  }
  std::memcpy(p, ea, na);
  std::memcpy(p + na, eb, nb + 1);
  g_owned_cstr_ptrs.insert(p);
  return p;
}

extern "C" DLLEXPORT void
styio_free_cstr(const char* s) {
  if (s == nullptr) {
    return;
  }
  auto it = g_owned_cstr_ptrs.find(s);
  if (it == g_owned_cstr_ptrs.end()) {
    return;
  }
  g_owned_cstr_ptrs.erase(it);
  std::free(const_cast<char*>(s));
}

thread_local char g_i64_dec_buf[32];
thread_local char g_f64_dec_buf[64];

extern "C" DLLEXPORT const char*
styio_i64_dec_cstr(int64_t v) {
  std::snprintf(
    g_i64_dec_buf,
    sizeof(g_i64_dec_buf),
    "%lld",
    static_cast<long long>(v));
  return g_i64_dec_buf;
}

extern "C" DLLEXPORT const char*
styio_f64_dec_cstr(double v) {
  std::snprintf(
    g_f64_dec_buf,
    sizeof(g_f64_dec_buf),
    "%.6f",
    v);
  return g_f64_dec_buf;
}

extern "C" DLLEXPORT int
styio_runtime_has_error() {
  return g_runtime_error ? 1 : 0;
}

extern "C" DLLEXPORT const char*
styio_runtime_last_error() {
  if (!g_runtime_error || g_runtime_error_message.empty()) {
    return nullptr;
  }
  return g_runtime_error_message.c_str();
}

extern "C" DLLEXPORT const char*
styio_runtime_last_error_subcode() {
  if (!g_runtime_error || g_runtime_error_subcode.empty()) {
    return nullptr;
  }
  return g_runtime_error_subcode.c_str();
}

extern "C" DLLEXPORT void
styio_runtime_clear_error() {
  g_runtime_error = false;
  g_runtime_error_message.clear();
  g_runtime_error_subcode.clear();
}

/* M9: write a C-string to stderr with trailing newline and immediate flush.
   Null-safe (no-op for nullptr). */
extern "C" DLLEXPORT void
styio_stderr_write_cstr(const char* s) {
  if (s != nullptr) {
    std::fprintf(stderr, "%s\n", s);
    std::fflush(stderr);
  }
}

/* M10: read one line from stdin into a thread-local buffer.
   Returns borrowed pointer (valid until next call on this thread).
   Returns nullptr on EOF. Strips trailing newline/CR. */
thread_local char g_stdin_line_buf[65536];

extern "C" DLLEXPORT const char*
styio_stdin_read_line() {
  if (std::fgets(g_stdin_line_buf, static_cast<int>(sizeof(g_stdin_line_buf)), stdin) == nullptr) {
    return nullptr;
  }
  size_t n = std::strlen(g_stdin_line_buf);
  while (n > 0 && (g_stdin_line_buf[n - 1] == '\n' || g_stdin_line_buf[n - 1] == '\r')) {
    g_stdin_line_buf[--n] = '\0';
  }
  return g_stdin_line_buf;
}

extern "C" DLLEXPORT int
something() {
  return 0;
}
