#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <future>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include "EnvTestUtil.hpp"

#include "../src/StyioExtern/ExternLib.cpp"

namespace {

class EnvVarGuard {
 public:
  explicit EnvVarGuard(std::string name)
    : name_(std::move(name))
  {
    if (const char* value = std::getenv(name_.c_str())) {
      original_ = std::string(value);
    }
  }

  ~EnvVarGuard() {
    if (original_.has_value()) {
      styio_test_setenv(name_.c_str(), original_->c_str(), 1);
    }
    else {
      styio_test_unsetenv(name_.c_str());
    }
  }

  void set(const std::string& value) {
    styio_test_setenv(name_.c_str(), value.c_str(), 1);
  }

 private:
  std::string name_;
  std::optional<std::string> original_;
};

std::string take_owned_cstr(const char* raw) {
  std::string out = raw != nullptr ? raw : "";
  styio_free_cstr(raw);
  return out;
}

std::vector<std::string> g_runtime_log_events;

void test_runtime_log_sink(const char* stream, const char* message) {
  g_runtime_log_events.push_back(
    std::string(stream != nullptr ? stream : "<null>")
    + "="
    + std::string(message != nullptr ? message : "<null>"));
}

struct BlockingTaskContext {
  std::atomic<bool>* entered;
  std::atomic<bool>* release;
  int64_t value;
};

int64_t run_blocking_task(void* raw) {
  auto* context = static_cast<BlockingTaskContext*>(raw);
  context->entered->store(true, std::memory_order_release);
  context->entered->notify_all();
  context->release->wait(false, std::memory_order_acquire);
  return context->value;
}

int64_t run_value_task(void* raw) {
  return *static_cast<int64_t*>(raw);
}

}  // namespace

TEST(StyioExternLibInternal, RuntimeStateSubsystemOwnsErrorsAndLogSink) {
  styio::runtime::clear_error_state();
  EXPECT_FALSE(styio::runtime::has_error());

  styio::runtime::set_error_once("STYIO_RUNTIME_NUMERIC_PARSE", "first");
  styio::runtime::set_error_once("STYIO_RUNTIME_FILE_OPEN_READ", "second");
  EXPECT_TRUE(styio::runtime::has_error());
  EXPECT_STREQ(styio::runtime::last_error(), "first");
  EXPECT_STREQ(styio::runtime::last_error_subcode(), "STYIO_RUNTIME_NUMERIC_PARSE");
  EXPECT_TRUE(styio::runtime::error_matches_effect("parse"));
  EXPECT_FALSE(styio::runtime::error_matches_effect("io"));

  std::thread worker([] {
    EXPECT_FALSE(styio::runtime::has_error());
    styio::runtime::set_error_once("STYIO_RUNTIME_FILE_OPEN_READ", "worker");
    EXPECT_TRUE(styio::runtime::error_matches_effect("io"));
    styio::runtime::clear_error_state();
  });
  worker.join();

  const styio::runtime::RuntimeErrorSnapshot snapshot =
    styio::runtime::take_error();
  EXPECT_TRUE(snapshot.has_error);
  EXPECT_EQ(snapshot.message, "first");
  EXPECT_EQ(snapshot.subcode, "STYIO_RUNTIME_NUMERIC_PARSE");
  EXPECT_FALSE(styio::runtime::has_error());

  g_runtime_log_events.clear();
  styio::runtime::set_log_sink(test_runtime_log_sink);
  styio::runtime::log_to_sink("stdout", "hello");
  styio::runtime::set_log_sink(nullptr);
  styio::runtime::log_to_sink("stderr", "ignored");
  ASSERT_EQ(g_runtime_log_events.size(), 1u);
  EXPECT_EQ(g_runtime_log_events[0], "stdout=hello");
}

TEST(StyioExternLibInternal, DictTemplateHelpersCoverNullLinearAndInvalidBackends) {
  rebuild_dict_index(static_cast<StyioDictI64*>(nullptr));
  dict_after_clone(static_cast<StyioDictI64*>(nullptr));

  size_t pos = 0;
  EXPECT_FALSE(dict_find_pos(static_cast<StyioDictI64*>(nullptr), "x", pos));
  dict_set(static_cast<StyioDictI64*>(nullptr), "x", int64_t{1});

  StyioDictI64 invalid(static_cast<StyioDictRuntimeImpl>(255));
  EXPECT_FALSE(dict_find_pos(&invalid, "x", pos));
  dict_set(&invalid, "x", int64_t{1});
  EXPECT_TRUE(invalid.entries.empty());
  invalid.entries.emplace_back("stale", 9);
  invalid.index_by_key["stale"] = 0;
  dict_after_clone(&invalid);
  EXPECT_TRUE(invalid.index_by_key.empty());

  StyioDictI64 linear(StyioDictRuntimeImpl::Linear);
  dict_set(&linear, "a", int64_t{1});
  dict_set(&linear, "a", int64_t{2});
  dict_set(&linear, nullptr, int64_t{3});
  ASSERT_EQ(linear.entries.size(), 1u);
  EXPECT_EQ(linear.entries[0].second, 2);
  EXPECT_TRUE(dict_find_pos(&linear, "a", pos));
  EXPECT_EQ(pos, 0u);
  EXPECT_FALSE(dict_find_pos(&linear, "missing", pos));
  EXPECT_FALSE(dict_find_pos(&linear, nullptr, pos));

  int releases = 0;
  StyioDictListHandle invalid_handles(static_cast<StyioDictRuntimeImpl>(255));
  dict_set_handle(&invalid_handles, "slot", int64_t{10}, [&](int64_t) { ++releases; });
  EXPECT_TRUE(invalid_handles.entries.empty());

  StyioDictListHandle handles(StyioDictRuntimeImpl::Linear);
  dict_set_handle(static_cast<StyioDictListHandle*>(nullptr), "slot", int64_t{10}, [&](int64_t) { ++releases; });
  dict_set_handle(&handles, "slot", int64_t{11}, [&](int64_t) { ++releases; });
  dict_set_handle(&handles, "slot", int64_t{12}, [&](int64_t) { ++releases; });
  dict_set_handle(&handles, nullptr, int64_t{13}, [&](int64_t) { ++releases; });
  ASSERT_EQ(handles.entries.size(), 1u);
  EXPECT_EQ(handles.entries[0].second, 12);
  EXPECT_EQ(releases, 1);
}

