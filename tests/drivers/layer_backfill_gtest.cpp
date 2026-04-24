/**
 * layer_backfill_gtest.cpp - Phase 150 layer creation backfill regression
 *
 * Verifies that CreateLayerInternal() applies the BackFill hook (default:
 * clear to pen 0) for every ClipRect of the newly created layer, so that a
 * fresh window starts with a clean background regardless of what was
 * previously rendered at those screen coordinates.
 *
 * Two test scenarios:
 *
 *   Scenario A (overlapping windows):
 *     - Open Window A, fill interior with pen 2.
 *     - Open Window B fully overlapping A.
 *     - Assert Window B's interior is pen 0, not pen 2.
 *
 *   Scenario B (sequential windows at same coordinates — the DPaint scenario):
 *     - Open Window A, fill with pen 3, close it.
 *     - Open Window B at the same coordinates.
 *     - Assert Window B's interior is pen 0, not pen 3 ghost pixels.
 */

#include "lxa_test.h"

using namespace lxa::testing;

/* =========================================================================
 * Scenario A — overlapping windows
 * ========================================================================= */

class LayerBackFillScenarioA : public LxaUITest {
protected:
    void SetUp() override {
        LxaUITest::SetUp();

        ASSERT_EQ(lxa_load_program("SYS:LayerBackFillTest", ""), 0);

        /* Stage a0: Window A open with pen 2 content */
        ASSERT_TRUE(WaitForWindows(1, 10000));
        ASSERT_TRUE(GetWindowInfo(0, &win_a_));

        WaitForEventLoop(100, 50000);
        RunCyclesWithVBlank(20, 50000);
        lxa_flush_display();
    }

    lxa_window_info_t win_a_{};
};

/* Window A must actually contain pen 2 pixels before Window B opens. */
TEST_F(LayerBackFillScenarioA, WindowAFilledWithPen2) {
    /* Sample a point in Window A's interior (offset from top-left corner,
     * well inside the border). */
    int pen = ReadPixel(win_a_.x + win_a_.width / 2, win_a_.y + win_a_.height / 2);
    EXPECT_EQ(pen, 2)
        << "Window A interior should be pen 2 before Window B opens";
}

/* After Window B is opened on top, its interior must be pen 0 (backfill). */
TEST_F(LayerBackFillScenarioA, WindowBInteriorIsClean) {
    /* Advance to stage a1: open Window B */
    TypeString(" ");
    ASSERT_TRUE(WaitForWindows(2, 5000));

    /* Window B was opened second — it is at lxa window index 1. */
    lxa_window_info_t win_b{};
    ASSERT_TRUE(GetWindowInfo(1, &win_b));   /* index 1 = second window = B */

    RunCyclesWithVBlank(20, 50000);
    lxa_flush_display();

    /* Select Window B's pixel buffer for reading (rootless mode: each window
     * has its own buffer; index 1 = second window = Window B). */
    SetActiveWindow(1);

    /* Interior centre of Window B must be pen 0, not pen 2 from Window A. */
    int cx = win_b.x + win_b.width  / 2;
    int cy = win_b.y + win_b.height / 2;
    int pen = ReadPixel(cx, cy);
    EXPECT_EQ(pen, 0)
        << "Window B interior must be pen 0 (backfill cleared it), "
           "not pen 2 inherited from Window A underneath";
}

/* =========================================================================
 * Scenario B — sequential windows at same coordinates (DPaint scenario)
 * ========================================================================= */

class LayerBackFillScenarioB : public LxaUITest {
protected:
    void SetUp() override {
        LxaUITest::SetUp();

        ASSERT_EQ(lxa_load_program("SYS:LayerBackFillTest", "b"), 0);

        /* Stage b0: Window A open with pen 3 content */
        ASSERT_TRUE(WaitForWindows(1, 10000));
        ASSERT_TRUE(GetWindowInfo(0, &win_a_));

        WaitForEventLoop(100, 50000);
        RunCyclesWithVBlank(20, 50000);
        lxa_flush_display();
    }

    lxa_window_info_t win_a_{};
};

/* Window A must have pen 3 pixels while open — confirms the scenario is set up
 * correctly (if this fails, the test itself has a bug). */
TEST_F(LayerBackFillScenarioB, WindowAFilledWithPen3) {
    int pen = ReadPixel(win_a_.x + win_a_.width / 2, win_a_.y + win_a_.height / 2);
    EXPECT_EQ(pen, 3)
        << "Window A interior should be pen 3 before closing";
}

/* After Window A is closed and Window B opens at the same position,
 * Window B's interior must be pen 0 — the backfill must have cleared
 * whatever was on screen from Window A. */
TEST_F(LayerBackFillScenarioB, WindowBInteriorIsCleanAfterWindowAClosed) {
    /* Advance to stage b1: close Window A */
    TypeString(" ");
    ASSERT_TRUE(WaitForWindows(0, 5000));  /* window count drops to 0 */

    RunCyclesWithVBlank(10, 50000);

    /* Advance to stage b2: Window B opens */
    RunCyclesWithVBlank(10, 50000);
    ASSERT_TRUE(WaitForWindows(1, 5000));

    lxa_window_info_t win_b{};
    ASSERT_TRUE(GetWindowInfo(0, &win_b));

    RunCyclesWithVBlank(20, 50000);
    lxa_flush_display();

    /* Select Window B's pixel buffer (rootless: index 0 = frontmost). */
    SetActiveWindow(0);

    int cx = win_b.x + win_b.width  / 2;
    int cy = win_b.y + win_b.height / 2;
    int pen = ReadPixel(cx, cy);
    EXPECT_EQ(pen, 0)
        << "Window B interior must be pen 0 (backfill); "
           "pen 3 ghost pixels from Window A must not be visible";
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
