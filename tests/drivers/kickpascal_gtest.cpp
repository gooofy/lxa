/**
 * kickpascal_gtest.cpp - Google Test version of KickPascal 2 test
 *
 * Tests KickPascal IDE - editor with text entry and cursor keys.
 * Verifies screen dimensions, editor initialization, Pascal code entry,
 * mouse input, and cursor key handling.
 */

#include "lxa_test.h"

#include <algorithm>
#include <string>

using namespace lxa::testing;

#define RAWKEY_BACKSPACE 0x41

class KickPascalTest : public LxaUITest {
protected:
    bool IsBlackPixel(const RgbImage& image, int x, int y) {
        size_t pixel_offset = (static_cast<size_t>(y) * static_cast<size_t>(image.width) +
                               static_cast<size_t>(x)) * 3U;

        return image.pixels[pixel_offset] == 0 &&
               image.pixels[pixel_offset + 1] == 0 &&
               image.pixels[pixel_offset + 2] == 0;
    }

    int CountBlackPixels(const RgbImage& image, int x1, int y1, int x2, int y2) {
        int count = 0;

        x1 = std::max(0, x1);
        y1 = std::max(0, y1);
        x2 = std::min(image.width - 1, x2);
        y2 = std::min(image.height - 1, y2);

        for (int y = y1; y <= y2; y++) {
            for (int x = x1; x <= x2; x++) {
                if (IsBlackPixel(image, x, y)) {
                    count++;
                }
            }
        }

        return count;
    }

    int CountPixelsDifferentFromSample(const RgbImage& image,
                                       int x1, int y1, int x2, int y2,
                                       int sample_x, int sample_y,
                                       int tolerance = 12) {
        int count = 0;
        size_t sample_offset;
        int sample_r;
        int sample_g;
        int sample_b;

        x1 = std::max(0, x1);
        y1 = std::max(0, y1);
        x2 = std::min(image.width - 1, x2);
        y2 = std::min(image.height - 1, y2);
        sample_x = std::max(0, std::min(image.width - 1, sample_x));
        sample_y = std::max(0, std::min(image.height - 1, sample_y));

        sample_offset = (static_cast<size_t>(sample_y) * static_cast<size_t>(image.width) +
                         static_cast<size_t>(sample_x)) * 3U;
        sample_r = image.pixels[sample_offset];
        sample_g = image.pixels[sample_offset + 1];
        sample_b = image.pixels[sample_offset + 2];

        for (int y = y1; y <= y2; y++) {
            for (int x = x1; x <= x2; x++) {
                size_t pixel_offset = (static_cast<size_t>(y) * static_cast<size_t>(image.width) +
                                       static_cast<size_t>(x)) * 3U;
                int delta = std::abs(static_cast<int>(image.pixels[pixel_offset]) - sample_r) +
                            std::abs(static_cast<int>(image.pixels[pixel_offset + 1]) - sample_g) +
                            std::abs(static_cast<int>(image.pixels[pixel_offset + 2]) - sample_b);

                if (delta > tolerance) {
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
        
        /* Load via APPS: assign (mapped to lxa-apps directory in LxaTest::SetUp) */
        ASSERT_EQ(lxa_load_program("APPS:kickpascal2/bin/KP2/KP", ""), 0) 
            << "Failed to load KickPascal via APPS: assign";
        
        ASSERT_TRUE(WaitForWindows(1, 10000)) 
            << "KickPascal window did not open";
        
        ASSERT_TRUE(GetWindowInfo(0, &window_info));
        ASSERT_TRUE(WaitForWindowDrawn(0, 5000))
            << "KickPascal window did not draw";
        
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

TEST_F(KickPascalTest, RootlessHostWindowUsesLandscapeExtent) {
    const std::string capture_path = ram_dir_path + "/kickpascal-window.png";
    RgbImage image;

    ASSERT_TRUE(CaptureWindow(capture_path.c_str(), 0))
        << "Rootless KickPascal window capture should succeed";

    image = LoadPng(capture_path);
    ASSERT_GT(image.width, 0);
    ASSERT_GT(image.height, 0);

    EXPECT_GT(image.width, window_info.width)
        << "Host-side KickPascal window should widen beyond the logical Amiga width";
    EXPECT_GT(image.height, window_info.height)
        << "Rootless KickPascal capture should include the screen strip above the logical window";
    EXPECT_GT(image.width, image.height)
        << "KickPascal should present a landscape host window rather than a portrait one";
}

TEST_F(KickPascalTest, SplashLogoStaysFullyInsideVisibleArea) {
    const std::string capture_path = ram_dir_path + "/kickpascal-window.png";
    const int search_x1 = 0;
    const int search_y1 = 16;
    const int search_x2 = 240;
    const int search_y2 = 72;
    RgbImage image;
    int min_x = search_x2 + 1;
    int min_y = search_y2 + 1;
    int max_x = -1;
    int max_y = -1;

    ASSERT_TRUE(CaptureWindow(capture_path.c_str(), 0))
        << "KickPascal splash capture should succeed";

    image = LoadPng(capture_path);
    ASSERT_GT(image.width, 0);
    ASSERT_GT(image.height, 0);

    for (int y = search_y1; y <= search_y2; y++) {
        for (int x = search_x1; x <= search_x2; x++) {
            if (IsBlackPixel(image, x, y)) {
                min_x = std::min(min_x, x);
                min_y = std::min(min_y, y);
                max_x = std::max(max_x, x);
                max_y = std::max(max_y, y);
            }
        }
    }

    ASSERT_GE(max_x, min_x) << "KickPascal splash logo should render visible dark pixels";
    EXPECT_GE(min_x, 90)
        << "KickPascal splash logo should not be pushed against the left edge";
    EXPECT_LE(max_x, 190)
        << "KickPascal splash logo should fit without clipping its right edge";
    EXPECT_GE(max_x - min_x + 1, 70)
        << "KickPascal splash logo should retain most of its expected width";
    EXPECT_GE(min_y, 18)
        << "KickPascal splash logo should stay within the upper splash band";
    EXPECT_LE(max_y, 64)
        << "KickPascal splash logo should stay within the upper splash band";
}

TEST_F(KickPascalTest, EditorVisible) {
    EXPECT_GT(window_info.height, 200)
        << "KickPascal should reserve a substantial editor workspace below the splash area";
    EXPECT_TRUE(lxa_is_running())
        << "KickPascal should remain running while the editor workspace is visible";
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

TEST_F(KickPascalTest, WorkspacePromptAcceptsDeletingDefaultEntry) {
    RunCyclesWithVBlank(20, 50000);

    PressKey(RAWKEY_BACKSPACE, 0);
    RunCyclesWithVBlank(5, 50000);
    PressKey(RAWKEY_BACKSPACE, 0);
    RunCyclesWithVBlank(5, 50000);
    PressKey(RAWKEY_BACKSPACE, 0);
    RunCyclesWithVBlank(5, 50000);

    TypeString("100");
    RunCyclesWithVBlank(10, 50000);
    PressKey(0x44, 0);
    RunCyclesWithVBlank(80, 50000);

    EXPECT_TRUE(lxa_is_running()) << "KickPascal should stay running after editing workspace size";
    EXPECT_GE(lxa_get_window_count(), 1) << "KickPascal should keep a window after workspace prompt submission";
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
