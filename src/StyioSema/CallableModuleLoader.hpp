#pragma once
#ifndef STYIO_CALLABLE_MODULE_LOADER_HPP_
#define STYIO_CALLABLE_MODULE_LOADER_HPP_

#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "CallableInterface.hpp"
#include "../StyioAST/AST.hpp"
#include "../StyioException/Exception.hpp"

class MainBlockAST;
class StyioSemaContext;

namespace styio::sema {

struct LoadedCallableModule
{
  std::string module_id;
  std::filesystem::path source_path;
  std::string source_text;
  CallableModuleInterface interface;
  std::vector<std::unique_ptr<StyioAST>> definition_owners;
  std::unordered_map<std::string, StyioAST*> definitions;
  std::unordered_set<std::string> direct_importers;
};

class CallableModuleGraph
{
public:
  void load_entry_imports(
    const std::filesystem::path& entry_source_path,
    MainBlockAST* entry_ast,
    std::string compiler_abi
  );

  void install_into(StyioSemaContext& context) const;

  const std::vector<const CallableModuleInterface*>&
  entry_dependencies() const {
    return entry_dependencies_;
  }

  const std::vector<std::unique_ptr<LoadedCallableModule>>&
  modules() const {
    return modules_;
  }

private:
  enum class LoadState
  {
    Visiting,
    Complete,
  };

  LoadedCallableModule* load_module(
    const std::filesystem::path& importer_directory,
    const std::string& import_path,
    const std::string& importer_module
  );

  std::filesystem::path resolve_source_path(
    const std::filesystem::path& importer_directory,
    const std::string& import_path
  ) const;

  std::string compiler_abi_;
  std::vector<std::unique_ptr<LoadedCallableModule>> modules_;
  std::unordered_map<std::string, LoadedCallableModule*> modules_by_path_;
  std::unordered_map<std::string, LoadState> load_states_;
  std::vector<const CallableModuleInterface*> entry_dependencies_;
};

}  // namespace styio::sema

#endif  // STYIO_CALLABLE_MODULE_LOADER_HPP_
