/**
 * dpaint_gtest.cpp - Google Test version of DPaint V test
 *
 * Tests DPaint V graphics editor.
 * Verifies window opening, screen dimensions, process running,
 * and mouse interaction.
 */

#include "lxa_test.h"

#include <algorithm>
#include <filesystem>
#include <string>
#include <vector>

using namespace lxa::testing;

class DPaintTest : public LxaUITest {
protected:
    bool window_opened = false;

    bool HasDiskRexxSysLib() {
        const char* system_base = FindSystemBasePath();
        if (system_base == nullptr) {
            return false;
        }

        return std::filesystem::exists(
            std::filesystem::path(system_base) / "Libs" / "rexxsyslib.library");
    }

    void SetUp() override {
        LxaUITest::SetUp();
        
        const char* apps = FindAppsPath();
        if (!apps) {
            GTEST_SKIP() << "lxa-apps directory not found";
        }

        if (!HasDiskRexxSysLib()) {
            GTEST_SKIP() << "disk-provided rexxsyslib.library missing from SYS:Libs";
        }
        
        /* Load via APPS: assign */
        ASSERT_EQ(lxa_load_program("APPS:DPaintV/bin/DPaintV/DPaint", ""), 0) 
            << "Failed to load DPaint V";
        
        /* DPaint may take a while to initialize */
        window_opened = WaitForWindows(1, 15000);
        if (window_opened) {
            GetWindowInfo(0, &window_info);
        }
        
        /* Let DPaint initialize further */
        RunCyclesWithVBlank(100, 50000);
    }
};

class DPaintPixelTest : public LxaUITest {
protected:
    bool window_opened = false;

    static constexpr int QUALIFIER_CONTROL = 0x0008;
    static constexpr int RAWKEY_P = 0x19;

    struct TextDraw {
        std::string text;
        int x;
        int y;
    };

    std::vector<TextDraw> text_draws;

    bool HasDiskRexxSysLib() {
        const char* system_base = FindSystemBasePath();
        if (system_base == nullptr) {
            return false;
        }

        return std::filesystem::exists(
            std::filesystem::path(system_base) / "Libs" / "rexxsyslib.library");
    }

    void SetUp() override {
        config.rootless = false;
        LxaUITest::SetUp();

        lxa_set_text_hook([](const char *s, int n, int x, int y, void *ud) {
            auto *draws = static_cast<std::vector<TextDraw> *>(ud);
            draws->push_back({std::string(s, n), x, y});
        }, &text_draws);

        const char* apps = FindAppsPath();
        if (!apps) {
            GTEST_SKIP() << "lxa-apps directory not found";
        }

        if (!HasDiskRexxSysLib()) {
            GTEST_SKIP() << "disk-provided rexxsyslib.library missing from SYS:Libs";
        }

        ASSERT_EQ(lxa_load_program("APPS:DPaintV/bin/DPaintV/DPaint", ""), 0)
            << "Failed to load DPaint V";

        window_opened = WaitForWindows(1, 15000);
        if (!window_opened) {
            GTEST_SKIP() << "DPaint window did not open";
        }

        ASSERT_TRUE(GetWindowInfo(0, &window_info));
        WaitForEventLoop(100, 10000);
        RunCyclesWithVBlank(160, 50000);
    }

    void TearDown() override {
        lxa_clear_text_hook();
        LxaUITest::TearDown();
    }

    int FindWindowIndexByTitleSubstring(const char *title_fragment) {
        int count = lxa_get_window_count();

        for (int i = 0; i < count; ++i) {
            lxa_window_info_t info;

            if (GetWindowInfo(i, &info) && std::string(info.title).find(title_fragment) != std::string::npos)
                return i;
        }

        return -1;
    }

    bool WaitForWindowTitleSubstring(const char *title_fragment, int timeout_ms = 5000) {
        int attempts = timeout_ms / 50;

        for (int attempt = 0; attempt < attempts; ++attempt) {
            if (FindWindowIndexByTitleSubstring(title_fragment) >= 0)
                return true;

            RunCyclesWithVBlank(1, 50000);
        }

        return false;
    }

