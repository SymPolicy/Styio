#pragma once
#ifndef STYIO_CALLABLE_INTERFACE_HPP_
#define STYIO_CALLABLE_INTERFACE_HPP_

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "SemaContext.hpp"

class MainBlockAST;
class StyioAST;

namespace styio::sema {

inline constexpr std::int64_t kCallableInterfaceSchemaVersion = 1;
inline constexpr std::string_view kCallableInterfaceFormat =
  "styio.callable-interface";

struct CallableInterfaceEntry
{
  std::string name;
  bool exported = false;
  bool has_scheme = false;
  StyioSemaContext::CallableTypeScheme scheme;
  StyioSemaContext::CallableEffectSummary effects;
  std::vector<StyioDataType> concrete_params;
  StyioDataType concrete_result{
    StyioDataTypeOption::Undefined, "undefined", 0
  };
  std::string checked_body;
  std::string checked_body_digest;
};

struct CallableModuleInterface
{
  std::int64_t schema_version = kCallableInterfaceSchemaVersion;
  std::string module_id;
  std::string compiler_abi;
  std::string source_digest;
  std::vector<std::string> dependency_modules;
  std::string dependency_digest;
  std::string abi_digest;
  std::vector<CallableInterfaceEntry> entries;
};

std::string callable_interface_sha256_hex(std::string_view text);

std::string callable_interface_dependency_digest(
  const std::vector<const CallableModuleInterface*>& dependencies
);

std::string callable_interface_abi_digest(
  const CallableModuleInterface& interface
);

CallableModuleInterface publish_callable_module_interface(
  std::string module_id,
  std::string_view source_text,
  std::string compiler_abi,
  MainBlockAST* ast,
  const StyioSemaContext& context,
  const std::vector<const CallableModuleInterface*>& dependencies
);

std::string serialize_callable_module_interface(
  const CallableModuleInterface& interface
);

CallableModuleInterface parse_callable_module_interface(
  std::string_view payload,
  std::string_view expected_module_id,
  std::string_view expected_source_text,
  std::string_view expected_compiler_abi,
  const std::vector<const CallableModuleInterface*>& dependencies
);

}  // namespace styio::sema

#endif  // STYIO_CALLABLE_INTERFACE_HPP_
