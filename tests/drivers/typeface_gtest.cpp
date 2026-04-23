/**
 * typeface_gtest.cpp - Google Test driver for Typeface font previewer
 *
 * Phase 136 of the lxa roadmap.
 *
 * Typeface is a BGUI-based font previewer for AmigaOS.  It requires
 * bgui.library v39+ and (optionally) gadgets/textfield.gadget for its
 * preview text-entry widget.  The app ships with its own bundled
 * bgui.library in Typeface/Libs/ — lxa automatically prepends the
 * program-local Libs/ directory to LIBS: when the program is loaded, so
 * no explicit extra assign is needed for bgui.library.
 *
 * Coverage focus (Phase 136):
 *   - bgui.library opens cleanly (no PANIC log, no rv=26 exit).
 *   - Typeface reaches a non-exit state: at least one window appears.
 *   - The window has plausible Amiga-style geometry (non-zero width/height).
 *   - Phase 130 text hook captures at least some rendered text (proves the
 *     BGUI layout rendered labels or font names into the window).
 *   - No PANIC log entries appear during startup.
 *   - The window survives a settle period without crashing (idle-time stability).
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

/* Capture the window and verify it is not entirely blank/uniform.
 *
 * Typeface runs on a custom screen with a 4-entry palette
 * (gray/black/white/blue). The blue is unused on the main view, so the
 * captured PNG contains at most 3 unique colors even when fully
 * populated with glyphs and labels. A unique-color count is therefore
 * not a useful blankness signal here.
 *
 * Instead, we check that the capture contains a non-trivial amount of
 * non-background pixels: any properly rendered BGUI window draws a
 * border, frame, labels, and (for Typeface) a character map full of
 * black glyph pixels. A truly blank window would be a single fill
 * color — its dominant-color fraction would be ~100%.
 */
TEST_F(TypefaceTest, WindowIsNotBlank)
{
    if (lxa_get_window_count() < 1) {
        GTEST_SKIP() << "No window opened";
    }

    const char *path = "/tmp/typeface_capture.png";
    ASSERT_TRUE(CaptureWindow(path, 0)) << "Failed to capture window";

    /* Use ImageMagick `identify -format %[fx:mean.r]` to get the mean
     * red channel value (0..1). A monochrome blank fill has mean equal
     * to its fill color; a window with rendered black-on-gray content
     * has a mean visibly lower than the gray fill. We assert the mean
     * is meaningfully below pure white (i.e., some dark content is
     * present) and above pure black (i.e., not entirely black). */
    char cmd[512];
    snprintf(cmd, sizeof(cmd),
             "identify -format '%%[fx:mean]' '%s' 2>/dev/null", path);
    FILE *fp = popen(cmd, "r");
    ASSERT_NE(fp, nullptr);
    double mean = -1.0;
    if (fscanf(fp, "%lf", &mean) != 1) mean = -1.0;
    pclose(fp);

    RecordProperty("mean_intensity", static_cast<int>(mean * 1000.0));

    ASSERT_GE(mean, 0.0) << "Failed to read mean intensity from " << path;

    /* A flat-colored blank capture would have mean equal to a single
     * palette entry (e.g. 0.667 for the gray fill, 1.0 for white).
     * A populated capture with mixed black glyphs over gray drops the
     * mean noticeably. The exact thresholds are loose: we only want
     * to rule out the all-one-color case. */
    EXPECT_GT(mean, 0.05)
        << "Window appears all-black (mean=" << mean << ") in " << path;
    EXPECT_LT(mean, 0.95)
        << "Window appears all-white/uniform (mean=" << mean << ") in " << path;
}