TEST(StyioExternLibInternal, RuntimeErrorEdgesForListMatrixDictAndTaskHandlesStayExplicit) {
  styio_runtime_clear_error();
  styio_runtime_report_integer_division_error(0);
  EXPECT_EQ(styio_runtime_has_error(), 1);
  EXPECT_STREQ(
    styio_runtime_last_error_subcode(),
    "arithmetic.divide_by_zero");
  styio_runtime_clear_error();
  styio_runtime_report_integer_division_error(1);
  EXPECT_STREQ(
    styio_runtime_last_error_subcode(),
    "arithmetic.signed_division_overflow");

  styio_runtime_clear_error();
  EXPECT_EQ(styio_list_len(987654321), 0);
  EXPECT_EQ(styio_runtime_has_error(), 1);
  EXPECT_EQ(styio_runtime_error_matches_effect("closed"), 1);

  styio_runtime_clear_error();
  int64_t list = styio_list_new_i64();
  styio_list_push_i64(list, 1);
  EXPECT_EQ(styio_list_get(list, -2), 0);
  EXPECT_EQ(styio_runtime_error_matches_effect("bounds"), 1);
  styio_runtime_clear_error();
  styio_list_release(list);

  EXPECT_EQ(styio_matrix_new_i64(-1, 2), 0);
  EXPECT_EQ(styio_runtime_has_error(), 1);
  styio_runtime_clear_error();
  int64_t matrix = styio_matrix_new_i64(1, 1);
  styio_matrix_set_i64(matrix, 2, 0, 9);
  EXPECT_EQ(styio_runtime_error_matches_effect("bounds"), 1);
  styio_runtime_clear_error();
  styio_matrix_release(matrix);

  int64_t dict = styio_dict_new_i64();
  EXPECT_EQ(styio_dict_get_i64(dict, nullptr), 0);
  EXPECT_EQ(styio_runtime_error_matches_effect("bounds"), 1);
  styio_runtime_clear_error();
  styio_dict_set_i64(dict, "a", 1);
  EXPECT_EQ(styio_dict_get_i64(dict, "a"), 1);
  EXPECT_EQ(styio_dict_get_i64(dict, "missing"), 0);
  EXPECT_EQ(styio_runtime_error_matches_effect("bounds"), 1);
  styio_runtime_clear_error();
  styio_dict_release(dict);

  int64_t task = styio_task_i64_ready(7);
  EXPECT_EQ(styio_task_i64_pull(task), 7);
  EXPECT_EQ(styio_task_i64_pull(task), 0);
  EXPECT_EQ(styio_runtime_error_matches_effect("closed"), 1);
  styio_runtime_clear_error();
  styio_task_release(task);
}

TEST(StyioExternLibInternal, RuntimeRepresentationCloneAndConfigHelpersStayExplicit) {
  std::vector<int64_t> ints;
  EXPECT_FALSE(parse_i64_list_literal("[1", ints));
  EXPECT_FALSE(parse_i64_list_literal("[1; 2]", ints));
  EXPECT_FALSE(parse_i64_list_literal("[", ints));

  std::vector<double> floats;
  EXPECT_TRUE(parse_f64_list_literal("[]", floats));
  EXPECT_TRUE(floats.empty());
  EXPECT_FALSE(parse_f64_list_literal("nope", floats));
  EXPECT_FALSE(parse_f64_list_literal("[1.0", floats));
  EXPECT_FALSE(parse_f64_list_literal("[1.0; 2.0]", floats));
  EXPECT_FALSE(parse_f64_list_literal("[", floats));
  EXPECT_FALSE(parse_f64_list_literal("[1.0, inf]", floats));
  EXPECT_TRUE(resolve_read_path(nullptr).empty());
  close_file(nullptr);
  EXPECT_EQ(stash_list(nullptr), 0);
  EXPECT_EQ(something(), 0);

  std::string rendered;
  append_list_handle_repr(rendered, 0);
  EXPECT_EQ(rendered, "[]");
  rendered.clear();
  append_dict_handle_repr(rendered, 0);
  EXPECT_EQ(rendered, "{}");

  EXPECT_GT(styio_dict_runtime_supported_impl_count(), 0);
  EXPECT_EQ(styio_dict_runtime_supported_impl_name(-1), nullptr);
  EXPECT_EQ(styio_dict_runtime_canonical_impl_name("missing-backend"), nullptr);
  EXPECT_EQ(styio_dict_runtime_set_impl(255), 0);
  ASSERT_EQ(styio_dict_runtime_set_impl_by_name("linear"), 1);
  EXPECT_STREQ(styio_dict_runtime_get_impl_name(), "linear");
  const auto saved_backend = g_default_dict_runtime_impl;
  g_default_dict_runtime_impl = static_cast<StyioDictRuntimeImpl>(255);
  EXPECT_STREQ(current_dict_backend_spec()->name, kStyioDictBackendRegistry[0].name);
  g_default_dict_runtime_impl = saved_backend;

  int64_t bool_dict = styio_dict_new_bool();
  styio_dict_set_bool(bool_dict, "flag", 1);
  styio_dict_set_bool(bool_dict, "other", 0);
  EXPECT_EQ(styio_dict_len(bool_dict), 2);
  EXPECT_EQ(take_owned_cstr(styio_dict_to_cstr(bool_dict)), "{\"flag\":true,\"other\":false}");
  int64_t bool_keys = styio_dict_keys(bool_dict);
  EXPECT_EQ(styio_list_len(bool_keys), 2);
  int64_t bool_values = styio_dict_values_bool(bool_dict);
  EXPECT_EQ(styio_list_len(bool_values), 2);
  styio_list_release(bool_values);
  styio_list_release(bool_keys);
  styio_dict_release(bool_dict);

  int64_t f64_dict = styio_dict_new_f64();
  styio_dict_set_f64(f64_dict, "x", 1.5);
  styio_dict_set_f64(f64_dict, "y", 2.5);
  EXPECT_EQ(take_owned_cstr(styio_dict_to_cstr(f64_dict)), "{\"x\":1.500000,\"y\":2.500000}");
  styio_dict_release(f64_dict);

  int64_t matrix = styio_matrix_new_f64(1, 2);
  styio_matrix_set_f64(matrix, 0, 0, 1.5);
  styio_matrix_set_f64(matrix, 0, 1, 2.5);
  int64_t cloned = clone_matrix_handle_value(matrix);
  ASSERT_NE(cloned, 0);
  EXPECT_DOUBLE_EQ(styio_matrix_get_f64(cloned, 0, 1), 2.5);
  EXPECT_EQ(take_owned_cstr(styio_matrix_to_cstr(cloned)), "[[1.500000,2.500000]]");
  styio_matrix_release(cloned);
  styio_matrix_release(matrix);

  auto* invalid_list = new StyioListBase(static_cast<StyioListElemKind>(255));
  const int64_t invalid_list_handle = stash_list(invalid_list);
  EXPECT_EQ(clone_list_handle_value(invalid_list_handle), 0);
  EXPECT_EQ(styio_list_slice(invalid_list_handle, 0, 1, 1), 0);
  styio_list_release(invalid_list_handle);
  delete invalid_list;

  auto* invalid_matrix = new StyioMatrixBase(static_cast<StyioMatrixElemKind>(255), 1, 1);
  const int64_t invalid_matrix_handle = stash_matrix(invalid_matrix);
  EXPECT_EQ(clone_matrix_handle_value(invalid_matrix_handle), 0);
  styio_matrix_release(invalid_matrix_handle);
  delete invalid_matrix;

  auto* invalid_dict = new StyioDictBase(
    static_cast<StyioDictValueKind>(255),
    StyioDictRuntimeImpl::Linear);
  const int64_t invalid_dict_handle = stash_dict(invalid_dict);
  EXPECT_EQ(clone_dict_handle_value(invalid_dict_handle), 0);
  EXPECT_EQ(styio_dict_len(invalid_dict_handle), 0);
  styio_dict_release(invalid_dict_handle);
  delete invalid_dict;
}

