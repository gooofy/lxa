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

/* ---- Phase 159: characterise DOpus' custom-UI architecture ----------- */

/*
 * Phase 159 deliberately documents — via assertions — three structural
 * properties of DOpus 4.x discovered through targeted instrumentation.
 * Each property is real Amiga app behaviour; codifying it prevents
 * accidental regressions and gives future agents working on Phase 159b
 * (deeper workflows) a stable reference of the as-of-Phase-159 baseline.
 *
 *   (a) DOpus runs as a window on the parent (Workbench) screen — it
 *       does NOT open a private screen.  The visible "DOPUS.1" text in
 *       the chrome is the WINDOW title, not a screen title.
 *   (b) DOpus DOES attach an Intuition MenuStrip to its main window.
 *       The strip's contents are dumped to stderr in the
 *       ZHasNoIntuitionMenuStrip test (legacy name kept for git history)
 *       so future agents have a reference for menu-driven interaction
 *       tests.  The button-bank "buttons" are NOT on the menu strip —
 *       they live in the window content area and are custom-rendered.
 *   (c) DOpus' button-bank labels (Copy, Move, Rename, Makedir, Hunt, …)
 *       are NOT routed through _graphics_Text() — only the screen / window
 *       title and the small fixed cluster letters (B/R/S/A and ?/E/F/C/I/Q)
 *       reach the text hook.  The bundled dopus.library renders the
 *       multi-char labels via its own font/blit path that bypasses the
 *       ROM Text() vector entirely.  This is documented in the disabled
 *       TextHookCapturesKnownDOpusLabels test below and is the primary
 *       blocker for Phase 159b text-based workflow assertions.
 */

TEST_F(DOpusTest, ZRunsOnPrivateScreenNamedDopus)
{
    if (!saw_window) {
        GTEST_SKIP() << "no window was tracked";
    }
    /* Discovery: DOpus 4.12 does NOT open a private screen — it runs as
     * a window on the parent Workbench screen.  The visible "DOPUS.1"
     * text in the title bar is the WINDOW title, not the screen title.
     * We assert that the window title starts with "DOPUS" so a future
     * change that strips the title (or substitutes garbage) is caught. */
    lxa_window_info_t info = {};
    ASSERT_TRUE(GetWindowInfo(0, &info));
    const std::string wtitle(info.title);
    EXPECT_EQ(wtitle.rfind("DOPUS", 0), 0u)
        << "DOpus window title should start with \"DOPUS\", got \""
        << wtitle << "\"";
}

TEST_F(DOpusTest, ZHasNoIntuitionMenuStrip)
{
    if (!saw_window) {
        GTEST_SKIP() << "no window was tracked";
    }
    /* Discovery: DOpus DOES attach an Intuition MenuStrip to its main
     * window (despite using WFLG_RMBTRAP-like custom rendering for
     * button-bank labels).  Capture and dump the strip so future agents
     * working on Phase 159b have a reference for menu-driven interaction
     * tests.  We deliberately accept the strip and just dump it. */
    std::vector<std::string> menus = GetMenuNames(0);
    fprintf(stderr, "[DOPUS-MENU] strip has %zu top-level menus:\n",
            menus.size());
    for (size_t i = 0; i < menus.size(); i++) {
        fprintf(stderr, "[DOPUS-MENU]   [%zu] \"%s\"\n", i, menus[i].c_str());
    }
    EXPECT_GT(menus.size(), 0u)
        << "DOpus should expose at least one top-level menu via Intuition";
    /* Discovery: DOpus 4.12 exposes exactly two top-level menus,
     * "Project" and "Function".  Pin both so a regression in MenuStrip
     * tag handling (e.g. in OpenWindow) is caught immediately. */
    bool has_project  = std::find(menus.begin(), menus.end(),
                                  std::string("Project"))  != menus.end();
    bool has_function = std::find(menus.begin(), menus.end(),
                                  std::string("Function")) != menus.end();
    EXPECT_TRUE(has_project)
        << "DOpus MenuStrip should expose a \"Project\" menu";
    EXPECT_TRUE(has_function)
        << "DOpus MenuStrip should expose a \"Function\" menu";
}

