/**
 * rgbboxes_gtest.cpp - Google Test version of RGBBoxes test
 *
 * Tests that the RGBBoxes sample:
 * 1. Runs to completion successfully
 * 2. Actually waits for the expected duration (250 WaitTOF frames)
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
    
    int result = RunProgram("SYS:RGBBoxes", "", 15000);
    
    auto end = std::chrono::steady_clock::now();
    auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    
    EXPECT_EQ(result, 0) << "RGBBoxes should exit cleanly";
    
    std::string output = GetOutput();
    EXPECT_NE(output.find("Custom view loaded"), std::string::npos)
        << "Expected 'Custom view loaded' in output";
    EXPECT_NE(output.find("Done."), std::string::npos)
        << "Expected 'Done.' in output";
    
    // 250 WaitTOF frames at ~20ms each = ~5000ms expected
    // Allow generous range: at least 3 seconds, at most 12 seconds
    EXPECT_GE(elapsed_ms, 3000)
        << "RGBBoxes completed too fast (" << elapsed_ms << "ms) - WaitTOF may not be waiting properly";
    EXPECT_LE(elapsed_ms, 12000)
        << "RGBBoxes took too long (" << elapsed_ms << "ms) - possible performance regression";
    
    printf("RGBBoxes elapsed time: %ldms (expected ~5000ms for 250 WaitTOF frames)\n", (long)elapsed_ms);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
