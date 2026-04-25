/**
 * simplegtgadget_gtest.cpp - Google Test version of SimpleGTGadget test
 *
 * Tests GadTools gadget creation and interaction.
 *
 * Phase 107: Both fixtures use SetUpTestSuite() to load SimpleGTGadget once,
 * avoiding redundant emulator init + program load per test case.
 * CloseWindow must be the last test in the behavioral fixture since it
 * terminates the emulated program. Tests that previously closed the window
 * (ClickButton, ClickCheckbox, ClickCycle, NumberGadgetAcceptsKeyboardInput)
 * have been refactored to verify gadget interaction without exiting.
 */

#include "lxa_test.h"

using namespace lxa::testing;

#define RAWKEY_7 0x07

/* Gadget positions from the sample:
 * BASE = 20 + WBorTop + (FontYSize + 1) = 20 + 11 + (8 + 1) = 40 (for lxa defaults)
 * Confirmed via log: "Created BUTTON_KIND gadget at 20,40 size 120x14"
 * 
 * Button:   (20, 40), size 120x14
 * Checkbox: (20, 60), size 26x11
 * Integer:  (80, 80), size 80x14   (LeftEdge was increased to 80 for PLACETEXT_LEFT)
 * Cycle:    (80, 100), size 120x14 (LeftEdge was increased to 80 for PLACETEXT_LEFT)
 */

// ============================================================================
// Behavioral tests (rootless mode - default)
// ============================================================================

