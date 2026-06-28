#pragma once

#ifndef STYIO_LSP_SERVER_HPP_
#define STYIO_LSP_SERVER_HPP_

#include <istream>
#include <unordered_map>
#include <optional>
#include <ostream>
#include <string>
#include <vector>

#include "StyioServices/StyioIDE/Service.hpp"
#include "llvm/Support/JSON.h"

#ifndef STYIO_IDE_INTERNAL_ACCESS
#define STYIO_IDE_INTERNAL_ACCESS private
#endif

namespace styio::lsp {

struct OutboundMessage
{
  llvm::json::Object payload;
  bool is_notification = false;
};

class Server
{
STYIO_IDE_INTERNAL_ACCESS:
  styio::ide::IdeService service_;
  std::unordered_map<std::string, std::vector<styio::ide::Diagnostic>> diagnostics_cache_;
  std::vector<OutboundMessage> pending_notifications_;
  std::string initialize_requested_root_uri_;
  std::string initialize_selected_root_uri_;
  std::vector<std::string> initialize_workspace_folders_;
  std::vector<std::string> initialize_ignored_workspace_folders_;

  llvm::json::Object make_diagnostic_notification(
    const std::string& uri,
    const styio::ide::TextBuffer& buffer,
    const std::vector<styio::ide::Diagnostic>& diagnostics);
  llvm::json::Object make_initialize_workspace_state() const;

public:
  std::vector<OutboundMessage> handle(llvm::json::Object request);
  std::vector<OutboundMessage> drain_runtime();
  std::vector<OutboundMessage> drain_runtime(std::size_t max_documents);
  const styio::ide::RuntimeCounters& runtime_counters() const;
  void run(std::istream& input, std::ostream& output);
};

}  // namespace styio::lsp

#endif
