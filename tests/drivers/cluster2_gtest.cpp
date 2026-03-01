/**
 * cluster2_gtest.cpp - Google Test version of Cluster2 IDE test
 *
 * Tests Cluster2 Oberon-2 IDE.
 * Verifies screen dimensions, editor initialization, text entry,
 * mouse input, cursor key handling, and menu access.
 */

#include "lxa_test.h"

using namespace lxa::testing;

class Cluster2Test : public LxaUITest {
protected:
    void SetUp() override {
        LxaUITest::SetUp();
        
        const char* apps = FindAppsPath();
        if (!apps) {
            GTEST_SKIP() << "lxa-apps directory not found";
        }
        
        /* Set up Cluster assign (host path needed for assign) */
        std::string cluster_base = std::string(apps) + "/Cluster2/bin/Cluster2";
        lxa_add_assign("Cluster", cluster_base.c_str());
        
        /* Load via APPS: assign (mapped to lxa-apps directory in LxaTest::SetUp) */
        ASSERT_EQ(lxa_load_program("APPS:Cluster2/bin/Cluster2/Editor", ""), 0) 
            << "Failed to load Cluster2 via APPS: assign";
        
        ASSERT_TRUE(WaitForWindows(1, 20000)) 
            << "Cluster2 window did not open";
        
        ASSERT_TRUE(GetWindowInfo(0, &window_info));
        RunCyclesWithVBlank(200, 50000);
    }
};

TEST_F(Cluster2Test, WindowOpens) {
    EXPECT_GT(window_info.width, 0);
    EXPECT_GT(window_info.height, 0);
}

TEST_F(Cluster2Test, ScreenDimensions) {
    int width, height, depth;
    if (lxa_get_screen_dimensions(&width, &height, &depth)) {
        EXPECT_GE(width, 640) << "Screen width should be >= 640";
        EXPECT_GE(height, 200) << "Screen height should be >= 200";
    }
}

TEST_F(Cluster2Test, EditorReady) {
    /* Verify Cluster2 is running and has windows */
    EXPECT_TRUE(lxa_is_running()) << "Cluster2 should still be running";
    EXPECT_GE(lxa_get_window_count(), 1) << "At least one window should be open";
}

TEST_F(Cluster2Test, RespondsToInput) {
    /* Type a simple Oberon-2 module */
    TypeString("MODULE Test;\nBEGIN\nEND Test.\n");
    RunCyclesWithVBlank(40, 50000);

    EXPECT_TRUE(lxa_is_running()) << "Cluster2 should still be running after typing";
    EXPECT_GE(lxa_get_window_count(), 1) << "Window should still be open after typing";
}

TEST_F(Cluster2Test, MouseInput) {
    /* Click in the editor area */
    int click_x = window_info.x + window_info.width / 2;
    int click_y = window_info.y + window_info.height / 2;
    
    Click(click_x, click_y);
    RunCyclesWithVBlank(20, 50000);
    
    EXPECT_TRUE(lxa_is_running()) << "Cluster2 should still be running after mouse click";
}

TEST_F(Cluster2Test, CursorKeys) {
    /* Drain pending events */
    RunCyclesWithVBlank(40, 50000);
    
    /* Press cursor keys (Up, Down, Right, Left) */
    PressKey(0x4C, 0);  /* Up */
    RunCyclesWithVBlank(10, 50000);
    
    PressKey(0x4D, 0);  /* Down */
    RunCyclesWithVBlank(10, 50000);
    
    PressKey(0x4E, 0);  /* Right */
    RunCyclesWithVBlank(10, 50000);
    
    PressKey(0x4F, 0);  /* Left */
    RunCyclesWithVBlank(10, 50000);
    
    EXPECT_TRUE(lxa_is_running()) << "Cluster2 should still be running after cursor keys";
    EXPECT_GE(lxa_get_window_count(), 1) << "Window should still be open after cursor keys";
}

TEST_F(Cluster2Test, RMBMenuAccess) {
    /* RMB click to access menu */
    int click_x = window_info.x + 50;   /* Near menu bar */
    int click_y = window_info.y + 10;   /* In title bar area */
    
    lxa_inject_rmb_click(click_x, click_y);
    RunCyclesWithVBlank(20, 50000);
    
    EXPECT_TRUE(lxa_is_running()) << "Cluster2 should still be running after RMB";
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