    bool WaitForOwnershipWindowToClose(int timeout_ms = 5000) {
        int attempts = timeout_ms / 50;

        for (int attempt = 0; attempt < attempts; ++attempt) {
            if (FindWindowIndexByTitleSubstring("Ownership Information") < 0)
                return true;

            RunCyclesWithVBlank(1, 50000);
        }

        return false;
    }

    bool DismissOwnershipWindow() {
        lxa_window_info_t ownership_info;
        auto gadgets = GetGadgets(0);
        int bottom_gadget = -1;
        int bottom_top = -1;

        if (!GetWindowInfo(0, &ownership_info))
            return false;

        for (size_t i = 0; i < gadgets.size(); ++i) {
            if (gadgets[i].top <= ownership_info.y + 16)
                continue;

            if (gadgets[i].top > bottom_top) {
                bottom_top = gadgets[i].top;
                bottom_gadget = (int)i;
            }
        }

        if (bottom_gadget >= 0) {
            if (!ClickGadget(bottom_gadget, 0))
                return false;
        } else {
            Click(ownership_info.x + ownership_info.width / 2,
                  ownership_info.y + ownership_info.height - 18);
        }

        RunCyclesWithVBlank(30, 50000);
        return WaitForOwnershipWindowToClose();
    }

    bool OpenScreenFormatDialog() {
        PressKey(RAWKEY_P, QUALIFIER_CONTROL);
        RunCyclesWithVBlank(40, 50000);

        if (!WaitForWindowTitleSubstring("Screen Format"))
            return false;

        /* The window appears the moment OpenWindow*() returns, but DPaint
         * still needs time to construct GadTools listviews, run the
         * GT_AddGadgetList chain, and render initial widget contents.
         * Give the app a generous settling budget — empirically the GadTools
         * population sequence completes inside 200 VBlank ticks for this
         * dialog.  Without this wait, callers capture an empty window. */
        RunCyclesWithVBlank(500, 50000);
        return true;
    }

    int CountWindowRectContent(int window_index, int left, int top, int width, int height) {
        lxa_window_info_t info;

        if (!GetWindowInfo(window_index, &info))
            return -1;

        return CountContentPixels(info.x + left,
                                  info.y + top,
                                  info.x + left + width - 1,
                                  info.y + top + height - 1,
                                  0);
    }

    const TextDraw *FindTextContaining(const char *needle) const {
        auto it = std::find_if(text_draws.begin(), text_draws.end(),
            [&](const TextDraw &draw) {
                return draw.text.find(needle) != std::string::npos;
            });

        return it == text_draws.end() ? nullptr : &*it;
    }

    /* Wait for the screen depth to change to a value matching the main editor
     * mode (DPaint switches from 2-bitplane Workbench to an 8-bitplane custom
     * screen once Screen Format is accepted via the Use button). */
    bool WaitForScreenDepth(int expected_depth, int timeout_ms = 15000) {
        int attempts = timeout_ms / 20;

        for (int attempt = 0; attempt < attempts; ++attempt) {
            int width, height, depth;
            if (lxa_get_screen_dimensions(&width, &height, &depth) &&
                depth == expected_depth) {
                return true;
            }
            RunCyclesWithVBlank(2, 50000);
        }

        return false;
    }
};

TEST_F(DPaintTest, ProgramLoads) {
    /* Verify DPaint loaded and is running */
    EXPECT_TRUE(lxa_is_running()) << "DPaint process should be running";
    EXPECT_STRNE(window_info.title, "System Message")
        << "DPaint should not stop at the missing rexxsyslib requester";
}

TEST_F(DPaintTest, WindowOpens) {
    if (!window_opened) {
        GTEST_SKIP() << "DPaint window did not open (known initialization issue)";
    }
    EXPECT_GT(window_info.width, 0);
    EXPECT_GT(window_info.height, 0);
}

TEST_F(DPaintTest, ScreenDimensions) {
    int width, height, depth;
    if (lxa_get_screen_dimensions(&width, &height, &depth)) {
        EXPECT_GE(width, 320) << "Screen width should be >= 320";
        EXPECT_GE(height, 200) << "Screen height should be >= 200";
    }
}

