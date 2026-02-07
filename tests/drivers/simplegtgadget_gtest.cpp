/**
 * simplegtgadget_gtest.cpp - Google Test version of SimpleGTGadget test
 *
 * Tests GadTools gadget creation and interaction.
 */

#include "lxa_test.h"

using namespace lxa::testing;

class SimpleGTGadgetTest : public LxaUITest {
protected:
    void SetUp() override {
        LxaUITest::SetUp();
        
        ASSERT_EQ(lxa_load_program("SYS:SimpleGTGadget", ""), 0);
        ASSERT_TRUE(WaitForWindows(1, 5000));
        ASSERT_TRUE(GetWindowInfo(0, &window_info));
        
        // Wait for program to initialize, render, and reach event loop
        WaitForEventLoop(50, 10000);
    }
};

TEST_F(SimpleGTGadgetTest, GadgetCreation) {
    std::string output = GetOutput();
    EXPECT_NE(output.find("Created BUTTON_KIND gadget"), std::string::npos);
    EXPECT_NE(output.find("Created CHECKBOX_KIND gadget"), std::string::npos);
    EXPECT_NE(output.find("Created INTEGER_KIND gadget"), std::string::npos);
    EXPECT_NE(output.find("Created CYCLE_KIND gadget"), std::string::npos);
}

TEST_F(SimpleGTGadgetTest, ClickButton) {
    printf("Window info: x=%d, y=%d, w=%d, h=%d\n", window_info.x, window_info.y, window_info.width, window_info.height);
    // Button is at (20, 32) in the window (verified via log)
    // We try a few Y coordinates around the button center
    bool success = false;
    for (int y_off : {35, 38, 41, 44}) {
        int clickX = window_info.x + 20 + 60; 
        int clickY = window_info.y + y_off;
        
        ClearOutput();
        lxa_inject_mouse(clickX, clickY, LXA_MOUSE_LEFT, LXA_EVENT_MOUSEBUTTON);
        RunCyclesWithVBlank(5);
        lxa_inject_mouse(clickX, clickY, 0, LXA_EVENT_MOUSEBUTTON);
        RunCyclesWithVBlank(10);
        
        std::string output = GetOutput();
        if (output.find("IDCMP_GADGETUP: gadget ID 1") != std::string::npos) {
            success = true;
            break;
        }
    }
    EXPECT_TRUE(success) << "Expected GADGETUP for button (ID 1)";
}

TEST_F(SimpleGTGadgetTest, ClickCycle) {
    // Cycle is at (20, 92) in the window
    bool success = false;
    for (int y_off : {95, 98, 101}) {
        int clickX = window_info.x + 20 + 60;
        int clickY = window_info.y + y_off;
        
        ClearOutput();
        lxa_inject_mouse(clickX, clickY, LXA_MOUSE_LEFT, LXA_EVENT_MOUSEBUTTON);
        RunCyclesWithVBlank(5);
        lxa_inject_mouse(clickX, clickY, 0, LXA_EVENT_MOUSEBUTTON);
        RunCyclesWithVBlank(10);
        
        std::string output = GetOutput();
        if (output.find("IDCMP_GADGETUP: gadget ID 4") != std::string::npos) {
            success = true;
            break;
        }
    }
    EXPECT_TRUE(success) << "Expected GADGETUP for cycle (ID 4)";
}

TEST_F(SimpleGTGadgetTest, CloseWindow) {
    // Click close gadget (usually at top-left)
    Click(window_info.x + 5, window_info.y + 5);
    RunCyclesWithVBlank(20);
    
    // Program should exit now
    EXPECT_TRUE(lxa_wait_exit(2000)) << "Program should exit after closing window";
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
