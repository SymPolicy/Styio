#include <gtest/gtest.h>

#include <filesystem>
#include <system_error>

#include "StyioPlatform/Platform.hpp"

namespace {

namespace fs = std::filesystem;

TEST(StyioPlatform, CurrentExecutablePathIsAbsoluteAndExists) {
  const fs::path executable = styio::platform::current_executable_path();

  ASSERT_FALSE(executable.empty());
  EXPECT_TRUE(executable.is_absolute());

  std::error_code ec;
  EXPECT_TRUE(fs::exists(executable, ec));
  EXPECT_FALSE(ec);
  EXPECT_TRUE(fs::is_regular_file(executable, ec));
  EXPECT_FALSE(ec);
}

TEST(StyioPlatform, CurrentExecutableDirectoryMatchesExecutablePath) {
  const fs::path executable = styio::platform::current_executable_path();
  const fs::path directory = styio::platform::current_executable_dir();

  ASSERT_FALSE(executable.empty());
  ASSERT_FALSE(directory.empty());
  EXPECT_EQ(directory, executable.parent_path());

  std::error_code ec;
  EXPECT_TRUE(fs::is_directory(directory, ec));
  EXPECT_FALSE(ec);
}

TEST(StyioPlatform, SharedLibrarySuffixMatchesOperatingSystem) {
#if defined(_WIN32)
  constexpr const char* expected_suffix = ".dll";
#elif defined(__APPLE__)
  constexpr const char* expected_suffix = ".dylib";
#else
  constexpr const char* expected_suffix = ".so";
#endif

  EXPECT_STREQ(styio::platform::shared_library_suffix(), expected_suffix);
}

}  // namespace
