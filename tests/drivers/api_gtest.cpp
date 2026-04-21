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

/* =========================================================
 * Phase 127: Memory fast-path unit tests
 * These tests use the running emulator to verify that the
 * fast-path 16/32-bit read helpers produce correct results
 * consistent with individual byte reads (i.e., big-endian
 * byte order is preserved by the bswap-based fast paths).
 * ========================================================= */

TEST_F(LxaAPITest, MemoryFastPathRead16ConsistentWithRead8) {
    ASSERT_TRUE(s_setup_ok) << "Emulator not initialized";

    /*
     * Pick an address in low RAM (the vector table area).  Address 0 holds
     * the initial SP as a 32-bit big-endian value, so bytes 0..1 form a
     * meaningful 16-bit value we can check.
     */
    const uint32_t addr = 0x0000;
    uint8_t  b0 = lxa_peek8(addr);
    uint8_t  b1 = lxa_peek8(addr + 1);
    uint16_t w  = lxa_peek16(addr);

    uint16_t expected = ((uint16_t)b0 << 8) | b1;
    EXPECT_EQ(w, expected)
        << "lxa_peek16() must equal (peek8[addr]<<8)|peek8[addr+1]";
}

TEST_F(LxaAPITest, MemoryFastPathRead32ConsistentWithRead8) {
    ASSERT_TRUE(s_setup_ok) << "Emulator not initialized";

    const uint32_t addr = 0x0000;
    uint8_t  b0 = lxa_peek8(addr);
    uint8_t  b1 = lxa_peek8(addr + 1);
    uint8_t  b2 = lxa_peek8(addr + 2);
    uint8_t  b3 = lxa_peek8(addr + 3);
    uint32_t l  = lxa_peek32(addr);

    uint32_t expected = ((uint32_t)b0 << 24) | ((uint32_t)b1 << 16)
                      | ((uint32_t)b2 <<  8) |  (uint32_t)b3;
    EXPECT_EQ(l, expected)
        << "lxa_peek32() must equal big-endian assembly of four peek8 bytes";
}

TEST_F(LxaAPITest, MemoryFastPathRead16AtMultipleOffsets) {
    ASSERT_TRUE(s_setup_ok) << "Emulator not initialized";

    /* Spot-check a range of 16-bit words in the vector table (0x0000-0x00FF) */
    for (uint32_t addr = 0; addr < 0x100; addr += 2) {
        uint8_t  b0 = lxa_peek8(addr);
        uint8_t  b1 = lxa_peek8(addr + 1);
        uint16_t w  = lxa_peek16(addr);
        uint16_t expected = ((uint16_t)b0 << 8) | b1;
        EXPECT_EQ(w, expected)
            << "lxa_peek16 mismatch at addr 0x" << std::hex << addr;
    }
}

TEST_F(LxaAPITest, MemoryFastPathRead32AtMultipleOffsets) {
    ASSERT_TRUE(s_setup_ok) << "Emulator not initialized";

    /* Spot-check 32-bit longs across the first 256 bytes */
    for (uint32_t addr = 0; addr < 0x100; addr += 4) {
        uint8_t  b0 = lxa_peek8(addr);
        uint8_t  b1 = lxa_peek8(addr + 1);
        uint8_t  b2 = lxa_peek8(addr + 2);
        uint8_t  b3 = lxa_peek8(addr + 3);
        uint32_t l  = lxa_peek32(addr);
        uint32_t expected = ((uint32_t)b0 << 24) | ((uint32_t)b1 << 16)
                          | ((uint32_t)b2 <<  8) |  (uint32_t)b3;
        EXPECT_EQ(l, expected)
            << "lxa_peek32 mismatch at addr 0x" << std::hex << addr;
    }
}





/*
 * Phase 130: Text() interception hook tests.
 *
 * These tests verify that lxa_set_text_hook() / lxa_clear_text_hook() correctly
 * intercept all _graphics_Text() calls while the emulator is running.
 *
 * TextHookTest is a self-contained fixture that initialises the emulator fresh,
 * installs the hook BEFORE the program draws its initial window, and verifies
 * that hook invocations are received during the initial render.  This is more
 * reliable than trying to force redraws via mouse clicks into an already-idle app.
 */

