#pragma once
#ifndef STYIO_UNICODE_H_
#define STYIO_UNICODE_H_

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace StyioUnicode {

enum class Backend {
  ASCII,
  ICU,
};

Backend
backend() noexcept;

bool
is_space(char ch) noexcept;

bool
is_digit(char ch) noexcept;

bool
is_ascii_alpha(char ch) noexcept;

bool
is_ascii_alnum(char ch) noexcept;

inline bool
is_identifier_start(char ch) noexcept {
  return is_ascii_alpha(ch) || ch == '_';
}

inline bool
is_identifier_continue(char ch) noexcept {
  return is_ascii_alnum(ch) || ch == '_';
}

bool
decode_utf8_codepoint(
  std::string_view text,
  std::size_t offset,
  std::uint32_t& codepoint,
  std::size_t& width) noexcept;

bool
is_identifier_start(std::uint32_t codepoint) noexcept;

bool
is_identifier_continue(std::uint32_t codepoint) noexcept;

bool
is_decimal_digit(std::uint32_t codepoint) noexcept;

} // namespace StyioUnicode

#endif
