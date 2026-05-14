#include <iostream>

#include "Server.hpp"

int
main() {
  styio::lsp::Server server;
  server.run(std::cin, std::cout);
  return 0;
}
