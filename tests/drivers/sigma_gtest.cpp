#include "lxa_test.h"

#include <algorithm>
#include <filesystem>
#include <string>

using namespace lxa::testing;

class SIGMAth2Test : public LxaUITest {
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

    bool HasSIGMAth2Bundle()
    {
        const char* apps = FindAppsPath();
        if (apps == nullptr) {
            return false;
        }

        const std::filesystem::path sigma_base = std::filesystem::path(apps) / "SIGMAth2";
        return std::filesystem::exists(sigma_base / "SIGMAth_2")
            && std::filesystem::exists(sigma_base / "SIGMAthResc.RCT");
    }

    void FlushAndSettle()
    {
        lxa_flush_display();
        RunCyclesWithVBlank(4, 50000);
        lxa_flush_display();
    }

    void SetUp() override
    {
        LxaUITest::SetUp();

        if (!SetupOriginalSystemAssigns(true, true, false) || !HasSIGMAth2Bundle()) {
            GTEST_SKIP() << "SIGMAth2 app bundle or original system disk not found";
        }

        ASSERT_EQ(lxa_load_program("APPS:SIGMAth2/SIGMAth_2", ""), 0)
            << "Failed to load SIGMAth2 via APPS: assign";

        ASSERT_TRUE(WaitForWindows(1, 20000))
            << "SIGMAth2 did not open a tracked window\n"
            << GetOutput();

        ASSERT_TRUE(GetWindowInfo(0, &window_info));
        ASSERT_TRUE(WaitForWindowDrawn(0, 5000))
            << "SIGMAth2 main window should draw visible content\n"
            << GetOutput();

        WaitForEventLoop(120, 10000);
        RunCyclesWithVBlank(120, 50000);
        FlushAndSettle();
    }
};

TEST_F(SIGMAth2Test, StartupOpensVisibleAnalysisWindow)
{
    EXPECT_TRUE(lxa_is_running())
        << "SIGMAth2 should still be running after startup\n"
        << GetOutput();
    EXPECT_STRNE(window_info.title, "System Message")
        << "SIGMAth2 should reach its analysis UI instead of a system requester";
    EXPECT_GE(window_info.width, 600);
    EXPECT_GE(window_info.height, 220);
}

TEST_F(SIGMAth2Test, StartupKeepsRunningWithoutResourceFailures)
{
    const std::string output = GetOutput();

    EXPECT_TRUE(lxa_is_running())
        << "SIGMAth2 should keep running after startup\n"
        << output;
    EXPECT_EQ(output.find("requested library"), std::string::npos)
        << output;
    EXPECT_EQ(output.find("SIGMAthResc.RCT"), std::string::npos)
        << output;
    EXPECT_EQ(output.find("MathIEEEDoubBas.Library"), std::string::npos)
        << output;
    EXPECT_EQ(output.find("RCT.Library"), std::string::npos)
        << output;
    EXPECT_EQ(output.find("Topaz.Font"), std::string::npos)
        << output;
}

TEST_F(SIGMAth2Test, MainWindowCaptureContainsVisibleUiRegions)
{
    EXPECT_TRUE(lxa_is_running())
        << "SIGMAth2 should remain running after startup\n"
        << GetOutput();
    EXPECT_STRNE(window_info.title, "System Message")
        << "SIGMAth2 should reach its analysis UI instead of a system requester";
    EXPECT_GE(window_info.width, 640);
    EXPECT_GE(window_info.height, 230);
    EXPECT_LE(window_info.width, 760);
    EXPECT_LE(window_info.height, 320);
    EXPECT_TRUE(WaitForWindowDrawn(0, 2000))
        << "SIGMAth2 startup window should remain drawable after settling\n"
        << GetOutput();
}

TEST_F(SIGMAth2Test, MenuBarRightMouseSmokeKeepsAnalysisWindowAlive)
{
    const int baseline_window_count = lxa_get_window_count();
    const int menu_bar_x = std::min(window_info.x + 32, window_info.x + window_info.width - 8);
    const int menu_bar_y = std::max(3, window_info.y / 2);
    const int first_item_y = std::min(window_info.y + 22, window_info.y + window_info.height - 8);
    lxa_window_info_t after_info = {};
    lxa_window_info_t added_info = {};
    int added_index = -1;
    int best_area = -1;

    lxa_inject_drag(menu_bar_x, menu_bar_y,
                    menu_bar_x, first_item_y,
                    LXA_MOUSE_RIGHT, 10);
    RunCyclesWithVBlank(30, 50000);
    FlushAndSettle();

    EXPECT_TRUE(lxa_is_running())
        << "SIGMAth2 should remain running after menu-bar interaction\n"
        << GetOutput();
    EXPECT_TRUE(WaitForWindows(1, 2000))
        << "SIGMAth2 should keep its main analysis window visible after menu interaction\n"
        << GetOutput();
    ASSERT_TRUE(GetWindowInfo(0, &after_info));
    EXPECT_GE(after_info.width, window_info.width)
        << "SIGMAth2 should keep or expand its visible rootless surface during menu interaction";
    EXPECT_GE(after_info.height, window_info.height)
        << "SIGMAth2 should keep or expand its visible rootless surface during menu interaction";
    EXPECT_GE(lxa_get_window_count(), baseline_window_count)
        << "SIGMAth2 menu interaction should not close its tracked analysis window";
    ASSERT_TRUE(WaitForWindows(baseline_window_count + 1, 3000))
        << "SIGMAth2 menu interaction should expose a tracked Analysis window\n"
        << GetOutput();
    for (int i = baseline_window_count; i < lxa_get_window_count(); ++i) {
        lxa_window_info_t info = {};
        int area = 0;

        if (!GetWindowInfo(i, &info)) {
            continue;
        }

        area = info.width * info.height;

        if (info.y > 0 && area > best_area) {
            best_area = area;
            added_info = info;
            added_index = i;
        }
    }

    ASSERT_GE(added_index, 0)
        << "SIGMAth2 should add at least one tracked content window after menu interaction\n"
        << GetOutput();
    EXPECT_STRNE(added_info.title, "System Message");
    EXPECT_GE(added_info.width, 500);
    EXPECT_GE(added_info.height, 150);
    EXPECT_TRUE(WaitForWindowDrawn(added_index, 5000))
        << "SIGMAth2 added tracked window should draw visible content\n"
        << GetOutput();
}

int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