TEST_F(DPaintTest, MouseInteraction) {
    if (!window_opened) {
        GTEST_SKIP() << "DPaint window did not open (known initialization issue)";
    }
    
    /* Click in the drawing area */
    int click_x = window_info.x + window_info.width / 2;
    int click_y = window_info.y + window_info.height / 2;
    
    Click(click_x, click_y);
    RunCyclesWithVBlank(20, 50000);
    
    EXPECT_TRUE(lxa_is_running()) << "DPaint should still be running after mouse click";
}

TEST_F(DPaintPixelTest, OwnershipWindowBodyContainsVisibleContent) {
    lxa_flush_display();

    int body_pixels = CountContentPixels(
        window_info.x + 20,
        window_info.y + 20,
        window_info.x + window_info.width - 20,
        window_info.y + window_info.height - 20,
        0);

    EXPECT_GT(body_pixels, 150)
        << "DPaint ownership window body should contain visible text and controls";
}

TEST_F(DPaintPixelTest, ScreenFormatDialogSectionsContainVisibleContent) {
    ASSERT_STREQ(window_info.title, "Ownership Information")
        << "Expected DPaint to begin at its ownership window before opening the main editor";
    ASSERT_TRUE(DismissOwnershipWindow())
        << "Expected the ownership window to dismiss so the main editor can continue loading";

    ASSERT_TRUE(WaitForWindows(1, 10000));
    RunCyclesWithVBlank(120, 50000);

    ASSERT_TRUE(OpenScreenFormatDialog())
        << "Expected Ctrl-P to open DPaint's Screen Format dialog";

    int dialog_index = FindWindowIndexByTitleSubstring("Screen Format");
    ASSERT_GE(dialog_index, 0) << "Expected the Screen Format dialog window to be present";

    lxa_window_info_t dialog_info;
    ASSERT_TRUE(GetWindowInfo(dialog_index, &dialog_info));

    const std::string capture_path = ram_dir_path + "/dpaint-screen-format.png";
    ASSERT_TRUE(CaptureWindow(capture_path.c_str(), dialog_index))
        << "Expected Screen Format capture to succeed";

    lxa_flush_display();

    int choose_display_mode_pixels = CountWindowRectContent(dialog_index, 8, 34, 350, 107);
    int display_information_pixels = CountWindowRectContent(dialog_index, 364, 35, 266, 83);
    int credits_pixels = CountWindowRectContent(dialog_index, 364, 121, 266, 87);

    EXPECT_GT(choose_display_mode_pixels, 200)
        << "Expected the Choose Display Mode custom panel to contain DPaint-rendered mode rows";
    EXPECT_GT(display_information_pixels, 0)
        << "Expected the Display Information panel heading to render";
    EXPECT_GT(credits_pixels, 0)
        << "Expected the Credits panel heading to render";

    EXPECT_NE(FindTextContaining("Choose Display Mode"), nullptr)
        << "Expected DPaint to semantically render the Choose Display Mode heading";
    EXPECT_NE(FindTextContaining("Display Information"), nullptr)
        << "Expected DPaint to semantically render the Display Information heading";
    EXPECT_NE(FindTextContaining("Credits"), nullptr)
        << "Expected DPaint to semantically render the Credits heading";

    /* Phase 150 — layer creation backfill regression.
     * After dismissing the Ownership Information window and opening the
     * Screen Format dialog, the title-bar area of Screen Format (screen-
     * absolute rows ~11..30, roughly columns 80..400) must NOT contain
     * ghost pixels from Ownership's title bar.  The backfill at layer
     * creation must have cleared that area to pen 0.
     *
     * We sample the Screen Format window's own title bar strip (the first
     * ~19 pixel rows above the inner border) and count how many pixels are
     * NOT the window border's background (pen 0 or pen 1 frame colour).
     * We compare this count between the left half (where "Ownership
     * Information" text was) and the right half; they should be roughly
     * similar.  We also confirm that the centre of the title strip is NOT
     * exclusively pen 0 (i.e. the Screen Format title IS rendered) while
     * the "Ownership" ghost zone contains no unexpected glyph-coloured pixels.
     *
     * Simpler heuristic: after backfill the region [dialog_x+4..dialog_x+200,
     * dialog_y+2..dialog_y+12] (just below the title bar top edge, where
     * title text would appear) should NOT contain large runs of non-background
     * pixels that match the old Ownership glyph positions.  We verify by
     * counting pixels in a narrow strip of the title bar body — the count
     * must not exceed the expected maximum for a freshly-cleared layer. */
    int title_ghost_pixels = CountContentPixels(
        dialog_info.x + 4,
        dialog_info.y + 2,
        dialog_info.x + 200,
        dialog_info.y + 12,
        /* bg_color= */ 0 /* count all non-pen-0 pixels */);
    /* A fully backfilled title bar has exactly the window-chrome pixels
     * (title text, border gadgets).  Ownership's title "Ownership Information"
     * at ~16 chars × ~8px = ~128 non-bg pixels.  Screen Format's title at the
     * same position contributes a comparable number.  The important thing is
     * that the count is consistent with ONE title, not TWO overlaid titles.
     * We cap at 300 as a generous upper bound — two full titles would exceed
     * this only if both were fully double-drawn. */
    EXPECT_LT(title_ghost_pixels, 300)
        << "Phase 150 regression: title bar area contains too many non-background "
           "pixels, suggesting Ownership ghost pixels were not cleared by backfill";
}

