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

#if defined(__linux__)
#include <unistd.h>
#elif defined(__APPLE__)
#include <mach/mach.h>
#endif

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

size_t
read_rss_bytes() {
#if defined(__linux__)
  std::ifstream in("/proc/self/statm");
  if (!in) {
    return 0;
  }
  long total_pages = 0;
  long resident_pages = 0;
  in >> total_pages >> resident_pages;
  if (!in || resident_pages <= 0) {
    return 0;
  }
  const long page_size = sysconf(_SC_PAGESIZE);
  if (page_size <= 0) {
    return 0;
  }
  return static_cast<size_t>(resident_pages) * static_cast<size_t>(page_size);
#elif defined(__APPLE__)
  mach_task_basic_info info{};
  mach_msg_type_number_t count = MACH_TASK_BASIC_INFO_COUNT;
  const kern_return_t rc = task_info(
    mach_task_self(),
    MACH_TASK_BASIC_INFO,
    reinterpret_cast<task_info_t>(&info),
    &count);
  if (rc != KERN_SUCCESS) {
    return 0;
  }
  return static_cast<size_t>(info.resident_size);
#else
  return 0;
#endif
}

fs::path
make_temp_line_file(const std::string& prefix, int lines) {
  const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
  const fs::path path = fs::temp_directory_path() / (prefix + std::to_string(stamp) + ".txt");
  std::ofstream out(path);
  if (!out.good()) {
    return {};
  }
  for (int i = 0; i < lines; ++i) {
    out << i << "\n";
  }
  return path;
}

struct TempFileGuard {
  fs::path path;

  ~TempFileGuard() {
    if (path.empty()) {
      return;
    }
    std::error_code ec;
    fs::remove(path, ec);
  }
};

::testing::AssertionResult
run_file_cycle(const fs::path& path, int lines, int iter) {
  styio_runtime_clear_error();
  const std::string raw = path.string();
  const int64_t h = styio_file_open(raw.c_str());
  if (h == 0) {
    return ::testing::AssertionFailure() << "open failed, iter=" << iter;
  }

  int read_lines = 0;
  while (const char* line = styio_file_read_line(h)) {
    ++read_lines;
    (void)styio_cstr_to_i64(line);
  }
  if (read_lines != lines) {
    styio_file_close(h);
    return ::testing::AssertionFailure()
           << "line mismatch, iter=" << iter << ", got=" << read_lines
           << ", want=" << lines;
  }

  styio_file_rewind(h);
  const char* first = styio_file_read_line(h);
  if (first == nullptr) {
    styio_file_close(h);
    return ::testing::AssertionFailure() << "rewind/read failed, iter=" << iter;
  }
  if (styio_cstr_to_i64(first) != 0) {
    styio_file_close(h);
    return ::testing::AssertionFailure()
           << "first line mismatch, iter=" << iter
           << ", got=" << styio_cstr_to_i64(first);
  }

  styio_file_close(h);
  styio_file_close(h); // idempotent close
  if (styio_runtime_has_error() != 0) {
    return ::testing::AssertionFailure()
           << "runtime error flag set, iter=" << iter;
  }
  return ::testing::AssertionSuccess();
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

  TempFileGuard tmp{make_temp_line_file("styio_soak_", lines)};
  ASSERT_FALSE(tmp.path.empty());

  for (int i = 0; i < loops; ++i) {
    ASSERT_TRUE(run_file_cycle(tmp.path, lines, i));
  }
}

TEST(StyioSoakSingleThread, FileHandleMemoryGrowthBound) {
  const int loops = read_env_i32("STYIO_SOAK_MEM_ITERS", 600, 50, 500000);
  const int lines = read_env_i32("STYIO_SOAK_MEM_FILE_LINES", 96, 8, 200000);
  const int limit_kib =
    read_env_i32("STYIO_SOAK_RSS_GROWTH_LIMIT_KIB", 65536, 4096, 1024 * 1024);

  TempFileGuard tmp{make_temp_line_file("styio_soak_mem_", lines)};
  ASSERT_FALSE(tmp.path.empty());

  const size_t rss_before = read_rss_bytes();
  if (rss_before == 0) {
    GTEST_SKIP() << "rss probe unavailable on this platform";
  }

  for (int i = 0; i < loops; ++i) {
    ASSERT_TRUE(run_file_cycle(tmp.path, lines, i));
  }

  const size_t rss_after = read_rss_bytes();
  if (rss_after == 0) {
    GTEST_SKIP() << "rss probe unavailable on this platform";
  }

  const size_t growth = (rss_after > rss_before) ? (rss_after - rss_before) : 0;
  const size_t limit_bytes = static_cast<size_t>(limit_kib) * 1024ULL;
  EXPECT_LE(growth, limit_bytes)
    << "rss_before=" << rss_before
    << " rss_after=" << rss_after
    << " growth=" << growth
    << " limit=" << limit_bytes;
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
