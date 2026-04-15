#pragma once

#ifndef STYIO_LSP_SERVER_HPP_
#define STYIO_LSP_SERVER_HPP_

#include <istream>
#include <optional>
#include <ostream>
#include <string>
#include <vector>

#include "../StyioIDE/Service.hpp"
#include "llvm/Support/JSON.h"

namespace styio::lsp {

struct OutboundMessage
{
  llvm::json::Object payload;
  bool is_notification = false;
};

class Server
{
private:
  styio::ide::IdeService service_;
  std::vector<OutboundMessage> pending_notifications_;

  llvm::json::Object make_diagnostic_notification(
    const std::string& uri,
    const styio::ide::TextBuffer& buffer,
    const std::vector<styio::ide::Diagnostic>& diagnostics);

public:
  std::vector<OutboundMessage> handle(llvm::json::Object request);
  void run(std::istream& input, std::ostream& output);
};

}  // namespace styio::lsp

#endif
