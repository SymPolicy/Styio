#pragma once
#ifndef STYIO_IR_EFFECT_KIND_HPP_
#define STYIO_IR_EFFECT_KIND_HPP_

#include <cstdint>

namespace styio::ir {

/// Side-effect classification for IR nodes.
/// Used by optimization passes to determine what can be elided/reordered.
enum class EffectKind : std::uint8_t {
  Pure,            // No side effects — safe to fold, CSE, or eliminate
  ReadsResource,   // Reads from a resource — can't eliminate, can reorder w.r.t other reads
  WritesResource,  // Writes to a resource — can't eliminate or reorder
  IO,              // External I/O — can't eliminate, reorder, or duplicate
  NativeExtern,    // FFI call — can't eliminate
  Task,            // Task/spawn operation
  ControlFlow,     // Branch/merge/loop — structural
  Unknown,         // Conservative: assume all side effects possible
};

} // namespace styio::ir

#endif // STYIO_IR_EFFECT_KIND_HPP_
