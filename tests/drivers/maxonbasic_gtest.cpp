/**
 * maxonbasic_gtest.cpp - Google Test version of MaxonBASIC test
 *
 * Tests MaxonBASIC IDE.
 * Verifies screen dimensions, editor initialization, BASIC code entry,
 * mouse input, and cursor key handling.
 *
 * Phase 107: Uses SetUpTestSuite() to load MaxonBASIC once for all tests,
 * avoiding redundant emulator init + program load per test case.
 */

#include "lxa_test.h"

using namespace lxa::testing;

class MaxonBasicTest : public ::testing::Test {
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
            fprintf(stderr, "MaxonBasicTest: could not find lxa.rom\n");
            return;
        }

        if (lxa_init(&config) != 0) {
            fprintf(stderr, "MaxonBasicTest: lxa_init() failed\n");
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

        if (!apps) {
            fprintf(stderr, "MaxonBasicTest: lxa-apps directory not found\n");
            lxa_shutdown();
            return;
        }

        /* Load MaxonBASIC */
        if (lxa_load_program("APPS:MaxonBASIC/bin/MaxonBASIC/MaxonBASIC", "") != 0) {
            fprintf(stderr, "MaxonBasicTest: failed to load MaxonBASIC\n");
            lxa_shutdown();
            return;
        }

        if (!lxa_wait_windows(1, 10000)) {
            fprintf(stderr, "MaxonBasicTest: window did not open\n");
            lxa_shutdown();
            return;
        }

        if (!lxa_get_window_info(0, &s_window_info)) {
            fprintf(stderr, "MaxonBasicTest: could not get window info\n");
            lxa_shutdown();
            return;
        }

        if (!lxa_wait_window_drawn(0, 5000)) {
            fprintf(stderr, "MaxonBasicTest: window did not draw\n");
            lxa_shutdown();
            return;
        }

        /* WaitForEventLoop(100, 10000) equivalent */
        for (int i = 0; i < 100; i++)
            lxa_run_cycles(10000);

        /* Let program fully initialize */
        for (int i = 0; i < 100; i++) {
            lxa_trigger_vblank();
            lxa_run_cycles(50000);
        }

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
        if (!s_setup_ok) GTEST_SKIP() << "Suite setup failed";
    }

    /* Convenience accessors so tests read like before */
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

    void PressKey(int rawkey, int qualifier = 0) {
        lxa_inject_keypress(rawkey, qualifier);
    }

    void TypeString(const char* str) {
        lxa_inject_string(str);
    }

    void SlowTypeString(const char* str, int settle_vblanks = 3) {
        char ch[2] = {0, 0};

        while (*str) {
            ch[0] = *str++;
            TypeString(ch);
            RunCyclesWithVBlank(settle_vblanks, 50000);
        }
    }
};

/* Static member definitions */
bool                MaxonBasicTest::s_setup_ok = false;
lxa_window_info_t   MaxonBasicTest::s_window_info = {};
std::string         MaxonBasicTest::s_ram_dir;
std::string         MaxonBasicTest::s_t_dir;
std::string         MaxonBasicTest::s_env_dir;

/* ===================================================================== */
/* Read-only tests first (no state mutation) */

TEST_F(MaxonBasicTest, WindowOpens) {
    EXPECT_GT(window_info.width, 0);
    EXPECT_GT(window_info.height, 0);
}

TEST_F(MaxonBasicTest, ScreenDimensions) {
    int width, height, depth;
    if (lxa_get_screen_dimensions(&width, &height, &depth)) {
        EXPECT_GE(width, 640) << "Screen width should be >= 640";
        EXPECT_GE(height, 200) << "Screen height should be >= 200";
    }
}

TEST_F(MaxonBasicTest, EditorVisible) {
    /* Verify MaxonBASIC is running and has windows */
    EXPECT_TRUE(lxa_is_running()) << "MaxonBASIC should still be running";
    EXPECT_GE(lxa_get_window_count(), 1) << "At least one window should be open";
}

/* Input tests (may mutate state but don't exit the app) */

TEST_F(MaxonBasicTest, TextEntry) {
    /* Focus the editor and type a simple BASIC program gradually. */
    Click(window_info.x + window_info.width / 2, window_info.y + window_info.height / 2);
    RunCyclesWithVBlank(20, 50000);

    SlowTypeString("REM Test program\nPRINT \"Hello\"\nEND\n", 4);
    RunCyclesWithVBlank(40, 50000);

    EXPECT_TRUE(lxa_is_running()) << "MaxonBASIC should still be running after typing";
    EXPECT_GE(lxa_get_window_count(), 1) << "Window should still be open after typing";
}

