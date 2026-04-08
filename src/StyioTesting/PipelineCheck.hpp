#pragma once

#include <string>

namespace styio {
namespace testing {

/**
 * Run the five-layer compiler pipeline for a case directory and compare to goldens.
 *
 * Layout:
 *   <case_dir>/input.styio
 *   <case_dir>/expected/tokens.txt     — L1 lexer: kind + tab + escaped lexeme per line
 *   <case_dir>/expected/ast.txt        — L2 AST after type inference (StyioRepr)
 *   <case_dir>/expected/styio_ir.txt   — L3 IR (StyioRepr)
 *   <case_dir>/expected/llvm_ir.txt    — L4 LLVM module print (no banners)
 *   <case_dir>/expected/stdout.txt     — L5 program stdout (see layer5_compiler_exe)
 *   <case_dir>/expected/stderr.txt     — optional L5 program stderr
 *
 * L5 compares the **observable** process output. In-process JIT uses `printf`, which does not
 * go through `std::cout`, so when `layer5_compiler_exe` is non-null this layer runs
 * `layer5_compiler_exe --file <case_dir>/input.styio` and diffs stdout; when `stderr.txt` exists,
 * it diffs stderr as well. When null, L5 is skipped.
 *
 * @return Empty on success; otherwise a message naming the layer and the first mismatch.
 */
std::string run_pipeline_case(
  const std::string& case_dir,
  const char* layer5_compiler_exe = nullptr);

} // namespace testing
} // namespace styio
