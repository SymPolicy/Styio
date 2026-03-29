#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>

#include "ExternLib.hpp"

namespace {

/* Alternating buffers so two consecutive reads (e.g. zip of two files) keep both lines valid. */
thread_local char g_read_line_bufs[2][65536];
thread_local int g_read_line_buf_which = 0;

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
  /* Append so repeated writes in one program (e.g. per-iteration << file) accumulate. */
  FILE* f = std::fopen(path, "ab");
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

extern "C" DLLEXPORT void
styio_file_rewind(int64_t h) {
  FILE* f = as_file(h);
  if (f != nullptr) {
    std::rewind(f);
  }
}

extern "C" DLLEXPORT const char*
styio_file_read_line(int64_t h) {
  FILE* f = as_file(h);
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
  FILE* f = as_file(h);
  if (f == nullptr || data == nullptr) {
    return;
  }
  std::fputs(data, f);
}

extern "C" DLLEXPORT int64_t
styio_read_file_i64line(const char* path) {
  if (path == nullptr) {
    return 0;
  }
  FILE* f = std::fopen(path, "rb");
  if (f == nullptr) {
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
  return p;
}

thread_local char g_i64_dec_buf[32];

extern "C" DLLEXPORT const char*
styio_i64_dec_cstr(int64_t v) {
  std::snprintf(
    g_i64_dec_buf,
    sizeof(g_i64_dec_buf),
    "%lld",
    static_cast<long long>(v));
  return g_i64_dec_buf;
}

extern "C" DLLEXPORT int
something() {
  return 0;
}
