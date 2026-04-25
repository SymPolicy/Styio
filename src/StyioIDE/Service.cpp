#include "Service.hpp"

#include <algorithm>
#include <chrono>

namespace styio::ide {

namespace {

std::uint64_t
elapsed_microseconds_since(std::chrono::steady_clock::time_point start_time) {
  const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
    std::chrono::steady_clock::now() - start_time);
  return static_cast<std::uint64_t>(elapsed.count());
}

}  // namespace

IdeService::IdeService() :
    semdb_(vfs_, project_) {
}

IdeService::RuntimeDocumentState&
IdeService::runtime_state_for(const std::string& path) {
  return runtime_documents_[path];
}

void
IdeService::refresh_dirty_open_indexes() {
  if (dirty_open_index_paths_.empty()) {
    return;
  }

  std::vector<std::string> dirty_paths(dirty_open_index_paths_.begin(), dirty_open_index_paths_.end());
  dirty_open_index_paths_.clear();
  for (const auto& path : dirty_paths) {
    semdb_.build_snapshot(path);
  }
}

void
IdeService::record_visible_snapshot(const std::shared_ptr<const DocumentSnapshot>& snapshot) {
  RuntimeDocumentState& state = runtime_state_for(snapshot->path);
  if (state.snapshot_id != snapshot->snapshot_id || state.version != snapshot->version) {
    state.generation += 1;
  }
  state.snapshot_id = snapshot->snapshot_id;
  state.version = snapshot->version;
}

void
IdeService::schedule_semantic_diagnostics(const std::shared_ptr<const DocumentSnapshot>& snapshot) {
  RuntimeDocumentState& state = runtime_state_for(snapshot->path);
  const bool replacing_pending = state.has_pending_semantic;
  state.has_pending_semantic = true;
  state.pending_semantic_snapshot_id = snapshot->snapshot_id;
  state.pending_semantic_version = snapshot->version;
  state.pending_semantic_generation = state.generation;
  if (!pending_semantic_path_set_.insert(snapshot->path).second) {
    if (replacing_pending) {
      runtime_counters_.semantic_diagnostic_debounces += 1;
    }
    return;
  }
  if (replacing_pending) {
    runtime_counters_.semantic_diagnostic_debounces += 1;
  }
  pending_semantic_paths_.push_back(snapshot->path);
}

IdeService::RuntimeDropReason
IdeService::drop_reason_for(const ForegroundRequestTicket& ticket) {
  if (canceled_request_ids_.erase(ticket.request_id) > 0) {
    runtime_counters_.canceled_requests += 1;
    return RuntimeDropReason::Canceled;
  }

  const auto it = runtime_documents_.find(ticket.path);
  if (it == runtime_documents_.end()) {
    runtime_counters_.stale_request_drops += 1;
    return RuntimeDropReason::Stale;
  }

  const RuntimeDocumentState& state = it->second;
  if (state.generation != ticket.generation
      || state.snapshot_id != ticket.snapshot_id
      || state.version != ticket.version) {
    runtime_counters_.stale_request_drops += 1;
    return RuntimeDropReason::Stale;
  }

  return RuntimeDropReason::None;
}

void
IdeService::record_latency(RuntimeLatencyStats& stats, std::uint64_t elapsed_microseconds) {
  stats.count += 1;
  stats.total_microseconds += elapsed_microseconds;
  stats.max_microseconds = std::max(stats.max_microseconds, elapsed_microseconds);
}

void
IdeService::initialize(const std::string& root_uri) {
  project_.set_root(path_from_uri(root_uri));
  semdb_.configure_cache_root(project_.cache_root());
  semdb_.index_workspace();
}

std::vector<Diagnostic>
IdeService::did_open(const std::string& uri, const std::string& text, DocumentVersion version) {
  const std::string path = path_from_uri(uri);
  const auto snapshot = vfs_.open(path, text, version);
  record_visible_snapshot(snapshot);
  dirty_open_index_paths_.insert(path);
  schedule_semantic_diagnostics(snapshot);
  return semdb_.syntax_diagnostics_for(path);
}

std::vector<Diagnostic>
IdeService::did_change(const std::string& uri, const std::string& text, DocumentVersion version) {
  const std::string path = path_from_uri(uri);
  const auto snapshot = vfs_.update(path, text, version);
  record_visible_snapshot(snapshot);
  dirty_open_index_paths_.insert(path);
  schedule_semantic_diagnostics(snapshot);
  return semdb_.syntax_diagnostics_for(path);
}

std::vector<Diagnostic>
IdeService::did_change(const std::string& uri, const DocumentDelta& delta, DocumentVersion version) {
  const std::string path = path_from_uri(uri);
  const auto update_result = vfs_.update(path, delta, version);
  record_visible_snapshot(update_result.snapshot);
  dirty_open_index_paths_.insert(path);
  schedule_semantic_diagnostics(update_result.snapshot);
  return semdb_.syntax_diagnostics_for(path);
}

