/**
 * talk2boopsi_gtest.cpp - Google Test version of Talk2Boopsi test
 *
 * Tests BOOPSI gadget communication (prop + string gadgets with ICA targets).
 * Verifies that the program opens, handles input without crashing, and
 * renders visible prop gadget content.
 *
 * Phase 107b: Both fixtures use SetUpTestSuite() to load Talk2Boopsi once,
 * avoiding redundant emulator init + program load per test case.
 * CloseWindowExitsCleanly must be the last behavioral test since it
 * terminates the program.
 */

#include "lxa_test.h"

using namespace lxa::testing;

namespace {

constexpr int PEN_GREY = 0;
constexpr int TITLE_BAR_HEIGHT = 11;

constexpr int PROP_LEFT = 5;
constexpr int PROP_TOP = 16;
constexpr int PROP_WIDTH = 10;
constexpr int PROP_HEIGHT = 80;

constexpr int STRING_LEFT = 20;
constexpr int STRING_TOP = 16;
constexpr int STRING_WIDTH = 45;
constexpr int STRING_HEIGHT = 18;

}  // namespace

// ============================================================================
// Behavioral tests (rootless mode - default)
// ============================================================================

class Talk2BoopsiTest : public ::testing::Test {
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
            fprintf(stderr, "Talk2BoopsiTest: could not find lxa.rom\n");
            return;
        }

        if (lxa_init(&config) != 0) {
            fprintf(stderr, "Talk2BoopsiTest: lxa_init() failed\n");
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

        /* Load Talk2Boopsi */
        if (lxa_load_program("SYS:Talk2Boopsi", "") != 0) {
            fprintf(stderr, "Talk2BoopsiTest: failed to load Talk2Boopsi\n");
            lxa_shutdown();
            return;
        }

        if (!lxa_wait_windows(1, 5000)) {
            fprintf(stderr, "Talk2BoopsiTest: window did not open\n");
            lxa_shutdown();
            return;
        }

        if (!lxa_get_window_info(0, &s_window_info)) {
            fprintf(stderr, "Talk2BoopsiTest: could not get window info\n");
            lxa_shutdown();
            return;
        }

        if (!lxa_wait_window_drawn(0, 5000)) {
            fprintf(stderr, "Talk2BoopsiTest: window not drawn\n");
            lxa_shutdown();
            return;
        }

        /* WaitForEventLoop(100, 10000) equivalent */
        for (int i = 0; i < 100; i++)
            lxa_run_cycles(10000);

        /* Extra VBlanks to let gadgets settle */
        for (int i = 0; i < 20; i++) {
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

    void TypeString(const char* str) {
        lxa_inject_string(str);
    }

    std::string GetOutput() {
        char buf[65536];
        lxa_get_output(buf, sizeof(buf));
        return std::string(buf);
    }
    void ClearOutput() { lxa_clear_output(); }
};

/* Static member definitions */
bool                Talk2BoopsiTest::s_setup_ok = false;
lxa_window_info_t   Talk2BoopsiTest::s_window_info = {};
std::string         Talk2BoopsiTest::s_ram_dir;
std::string         Talk2BoopsiTest::s_t_dir;
std::string         Talk2BoopsiTest::s_env_dir;

/* ----- Tests: non-destructive first, CloseWindowExitsCleanly last ----- */

TEST_F(Talk2BoopsiTest, StaysOpenUntilClosed) {
    RunCyclesWithVBlank(80, 50000);
    EXPECT_TRUE(lxa_is_running());
}

TEST_F(Talk2BoopsiTest, HandlesMouseAndKeyboardInputWithoutExiting) {
    int prop_x = window_info.x + PROP_LEFT + (PROP_WIDTH / 2);
    int prop_y = window_info.y + TITLE_BAR_HEIGHT + PROP_TOP + (PROP_HEIGHT / 2);
    int string_x = window_info.x + STRING_LEFT + (STRING_WIDTH / 2);
    int string_y = window_info.y + TITLE_BAR_HEIGHT + STRING_TOP + (STRING_HEIGHT / 2);

    Click(prop_x, prop_y);
    RunCyclesWithVBlank(20, 50000);

    Click(string_x, string_y);
    RunCyclesWithVBlank(20, 50000);

    TypeString("55");
    RunCyclesWithVBlank(40, 50000);

    EXPECT_TRUE(lxa_is_running());
}

/* CloseWindowExitsCleanly MUST be the last test — it terminates the program. */
TEST_F(Talk2BoopsiTest, CloseWindowExitsCleanly) {
    Click(window_info.x + 5, window_info.y + 5);
    RunCyclesWithVBlank(20, 50000);

    EXPECT_TRUE(lxa_wait_exit(5000));
}

// ============================================================================
// Pixel verification tests (non-rootless mode)
// ============================================================================

class Talk2BoopsiPixelTest : public ::testing::Test {
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
            fprintf(stderr, "Talk2BoopsiPixelTest: could not find lxa.rom\n");
            return;
        }

        if (lxa_init(&config) != 0) {
            fprintf(stderr, "Talk2BoopsiPixelTest: lxa_init() failed\n");
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

        /* Load Talk2Boopsi */
        if (lxa_load_program("SYS:Talk2Boopsi", "") != 0) {
            fprintf(stderr, "Talk2BoopsiPixelTest: failed to load Talk2Boopsi\n");
            lxa_shutdown();
            return;
        }

        if (!lxa_wait_windows(1, 5000)) {
            fprintf(stderr, "Talk2BoopsiPixelTest: window did not open\n");
            lxa_shutdown();
            return;
        }

        if (!lxa_get_window_info(0, &s_window_info)) {
            fprintf(stderr, "Talk2BoopsiPixelTest: could not get window info\n");
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

        lxa_flush_display();

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
bool                Talk2BoopsiPixelTest::s_setup_ok = false;
lxa_window_info_t   Talk2BoopsiPixelTest::s_window_info = {};
std::string         Talk2BoopsiPixelTest::s_ram_dir;
std::string         Talk2BoopsiPixelTest::s_t_dir;
std::string         Talk2BoopsiPixelTest::s_env_dir;

/* ----- Pixel tests ----- */

TEST_F(Talk2BoopsiPixelTest, PropGadgetRendersVisibleContent) {
    int content = CountContentPixels(window_info.x + PROP_LEFT,
                                     window_info.y + PROP_TOP,
                                     window_info.x + PROP_LEFT + PROP_WIDTH - 1,
                                     window_info.y + PROP_TOP + PROP_HEIGHT - 1,
                                     PEN_GREY);

    EXPECT_GT(content, 20);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
