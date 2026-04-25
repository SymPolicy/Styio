#pragma once

#ifndef STYIO_IDE_SERVICE_HPP_
#define STYIO_IDE_SERVICE_HPP_

#include <cstddef>
#include <cstdint>
#include <deque>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "Common.hpp"
#include "Index.hpp"
#include "Project.hpp"
#include "SemDB.hpp"
#include "VFS.hpp"

namespace styio::ide {

enum class RuntimeRequestKind
{
  Completion,
  Hover,
  Definition,
  References,
};

struct ForegroundRequestTicket
{
  std::uint64_t request_id = 0;
  std::string path;
  SnapshotId snapshot_id = 0;
  DocumentVersion version = 0;
  std::uint64_t generation = 0;
  RuntimeRequestKind kind = RuntimeRequestKind::Completion;
};

struct DiagnosticPublication
{
  std::shared_ptr<const DocumentSnapshot> snapshot;
  std::vector<Diagnostic> diagnostics;
  bool includes_semantic = false;
};

struct RuntimeLatencyStats
{
  std::uint64_t count = 0;
  std::uint64_t total_microseconds = 0;
  std::uint64_t max_microseconds = 0;
};

struct RuntimeCounters
{
  std::uint64_t stale_request_drops = 0;
  std::uint64_t canceled_requests = 0;
  std::uint64_t semantic_diagnostic_runs = 0;
  std::uint64_t semantic_diagnostic_debounces = 0;
  std::uint64_t background_tasks_enqueued = 0;
  std::uint64_t background_tasks_completed = 0;
  std::uint64_t foreground_yield_events = 0;
  RuntimeLatencyStats completion_latency;
  RuntimeLatencyStats hover_latency;
  RuntimeLatencyStats definition_latency;
  RuntimeLatencyStats references_latency;
  RuntimeLatencyStats semantic_diagnostics_latency;
  RuntimeLatencyStats background_index_latency;
};

class IdeService
{
private:
  struct RuntimeDocumentState
  {
    SnapshotId snapshot_id = 0;
    DocumentVersion version = 0;
    std::uint64_t generation = 0;
    bool has_pending_semantic = false;
    SnapshotId pending_semantic_snapshot_id = 0;
    DocumentVersion pending_semantic_version = 0;
    std::uint64_t pending_semantic_generation = 0;
  };

  enum class RuntimeDropReason
  {
    None,
    Stale,
    Canceled,
  };

  VirtualFileSystem vfs_;
  Project project_;
  SemanticDB semdb_;
  std::unordered_map<std::string, RuntimeDocumentState> runtime_documents_;
  std::deque<std::string> pending_semantic_paths_;
  std::unordered_set<std::string> pending_semantic_path_set_;
  std::deque<std::string> background_index_paths_;
  std::unordered_set<std::string> background_index_path_set_;
  std::unordered_set<std::string> dirty_open_index_paths_;
  std::unordered_set<std::uint64_t> canceled_request_ids_;
  std::uint64_t next_request_id_ = 1;
  RuntimeCounters runtime_counters_;

  RuntimeDocumentState& runtime_state_for(const std::string& path);
  void refresh_dirty_open_indexes();
  void record_visible_snapshot(const std::shared_ptr<const DocumentSnapshot>& snapshot);
  void schedule_semantic_diagnostics(const std::shared_ptr<const DocumentSnapshot>& snapshot);
  RuntimeDropReason drop_reason_for(const ForegroundRequestTicket& ticket);
  void record_latency(RuntimeLatencyStats& stats, std::uint64_t elapsed_microseconds);

public:
  IdeService();

  void initialize(const std::string& root_uri);

  std::vector<Diagnostic> did_open(const std::string& uri, const std::string& text, DocumentVersion version);
  std::vector<Diagnostic> did_change(const std::string& uri, const std::string& text, DocumentVersion version);
  std::vector<Diagnostic> did_change(const std::string& uri, const DocumentDelta& delta, DocumentVersion version);
  void did_close(const std::string& uri);

  ForegroundRequestTicket begin_foreground_request(const std::string& uri, RuntimeRequestKind kind);
  ForegroundRequestTicket begin_foreground_request(const std::string& uri, RuntimeRequestKind kind, std::uint64_t request_id);
  void cancel_request(std::uint64_t request_id);
  std::vector<DiagnosticPublication> drain_semantic_diagnostics(std::size_t max_documents = static_cast<std::size_t>(-1));
  void schedule_background_index_refresh();
  std::size_t run_background_tasks(std::size_t budget = 1);
  std::size_t pending_semantic_diagnostic_count() const;
  std::size_t pending_background_task_count() const;
  const RuntimeCounters& runtime_counters() const;
  void reset_runtime_counters();

  CompletionContext completion_context(const std::string& uri, Position position);
  std::vector<CompletionItem> completion(const std::string& uri, Position position);
  std::vector<CompletionItem> completion(const ForegroundRequestTicket& ticket, Position position);
  std::optional<HoverResult> hover(const std::string& uri, Position position);
  std::optional<HoverResult> hover(const ForegroundRequestTicket& ticket, Position position);
  std::vector<Location> definition(const std::string& uri, Position position);
  std::vector<Location> definition(const ForegroundRequestTicket& ticket, Position position);
  std::vector<Location> references(const std::string& uri, Position position);
  std::vector<Location> references(const ForegroundRequestTicket& ticket, Position position);
  std::vector<DocumentSymbol> document_symbols(const std::string& uri);
  std::vector<IndexedSymbol> workspace_symbols(const std::string& query);
  std::vector<std::uint32_t> semantic_tokens(const std::string& uri);
  std::shared_ptr<const DocumentSnapshot> snapshot_for_uri(const std::string& uri);
};

}  // namespace styio::ide

#endif
