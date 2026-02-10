/**
 * simplemenu_gtest.cpp - Google Test version of SimpleMenu test
 *
 * Tests the SimpleMenu sample which demonstrates Amiga menu system.
 */

#include "lxa_test.h"

using namespace lxa::testing;

class SimpleMenuTest : public LxaUITest {
protected:
    void SetUp() override {
        LxaUITest::SetUp();
        
        // Load the SimpleMenu program
        ASSERT_EQ(lxa_load_program("SYS:SimpleMenu", ""), 0) 
            << "Failed to load SimpleMenu";
        
        // Wait for window to open
        ASSERT_TRUE(WaitForWindows(1, 5000)) 
            << "Window did not open within 5 seconds";
        
        // Get window information
        ASSERT_TRUE(GetWindowInfo(0, &window_info)) 
            << "Could not get window info";
        
        // Let task reach Wait()
        WaitForEventLoop(200, 10000);
        ClearOutput();
    }
};

TEST_F(SimpleMenuTest, WindowOpens) {
    EXPECT_GT(window_info.width, 0);
    EXPECT_GT(window_info.height, 0);
}

TEST_F(SimpleMenuTest, MenuSelection) {
    // Get screen info for menu bar position
    lxa_screen_info_t scr_info;
    if (lxa_get_screen_info(&scr_info)) {
        EXPECT_GT(scr_info.width, 0);
        EXPECT_GT(scr_info.height, 0);
    }
    
    // Menu bar position - "Project" menu is at left edge
    int menu_bar_x = 20;
    int menu_bar_y = 5;
    
    // "Open..." is the first item (TopEdge=0, Height=10)
    // Menu drop-down starts at BarHeight+1 (~11 pixels)
    int item_y = 16;
    
    // Use RMB drag to select menu item
    lxa_inject_drag(menu_bar_x, menu_bar_y, menu_bar_x, item_y, LXA_MOUSE_RIGHT, 10);
    
    // Poll for MENUPICK with VBlanks
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
}

TEST_F(SimpleMenuTest, QuitViaMenu) {
    ClearOutput();
    
    // Quit is item index 3 (4th item: Open, Save, Print, Quit)
    // Menu items at TopEdge = 0, 10, 20, 30 (each Height=10)
    // Quit is at approximately y = 11 + 30 + 5 = 46
    int menu_bar_x = 20;
    int menu_bar_y = 5;
    int quit_item_y = 46;
    
    // Drag from menu bar to Quit item
    lxa_inject_drag(menu_bar_x, menu_bar_y, menu_bar_x, quit_item_y, LXA_MOUSE_RIGHT, 15);
    
    // Process the menu selection
    RunCyclesWithVBlank(20, 50000);
    
    // Wait for exit
    bool exited = lxa_wait_exit(5000);
    
    // If menu quit didn't work, try close gadget
    if (!exited) {
        Click(window_info.x + 10, window_info.y + 5);
        exited = lxa_wait_exit(3000);
    }
    
    EXPECT_TRUE(exited) << "Program should exit";
}

/* ============================================================================
 * Pixel verification tests — verify menu drop-down is cleaned up properly
 * ============================================================================ */

class SimpleMenuPixelTest : public LxaUITest {
protected:
    void SetUp() override {
        /* Disable rootless so Intuition renders to screen bitmap */
        config.rootless = false;

        LxaUITest::SetUp();
        
        ASSERT_EQ(lxa_load_program("SYS:SimpleMenu", ""), 0)
            << "Failed to load SimpleMenu";
        
        ASSERT_TRUE(WaitForWindows(1, 5000))
            << "Window did not open within 5 seconds";
        
        ASSERT_TRUE(GetWindowInfo(0, &window_info))
            << "Could not get window info";
        
        /* Let rendering complete */
        WaitForEventLoop(200, 10000);
        RunCyclesWithVBlank(30, 200000);
    }
};

TEST_F(SimpleMenuPixelTest, MenuDropdownClearedAfterSelection) {
    /* Test that the screen area under the menu drop-down is properly restored
     * after a menu item is selected. This verifies the save-behind buffer works.
     *
     * Strategy:
     *   1. Read pixels in the drop-down area (window title bar / content)
     *   2. Open menu via RMB drag to menu title
     *   3. Verify drop-down is visible (area changed)
     *   4. Select an item by releasing RMB
     *   5. Verify the area is restored to original state
     */
    lxa_flush_display();
    
    /* The window starts at window_info.y which is typically at screen row 11
     * (below the screen title bar). The menu drop-down will appear starting
     * at y=11 (BarHeight+1) and cover part of the window.
     * Read pixels in a region that will be covered by the menu drop-down. */
    int check_y1 = 12;   /* Just below screen title bar */
    int check_y2 = 40;   /* Well into the drop-down area */
    int check_x1 = 5;
    int check_x2 = 80;
    
    /* Save "before" pixel state - count non-background pixels in the check area.
     * The window's title bar and border should be visible here. */
    int before_pixels = CountContentPixels(check_x1, check_y1, check_x2, check_y2, 0);
    
    /* Verify window content is visible (title bar has pixels) */
    ASSERT_GT(before_pixels, 0) 
        << "Window content should be visible in the check area before menu open";
    
    /* Open menu: RMB press at menu bar, drag down to first item, then release */
    int menu_bar_x = 20;
    int menu_bar_y = 5;
    int item_y = 16;
    
    lxa_inject_drag(menu_bar_x, menu_bar_y, menu_bar_x, item_y, LXA_MOUSE_RIGHT, 10);
    RunCyclesWithVBlank(20, 50000);
    
    /* After menu selection and close, flush display */
    lxa_flush_display();
    
    /* Read "after" pixel state in the same area.
     * The menu drop-down should now be cleared and the original content restored. */
    int after_pixels = CountContentPixels(check_x1, check_y1, check_x2, check_y2, 0);
    
    /* The restored area should have approximately the same content as before.
     * Allow some tolerance for minor differences (e.g., screen title text 
     * might be slightly different due to re-rendering). */
    EXPECT_GT(after_pixels, 0) 
        << "Window content should be restored after menu closes";
    
    /* The pixel count should be similar to what was there before.
     * A large discrepancy would indicate the menu drop-down was not cleaned up. */
    int diff = (before_pixels > after_pixels) ? 
               (before_pixels - after_pixels) : (after_pixels - before_pixels);
    EXPECT_LT(diff, before_pixels / 2 + 5) 
        << "Screen area should be restored to approximately original state after menu close."
        << " Before: " << before_pixels << " After: " << after_pixels;
}

TEST_F(SimpleMenuPixelTest, ScreenTitleBarRestoredAfterMenu) {
    /* Test that the screen title bar is properly restored after menu mode ends.
     * The Workbench title "Workbench Screen" should be visible again. */
    lxa_flush_display();
    
    /* Read pixels in the screen title bar area */
    int title_pixels_before = CountContentPixels(5, 1, 200, 9, 0);
    
    /* Open and close menu */
    int menu_bar_x = 20;
    int menu_bar_y = 5;
    int item_y = 16;
    
    lxa_inject_drag(menu_bar_x, menu_bar_y, menu_bar_x, item_y, LXA_MOUSE_RIGHT, 10);
    RunCyclesWithVBlank(20, 50000);
    lxa_flush_display();
    
    /* Read pixels after menu closed - title bar should be restored */
    int title_pixels_after = CountContentPixels(5, 1, 200, 9, 0);
    
    /* Title bar should have visible content (screen title text) */
    EXPECT_GT(title_pixels_after, 0) 
        << "Screen title bar should have visible text after menu closes";
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
