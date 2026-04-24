/**
 * dopus_gtest.cpp - Google Test driver for Directory Opus 4.x
 *
 * Phase 121 of the app-coverage roadmap.
 *
 * Directory Opus 4 is a third-party file manager with a heavily custom UI:
 * a menu bar, two file-list panes, and a configurable button area.  It
 * ships with several bundled libraries (dopus.library, arp.library,
 * iff.library, powerpacker.library, Explode.Library, Icon.Library) plus
 * its own c/, l/, s/, and Modules/ subdirectories.  Because lxa
 * automatically prepends a program's own libs/, s/, c/, l/ and devs/
 * directories to the corresponding assigns when the executable is loaded,
 * we only need to assign APPS: and load the binary by path.
 *
 * Coverage focus:
 *   - Startup completes far enough to open the main DOPUS window.
 *   - Bundled disk libraries load (no missing-library errors for libs we
 *     ship; absent third-party libs such as commodities/inovamusic are
 *     observed but tolerated by DOpus itself).
 *   - The tracked window has plausible Amiga-style geometry.
 *   - The window survives a benign right-mouse menu probe.
 *   - The startup capture can be saved as an artifact for vision review.
 *   - DOpus uses a custom-rendered UI (very few Intuition gadgets); we
 *     verify that the gadget enumeration call returns a sane (non-negative)
 *     count rather than crashing the emulator.
 *   - The window content area contains a non-trivial number of non-grey
 *     pixels (proves DOpus actually rendered something, not just opened
 *     an empty window).
 */

#include "lxa_test.h"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <time.h>

using namespace lxa::testing;

class DOpusTest : public LxaUITest {
protected:
    bool        saw_window         = false;
    int         captured_width     = 0;
    int         captured_height    = 0;
    bool        capture_succeeded  = false;
    std::string capture_path;
    int         gadget_count       = -1;
    int         content_pixels     = 0;
    std::string startup_output;
    long        startup_ms_        = -1;  /* Phase 126: latency baseline */

    void FlushAndSettle()
    {
        lxa_flush_display();
        RunCyclesWithVBlank(4, 50000);
        lxa_flush_display();
    }

    void SetUp() override
    {
        const char* rom_path = FindRomPath();
        if (rom_path != nullptr) {
            config.rom_path = rom_path;
        }

        LxaUITest::SetUp();

        const char* apps = FindAppsPath();
        if (apps == nullptr) {
            GTEST_SKIP() << "lxa-apps directory not found";
        }

        const std::filesystem::path dopus_base =
            std::filesystem::path(apps) / "DirectoryOpus" / "bin" / "DOPUS";
        const std::filesystem::path dopus_bin = dopus_base / "DirectoryOpus";
        if (!std::filesystem::exists(dopus_bin)) {
            GTEST_SKIP() << "DirectoryOpus binary not found at "
                         << dopus_bin.string();
        }

        /* DOpus' libs/, s/, c/, l/ are auto-prepended to the matching
         * assigns by lxa when the program is loaded from its own dir. */
        struct timespec _t0, _t1;
        clock_gettime(CLOCK_MONOTONIC, &_t0);
        ASSERT_EQ(lxa_load_program("APPS:DirectoryOpus/bin/DOPUS/DirectoryOpus", ""), 0)
            << "Failed to load DirectoryOpus via APPS: assign";

        ASSERT_TRUE(WaitForWindows(1, 15000))
            << "DirectoryOpus did not open a tracked window\n"
            << GetOutput();
        clock_gettime(CLOCK_MONOTONIC, &_t1);
        startup_ms_ = (_t1.tv_sec - _t0.tv_sec) * 1000L +
                      (_t1.tv_nsec - _t0.tv_nsec) / 1000000L;

        if (GetWindowInfo(0, &window_info)) {
            saw_window      = true;
            captured_width  = window_info.width;
            captured_height = window_info.height;
        }

        WaitForWindowDrawn(0, 5000);
        WaitForEventLoop(150, 10000);
        /* DOpus renders its button bank via a VBlank-driven timer event that
         * fires around VBlank 400 after startup.  Run 600 VBlank iterations
         * to ensure the full button bank is rendered before taking the
         * content-pixel snapshot. */
        RunCyclesWithVBlank(600, 50000);
        FlushAndSettle();

        /* Re-fetch geometry in case DOpus resized the window after init. */
        if (GetWindowInfo(0, &window_info)) {
            captured_width  = std::max(captured_width,  (int)window_info.width);
            captured_height = std::max(captured_height, (int)window_info.height);
        }

        /* Capture artifact for visual review. */
        capture_path = "/tmp/dopus_startup.png";
        capture_succeeded = lxa_capture_window(0, capture_path.c_str());

        /* Snapshot gadget enumeration and content-pixel count. */
        gadget_count = GetGadgetCount(0);
        if (saw_window) {
            const int x1 = std::max(2, (int)window_info.x + 4);
            const int y1 = std::max(2, (int)window_info.y + 12);
            const int x2 = window_info.x + window_info.width  - 4;
            const int y2 = window_info.y + window_info.height - 4;
            /* Background pen is grey (0) on the standard Workbench palette. */
            content_pixels = CountContentPixels(x1, y1, x2, y2, 0);
        }

        startup_output = GetOutput();
    }
};

