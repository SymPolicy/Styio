#pragma once
#ifndef STYIO_BOUNDED_TYPE_H_
#define STYIO_BOUNDED_TYPE_H_

// Shared parsing for Topology v2 bounded buffer spellings on StyioDataType (see TypeAST::CreateBoundedRingBuffer).

#include "../StyioToken/Token.hpp"

#include <cstdint>
#include <optional>
#include <string>

inline constexpr const char* kStyioBoundedRingPrefix = "bounded_ring:";

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
    unsigned long long v = std::stoull(n.substr(p.size()), &pos, 10);
    if (pos == 0 || v == 0) {
      return std::nullopt;
    }
    return static_cast<std::uint64_t>(v);
  } catch (...) {
    return std::nullopt;
  }
}

#endif
