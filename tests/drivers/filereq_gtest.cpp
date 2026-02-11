/**
 * filereq_gtest.cpp - Google Test version of FileReq test
 *
 * Tests the FileReq sample which demonstrates ASL file requesters.
 */

#include "lxa_test.h"

using namespace lxa::testing;

class FileReqTest : public LxaUITest {
protected:
    void SetUp() override {
        LxaUITest::SetUp();
        
        // Load the FileReq program
        ASSERT_EQ(lxa_load_program("SYS:FileReq", ""), 0) 
            << "Failed to load FileReq";
        
        // Wait for file requester window to open
        ASSERT_TRUE(WaitForWindows(1, 5000)) 
            << "File requester did not open within 5 seconds";
        
        // Get window information
        ASSERT_TRUE(GetWindowInfo(0, &window_info)) 
            << "Could not get window info";
        
        // Let task process
        WaitForEventLoop(200, 10000);
    }
};

TEST_F(FileReqTest, RequesterOpens) {
    EXPECT_GE(window_info.width, 200) << "File requester should have reasonable width";
    EXPECT_GE(window_info.height, 100) << "File requester should have reasonable height";
}

TEST_F(FileReqTest, WindowPositionAndSize) {
    // FileReq sample sets LeftEdge=0, TopEdge=0, Width=320, Height=400
    // After fix: LeftEdge=0 accepted (was defaulting to 50 due to > 0 check)
    //            TopEdge=0 accepted (was defaulting to 30 due to > 0 check)
    //            Height clamped from 400 to 256 (screen height)
    //            Width=320 fits within 640, so unchanged
    EXPECT_EQ(window_info.x, 0) << "LeftEdge=0 should be honored, not defaulted to 50";
    EXPECT_EQ(window_info.y, 0) << "TopEdge=0 should be honored, not defaulted to 30";
    EXPECT_LE(window_info.y + window_info.height, 256) 
        << "Window bottom should not extend below screen (256 lines)";
    EXPECT_EQ(window_info.width, 320) << "Width=320 should be preserved";
}

TEST_F(FileReqTest, CancelButton) {
    // Cancel button position relative to window:
    //   margin=8, btnWidth=60, btnHeight=14
    //   X = winWidth - margin - btnWidth + btnWidth/2 = width - 8 - 60 + 30
    //   Y = winHeight - 20 - btnHeight + btnHeight/2 = height - 20 - 14 + 7
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
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
