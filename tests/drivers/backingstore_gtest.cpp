/**
 * backingstore_gtest.cpp - Phase 132 SMART_REFRESH backing-store regression
 *
 * Drives the BackingStoreTest sample through three deterministic stages:
 *
 *   Stage 0: main window open, four-rect colour pattern drawn
 *   Stage 1: dialog open on top, partially obscuring two of the rects
 *   Stage 2: dialog closed; backing store must restore the obscured area
 *
 * The dialog window is at window-local (90,25), size 120x60, so the area
 * (90..209, 25..84) of the main window's interior is obscured during
 * Stage 1.  We pick three probe points whose expected colour differs at
 * each stage:
 *
 *   probe A: window-local (50, 45)  -> always pen 1 (never obscured)
 *   probe B: window-local (130, 45) -> pen 2 (stage 0,2), pen 5 (stage 1)
 *   probe C: window-local (40, 90)  -> always pen 3 (never obscured)
 *
 * After dialog close (stage 2), probe B must read pen 2 again.  A failure
 * here means backing store either did not save the screen pixels on
 * obscure or did not restore them on uncover — the dialog-stamping
 * regression Phase 132 explicitly exists to catch.
 */

#include "lxa_test.h"

#include <cstdint>

using namespace lxa::testing;

namespace {

constexpr int PEN_BG    = 0;
constexpr int PEN_1     = 1;
constexpr int PEN_2     = 2;
constexpr int PEN_3     = 3;
constexpr int PEN_DLG   = 5;

/* Probe points, window-local */
constexpr int A_X = 50,  A_Y = 45;   /* always pen 1 */
constexpr int B_X = 130, B_Y = 45;   /* pen 2 / pen 5 */
constexpr int C_X = 40,  C_Y = 90;   /* always pen 3 */

}  // namespace

class BackingStoreTest : public LxaUITest {
protected:
    void SetUp() override {
        LxaUITest::SetUp();

        ASSERT_EQ(lxa_load_program("SYS:BackingStoreTest", ""), 0);
        ASSERT_TRUE(WaitForWindows(1, 10000));
        ASSERT_TRUE(GetWindowInfo(0, &main_info_));

        /* Let the app draw the pattern and reach WaitPort(). */
        WaitForEventLoop(100, 50000);
        RunCyclesWithVBlank(20, 50000);
        lxa_flush_display();
    }

    /* Send a vanilla-key press to the active window so the app's
     * wait_for_signal() returns. */
    void AdvanceStage() {
        TypeString(" ");
        RunCyclesWithVBlank(40, 50000);
        lxa_flush_display();
    }

    int ReadAt(const lxa_window_info_t& w, int local_x, int local_y) {
        return ReadPixel(w.x + local_x, w.y + local_y);
    }

    lxa_window_info_t main_info_;
};

/* Stage 0: pattern is drawn correctly. */
TEST_F(BackingStoreTest, Stage0PatternRenders) {
    EXPECT_EQ(ReadAt(main_info_, A_X, A_Y), PEN_1)
        << "pen 1 rect should render at (50,45)";
    EXPECT_EQ(ReadAt(main_info_, B_X, B_Y), PEN_2)
        << "pen 2 rect should render at (130,45)";
    EXPECT_EQ(ReadAt(main_info_, C_X, C_Y), PEN_3)
        << "pen 3 rect should render at (40,90)";
}

/* Stage 1: dialog opens, obscuring probe B; A and C remain visible. */
TEST_F(BackingStoreTest, Stage1DialogObscuresPattern) {
    /* Verify stage 0 first. */
    ASSERT_EQ(ReadAt(main_info_, B_X, B_Y), PEN_2);

    AdvanceStage();
    ASSERT_TRUE(WaitForWindows(2, 10000))
        << "dialog window should open after first signal";

    lxa_window_info_t dlg;
    ASSERT_TRUE(GetWindowInfo(1, &dlg));

    /* Dialog interior is filled with pen 5; probe B must now read pen 5
     * (or at least NOT pen 2 — it might fall on the dialog border). */
    int b_pixel = ReadAt(main_info_, B_X, B_Y);
    EXPECT_NE(b_pixel, PEN_2)
        << "dialog should obscure pen 2 rect; still saw pen 2 at (130,45)";

    /* A and C remain visible (outside dialog area). */
    EXPECT_EQ(ReadAt(main_info_, A_X, A_Y), PEN_1);
    EXPECT_EQ(ReadAt(main_info_, C_X, C_Y), PEN_3);
}

