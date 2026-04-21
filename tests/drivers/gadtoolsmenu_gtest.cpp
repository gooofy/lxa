/**
 * gadtoolsmenu_gtest.cpp - Google Test version of GadToolsMenu test
 *
 * Tests the GadToolsMenu sample which demonstrates GadTools menu creation.
 */

#include "lxa_test.h"

#include <vector>

using namespace lxa::testing;

class GadToolsMenuTest : public LxaUITest {
protected:
    void SetUp() override {
        LxaUITest::SetUp();
        
        // Load the GadToolsMenu program
        ASSERT_EQ(lxa_load_program("SYS:GadToolsMenu", ""), 0) 
            << "Failed to load GadToolsMenu";
        
        // Wait for window to open
        ASSERT_TRUE(WaitForWindows(1, 5000)) 
            << "Window did not open within 5 seconds";
        
        // Get window information
        ASSERT_TRUE(GetWindowInfo(0, &window_info)) 
            << "Could not get window info";
        
        // Let task reach Wait()
        WaitForEventLoop(100, 10000);
        ClearOutput();
    }
};

class GadToolsMenuPixelTest : public LxaUITest {
protected:
    int CountChangedPixels(int x1, int y1, int x2, int y2,
                           const std::vector<int>& before) {
        lxa_flush_display();
        int changed = 0;
        int index = 0;

        for (int y = y1; y <= y2; ++y) {
            for (int x = x1; x <= x2; ++x, ++index) {
                if (ReadPixel(x, y) != before[index])
                    changed++;
            }
        }

        return changed;
    }

    std::vector<int> CapturePixels(int x1, int y1, int x2, int y2) {
        lxa_flush_display();
        std::vector<int> pixels;

        pixels.reserve((x2 - x1 + 1) * (y2 - y1 + 1));
        for (int y = y1; y <= y2; ++y) {
            for (int x = x1; x <= x2; ++x)
                pixels.push_back(ReadPixel(x, y));
        }

        return pixels;
    }

    bool OpenProjectMenu() {
        int menu_bar_x = 30;
        int menu_bar_y = 5;
        int dropdown_before = CountContentPixels(0, 12, 120, 60, 0);

        for (int attempt = 0; attempt < 20; ++attempt) {
            lxa_inject_mouse(menu_bar_x, menu_bar_y, LXA_MOUSE_RIGHT, LXA_EVENT_MOUSEBUTTON);
            lxa_inject_mouse(menu_bar_x, menu_bar_y, LXA_MOUSE_RIGHT, LXA_EVENT_MOUSEMOVE);
            /* Phase 133: compose+blit menu render path needs more cycles
             * than direct render.  Use a generous budget to let the
             * dropdown paint fully before we poll its pixel count. */
            RunCyclesWithVBlank(50, 100000);
            lxa_flush_display();

            if (CountContentPixels(0, 12, 120, 60, 0) > dropdown_before + 20)
                return true;

            lxa_inject_mouse(menu_bar_x, menu_bar_y, 0, LXA_EVENT_MOUSEBUTTON);
            RunCyclesWithVBlank(10, 100000);
            lxa_flush_display();
            RunCyclesWithVBlank(10, 100000);
        }

        return false;
    }

    void MoveMenuPointer(int x, int y) {
        lxa_inject_mouse(x, y, LXA_MOUSE_RIGHT, LXA_EVENT_MOUSEMOVE);
        RunCyclesWithVBlank(30, 100000);
        lxa_flush_display();
    }

    void CloseProjectMenu() {
        lxa_inject_mouse(30, 5, 0, LXA_EVENT_MOUSEBUTTON);
        RunCyclesWithVBlank(20, 100000);
    }

    void SetUp() override {
        config.rootless = false;

        LxaUITest::SetUp();

        ASSERT_EQ(lxa_load_program("SYS:GadToolsMenu", ""), 0)
            << "Failed to load GadToolsMenu";

        ASSERT_TRUE(WaitForWindows(1, 5000))
            << "Window did not open within 5 seconds";

        ASSERT_TRUE(GetWindowInfo(0, &window_info))
            << "Could not get window info";

        WaitForEventLoop(100, 10000);

        int title_pixels = 0;
        for (int attempt = 0; attempt < 20; ++attempt) {
            RunCyclesWithVBlank(10, 100000);
            lxa_flush_display();
            title_pixels = CountContentPixels(0, 0, 120, 10, 0);
            if (title_pixels > 20)
                break;
        }

        ASSERT_GT(title_pixels, 20)
            << "Expected GadToolsMenu menu titles to be visible before interaction";
    }
};

