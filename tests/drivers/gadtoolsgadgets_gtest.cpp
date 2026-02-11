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
        RunCyclesWithVBlank(30, 100000);

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
     * topborder = WBorTop(11) + FontYSize(8) + 1 = 20
     * Gadget TopEdge progression: 20+20=40, +20=60, +20=80, +20=100, +20=120
     * Button TopEdge = 120, Width=100, Height=12
     * Click center of button. */
    int btn_x = window_info.x + 190 + 50;  /* center of 100px wide button */
    int btn_y = window_info.y + 120 + 6;   /* center of 12px tall button */

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

TEST_F(GadToolsGadgetsTest, SliderClick) {
    /* The slider is the first gadget: ng_LeftEdge=140, TopEdge=40, Width=200, Height=12.
     * Initial level is 5, min=1, max=20.
     * Click on the right side of the slider to set a higher level,
     * which should generate GADGETDOWN + GADGETUP with the level as Code.
     */
    int slider_left = 140;
    int slider_top  = 40;
    int slider_w    = 200;
    int slider_h    = 12;

    /* Click at 80% of the slider width (expect level around 16-17) */
    int click_x = window_info.x + slider_left + (int)(slider_w * 0.8);
    int click_y = window_info.y + slider_top + slider_h / 2;

    ClearOutput();
    Click(click_x, click_y);
    RunCyclesWithVBlank(30, 100000);

    std::string click_output = GetOutput();
    /* The sample program prints "Slider at level <N>" for GADGETDOWN/MOUSEMOVE/GADGETUP */
    EXPECT_NE(click_output.find("Slider at level"), std::string::npos)
        << "Slider click should report slider level. Output: " << click_output;
}

TEST_F(GadToolsGadgetsTest, SliderDrag) {
    /* Drag the slider from left side to right side.
     * This should generate MOUSEMOVE messages with changing level values.
     * The slider: LeftEdge=140, TopEdge=40, Width=200, Height=12.
     *
     * Note: Printf output from the Amiga program may be buffered in the
     * DOS file handle and only flushed when the program calls WaitPort().
     * We capture all output (including startup) and count "Slider at level"
     * occurrences: the startup output has none, so any we find came from
     * the drag interaction.
     */
    int slider_left = 140;
    int slider_top  = 40;
    int slider_w    = 200;
    int slider_h    = 12;

    int start_x = window_info.x + slider_left + slider_w / 4;
    int start_y = window_info.y + slider_top + slider_h / 2;
    int end_x   = window_info.x + slider_left + (int)(slider_w * 0.9);
    int end_y   = start_y;

    /* Record how many "Slider at level" lines exist before the drag */
    std::string before = GetOutput();
    int before_count = 0;
    {
        size_t pos = 0;
        while ((pos = before.find("Slider at level", pos)) != std::string::npos) {
            before_count++;
            pos += 15;
        }
    }

    lxa_inject_drag(start_x, start_y, end_x, end_y, LXA_MOUSE_LEFT, 5);
    RunCyclesWithVBlank(60, 200000);

    std::string after = GetOutput();
    int after_count = 0;
    {
        size_t pos = 0;
        while ((pos = after.find("Slider at level", pos)) != std::string::npos) {
            after_count++;
            pos += 15;
        }
    }

    int drag_messages = after_count - before_count;
    EXPECT_GE(drag_messages, 1)
        << "Slider drag should produce at least one 'Slider at level' message. "
        << "Before: " << before_count << ", After: " << after_count
        << ". Output: " << after;

    EXPECT_GE(drag_messages, 2)
        << "Slider drag should produce multiple 'Slider at level' messages (MOUSEMOVE). "
        << "Before: " << before_count << ", After: " << after_count;
}

