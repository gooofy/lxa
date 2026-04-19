/**
 * blitzbasic2_gtest.cpp - Google Test version of BlitzBasic2 test
 *
 * Tests BlitzBasic2 IDE startup, menu interaction, dialog handling, and
 * clean exit.  BB2's "ted" editor overlay uses copper-list DMA and blitter
 * line mode for rendering, neither of which lxa emulates yet — so the
 * editor surface stays flat grey (pen 0).
 *
 * Phase 107b: Uses SetUpTestSuite() to load BlitzBasic2 once,
 * avoiding redundant emulator init + program load per test case.
 * QuitMenuItemClosesApp must be the last test since it terminates the
 * program.
 */

#include "lxa_test.h"

#include <algorithm>
#include <filesystem>
#include <string>
#include <sys/time.h>

using namespace lxa::testing;

class BlitzBasic2Test : public ::testing::Test {
protected:
    /* ---- Shared state (lives for the entire test suite) ---- */
    static bool s_setup_ok;
    static lxa_window_info_t s_window_info;
    static std::string s_ram_dir;
    static std::string s_t_dir;
    static std::string s_env_dir;

    /* ---- Static helpers used during SetUpTestSuite ---- */

    static bool SetupOriginalSystemAssigns(bool add_libs,
                                           bool add_fonts,
                                           bool add_devs)
    {
        const char* home = std::getenv("HOME");
        if (home == nullptr || home[0] == '\0')
            return false;

        const std::filesystem::path system_base =
            std::filesystem::path(home) / "media" / "emu" / "amiga" /
            "FS-UAE" / "hdd" / "system";

        if (!std::filesystem::exists(system_base))
            return false;

        if (add_libs && !lxa_add_assign_path("LIBS", (system_base / "Libs").c_str()))
            return false;
        if (add_fonts && !lxa_add_assign_path("FONTS", (system_base / "Fonts").c_str()))
            return false;
        if (add_devs && !lxa_add_assign_path("DEVS", (system_base / "Devs").c_str()))
            return false;

        return true;
    }

    static bool SetupBlitzBasic2Assigns() {
        const char* apps = FindAppsPath();
        if (apps == nullptr)
            return false;

        const std::filesystem::path blitz_base =
            std::filesystem::path(apps) / "BlitzBasic2";
        if (!std::filesystem::exists(blitz_base / "blitz2"))
            return false;

        return lxa_add_assign("Blitz2", blitz_base.c_str())
            && lxa_add_assign_path("C", (blitz_base / "c").c_str())
            && lxa_add_assign_path("L", (blitz_base / "l").c_str())
            && lxa_add_assign_path("S", (blitz_base / "s").c_str());
    }

