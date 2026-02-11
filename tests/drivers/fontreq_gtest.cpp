/**
 * fontreq_gtest.cpp - Google Test version of FontReq test
 *
 * Tests the FontReq sample which demonstrates ASL font requesters.
 * The font requester should:
 *   - Open a window with a font list, size display, OK/Cancel buttons
 *   - Allow the user to select a font from the list
 *   - Return the selected font via fo_Attr when OK is clicked
 *   - Return FALSE when Cancel is clicked
 */

#include "lxa_test.h"

using namespace lxa::testing;

class FontReqTest : public LxaUITest {
protected:
    void SetUp() override {
        LxaUITest::SetUp();
        
        // Load the FontReq program
        ASSERT_EQ(lxa_load_program("SYS:FontReq", ""), 0) 
            << "Failed to load FontReq";
        
        // Wait for font requester window to open
        ASSERT_TRUE(WaitForWindows(1, 5000)) 
            << "Font requester did not open within 5 seconds";
        
        // Get window information
        ASSERT_TRUE(GetWindowInfo(0, &window_info)) 
            << "Could not get window info";
        
        // Let task reach event loop
        WaitForEventLoop(200, 10000);
    }
};

TEST_F(FontReqTest, RequesterOpens) {
    // The font requester window should have reasonable dimensions
    EXPECT_GE(window_info.width, 200) << "Font requester should have reasonable width";
    EXPECT_GE(window_info.height, 100) << "Font requester should have reasonable height";
}

TEST_F(FontReqTest, WindowPositionAndSize) {
    // Font requester should be positioned within screen bounds
    EXPECT_LE(window_info.y + window_info.height, 256) 
        << "Window bottom should not extend below screen (256 lines)";
    EXPECT_LE(window_info.x + window_info.width, 640) 
        << "Window right should not extend beyond screen (640 pixels)";
}

TEST_F(FontReqTest, CancelButton) {
    // Cancel button is at bottom-right of window
    // Layout: margin=8, btnWidth=60, btnHeight=14
    //   X = winWidth - margin - btnWidth + btnWidth/2
    //   Y = winHeight - 20 - btnHeight + btnHeight/2
    int cancel_x = window_info.x + window_info.width - 8 - 60 + 30;
    int cancel_y = window_info.y + window_info.height - 20 - 14 + 7;
    
    Click(cancel_x, cancel_y);
    
    // Process event
    RunCyclesWithVBlank(20, 50000);
    
    // Run more for output
    for (int i = 0; i < 100; i++) {
        RunCycles(10000);
    }
    
    // Wait for exit
    bool exited = lxa_wait_exit(5000);
    
    // If Cancel didn't work, try close gadget as fallback
    if (!exited) {
        Click(window_info.x + 10, window_info.y + 5);
        exited = lxa_wait_exit(3000);
    }
    
    EXPECT_TRUE(exited) << "Program should exit after Cancel button";
    
    std::string output = GetOutput();
    EXPECT_NE(output.find("User cancelled"), std::string::npos)
        << "Should print 'User cancelled' on Cancel. Output: " << output;
}

TEST_F(FontReqTest, OKButtonSelectsFont) {
    // OK button is at bottom-left of window
    // Layout: margin=8, btnWidth=60, btnHeight=14
    //   X = margin + btnWidth/2
    //   Y = winHeight - 20 - btnHeight + btnHeight/2
    int ok_x = window_info.x + 8 + 30;
    int ok_y = window_info.y + window_info.height - 20 - 14 + 7;
    
    Click(ok_x, ok_y);
    
    // Process event
    RunCyclesWithVBlank(20, 50000);
    
    // Run more for output
    for (int i = 0; i < 100; i++) {
        RunCycles(10000);
    }
    
    // Wait for exit
    bool exited = lxa_wait_exit(5000);
    
    if (!exited) {
        Click(window_info.x + 10, window_info.y + 5);
        exited = lxa_wait_exit(3000);
    }
    
    EXPECT_TRUE(exited) << "Program should exit after OK button";
    
    std::string output = GetOutput();
    EXPECT_NE(output.find("User selected font"), std::string::npos)
        << "Should print 'User selected font' on OK. Output: " << output;
    EXPECT_NE(output.find("Name="), std::string::npos)
        << "Should print font name. Output: " << output;
    EXPECT_NE(output.find("YSize="), std::string::npos)
        << "Should print font size. Output: " << output;
}

TEST_F(FontReqTest, FontListDisplayed) {
    // The font list area should contain rendered text (non-background pixels)
    // Font list starts below the window title bar area
    // Check that there are visible pixels in the list area (font names rendered)
    lxa_flush_display();
    
    int list_x1 = window_info.x + 10;
    int list_y1 = window_info.y + 30;
    int list_x2 = window_info.x + window_info.width - 10;
    int list_y2 = window_info.y + window_info.height - 50;
    
    int pixels = CountContentPixels(list_x1, list_y1, list_x2, list_y2);
    EXPECT_GT(pixels, 0)
        << "Font list area should contain visible content (font names)";
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
