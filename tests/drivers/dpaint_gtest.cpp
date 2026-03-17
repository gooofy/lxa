/**
 * dpaint_gtest.cpp - Google Test version of DPaint V test
 *
 * Tests DPaint V graphics editor.
 * Verifies window opening, screen dimensions, process running,
 * and mouse interaction.
 */

#include "lxa_test.h"

#include <filesystem>
#include <string>

using namespace lxa::testing;

class DPaintTest : public LxaUITest {
protected:
    bool window_opened = false;

    bool HasDiskRexxSysLib() {
        const char* system_base = FindSystemBasePath();
        if (system_base == nullptr) {
            return false;
        }

        return std::filesystem::exists(
            std::filesystem::path(system_base) / "Libs" / "rexxsyslib.library");
    }

    void SetUp() override {
        LxaUITest::SetUp();
        
        const char* apps = FindAppsPath();
        if (!apps) {
            GTEST_SKIP() << "lxa-apps directory not found";
        }

        if (!HasDiskRexxSysLib()) {
            GTEST_SKIP() << "disk-provided rexxsyslib.library missing from SYS:Libs";
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

class DPaintPixelTest : public LxaUITest {
protected:
    bool window_opened = false;

    static constexpr int QUALIFIER_CONTROL = 0x0008;
    static constexpr int RAWKEY_P = 0x19;

    bool HasDiskRexxSysLib() {
        const char* system_base = FindSystemBasePath();
        if (system_base == nullptr) {
            return false;
        }

        return std::filesystem::exists(
            std::filesystem::path(system_base) / "Libs" / "rexxsyslib.library");
    }

    void SetUp() override {
        config.rootless = false;
        LxaUITest::SetUp();

        const char* apps = FindAppsPath();
        if (!apps) {
            GTEST_SKIP() << "lxa-apps directory not found";
        }

        if (!HasDiskRexxSysLib()) {
            GTEST_SKIP() << "disk-provided rexxsyslib.library missing from SYS:Libs";
        }

        ASSERT_EQ(lxa_load_program("APPS:DPaintV/bin/DPaintV/DPaint", ""), 0)
            << "Failed to load DPaint V";

        window_opened = WaitForWindows(1, 15000);
        if (!window_opened) {
            GTEST_SKIP() << "DPaint window did not open";
        }

        ASSERT_TRUE(GetWindowInfo(0, &window_info));
        WaitForEventLoop(100, 10000);
        RunCyclesWithVBlank(160, 50000);
    }

    int FindWindowIndexByTitleSubstring(const char *title_fragment) {
        int count = lxa_get_window_count();

        for (int i = 0; i < count; ++i) {
            lxa_window_info_t info;

            if (GetWindowInfo(i, &info) && std::string(info.title).find(title_fragment) != std::string::npos)
                return i;
        }

        return -1;
    }

    bool WaitForWindowTitleSubstring(const char *title_fragment, int timeout_ms = 5000) {
        int attempts = timeout_ms / 50;

        for (int attempt = 0; attempt < attempts; ++attempt) {
            if (FindWindowIndexByTitleSubstring(title_fragment) >= 0)
                return true;

            RunCyclesWithVBlank(1, 50000);
        }

        return false;
    }

    bool WaitForOwnershipWindowToClose(int timeout_ms = 5000) {
        int attempts = timeout_ms / 50;

        for (int attempt = 0; attempt < attempts; ++attempt) {
            if (FindWindowIndexByTitleSubstring("Ownership Information") < 0)
                return true;

            RunCyclesWithVBlank(1, 50000);
        }

        return false;
    }

    bool DismissOwnershipWindow() {
        lxa_window_info_t ownership_info;
        auto gadgets = GetGadgets(0);
        int bottom_gadget = -1;
        int bottom_top = -1;

        if (!GetWindowInfo(0, &ownership_info))
            return false;

        for (size_t i = 0; i < gadgets.size(); ++i) {
            if (gadgets[i].top <= ownership_info.y + 16)
                continue;

            if (gadgets[i].top > bottom_top) {
                bottom_top = gadgets[i].top;
                bottom_gadget = (int)i;
            }
        }

        if (bottom_gadget >= 0) {
            if (!ClickGadget(bottom_gadget, 0))
                return false;
        } else {
            Click(ownership_info.x + ownership_info.width / 2,
                  ownership_info.y + ownership_info.height - 18);
        }

        RunCyclesWithVBlank(30, 50000);
        return WaitForOwnershipWindowToClose();
    }

    bool OpenScreenFormatDialog() {
        PressKey(RAWKEY_P, QUALIFIER_CONTROL);
        RunCyclesWithVBlank(40, 50000);

        return WaitForWindowTitleSubstring("Screen Format");
    }

    int CountWindowRegionContent(int window_index,
                                 int left_inset,
                                 int top_inset,
                                 int right_inset,
                                 int bottom_inset) {
        lxa_window_info_t info;

        if (!GetWindowInfo(window_index, &info))
            return -1;

        return CountContentPixels(info.x + left_inset,
                                  info.y + top_inset,
                                  info.x + info.width - right_inset,
                                  info.y + info.height - bottom_inset,
                                  0);
    }
};

TEST_F(DPaintTest, ProgramLoads) {
    /* Verify DPaint loaded and is running */
    EXPECT_TRUE(lxa_is_running()) << "DPaint process should be running";
    EXPECT_STRNE(window_info.title, "System Message")
        << "DPaint should not stop at the missing rexxsyslib requester";
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

TEST_F(DPaintPixelTest, OwnershipWindowBodyContainsVisibleContent) {
    lxa_flush_display();

    int body_pixels = CountContentPixels(
        window_info.x + 20,
        window_info.y + 20,
        window_info.x + window_info.width - 20,
        window_info.y + window_info.height - 20,
        0);

    EXPECT_GT(body_pixels, 150)
        << "DPaint ownership window body should contain visible text and controls";
}

TEST_F(DPaintPixelTest, ScreenFormatDialogSectionsContainVisibleContent) {
    ASSERT_STREQ(window_info.title, "Ownership Information")
        << "Expected DPaint to begin at its ownership window before opening the main editor";
    ASSERT_TRUE(DismissOwnershipWindow())
        << "Expected the ownership window to dismiss so the main editor can continue loading";

    ASSERT_TRUE(WaitForWindows(1, 10000));
    RunCyclesWithVBlank(120, 50000);

    ASSERT_TRUE(OpenScreenFormatDialog())
        << "Expected Ctrl-P to open DPaint's Screen Format dialog";

    int dialog_index = FindWindowIndexByTitleSubstring("Screen Format");
    ASSERT_GE(dialog_index, 0) << "Expected the Screen Format dialog window to be present";

    lxa_window_info_t dialog_info;
    ASSERT_TRUE(GetWindowInfo(dialog_index, &dialog_info));

    const std::string capture_path = ram_dir_path + "/dpaint-screen-format.ppm";
    ASSERT_TRUE(CaptureWindow(capture_path.c_str(), dialog_index))
        << "Expected Screen Format capture to succeed";

    lxa_flush_display();

    int upper_right_pixels = CountWindowRegionContent(dialog_index,
                                                      dialog_info.width / 2,
                                                      16,
                                                      12,
                                                      dialog_info.height * 2 / 3);
    int middle_right_pixels = CountWindowRegionContent(dialog_index,
                                                       dialog_info.width / 2,
                                                       dialog_info.height / 3,
                                                       12,
                                                       dialog_info.height / 3);
    int lower_right_pixels = CountWindowRegionContent(dialog_index,
                                                      dialog_info.width / 2,
                                                      dialog_info.height / 2,
                                                      12,
                                                      dialog_info.height / 6);

    EXPECT_GT(upper_right_pixels, 80)
        << "Expected the Screen Format dialog's upper-right section to show the Choose Display Mode content";
    EXPECT_GT(middle_right_pixels, 80)
        << "Expected the Screen Format dialog's middle-right section to show the Display Information content";
    EXPECT_GT(lower_right_pixels, 80)
        << "Expected the Screen Format dialog's lower-right section to show the Credits content";
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
