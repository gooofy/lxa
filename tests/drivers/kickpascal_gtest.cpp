/**
 * kickpascal_gtest.cpp - Google Test version of KickPascal 2 test
 *
 * Tests KickPascal IDE - editor with text entry and cursor keys.
 * Verifies screen dimensions, editor initialization, Pascal code entry,
 * mouse input, and cursor key handling.
 */

#include "lxa_test.h"

using namespace lxa::testing;

class KickPascalTest : public LxaUITest {
protected:
    void SetUp() override {
        LxaUITest::SetUp();
        
        const char* apps = FindAppsPath();
        if (!apps) {
            GTEST_SKIP() << "lxa-apps directory not found";
        }
        
        /* Load via APPS: assign (mapped to lxa-apps directory in LxaTest::SetUp) */
        ASSERT_EQ(lxa_load_program("APPS:kickpascal2/bin/KP2/KP", ""), 0) 
            << "Failed to load KickPascal via APPS: assign";
        
        ASSERT_TRUE(WaitForWindows(1, 10000)) 
            << "KickPascal window did not open";
        
        ASSERT_TRUE(GetWindowInfo(0, &window_info));
        
        /* Let KickPascal initialize */
        RunCyclesWithVBlank(100, 50000);
    }
};

TEST_F(KickPascalTest, WindowOpens) {
    EXPECT_GT(window_info.width, 0);
    EXPECT_GT(window_info.height, 0);
}

TEST_F(KickPascalTest, ScreenDimensions) {
    int width, height, depth;
    if (lxa_get_screen_dimensions(&width, &height, &depth)) {
        EXPECT_GE(width, 640) << "Screen width should be >= 640";
        EXPECT_GE(height, 200) << "Screen height should be >= 200";
    }
}

TEST_F(KickPascalTest, EditorVisible) {
    int content_pixels = CountContentPixels(0, 0, 100, 100, 0);
    EXPECT_GT(content_pixels, 0) << "Editor should be visible";
}

TEST_F(KickPascalTest, TextEntry) {
    /* Type a simple Pascal program */
    TypeString("program Test;\nbegin\n  writeln('Hello');\nend.\n");
    RunCyclesWithVBlank(40, 50000);

    EXPECT_TRUE(lxa_is_running()) << "KickPascal should still be running after typing";
    EXPECT_GE(lxa_get_window_count(), 1) << "Window should still be open after typing";
}

TEST_F(KickPascalTest, MouseInput) {
    /* Click in the editor area */
    int click_x = window_info.x + window_info.width / 2;
    int click_y = window_info.y + window_info.height / 2;
    
    Click(click_x, click_y);
    RunCyclesWithVBlank(20, 50000);
    
    EXPECT_TRUE(lxa_is_running()) << "KickPascal should still be running after mouse click";
}

TEST_F(KickPascalTest, CursorKeys) {
    /* Drain pending events */
    RunCyclesWithVBlank(40, 50000);
    
    /* Test cursor key handling (RAWKEY 0x4C-0x4F) */
    PressKey(0x4C, 0);  /* Up */
    RunCyclesWithVBlank(10, 50000);
    
    PressKey(0x4D, 0);  /* Down */
    RunCyclesWithVBlank(10, 50000);
    
    PressKey(0x4E, 0);  /* Right */
    RunCyclesWithVBlank(10, 50000);
    
    PressKey(0x4F, 0);  /* Left */
    RunCyclesWithVBlank(10, 50000);
    
    EXPECT_TRUE(lxa_is_running()) << "KickPascal should still be running after cursor keys";
    EXPECT_GE(lxa_get_window_count(), 1) << "Window should still be open after cursor keys";
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
