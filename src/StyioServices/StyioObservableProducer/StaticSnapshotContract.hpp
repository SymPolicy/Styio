#pragma once
#ifndef STYIO_OBSERVABLE_STATIC_SNAPSHOT_CONTRACT_HPP_
#define STYIO_OBSERVABLE_STATIC_SNAPSHOT_CONTRACT_HPP_

#include <cstdint>
#include <string>
#include <string_view>

#include "StyioResourceTopology/ResourceTopology.hpp"
#include "StyioServices/StyioConfig/CompilePlanContract.hpp"
#include "StyioServices/StyioObservable/Snapshot.hpp"

namespace styio::observable {

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
