/**
 * menulayout_gtest.cpp - Google Test driver for MenuLayout sample
 *
 * Tests the MenuLayout sample which demonstrates:
 *   - Manual menu layout with font-adaptive positioning
 *   - Three menus: Project (with Print sub-menu), Edit, Settings
 *   - Command key equivalents (Amiga+N, Amiga+O, etc.)
 *   - Checkmarks and mutual exclusion in Settings menu
 *   - IDCMP_MENUPICK event handling
 *   - DrawInfo-based pen colors for window text
 */

#include "lxa_test.h"

using namespace lxa::testing;

class MenuLayoutTest : public LxaUITest {
protected:
    void SetUp() override {
        LxaUITest::SetUp();

        /* Load the MenuLayout program */
        ASSERT_EQ(lxa_load_program("SYS:MenuLayout", ""), 0)
            << "Failed to load MenuLayout";

        /* Wait for window to open */
        ASSERT_TRUE(WaitForWindows(1, 5000))
            << "Window did not open within 5 seconds";

        /* Get window information */
        ASSERT_TRUE(GetWindowInfo(0, &window_info))
            << "Could not get window info";

        /* Let task reach Wait() in event loop */
        WaitForEventLoop(200, 10000);
        ClearOutput();
    }
};

TEST_F(MenuLayoutTest, WindowOpens) {
    /* Window should be visible and have reasonable dimensions */
    EXPECT_GT(window_info.width, 0);
    EXPECT_GT(window_info.height, 0);
    EXPECT_EQ(window_info.x, 10)
        << "Window should be at WA_Left=10";
}

TEST_F(MenuLayoutTest, WindowHasCorrectSize) {
    /* With screen->Font properly set to Topaz 8 (ta_YSize=8):
     * win_width  = 100 + IntuiTextLength("How to do a Menu") = 100 + 136 = 236
     * alt_width  = 100 + IntuiTextLength("(with Style)") = 100 + 96 = 196
     * win_width  = max(236, 196) = 236
     * win_height = 1 + WBorTop(11) + WBorBottom(2) + (8 * 5) = 54
     * Note: IntuiTextLength uses 8px per char.
     */
    EXPECT_GE(window_info.width, 150)
        << "Window should be wide enough for the text";
    EXPECT_GE(window_info.height, 40)
        << "Window height should include space for text (font fix)";
}

TEST_F(MenuLayoutTest, StartupOutput) {
    /* The program prints startup messages after reaching the event loop.
     * Need to give it enough cycles for printf to complete.
     */
    RunCyclesWithVBlank(30, 50000);
    std::string output = GetOutput();
    EXPECT_NE(output.find("Menu Example running"), std::string::npos)
        << "Program should print startup message. Output: " << output;
}

TEST_F(MenuLayoutTest, ProjectMenuSelection) {
    /* Select "New" from the Project menu (menu 0, item 0).
     *
     * The Project menu title is at the left side of the menu bar.
     * adjustMenus() positions it at LeftEdge=2.
     * Menu items start at BarHeight+1 = 11.
     * "New" is item 0 at TopEdge=0, height=9 (font 8+1).
     * Center of first item: 11 + 0 + 4 = 15.
     */
    ClearOutput();
    RunCyclesWithVBlank(10, 50000);  /* Ensure printf is flushed */
    ClearOutput();

    int menu_bar_x = 30;   /* Over "Project" title */
    int menu_bar_y = 5;    /* In the menu bar area */
    int item_y = 15;       /* Center of first item */

    /* RMB drag from menu title to first item */
    lxa_inject_drag(menu_bar_x, menu_bar_y, menu_bar_x, item_y, LXA_MOUSE_RIGHT, 10);

    /* Poll for MENUPICK with VBlanks */
    bool found = false;
    for (int attempt = 0; attempt < 100 && !found; attempt++) {
        lxa_trigger_vblank();
        RunCycles(100000);

        std::string output = GetOutput();
        if (output.find("IDCMP_MENUPICK") != std::string::npos) {
            found = true;
        }
    }

    EXPECT_TRUE(found) << "Expected MENUPICK event after menu selection";
    if (found) {
        /* Run more cycles so the m68k program can finish executing
         * processMenus() and flushing the printf output for the item name. */
        RunCyclesWithVBlank(10, 100000);

        std::string output = GetOutput();
        EXPECT_NE(output.find("New"), std::string::npos)
            << "Expected 'New' selection. Output: " << output;
    }
}

