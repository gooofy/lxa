/**
 * custompointer_gtest.cpp - Google Test version of CustomPointer test
 *
 * Tests the CustomPointer sample which demonstrates SetPointer/ClearPointer.
 */

#include "lxa_test.h"

using namespace lxa::testing;

class CustomPointerTest : public LxaUITest {
protected:
    void SetUp() override {
        LxaUITest::SetUp();
        
        // Load the CustomPointer program
        ASSERT_EQ(lxa_load_program("SYS:CustomPointer", ""), 0) 
            << "Failed to load CustomPointer";
        
        // Wait for window to open
        ASSERT_TRUE(WaitForWindows(1, 5000)) 
            << "Window did not open within 5 seconds";
        
        // Get window information
        ASSERT_TRUE(GetWindowInfo(0, &window_info)) 
            << "Could not get window info";
    }
};

TEST_F(CustomPointerTest, WindowOpens) {
    EXPECT_GT(window_info.width, 0) << "Window should have positive width";
    EXPECT_GT(window_info.height, 0) << "Window should have positive height";
}

TEST_F(CustomPointerTest, CompletesDelaySequence) {
    // The program does:
    //   Delay(50)   = 1 second
    //   Delay(100)  = 2 seconds
    //   Delay(100)  = 2 seconds
    // Total: ~5 seconds plus startup time
    // Give it 15 seconds to be safe
    
    bool exited = lxa_wait_exit(15000);
    EXPECT_TRUE(exited) 
        << "Program should complete Delay() sequences and exit within 15 seconds";
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
