#include <cstddef>
#include <cstdint>
#include <string>

#include "StyioException/Exception.hpp"
#include "StyioParser/Tokenizer.hpp"
#include "StyioSession/CompilationSession.hpp"

extern "C" int
LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  if (data == nullptr) {
    return 0;
  }

  try {
    StyioTokenStream stream = StyioTokenizer::tokenizeOwned(
      std::string(reinterpret_cast<const char*>(data), size));
    CompilationSession session;
    session.adopt_tokens(stream.release());
  } catch (const StyioLexError&) {
    // expected for malformed inputs
  } catch (const StyioBaseException&) {
    // keep fuzzing; parser/analyzer are not involved in this target
  } catch (...) {
    // let sanitizer/fuzzer report hard crashes; soft exceptions are ignored
  }
  return 0;
}
