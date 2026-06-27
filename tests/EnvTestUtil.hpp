#pragma once
#ifndef STYIO_TESTS_ENV_TEST_UTIL_HPP_
#define STYIO_TESTS_ENV_TEST_UTIL_HPP_

#include <cstdlib>

inline void
styio_test_setenv(const char* name, const char* value, int overwrite) {
#if defined(_WIN32)
  if (!overwrite && std::getenv(name) != nullptr) {
    return;
  }
  (void)_putenv_s(name, value != nullptr ? value : "");
#else
  (void)setenv(name, value, overwrite);
#endif
}

inline void
styio_test_unsetenv(const char* name) {
#if defined(_WIN32)
  (void)_putenv_s(name, "");
#else
  (void)unsetenv(name);
#endif
}

#endif  // STYIO_TESTS_ENV_TEST_UTIL_HPP_
