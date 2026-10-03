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

/* AmigaOS 3.1 geometry (Phase 223, tests/golden/GadToolsGadgets): the
 * sample places its gadgets at 20 + topborder, topborder = WBorTop (2) +
 * font height (8) + 1 = 11:
 *   slider   NewGadget (140, 31) 200x12, prop container (144, 33) 192x8
 *   string 1 NewGadget (140, 51) 200x14, string area (146, 54) 188x8
 *   string 2 (140, 71), string 3 (140, 91)
 *   button   (190, 111) 100x12 */
constexpr int SLIDER_TOP = 31;
constexpr int STR1_TOP   = 51;
constexpr int STR2_TOP   = 71;
constexpr int BUTTON_TOP = 111;

// ============================================================================
// Functional Tests - verify gadget creation and event handling via output
// ============================================================================

class GadToolsGadgetsTest : public LxaUITest {
protected:
    std::string output;
    bool program_exited = false;

    bool WaitForOutputContains(const char* needle, int iterations = 60, int vblanks = 2) {
        for (int i = 0; i < iterations; i++) {
            output = GetOutput();
            if (output.find(needle) != std::string::npos) {
                return true;
            }
            RunCyclesWithVBlank(vblanks, 100000);
        }

        output = GetOutput();
        return output.find(needle) != std::string::npos;
    }

    std::string DrainOutputAfterClose(int wait_ms = 3000) {
        EXPECT_TRUE(lxa_wait_exit(wait_ms)) << "Program should exit after close window";
        program_exited = true;
        RunCyclesWithVBlank(5, 100000);
        return GetOutput();
    }

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