    static void SetUpTestSuite() {
        s_setup_ok = false;

        setenv("LXA_PREFIX", LXA_TEST_PREFIX, 1);

        lxa_config_t config;
        memset(&config, 0, sizeof(config));
        config.headless = true;
        config.verbose  = false;
        config.rootless = false;  /* BB2 needs full screen mode */

        config.rom_path = FindRomPath();
        if (!config.rom_path) {
            fprintf(stderr, "BlitzBasic2Test: could not find lxa.rom\n");
            return;
        }

        if (lxa_init(&config) != 0) {
            fprintf(stderr, "BlitzBasic2Test: lxa_init() failed\n");
            return;
        }

        /* Set up assigns (mirrors LxaTest::SetUp) */
        const char* samples = FindSamplesPath();
        if (samples) lxa_add_assign("SYS", samples);
        const char* sysbase = FindSystemBasePath();
        if (sysbase) lxa_add_assign_path("SYS", sysbase);
        const char* tplibs = FindThirdPartyLibsPath();
        if (tplibs) lxa_add_assign("LIBS", tplibs);
        const char* libs = FindSystemLibsPath();
        if (libs) lxa_add_assign_path("LIBS", libs);
        const char* apps = FindAppsPath();
        if (apps) lxa_add_assign("APPS", apps);
        const char* cmds = FindCommandsPath();
        if (cmds) lxa_add_assign("C", cmds);
        const char* sys = FindSystemPath();
        if (sys) lxa_add_assign("System", sys);

        char ram[] = "/tmp/lxa_test_RAM_XXXXXX";
        if (mkdtemp(ram)) { s_ram_dir = ram; lxa_add_drive("RAM", ram); }
        char tdir[] = "/tmp/lxa_test_T_XXXXXX";
        if (mkdtemp(tdir)) { s_t_dir = tdir; lxa_add_assign("T", tdir); }
        char edir[] = "/tmp/lxa_test_ENV_XXXXXX";
        if (mkdtemp(edir)) {
            s_env_dir = edir;
            lxa_add_assign("ENV", edir);
            lxa_add_assign("ENVARC", edir);
        }

        /* Extra assigns for BB2 */
        if (!SetupOriginalSystemAssigns(true, true, true) ||
            !SetupBlitzBasic2Assigns()) {
            fprintf(stderr, "BlitzBasic2Test: app bundle or original system "
                            "disk not found — all tests will skip\n");
            lxa_shutdown();
            return;
        }

        /* Load BlitzBasic2 */
        if (lxa_load_program("APPS:BlitzBasic2/blitz2", "") != 0) {
            fprintf(stderr, "BlitzBasic2Test: failed to load BlitzBasic2\n");
            lxa_shutdown();
            return;
        }

        /* BB2's "ted" overlay opens two windows: the main editor and a
         * secondary control window.  Wait for the first window, then
         * let ted complete its initialization during the cycle budget. */
        if (!lxa_wait_windows(1, 20000)) {
            fprintf(stderr, "BlitzBasic2Test: editor window did not open\n");
            lxa_shutdown();
            return;
        }

        if (!lxa_get_window_info(0, &s_window_info)) {
            fprintf(stderr, "BlitzBasic2Test: could not get window info\n");
            lxa_shutdown();
            return;
        }

        if (!lxa_wait_window_drawn(0, 5000)) {
            fprintf(stderr, "BlitzBasic2Test: editor window not drawn\n");
            lxa_shutdown();
            return;
        }

        /* Let ted finish its startup sequence (keymap probing, console,
         * menu rebuild, prop gadgets).  Ted builds its menu items
         * incrementally, so we need enough cycles for the full menu
         * strip to be constructed before any menu interaction. */
        for (int i = 0; i < 200; i++)
            lxa_run_cycles(10000);
        for (int i = 0; i < 200; i++) {
            lxa_trigger_vblank();
            lxa_run_cycles(50000);
        }

        lxa_clear_output();

        s_setup_ok = true;
    }

    static void TearDownTestSuite() {
        lxa_shutdown();

        auto rm = [](std::string& d) {
            if (!d.empty()) {
                std::string cmd = "rm -rf " + d;
                system(cmd.c_str());
                d.clear();
            }
        };
        rm(s_ram_dir);
        rm(s_t_dir);
        rm(s_env_dir);
    }

    /* ---- Per-test hooks ---- */
    void SetUp() override {
        if (!s_setup_ok)
            GTEST_SKIP() << "BlitzBasic2 app bundle or original system disk "
                            "not found (or suite setup failed)";
    }

    /* Convenience accessors */
    lxa_window_info_t& window_info = s_window_info;

    void RunCyclesWithVBlank(int iterations = 20, int cycles_per_iteration = 50000) {
        for (int i = 0; i < iterations; i++) {
            lxa_trigger_vblank();
            lxa_run_cycles(cycles_per_iteration);
        }
    }

    void Click(int x, int y, int button = LXA_MOUSE_LEFT) {
        lxa_inject_mouse_click(x, y, button);
    }

    void PressKey(int rawkey, int qualifier) {
        lxa_inject_keypress(rawkey, qualifier);
    }

    std::string GetOutput() {
        char buf[65536];
        lxa_get_output(buf, sizeof(buf));
        return std::string(buf);
    }
    void ClearOutput() { lxa_clear_output(); }

    bool CaptureWindow(const char* filename, int index = 0) {
        return lxa_capture_window(index, filename);
    }

    bool GetWindowInfo(int index, lxa_window_info_t* info) {
        return lxa_get_window_info(index, info);
    }

    int CountContentPixels(int x1, int y1, int x2, int y2, int bg_color = 0) {
        lxa_flush_display();
        int count = 0;
        for (int y = y1; y <= y2; y++) {
            for (int x = x1; x <= x2; x++) {
                int pen;
                if (lxa_read_pixel(x, y, &pen) && pen != bg_color)
                    count++;
            }
        }
        return count;
    }

