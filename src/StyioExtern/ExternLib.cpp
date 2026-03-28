#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>

#include "ExternLib.hpp"

namespace {

thread_local char g_read_line_buf[65536];

FILE*
as_file(int64_t h) {
  if (h == 0) {
    return nullptr;
  }
  return reinterpret_cast<FILE*>(static_cast<uintptr_t>(static_cast<uint64_t>(h)));
}

}  // namespace

extern "C" DLLEXPORT int64_t
styio_file_open(const char* path) {
  if (path == nullptr) {
    std::fprintf(stderr, "styio: file path is null\n");
    std::exit(1);
  }
  FILE* f = std::fopen(path, "rb");
  if (f == nullptr) {
    std::fprintf(stderr, "styio: cannot open file for read: %s\n", path);
    std::exit(1);
  }
  return static_cast<int64_t>(reinterpret_cast<uintptr_t>(f));
}

extern "C" DLLEXPORT int64_t
styio_file_open_auto(const char* path) {
  /* M5: same as explicit file path for local filesystem paths. */
  return styio_file_open(path);
}

extern "C" DLLEXPORT int64_t
styio_file_open_write(const char* path) {
  if (path == nullptr) {
    std::fprintf(stderr, "styio: file path is null\n");
    std::exit(1);
  }
  FILE* f = std::fopen(path, "wb");
  if (f == nullptr) {
    std::fprintf(stderr, "styio: cannot open file for write: %s\n", path);
    std::exit(1);
  }
  return static_cast<int64_t>(reinterpret_cast<uintptr_t>(f));
}

extern "C" DLLEXPORT void
styio_file_close(int64_t h) {
  FILE* f = as_file(h);
  if (f != nullptr) {
    std::fclose(f);
  }
}

extern "C" DLLEXPORT const char*
styio_file_read_line(int64_t h) {
  FILE* f = as_file(h);
  if (f == nullptr) {
    return nullptr;
  }
  if (std::fgets(g_read_line_buf, static_cast<int>(sizeof(g_read_line_buf)), f) == nullptr) {
    return nullptr;
  }
  size_t n = std::strlen(g_read_line_buf);
  while (n > 0 && (g_read_line_buf[n - 1] == '\n' || g_read_line_buf[n - 1] == '\r')) {
    g_read_line_buf[--n] = '\0';
  }
  return g_read_line_buf;
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
  FILE* f = as_file(h);
  if (f == nullptr || data == nullptr) {
    return;
  }
  std::fputs(data, f);
}

extern "C" DLLEXPORT int
something() {
  return 0;
}
