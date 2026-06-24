#include <gtest/gtest.h>

#include "StyioSession/SymbolInterner.hpp"

using styio::session::SymbolInterner;
using styio::session::SymbolId;
using styio::session::kInvalidSymbolId;

TEST(SymbolInterner, Empty) {
  SymbolInterner si;
  EXPECT_EQ(si.size(), 0u);
  EXPECT_EQ(si.lookup("foo"), kInvalidSymbolId);
  EXPECT_FALSE(si.contains("foo"));
  EXPECT_EQ(si.resolve(kInvalidSymbolId), "<invalid>");
}

TEST(SymbolInterner, InternBasic) {
  SymbolInterner si;
  SymbolId a = si.intern("hello");
  SymbolId b = si.intern("world");
  EXPECT_NE(a, kInvalidSymbolId);
  EXPECT_NE(b, kInvalidSymbolId);
  EXPECT_NE(a, b);
  EXPECT_EQ(si.size(), 2u);
  EXPECT_EQ(si.resolve(a), "hello");
  EXPECT_EQ(si.resolve(b), "world");
}

TEST(SymbolInterner, InternIsIdempotent) {
  SymbolInterner si;
  SymbolId a1 = si.intern("hello");
  SymbolId a2 = si.intern("hello");
  EXPECT_EQ(a1, a2);
  EXPECT_EQ(si.size(), 1u);
}

TEST(SymbolInterner, Lookup) {
  SymbolInterner si;
  si.intern("hello");
  EXPECT_EQ(si.lookup("hello"), si.intern("hello"));
  EXPECT_EQ(si.lookup("nonexistent"), kInvalidSymbolId);
}

TEST(SymbolInterner, Contains) {
  SymbolInterner si;
  si.intern("test");
  EXPECT_TRUE(si.contains("test"));
  EXPECT_FALSE(si.contains("nope"));
}

TEST(SymbolInterner, TransparentLookup) {
  // Lookup with string_view (not string) works via transparent hash
  SymbolInterner si;
  si.intern("transparent");
  std::string_view sv = "transparent";
  EXPECT_TRUE(si.contains(sv));
  EXPECT_NE(si.lookup(sv), kInvalidSymbolId);
}

TEST(SymbolInterner, ResolveOutOfBounds) {
  SymbolInterner si;
  EXPECT_EQ(si.resolve(99999), "<invalid>");
}

TEST(SymbolInterner, Reserve) {
  SymbolInterner si;
  si.reserve(1000);
  for (int i = 0; i < 1000; ++i) {
    si.intern("sym_" + std::to_string(i));
  }
  EXPECT_EQ(si.size(), 1000u);
}

TEST(SymbolInterner, MultipleStrings) {
  SymbolInterner si;
  for (int i = 0; i < 500; ++i) {
    std::string s = "var_" + std::to_string(i);
    SymbolId id = si.intern(s);
    EXPECT_NE(id, kInvalidSymbolId);
    EXPECT_EQ(si.resolve(id), s);
  }
  EXPECT_EQ(si.size(), 500u);
  // Re-lookup all
  for (int i = 0; i < 500; ++i) {
    std::string s = "var_" + std::to_string(i);
    EXPECT_TRUE(si.contains(s));
    EXPECT_NE(si.lookup(s), kInvalidSymbolId);
  }
}

TEST(SymbolInterner, StringViewStability) {
  // After many insertions, previously obtained string_views must remain valid
  SymbolInterner si;
  std::vector<std::string_view> views;
  for (int i = 0; i < 200; ++i) {
    std::string s = "stable_" + std::to_string(i);
    SymbolId id = si.intern(s);
    views.push_back(si.resolve(id));
  }
  // Verify all views still correct
  for (int i = 0; i < 200; ++i) {
    std::string expected = "stable_" + std::to_string(i);
    EXPECT_EQ(views[i], expected);
  }
}
