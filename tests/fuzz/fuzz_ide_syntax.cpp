#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>

#include "StyioIDE/Syntax.hpp"
#include "StyioIDE/VFS.hpp"
#include "fuzz_ide_common.hpp"

extern "C" int
LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
  if (data == nullptr) {
    return 0;
  }

  const std::string source = styio::fuzz::make_printable_source(data, size);
  const std::string path = styio::fuzz::temp_workspace_root("styio-ide-fuzz-syntax") + "/syntax.styio";

  styio::ide::VirtualFileSystem vfs;
  styio::ide::SyntaxParser parser;
  const auto snapshot = vfs.open(path, source, 1);
  (void)parser.parse(*snapshot);

  if (source.empty()) {
    return 0;
  }

  const std::size_t start = static_cast<std::size_t>(data[0]) % source.size();
  const std::size_t width = size > 1 ? static_cast<std::size_t>(data[1] % 4) : 0;
  const std::size_t end = std::min(start + width, source.size());
  const char replacement_char = size > 2 ? static_cast<char>('a' + (data[2] % 26)) : 'x';

  styio::ide::DocumentDelta delta;
  delta.edits.push_back(styio::ide::TextEdit{
    styio::ide::TextRange{start, end},
    std::string(1, replacement_char)});
  const auto update = vfs.update(path, delta, 2);
  if (update.snapshot != nullptr) {
    (void)parser.parse(*update.snapshot);
  }
  return 0;
}
