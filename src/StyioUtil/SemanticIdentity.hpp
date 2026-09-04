#pragma once
#ifndef STYIO_SEMANTIC_IDENTITY_HPP_
#define STYIO_SEMANTIC_IDENTITY_HPP_

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace styio::semantic_identity {

enum class CanonicalModuleError : std::uint8_t
{
  None,
  NotCanonicalSlashForm,
  InvalidSegment,
};

CanonicalModuleError canonical_module_error(std::string_view module) noexcept;

class Scope
{
  bool qualified_ = false;
  std::string project_package_identity_;
  std::string logical_module_identity_;

  Scope(bool qualified, std::string project_package, std::string module);

public:
  static Scope qualified(
    std::string project_package_identity,
    std::string logical_module_identity);
  static Scope anonymous();

  bool is_globally_comparable() const noexcept { return qualified_; }
  const std::string& project_package_identity() const noexcept {
    return project_package_identity_;
  }
  const std::string& logical_module_identity() const noexcept {
    return logical_module_identity_;
  }

  friend bool operator==(const Scope&, const Scope&) = default;
};

struct SemanticIdentity
{
  std::array<std::uint8_t, 16> bytes{};

  friend bool operator==(const SemanticIdentity&, const SemanticIdentity&) = default;
};

struct SemanticIdentityHash
{
  std::size_t operator()(const SemanticIdentity& identity) const noexcept;
};

SemanticIdentity derive(
  const Scope& scope,
  const std::vector<std::string>& owner_components,
  std::string_view semantic_role,
  const std::vector<std::string>& discriminator_components);

class CollisionGuard
{
  std::unordered_map<SemanticIdentity, std::string, SemanticIdentityHash> entries_;

public:
  SemanticIdentity derive_and_record(
    const Scope& scope,
    const std::vector<std::string>& owner_components,
    std::string_view semantic_role,
    const std::vector<std::string>& discriminator_components);

  void record_for_test(
    const SemanticIdentity& identity,
    std::string_view canonical_preimage);
};

} // namespace styio::semantic_identity

#endif