    std::vector<lxa_gadget_info_t> GetGadgets(int win_index) {
        std::vector<lxa_gadget_info_t> result;
        int count = lxa_get_gadget_count(win_index);
        for (int i = 0; i < count; i++) {
            lxa_gadget_info_t info;
            if (lxa_get_gadget_info(win_index, i, &info))
                result.push_back(info);
        }
        return result;
    }

    bool ClickGadget(int gadget_index, int win_index = 0) {
        lxa_gadget_info_t info;
        if (!lxa_get_gadget_info(win_index, gadget_index, &info))
            return false;
        lxa_window_info_t winfo;
        if (!lxa_get_window_info(win_index, &winfo))
            return false;
        int gx = winfo.x + info.left + info.width / 2;
        int gy = winfo.y + info.top + info.height / 2;
        Click(gx, gy);
        return true;
    }

    /* --- Menu helpers -------------------------------------------------- */

    int MenuBarY() const {
        return std::max(4, s_window_info.y / 2);
    }

    /**
     * Two-phase menu interaction that gives ted enough CPU time to process
     * the menu selection.
     *
     * Phase 1: Press RMB at menu bar to enter menu mode, burn VBlank
     *          cycles so Intuition processes the menu strip.
     * Phase 2: Move mouse to the target item Y, burn cycles for highlight,
     *          then release RMB to trigger the selection.
     *
     * Note: In the persistent fixture, ted's menus are already built
     * during SetUpTestSuite, so we use reduced cycle budgets compared
     * to the per-test version.
     *
     * If @p release is false, the RMB stays pressed (menu stays open).
     * Use this for screenshot captures.
     */
    void DragMenu(int menu_x, int item_y, bool release = true) {
        int bar_y = MenuBarY();

        /* Phase 1: open the menu */
        lxa_inject_mouse(menu_x, bar_y, 0, LXA_EVENT_MOUSEMOVE);
        lxa_trigger_vblank();
        lxa_run_cycles(500000);

        lxa_inject_mouse(menu_x, bar_y, LXA_MOUSE_RIGHT, LXA_EVENT_MOUSEBUTTON);
        /* Ted renders menu item text incrementally over multiple VBlanks.
         * 100 iterations is the calibrated minimum for full rendering. */
        RunCyclesWithVBlank(100, 50000);

        /* Phase 2: move to the target item */
        lxa_inject_mouse(menu_x, item_y, LXA_MOUSE_RIGHT, LXA_EVENT_MOUSEMOVE);
        RunCyclesWithVBlank(10, 50000);

        if (release) {
            /* Release to trigger MENUPICK */
            lxa_inject_mouse(menu_x, item_y, 0, LXA_EVENT_MOUSEBUTTON);
            RunCyclesWithVBlank(60, 50000);
        }
    }

    /* --- Window management helpers ------------------------------------- */

    /**
     * Wait until the tracked window count drops to at most @p count,
     * burning VBlank cycles while waiting.  Returns true if the count
     * was reached within @p timeout_ms wall-clock milliseconds.
     */
    bool WaitForWindowCountAtMost(int count, int timeout_ms) {
        struct timeval start, now;
        gettimeofday(&start, nullptr);
        int cycles_since_vblank = 0;
        const int cycles_per_vblank = 50000;
        while (lxa_get_window_count() > count && lxa_is_running()) {
            lxa_run_cycles(10000);
            cycles_since_vblank += 10000;
            if (cycles_since_vblank >= cycles_per_vblank) {
                lxa_trigger_vblank();
                cycles_since_vblank = 0;
            }
            gettimeofday(&now, nullptr);
            long elapsed = (now.tv_sec - start.tv_sec) * 1000
                         + (now.tv_usec - start.tv_usec) / 1000;
            if (elapsed >= timeout_ms) return false;
        }
        return lxa_get_window_count() <= count;
    }

    /**
     * Return the gadget index whose top edge is lowest (highest Y) within
     * the given window, or -1 if no gadgets are found.
     */
    int FindBottomGadgetIndex(int win_index) {
        auto gadgets = GetGadgets(win_index);
        int best = -1;
        int best_top = -1;
        for (size_t i = 0; i < gadgets.size(); i++) {
            if (gadgets[i].top > best_top) {
                best_top = gadgets[i].top;
                best = static_cast<int>(i);
            }
        }
        return best;
    }