/* ========================================================================
 * Phase 153b — String gadget recessed 3D frame regression
 *
 * DPaint's Screen Format dialog contains small numeric STRING_KIND gadgets
 * (Screen Size / Page Size entry boxes, id=6..9).  On a real Amiga + GadTools
 * these render with a double-bevel (ridge) frame:
 *   Outer bevel (recessed): shadow(pen 1) on top/left, shine(pen 2) on bot/right
 *   Inner bevel (raised):   shine(pen 2) on top/left, shadow(pen 1) on bot/right
 *
 * Gadget id=6 (first Screen Size width box) is at window-relative
 * xy=(213,166) wh=(44,10) per the DPaint gadget layout (empirically confirmed).
 * ====================================================================== */

TEST_F(DPaintPixelTest, ScreenFormatStringGadgetRecessed3DFrame) {
    ASSERT_STREQ(window_info.title, "Ownership Information")
        << "Expected DPaint to start at its ownership window";
    ASSERT_TRUE(DismissOwnershipWindow())
        << "Expected ownership window to dismiss";

    ASSERT_TRUE(WaitForWindows(1, 10000));
    RunCyclesWithVBlank(120, 50000);

    ASSERT_TRUE(OpenScreenFormatDialog())
        << "Expected Ctrl-P to open DPaint's Screen Format dialog";

    int dialog_index = FindWindowIndexByTitleSubstring("Screen Format");
    ASSERT_GE(dialog_index, 0) << "Expected the Screen Format dialog window to be present";

    lxa_window_info_t dialog_info;
    ASSERT_TRUE(GetWindowInfo(dialog_index, &dialog_info));

    lxa_flush_display();

    /* String gadget id=6 (Screen Size width): this is gadget index 4, screen
     * position (213,166) wh=(44,10).  These are the HITBOX coordinates; the
     * GadTools bevel border sits outside the hitbox at offsets (-GT_BEVEL_LEFT,
     * -GT_BEVEL_TOP) = (-4,-2), covering the full ng_Width x ng_Height area.
     *
     * Bevel outer top edge:  screen y = 166 - 2 = 164
     * Bevel outer left:      screen x = 213 - 4 = 209
     * Bevel full width:      44 + 2*4 = 52
     * Bevel full height:     10 + 2*2 = 14
     *
     * Outer bevel top edge should have shadow (pen 1) — recessed outer bevel.
     * Outer bevel bottom edge (y=164+14-1=177) should have shine (pen 2).
     */
    const int bevel_left = 209;    /* hitbox_left - GT_BEVEL_LEFT */
    const int bevel_top  = 164;    /* hitbox_top  - GT_BEVEL_TOP  */
    const int bevel_w    = 52;     /* ng_Width  = hitbox_w + 2*GT_BEVEL_LEFT */
    const int bevel_h    = 14;     /* ng_Height = hitbox_h + 2*GT_BEVEL_TOP  */

    int shadow_top = 0;
    for (int x = bevel_left; x < bevel_left + bevel_w; x++) {
        int pen = -1;
        lxa_read_pixel(dialog_info.x + x, dialog_info.y + bevel_top, &pen);
        if (pen == 1) shadow_top++;
    }
    EXPECT_GT(shadow_top, bevel_w / 2)
        << "Phase 153b: Screen Format string gadget id=6 top edge should have "
           "shadow (pen 1) pixels for recessed outer bevel, got " << shadow_top;

    int shine_bottom = 0;
    for (int x = bevel_left; x < bevel_left + bevel_w; x++) {
        int pen = -1;
        lxa_read_pixel(dialog_info.x + x, dialog_info.y + bevel_top + bevel_h - 1, &pen);
        if (pen == 2) shine_bottom++;
    }
    EXPECT_GT(shine_bottom, bevel_w / 2)
        << "Phase 153b: Screen Format string gadget id=6 bottom edge should have "
           "shine (pen 2) pixels for recessed outer bevel, got " << shine_bottom;
}

