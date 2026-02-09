/**
 * rgbboxes_gtest.cpp - Google Test version of RGBBoxes test
 *
 * Tests that the RGBBoxes sample (adapted from RKM):
 * 1. Creates a custom View/ViewPort with GetColorMap and LoadRGB4
 * 2. Draws colored boxes into bitplane memory
 * 3. Waits for Delay(10*TICKS_PER_SECOND) (~10 seconds)
 * 4. Cleans up and exits successfully
 */

#include "lxa_test.h"
#include <chrono>

using namespace lxa::testing;

class RGBBoxesTest : public LxaTest {
protected:
    void SetUp() override {
        LxaTest::SetUp();
    }
};

TEST_F(RGBBoxesTest, RunsToCompletion) {
    auto start = std::chrono::steady_clock::now();

    int result = RunProgram("SYS:RGBBoxes", "", 20000);

    auto end = std::chrono::steady_clock::now();
    auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

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

    /* Delay(10*TICKS_PER_SECOND) = ~10 seconds.
     * Allow generous range: at least 5 seconds, at most 18 seconds */
    EXPECT_GE(elapsed_ms, 5000)
        << "RGBBoxes completed too fast (" << elapsed_ms
        << "ms) - Delay() may not be waiting properly";
    EXPECT_LE(elapsed_ms, 18000)
        << "RGBBoxes took too long (" << elapsed_ms
        << "ms) - possible performance regression";

    printf("RGBBoxes elapsed time: %ldms (expected ~10000ms for Delay(10*TICKS_PER_SECOND))\n",
           (long)elapsed_ms);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