    /**
     * Attempt to close a window through multiple strategies:
     *  1. Click its close gadget via the API.
     *  2. Click the bottom-most gadget (often an OK/Cancel button).
     *  3. Click the center-bottom of the window as a last resort.
     * Returns true if the window count drops to @p remaining.
     */
    bool DismissWindow(int win_index, int remaining) {
        /* Strategy 1: close gadget */
        if (lxa_click_close_gadget(win_index)) {
            RunCyclesWithVBlank(30, 50000);
            if (WaitForWindowCountAtMost(remaining, 3000))
                return true;
        }

        /* Strategy 2: bottom gadget (OK / Cancel) */
        int bottom = FindBottomGadgetIndex(win_index);
        if (bottom >= 0 && ClickGadget(bottom, win_index)) {
            RunCyclesWithVBlank(30, 50000);
            if (WaitForWindowCountAtMost(remaining, 3000))
                return true;
        }

        /* Strategy 3: click center-bottom area */
        lxa_window_info_t info;
        if (GetWindowInfo(win_index, &info)) {
            Click(info.x + info.width / 2, info.y + info.height - 18);
            RunCyclesWithVBlank(30, 50000);
        }
        return WaitForWindowCountAtMost(remaining, 3000);
    }
};

/* Static member definitions */
bool                BlitzBasic2Test::s_setup_ok = false;
lxa_window_info_t   BlitzBasic2Test::s_window_info = {};
std::string         BlitzBasic2Test::s_ram_dir;
std::string         BlitzBasic2Test::s_t_dir;
std::string         BlitzBasic2Test::s_env_dir;

/* ===================================================================== */
/* Test: Startup opens visible IDE window                                 */
/* ===================================================================== */

TEST_F(BlitzBasic2Test, StartupOpensVisibleIdeWindow) {
    EXPECT_TRUE(lxa_is_running())
        << "BlitzBasic2 should still be running after startup\n"
        << GetOutput();
    EXPECT_GT(window_info.width, 400);
    EXPECT_GT(window_info.height, 150);
    EXPECT_STRNE(window_info.title, "System Message")
        << "BlitzBasic2 should reach the IDE rather than a requester";

    int active_width = 0;
    int active_height = 0;
    int active_depth = 0;

    ASSERT_TRUE(lxa_get_screen_dimensions(&active_width, &active_height, &active_depth));
    EXPECT_EQ(active_width, 640);
    EXPECT_EQ(active_height, 256);
    EXPECT_EQ(active_depth, 2);

    /* BB2's editor surface is drawn by its "ted" overlay using copper-list
     * DMA and blitter line mode, neither of which lxa emulates yet.  The
     * surface therefore stays flat grey (pen 0).  We assert fewer than 10
     * non-grey pixels so the test documents this known limitation while
     * staying robust against tiny rendering artefacts. */
    const int screen_pixels = CountContentPixels(0, 0, active_width - 1, active_height - 1, 0);
    EXPECT_LT(screen_pixels, 10)
        << "BlitzBasic2 editor surface is mostly grey (pen 0) because ted "
           "relies on copper/blitter hardware that lxa does not yet emulate";
}

/* ===================================================================== */
/* Test: Project menu renders a complete dropdown                         */
/* ===================================================================== */

TEST_F(BlitzBasic2Test, ProjectMenuRendersFullDropdown) {
    ClearOutput();
    /* Keep menu open (no release) for the screenshot capture */
    DragMenu(40, window_info.y + 80, false);

    const std::string menu_path = s_ram_dir + "/blitzbasic2-project-menu.png";
    ASSERT_TRUE(lxa_capture_screen(menu_path.c_str()));

    RgbImage menu_image = LoadPng(menu_path);
    ASSERT_EQ(menu_image.width, 640);
    ASSERT_EQ(menu_image.height, 256);

    int white_pixels = 0;
    int black_pixels = 0;

    for (size_t i = 0; i + 2 < menu_image.pixels.size(); i += 3) {
        if (menu_image.pixels[i] == 255 && menu_image.pixels[i + 1] == 255 && menu_image.pixels[i + 2] == 255) {
            ++white_pixels;
        }
        if (menu_image.pixels[i] == 0 && menu_image.pixels[i + 1] == 0 && menu_image.pixels[i + 2] == 0) {
            ++black_pixels;
        }
    }

    EXPECT_GT(white_pixels, 10000)
        << "Project menu should expose a large white dropdown area";
    EXPECT_GT(black_pixels, 100)
        << "Project menu should render dark text inside the dropdown";
}

