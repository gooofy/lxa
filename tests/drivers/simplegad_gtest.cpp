/**
 * simplegad_gtest.cpp - Google Test version of SimpleGad test
 *
 * This test driver uses Google Test + liblxa to:
 * 1. Start the SimpleGad sample (RKM original)
 * 2. Wait for the window to open
 * 3. Click the button and verify GADGETDOWN + GADGETUP (ID 3)
 * 4. Click the close gadget to close the window
 * 5. Verify the program exits cleanly
 */

#include "lxa_test.h"

using namespace lxa::testing;

// Gadget positions (from simplegad.c RKM original)
constexpr int BUTTON_LEFT = 20;
constexpr int BUTTON_TOP = 20;
constexpr int BUTTON_WIDTH = 100;
constexpr int BUTTON_HEIGHT = 50;

// Window title bar height (approximate)
constexpr int TITLE_BAR_HEIGHT = 11;

class SimpleGadTest : public LxaUITest {
protected:
    void SetUp() override {
        LxaUITest::SetUp();
        
        // Load the SimpleGad program
        ASSERT_EQ(lxa_load_program("SYS:SimpleGad", ""), 0) 
            << "Failed to load SimpleGad";
        
        // Wait for window to open
        ASSERT_TRUE(WaitForWindows(1, 5000)) 
            << "Window did not open within 5 seconds";
        
        // Get window information
        ASSERT_TRUE(GetWindowInfo(0, &window_info)) 
            << "Could not get window info";
        
        // Let task reach WaitPort() - CRITICAL for event handling
        WaitForEventLoop(200, 10000);
        ClearOutput();
    }
};

TEST_F(SimpleGadTest, WindowOpens) {
    // Just verify the window opened successfully
    EXPECT_GT(window_info.width, 0);
    EXPECT_GT(window_info.height, 0);
}

TEST_F(SimpleGadTest, ButtonClick) {
    // Calculate button center
    int btn_x = window_info.x + BUTTON_LEFT + (BUTTON_WIDTH / 2);
    int btn_y = window_info.y + TITLE_BAR_HEIGHT + BUTTON_TOP + (BUTTON_HEIGHT / 2);
    
    // Click the button
    Click(btn_x, btn_y);
    
    // Process event through VBlanks
    RunCyclesWithVBlank(20, 50000);
    
    // Run additional cycles for output
    for (int i = 0; i < 100; i++) {
        RunCycles(10000);
    }
    
    // Check output
    std::string output = GetOutput();
    EXPECT_NE(output.find("IDCMP_GADGETDOWN"), std::string::npos) 
        << "Expected GADGETDOWN event";
    EXPECT_NE(output.find("IDCMP_GADGETUP"), std::string::npos) 
        << "Expected GADGETUP event";
    EXPECT_NE(output.find("gadget number 3"), std::string::npos) 
        << "Expected gadget ID 3";
}

TEST_F(SimpleGadTest, CloseWindow) {
    ClearOutput();
    
    // Click close gadget (top-left of window)
    int close_x = window_info.x + 10;
    int close_y = window_info.y + 5;
    
    Click(close_x, close_y);
    
    // Wait for exit
    ASSERT_TRUE(lxa_wait_exit(5000)) 
        << "Program did not exit within 5 seconds";
    
    // Verify CLOSEWINDOW event
    std::string output = GetOutput();
    EXPECT_NE(output.find("IDCMP_CLOSEWINDOW"), std::string::npos) 
        << "Expected CLOSEWINDOW event";
}

// Main function for Google Test
int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
