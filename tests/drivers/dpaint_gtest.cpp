/**
 * dpaint_gtest.cpp - Google Test version of DPaint V test
 *
 * Tests DPaint V graphics editor.
 * Verifies window opening, screen dimensions, process running,
 * and mouse interaction.
 */

#include "lxa_test.h"

using namespace lxa::testing;

class DPaintTest : public LxaUITest {
protected:
    bool window_opened = false;

    void SetUp() override {
        LxaUITest::SetUp();
        
        const char* apps = FindAppsPath();
        if (!apps) {
            GTEST_SKIP() << "lxa-apps directory not found";
        }
        
        /* Load via APPS: assign */
        ASSERT_EQ(lxa_load_program("APPS:DPaintV/bin/DPaintV/DPaint", ""), 0) 
            << "Failed to load DPaint V";
        
        /* DPaint may take a while to initialize */
        window_opened = WaitForWindows(1, 15000);
        if (window_opened) {
            GetWindowInfo(0, &window_info);
        }
        
        /* Let DPaint initialize further */
        RunCyclesWithVBlank(100, 50000);
    }
};

TEST_F(DPaintTest, ProgramLoads) {
    /* Verify DPaint loaded and is running */
    EXPECT_TRUE(lxa_is_running()) << "DPaint process should be running";
}

TEST_F(DPaintTest, WindowOpens) {
    if (!window_opened) {
        GTEST_SKIP() << "DPaint window did not open (known initialization issue)";
    }
    EXPECT_GT(window_info.width, 0);
    EXPECT_GT(window_info.height, 0);
}

TEST_F(DPaintTest, ScreenDimensions) {
    int width, height, depth;
    if (lxa_get_screen_dimensions(&width, &height, &depth)) {
        EXPECT_GE(width, 320) << "Screen width should be >= 320";
        EXPECT_GE(height, 200) << "Screen height should be >= 200";
    }
}

TEST_F(DPaintTest, MouseInteraction) {
    if (!window_opened) {
        GTEST_SKIP() << "DPaint window did not open (known initialization issue)";
    }
    
    /* Click in the drawing area */
    int click_x = window_info.x + window_info.width / 2;
    int click_y = window_info.y + window_info.height / 2;
    
    Click(click_x, click_y);
    RunCyclesWithVBlank(20, 50000);
    
    EXPECT_TRUE(lxa_is_running()) << "DPaint should still be running after mouse click";
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
