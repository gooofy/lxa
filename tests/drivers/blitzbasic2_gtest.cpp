#include "lxa_test.h"

#include <algorithm>
#include <filesystem>
#include <string>

using namespace lxa::testing;

class BlitzBasic2Test : public LxaUITest {
protected:
    bool SetupOriginalSystemAssigns(bool add_libs = false,
                                    bool add_fonts = false,
                                    bool add_devs = false)
    {
        const char* home = std::getenv("HOME");
        if (home == nullptr || home[0] == '\0') {
            return false;
        }

        const std::filesystem::path system_base =
            std::filesystem::path(home) / "media" / "emu" / "amiga" / "FS-UAE" / "hdd" / "system";

        if (!std::filesystem::exists(system_base)) {
            return false;
        }

        if (add_libs && !lxa_add_assign_path("LIBS", (system_base / "Libs").c_str())) {
            return false;
        }

        if (add_fonts && !lxa_add_assign_path("FONTS", (system_base / "Fonts").c_str())) {
            return false;
        }

        if (add_devs && !lxa_add_assign_path("DEVS", (system_base / "Devs").c_str())) {
            return false;
        }

        return true;
    }

    bool SetupBlitzBasic2Assigns() {
        const char* apps = FindAppsPath();
        if (apps == nullptr) {
            return false;
        }

        const std::filesystem::path blitz_base = std::filesystem::path(apps) / "BlitzBasic2";
        if (!std::filesystem::exists(blitz_base / "blitz2")) {
            return false;
        }

        return lxa_add_assign("Blitz2", blitz_base.c_str())
            && lxa_add_assign_path("C", (blitz_base / "c").c_str())
            && lxa_add_assign_path("L", (blitz_base / "l").c_str())
            && lxa_add_assign_path("S", (blitz_base / "s").c_str());
    }

    void SetUp() override {
        config.rootless = false;
        LxaUITest::SetUp();

        if (!SetupOriginalSystemAssigns(true, true, true) || !SetupBlitzBasic2Assigns()) {
            GTEST_SKIP() << "BlitzBasic2 app bundle or original system disk not found";
        }

        ASSERT_EQ(lxa_load_program("APPS:BlitzBasic2/blitz2", ""), 0)
            << "Failed to load BlitzBasic2 via APPS: assign";

        ASSERT_TRUE(WaitForWindows(1, 20000))
            << "BlitzBasic2 did not open its editor window\n"
            << GetOutput();

        ASSERT_TRUE(GetWindowInfo(0, &window_info));
        ASSERT_TRUE(WaitForWindowDrawn(0, 5000))
            << "BlitzBasic2 editor window should expose visible content\n"
            << GetOutput();

        WaitForEventLoop(100, 10000);
        RunCyclesWithVBlank(120, 50000);
    }

    int MenuBarY() const {
        return std::max(4, window_info.y / 2);
    }

    void DragMenu(int menu_x, int item_y) {
        lxa_inject_drag(menu_x, MenuBarY(), menu_x, item_y, LXA_MOUSE_RIGHT, 10);
        RunCyclesWithVBlank(20, 50000);
    }
};

TEST_F(BlitzBasic2Test, StartupOpensVisibleIdeWindow) {
    EXPECT_TRUE(lxa_is_running())
        << "BlitzBasic2 should still be running after startup\n"
        << GetOutput();
    EXPECT_GT(window_info.width, 400);
    EXPECT_GT(window_info.height, 150);
    EXPECT_STRNE(window_info.title, "System Message")
        << "BlitzBasic2 should reach the IDE rather than a requester";

    int active_width = 0;
    int active_height = 0;
    int active_depth = 0;

    ASSERT_TRUE(lxa_get_screen_dimensions(&active_width, &active_height, &active_depth));
    EXPECT_EQ(active_width, 640);
    EXPECT_EQ(active_height, 256);
    EXPECT_EQ(active_depth, 2);

    const int screen_pixels = CountContentPixels(0, 0, active_width - 1, active_height - 1, 0);
    EXPECT_EQ(screen_pixels, 0)
        << "BlitzBasic2 currently exposes a flat grey startup surface; keep this regression visible until editor drawing lands";
}

TEST_F(BlitzBasic2Test, DISABLED_CaptureScreenshotsForReview) {
    ASSERT_TRUE(CaptureWindow("/tmp/blitzbasic2-startup.png", 0));
    DragMenu(40, window_info.y + 80);
    ASSERT_TRUE(lxa_capture_screen("/tmp/blitzbasic2-project-menu.png"));
}

TEST_F(BlitzBasic2Test, ProjectMenuOpensButDropdownIsCurrentlyClipped) {
    ClearOutput();
    DragMenu(40, window_info.y + 80);

    const std::string menu_path = ram_dir_path + "/blitzbasic2-project-menu.png";
    ASSERT_TRUE(lxa_capture_screen(menu_path.c_str()));

    RgbImage menu_image = LoadPng(menu_path);
    ASSERT_EQ(menu_image.width, 640);
    ASSERT_EQ(menu_image.height, 256);

    int white_pixels = 0;
    int black_pixels = 0;

    for (size_t i = 0; i + 2 < menu_image.pixels.size(); i += 3) {
        if (menu_image.pixels[i] == 255 && menu_image.pixels[i + 1] == 255 && menu_image.pixels[i + 2] == 255) {
            ++white_pixels;
        }
        if (menu_image.pixels[i] == 0 && menu_image.pixels[i + 1] == 0 && menu_image.pixels[i + 2] == 0) {
            ++black_pixels;
        }
    }

    EXPECT_GT(white_pixels, 10000)
        << "BlitzBasic2 should expose a large white dropdown when the Project menu opens";
    EXPECT_GT(black_pixels, 100)
        << "BlitzBasic2 Project menu should render dark text inside the dropdown";
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
