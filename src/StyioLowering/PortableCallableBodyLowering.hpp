#pragma once
#ifndef STYIO_LOWERING_PORTABLE_CALLABLE_BODY_HPP_
#define STYIO_LOWERING_PORTABLE_CALLABLE_BODY_HPP_

#include <memory>

#include "../StyioIR/PortableCallableBody.hpp"

class StyioAST;

namespace styio::ir {

PortableCallableBody build_portable_callable_body(
  StyioAST* definition,
  const PortableCallableSignature& signature
);

std::unique_ptr<StyioAST> materialize_portable_callable_body(
  const PortableCallableBody& body
);

}  // namespace styio::ir

#endif  // STYIO_LOWERING_PORTABLE_CALLABLE_BODY_HPP_
