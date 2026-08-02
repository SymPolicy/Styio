#pragma once
#ifndef STYIO_CALLABLE_USAGE_HPP_
#define STYIO_CALLABLE_USAGE_HPP_

#include <algorithm>
#include <cstdint>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace styio::sema {

enum class CallableUsageKind : std::uint8_t {
  Consume,
  Copy,
  ExclusiveBorrow,
  SharedBorrow,
};

constexpr std::string_view
callable_usage_kind_name(CallableUsageKind kind) {
  switch (kind) {
    case CallableUsageKind::Consume:
      return "consume";
    case CallableUsageKind::Copy:
      return "copy";
    case CallableUsageKind::ExclusiveBorrow:
      return "exclusive_borrow";
    case CallableUsageKind::SharedBorrow:
      return "shared_borrow";
  }
  return "unknown";
}

inline bool
callable_usage_kind_from_name(
  std::string_view name,
  CallableUsageKind& output
) {
  if (name == "consume") {
    output = CallableUsageKind::Consume;
    return true;
  }
  if (name == "copy") {
    output = CallableUsageKind::Copy;
    return true;
  }
  if (name == "exclusive_borrow") {
    output = CallableUsageKind::ExclusiveBorrow;
    return true;
  }
  if (name == "shared_borrow") {
    output = CallableUsageKind::SharedBorrow;
    return true;
  }
  return false;
}

class CallableUsageSet
{
public:
  bool add(CallableUsageKind kind) {
    const auto position =
      std::lower_bound(kinds_.begin(), kinds_.end(), kind);
    if (position != kinds_.end() && *position == kind) {
      return false;
    }
    kinds_.insert(position, kind);
    return true;
  }

  bool merge(const CallableUsageSet& other) {
    bool changed = false;
    for (const auto kind : other.kinds_) {
      changed = add(kind) || changed;
    }
    return changed;
  }

  bool contains(CallableUsageKind kind) const {
    return std::binary_search(kinds_.begin(), kinds_.end(), kind);
  }

  bool empty() const {
    return kinds_.empty();
  }

  const std::vector<CallableUsageKind>& kinds() const {
    return kinds_;
  }

  std::string canonical() const {
    std::ostringstream output;
    output << "{";
    for (std::size_t i = 0; i < kinds_.size(); ++i) {
      if (i != 0) {
        output << ",";
      }
      output << callable_usage_kind_name(kinds_[i]);
    }
    output << "}";
    return output.str();
  }

  friend bool operator==(
    const CallableUsageSet& lhs,
    const CallableUsageSet& rhs
  ) = default;

private:
  std::vector<CallableUsageKind> kinds_;
};

}  // namespace styio::sema

#endif  // STYIO_CALLABLE_USAGE_HPP_
