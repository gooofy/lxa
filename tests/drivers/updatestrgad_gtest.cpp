/**
 * updatestrgad_gtest.cpp - Google Test version of UpdateStrGad test
 *
 * Tests string gadget rendering and interaction.
 *
 * UpdateStrGad sample layout:
 *   - Window: 400x100 with WA_CloseGadget
 *   - String gadget: at (20, 20), 200x8
 *   - Border: at (-2, -2), draws 5-point rectangle around gadget
 *   - Buffer: "START" (5 chars), GACT_STRINGCENTER (text centered)
 *   - IDCMP: ACTIVEWINDOW, CLOSEWINDOW, GADGETUP
 */

#include "lxa_test.h"

using namespace lxa::testing;

// ============================================================================
// Pixel verification tests (non-rootless mode)
//
// Verify that string gadget contents ("START") are rendered to the screen.
// Uses non-rootless mode so Intuition draws everything to the screen bitmap.
// ============================================================================

// String gadget constants from updatestrgad.c
static const int STRGAD_LEFT = 20;
static const int STRGAD_TOP = 20;
static const int STRGAD_WIDTH = 200;
static const int STRGAD_HEIGHT = 8;
static const int PEN_BLACK = 1;
static const int PEN_GREY = 0;

class UpdateStrGadPixelTest : public LxaUITest {
protected:
    void SetUp() override {
        // Disable rootless mode so Intuition renders window frames
        // and gadget contents to the screen bitmap
        config.rootless = false;
        
        LxaUITest::SetUp();
        
        ASSERT_EQ(lxa_load_program("SYS:UpdateStrGad", ""), 0)
            << "Failed to load UpdateStrGad";
        
        ASSERT_TRUE(WaitForWindows(1, 5000))
            << "Window did not open within 5 seconds";
        
        ASSERT_TRUE(GetWindowInfo(0, &window_info))
            << "Could not get window info";
        
        // Let task reach WaitPort() and allow rendering to complete
        WaitForEventLoop(100, 10000);
        
        // Extra VBlanks to ensure rendering + planar->chunky conversion
        RunCyclesWithVBlank(50, 100000);
    }
};

TEST_F(UpdateStrGadPixelTest, StringGadgetBorderRendered) {
    // The string gadget has a Border at (-2, -2) that draws a rectangle.
    // Border XY: {0,0, 203,0, 203,11, 0,11, 0,0}
    // Absolute border position: (20-2, 20-2) = (18, 18), extends to (18+203, 18+11) = (221, 29)
    int bx0 = STRGAD_LEFT - 2;  // 18
    int by0 = STRGAD_TOP - 2;   // 18
    int bx1 = bx0 + STRGAD_WIDTH + 3;  // 221
    int by1 = by0 + STRGAD_HEIGHT + 3; // 29
    
    // Verify top edge of border: horizontal line at by0 from bx0 to bx1
    int top_edge_pixels = 0;
    for (int x = bx0; x <= bx1; x++) {
        int pen = ReadPixel(window_info.x + x, window_info.y + by0);
        if (pen == PEN_BLACK) top_edge_pixels++;
    }
    EXPECT_GT(top_edge_pixels, 180)
        << "Top edge of string gadget border should have pen-1 pixels (got " 
        << top_edge_pixels << ")";
    
    // Verify left edge of border: vertical line at bx0 from by0 to by1
    int left_edge_pixels = 0;
    for (int y = by0; y <= by1; y++) {
        int pen = ReadPixel(window_info.x + bx0, window_info.y + y);
        if (pen == PEN_BLACK) left_edge_pixels++;
    }
    EXPECT_GT(left_edge_pixels, 8)
        << "Left edge of string gadget border should have pen-1 pixels (got " 
        << left_edge_pixels << ")";
}

TEST_F(UpdateStrGadPixelTest, StringGadgetTextRendered) {
    // The sample activates the gadget on window activation and updates the
    // visible text to "Activated". The text area should contain visible pixels.
    
    // Scan the entire gadget interior for non-background (non-pen-0) pixels
    int text_pixels = CountContentPixels(
        window_info.x + STRGAD_LEFT,
        window_info.y + STRGAD_TOP,
        window_info.x + STRGAD_LEFT + STRGAD_WIDTH - 1,
        window_info.y + STRGAD_TOP + STRGAD_HEIGHT - 1,
        PEN_GREY
    );
    
    EXPECT_GT(text_pixels, 20)
        << "String gadget should have rendered text inside gadget area "
        << "(found " << text_pixels << " non-background pixels)";
}

