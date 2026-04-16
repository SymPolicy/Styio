#include <cstddef>
#include <cstdint>
#include <string>

#include "StyioIDE/Project.hpp"
#include "StyioIDE/SemDB.hpp"
#include "StyioIDE/VFS.hpp"
#include "fuzz_ide_common.hpp"

extern "C" int
LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
  if (data == nullptr) {
    return 0;
  }

  try {
    const std::string source = styio::fuzz::make_printable_source(data, size);
    const std::string root = styio::fuzz::temp_workspace_root("styio-ide-fuzz-completion");
    const std::string path = root + "/completion.styio";

    styio::ide::VirtualFileSystem vfs;
    styio::ide::Project project;
    project.set_root(root);
    styio::ide::SemanticDB semdb(vfs, project);
    semdb.configure_cache_root(project.cache_root());

    vfs.open(path, source, 1);
    const std::size_t offset = source.empty() ? 0 : static_cast<std::size_t>(data[0]) % (source.size() + 1);
    (void)semdb.completion_context_at(path, offset);
    (void)semdb.complete_at(path, offset);
    (void)semdb.hover_at(path, offset);
    (void)semdb.definition_at(path, offset);
    (void)semdb.references_of(path, offset);
  } catch (...) {
    // Keep fuzzing on soft parser/semantic failures; sanitizers handle hard faults.
  }

  return 0;
}
