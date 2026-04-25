/**
 * mousetest_gtest.cpp - Google Test version of MouseTest
 *
 * Tests mouse input handling via IDCMP.
 * Verifies MOUSEMOVE, MOUSEBUTTONS (left/right down/up), and close gadget.
 *
 * Phase 107b: Uses SetUpTestSuite() to load MouseTest once,
 * avoiding redundant emulator init + program load per test case.
 * CloseGadgetExit must be the last test since it terminates the program.
 */

#include "lxa_test.h"

using namespace lxa::testing;

constexpr int TITLE_BAR_HEIGHT = 11;

class MouseTestTest : public ::testing::Test {
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
            fprintf(stderr, "MouseTestTest: could not find lxa.rom\n");
            return;
        }

        if (lxa_init(&config) != 0) {
            fprintf(stderr, "MouseTestTest: lxa_init() failed\n");
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

        /* Load MouseTest */
        if (lxa_load_program("SYS:MouseTest", "") != 0) {
            fprintf(stderr, "MouseTestTest: failed to load MouseTest\n");
            lxa_shutdown();
            return;
        }

        if (!lxa_wait_windows(1, 5000)) {
            fprintf(stderr, "MouseTestTest: window did not open\n");
            lxa_shutdown();
            return;
        }

        if (!lxa_get_window_info(0, &s_window_info)) {
            fprintf(stderr, "MouseTestTest: could not get window info\n");
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
        lxa_capture_screen("/home/guenter/projects/amiga/lxa/src/lxa/screenshots/lxa-tests/mousetest.png");
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
bool                MouseTestTest::s_setup_ok = false;
lxa_window_info_t   MouseTestTest::s_window_info = {};
std::string         MouseTestTest::s_ram_dir;
std::string         MouseTestTest::s_t_dir;
std::string         MouseTestTest::s_env_dir;

/* ----- Tests: non-destructive first, CloseGadgetExit last ----- */

TEST_F(MouseTestTest, WindowOpens) {
    EXPECT_GT(window_info.width, 0);
    EXPECT_GT(window_info.height, 0);
}

TEST_F(MouseTestTest, MouseMovement) {
    int move_x = window_info.x + 100;
    int move_y = window_info.y + TITLE_BAR_HEIGHT + 50;
    
    ClearOutput();
    lxa_inject_mouse(move_x, move_y, 0, LXA_EVENT_MOUSEMOVE);
    RunCyclesWithVBlank(20, 50000);
    
    for (int i = 0; i < 100; i++) RunCycles(10000);
    
    std::string output = GetOutput();
    EXPECT_NE(output.find("MouseMove"), std::string::npos) 
        << "Expected MOUSEMOVE event";
}

TEST_F(MouseTestTest, LeftButtonClick) {
    ClearOutput();
    
    int click_x = window_info.x + 100;
    int click_y = window_info.y + TITLE_BAR_HEIGHT + 50;
    
    Click(click_x, click_y);
    RunCyclesWithVBlank(20, 50000);
    for (int i = 0; i < 100; i++) RunCycles(10000);
    
    std::string output = GetOutput();
    EXPECT_NE(output.find("Left Button Down"), std::string::npos) 
        << "Expected left button down event";
    EXPECT_NE(output.find("Left Button Up"), std::string::npos) 
        << "Expected left button up event";
}

TEST_F(MouseTestTest, RightButtonClick) {
    ClearOutput();
    
    int click_x = window_info.x + 100;
    int click_y = window_info.y + TITLE_BAR_HEIGHT + 50;
    
    lxa_inject_mouse_click(click_x, click_y, LXA_MOUSE_RIGHT);
    RunCyclesWithVBlank(20, 50000);
    for (int i = 0; i < 100; i++) RunCycles(10000);
    
    std::string output = GetOutput();
    EXPECT_NE(output.find("Right Button Down"), std::string::npos) 
        << "Expected right button down event";
    EXPECT_NE(output.find("Right Button Up"), std::string::npos) 
        << "Expected right button up event";
}

/* CloseGadgetExit MUST be the last test — it terminates the emulated program. */
TEST_F(MouseTestTest, CloseGadgetExit) {
    ClearOutput();
    
    lxa_click_close_gadget(0);
    
    if (lxa_wait_exit(5000)) {
        std::string output = GetOutput();
        EXPECT_NE(output.find("Close gadget clicked"), std::string::npos) 
            << "Expected close event output";
        EXPECT_NE(output.find("Window closed"), std::string::npos) 
            << "Expected window closed output";
    }
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