TEST(StyioExternLibInternal, I64ListDataProbeDoesNotDiagnoseAndTracksStorage) {
  styio_runtime_clear_error();
  EXPECT_EQ(styio_list_i64_data(0), nullptr);
  EXPECT_EQ(styio_list_i64_len(0), 0);
  EXPECT_EQ(styio_runtime_has_error(), 0);

  int64_t ints = styio_list_new_i64();
  styio_list_push_i64(ints, 7);
  styio_list_push_i64(ints, 9);
  EXPECT_EQ(styio_list_i64_len(ints), 2);
  const int64_t* data = styio_list_i64_data(ints);
  ASSERT_NE(data, nullptr);
  EXPECT_EQ(data[0], 7);
  EXPECT_EQ(data[1], 9);
  styio_list_push_i64(ints, 11);
  EXPECT_EQ(styio_list_i64_len(ints), 3);
  data = styio_list_i64_data(ints);
  ASSERT_NE(data, nullptr);
  EXPECT_EQ(data[2], 11);
  styio_list_release(ints);
}

TEST(StyioExternLibInternal, ListCloneSliceAndEmptyMutationEdgesStayExplicit) {
  list_insert_value(static_cast<StyioListI64*>(nullptr), 0, int64_t{1});
  list_set_value(static_cast<StyioListI64*>(nullptr), 0, int64_t{1});
  EXPECT_EQ(slice_plain_list(static_cast<StyioListI64*>(nullptr), 0, 1, true), 0);

  int64_t bools = styio_list_new_bool();
  styio_list_push_bool(bools, 1);
  int64_t bool_clone = clone_list_handle_value(bools);
  ASSERT_NE(bool_clone, 0);
  EXPECT_EQ(styio_list_get_bool(bool_clone, 0), 1);
  styio_list_release(bool_clone);
  styio_list_release(bools);

  int64_t chars = styio_list_new_char();
  styio_list_push_char(chars, static_cast<int8_t>('a'));
  int64_t char_clone = clone_list_handle_value(chars);
  ASSERT_NE(char_clone, 0);
  EXPECT_EQ(styio_list_get_char(char_clone, 0), static_cast<int8_t>('a'));
  styio_list_release(char_clone);
  styio_list_release(chars);

  int64_t ints = styio_list_new_i64();
  styio_list_push_i64(ints, 7);
  int64_t int_clone = clone_list_handle_value(ints);
  ASSERT_NE(int_clone, 0);
  EXPECT_EQ(styio_list_get(int_clone, 0), 7);
  styio_list_release(int_clone);
  styio_list_release(ints);

  int64_t floats = styio_list_new_f64();
  styio_list_push_f64(floats, 3.25);
  int64_t float_clone = clone_list_handle_value(floats);
  ASSERT_NE(float_clone, 0);
  EXPECT_DOUBLE_EQ(styio_list_get_f64(float_clone, 0), 3.25);
  styio_list_release(float_clone);
  styio_list_release(floats);

  int64_t strings = styio_list_new_cstr();
  styio_list_push_cstr(strings, "s");
  int64_t string_clone = clone_list_handle_value(strings);
  ASSERT_NE(string_clone, 0);
  EXPECT_EQ(take_owned_cstr(styio_list_get_cstr(string_clone, 0)), "s");
  styio_list_release(string_clone);
  styio_list_release(strings);

  int64_t inner = styio_list_new_i64();
  styio_list_push_i64(inner, 4);
  int64_t list_list = styio_list_new_list();
  styio_list_push_list(list_list, inner);
  int64_t list_clone = clone_list_handle_value(list_list);
  ASSERT_NE(list_clone, 0);
  int64_t cloned_inner = styio_list_get_list(list_clone, 0);
  ASSERT_NE(cloned_inner, 0);
  EXPECT_EQ(styio_list_get(cloned_inner, 0), 4);
  styio_list_release(cloned_inner);
  styio_list_release(list_clone);
  styio_list_release(list_list);
  styio_list_release(inner);

  int64_t dict = styio_dict_new_i64();
  styio_dict_set_i64(dict, "k", 9);
  int64_t list_dict = styio_list_new_dict();
  styio_list_push_dict(list_dict, dict);
  int64_t dict_list_clone = clone_list_handle_value(list_dict);
  ASSERT_NE(dict_list_clone, 0);
  int64_t cloned_dict = styio_list_get_dict(dict_list_clone, 0);
  ASSERT_NE(cloned_dict, 0);
  EXPECT_EQ(styio_dict_get_i64(cloned_dict, "k"), 9);
  styio_dict_release(cloned_dict);
  styio_list_release(dict_list_clone);
  styio_list_release(list_dict);
  styio_dict_release(dict);

  int64_t matrix = styio_matrix_new_i64(1, 1);
  styio_matrix_set_i64(matrix, 0, 0, 5);
  int64_t list_matrix = styio_list_new_matrix();
  styio_list_push_matrix(list_matrix, matrix);
  int64_t matrix_list_clone = clone_list_handle_value(list_matrix);
  ASSERT_NE(matrix_list_clone, 0);
  int64_t cloned_matrix = styio_list_get_matrix(matrix_list_clone, 0);
  ASSERT_NE(cloned_matrix, 0);
  EXPECT_EQ(styio_matrix_get_i64(cloned_matrix, 0, 0), 5);
  styio_matrix_release(cloned_matrix);
  styio_list_release(matrix_list_clone);
  styio_list_release(list_matrix);
  styio_matrix_release(matrix);

  int64_t empty_bool = styio_list_new_bool();
  int64_t empty_char = styio_list_new_char();
  int64_t empty_i64 = styio_list_new_i64();
  int64_t empty_f64 = styio_list_new_f64();
  int64_t empty_cstr = styio_list_new_cstr();
  int64_t empty_list = styio_list_new_list();
  int64_t empty_dicts = styio_list_new_dict();
  int64_t empty_matrices = styio_list_new_matrix();
  EXPECT_EQ(styio_list_get_bool(empty_bool, 0), 0);
  EXPECT_EQ(styio_list_get_char(empty_char, 0), 0);
  EXPECT_EQ(styio_list_get(empty_i64, 0), 0);
  EXPECT_DOUBLE_EQ(styio_list_get_f64(empty_f64, 0), 0.0);
  EXPECT_EQ(styio_list_get_cstr(empty_cstr, 0), nullptr);
  EXPECT_EQ(styio_list_get_list(empty_list, 0), 0);
  EXPECT_EQ(styio_list_get_dict(empty_dicts, 0), 0);
  EXPECT_EQ(styio_list_get_matrix(empty_matrices, 0), 0);

  EXPECT_EQ(styio_list_slice(0, 0, 1, 1), 0);
  EXPECT_EQ(styio_list_slice(empty_list, -1, 0, 1), 0);
  EXPECT_EQ(styio_list_slice(empty_dicts, 0, 1, 1), 0);
  EXPECT_EQ(styio_list_slice(empty_matrices, 0, 1, 1), 0);
  styio_list_insert_list(0, 0, 0);
  styio_list_insert_dict(0, 0, 0);
  styio_list_insert_matrix(0, 0, 0);
  styio_list_set_list(0, 0, 0);
  styio_list_set_dict(0, 0, 0);
  styio_list_set_matrix(0, 0, 0);
  EXPECT_EQ(styio_list_to_cstr(0), nullptr);

  styio_runtime_clear_error();
  styio_list_pop(0);
  EXPECT_EQ(styio_runtime_has_error(), 0);

  std::vector<int64_t> empties = {
    empty_bool,
    empty_char,
    empty_i64,
    empty_f64,
    empty_cstr,
    empty_list,
    empty_dicts,
    empty_matrices,
  };
  for (int64_t h : empties) {
    styio_runtime_clear_error();
    styio_list_pop(h);
    EXPECT_EQ(styio_runtime_error_matches_effect("bounds"), 1);
    styio_list_release(h);
  }
}

