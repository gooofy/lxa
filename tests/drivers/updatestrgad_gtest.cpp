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
        WaitForEventLoop(200, 10000);
        
        // Extra VBlanks to ensure rendering + planar->chunky conversion
        RunCyclesWithVBlank(30, 200000);
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
    // The string gadget buffer contains "START" (5 chars), centered in 200px.
    // With topaz 8 font: 5 chars * 8px = 40px text width.
    // Centered: textX = 20 + (200 - 40) / 2 = 20 + 80 = 100
    // The text area should have non-background pixels.
    
    // Scan the entire gadget interior for non-background (non-pen-0) pixels
    int text_pixels = CountContentPixels(
        window_info.x + STRGAD_LEFT,
        window_info.y + STRGAD_TOP,
        window_info.x + STRGAD_LEFT + STRGAD_WIDTH - 1,
        window_info.y + STRGAD_TOP + STRGAD_HEIGHT - 1,
        PEN_GREY
    );
    
    // "START" is 5 characters of 8x8 font. Each character has several pen-1 pixels.
    // Expect at least some text pixels rendered (conservative: > 20 pixels)
    EXPECT_GT(text_pixels, 20)
        << "String gadget should have rendered text 'START' inside gadget area "
        << "(found " << text_pixels << " non-background pixels)";
}

TEST_F(UpdateStrGadPixelTest, StringGadgetTextCentered) {
    // With GACT_STRINGCENTER, "START" (40px) should be centered in 200px gadget.
    // Center region: approximately x=[90..130] relative to gadget left
    // Left region: x=[0..60] relative to gadget left should be mostly empty
    // Right region: x=[140..200] relative to gadget left should be mostly empty
    
    // Count text pixels in the center region (where "START" should be)
    int center_x = STRGAD_LEFT + (STRGAD_WIDTH / 2) - 25;  // Start 25px left of center
    int center_pixels = CountContentPixels(
        window_info.x + center_x,
        window_info.y + STRGAD_TOP,
        window_info.x + center_x + 50,
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
    EXPECT_GT(center_pixels, 10)
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
        WaitForEventLoop(200, 10000);
        ClearOutput();
    }
};

TEST_F(UpdateStrGadTest, TypeIntoGadget) {
    // String gadget is at (20, 20) in the window, 200x8
    // Click center of string gadget to activate it
    int clickX = window_info.x + STRGAD_LEFT + (STRGAD_WIDTH / 2);
    int clickY = window_info.y + STRGAD_TOP + (STRGAD_HEIGHT / 2);
    
    // Click to activate the string gadget - cursor goes to end of existing text ("START")
    Click(clickX, clickY);
    RunCyclesWithVBlank(20, 50000);
    
    // Type "Hello" and Return - appends to existing "START" text
    ClearOutput();
    TypeString("Hello\n");
    
    // Need enough cycles for:
    // 1. Each keystroke to be processed through input.device -> Intuition
    // 2. String gadget key handler to insert characters
    // 3. Return key to trigger GADGETUP IDCMP
    // 4. Task to wake from Wait(), GetMsg(), and printf()
    RunCyclesWithVBlank(80, 50000);
    
    std::string output = GetOutput();
    EXPECT_NE(output.find("IDCMP_GADGETUP: string is 'STARTHello'"), std::string::npos)
        << "Expected GADGETUP with typed string 'STARTHello' (START from initial buffer + Hello typed). Output was: " << output;
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
