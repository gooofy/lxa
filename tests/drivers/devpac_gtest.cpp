/**
 * devpac_gtest.cpp - Google Test version of Devpac (HiSoft) test
 *
 * Tests Devpac assembler - opens and displays editor.
 * Verifies screen dimensions, editor initialization, text entry,
 * mouse input, and cursor key handling.
 */

#include "lxa_test.h"

#include <cstdint>
#include <fstream>
#include <vector>

using namespace lxa::testing;

class DevpacTest : public LxaUITest {
protected:
    struct PpmImage {
        int width;
        int height;
        std::vector<uint8_t> pixels;
    };

    void SlowTypeString(const char* str, int settle_vblanks = 3) {
        char ch[2] = {0, 0};

        while (*str) {
            ch[0] = *str++;
            TypeString(ch);
            RunCyclesWithVBlank(settle_vblanks, 50000);
        }
    }

    PpmImage LoadPpm(const std::string& path) {
        std::ifstream capture(path, std::ios::binary);
        std::string magic;
        int width = 0;
        int height = 0;
        int max_value = 0;
        PpmImage image = {0, 0, {}};

        EXPECT_TRUE(capture.good()) << "Failed to open captured Devpac window image";
        if (!capture.good()) {
            return image;
        }

        capture >> magic >> width >> height >> max_value;
        EXPECT_EQ(magic, "P6") << "Captured Devpac window should use the PPM format";
        EXPECT_GT(width, 0);
        EXPECT_GT(height, 0);
        EXPECT_EQ(max_value, 255);
        if (magic != "P6" || width <= 0 || height <= 0 || max_value != 255) {
            return image;
        }

        capture.get();
        image.width = width;
        image.height = height;
        image.pixels.resize(static_cast<size_t>(width) * static_cast<size_t>(height) * 3U);
        capture.read(reinterpret_cast<char*>(image.pixels.data()), static_cast<std::streamsize>(image.pixels.size()));
        EXPECT_EQ(capture.gcount(), static_cast<std::streamsize>(image.pixels.size()));

        return image;
    }

    int CountNonBlackPixels(const PpmImage& image, int x1, int y1, int x2, int y2) {
        int count = 0;

        for (int y = y1; y <= y2; y++) {
            for (int x = x1; x <= x2; x++) {
                size_t pixel_offset = (static_cast<size_t>(y) * static_cast<size_t>(image.width) +
                                       static_cast<size_t>(x)) * 3U;

                if (image.pixels[pixel_offset] != 0 ||
                    image.pixels[pixel_offset + 1] != 0 ||
                    image.pixels[pixel_offset + 2] != 0) {
                    count++;
                }
            }
        }

        return count;
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
    PpmImage image;

    ASSERT_GT(window_info.y, 0) << "Devpac should open below the screen origin in rootless mode";
    ASSERT_TRUE(CaptureWindow(capture_path.c_str(), 0))
        << "Rootless Devpac window capture should succeed";

    image = LoadPpm(capture_path);

    EXPECT_EQ(image.width, window_info.width)
        << "Devpac should preserve its logical width in the host window capture";
    EXPECT_GT(image.height, window_info.height)
        << "Rootless Devpac capture should include the screen strip above the logical window";
}

TEST_F(DevpacTest, MenuBarRemainsVisibleAfterRepeatedMenuOpenClose) {
    const std::string before_path = ram_dir_path + "/devpac-menu-before.ppm";
    const std::string after_path = ram_dir_path + "/devpac-menu-after.ppm";
    const int menu_bar_x = window_info.x + 32;
    const int menu_bar_y = window_info.y / 2;
    const int first_item_y = window_info.y + 18;
    PpmImage before_image;
    PpmImage after_image;
    int before_non_black = 0;
    int after_non_black = 0;

    ASSERT_GT(window_info.y, 0) << "Devpac should expose the menu bar strip above the window";
    ASSERT_TRUE(CaptureWindow(before_path.c_str(), 0));
    before_image = LoadPpm(before_path);

    for (int i = 0; i < 3; i++) {
        lxa_inject_drag(menu_bar_x, menu_bar_y,
                        menu_bar_x, first_item_y,
                        LXA_MOUSE_RIGHT, 10);
        RunCyclesWithVBlank(20, 50000);
    }

    ASSERT_TRUE(CaptureWindow(after_path.c_str(), 0));
    after_image = LoadPpm(after_path);

    before_non_black = CountNonBlackPixels(before_image, 0, 0,
                                           before_image.width - 1, window_info.y - 1);
    after_non_black = CountNonBlackPixels(after_image, 0, 0,
                                          after_image.width - 1, window_info.y - 1);

    EXPECT_GT(before_non_black, 500)
        << "Devpac menu bar strip should render visible content before interaction";
    EXPECT_GT(after_non_black, 500)
        << "Devpac menu bar strip should still render visible content after repeated menu interaction";
    EXPECT_GE(after_non_black, before_non_black * 2 / 7)
        << "Devpac menu repaint should keep a meaningful amount of menu bar content after repeated menu interaction";
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
