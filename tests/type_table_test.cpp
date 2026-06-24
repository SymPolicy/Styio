#include <gtest/gtest.h>

#include "StyioSession/TypeTable.hpp"

using styio::session::TypeTable;
using styio::session::TypeId;
using styio::session::TypeKey;
using styio::session::kInvalidTypeId;

TEST(TypeTable, Empty) {
  TypeTable tt;
  EXPECT_EQ(tt.size(), 0u);
  EXPECT_EQ(tt.resolve(kInvalidTypeId).option, StyioDataTypeOption::Undefined);
}

TEST(TypeTable, InternBasic) {
  TypeTable tt;
  TypeKey k1{StyioDataTypeOption::Integer, 0, 64};
  TypeKey k2{StyioDataTypeOption::Float, 0, 64};

  TypeId a = tt.intern(k1);
  TypeId b = tt.intern(k2);
  EXPECT_NE(a, kInvalidTypeId);
  EXPECT_NE(b, kInvalidTypeId);
  EXPECT_NE(a, b);
  EXPECT_EQ(tt.size(), 2u);
}

TEST(TypeTable, InternIsIdempotent) {
  TypeTable tt;
  TypeKey k{StyioDataTypeOption::Bool, 0, 1};
  TypeId a1 = tt.intern(k);
  TypeId a2 = tt.intern(k);
  EXPECT_EQ(a1, a2);
  EXPECT_EQ(tt.size(), 1u);
}

TEST(TypeTable, Equals) {
  TypeTable tt;
  TypeKey ki64{StyioDataTypeOption::Integer, 0, 64};
  TypeKey kf64{StyioDataTypeOption::Float, 0, 64};

  TypeId i64 = tt.intern(ki64);
  TypeId f64 = tt.intern(kf64);
  EXPECT_TRUE(tt.equals(i64, i64));
  EXPECT_TRUE(tt.equals(f64, f64));
  EXPECT_FALSE(tt.equals(i64, f64));
}

TEST(TypeTable, Builtins) {
  TypeTable tt;
  tt.register_builtins();
  EXPECT_NE(tt.builtin_i64(), kInvalidTypeId);
  EXPECT_NE(tt.builtin_f64(), kInvalidTypeId);
  EXPECT_NE(tt.builtin_bool(), kInvalidTypeId);
  EXPECT_NE(tt.builtin_string(), kInvalidTypeId);
  EXPECT_NE(tt.builtin_void(), kInvalidTypeId);
  EXPECT_GT(tt.size(), 4u);
}

TEST(TypeTable, ResolveRoundTrip) {
  TypeTable tt;
  TypeKey k{StyioDataTypeOption::String, 0, 0};
  TypeId id = tt.intern(k);
  const TypeKey& resolved = tt.resolve(id);
  EXPECT_EQ(resolved.option, StyioDataTypeOption::String);
  EXPECT_EQ(resolved.bit_width, 0u);
}

TEST(TypeTable, DifferentBitWidthsAreDifferentTypes) {
  TypeTable tt;
  TypeKey i32{StyioDataTypeOption::Integer, 0, 32};
  TypeKey i64{StyioDataTypeOption::Integer, 0, 64};
  TypeId a = tt.intern(i32);
  TypeId b = tt.intern(i64);
  EXPECT_NE(a, b);
}

TEST(TypeTable, SameKeyProducesSameId) {
  TypeTable tt;
  TypeKey k1{StyioDataTypeOption::Integer, 0, 64};
  TypeKey k2{StyioDataTypeOption::Integer, 0, 64};
  EXPECT_EQ(tt.intern(k1), tt.intern(k2));
}
