/**
 * propgadget_chrome_gtest.cpp - Phase 152 prop-gadget track-frame regression
 *
 * Verifies that PROPGADGETs render with a recessed 3D track frame
 * (shadow pen 1 on top/left, shine pen 2 on bottom/right) on the outer
 * perimeter, instead of just rendering the knob alone.
 *
 * Sample: SYS:PropGad opens a window with one vertical (FREEVERT) and
 * one horizontal (FREEHORIZ) PROPGADGET, both with PROPNEWLOOK and
 * AUTOKNOB flags and a partial body (knob covers ~25% of track) so the
 * track strip is exposed alongside the knob.
 *
 * Phase 152 sub-problem checklist:
 *   - Recessed track frame around the outer perimeter
 *   - Honour FREEHORIZ vs FREEVERT (no flag actually changes the frame —
 *     both orientations render the same chrome — but the test exercises
 *     both)
 *   - Honour PROPBORDERLESS (covered indirectly: the sample does NOT set
 *     it, so we expect the frame to appear)
 *   - The frame edges must be recessed: top/left = SHADOWPEN (pen 1),
 *     bottom/right = SHINEPEN (pen 2)
 */

#include "lxa_test.h"

using namespace lxa::testing;

namespace {

constexpr int kPenShadow = 1;
constexpr int kPenShine  = 2;

class PropGadgetChromeTest : public LxaUITest {
protected:
    void SetUp() override {
        LxaUITest::SetUp();
        ASSERT_EQ(lxa_load_program("SYS:PropGadget", ""), 0);
        ASSERT_TRUE(WaitForWindows(1, 10000));
        ASSERT_TRUE(WaitForWindowDrawn(0, 5000));
        WaitForEventLoop(50, 50000);
        RunCyclesWithVBlank(20, 50000);
        lxa_flush_display();
    }

    /* Find the prop gadget with the matching gadget_id (1=vertical, 2=horizontal). */
    bool FindPropGadget(uint16_t gadget_id, lxa_gadget_info_t* out) {
        int count = lxa_get_gadget_count(0);
        for (int i = 0; i < count; ++i) {
            lxa_gadget_info_t g;
            if (!lxa_get_gadget_info(0, i, &g)) continue;
            if (g.gadget_id == gadget_id) {
                *out = g;
                return true;
            }
        }
        return false;
    }

    /* Count occurrences of a specific pen along a horizontal line. */
    int CountPenAlongRow(int x0, int x1, int y, int pen) const {
        int count = 0;
        for (int x = x0; x <= x1; ++x) {
            int p = -1;
            if (lxa_read_pixel(x, y, &p) && p == pen) ++count;
        }
        return count;
    }

