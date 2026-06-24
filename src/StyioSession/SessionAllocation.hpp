#pragma once
#ifndef STYIO_SESSION_ALLOCATION_HPP_
#define STYIO_SESSION_ALLOCATION_HPP_

#include <cstddef>
#include <new>
#include <utility>
#include <vector>

namespace styio::session { class SymbolInterner; }

namespace styio::session_alloc {

class SessionArena
{
  struct Block
  {
    std::byte* data = nullptr;
    std::size_t capacity = 0;
    std::size_t used = 0;
  };

  static constexpr std::size_t kAlignment = alignof(std::max_align_t);

  std::vector<Block> blocks_;
  std::size_t default_block_bytes_ = 64u * 1024u;
  std::size_t bytes_used_ = 0;

  static constexpr std::size_t
  round_up(std::size_t value) noexcept {
    return (value + (kAlignment - 1)) & ~(kAlignment - 1);
  }

  Block&
  ensure_block(std::size_t span_bytes) {
    if (!blocks_.empty()) {
      Block& tail = blocks_.back();
      if (tail.capacity - tail.used >= span_bytes) {
        return tail;
      }
    }

    const std::size_t capacity =
      round_up(span_bytes > default_block_bytes_ ? span_bytes : default_block_bytes_);
    Block block;
    block.data = static_cast<std::byte*>(::operator new(capacity));
    block.capacity = capacity;
    block.used = 0;
    blocks_.push_back(block);
    return blocks_.back();
  }

public:
  explicit SessionArena(std::size_t default_block_bytes = 64u * 1024u) :
      default_block_bytes_(round_up(default_block_bytes)) {
  }

  SessionArena(const SessionArena&) = delete;
  SessionArena& operator=(const SessionArena&) = delete;

  SessionArena(SessionArena&& other) noexcept :
      blocks_(std::move(other.blocks_)),
      default_block_bytes_(other.default_block_bytes_),
      bytes_used_(other.bytes_used_) {
    other.bytes_used_ = 0;
  }

  SessionArena&
  operator=(SessionArena&& other) noexcept {
    if (this == &other) {
      return *this;
    }
    release();
    blocks_ = std::move(other.blocks_);
    default_block_bytes_ = other.default_block_bytes_;
    bytes_used_ = other.bytes_used_;
    other.bytes_used_ = 0;
    return *this;
  }

  ~SessionArena() {
    release();
  }

  void*
  allocate_span(std::size_t span_bytes) {
    const std::size_t aligned_span = round_up(span_bytes);
    Block& block = ensure_block(aligned_span);
    std::byte* mem = block.data + block.used;
    block.used += aligned_span;
    bytes_used_ += aligned_span;
    return mem;
  }

  void
  release() noexcept {
    for (auto& block : blocks_) {
      ::operator delete(block.data);
      block.data = nullptr;
      block.capacity = 0;
      block.used = 0;
    }
    blocks_.clear();
    bytes_used_ = 0;
  }

  std::size_t
  bytes_used() const noexcept {
    return bytes_used_;
  }
};

struct alignas(std::max_align_t) AllocationHeader
{
  SessionArena* arena = nullptr;
  std::size_t span_bytes = 0;
};

inline thread_local SessionArena* current_ast_arena = nullptr;
inline thread_local SessionArena* current_token_arena = nullptr;

inline SessionArena*
set_current_ast_arena(SessionArena* arena) noexcept {
  SessionArena* previous = current_ast_arena;
  current_ast_arena = arena;
  return previous;
}

inline SessionArena*
set_current_token_arena(SessionArena* arena) noexcept {
  SessionArena* previous = current_token_arena;
  current_token_arena = arena;
  return previous;
}

inline bool
ast_arena_active() noexcept {
  return current_ast_arena != nullptr;
}

inline bool
token_arena_active() noexcept {
  return current_token_arena != nullptr;
}

inline void*
allocate_object(SessionArena* arena, std::size_t object_size) {
  const std::size_t span_bytes = sizeof(AllocationHeader) + object_size;
  std::byte* raw = arena != nullptr
    ? static_cast<std::byte*>(arena->allocate_span(span_bytes))
    : static_cast<std::byte*>(::operator new(span_bytes));
  auto* header = reinterpret_cast<AllocationHeader*>(raw);
  header->arena = arena;
  header->span_bytes = span_bytes;
  return raw + sizeof(AllocationHeader);
}

inline void
free_object(void* ptr) noexcept {
  if (ptr == nullptr) {
    return;
  }
  std::byte* raw = static_cast<std::byte*>(ptr) - sizeof(AllocationHeader);
  auto* header = reinterpret_cast<AllocationHeader*>(raw);
  if (header->arena == nullptr) {
    ::operator delete(raw);
  }
}

inline void*
allocate_ast_object(std::size_t object_size) {
  return allocate_object(current_ast_arena, object_size);
}

inline void*
allocate_token_object(std::size_t object_size) {
  return allocate_object(current_token_arena, object_size);
}

// Thread-local symbol interner (mirrors arena pattern for session-scoped access).
class SymbolInterner;
inline thread_local SymbolInterner* current_interner = nullptr;

inline SymbolInterner*
set_current_interner(SymbolInterner* interner) noexcept {
  SymbolInterner* prev = current_interner;
  current_interner = interner;
  return prev;
}

/// Allocation statistics for measuring arena vs heap allocation behaviour.
struct SessionAllocationStats
{
  std::size_t arena_allocations = 0;   // nodes placed via arena (make_ir with active arena)
  std::size_t raw_allocations = 0;     // nodes allocated via raw ::new (make_ir fallback or explicit new)
  std::size_t bytes_allocated = 0;     // total bytes requested (sum of sizeof(T) across all allocations)
  std::size_t node_count = 0;          // currently alive nodes
  std::size_t max_node_count = 0;      // peak concurrent node_count
  std::size_t destructor_calls = 0;    // total StyioIR destructor invocations

