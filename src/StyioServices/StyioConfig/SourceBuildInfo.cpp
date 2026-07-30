#include "CompilePlanContract.hpp"
#include "SourceBuildInfo.hpp"

#include "StyioParser/SymbolRegistry.hpp"

#include <cstdint>
#include <initializer_list>

#include "llvm/Support/JSON.h"

namespace styio::config {

namespace {

llvm::json::Array
component_paths(std::initializer_list<const char*> paths) {
  llvm::json::Array array;
  for (const char* path : paths) {
    array.push_back(path);
  }
  return array;
}

llvm::json::Array
source_channel_entries() {
  llvm::json::Array channels;
  channels.push_back(llvm::json::Object{{"name", "stable"}, {"branch", "stable"}});
  channels.push_back(llvm::json::Object{{"name", "nightly"}, {"branch", "nightly"}});
  return channels;
}

llvm::json::Array
controlled_components() {
  llvm::json::Array components;
  components.push_back(llvm::json::Object{
    {"id", "compiler_core"},
    {"cmake_targets", llvm::json::Array{"styio_frontend_core", "styio_core"}},
    {"paths", component_paths({
      "src/StyioToken/",
      "src/StyioUnicode/",
      "src/StyioParser/",
      "src/StyioAST/",
      "src/StyioSema/",
      "src/StyioLowering/",
      "src/StyioIR/",
      "src/StyioCodeGen/",
      "src/StyioJIT/",
      "src/StyioExtern/",
      "src/StyioSession/",
      "src/StyioToString/",
    })},
    {"description", "Official parser-to-codegen compiler graph consumed by spio build."},
  });
  components.push_back(llvm::json::Object{
    {"id", "std_symbols"},
    {"cmake_targets", llvm::json::Array{"styio_symbol_core"}},
    {"paths", component_paths({
      "src/StyioParser/SymbolRegistry.hpp",
      "src/StyioParser/SymbolRegistry.cpp",
      "share/styio/prelude/resources.styio",
    })},
    {"description", "Default symbol, builtin type, and macro registry used as the source-build override surface."},
  });
  components.push_back(llvm::json::Object{
    {"id", "runtime"},
    {"cmake_targets", llvm::json::Array{"styio_runtime_core"}},
    {"paths", component_paths({
      "src/StyioRuntime/",
      "src/StyioExtern/",
    })},
    {"description", "Runtime support and intrinsic lowering implementation linked into build-channel artifacts."},
  });
  components.push_back(llvm::json::Object{
    {"id", "services"},
    {"cmake_targets", llvm::json::Array{
      "styio_cli_contract_core",
      "styio_ide_core",
      "styio_lspd",
    }},
    {"paths", component_paths({
      "src/StyioServices/StyioCLI/",
      "src/StyioServices/StyioConfig/",
      "src/StyioServices/StyioIDE/",
      "src/StyioServices/StyioLSP/",
    })},
    {"description", "Consumer-neutral language service layer for package managers, IDEs, CI, and external tooling."},
  });
  components.push_back(llvm::json::Object{
    {"id", "macro_prelude"},
    {"cmake_targets", llvm::json::Array{"styio_symbol_core"}},
    {"paths", component_paths({
      "src/StyioParser/SymbolRegistry.hpp",
      "src/StyioParser/SymbolRegistry.cpp",
      "share/styio/prelude/resources.styio",
    })},
    {"description", "Default macro/prelude symbol layer; parser syntax stays fixed while this layer remains overridable."},
  });
  return components;
}

} // namespace

const char*
default_source_origin() {
  return "https://github.com/eBioRing/Styio.git";
}

const char*
source_branch_for_channel(const std::string& channel) {
  if (channel == "nightly") {
    return "nightly";
  }
  return "stable";
}

std::string
source_build_info_json(const SourceBuildInfoOptions& options) {
  llvm::json::Array macro_symbols;
  for (const auto& name : styio::symbols::default_symbol_names_by_origin(
         styio::symbols::RegistryOrigin::Macro)) {
    macro_symbols.push_back(name);
  }

  llvm::json::Object symbol_registry{
    {"version", static_cast<std::int64_t>(styio::symbols::default_symbol_registry_version())},
    {"path", "src/StyioParser/SymbolRegistry.cpp"},
    {"total_symbols", static_cast<std::int64_t>(styio::symbols::default_symbol_registry().size())},
    {"macro_like_symbols", std::move(macro_symbols)},
  };

  llvm::json::Object root{
    {"tool", "styio"},
    {"contract", "source-build-info"},
    {"source_layout_version", 1},
    {"compiler_version", options.compiler_version},
    {"binary_channel", options.compiler_channel},
    {"edition_max", options.edition_max},
    {"official_source_origin", default_source_origin()},
    {"source_channels", source_channel_entries()},
    {"source_overrides", llvm::json::Object{
      {"source_root", true},
      {"source_rev", true},
    }},
    {"public_build_entrypoints", llvm::json::Object{
      {"cmake_root", "CMakeLists.txt"},
      {"helper_script", "scripts/source-build-minimal.sh"},
      {"cmake_target", "styio"},
      {"compiler_binary", "styio"},
      {"compile_plan_consumer", "--compile-plan"},
      {"syntax_check_consumer", "check --syntax --json --file"},
    }},
    {"controlled_components", controlled_components()},
    {"symbol_registry", std::move(symbol_registry)},
    {"notes", llvm::json::Array{
      "machine-info remains the binary-channel handshake contract",
      "source-build-info describes Styio's own official source layout",
    }},
  };

  return llvm::formatv("{0:2}", llvm::json::Value(std::move(root))).str();
}

} // namespace styio::config
