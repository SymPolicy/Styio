#pragma once
#ifndef STYIO_SEMANTIC_ANALYSIS_H_
#define STYIO_SEMANTIC_ANALYSIS_H_

#include "SemaContext.hpp"

class StyioAST;

namespace styio::sema {

void require_semantic_analysis(StyioAST* ast, StyioSemaContext* context);

}  // namespace styio::sema

#endif  // STYIO_SEMANTIC_ANALYSIS_H_