TEST(StyioExternLibInternal, MatrixAndDictInvalidApiEdgesStayExplicit) {
  styio_runtime_clear_error();
  EXPECT_EQ(styio_matrix_new_f64(-1, 2), 0);
  EXPECT_EQ(styio_matrix_shape(0), 0);
  EXPECT_EQ(styio_matrix_add_i64(0, 0), 0);
  EXPECT_EQ(styio_matrix_add_f64(0, 0), 0);
  EXPECT_EQ(styio_matrix_sub_i64(0, 0), 0);
  EXPECT_EQ(styio_matrix_hadamard_i64(0, 0), 0);
  EXPECT_EQ(styio_matrix_hadamard_f64(0, 0), 0);
  EXPECT_EQ(styio_matrix_matmul_i64(0, 0), 0);
  EXPECT_EQ(styio_matrix_matmul_f64(0, 0), 0);
  EXPECT_EQ(styio_matrix_scale_i64(0, 2), 0);
  EXPECT_EQ(styio_matrix_scale_f64(0, 2.0), 0);
  EXPECT_EQ(styio_matrix_transpose_i64(0), 0);
  EXPECT_EQ(styio_matrix_transpose_f64(0), 0);
  EXPECT_EQ(styio_matrix_dot_i64(0, 0), 0);
  EXPECT_DOUBLE_EQ(styio_matrix_dot_f64(0, 0), 0.0);
  EXPECT_EQ(styio_matrix_sum_i64(0), 0);
  EXPECT_DOUBLE_EQ(styio_matrix_sum_f64(0), 0.0);
  EXPECT_DOUBLE_EQ(styio_matrix_norm(0), 0.0);
  EXPECT_EQ(styio_matrix_data_i64(0), nullptr);
  EXPECT_EQ(styio_matrix_data_f64(0), nullptr);

  int64_t a = styio_matrix_new_i64(1, 2);
  int64_t b = styio_matrix_new_i64(2, 1);
  EXPECT_EQ(styio_matrix_add_i64(a, b), 0);
  EXPECT_EQ(styio_matrix_matmul_i64(a, a), 0);
  int64_t scaled_f64 = styio_matrix_scale_f64(a, 0.5);
  ASSERT_NE(scaled_f64, 0);
  EXPECT_DOUBLE_EQ(styio_matrix_get_f64(scaled_f64, 0, 0), 0.0);
  int64_t af = styio_matrix_clone_f64(a);
  int64_t bf = styio_matrix_clone_f64(b);
  EXPECT_EQ(styio_matrix_add_f64(af, bf), 0);
  EXPECT_EQ(styio_matrix_matmul_f64(af, af), 0);
  styio_matrix_release(bf);
  styio_matrix_release(af);
  styio_matrix_release(scaled_f64);
  styio_matrix_release(b);
  styio_matrix_release(a);

  styio_dict_set_bool(0, "x", 1);
  styio_dict_set_i64(0, "x", 1);
  styio_dict_set_f64(0, "x", 1.0);
  styio_dict_set_cstr(0, "x", "v");
  styio_dict_set_list(0, "x", 0);
  styio_dict_set_dict(0, "x", 0);
  EXPECT_EQ(styio_dict_keys(0), 0);
  EXPECT_EQ(styio_dict_values_bool(0), 0);
  EXPECT_EQ(styio_dict_values_i64(0), 0);
  EXPECT_EQ(styio_dict_values_f64(0), 0);
  EXPECT_EQ(styio_dict_values_cstr(0), 0);
  EXPECT_EQ(styio_dict_values_list(0), 0);
  EXPECT_EQ(styio_dict_values_dict(0), 0);
  EXPECT_EQ(styio_dict_to_cstr(0), nullptr);

  int64_t inner_a = styio_dict_new_i64();
  int64_t inner_b = styio_dict_new_i64();
  styio_dict_set_i64(inner_a, "v", 1);
  styio_dict_set_i64(inner_b, "v", 2);
  int64_t dicts = styio_dict_new_dict();
  styio_dict_set_dict(dicts, "a", inner_a);
  styio_dict_set_dict(dicts, "b", inner_b);
  EXPECT_EQ(take_owned_cstr(styio_dict_to_cstr(dicts)), "{\"a\":{\"v\":1},\"b\":{\"v\":2}}");
  int64_t dicts_clone = styio_dict_clone(dicts);
  ASSERT_NE(dicts_clone, 0);
  int64_t values = styio_dict_values_dict(dicts_clone);
  EXPECT_EQ(styio_list_len(values), 2);
  styio_list_release(values);
  styio_dict_release(dicts_clone);
  styio_dict_release(dicts);
  styio_dict_release(inner_b);
  styio_dict_release(inner_a);

  int64_t list_value = styio_list_new_i64();
  styio_list_push_i64(list_value, 42);
  int64_t list_dict = styio_dict_new_list();
  styio_dict_set_list(list_dict, "items", list_value);
  int64_t list_dict_clone = styio_dict_clone(list_dict);
  ASSERT_NE(list_dict_clone, 0);
  int64_t cloned_items = styio_dict_get_list(list_dict_clone, "items");
  ASSERT_NE(cloned_items, 0);
  EXPECT_EQ(styio_list_get(cloned_items, 0), 42);
  int64_t list_values = styio_dict_values_list(list_dict_clone);
  EXPECT_EQ(styio_list_len(list_values), 1);
  styio_list_release(list_values);
  styio_list_release(cloned_items);
  styio_dict_release(list_dict_clone);
  styio_dict_release(list_dict);
  styio_list_release(list_value);
}