/* ---- Startup smoke ---------------------------------------------------- */

TEST_F(DOpusTest, StartupOpensVisibleMainWindow)
{
    EXPECT_TRUE(saw_window)
        << "DirectoryOpus should open at least one tracked window\n"
        << startup_output;
    /* DOpus 4 default main window is ~640x245 on a PAL screen. */
    EXPECT_GE(captured_width, 320)
        << "DOpus main window should be at least 320 pixels wide";
    EXPECT_LE(captured_width, 1280)
        << "DOpus main window should not exceed reasonable max width";
    EXPECT_GE(captured_height, 180)
        << "DOpus main window should be at least 180 pixels tall";
    EXPECT_LE(captured_height, 800)
        << "DOpus main window should not exceed reasonable max height";
}

TEST_F(DOpusTest, StartupBundledLibrariesLoad)
{
    /* The bundled disk libraries (dopus, arp, iff, powerpacker, Explode,
     * Icon) live in DOpus' own libs/ subdirectory and must resolve via
     * lxa's auto-PROGDIR LIBS prepend.  None of them should appear in a
     * "requested library ... was not found" log line.
     *
     * NOTE: commodities.library and inovamusic.library are NOT shipped
     * with DOpus and are NOT shipped by lxa as stubs; DOpus tolerates
     * their absence.  We deliberately do not assert on those names. */
    static const char* required_libs[] = {
        "dopus.library",
        "arp.library",
        "iff.library",
        "powerpacker.library",
        "Explode.Library",
        "Icon.Library",
    };

    for (const char* lib : required_libs) {
        const std::string needle =
            std::string("requested library ") + lib + " was not found";
        EXPECT_EQ(startup_output.find(needle), std::string::npos)
            << "Bundled DOpus library '" << lib
            << "' should resolve via PROGDIR auto-prepend, but log says it "
               "was not found.\n"
            << startup_output;
    }
}

TEST_F(DOpusTest, StartupHasNoPanicOrBitmapErrors)
{
    EXPECT_EQ(startup_output.find("PANIC"), std::string::npos)
        << "DOpus startup must not trigger a ROM PANIC\n"
        << startup_output;
    EXPECT_EQ(startup_output.find("invalid RastPort BitMap"), std::string::npos)
        << "DOpus startup must not hit a null-BitMap rendering path\n"
        << startup_output;
}

TEST_F(DOpusTest, StartupRemainsRunning)
{
    EXPECT_TRUE(lxa_is_running())
        << "DirectoryOpus should still be running after initial settle\n"
        << startup_output;
}

/* ---- Window geometry & rendering ------------------------------------- */

TEST_F(DOpusTest, MainWindowContainsRenderedContent)
{
    if (!saw_window) {
        GTEST_SKIP() << "no window was tracked";
    }
    /* DOpus draws a menu bar, two file-list frames, and a button area.
     * Even with empty file lists, this should produce thousands of
     * non-grey pixels in the content region. */
    EXPECT_GT(content_pixels, 500)
        << "DOpus main window should contain rendered UI content "
           "(got " << content_pixels << " non-background pixels)\n"
        << startup_output;
}

TEST_F(DOpusTest, GadgetEnumerationIsSafe)
{
    if (!saw_window) {
        GTEST_SKIP() << "no window was tracked";
    }
    /* DOpus uses a custom-rendered UI: most "gadgets" are drawn by hand
     * and Intuition does not know about them.  The gadget count may be
     * zero or a small positive number, but the call must NOT crash and
     * must NOT return a negative value (which would indicate failure). */
    EXPECT_GE(gadget_count, 0)
        << "GetGadgetCount should return a non-negative value, got "
        << gadget_count;
}