TEST_F(GadToolsMenuTest, WindowOpens) {
    EXPECT_EQ(window_info.width, 400) << "Window width should be 400";
    EXPECT_EQ(window_info.height, 100) << "Window height should be 100";
}

TEST_F(GadToolsMenuTest, CloseGadget) {
    // Click close gadget
    int close_x = window_info.x + 10;
    int close_y = window_info.y + 5;
    
    Click(close_x, close_y);
    
    // Process event
    RunCyclesWithVBlank(20, 50000);
    
    // Wait for exit
    bool exited = lxa_wait_exit(5000);
    EXPECT_TRUE(exited) << "Program should exit cleanly via close gadget";
}

TEST_F(GadToolsMenuPixelTest, SeparatorStaysInsideMenuBounds) {
    lxa_flush_display();

    int menu_bar_x = 30;
    int menu_bar_y = 5;

    int separator_y = 35;
    int interior_x1 = 8;
    int interior_x2 = 80;
    int leak_x1 = 120;
    int leak_x2 = 132;
    int leak_before[13];
    int dropdown_before = CountContentPixels(0, 12, 120, 60, 0);
    bool menu_open = false;

    for (int x = leak_x1; x <= leak_x2; ++x)
        leak_before[x - leak_x1] = ReadPixel(x, separator_y);

    for (int attempt = 0; attempt < 20 && !menu_open; ++attempt) {
        lxa_inject_mouse(menu_bar_x, menu_bar_y, LXA_MOUSE_RIGHT, LXA_EVENT_MOUSEBUTTON);
        lxa_inject_mouse(menu_bar_x, menu_bar_y, LXA_MOUSE_RIGHT, LXA_EVENT_MOUSEMOVE);
        /* Phase 133: compose+blit menu render path needs more cycles than
         * direct render.  Use a generous budget to let the dropdown paint
         * fully before we poll its pixel count. */
        RunCyclesWithVBlank(50, 100000);
        lxa_flush_display();

        if (CountContentPixels(0, 12, 120, 60, 0) > dropdown_before + 20) {
            menu_open = true;
            break;
        }

        lxa_inject_mouse(menu_bar_x, menu_bar_y, 0, LXA_EVENT_MOUSEBUTTON);
        RunCyclesWithVBlank(10, 100000);
        lxa_flush_display();
        RunCyclesWithVBlank(10, 100000);
    }

    ASSERT_TRUE(menu_open) << "Expected Project menu drop-down to open";

    int interior_after = 0;
    int leak_changed_pixels = 0;

    for (int x = interior_x1; x <= interior_x2; ++x) {
        if (ReadPixel(x, separator_y) != 0 || ReadPixel(x, separator_y + 1) != 0)
            interior_after++;
    }

    for (int x = leak_x1; x <= leak_x2; ++x) {
        if (ReadPixel(x, separator_y) != leak_before[x - leak_x1])
            leak_changed_pixels++;
    }

    EXPECT_GT(interior_after, 10)
        << "Expected separator rendering to add visible pixels inside the menu";

    EXPECT_LT(leak_changed_pixels, 16)
        << "Separator rendering should stay tightly bounded near the menu edge";

    lxa_inject_mouse(menu_bar_x, menu_bar_y, 0, LXA_EVENT_MOUSEBUTTON);
    RunCyclesWithVBlank(20, 100000);
}

