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

using namespace lxa::testing;

/* Layout constants matching BuildEasyRequestArgs calculation:
 * border_left=4, border_top=11, text_margin=16
 * body: 3 lines * (8+2)px = 30px
 * gad_row_y = 11 + 16 + 30 + 16 = 73
 * gad_height = 16 (8px font + 4px padding top + 4px padding bottom)
 * gad_spacing = 8
 * 3 gadgets: Yes(60px) | 3125794(72px) | No(60px)
 * total_gad_width = 60 + 72 + 60 + 2*8 = 208
 * content_w = max(35*8+32, 208+24) = max(312, 232) = 312
 * gad_start_x = 4 + (312 - 208)/2 = 56
 */
constexpr int GAD_ROW_Y = 73;
constexpr int GAD_HEIGHT = 16;
constexpr int GAD_START_X = 56;
/* Yes button: x=56, w=60 -> center_x=86 */
constexpr int YES_CENTER_X = 86;
/* Middle button: x=56+60+8=124, w=72 -> center_x=160 */
constexpr int MID_CENTER_X = 160;
/* No button: x=124+72+8=204, w=60 -> center_x=234 */
constexpr int NO_CENTER_X = 234;
/* Gadget center Y */
constexpr int GAD_CENTER_Y = GAD_ROW_Y + GAD_HEIGHT / 2;

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
        WaitForEventLoop(200, 10000);

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

    ClearOutput();
    Click(info.x + NO_CENTER_X, info.y + GAD_CENTER_Y);
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
    Click(info.x + YES_CENTER_X, info.y + GAD_CENTER_Y);
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
    Click(info.x + NO_CENTER_X, info.y + GAD_CENTER_Y);
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
        WaitForEventLoop(200, 10000);
        RunCyclesWithVBlank(30, 200000);
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

    int gadget_pixels = CountContentPixels(
        window_info.x + GAD_START_X,
        window_info.y + GAD_ROW_Y,
        window_info.x + GAD_START_X + 208,  /* total gadget row width */
        window_info.y + GAD_ROW_Y + GAD_HEIGHT,
        0  /* background pen */
    );
    EXPECT_GT(gadget_pixels, 15)
        << "Gadget row should have visible button borders and text";
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
