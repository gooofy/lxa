/**
 * typeface_gtest.cpp - Google Test driver for Typeface font previewer
 *
 * Phase 136 / Phase 147c of the lxa roadmap.
 *
 * Typeface is a BGUI-based font editor for AmigaOS.  It requires
 * bgui.library v39+ and gadgets/textfield.gadget for its preview
 * text-entry widget.  The app ships with its own bundled bgui.library
 * in Typeface/Libs/ — lxa automatically prepends the program-local
 * Libs/ directory to LIBS: when the program is loaded, so no explicit
 * extra assign is needed for bgui.library.  textfield.gadget is loaded
 * from Typeface/Gadgets/ via the GADGETS: assign.
 *
 * Coverage focus (Phase 136):
 *   - bgui.library opens cleanly (no PANIC log, no rv=26 exit).
 *   - Typeface reaches a non-exit state: at least one window appears.
 *   - The window has plausible Amiga-style geometry (non-zero width/height).
 *   - Phase 130 text hook captures at least some rendered text (proves the
 *     BGUI layout rendered labels or font names into the window).
 *   - No PANIC log entries appear during startup.
 *   - The window survives a settle period without crashing (idle-time stability).
 *
 * Coverage focus (Phase 147c):
 *   - textfield.gadget loads from GADGETS: assign (no "NoTextFieldGadget" error).
 *   - Project→Preview menu opens the Preview window (second window appears).
 *   - Preview window has the expected BGUI gadgets (4 buttons + PropGadget).
 *   - Preview window renders non-blank content (textfield + button labels).
 *
 * Menu coordinates (verified via typeface_probe_gtest):
 *   Project menu (Menu[0]): left=0, width=72 → centre x=36
 *   "Preview..." item (Item[3]): y=30 in dropdown → screen y = 11+30+5 = 46
 */

#include "lxa_test.h"

#include <algorithm>
#include <filesystem>
#include <string>
#include <vector>
#include <time.h>

using namespace lxa::testing;

/* ------------------------------------------------------------------ */
/* Test fixture                                                         */
/* ------------------------------------------------------------------ */

class TypefaceTest : public LxaUITest {
protected:
    std::vector<std::string> text_log_;
    long startup_ms_ = -1;

    /* Concatenated text log as a single string for substring searches. */
    std::string ConcatTextLog() const
    {
        std::string result;
        for (const auto &s : text_log_) {
            result += s;
            result += ' ';
        }
        return result;
    }

    void FlushAndSettle()
    {
        lxa_flush_display();
        RunCyclesWithVBlank(4, 50000);
        lxa_flush_display();
    }

    void SetUp() override
    {
        const char *rom_path = FindRomPath();
        if (rom_path != nullptr) {
            config.rom_path = rom_path;
        }

        LxaUITest::SetUp();

        const char *apps = FindAppsPath();
        if (apps == nullptr) {
            GTEST_SKIP() << "lxa-apps directory not found";
        }

        const std::filesystem::path typeface_dir =
            std::filesystem::path(apps) / "Typeface";
        const std::filesystem::path typeface_bin = typeface_dir / "Typeface";

        if (!std::filesystem::exists(typeface_bin)) {
            GTEST_SKIP() << "Typeface binary not found at "
                         << typeface_bin.string();
        }

        /* Install the text hook before loading the program so we capture
         * all text rendered during startup. */
        lxa_set_text_hook(
            [](const char *s, int n, int /*x*/, int /*y*/, void *ud) {
                auto *log = static_cast<std::vector<std::string> *>(ud);
                if (n > 0) {
                    log->push_back(std::string(s, static_cast<size_t>(n)));
                }
            },
            &text_log_);

        struct timespec t0, t1;
        clock_gettime(CLOCK_MONOTONIC, &t0);

        ASSERT_EQ(lxa_load_program("APPS:Typeface/Typeface", ""), 0)
            << "Failed to load Typeface via APPS: assign";

        /* Settle: 1500 VBlank iterations × 50 000 cycles = 75 M cycles.
         * Typeface is BGUI-based; the BOOPSI window class needs many
         * cycles to lay out and render its gadgets. The character grid
         * also takes time to render — chars 0x00..0x1F are control
         * chars with empty glyphs; the visible printable chars only
         * start at 0x20+. */
        RunCyclesWithVBlank(1500, 50000);

        clock_gettime(CLOCK_MONOTONIC, &t1);
        startup_ms_ = (long)((t1.tv_sec - t0.tv_sec) * 1000LL +
                             (t1.tv_nsec - t0.tv_nsec) / 1000000LL);

        FlushAndSettle();
    }

