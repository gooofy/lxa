#include "lxa_test.h"

#include <algorithm>
#include <string>

using namespace lxa::testing;

class SysInfoTest : public LxaUITest {
protected:
    static constexpr int PEN_BLACK = 1;
    static constexpr int GADGET_ID_QUIT = 1;
    static constexpr int GADGET_ID_SPEED = 2;
    static constexpr int GADGET_ID_DRIVES = 4;
    static constexpr int GADGET_ID_BOARDS = 6;
    static constexpr int GADGET_ID_MEMORY = 7;
    static constexpr int GADGET_ID_LIBRARIES = 9;
    static constexpr int GADGET_ID_EXPAND = 10;

    void SetUp() override {
        LxaUITest::SetUp();

        if (FindAppsPath() == nullptr) {
            GTEST_SKIP() << "lxa-apps directory not found";
        }

        ASSERT_EQ(lxa_load_program("APPS:SysInfo/bin/SysInfo/SysInfo", ""), 0)
            << "Failed to load SysInfo via APPS: assign";

        ASSERT_TRUE(WaitForWindows(1, 15000))
            << "SysInfo did not open a rootless window\n"
            << GetOutput();

        ASSERT_TRUE(GetWindowInfo(0, &window_info));
        ASSERT_TRUE(WaitForWindowDrawn(0, 5000))
            << "SysInfo rootless window should expose visible content\n"
            << GetOutput();

        WaitForEventLoop(120, 10000);
        RunCyclesWithVBlank(120, 50000);
    }

    int CountNonBlackPixels(const RgbImage& image, int x1, int y1, int x2, int y2) {
        int count = 0;
        const int left = std::max(0, x1);
        const int top = std::max(0, y1);
        const int right = std::min(x2, image.width - 1);
        const int bottom = std::min(y2, image.height - 1);

        for (int y = top; y <= bottom; ++y) {
            for (int x = left; x <= right; ++x) {
                size_t offset = (static_cast<size_t>(y) * static_cast<size_t>(image.width)
                                 + static_cast<size_t>(x)) * 3U;
                if (image.pixels[offset] != 0 ||
                    image.pixels[offset + 1] != 0 ||
                    image.pixels[offset + 2] != 0) {
                    ++count;
                }
            }
        }

        return count;
    }

    RgbImage CaptureMainWindow(const std::string& file_name) {
        const std::string path = ram_dir_path + "/" + file_name;
        EXPECT_TRUE(CaptureWindow(path.c_str(), 0))
            << "Failed to capture SysInfo main window to " << path;
        return LoadPng(path);
    }

    int FindGadgetIndexById(uint16_t gadget_id) {
        auto gadgets = GetGadgets(0);

        for (size_t i = 0; i < gadgets.size(); ++i) {
            if (gadgets[i].gadget_id == gadget_id) {
                return static_cast<int>(i);
            }
        }

        return -1;
    }

    int CountImageDifferences(const RgbImage& before, const RgbImage& after,
                              int x1, int y1, int x2, int y2) {
        int count = 0;
        const int left = std::max(0, x1);
        const int top = std::max(0, y1);
        const int right = std::min(std::min(before.width, after.width) - 1, x2);
        const int bottom = std::min(std::min(before.height, after.height) - 1, y2);

        for (int y = top; y <= bottom; ++y) {
            for (int x = left; x <= right; ++x) {
                size_t offset = (static_cast<size_t>(y) * static_cast<size_t>(before.width)
                                 + static_cast<size_t>(x)) * 3U;
                if (before.pixels[offset] != after.pixels[offset] ||
                    before.pixels[offset + 1] != after.pixels[offset + 1] ||
                    before.pixels[offset + 2] != after.pixels[offset + 2]) {
                    ++count;
                }
            }
        }

        return count;
    }

};

TEST_F(SysInfoTest, StartupOpensAndDrawsMainWindow) {
    int screen_width = 0;
    int screen_height = 0;
    int screen_depth = 0;

    EXPECT_TRUE(lxa_is_running())
        << "SysInfo should remain running after startup\n"
        << GetOutput();
    EXPECT_EQ(window_info.width, 640);
    EXPECT_EQ(window_info.height, 256);
    EXPECT_STREQ(window_info.title, "LXA Window");
    EXPECT_GT(lxa_get_window_content(0), 10000)
        << "SysInfo should expose a large amount of visible rootless content";

    ASSERT_TRUE(lxa_get_screen_dimensions(&screen_width, &screen_height, &screen_depth));
    EXPECT_EQ(screen_width, 640);
    EXPECT_EQ(screen_height, 256);
    EXPECT_GE(screen_depth, 3);
}