TEST_F(DOpusTest, StartupCaptureProducesArtifact)
{
    if (!saw_window) {
        GTEST_SKIP() << "no window was tracked";
    }
    EXPECT_TRUE(capture_succeeded)
        << "DOpus startup capture must succeed (target: " << capture_path << ")";
    if (capture_succeeded) {
        std::error_code ec;
        const auto sz = std::filesystem::file_size(capture_path, ec);
        EXPECT_FALSE(ec) << "capture file must be readable: " << ec.message();
        if (!ec) {
            EXPECT_GT(static_cast<unsigned long long>(sz), 100ULL)
                << "capture PNG should contain non-trivial data";
        }
    }
}

/* ---- Z-tests: deeper interaction (run after startup tests) ----------- */

TEST_F(DOpusTest, ZRightMouseMenuProbeKeepsWindowAlive)
{
    if (!lxa_is_running() || !saw_window) {
        GTEST_SKIP() << "DOpus not running or no window tracked";
    }

    /* Press RMB inside the menu-bar band, drag a tiny distance, release.
     * lxa_inject_drag is atomic — we only verify that DOpus survives the
     * menu-open/close cycle. */
    const int menu_bar_x = std::min(window_info.x + 32,
                                    window_info.x + (int)window_info.width - 8);
    const int menu_bar_y = std::max(3, window_info.y / 2);
    const int item_y     = std::min(window_info.y + 20,
                                    window_info.y + (int)window_info.height - 8);

    lxa_inject_drag(menu_bar_x, menu_bar_y,
                    menu_bar_x, item_y,
                    LXA_MOUSE_RIGHT, 10);
    RunCyclesWithVBlank(30, 50000);
    FlushAndSettle();

    EXPECT_TRUE(lxa_is_running())
        << "DOpus must survive a right-mouse menu probe\n"
        << GetOutput();
    EXPECT_TRUE(WaitForWindows(1, 2000))
        << "DOpus main window must still be tracked after menu probe\n"
        << GetOutput();

    lxa_window_info_t post = {};
    ASSERT_TRUE(GetWindowInfo(0, &post));
    EXPECT_EQ(post.width,  window_info.width)
        << "Menu probe must not resize the window";
    EXPECT_EQ(post.height, window_info.height)
        << "Menu probe must not resize the window";
}

TEST_F(DOpusTest, ZWindowGeometryStableAcrossSettle)
{
    if (!saw_window) {
        GTEST_SKIP() << "no window was tracked";
    }
    /* Run additional settle cycles and confirm the window does not
     * mysteriously disappear or change size. */
    RunCyclesWithVBlank(60, 50000);
    FlushAndSettle();

    EXPECT_TRUE(lxa_is_running()) << GetOutput();
    EXPECT_TRUE(WaitForWindows(1, 2000)) << GetOutput();

    lxa_window_info_t after = {};
    ASSERT_TRUE(GetWindowInfo(0, &after));
    EXPECT_EQ(after.width,  window_info.width);
    EXPECT_EQ(after.height, window_info.height);
}

TEST_F(DOpusTest, ZContentPixelCountSurvivesIdlePeriod)
{
    if (!saw_window) {
        GTEST_SKIP() << "no window was tracked";
    }
    /* DOpus must keep its UI painted across an idle period.  This catches
     * regressions where an idle-time event (INTUITICKS, refresh) corrupts
     * the displayed content. */
    RunCyclesWithVBlank(60, 50000);
    FlushAndSettle();

    const int x1 = std::max(2, (int)window_info.x + 4);
    const int y1 = std::max(2, (int)window_info.y + 12);
    const int x2 = window_info.x + window_info.width  - 4;
    const int y2 = window_info.y + window_info.height - 4;
    const int after = CountContentPixels(x1, y1, x2, y2, 0);

    /* Allow some variation but require the content to persist. */
    EXPECT_GT(after, content_pixels / 2)
        << "DOpus UI content collapsed during idle period "
           "(before=" << content_pixels << ", after=" << after << ")\n"
        << GetOutput();
}

/* Phase 126: startup latency baseline sentinel */
TEST_F(DOpusTest, ZStartupLatency)
{
    ASSERT_GE(startup_ms_, 0) << "Startup time was not recorded";
    EXPECT_LE(startup_ms_, 15000L)
        << "DOpus startup latency " << startup_ms_ << " ms exceeds 15 s baseline";
    fprintf(stderr, "[LATENCY] DOpus startup: %ld ms\n", startup_ms_);
}

/* ---- Phase 130: text hook integration --------------------------------- */