void
IdeService::did_close(const std::string& uri) {
  const std::string path = path_from_uri(uri);
  vfs_.close(path);
  semdb_.drop_open_file(path);

  pending_semantic_path_set_.erase(path);
  pending_semantic_paths_.erase(
    std::remove(pending_semantic_paths_.begin(), pending_semantic_paths_.end(), path),
    pending_semantic_paths_.end());
  background_index_path_set_.erase(path);
  background_index_paths_.erase(
    std::remove(background_index_paths_.begin(), background_index_paths_.end(), path),
    background_index_paths_.end());
  dirty_open_index_paths_.erase(path);
  runtime_documents_.erase(path);
}

ForegroundRequestTicket
IdeService::begin_foreground_request(const std::string& uri, RuntimeRequestKind kind) {
  return begin_foreground_request(uri, kind, next_request_id_++);
}

ForegroundRequestTicket
IdeService::begin_foreground_request(const std::string& uri, RuntimeRequestKind kind, std::uint64_t request_id) {
  const std::string path = path_from_uri(uri);
  const auto snapshot = vfs_.snapshot_for(path);
  record_visible_snapshot(snapshot);
  if (!background_index_paths_.empty()) {
    runtime_counters_.foreground_yield_events += 1;
  }

  const RuntimeDocumentState& state = runtime_state_for(path);
  return ForegroundRequestTicket{
    request_id,
    path,
    snapshot->snapshot_id,
    snapshot->version,
    state.generation,
    kind};
}

void
IdeService::cancel_request(std::uint64_t request_id) {
  canceled_request_ids_.insert(request_id);
}

std::vector<DiagnosticPublication>
IdeService::drain_semantic_diagnostics(std::size_t max_documents) {
  std::vector<DiagnosticPublication> publications;
  while (!pending_semantic_paths_.empty() && publications.size() < max_documents) {
    const std::string path = pending_semantic_paths_.front();
    pending_semantic_paths_.pop_front();
    pending_semantic_path_set_.erase(path);

    auto state_it = runtime_documents_.find(path);
    if (state_it == runtime_documents_.end() || !state_it->second.has_pending_semantic) {
      continue;
    }

    RuntimeDocumentState pending_state = state_it->second;
    state_it->second.has_pending_semantic = false;

    if (pending_state.pending_semantic_generation != pending_state.generation
        || pending_state.pending_semantic_snapshot_id != pending_state.snapshot_id
        || pending_state.pending_semantic_version != pending_state.version) {
      runtime_counters_.stale_request_drops += 1;
      continue;
    }

    const auto snapshot = vfs_.snapshot_for(path);
    if (snapshot->snapshot_id != pending_state.pending_semantic_snapshot_id
        || snapshot->version != pending_state.pending_semantic_version) {
      runtime_counters_.stale_request_drops += 1;
      continue;
    }

    const auto start_time = std::chrono::steady_clock::now();
    auto diagnostics = semdb_.diagnostics_for(path);
    const std::uint64_t elapsed_microseconds = elapsed_microseconds_since(start_time);

    state_it = runtime_documents_.find(path);
    if (state_it == runtime_documents_.end()
        || state_it->second.generation != pending_state.pending_semantic_generation
        || state_it->second.snapshot_id != pending_state.pending_semantic_snapshot_id
        || state_it->second.version != pending_state.pending_semantic_version) {
      runtime_counters_.stale_request_drops += 1;
      continue;
    }

    runtime_counters_.semantic_diagnostic_runs += 1;
    record_latency(runtime_counters_.semantic_diagnostics_latency, elapsed_microseconds);
    publications.push_back(DiagnosticPublication{snapshot, std::move(diagnostics), true});
  }
  return publications;
}

void
IdeService::schedule_background_index_refresh() {
  const std::vector<std::string> open_paths = vfs_.open_paths();
  for (const auto& path : project_.workspace_files()) {
    if (std::find(open_paths.begin(), open_paths.end(), path) != open_paths.end()) {
      continue;
    }
    if (background_index_path_set_.insert(path).second) {
      background_index_paths_.push_back(path);
      runtime_counters_.background_tasks_enqueued += 1;
    }
  }
}

std::size_t
IdeService::run_background_tasks(std::size_t budget) {
  std::size_t completed = 0;
  while (completed < budget && !background_index_paths_.empty()) {
    const std::string path = background_index_paths_.front();
    background_index_paths_.pop_front();
    background_index_path_set_.erase(path);

    const auto start_time = std::chrono::steady_clock::now();
    semdb_.index_workspace_file(path);
    const std::uint64_t elapsed_microseconds = elapsed_microseconds_since(start_time);
    runtime_counters_.background_tasks_completed += 1;
    record_latency(runtime_counters_.background_index_latency, elapsed_microseconds);
    completed += 1;
  }
  return completed;
}

std::size_t
IdeService::pending_semantic_diagnostic_count() const {
  return pending_semantic_paths_.size();
}

