/**
 * gadtoolsgadgets_gtest.cpp - Google Test driver for GadToolsGadgets sample
 *
 * Tests GadTools gadget creation with STRING_KIND, SLIDER_KIND, and BUTTON_KIND.
 * Verifies that CreateGadgetA correctly allocates StringInfo for string gadgets.
 * Pixel tests verify bevel borders and text labels are rendered.
 */

#include "lxa_test.h"

using namespace lxa::testing;

/* Standard Workbench pens */
constexpr int PEN_GREY  = 0;  /* Background */
constexpr int PEN_BLACK = 1;  /* Shadow */
constexpr int PEN_WHITE = 2;  /* Shine */
constexpr int PEN_BLUE  = 3;  /* Blue */

constexpr int TITLE_BAR_HEIGHT = 11;

/* GadTools bevel constants (must match GT_BEVEL_* in lxa_gadtools.c) */
constexpr int GT_BEVEL_LEFT = 4;
constexpr int GT_BEVEL_TOP  = 2;

// ============================================================================
// Functional Tests - verify gadget creation and event handling via output
// ============================================================================

class GadToolsGadgetsTest : public LxaUITest {
protected:
    std::string output;
    bool program_exited = false;

    void SetUp() override {
        LxaUITest::SetUp();

        /* Load the GadToolsGadgets program (now interactive with event loop) */
        ASSERT_EQ(lxa_load_program("SYS:GadToolsGadgets", ""), 0)
            << "Failed to load GadToolsGadgets";

        /* Wait for window to open */
        ASSERT_TRUE(WaitForWindows(1, 5000))
            << "Window did not open within 5 seconds";
        ASSERT_TRUE(GetWindowInfo(0, &window_info))
            << "Could not get window info";

        /* Let task reach event loop and flush Printf output */
        RunCyclesWithVBlank(50, 100000);

        /* Capture the startup output */
        output = GetOutput();
    }

    void TearDown() override {
        if (!program_exited) {
            /* Close the window to let the program exit cleanly */
            lxa_click_close_gadget(0);
            RunCyclesWithVBlank(20, 50000);
            lxa_wait_exit(3000);
        }

        LxaUITest::TearDown();
    }
};

TEST_F(GadToolsGadgetsTest, AllGadgetsCreated) {
    /* Verify context creation */
    EXPECT_NE(output.find("Context created at"), std::string::npos)
        << "CreateContext() should succeed";

    /* Verify slider gadget created */
    EXPECT_NE(output.find("Slider created at"), std::string::npos)
        << "SLIDER_KIND gadget should be created";

    /* Verify all 3 string gadgets created */
    EXPECT_NE(output.find("String 1 created at"), std::string::npos)
        << "STRING_KIND gadget 1 should be created";
    EXPECT_NE(output.find("String 2 created at"), std::string::npos)
        << "STRING_KIND gadget 2 should be created";
    EXPECT_NE(output.find("String 3 created at"), std::string::npos)
        << "STRING_KIND gadget 3 should be created";

    /* Verify button gadget created */
    EXPECT_NE(output.find("Button created at"), std::string::npos)
        << "BUTTON_KIND gadget should be created";

    /* Verify all gadgets reported as created successfully */
    EXPECT_NE(output.find("All gadgets created successfully"), std::string::npos)
        << "All gadgets should be created successfully";
}

TEST_F(GadToolsGadgetsTest, WindowOpened) {
    /* Verify window was opened */
    EXPECT_NE(output.find("Window opened at"), std::string::npos)
        << "Window should open successfully";

    /* Verify window size is reported */
    EXPECT_NE(output.find("Window size:"), std::string::npos)
        << "Window size should be reported";
}

TEST_F(GadToolsGadgetsTest, GadgetsRefreshed) {
    /* Verify GT_RefreshWindow was called */
    EXPECT_NE(output.find("Gadgets refreshed"), std::string::npos)
        << "GT_RefreshWindow should complete";
}

TEST_F(GadToolsGadgetsTest, EventLoopEntered) {
    /* Verify the program entered its event loop (interactive mode) */
    EXPECT_NE(output.find("Entering event loop"), std::string::npos)
        << "Program should enter event loop";
}

TEST_F(GadToolsGadgetsTest, ButtonClick) {
    /* The button is the last gadget: ng_LeftEdge=140+50=190, at the bottom.
     * topborder = WBorTop(11) + FontYSize(0) + 1 = 12
     * Gadget TopEdge progression: 20+12=32, +20=52, +20=72, +20=92, +20=112
     * Button TopEdge = 112, Width=100, Height=12
     * Click center of button. */
    int btn_x = window_info.x + 190 + 50;  /* center of 100px wide button */
    int btn_y = window_info.y + 112 + 6;   /* center of 12px tall button */

    ClearOutput();
    Click(btn_x, btn_y);
    RunCyclesWithVBlank(30, 100000);

    std::string click_output = GetOutput();
    EXPECT_NE(click_output.find("Button was pressed, slider reset to 10"), std::string::npos)
        << "Button click should trigger handleGadgetEvent and reset slider. Output: " << click_output;
}

