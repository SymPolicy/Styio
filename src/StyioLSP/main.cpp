#include <cstdio>
#include <iostream>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

#include "Server.hpp"

namespace {

bool
configure_stdio_binary() {
#ifdef _WIN32
  return _setmode(_fileno(stdin), _O_BINARY) != -1
         && _setmode(_fileno(stdout), _O_BINARY) != -1;
#else
  return true;
#endif
}

}  // namespace

int
main() {
  if (!configure_stdio_binary()) {
    std::cerr << "styio_lspd: failed to switch stdio to binary mode" << std::endl;
    return 1;
  }

  styio::lsp::Server server;
  server.run(std::cin, std::cout);
  return 0;
}
