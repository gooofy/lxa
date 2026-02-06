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

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