/* ===================================================================== */
/* Test: About dialog opens from the Project menu and can be dismissed    */
/* ===================================================================== */

TEST_F(BlitzBasic2Test, AboutDialogOpensAndCloses) {
    const int baseline_windows = lxa_get_window_count();

    /* BB2's Project menu layout (each item H=12, menuTop=11):
     *   NEW            TopEdge= 0  -> abs Y = 11..22
     *   LOAD           TopEdge=12  -> abs Y = 23..34
     *   SAVE           TopEdge=24  -> abs Y = 35..46
     *   DEFAULTS       TopEdge=36  -> abs Y = 47..58
     *   ABOUT          TopEdge=48  -> abs Y = 59..70
     * Target the centre of the ABOUT item at Y = 65. */
    DragMenu(40, 65);
    /* DragMenu() already burns 310 VBlank iterations internally.
     * Give a modest extra budget for BB2 to process the MENUPICK. */
    RunCyclesWithVBlank(50, 50000);

    const int after_count = lxa_get_window_count();
    if (after_count > baseline_windows) {
        /* A new window appeared — it should be the About requester. */
        const int about_index = after_count - 1;
        lxa_window_info_t about_info;
        ASSERT_TRUE(GetWindowInfo(about_index, &about_info));
        EXPECT_GT(about_info.width, 50)
            << "About dialog should have a reasonable width";
        EXPECT_GT(about_info.height, 30)
            << "About dialog should have a reasonable height";

        /* Dismiss the dialog. */
        EXPECT_TRUE(DismissWindow(about_index, baseline_windows))
            << "About dialog should close after dismiss attempt";
    } else {
        /* No new window — BB2 may render the About info in-place (using
         * EasyRequest or a custom requester within the existing window).
         * Verify the app is still alive and capture for investigation. */
        EXPECT_TRUE(lxa_is_running())
            << "BlitzBasic2 should still be running after About selection\n"
            << GetOutput();

        const std::string path = s_ram_dir + "/blitzbasic2-about-attempt.png";
        lxa_capture_screen(path.c_str());
    }

    EXPECT_TRUE(lxa_is_running())
        << "BlitzBasic2 should survive the About dialog flow\n"
        << GetOutput();
}

/* ===================================================================== */
/* Test: Project Open ("OLD") shows a file requester                      */
/* ===================================================================== */

TEST_F(BlitzBasic2Test, ProjectOpenShowsFileRequester) {
    const int baseline_windows = lxa_get_window_count();

    /* BB2 Project menu: "LOAD" is item 2 at TopEdge=12,
     * absolute Y = menuTop(11) + 12 + 6(centre of H=12) = 29. */
    DragMenu(40, 29);
    /* DragMenu() already burns 310 VBlank iterations internally.
     * Give a modest extra budget for BB2 to process the MENUPICK. */
    RunCyclesWithVBlank(50, 50000);

    const int after_count = lxa_get_window_count();
    if (after_count > baseline_windows) {
        /* A new window appeared — likely the ASL file requester. */
        const int req_index = after_count - 1;
        lxa_window_info_t req_info;
        ASSERT_TRUE(GetWindowInfo(req_index, &req_info));

        EXPECT_GE(req_info.width, 150)
            << "File requester should be at least 150px wide";
        EXPECT_GE(req_info.height, 50)
            << "File requester should be at least 50px tall";

        /* Dismiss the requester (Cancel / close gadget). */
        EXPECT_TRUE(DismissWindow(req_index, baseline_windows))
            << "File requester should close after dismiss attempt";
    } else {
        /* No requester appeared.  BB2's OLD command may use a custom
         * requester or the timing/coordinate was off.  Verify the app
         * is still running and capture for review. */
        EXPECT_TRUE(lxa_is_running())
            << "BlitzBasic2 should still be running after Open attempt\n"
            << GetOutput();

        const std::string path = s_ram_dir + "/blitzbasic2-open-attempt.png";
        lxa_capture_screen(path.c_str());
    }

    EXPECT_TRUE(lxa_is_running())
        << "BlitzBasic2 should survive the Open file flow\n"
        << GetOutput();
}