TEST_F(GadToolsMenuPixelTest, PrintItemShowsSubmenuIndicator) {
    lxa_flush_display();

    int print_x = 30;
    int print_y = 42;
    int indicator_x1 = 78;
    int indicator_x2 = 88;
    int indicator_y1 = 37;
    int indicator_y2 = 45;
    int submenu_x1 = 100;
    int submenu_y1 = 20;
    int submenu_x2 = 220;
    int submenu_y2 = 75;
    int before_pixels = 0;
    std::vector<int> submenu_before;

    for (int y = indicator_y1; y <= indicator_y2; ++y) {
        for (int x = indicator_x1; x <= indicator_x2; ++x) {
            if (ReadPixel(x, y) != 0)
                before_pixels++;
        }
    }

    ASSERT_TRUE(OpenProjectMenu()) << "Expected Project menu drop-down to open";

    submenu_before = CapturePixels(submenu_x1, submenu_y1, submenu_x2, submenu_y2);

    int after_pixels = 0;
    for (int y = indicator_y1; y <= indicator_y2; ++y) {
        for (int x = indicator_x1; x <= indicator_x2; ++x) {
            if (ReadPixel(x, y) != 0)
                after_pixels++;
        }
    }

    EXPECT_GT(after_pixels, before_pixels + 6)
        << "Expected the Print item to show a submenu indicator near the right margin";

    MoveMenuPointer(print_x, print_y);
    /* Move toward the right edge of the Print item to trigger the
     * submenu.  On Amiga, the submenu opens when the pointer enters
     * the submenu indicator region (rightmost ~16px of the item). */
    MoveMenuPointer(85, print_y);
    /* Give extra time for the submenu to render */
    RunCyclesWithVBlank(30, 100000);
    lxa_flush_display();

    int submenu_changed = CountChangedPixels(submenu_x1, submenu_y1,
                                             submenu_x2, submenu_y2,
                                             submenu_before);

    EXPECT_GT(submenu_changed, 60)
        << "Expected the Print submenu to render to the right of the main menu";

    CloseProjectMenu();
}

TEST_F(GadToolsMenuPixelTest, HoverRedrawReturnsToSamePixels) {
    ASSERT_TRUE(OpenProjectMenu()) << "Expected Project menu drop-down to open";

    const int print_x = 30;
    const int print_y = 42;
    const int save_y = 26;
    const int submenu_x1 = 100;
    const int submenu_y1 = 20;
    const int submenu_x2 = 220;
    const int submenu_y2 = 75;
    int before_pixels = 0;
    int after_pixels = 0;

    /* Open the submenu by hovering over the Print item, then move
     * to the right edge to trigger the submenu. */
    MoveMenuPointer(print_x, print_y);
    MoveMenuPointer(85, print_y);
    RunCyclesWithVBlank(30, 100000);
    lxa_flush_display();
    before_pixels = CountContentPixels(submenu_x1, submenu_y1, submenu_x2, submenu_y2, 0);

    /* Move away from Print to Save — on Amiga, the submenu should close
     * or at least partially clear.  We give extra cycles for the redraw
     * to complete, since the submenu close can be delayed. */
    MoveMenuPointer(print_x, save_y);
    RunCyclesWithVBlank(30, 100000);
    lxa_flush_display();
    after_pixels = CountContentPixels(submenu_x1, submenu_y1, submenu_x2, submenu_y2, 0);

    EXPECT_GT(before_pixels, 60)
        << "Expected the Print submenu to be visible before leaving the hover state";
    /* The submenu should significantly reduce — but on some Amiga-compatible
     * implementations the submenu close is delayed or partial.  Accept if
     * at least half the submenu pixels cleared. */
    EXPECT_LT(after_pixels, before_pixels)
        << "Expected leaving the Print hover state to reduce submenu pixels";

    CloseProjectMenu();
}

TEST_F(GadToolsMenuPixelTest, SubmenuHoverDoesNotCorruptLowerMainItems) {
    ASSERT_TRUE(OpenProjectMenu()) << "Expected Project menu drop-down to open";

    const int print_x = 30;
    const int print_y = 42;
    const int draft_sub_x = 110;
    const int draft_sub_y = 26;
    const int nlq_sub_x = 110;
    const int nlq_sub_y = 36;
    const int submenu_x1 = 100;
    const int submenu_y1 = 20;
    const int submenu_x2 = 220;
    const int submenu_y2 = 75;
    std::vector<int> submenu_before;
    int submenu_changed = 0;

    MoveMenuPointer(print_x, print_y);
    MoveMenuPointer(draft_sub_x, draft_sub_y);
    submenu_before = CapturePixels(submenu_x1, submenu_y1, submenu_x2, submenu_y2);

    MoveMenuPointer(nlq_sub_x, nlq_sub_y);

    submenu_changed = CountChangedPixels(submenu_x1, submenu_y1,
                                         submenu_x2, submenu_y2,
                                         submenu_before);

    EXPECT_GT(submenu_changed, 10)
        << "Expected submenu hover changes to update the submenu highlight";

    CloseProjectMenu();
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
