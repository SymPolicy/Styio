#pragma once
#ifndef STYIO_IR_PORTABLE_CALLABLE_BODY_HPP_
#define STYIO_IR_PORTABLE_CALLABLE_BODY_HPP_

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "../StyioSema/SemaContext.hpp"

class StyioAST;

namespace styio::ir {

inline constexpr std::int64_t kPortableCallableBodySchemaVersion = 1;
inline constexpr const char* kPortableCallableBodyFormat =
  "styio.portable-styioir";
inline constexpr std::size_t kMaximumPortableCallableNodes = 65536;
inline constexpr std::size_t kMaximumPortableCallableInputs = 262144;
inline constexpr std::size_t kMaximumPortableCallableStringBytes =
  1024 * 1024;

using PortableCallableTypeTerm =
  StyioSemaContext::CallableTypeTerm;
using PortableCallableTypeConstraint =
  StyioSemaContext::CallableTypeConstraint;

struct PortableCallableSignature
{
  std::string name;
  std::vector<PortableCallableTypeTerm> params;
  PortableCallableTypeTerm result;
  std::vector<PortableCallableTypeConstraint> constraints;
};

using PortableCallableCatalog =
  std::unordered_map<std::string, PortableCallableSignature>;

struct PortableCallableParameter
{
  std::string name;
  PortableCallableTypeTerm type;
};

struct PortableCallableNode
{
  std::string opcode;
  std::vector<std::uint32_t> inputs;
  std::string symbol;
  std::string value;
  std::string operation;
  PortableCallableTypeTerm type;
  bool has_type = false;
};

struct PortableCallableBody
{
  std::int64_t schema_version =
    kPortableCallableBodySchemaVersion;
  std::string format = kPortableCallableBodyFormat;
  std::string name;
  bool is_block = false;
  bool is_unique = false;
  std::vector<PortableCallableParameter> params;
  PortableCallableTypeTerm result;
  std::vector<PortableCallableNode> nodes;
  std::uint32_t root = 0;
  std::string semantic_digest;
};

PortableCallableTypeTerm portable_callable_term_from_data_type(
  const StyioDataType& type
);

bool portable_callable_terms_equal(
  const PortableCallableTypeTerm& lhs,
  const PortableCallableTypeTerm& rhs
);

std::string portable_callable_term_canonical_text(
  const PortableCallableTypeTerm& term
);

PortableCallableBody build_portable_callable_body(
  StyioAST* definition,
  const PortableCallableSignature& signature
);

std::string verify_and_annotate_portable_callable_body(
  PortableCallableBody& body,
  const PortableCallableSignature& signature,
  const PortableCallableCatalog& catalog,
  bool require_encoded_types
);

std::unique_ptr<StyioAST> materialize_portable_callable_body(
  const PortableCallableBody& body
);

std::string portable_callable_body_semantic_text(
  const PortableCallableBody& body
);

}  // namespace styio::ir

#endif  // STYIO_IR_PORTABLE_CALLABLE_BODY_HPP_
