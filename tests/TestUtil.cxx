#include <gtest/gtest.h>

#include "Util/Tools.h"

TEST(ToolsTests, TestNoCaseCmp) {
  EXPECT_TRUE(util::NoCaseCmp("red", "RED"));
  EXPECT_TRUE(util::NoCaseCmp("rEd", "Red"));
  EXPECT_FALSE(util::NoCaseCmp("read", "rea"));
  EXPECT_TRUE(util::NoCaseCmp("RED", "red"));
  EXPECT_TRUE(util::NoCaseCmp("red", "red"));
  EXPECT_TRUE(util::NoCaseCmp("RED", "RED"));
  EXPECT_FALSE(util::NoCaseCmp("RED", "REDDER"));
  EXPECT_FALSE(util::NoCaseCmp("yellow", "purple"));
}