TEST(StyioExternLibInternal, MatrixMixedKindKernelsPreserveRowMajorResults) {
  const int64_t left_i64 = styio_matrix_new_i64(2, 3);
  const int64_t right_i64 = styio_matrix_new_i64(3, 2);
  ASSERT_NE(left_i64, 0);
  ASSERT_NE(right_i64, 0);

  for (int64_t index = 0; index < 6; ++index) {
    styio_matrix_set_i64(left_i64, index / 3, index % 3, index + 1);
    styio_matrix_set_i64(right_i64, index / 2, index % 2, index + 7);
  }
  const int64_t left_f64 = styio_matrix_clone_f64(left_i64);
  const int64_t right_f64 = styio_matrix_clone_f64(right_i64);
  ASSERT_NE(left_f64, 0);
  ASSERT_NE(right_f64, 0);

  const int64_t products[] = {
    styio_matrix_matmul_f64(left_i64, right_i64),
    styio_matrix_matmul_f64(left_i64, right_f64),
    styio_matrix_matmul_f64(left_f64, right_i64),
    styio_matrix_matmul_f64(left_f64, right_f64),
  };
  for (const int64_t product : products) {
    ASSERT_NE(product, 0);
    EXPECT_DOUBLE_EQ(styio_matrix_get_f64(product, 0, 0), 58.0);
    EXPECT_DOUBLE_EQ(styio_matrix_get_f64(product, 0, 1), 64.0);
    EXPECT_DOUBLE_EQ(styio_matrix_get_f64(product, 1, 0), 139.0);
    EXPECT_DOUBLE_EQ(styio_matrix_get_f64(product, 1, 1), 154.0);
    styio_matrix_release(product);
  }

  const int64_t difference = styio_matrix_sub_f64(left_i64, left_f64);
  ASSERT_NE(difference, 0);
  EXPECT_DOUBLE_EQ(styio_matrix_sum_f64(difference), 0.0);
  styio_matrix_release(difference);
  styio_matrix_release(right_f64);
  styio_matrix_release(left_f64);
  styio_matrix_release(right_i64);
  styio_matrix_release(left_i64);
}