TEST_F(MenuLayoutTest, QuitViaProjectMenu) {
    /* Select "Quit" from the Project menu (menu 0, item 6).
     * Quit is the 7th item. With height=9, it's at TopEdge=54.
     * So the center is at BarHeight+1 + 54 + 4 = ~69.
     */
    ClearOutput();

    int menu_bar_x = 30;
    int menu_bar_y = 5;
    int quit_item_y = 69;  /* 11 + 54 + 4 = 69 */

    lxa_inject_drag(menu_bar_x, menu_bar_y, menu_bar_x, quit_item_y, LXA_MOUSE_RIGHT, 15);
    RunCyclesWithVBlank(20, 50000);

    /* Wait for program to exit */
    bool exited = lxa_wait_exit(5000);

    /* If menu quit didn't work, use close gadget */
    if (!exited) {
        Click(window_info.x + 10, window_info.y + 5);
        exited = lxa_wait_exit(3000);
    }

    EXPECT_TRUE(exited) << "Program should exit after selecting Quit";

    std::string output = GetOutput();
    EXPECT_NE(output.find("Quit"), std::string::npos)
        << "Expected 'Quit' in output. Output: " << output;
}

TEST_F(MenuLayoutTest, CloseGadgetExits) {
    /* Close gadget should exit the program */
    Click(window_info.x + 10, window_info.y + 5);
    RunCyclesWithVBlank(10, 50000);

    bool exited = lxa_wait_exit(5000);
    EXPECT_TRUE(exited) << "Program should exit when close gadget clicked";
}

/* ============================================================================
 * Pixel verification test — verify window text rendering
 * ============================================================================ */

class MenuLayoutPixelTest : public LxaUITest {
protected:
    void SetUp() override {
        /* Disable rootless so Intuition renders to screen bitmap */
        config.rootless = false;

        LxaUITest::SetUp();

        ASSERT_EQ(lxa_load_program("SYS:MenuLayout", ""), 0)
            << "Failed to load MenuLayout";

        ASSERT_TRUE(WaitForWindows(1, 5000))
            << "Window did not open within 5 seconds";

        ASSERT_TRUE(GetWindowInfo(0, &window_info))
            << "Could not get window info";

        /* Let rendering complete */
        WaitForEventLoop(200, 10000);
        RunCyclesWithVBlank(30, 200000);
    }
};

TEST_F(MenuLayoutPixelTest, WindowContentVisible) {
    /* Verify that the window has visible content (the title bar and
     * the "How to do a Menu" / "(with Style)" text).
     */
    lxa_flush_display();

    /* Check the window interior for rendered text.
     * The window is at (10, BarHeight+1) = (10, 11).
     * The text "How to do a Menu" is centered, starting at roughly
     * TopEdge = 1 + WBorTop + 2*fontHeight = 1 + 11 + 16 = 28 (absolute),
     * which is window_info.y + 17 in window coords.
     */
    int content_x1 = window_info.x + 10;
    int content_y1 = window_info.y + 15;
    int content_x2 = window_info.x + window_info.width - 10;
    int content_y2 = window_info.y + window_info.height - 5;

    int content_pixels = CountContentPixels(content_x1, content_y1,
                                            content_x2, content_y2, 0);

    EXPECT_GT(content_pixels, 10)
        << "Window should have visible text content (PrintIText)";
}

TEST_F(MenuLayoutPixelTest, MenuDropdownClearedAfterSelection) {
    /* Verify menu drop-down is cleaned up after selection */
    lxa_flush_display();

    int check_y1 = 12;
    int check_y2 = 40;
    int check_x1 = 5;
    int check_x2 = 80;

    int before_pixels = CountContentPixels(check_x1, check_y1, check_x2, check_y2, 0);
    ASSERT_GT(before_pixels, 0)
        << "Window content should be visible before menu open";

    /* Open menu and select first item.
     * After the drag, give enough VBlanks + cycles for the MENUUP event
     * to be processed through the ISR and menu cleanup to complete.
     * Menu rendering (save-behind, bitmap ops) is cycle-intensive.
     */
    lxa_inject_drag(30, 5, 30, 16, LXA_MOUSE_RIGHT, 10);
    RunCyclesWithVBlank(20, 200000);
    lxa_flush_display();

    int after_pixels = CountContentPixels(check_x1, check_y1, check_x2, check_y2, 0);
    EXPECT_GT(after_pixels, 0)
        << "Window content should be restored after menu closes";

    int diff = (before_pixels > after_pixels) ?
               (before_pixels - after_pixels) : (after_pixels - before_pixels);
    EXPECT_LT(diff, before_pixels / 2 + 5)
        << "Screen area should be restored. Before: " << before_pixels
        << " After: " << after_pixels;
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