class SimpleGTGadgetTest : public ::testing::Test {
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
            fprintf(stderr, "SimpleGTGadgetTest: could not find lxa.rom\n");
            return;
        }

        if (lxa_init(&config) != 0) {
            fprintf(stderr, "SimpleGTGadgetTest: lxa_init() failed\n");
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

        /* Load SimpleGTGadget */
        if (lxa_load_program("SYS:SimpleGTGadget", "") != 0) {
            fprintf(stderr, "SimpleGTGadgetTest: failed to load SimpleGTGadget\n");
            lxa_shutdown();
            return;
        }

        if (!lxa_wait_windows(1, 5000)) {
            fprintf(stderr, "SimpleGTGadgetTest: window did not open\n");
            lxa_shutdown();
            return;
        }

        if (!lxa_get_window_info(0, &s_window_info)) {
            fprintf(stderr, "SimpleGTGadgetTest: could not get window info\n");
            lxa_shutdown();
            return;
        }

        /* Wait for program to initialize, render, and flush startup output */
        for (int i = 0; i < 100; i++)
            lxa_run_cycles(10000);

        /* Extra VBlanks for rendering to settle */
        for (int i = 0; i < 30; i++) {
            lxa_trigger_vblank();
            lxa_run_cycles(100000);
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

    bool WaitForOutputContains(const char* needle, int iterations = 60, int vblanks = 2) {
        for (int i = 0; i < iterations; i++) {
            std::string output = GetOutput();
            if (output.find(needle) != std::string::npos) {
                return true;
            }
            RunCyclesWithVBlank(vblanks, 100000);
        }
        return GetOutput().find(needle) != std::string::npos;
    }
};

/* Static member definitions */
bool                SimpleGTGadgetTest::s_setup_ok = false;
lxa_window_info_t   SimpleGTGadgetTest::s_window_info = {};
std::string         SimpleGTGadgetTest::s_ram_dir;
std::string         SimpleGTGadgetTest::s_t_dir;
std::string         SimpleGTGadgetTest::s_env_dir;

/* ----- Tests: non-destructive first, CloseWindow last ----- */

TEST_F(SimpleGTGadgetTest, GadgetCreation) {
    std::string output = GetOutput();
    EXPECT_NE(output.find("Created BUTTON_KIND gadget"), std::string::npos);
    EXPECT_NE(output.find("Created CHECKBOX_KIND gadget"), std::string::npos);
    EXPECT_NE(output.find("Created INTEGER_KIND gadget"), std::string::npos);
    EXPECT_NE(output.find("Created CYCLE_KIND gadget"), std::string::npos);
}

TEST_F(SimpleGTGadgetTest, ClickButton) {
    // Button is at (20, 40) in the window, size 120x14.
    int clickX = window_info.x + 20 + 60;  // center of 120px wide button
    int clickY = window_info.y + 40 + 7;   // center of 14px tall button
    
    ClearOutput();
    Click(clickX, clickY);
    RunCyclesWithVBlank(20, 100000);

    std::string output = GetOutput();
    EXPECT_NE(output.find("gadget ID 1"), std::string::npos)
        << "Button click should produce gadget ID 1 event. Output: " << output;
}

TEST_F(SimpleGTGadgetTest, ClickCheckbox) {
    // Checkbox is at (20, 60) in the window, size 26x11.
    int clickX = window_info.x + 20 + 13;   // center of 26px wide gadget
    int clickY = window_info.y + 60 + 5;    // center of 11px tall gadget

    ClearOutput();
    Click(clickX, clickY);
    ASSERT_TRUE(WaitForOutputContains("gadget ID 2")) << GetOutput();

    std::string output = GetOutput();
    EXPECT_NE(output.find("gadget ID 2"), std::string::npos)
        << output;
}

TEST_F(SimpleGTGadgetTest, ClickCycle) {
    // Cycle is at (80, 100) in the window, size 120x14.
    int clickX = window_info.x + 80 + 60;  // center of 120px wide gadget
    int clickY = window_info.y + 100 + 7;  // center of 14px tall gadget

    ClearOutput();
    Click(clickX, clickY);
    RunCyclesWithVBlank(30, 100000);
    Click(clickX, clickY);
    RunCyclesWithVBlank(30, 100000);

    std::string output = GetOutput();
    EXPECT_NE(output.find("gadget ID 4"), std::string::npos)
        << output;
    EXPECT_NE(output.find("code 1"), std::string::npos)
        << output;
}

TEST_F(SimpleGTGadgetTest, NumberGadgetAcceptsKeyboardInput) {
    int clickX = window_info.x + 80 + 40;
    int clickY = window_info.y + 80 + 7;

    Click(clickX, clickY);
    RunCyclesWithVBlank(20, 100000);

    ClearOutput();
    PressKey(RAWKEY_7, 0);
    RunCyclesWithVBlank(20, 100000);
    PressKey(0x44, 0);
    RunCyclesWithVBlank(60, 100000);

    std::string output = GetOutput();
    EXPECT_NE(output.find("IDCMP_GADGETUP: gadget ID 3"), std::string::npos)
        << output;
}

/* CloseWindow MUST be the last test — it terminates the emulated program. */
TEST_F(SimpleGTGadgetTest, CloseWindow) {
    // Click close gadget (usually at top-left)
    Click(window_info.x + 5, window_info.y + 5);
    RunCyclesWithVBlank(20);
    
    // Program should exit now
    EXPECT_TRUE(lxa_wait_exit(2000)) << "Program should exit after closing window";
}

// ============================================================================
// Pixel verification tests (non-rootless mode)
// ============================================================================

class SimpleGTGadgetPixelTest : public ::testing::Test {
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
            fprintf(stderr, "SimpleGTGadgetPixelTest: could not find lxa.rom\n");
            return;
        }

        if (lxa_init(&config) != 0) {
            fprintf(stderr, "SimpleGTGadgetPixelTest: lxa_init() failed\n");
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

        /* Load SimpleGTGadget */
        if (lxa_load_program("SYS:SimpleGTGadget", "") != 0) {
            fprintf(stderr, "SimpleGTGadgetPixelTest: failed to load SimpleGTGadget\n");
            lxa_shutdown();
            return;
        }

        if (!lxa_wait_windows(1, 5000)) {
            fprintf(stderr, "SimpleGTGadgetPixelTest: window did not open\n");
            lxa_shutdown();
            return;
        }

        if (!lxa_get_window_info(0, &s_window_info)) {
            fprintf(stderr, "SimpleGTGadgetPixelTest: could not get window info\n");
            lxa_shutdown();
            return;
        }

        /* Wait for program to initialize */
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

    void Click(int x, int y, int button = LXA_MOUSE_LEFT) {
        lxa_inject_mouse_click(x, y, button);
    }

    void PressKey(int rawkey, int qualifier) {
        lxa_inject_keypress(rawkey, qualifier);
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
bool                SimpleGTGadgetPixelTest::s_setup_ok = false;
lxa_window_info_t   SimpleGTGadgetPixelTest::s_window_info = {};
std::string         SimpleGTGadgetPixelTest::s_ram_dir;
std::string         SimpleGTGadgetPixelTest::s_t_dir;
std::string         SimpleGTGadgetPixelTest::s_env_dir;

/* ----- Pixel tests ----- */

TEST_F(SimpleGTGadgetPixelTest, CheckboxBorderRendered) {
    // Checkbox gadget at (20, 60) size 26x11 should have a bevel border
    // The bevel border adds visible non-background pixels around the checkbox area
    lxa_flush_display();
    
    int checkbox_content = CountContentPixels(
        window_info.x + 20,
        window_info.y + 60,
        window_info.x + 20 + 25,  // 26px wide
        window_info.y + 60 + 10,  // 11px tall
        0  // background pen
    );
    EXPECT_GT(checkbox_content, 0)
        << "Checkbox should have visible border/bevel pixels (non-background content)";
}

TEST_F(SimpleGTGadgetPixelTest, CheckboxCheckmarkRenderedAfterClick) {
    int inner_x1 = window_info.x + 20 + 3;
    int inner_y1 = window_info.y + 60 + 2;
    int inner_x2 = window_info.x + 20 + 22;
    int inner_y2 = window_info.y + 60 + 8;
    int clickX = window_info.x + 20 + 13;
    int clickY = window_info.y + 60 + 5;

    lxa_flush_display();
    int before_pixels = CountContentPixels(inner_x1, inner_y1, inner_x2, inner_y2, 0);

    Click(clickX, clickY);
    RunCyclesWithVBlank(20, 100000);
    lxa_flush_display();

    int after_pixels = CountContentPixels(inner_x1, inner_y1, inner_x2, inner_y2, 0);
    EXPECT_GT(after_pixels, before_pixels + 3)
        << "Checkbox interior should gain checkmark pixels after clicking";
}

TEST_F(SimpleGTGadgetPixelTest, NumberGadgetShowsCursorAndTypedDigitImmediately) {
    int clickX = window_info.x + 80 + 40;
    int clickY = window_info.y + 80 + 7;
    int cursor_x1 = window_info.x + 84 + 16;
    int cursor_y1 = window_info.y + 82 + 1;
    int cursor_x2 = cursor_x1 + 4;
    int cursor_y2 = cursor_y1 + 7;
    int typed_x1 = window_info.x + 84 + 16;
    int typed_y1 = window_info.y + 82 + 1;
    int typed_x2 = typed_x1 + 12;
    int typed_y2 = typed_y1 + 7;

    lxa_flush_display();
    int before_cursor_pixels = CountContentPixels(cursor_x1, cursor_y1, cursor_x2, cursor_y2, 0);
    int before_typed_pixels = CountContentPixels(typed_x1, typed_y1, typed_x2, typed_y2, 0);

    Click(clickX, clickY);
    RunCyclesWithVBlank(20, 100000);
    lxa_flush_display();

    int active_cursor_pixels = CountContentPixels(cursor_x1, cursor_y1, cursor_x2, cursor_y2, 0);
    EXPECT_GT(active_cursor_pixels, before_cursor_pixels)
        << "Number gadget should draw its caret as soon as it becomes active";

    PressKey(RAWKEY_7, 0);
    RunCyclesWithVBlank(20, 100000);
    lxa_flush_display();

    int after_typed_pixels = CountContentPixels(typed_x1, typed_y1, typed_x2, typed_y2, 0);
    EXPECT_GT(after_typed_pixels, before_typed_pixels)
        << "Typed digit should be rendered before the number gadget loses focus";
}

/* ---- Phase 153a: Cycle gadget rendering tests (spec-correct geometry) ---- */

/* Cycle gadget layout in SimpleGTGadget:
 *   left=80, top=100 (relative to window inner area), size=120x14
 *   Per cycle_gadget_spec.md:
 *     CYCLEGLYPHWIDTH = 20 px
 *     Glyph border: LeftEdge = LEFTTRIM+2 = 6 (glyph polygon x=6..16, height=max(9,14-5)=9)
 *     Glyph vertical centre: top + (14-9)/2 = top+2
 *     Divider shadow: x=20, y=2..11 (gadHeight-3 = 11)
 *     Divider shine:  x=21, y=2..11
 *     Label centred in [22, gadWidth-1] = [22, 119]
 *     Right edge (x=108..119): text only, no dropdown arrow
 */
TEST_F(SimpleGTGadgetPixelTest, CycleGadgetIconRegionHasPixels)
{
    /* The glyph polygon occupies x=6..16 relative to gadget left (LeftEdge=6, glyph spans 11px).
     * We check x=left+6..left+16 has non-background pixels. */
    int gad_left = window_info.x + 80;
    int gad_top  = window_info.y + 100;

    int icon_x1 = gad_left + 6;
    int icon_y1 = gad_top  + 2;
    int icon_x2 = gad_left + 16;
    int icon_y2 = gad_top  + 11;

    lxa_flush_display();
    int icon_pixels = CountContentPixels(icon_x1, icon_y1, icon_x2, icon_y2, 0);
    EXPECT_GT(icon_pixels, 4)
        << "Cycle gadget glyph region (left+6..left+16) should contain arrow polyline pixels";
}

TEST_F(SimpleGTGadgetPixelTest, CycleGadgetDividerIsPresent)
{
    /* Per spec: divider shadow at x=CYCLEGLYPHWIDTH=20, shine at x=21,
     * both from y=2 to y=gadHeight-3=11. */
    int gad_left = window_info.x + 80;
    int gad_top  = window_info.y + 100;

    int div_x1 = gad_left + 20;
    int div_y1 = gad_top  + 2;
    int div_x2 = gad_left + 21;
    int div_y2 = gad_top  + 11;

    lxa_flush_display();
    int div_pixels = CountContentPixels(div_x1, div_y1, div_x2, div_y2, 0);
    EXPECT_GT(div_pixels, 3)
        << "Cycle gadget divider (left+20..21) should have visible pixels";
}

TEST_F(SimpleGTGadgetPixelTest, CycleGadgetNoDropDownArrowOnRightEdge)
{
    /* The rightmost inner region (last 12 pixels) should contain only
     * label text pixels (sparse), not a filled drop-down arrow block.
     * A drop-down arrow would fill a dense rectangle; text is sparse. */
    int gad_left = window_info.x + 80;
    int gad_top  = window_info.y + 100;

    int right_x1 = gad_left + 120 - 12;
    int right_y1 = gad_top  + 3;
    int right_x2 = gad_left + 120 - 2;
    int right_y2 = gad_top  + 10;

    lxa_flush_display();
    int right_pixels = CountContentPixels(right_x1, right_y1, right_x2, right_y2, 0);
    EXPECT_LE(right_pixels, 4)
        << "Cycle gadget right edge should not have a drop-down arrow (Amiga style = no dropdown)";
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