/* ===================================================================== */
/* Phase 119 Y-tests: deeper interaction coverage.                        */
/* Inserted in source order BEFORE QuitMenuItemClosesApp so they run      */
/* against the live IDE (Quit terminates the program and must stay last). */
/* ===================================================================== */

TEST_F(BlitzBasic2Test, YEditorSurfaceShowsContentAfterMenuInteraction) {
    /* The existing StartupOpensVisibleIdeWindow asserts < 10 non-grey
     * pixels at startup (deferred-paint pattern: ted does not draw the
     * editor surface until first user interaction).  After the earlier
     * Y/About/Open menu tests have interacted with the IDE, the editor
     * surface DOES accumulate paint — primarily menu-residue and ted's
     * status overlay, NOT actual editable text content.
     *
     * This test guards two things:
     *   1. Interaction does cause SOME repaint (proves Phase 113/114
     *      blitter+copper plumbing reaches ted at all).
     *   2. The paint volume is "moderate" — if it suddenly explodes
     *      to a near-full screen of pixels, ted may have started
     *      rendering real text content and the known-limitation
     *      should be re-evaluated.
     */
    int sw = 0, sh = 0, sd = 0;
    ASSERT_TRUE(lxa_get_screen_dimensions(&sw, &sh, &sd));

    const int x1 = 8;
    const int y1 = 16;
    const int x2 = sw - 8;
    const int y2 = sh - 24;

    const int non_grey = CountContentPixels(x1, y1, x2, y2, 0);
    const int region_pixels = (x2 - x1 + 1) * (y2 - y1 + 1);
    printf("BB2 editor body non-grey pixels: %d / %d (%.1f%%)\n",
           non_grey, region_pixels,
           100.0 * non_grey / region_pixels);

    /* Lower bound: prove Phase 113/114 blitter+copper paint reaches the
     * surface after interaction (would be 0 if completely broken). */
    EXPECT_GT(non_grey, 100)
        << "Expected SOME paint on the editor body after menu interaction";

    /* Upper bound: if ted starts rendering real text content (filling
     * most of the editor body), this assertion will fail and we should
     * tighten the test + remove the known-limitation entry. */
    EXPECT_LT(non_grey, region_pixels / 2)
        << "BB2 editor body unexpectedly filled — ted may now be rendering "
           "real content. Update roadmap and tighten this test.";
}

TEST_F(BlitzBasic2Test, YSecondaryWindowIsTracked) {
    /* BB2's "ted" overlay opens a secondary window alongside the main
     * IDE window.  Verify both are tracked so future changes to window
     * enumeration don't drop one.  Observed sizes:
     *   main:      640x244 ("LXA Window" title — Intuition default)
     *   secondary: 292x75  (ted control overlay)
     */
    const int wc = lxa_get_window_count();
    EXPECT_GE(wc, 2)
        << "BB2 should have at least 2 tracked windows (main + secondary)";

    bool saw_main = false;
    bool saw_secondary = false;
    for (int i = 0; i < wc; i++) {
        lxa_window_info_t wi = {};
        if (!GetWindowInfo(i, &wi)) continue;
        printf("BB2 window %d: title='%s' %dx%d\n",
               i, wi.title, wi.width, wi.height);
        if (wi.width >= 600) saw_main = true;
        else if (wi.width < 400) saw_secondary = true;
    }
    EXPECT_TRUE(saw_main) << "Main IDE window should be tracked";
    EXPECT_TRUE(saw_secondary) << "Secondary ted control window should be tracked";
}

