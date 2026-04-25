#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size);

namespace {

std::vector<fs::path>
collect_inputs(const fs::path& path) {
  std::vector<fs::path> inputs;
  if (fs::is_regular_file(path)) {
    inputs.push_back(path);
    return inputs;
  }

  if (!fs::is_directory(path)) {
    return inputs;
  }

  for (const fs::directory_entry& entry : fs::directory_iterator(path)) {
    if (entry.is_regular_file()) {
      inputs.push_back(entry.path());
    }
  }
  std::sort(inputs.begin(), inputs.end());
  return inputs;
}

bool
read_file_bytes(const fs::path& path, std::vector<std::uint8_t>& bytes) {
  std::ifstream input(path, std::ios::binary);
  if (!input.is_open()) {
    return false;
  }

  bytes.assign(
    std::istreambuf_iterator<char>(input),
    std::istreambuf_iterator<char>());
  return true;
}

int
parse_runs(const char* raw) {
  try {
    const int runs = std::stoi(raw);
    return runs > 0 ? runs : 0;
  } catch (...) {
    return 0;
  }
}

}  // namespace

int
main(int argc, char** argv) {
  if (argc < 2 || argc > 3) {
    std::cerr << "Usage: " << argv[0] << " <corpus-path> [runs]\n";
    return 2;
  }

  const fs::path corpus_path(argv[1]);
  std::vector<fs::path> inputs = collect_inputs(corpus_path);
  if (inputs.empty()) {
    std::cerr << "No corpus inputs found at " << corpus_path << "\n";
    return 2;
  }

  int runs = static_cast<int>(inputs.size());
  if (argc == 3) {
    runs = parse_runs(argv[2]);
    if (runs <= 0) {
      std::cerr << "Invalid runs value: " << argv[2] << "\n";
      return 2;
    }
  }

  std::vector<std::uint8_t> bytes;
  static constexpr std::uint8_t kEmptyByte = 0;
  for (int i = 0; i < runs; ++i) {
    const fs::path& input = inputs[static_cast<std::size_t>(i) % inputs.size()];
    if (!read_file_bytes(input, bytes)) {
      std::cerr << "Unable to read corpus input: " << input << "\n";
      return 1;
    }
    const std::uint8_t* data = bytes.empty() ? &kEmptyByte : bytes.data();
    LLVMFuzzerTestOneInput(data, bytes.size());
  }

  return 0;
}
