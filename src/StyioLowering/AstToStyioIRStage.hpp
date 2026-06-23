#pragma once
#ifndef STYIO_AST_TO_STYIO_IR_STAGE_H_
#define STYIO_AST_TO_STYIO_IR_STAGE_H_

#include "AstToStyioIRLowerer.hpp"

class StyioAST;
class StyioIR;

namespace styio::lowering {

StyioIR* lower_semantic_ast_to_styio_ir(StyioAST* ast, AstToStyioIRLowerer* lowerer);

}  // namespace styio::lowering

#endif  // STYIO_AST_TO_STYIO_IR_STAGE_H_