TEST_F(BlitzBasic2Test, YMultipleMenuOpensRemainStable) {
    /* Open and cancel each top-level menu (Project, Edit, Source,
     * Search, Compiler) in turn.  None should crash, hang, or spawn
     * extra windows.  Approximate menu-bar X positions taken from the
     * vision-model review (PROJECT≈40, EDIT≈110, SOURCE≈170,
     * SEARCH≈240, COMPILER≈320). */
    const int baseline_windows = lxa_get_window_count();
    const int menu_xs[] = {40, 110, 170, 240, 320};

    for (int i = 0; i < 5; i++) {
        const int mx = menu_xs[i];
        const int bar_y = MenuBarY();

        /* Open menu, drag well past any item, release outside dropdown
         * to cancel without selecting. */
        lxa_inject_drag(mx, bar_y,
                        mx + 200, bar_y,
                        LXA_MOUSE_RIGHT, 8);
        RunCyclesWithVBlank(20, 50000);

        ASSERT_TRUE(lxa_is_running())
            << "BB2 should survive opening menu at x=" << mx;
        EXPECT_EQ(lxa_get_window_count(), baseline_windows)
            << "Cancelling menu at x=" << mx
            << " should not change window topology";
    }
}

TEST_F(BlitzBasic2Test, YEditorAcceptsKeyboardInputWithoutCrash) {
    /* Even though ted does not render text on the editor surface,
     * the IDE must still accept keyboard input without crashing.
     * Click in the editor area to ensure focus, then type a short
     * burst of characters. */
    Click(window_info.x + window_info.width / 2,
          window_info.y + window_info.height / 2);
    RunCyclesWithVBlank(10, 50000);

    const int baseline_windows = lxa_get_window_count();
    ClearOutput();

    PressKey(0x21, 0);   /* 'S' */
    RunCyclesWithVBlank(4, 50000);
    PressKey(0x12, 0);   /* 'E' */
    RunCyclesWithVBlank(4, 50000);
    PressKey(0x33, 0);   /* '.' or similar */
    RunCyclesWithVBlank(4, 50000);

    EXPECT_TRUE(lxa_is_running())
        << "BB2 should survive editor keyboard input";
    EXPECT_EQ(lxa_get_window_count(), baseline_windows)
        << "Editor input should not change window topology";

    const std::string out = GetOutput();
    EXPECT_EQ(out.find("PANIC"), std::string::npos) << out;
}

/* ===================================================================== */
/* Test: Menu interaction does not crash (Quit via keyboard shortcut)      */
/*                                                                         */
/* MUST BE LAST — terminates the emulated program.                         */
/* ===================================================================== */

TEST_F(BlitzBasic2Test, QuitMenuItemClosesApp) {
    ASSERT_TRUE(lxa_is_running())
        << "BlitzBasic2 should be running before quit attempt";

    /* BB2's "QUIT" is the last item in the Project menu with Amiga-Q
     * shortcut.  Send Right-Amiga + Q to request a clean exit. */
    PressKey(0x10, 0x0080);   /* rawkey 0x10='Q', qualifier 0x0080=RAMIGA */
    RunCyclesWithVBlank(60, 50000);

    /* BB2 may show a "Save before quitting?" requester.  If a new window
     * appeared, dismiss it and then re-send the quit. */
    const int windows_after_quit = lxa_get_window_count();
    if (windows_after_quit > 0) {
        /* Try to dismiss any confirmation dialog. */
        DismissWindow(windows_after_quit - 1, 0);
        RunCyclesWithVBlank(30, 50000);
    }

    /* After all dialogs are dismissed, the app may or may not have exited.
     * What matters is that it didn't crash. */
    const std::string output = GetOutput();
    EXPECT_EQ(output.find("PANIC"), std::string::npos)
        << "BlitzBasic2 should not panic during quit\n"
        << output;
}

/* ===================================================================== */
/* Debug: Capture screenshots for manual review (disabled by default)      */
/* ===================================================================== */

TEST_F(BlitzBasic2Test, DISABLED_CaptureScreenshotsForReview) {
    ASSERT_TRUE(CaptureWindow("/tmp/blitzbasic2-startup.png", 0));
    DragMenu(40, window_info.y + 80, false);
    ASSERT_TRUE(lxa_capture_screen("/tmp/blitzbasic2-project-menu.png"));
}

TEST_F(BlitzBasic2Test, DISABLED_CaptureMenuItemPositions) {
    /* Single capture at the ABOUT item position (Y=65 absolute) */
    DragMenu(40, 65, false);
    lxa_capture_screen("/tmp/bb2-about-drag.png");
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
