/**
 * easyrequest_gtest.cpp - Google Test driver for EasyRequest sample
 *
 * Tests that the EasyRequest sample matches RKM behavior:
 * - Opens a requester window with title "Request Window Name"
 * - Body text has 3 lines with %s variable substitution
 * - 3 buttons: Yes | 3125794 | No (with %ld substitution in middle)
 * - Button IDs: Yes=1, middle=2, No=0
 * - Program prints which button was selected
 */

#include "lxa_test.h"

#include <algorithm>
#include <vector>

using namespace lxa::testing;

namespace {
constexpr int PEN_GREY = 0;
constexpr int PEN_BLACK = 1;
constexpr int PEN_WHITE = 2;
}

/* AmigaOS 3.1 EasyRequest layout (gallery-easyrequest reference): the
 * buttons are raised one-pixel frames, label width + 24 wide and font
 * height + 6 high, the first left aligned and the last right aligned with
 * the body text frame. Their positions are taken from the live gadget list
 * (screen coordinates); the system gadgets come first and are skipped. */
static std::vector<lxa_gadget_info_t> RequesterButtons(int window_index = 0)
{
    std::vector<lxa_gadget_info_t> result;
    int count = lxa_get_gadget_count(window_index);
    for (int i = 0; i < count; i++) {
        lxa_gadget_info_t g;
        if (lxa_get_gadget_info(window_index, i, &g) && !(g.gadget_type & 0x8000))
            result.push_back(g);
    }
    std::sort(result.begin(), result.end(),
              [](const lxa_gadget_info_t &x, const lxa_gadget_info_t &y) { return x.left < y.left; });
    return result;
}

/* ============================================================================
 * Behavioral tests — verify requester opens and responds to clicks
 * ============================================================================ */

class EasyRequestTest : public LxaUITest {
protected:
    void SetUp() override {
        LxaUITest::SetUp();
    }

    /**
     * Load the program and wait for the requester window to appear.
     * EasyRequest opens a window and blocks waiting for input.
     */
    bool LoadAndWaitForRequester() {
        if (lxa_load_program("SYS:EasyRequest", "") != 0)
            return false;

        /* The program calls EasyRequest() which opens a requester window. */
        if (!WaitForWindows(1, 10000))
            return false;

        /* Critical: let the task settle into WaitPort() inside SysReqHandler.
         * The Intuition input handler chain needs time to initialize. */
        WaitForEventLoop(100, 10000);
        /* the buttons are added once the requester body is drawn */
        for (int i = 0; i < 200 && RequesterButtons().size() < 3; i++)
            RunFrames(1);

        return true;
    }
};

TEST_F(EasyRequestTest, RequesterWindowOpens) {
    /* The main bug was that EasyRequest was a stub that just returned 1
     * without creating any UI. Verify a window actually opens. */
    ASSERT_TRUE(LoadAndWaitForRequester())
        << "EasyRequest should open a requester window";

    lxa_window_info_t info;
    ASSERT_TRUE(GetWindowInfo(0, &info));

    /* Window should have a reasonable size (not zero, not full screen) */
    EXPECT_GT(info.width, 100)
        << "Requester window should have reasonable width";
    EXPECT_GT(info.height, 40)
        << "Requester window should have reasonable height";
    EXPECT_LT(info.width, 600)
        << "Requester window should not be full screen width";
    EXPECT_LT(info.height, 200)
        << "Requester window should not be full screen height";
}

TEST_F(EasyRequestTest, ClickRightmostButton) {
    /* Clicking the rightmost button (No) should return 0 */
    ASSERT_TRUE(LoadAndWaitForRequester());

    lxa_window_info_t info;
    ASSERT_TRUE(GetWindowInfo(0, &info));

    auto buttons = RequesterButtons();
    ASSERT_EQ(buttons.size(), 3u);
    const auto &no = buttons.back();
    ClearOutput();
    Click(no.left + no.width / 2, no.top + no.height / 2);
    RunCyclesWithVBlank(40, 50000);

    /* Wait for program to exit after requester is dismissed */
    EXPECT_TRUE(lxa_wait_exit(10000))
        << "Program should exit after clicking No button";

    std::string output = GetOutput();
    EXPECT_NE(output.find("selected 'No'"), std::string::npos)
        << "Clicking rightmost button should select 'No' (return 0). Output: " << output;
}

TEST_F(EasyRequestTest, ClickLeftmostButton) {
    /* Clicking the leftmost button (Yes) should return 1 */
    ASSERT_TRUE(LoadAndWaitForRequester());

    lxa_window_info_t info;
    ASSERT_TRUE(GetWindowInfo(0, &info));

    ClearOutput();
    auto buttons = RequesterButtons();
    ASSERT_EQ(buttons.size(), 3u);
    const auto &yes = buttons.front();
    Click(yes.left + yes.width / 2, yes.top + yes.height / 2);
    RunCyclesWithVBlank(40, 50000);

    EXPECT_TRUE(lxa_wait_exit(10000))
        << "Program should exit after clicking Yes button";

    std::string output = GetOutput();
    EXPECT_NE(output.find("selected 'Yes'"), std::string::npos)
        << "Clicking leftmost button should select 'Yes' (return 1). Output: " << output;
}

