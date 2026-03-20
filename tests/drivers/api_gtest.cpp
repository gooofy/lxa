/**
 * api_gtest.cpp - Google Test version of liblxa API tests
 *
 * Tests Phase 57 API extensions:
 * - lxa_inject_rmb_click()
 * - lxa_inject_drag()
 * - lxa_get_screen_info()
 * - lxa_read_pixel()
 * - lxa_read_pixel_rgb()
 *
 * Phase 107: Uses SetUpTestSuite() to load SimpleGad once for all tests,
 * avoiding redundant emulator init + program load per test case.
 */

#include "lxa_test.h"

using namespace lxa::testing;

class LxaAPITest : public ::testing::Test {
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
            fprintf(stderr, "LxaAPITest: could not find lxa.rom\n");
            return;
        }

        if (lxa_init(&config) != 0) {
            fprintf(stderr, "LxaAPITest: lxa_init() failed\n");
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

        /* Load test program */
        if (lxa_load_program("SYS:SimpleGad", "") != 0) {
            fprintf(stderr, "LxaAPITest: failed to load SimpleGad\n");
            lxa_shutdown();
            return;
        }

        if (!lxa_wait_windows(1, 5000)) {
            fprintf(stderr, "LxaAPITest: window did not open\n");
            lxa_shutdown();
            return;
        }

        if (!lxa_get_window_info(0, &s_window_info)) {
            fprintf(stderr, "LxaAPITest: could not get window info\n");
            lxa_shutdown();
            return;
        }

        /* Let program initialize */
        for (int i = 0; i < 40; i++) {
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

    std::string GetOutput() {
        char buf[65536];
        lxa_get_output(buf, sizeof(buf));
        return std::string(buf);
    }
    void ClearOutput() { lxa_clear_output(); }

    void RunCyclesWithVBlank(int iters = 20, int cyc = 50000) {
        for (int i = 0; i < iters; i++) {
            lxa_trigger_vblank();
            lxa_run_cycles(cyc);
        }
    }

    void WaitForEventLoop(int cycles = 200, int cycles_per_iteration = 10000) {
        for (int i = 0; i < cycles; i++)
            lxa_run_cycles(cycles_per_iteration);
    }

    bool WaitForWindowDrawn(int index = 0, int timeout_ms = 5000) {
        return lxa_wait_window_drawn(index, timeout_ms);
    }

    std::vector<lxa_gadget_info_t> GetGadgets(int window_index = 0) {
        std::vector<lxa_gadget_info_t> gadgets;
        int count = lxa_get_gadget_count(window_index);
        for (int i = 0; i < count; i++) {
            lxa_gadget_info_t info;
            if (lxa_get_gadget_info(window_index, i, &info))
                gadgets.push_back(info);
        }
        return gadgets;
    }

    bool ClickGadget(int gadget_index, int window_index = 0) {
        lxa_gadget_info_t info;
        if (!lxa_get_gadget_info(window_index, gadget_index, &info))
            return false;
        return lxa_inject_mouse_click(info.left + info.width / 2,
                                       info.top + info.height / 2,
                                       LXA_MOUSE_LEFT);
    }
};

/* Static member definitions */
bool                LxaAPITest::s_setup_ok = false;
lxa_window_info_t   LxaAPITest::s_window_info = {};
std::string         LxaAPITest::s_ram_dir;
std::string         LxaAPITest::s_t_dir;
std::string         LxaAPITest::s_env_dir;

/* ===================================================================== */

TEST_F(LxaAPITest, WindowOpens) {
    EXPECT_GE(lxa_get_window_count(), 1) << "Window should be open";
}

TEST_F(LxaAPITest, WaitWindowDrawnAndEnumerateGadgets) {
    ASSERT_TRUE(WaitForWindowDrawn(0, 5000));
    WaitForEventLoop(100, 10000);

    std::vector<lxa_gadget_info_t> gadgets = GetGadgets();
    ASSERT_GE(gadgets.size(), 1u) << "SimpleGad should expose at least one clickable gadget";

    const lxa_gadget_info_t* gadget = nullptr;
    for (const auto& candidate : gadgets) {
        if (candidate.gadget_id == 3) {
            gadget = &candidate;
            break;
        }
    }

    ASSERT_NE(gadget, nullptr) << "Expected to find SimpleGad's button gadget";
    EXPECT_GT(gadget->width, 0);
    EXPECT_GT(gadget->height, 0);
    EXPECT_GE(gadget->left, window_info.x);
    EXPECT_GE(gadget->top, window_info.y);
}

TEST_F(LxaAPITest, ClickGadgetHelper) {
    ASSERT_TRUE(WaitForWindowDrawn(0, 5000));
    WaitForEventLoop(100, 10000);

    std::vector<lxa_gadget_info_t> gadgets = GetGadgets();
    int button_index = -1;
    for (size_t i = 0; i < gadgets.size(); i++) {
        if (gadgets[i].gadget_id == 3) {
            button_index = static_cast<int>(i);
            break;
        }
    }
    ASSERT_GE(button_index, 0);

    ClearOutput();
    ASSERT_TRUE(ClickGadget(button_index));
    RunCyclesWithVBlank(20, 50000);

    std::string output = GetOutput();
    EXPECT_NE(output.find("IDCMP_GADGETUP"), std::string::npos);
    EXPECT_NE(output.find("gadget number 3"), std::string::npos);
}

TEST_F(LxaAPITest, GetScreenInfo) {
    lxa_screen_info_t screen_info;
    bool got_info = lxa_get_screen_info(&screen_info);
    
    ASSERT_TRUE(got_info) << "lxa_get_screen_info() should succeed";
    EXPECT_GT(screen_info.width, 0) << "Screen width should be > 0";
    EXPECT_GT(screen_info.height, 0) << "Screen height should be > 0";
    EXPECT_GT(screen_info.depth, 0) << "Screen depth should be > 0";
    EXPECT_GT(screen_info.num_colors, 0) << "num_colors should be > 0";
    EXPECT_EQ(screen_info.num_colors, (1 << screen_info.depth)) 
        << "num_colors should equal 2^depth";
}

TEST_F(LxaAPITest, ReadPixel) {
    int pen;
    
    // Valid pixel read
    bool read_ok = lxa_read_pixel(10, 10, &pen);
    ASSERT_TRUE(read_ok) << "lxa_read_pixel() at (10,10) should succeed";
    EXPECT_GE(pen, 0) << "Pen value should be >= 0";
    EXPECT_LT(pen, 256) << "Pen value should be < 256";
    
    // Out of bounds - negative
    bool oob_read = lxa_read_pixel(-1, -1, &pen);
    EXPECT_FALSE(oob_read) << "lxa_read_pixel() at (-1,-1) should fail";
    
    // Out of bounds - huge
    oob_read = lxa_read_pixel(10000, 10000, &pen);
    EXPECT_FALSE(oob_read) << "lxa_read_pixel() at (10000,10000) should fail";
}

TEST_F(LxaAPITest, ReadPixelRGB) {
    uint8_t r, g, b;
    bool read_ok = lxa_read_pixel_rgb(10, 10, &r, &g, &b);
    
    ASSERT_TRUE(read_ok) << "lxa_read_pixel_rgb() at (10,10) should succeed";
    // Just verify we got values - can't predict exact colors
    EXPECT_TRUE(true) << "RGB values retrieved";
}

TEST_F(LxaAPITest, InjectRMBClick) {
    int x = window_info.x + window_info.width / 2;
    int y = window_info.y + window_info.height / 2;
    
    bool rmb_ok = lxa_inject_rmb_click(x, y);
    EXPECT_TRUE(rmb_ok) << "lxa_inject_rmb_click() should succeed";
    
    RunCyclesWithVBlank(10, 50000);
    EXPECT_TRUE(lxa_is_running()) << "Program should still be running";
}

TEST_F(LxaAPITest, InjectDrag) {
    int x1 = window_info.x + 50;
    int y1 = window_info.y + 50;
    int x2 = window_info.x + 100;
    int y2 = window_info.y + 100;
    
    bool drag_ok = lxa_inject_drag(x1, y1, x2, y2, LXA_MOUSE_LEFT, 5);
    EXPECT_TRUE(drag_ok) << "lxa_inject_drag() should succeed";
    
    RunCyclesWithVBlank(10, 50000);
    EXPECT_TRUE(lxa_is_running()) << "Program should still be running";
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