  void reset() noexcept { *this = SessionAllocationStats{}; }
};

// Thread-local IR arena (TASK-08) and statistics.
class StyioIR;
inline thread_local SessionArena* current_ir_arena = nullptr;
inline thread_local SessionAllocationStats* current_ir_stats = nullptr;

inline SessionArena*
set_current_ir_arena(SessionArena* arena) noexcept {
  SessionArena* prev = current_ir_arena;
  current_ir_arena = arena;
  return prev;
}

inline SessionAllocationStats*
set_current_ir_stats(SessionAllocationStats* stats) noexcept {
  SessionAllocationStats* prev = current_ir_stats;
  current_ir_stats = stats;
  return prev;
}

inline bool
ir_stats_active() noexcept {
  return current_ir_stats != nullptr;
}

/// Helper: record a raw heap allocation for tracking purposes.
/// Returns the pointer unchanged so it can be used inline.
template <typename T>
T*
track_raw_allocation(T* ptr) noexcept {
  if (current_ir_stats) {
    current_ir_stats->raw_allocations++;
    current_ir_stats->bytes_allocated += sizeof(T);
    current_ir_stats->node_count++;
    if (current_ir_stats->node_count > current_ir_stats->max_node_count) {
      current_ir_stats->max_node_count = current_ir_stats->node_count;
    }
  }
  return ptr;
}

/// Allocate a raw IR node via ::new T() and track it.
template <typename T>
T*
raw_new() {
  return track_raw_allocation(::new T());
}

/// Placement-new factory for IR nodes. Uses the thread-local IR arena when
/// active, falls back to ::operator new otherwise (backward compatible).
/// Increments allocation counters via thread-local SessionAllocationStats.
template <typename T, typename... Args>
T* make_ir(Args&&... args) {
  void* mem = nullptr;
  if (current_ir_arena) {
    mem = current_ir_arena->allocate_span(sizeof(T));
    if (current_ir_stats) {
      current_ir_stats->arena_allocations++;
      current_ir_stats->bytes_allocated += sizeof(T);
    }
  } else {
    mem = ::operator new(sizeof(T));
    if (current_ir_stats) {
      current_ir_stats->raw_allocations++;
      current_ir_stats->bytes_allocated += sizeof(T);
    }
  }
  if (current_ir_stats) {
    current_ir_stats->node_count++;
    if (current_ir_stats->node_count > current_ir_stats->max_node_count) {
      current_ir_stats->max_node_count = current_ir_stats->node_count;
    }
  }
  return ::new (mem) T(std::forward<Args>(args)...);
}

/// Non-recursive IR subtree destruction. Calls destructors on all reachable
/// nodes via collect_children(), then (if no arena active) frees memory.
/// When an IR arena is active, individual deallocations are no-ops;
/// the arena reset handles bulk cleanup.
void destroy_ir_subtree(StyioIR* root);

} // namespace styio::session_alloc

#endif // STYIO_SESSION_ALLOCATION_HPP_
