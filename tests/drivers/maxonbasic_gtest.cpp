/**
 * maxonbasic_gtest.cpp - Google Test version of MaxonBASIC test
 *
 * Tests MaxonBASIC IDE.
 * Verifies screen dimensions, editor initialization, BASIC code entry,
 * mouse input, and cursor key handling.
 */

#include "lxa_test.h"

using namespace lxa::testing;

class MaxonBasicTest : public LxaUITest {
protected:
    void SlowTypeString(const char* str, int settle_vblanks = 3) {
        char ch[2] = {0, 0};

        while (*str) {
            ch[0] = *str++;
            TypeString(ch);
            RunCyclesWithVBlank(settle_vblanks, 50000);
        }
    }

    void SetUp() override {
        LxaUITest::SetUp();
        
        const char* apps = FindAppsPath();
        if (!apps) {
            GTEST_SKIP() << "lxa-apps directory not found";
        }
        
        /* Load via APPS: assign (mapped to lxa-apps directory in LxaTest::SetUp) */
        ASSERT_EQ(lxa_load_program("APPS:MaxonBASIC/bin/MaxonBASIC/MaxonBASIC", ""), 0) 
            << "Failed to load MaxonBASIC via APPS: assign";
        
        ASSERT_TRUE(WaitForWindows(1, 10000)) 
            << "MaxonBASIC window did not open";
        
        ASSERT_TRUE(GetWindowInfo(0, &window_info));
        ASSERT_TRUE(WaitForWindowDrawn(0, 5000))
            << "MaxonBASIC window did not draw";
        WaitForEventLoop(100, 10000);
        RunCyclesWithVBlank(100, 50000);
    }
};

TEST_F(MaxonBasicTest, WindowOpens) {
    EXPECT_GT(window_info.width, 0);
    EXPECT_GT(window_info.height, 0);
}

TEST_F(MaxonBasicTest, ScreenDimensions) {
    int width, height, depth;
    if (lxa_get_screen_dimensions(&width, &height, &depth)) {
        EXPECT_GE(width, 640) << "Screen width should be >= 640";
        EXPECT_GE(height, 200) << "Screen height should be >= 200";
    }
}

TEST_F(MaxonBasicTest, EditorVisible) {
    /* Verify MaxonBASIC is running and has windows */
    EXPECT_TRUE(lxa_is_running()) << "MaxonBASIC should still be running";
    EXPECT_GE(lxa_get_window_count(), 1) << "At least one window should be open";
}

TEST_F(MaxonBasicTest, TextEntry) {
    /* Focus the editor and type a simple BASIC program gradually. */
    Click(window_info.x + window_info.width / 2, window_info.y + window_info.height / 2);
    RunCyclesWithVBlank(20, 50000);

    SlowTypeString("REM Test program\nPRINT \"Hello\"\nEND\n", 4);
    RunCyclesWithVBlank(40, 50000);

    EXPECT_TRUE(lxa_is_running()) << "MaxonBASIC should still be running after typing";
    EXPECT_GE(lxa_get_window_count(), 1) << "Window should still be open after typing";
}

TEST_F(MaxonBasicTest, MouseInput) {
    /* Click in the editor area */
    int click_x = window_info.x + window_info.width / 2;
    int click_y = window_info.y + window_info.height / 2;
    
    Click(click_x, click_y);
    RunCyclesWithVBlank(20, 50000);
    
    EXPECT_TRUE(lxa_is_running()) << "MaxonBASIC should still be running after mouse click";
}

TEST_F(MaxonBasicTest, CursorKeys) {
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
    
    EXPECT_TRUE(lxa_is_running()) << "MaxonBASIC should still be running after cursor keys";
    EXPECT_GE(lxa_get_window_count(), 1) << "Window should still be open after cursor keys";
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
