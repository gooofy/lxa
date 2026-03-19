#include "lxa_test.h"

#include <algorithm>
#include <string>

using namespace lxa::testing;

class PPaintTest : public LxaUITest {
protected:
    const char* ResolveBuiltRomPath()
    {
        return "/home/guenter/projects/amiga/lxa/src/lxa/build/target/rom/lxa.rom";
    }

    void FlushAndSettle()
    {
        lxa_flush_display();
        RunCyclesWithVBlank(4, 50000);
        lxa_flush_display();
    }

    void SetUp() override
    {
        const char* rom_path = ResolveBuiltRomPath();

        if (rom_path != nullptr) {
            config.rom_path = rom_path;
        }

        LxaUITest::SetUp();

        if (FindAppsPath() == nullptr) {
            GTEST_SKIP() << "lxa-apps directory not found";
        }

        ASSERT_EQ(lxa_load_program("APPS:ppaint/ppaint", ""), 0)
            << "Failed to load ppaint via APPS: assign";

        ASSERT_TRUE(WaitForWindows(1, 15000))
            << "ppaint did not open a tracked window\n"
            << GetOutput();

        ASSERT_TRUE(GetWindowInfo(0, &window_info));
        ASSERT_TRUE(WaitForWindowDrawn(0, 5000))
            << "ppaint main window should draw visible content\n"
            << GetOutput();

        WaitForEventLoop(150, 10000);
        RunCyclesWithVBlank(150, 50000);
        FlushAndSettle();
    }
};

TEST_F(PPaintTest, StartupOpensVisiblePaintWindow)
{
    EXPECT_TRUE(lxa_is_running())
        << "ppaint should still be running after startup\n"
        << GetOutput();
    EXPECT_GT(window_info.width, 300);
    EXPECT_GT(window_info.height, 150);
    EXPECT_STRNE(window_info.title, "System Message")
        << "ppaint should reach its editor window instead of a requester";
}

TEST_F(PPaintTest, StartupKeepsRunningWithoutLibraryFailures)
{
    const std::string output = GetOutput();

    EXPECT_TRUE(lxa_is_running())
        << "ppaint should keep running after startup\n"
        << output;
    EXPECT_EQ(output.find("requested library"), std::string::npos)
        << output;
}

TEST_F(PPaintTest, RightMouseMenuInteractionKeepsWindowVisible)
{
    const int menu_bar_x = std::min(window_info.x + 32, window_info.x + window_info.width - 8);
    const int menu_bar_y = std::max(3, window_info.y / 2);
    const int first_item_y = std::min(window_info.y + 22, window_info.y + window_info.height - 8);

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

    lxa_window_info_t info = {};
    ASSERT_TRUE(GetWindowInfo(0, &info));
    EXPECT_GT(info.width, 300);
    EXPECT_GT(info.height, 150);
}

TEST_F(PPaintTest, DISABLED_CaptureStartupForReview)
{
    GTEST_SKIP() << "PPaint currently switches to app-owned screens that are not capturable through the existing screenshot API";
}

int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