TEST_F(MaxonBasicTest, MouseInput) {
    /* Click in the editor area */
    int click_x = window_info.x + window_info.width / 2;
    int click_y = window_info.y + window_info.height / 2;
    
    Click(click_x, click_y);
    RunCyclesWithVBlank(20, 50000);
    
    EXPECT_TRUE(lxa_is_running()) << "MaxonBASIC should still be running after mouse click";
}

TEST_F(MaxonBasicTest, CursorKeys) {
    /* Drain pending events */
    RunCyclesWithVBlank(40, 50000);
    
    /* Press cursor keys (Up, Down, Right, Left) */
    PressKey(0x4C, 0);  /* Up */
    RunCyclesWithVBlank(10, 50000);
    
    PressKey(0x4D, 0);  /* Down */
    RunCyclesWithVBlank(10, 50000);
    
    PressKey(0x4E, 0);  /* Right */
    RunCyclesWithVBlank(10, 50000);
    
    PressKey(0x4F, 0);  /* Left */
    RunCyclesWithVBlank(10, 50000);
    
    EXPECT_TRUE(lxa_is_running()) << "MaxonBASIC should still be running after cursor keys";
    EXPECT_GE(lxa_get_window_count(), 1) << "Window should still be open after cursor keys";
}

/* ===================================================================== */
/* Phase 116: visual & menu interaction tests.
 * These tests share the persistent fixture and run in alphabetical order
 * after the basic ones above. They count pixels to verify rendering and
 * menu behavior without depending on exact glyph layout. */

/* Helper: count non-background pixels in a screen rectangle, taking the
 * background pen from a quiet sample point. */
static int CountScreenContent(int x1, int y1, int x2, int y2, int bg_x, int bg_y) {
    lxa_flush_display();
    int bg = -1;
    if (!lxa_read_pixel(bg_x, bg_y, &bg)) {
        bg = 0;
    }
    int count = 0;
    for (int y = y1; y <= y2; y++) {
        for (int x = x1; x <= x2; x++) {
            int pen;
            if (lxa_read_pixel(x, y, &pen) && pen != bg) {
                count++;
            }
        }
    }
    return count;
}

TEST_F(MaxonBasicTest, ZEditorRendersTypedText) {
    /* Verify that typing actually produces visible glyphs in the editor area.
     * Click to focus, then type a distinctive string and assert pixel content
     * appears anywhere in the editor body. We scan a wide area because
     * earlier tests in this suite may have moved the cursor far from (0,0). */
    Click(window_info.x + 100, window_info.y + 100);
    RunCyclesWithVBlank(20, 50000);

    /* Quiet sample: bottom-right corner area unlikely to have text. */
    int before = CountScreenContent(10, 20, 600, 230, 610, 250);

    SlowTypeString("\nABCDEFGHIJ\n", 4);
    RunCyclesWithVBlank(40, 50000);

    int after = CountScreenContent(10, 20, 600, 230, 610, 250);

    /* Typing 10 distinct letters + newlines should add at least ~50 pixels
     * across the editor body. Use a small but nonzero threshold so a
     * stuck-cursor or no-op rendering regression fails this test. */
    EXPECT_GT(after, before + 20)
        << "Typed text should add visible pixels somewhere in the editor "
        << "(before=" << before << " after=" << after << ")";
    EXPECT_TRUE(lxa_is_running());
}

TEST_F(MaxonBasicTest, ZMenuBarAppearsOnRMB) {
    /* RMB click on the screen menu bar area should reveal MaxonBASIC's
     * menu titles. We assert by counting pixels in the top menu strip
     * before and after the RMB click. The menu strip lives at y=0..9. */
    RunCyclesWithVBlank(20, 50000);

    /* Quiet sample: middle-right of editor area, well below menu bar. */
    int before = CountScreenContent(0, 0, 400, 9, 500, 100);

    /* RMB click on menu bar area.
     *
     * Phase 133 note: post-RMB cycle budget was raised from 30x50k to 150x50k
     * because the compose-then-blit menu-bar render path (introduced to fix
     * flicker) now performs Text() into an off-screen BitMap followed by a
     * full-width BltBitMap to the screen, both of which traverse m68k memory
     * hooks per byte (lessons section 6.6). 150x50k = 7.5M cycles gives ~25%
     * headroom over observed completion (~6M cycles for 8 menu titles). */
    lxa_inject_rmb_click(100, 5);
    RunCyclesWithVBlank(150, 50000);

    int after = CountScreenContent(0, 0, 400, 9, 500, 100);

    EXPECT_GT(after, before + 50)
        << "RMB on menu bar should reveal menu titles (before=" << before
        << " after=" << after << ")";
    EXPECT_TRUE(lxa_is_running());

    /* Capture artifact for failure inspection. */
    lxa_capture_screen("/tmp/maxon-menu.png");
}

