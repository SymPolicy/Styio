#pragma once

#ifndef STYIO_IDE_PROJECT_HPP_
#define STYIO_IDE_PROJECT_HPP_

#include <string>
#include <vector>

#include "Common.hpp"

namespace styio::ide {

class Project
{
private:
  ProjectId project_id_ = 1;
  std::string root_path_;
  std::string cache_root_;
  std::vector<std::string> workspace_files_;

public:
  void set_root(const std::string& root_path);

  ProjectId project_id() const {
    return project_id_;
  }

  const std::string& root_path() const {
    return root_path_;
  }

  const std::string& cache_root() const {
    return cache_root_;
  }

  const std::vector<std::string>& workspace_files() const {
    return workspace_files_;
  }

  void scan_workspace();
};

}  // namespace styio::ide

#endif