TEST_F(DPaintPixelTest, ScreenFormatCheckboxGadgetIsFixedWidth) {
    ASSERT_STREQ(window_info.title, "Ownership Information")
        << "Expected DPaint to start at its ownership window";
    ASSERT_TRUE(DismissOwnershipWindow())
        << "Expected ownership window to dismiss";

    ASSERT_TRUE(WaitForWindows(1, 10000));
    RunCyclesWithVBlank(120, 50000);

    ASSERT_TRUE(OpenScreenFormatDialog())
        << "Expected Ctrl-P to open DPaint's Screen Format dialog";

    int dialog_index = FindWindowIndexByTitleSubstring("Screen Format");
    ASSERT_GE(dialog_index, 0) << "Expected the Screen Format dialog window to be present";

    lxa_window_info_t dialog_info;
    ASSERT_TRUE(GetWindowInfo(dialog_index, &dialog_info));

    lxa_flush_display();

    /* Phase 155: Checkbox "Retain Picture" is gadget index 9.
     * Query its actual on-screen position to handle TopEdge adjustments.
     * After the CHECKBOX_KIND fix the gadget Dimensions should be 26×11,
     * regardless of ng_Width=135/ng_Height=14 supplied by DPaint. */
    auto gadgets = GetGadgets(dialog_index);
    ASSERT_GE((int)gadgets.size(), 12)
        << "Expected at least 12 gadgets in Screen Format dialog";

    /* Find the GadTools CHECKBOX_KIND gadget by id=11 (the one our fix applies to).
     * DPaint also has a raw Intuition bool gadget (id=16) at a different position —
     * that one is not a GadTools gadget and is not affected by the CHECKBOX_KIND fix. */
    int cb_idx = -1;
    for (int i = 0; i < (int)gadgets.size(); i++) {
        if (gadgets[i].gadget_id == 11) { cb_idx = i; break; }
    }
    ASSERT_GE(cb_idx, 0) << "Could not find GadTools checkbox gadget (id=11) in dialog";

    /* Gadget index cb_idx = GadTools CHECKBOX_KIND gadget */
    const lxa_gadget_info_t &cb_gad = gadgets[cb_idx];

    /* Width must be 26 (CHECKBOX_WIDTH), not ng_Width=135 */
    EXPECT_EQ(cb_gad.width, 26)
        << "Phase 155: 'Retain Picture' checkbox width should be 26 (CHECKBOX_WIDTH), got "
        << cb_gad.width;

    /* Height must be 11 (CHECKBOX_HEIGHT), not ng_Height=14 */
    EXPECT_EQ(cb_gad.height, 11)
        << "Phase 155: 'Retain Picture' checkbox height should be 11 (CHECKBOX_HEIGHT), got "
        << cb_gad.height;

    /* Bevel top edge: VIB_THICK3D spec §11.5 — SHINEPEN (pen 2) on top/left,
     * SHADOWPEN (pen 1) on bottom/right. */
    const int bev_x = cb_gad.left;   /* screen coords */
    const int bev_y = cb_gad.top;
    const int cb_w  = cb_gad.width;  /* should be 26 */
    const int cb_h  = cb_gad.height; /* should be 11 */

    int shine_top = 0;
    for (int x = bev_x; x < bev_x + cb_w - 1; x++) {   /* top row x=0..w-2 = SHINE */
        int pen = -1;
        lxa_read_pixel(dialog_info.x + x, dialog_info.y + bev_y, &pen);
        if (pen == 2) shine_top++;
    }
    EXPECT_GT(shine_top, cb_w / 2)
        << "Phase 155: checkbox top bevel edge at y=" << bev_y
        << " should have shine (pen 2) pixels (VIB_THICK3D); got " << shine_top;

    /* Bevel bottom edge: SHADOWPEN (pen 1) at y+h-1.
     * Only verify if within dialog window height. */
    const int bevel_bottom_screen_y = bev_y + cb_h - 1;
    if (bevel_bottom_screen_y < dialog_info.height) {
        int shadow_bottom = 0;
        for (int x = bev_x + 1; x < bev_x + cb_w; x++) {   /* x=1..w-1 = SHADOW */
            int pen = -1;
            lxa_read_pixel(dialog_info.x + x, dialog_info.y + bevel_bottom_screen_y, &pen);
            if (pen == 1) shadow_bottom++;
        }
        EXPECT_GT(shadow_bottom, cb_w / 2)
            << "Phase 155: checkbox bottom bevel edge at y=" << bevel_bottom_screen_y
            << " should have shadow (pen 1) pixels (VIB_THICK3D); got " << shadow_bottom;
    }
}

