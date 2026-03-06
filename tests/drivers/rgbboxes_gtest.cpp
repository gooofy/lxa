/**
 * rgbboxes_gtest.cpp - Google Test version of RGBBoxes test
 *
 * Tests that the RGBBoxes sample (adapted from RKM):
 * 1. Creates a custom View/ViewPort with GetColorMap and LoadRGB4
 * 2. Draws colored boxes into bitplane memory
 * 3. Executes the Delay() path in the sample
 * 4. Cleans up and exits successfully
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
    int result = RunProgram("SYS:RGBBoxes", "", 20000);

    EXPECT_EQ(result, 0) << "RGBBoxes should exit cleanly";

    std::string output = GetOutput();

    /* Verify custom view creation */
    EXPECT_NE(output.find("Custom view loaded"), std::string::npos)
        << "Expected 'Custom view loaded' in output. Output: " << output;

    /* Verify ColorMap/LoadRGB4 path executed */
    EXPECT_NE(output.find("ColorMap loaded"), std::string::npos)
        << "Expected 'ColorMap loaded' in output. Output: " << output;

    /* Verify colored boxes were drawn */
    EXPECT_NE(output.find("Drew 3 colored boxes"), std::string::npos)
        << "Expected 'Drew 3 colored boxes' in output. Output: " << output;

    /* Verify clean completion */
    EXPECT_NE(output.find("Done."), std::string::npos)
        << "Expected 'Done.' in output. Output: " << output;

}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