TEST_F(EasyRequestTest, ProgramExitsCleanly) {
    /* Verify the program exits after any button click without crashes */
    ASSERT_TRUE(LoadAndWaitForRequester());

    lxa_window_info_t info;
    ASSERT_TRUE(GetWindowInfo(0, &info));

    /* Click any button to dismiss */
    auto buttons = RequesterButtons();
    ASSERT_FALSE(buttons.empty());
    Click(buttons.back().left + buttons.back().width / 2,
          buttons.back().top + buttons.back().height / 2);
    RunCyclesWithVBlank(40, 50000);

    EXPECT_TRUE(lxa_wait_exit(10000))
        << "Program should exit cleanly after requester is dismissed";
}

/* ============================================================================
 * Pixel verification tests — verify requester is visually rendered
 * ============================================================================ */

class EasyRequestPixelTest : public LxaUITest {
protected:
    void SetUp() override {
        /* Disable rootless so Intuition renders to screen bitmap */
        config.rootless = false;

        LxaUITest::SetUp();

        ASSERT_EQ(lxa_load_program("SYS:EasyRequest", ""), 0);
        ASSERT_TRUE(WaitForWindows(1, 10000));
        ASSERT_TRUE(GetWindowInfo(0, &window_info));

        /* Let rendering complete */
        WaitForEventLoop(100, 10000);
        RunCyclesWithVBlank(50, 100000);
    }
};

TEST_F(EasyRequestPixelTest, TitleBarVisible) {
    /* The requester window should have a title bar with text.
     * Title bar is in the top 11 pixels of the window. */
    lxa_flush_display();

    int title_pixels = CountContentPixels(
        window_info.x + 20,              /* skip left system gadgets */
        window_info.y + 1,               /* top of title bar */
        window_info.x + window_info.width - 20,  /* skip right system gadgets */
        window_info.y + 9,               /* bottom of title bar area */
        0  /* background pen */
    );
    EXPECT_GT(title_pixels, 10)
        << "Title bar should have visible text pixels";
}

TEST_F(EasyRequestPixelTest, BodyTextVisible) {
    /* The body text area should contain rendered text.
     * Body text is below the title bar and above the gadget row. */
    lxa_flush_display();

    /* Body text starts after border_top(11) + text_margin(16) */
    int body_pixels = CountContentPixels(
        window_info.x + 20,              /* margin */
        window_info.y + 27,              /* border_top + text_margin */
        window_info.x + window_info.width - 20,
        window_info.y + window_info.height - 30,  /* above gadget row */
        0  /* background pen */
    );
    EXPECT_GT(body_pixels, 20)
        << "Body text area should have visible text pixels (3 lines of text)";
}

TEST_F(EasyRequestPixelTest, GadgetButtonsVisible) {
    /* The gadget row near the bottom should have visible button borders/text */
    lxa_flush_display();
    auto buttons = RequesterButtons();
    ASSERT_EQ(buttons.size(), 3u);

    int gadget_pixels = CountContentPixels(
        buttons.front().left,
        buttons.front().top,
        buttons.back().left + buttons.back().width - 1,
        buttons.back().top + buttons.back().height - 1,
        0  /* background pen */
    );
    EXPECT_GT(gadget_pixels, 15)
        << "Gadget row should have visible button borders and text";
}

TEST_F(EasyRequestPixelTest, YesButtonFrameMatchesReference) {
    /* AmigaOS 3.1: thin raised frame without corner pixels (shine top row
     * and left column, shadow bottom row and right column), BACKGROUNDPEN
     * interior, label width + 24 wide, font height + 6 high */
    lxa_flush_display();
    auto buttons = RequesterButtons();
    ASSERT_EQ(buttons.size(), 3u);
    const auto &g = buttons.front();

    EXPECT_EQ(g.width, 3 * 8 + 24) << "'Yes' button width";
    EXPECT_EQ(g.height, 8 + 6) << "button height";
    EXPECT_EQ(ReadPixel(g.left + 1, g.top), PEN_WHITE);
    EXPECT_EQ(ReadPixel(g.left, g.top + 1), PEN_WHITE);
    EXPECT_EQ(ReadPixel(g.left + g.width - 1, g.top + 1), PEN_BLACK);
    EXPECT_EQ(ReadPixel(g.left + 1, g.top + g.height - 1), PEN_BLACK);
    EXPECT_NE(ReadPixel(g.left + g.width - 1, g.top), PEN_BLACK)
        << "top-right corner is not part of the frame";
    EXPECT_EQ(ReadPixel(g.left + 2, g.top + 2), PEN_GREY)
        << "button interior is BACKGROUNDPEN";
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