    /* Count occurrences of a specific pen along a vertical line. */
    int CountPenAlongCol(int x, int y0, int y1, int pen) const {
        int count = 0;
        for (int y = y0; y <= y1; ++y) {
            int p = -1;
            if (lxa_read_pixel(x, y, &p) && p == pen) ++count;
        }
        return count;
    }
};

/* ---- Vertical prop gadget (FREEVERT) ---- */

TEST_F(PropGadgetChromeTest, VerticalPropTopEdgeIsShadow) {
    lxa_gadget_info_t g;
    ASSERT_TRUE(FindPropGadget(1, &g));

    /* Top edge: y = g.top, x ∈ [g.left, g.left+g.width-1] should be all shadow. */
    int x0 = g.left;
    int x1 = g.left + g.width - 1;
    int y  = g.top;
    int shadow_pixels = CountPenAlongRow(x0, x1, y, kPenShadow);
    EXPECT_GE(shadow_pixels, g.width - 1)
        << "Vertical prop top edge should be shadow pen (track frame). "
        << "shadow_count=" << shadow_pixels << " expected≥" << (g.width - 1);
}

TEST_F(PropGadgetChromeTest, VerticalPropLeftEdgeIsShadow) {
    lxa_gadget_info_t g;
    ASSERT_TRUE(FindPropGadget(1, &g));

    int x  = g.left;
    int y0 = g.top;
    int y1 = g.top + g.height - 1;
    int shadow_pixels = CountPenAlongCol(x, y0, y1, kPenShadow);
    EXPECT_GE(shadow_pixels, g.height - 1)
        << "Vertical prop left edge should be shadow pen (track frame). "
        << "shadow_count=" << shadow_pixels << " expected≥" << (g.height - 1);
}

TEST_F(PropGadgetChromeTest, VerticalPropBottomEdgeIsShine) {
    lxa_gadget_info_t g;
    ASSERT_TRUE(FindPropGadget(1, &g));

    int x0 = g.left;
    int x1 = g.left + g.width - 1;
    int y  = g.top + g.height - 1;
    int shine_pixels = CountPenAlongRow(x0, x1, y, kPenShine);
    EXPECT_GE(shine_pixels, g.width - 1)
        << "Vertical prop bottom edge should be shine pen (track frame). "
        << "shine_count=" << shine_pixels << " expected≥" << (g.width - 1);
}

TEST_F(PropGadgetChromeTest, VerticalPropRightEdgeIsShine) {
    lxa_gadget_info_t g;
    ASSERT_TRUE(FindPropGadget(1, &g));

    int x  = g.left + g.width - 1;
    int y0 = g.top;
    int y1 = g.top + g.height - 1;
    int shine_pixels = CountPenAlongCol(x, y0, y1, kPenShine);
    EXPECT_GE(shine_pixels, g.height - 1)
        << "Vertical prop right edge should be shine pen (track frame). "
        << "shine_count=" << shine_pixels << " expected≥" << (g.height - 1);
}

/* ---- Horizontal prop gadget (FREEHORIZ) ---- */

TEST_F(PropGadgetChromeTest, HorizontalPropTopEdgeIsShadow) {
    lxa_gadget_info_t g;
    ASSERT_TRUE(FindPropGadget(2, &g));

    int x0 = g.left;
    int x1 = g.left + g.width - 1;
    int y  = g.top;
    int shadow_pixels = CountPenAlongRow(x0, x1, y, kPenShadow);
    EXPECT_GE(shadow_pixels, g.width - 1)
        << "Horizontal prop top edge should be shadow pen. "
        << "shadow_count=" << shadow_pixels;
}

TEST_F(PropGadgetChromeTest, HorizontalPropBottomEdgeIsShine) {
    lxa_gadget_info_t g;
    ASSERT_TRUE(FindPropGadget(2, &g));

    int x0 = g.left;
    int x1 = g.left + g.width - 1;
    int y  = g.top + g.height - 1;
    int shine_pixels = CountPenAlongRow(x0, x1, y, kPenShine);
    EXPECT_GE(shine_pixels, g.width - 1)
        << "Horizontal prop bottom edge should be shine pen. "
        << "shine_count=" << shine_pixels;
}

/* ---- Knob still renders inside the framed container ---- */

TEST_F(PropGadgetChromeTest, VerticalPropKnobStillRenders) {
    lxa_gadget_info_t g;
    ASSERT_TRUE(FindPropGadget(1, &g));

    /* Inside the inset container (skip the 1-pixel frame), there must be at
     * least some SHINEPEN pixels (the knob fill is pen 2). */
    int knob_pixels = 0;
    for (int y = g.top + 1; y < g.top + g.height - 1; ++y) {
        for (int x = g.left + 1; x < g.left + g.width - 1; ++x) {
            int p = -1;
            if (lxa_read_pixel(x, y, &p) && p == kPenShine) ++knob_pixels;
        }
    }
    EXPECT_GT(knob_pixels, 4)
        << "Vertical prop interior should contain knob (SHINEPEN) pixels; "
           "found " << knob_pixels;
}

}  // namespace

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
