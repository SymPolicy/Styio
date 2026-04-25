#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>

namespace styio::fuzz {

inline std::string
make_printable_source(const std::uint8_t* data, std::size_t size) {
  static const char kAlphabet[] =
    "abcdefghijklmnopqrstuvwxyz"
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "0123456789"
    "_#:@[]{}(),.+-*/%<>=?&|!^\" \n";

  std::string source;
  source.reserve(size);
  const std::size_t alphabet_size = sizeof(kAlphabet) - 1;
  for (std::size_t i = 0; i < size; ++i) {
    source.push_back(kAlphabet[data[i] % alphabet_size]);
  }
  return source;
}

inline std::string
temp_workspace_root(const std::string& name) {
  const std::filesystem::path root = std::filesystem::temp_directory_path() / name;
  std::filesystem::create_directories(root);
  return root.string();
}

}  // namespace styio::fuzz