/**
 * DOpusTextHookTest - self-contained fixture that installs the text hook
 * BEFORE DOpus starts drawing, so initial UI text (button labels, file
 * pane headers) is intercepted.
 *
 * Extends LxaTest to inherit lxa_init() + assign setup, but loads DOpus
 * manually after installing the hook.
 */
class DOpusTextHookTest : public LxaTest {
protected:
    std::vector<std::string> text_log_;
    bool loaded_ = false;

    void SetUp() override {
        LxaTest::SetUp();  /* lxa_init() + assigns */

        const char* apps = FindAppsPath();
        if (apps == nullptr) {
            GTEST_SKIP() << "lxa-apps directory not found";
        }

        const std::filesystem::path dopus_bin =
            std::filesystem::path(apps) / "DirectoryOpus" / "bin" / "DOPUS" / "DirectoryOpus";
        if (!std::filesystem::exists(dopus_bin)) {
            GTEST_SKIP() << "DirectoryOpus binary not found at " << dopus_bin.string();
        }

        /* Install hook before loading so we capture the initial draw. */
        lxa_set_text_hook([](const char *s, int n, int x, int y, void *ud) {
            if (n > 0) {
                std::string str(s, n);
                fprintf(stderr, "[TEXT-HOOK] x=%d y=%d n=%d str='%s'\n", x, y, n, str.c_str());
                static_cast<std::vector<std::string>*>(ud)->push_back(str);
            }
        }, &text_log_);

        ASSERT_EQ(lxa_load_program(
            "APPS:DirectoryOpus/bin/DOPUS/DirectoryOpus", ""), 0)
            << "Failed to load DirectoryOpus";

        ASSERT_TRUE(lxa_wait_windows(1, 15000))
            << "DirectoryOpus did not open a tracked window";

        /* Let the UI settle so button labels are drawn.
         * DOpus renders its button bank in response to a VBlank-driven timer
         * event after completing the initial UI skeleton.  The button bank
         * render starts around VBlank 400 after startup.  Run 600 VBlank
         * iterations (30M cycles, ~600ms on a real 50 Hz PAL Amiga) to give
         * dopus_task time to reach and complete the full button bank render. */
        for (int i = 0; i < 600; i++) {
            lxa_trigger_vblank();
            lxa_run_cycles(50000);
        }

        loaded_ = true;
    }

    void TearDown() override {
        lxa_clear_text_hook();
        LxaTest::TearDown();
    }
};

TEST_F(DOpusTextHookTest, DISABLED_TextHookCapturesKnownDOpusLabels)
{
    ASSERT_TRUE(loaded_) << "DOpus did not load correctly";

    /* Dump all captured text to stderr for diagnostic purposes. */
    std::string all;
    for (const auto &s : text_log_) all += s;
    fprintf(stderr, "[DOPUS-TEXT] %zu strings captured, combined: \"%s\"\n",
            text_log_.size(), all.c_str());

    ASSERT_FALSE(text_log_.empty())
        << "Text hook captured no strings during DOpus startup";

    /* Phase 134 goal: assert button-bank label words appear.
     * DOpus renders: Copy, Move, Rename, Makedir (main button bank rows)
     * plus the fixed clusters B/R/S/A and ?/E/F/C/I/Q.
     * Note: DOpus renders text character-by-character for the small
     * clusters, but multi-char strings for the main button bank. */
    bool has_multi_char = false;
    for (const auto &s : text_log_) {
        if (s.size() > 1) { has_multi_char = true; break; }
    }

    bool has_copy   = all.find("Copy")   != std::string::npos;
    bool has_move   = all.find("Move")   != std::string::npos;
    bool has_rename = all.find("Rename") != std::string::npos;

    fprintf(stderr, "[DOPUS-TEXT] has_multi_char=%d has_Copy=%d has_Move=%d has_Rename=%d\n",
            (int)has_multi_char, (int)has_copy, (int)has_move, (int)has_rename);

    /* Phase 134: button bank labels must be present. */
    EXPECT_TRUE(has_multi_char)
        << "DOpus should render multi-character button labels, not just single chars";
    EXPECT_TRUE(has_copy)
        << "DOpus button bank 'Copy' label missing from rendered text";
    EXPECT_TRUE(has_move)
        << "DOpus button bank 'Move' label missing from rendered text";
    EXPECT_TRUE(has_rename)
        << "DOpus button bank 'Rename' label missing from rendered text.\n"
           "Captured (" << text_log_.size() << " strings): "
        << [&]() {
               std::string out;
               int n = 0;
               for (const auto &s : text_log_) {
                   if (++n > 30) { out += "..."; break; }
                   out += '"'; out += s; out += "\" ";
               }
               return out;
           }();
}

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
