#pragma once
#ifndef STYIO_OBSERVABLE_STATIC_SNAPSHOT_CONTRACT_HPP_
#define STYIO_OBSERVABLE_STATIC_SNAPSHOT_CONTRACT_HPP_

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "StyioResourceTopology/ResourceTopology.hpp"
#include "StyioServices/StyioConfig/CompilePlanContract.hpp"

namespace styio::observable {

inline constexpr int kStaticSnapshotSchemaVersion = 1;
inline constexpr std::string_view kStaticSnapshotContractName =
  "styio.observable.static-snapshot";
inline constexpr std::string_view kStaticSnapshotStability = "incubating";
inline constexpr std::string_view kStaticSnapshotArtifactSuffix =
  ".observable-static-snapshot.json";
inline constexpr std::string_view kCompletenessValidatedTopology =
  "complete/validated-topology";
inline constexpr std::string_view kCompletenessProvenScalarNoop =
  "complete/proven-scalar-noop";
inline constexpr std::string_view kAnchorPrecisionFile = "file";
inline constexpr std::string_view kFactPredicateCapabilities = "capabilities";
inline constexpr std::string_view kFactPredicateTypeState = "type-state";

inline constexpr std::array<std::string_view, 5> kStaticSnapshotCapabilities{
  "file-source-anchors",
  "producer-evidence",
  "static-topology-edges",
  "static-topology-facts",
  "static-topology-nodes",
};

inline constexpr std::string_view kNodeIdPrefix = "n1_";
inline constexpr std::string_view kEdgeIdPrefix = "e1_";
inline constexpr std::string_view kFactIdPrefix = "f1_";
inline constexpr std::string_view kAnchorIdPrefix = "a1_";
inline constexpr std::string_view kEvidenceIdPrefix = "v1_";

bool static_snapshot_capability_is_supported(std::string_view name) noexcept;

struct SnapshotProducer
{
  std::string name = "styio";
  std::string version;
};

struct SnapshotCounts
{
  std::size_t nodes = 0;
  std::size_t edges = 0;
  std::size_t facts = 0;
  std::size_t anchors = 0;
  std::size_t evidence = 0;
  std::size_t serialized_bytes = 0;
};

struct SnapshotPublishResult
{
  bool ok = false;
  std::string error;
  std::string json;
  SnapshotCounts counts;
};

enum class SnapshotFault : std::uint8_t
{
  None,
  MissingDescriptor,
  DanglingEndpoint,
  DuplicateIdentity,
  EvidenceCycle,
};

SnapshotPublishResult publish_validated_topology(
  const styio::resource_topology::ValidatedArtifact& artifact,
  const styio::config::CompilationUnit& unit,
  SnapshotProducer producer,
  SnapshotFault fault = SnapshotFault::None);

SnapshotPublishResult publish_proven_scalar_noop(
  const styio::config::CompilationUnit& unit,
  SnapshotProducer producer);

} // namespace styio::observable

#endif
