#include "StyioRuntime/RuntimeState.hpp"

#include <cstring>

namespace {

thread_local bool g_runtime_error = false;
thread_local std::string g_runtime_error_message;
thread_local std::string g_runtime_error_subcode;
thread_local StyioRuntimeLogSink g_runtime_log_sink = nullptr;

bool
runtime_subcode_matches_effect_family(const char* subcode, const char* effect_name) {
  if (subcode == nullptr || effect_name == nullptr) {
    return false;
  }
  if (std::strcmp(effect_name, "io") == 0) {
    return std::strcmp(subcode, "STYIO_RUNTIME_FILE_PATH_NULL") == 0
           || std::strcmp(subcode, "STYIO_RUNTIME_FILE_OPEN_READ") == 0
           || std::strcmp(subcode, "STYIO_RUNTIME_FILE_OPEN_WRITE") == 0;
  }
  if (std::strcmp(effect_name, "parse") == 0) {
    return std::strcmp(subcode, "STYIO_RUNTIME_NUMERIC_PARSE") == 0
           || std::strcmp(subcode, "STYIO_RUNTIME_LIST_PARSE") == 0;
  }
  if (std::strcmp(effect_name, "bounds") == 0) {
    return std::strcmp(subcode, "STYIO_RUNTIME_LIST_INDEX") == 0
           || std::strcmp(subcode, "STYIO_RUNTIME_DICT_KEY") == 0
           || std::strcmp(subcode, "STYIO_RUNTIME_MATRIX_INDEX") == 0;
  }
  if (std::strcmp(effect_name, "closed") == 0) {
    return std::strcmp(subcode, "STYIO_RUNTIME_INVALID_FILE_HANDLE") == 0
           || std::strcmp(subcode, "STYIO_RUNTIME_INVALID_LIST_HANDLE") == 0
           || std::strcmp(subcode, "STYIO_RUNTIME_INVALID_DICT_HANDLE") == 0
           || std::strcmp(subcode, "STYIO_RUNTIME_INVALID_MATRIX_HANDLE") == 0
           || std::strcmp(subcode, "STYIO_RUNTIME_INVALID_TASK_HANDLE") == 0
           || std::strcmp(subcode, "STYIO_RUNTIME_TASK_CONSUMED") == 0;
  }
  if (std::strcmp(effect_name, "cleanup") == 0) {
    return std::strcmp(subcode, "STYIO_RUNTIME_FILE_CLEANUP_FAILURE") == 0;
  }
  return false;
}

} // namespace

namespace styio::runtime {

void
clear_error_state() {
  g_runtime_error = false;
  if (!g_runtime_error_message.empty()) {
    g_runtime_error_message.clear();
  }
  if (!g_runtime_error_subcode.empty()) {
    g_runtime_error_subcode.clear();
  }
}

void
set_error_once(const char* subcode, const std::string& message) {
  g_runtime_error = true;
  if (g_runtime_error_message.empty()) {
    g_runtime_error_message = message;
    g_runtime_error_subcode = (subcode != nullptr) ? subcode : "";
  }
}

bool
has_error() {
  return g_runtime_error;
}

const char*
last_error() {
  if (!g_runtime_error || g_runtime_error_message.empty()) {
    return nullptr;
  }
  return g_runtime_error_message.c_str();
}

const char*
last_error_subcode() {
  if (!g_runtime_error || g_runtime_error_subcode.empty()) {
    return nullptr;
  }
  return g_runtime_error_subcode.c_str();
}

bool
error_matches_effect(const char* effect_name) {
  if (!g_runtime_error || g_runtime_error_subcode.empty()) {
    return false;
  }
  return runtime_subcode_matches_effect_family(g_runtime_error_subcode.c_str(), effect_name);
}

RuntimeErrorSnapshot
take_error() {
  RuntimeErrorSnapshot snapshot;
  snapshot.has_error = g_runtime_error;
  if (g_runtime_error) {
    snapshot.message = !g_runtime_error_message.empty()
      ? g_runtime_error_message
      : std::string("runtime error");
    snapshot.subcode = g_runtime_error_subcode;
  }
  clear_error_state();
  return snapshot;
}

void
set_log_sink(StyioRuntimeLogSink sink) {
  g_runtime_log_sink = sink;
}

void
log_to_sink(const char* stream, const char* message) {
  if (g_runtime_log_sink != nullptr) {
    g_runtime_log_sink(stream, message);
  }
}

} // namespace styio::runtime
