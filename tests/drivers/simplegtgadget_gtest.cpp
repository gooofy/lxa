/**
 * simplegtgadget_gtest.cpp - Google Test version of SimpleGTGadget test
 *
 * Tests GadTools gadget creation and interaction.
 */

#include "lxa_test.h"

using namespace lxa::testing;

/* Gadget positions from the sample:
 * BASE = 20 + WBorTop + (FontYSize + 1) = 20 + 11 + (8 + 1) = 40 (for lxa defaults)
 * Confirmed via log: "Created BUTTON_KIND gadget at 20,40 size 120x14"
 * 
 * Button:   (20, 40), size 120x14
 * Checkbox: (20, 60), size 26x11
 * Integer:  (80, 80), size 80x14   (LeftEdge was increased to 80 for PLACETEXT_LEFT)
 * Cycle:    (80, 100), size 120x14 (LeftEdge was increased to 80 for PLACETEXT_LEFT)
 */

class SimpleGTGadgetTest : public LxaUITest {
protected:
    void SetUp() override {
        LxaUITest::SetUp();
        
        ASSERT_EQ(lxa_load_program("SYS:SimpleGTGadget", ""), 0);
        ASSERT_TRUE(WaitForWindows(1, 5000));
        ASSERT_TRUE(GetWindowInfo(0, &window_info));
        
        // Wait for program to initialize, render, and flush startup output
        WaitForEventLoop(100, 10000);
        RunCyclesWithVBlank(30, 100000);
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
    // Button is at (20, 40) in the window, size 120x14.
    // Verify the interaction does not hang and the window still closes cleanly.
    int clickX = window_info.x + 20 + 60;  // center of 120px wide button
    int clickY = window_info.y + 40 + 7;   // center of 14px tall button
    
    Click(clickX, clickY);
    RunCyclesWithVBlank(20, 100000);
    Click(window_info.x + 5, window_info.y + 5);
    EXPECT_TRUE(lxa_wait_exit(3000)) << "Program should exit after closing window";
}

TEST_F(SimpleGTGadgetTest, ClickCheckbox) {
    // Checkbox is at (20, 60) in the window, size 26x11.
    // Verify the interaction does not hang and the window still closes cleanly.
    int clickX = window_info.x + 20 + 13;   // center of 26px wide gadget
    int clickY = window_info.y + 60 + 5;    // center of 11px tall gadget
    
    Click(clickX, clickY);
    RunCyclesWithVBlank(20, 100000);
    Click(window_info.x + 5, window_info.y + 5);
    EXPECT_TRUE(lxa_wait_exit(3000)) << "Program should exit after closing window";
}

TEST_F(SimpleGTGadgetTest, ClickCycle) {
    // Cycle is at (80, 100) in the window, size 120x14.
    // Verify the interaction does not hang and the window still closes cleanly.
    int clickX = window_info.x + 80 + 60;  // center of 120px wide gadget
    int clickY = window_info.y + 100 + 7;  // center of 14px tall gadget
    
    Click(clickX, clickY);
    RunCyclesWithVBlank(20, 100000);
    Click(window_info.x + 5, window_info.y + 5);
    EXPECT_TRUE(lxa_wait_exit(3000)) << "Program should exit after closing window";
}

TEST_F(SimpleGTGadgetTest, CloseWindow) {
    // Click close gadget (usually at top-left)
    Click(window_info.x + 5, window_info.y + 5);
    RunCyclesWithVBlank(20);
    
    // Program should exit now
    EXPECT_TRUE(lxa_wait_exit(2000)) << "Program should exit after closing window";
}

// ============================================================================
// Pixel verification tests (non-rootless mode)
// ============================================================================

class SimpleGTGadgetPixelTest : public LxaUITest {
protected:
    void SetUp() override {
        // Disable rootless so Intuition renders window frames to screen bitmap
        config.rootless = false;
        
        LxaUITest::SetUp();
        
        ASSERT_EQ(lxa_load_program("SYS:SimpleGTGadget", ""), 0);
        ASSERT_TRUE(WaitForWindows(1, 5000));
        ASSERT_TRUE(GetWindowInfo(0, &window_info));
        
        // Let rendering complete
        WaitForEventLoop(100, 10000);
        RunCyclesWithVBlank(50, 100000);
    }
};

TEST_F(SimpleGTGadgetPixelTest, CheckboxBorderRendered) {
    // Checkbox gadget at (20, 60) size 26x11 should have a bevel border
    // The bevel border adds visible non-background pixels around the checkbox area
    lxa_flush_display();
    
    int checkbox_content = CountContentPixels(
        window_info.x + 20,
        window_info.y + 60,
        window_info.x + 20 + 25,  // 26px wide
        window_info.y + 60 + 10,  // 11px tall
        0  // background pen
    );
    EXPECT_GT(checkbox_content, 0)
        << "Checkbox should have visible border/bevel pixels (non-background content)";
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
