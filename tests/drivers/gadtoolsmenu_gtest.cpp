/**
 * gadtoolsmenu_gtest.cpp - Google Test version of GadToolsMenu test
 *
 * Tests the GadToolsMenu sample which demonstrates GadTools menu creation.
 */

#include "lxa_test.h"

using namespace lxa::testing;

class GadToolsMenuTest : public LxaUITest {
protected:
    void SetUp() override {
        LxaUITest::SetUp();
        
        // Load the GadToolsMenu program
        ASSERT_EQ(lxa_load_program("SYS:GadToolsMenu", ""), 0) 
            << "Failed to load GadToolsMenu";
        
        // Wait for window to open
        ASSERT_TRUE(WaitForWindows(1, 5000)) 
            << "Window did not open within 5 seconds";
        
        // Get window information
        ASSERT_TRUE(GetWindowInfo(0, &window_info)) 
            << "Could not get window info";
        
        // Let task reach Wait()
        WaitForEventLoop(200, 10000);
        ClearOutput();
    }
};

TEST_F(GadToolsMenuTest, WindowOpens) {
    EXPECT_EQ(window_info.width, 400) << "Window width should be 400";
    EXPECT_EQ(window_info.height, 100) << "Window height should be 100";
}

TEST_F(GadToolsMenuTest, CloseGadget) {
    // Click close gadget
    int close_x = window_info.x + 10;
    int close_y = window_info.y + 5;
    
    Click(close_x, close_y);
    
    // Process event
    RunCyclesWithVBlank(20, 50000);
    
    // Wait for exit
    bool exited = lxa_wait_exit(5000);
    EXPECT_TRUE(exited) << "Program should exit cleanly via close gadget";
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
