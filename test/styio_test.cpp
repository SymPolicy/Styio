// [C++ STL]
#include <filesystem>

// [Google Test]
#include <gtest/gtest.h>

void walkdirs() {
  using dir_iter = std::filesystem::recursive_directory_iterator;

  auto test_path = std::filesystem::current_path()/"test/example/demo";

  if (std::filesystem::exists(test_path)) {
    for (const auto& dirEntry : dir_iter(test_path)) {
      if (std::filesystem::is_regular_file(dirEntry)) {
        std::cout << dirEntry << std::endl;
      }
    }
  }
}


int add(int a, int b) {
  return a + b;
}

TEST(sum, simple) {
  EXPECT_EQ(add(2, 3), 5);
}

int main(int argc, char **argv) {
  walkdirs();

  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}