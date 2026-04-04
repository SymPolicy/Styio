#include <gtest/gtest.h>

#include <array>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "StyioExtern/ExternLib.hpp"
#include "StyioParser/Tokenizer.hpp"
#include "StyioToken/Token.hpp"

namespace fs = std::filesystem;

#ifndef STYIO_SOURCE_DIR
#define STYIO_SOURCE_DIR "."
#endif

#ifndef STYIO_COMPILER_EXE
#define STYIO_COMPILER_EXE ""
#endif

namespace {

int
read_env_i32(const char* key, int fallback, int min_v, int max_v) {
  const char* raw = std::getenv(key);
  if (raw == nullptr || raw[0] == '\0') {
    return fallback;
  }
  char* end = nullptr;
  long v = std::strtol(raw, &end, 10);
  if (end == raw || *end != '\0') {
    return fallback;
  }
  if (v < min_v) {
    return min_v;
  }
  if (v > max_v) {
    return max_v;
  }
  return static_cast<int>(v);
}

void
normalize_text(std::string& s) {
  while (!s.empty() && (s.back() == '\n' || s.back() == '\r')) {
    s.pop_back();
  }
  s.push_back('\n');
}

std::string
read_text_file(const fs::path& path) {
  std::ifstream in(path);
  if (!in) {
    return {};
  }
  std::ostringstream ss;
  ss << in.rdbuf();
  return ss.str();
}

std::string
capture_stdout(const std::string& cmd) {
  std::array<char, 4096> buf{};
  std::string out;
  FILE* pipe = popen(cmd.c_str(), "r");
  if (pipe == nullptr) {
    return {};
  }
  while (fgets(buf.data(), static_cast<int>(buf.size()), pipe) != nullptr) {
    out += buf.data();
  }
  pclose(pipe);
  return out;
}

} // namespace

TEST(StyioSoakSingleThread, TokenizerIngestionLoop) {
  const int loops = read_env_i32("STYIO_SOAK_LEXER_ITERS", 120, 1, 200000);
  const int lines = read_env_i32("STYIO_SOAK_INGEST_LINES", 256, 16, 200000);

  std::string src;
  src.reserve(static_cast<size_t>(lines) * 24);
  for (int i = 0; i < lines; ++i) {
    src += "x" + std::to_string(i) + " = " + std::to_string(i) + "\n";
  }

  size_t total_tokens = 0;
  for (int i = 0; i < loops; ++i) {
    auto tokens = StyioTokenizer::tokenize(src);
    ASSERT_FALSE(tokens.empty());
    ASSERT_NE(tokens.back(), nullptr);
    EXPECT_EQ(tokens.back()->type, StyioTokenType::TOK_EOF);
    total_tokens += tokens.size();
    for (auto* tok : tokens) {
      delete tok;
    }
  }

  EXPECT_GT(total_tokens, static_cast<size_t>(loops) * 10);
}

TEST(StyioSoakSingleThread, FileHandleLifecycleLoop) {
  const int loops = read_env_i32("STYIO_SOAK_FILE_ITERS", 200, 1, 500000);
  const int lines = read_env_i32("STYIO_SOAK_FILE_LINES", 64, 1, 100000);

  const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
  const fs::path tmp = fs::temp_directory_path() / ("styio_soak_" + std::to_string(stamp) + ".txt");

  {
    std::ofstream out(tmp);
    ASSERT_TRUE(out.good());
    for (int i = 0; i < lines; ++i) {
      out << i << "\n";
    }
  }

  for (int i = 0; i < loops; ++i) {
    styio_runtime_clear_error();
    const std::string path = tmp.string();
    const int64_t h = styio_file_open(path.c_str());
    ASSERT_NE(h, 0) << "iter=" << i;

    int read_lines = 0;
    while (const char* line = styio_file_read_line(h)) {
      ++read_lines;
      (void)styio_cstr_to_i64(line);
    }
    EXPECT_EQ(read_lines, lines) << "iter=" << i;

    styio_file_rewind(h);
    const char* first = styio_file_read_line(h);
    ASSERT_NE(first, nullptr) << "iter=" << i;
    EXPECT_EQ(styio_cstr_to_i64(first), 0) << "iter=" << i;

    styio_file_close(h);
    styio_file_close(h); // idempotent close
    EXPECT_EQ(styio_runtime_has_error(), 0) << "iter=" << i;
  }

  fs::remove(tmp);
}

TEST(StyioSoakSingleThread, StreamProgramLoop) {
  const int loops = read_env_i32("STYIO_SOAK_STREAM_ITERS", 20, 1, 100000);
  const fs::path src = fs::path(STYIO_SOURCE_DIR) / "tests" / "milestones" / "m6" / "t02_running_max.styio";
  const fs::path exp = fs::path(STYIO_SOURCE_DIR) / "tests" / "milestones" / "m6" / "expected" / "t02_running_max.out";

  const char* runner = std::getenv("STYIO_COMPILER_EXE");
  if (runner == nullptr || runner[0] == '\0') {
    runner = STYIO_COMPILER_EXE;
  }

  ASSERT_TRUE(runner != nullptr && runner[0] != '\0');
  ASSERT_TRUE(fs::exists(src));
  ASSERT_TRUE(fs::exists(exp));

  std::string expected = read_text_file(exp);
  ASSERT_FALSE(expected.empty());
  normalize_text(expected);

  const std::string cmd = std::string("\"") + runner + "\" --file \"" + src.string() + "\" 2>/dev/null";

  for (int i = 0; i < loops; ++i) {
    std::string out = capture_stdout(cmd);
    normalize_text(out);
    EXPECT_EQ(out, expected) << "iter=" << i;
  }
}