std::size_t
IdeService::pending_background_task_count() const {
  return background_index_paths_.size();
}

const RuntimeCounters&
IdeService::runtime_counters() const {
  return runtime_counters_;
}

void
IdeService::reset_runtime_counters() {
  runtime_counters_ = RuntimeCounters{};
}

CompletionContext
IdeService::completion_context(const std::string& uri, Position position) {
  const std::string path = path_from_uri(uri);
  auto snapshot = vfs_.snapshot_for(path);
  return semdb_.completion_context_at(path, snapshot->buffer.offset_at(position));
}

std::vector<CompletionItem>
IdeService::completion(const ForegroundRequestTicket& ticket, Position position) {
  if (drop_reason_for(ticket) != RuntimeDropReason::None) {
    return {};
  }

  const auto snapshot = vfs_.snapshot_for(ticket.path);
  const auto start_time = std::chrono::steady_clock::now();
  auto items = semdb_.complete_at(ticket.path, snapshot->buffer.offset_at(position));
  const std::uint64_t elapsed_microseconds = elapsed_microseconds_since(start_time);

  if (drop_reason_for(ticket) != RuntimeDropReason::None) {
    return {};
  }

  record_latency(runtime_counters_.completion_latency, elapsed_microseconds);
  return items;
}

std::vector<CompletionItem>
IdeService::completion(const std::string& uri, Position position) {
  return completion(begin_foreground_request(uri, RuntimeRequestKind::Completion), position);
}

std::optional<HoverResult>
IdeService::hover(const ForegroundRequestTicket& ticket, Position position) {
  if (drop_reason_for(ticket) != RuntimeDropReason::None) {
    return std::nullopt;
  }

  const auto snapshot = vfs_.snapshot_for(ticket.path);
  const auto start_time = std::chrono::steady_clock::now();
  auto result = semdb_.hover_at(ticket.path, snapshot->buffer.offset_at(position));
  const std::uint64_t elapsed_microseconds = elapsed_microseconds_since(start_time);

  if (drop_reason_for(ticket) != RuntimeDropReason::None) {
    return std::nullopt;
  }

  record_latency(runtime_counters_.hover_latency, elapsed_microseconds);
  return result;
}

std::optional<HoverResult>
IdeService::hover(const std::string& uri, Position position) {
  return hover(begin_foreground_request(uri, RuntimeRequestKind::Hover), position);
}

std::vector<Location>
IdeService::definition(const ForegroundRequestTicket& ticket, Position position) {
  if (drop_reason_for(ticket) != RuntimeDropReason::None) {
    return {};
  }

  refresh_dirty_open_indexes();
  const auto snapshot = vfs_.snapshot_for(ticket.path);
  const auto start_time = std::chrono::steady_clock::now();
  auto locations = semdb_.definition_at(ticket.path, snapshot->buffer.offset_at(position));
  const std::uint64_t elapsed_microseconds = elapsed_microseconds_since(start_time);

  if (drop_reason_for(ticket) != RuntimeDropReason::None) {
    return {};
  }

  record_latency(runtime_counters_.definition_latency, elapsed_microseconds);
  return locations;
}

std::vector<Location>
IdeService::definition(const std::string& uri, Position position) {
  return definition(begin_foreground_request(uri, RuntimeRequestKind::Definition), position);
}

std::vector<Location>
IdeService::references(const ForegroundRequestTicket& ticket, Position position) {
  if (drop_reason_for(ticket) != RuntimeDropReason::None) {
    return {};
  }

  const auto snapshot = vfs_.snapshot_for(ticket.path);
  const auto start_time = std::chrono::steady_clock::now();
  auto locations = semdb_.references_of(ticket.path, snapshot->buffer.offset_at(position));
  const std::uint64_t elapsed_microseconds = elapsed_microseconds_since(start_time);

  if (drop_reason_for(ticket) != RuntimeDropReason::None) {
    return {};
  }

  record_latency(runtime_counters_.references_latency, elapsed_microseconds);
  return locations;
}

std::vector<Location>
IdeService::references(const std::string& uri, Position position) {
  return references(begin_foreground_request(uri, RuntimeRequestKind::References), position);
}

std::vector<DocumentSymbol>
IdeService::document_symbols(const std::string& uri) {
  return semdb_.document_symbols(path_from_uri(uri));
}

std::vector<IndexedSymbol>
IdeService::workspace_symbols(const std::string& query) {
  refresh_dirty_open_indexes();
  return semdb_.workspace_symbols(query);
}

std::vector<std::uint32_t>
IdeService::semantic_tokens(const std::string& uri) {
  return semdb_.semantic_tokens_for(path_from_uri(uri));
}

std::shared_ptr<const DocumentSnapshot>
IdeService::snapshot_for_uri(const std::string& uri) {
  const auto snapshot = vfs_.snapshot_for(path_from_uri(uri));
  record_visible_snapshot(snapshot);
  return snapshot;
}

}  // namespace styio::ide
