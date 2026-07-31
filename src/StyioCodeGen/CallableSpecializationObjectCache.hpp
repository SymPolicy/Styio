#pragma once

#ifndef STYIO_CALLABLE_SPECIALIZATION_OBJECT_CACHE_HPP
#define STYIO_CALLABLE_SPECIALIZATION_OBJECT_CACHE_HPP

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>

#include "llvm/ExecutionEngine/ObjectCache.h"

namespace llvm {
class MemoryBuffer;
class Module;
}  // namespace llvm

namespace styio::codegen {

struct CallableSpecializationCacheLimits
{
  std::uint64_t max_age_seconds = 7 * 24 * 60 * 60;
  std::uint64_t max_bytes = 256 * 1024 * 1024;
  std::uint64_t max_files = 4096;
};

struct CallableSpecializationCacheConfig
{
  std::filesystem::path root;
  std::string namespace_abi;
  std::string target_triple;
  CallableSpecializationCacheLimits limits;
};

struct CallableSpecializationCacheStats
{
  std::uint64_t lookups = 0;
  std::uint64_t hits = 0;
  std::uint64_t misses = 0;
  std::uint64_t corruptions = 0;
  std::uint64_t writes = 0;
  std::uint64_t evictions = 0;
  std::uint64_t io_failures = 0;
  std::uint64_t hashing_ns = 0;
  std::uint64_t lookup_ns = 0;
  std::uint64_t verification_ns = 0;
  std::uint64_t materialization_ns = 0;
};

std::string callable_specialization_cache_namespace_digest(
  std::string_view namespace_abi
);

class CallableSpecializationObjectCache final : public llvm::ObjectCache
{
public:
  struct Descriptor
  {
    std::string content_digest;
    std::string symbol;
  };

  explicit CallableSpecializationObjectCache(
    CallableSpecializationCacheConfig config
  );

  void notifyObjectCompiled(
    const llvm::Module* module,
    llvm::MemoryBufferRef object
  ) override;

  std::unique_ptr<llvm::MemoryBuffer> getObject(
    const llvm::Module* module
  ) override;

  bool enabled() const {
    return enabled_;
  }

  CallableSpecializationCacheStats stats() const;
  std::string stats_json() const;

private:
  std::optional<Descriptor> descriptor_for(
    const llvm::Module* module
  ) const;

  bool ensure_namespace_directory();
  bool prune_once();
  void prune_locked();
  std::filesystem::path entry_path(
    std::string_view content_digest
  ) const;

  CallableSpecializationCacheConfig config_;
  std::string namespace_digest_;
  std::filesystem::path namespace_directory_;
  bool enabled_ = false;
  bool prune_attempted_ = false;
  bool namespace_ready_ = false;
  std::uint64_t retained_bytes_ = 0;
  std::uint64_t retained_files_ = 0;
  mutable std::mutex filesystem_mutex_;

  std::atomic<std::uint64_t> lookups_{0};
  std::atomic<std::uint64_t> hits_{0};
  std::atomic<std::uint64_t> misses_{0};
  std::atomic<std::uint64_t> corruptions_{0};
  std::atomic<std::uint64_t> writes_{0};
  std::atomic<std::uint64_t> evictions_{0};
  std::atomic<std::uint64_t> io_failures_{0};
  std::atomic<std::uint64_t> hashing_ns_{0};
  std::atomic<std::uint64_t> lookup_ns_{0};
  std::atomic<std::uint64_t> verification_ns_{0};
  std::atomic<std::uint64_t> materialization_ns_{0};
};

}  // namespace styio::codegen

#endif  // STYIO_CALLABLE_SPECIALIZATION_OBJECT_CACHE_HPP