TEST(StyioExternLibInternal, NullAndWrongKindHandleCastsStayExplicit) {
  styio_runtime_clear_error();

  EXPECT_EQ(as_list_base(0), nullptr);
  EXPECT_EQ(as_list_i64(0), nullptr);
  EXPECT_EQ(as_list_bool(0), nullptr);
  EXPECT_EQ(as_list_char(0), nullptr);
  EXPECT_EQ(as_list_f64(0), nullptr);
  EXPECT_EQ(as_list_string(0), nullptr);
  EXPECT_EQ(as_list_list_handle(0), nullptr);
  EXPECT_EQ(as_list_dict_handle(0), nullptr);
  EXPECT_EQ(as_list_matrix_handle(0), nullptr);
  close_list(nullptr);

  const int64_t list = styio_list_new_i64();
  EXPECT_NE(as_list_i64(list), nullptr);
  EXPECT_EQ(as_list_bool(list, true), nullptr);
  EXPECT_EQ(as_list_char(list, true), nullptr);
  EXPECT_EQ(as_list_f64(list, true), nullptr);
  EXPECT_EQ(as_list_string(list, true), nullptr);
  EXPECT_EQ(as_list_list_handle(list, true), nullptr);
  EXPECT_EQ(as_list_dict_handle(list, true), nullptr);
  EXPECT_EQ(as_list_matrix_handle(list, true), nullptr);
  styio_runtime_clear_error();
  styio_list_release(list);

  EXPECT_EQ(stash_matrix(nullptr), 0);
  EXPECT_EQ(as_matrix_base(0), nullptr);
  EXPECT_EQ(as_matrix_i64(0), nullptr);
  EXPECT_EQ(as_matrix_f64(0), nullptr);
  EXPECT_FALSE(check_matrix_index(nullptr, 0, 0));
  int64_t begin = 0;
  int64_t finish = 0;
  EXPECT_FALSE(check_matrix_row_slice_bounds(nullptr, 0, 1, true, begin, finish));
  EXPECT_FALSE(same_matrix_shape(nullptr, nullptr));
  EXPECT_EQ(clone_matrix(static_cast<StyioMatrixI64*>(nullptr)), 0);
  close_matrix(nullptr);

  const int64_t matrix = styio_matrix_new_f64(1, 1);
  EXPECT_NE(as_matrix_f64(matrix), nullptr);
  EXPECT_EQ(as_matrix_i64(matrix, true), nullptr);
  styio_runtime_clear_error();
  styio_matrix_release(matrix);

  EXPECT_EQ(stash_dict(nullptr), 0);
  EXPECT_EQ(as_dict_base(0), nullptr);
  EXPECT_EQ(as_dict_bool(0), nullptr);
  EXPECT_EQ(as_dict_i64(0), nullptr);
  EXPECT_EQ(as_dict_f64(0), nullptr);
  EXPECT_EQ(as_dict_string(0), nullptr);
  EXPECT_EQ(as_dict_list(0), nullptr);
  EXPECT_EQ(as_dict_dict(0), nullptr);
  close_dict(nullptr);

  const int64_t dict = styio_dict_new_bool();
  EXPECT_NE(as_dict_bool(dict), nullptr);
  EXPECT_EQ(as_dict_i64(dict, true), nullptr);
  EXPECT_EQ(as_dict_f64(dict, true), nullptr);
  EXPECT_EQ(as_dict_string(dict, true), nullptr);
  EXPECT_EQ(as_dict_list(dict, true), nullptr);
  EXPECT_EQ(as_dict_dict(dict, true), nullptr);
  styio_runtime_clear_error();
  styio_dict_release(dict);
}

