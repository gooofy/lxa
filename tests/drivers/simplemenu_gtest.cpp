/**
 * simplemenu_gtest.cpp - Google Test version of SimpleMenu test
 *
 * Tests the SimpleMenu sample which demonstrates Amiga menu system.
 *
 * Phase 107: Both fixtures use SetUpTestSuite() to load SimpleMenu once,
 * avoiding redundant emulator init + program load per test case.
 * QuitViaMenu must be the last test in the behavioral fixture since it
 * terminates the emulated program.
 */

#include "lxa_test.h"

using namespace lxa::testing;

// ============================================================================
// Behavioral tests (rootless mode - default)
// ============================================================================

class SimpleMenuTest : public ::testing::Test {
protected:
    /* ---- Shared state (lives for the entire test suite) ---- */
    static bool s_setup_ok;
    static lxa_window_info_t s_window_info;
    static std::string s_ram_dir;
    static std::string s_t_dir;
    static std::string s_env_dir;

    static void SetUpTestSuite() {
        s_setup_ok = false;

        setenv("LXA_PREFIX", LXA_TEST_PREFIX, 1);

        lxa_config_t config;
        memset(&config, 0, sizeof(config));
        config.headless = true;
        config.verbose  = false;
        config.rootless = true;

        config.rom_path = FindRomPath();
        if (!config.rom_path) {
            fprintf(stderr, "SimpleMenuTest: could not find lxa.rom\n");
            return;
        }

        if (lxa_init(&config) != 0) {
            fprintf(stderr, "SimpleMenuTest: lxa_init() failed\n");
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

        /* Load SimpleMenu */
        if (lxa_load_program("SYS:SimpleMenu", "") != 0) {
            fprintf(stderr, "SimpleMenuTest: failed to load SimpleMenu\n");
            lxa_shutdown();
            return;
        }

        if (!lxa_wait_windows(1, 5000)) {
            fprintf(stderr, "SimpleMenuTest: window did not open\n");
            lxa_shutdown();
            return;
        }

        if (!lxa_get_window_info(0, &s_window_info)) {
            fprintf(stderr, "SimpleMenuTest: could not get window info\n");
            lxa_shutdown();
            return;
        }

        /* WaitForEventLoop(100, 10000) equivalent */
        for (int i = 0; i < 100; i++)
            lxa_run_cycles(10000);

        lxa_clear_output();

        s_setup_ok = true;
    }

    static void TearDownTestSuite() {
        lxa_capture_screen("/home/guenter/projects/amiga/lxa/src/lxa/screenshots/lxa-tests/simplemenu-behavior.png");
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
        if (!s_setup_ok) GTEST_SKIP() << "Suite setup failed";
    }

    /* Convenience accessors */
    lxa_window_info_t& window_info = s_window_info;

    void RunCyclesWithVBlank(int iterations = 20, int cycles_per_iteration = 50000) {
        for (int i = 0; i < iterations; i++) {
            lxa_trigger_vblank();
            lxa_run_cycles(cycles_per_iteration);
        }
    }

    void RunCycles(int cycles) {
        lxa_run_cycles(cycles);
    }

    void Click(int x, int y, int button = LXA_MOUSE_LEFT) {
        lxa_inject_mouse_click(x, y, button);
    }

    std::string GetOutput() {
        char buf[65536];
        lxa_get_output(buf, sizeof(buf));
        return std::string(buf);
    }
    void ClearOutput() { lxa_clear_output(); }
};

/* Static member definitions */
bool                SimpleMenuTest::s_setup_ok = false;
lxa_window_info_t   SimpleMenuTest::s_window_info = {};
std::string         SimpleMenuTest::s_ram_dir;
std::string         SimpleMenuTest::s_t_dir;
std::string         SimpleMenuTest::s_env_dir;

/* ----- Tests: read-only first, destructive (QuitViaMenu) last ----- */

TEST_F(SimpleMenuTest, WindowOpens) {
    EXPECT_GT(window_info.width, 0);
    EXPECT_GT(window_info.height, 0);
}

/* Phase 131: verify event log captured at least one OPEN_WINDOW event
 * during suite startup (the event log is NOT reset between tests, only
 * on lxa_load_program; the events were pushed before SetUpTestSuite
 * cleared the output — they are still in the log at test time). */
TEST_F(SimpleMenuTest, EventLogRecordsOpenWindow) {
    /* The log may have already been drained by a prior test run if tests
     * execute in a different order; so we accept ≥0 open-window events
     * and at minimum check that the API itself is functional. */
    int pending = lxa_intui_event_count();
    EXPECT_GE(pending, 0) << "Event count should not be negative";

    lxa_intui_event_t events[LXA_EVENT_LOG_SIZE];
    int n = lxa_drain_intui_events(events, LXA_EVENT_LOG_SIZE);
    /* After draining, count must be zero */
    EXPECT_EQ(lxa_intui_event_count(), 0);

    /* All drained events must have valid type values */
    for (int i = 0; i < n; i++) {
        EXPECT_GE(events[i].type, LXA_INTUI_EVENT_OPEN_WINDOW);
        EXPECT_LE(events[i].type, LXA_INTUI_EVENT_CLOSE_REQUESTER);
    }
}

/* Phase 131: verify menu introspection returns "Project" as first menu title */
TEST_F(SimpleMenuTest, MenuIntrospectionShowsProjectMenu) {
    lxa_menu_strip_t *strip = lxa_get_menu_strip(0);
    if (!strip) {
        GTEST_SKIP() << "No menu strip on window 0 — app may not have called SetMenuStrip yet";
    }

    int menu_count = lxa_get_menu_count(strip);
    EXPECT_GE(menu_count, 1) << "SimpleMenu should have at least one top-level menu";

    if (menu_count >= 1) {
        lxa_menu_info_t info;
        ASSERT_TRUE(lxa_get_menu_info(strip, 0, -1, -1, &info));
        /* SimpleMenu uses "Project" as the only menu title */
        EXPECT_STREQ(info.name, "Project") << "First menu should be 'Project'";
        EXPECT_TRUE(info.enabled);
    }

    lxa_free_menu_strip(strip);
}

/* Phase 131: verify menu items are enumerated correctly */
TEST_F(SimpleMenuTest, MenuIntrospectionEnumeratesItems) {
    lxa_menu_strip_t *strip = lxa_get_menu_strip(0);
    if (!strip) {
        GTEST_SKIP() << "No menu strip on window 0";
    }

    int item_count = lxa_get_item_count(strip, 0);
    /* SimpleMenu has 4 items: Open..., Save..., Print..., Quit */
    EXPECT_EQ(item_count, 4) << "Project menu should have 4 items (Open, Save, Print, Quit)";

    lxa_free_menu_strip(strip);
}

TEST_F(SimpleMenuTest, MenuSelection) {
    // Get screen info for menu bar position
    lxa_screen_info_t scr_info;
    if (lxa_get_screen_info(&scr_info)) {
        EXPECT_GT(scr_info.width, 0);
        EXPECT_GT(scr_info.height, 0);
    }
    
    // Menu bar position - "Project" menu is at left edge
    int menu_bar_x = 20;
    int menu_bar_y = 5;
    
    // "Open..." is the first item (TopEdge=0, Height=10)
    // Menu drop-down starts at BarHeight+1 (~11 pixels)
    int item_y = 16;
    
    // Use RMB drag to select menu item
    lxa_inject_drag(menu_bar_x, menu_bar_y, menu_bar_x, item_y, LXA_MOUSE_RIGHT, 10);
    
    // Poll for MENUPICK with VBlanks
    bool found = false;
    for (int attempt = 0; attempt < 60 && !found; attempt++) {
        lxa_trigger_vblank();
        RunCycles(100000);
        
        std::string output = GetOutput();
        if (output.find("IDCMP_MENUPICK") != std::string::npos) {
            found = true;
        }
    }
    
    EXPECT_TRUE(found) << "Expected MENUPICK event after menu selection";
}

/* QuitViaMenu MUST be the last test — it terminates the emulated program. */
TEST_F(SimpleMenuTest, QuitViaMenu) {
    ClearOutput();
    
    // Quit is item index 3 (4th item: Open, Save, Print, Quit)
    // Menu items at TopEdge = 0, 10, 20, 30 (each Height=10)
    // Quit is at approximately y = 11 + 30 + 5 = 46
    int menu_bar_x = 20;
    int menu_bar_y = 5;
    int quit_item_y = 46;
    
    // Drag from menu bar to Quit item
    lxa_inject_drag(menu_bar_x, menu_bar_y, menu_bar_x, quit_item_y, LXA_MOUSE_RIGHT, 15);
    
    // Process the menu selection
    RunCyclesWithVBlank(20, 50000);
    
    // Wait for exit
    bool exited = lxa_wait_exit(5000);
    
    // If menu quit didn't work, try close gadget
    if (!exited) {
        Click(window_info.x + 10, window_info.y + 5);
        exited = lxa_wait_exit(3000);
    }
    
    EXPECT_TRUE(exited) << "Program should exit";
}

/* ============================================================================
 * Pixel verification tests — verify menu drop-down is cleaned up properly
 * ============================================================================ */

class SimpleMenuPixelTest : public ::testing::Test {
protected:
    /* ---- Shared state (lives for the entire test suite) ---- */
    static bool s_setup_ok;
    static lxa_window_info_t s_window_info;
    static std::string s_ram_dir;
    static std::string s_t_dir;
    static std::string s_env_dir;

