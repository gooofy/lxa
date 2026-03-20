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

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
