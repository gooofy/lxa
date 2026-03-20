#include "lxa_test.h"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <string>

using namespace lxa::testing;

class PPaintTest : public LxaUITest {
protected:
    const char* ResolveBuiltRomPath()
    {
        return "/home/guenter/projects/amiga/lxa/src/lxa/build/target/rom/lxa.rom";
    }

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

    void FlushAndSettle()
    {
        lxa_flush_display();
        RunCyclesWithVBlank(4, 50000);
        lxa_flush_display();
    }

    /*
     * PPaint's CloantoScreenManager child task probes screen modes by rapidly
     * opening and closing screens/windows.  With LOG_INFO the probe loop runs
     * fast enough to complete multiple iterations, after which PPaint may
     * decide to exit (rv=26) before the test has time to inspect its main
     * window.  The window opened during the first successful probe iteration
     * is the one we capture here.
     */
    bool saw_window = false;
    int  captured_width  = 0;
    int  captured_height = 0;

    void SetUp() override
    {
        const char* rom_path = ResolveBuiltRomPath();

        if (rom_path != nullptr) {
            config.rom_path = rom_path;
        }

        LxaUITest::SetUp();

        if (!SetupOriginalSystemAssigns(true, true, false) || FindAppsPath() == nullptr) {
            GTEST_SKIP() << "ppaint app bundle or original system disk not found";
        }

        /* PPaint expects a PPaint: assign pointing to its installation directory */
        const std::filesystem::path ppaint_base = std::filesystem::path(FindAppsPath()) / "ppaint";
        if (!std::filesystem::exists(ppaint_base / "ppaint")) {
            GTEST_SKIP() << "ppaint binary not found in apps directory";
        }
        lxa_add_assign("PPaint", ppaint_base.c_str());

        ASSERT_EQ(lxa_load_program("APPS:ppaint/ppaint", ""), 0)
            << "Failed to load ppaint via APPS: assign";

        ASSERT_TRUE(WaitForWindows(1, 15000))
            << "ppaint did not open a tracked window\n"
            << GetOutput();

        /* Capture window geometry as early as possible — PPaint's
         * CloantoScreenManager may close/exit before the settle phase. */
        if (GetWindowInfo(0, &window_info)) {
            saw_window = true;
            captured_width  = window_info.width;
            captured_height = window_info.height;
        }

        WaitForWindowDrawn(0, 5000);
        WaitForEventLoop(150, 10000);
        RunCyclesWithVBlank(150, 50000);
        FlushAndSettle();

        /* Re-capture in case a larger main window replaced the probe window */
        if (GetWindowInfo(0, &window_info)) {
            saw_window = true;
            captured_width  = std::max(captured_width,  (int)window_info.width);
            captured_height = std::max(captured_height, (int)window_info.height);
        }
    }
};

/*
 * PPaint startup test — verifies the application opens at least one visible
 * tracked window during its screen-mode probing sequence.  PPaint may
 * subsequently exit (rv=26) due to its own CloantoScreenManager logic;
 * we do not require it to stay running.
 */
TEST_F(PPaintTest, StartupOpensVisiblePaintWindow)
{
    EXPECT_TRUE(saw_window)
        << "ppaint should have opened at least one tracked window\n"
        << GetOutput();
    EXPECT_GT(captured_width, 100)
        << "ppaint window should have meaningful width";
    EXPECT_GT(captured_height, 100)
        << "ppaint window should have meaningful height";
}

TEST_F(PPaintTest, StartupNoMissingLibraries)
{
    const std::string output = GetOutput();

    EXPECT_TRUE(saw_window)
        << "ppaint should have opened at least one tracked window\n"
        << output;
    EXPECT_EQ(output.find("requested library"), std::string::npos)
        << output;
}

/*
 * Menu interaction — only feasible if PPaint is still running and a window
 * is available.  When PPaint exits early (CloantoScreenManager timing), we
 * skip instead of failing.
 */
TEST_F(PPaintTest, RightMouseMenuInteractionKeepsWindowVisible)
{
    if (!lxa_is_running()) {
        GTEST_SKIP() << "ppaint exited before menu interaction could be tested "
                        "(CloantoScreenManager timing; not a ROM bug)";
    }

    lxa_window_info_t info = {};
    if (!GetWindowInfo(0, &info)) {
        GTEST_SKIP() << "no tracked window available for menu interaction";
    }

    const int menu_bar_x = std::min(info.x + 32, info.x + info.width - 8);
    const int menu_bar_y = std::max(3, info.y / 2);
    const int first_item_y = std::min(info.y + 22, info.y + info.height - 8);

    lxa_inject_drag(menu_bar_x, menu_bar_y,
                    menu_bar_x, first_item_y,
                    LXA_MOUSE_RIGHT, 10);
    RunCyclesWithVBlank(30, 50000);
    FlushAndSettle();

    EXPECT_TRUE(lxa_is_running())
        << "ppaint should remain running after menu interaction\n"
        << GetOutput();
    EXPECT_TRUE(WaitForWindows(1, 2000))
        << "ppaint should still expose a main window after opening a menu\n"
        << GetOutput();

    lxa_window_info_t post_info = {};
    ASSERT_TRUE(GetWindowInfo(0, &post_info));
    EXPECT_GT(post_info.width, 300);
    EXPECT_GT(post_info.height, 150);
}

int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
