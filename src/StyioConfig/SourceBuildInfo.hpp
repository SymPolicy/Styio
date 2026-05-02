#ifndef STYIO_CONFIG_SOURCE_BUILD_INFO_HPP_
#define STYIO_CONFIG_SOURCE_BUILD_INFO_HPP_

#include <string>

namespace styio::config {

struct SourceBuildInfoOptions
{
  std::string compiler_version;
  std::string compiler_channel;
  std::string edition_max;
};

const char*
default_source_origin();

const char*
source_branch_for_channel(const std::string& channel);

std::string
source_build_info_json(const SourceBuildInfoOptions& options);

} // namespace styio::config

#endif
