/**
 * devpac_gtest.cpp - Google Test version of Devpac (HiSoft) test
 *
 * Tests Devpac assembler - opens and displays editor.
 * Verifies screen dimensions, editor initialization, text entry,
 * mouse input, and cursor key handling.
 */

#include "lxa_test.h"

#include <fstream>

using namespace lxa::testing;

class DevpacTest : public LxaUITest {
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
        
        /* Load the Devpac IDE (not GenAm which is the CLI assembler) */
        ASSERT_EQ(lxa_load_program("APPS:DevPac/bin/Devpac/Devpac", ""), 0) 
            << "Failed to load Devpac via APPS: assign";
        
        ASSERT_TRUE(WaitForWindows(1, 10000)) 
            << "Devpac window did not open";
        
        ASSERT_TRUE(GetWindowInfo(0, &window_info));
        ASSERT_TRUE(WaitForWindowDrawn(0, 5000));
        WaitForEventLoop(100, 10000);
        
        /* Let Devpac initialize */
        RunCyclesWithVBlank(100, 50000);
    }
};

TEST_F(DevpacTest, WindowOpens) {
    EXPECT_GT(window_info.width, 0);
    EXPECT_GT(window_info.height, 0);
}

TEST_F(DevpacTest, ScreenDimensions) {
    int width, height, depth;
    if (lxa_get_screen_dimensions(&width, &height, &depth)) {
        EXPECT_GE(width, 640) << "Screen width should be >= 640";
        EXPECT_GE(height, 200) << "Screen height should be >= 200";
    }
}

TEST_F(DevpacTest, EditorVisible) {
    /* Verify Devpac is running and has windows */
    EXPECT_TRUE(lxa_is_running()) << "Devpac should still be running";
    EXPECT_GE(lxa_get_window_count(), 1) << "At least one window should be open";
}

TEST_F(DevpacTest, RootlessHostWindowIncludesScreenStripAboveWindow) {
    const std::string capture_path = ram_dir_path + "/devpac-window.ppm";
    std::ifstream capture;
    std::string magic;
    int captured_width = 0;
    int captured_height = 0;
    int max_value = 0;

    ASSERT_GT(window_info.y, 0) << "Devpac should open below the screen origin in rootless mode";
    ASSERT_TRUE(CaptureWindow(capture_path.c_str(), 0))
        << "Rootless Devpac window capture should succeed";

    capture.open(capture_path, std::ios::binary);
    ASSERT_TRUE(capture.good()) << "Failed to open captured Devpac window image";

    capture >> magic >> captured_width >> captured_height >> max_value;

    ASSERT_EQ(magic, "P6") << "Captured Devpac window should use the PPM format";
    EXPECT_EQ(captured_width, window_info.width)
        << "Devpac should preserve its logical width in the host window capture";
    EXPECT_GT(captured_height, window_info.height)
        << "Rootless Devpac capture should include the screen strip above the logical window";
}

TEST_F(DevpacTest, RespondsToInput) {
    /* Focus the editor and type a simple assembly program gradually. */
    Click(window_info.x + window_info.width / 2, window_info.y + window_info.height / 2);
    RunCyclesWithVBlank(20, 50000);

    SlowTypeString("; Devpac test\n\tmove.l\t#0,d0\n\trts\n", 4);
    RunCyclesWithVBlank(40, 50000);

    EXPECT_TRUE(lxa_is_running()) << "Devpac should still be running after typing";
    EXPECT_GE(lxa_get_window_count(), 1) << "Window should still be open after typing";
}

TEST_F(DevpacTest, MouseInput) {
    /* Click in the editor area */
    int click_x = window_info.x + window_info.width / 2;
    int click_y = window_info.y + window_info.height / 2;
    
    Click(click_x, click_y);
    RunCyclesWithVBlank(20, 50000);
    
    EXPECT_TRUE(lxa_is_running()) << "Devpac should still be running after mouse click";
}

TEST_F(DevpacTest, CursorKeys) {
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
    
    EXPECT_TRUE(lxa_is_running()) << "Devpac should still be running after cursor keys";
    EXPECT_GE(lxa_get_window_count(), 1) << "Window should still be open after cursor keys";
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