TEST_F(SysInfoTest, ScreenPixelsUseMultiplePensAfterStartup) {
    int screen_width = 0;
    int screen_height = 0;
    int screen_depth = 0;
    int unique_pen_count = 0;
    bool seen[8] = {false, false, false, false, false, false, false, false};

    ASSERT_TRUE(lxa_get_screen_dimensions(&screen_width, &screen_height, &screen_depth));

    for (int y = 0; y < screen_height; y += 16) {
        for (int x = 0; x < screen_width; x += 16) {
            int pen = -1;
            ASSERT_TRUE(lxa_read_pixel(x, y, &pen));
            if (pen >= 0 && pen < 8 && !seen[pen]) {
                seen[pen] = true;
                ++unique_pen_count;
            }
        }
    }

    EXPECT_GE(unique_pen_count, 4)
        << "SysInfo should populate the custom screen with several distinct pens after startup";
}

TEST_F(SysInfoTest, MainWindowCaptureIncludesVisibleUiPixels) {
    RgbImage image = CaptureMainWindow("sysinfo-main-window.png");
    const int all_pixels = CountNonBlackPixels(image, 0, 0, image.width - 1, image.height - 1);

    ASSERT_EQ(image.width, 640);
    ASSERT_EQ(image.height, 256);
    EXPECT_GT(all_pixels, 30000)
        << "SysInfo rootless capture should include a substantial amount of drawn UI content";
}

TEST_F(SysInfoTest, MainWindowCaptureContainsContentInKeyRegions) {
    RgbImage image = CaptureMainWindow("sysinfo-main-window-regions.png");

    const int software_panel_pixels = CountNonBlackPixels(image, 24, 24, 280, 182);
    const int hardware_panel_pixels = CountNonBlackPixels(image, 332, 24, 620, 182);
    const int bottom_gadget_pixels = CountNonBlackPixels(image, 20, 184, 620, 248);

    EXPECT_GT(software_panel_pixels, 12000)
        << "SysInfo should render visible software-list content in the left panel";
    EXPECT_GT(hardware_panel_pixels, 7000)
        << "SysInfo should render visible hardware summary content in the right panel";
    EXPECT_GT(bottom_gadget_pixels, 8000)
        << "SysInfo should render its lower gadget rows and status area";
}

TEST_F(SysInfoTest, MainWindowExposesTrackedGadgetsForPrimaryActions) {
    auto gadgets = GetGadgets(0);

    ASSERT_GE(gadgets.size(), 10u)
        << "SysInfo should expose its main action gadgets through tracked Intuition gadgets";

    EXPECT_GE(FindGadgetIndexById(GADGET_ID_QUIT), 0);
    EXPECT_GE(FindGadgetIndexById(GADGET_ID_SPEED), 0);
    EXPECT_GE(FindGadgetIndexById(GADGET_ID_MEMORY), 0);
    EXPECT_GE(FindGadgetIndexById(GADGET_ID_BOARDS), 0);
    EXPECT_GE(FindGadgetIndexById(GADGET_ID_DRIVES), 0);
    EXPECT_GE(FindGadgetIndexById(GADGET_ID_LIBRARIES), 0);
    EXPECT_GE(FindGadgetIndexById(GADGET_ID_EXPAND), 0);
}

TEST_F(SysInfoTest, LibrariesGadgetIsTrackedViaScreenGadgetList) {
    const int gadget_index = FindGadgetIndexById(GADGET_ID_LIBRARIES);

    ASSERT_GE(gadget_index, 0) << "SysInfo should expose the LIBRARIES gadget";
    EXPECT_GE(GetGadgetCount(0), 15)
        << "SysInfo should expose both window gadgets and screen-level gadgets to host-side tests";
}

TEST_F(SysInfoTest, SpeedGadgetRefreshesComparisonArea) {
    const int gadget_index = FindGadgetIndexById(GADGET_ID_SPEED);
    const std::string before_path = ram_dir_path + "/sysinfo-speed-before.png";
    const std::string after_path = ram_dir_path + "/sysinfo-speed-after.png";
    RgbImage before_image;
    RgbImage after_image;
    int comparison_diff = 0;

    ASSERT_GE(gadget_index, 0) << "SysInfo should expose the SPEED gadget";
    ASSERT_TRUE(CaptureWindow(before_path.c_str(), 0));
    before_image = LoadPng(before_path);

    ASSERT_TRUE(ClickGadget(gadget_index, 0));
    RunCyclesWithVBlank(240, 100000);

    ASSERT_TRUE(CaptureWindow(after_path.c_str(), 0));
    after_image = LoadPng(after_path);

    comparison_diff = CountImageDifferences(before_image, after_image, 16, 90, 394, 198);
    EXPECT_GT(comparison_diff, 500)
        << "Clicking SPEED should refresh the speed-comparison area";
}

TEST_F(SysInfoTest, CloseGadgetLeavesApplicationRunning) {
    ASSERT_TRUE(lxa_click_close_gadget(0));
    RunCyclesWithVBlank(120, 50000);

    EXPECT_TRUE(lxa_is_running())
        << "SysInfo should ignore the close gadget and keep its custom screen running";
    EXPECT_EQ(lxa_get_window_count(), 1)
        << "SysInfo should keep its main window open after a close-gadget click";
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
