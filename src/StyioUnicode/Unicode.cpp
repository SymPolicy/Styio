#include "Unicode.hpp"

#include <cctype>

#if defined(STYIO_USE_ICU)
#include <unicode/uchar.h>
#endif

namespace {

inline unsigned char
as_uchar(char ch) noexcept {
  return static_cast<unsigned char>(ch);
}

inline bool
is_utf8_continuation(unsigned char ch) noexcept {
  return (ch & 0xC0U) == 0x80U;
}

} // namespace

namespace StyioUnicode {

Backend
backend() noexcept {
#if defined(STYIO_USE_ICU)
  return Backend::ICU;
#else
  return Backend::ASCII;
#endif
}

bool
is_space(char ch) noexcept {
  return std::isspace(as_uchar(ch)) != 0;
}

bool
is_digit(char ch) noexcept {
  return std::isdigit(as_uchar(ch)) != 0;
}

bool
is_ascii_alpha(char ch) noexcept {
  return std::isalpha(as_uchar(ch)) != 0;
}

bool
is_ascii_alnum(char ch) noexcept {
  return std::isalnum(as_uchar(ch)) != 0;
}

bool
decode_utf8_codepoint(
  std::string_view text,
  std::size_t offset,
  std::uint32_t& codepoint,
  std::size_t& width) noexcept {
  codepoint = 0;
  width = 0;

  if (offset >= text.size()) {
    return false;
  }

  const auto* bytes = reinterpret_cast<const unsigned char*>(text.data());
  const unsigned char b0 = bytes[offset];

  if (b0 <= 0x7FU) {
    codepoint = b0;
    width = 1;
    return true;
  }

  if ((b0 & 0xE0U) == 0xC0U) {
    if (offset + 1 >= text.size()) {
      return false;
    }

    const unsigned char b1 = bytes[offset + 1];
    if (!is_utf8_continuation(b1) || b0 < 0xC2U) {
      return false;
    }

    codepoint = ((static_cast<std::uint32_t>(b0 & 0x1FU)) << 6)
                | static_cast<std::uint32_t>(b1 & 0x3FU);
    width = 2;
    return true;
  }

  if ((b0 & 0xF0U) == 0xE0U) {
    if (offset + 2 >= text.size()) {
      return false;
    }

    const unsigned char b1 = bytes[offset + 1];
    const unsigned char b2 = bytes[offset + 2];
    if (!is_utf8_continuation(b1) || !is_utf8_continuation(b2)) {
      return false;
    }

    if ((b0 == 0xE0U && b1 < 0xA0U) || (b0 == 0xEDU && b1 >= 0xA0U)) {
      return false;
    }

    codepoint = ((static_cast<std::uint32_t>(b0 & 0x0FU)) << 12)
                | ((static_cast<std::uint32_t>(b1 & 0x3FU)) << 6)
                | static_cast<std::uint32_t>(b2 & 0x3FU);
    width = 3;
    return true;
  }

  if ((b0 & 0xF8U) == 0xF0U) {
    if (offset + 3 >= text.size()) {
      return false;
    }

    const unsigned char b1 = bytes[offset + 1];
    const unsigned char b2 = bytes[offset + 2];
    const unsigned char b3 = bytes[offset + 3];
    if (!is_utf8_continuation(b1)
        || !is_utf8_continuation(b2)
        || !is_utf8_continuation(b3)) {
      return false;
    }

    if (b0 < 0xF0U || b0 > 0xF4U) {
      return false;
    }

    if ((b0 == 0xF0U && b1 < 0x90U) || (b0 == 0xF4U && b1 > 0x8FU)) {
      return false;
    }

    codepoint = ((static_cast<std::uint32_t>(b0 & 0x07U)) << 18)
                | ((static_cast<std::uint32_t>(b1 & 0x3FU)) << 12)
                | ((static_cast<std::uint32_t>(b2 & 0x3FU)) << 6)
                | static_cast<std::uint32_t>(b3 & 0x3FU);
    width = 4;
    return true;
  }

  return false;
}

bool
is_identifier_start(std::uint32_t codepoint) noexcept {
#if defined(STYIO_USE_ICU)
  return u_hasBinaryProperty(static_cast<UChar32>(codepoint), UCHAR_XID_START) != 0;
#else
  if (codepoint > 0x7FU) {
    return false;
  }
  return is_identifier_start(static_cast<char>(codepoint));
#endif
}

bool
is_identifier_continue(std::uint32_t codepoint) noexcept {
#if defined(STYIO_USE_ICU)
  return u_hasBinaryProperty(static_cast<UChar32>(codepoint), UCHAR_XID_CONTINUE) != 0;
#else
  if (codepoint > 0x7FU) {
    return false;
  }
  return is_identifier_continue(static_cast<char>(codepoint));
#endif
}

bool
is_decimal_digit(std::uint32_t codepoint) noexcept {
#if defined(STYIO_USE_ICU)
  return u_isdigit(static_cast<UChar32>(codepoint)) != 0;
#else
  if (codepoint > 0x7FU) {
    return false;
  }
  return is_digit(static_cast<char>(codepoint));
#endif
}

} // namespace StyioUnicode
