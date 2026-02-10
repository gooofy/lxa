/**
 * intuitext_gtest.cpp - Google Test driver for IntuiText sample
 *
 * Tests that the IntuiText sample matches RKM behavior:
 * - Window opens full-screen (width == screen width, height == screen height)
 * - No title bar (RKM original uses no WA_Title)
 * - Text rendered via PrintIText() is visible and doesn't collide with border
 */

#include "lxa_test.h"

using namespace lxa::testing;

/* ============================================================================
 * Behavioral tests (rootless mode, event-driven)
 * ============================================================================ */

class IntuiTextTest : public LxaUITest {
protected:
    void SetUp() override {
        LxaUITest::SetUp();

        ASSERT_EQ(lxa_load_program("SYS:IntuiText", ""), 0);
        ASSERT_TRUE(WaitForWindows(1, 5000));
        ASSERT_TRUE(GetWindowInfo(0, &window_info));

        /* Let program initialize and draw text */
        RunCyclesWithVBlank(30);
    }
};

TEST_F(IntuiTextTest, WindowOpensFullScreen) {
    /* The RKM original opens with OpenWindowTags(NULL, WA_PubScreen, screen,
     * WA_RMBTrap, TRUE, TAG_END) — no WA_Width or WA_Height.
     * Per AROS/AmigaOS behavior, this should default to full screen size. */
    lxa_screen_info_t screen_info;
    ASSERT_TRUE(lxa_get_screen_info(&screen_info));

    EXPECT_EQ(window_info.width, screen_info.width)
        << "Window width should equal screen width (full-screen)";
    EXPECT_EQ(window_info.height, screen_info.height)
        << "Window height should equal screen height (full-screen)";
}

TEST_F(IntuiTextTest, WindowHasNoBorderTop) {
    /* Without WA_Title, WA_DragBar, WA_CloseGadget, or WA_DepthGadget,
     * the window should have a thin border (BorderTop == WBorBottom, typically 2)
     * rather than a title bar (BorderTop == WBorTop, typically 11). */
    std::string output = GetOutput();

    /* The sample logs BorderTop value */
    EXPECT_NE(output.find("BorderTop=2"), std::string::npos)
        << "Window should have thin border (BorderTop=2, no title bar). Output: " << output;
}

TEST_F(IntuiTextTest, TextDrawnAndExitsCleanly) {
    /* Wait for program to finish (it has a Delay(100) = 2s then exits) */
    EXPECT_TRUE(lxa_wait_exit(10000))
        << "Program should complete within 10 seconds";

    /* Verify PrintIText was called and completed */
    std::string output = GetOutput();

    EXPECT_NE(output.find("Text drawn successfully"), std::string::npos)
        << "PrintIText should complete successfully. Output: " << output;
    EXPECT_NE(output.find("Chained text drawn successfully"), std::string::npos)
        << "Chained PrintIText should complete successfully. Output: " << output;
    EXPECT_NE(output.find("Closing window"), std::string::npos)
        << "Program should close window before exit. Output: " << output;
}

/* ============================================================================
 * Pixel verification tests (non-rootless mode)
 * ============================================================================ */

class IntuiTextPixelTest : public LxaUITest {
protected:
    void SetUp() override {
        /* Disable rootless so Intuition renders window frames to screen bitmap */
        config.rootless = false;

        LxaUITest::SetUp();

        ASSERT_EQ(lxa_load_program("SYS:IntuiText", ""), 0);
        ASSERT_TRUE(WaitForWindows(1, 5000));
        ASSERT_TRUE(GetWindowInfo(0, &window_info));

        /* Let rendering complete */
        WaitForEventLoop(200, 10000);
        RunCyclesWithVBlank(30, 200000);
    }
};

TEST_F(IntuiTextPixelTest, TextIsVisible) {
    /* The "Hello, World.  ;-)" text is drawn at PrintIText(rp, &myIText, 10, 10).
     * With BorderTop=2 and no title bar, the text at y=10 should be well inside
     * the window client area. Check for non-background pixels in the text area. */
    lxa_flush_display();

    /* Text starts at window origin + (10 + LeftEdge, 10 + TopEdge) in the
     * IntuiText struct (LeftEdge=0, TopEdge=0), plus the window's border offsets.
     * With BorderTop=2, BorderLeft=4, the text at PrintIText(rp, &myIText, 10, 10)
     * appears at window_x + 4 + 10, window_y + 2 + 10 = window_x+14, window_y+12.
     * The text is ~18 chars * 8px wide = ~144px, 8px tall. */
    int text_content = CountContentPixels(
        window_info.x + 14,      /* BorderLeft(4) + PrintIText x offset(10) */
        window_info.y + 12,      /* BorderTop(2) + PrintIText y offset(10) */
        window_info.x + 14 + 143, /* ~18 chars * 8px */
        window_info.y + 12 + 7,   /* 8px font height */
        0  /* background pen */
    );
    EXPECT_GT(text_content, 10)
        << "Text 'Hello, World.  ;-)' should have visible pixels (non-background content)";
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
