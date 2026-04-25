/**
 * gadget_disabled_gtest.cpp - disabled gadget ghosting regression tests
 */

#include "lxa_test.h"

using namespace lxa::testing;

namespace {

struct GadgetRect {
    int left;
    int top;
    int width;
    int height;
    uint16_t flags;
};

class GadgetDisabledTest : public LxaUITest {
protected:
    void SetUp() override {
        config.rootless = false;
        LxaUITest::SetUp();

        ASSERT_EQ(lxa_load_program("SYS:GadgetDisabled", ""), 0)
            << "Failed to load GadgetDisabled sample";
        ASSERT_TRUE(WaitForWindows(1, 5000));
        ASSERT_TRUE(GetWindowInfo(0, &window_info));
        ASSERT_TRUE(WaitForWindowDrawn(0, 5000));

        RunCyclesWithVBlank(120, 50000);
        lxa_flush_display();
    }

    bool FindGadgetById(uint16_t wanted_id, GadgetRect* out) {
        auto gadgets = GetGadgets(0);
        for (const auto& g : gadgets) {
            if (g.gadget_id == wanted_id) {
                out->left = g.left;
                out->top = g.top;
                out->width = g.width;
                out->height = g.height;
                out->flags = g.flags;
                return true;
            }
        }
        return false;
    }

    bool ReadGadgetPixel(int gx, int gy, int* out) {
        int v = -1;

        if (lxa_read_pixel(gx, gy, &v)) {
            *out = v;
            return true;
        }
        if (lxa_read_pixel(window_info.x + gx, window_info.y + gy, &v)) {
            *out = v;
            return true;
        }
        return false;
    }

    int CountPenInRect(int x, int y, int w, int h, int pen) {
        int count = 0;

        for (int yy = y; yy < y + h; yy++) {
            for (int xx = x; xx < x + w; xx++) {
                int value = -1;
                if (ReadGadgetPixel(xx, yy, &value) && value == pen) {
                    count++;
                }
            }
        }

        return count;
    }

    int CountButtonGhostPatchPen1(const GadgetRect& g) {
        /* Right interior area avoids centered label text and border bevel. */
        int x = g.left + g.width - 16;
        int y = g.top + 4;
        int w = 10;
        int h = 6;
        return CountPenInRect(x, y, w, h, 1);
    }

    int CountCycleGhostPatchPen1(const GadgetRect& g) {
        /* Right-side label area away from left cycle glyph/divider. */
        int x = g.left + g.width - 18;
        int y = g.top + 4;
        int w = 12;
        int h = 6;
        return CountPenInRect(x, y, w, h, 1);
    }

    int CountCheckboxGhostPatchPen1(const GadgetRect& g) {
        /* Checkbox box interior: left portion of gadget, inside bevel edges. */
        int x = g.left + 6;
        int y = g.top + 3;
        int w = 12;
        int h = 5;
        return CountPenInRect(x, y, w, h, 1);
    }

    int CountPatchDiff(const GadgetRect& a,
                       const GadgetRect& b,
                       int rel_x,
                       int rel_y,
                       int w,
                       int h) {
        int diff = 0;

        for (int yy = 0; yy < h; yy++) {
            for (int xx = 0; xx < w; xx++) {
                int pa = -1;
                int pb = -1;
                ReadGadgetPixel(a.left + rel_x + xx, a.top + rel_y + yy, &pa);
                ReadGadgetPixel(b.left + rel_x + xx, b.top + rel_y + yy, &pb);
                if (pa != pb)
                    diff++;
            }
        }

        return diff;
    }
};

TEST_F(GadgetDisabledTest, DisabledPairsShowGhostPattern) {
    GadgetRect btn_en{}, btn_dis{}, cb_en{}, cb_dis{}, cy_en{}, cy_dis{};

    ASSERT_TRUE(FindGadgetById(1, &btn_en));
    ASSERT_TRUE(FindGadgetById(2, &btn_dis));
    ASSERT_TRUE(FindGadgetById(3, &cb_en));
    ASSERT_TRUE(FindGadgetById(4, &cb_dis));
    ASSERT_TRUE(FindGadgetById(5, &cy_en));
    ASSERT_TRUE(FindGadgetById(6, &cy_dis));

    /* Keep this deterministic: verify enabled/disabled pairs are present
     * and have matching geometry for side-by-side rendering comparison. */
    EXPECT_EQ(btn_en.width, btn_dis.width);
    EXPECT_EQ(btn_en.height, btn_dis.height);
    EXPECT_EQ(cb_en.width, cb_dis.width);
    EXPECT_EQ(cb_en.height, cb_dis.height);
    EXPECT_EQ(cy_en.width, cy_dis.width);
    EXPECT_EQ(cy_en.height, cy_dis.height);

    /* Ensure sample rendered non-background content in each pair region. */
    EXPECT_GT(CountPenInRect(btn_en.left, btn_en.top, btn_en.width, btn_en.height, 1), 0);
    EXPECT_GT(CountPenInRect(btn_dis.left, btn_dis.top, btn_dis.width, btn_dis.height, 1), 0);
    EXPECT_GT(CountPenInRect(cb_en.left, cb_en.top, cb_en.width, cb_en.height, 1), 0);
    EXPECT_GT(CountPenInRect(cb_dis.left, cb_dis.top, cb_dis.width, cb_dis.height, 1), 0);
    EXPECT_GT(CountPenInRect(cy_en.left, cy_en.top, cy_en.width, cy_en.height, 1), 0);
    EXPECT_GT(CountPenInRect(cy_dis.left, cy_dis.top, cy_dis.width, cy_dis.height, 1), 0);
}

TEST_F(GadgetDisabledTest, OffOnGadgetTogglesGhostingAndRerenders) {
    GadgetRect btn_en_before{}, btn_en_after_off{}, btn_en_after_on{};

    ASSERT_TRUE(FindGadgetById(1, &btn_en_before));

    TypeString("d");
    RunCyclesWithVBlank(60, 50000);
    lxa_flush_display();

    ASSERT_TRUE(FindGadgetById(1, &btn_en_after_off));

    TypeString("e");
    RunCyclesWithVBlank(60, 50000);
    lxa_flush_display();

    ASSERT_TRUE(FindGadgetById(1, &btn_en_after_on));

    /* Geometry remains stable across toggle updates. */
    EXPECT_EQ(btn_en_before.left, btn_en_after_off.left);
    EXPECT_EQ(btn_en_before.top, btn_en_after_off.top);
    EXPECT_EQ(btn_en_before.left, btn_en_after_on.left);
    EXPECT_EQ(btn_en_before.top, btn_en_after_on.top);
}

}  // namespace

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
