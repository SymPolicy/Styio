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
extern "C" DLLEXPORT void styio_file_rewind(int64_t h);
/* Borrowed pointer backed by thread-local buffers; valid until next read call on this thread. */
/* Caller must NOT pass the return value to styio_free_cstr. */
extern "C" DLLEXPORT const char* styio_file_read_line(int64_t h);
extern "C" DLLEXPORT void styio_file_write_cstr(int64_t h, const char* data);
extern "C" DLLEXPORT int64_t styio_cstr_to_i64(const char* s);

/* M7: first line of file as integer; string concat (malloc result). */
extern "C" DLLEXPORT int64_t styio_read_file_i64line(const char* path);
/* Owns heap memory; release with styio_free_cstr. */
extern "C" DLLEXPORT const char* styio_strcat_ab(const char* a, const char* b);
/* Safe no-op for null/non-owned pointers; frees only styio-owned cstr allocations. */
extern "C" DLLEXPORT void styio_free_cstr(const char* s);
/* Borrowed thread-local decimal buffers; do not free. */
extern "C" DLLEXPORT const char* styio_i64_dec_cstr(int64_t v);
extern "C" DLLEXPORT const char* styio_f64_dec_cstr(double v);
extern "C" DLLEXPORT int styio_runtime_has_error();
/* Borrowed pointer to last runtime error message; null when no runtime error is set. */
extern "C" DLLEXPORT const char* styio_runtime_last_error();
/* Borrowed pointer to last runtime error subcode; null when no runtime error is set. */
extern "C" DLLEXPORT const char* styio_runtime_last_error_subcode();
extern "C" DLLEXPORT void styio_runtime_clear_error();

extern "C" DLLEXPORT int something();

#endif  // STYIO_EXTERN_LIB_H
