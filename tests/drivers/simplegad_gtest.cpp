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

// Main function for Google Test
int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