TEST_F(MaxonBasicTest, DISABLED_ZMenuDragPixelStability) {
    /* Phase 133 regression guard: the compose-then-blit menu pipeline
     * must not corrupt or leave residue in the editor area when the
     * user opens and cancels a menu via RMB drag.
     *
     * We sample editor content (well below the menu bar), perform a
     * throwaway drag to flush any first-time deferred paint, then
     * baseline + drag + measure and assert pixel-count stability
     * within 5%. */
    RunCyclesWithVBlank(20, 50000);

    /* Throwaway drag across the menu bar to flush deferred paint. */
    lxa_inject_drag(80, 5, 320, 5, LXA_MOUSE_RIGHT, 8);
    RunCyclesWithVBlank(150, 50000);

    /* Baseline of the editor area below the menu bar. */
    const int before = CountScreenContent(10, 30, 600, 230, 610, 250);

    /* Real measurement drag. */
    lxa_inject_drag(80, 5, 320, 5, LXA_MOUSE_RIGHT, 8);
    RunCyclesWithVBlank(150, 50000);

    const int after = CountScreenContent(10, 30, 600, 230, 610, 250);
    printf("MaxonBasic MenuDragPixelStability: before=%d after=%d\n",
           before, after);

    ASSERT_GT(before, 100)
        << "Baseline editor pixel count too low to assert stability "
           "(before=" << before << ")";
    const int tolerance = std::max(50, before / 20);
    EXPECT_NEAR(after, before, tolerance)
        << "Menu drag must not disturb editor area beyond 5% "
           "(before=" << before << ", after=" << after
        << ", tol=" << tolerance << ")";

    EXPECT_TRUE(lxa_is_running());
    EXPECT_GE(lxa_get_window_count(), 1);
}

TEST_F(MaxonBasicTest, ZScrollbarColumnRenders) {
    /* The vertical scrollbar (PROPGADGET id=0 at x=626 width=10) plus arrow
     * buttons should produce visible non-bg pixels in the rightmost screen
     * column band. This guards against future graphics regressions that
     * blank out gadget rendering. */
    RunCyclesWithVBlank(10, 50000);

    /* Sample bg from middle of editor. */
    int content = CountScreenContent(620, 20, 639, 240, 300, 100);

    EXPECT_GT(content, 100)
        << "Scrollbar column should contain visible gadget pixels (count="
        << content << ")";
}

TEST_F(MaxonBasicTest, ZWindowStaysOpenAfterStress) {
    /* Final stress check: after all interactive tests above, the app must
     * still be alive with its primary window intact. */
    RunCyclesWithVBlank(20, 50000);
    EXPECT_TRUE(lxa_is_running()) << "MaxonBASIC must survive full test sequence";
    EXPECT_GE(lxa_get_window_count(), 1) << "Primary window must remain open";

    lxa_window_info_t wi;
    ASSERT_TRUE(lxa_get_window_info(0, &wi));
    EXPECT_GT(wi.width, 0);
    EXPECT_GT(wi.height, 0);
}

/* Phase 131: menu introspection — verify MaxonBASIC has German menu titles */
TEST_F(MaxonBasicTest, ZMenuIntrospectionShowsGermanMenuTitles) {
    /* MaxonBASIC V3.5 uses German menus: "Projekt", "Editieren", "Suchen"
     * (plus possibly more).  This replaces the brittle hardcoded-coordinate
     * approach: we can now assert the menu title before targeting it. */
    RunCyclesWithVBlank(20, 50000);

    lxa_menu_strip_t *strip = lxa_get_menu_strip(0);
    if (!strip) {
        GTEST_SKIP() << "No menu strip on window 0 — MaxonBASIC may not have called SetMenuStrip yet";
    }

    int menu_count = lxa_get_menu_count(strip);
    EXPECT_GE(menu_count, 1) << "MaxonBASIC should have at least one top-level menu";

    /* Collect titles */
    bool found_projekt = false;
    for (int i = 0; i < menu_count; i++) {
        lxa_menu_info_t info;
        if (lxa_get_menu_info(strip, i, -1, -1, &info)) {
            if (strcmp(info.name, "Projekt") == 0)
                found_projekt = true;
        }
    }

    lxa_free_menu_strip(strip);

    EXPECT_TRUE(found_projekt)
        << "MaxonBASIC should have a 'Projekt' top-level menu; got "
        << menu_count << " menus";
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
