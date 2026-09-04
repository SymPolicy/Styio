#include "SemanticIdentity.hpp"

#include <algorithm>
#include <limits>
#include <stdexcept>

#include "llvm/ADT/StringRef.h"
#include "llvm/Support/SHA256.h"

namespace styio::semantic_identity {
namespace {

void append_u32(std::string& out, std::size_t value) {
  if (value > std::numeric_limits<std::uint32_t>::max()) {
    throw std::length_error("semantic identity field exceeds supported size");
  }
  const auto encoded = static_cast<std::uint32_t>(value);
  for (int shift = 24; shift >= 0; shift -= 8) {
    out.push_back(static_cast<char>((encoded >> shift) & 0xffu));
  }
}

void append_field(std::string& out, std::string_view value) {
  append_u32(out, value.size());
  out.append(value.data(), value.size());
}

void append_fields(std::string& out, const std::vector<std::string>& values) {
  append_u32(out, values.size());
  for (const auto& value : values) {
    append_field(out, value);
  }
}

bool package_name_is_path_shaped(std::string_view name) noexcept {
  return name.find('/') != std::string_view::npos
    || name.find('\\') != std::string_view::npos
    || name.find('@') != std::string_view::npos
    || name.find(' ') != std::string_view::npos;
}

} // namespace

CanonicalModuleError canonical_module_error(std::string_view module) noexcept {
  if (module.empty() || module.front() == '/' || module.back() == '/'
      || module.find('\\') != std::string_view::npos
      || module.find('.') != std::string_view::npos) {
    return CanonicalModuleError::NotCanonicalSlashForm;
  }
  std::size_t begin = 0;
  while (begin <= module.size()) {
    const std::size_t end = module.find('/', begin);
    const std::size_t stop = end == std::string_view::npos ? module.size() : end;
    const std::string_view segment = module.substr(begin, stop - begin);
    if (segment.empty() || segment == "." || segment == "..") {
      return CanonicalModuleError::InvalidSegment;
    }
    if (end == std::string_view::npos) {
      break;
    }
    begin = end + 1;
  }
  return CanonicalModuleError::None;
}

CanonicalRelativePathError canonical_relative_path_error(std::string_view path) noexcept {
  if (path.empty()) {
    return CanonicalRelativePathError::Empty;
  }
  if (path.front() == '/' || path.back() == '/'
      || path.find('\\') != std::string_view::npos) {
    return CanonicalRelativePathError::NotCanonicalSlashForm;
  }
  std::size_t begin = 0;
  while (begin <= path.size()) {
    const std::size_t end = path.find('/', begin);
    const std::size_t stop = end == std::string_view::npos ? path.size() : end;
    const std::string_view segment = path.substr(begin, stop - begin);
    if (segment.empty() || segment == "." || segment == "..") {
      return CanonicalRelativePathError::InvalidSegment;
    }
    if (end == std::string_view::npos) {
      break;
    }
    begin = end + 1;
  }
  return CanonicalRelativePathError::None;
}

Scope::Scope(
  bool qualified,
  std::string package_name,
  std::string manifest_relative_path,
  std::string entry_relative_path
) :
    qualified_(qualified),
    package_name_(std::move(package_name)),
    manifest_relative_path_(std::move(manifest_relative_path)),
    entry_relative_path_(std::move(entry_relative_path)) {}

Scope Scope::qualified(
  std::string package_name,
  std::string manifest_relative_path,
  std::string entry_relative_path
) {
  if (package_name.empty() || package_name == "." || package_name == ".."
      || package_name_is_path_shaped(package_name)) {
    throw std::invalid_argument(
      "package name must be a non-empty namespaced identity and must not be path-shaped");
  }
  if (canonical_relative_path_error(manifest_relative_path)
      != CanonicalRelativePathError::None) {
    throw std::invalid_argument(
      "manifest-relative path must use canonical slash form without dot segments");
  }
  if (canonical_relative_path_error(entry_relative_path)
      != CanonicalRelativePathError::None) {
    throw std::invalid_argument(
      "entry-relative path must use canonical slash form without dot segments");
  }
  return Scope(
    true,
    std::move(package_name),
    std::move(manifest_relative_path),
    std::move(entry_relative_path));
}

Scope Scope::anonymous() {
  return Scope(false, {}, {}, {});
}

std::size_t SemanticIdentityHash::operator()(const SemanticIdentity& identity) const noexcept {
  std::size_t value = 0;
  for (const auto byte : identity.bytes) {
    value = (value * 131u) ^ byte;
  }
  return value;
}

namespace {

std::string canonical_preimage(
  const Scope& scope,
  const std::vector<std::string>& owners,
  std::string_view role,
  const std::vector<std::string>& discriminators
) {
  std::string preimage;
  append_field(preimage, "styio.semantic-resource-node.v2");
  preimage.push_back(scope.is_globally_comparable() ? '\x01' : '\x00');
  if (scope.is_globally_comparable()) {
    append_field(preimage, scope.package_name());
    append_field(preimage, scope.manifest_relative_path());
    append_field(preimage, scope.entry_relative_path());
  }
  append_fields(preimage, owners);
  append_field(preimage, role);
  append_fields(preimage, discriminators);

  return preimage;
}

SemanticIdentity identity_from_preimage(std::string_view preimage) {
  llvm::SHA256 hasher;
  hasher.update(llvm::StringRef(preimage.data(), preimage.size()));
  const auto digest = hasher.final();
  SemanticIdentity identity;
  std::copy_n(digest.begin(), identity.bytes.size(), identity.bytes.begin());
  return identity;
}

} // namespace

SemanticIdentity derive(
  const Scope& scope,
  const std::vector<std::string>& owners,
  std::string_view role,
  const std::vector<std::string>& discriminators
) {
  return identity_from_preimage(canonical_preimage(scope, owners, role, discriminators));
}

std::string encode_hex(const SemanticIdentity& identity) {
  static constexpr char kHex[] = "0123456789abcdef";
  std::string out;
  out.resize(identity.bytes.size() * 2);
  for (std::size_t i = 0; i < identity.bytes.size(); ++i) {
    out[i * 2] = kHex[identity.bytes[i] >> 4];
    out[i * 2 + 1] = kHex[identity.bytes[i] & 0x0fu];
  }
  return out;
}

SemanticIdentity CollisionGuard::derive_and_record(
  const Scope& scope,
  const std::vector<std::string>& owners,
  std::string_view role,
  const std::vector<std::string>& discriminators
) {
  const std::string preimage = canonical_preimage(scope, owners, role, discriminators);
  const SemanticIdentity identity = identity_from_preimage(preimage);
  record_for_test(identity, preimage);
  return identity;
}

void CollisionGuard::record_for_test(
  const SemanticIdentity& identity,
  std::string_view preimage
) {
  auto [it, inserted] = entries_.try_emplace(identity, preimage);
  if (inserted) {
    return;
  }
  if (it->second == preimage) {
    throw std::logic_error("duplicate semantic identity key");
  }
  throw std::logic_error("semantic identity collision");
}

} // namespace styio::semantic_identity