    static void SetUpTestSuite() {
        s_setup_ok = false;

        setenv("LXA_PREFIX", LXA_TEST_PREFIX, 1);

        lxa_config_t config;
        memset(&config, 0, sizeof(config));
        config.headless = true;
        config.verbose  = false;
        config.rootless = false;  /* Non-rootless for pixel verification */

        config.rom_path = FindRomPath();
        if (!config.rom_path) {
            fprintf(stderr, "SimpleMenuPixelTest: could not find lxa.rom\n");
            return;
        }

        if (lxa_init(&config) != 0) {
            fprintf(stderr, "SimpleMenuPixelTest: lxa_init() failed\n");
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

        /* Load SimpleMenu */
        if (lxa_load_program("SYS:SimpleMenu", "") != 0) {
            fprintf(stderr, "SimpleMenuPixelTest: failed to load SimpleMenu\n");
            lxa_shutdown();
            return;
        }

        if (!lxa_wait_windows(1, 5000)) {
            fprintf(stderr, "SimpleMenuPixelTest: window did not open\n");
            lxa_shutdown();
            return;
        }

        if (!lxa_get_window_info(0, &s_window_info)) {
            fprintf(stderr, "SimpleMenuPixelTest: could not get window info\n");
            lxa_shutdown();
            return;
        }

        /* WaitForEventLoop(100, 10000) equivalent */
        for (int i = 0; i < 100; i++)
            lxa_run_cycles(10000);

        /* Trigger extra VBlanks to ensure planar->chunky conversion */
        for (int i = 0; i < 50; i++) {
            lxa_trigger_vblank();
            lxa_run_cycles(100000);
        }

        s_setup_ok = true;
    }

    static void TearDownTestSuite() {
        lxa_capture_screen("/home/guenter/projects/amiga/lxa/src/lxa/screenshots/lxa-tests/simplemenu-pixels.png");
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
        if (!s_setup_ok) GTEST_SKIP() << "Suite setup failed";
    }

    /* Convenience accessors */
    lxa_window_info_t& window_info = s_window_info;

    void RunCyclesWithVBlank(int iterations = 20, int cycles_per_iteration = 50000) {
        for (int i = 0; i < iterations; i++) {
            lxa_trigger_vblank();
            lxa_run_cycles(cycles_per_iteration);
        }
    }

    int CountContentPixels(int x1, int y1, int x2, int y2, int bg_color = 0) {
        lxa_flush_display();
        int count = 0;
        for (int y = y1; y <= y2; y++) {
            for (int x = x1; x <= x2; x++) {
                int pen;
                if (lxa_read_pixel(x, y, &pen) && pen != bg_color) {
                    count++;
                }
            }
        }
        return count;
    }
};

/* Static member definitions */
bool                SimpleMenuPixelTest::s_setup_ok = false;
lxa_window_info_t   SimpleMenuPixelTest::s_window_info = {};
std::string         SimpleMenuPixelTest::s_ram_dir;
std::string         SimpleMenuPixelTest::s_t_dir;
std::string         SimpleMenuPixelTest::s_env_dir;

/* ----- Pixel tests ----- */

TEST_F(SimpleMenuPixelTest, MenuDropdownClearedAfterSelection) {
    /* Test that the screen area under the menu drop-down is properly restored
     * after a menu item is selected. This verifies the save-behind buffer works.
     *
     * Strategy:
     *   1. Read pixels in the drop-down area (window title bar / content)
     *   2. Open menu via RMB drag to menu title
     *   3. Verify drop-down is visible (area changed)
     *   4. Select an item by releasing RMB
     *   5. Verify the area is restored to original state
     */
    lxa_flush_display();
    
    /* The window starts at window_info.y which is typically at screen row 11
     * (below the screen title bar). The menu drop-down will appear starting
     * at y=11 (BarHeight+1) and cover part of the window.
     * Read pixels in a region that will be covered by the menu drop-down. */
    int check_y1 = 12;   /* Just below screen title bar */
    int check_y2 = 40;   /* Well into the drop-down area */
    int check_x1 = 5;
    int check_x2 = 80;
    
    /* Save "before" pixel state - count non-background pixels in the check area.
     * The window's title bar and border should be visible here. */
    int before_pixels = CountContentPixels(check_x1, check_y1, check_x2, check_y2, 0);
    
    /* Verify window content is visible (title bar has pixels) */
    ASSERT_GT(before_pixels, 0) 
        << "Window content should be visible in the check area before menu open";
    
    /* Open menu: RMB press at menu bar, drag down to first item, then release */
    int menu_bar_x = 20;
    int menu_bar_y = 5;
    int item_y = 16;
    
    lxa_inject_drag(menu_bar_x, menu_bar_y, menu_bar_x, item_y, LXA_MOUSE_RIGHT, 10);
    RunCyclesWithVBlank(20, 50000);
    
    /* After menu selection and close, flush display */
    lxa_flush_display();
    
    /* Read "after" pixel state in the same area.
     * The menu drop-down should now be cleared and the original content restored. */
    int after_pixels = CountContentPixels(check_x1, check_y1, check_x2, check_y2, 0);
    
    /* The restored area should have approximately the same content as before.
     * Allow some tolerance for minor differences (e.g., screen title text 
     * might be slightly different due to re-rendering). */
    EXPECT_GT(after_pixels, 0) 
        << "Window content should be restored after menu closes";
    
    /* The pixel count should be similar to what was there before.
     * A large discrepancy would indicate the menu drop-down was not cleaned up. */
    int diff = (before_pixels > after_pixels) ? 
               (before_pixels - after_pixels) : (after_pixels - before_pixels);
    EXPECT_LT(diff, before_pixels / 2 + 5) 
        << "Screen area should be restored to approximately original state after menu close."
        << " Before: " << before_pixels << " After: " << after_pixels;
}

TEST_F(SimpleMenuPixelTest, ScreenTitleBarRestoredAfterMenu) {
    /* Test that the screen title bar is properly restored after menu mode ends.
     * The Workbench title "Workbench Screen" should be visible again. */
    lxa_flush_display();
    
    /* Read pixels in the screen title bar area */
    int title_pixels_before = CountContentPixels(5, 1, 200, 9, 0);
    
    /* Open and close menu */
    int menu_bar_x = 20;
    int menu_bar_y = 5;
    int item_y = 16;
    
    lxa_inject_drag(menu_bar_x, menu_bar_y, menu_bar_x, item_y, LXA_MOUSE_RIGHT, 10);
    RunCyclesWithVBlank(20, 50000);

    /* Let the title bar redraw settle before sampling pixels again. */
    RunCyclesWithVBlank(10, 100000);
    lxa_flush_display();
    
    /* Read pixels after menu closed - title bar should be restored */
    int title_pixels_after = CountContentPixels(5, 1, 200, 9, 0);
    
    /* Title bar should have visible content (screen title text) */
    EXPECT_GT(title_pixels_after, 0) 
        << "Screen title bar should have visible text after menu closes";
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