/* Stage 2: dialog closes; backing store restores obscured area. */
TEST_F(BackingStoreTest, Stage2BackingStoreRestoresPattern) {
    /* Stage 0 -> 1 */
    AdvanceStage();
    ASSERT_TRUE(WaitForWindows(2, 10000));

    /* Stage 1 -> 2: signal the dialog window to close.
     * Click anywhere inside the dialog so the click message goes to dlg. */
    lxa_window_info_t dlg;
    ASSERT_TRUE(GetWindowInfo(1, &dlg));
    Click(dlg.x + dlg.width / 2, dlg.y + dlg.height / 2);
    RunCyclesWithVBlank(40, 50000);

    /* App is single-stage advance per signal, but the dialog only listens
     * for input via its own port. Wait for it to close. */
    /* Wait for dialog to close (window count drops from 2 to 1).
     * If the app rushes to exit (count -> 0) we still want to inspect
     * pixels: the main-window framebuffer survives until SDL teardown. */
    bool dialog_closed = false;
    for (int i = 0; i < 100 && !dialog_closed; i++) {
        RunCyclesWithVBlank(10, 50000);
        if (lxa_get_window_count() <= 1) {
            dialog_closed = true;
        }
    }
    lxa_flush_display();
    ASSERT_TRUE(dialog_closed) << "dialog did not close after click";

    /* The whole point of the test: probe B must read pen 2 again,
     * proving the backing store restored the obscured pixels. */
    EXPECT_EQ(ReadAt(main_info_, B_X, B_Y), PEN_2)
        << "SMART_REFRESH backing store should restore pen 2 at (130,45) "
           "after dialog close";

    /* Sanity: A and C unchanged. */
    EXPECT_EQ(ReadAt(main_info_, A_X, A_Y), PEN_1);
    EXPECT_EQ(ReadAt(main_info_, C_X, C_Y), PEN_3);
}

/* Stage 2 sweep: read multiple pixels in the formerly-obscured area to
 * make sure the entire restored region is correct, not just one probe. */
TEST_F(BackingStoreTest, Stage2RestoredAreaSweep) {
    AdvanceStage();
    ASSERT_TRUE(WaitForWindows(2, 10000));

    lxa_window_info_t dlg;
    ASSERT_TRUE(GetWindowInfo(1, &dlg));
    Click(dlg.x + dlg.width / 2, dlg.y + dlg.height / 2);
    RunCyclesWithVBlank(40, 50000);

    bool dialog_closed = false;
    for (int i = 0; i < 100 && !dialog_closed; i++) {
        RunCyclesWithVBlank(10, 50000);
        if (lxa_get_window_count() <= 1) { dialog_closed = true; }
    }
    lxa_flush_display();
    ASSERT_TRUE(dialog_closed);

    /* Sample several points inside the previously-obscured area:
     * pen 2 rect spans (100,30)-(160,60); the dialog was at (90,25)-
     * (209,84) (window-local).  So x in 100..160, y in 30..60 should
     * all read pen 2. */
    int pen2_hits = 0;
    int pen0_hits = 0;
    int other_hits = 0;
    for (int y = 32; y <= 58; y += 4) {
        for (int x = 102; x <= 158; x += 8) {
            int p = ReadAt(main_info_, x, y);
            if (p == PEN_2)      pen2_hits++;
            else if (p == PEN_BG) pen0_hits++;
            else                  other_hits++;
        }
    }
    EXPECT_GT(pen2_hits, 30) << "most sampled pixels in pen-2 rect should "
                                "have been restored to pen 2";
    EXPECT_EQ(other_hits, 0) << "no garbage pixels expected in the restored "
                                "area (no pen 5, no border colours)";
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
