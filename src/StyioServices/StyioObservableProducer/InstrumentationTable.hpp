#pragma once
#ifndef STYIO_OBSERVABLE_INSTRUMENTATION_TABLE_HPP_
#define STYIO_OBSERVABLE_INSTRUMENTATION_TABLE_HPP_

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "StyioResourceTopology/ResourceTopology.hpp"
#include "StyioServices/StyioConfig/CompilePlanContract.hpp"
#include "StyioServices/StyioObservable/RuntimeCorrelation.hpp"
#include "StyioServices/StyioObservable/Snapshot.hpp"

class MainBlockAST;
class StyioAST;
class StyioSemaContext;

namespace styio::observable {

struct InstrumentationDescriptor
{
  std::string snapshot_id;
  std::string site_id;
  SiteRole role = SiteRole::Task;
};

struct InstrumentationTable
{
  std::uint32_t generation = 1;
  std::string snapshot_id;
  std::vector<InstrumentationDescriptor> descriptors;
  std::unordered_map<const StyioAST*, std::uint32_t> ast_index;
};

InstrumentationTable build_instrumentation_table(
  const styio::resource_topology::ValidatedArtifact& artifact,
  const Snapshot& snapshot
);

bool bind_runtime_observation(
  StyioSemaContext& sema,
  const MainBlockAST* root,
  const styio::config::CompilationUnit& unit,
  std::string_view producer_version,
  InstrumentationTable& out_table,
  std::string& error_message
);

} // namespace styio::observable

#endif