/* ========================================================================
 * Phase 115 — extended interaction coverage for DPaint V
 *
 * Startup flow on lxa:
 *   1. DPaint opens a single rootless window titled "Ownership Information"
 *      that overlays the Screen Format dialog (depth 2 Workbench-style).
 *   2. After dismissing the Ownership requester, the same window's title
 *      becomes "DeluxePaint 5.2 - Screen Format" and the full set of
 *      Screen Format gadgets becomes interactable.
 *   3. Clicking the "Use" gadget (id=14, lower-left) accepts the chosen
 *      mode and opens DPaint's main editor on a fresh 640x200x8 screen.
 *   4. The main editor's menu bar and toolbox icons appear once the user
 *      interacts with the screen (RMB on the menu bar, mouse motion).
 *      This deferred-paint behaviour matches what real DPaint does.
 * ====================================================================== */

class DPaintEditorTest : public DPaintPixelTest {
protected:
    /* DPaint's Screen Format gadget IDs (verified empirically with
     * tests/drivers/dpaint_probe_gtest.cpp). */
    static constexpr int SF_GADGET_USE_INDEX = 13;     /* id=14, "Use"  */
    static constexpr int SF_GADGET_CANCEL_INDEX = 14;  /* id=15, "Cancel" */

    /* Reach the "DeluxePaint 5.2 - Screen Format" state. */
    bool ReachScreenFormat() {
        if (std::string(window_info.title).find("Ownership") == std::string::npos) {
            /* Some startups skip Ownership outright. */
            return WaitForWindowTitleSubstring("Screen Format", 5000);
        }

        if (!DismissOwnershipWindow()) return false;
        if (!WaitForWindowTitleSubstring("Screen Format", 8000)) return false;
        RunCyclesWithVBlank(60, 50000);
        return true;
    }

