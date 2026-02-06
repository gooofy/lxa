/**
 * rgbboxes_gtest.cpp - Google Test version of RGBBoxes test
 */

#include "lxa_test.h"

using namespace lxa::testing;

class RGBBoxesTest : public LxaTest {
protected:
    void SetUp() override {
        LxaTest::SetUp();
    }
};

TEST_F(RGBBoxesTest, RunsToCompletion) {
    int result = RunProgram("SYS:RGBBoxes", "", 10000);
    EXPECT_EQ(result, 0);
    
    std::string output = GetOutput();
    EXPECT_NE(output.find("Custom view loaded"), std::string::npos);
    EXPECT_NE(output.find("Done."), std::string::npos);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