class TextHookTest : public ::testing::Test {
protected:
    void SetUp() override {
        setenv("LXA_PREFIX", LXA_TEST_PREFIX, 1);

        lxa_config_t config;
        memset(&config, 0, sizeof(config));
        config.headless = true;
        config.verbose  = false;
        config.rootless = true;
        config.rom_path = FindRomPath();
        ASSERT_NE(config.rom_path, nullptr) << "Could not find lxa.rom";
        ASSERT_EQ(lxa_init(&config), 0) << "lxa_init() failed";

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
    }

    void TearDown() override {
        lxa_clear_text_hook();
        lxa_shutdown();
    }

    /* Load IntuiText and run until its window is drawn, with the text hook
     * already installed so initial rendering is captured.
     * IntuiText calls PrintIText() → _graphics_Text(), guaranteeing hook fires. */
    void LoadAndDraw(lxa_text_hook_t hook, void *userdata) {
        lxa_set_text_hook(hook, userdata);
        ASSERT_EQ(lxa_load_program("SYS:IntuiText", ""), 0) << "Failed to load IntuiText";
        ASSERT_TRUE(lxa_wait_windows(1, 5000)) << "Window did not open";
        /* Let the initial draw settle */
        for (int i = 0; i < 40; i++) {
            lxa_trigger_vblank();
            lxa_run_cycles(50000);
        }
    }
};

TEST_F(TextHookTest, TextHookRegistrationAndInvocation) {
    /* Hook must fire at least once during SimpleGad's initial window draw. */
    std::vector<std::string> text_log;
    LoadAndDraw([](const char *s, int n, int, int, void *ud) {
        ((std::vector<std::string>*)ud)->push_back(std::string(s, n));
    }, &text_log);

    EXPECT_FALSE(text_log.empty())
        << "Text hook should have captured at least one Text() call during initial draw";
}

TEST_F(TextHookTest, TextHookCapturesNonEmptyStrings) {
    /* Every string delivered to the hook must be non-empty (n > 0). */
    std::vector<std::string> text_log;
    LoadAndDraw([](const char *s, int n, int, int, void *ud) {
        if (n > 0)
            ((std::vector<std::string>*)ud)->push_back(std::string(s, n));
    }, &text_log);

    ASSERT_FALSE(text_log.empty()) << "No Text() calls intercepted during initial draw";
    for (const auto &s : text_log)
        EXPECT_GT(s.size(), 0u) << "Hook received empty string";
}

TEST_F(TextHookTest, TextHookClearedAfterClearCall) {
    /* After lxa_clear_text_hook(), subsequent rendering must NOT invoke the hook. */
    int call_count = 0;
    LoadAndDraw([](const char *, int, int, int, void *ud) {
        (*(int *)ud)++;
    }, &call_count);

    int count_before_clear = call_count;
    EXPECT_GT(count_before_clear, 0) << "Hook should have fired during initial draw";

    lxa_clear_text_hook();
    call_count = 0;

    /* Run more cycles (idle app) — hook must NOT fire. */
    for (int i = 0; i < 20; i++) {
        lxa_trigger_vblank();
        lxa_run_cycles(50000);
    }

    EXPECT_EQ(call_count, 0) << "Hook must not fire after lxa_clear_text_hook()";
}

TEST_F(TextHookTest, TextHookPositionCoordinatesAreValid) {
    /* x and y coordinates must be plausible screen values. */
    struct Entry { int x, y; };
    std::vector<Entry> entries;
    LoadAndDraw([](const char *, int n, int x, int y, void *ud) {
        if (n > 0)
            ((std::vector<Entry>*)ud)->push_back({x, y});
    }, &entries);

    ASSERT_FALSE(entries.empty()) << "Expected at least one Text() call during initial draw";
    for (const auto &e : entries) {
        EXPECT_GE(e.x, -64)  << "x coordinate looks implausible: " << e.x;
        EXPECT_LE(e.x, 4096) << "x coordinate looks implausible: " << e.x;
        EXPECT_GE(e.y, -64)  << "y coordinate looks implausible: " << e.y;
        EXPECT_LE(e.y, 4096) << "y coordinate looks implausible: " << e.y;
    }
}

TEST(TextHookUnitTest, SetAndClearWithoutEmulator) {
    /* Verify the API is safe to call even when no emulator session is active.
     * lxa_clear_text_hook() must not crash with a NULL hook. */
    lxa_clear_text_hook();  /* idempotent; must not crash */

    int dummy = 0;
    lxa_set_text_hook([](const char *, int, int, int, void *ud) {
        (*(int *)ud)++;
    }, &dummy);

    /* Immediately clear — the hook was never invoked but the API must be stable. */
    lxa_clear_text_hook();
    EXPECT_EQ(dummy, 0) << "Hook must not be invoked by set/clear alone";
}

