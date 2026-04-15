#pragma once
#ifndef STYIO_SESSION_ALLOCATION_HPP_
#define STYIO_SESSION_ALLOCATION_HPP_

#include <cstddef>
#include <new>
#include <utility>
#include <vector>

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

} // namespace styio::session_alloc

#endif // STYIO_SESSION_ALLOCATION_HPP_