TEST(StyioExternLibInternal, TaskSchedulerAndPendingTaskReleaseEdgesStayExplicit) {
  EnvVarGuard threads("STYIO_TASK_THREADS");
  threads.set("1");
  EnvVarGuard capacity("STYIO_TASK_QUEUE_CAPACITY");
  capacity.set("1");

  EXPECT_EQ(
    StyioTaskScheduler::instance().ready_queue_kind(),
    styio::runtime::ReadyQueueKind::BoundedWait);
  const auto queue = StyioTaskScheduler::instance().ready_queue_snapshot();
  EXPECT_EQ(queue.capacity, 1u);
  EXPECT_FALSE(queue.closed);

  std::atomic<bool> start_workers{false};
  std::vector<std::size_t> worker_counts(32, 0);
  std::vector<std::thread> starters;
  for (std::size_t i = 0; i < worker_counts.size(); ++i) {
    starters.emplace_back([&, i]() {
      while (!start_workers.load(std::memory_order_acquire)) {
        std::this_thread::yield();
      }
      worker_counts[i] = StyioTaskScheduler::instance().worker_count();
    });
  }
  start_workers.store(true, std::memory_order_release);
  for (auto& starter : starters) {
    starter.join();
  }
  for (const std::size_t workers : worker_counts) {
    EXPECT_EQ(workers, 1u);
  }

  styio_task_scheduler_profile_reset();
  styio_task_scheduler_profile_enable(1);
  std::atomic<bool> entered{false};
  std::atomic<bool> release{false};
  auto* first = new StyioTask(StyioTaskValueKind::I64);
  first->i64_fn = run_blocking_task;
  first->ctx = std::malloc(sizeof(BlockingTaskContext));
  ASSERT_NE(first->ctx, nullptr);
  *static_cast<BlockingTaskContext*>(first->ctx) = {&entered, &release, 40};
  StyioTaskScheduler::instance().enqueue(first);
  entered.wait(false, std::memory_order_acquire);
  // The blocked callback keeps the executor live: both it and this test own
  // references until completion and owner release, respectively.
  EXPECT_EQ(first->references.load(std::memory_order_acquire), 2u);

  auto make_value_task = [](int64_t value) {
    auto* task = new StyioTask(StyioTaskValueKind::I64);
    task->i64_fn = run_value_task;
    task->ctx = std::malloc(sizeof(int64_t));
    if (task->ctx != nullptr) {
      *static_cast<int64_t*>(task->ctx) = value;
    }
    return task;
  };
  StyioTask* second = make_value_task(1);
  StyioTask* third = make_value_task(1);
  ASSERT_NE(second->ctx, nullptr);
  ASSERT_NE(third->ctx, nullptr);
  StyioTaskScheduler::instance().enqueue(second);
  auto third_enqueue = std::async(std::launch::async, [&]() {
    StyioTaskScheduler::instance().enqueue(third);
  });
  const auto pressure_deadline = std::chrono::steady_clock::now()
    + std::chrono::seconds(2);
  while (StyioTaskScheduler::instance().ready_queue_snapshot().producer_waits < 1
         && std::chrono::steady_clock::now() < pressure_deadline) {
    std::this_thread::yield();
  }
  ASSERT_EQ(
    StyioTaskScheduler::instance().ready_queue_snapshot().producer_waits,
    1u);
  styio_runtime_clear_error();
  release.store(true, std::memory_order_release);
  release.notify_all();
  ASSERT_EQ(
    third_enqueue.wait_for(std::chrono::seconds(2)),
    std::future_status::ready);
  wait_until_task_ready(first);
  wait_until_task_ready(second);
  wait_until_task_ready(third);
  StyioTaskSchedulerProfileSnapshot snapshot{};
  styio_task_scheduler_profile_snapshot(&snapshot);
  EXPECT_EQ(snapshot.ready_queue_kind, 1);
  EXPECT_EQ(snapshot.queue_capacity, 1);
  EXPECT_EQ(snapshot.queue_current_depth, 0);
  EXPECT_EQ(snapshot.queue_accepted_pushes, 3);
  EXPECT_EQ(snapshot.queue_pops, 3);
  EXPECT_EQ(snapshot.queue_pressure_events, 1);
  EXPECT_EQ(snapshot.queue_producer_waits, 1);
  EXPECT_EQ(snapshot.max_queue_depth, 1);
  EXPECT_EQ(snapshot.queue_closed, 0);
  EXPECT_FALSE(styio::runtime::has_error());
  styio_task_scheduler_profile_enable(0);
  // Ready publication precedes the executor's final access. Release the owner
  // reference instead of deleting underneath run()'s terminal decrement.
  close_task(first);
  close_task(second);
  close_task(third);

  auto* task = make_value_task(42);
  ASSERT_NE(task->ctx, nullptr);
  ++g_active_task_handles;
  EXPECT_FALSE(task->ready.load(std::memory_order_acquire));
  // Closing a pending owner must demand a real launch and settle it. A helper
  // thread publishing ready would race completion and bypass this contract.
  close_task(task);
  EXPECT_EQ(g_active_task_handles, 0);
}

TEST(StyioExternLibInternal, ReadyTaskPullsStaySettledWithoutEnqueueOrRerun) {
  styio_task_scheduler_profile_reset();
  styio_task_scheduler_profile_enable(1);

  StyioTaskSchedulerProfileSnapshot before{};
  styio_task_scheduler_profile_snapshot(&before);

  const int64_t i64_task = styio_task_i64_ready(7);
  const int64_t f64_task = styio_task_f64_ready(2.5);
  const int64_t cstr_task = styio_task_cstr_ready("ready-text");

  // Construction settles the result; nothing may be enqueued before a pull.
  StyioTaskSchedulerProfileSnapshot prepared{};
  styio_task_scheduler_profile_snapshot(&prepared);
  EXPECT_EQ(prepared.enqueued_tasks, before.enqueued_tasks);
  EXPECT_EQ(prepared.started_tasks, before.started_tasks);

  EXPECT_EQ(styio_task_i64_pull(i64_task), 7);
  EXPECT_DOUBLE_EQ(styio_task_f64_pull(f64_task), 2.5);
  EXPECT_EQ(take_owned_cstr(styio_task_cstr_pull(cstr_task)), "ready-text");

  // Pulling a settled task consumes the retained result without launching a
  // worker; no enqueue, start, completion or queue pop may follow the pull.
  StyioTaskSchedulerProfileSnapshot after{};
  styio_task_scheduler_profile_snapshot(&after);
  EXPECT_EQ(after.enqueued_tasks, prepared.enqueued_tasks);
  EXPECT_EQ(after.started_tasks, prepared.started_tasks);
  EXPECT_EQ(after.completed_tasks, prepared.completed_tasks);
  EXPECT_EQ(after.queue_pops, prepared.queue_pops);
  EXPECT_EQ(after.fast_ready_pulls, prepared.fast_ready_pulls + 3);
  EXPECT_FALSE(styio::runtime::has_error());

  // A second pull stays rejected and never falls back to running the task.
  EXPECT_EQ(styio_task_i64_pull(i64_task), 0);
  EXPECT_EQ(styio_runtime_error_matches_effect("closed"), 1);
  styio_runtime_clear_error();
  StyioTaskSchedulerProfileSnapshot rejected{};
  styio_task_scheduler_profile_snapshot(&rejected);
  EXPECT_EQ(rejected.enqueued_tasks, prepared.enqueued_tasks);
  EXPECT_EQ(rejected.started_tasks, prepared.started_tasks);

  styio_task_scheduler_profile_enable(0);
  styio_task_release(i64_task);
  styio_task_release(f64_task);
  styio_task_release(cstr_task);
}

