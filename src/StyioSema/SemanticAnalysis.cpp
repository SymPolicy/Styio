#include "SemanticAnalysis.hpp"

#include "../StyioAST/AST.hpp"
#include "../StyioException/Exception.hpp"

namespace styio::sema {

void
require_semantic_analysis(StyioAST* ast, StyioSemaContext* context) {
  if (ast == nullptr) {
    throw StyioTypeError("semantic analysis requires a non-null AST");
  }
  if (context == nullptr) {
    throw StyioTypeError("semantic analysis requires a non-null semantic context");
  }
  ast->typeInfer(context);
}

}  // namespace styio::sema
