#ifndef STYIO_EXTERN_LIB_H
#define STYIO_EXTERN_LIB_H

#ifdef _WIN32
#define DLLEXPORT __declspec(dllexport)
#else
#define DLLEXPORT
#endif

#include <cstdint>

extern "C" DLLEXPORT int64_t styio_file_open(const char* path);
extern "C" DLLEXPORT int64_t styio_file_open_auto(const char* path);
extern "C" DLLEXPORT int64_t styio_file_open_write(const char* path);
extern "C" DLLEXPORT void styio_file_close(int64_t h);
extern "C" DLLEXPORT const char* styio_file_read_line(int64_t h);
extern "C" DLLEXPORT void styio_file_write_cstr(int64_t h, const char* data);
extern "C" DLLEXPORT int64_t styio_cstr_to_i64(const char* s);

extern "C" DLLEXPORT int something();

#endif  // STYIO_EXTERN_LIB_H
