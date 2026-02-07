/**
 * simplegad_gtest.cpp - Google Test version of SimpleGad test
 *
 * This test driver uses Google Test + liblxa to:
 * 1. Start the SimpleGad sample (RKM original)
 * 2. Wait for the window to open
 * 3. Verify pixel-level correctness of window rendering (non-rootless mode)
 * 4. Click the button and verify GADGETDOWN + GADGETUP (ID 3)
 * 5. Click the close gadget to close the window
 * 6. Verify the program exits cleanly
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

// Amiga default palette pen indices
constexpr int PEN_GREY = 0;   // Background grey
constexpr int PEN_BLACK = 1;  // Black (0x00, 0x00, 0x00)
constexpr int PEN_WHITE = 2;  // White (0xFF, 0xFF, 0xFF)

// ============================================================================
// Behavioral tests (rootless mode - default)
// ============================================================================

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
    
    // Process event through VBlanks - need enough for the full Intuition pipeline:
    // event -> input.device -> Intuition -> IDCMP -> task signal -> task runs -> printf
    RunCyclesWithVBlank(40, 50000);
    
    // Check output
    std::string output = GetOutput();
    EXPECT_NE(output.find("IDCMP_GADGETDOWN"), std::string::npos) 
        << "Expected GADGETDOWN event. Output was: " << output;
    EXPECT_NE(output.find("IDCMP_GADGETUP"), std::string::npos) 
        << "Expected GADGETUP event. Output was: " << output;
    EXPECT_NE(output.find("gadget number 3"), std::string::npos) 
        << "Expected gadget ID 3. Output was: " << output;
}

TEST_F(SimpleGadTest, CloseWindow) {
    ClearOutput();
    
    // Click close gadget (top-left of window)
    int close_x = window_info.x + 10;
    int close_y = window_info.y + 5;
    
    Click(close_x, close_y);
    
    // Give additional VBlanks for the close event to propagate through
    // Intuition and for the task to process it and begin cleanup
    RunCyclesWithVBlank(10, 50000);
    
    // Wait for exit - now with VBlank triggering for reliable cleanup
    ASSERT_TRUE(lxa_wait_exit(5000)) 
        << "Program did not exit within 5 seconds";
    
    // Verify CLOSEWINDOW event
    std::string output = GetOutput();
    EXPECT_NE(output.find("IDCMP_CLOSEWINDOW"), std::string::npos) 
        << "Expected CLOSEWINDOW event. Output was: " << output;
}

// ============================================================================
// Pixel verification tests (non-rootless mode)
//
// In rootless mode, Intuition skips window frame rendering since the host
// window manager provides decorations. For pixel-level verification of
// Amiga-side rendering (borders, gadgets, title bars), we must use
// non-rootless mode so everything is drawn to the screen bitmap.
// ============================================================================

class SimpleGadPixelTest : public LxaUITest {
protected:
    void SetUp() override {
        // Disable rootless mode so Intuition renders window frames
        // to the screen bitmap, making them visible via display_read_pixel()
        config.rootless = false;
        
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
        
        // Let task reach WaitPort() and allow rendering to complete
        WaitForEventLoop(200, 10000);
        
        // Trigger extra VBlanks to ensure planar->chunky conversion
        // Use more cycles to ensure _render_window_frame() and gadget rendering complete
        RunCyclesWithVBlank(30, 200000);
    }
};

TEST_F(SimpleGadPixelTest, WindowBorderRendered) {
    // Verify that the window title bar has non-background pixels
    // The title bar should contain the close gadget and title text drawn in pen 1 (black)
    int title_content = CountContentPixels(
        window_info.x + 1,
        window_info.y + 1,
        window_info.x + window_info.width - 2,
        window_info.y + TITLE_BAR_HEIGHT - 1,
        PEN_GREY
    );
    EXPECT_GT(title_content, 0)
        << "Title bar should contain non-background pixels (close gadget, title text)";
    
    // Verify the window border - top edge should be non-grey
    int top_border_pen = ReadPixel(window_info.x, window_info.y);
    EXPECT_NE(top_border_pen, -1) << "Should be able to read pixel at window top-left corner";
    // Top-left border pixel should be either white (pen 2) or black (pen 1) - not grey background
    EXPECT_NE(top_border_pen, PEN_GREY)
        << "Window border should not be background color (pen=" << top_border_pen << ")";
}

TEST_F(SimpleGadPixelTest, GadgetBorderRendered) {
    // The button gadget has a border drawn in pen 1 (black).
    // Gadget LeftEdge=20, TopEdge=20 relative to window origin.
    // Border LeftEdge=-1, TopEdge=-1, so border starts at window-relative (19, 19).
    // Border XY data: {0,0, 101,0, 101,51, 0,51, 0,0} (5-point rectangle)
    // So border spans from (19,19) to (19+101,19+51) = (120,70) in window coords.
    
    int bx0 = 19;  // BUTTON_LEFT + border->LeftEdge = 20 + (-1)
    int by0 = 19;  // BUTTON_TOP + border->TopEdge = 20 + (-1)
    int bx1 = 19 + BUTTON_WIDTH + 1;   // 120 (border data goes to MYBUTTONGADWIDTH+1=101)
    int by1 = 19 + BUTTON_HEIGHT + 1;  // 70  (border data goes to MYBUTTONGADHEIGHT+1=51)
    
    // Verify top edge of gadget border: horizontal line at by0 from bx0 to bx1
    int top_edge_pixels = 0;
    for (int x = bx0; x <= bx1; x++) {
        int pen = ReadPixel(window_info.x + x, window_info.y + by0);
        if (pen == PEN_BLACK) top_edge_pixels++;
    }
    EXPECT_GT(top_edge_pixels, 90)
        << "Top edge of gadget border should have pen-1 pixels (got " << top_edge_pixels << ")";
    
    // Verify left edge of gadget border: vertical line at bx0 from by0 to by1
    int left_edge_pixels = 0;
    for (int y = by0; y <= by1; y++) {
        int pen = ReadPixel(window_info.x + bx0, window_info.y + y);
        if (pen == PEN_BLACK) left_edge_pixels++;
    }
    EXPECT_GT(left_edge_pixels, 45)
        << "Left edge of gadget border should have pen-1 pixels (got " << left_edge_pixels << ")";
    
    // Verify right edge of gadget border: vertical line at bx1 from by0 to by1
    int right_edge_pixels = 0;
    for (int y = by0; y <= by1; y++) {
        int pen = ReadPixel(window_info.x + bx1, window_info.y + y);
        if (pen == PEN_BLACK) right_edge_pixels++;
    }
    EXPECT_GT(right_edge_pixels, 45)
        << "Right edge of gadget border should have pen-1 pixels (got " << right_edge_pixels << ")";
    
    // Verify bottom edge of gadget border: horizontal line at by1 from bx0 to bx1
    int bottom_edge_pixels = 0;
    for (int x = bx0; x <= bx1; x++) {
        int pen = ReadPixel(window_info.x + x, window_info.y + by1);
        if (pen == PEN_BLACK) bottom_edge_pixels++;
    }
    EXPECT_GT(bottom_edge_pixels, 90)
        << "Bottom edge of gadget border should have pen-1 pixels (got " << bottom_edge_pixels << ")";
    
    // Verify interior of gadget is pen 0 (grey background - not drawn over)
    int interior_pen = ReadPixel(window_info.x + BUTTON_LEFT + 10,
                                 window_info.y + BUTTON_TOP + 10);
    EXPECT_EQ(interior_pen, PEN_GREY)
        << "Gadget interior should be background color (pen 0)";
}

TEST_F(SimpleGadPixelTest, WindowInteriorColor) {
    // The window interior (outside the gadget area) should be pen 0 (grey)
    // Check a point to the right of the gadget
    int check_x = window_info.x + BUTTON_LEFT + BUTTON_WIDTH + 20;
    int check_y = window_info.y + TITLE_BAR_HEIGHT + 10;
    
    // Make sure we're within window bounds
    if (check_x < window_info.x + window_info.width - 2 &&
        check_y < window_info.y + window_info.height - 2) {
        int pen = ReadPixel(check_x, check_y);
        EXPECT_NE(pen, -1) << "Should be able to read pixel in window interior";
        EXPECT_EQ(pen, PEN_GREY)
            << "Window interior (away from gadgets) should be grey background (pen 0)";
    }
    
    // Verify the RGB value of the grey background using the display palette.
    // Workbench pen 0 is 0x0AAA (4-bit RGB), which converts to 0xAAAAAA
    // (8-bit RGB) via the palette pipeline (SetRGB4 -> display_set_color).
    uint8_t r, g, b;
    if (ReadPixelRGB(check_x, check_y, &r, &g, &b)) {
        // Amiga Workbench pen 0 = 0x0AAA -> R=0xAA, G=0xAA, B=0xAA
        EXPECT_EQ(r, 0xAA) << "Background red component should be 0xAA (Workbench grey)";
        EXPECT_EQ(g, 0xAA) << "Background green component should be 0xAA (Workbench grey)";
        EXPECT_EQ(b, 0xAA) << "Background blue component should be 0xAA (Workbench grey)";
    }
}

TEST_F(SimpleGadPixelTest, NoDepthGadgetInTopRight) {
    // SimpleGad uses WA_CloseGadget but NOT WA_DepthGadget.
    // Verify the top-right corner of the title bar does NOT contain a gadget frame.
    //
    // System gadgets are 18px wide (SYS_GADGET_WIDTH). If a depth gadget existed,
    // it would be at (Width - BorderRight - 18, 0) and have a 3D frame with
    // pen 2 (shine/white) on top/left edges and pen 1 (shadow/black) on bottom/right.
    //
    // The title bar area (excluding the outer border) should be a flat fill
    // in the top-right corner with NO vertical separator lines.
    
    constexpr int SYS_GADGET_WIDTH = 18;
    constexpr int BORDER_RIGHT = 2;  // Standard Amiga window right border
    
    // The area where a depth gadget WOULD be: 
    // x from (width - borderRight - gadgetWidth) to (width - borderRight - 1)
    // y from 1 to titleBarBottom (around 10)
    int gadget_area_x0 = window_info.width - BORDER_RIGHT - SYS_GADGET_WIDTH;
    int gadget_area_x1 = window_info.width - BORDER_RIGHT - 1;
    int gadget_area_y0 = 1;  // Just inside the top border
    int gadget_area_y1 = TITLE_BAR_HEIGHT - 1;  // Just above title bar bottom line
    
    // In the potential depth gadget area, scan for vertical separator lines.
    // A gadget frame would have a vertical shine (pen 2) line at gadget_area_x0
    // and a vertical shadow (pen 1) line at gadget_area_x1.
    // Without a depth gadget, the left edge of this area should just be
    // the title bar fill color — NOT a vertical pen 2 (shine) line.
    
    // Check the vertical line at where the depth gadget's left edge would be.
    // If no depth gadget exists, this column should be the title bar fill color
    // (active window = blkPen which is pen 1 for standard screen pens).
    // A gadget frame would have a distinct shine (pen 2) vertical line here.
    int shine_count = 0;
    int shadow_count = 0;
    for (int y = gadget_area_y0; y <= gadget_area_y1; y++) {
        int pen = ReadPixel(window_info.x + gadget_area_x0, window_info.y + y);
        if (pen == PEN_WHITE) shine_count++;
        if (pen == PEN_BLACK) shadow_count++;
    }
    
    // A depth gadget 3D frame would have a full vertical shine line (pen 2) on the left edge.
    // Without a depth gadget, we should NOT see a full column of pen 2 here.
    // The title bar interior should be filled with the fill pen (pen 1 for active window).
    // We allow some tolerance (e.g., the very top pixel could be border-related).
    int gadget_height = gadget_area_y1 - gadget_area_y0 + 1;
    EXPECT_LT(shine_count, gadget_height - 1)
        << "Left edge of potential depth gadget area should NOT have a full vertical "
           "shine line (pen 2). This would indicate a gadget frame is being drawn. "
           "shine_count=" << shine_count << " out of " << gadget_height << " pixels";
    
    // Also check right edge — a gadget frame would have a vertical shadow (pen 1) line
    // at the right edge. But since the title bar fill is ALSO pen 1 (active window),
    // we need a different approach: check for pen 2 (shine) at the TOP of the gadget area.
    // A gadget frame draws: shine on top-left corner going right, then shadow on right.
    // The top-left corner of a gadget frame would be pen 2 (shine).
    // Without a gadget, this area is just title bar fill.
    
    // More definitive check: scan the top row of the potential gadget area.
    // A gadget 3D frame draws a horizontal shine (pen 2) line along the top.
    // Without a gadget, there's no such horizontal line — just the fill color.
    int top_row_shine = 0;
    for (int x = gadget_area_x0; x <= gadget_area_x1; x++) {
        int pen = ReadPixel(window_info.x + x, window_info.y + gadget_area_y0);
        if (pen == PEN_WHITE) top_row_shine++;
    }
    
    // A gadget frame would have nearly all pixels in the top row as pen 2 (shine).
    // Without it, at most a few stray pixels might match. Threshold: less than half.
    EXPECT_LT(top_row_shine, SYS_GADGET_WIDTH / 2)
        << "Top row of potential depth gadget area should NOT have a shine line. "
           "This would indicate a gadget frame is drawn in the top-right corner. "
           "top_row_shine=" << top_row_shine << " out of " << SYS_GADGET_WIDTH;
}

// Main function for Google Test
int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
