#include <gtest/gtest.h>

#include "StyioUtil/SourceMap.hpp"

using styio::util::SourceMap;

TEST(SourceMap, EmptyFile) {
  SourceMap sm("");
  EXPECT_EQ(sm.line_count(), 1u);
  auto p = sm.position_at(0);
  EXPECT_EQ(p.line, 0u);
  EXPECT_EQ(p.character, 0u);
}

TEST(SourceMap, SingleLine) {
  SourceMap sm("hello world");
  EXPECT_EQ(sm.line_count(), 1u);
  auto p = sm.position_at(0);
  EXPECT_EQ(p.line, 0u);
  EXPECT_EQ(p.character, 0u);
  p = sm.position_at(5);
  EXPECT_EQ(p.line, 0u);
  EXPECT_EQ(p.character, 5u);
}

TEST(SourceMap, MultiLine) {
  SourceMap sm("line1\nline2\nline3");
  EXPECT_EQ(sm.line_count(), 3u);

  auto p = sm.position_at(0);
  EXPECT_EQ(p.line, 0u);
  EXPECT_EQ(p.character, 0u);

  p = sm.position_at(6);  // start of "line2"
  EXPECT_EQ(p.line, 1u);
  EXPECT_EQ(p.character, 0u);

  p = sm.position_at(12); // start of "line3"
  EXPECT_EQ(p.line, 2u);
  EXPECT_EQ(p.character, 0u);
}

TEST(SourceMap, TrailingNewline) {
  SourceMap sm("line1\nline2\n");
  EXPECT_EQ(sm.line_count(), 3u); // empty line at end

  auto p = sm.position_at(12); // position at the trailing empty line
  EXPECT_EQ(p.line, 2u);
  EXPECT_EQ(p.character, 0u);
}

TEST(SourceMap, OffsetBeyondEnd) {
  SourceMap sm("hello");
  auto p = sm.position_at(100);
  EXPECT_EQ(p.line, 0u);
  EXPECT_EQ(p.character, 5u); // clamped to text size
}

TEST(SourceMap, OneBasedQueries) {
  SourceMap sm("line1\nline2\nline3");
  auto p = sm.position_at_1based(0);
  EXPECT_EQ(p.first, 1u);
  EXPECT_EQ(p.second, 1u);

  p = sm.position_at_1based(6);
  EXPECT_EQ(p.first, 2u);
  EXPECT_EQ(p.second, 1u);
}

TEST(SourceMap, OffsetAt) {
  SourceMap sm("abc\ndef\n");
  EXPECT_EQ(sm.offset_at({0, 0}), 0u);
  EXPECT_EQ(sm.offset_at({0, 2}), 2u);
  EXPECT_EQ(sm.offset_at({1, 0}), 4u);
  EXPECT_EQ(sm.offset_at({1, 1}), 5u);
}

TEST(SourceMap, OffsetAtOutOfBounds) {
  SourceMap sm("abc\n");
  // Out of range line — returns text size
  EXPECT_EQ(sm.offset_at({5, 0}), 4u);
  // Out of range character — clamped to line end boundary (next line start)
  EXPECT_EQ(sm.offset_at({0, 100}), 4u);
}

TEST(SourceMap, LineText) {
  SourceMap sm("line1\nline2\nline3");
  EXPECT_EQ(sm.line_text(0), "line1");
  EXPECT_EQ(sm.line_text(1), "line2");
  EXPECT_EQ(sm.line_text(2), "line3");
  EXPECT_EQ(sm.line_text(100), "");
}

TEST(SourceMap, BuildLineSeps) {
  SourceMap sm("abc\ndef\nghi");
  auto seps = sm.build_line_seps();
  ASSERT_EQ(seps.size(), 3u);
  EXPECT_EQ(seps[0].first, 0u);
  EXPECT_EQ(seps[0].second, 4u); // "abc\n"
  EXPECT_EQ(seps[1].first, 4u);
  EXPECT_EQ(seps[1].second, 4u); // "def\n"
  EXPECT_EQ(seps[2].first, 8u);
  EXPECT_EQ(seps[2].second, 3u); // "ghi"
}

TEST(SourceMap, Rebuild) {
  SourceMap sm("old");
  EXPECT_EQ(sm.line_count(), 1u);
  sm.rebuild("new\ntext");
  EXPECT_EQ(sm.line_count(), 2u);
  EXPECT_EQ(sm.line_text(0), "new");
  EXPECT_EQ(sm.line_text(1), "text");
}

TEST(SourceMap, CarriageReturnInLineText) {
  SourceMap sm("line1\r\nline2");
  EXPECT_EQ(sm.line_text(0), "line1");  // \r stripped
  EXPECT_EQ(sm.line_text(1), "line2");
}

// ---------------------------------------------------------------------------
// Edge case tests for line_text() — prevents out-of-bounds access
// ---------------------------------------------------------------------------