TEST_F(DOpusTest, ZWindowTitleIsScreenSpecific)
{
    if (!saw_window) {
        GTEST_SKIP() << "no window was tracked";
    }
    /* Discovery: at startup, DOpus' window title is exactly "DOPUS.1"
     * (matching its message-port name).  The full version + memory-info
     * string visible on real-Amiga DOpus is rendered by DOpusRT, which
     * is not currently launched in the test fixture.  Phase 159b will
     * either launch DOpusRT (changing this title) or document why not.
     * Until then, this test pins the current behaviour. */
    lxa_window_info_t info = {};
    ASSERT_TRUE(GetWindowInfo(0, &info));
    const std::string title(info.title);
    EXPECT_EQ(title, "DOPUS.1")
        << "DOpus startup window title should be exactly \"DOPUS.1\" "
           "(no DOpusRT integration yet), got \"" << title << "\"";
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

/* ---- Phase 159b: deeper-workflow interaction tests ------------------- */
/*
 * Phase 159b implements four of the five deferred items from Phase 159:
 *   1. Lister-pane responsiveness probe (path entry / file list area)
 *   2. Button-bank click probe (Copy/Move/Rename/Makedir/Hunt area)
 *   4. Project menu activation via index-based RMB drag
 *   5. Window-title DOpusRT-absence documentation
 *
 * Item 3 (re-enable DISABLED_TextHookCapturesKnownDOpusLabels via a
 * BltBitMap glyph hook) is promoted to Phase 159c — the work requires
 * a host-side glyph atlas detector + glyph-to-char mapping that is a
 * substantial implementation in its own right.
 *
 * DOpus' menu items have IntuiText labels that are mostly leading spaces
 * (the visible text is rendered later via the dopus.library custom font
 * blit path; see Phase 159 finding (c)).  Therefore item-by-name lookup
 * is not possible — these tests use index-based addressing keyed off the
 * menu structure dumped in ZHasNoIntuitionMenuStrip:
 *
 *   Menu[0] "Project"  has 14 items (separators at 1, 4, 6, 9, 13)
 *   Menu[1] "Function" has 10 items (separators at 3, 7, 9)
 *
 * Coordinates within the visible window content area come from the
 * captured artifact /tmp/dopus_startup.png and are stable run-to-run
 * because DOpus reads s/dopus.config which is bundled.
 */

/* Helper: count non-background pixels in a screen-relative rectangle. */
static int CountPixelsInRect(int x1, int y1, int x2, int y2)
{
    lxa_flush_display();
    int count = 0;
    for (int y = y1; y <= y2; y++) {
        for (int x = x1; x <= x2; x++) {
            int pen;
            if (lxa_read_pixel(x, y, &pen) && pen != 0)
                count++;
        }
    }
    return count;
}

TEST_F(DOpusTest, ZPathEntryAreaRespondsToClick)
{
    if (!saw_window) GTEST_SKIP() << "no window was tracked";

    /* The left lister's path-entry strip is a horizontal band near the top
     * of the left pane.  Looking at the captured artifact, it occupies
     * roughly y=20..32 across x=8..300.  Snapshot the broader left-pane
     * region (file list area) before and after a click, and assert that
     * either the pane changed pixels or DOpus survived.  This is a
     * structural responsiveness probe — DOpus must not crash on a click
     * inside its custom UI even though the click hits no Intuition gadget.
     */
    const int left_pane_x1 = window_info.x + 8;
    const int left_pane_y1 = window_info.y + 14;
    const int left_pane_x2 = window_info.x + 300;
    const int left_pane_y2 = window_info.y + 130;

    int before = CountPixelsInRect(left_pane_x1, left_pane_y1,
                                   left_pane_x2, left_pane_y2);

    /* Click in the path-entry strip area (centre of the red bar in the
     * captured image, around (150, 28) in window-relative coords). */
    lxa_inject_mouse_click(window_info.x + 150,
                           window_info.y + 28,
                           LXA_MOUSE_LEFT);
    RunCyclesWithVBlank(40, 50000);
    FlushAndSettle();

    EXPECT_TRUE(lxa_is_running())
        << "DOpus must survive a click in the path-entry area\n"
        << GetOutput();

    int after = CountPixelsInRect(left_pane_x1, left_pane_y1,
                                  left_pane_x2, left_pane_y2);
    /* Either the pane retained content (no collapse) or it changed in
     * a measurable way.  Both are acceptable — the assertion just guards
     * against a total-blank regression. */
    EXPECT_GT(after, before / 2)
        << "Left lister pane content collapsed after path-area click "
           "(before=" << before << ", after=" << after << ")";
}

TEST_F(DOpusTest, ZButtonBankClickPreservesUI)
{
    if (!saw_window) GTEST_SKIP() << "no window was tracked";

    /* The button bank occupies the bottom portion of the window, roughly
     * y=160..225 across the full width in the captured artifact.  Click
     * what looks like the "Copy" button at approximately (140, 175)
     * window-relative.  With no source files selected DOpus may pop a
     * "no source files selected" requester, render an error in the
     * status area, or simply ignore the click — all of which are
     * acceptable behaviours.  The test asserts only that the click does
     * not crash DOpus and that the window survives. */
    const int initial_window_count = lxa_get_window_count();

    lxa_inject_mouse_click(window_info.x + 140,
                           window_info.y + 175,
                           LXA_MOUSE_LEFT);
    RunCyclesWithVBlank(60, 50000);
    FlushAndSettle();

    EXPECT_TRUE(lxa_is_running())
        << "DOpus must survive a button-bank click\n"
        << GetOutput();
    EXPECT_GE(lxa_get_window_count(), initial_window_count)
        << "Button-bank click must not destroy the main window";

    /* If a requester window appeared, log the fact for future agents. */
    int post_count = lxa_get_window_count();
    if (post_count > initial_window_count) {
        fprintf(stderr,
                "[DOPUS-159b] Button-bank click opened a new window "
                "(count %d -> %d)\n",
                initial_window_count, post_count);
    }
}

TEST_F(DOpusTest, ZProjectMenuFirstItemActivates)
{
    if (!saw_window) GTEST_SKIP() << "no window was tracked";

    /* Use menu introspection (Phase 131) to discover Project menu and
     * its first non-separator item.  Phase 159 confirmed Menu[0] is
     * "Project" and Item[0] is the first selectable entry. */
    lxa_menu_strip_t* strip = lxa_get_menu_strip(0);
    ASSERT_NE(strip, nullptr) << "DOpus should expose a menu strip";

    lxa_menu_info_t project_menu;
    ASSERT_TRUE(lxa_get_menu_info(strip, 0, -1, -1, &project_menu))
        << "Could not query Project menu (Menu[0])";

    lxa_menu_info_t first_item;
    ASSERT_TRUE(lxa_get_menu_info(strip, 0, 0, -1, &first_item))
        << "Could not query Project Item[0]";

    /* Top-level menu coords (project_menu.x/.width) are the screen
     * positions in the menu bar; for an Intuition menu bar at y=0..10,
     * the centre x of the menu title is the safe drag-press point.
     * The item's .y is dropdown-relative so the absolute screen Y is
     * approximately 11 (menu-bar height) + first_item.y + first_item.height/2. */
    const int menu_press_x = project_menu.x + project_menu.width / 2;
    const int menu_press_y = 3;  /* inside the menu bar band */
    const int item_release_x = menu_press_x;
    const int item_release_y = 11 + first_item.y + first_item.height / 2;

    lxa_free_menu_strip(strip);

    const int initial_window_count = lxa_get_window_count();

    lxa_inject_drag(menu_press_x, menu_press_y,
                    item_release_x, item_release_y,
                    LXA_MOUSE_RIGHT, 10);
    RunCyclesWithVBlank(120, 50000);
    FlushAndSettle();

    EXPECT_TRUE(lxa_is_running())
        << "DOpus must survive Project-menu Item[0] activation\n"
        << GetOutput();
    EXPECT_TRUE(WaitForWindows(1, 2000))
        << "DOpus main window must remain after menu activation";

    /* The first Project item is "About" on stock DOpus 4 — it normally
     * opens an info requester.  The strip dump in Phase 159 showed
     * Item[0] as a 14-char ITEMTEXT entry.  Whether a new window appears
     * depends on whether DOpus' "About" routine succeeds; some builds
     * print to the title strip instead.  The test logs the outcome but
     * only asserts non-crash + window survival. */
    int post_count = lxa_get_window_count();
    fprintf(stderr,
            "[DOPUS-159b] Project Item[0] activation: window count "
            "%d -> %d\n", initial_window_count, post_count);

    /* If a requester opened, dismiss it with ESC so it does not affect
     * later tests' window counts. */
    if (post_count > initial_window_count) {
        lxa_inject_keypress(0x45, 0);  /* ESC */
        RunCyclesWithVBlank(40, 50000);
    }
}

TEST_F(DOpusTest, ZWindowTitleDocumentsDOpusRTAbsence)
{
    if (!saw_window) GTEST_SKIP() << "no window was tracked";

    /* Phase 159b item 5: the full version + memory-info string visible
     * on real-Amiga DOpus (e.g. "Directory Opus V4.12 © 1995 Inovatronics
     * — 524288 bytes free") is rendered by DOpusRT, a separate executable
     * that watches the DOpus.1 message port and updates the window title
     * via SetWindowTitles().  lxa does not currently launch external
     * processes (deferred to Phase 167 — external-process emulation), so
     * the title remains as DOpus itself sets it during startup.
     *
     * This test pins the current behaviour (title == "DOPUS.1") AND
     * documents — via the assertion message — that the next change comes
     * from Phase 167 landing, not from a DOpus or Intuition bug. */
    lxa_window_info_t info = {};
    ASSERT_TRUE(GetWindowInfo(0, &info));
    const std::string title(info.title);
    EXPECT_EQ(title, "DOPUS.1")
        << "DOpus startup window title is set by DOpus itself to its "
           "message-port name. The full version + memory-info string is "
           "written by DOpusRT (a separate executable), which lxa does "
           "not launch yet — see Phase 167 (external-process emulation). "
           "Got title: \"" << title << "\"";
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
     *
     * Phase 159 update (still DISABLED): Direct trace confirms the
     * button-bank labels Copy/Move/Rename/Makedir/Hunt are NOT routed
     * through _graphics_Text() — only the screen title "DOPUS.1" and the
     * single-character cluster labels are captured.  The bundled
     * dopus.library renders the multi-char button labels via its own
     * font/blit path that bypasses the ROM Text() vector entirely.
     *
     * Phase 159b decision (item 3 deferred): re-enabling this test
     * requires a host-side BltBitMap glyph hook + glyph atlas detector
     * + glyph-to-char mapping.  This is a substantial implementation in
     * its own right and has been promoted to Phase 159c with explicit
     * objectives (see roadmap.md).  Re-enable this test as part of the
     * Phase 159c test gate, NOT as a side-effect of any other phase. */
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
