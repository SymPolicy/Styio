#include "CallableSpecializationObjectCache.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstring>
#include <limits>
#include <system_error>
#include <utility>
#include <vector>

#if !defined(_WIN32)
#include <unistd.h>
#endif

#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/IR/Metadata.h"
#include "llvm/IR/Module.h"
#include "llvm/Object/ObjectFile.h"
#include "llvm/Support/Error.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/SHA256.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/TargetParser/Triple.h"

namespace styio::codegen {
namespace {

using Clock = std::chrono::steady_clock;

constexpr std::array<char, 8> kMagic = {
  'S', 'T', 'Y', 'I', 'O', 'C', 'O', '1'
};
constexpr std::uint32_t kSchemaVersion = 1;
constexpr std::size_t kDigestLength = 64;
constexpr std::size_t kFixedHeaderSize =
  kMagic.size()
  + sizeof(std::uint32_t)
  + sizeof(std::uint32_t)
  + sizeof(std::uint64_t)
  + 3 * kDigestLength;
constexpr std::size_t kMaxSymbolLength = 1024;
constexpr std::uint64_t kMaxArtifactBytes =
  256ULL * 1024ULL * 1024ULL;
constexpr std::uint64_t kMaxEntryBytes =
  kMaxArtifactBytes + kFixedHeaderSize + kMaxSymbolLength;
constexpr llvm::StringLiteral kModuleMetadataName =
  "styio.callable.specialization.cache";

std::uint64_t
elapsed_ns(Clock::time_point started) {
  return static_cast<std::uint64_t>(
    std::chrono::duration_cast<std::chrono::nanoseconds>(
      Clock::now() - started).count());
}

bool
is_lower_hex_digest(std::string_view value) {
  return value.size() == kDigestLength
    && std::all_of(
      value.begin(),
      value.end(),
      [](unsigned char ch)
      {
        return (ch >= '0' && ch <= '9')
          || (ch >= 'a' && ch <= 'f');
      });
}

std::string
sha256_hex(llvm::StringRef value) {
  const auto bytes = llvm::ArrayRef<std::uint8_t>(
    reinterpret_cast<const std::uint8_t*>(value.data()),
    value.size());
  return llvm::toHex(llvm::SHA256::hash(bytes), true);
}

void
append_u32(std::string& output, std::uint32_t value) {
  for (unsigned shift = 0; shift != 32; shift += 8) {
    output.push_back(
      static_cast<char>((value >> shift) & 0xffU));
  }
}

void
append_u64(std::string& output, std::uint64_t value) {
  for (unsigned shift = 0; shift != 64; shift += 8) {
    output.push_back(
      static_cast<char>((value >> shift) & 0xffU));
  }
}

bool
read_u32(
  llvm::StringRef input,
  std::size_t offset,
  std::uint32_t& value
) {
  if (offset > input.size()
      || input.size() - offset < sizeof(std::uint32_t)) {
    return false;
  }
  value = 0;
  for (unsigned index = 0; index != 4; ++index) {
    value |= static_cast<std::uint32_t>(
      static_cast<unsigned char>(input[offset + index]))
      << (8 * index);
  }
  return true;
}

bool
read_u64(
  llvm::StringRef input,
  std::size_t offset,
  std::uint64_t& value
) {
  if (offset > input.size()
      || input.size() - offset < sizeof(std::uint64_t)) {
    return false;
  }
  value = 0;
  for (unsigned index = 0; index != 8; ++index) {
    value |= static_cast<std::uint64_t>(
      static_cast<unsigned char>(input[offset + index]))
      << (8 * index);
  }
  return true;
}

std::string
serialize_entry(
  const CallableSpecializationObjectCache::Descriptor& descriptor,
  std::string_view namespace_digest,
  llvm::MemoryBufferRef object,
  std::string_view object_digest
) {
  std::string output;
  output.reserve(
    kFixedHeaderSize
    + descriptor.symbol.size()
    + object.getBufferSize());
  output.append(kMagic.data(), kMagic.size());
  append_u32(output, kSchemaVersion);
  append_u32(
    output,
    static_cast<std::uint32_t>(descriptor.symbol.size()));
  append_u64(
    output,
    static_cast<std::uint64_t>(object.getBufferSize()));
  output.append(descriptor.content_digest);
  output.append(namespace_digest);
  output.append(object_digest);
  output.append(descriptor.symbol);
  output.append(
    object.getBufferStart(),
    object.getBufferSize());
  return output;
}

struct ParsedEntry
{
  llvm::StringRef object;
};

bool
parse_entry(
  llvm::StringRef input,
  const CallableSpecializationObjectCache::Descriptor& expected,
  std::string_view expected_namespace,
  ParsedEntry& parsed
) {
  if (input.size() < kFixedHeaderSize
      || !std::equal(
           kMagic.begin(),
           kMagic.end(),
           input.begin())) {
    return false;
  }

  std::uint32_t schema = 0;
  std::uint32_t symbol_size = 0;
  std::uint64_t object_size = 0;
  if (!read_u32(input, kMagic.size(), schema)
      || !read_u32(
           input,
           kMagic.size() + sizeof(std::uint32_t),
           symbol_size)
      || !read_u64(
           input,
           kMagic.size() + 2 * sizeof(std::uint32_t),
           object_size)
      || schema != kSchemaVersion
      || symbol_size == 0
      || symbol_size > kMaxSymbolLength
      || object_size == 0
      || object_size > kMaxArtifactBytes) {
    return false;
  }

  std::size_t offset =
    kMagic.size()
    + 2 * sizeof(std::uint32_t)
    + sizeof(std::uint64_t);
  const llvm::StringRef content_digest =
    input.substr(offset, kDigestLength);
  offset += kDigestLength;
  const llvm::StringRef namespace_digest =
    input.substr(offset, kDigestLength);
  offset += kDigestLength;
  const llvm::StringRef object_digest =
    input.substr(offset, kDigestLength);
  offset += kDigestLength;

  if (content_digest != expected.content_digest
      || namespace_digest
           != llvm::StringRef(
                expected_namespace.data(),
                expected_namespace.size())
      || !is_lower_hex_digest(object_digest.str())
      || input.size() - offset
           != static_cast<std::size_t>(symbol_size)
                + static_cast<std::size_t>(object_size)) {
    return false;
  }

  const llvm::StringRef symbol =
    input.substr(offset, symbol_size);
  offset += symbol_size;
  if (symbol != expected.symbol) {
    return false;
  }

  parsed.object =
    input.substr(offset, static_cast<std::size_t>(object_size));
  return sha256_hex(parsed.object) == object_digest;
}

bool
object_matches_target_and_symbol(
  llvm::StringRef object_bytes,
  std::string_view target_triple,
  std::string_view expected_symbol
) {
  auto buffer = llvm::MemoryBuffer::getMemBuffer(
    object_bytes,
    "styio-callable-cache-verify",
    false);
  auto object_or_error =
    llvm::object::ObjectFile::createObjectFile(
      buffer->getMemBufferRef());
  if (!object_or_error) {
    llvm::consumeError(object_or_error.takeError());
    return false;
  }

  const llvm::Triple triple{
    std::string(target_triple)
  };
  if (triple.getArch() != llvm::Triple::UnknownArch
      && (*object_or_error)->getArch() != triple.getArch()) {
    return false;
  }

  bool found_symbol = false;
  for (const llvm::object::SymbolRef& symbol
       : (*object_or_error)->symbols()) {
    auto name_or_error = symbol.getName();
    if (!name_or_error) {
      llvm::consumeError(name_or_error.takeError());
      return false;
    }
    auto flags_or_error = symbol.getFlags();
    if (!flags_or_error) {
      llvm::consumeError(flags_or_error.takeError());
      return false;
    }
    if ((*flags_or_error & llvm::object::SymbolRef::SF_Undefined)
        != 0) {
      continue;
    }
    llvm::StringRef name = *name_or_error;
    const llvm::StringRef expected(
      expected_symbol.data(),
      expected_symbol.size());
    if (name == expected
        || (name.starts_with("_")
            && name.drop_front() == expected)) {
      found_symbol = true;
      break;
    }
  }
  return found_symbol;
}

bool
safe_regular_cache_file(
  const std::filesystem::path& path,
  llvm::sys::fs::file_status& status
) {
  bool symlink = false;
  if (llvm::sys::fs::is_symlink_file(path.string(), symlink)
      || symlink
      || llvm::sys::fs::status(path.string(), status)
      || !llvm::sys::fs::is_regular_file(status)) {
    return false;
  }
#if !defined(_WIN32)
  if (status.getUser() != static_cast<std::uint32_t>(::getuid())) {
    return false;
  }
  const auto unsafe_write_bits =
    llvm::sys::fs::group_write
    | llvm::sys::fs::others_write;
  return (status.permissions() & unsafe_write_bits)
    == llvm::sys::fs::no_perms;
#else
  return true;
#endif
}

}  // namespace

std::string
callable_specialization_cache_namespace_digest(
  std::string_view namespace_abi
) {
  return sha256_hex(
    llvm::StringRef(namespace_abi.data(), namespace_abi.size()));
}

CallableSpecializationObjectCache::
CallableSpecializationObjectCache(
  CallableSpecializationCacheConfig config
) :
    config_(std::move(config)),
    namespace_digest_(
      callable_specialization_cache_namespace_digest(
        config_.namespace_abi)) {
  enabled_ =
    !config_.root.empty()
    && is_lower_hex_digest(namespace_digest_)
    && !config_.target_triple.empty()
    && config_.limits.max_age_seconds > 0
    && config_.limits.max_bytes > 0
    && config_.limits.max_files > 0;
  if (!enabled_) {
    return;
  }

  std::error_code error;
  std::filesystem::path absolute_root =
    std::filesystem::absolute(config_.root, error);
  if (error || absolute_root.empty()) {
    enabled_ = false;
    return;
  }
  namespace_directory_ =
    absolute_root.lexically_normal()
    / "styio"
    / "callable-specializations"
    / "v1"
    / namespace_digest_;
}

std::optional<CallableSpecializationObjectCache::Descriptor>
CallableSpecializationObjectCache::descriptor_for(
  const llvm::Module* module
) const {
  if (!enabled_ || module == nullptr) {
    return std::nullopt;
  }
  const llvm::NamedMDNode* metadata =
    module->getNamedMetadata(kModuleMetadataName);
  if (metadata == nullptr || metadata->getNumOperands() != 1) {
    return std::nullopt;
  }
  const llvm::MDNode* node = metadata->getOperand(0);
  if (node == nullptr || node->getNumOperands() != 2) {
    return std::nullopt;
  }
  const auto* digest =
    llvm::dyn_cast<llvm::MDString>(node->getOperand(0));
  const auto* symbol =
    llvm::dyn_cast<llvm::MDString>(node->getOperand(1));
  if (digest == nullptr
      || symbol == nullptr
      || !is_lower_hex_digest(digest->getString().str())
      || symbol->getString().empty()
      || symbol->getString().size() > kMaxSymbolLength
      || module->getModuleIdentifier() != digest->getString()
      || module->getFunction(symbol->getString()) == nullptr
      || module->getFunction(symbol->getString())->isDeclaration()) {
    return std::nullopt;
  }
  return Descriptor{
    digest->getString().str(),
    symbol->getString().str(),
  };
}

std::filesystem::path
CallableSpecializationObjectCache::entry_path(
  std::string_view content_digest
) const {
  return namespace_directory_
    / (std::string(content_digest) + ".styobj");
}

bool
CallableSpecializationObjectCache::ensure_namespace_directory() {
  if (!enabled_ || namespace_directory_.empty()) {
    return false;
  }
  std::error_code error;
  std::filesystem::create_directories(
    namespace_directory_,
    error);
  if (error) {
    return false;
  }

  bool symlink = false;
  if (llvm::sys::fs::is_symlink_file(
        namespace_directory_.string(),
        symlink)
      || symlink
      || llvm::sys::fs::setPermissions(
           namespace_directory_.string(),
           llvm::sys::fs::owner_all)) {
    return false;
  }

  llvm::sys::fs::file_status status;
  if (llvm::sys::fs::status(
        namespace_directory_.string(),
        status)
      || !llvm::sys::fs::is_directory(status)) {
    return false;
  }
#if !defined(_WIN32)
  if (status.getUser() != static_cast<std::uint32_t>(::getuid())) {
    return false;
  }
#endif
  return true;
}

bool
CallableSpecializationObjectCache::prune_once() {
  std::lock_guard<std::mutex> lock(filesystem_mutex_);
  if (prune_attempted_) {
    return namespace_ready_;
  }
  prune_attempted_ = true;
  namespace_ready_ = ensure_namespace_directory();
  if (!namespace_ready_) {
    io_failures_.fetch_add(1, std::memory_order_relaxed);
    return false;
  }
  prune_locked();
  return true;
}

void
CallableSpecializationObjectCache::prune_locked() {
  struct Candidate
  {
    std::filesystem::path path;
    llvm::sys::TimePoint<> modified;
    std::uint64_t size = 0;
  };

  std::vector<Candidate> candidates;
  std::uint64_t total_bytes = 0;
  std::error_code error;
  for (llvm::sys::fs::directory_iterator iterator(
         namespace_directory_.string(),
         error),
       end;
       !error && iterator != end;
       iterator.increment(error)) {
    const std::filesystem::path path(iterator->path());
    const std::string filename = path.filename().string();
    if (filename.size() != kDigestLength + 7
        || filename.substr(kDigestLength) != ".styobj"
        || !is_lower_hex_digest(
             std::string_view(filename).substr(
               0,
               kDigestLength))) {
      continue;
    }
    llvm::sys::fs::file_status status;
    if (!safe_regular_cache_file(path, status)) {
      continue;
    }
    candidates.push_back(Candidate{
      path,
      status.getLastModificationTime(),
      status.getSize(),
    });
    if (std::numeric_limits<std::uint64_t>::max()
          - total_bytes
        < status.getSize()) {
      total_bytes = std::numeric_limits<std::uint64_t>::max();
    }
    else {
      total_bytes += status.getSize();
    }
  }
  if (error) {
    io_failures_.fetch_add(1, std::memory_order_relaxed);
    return;
  }

  std::sort(
    candidates.begin(),
    candidates.end(),
    [](const Candidate& lhs, const Candidate& rhs)
    {
      if (lhs.modified != rhs.modified) {
        return lhs.modified < rhs.modified;
      }
      return lhs.path.filename().string()
        < rhs.path.filename().string();
    });

  const auto now = std::chrono::system_clock::now();
  std::size_t live_files = candidates.size();
  for (const Candidate& candidate : candidates) {
    const bool expired =
      candidate.modified <= now
      && std::chrono::duration_cast<std::chrono::seconds>(
           now - candidate.modified).count()
           > static_cast<std::int64_t>(
               config_.limits.max_age_seconds);
    const bool over_count =
      live_files > config_.limits.max_files;
    const bool over_bytes =
      total_bytes > config_.limits.max_bytes;
    if (!expired && !over_count && !over_bytes) {
      continue;
    }
    if (!llvm::sys::fs::remove(candidate.path.string())) {
      --live_files;
      total_bytes =
        candidate.size > total_bytes
          ? 0
          : total_bytes - candidate.size;
      evictions_.fetch_add(1, std::memory_order_relaxed);
    }
    else {
      io_failures_.fetch_add(1, std::memory_order_relaxed);
    }
  }
  retained_files_ = static_cast<std::uint64_t>(live_files);
  retained_bytes_ = total_bytes;
}

std::unique_ptr<llvm::MemoryBuffer>
CallableSpecializationObjectCache::getObject(
  const llvm::Module* module
) {
  const auto descriptor = descriptor_for(module);
  if (!descriptor.has_value()) {
    return nullptr;
  }
  lookups_.fetch_add(1, std::memory_order_relaxed);
  if (!prune_once()) {
    misses_.fetch_add(1, std::memory_order_relaxed);
    return nullptr;
  }

  const auto lookup_started = Clock::now();
  const std::filesystem::path path =
    entry_path(descriptor->content_digest);
  llvm::sys::fs::file_status status;
  if (!safe_regular_cache_file(path, status)) {
    lookup_ns_.fetch_add(
      elapsed_ns(lookup_started),
      std::memory_order_relaxed);
    misses_.fetch_add(1, std::memory_order_relaxed);
    return nullptr;
  }
  if (status.getSize() < kFixedHeaderSize
      || status.getSize() > kMaxEntryBytes) {
    lookup_ns_.fetch_add(
      elapsed_ns(lookup_started),
      std::memory_order_relaxed);
    misses_.fetch_add(1, std::memory_order_relaxed);
    corruptions_.fetch_add(1, std::memory_order_relaxed);
    std::lock_guard<std::mutex> lock(filesystem_mutex_);
    if (!llvm::sys::fs::remove(path.string())) {
      if (retained_files_ > 0) {
        --retained_files_;
      }
      retained_bytes_ =
        status.getSize() > retained_bytes_
          ? 0
          : retained_bytes_ - status.getSize();
    }
    return nullptr;
  }

  auto file_or_error = llvm::MemoryBuffer::getFile(
    path.string(),
    false,
    false,
    true);
  lookup_ns_.fetch_add(
    elapsed_ns(lookup_started),
    std::memory_order_relaxed);
  if (!file_or_error) {
    misses_.fetch_add(1, std::memory_order_relaxed);
    io_failures_.fetch_add(1, std::memory_order_relaxed);
    return nullptr;
  }

  const auto verification_started = Clock::now();
  const auto hashing_started = Clock::now();
  ParsedEntry parsed;
  const bool entry_valid =
    parse_entry(
      (*file_or_error)->getBuffer(),
      *descriptor,
      namespace_digest_,
      parsed);
  hashing_ns_.fetch_add(
    elapsed_ns(hashing_started),
    std::memory_order_relaxed);
  const bool object_valid =
    entry_valid
    && object_matches_target_and_symbol(
         parsed.object,
         config_.target_triple,
         descriptor->symbol);
  verification_ns_.fetch_add(
    elapsed_ns(verification_started),
    std::memory_order_relaxed);
  if (!object_valid) {
    misses_.fetch_add(1, std::memory_order_relaxed);
    corruptions_.fetch_add(1, std::memory_order_relaxed);
    std::lock_guard<std::mutex> lock(filesystem_mutex_);
    if (safe_regular_cache_file(path, status)) {
      if (!llvm::sys::fs::remove(path.string())) {
        if (retained_files_ > 0) {
          --retained_files_;
        }
        retained_bytes_ =
          status.getSize() > retained_bytes_
            ? 0
            : retained_bytes_ - status.getSize();
      }
    }
    return nullptr;
  }

  const auto materialization_started = Clock::now();
  auto materialized = llvm::MemoryBuffer::getMemBufferCopy(
    parsed.object,
    descriptor->symbol);
  materialization_ns_.fetch_add(
    elapsed_ns(materialization_started),
    std::memory_order_relaxed);
  hits_.fetch_add(1, std::memory_order_relaxed);
  return materialized;
}

void
CallableSpecializationObjectCache::notifyObjectCompiled(
  const llvm::Module* module,
  llvm::MemoryBufferRef object
) {
  const auto descriptor = descriptor_for(module);
  if (!descriptor.has_value()
      || object.getBufferSize() == 0
      || object.getBufferSize() > kMaxArtifactBytes) {
    return;
  }

  const auto hashing_started = Clock::now();
  const std::string object_digest =
    sha256_hex(object.getBuffer());
  const std::string serialized =
    serialize_entry(
      *descriptor,
      namespace_digest_,
      object,
      object_digest);
  hashing_ns_.fetch_add(
    elapsed_ns(hashing_started),
    std::memory_order_relaxed);

  std::lock_guard<std::mutex> lock(filesystem_mutex_);
  if (!ensure_namespace_directory()) {
    io_failures_.fetch_add(1, std::memory_order_relaxed);
    return;
  }

  int descriptor_fd = -1;
  llvm::SmallString<256> temporary_path;
  const std::string temporary_model =
    (entry_path(descriptor->content_digest).string()
      + ".tmp-%%%%%%%%");
  if (llvm::sys::fs::createUniqueFile(
        temporary_model,
        descriptor_fd,
        temporary_path)) {
    io_failures_.fetch_add(1, std::memory_order_relaxed);
    return;
  }

  bool write_ok = true;
  {
    llvm::raw_fd_ostream output(
      descriptor_fd,
      true,
      false);
    output.write(serialized.data(), serialized.size());
    output.flush();
    if (output.has_error()) {
      write_ok = false;
      output.clear_error();
    }
  }
  const std::filesystem::path destination =
    entry_path(descriptor->content_digest);
  llvm::sys::fs::file_status previous_status;
  const bool replaced_existing =
    safe_regular_cache_file(
      destination,
      previous_status);
  if (!write_ok
      || llvm::sys::fs::setPermissions(
           temporary_path,
           llvm::sys::fs::owner_read
             | llvm::sys::fs::owner_write)
      || llvm::sys::fs::rename(
           temporary_path,
           destination.string())) {
    (void)llvm::sys::fs::remove(temporary_path);
    io_failures_.fetch_add(1, std::memory_order_relaxed);
    return;
  }

  writes_.fetch_add(1, std::memory_order_relaxed);
  if (replaced_existing) {
    retained_bytes_ =
      previous_status.getSize() > retained_bytes_
        ? 0
        : retained_bytes_ - previous_status.getSize();
  }
  else if (retained_files_
           != std::numeric_limits<std::uint64_t>::max()) {
    ++retained_files_;
  }
  if (std::numeric_limits<std::uint64_t>::max()
        - retained_bytes_
      < serialized.size()) {
    retained_bytes_ =
      std::numeric_limits<std::uint64_t>::max();
  }
  else {
    retained_bytes_ +=
      static_cast<std::uint64_t>(serialized.size());
  }
  if (retained_files_ > config_.limits.max_files
      || retained_bytes_ > config_.limits.max_bytes
      || (writes_.load(std::memory_order_relaxed) % 64) == 0) {
    prune_locked();
  }
}

CallableSpecializationCacheStats
CallableSpecializationObjectCache::stats() const {
  return CallableSpecializationCacheStats{
    lookups_.load(std::memory_order_relaxed),
    hits_.load(std::memory_order_relaxed),
    misses_.load(std::memory_order_relaxed),
    corruptions_.load(std::memory_order_relaxed),
    writes_.load(std::memory_order_relaxed),
    evictions_.load(std::memory_order_relaxed),
    io_failures_.load(std::memory_order_relaxed),
    hashing_ns_.load(std::memory_order_relaxed),
    lookup_ns_.load(std::memory_order_relaxed),
    verification_ns_.load(std::memory_order_relaxed),
    materialization_ns_.load(std::memory_order_relaxed),
  };
}

std::string
CallableSpecializationObjectCache::stats_json() const {
  const auto snapshot = stats();
  std::string output;
  llvm::raw_string_ostream stream(output);
  stream
    << "{\"schema\":\"styio.callable-cache-stats.v1\""
    << ",\"lookups\":" << snapshot.lookups
    << ",\"hits\":" << snapshot.hits
    << ",\"misses\":" << snapshot.misses
    << ",\"corruptions\":" << snapshot.corruptions
    << ",\"writes\":" << snapshot.writes
    << ",\"evictions\":" << snapshot.evictions
    << ",\"io_failures\":" << snapshot.io_failures
    << ",\"timing_ns\":{\"hashing\":"
    << snapshot.hashing_ns
    << ",\"lookup\":" << snapshot.lookup_ns
    << ",\"verification\":" << snapshot.verification_ns
    << ",\"materialization\":"
    << snapshot.materialization_ns
    << "}}";
  stream.flush();
  return output;
}

}  // namespace styio::codegen
