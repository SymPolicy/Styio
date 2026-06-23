#pragma once
#ifndef STYIO_RUNTIME_RUNTIME_STATE_HPP_
#define STYIO_RUNTIME_RUNTIME_STATE_HPP_

#include <string>

using StyioRuntimeLogSink = void (*)(const char* stream, const char* message);

namespace styio::runtime {

struct RuntimeErrorSnapshot
{
  bool has_error = false;
  std::string message;
  std::string subcode;
};

void clear_error_state();
void set_error_once(const char* subcode, const std::string& message);
bool has_error();
const char* last_error();
const char* last_error_subcode();
bool error_matches_effect(const char* effect_name);
RuntimeErrorSnapshot take_error();

void set_log_sink(StyioRuntimeLogSink sink);
void log_to_sink(const char* stream, const char* message);

} // namespace styio::runtime

#endif // STYIO_RUNTIME_RUNTIME_STATE_HPP_