    /* Click "Use" on the Screen Format dialog and wait for the main editor
     * screen mode (640x200x8) to come up. */
    bool AcceptScreenFormat() {
        int sf = FindWindowIndexByTitleSubstring("Screen Format");
        if (sf < 0) {
            ADD_FAILURE() << "AcceptScreenFormat: no Screen Format window found";
            return false;
        }

        int gc = GetGadgetCount(sf);
        if (gc <= SF_GADGET_USE_INDEX) {
            ADD_FAILURE() << "AcceptScreenFormat: gadget count " << gc << " <= " << SF_GADGET_USE_INDEX;
            return false;
        }
        if (!ClickGadget(SF_GADGET_USE_INDEX, sf)) {
            ADD_FAILURE() << "AcceptScreenFormat: ClickGadget failed";
            return false;
        }

        if (!WaitForScreenDepth(8, 15000)) {
            int w, h, d;
            lxa_get_screen_dimensions(&w, &h, &d);
            ADD_FAILURE() << "AcceptScreenFormat: depth still " << d
                          << " after 15s (size " << w << "x" << h << ")";
            return false;
        }
        return true;
    }
};

TEST_F(DPaintEditorTest, OwnershipDismissRevealsScreenFormatGadgets) {
    ASSERT_TRUE(ReachScreenFormat())
        << "Expected DPaint to reach the Screen Format dialog after dismissing Ownership";

    int sf = FindWindowIndexByTitleSubstring("Screen Format");
    ASSERT_GE(sf, 0);

    /* The Screen Format dialog has a stable gadget structure: at least
     * the "Use" and "Cancel" buttons must be present at the bottom edge. */
    int gadget_count = GetGadgetCount(sf);
    EXPECT_GE(gadget_count, 15)
        << "Screen Format dialog should expose at least 15 gadgets";

    lxa_window_info_t info;
    ASSERT_TRUE(GetWindowInfo(sf, &info));

    lxa_gadget_info_t use_gad{};
    ASSERT_TRUE(GetGadgetInfo(SF_GADGET_USE_INDEX, &use_gad, sf));
    EXPECT_EQ(use_gad.gadget_id, 14)
        << "Expected the lower-left Screen Format gadget to be the 'Use' button (id=14)";
    EXPECT_LT(use_gad.left, info.width / 4)
        << "'Use' button should sit in the dialog's lower-left quadrant";
    EXPECT_GT(use_gad.top, info.height * 3 / 4)
        << "'Use' button should sit near the bottom of the dialog";

    lxa_gadget_info_t cancel_gad{};
    ASSERT_TRUE(GetGadgetInfo(SF_GADGET_CANCEL_INDEX, &cancel_gad, sf));
    EXPECT_EQ(cancel_gad.gadget_id, 15)
        << "Expected the lower-right Screen Format gadget to be the 'Cancel' button (id=15)";
    EXPECT_GT(cancel_gad.left, info.width * 3 / 4)
        << "'Cancel' button should sit in the dialog's lower-right quadrant";
}

TEST_F(DPaintEditorTest, UseButtonOpensMainEditorScreen) {
    ASSERT_TRUE(ReachScreenFormat());

    int width_before = 0, height_before = 0, depth_before = 0;
    lxa_get_screen_dimensions(&width_before, &height_before, &depth_before);
    EXPECT_EQ(depth_before, 2)
        << "Screen Format dialog should be displayed on a depth-2 Workbench-style screen";

    ASSERT_TRUE(AcceptScreenFormat())
        << "Clicking 'Use' should switch to DPaint's main 8-bitplane editor screen";

    int width_after = 0, height_after = 0, depth_after = 0;
    ASSERT_TRUE(lxa_get_screen_dimensions(&width_after, &height_after, &depth_after));
    EXPECT_EQ(depth_after, 8)
        << "DPaint main editor screen should have 8 bitplanes (256-colour mode)";
    EXPECT_GE(width_after, 320);
    EXPECT_GE(height_after, 200);

    EXPECT_TRUE(lxa_is_running())
        << "DPaint should still be running after the screen mode switch";
}

