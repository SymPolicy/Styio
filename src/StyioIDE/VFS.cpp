#include "VFS.hpp"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <utility>

namespace styio::ide {

std::string
VirtualFileSystem::normalize_path(const std::string& path) const {
  if (path.empty()) {
    return path;
  }
  return std::filesystem::path(path).lexically_normal().string();
}

VirtualFileSystem::DocumentState&
VirtualFileSystem::ensure_document(const std::string& path) {
  const std::string normalized = normalize_path(path);
  auto it = documents_.find(normalized);
  if (it != documents_.end()) {
    return it->second;
  }

  DocumentState state;
  state.file_id = next_file_id_++;
  state.snapshot_id = 0;
  state.path = normalized;
  auto inserted = documents_.emplace(normalized, std::move(state));
  return inserted.first->second;
}

std::shared_ptr<const DocumentSnapshot>
VirtualFileSystem::make_snapshot(const DocumentState& state) const {
  auto snapshot = std::make_shared<DocumentSnapshot>();
  snapshot->file_id = state.file_id;
  snapshot->snapshot_id = state.snapshot_id;
  snapshot->path = state.path;
  snapshot->version = state.version;
  snapshot->buffer = state.buffer;
  snapshot->is_open = state.is_open;
  snapshot->from_full_sync = state.from_full_sync;
  snapshot->needs_full_resync = state.needs_full_resync;
  snapshot->applied_edits = state.applied_edits;
  snapshot->resync_reason = state.resync_reason;
  return snapshot;
}

std::shared_ptr<const DocumentSnapshot>
VirtualFileSystem::open(const std::string& path, std::string text, DocumentVersion version) {
  DocumentState& state = ensure_document(path);
  state.snapshot_id = next_snapshot_id_++;
  state.version = version;
  state.is_open = true;
  state.from_full_sync = true;
  state.needs_full_resync = false;
  state.applied_edits.clear();
  state.resync_reason.clear();
  state.buffer.reset(std::move(text));
  return make_snapshot(state);
}

std::shared_ptr<const DocumentSnapshot>
VirtualFileSystem::update(const std::string& path, std::string text, DocumentVersion version) {
  DocumentState& state = ensure_document(path);
  state.snapshot_id = next_snapshot_id_++;
  state.version = version;
  state.is_open = true;
  state.from_full_sync = true;
  state.needs_full_resync = false;
  state.applied_edits.clear();
  state.resync_reason.clear();
  state.buffer.reset(std::move(text));
  return make_snapshot(state);
}

DocumentUpdateResult
VirtualFileSystem::update(const std::string& path, const DocumentDelta& delta, DocumentVersion version) {
  DocumentState& state = ensure_document(path);

  if (delta.requires_full_resync) {
    state.snapshot_id = next_snapshot_id_++;
    state.version = version;
    state.is_open = true;
    state.from_full_sync = false;
    state.needs_full_resync = true;
    state.applied_edits.clear();
    state.resync_reason = delta.resync_reason.empty() ? "invalid incremental edit range" : delta.resync_reason;
    return DocumentUpdateResult{
      make_snapshot(state),
      false,
      false,
      true,
      state.resync_reason};
  }

  if (delta.is_full_sync) {
    auto snapshot = update(path, delta.full_text, version);
    return DocumentUpdateResult{
      snapshot,
      false,
      true,
      false,
      ""};
  }

  std::string working_text = state.buffer.text();
  std::vector<TextEdit> applied_edits;
  applied_edits.reserve(delta.edits.size());

  for (const auto& edit : delta.edits) {
    if (edit.range.start > edit.range.end || edit.range.end > working_text.size()) {
      state.snapshot_id = next_snapshot_id_++;
      state.version = version;
      state.is_open = true;
      state.from_full_sync = false;
      state.needs_full_resync = true;
      state.applied_edits.clear();
      state.resync_reason = "invalid incremental edit range";
      return DocumentUpdateResult{
        make_snapshot(state),
        false,
        false,
        true,
        state.resync_reason};
    }

    applied_edits.push_back(edit);
    working_text.replace(edit.range.start, edit.range.length(), edit.replacement);
  }

  state.snapshot_id = next_snapshot_id_++;
  state.version = version;
  state.is_open = true;
  state.from_full_sync = false;
  state.needs_full_resync = false;
  state.applied_edits = std::move(applied_edits);
  state.resync_reason.clear();
  state.buffer.reset(std::move(working_text));

  return DocumentUpdateResult{
    make_snapshot(state),
    true,
    false,
    false,
    ""};
}

void
VirtualFileSystem::close(const std::string& path) {
  const std::string normalized = normalize_path(path);
  auto it = documents_.find(normalized);
  if (it == documents_.end()) {
    return;
  }
  it->second.is_open = false;
}

std::shared_ptr<const DocumentSnapshot>
VirtualFileSystem::snapshot_for(const std::string& path) {
  DocumentState& state = ensure_document(path);
  if (!state.is_open && state.buffer.empty()) {
    std::ifstream input(state.path);
    std::ostringstream contents;
    contents << input.rdbuf();
    state.buffer.reset(contents.str());
    state.snapshot_id = next_snapshot_id_++;
    state.from_full_sync = true;
    state.needs_full_resync = false;
    state.applied_edits.clear();
    state.resync_reason.clear();
  }
  return make_snapshot(state);
}

std::vector<std::string>
VirtualFileSystem::open_paths() const {
  std::vector<std::string> paths;
  for (const auto& entry : documents_) {
    if (entry.second.is_open) {
      paths.push_back(entry.first);
    }
  }
  return paths;
}

}  // namespace styio::ide