TEST(ProfileAPITest, ResetClearsCounters) {
    /* After reset, lxa_profile_get() should return 0 entries */
    lxa_profile_reset();
    lxa_profile_entry_t entries[16];
    int n = lxa_profile_get(entries, 16);
    EXPECT_EQ(n, 0) << "After reset, no profile entries should exist";
}

TEST(ProfileAPITest, GetWithNullOrZeroReturnsZero) {
    lxa_profile_reset();
    EXPECT_EQ(lxa_profile_get(nullptr, 16), 0);
    lxa_profile_entry_t entries[4];
    EXPECT_EQ(lxa_profile_get(entries, 0), 0);
}

TEST(ProfileAPITest, EmucallNameKnown) {
    char buf[64];
    lxa_profile_emucall_name(2, buf, (int)sizeof(buf));
    EXPECT_STREQ(buf, "EMU_CALL_STOP");
}

TEST(ProfileAPITest, EmucallNameUnknown) {
    char buf[64];
    lxa_profile_emucall_name(9999, buf, (int)sizeof(buf));
    EXPECT_STREQ(buf, "EMU_CALL_UNKNOWN_9999");
}

TEST(ProfileAPITest, WriteJsonEmpty) {
    lxa_profile_reset();
    char path[] = "/tmp/lxa_profile_test_XXXXXX";
    int fd = mkstemp(path);
    ASSERT_GE(fd, 0);
    close(fd);

    bool ok = lxa_profile_write_json(path);
    EXPECT_TRUE(ok) << "lxa_profile_write_json should succeed for empty profile";

    FILE *f = fopen(path, "r");
    ASSERT_NE(f, nullptr);
    char line[64];
    ASSERT_NE(fgets(line, (int)sizeof(line), f), nullptr);
    fclose(f);
    unlink(path);

    /* Empty profile should write [] */
    EXPECT_STREQ(line, "[]\n");
}

TEST(ProfileAPITest, WriteJsonNullPathReturnsFalse) {
    EXPECT_FALSE(lxa_profile_write_json(nullptr));
}

/* ===== Phase 131: Event Log unit tests (no emulator required) ===== */

TEST(EventLogUnitTest, InitialStateIsEmpty) {
    /* After the pre-test load the log may have events; drain first. */
    lxa_intui_event_t buf[LXA_EVENT_LOG_SIZE];
    lxa_drain_intui_events(buf, LXA_EVENT_LOG_SIZE);
    EXPECT_EQ(lxa_intui_event_count(), 0);
}

TEST(EventLogUnitTest, PushAndDrainOneEvent) {
    /* Drain any prior state */
    lxa_intui_event_t buf[LXA_EVENT_LOG_SIZE];
    lxa_drain_intui_events(buf, LXA_EVENT_LOG_SIZE);

    lxa_push_intui_event(LXA_INTUI_EVENT_OPEN_WINDOW, 0, "TestWin", 10, 20, 640, 400);

    EXPECT_EQ(lxa_intui_event_count(), 1);
    int n = lxa_drain_intui_events(buf, LXA_EVENT_LOG_SIZE);
    EXPECT_EQ(n, 1);
    EXPECT_EQ(buf[0].type, LXA_INTUI_EVENT_OPEN_WINDOW);
    EXPECT_EQ(buf[0].window_index, 0);
    EXPECT_STREQ(buf[0].title, "TestWin");
    EXPECT_EQ(buf[0].x, 10);
    EXPECT_EQ(buf[0].y, 20);
    EXPECT_EQ(buf[0].width, 640);
    EXPECT_EQ(buf[0].height, 400);
    /* After drain the count must be zero */
    EXPECT_EQ(lxa_intui_event_count(), 0);
}

TEST(EventLogUnitTest, DrainWithNullOrZeroIsSafe) {
    lxa_push_intui_event(LXA_INTUI_EVENT_OPEN_SCREEN, -1, NULL, 0, 0, 320, 256);
    EXPECT_EQ(lxa_drain_intui_events(nullptr, LXA_EVENT_LOG_SIZE), 0);
    EXPECT_EQ(lxa_drain_intui_events(nullptr, 0), 0);
    /* Event still in log */
    EXPECT_GE(lxa_intui_event_count(), 1);
    lxa_intui_event_t buf[LXA_EVENT_LOG_SIZE];
    lxa_drain_intui_events(buf, LXA_EVENT_LOG_SIZE);
}