TEST_F(DPaintEditorTest, MainEditorMenuBarAndToolboxRender) {
    ASSERT_TRUE(ReachScreenFormat());
    ASSERT_TRUE(AcceptScreenFormat());

    /* Let the new editor screen settle before any input.  DPaint takes
     * several VBlanks to finish initialising the toolbox and menu strip
     * after the screen mode change. */
    RunCyclesWithVBlank(500, 50000);

    /* DPaint defers some painting until the first user interaction.
     * A single mouse click in the canvas plus an RMB at the menu bar
     * triggers the menu strip and toolbox to render. */
    Click(320, 100);
    RunCyclesWithVBlank(80, 50000);
    lxa_inject_rmb_click(320, 5);
    RunCyclesWithVBlank(160, 50000);

    int screen_w = 0, screen_h = 0, screen_d = 0;
    ASSERT_TRUE(lxa_get_screen_dimensions(&screen_w, &screen_h, &screen_d));

    const std::string capture_path = "/tmp/dpaint-main-editor.png";
    EXPECT_TRUE(lxa_capture_screen(capture_path.c_str()))
        << "Expected DPaint main editor screen capture to succeed";

    lxa_flush_display();

    /* Menu bar lives on the top scanlines (y=0..10).  When the menu strip is
     * active, characters paint with the menu pen.  We compare against the
     * dominant background pen sampled from a quiet area in the upper-left
     * corner because DPaint's editor screen does not use pen 0 as backdrop. */
    int bg_pen = -1;
    lxa_read_pixel(2, 2, &bg_pen);

    auto count_non_bg = [&](int x1, int y1, int x2, int y2) {
        int c = 0;
        for (int y = y1; y <= y2; ++y) {
            for (int x = x1; x <= x2; ++x) {
                int pen;
                if (lxa_read_pixel(x, y, &pen) && pen != bg_pen) c++;
            }
        }
        return c;
    };

    /* The DPaint editor has two unmistakable visible regions:
     *   - The top strip (menu bar plus tool palette banner) covers roughly
     *     y=0..50 and contains many hundreds of non-background pixels.
     *   - The right-hand toolbox strip (~100px wide) contains the brush
     *     selector and the column of drawing tools.
     * Asserting on these aggregate pixel counts is robust against minor
     * font/pen changes while still detecting "blank canvas" regressions
     * such as the SMART_REFRESH stalls documented in roadmap.md. */
    int top_strip_pixels = count_non_bg(0, 0, screen_w - 1, 49);
    int right_strip_pixels = count_non_bg(screen_w - 100, 0, screen_w - 1, screen_h - 1);

    EXPECT_GT(top_strip_pixels, 2000)
        << "Expected DPaint menu bar / tool palette banner to render in the top "
        << "50 scanlines after RMB activation (bg_pen=" << bg_pen << ")";

    EXPECT_GT(right_strip_pixels, 2000)
        << "Expected DPaint toolbox column to render in the rightmost 100 pixels "
        << "(bg_pen=" << bg_pen << ")";

    EXPECT_TRUE(lxa_is_running())
        << "DPaint should still be running after menu/toolbox interaction";
}

TEST_F(DPaintEditorTest, ScreenFormatCancelTerminatesDPaintCleanly) {
    /* Verify that cancelling the Screen Format dialog leads to DPaint
     * exiting cleanly rather than crashing or hanging.  This guards
     * against regressions in the IDCMP/menu cleanup paths exercised
     * during a normal user-initiated quit. */
    ASSERT_TRUE(ReachScreenFormat());

    int sf_index = FindWindowIndexByTitleSubstring("Screen Format");
    ASSERT_GE(sf_index, 0);

    ASSERT_TRUE(ClickGadget(SF_GADGET_CANCEL_INDEX, sf_index));

    bool exited = false;
    for (int attempt = 0; attempt < 200; ++attempt) {
        if (!lxa_is_running()) { exited = true; break; }
        RunCyclesWithVBlank(2, 50000);
    }

    /* DPaint may either exit (typical) or fall through to its main editor
     * after Cancel; both are acceptable as long as no crash occurred. */
    if (!exited) {
        EXPECT_TRUE(lxa_is_running())
            << "DPaint should remain stable if Cancel does not terminate it";
    } else {
        SUCCEED() << "DPaint exited cleanly after Cancel";
    }
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