TEST_F(GadToolsGadgetsTest, ButtonResetsSlider) {
    /* First click slider to change its level, then click button to reset to 10.
     * The sample program prints "Button was pressed, slider reset to 10."
     * and calls GT_SetGadgetAttrs to update the slider.
     */
    int slider_left = 140;
    int slider_top  = 40;
    int slider_w    = 200;
    int slider_h    = 12;

    /* Click slider at right side to set a high level */
    int slider_click_x = window_info.x + slider_left + (int)(slider_w * 0.9);
    int slider_click_y = window_info.y + slider_top + slider_h / 2;

    Click(slider_click_x, slider_click_y);
    RunCyclesWithVBlank(20, 100000);

    /* Now click the button to reset slider to 10 */
    int btn_x = window_info.x + 190 + 50;   /* center of 100px wide button */
    int btn_y = window_info.y + 120 + 6;    /* center of 12px tall button */

    ClearOutput();
    Click(btn_x, btn_y);
    RunCyclesWithVBlank(30, 100000);

    std::string btn_output = GetOutput();
    EXPECT_NE(btn_output.find("Button was pressed, slider reset to 10"), std::string::npos)
        << "Button click should reset slider. Output: " << btn_output;
}

TEST_F(GadToolsGadgetsTest, DepthGadgetClick) {
    /* Click the depth gadget (top-right of window) and verify the window
     * still works afterward.  With only one window, WindowToBack has no
     * visible effect, but we verify: no crash, no hang, window still
     * responds to the close gadget afterward.
     * Depth gadget: rightmost 18px of title bar. */
    int gadWidth = 18;
    int depth_cx = window_info.x + window_info.width - gadWidth / 2;
    int depth_cy = window_info.y + TITLE_BAR_HEIGHT / 2;

    Click(depth_cx, depth_cy);
    RunCyclesWithVBlank(20, 100000);

    /* Window should still be responsive — close it */
    ClearOutput();
    lxa_click_close_gadget(0);
    RunCyclesWithVBlank(20, 50000);
    EXPECT_TRUE(lxa_wait_exit(3000))
        << "Program should exit after close window (depth gadget did not break it)";
    program_exited = true;

    std::string close_output = GetOutput();
    EXPECT_NE(close_output.find("IDCMP_CLOSEWINDOW"), std::string::npos)
        << "IDCMP_CLOSEWINDOW should still be delivered after depth gadget click";
}

TEST_F(GadToolsGadgetsTest, TabCyclesStringGadgets) {
    /* Test TAB cycling between string gadgets.
     * Per RKRM, TAB moves focus forward to next string gadget, Shift-TAB backward.
     * String gadget 1: at TopEdge=60 (LeftEdge=140, Width=200, Height=14)
     * String gadget 2: at TopEdge=80
     * String gadget 3: at TopEdge=100
     * Initial contents: "Try pressing", "TAB or Shift-TAB", "To see what happens!" */

    constexpr int RAWKEY_TAB = 0x42;
    constexpr int IEQUALIFIER_LSHIFT = 0x0001;

    /* Click on string gadget 1 to activate it */
    int str1_x = window_info.x + 140 + 100;  /* center of 200px wide */
    int str1_y = window_info.y + 60 + 7;     /* center of 14px tall */
    Click(str1_x, str1_y);
    RunCyclesWithVBlank(20, 100000);

    /* Press TAB to cycle to string gadget 2 */
    PressKey(RAWKEY_TAB, 0);
    RunCyclesWithVBlank(10, 100000);

    /* Type "XYZ" into string gadget 2, then press Return to get GADGETUP */
    ClearOutput();
    TypeString("XYZ\n");
    RunCyclesWithVBlank(60, 200000);

    std::string output2 = GetOutput();
    /* The GADGETUP should report string gadget 2's contents.
     * Original text was "TAB or Shift-TAB", cursor goes to end, so we typed "XYZ" + Return.
     * Result: "TAB or Shift-TABXYZ" */
    EXPECT_NE(output2.find("String gadget 2:"), std::string::npos)
        << "TAB should cycle focus from string 1 to string 2. Output: " << output2;
    EXPECT_NE(output2.find("XYZ"), std::string::npos)
        << "Typed text should appear in string gadget 2. Output: " << output2;
}