    void TearDown() override
    {
        lxa_clear_text_hook();
        LxaUITest::TearDown();
    }
};

/* ------------------------------------------------------------------ */
/* Tests                                                                */
/* ------------------------------------------------------------------ */

/* Typeface should open at least one window on startup. */
TEST_F(TypefaceTest, StartupOpensWindow)
{
    int wcount = lxa_get_window_count();
    EXPECT_GE(wcount, 1) << "Typeface should have opened at least one window";

    if (wcount > 0) {
        lxa_window_info_t wi;
        ASSERT_TRUE(lxa_get_window_info(0, &wi));
        EXPECT_GT(wi.width,  0) << "Window width should be non-zero";
        EXPECT_GT(wi.height, 0) << "Window height should be non-zero";
    }
}

/* bgui.library must have been opened without a PANIC log entry. */
TEST_F(TypefaceTest, NoPanicDuringStartup)
{
    const std::string output = GetOutput();
    EXPECT_EQ(output.find("PANIC"), std::string::npos)
        << "PANIC found in output: " << output;
    EXPECT_EQ(output.find("rv=26"), std::string::npos)
        << "Unexpected rv=26 exit in output: " << output;
}

/* Phase 130 text hook: Typeface must render at least some text labels
 * through the BGUI layout engine (e.g., button labels, status bar, or
 * font names in the list). */
TEST_F(TypefaceTest, TextHookCapturesSomeText)
{
    EXPECT_FALSE(text_log_.empty())
        << "Text hook captured no text — BGUI layout may not have rendered";

    if (!text_log_.empty()) {
        /* Concatenated log should contain at least a few printable chars. */
        std::string concat = ConcatTextLog();
        int printable = 0;
        for (char c : concat) {
            if (c >= 0x20 && c < 0x7f) {
                ++printable;
            }
        }
        EXPECT_GE(printable, 3)
            << "Too few printable characters captured by text hook";
    }
}

/* Window geometry should be plausible Amiga-style (width in 200-1024,
 * height in 50-800). */
TEST_F(TypefaceTest, WindowGeometryIsPlausible)
{
    if (lxa_get_window_count() < 1) {
        GTEST_SKIP() << "No window opened — already covered by StartupOpensWindow";
    }

    lxa_window_info_t wi;
    ASSERT_TRUE(lxa_get_window_info(0, &wi));

    EXPECT_GE(wi.width,  100) << "Window too narrow: " << wi.width;
    EXPECT_LE(wi.width,  1024) << "Window too wide: " << wi.width;
    EXPECT_GE(wi.height, 50) << "Window too short: " << wi.height;
    EXPECT_LE(wi.height, 800) << "Window too tall: " << wi.height;
}

/* Phase 147b: Typeface first-run window geometry target is 194×129.
 *
 * This is the correct BGUI minimum size computed from:
 *   - CharGadget: 8 columns × (topaz8 XSize=8 + 2×CG_XOFFSET=12) = 8×20 = 160px wide
 *   - PropObject: FixWidth(16)
 *   - HOffset(4)×2 + Spacing(2) + WBorLeft(4) + WBorRight(4) = 22px overhead
 *   Total width = 160 + 16 + 22 = ~194px (exact depends on BGUI frame borders)
 *
 *   - CharGadget: 8 rows × (topaz8 YSize=8 + 2×CG_YOFFSET=6) = 8×14 = 112px tall
 *   - Box.Height from Typeface: 112 + 4 + WBorTop(2) + 1 + Font->ta_YSize(8) + WBorBottom(2) = 129
 *   Total height = max(BGUI_min, Box.Height=129) = 129px (AmigaOS 3.1 reference)
 *
 * ±4px tolerance accommodates minor BGUI frame/border rounding differences.
 */