TEST_F(GadToolsGadgetsTest, CloseWindow) {
    /* Close the window and verify the program handles it */
    ClearOutput();
    lxa_click_close_gadget(0);
    RunCyclesWithVBlank(20, 50000);

    /* Wait for program exit */
    EXPECT_TRUE(lxa_wait_exit(3000))
        << "Program should exit after close window";
    program_exited = true;

    std::string close_output = GetOutput();
    EXPECT_NE(close_output.find("IDCMP_CLOSEWINDOW"), std::string::npos)
        << "IDCMP_CLOSEWINDOW should be reported";
    EXPECT_NE(close_output.find("Window closed"), std::string::npos)
        << "Window should be closed";
    EXPECT_NE(close_output.find("demo complete"), std::string::npos)
        << "Demo should complete successfully";
}

// ============================================================================
// Pixel Tests - verify bevel borders and text labels are rendered
// ============================================================================

class GadToolsGadgetsPixelTest : public LxaUITest {
protected:
    void SetUp() override {
        /* Disable rootless mode for pixel verification */
        config.rootless = false;

        LxaUITest::SetUp();

        ASSERT_EQ(lxa_load_program("SYS:GadToolsGadgets", ""), 0)
            << "Failed to load GadToolsGadgets";
        ASSERT_TRUE(WaitForWindows(1, 5000))
            << "Window did not open within 5 seconds";
        ASSERT_TRUE(GetWindowInfo(0, &window_info))
            << "Could not get window info";

        /* Let task reach event loop and ensure all rendering completes.
         * GadTools creates 6 gadgets; rendering all of them via
         * _render_window_frame() can span multiple VBlank cycles.
         * Use a generous budget so every gadget is fully drawn. */
        RunCyclesWithVBlank(100, 200000);
    }

    void TearDown() override {
        /* Close the window to let the program exit cleanly */
        lxa_click_close_gadget(0);
        RunCyclesWithVBlank(20, 50000);
        lxa_wait_exit(3000);

        LxaUITest::TearDown();
    }
};

TEST_F(GadToolsGadgetsPixelTest, WindowTitleBarRendered) {
    /* Verify the window title bar has non-background pixels */
    int title_content = CountContentPixels(
        window_info.x + 1,
        window_info.y + 1,
        window_info.x + window_info.width - 2,
        window_info.y + TITLE_BAR_HEIGHT - 1,
        PEN_GREY
    );
    EXPECT_GT(title_content, 0)
        << "Title bar should contain non-background pixels";
}

TEST_F(GadToolsGadgetsPixelTest, ButtonBevelBorderRendered) {
    /* Button gadget: ng_LeftEdge = 140+50 = 190, ng_Width = 100, ng_Height = 12.
     * topborder = WBorTop(11) + FontYSize(0) + 1 = 12
     * Gadget TopEdge progression: 20+12=32, +20=52, +20=72, +20=92, +20=112
     * The bevel border is drawn at the gadget position:
     *   Raised bevel: shine(2) top-left L, shadow(1) bottom-right L
     */
    int btn_left = 190;
    int btn_top  = 112;
    int btn_w = 100;
    int btn_h = 12;

    /* Check top edge of button bevel: should have shine (pen 2) pixels */
    int shine_count = 0;
    for (int x = btn_left; x < btn_left + btn_w; x++) {
        int pen = ReadPixel(window_info.x + x, window_info.y + btn_top);
        if (pen == PEN_WHITE) shine_count++;
    }
    EXPECT_GT(shine_count, btn_w / 2)
        << "Button top edge should have shine pixels (pen 2), got " << shine_count;

    /* Check left edge of button bevel: should have shine (pen 2) pixels */
    int left_shine = 0;
    for (int y = btn_top; y < btn_top + btn_h; y++) {
        int pen = ReadPixel(window_info.x + btn_left, window_info.y + y);
        if (pen == PEN_WHITE) left_shine++;
    }
    EXPECT_GT(left_shine, btn_h / 2)
        << "Button left edge should have shine pixels (pen 2), got " << left_shine;

    /* Check bottom edge of button bevel: should have shadow (pen 1) pixels */
    int shadow_count = 0;
    for (int x = btn_left; x < btn_left + btn_w; x++) {
        int pen = ReadPixel(window_info.x + x, window_info.y + btn_top + btn_h - 1);
        if (pen == PEN_BLACK) shadow_count++;
    }
    EXPECT_GT(shadow_count, btn_w / 2)
        << "Button bottom edge should have shadow pixels (pen 1), got " << shadow_count;

    /* Check right edge of button bevel: should have shadow (pen 1) pixels */
    int right_shadow = 0;
    for (int y = btn_top; y < btn_top + btn_h; y++) {
        int pen = ReadPixel(window_info.x + btn_left + btn_w - 1, window_info.y + y);
        if (pen == PEN_BLACK) right_shadow++;
    }
    EXPECT_GT(right_shadow, btn_h / 2)
        << "Button right edge should have shadow pixels (pen 1), got " << right_shadow;
}