TEST_F(UpdateStrGadPixelTest, StringGadgetTextCentered) {
    // With GACT_STRINGCENTER, the activated text should still remain centered.
    
    // Count text pixels in the center region (where "START" should be)
    int center_x = STRGAD_LEFT + (STRGAD_WIDTH / 2) - 35;
    int center_pixels = CountContentPixels(
        window_info.x + center_x,
        window_info.y + STRGAD_TOP,
        window_info.x + center_x + 70,
        window_info.y + STRGAD_TOP + STRGAD_HEIGHT - 1,
        PEN_GREY
    );
    
    // Count pixels in the far left (should be empty if centered)
    int left_pixels = CountContentPixels(
        window_info.x + STRGAD_LEFT,
        window_info.y + STRGAD_TOP,
        window_info.x + STRGAD_LEFT + 30,
        window_info.y + STRGAD_TOP + STRGAD_HEIGHT - 1,
        PEN_GREY
    );
    
    // Count pixels in the far right (should be empty if centered)
    int right_pixels = CountContentPixels(
        window_info.x + STRGAD_LEFT + STRGAD_WIDTH - 30,
        window_info.y + STRGAD_TOP,
        window_info.x + STRGAD_LEFT + STRGAD_WIDTH - 1,
        window_info.y + STRGAD_TOP + STRGAD_HEIGHT - 1,
        PEN_GREY
    );
    
    // Center should have text, edges should be mostly empty
    EXPECT_GT(center_pixels, 8)
        << "Center region should contain text pixels (got " << center_pixels << ")";
    EXPECT_LT(left_pixels, center_pixels)
        << "Left edge (" << left_pixels << " pixels) should have fewer pixels than center (" 
        << center_pixels << ") when text is centered";
    EXPECT_LT(right_pixels, center_pixels)
        << "Right edge (" << right_pixels << " pixels) should have fewer pixels than center (" 
        << center_pixels << ") when text is centered";
}

// ============================================================================
// Interactive tests (rootless mode - default)
//
// These tests verify string gadget keyboard interaction.
// ============================================================================

class UpdateStrGadTest : public LxaUITest {
protected:
    void SetUp() override {
        LxaUITest::SetUp();
        
        ASSERT_EQ(lxa_load_program("SYS:UpdateStrGad", ""), 0);
        ASSERT_TRUE(WaitForWindows(1, 5000));
        ASSERT_TRUE(GetWindowInfo(0, &window_info));
        
        // Let task reach WaitPort() - CRITICAL for event handling
        WaitForEventLoop(100, 10000);

        bool saw_activation = false;
        for (int i = 0; i < 20; i++) {
            RunCyclesWithVBlank(2, 50000);

            if (GetOutput().find("Gadget activated, updating to 'Activated'") != std::string::npos) {
                saw_activation = true;
                break;
            }
        }

        ASSERT_TRUE(saw_activation) << "UpdateStrGad did not process IDCMP_ACTIVEWINDOW";

        Click(window_info.x + STRGAD_LEFT + (STRGAD_WIDTH / 2),
              window_info.y + STRGAD_TOP + (STRGAD_HEIGHT / 2));
        RunCyclesWithVBlank(20, 50000);
    }
};

TEST_F(UpdateStrGadTest, ActivatesToActivatedOnOpen) {
    std::string output = GetOutput();

    EXPECT_NE(output.find("Gadget activated, updating to 'Activated'"), std::string::npos)
        << "Expected ACTIVEWINDOW handling to update the string gadget. Output was: " << output;

    EXPECT_FALSE(lxa_wait_exit(250))
        << "Sample should stay interactive after auto-activation";
}

TEST_F(UpdateStrGadTest, CloseWindow) {
    // Click close gadget (top-left of window, matching SimpleGad's pattern)
    int close_x = window_info.x + 10;
    int close_y = window_info.y + 5;
    
    Click(close_x, close_y);
    
    // Give VBlanks for the close event to propagate through Intuition
    RunCyclesWithVBlank(10, 50000);
    
    // Program should exit now
    EXPECT_TRUE(lxa_wait_exit(5000)) << "Program should exit after closing window";
    
    // Verify CLOSEWINDOW event in output
    std::string output = GetOutput();
    EXPECT_NE(output.find("IDCMP_CLOSEWINDOW"), std::string::npos)
        << "Expected CLOSEWINDOW event. Output was: " << output;
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
