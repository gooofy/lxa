/**
 * devpac_gtest.cpp - Google Test version of Devpac (HiSoft) test
 *
 * Tests Devpac assembler - opens and displays editor.
 * Verifies screen dimensions, editor initialization, text entry,
 * mouse input, and cursor key handling.
 */

#include "lxa_test.h"

#include <algorithm>

using namespace lxa::testing;

class DevpacTest : public LxaUITest {
protected:
    static constexpr int PEN_GREY = 0;
    static constexpr int PEN_BLACK = 1;
    static constexpr int MENU_BAR_HEIGHT = 11;

    void SlowTypeString(const char* str, int settle_vblanks = 3) {
        char ch[2] = {0, 0};

        while (*str) {
            ch[0] = *str++;
            TypeString(ch);
            RunCyclesWithVBlank(settle_vblanks, 50000);
        }
    }

    int CountNonBlackPixels(const RgbImage& image, int x1, int y1, int x2, int y2) {
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

    bool OpenAboutDialog(lxa_window_info_t* about_info = nullptr) {
        const int menu_bar_x = window_info.x + 32;
        const int menu_bar_y = std::max(3, window_info.y / 2);
        const int about_item_y = 128;

        lxa_inject_drag(menu_bar_x, menu_bar_y,
                        menu_bar_x, about_item_y,
                        LXA_MOUSE_RIGHT, 10);
        RunCyclesWithVBlank(40, 50000);

        if (!WaitForWindows(2, 5000)) {
            return false;
        }

        if (about_info != nullptr) {
            if (!GetWindowInfo(1, about_info)) {
                return false;
            }
        }

        return WaitForWindowDrawn(1, 5000);
    }

    int FindBottomGadgetIndex(int window_index) {
        auto gadgets = GetGadgets(window_index);
        int bottom_gadget = -1;
        int bottom_top = -1;

        for (size_t i = 0; i < gadgets.size(); ++i) {
            if (gadgets[i].top > bottom_top) {
                bottom_top = gadgets[i].top;
                bottom_gadget = static_cast<int>(i);
            }
        }

        return bottom_gadget;
    }

    int CountPenPixelsInRow(int left, int right, int y, int pen) {
        int count = 0;

        for (int x = left; x <= right; ++x) {
            if (ReadPixel(x, y) == pen) {
                count++;
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
    const std::string capture_path = ram_dir_path + "/devpac-window.png";
    RgbImage image;
    int top_strip_non_black = 0;

    ASSERT_TRUE(CaptureWindow(capture_path.c_str(), 0))
        << "Rootless Devpac window capture should succeed";

    image = LoadPng(capture_path);

    top_strip_non_black = CountNonBlackPixels(image,
                                              0,
                                              0,
                                              image.width - 1,
                                              std::min(image.height - 1, MENU_BAR_HEIGHT - 1));

    EXPECT_EQ(image.width, window_info.width)
        << "Devpac should preserve its logical width in the host window capture";
    EXPECT_GE(image.height, window_info.height)
        << "Rootless Devpac capture should preserve at least the logical window height";
    EXPECT_GT(top_strip_non_black, 500)
        << "Rootless Devpac capture should include visible menu-bar content in the top strip";
}

TEST_F(DevpacTest, MenuBarRemainsVisibleAfterRepeatedMenuOpenClose) {
    const std::string before_path = ram_dir_path + "/devpac-menu-before.png";
    const std::string after_path = ram_dir_path + "/devpac-menu-after.png";
    const int menu_bar_x = window_info.x + 32;
    const int menu_bar_y = std::max(3, window_info.y / 2);
    const int first_item_y = window_info.y + 18;
    RgbImage before_image;
    RgbImage after_image;
    int before_non_black = 0;
    int after_non_black = 0;

    ASSERT_TRUE(CaptureWindow(before_path.c_str(), 0));
    before_image = LoadPng(before_path);

    for (int i = 0; i < 3; i++) {
        lxa_inject_drag(menu_bar_x, menu_bar_y,
                        menu_bar_x, first_item_y,
                        LXA_MOUSE_RIGHT, 10);
        RunCyclesWithVBlank(20, 50000);
    }

    ASSERT_TRUE(CaptureWindow(after_path.c_str(), 0));
    after_image = LoadPng(after_path);

    after_non_black = CountNonBlackPixels(after_image, 0, 0,
                                          after_image.width - 1,
                                          std::min(after_image.height - 1, MENU_BAR_HEIGHT - 1));

    before_non_black = CountNonBlackPixels(before_image, 0, 0,
                                           before_image.width - 1,
                                           std::min(before_image.height - 1, MENU_BAR_HEIGHT - 1));

    EXPECT_GT(before_non_black, 500)
        << "Devpac menu bar strip should render visible content before interaction";
    EXPECT_GT(after_non_black, 500)
        << "Devpac menu bar strip should still render visible content after repeated menu interaction";
}

TEST_F(DevpacTest, AboutDialogDoesNotDuplicateInsideMainWindow) {
    const std::string main_before_path = ram_dir_path + "/devpac-about-main-before.png";
    const std::string main_after_path = ram_dir_path + "/devpac-about-main-after.png";
    RgbImage before_image;
    RgbImage after_image;
    int before_main_pixels = 0;
    int after_main_pixels = 0;
    lxa_window_info_t about_info;

    ASSERT_TRUE(CaptureWindow(main_before_path.c_str(), 0));
    before_image = LoadPng(main_before_path);

    ASSERT_TRUE(OpenAboutDialog(&about_info))
        << "Devpac About dialog should open as a second window";

    ASSERT_TRUE(CaptureWindow(main_after_path.c_str(), 0));
    after_image = LoadPng(main_after_path);

    before_main_pixels = CountNonBlackPixels(before_image,
                                             8,
                                             std::min(before_image.height - 1, MENU_BAR_HEIGHT + 24),
                                             before_image.width - 9,
                                             before_image.height - 9);
    after_main_pixels = CountNonBlackPixels(after_image,
                                            8,
                                            std::min(after_image.height - 1, MENU_BAR_HEIGHT + 24),
                                            after_image.width - 9,
                                            after_image.height - 9);

    EXPECT_STREQ(about_info.title, "Devpac Amiga")
        << "The second rootless window should be the Devpac About dialog";
    EXPECT_LT(before_main_pixels > after_main_pixels
                  ? before_main_pixels - after_main_pixels
                  : after_main_pixels - before_main_pixels,
              before_main_pixels / 10)
        << "Opening the About dialog should leave the main Devpac content largely unchanged";
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
