#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "StyioException/Exception.hpp"
#include "StyioParser/Tokenizer.hpp"

extern "C" int
LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  if (data == nullptr) {
    return 0;
  }

  std::string src(reinterpret_cast<const char*>(data), size);
  try {
    std::vector<StyioToken*> tokens = StyioTokenizer::tokenize(src);
    for (auto* t : tokens) {
      delete t;
    }
  } catch (const StyioLexError&) {
    // expected for malformed inputs
  } catch (const StyioBaseException&) {
    // keep fuzzing; parser/analyzer are not involved in this target
  } catch (...) {
    // let sanitizer/fuzzer report hard crashes; soft exceptions are ignored
  }
  return 0;
}
