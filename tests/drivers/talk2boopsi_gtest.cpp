#include "lxa_test.h"

using namespace lxa::testing;

namespace {

constexpr int PEN_GREY = 0;
constexpr int TITLE_BAR_HEIGHT = 11;

constexpr int PROP_LEFT = 5;
constexpr int PROP_TOP = 16;
constexpr int PROP_WIDTH = 10;
constexpr int PROP_HEIGHT = 80;

constexpr int STRING_LEFT = 20;
constexpr int STRING_TOP = 16;
constexpr int STRING_WIDTH = 45;
constexpr int STRING_HEIGHT = 18;

}  // namespace

class Talk2BoopsiTest : public LxaUITest {
protected:
    void SetUp() override {
        LxaUITest::SetUp();

        ASSERT_EQ(lxa_load_program("SYS:Talk2Boopsi", ""), 0);
        ASSERT_TRUE(WaitForWindows(1, 5000));
        ASSERT_TRUE(GetWindowInfo(0, &window_info));
        ASSERT_TRUE(WaitForWindowDrawn(0, 5000));

        WaitForEventLoop(100, 10000);
        RunCyclesWithVBlank(20, 50000);
    }
};

TEST_F(Talk2BoopsiTest, StaysOpenUntilClosed) {
    RunCyclesWithVBlank(80, 50000);
    EXPECT_TRUE(lxa_is_running());
}

TEST_F(Talk2BoopsiTest, HandlesMouseAndKeyboardInputWithoutExiting) {
    int prop_x = window_info.x + PROP_LEFT + (PROP_WIDTH / 2);
    int prop_y = window_info.y + TITLE_BAR_HEIGHT + PROP_TOP + (PROP_HEIGHT / 2);
    int string_x = window_info.x + STRING_LEFT + (STRING_WIDTH / 2);
    int string_y = window_info.y + TITLE_BAR_HEIGHT + STRING_TOP + (STRING_HEIGHT / 2);

    Click(prop_x, prop_y);
    RunCyclesWithVBlank(20, 50000);

    Click(string_x, string_y);
    RunCyclesWithVBlank(20, 50000);

    TypeString("55");
    RunCyclesWithVBlank(40, 50000);

    EXPECT_TRUE(lxa_is_running());
}

TEST_F(Talk2BoopsiTest, CloseWindowExitsCleanly) {
    Click(window_info.x + 5, window_info.y + 5);
    RunCyclesWithVBlank(20, 50000);

    EXPECT_TRUE(lxa_wait_exit(5000));
}

class Talk2BoopsiPixelTest : public LxaUITest {
protected:
    void SetUp() override {
        config.rootless = false;
        LxaUITest::SetUp();

        ASSERT_EQ(lxa_load_program("SYS:Talk2Boopsi", ""), 0);
        ASSERT_TRUE(WaitForWindows(1, 5000));
        ASSERT_TRUE(GetWindowInfo(0, &window_info));

        WaitForEventLoop(100, 10000);
        RunCyclesWithVBlank(50, 100000);
        lxa_flush_display();
    }
};

TEST_F(Talk2BoopsiPixelTest, PropGadgetRendersVisibleContent) {
    int content = CountContentPixels(window_info.x + PROP_LEFT,
                                     window_info.y + PROP_TOP,
                                     window_info.x + PROP_LEFT + PROP_WIDTH - 1,
                                     window_info.y + PROP_TOP + PROP_HEIGHT - 1,
                                     PEN_GREY);

    EXPECT_GT(content, 20);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
