#include "Project.hpp"

#include <cstdlib>
#include <filesystem>

namespace styio::ide {

void
Project::set_root(const std::string& root_path) {
  root_path_ = std::filesystem::path(root_path).lexically_normal().string();

  const char* xdg_cache_home = std::getenv("XDG_CACHE_HOME");
  const char* home = std::getenv("HOME");
  if (xdg_cache_home != nullptr && *xdg_cache_home != '\0') {
    cache_root_ = (std::filesystem::path(xdg_cache_home) / "styio" / "ide").string();
  } else if (home != nullptr && *home != '\0') {
    cache_root_ = (std::filesystem::path(home) / ".cache" / "styio" / "ide").string();
  } else {
    cache_root_ = (std::filesystem::temp_directory_path() / "styio-ide-cache").string();
  }

  scan_workspace();
}

void
Project::scan_workspace() {
  workspace_files_.clear();
  if (root_path_.empty()) {
    return;
  }

  for (const auto& entry : std::filesystem::recursive_directory_iterator(root_path_)) {
    if (!entry.is_regular_file()) {
      continue;
    }
    const auto path = entry.path();
    const std::string text = path.string();
    if (text.find("/.git/") != std::string::npos
        || text.find("/build/") != std::string::npos
        || text.find("/build-codex/") != std::string::npos) {
      continue;
    }
    if (path.extension() == ".styio") {
      workspace_files_.push_back(path.lexically_normal().string());
    }
  }
}

}  // namespace styio::ide