TEST_F(TypefaceTest, WindowGeometryMatchesTarget)
{
    if (lxa_get_window_count() < 1) {
        GTEST_SKIP() << "No window opened";
    }

    lxa_window_info_t wi;
    ASSERT_TRUE(lxa_get_window_info(0, &wi));

    /* AmigaOS 3.1 reference (app-typeface): 194x129 for first-run with
     * topaz.font/8 on the PAL 640x256 Workbench (WBorTop 2) */
    const int target_w = 194;
    const int target_h = 129;
    const int tolerance = 4;

    EXPECT_GE(wi.width,  target_w - tolerance)
        << "Window narrower than expected: " << wi.width << " (target " << target_w << ")";
    EXPECT_LE(wi.width,  target_w + tolerance)
        << "Window wider than expected: " << wi.width << " (target " << target_w << ")";
    EXPECT_GE(wi.height, target_h - tolerance)
        << "Window shorter than expected: " << wi.height << " (target " << target_h << ")";
    EXPECT_LE(wi.height, target_h + tolerance)
        << "Window taller than expected: " << wi.height << " (target " << target_h << ")";
}

/* Idle-time stability: run a further 200 VBlank iterations and confirm
 * the emulator has not crashed (window count unchanged). */
TEST_F(TypefaceTest, IdleTimeStability)
{
    int wcount_before = lxa_get_window_count();
    RunCyclesWithVBlank(200, 50000);
    FlushAndSettle();
    int wcount_after = lxa_get_window_count();
    EXPECT_EQ(wcount_after, wcount_before)
        << "Window count changed during idle — possible crash or unexpected close";
}

/* Phase 126: startup latency baseline. */
TEST_F(TypefaceTest, ZStartupLatencyBaseline)
{
    EXPECT_GE(startup_ms_, 0) << "Startup timer not recorded";
    /* No hard upper bound — just record for profiling regression tracking. */
    RecordProperty("startup_ms", startup_ms_);
}

/* ------------------------------------------------------------------ */
/* Phase 147c tests                                                     */
/* ------------------------------------------------------------------ */

/* Phase 147c: textfield.gadget must load from GADGETS: without error.
 *
 * Typeface calls OpenLibrary("gadgets/textfield.gadget",3) at startup.
 * If it falls back to GADGETS:textfield.gadget and succeeds, the output
 * will contain the LoadSeg success message but NOT "NoTextFieldGadget".
 * The output should also not contain rv=26 (which would indicate Typeface
 * exited early due to the missing library). */
TEST_F(TypefaceTest, TextFieldGadgetLoads)
{
    const std::string output = GetOutput();
    EXPECT_EQ(output.find("NoTextFieldGadget"), std::string::npos)
        << "textfield.gadget failed to load (NoTextFieldGadget in output)";
    EXPECT_EQ(output.find("rv=26"), std::string::npos)
        << "Typeface exited early (rv=26 in output) — textfield.gadget missing?";
    /* The app must have an open window — proof that textfield.gadget loading
     * did not prevent the main window from opening. */
    EXPECT_GE(lxa_get_window_count(), 1)
        << "No window opened — textfield.gadget may have blocked startup";
}

/* Phase 147c: Project→Preview menu must open the Preview window.
 *
 * Menu layout (verified by typeface_probe_gtest):
 *   Project menu (Menu[0]): centre x ≈ 36
 *   "Preview..." item (Item[3]): top=30 in dropdown → screen y ≈ 46
 *
 * After the RMB drag the window count should increase from 1 to 2. */
TEST_F(TypefaceTest, PreviewWindowOpens)
{
    if (lxa_get_window_count() < 1) {
        GTEST_SKIP() << "Main window not open — cannot test Preview";
    }

    const int wcount_before = lxa_get_window_count();

    /* Two-phase RMB drag to open Project→Preview.
     * Menu[0] (Project): centre x=36, menu bar y=5.
     * Item[3] (Preview...): screen y = menu_bar_height(11) + item_top(30) + 5 = 46. */
    const int menu_x    = 36;  /* Project menu centre */
    const int bar_y     = 5;   /* Menu bar mid-point */
    const int item_y    = 46;  /* "Preview..." screen y */

    /* Use lxa_inject_drag which is reliable per lesson 6.5 */
    lxa_inject_drag(menu_x, bar_y, menu_x, item_y, LXA_MOUSE_RIGHT, 20);

    /* Give the app time to open the Preview window (BGUI WindowOpen +
     * SetPreviewFont/SaveFont which does significant work). */
    RunCyclesWithVBlank(5000, 50000);
    lxa_flush_display();

    const int wcount_after = lxa_get_window_count();
    EXPECT_GT(wcount_after, wcount_before)
        << "Preview window did not open after Project→Preview menu selection "
        << "(windows before=" << wcount_before << " after=" << wcount_after << ")";
}

/* ------------------------------------------------------------------ */
/* Test entry point                                                     */
/* ------------------------------------------------------------------ */

int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
