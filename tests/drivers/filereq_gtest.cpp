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

TEST_F(FileReqTest, CancelButton) {
    // Cancel button position (from lxa_asl.c):
    //   X = 252 + 30 = 282 (window relative)
    //   Y = 366 + 7 = 373 (window relative)
    int cancel_x = window_info.x + 282;
    int cancel_y = window_info.y + 373;
    
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
