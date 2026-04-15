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
extern "C" DLLEXPORT double styio_cstr_to_f64(const char* s);

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

/* M9: write to stderr */
extern "C" DLLEXPORT void styio_stderr_write_cstr(const char* s);

/* M10: read one line from stdin */
extern "C" DLLEXPORT const char* styio_stdin_read_line();

extern "C" DLLEXPORT int64_t styio_list_i64_read_stdin();
extern "C" DLLEXPORT int64_t styio_list_cstr_read_stdin();
extern "C" DLLEXPORT int64_t styio_list_new_bool();
extern "C" DLLEXPORT int64_t styio_list_new_i64();
extern "C" DLLEXPORT int64_t styio_list_new_f64();
extern "C" DLLEXPORT int64_t styio_list_new_cstr();
extern "C" DLLEXPORT int64_t styio_list_new_list();
extern "C" DLLEXPORT int64_t styio_list_new_dict();
extern "C" DLLEXPORT void styio_list_push_bool(int64_t h, int64_t value);
extern "C" DLLEXPORT void styio_list_push_i64(int64_t h, int64_t value);
extern "C" DLLEXPORT void styio_list_push_f64(int64_t h, double value);
extern "C" DLLEXPORT void styio_list_push_cstr(int64_t h, const char* value);
extern "C" DLLEXPORT void styio_list_push_list(int64_t h, int64_t value);
extern "C" DLLEXPORT void styio_list_push_dict(int64_t h, int64_t value);
extern "C" DLLEXPORT int64_t styio_list_clone(int64_t h);
extern "C" DLLEXPORT int64_t styio_list_len(int64_t h);
extern "C" DLLEXPORT int64_t styio_list_get_bool(int64_t h, int64_t idx);
extern "C" DLLEXPORT int64_t styio_list_get(int64_t h, int64_t idx);
extern "C" DLLEXPORT double styio_list_get_f64(int64_t h, int64_t idx);
extern "C" DLLEXPORT const char* styio_list_get_cstr(int64_t h, int64_t idx);
extern "C" DLLEXPORT int64_t styio_list_get_list(int64_t h, int64_t idx);
extern "C" DLLEXPORT int64_t styio_list_get_dict(int64_t h, int64_t idx);
extern "C" DLLEXPORT void styio_list_set(int64_t h, int64_t idx, int64_t value);
extern "C" DLLEXPORT const char* styio_list_to_cstr(int64_t h);
extern "C" DLLEXPORT void styio_list_release(int64_t h);
extern "C" DLLEXPORT int64_t styio_list_active_count();

extern "C" DLLEXPORT int64_t styio_dict_new_bool();
extern "C" DLLEXPORT int64_t styio_dict_new_i64();
extern "C" DLLEXPORT int64_t styio_dict_new_f64();
extern "C" DLLEXPORT int64_t styio_dict_new_cstr();
extern "C" DLLEXPORT int64_t styio_dict_new_list();
extern "C" DLLEXPORT int64_t styio_dict_new_dict();
extern "C" DLLEXPORT int64_t styio_dict_clone(int64_t h);
extern "C" DLLEXPORT int64_t styio_dict_len(int64_t h);
extern "C" DLLEXPORT int64_t styio_dict_get_bool(int64_t h, const char* key);
extern "C" DLLEXPORT int64_t styio_dict_get_i64(int64_t h, const char* key);
extern "C" DLLEXPORT double styio_dict_get_f64(int64_t h, const char* key);
extern "C" DLLEXPORT const char* styio_dict_get_cstr(int64_t h, const char* key);
extern "C" DLLEXPORT int64_t styio_dict_get_list(int64_t h, const char* key);
extern "C" DLLEXPORT int64_t styio_dict_get_dict(int64_t h, const char* key);
extern "C" DLLEXPORT void styio_dict_set_bool(int64_t h, const char* key, int64_t value);
extern "C" DLLEXPORT void styio_dict_set_i64(int64_t h, const char* key, int64_t value);
extern "C" DLLEXPORT void styio_dict_set_f64(int64_t h, const char* key, double value);
extern "C" DLLEXPORT void styio_dict_set_cstr(int64_t h, const char* key, const char* value);
extern "C" DLLEXPORT void styio_dict_set_list(int64_t h, const char* key, int64_t value);
extern "C" DLLEXPORT void styio_dict_set_dict(int64_t h, const char* key, int64_t value);
extern "C" DLLEXPORT int64_t styio_dict_keys(int64_t h);
extern "C" DLLEXPORT int64_t styio_dict_values_bool(int64_t h);
extern "C" DLLEXPORT int64_t styio_dict_values_i64(int64_t h);
extern "C" DLLEXPORT int64_t styio_dict_values_f64(int64_t h);
extern "C" DLLEXPORT int64_t styio_dict_values_cstr(int64_t h);
extern "C" DLLEXPORT int64_t styio_dict_values_list(int64_t h);
extern "C" DLLEXPORT int64_t styio_dict_values_dict(int64_t h);
extern "C" DLLEXPORT const char* styio_dict_to_cstr(int64_t h);
extern "C" DLLEXPORT void styio_dict_release(int64_t h);
extern "C" DLLEXPORT int64_t styio_dict_active_count();
extern "C" DLLEXPORT int styio_dict_runtime_supported_impl_count();
extern "C" DLLEXPORT const char* styio_dict_runtime_supported_impl_name(int index);
extern "C" DLLEXPORT const char* styio_dict_runtime_canonical_impl_name(const char* raw_name);
extern "C" DLLEXPORT int styio_dict_runtime_set_impl_by_name(const char* raw_name);
extern "C" DLLEXPORT const char* styio_dict_runtime_get_impl_name();
extern "C" DLLEXPORT int styio_dict_runtime_set_impl(int impl);
extern "C" DLLEXPORT int styio_dict_runtime_get_impl();

extern "C" DLLEXPORT int something();

#endif  // STYIO_EXTERN_LIB_H
