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

Scope::Scope(bool qualified, std::string project_package, std::string module) :
    qualified_(qualified),
    project_package_identity_(std::move(project_package)),
    logical_module_identity_(std::move(module)) {}

Scope Scope::qualified(std::string project_package, std::string module) {
  if (project_package.empty() || project_package == "." || project_package == ".."
      || project_package.find('/') != std::string::npos
      || project_package.find('\\') != std::string::npos) {
    throw std::invalid_argument(
      "project/package identity must be non-empty and must not be path-shaped");
  }
  if (canonical_module_error(module) != CanonicalModuleError::None) {
    throw std::invalid_argument("logical module identity must use canonical slash form");
  }
  return Scope(true, std::move(project_package), std::move(module));
}

Scope Scope::anonymous() {
  return Scope(false, {}, {});
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
  append_field(preimage, "styio.semantic-resource-node.v1");
  preimage.push_back(scope.is_globally_comparable() ? '\x01' : '\x00');
  if (scope.is_globally_comparable()) {
    append_field(preimage, scope.project_package_identity());
    append_field(preimage, scope.logical_module_identity());
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