TEST_F(GadToolsGadgetsPixelTest, StringGadgetBevelBorderRendered) {
    /* First string gadget: ng_LeftEdge = 140, ng_Width = 200, ng_Height = 14.
     * topborder = WBorTop(11) + FontYSize(0) + 1 = 12
     * Gadget TopEdge progression: 20+12=32 (slider), +20=52 (string 1)
     * String 1 TopEdge = 52 (relative to window top)
     * The bevel border is the full ng_Width x ng_Height, drawn at negative
     * offsets from the shrunk gadget hitbox.
     * Recessed bevel: shadow(1) top-left L, shine(2) bottom-right L
     *
     * The bevel's screen position is at (ng_LeftEdge, ng_TopEdge), covering
     * the full ng_Width x ng_Height.
     */
    int str1_left = 140;                     /* ng_LeftEdge */
    int str1_top  = 52;                      /* first string gadget TopEdge */
    int str1_w = 200;                        /* ng_Width */
    int str1_h = 14;                         /* ng_Height */

    /* Check top edge of string bevel: should have shadow (pen 1) pixels (recessed) */
    int shadow_top = 0;
    for (int x = str1_left; x < str1_left + str1_w; x++) {
        int pen = ReadPixel(window_info.x + x, window_info.y + str1_top);
        if (pen == PEN_BLACK) shadow_top++;
    }
    EXPECT_GT(shadow_top, str1_w / 2)
        << "String gadget top edge should have shadow pixels (pen 1, recessed), got " << shadow_top;

    /* Check bottom edge: should have shine (pen 2) pixels (recessed) */
    int shine_bottom = 0;
    for (int x = str1_left; x < str1_left + str1_w; x++) {
        int pen = ReadPixel(window_info.x + x, window_info.y + str1_top + str1_h - 1);
        if (pen == PEN_WHITE) shine_bottom++;
    }
    EXPECT_GT(shine_bottom, str1_w / 2)
        << "String gadget bottom edge should have shine pixels (pen 2, recessed), got " << shine_bottom;
}

TEST_F(GadToolsGadgetsPixelTest, StringGadgetTextRendered) {
    /* First string gadget at ng_LeftEdge=140, the text area is inset by GT_BEVEL_LEFT.
     * The initial text is "Try pressing" which should produce non-background pixels
     * in the text area.
     * topborder = 12, string 1 TopEdge = 52
     */
    int str1_left = 140 + GT_BEVEL_LEFT;     /* text area starts after bevel inset */
    int str1_top  = 52 + GT_BEVEL_TOP;
    int str1_w = 200 - 2 * GT_BEVEL_LEFT;
    int str1_h = 14 - 2 * GT_BEVEL_TOP;

    /* Count non-background pixels in the text area */
    int text_pixels = CountContentPixels(
        window_info.x + str1_left,
        window_info.y + str1_top,
        window_info.x + str1_left + str1_w - 1,
        window_info.y + str1_top + str1_h - 1,
        PEN_GREY
    );
    EXPECT_GT(text_pixels, 0)
        << "String gadget text area should contain rendered text pixels";
}

TEST_F(GadToolsGadgetsPixelTest, ButtonLabelRendered) {
    /* Button label "Click Here" (with underscore stripped) is centered inside.
     * The button is at (190, 112) with size 100x12.
     * PLACETEXT_IN centers the text both horizontally and vertically.
     * Check that the interior of the button has non-background pixels (the text).
     */
    int btn_left = 190;
    int btn_top  = 112;
    int btn_w = 100;
    int btn_h = 12;

    /* Check interior pixels — should have text (pen 1) */
    int interior_content = CountContentPixels(
        window_info.x + btn_left + 2,
        window_info.y + btn_top + 2,
        window_info.x + btn_left + btn_w - 3,
        window_info.y + btn_top + btn_h - 3,
        PEN_GREY
    );
    EXPECT_GT(interior_content, 0)
        << "Button interior should contain text label pixels";
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