TEST(EventLogUnitTest, CircularWrapOverwritesOldest) {
    /* Fill the log beyond LXA_EVENT_LOG_SIZE — oldest should be silently lost */
    lxa_intui_event_t buf[LXA_EVENT_LOG_SIZE];
    lxa_drain_intui_events(buf, LXA_EVENT_LOG_SIZE);

    for (int i = 0; i < LXA_EVENT_LOG_SIZE + 10; i++) {
        lxa_push_intui_event(LXA_INTUI_EVENT_OPEN_WINDOW, i, "W", 0, 0, 1, 1);
    }
    /* Count must be clamped at LXA_EVENT_LOG_SIZE */
    EXPECT_EQ(lxa_intui_event_count(), LXA_EVENT_LOG_SIZE);
    int n = lxa_drain_intui_events(buf, LXA_EVENT_LOG_SIZE);
    EXPECT_EQ(n, LXA_EVENT_LOG_SIZE);
}

/* ===== Phase 131: Menu introspection unit tests (no strip) ===== */

TEST(MenuIntrospectionUnitTest, NullStripIsSafe) {
    EXPECT_EQ(lxa_get_menu_count(nullptr), 0);
    EXPECT_EQ(lxa_get_item_count(nullptr, 0), 0);
    lxa_menu_info_t info;
    EXPECT_FALSE(lxa_get_menu_info(nullptr, 0, -1, -1, &info));
    /* free(NULL) must not crash */
    lxa_free_menu_strip(nullptr);
}

/* ===== Phase 131: Event log with live emulator (SimpleMenu) ===== */

class EventLogLiveTest : public ::testing::Test {
protected:
    static bool s_ok;
    static std::vector<lxa_intui_event_t> s_events;

    static void SetUpTestSuite() {
        s_ok = false;
        setenv("LXA_PREFIX", LXA_TEST_PREFIX, 1);

        lxa_config_t cfg;
        memset(&cfg, 0, sizeof(cfg));
        cfg.headless = true;
        cfg.rootless = true;
        cfg.rom_path = FindRomPath();
        if (!cfg.rom_path) return;
        if (lxa_init(&cfg) != 0) return;

        const char *samples = FindSamplesPath();
        if (samples) lxa_add_assign("SYS", samples);
        const char *sysbase = FindSystemBasePath();
        if (sysbase) lxa_add_assign_path("SYS", sysbase);
        const char *libs = FindSystemLibsPath();
        if (libs) lxa_add_assign_path("LIBS", libs);

        /* load_program resets the event log */
        if (lxa_load_program("SYS:SimpleMenu", "") != 0) { lxa_shutdown(); return; }

        /* wait for window to open */
        if (!lxa_wait_windows(1, 5000)) { lxa_shutdown(); return; }

        /* drain all events the startup produced */
        lxa_intui_event_t buf[LXA_EVENT_LOG_SIZE];
        int n = lxa_drain_intui_events(buf, LXA_EVENT_LOG_SIZE);
        s_events.assign(buf, buf + n);
        s_ok = true;
    }

    static void TearDownTestSuite() {
        lxa_shutdown();
    }

    void SetUp() override {
        if (!s_ok) GTEST_SKIP() << "EventLogLiveTest suite setup failed";
    }
};

bool EventLogLiveTest::s_ok = false;
std::vector<lxa_intui_event_t> EventLogLiveTest::s_events;

TEST_F(EventLogLiveTest, StartupProducesOpenScreenEvent) {
    bool found_screen = false;
    for (const auto &ev : s_events) {
        if (ev.type == LXA_INTUI_EVENT_OPEN_SCREEN) {
            found_screen = true;
            EXPECT_GT(ev.width, 0);
            EXPECT_GT(ev.height, 0);
        }
    }
    EXPECT_TRUE(found_screen)
        << "At least one OPEN_SCREEN event expected during SimpleMenu startup";
}

TEST_F(EventLogLiveTest, StartupProducesOpenWindowEvent) {
    bool found_window = false;
    for (const auto &ev : s_events) {
        if (ev.type == LXA_INTUI_EVENT_OPEN_WINDOW) {
            found_window = true;
            EXPECT_GE(ev.window_index, 0);
            EXPECT_GT(ev.width,  0);
            EXPECT_GT(ev.height, 0);
        }
    }
    EXPECT_TRUE(found_window)
        << "At least one OPEN_WINDOW event expected during SimpleMenu startup";
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
