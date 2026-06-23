#include "AstToStyioIRStage.hpp"

#include "../StyioAST/AST.hpp"
#include "../StyioException/Exception.hpp"

namespace styio::lowering {

StyioIR*
lower_semantic_ast_to_styio_ir(StyioAST* ast, AstToStyioIRLowerer* lowerer) {
  if (ast == nullptr) {
    throw StyioTypeError("StyioIR lowering requires a non-null AST");
  }
  if (lowerer == nullptr) {
    throw StyioTypeError("StyioIR lowering requires a non-null lowerer");
  }
  return ast->toStyioIR(lowerer);
}

}  // namespace styio::lowering
