#pragma once
#ifndef STYIO_RUNTIME_HANDLE_TABLE_HPP_
#define STYIO_RUNTIME_HANDLE_TABLE_HPP_

#include <cstddef>
#include <cstdint>
#include <unordered_map>

/**
 * Checkpoint C.1 shell.
 * Runtime C ABI migration will progressively route file/resource handles via this table.
 */
class StyioHandleTable
{
public:
  using HandleId = int64_t;

  enum class HandleKind
  {
    File = 0,
    Unknown = 1,
  };

  struct Entry
  {
    HandleKind kind = HandleKind::Unknown;
    void* ptr = nullptr;
    bool valid = false;
  };

private:
  std::unordered_map<HandleId, Entry> entries_;
  HandleId next_handle_ = 1;

public:
  HandleId reserve_stub(HandleKind kind) {
    const HandleId id = next_handle_++;
    entries_[id] = Entry{kind, nullptr, false};
    return id;
  }

  bool contains(HandleId id) const {
    return entries_.find(id) != entries_.end();
  }

  void invalidate(HandleId id) {
    auto it = entries_.find(id);
    if (it != entries_.end()) {
      it->second.valid = false;
      it->second.ptr = nullptr;
    }
  }

  size_t size() const {
    return entries_.size();
  }
};

#endif // STYIO_RUNTIME_HANDLE_TABLE_HPP_