TEST_F(GadToolsGadgetsTest, ShiftTabCyclesBackward) {
    /* Test Shift-TAB cycling backward between string gadgets.
     * Start by activating string gadget 2, then Shift-TAB should go to string gadget 1. */

    constexpr int RAWKEY_TAB = 0x42;
    constexpr int IEQUALIFIER_LSHIFT = 0x0001;

    /* Click on string gadget 2 to activate it */
    int str2_x = window_info.x + 140 + 100;
    int str2_y = window_info.y + 80 + 7;
    Click(str2_x, str2_y);
    RunCyclesWithVBlank(20, 100000);

    /* Press Shift-TAB to cycle backward to string gadget 1 */
    PressKey(RAWKEY_TAB, IEQUALIFIER_LSHIFT);
    RunCyclesWithVBlank(10, 100000);

    /* Type "ABC" into string gadget 1, then press Return */
    ClearOutput();
    TypeString("ABC\n");
    RunCyclesWithVBlank(60, 200000);

    std::string output1 = GetOutput();
    /* Should report string gadget 1's contents */
    EXPECT_NE(output1.find("String gadget 1:"), std::string::npos)
        << "Shift-TAB should cycle focus from string 2 to string 1. Output: " << output1;
    EXPECT_NE(output1.find("ABC"), std::string::npos)
        << "Typed text should appear in string gadget 1. Output: " << output1;
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
        RunCyclesWithVBlank(70, 200000);
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
     * topborder = WBorTop(11) + FontYSize(8) + 1 = 20
     * Gadget TopEdge progression: 20+20=40, +20=60, +20=80, +20=100, +20=120
     * The bevel border is drawn at the gadget position:
     *   Raised bevel: shine(2) top-left L, shadow(1) bottom-right L
     */
    int btn_left = 190;
    int btn_top  = 120;
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
     * topborder = WBorTop(11) + FontYSize(8) + 1 = 20
     * Gadget TopEdge progression: 20+20=40 (slider), +20=60 (string 1)
     * String 1 TopEdge = 60 (relative to window top)
     * The bevel border is the full ng_Width x ng_Height, drawn at negative
     * offsets from the shrunk gadget hitbox.
     * Recessed bevel: shadow(1) top-left L, shine(2) bottom-right L
     *
     * The bevel's screen position is at (ng_LeftEdge, ng_TopEdge), covering
     * the full ng_Width x ng_Height.
     */
    int str1_left = 140;                     /* ng_LeftEdge */
    int str1_top  = 60;                      /* first string gadget TopEdge */
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

TEST_F(GadToolsGadgetsPixelTest, StringGadgetDoubleBevelRendered) {
    /* STRING_KIND gadgets should use a double-bevel (ridge/groove) border style,
     * like FRAME_RIDGE in AROS frameiclass.c:
     *
     * Outer bevel (row 0, last row, col 0, last col) - RECESSED:
     *   top/left = SHADOWPEN (1), bottom/right = SHINEPEN (2)
     *
     * Inner bevel (row 1, second-to-last row, col 1, second-to-last col) - RAISED:
     *   top/left = SHINEPEN (2), bottom/right = SHADOWPEN (1)
     *
     * This creates a groove effect: dark-light-content-dark-light
     *
     * First string gadget: ng_LeftEdge=140, ng_TopEdge=60, ng_Width=200, ng_Height=14
     */
    int str1_left = 140;
    int str1_top  = 60;
    int str1_w = 200;
    int str1_h = 14;

    /* Inner bevel row 1 (y = str1_top + 1): should have SHINE (pen 2) pixels (raised) */
    int inner_shine_top = 0;
    for (int x = str1_left + 1; x < str1_left + str1_w - 1; x++) {
        int pen = ReadPixel(window_info.x + x, window_info.y + str1_top + 1);
        if (pen == PEN_WHITE) inner_shine_top++;
    }
    EXPECT_GT(inner_shine_top, (str1_w - 2) / 2)
        << "Inner bevel row 1 should have shine (pen 2) pixels (raised), got "
        << inner_shine_top;

    /* Inner bevel second-to-last row (y = str1_top + str1_h - 2):
     * should have SHADOW (pen 1) pixels (raised) */
    int inner_shadow_bot = 0;
    for (int x = str1_left + 1; x < str1_left + str1_w - 1; x++) {
        int pen = ReadPixel(window_info.x + x, window_info.y + str1_top + str1_h - 2);
        if (pen == PEN_BLACK) inner_shadow_bot++;
    }
    EXPECT_GT(inner_shadow_bot, (str1_w - 2) / 2)
        << "Inner bevel second-to-last row should have shadow (pen 1) pixels (raised), got "
        << inner_shadow_bot;

    /* Inner bevel col 1 (x = str1_left + 1): should have SHINE (pen 2) pixels (raised) */
    int inner_shine_left = 0;
    for (int y = str1_top + 1; y < str1_top + str1_h - 1; y++) {
        int pen = ReadPixel(window_info.x + str1_left + 1, window_info.y + y);
        if (pen == PEN_WHITE) inner_shine_left++;
    }
    EXPECT_GT(inner_shine_left, (str1_h - 2) / 2)
        << "Inner bevel col 1 should have shine (pen 2) pixels (raised), got "
        << inner_shine_left;

    /* Inner bevel second-to-last col (x = str1_left + str1_w - 2):
     * should have SHADOW (pen 1) pixels (raised) */
    int inner_shadow_right = 0;
    for (int y = str1_top + 1; y < str1_top + str1_h - 1; y++) {
        int pen = ReadPixel(window_info.x + str1_left + str1_w - 2, window_info.y + y);
        if (pen == PEN_BLACK) inner_shadow_right++;
    }
    EXPECT_GT(inner_shadow_right, (str1_h - 2) / 2)
        << "Inner bevel second-to-last col should have shadow (pen 1) pixels (raised), got "
        << inner_shadow_right;
}

TEST_F(GadToolsGadgetsPixelTest, StringGadgetTextRendered) {
    /* First string gadget at ng_LeftEdge=140, the text area is inset by GT_BEVEL_LEFT.
     * The initial text is "Try pressing" which should produce non-background pixels
     * in the text area.
     * topborder = 20, string 1 TopEdge = 60
     */
    int str1_left = 140 + GT_BEVEL_LEFT;     /* text area starts after bevel inset */
    int str1_top  = 60 + GT_BEVEL_TOP;
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
     * The button is at (190, 120) with size 100x12.
     * PLACETEXT_IN centers the text both horizontally and vertically.
     * Check that the interior of the button has non-background pixels (the text).
     */
    int btn_left = 190;
    int btn_top  = 120;
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

TEST_F(GadToolsGadgetsPixelTest, SliderKnobVisible) {
    /* Slider gadget: ng_LeftEdge=140, TopEdge=40, Width=200, Height=12.
     * The slider has a recessed bevel border (1px), so the container
     * interior is at (141, 41) with size 198x10.
     * The knob should be rendered with non-background pixels (bevel edges).
     * Initial level is 5, min=1, max=20.
     */
    int slider_left = 140;
    int slider_top  = 40;
    int slider_w    = 200;
    int slider_h    = 12;

    /* Check for non-background pixels inside the slider container.
     * The knob is drawn with pen 2 (shine) and pen 1 (shadow) edges.
     * The container interior (after 1px border) should have these pixels. */
    int knob_pixels = CountContentPixels(
        window_info.x + slider_left + 2,
        window_info.y + slider_top + 2,
        window_info.x + slider_left + slider_w - 3,
        window_info.y + slider_top + slider_h - 3,
        PEN_GREY
    );
    EXPECT_GT(knob_pixels, 0)
        << "Slider knob should contain non-background pixels (bevel edges)";

    /* Additionally check for shine pixels (pen 2) in the knob area,
     * which indicates the raised bevel knob top/left edge */
    int shine_pixels = 0;
    for (int x = slider_left + 2; x < slider_left + slider_w - 2; x++) {
        for (int y = slider_top + 2; y < slider_top + slider_h - 2; y++) {
            int pen = ReadPixel(window_info.x + x, window_info.y + y);
            if (pen == PEN_WHITE) shine_pixels++;
        }
    }
    EXPECT_GT(shine_pixels, 0)
        << "Slider knob should have shine (pen 2) pixels from raised bevel";
}

TEST_F(GadToolsGadgetsPixelTest, SliderBevelBorderRendered) {
    /* Slider gadget: ng_LeftEdge=140, TopEdge=40, Width=200, Height=12.
     * The slider uses a RECESSED bevel (shadow top-left, shine bottom-right).
     */
    int slider_left = 140;
    int slider_top  = 40;
    int slider_w    = 200;
    int slider_h    = 12;

    /* Check top edge for shadow (pen 1) - recessed bevel */
    int shadow_top = 0;
    for (int x = slider_left; x < slider_left + slider_w; x++) {
        int pen = ReadPixel(window_info.x + x, window_info.y + slider_top);
        if (pen == PEN_BLACK) shadow_top++;
    }
    EXPECT_GT(shadow_top, slider_w / 2)
        << "Slider top edge should have shadow pixels (pen 1, recessed), got " << shadow_top;

    /* Check bottom edge for shine (pen 2) - recessed bevel */
    int shine_bottom = 0;
    for (int x = slider_left; x < slider_left + slider_w; x++) {
        int pen = ReadPixel(window_info.x + x, window_info.y + slider_top + slider_h - 1);
        if (pen == PEN_WHITE) shine_bottom++;
    }
    EXPECT_GT(shine_bottom, slider_w / 2)
        << "Slider bottom edge should have shine pixels (pen 2, recessed), got " << shine_bottom;
}

TEST_F(GadToolsGadgetsPixelTest, UnderscoreLabelRendered) {
    /* Verify that GT_Underscore labels are rendered correctly:
     * - The underscore prefix character '_' is stripped from the displayed text
     * - The shortcut character has an underline drawn beneath it
     *
     * The slider label "_Volume:   " should display as "Volume:   " with
     * 'V' underlined. The label is positioned to the left of the slider gadget.
     *
     * Slider gadget: LeftEdge=140, TopEdge=40, Width=200, Height=12.
     * Label: "Volume:   " = 10 chars = 80px wide
     * PLACETEXT_LEFT with NG_HIGHLABEL:
     *   LeftEdge = -80 - 4 = -84 (relative to gadget)
     *   TopEdge  = (12 - 8) / 2 + 6 = 8 (relative to gadget, baseline position)
     *
     * So the label text starts at screen position:
     *   x = window_x + 140 + (-84) = window_x + 56
     *   baseline y = window_y + 40 + 8 = window_y + 48
     *   character top = baseline_y - 6 = window_y + 42
     *   character bottom = baseline_y + 1 = window_y + 49
     *
     * The underline '_' character is drawn at ul_pos=0 (under 'V'),
     * at the same position as the 'V' character.
     */
    int label_x = 56;   /* Label text starts here (relative to window) */
    int char_top = 42;  /* Top of character cell (baseline - 6) */
    int char_h = 8;     /* Character height in topaz 8 */

    /* Check that label area has non-background pixels (the "Volume:" text) */
    int label_content = CountContentPixels(
        window_info.x + label_x,
        window_info.y + char_top,
        window_info.x + label_x + 79,  /* 10 chars * 8px */
        window_info.y + char_top + char_h - 1,
        PEN_GREY
    );
    EXPECT_GT(label_content, 20)
        << "Slider label area should contain text pixels ('Volume:')";

    /* Check for underline pixels under the 'V' character.
     * The underscore '_' glyph in topaz 8 occupies the bottom row(s) of the cell.
     * Check the bottom 2 rows of the first character cell (where 'V' is),
     * which is where the underline '_' should add pixels.
     * Row 7 (the last row) of the cell should have the underscore bar.
     */
    int underline_pixels = 0;
    for (int x = label_x; x < label_x + 8; x++) {
        /* Check bottom row of the character cell (row 7 = char_top + 7) */
        int pen = ReadPixel(window_info.x + x, window_info.y + char_top + 7);
        if (pen != PEN_GREY) underline_pixels++;
    }
    EXPECT_GT(underline_pixels, 0)
        << "Bottom row of 'V' cell should have underline pixels from '_' glyph";

    /* Also verify the button label "_Click Here" has its underscore rendered.
     * Button: LeftEdge=190, TopEdge=120, Width=100, Height=12
     * Label: "Click Here" = 10 chars = 80px, PLACETEXT_IN
     *   LeftEdge = (100 - 80) / 2 = 10 (relative to gadget)
     *   TopEdge  = (12 - 8) / 2 + 6 = 8 (relative to gadget, baseline)
     *
     * Button label screen position:
     *   x = window_x + 190 + 10 = window_x + 200
     *   baseline y = window_y + 120 + 8 = window_y + 128
     *   char_top = window_y + 122
     */
    int btn_label_x = 200;      /* Button label text starts here */
    int btn_char_top = 122;     /* Top of character cell */

    /* Check bottom row of the 'C' cell for underline */
    int btn_underline = 0;
    for (int x = btn_label_x; x < btn_label_x + 8; x++) {
        int pen = ReadPixel(window_info.x + x, window_info.y + btn_char_top + 7);
        if (pen != PEN_GREY) btn_underline++;
    }
    EXPECT_GT(btn_underline, 0)
        << "Bottom row of 'C' cell in button label should have underline pixels";
}

TEST_F(GadToolsGadgetsPixelTest, SizeGadgetRendered) {
    /* When WA_SizeGadget=TRUE, a sizing gadget should be created in the
     * bottom-right corner of the window. The sizing gadget has a 3D frame
     * and a sizing icon drawn inside it.
     *
     * When WFLG_SIZEGADGET is set, the window's BorderBottom should be
     * enlarged to accommodate the sizing gadget (from 2 to at least 10).
     *
     * The sizing gadget's 3D frame should have:
     * - Shine (pen 2) pixels on the top-left edges
     * - Shadow (pen 1) pixels on the bottom-right edges
     *
     * Check that the interior of the sizing gadget area (not just the outer
     * border) contains shine pixels from the 3D gadget frame.
     */
    int win_w = window_info.width;
    int win_h = window_info.height;

    /* The sizing gadget area is 18x10 in the bottom-right corner.
     * Check the interior (inset 2px from edges to avoid outer window border)
     * for shine (pen 2) pixels that come from the sizing gadget's 3D frame. */
    int size_area_left = window_info.x + win_w - 16;
    int size_area_top  = window_info.y + win_h - 8;
    int size_area_right = window_info.x + win_w - 3;
    int size_area_bottom = window_info.y + win_h - 3;

    int shine_count = 0;
    for (int x = size_area_left; x <= size_area_right; x++) {
        for (int y = size_area_top; y <= size_area_bottom; y++) {
            int pen = ReadPixel(x, y);
            if (pen == PEN_WHITE) shine_count++;
        }
    }
    EXPECT_GT(shine_count, 3)
        << "Sizing gadget interior should have shine (pen 2) pixels from 3D frame, got "
        << shine_count;

    /* Also verify that shadow (pen 1) pixels exist in the sizing area */
    int shadow_count = 0;
    for (int x = size_area_left; x <= size_area_right; x++) {
        for (int y = size_area_top; y <= size_area_bottom; y++) {
            int pen = ReadPixel(x, y);
            if (pen == PEN_BLACK) shadow_count++;
        }
    }
    EXPECT_GT(shadow_count, 3)
        << "Sizing gadget interior should have shadow (pen 1) pixels from 3D frame, got "
        << shadow_count;
}

TEST_F(GadToolsGadgetsPixelTest, DepthGadgetRendered) {
    /* The depth gadget is in the top-right corner of the window title bar.
     * It should show two overlapping window-outline rectangles (AROS style):
     * - Back rectangle (top-left): outline in shadow pen (pen 1)
     * - Front rectangle (bottom-right): outline in shadow pen, filled with shine pen (pen 2)
     * - Background (pen 0) fills the rest of the gadget interior
     * Gadget position: rightX = Width - gadWidth (at far right of title bar)
     */
    int gadWidth = 18;
    int gadHeight = TITLE_BAR_HEIGHT - 1;  /* 10 */

    /* Depth gadget is at the far right of the window, inside the title bar */
    int depth_left = window_info.x + window_info.width - gadWidth;
    int depth_top = window_info.y;

    /* Count shadow pixels (pen 1) in the depth gadget interior.
     * Both the back and front rectangle outlines are drawn in shadow pen,
     * so there should be a significant number. */
    int shadow_count = 0;
    int shine_count = 0;
    int bg_count = 0;
    for (int x = depth_left + 2; x < depth_left + gadWidth - 1; x++) {
        for (int y = depth_top + 2; y < depth_top + gadHeight - 1; y++) {
            int pen = ReadPixel(x, y);
            if (pen == PEN_BLACK) shadow_count++;
            else if (pen == PEN_WHITE) shine_count++;
            else if (pen == 0) bg_count++;
        }
    }
    /* Shadow pixels form rectangle outlines - expect multiple edges worth */
    EXPECT_GT(shadow_count, 8)
        << "Depth gadget should have shadow (pen 1) pixels for rectangle outlines";

    /* Shine pixels fill the front rectangle interior */
    EXPECT_GT(shine_count, 2)
        << "Depth gadget should have shine (pen 2) pixels for front rectangle fill";

    /* Background pixels fill the back rectangle interior and margins */
    EXPECT_GT(bg_count, 2)
        << "Depth gadget should have background (pen 0) pixels in margins/back rect";
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
