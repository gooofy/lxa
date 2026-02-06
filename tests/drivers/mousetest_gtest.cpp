/**
 * mousetest_gtest.cpp - Google Test version of MouseTest
 *
 * Tests mouse input handling via IDCMP.
 */

#include "lxa_test.h"

using namespace lxa::testing;

constexpr int TITLE_BAR_HEIGHT = 11;

class MouseTestTest : public LxaUITest {
protected:
    void SetUp() override {
        LxaUITest::SetUp();
        
        ASSERT_EQ(lxa_load_program("SYS:MouseTest", ""), 0);
        ASSERT_TRUE(WaitForWindows(1, 5000));
        ASSERT_TRUE(GetWindowInfo(0, &window_info));
        
        WaitForEventLoop(200, 10000);
        ClearOutput();
    }
};

TEST_F(MouseTestTest, WindowOpens) {
    EXPECT_GT(window_info.width, 0);
    EXPECT_GT(window_info.height, 0);
}

TEST_F(MouseTestTest, MouseMovement) {
    int move_x = window_info.x + 100;
    int move_y = window_info.y + TITLE_BAR_HEIGHT + 50;
    
    lxa_inject_mouse(move_x, move_y, 0, LXA_EVENT_MOUSEMOVE);
    RunCyclesWithVBlank(20, 50000);
    
    for (int i = 0; i < 100; i++) RunCycles(10000);
    
    std::string output = GetOutput();
    EXPECT_NE(output.find("MouseMove"), std::string::npos) 
        << "Expected MOUSEMOVE event";
}

TEST_F(MouseTestTest, LeftButtonClick) {
    ClearOutput();
    
    int click_x = window_info.x + 100;
    int click_y = window_info.y + TITLE_BAR_HEIGHT + 50;
    
    Click(click_x, click_y);
    RunCyclesWithVBlank(20, 50000);
    for (int i = 0; i < 100; i++) RunCycles(10000);
    
    std::string output = GetOutput();
    EXPECT_NE(output.find("Left Button"), std::string::npos) 
        << "Expected left button events";
}

TEST_F(MouseTestTest, RightButtonClick) {
    ClearOutput();
    
    int click_x = window_info.x + 100;
    int click_y = window_info.y + TITLE_BAR_HEIGHT + 50;
    
    lxa_inject_mouse_click(click_x, click_y, LXA_MOUSE_RIGHT);
    RunCyclesWithVBlank(20, 50000);
    for (int i = 0; i < 100; i++) RunCycles(10000);
    
    std::string output = GetOutput();
    EXPECT_NE(output.find("Right Button"), std::string::npos) 
        << "Expected right button events";
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
