#pragma once
#ifndef STYIO_EFFECT_ROW_HPP_
#define STYIO_EFFECT_ROW_HPP_

#include <algorithm>
#include <cstdint>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace styio::sema {

enum class CallableEffectLabel : std::uint8_t {
  Capture,
  Handler,
  Native,
  Output,
  Resource,
  Task,
  Unknown,
};

constexpr std::string_view
callable_effect_label_name(CallableEffectLabel label) {
  switch (label) {
    case CallableEffectLabel::Capture:
      return "capture";
    case CallableEffectLabel::Handler:
      return "handler";
    case CallableEffectLabel::Native:
      return "native";
    case CallableEffectLabel::Output:
      return "output";
    case CallableEffectLabel::Resource:
      return "resource";
    case CallableEffectLabel::Task:
      return "task";
    case CallableEffectLabel::Unknown:
      return "unknown";
  }
  return "unknown";
}

inline std::optional<CallableEffectLabel>
callable_effect_label_from_name(std::string_view name) {
  if (name == "capture") {
    return CallableEffectLabel::Capture;
  }
  if (name == "handler") {
    return CallableEffectLabel::Handler;
  }
  if (name == "native") {
    return CallableEffectLabel::Native;
  }
  if (name == "output") {
    return CallableEffectLabel::Output;
  }
  if (name == "resource") {
    return CallableEffectLabel::Resource;
  }
  if (name == "task") {
    return CallableEffectLabel::Task;
  }
  if (name == "unknown") {
    return CallableEffectLabel::Unknown;
  }
  return std::nullopt;
}

class CallableEffectRow
{
public:
  static CallableEffectRow unknown() {
    CallableEffectRow row;
    row.add(CallableEffectLabel::Unknown);
    return row;
  }

  bool add(CallableEffectLabel label) {
    const auto position =
      std::lower_bound(labels_.begin(), labels_.end(), label);
    if (position != labels_.end() && *position == label) {
      return false;
    }
    labels_.insert(position, label);
    return true;
  }

  bool merge_known(const CallableEffectRow& other) {
    bool changed = false;
    for (const auto label : other.labels_) {
      changed = add(label) || changed;
    }
    return changed;
  }

  bool set_open_tail(std::uint32_t tail) {
    if (open_tail_ == tail) {
      return false;
    }
    open_tail_ = tail;
    return true;
  }

  bool contains(CallableEffectLabel label) const {
    return std::binary_search(labels_.begin(), labels_.end(), label);
  }

  bool is_closed() const {
    return !open_tail_.has_value();
  }

  bool proven_pure() const {
    return labels_.empty() && is_closed();
  }

  const std::vector<CallableEffectLabel>& labels() const {
    return labels_;
  }

  const std::optional<std::uint32_t>& open_tail() const {
    return open_tail_;
  }

  std::string canonical() const {
    std::ostringstream output;
    output << "{";
    for (std::size_t i = 0; i < labels_.size(); ++i) {
      if (i != 0) {
        output << ",";
      }
      output << callable_effect_label_name(labels_[i]);
    }
    if (open_tail_.has_value()) {
      output << "|'e" << *open_tail_;
    }
    output << "}";
    return output.str();
  }

  friend bool operator==(
    const CallableEffectRow& lhs,
    const CallableEffectRow& rhs
  ) = default;

private:
  std::vector<CallableEffectLabel> labels_;
  std::optional<std::uint32_t> open_tail_;
};

}  // namespace styio::sema

#endif  // STYIO_EFFECT_ROW_HPP_