namespace {

struct NestedWaitChildSpec {
  int64_t value = 0;
  bool raise = false;
};

int64_t
run_nested_wait_child(void* raw) {
  auto* spec = static_cast<NestedWaitChildSpec*>(raw);
  if (spec->raise) {
    /* Provoke a real runtime error so the task settles as failed. */
    (void)styio_task_i64_pull(0);
    return 0;
  }
  return spec->value;
}

struct NestedWaitParentSpec {
  int64_t child_value = 0;
  bool child_raises = false;
  bool child_held = false;
  int64_t predecessor_value = 0;
  bool release_without_pull = false;
  int64_t parent_value = 0;
};

/* Mirrors compiled task creation inside a task body: create the child on the
   executing thread, keep the handle thread-local, then pull or release it. */
int64_t
run_nested_wait_parent(void* raw) {
  auto* spec = static_cast<NestedWaitParentSpec*>(raw);
  auto* child = new StyioTask(StyioTaskValueKind::I64);
  (*child).i64_fn = run_nested_wait_child;
  (*child).ctx = new NestedWaitChildSpec{spec->child_value, spec->child_raises};
  const int64_t child_handle = stash_spawned_task(child, spec->child_held);
  int64_t predecessor_handle = 0;
  if (spec->child_held) {
    auto* predecessor = new StyioTask(StyioTaskValueKind::I64);
    (*predecessor).i64_fn = run_value_task;
    (*predecessor).ctx = new int64_t(spec->predecessor_value);
    predecessor_handle = stash_spawned_task(predecessor, true);
    styio_task_depend(predecessor_handle, child_handle);
  }
  int64_t pulled = spec->parent_value;
  if (spec->release_without_pull) {
    styio_task_release(child_handle);
  }
  else {
    pulled += styio_task_i64_pull(child_handle);
    styio_task_release(child_handle);
  }
  if (predecessor_handle != 0) {
    styio_task_release(predecessor_handle);
  }
  return pulled;
}

int64_t
drive_nested_wait_parent(const NestedWaitParentSpec& spec) {
  auto* parent = new StyioTask(StyioTaskValueKind::I64);
  (*parent).i64_fn = run_nested_wait_parent;
  (*parent).ctx = new NestedWaitParentSpec(spec);
  const int64_t handoff = stash_spawned_task(parent, false);
  const int64_t pulled = styio_task_i64_pull(handoff);
  styio_task_release(handoff);
  return pulled;
}

}  // namespace

TEST(StyioExternLibInternal, NestedTaskWaitsKeepSingleWorkerForwardProgress) {
  // The scheduler-shape test above starts the process-wide scheduler with
  // STYIO_TASK_THREADS=1 and a bounded queue, so this test runs exactly the
  // single-worker configuration the wait assist exists for: a task body that
  // waits on another task is the only executor, and without helping drain
  // dependency-satisfied work every shape below strands its child behind the
  // blocked worker and never returns.
  const int64_t main_handles_before = g_active_task_handles;
  styio_task_scheduler_profile_reset();
  styio_task_scheduler_profile_enable(1);

  StyioTaskSchedulerProfileSnapshot before{};
  styio_task_scheduler_profile_snapshot(&before);

  // 1. A task body creates and pulls a finite child while the queue has space.
  const int64_t pulled = drive_nested_wait_parent(NestedWaitParentSpec{.child_value = 42});
  EXPECT_EQ(pulled, 42);
  EXPECT_FALSE(styio::runtime::has_error());

  // 2. A held child behind a never-launched predecessor: the demand pull must
  //    launch the predecessor first, and both run before the child settles.
  const int64_t ordered = drive_nested_wait_parent(NestedWaitParentSpec{
    .child_value = 21, .child_held = true, .predecessor_value = 7, .parent_value = 1});
  EXPECT_EQ(ordered, 22);
  EXPECT_FALSE(styio::runtime::has_error());

  // 3. Scope release without a pull: releasing the unready child waits for it,
  //    and that wait must also keep the single worker moving.
  const int64_t released = drive_nested_wait_parent(NestedWaitParentSpec{
    .child_value = 13, .release_without_pull = true, .parent_value = 55});
  EXPECT_EQ(released, 55);
  EXPECT_FALSE(styio::runtime::has_error());

  // Every scenario spawns, enqueues, starts and completes its tasks exactly
  // once; the ordered scenario also runs its predecessor. Nothing reruns.
  StyioTaskSchedulerProfileSnapshot after{};
  styio_task_scheduler_profile_snapshot(&after);
  EXPECT_EQ(after.spawned_tasks - before.spawned_tasks, 7);
  EXPECT_EQ(after.enqueued_tasks - before.enqueued_tasks, 7);
  EXPECT_EQ(after.started_tasks - before.started_tasks, 7);
  EXPECT_EQ(after.completed_tasks - before.completed_tasks, 7);
  EXPECT_EQ(after.queue_pops - before.queue_pops, 7);
  const auto queue = StyioTaskScheduler::instance().ready_queue_snapshot();
  EXPECT_EQ(queue.current_depth, 0u);

  // 4. A failing child settles through the same wait, surfaces its error and
  //    leaves the scheduler balanced.
  const int64_t failed = drive_nested_wait_parent(NestedWaitParentSpec{.child_raises = true});
  EXPECT_EQ(failed, 0);
  EXPECT_TRUE(styio::runtime::has_error());
  styio_runtime_clear_error();

  StyioTaskSchedulerProfileSnapshot settled{};
  styio_task_scheduler_profile_snapshot(&settled);
  EXPECT_EQ(settled.started_tasks, settled.completed_tasks);
  EXPECT_EQ(
    StyioTaskScheduler::instance().ready_queue_snapshot().current_depth,
    0u);

  styio_task_scheduler_profile_enable(0);
  EXPECT_EQ(g_active_task_handles, main_handles_before);
}
