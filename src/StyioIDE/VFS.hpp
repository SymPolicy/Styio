#pragma once

#ifndef STYIO_IDE_VFS_HPP_
#define STYIO_IDE_VFS_HPP_

#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "Common.hpp"

namespace styio::ide {

struct DocumentSnapshot
{
  FileId file_id = 0;
  SnapshotId snapshot_id = 0;
  std::string path;
  DocumentVersion version = 0;
  TextBuffer buffer;
  bool is_open = false;
};

class VirtualFileSystem
{
private:
  struct DocumentState
  {
    FileId file_id = 0;
    SnapshotId snapshot_id = 0;
    DocumentVersion version = 0;
    bool is_open = false;
    std::string path;
    TextBuffer buffer;
  };

  FileId next_file_id_ = 1;
  SnapshotId next_snapshot_id_ = 1;
  std::unordered_map<std::string, DocumentState> documents_;

  std::string normalize_path(const std::string& path) const;
  DocumentState& ensure_document(const std::string& path);
  std::shared_ptr<const DocumentSnapshot> make_snapshot(const DocumentState& state) const;

public:
  std::shared_ptr<const DocumentSnapshot> open(const std::string& path, std::string text, DocumentVersion version);
  std::shared_ptr<const DocumentSnapshot> update(const std::string& path, std::string text, DocumentVersion version);
  void close(const std::string& path);

  std::shared_ptr<const DocumentSnapshot> snapshot_for(const std::string& path);
  std::vector<std::string> open_paths() const;
};

}  // namespace styio::ide

#endif
