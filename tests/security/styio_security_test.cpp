/**
 * Security-focused regression tests for the Styio toolchain.
 *
 * These tests encode expectations around untrusted/malformed source input and
 * runtime helper behaviour. Some expectations document *current* behaviour
 * (e.g. exceptions) until the lexer reports structured errors.
 */

#include <gtest/gtest.h>

#include <chrono>
#include <cstring>
#include <cstdlib>
#include <future>
#include <stdexcept>
#include <string>
#include <vector>

#include "StyioException/Exception.hpp"
#include "StyioExtern/ExternLib.hpp"
#include "StyioParser/Tokenizer.hpp"
#include "StyioUnicode/Unicode.hpp"

namespace {

void
free_tokens(std::vector<StyioToken*>& tokens) {
  for (auto* t : tokens) {
    delete t;
  }
}

} // namespace

TEST(StyioSecurityLexer, EmptySourceProducesEof) {
  auto tokens = StyioTokenizer::tokenize("");
  ASSERT_FALSE(tokens.empty());
  EXPECT_EQ(tokens.back()->type, StyioTokenType::TOK_EOF);
  free_tokens(tokens);
}

TEST(StyioSecurityLexer, UnterminatedStringThrowsLexError) {
  // Unterminated string must produce a structured lexer error, not UB/crash.
  EXPECT_THROW(
    {
      auto tokens = StyioTokenizer::tokenize(R"(print "unterminated)");
      free_tokens(tokens);
    },
    StyioLexError);
}

TEST(StyioSecurityLexer, UnterminatedBlockCommentThrowsLexError) {
  EXPECT_THROW(
    {
      auto tokens = StyioTokenizer::tokenize("a /* no closing");
      free_tokens(tokens);
    },
    StyioLexError);
}

TEST(StyioSecurityLexer, LineCommentAtEofWithoutNewlineDoesNotThrow) {
  auto tokens = StyioTokenizer::tokenize("x // eof-no-newline");
  ASSERT_FALSE(tokens.empty());
  EXPECT_EQ(tokens.back()->type, StyioTokenType::TOK_EOF);
  free_tokens(tokens);
}

TEST(StyioSecurityLexer, EmbeddedNulByteDoesNotHang) {
  // If the inner switch hits `default: break` without advancing `loc`, tokenization
  // never terminates (denial-of-service). Expect completion within a short budget.
  std::string src = "a";
  src.push_back('\0');
  src += 'b';

  auto fut = std::async(std::launch::async, [&src] {
    return StyioTokenizer::tokenize(src);
  });
  const auto deadline = std::chrono::milliseconds(800);
  ASSERT_EQ(fut.wait_for(deadline), std::future_status::ready)
    << "Lexer should finish; hung input likely stuck on embedded NUL (loc not advanced).";

  auto tokens = fut.get();
  ASSERT_FALSE(tokens.empty());
  EXPECT_EQ(tokens.back()->type, StyioTokenType::TOK_EOF);
  free_tokens(tokens);
}

TEST(StyioSecurityLexer, VeryLongIdentifierCompletes) {
  std::string id(200'000, 'a');
  auto tokens = StyioTokenizer::tokenize(id);
  ASSERT_EQ(tokens.size(), 2u);
  EXPECT_EQ(tokens[0]->type, StyioTokenType::NAME);
  EXPECT_EQ(tokens[0]->original.size(), 200'000u);
  EXPECT_EQ(tokens[1]->type, StyioTokenType::TOK_EOF);
  free_tokens(tokens);
}

TEST(StyioSecurityUnicode, ByteClassificationIsStable) {
  EXPECT_TRUE(StyioUnicode::is_identifier_start('a'));
  EXPECT_TRUE(StyioUnicode::is_identifier_start('_'));
  EXPECT_FALSE(StyioUnicode::is_identifier_start('9'));

  EXPECT_TRUE(StyioUnicode::is_identifier_continue('9'));
  EXPECT_FALSE(StyioUnicode::is_identifier_continue('-'));

  EXPECT_TRUE(StyioUnicode::is_digit('7'));
  EXPECT_TRUE(StyioUnicode::is_space(' '));
  EXPECT_FALSE(StyioUnicode::is_space('x'));
}

TEST(StyioSecurityUnicode, DecodeUtf8CodepointBoundaries) {
  {
    std::string valid = "\xE4\xB8\xAD"; // U+4E2D
    std::uint32_t cp = 0;
    std::size_t width = 0;
    EXPECT_TRUE(StyioUnicode::decode_utf8_codepoint(valid, 0, cp, width));
    EXPECT_EQ(cp, 0x4E2Du);
    EXPECT_EQ(width, 3u);
  }

  {
    std::string overlong = "\xC0\xAF";
    std::uint32_t cp = 0;
    std::size_t width = 0;
    EXPECT_FALSE(StyioUnicode::decode_utf8_codepoint(overlong, 0, cp, width));
  }

  {
    std::string truncated = "\xF0\x9F\x92";
    std::uint32_t cp = 0;
    std::size_t width = 0;
    EXPECT_FALSE(StyioUnicode::decode_utf8_codepoint(truncated, 0, cp, width));
  }
}

TEST(StyioSecurityRuntime, StrcatAbAllocatesWithoutPairingFree) {
  // Document: each successful call returns malloc-backed memory; generated IR
  // does not emit free today — repeated use leaks (verify with ASan).
  const char* p = styio_strcat_ab("a", "b");
  ASSERT_NE(p, nullptr);
  ASSERT_STREQ(p, "ab");
  std::free(const_cast<char*>(p)); // Test harness cleanup only; JIT programs lack this.

  const char* chain = styio_strcat_ab("x", "y");
  const char* chain2 = styio_strcat_ab(chain, "z");
  std::free(const_cast<char*>(chain));
  std::free(const_cast<char*>(chain2));
}

TEST(StyioSecurityRuntime, StrcatAbHugeInputDoesNotCrash) {
  std::string a(40000, 'x');
  std::string b(40000, 'y');
  const char* p = styio_strcat_ab(a.c_str(), b.c_str());
  ASSERT_NE(p, nullptr);
  EXPECT_EQ(strlen(p), 80000u);
  std::free(const_cast<char*>(p));
}

TEST(StyioSecurityRuntime, MissingFileOpenReturnsZeroHandle) {
  const int64_t h = styio_file_open("/tmp/styio_missing_9b8fe8e2_7dfe_42ed_9ce2_4f9e587f7f6d.txt");
  EXPECT_EQ(h, 0);
}

TEST(StyioSecurityRuntime, InvalidHandleOperationsAreSafe) {
  styio_file_close(123456789);
  styio_file_rewind(123456789);
  EXPECT_EQ(styio_file_read_line(123456789), nullptr);
}

TEST(StyioSecurityRuntime, FreeCstrAcceptsNull) {
  styio_free_cstr(nullptr);
  SUCCEED();
}
