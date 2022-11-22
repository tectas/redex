/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "TypeUtil.h"

#include "RedexTest.h"

class TypeUtilTest : public RedexTest {

 protected:
  const std::array<char, 9> PRIMS{
      {'Z', 'B', 'S', 'C', 'I', 'J', 'F', 'D', 'V'}};
};

TEST_F(TypeUtilTest, test_reference_type_wrappers) {
  EXPECT_EQ(type::get_boxed_reference_type(DexType::make_type("Z")),
            DexType::make_type("Ljava/lang/Boolean;"));
  EXPECT_EQ(type::get_boxed_reference_type(DexType::make_type("B")),
            DexType::make_type("Ljava/lang/Byte;"));
  EXPECT_EQ(type::get_boxed_reference_type(DexType::make_type("S")),
            DexType::make_type("Ljava/lang/Short;"));
  EXPECT_EQ(type::get_boxed_reference_type(DexType::make_type("C")),
            DexType::make_type("Ljava/lang/Character;"));
  EXPECT_EQ(type::get_boxed_reference_type(DexType::make_type("I")),
            DexType::make_type("Ljava/lang/Integer;"));
  EXPECT_EQ(type::get_boxed_reference_type(DexType::make_type("J")),
            DexType::make_type("Ljava/lang/Long;"));
  EXPECT_EQ(type::get_boxed_reference_type(DexType::make_type("F")),
            DexType::make_type("Ljava/lang/Float;"));
  EXPECT_EQ(type::get_boxed_reference_type(DexType::make_type("D")),
            DexType::make_type("Ljava/lang/Double;"));
}

TEST_F(TypeUtilTest, is_valid_empty) {
  using namespace type;

  EXPECT_FALSE(is_valid(""));
}

TEST_F(TypeUtilTest, is_valid_primitive) {
  using namespace type;

  for (char c : PRIMS) {
    std::string str(1, c);
    EXPECT_TRUE(is_valid(str)) << str;
    str.append("X");
    EXPECT_FALSE(is_valid(str)) << str;
  }
}

TEST_F(TypeUtilTest, is_valid_primitive_array) {
  using namespace type;

  EXPECT_FALSE(is_valid("["));

  std::string prefix = "[";

  for (char c : PRIMS) {
    std::string ok = prefix + c;
    EXPECT_TRUE(is_valid(ok)) << ok;

    std::string not_ok = ok + 'X';
    EXPECT_FALSE(is_valid(not_ok)) << not_ok;

    std::string nested_ok = prefix + ok;
    EXPECT_TRUE(is_valid(nested_ok)) << nested_ok;

    std::string nested_not_ok = nested_ok + 'X';
    EXPECT_FALSE(is_valid(nested_not_ok)) << nested_not_ok;
  }
}

namespace {

std::array<std::pair<std::string, bool>, 8> REF_SAMPLES = {
    std::make_pair<std::string, bool>("Foo", false),
    std::make_pair<std::string, bool>("LFoo", false),
    std::make_pair<std::string, bool>("LFoo;", true),
    std::make_pair<std::string, bool>("LFoo;;", false),

    std::make_pair<std::string, bool>("LFoo_Bar-Baz$A0123;", true),

    std::make_pair<std::string, bool>("Lfoo/bar/Baz;", true),
    std::make_pair<std::string, bool>("Lfoo;bar/Baz;", false),
    std::make_pair<std::string, bool>("Lfoo//Baz;", false),
};

} // namespace

TEST_F(TypeUtilTest, is_valid_reference) {
  using namespace type;

  for (const auto& p : REF_SAMPLES) {
    EXPECT_EQ(p.second, is_valid(p.first)) << p.first;
  }
}

TEST_F(TypeUtilTest, is_valid_reference_array) {
  using namespace type;

  std::string prefix = "[";

  for (const auto& p : REF_SAMPLES) {
    std::string single = prefix + p.first;
    EXPECT_EQ(p.second, is_valid(single)) << single;

    std::string not_ok = single + 'X';
    EXPECT_FALSE(is_valid(not_ok)) << not_ok;

    std::string nested = prefix + single;
    EXPECT_EQ(p.second, is_valid(nested)) << nested;

    std::string nested_not_ok = nested + 'X';
    EXPECT_FALSE(is_valid(nested_not_ok)) << nested_not_ok;
  }
}

TEST_F(TypeUtilTest, is_valid_array) {
  using namespace type;

  // Invalid arrays.
  EXPECT_FALSE(is_valid("["));
  EXPECT_FALSE(is_valid("[["));
  EXPECT_FALSE(is_valid("[o"));
  EXPECT_FALSE(is_valid("[L;"));
  EXPECT_FALSE(is_valid("[;"));
}

TEST_F(TypeUtilTest, check_cast_array) {
  using namespace type;

  EXPECT_FALSE(check_cast(DexType::make_type("[I"), DexType::make_type("[J")));
  EXPECT_FALSE(check_cast(DexType::make_type("[Z"), DexType::make_type("[B")));
  EXPECT_FALSE(check_cast(DexType::make_type("[F"), DexType::make_type("[D")));
  EXPECT_TRUE(check_cast(DexType::make_type("[I"),
                         DexType::make_type("Ljava/lang/Object;")));

  EXPECT_TRUE(check_cast(DexType::make_type("[Ljava/lang/Object;"),
                         DexType::make_type("[Ljava/lang/Object;")));
  EXPECT_TRUE(check_cast(DexType::make_type("[Ljava/lang/Object;"),
                         DexType::make_type("Ljava/lang/Object;")));
  EXPECT_TRUE(check_cast(DexType::make_type("[[Ljava/lang/Object;"),
                         DexType::make_type("[[Ljava/lang/Object;")));
  EXPECT_FALSE(check_cast(DexType::make_type("[Ljava/lang/Object;"),
                          DexType::make_type("[[Ljava/lang/Object;")));
  EXPECT_TRUE(check_cast(DexType::make_type("[[Ljava/lang/Object;"),
                         DexType::make_type("[Ljava/lang/Object;")));
}
