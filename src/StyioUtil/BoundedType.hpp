#pragma once
#ifndef STYIO_BOUNDED_TYPE_H_
#define STYIO_BOUNDED_TYPE_H_

// Shared parsing for Topology v2 bounded buffer spellings on StyioDataType (see TypeAST::CreateBoundedRingBuffer).

#include "../StyioToken/Token.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

inline constexpr const char* kStyioBoundedRingPrefix = "bounded_ring:";

inline std::optional<std::string>
styio_bounded_ring_value_type_name(const StyioDataType& dt) {
  if (dt.option != StyioDataTypeOption::Defined) {
    return std::nullopt;
  }
  const std::string& n = dt.name;
  if (!n.starts_with(kStyioBoundedRingPrefix)) {
    return std::nullopt;
  }
  const std::size_t prefix_len = std::string_view(kStyioBoundedRingPrefix).size();
  const std::string rest = n.substr(prefix_len);
  const std::size_t sep = rest.find(':');
  if (sep == std::string::npos) {
    return std::string("i64");
  }
  if (sep == 0) {
    return std::nullopt;
  }
  return rest.substr(0, sep);
}

inline std::optional<std::uint64_t>
styio_bounded_ring_capacity(const StyioDataType& dt) {
  if (dt.option != StyioDataTypeOption::Defined) {
    return std::nullopt;
  }
  const std::string& n = dt.name;
  if (!n.starts_with(kStyioBoundedRingPrefix)) {
    return std::nullopt;
  }
  try {
    std::size_t pos = 0;
    const std::size_t prefix_len = std::string_view(kStyioBoundedRingPrefix).size();
    const std::string rest = n.substr(prefix_len);
    const std::size_t sep = rest.find(':');
    const std::string digits = sep == std::string::npos
      ? rest
      : rest.substr(sep + 1);
    unsigned long long v = std::stoull(digits, &pos, 10);
    if (pos == 0 || pos != digits.size() || v == 0) {
      return std::nullopt;
    }
    return static_cast<std::uint64_t>(v);
  } catch (...) {
    return std::nullopt;
  }
}

#endif