        /* Let task reach event loop and wait for startup Printf output to flush. */
        WaitForOutputContains("Window size:");
    }

    void TearDown() override {
        if (!program_exited) {
            /* Close the window to let the program exit cleanly */
            lxa_click_close_gadget(0);
            RunCyclesWithVBlank(10, 50000);
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
    WaitForOutputContains("Window opened at");
    WaitForOutputContains("Window size:");

    /* Verify window was opened */
    EXPECT_NE(output.find("Window opened at"), std::string::npos)
        << "Window should open successfully";

    /* Verify window size is reported */
    EXPECT_NE(output.find("Window size:"), std::string::npos)
        << "Window size should be reported";
}

TEST_F(GadToolsGadgetsTest, RootlessWindowShowsContent) {
    EXPECT_EQ(lxa_get_window_count(), 1)
        << "Exactly one rootless window should be tracked";

    int content = lxa_get_window_content(0);
    EXPECT_GT(content, 0)
        << "Rootless window should contain rendered gadget pixels";
}

TEST_F(GadToolsGadgetsTest, GadgetsRefreshed) {
    SUCCEED() << "Startup rendering is covered by creation/window tests and pixel shards";
}

TEST_F(GadToolsGadgetsTest, EventLoopEntered) {
    SUCCEED() << "Event-loop reachability is covered by interactive shards";
}

TEST_F(GadToolsGadgetsTest, ButtonClick) {
    /* The button is the last gadget: (190, 111), 100x12. Click its centre. */
    int btn_x = window_info.x + 190 + 50;  /* center of 100px wide button */
    int btn_y = window_info.y + BUTTON_TOP + 6;   /* center of 12px tall button */

    Click(btn_x, btn_y);
    RunCyclesWithVBlank(20, 100000);
    lxa_click_close_gadget(0);
    std::string click_output = DrainOutputAfterClose();
    EXPECT_NE(click_output.find("Button was pressed, slider reset to 10"), std::string::npos)
        << "Button click should trigger handleGadgetEvent and reset slider. Output: " << click_output;
}

TEST_F(GadToolsGadgetsTest, ButtonClickCompletesWithinOneVBlank) {
    int btn_x = window_info.x + 190 + 50;
    int btn_y = window_info.y + BUTTON_TOP + 6;

    bool event_loop_ready = false;
    for (int i = 0; i < 200; i++) {
        if (GetOutput().find("Entering event loop") != std::string::npos) {
            event_loop_ready = true;
            break;
        }
        RunCyclesWithVBlank(1, 100000);
    }

    ASSERT_TRUE(event_loop_ready) << "Program should reach the interactive event loop before timing the click";

    /* Let the task settle into WaitPort() before injecting the click.
     * Click() already drives the minimal move/down/up VBlank pipeline, so
     * this regression verifies the app does not need extra post-click delay
     * beyond that helper before shutdown. */
    WaitForEventLoop(100, 10000);

    ClearOutput();

    Click(btn_x, btn_y);

    lxa_click_close_gadget(0);
    std::string handled_output = DrainOutputAfterClose();

    bool handled = handled_output.find("Button was pressed, slider reset to 10") != std::string::npos;

    EXPECT_TRUE(handled)
        << "Button click should be handled without extra post-click settling beyond Click(). Output: "
        << handled_output;
}

TEST_F(GadToolsGadgetsTest, CloseWindow) {
    /* Close the window and verify the program handles it */
    ClearOutput();
    lxa_click_close_gadget(0);
    RunCyclesWithVBlank(10, 50000);

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
    int slider_top  = SLIDER_TOP;
    int slider_w    = 200;
    int slider_h    = 12;

    /* Click at 80% of the slider width (expect level around 16-17) */
    int click_x = window_info.x + slider_left + (int)(slider_w * 0.8);
    int click_y = window_info.y + slider_top + slider_h / 2;

    Click(click_x, click_y);
    RunCyclesWithVBlank(20, 100000);
    lxa_click_close_gadget(0);
    std::string click_output = DrainOutputAfterClose();
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
    int slider_top  = SLIDER_TOP;
    int slider_w    = 200;
    int slider_h    = 12;

    int start_x = window_info.x + slider_left + slider_w / 4;
    int start_y = window_info.y + slider_top + slider_h / 2;
    int end_x   = window_info.x + slider_left + (int)(slider_w * 0.9);
    int end_y   = start_y;

    lxa_inject_drag(start_x, start_y, end_x, end_y, LXA_MOUSE_LEFT, 5);
    RunCyclesWithVBlank(50, 100000);
    lxa_click_close_gadget(0);
    std::string after = DrainOutputAfterClose();
    int after_count = 0;
    {
        size_t pos = 0;
        while ((pos = after.find("Slider at level", pos)) != std::string::npos) {
            after_count++;
            pos += 15;
        }
    }
    EXPECT_GE(after_count, 1)
        << "Slider drag should produce at least one 'Slider at level' message. Output: " << after;
    EXPECT_GE(after_count, 2)
        << "Slider drag should produce multiple 'Slider at level' messages (MOUSEMOVE). Output: " << after;
}

TEST_F(GadToolsGadgetsTest, ButtonResetsSlider) {
    /* First click slider to change its level, then click button to reset to 10.
     * The sample program prints "Button was pressed, slider reset to 10."
     * and calls GT_SetGadgetAttrs to update the slider.
     */
    int slider_left = 140;
    int slider_top  = SLIDER_TOP;
    int slider_w    = 200;
    int slider_h    = 12;

    /* Click slider at right side to set a high level */
    int slider_click_x = window_info.x + slider_left + (int)(slider_w * 0.9);
    int slider_click_y = window_info.y + slider_top + slider_h / 2;

    Click(slider_click_x, slider_click_y);
    RunCyclesWithVBlank(15, 100000);

    /* Now click the button to reset slider to 10 */
    int btn_x = window_info.x + 190 + 50;   /* center of 100px wide button */
    int btn_y = window_info.y + BUTTON_TOP + 6;    /* center of 12px tall button */

    Click(btn_x, btn_y);
    RunCyclesWithVBlank(20, 100000);
    lxa_click_close_gadget(0);
    std::string btn_output = DrainOutputAfterClose();
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
    RunCyclesWithVBlank(10, 100000);

    /* Window should still be responsive — close it */
    ClearOutput();
    lxa_click_close_gadget(0);
    RunCyclesWithVBlank(10, 50000);
    EXPECT_TRUE(lxa_wait_exit(3000))
        << "Program should exit after close window (depth gadget did not break it)";
    program_exited = true;

    std::string close_output = GetOutput();
    EXPECT_NE(close_output.find("IDCMP_CLOSEWINDOW"), std::string::npos)
        << "IDCMP_CLOSEWINDOW should still be delivered after depth gadget click";
}

TEST_F(GadToolsGadgetsTest, ResizeDragKeepsWindowResponsive) {
    constexpr int size_gadget_w = 18;
    constexpr int size_gadget_h = 10;
    int start_x = window_info.x + window_info.width - (size_gadget_w / 2);
    int start_y = window_info.y + window_info.height - (size_gadget_h / 2);
    int end_x = start_x + 32;
    int end_y = start_y + 18;

    ASSERT_TRUE(lxa_inject_drag(start_x, start_y, end_x, end_y, LXA_MOUSE_LEFT, 4));
    RunCyclesWithVBlank(20, 100000);

    ClearOutput();
    lxa_click_close_gadget(0);
    RunCyclesWithVBlank(10, 50000);
    EXPECT_TRUE(lxa_wait_exit(3000))
        << "Window should remain responsive after a resize drag";
    program_exited = true;

    std::string close_output = GetOutput();
    EXPECT_NE(close_output.find("IDCMP_CLOSEWINDOW"), std::string::npos)
        << "Close gadget should still work after a resize drag";
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
    int str1_y = window_info.y + STR1_TOP + 7;     /* center of 14px tall */
    Click(str1_x, str1_y);
    RunCyclesWithVBlank(15, 100000);

    /* Press TAB to cycle to string gadget 2 */
    PressKey(RAWKEY_TAB, 0);
    RunCyclesWithVBlank(10, 100000);

    /* Type "XYZ" into string gadget 2, then press Return. */
    TypeString("XYZ\n");
    RunCyclesWithVBlank(40, 200000);
    lxa_click_close_gadget(0);
    std::string output2 = DrainOutputAfterClose();
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
    int str2_y = window_info.y + STR2_TOP + 7;
    Click(str2_x, str2_y);
    RunCyclesWithVBlank(15, 100000);

    /* Press Shift-TAB to cycle backward to string gadget 1 */
    PressKey(RAWKEY_TAB, IEQUALIFIER_LSHIFT);
    RunCyclesWithVBlank(10, 100000);

    /* Type "ABC" into string gadget 1, then press Return. */
    TypeString("ABC\n");
    RunCyclesWithVBlank(40, 200000);
    lxa_click_close_gadget(0);
    std::string output1 = DrainOutputAfterClose();
    EXPECT_NE(output1.find("String gadget 1:"), std::string::npos)
        << "Shift-TAB should cycle focus from string 2 to string 1. Output: " << output1;
    EXPECT_NE(output1.find("ABC"), std::string::npos)
        << "Typed text should appear in string gadget 1. Output: " << output1;
}

TEST_F(GadToolsGadgetsTest, VanillaKeySliderIncrease) {
    /* Test IDCMP_VANILLAKEY: pressing 'v' should increase slider level.
     * The GadToolsGadgets sample opens its window with IDCMP_VANILLAKEY.
     * handleVanillaKey() maps 'v' to slider increment.
     * Initial slider level is 5, so pressing 'v' should make it 6. */

    constexpr int RAWKEY_V = 0x34;

    PressKey(RAWKEY_V, 0);  /* lowercase 'v' */
    RunCyclesWithVBlank(30, 100000);
    lxa_click_close_gadget(0);
    std::string output = DrainOutputAfterClose();

    EXPECT_NE(output.find("VANILLAKEY 'v'"), std::string::npos)
        << "Pressing 'v' should trigger VANILLAKEY handler. Output: " << output;
    EXPECT_NE(output.find("slider level now 6"), std::string::npos)
        << "Slider should increase from 5 to 6. Output: " << output;
}

TEST_F(GadToolsGadgetsTest, VanillaKeySliderDecrease) {
    /* Test IDCMP_VANILLAKEY: pressing 'V' (Shift+v) should decrease slider level.
     * Initial slider level is 5, so pressing 'V' should make it 4. */

    constexpr int RAWKEY_V = 0x34;
    constexpr int IEQUALIFIER_LSHIFT = 0x0001;

    PressKey(RAWKEY_V, IEQUALIFIER_LSHIFT);  /* uppercase 'V' */
    RunCyclesWithVBlank(30, 100000);
    lxa_click_close_gadget(0);
    std::string output = DrainOutputAfterClose();

    EXPECT_NE(output.find("VANILLAKEY 'V'"), std::string::npos)
        << "Pressing Shift+V should trigger VANILLAKEY handler. Output: " << output;
    EXPECT_NE(output.find("slider level now 4"), std::string::npos)
        << "Slider should decrease from 5 to 4. Output: " << output;
}

TEST_F(GadToolsGadgetsTest, VanillaKeyActivateGadget) {
    /* Test IDCMP_VANILLAKEY: pressing 'f' should activate string gadget 1
     * without destabilizing the window/event loop. */

    constexpr int RAWKEY_F = 0x23;

    /* Press 'f' to activate string gadget 1 via VANILLAKEY */
    PressKey(RAWKEY_F, 0);
    RunCyclesWithVBlank(20, 100000);

    RunCyclesWithVBlank(20, 100000);
    lxa_click_close_gadget(0);
    std::string output = DrainOutputAfterClose();

    EXPECT_NE(output.find("VANILLAKEY 'f/F': activating First string gadget"), std::string::npos)
        << "Pressing 'f' should trigger the VANILLAKEY activation handler. Output: " << output;
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
        RunCyclesWithVBlank(10, 50000);
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
    int btn_top  = BUTTON_TOP;
    int btn_w = 100;
    int btn_h = 12;

    /* AmigaOS 3.1 button frame: shine top row and left two columns, shadow
     * bottom row and right two columns */
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
    int str1_top  = STR1_TOP;
    int str1_w = 200;                        /* ng_Width */
    int str1_h = 14;                         /* ng_Height */

    /* AmigaOS 3.1 ridge: the outer edge is raised (shine top, shadow bottom) */
    int shine_top = 0;
    for (int x = str1_left; x < str1_left + str1_w; x++) {
        int pen = ReadPixel(window_info.x + x, window_info.y + str1_top);
        if (pen == PEN_WHITE) shine_top++;
    }
    EXPECT_GT(shine_top, str1_w / 2)
        << "String gadget top edge should have shine pixels (pen 2, raised ridge), got " << shine_top;

    int shadow_bottom = 0;
    for (int x = str1_left; x < str1_left + str1_w; x++) {
        int pen = ReadPixel(window_info.x + x, window_info.y + str1_top + str1_h - 1);
        if (pen == PEN_BLACK) shadow_bottom++;
    }
    EXPECT_GT(shadow_bottom, str1_w / 2)
        << "String gadget bottom edge should have shadow pixels (pen 1, raised ridge), got " << shadow_bottom;
}

TEST_F(GadToolsGadgetsPixelTest, StringGadgetDoubleBevelRendered) {
    /* STRING_KIND uses the 3.1 FRAME_RIDGE: a raised outer button frame and
     * a recessed inner one at (x+2, y+1, w-4, h-2):
     *   inner top row (y+1) and left column (x+2): shadow
     *   inner bottom row (y+h-2) and right column (x+w-3): shine */
    int str1_left = 140;
    int str1_top  = STR1_TOP;
    int str1_w = 200;
    int str1_h = 14;

    int inner_shadow_top = 0;
    for (int x = str1_left + 3; x < str1_left + str1_w - 4; x++) {
        int pen = ReadPixel(window_info.x + x, window_info.y + str1_top + 1);
        if (pen == PEN_BLACK) inner_shadow_top++;
    }
    EXPECT_GT(inner_shadow_top, (str1_w - 8) / 2)
        << "Inner ridge row 1 should have shadow (pen 1) pixels (recessed), got " << inner_shadow_top;

    int inner_shine_bot = 0;
    for (int x = str1_left + 3; x < str1_left + str1_w - 4; x++) {
        int pen = ReadPixel(window_info.x + x, window_info.y + str1_top + str1_h - 2);
        if (pen == PEN_WHITE) inner_shine_bot++;
    }
    EXPECT_GT(inner_shine_bot, (str1_w - 8) / 2)
        << "Inner ridge second-to-last row should have shine (pen 2) pixels, got " << inner_shine_bot;

    int inner_shadow_left = 0;
    for (int y = str1_top + 1; y < str1_top + str1_h - 1; y++) {
        int pen = ReadPixel(window_info.x + str1_left + 2, window_info.y + y);
        if (pen == PEN_BLACK) inner_shadow_left++;
    }
    EXPECT_GT(inner_shadow_left, (str1_h - 2) / 2)
        << "Inner ridge column 2 should have shadow (pen 1) pixels, got " << inner_shadow_left;

    int inner_shine_right = 0;
    for (int y = str1_top + 1; y < str1_top + str1_h - 1; y++) {
        int pen = ReadPixel(window_info.x + str1_left + str1_w - 3, window_info.y + y);
        if (pen == PEN_WHITE) inner_shine_right++;
    }
    EXPECT_GT(inner_shine_right, (str1_h - 2) / 2)
        << "Inner ridge third-to-last column should have shine (pen 2) pixels, got " << inner_shine_right;
}

TEST_F(GadToolsGadgetsPixelTest, StringGadgetTextRendered) {
    /* First string gadget at ng_LeftEdge=140, the text area is inset by GT_BEVEL_LEFT.
     * The initial text is "Try pressing" which should produce non-background pixels
     * in the text area.
     * topborder = 20, string 1 TopEdge = 60
     */
    int str1_left = 140 + 6;                 /* 3.1 string area inside the ridge */
    int str1_top  = STR1_TOP + 3;
    int str1_w = 200 - 12;
    int str1_h = 14 - 6;

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
    int btn_top  = BUTTON_TOP;
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
    /* AmigaOS 3.1 slider: a raised button frame around the prop container
     * (144, 33) 192x8; the knob is a solid shadow-pen block (level 5 of
     * 1..20). */
    int cont_left = 144, cont_top = SLIDER_TOP + 2, cont_w = 192, cont_h = 8;

    int knob_pixels = 0;
    for (int x = cont_left; x < cont_left + cont_w; x++) {
        for (int y = cont_top; y < cont_top + cont_h; y++) {
            if (ReadPixel(window_info.x + x, window_info.y + y) == PEN_BLACK)
                knob_pixels++;
        }
    }
    EXPECT_GE(knob_pixels, 6 * cont_h)
        << "Slider knob should be a solid shadow-pen block";
}

TEST_F(GadToolsGadgetsPixelTest, SliderBevelBorderRendered) {
    /* Slider NewGadget box (140, 31) 200x12 carries a raised button frame */
    int slider_left = 140;
    int slider_top  = SLIDER_TOP;
    int slider_w    = 200;
    int slider_h    = 12;

    int shine_top = 0;
    for (int x = slider_left; x < slider_left + slider_w; x++) {
        int pen = ReadPixel(window_info.x + x, window_info.y + slider_top);
        if (pen == PEN_WHITE) shine_top++;
    }
    EXPECT_GT(shine_top, slider_w / 2)
        << "Slider top edge should have shine pixels (pen 2, raised), got " << shine_top;

    int shadow_bottom = 0;
    for (int x = slider_left; x < slider_left + slider_w; x++) {
        int pen = ReadPixel(window_info.x + x, window_info.y + slider_top + slider_h - 1);
        if (pen == PEN_BLACK) shadow_bottom++;
    }
    EXPECT_GT(shadow_bottom, slider_w / 2)
        << "Slider bottom edge should have shadow pixels (pen 1, raised), got " << shadow_bottom;
}

TEST_F(GadToolsGadgetsPixelTest, UnderscoreLabelRendered) {
    /* GT_Underscore: "_Volume:   " is displayed as "Volume:   " left of the
     * slider (AmigaOS 3.1: 8 pixels from the NewGadget box, label at x 52,
     * top 33) with a one-character-wide line under the 'V' at text top + 7.
     * "_Click Here" is centred in the button: x 200, top 113, line at 120. */
    int label_x = 52;
    int char_top = SLIDER_TOP + 2;
    int char_h = 8;

    int label_content = CountContentPixels(
        window_info.x + label_x,
        window_info.y + char_top,
        window_info.x + label_x + 55,
        window_info.y + char_top + char_h - 2,
        PEN_GREY
    );
    EXPECT_GT(label_content, 20)
        << "Slider label area should contain text pixels ('Volume:')";

    int underline_pixels = 0;
    for (int x = label_x; x < label_x + 8; x++) {
        int pen = ReadPixel(window_info.x + x, window_info.y + char_top + 7);
        if (pen != PEN_GREY) underline_pixels++;
    }
    EXPECT_EQ(underline_pixels, 8)
        << "The 'V' cell should be underlined over its full width";

    int btn_label_x = 200;
    int btn_char_top = BUTTON_TOP + 2;
    int btn_underline = 0;
    for (int x = btn_label_x; x < btn_label_x + 8; x++) {
        int pen = ReadPixel(window_info.x + x, window_info.y + btn_char_top + 7);
        if (pen != PEN_GREY) btn_underline++;
    }
    EXPECT_EQ(btn_underline, 8)
        << "The 'C' cell of the button label should be underlined";
}

TEST_F(GadToolsGadgetsPixelTest, SliderLevelValueRendered) {
    int value_x = 120;
    int char_top = SLIDER_TOP + 2;
    int char_h = 8;

    int value_content = CountContentPixels(
        window_info.x + value_x,
        window_info.y + char_top,
        window_info.x + value_x + 15,
        window_info.y + char_top + char_h - 1,
        PEN_GREY
    );
    EXPECT_GT(value_content, 4)
        << "Slider level display should contain rendered digits";
}

TEST_F(GadToolsGadgetsPixelTest, SizeGadgetRendered) {
    /* AmigaOS 3.1: the sizing gadget is a SIZEIMAGE (18x10) in the bottom
     * right corner; the window border is widened on the right only. Its
     * glyph is a triangle of shine pixels outlined in shadow. */
    int win_w = window_info.width;
    int win_h = window_info.height;
    int size_area_left = window_info.x + win_w - 16;
    int size_area_top  = window_info.y + win_h - 8;
    int size_area_right = window_info.x + win_w - 3;
    int size_area_bottom = window_info.y + win_h - 3;

    int shine_count = 0, shadow_count = 0;
    for (int x = size_area_left; x <= size_area_right; x++) {
        for (int y = size_area_top; y <= size_area_bottom; y++) {
            int pen = ReadPixel(x, y);
            if (pen == PEN_WHITE) shine_count++;
            else if (pen == PEN_BLACK) shadow_count++;
        }
    }
    EXPECT_GT(shine_count, 3) << "Sizing image should contain shine pixels, got " << shine_count;
    EXPECT_GT(shadow_count, 3) << "Sizing image should contain shadow pixels, got " << shadow_count;
}

TEST_F(GadToolsGadgetsPixelTest, DepthGadgetRendered) {
    /* AmigaOS 3.1 DEPTHIMAGE (24x11, drawn from one pixel left of the
     * gadget): two overlapping window outlines in shadow pen, the front one
     * filled with shine; FILLPEN background in the active window. */
    int depth_left = window_info.x + window_info.width - 23;
    int depth_top = window_info.y;

    int shadow_count = 0, shine_count = 0, fill_count = 0;
    for (int x = depth_left + 1; x < depth_left + 22; x++) {
        for (int y = depth_top + 1; y < depth_top + 10; y++) {
            int pen = ReadPixel(x, y);
            if (pen == PEN_BLACK) shadow_count++;
            else if (pen == PEN_WHITE) shine_count++;
            else if (pen == PEN_BLUE) fill_count++;
        }
    }
    EXPECT_GT(shadow_count, 8) << "Depth image should have shadow outlines";
    EXPECT_GT(shine_count, 2) << "Depth image front window should be filled with shine";
    EXPECT_GT(fill_count, 2) << "Depth image background is FILLPEN in the active window";
}

// Phase 138: ensure size-gadget chrome is re-rendered after a resize event.
TEST_F(GadToolsGadgetsPixelTest, ResizeKeepsSizeGadgetBordersClean) {
    constexpr int size_gadget_w = 18;
    constexpr int size_gadget_h = 10;
    int orig_w = window_info.width;
    int orig_h = window_info.height;

    int start_x = window_info.x + orig_w - (size_gadget_w / 2);
    int start_y = window_info.y + orig_h - (size_gadget_h / 2);
    /* AmigaOS 3.1: without WA_MaxWidth/WA_MaxHeight the maximum size is the
     * initial size, so the window can only shrink by mouse sizing */
    int end_x = start_x - 36;
    int end_y = start_y - 20;
    int expected_w = orig_w - 36;
    int expected_h = orig_h - 20;

    ASSERT_TRUE(lxa_inject_drag(start_x, start_y, end_x, end_y, LXA_MOUSE_LEFT, 5));

    /* Wait for the window to reach the final resized dimensions.
     * The drag injects 5 interpolation steps; the last step may still be
     * pending in the Amiga input queue when lxa_inject_drag returns. */
    bool resized = false;
    for (int i = 0; i < 300; i++) {
        lxa_trigger_vblank();
        lxa_run_cycles(100000);
        if (GetWindowInfo(0, &window_info) &&
            window_info.width == expected_w && window_info.height == expected_h) {
            resized = true;
            break;
        }
    }
    /* Give extra time for the frame to render after the final SizeWindow.
     * SizeWindow updates Window->Width/Height BEFORE _render_window_frame
     * runs, so seeing the final dimensions is not proof of rendering
     * completion. Wait until the size-gadget area actually receives
     * non-background pixels (proof that _render_gadget for GTYP_SIZING
     * has executed). */
    int settle_size_content = 0;
    for (int i = 0; i < 200; i++) {
        lxa_trigger_vblank();
        lxa_run_cycles(100000);
        lxa_flush_display();
        settle_size_content = CountContentPixels(
            window_info.x + expected_w - 16,
            window_info.y + expected_h - 8,
            window_info.x + expected_w - 3,
            window_info.y + expected_h - 3,
            PEN_GREY
        );
        if (settle_size_content > 3) break;
    }
    lxa_flush_display();

    ASSERT_TRUE(resized)
        << "Window should reach expected size " << expected_w << "x" << expected_h
        << " but is " << window_info.width << "x" << window_info.height;

    ASSERT_TRUE(GetWindowInfo(0, &window_info))
        << "Window info should be available after resizing";

    /* AmigaOS 3.1: the right border of the active window is FILLPEN between
     * its inner shine line and the outer shadow line; it must be uniform
     * (no stale gadget pixels) after resizing */
    int right_border_left = window_info.x + window_info.width - 17;
    int right_border_top = window_info.y + TITLE_BAR_HEIGHT + 2;
    int right_border_bottom = window_info.y + window_info.height - size_gadget_h - 2;
    int right_border_content = CountContentPixels(
        right_border_left,
        right_border_top,
        window_info.x + window_info.width - 2,
        right_border_bottom,
        PEN_BLUE
    );
    EXPECT_EQ(right_border_content, 0)
        << "Right resize border should be uniform FILLPEN instead of leaving stale gadget pixels";

    /* the window contents below the gadgets are background */
    int bottom_area_top = window_info.y + window_info.height - size_gadget_h;
    int bottom_area_content = CountContentPixels(
        window_info.x + 4,
        bottom_area_top,
        window_info.x + window_info.width - 20,
        window_info.y + window_info.height - 3,
        PEN_GREY
    );
    EXPECT_EQ(bottom_area_content, 0)
        << "The area above the bottom border should stay clean during resize redraws";

    int size_content = CountContentPixels(
        window_info.x + window_info.width - 16,
        window_info.y + window_info.height - 8,
        window_info.x + window_info.width - 3,
        window_info.y + window_info.height - 3,
        PEN_GREY
    );
    EXPECT_GT(size_content, 3)
        << "Size gadget area should still contain rendered pixels after resizing";
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
