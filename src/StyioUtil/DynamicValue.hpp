#pragma once
#ifndef STYIO_DYNAMIC_VALUE_H_
#define STYIO_DYNAMIC_VALUE_H_

#include <cstdint>

enum class StyioDynamicTag : std::int64_t
{
  Undef = 0,
  Bool = 1,
  I64 = 2,
  F64 = 3,
  CStr = 4,
  List = 5,
  Dict = 6,
  Matrix = 7,
  Task = 8,
};

inline constexpr std::int64_t
styio_dynamic_tag_value(StyioDynamicTag tag) {
  return static_cast<std::int64_t>(tag);
}

inline constexpr bool
styio_dynamic_tag_is_owned_resource(StyioDynamicTag tag) {
  return tag == StyioDynamicTag::List
    || tag == StyioDynamicTag::Dict
    || tag == StyioDynamicTag::Matrix
    || tag == StyioDynamicTag::Task;
}

inline constexpr bool
styio_dynamic_tag_is_owned_resource(std::int64_t tag) {
  return tag == styio_dynamic_tag_value(StyioDynamicTag::List)
    || tag == styio_dynamic_tag_value(StyioDynamicTag::Dict)
    || tag == styio_dynamic_tag_value(StyioDynamicTag::Matrix)
    || tag == styio_dynamic_tag_value(StyioDynamicTag::Task);
}

#endif