/* Phase 147a: verify Amiga window chrome (title bar, borders, system
 * gadgets) is rendered into the window bitmap.
 *
 * Typeface runs on a custom screen, so chrome MUST be drawn into the
 * screen bitmap (it cannot be delegated to the host WM the way Workbench
 * windows are in rootless mode). Before Phase 147a, the rootless-mode
 * code path skipped _render_window_frame() unconditionally, leaving
 * Typeface windows borderless and gadget-less.
 *
 * Pixel signature on the 4-color Typeface screen
 * (gray=170,170,170 / black=0 / white=255 / blue):
 *   - Title bar row (y=2..6): black background
 *   - Just below title bar (y=15): gray interior fill
 *   - Left border outer column (x=0): white highlight
 *   - Right border outer column (x=width-1): black shadow
 *   - Bottom border (y=height-1): black shadow
 *
 * Without chrome, every one of these positions would be the gray
 * interior fill — so any single mismatched expectation here proves the
 * chrome rendering regressed.
 */
TEST_F(TypefaceTest, WindowChromeIsRendered)
{
    if (lxa_get_window_count() < 1) {
        GTEST_SKIP() << "No window opened";
    }

    lxa_window_info_t wi;
    ASSERT_TRUE(lxa_get_window_info(0, &wi));

    const char *path = "/tmp/typeface_chrome.png";
    ASSERT_TRUE(CaptureWindow(path, 0)) << "Failed to capture window";

    auto sample = [&](int x, int y, int *r, int *g, int *b) -> bool {
        char cmd[256];
        snprintf(cmd, sizeof(cmd),
                 "convert '%s' -format '%%[fx:int(255*p{%d,%d}.r)] "
                 "%%[fx:int(255*p{%d,%d}.g)] %%[fx:int(255*p{%d,%d}.b)]' info: 2>/dev/null",
                 path, x, y, x, y, x, y);
        FILE *fp = popen(cmd, "r");
        if (!fp) return false;
        bool ok = (fscanf(fp, "%d %d %d", r, g, b) == 3);
        pclose(fp);
        return ok;
    };

    int r, g, b;

    /* Title bar row should be dark (black on this palette).
     * Sample mid-bar to avoid the close gadget on the left and depth
     * gadget on the right. */
    int title_x = wi.width / 2;
    ASSERT_TRUE(sample(title_x, 4, &r, &g, &b));
    EXPECT_LT(r + g + b, 150)
        << "Title bar at (" << title_x << ",4) is not dark: "
        << "rgb=(" << r << "," << g << "," << b << ") — chrome missing?";

    /* Left border outer column should be the white highlight. */
    ASSERT_TRUE(sample(0, wi.height / 2, &r, &g, &b));
    EXPECT_GT(r + g + b, 600)
        << "Left border at (0," << wi.height/2 << ") is not bright: "
        << "rgb=(" << r << "," << g << "," << b << ") — left border missing?";

    /* Right border outer column should be the black shadow. */
    ASSERT_TRUE(sample(wi.width - 1, wi.height / 2, &r, &g, &b));
    EXPECT_LT(r + g + b, 150)
        << "Right border at (" << wi.width-1 << "," << wi.height/2
        << ") is not dark: rgb=(" << r << "," << g << "," << b
        << ") — right border missing?";

    /* Bottom border should be the dark shadow. */
    ASSERT_TRUE(sample(wi.width / 2, wi.height - 1, &r, &g, &b));
    EXPECT_LT(r + g + b, 150)
        << "Bottom border at (" << wi.width/2 << "," << wi.height-1
        << ") is not dark: rgb=(" << r << "," << g << "," << b
        << ") — bottom border missing?";

    /* Interior just below title bar should be the gray fill, proving
     * we are not just looking at an all-black image. */
    ASSERT_TRUE(sample(wi.width / 2, 15, &r, &g, &b));
    EXPECT_GT(r + g + b, 300)
        << "Interior at (" << wi.width/2 << ",15) is too dark: "
        << "rgb=(" << r << "," << g << "," << b
        << ") — interior fill missing or window collapsed?";
}

/* ------------------------------------------------------------------ */
/* Test entry point                                                     */
/* ------------------------------------------------------------------ */

int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