TEST(SourceMap, LineTextEmptyInput) {
  SourceMap sm("");
  EXPECT_EQ(sm.line_text(0), "");
  EXPECT_EQ(sm.line_text(1), "");  // out-of-range
}

TEST(SourceMap, LineTextSingleChar_A) {
  SourceMap sm("a");
  EXPECT_EQ(sm.line_text(0), "a");
}

TEST(SourceMap, LineTextSingleChar_Newline) {
  SourceMap sm("\n");
  EXPECT_EQ(sm.line_text(0), "");
  EXPECT_EQ(sm.line_text(1), "");  // trailing empty line
}

TEST(SourceMap, LineTextSingleChar_CarriageReturn) {
  // \r alone is NOT a line terminator — it's part of the line content.
  SourceMap sm("\r");
  EXPECT_EQ(sm.line_text(0), "\r");
}

TEST(SourceMap, LineText_ANewline) {
  SourceMap sm("a\n");
  EXPECT_EQ(sm.line_text(0), "a");
  EXPECT_EQ(sm.line_text(1), "");
}

TEST(SourceMap, LineText_ACRLF) {
  SourceMap sm("a\r\n");
  EXPECT_EQ(sm.line_text(0), "a");
  EXPECT_EQ(sm.line_text(1), "");
}

TEST(SourceMap, LineText_ACRLF_B) {
  SourceMap sm("a\r\nb");
  EXPECT_EQ(sm.line_text(0), "a");
  EXPECT_EQ(sm.line_text(1), "b");
}

TEST(SourceMap, LineText_ABC) {
  SourceMap sm("abc");
  EXPECT_EQ(sm.line_text(0), "abc");
}

TEST(SourceMap, LineText_ABC_Newline) {
  SourceMap sm("abc\n");
  EXPECT_EQ(sm.line_text(0), "abc");
}

TEST(SourceMap, LineText_ABC_CRLF) {
  SourceMap sm("abc\r\n");
  EXPECT_EQ(sm.line_text(0), "abc");
}

TEST(SourceMap, LineText_CRLF_Only) {
  SourceMap sm("\r\n");
  EXPECT_EQ(sm.line_text(0), "");
  EXPECT_EQ(sm.line_text(1), "");
}

TEST(SourceMap, LineText_SingleChar_CR_NoNewline) {
  // CR without LF — not a line terminator, so it's part of the line
  SourceMap sm("x\ry");
  EXPECT_EQ(sm.line_count(), 1u);
  EXPECT_EQ(sm.line_text(0), "x\ry");
}

TEST(SourceMap, LineText_OffsetEqualsTextSize) {
  SourceMap sm("hello");
  auto p = sm.position_at(5);  // offset == text_.size()
  EXPECT_EQ(p.line, 0u);
  EXPECT_EQ(p.character, 5u);  // clamped to text size
}

TEST(SourceMap, LineText_OffsetAtPositionAtRoundtrip) {
  SourceMap sm("abc\ndef\nghi");
  for (std::size_t off = 0; off <= sm.line_text(0).size() + sm.line_text(1).size() + sm.line_text(2).size() + 5; ++off) {
    auto pos = sm.position_at(off);
    std::size_t back = sm.offset_at(pos);
    // offset_at(position_at(off)) should be bounded
    EXPECT_LE(back, sm.line_text(0).size() + sm.line_text(1).size() + sm.line_text(2).size() + 2);
  }
}

TEST(SourceMap, LineText_MultipleEmptyLines) {
  SourceMap sm("\n\n\n");
  EXPECT_EQ(sm.line_count(), 4u);
  EXPECT_EQ(sm.line_text(0), "");
  EXPECT_EQ(sm.line_text(1), "");
  EXPECT_EQ(sm.line_text(2), "");
  EXPECT_EQ(sm.line_text(3), "");
}

TEST(SourceMap, LineText_AllCarriageReturns) {
  SourceMap sm("\r\r\r");
  EXPECT_EQ(sm.line_count(), 1u);  // \r alone is not a line break
  EXPECT_EQ(sm.line_text(0), "\r\r\r");  // standalone \r is NOT stripped
}

TEST(SourceMap, LineText_MixedLineEndings) {
  SourceMap sm("a\r\nb\nc\r\nd");
  EXPECT_EQ(sm.line_count(), 4u);
  EXPECT_EQ(sm.line_text(0), "a");
  EXPECT_EQ(sm.line_text(1), "b");
  EXPECT_EQ(sm.line_text(2), "c");
  EXPECT_EQ(sm.line_text(3), "d");
}

TEST(SourceMap, LineText_EdgeCase_EndMinus2AtRisk) {
  // This specifically tests the old buggy pattern: text_[end-2] when end == 1
  // (single character '\n' with line_starts_ = [0, 1], line 0: start=0, end=1)
  SourceMap sm("\n");
  EXPECT_EQ(sm.line_text(0), "");
  EXPECT_EQ(sm.line_count(), 2u);
}
