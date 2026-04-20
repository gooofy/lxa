/**
 * sysinfo_gtest.cpp - Google Test driver for SysInfo
 *
 * Tests SysInfo hardware/software information tool.
 * Verifies screen dimensions, custom screen rendering, gadget tracking,
 * and button interactions (SPEED, MEMORY, BOARDS).
 *
 * Phase 108: Converted to SetUpTestSuite() persistent fixture to avoid
 * redundant emulator init + program load per test case.  Added MEMORY
 * and BOARDS button interaction tests.
 *
 * Phase 108c: Added ExecBase field verification tests (AttnFlags,
 * VBlankFrequency, MaxLocMem, EClockFrequency), meaningful content
 * checks for LIBRARIES view, and screenshot capture for visual review.
 */

#include "lxa_test.h"

#include <algorithm>
#include <string>
#include <time.h>

using namespace lxa::testing;

/* ExecBase field offsets (from NDK StructOffsets test) */
static constexpr uint32_t EXECBASE_OFF_MAXLOCMEM        = 62;
static constexpr uint32_t EXECBASE_OFF_ATTNFLAGS         = 296;
static constexpr uint32_t EXECBASE_OFF_VBLANKFREQUENCY   = 530;
static constexpr uint32_t EXECBASE_OFF_POWERSUPPLYFREQ   = 531;
static constexpr uint32_t EXECBASE_OFF_ECLOCKFREQUENCY   = 568;

/* AttnFlags constants (from exec/execbase.h) */
static constexpr uint16_t AFF_68010 = (1 << 0);
static constexpr uint16_t AFF_68020 = (1 << 1);
static constexpr uint16_t AFF_68030 = (1 << 2);

class SysInfoTest : public ::testing::Test {
protected:
    /* ---- Gadget IDs used by SysInfo ---- */
    static constexpr int GADGET_ID_QUIT = 1;
    static constexpr int GADGET_ID_SPEED = 2;
    static constexpr int GADGET_ID_DRIVES = 4;
    static constexpr int GADGET_ID_BOARDS = 6;
    static constexpr int GADGET_ID_MEMORY = 7;
    static constexpr int GADGET_ID_LIBRARIES = 9;
    static constexpr int GADGET_ID_EXPAND = 10;

    /* ---- Shared state (lives for the entire test suite) ---- */
    static bool s_setup_ok;
    static lxa_window_info_t s_window_info;
    static std::string s_ram_dir;
    static std::string s_t_dir;
    static std::string s_env_dir;
    static long s_startup_ms;   /* Phase 126: startup latency baseline */

    static void SetUpTestSuite()
    {
        s_setup_ok = false;

        setenv("LXA_PREFIX", LXA_TEST_PREFIX, 1);

        lxa_config_t config;
        memset(&config, 0, sizeof(config));
        config.headless = true;
        config.verbose  = false;
        config.rootless = true;

        config.rom_path = FindRomPath();
        if (!config.rom_path) {
            fprintf(stderr, "SysInfoTest: could not find lxa.rom\n");
            return;
        }

        if (lxa_init(&config) != 0) {
            fprintf(stderr, "SysInfoTest: lxa_init() failed\n");
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
            fprintf(stderr, "SysInfoTest: lxa-apps directory not found\n");
            lxa_shutdown();
            return;
        }

        /* Load SysInfo */
        struct timespec _t0, _t1;
        clock_gettime(CLOCK_MONOTONIC, &_t0);
        if (lxa_load_program("APPS:SysInfo/bin/SysInfo/SysInfo", "") != 0) {
            fprintf(stderr, "SysInfoTest: failed to load SysInfo\n");
            lxa_shutdown();
            return;
        }

        if (!lxa_wait_windows(1, 15000)) {
            fprintf(stderr, "SysInfoTest: window did not open\n");
            lxa_shutdown();
            return;
        }
        clock_gettime(CLOCK_MONOTONIC, &_t1);
        s_startup_ms = (_t1.tv_sec - _t0.tv_sec) * 1000L +
                       (_t1.tv_nsec - _t0.tv_nsec) / 1000000L;

        if (!lxa_get_window_info(0, &s_window_info)) {
            fprintf(stderr, "SysInfoTest: could not get window info\n");
            lxa_shutdown();
            return;
        }

        if (!lxa_wait_window_drawn(0, 5000)) {
            fprintf(stderr, "SysInfoTest: rootless window has no visible content\n");
            lxa_shutdown();
            return;
        }

        /* Let the program settle — generous budget for SysInfo's hardware
         * enumeration to complete and the screen to stabilise. */
        for (int i = 0; i < 240; i++) {
            lxa_trigger_vblank();
            lxa_run_cycles(50000);
        }

        s_setup_ok = true;
    }

    static void TearDownTestSuite()
    {
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
    void SetUp() override
    {
        if (!s_setup_ok) GTEST_SKIP() << "Suite setup failed";
    }

    /* Convenience accessors so tests read like before */
    lxa_window_info_t& window_info = s_window_info;

    void RunCyclesWithVBlank(int iterations = 20, int cycles_per_iteration = 50000)
    {
        for (int i = 0; i < iterations; i++) {
            lxa_trigger_vblank();
            lxa_run_cycles(cycles_per_iteration);
        }
    }

    /* ---- Image helpers ---- */
    static int CountNonBlackPixels(const RgbImage& image,
                                   int x1, int y1, int x2, int y2)
    {
        int count = 0;
        const int left   = std::max(0, x1);
        const int top    = std::max(0, y1);
        const int right  = std::min(x2, image.width - 1);
        const int bottom = std::min(y2, image.height - 1);

        for (int y = top; y <= bottom; ++y) {
            for (int x = left; x <= right; ++x) {
                size_t offset = (static_cast<size_t>(y) * static_cast<size_t>(image.width)
                                 + static_cast<size_t>(x)) * 3U;
                if (image.pixels[offset] != 0 ||
                    image.pixels[offset + 1] != 0 ||
                    image.pixels[offset + 2] != 0) {
                    ++count;
                }
            }
        }

        return count;
    }

    RgbImage CaptureMainWindow(const std::string& file_name)
    {
        const std::string path = s_ram_dir + "/" + file_name;
        EXPECT_TRUE(lxa_capture_window(0, path.c_str()))
            << "Failed to capture SysInfo main window to " << path;
        return LoadPng(path);
    }

    static int FindGadgetIndexById(uint16_t gadget_id)
    {
        int count = lxa_get_gadget_count(0);
        for (int i = 0; i < count; ++i) {
            lxa_gadget_info_t info;
            if (lxa_get_gadget_info(0, i, &info) && info.gadget_id == gadget_id) {
                return i;
            }
        }
        return -1;
    }

    static bool ClickGadget(int gadget_index, int window_index = 0)
    {
        lxa_gadget_info_t info;
        if (!lxa_get_gadget_info(window_index, gadget_index, &info))
            return false;
        int click_x = info.left + info.width / 2;
        int click_y = info.top + info.height / 2;
        return lxa_inject_mouse_click(click_x, click_y, LXA_MOUSE_LEFT);
    }

    static int CountImageDifferences(const RgbImage& before, const RgbImage& after,
                                     int x1, int y1, int x2, int y2)
    {
        int count = 0;
        const int left   = std::max(0, x1);
        const int top    = std::max(0, y1);
        const int right  = std::min(std::min(before.width, after.width) - 1, x2);
        const int bottom = std::min(std::min(before.height, after.height) - 1, y2);

        for (int y = top; y <= bottom; ++y) {
            for (int x = left; x <= right; ++x) {
                size_t offset = (static_cast<size_t>(y) * static_cast<size_t>(before.width)
                                 + static_cast<size_t>(x)) * 3U;
                if (before.pixels[offset] != after.pixels[offset] ||
                    before.pixels[offset + 1] != after.pixels[offset + 1] ||
                    before.pixels[offset + 2] != after.pixels[offset + 2]) {
                    ++count;
                }
            }
        }

        return count;
    }
};

/* Static member definitions */
bool                SysInfoTest::s_setup_ok = false;
lxa_window_info_t   SysInfoTest::s_window_info = {};
std::string         SysInfoTest::s_ram_dir;
std::string         SysInfoTest::s_t_dir;
std::string         SysInfoTest::s_env_dir;
long                SysInfoTest::s_startup_ms = -1;

/* ===================================================================== */
/* Read-only tests first (no state mutation)                             */
/* ===================================================================== */

TEST_F(SysInfoTest, StartupOpensAndDrawsMainWindow)
{
    int screen_width = 0;
    int screen_height = 0;
    int screen_depth = 0;

    EXPECT_TRUE(lxa_is_running())
        << "SysInfo should remain running after startup";
    EXPECT_EQ(window_info.width, 640);
    EXPECT_EQ(window_info.height, 256);
    EXPECT_STREQ(window_info.title, "LXA Window");
    EXPECT_GT(lxa_get_window_content(0), 10000)
        << "SysInfo should expose a large amount of visible rootless content";

    ASSERT_TRUE(lxa_get_screen_dimensions(&screen_width, &screen_height, &screen_depth));
    EXPECT_EQ(screen_width, 640);
    EXPECT_EQ(screen_height, 256);
    EXPECT_GE(screen_depth, 3);
}

TEST_F(SysInfoTest, ScreenPixelsUseMultiplePensAfterStartup)
{
    int screen_width = 0;
    int screen_height = 0;
    int screen_depth = 0;
    int unique_pen_count = 0;
    bool seen[8] = {};

    ASSERT_TRUE(lxa_get_screen_dimensions(&screen_width, &screen_height, &screen_depth));

    for (int y = 0; y < screen_height; y += 16) {
        for (int x = 0; x < screen_width; x += 16) {
            int pen = -1;
            ASSERT_TRUE(lxa_read_pixel(x, y, &pen));
            if (pen >= 0 && pen < 8 && !seen[pen]) {
                seen[pen] = true;
                ++unique_pen_count;
            }
        }
    }

    EXPECT_GE(unique_pen_count, 4)
        << "SysInfo should populate the custom screen with several distinct pens after startup";
}

TEST_F(SysInfoTest, MainWindowCaptureIncludesVisibleUiPixels)
{
    RgbImage image = CaptureMainWindow("sysinfo-main-window.png");
    const int all_pixels = CountNonBlackPixels(image, 0, 0,
                                                image.width - 1, image.height - 1);

    ASSERT_EQ(image.width, 640);
    ASSERT_EQ(image.height, 256);
    EXPECT_GT(all_pixels, 30000)
        << "SysInfo rootless capture should include a substantial amount of drawn UI content";
}

TEST_F(SysInfoTest, MainWindowCaptureContainsContentInKeyRegions)
{
    RgbImage image = CaptureMainWindow("sysinfo-main-window-regions.png");

    const int software_panel_pixels  = CountNonBlackPixels(image, 24, 24, 280, 182);
    const int hardware_panel_pixels  = CountNonBlackPixels(image, 332, 24, 620, 182);
    const int bottom_gadget_pixels   = CountNonBlackPixels(image, 20, 184, 620, 248);

    EXPECT_GT(software_panel_pixels, 12000)
        << "SysInfo should render visible software-list content in the left panel";
    EXPECT_GT(hardware_panel_pixels, 7000)
        << "SysInfo should render visible hardware summary content in the right panel";
    EXPECT_GT(bottom_gadget_pixels, 8000)
        << "SysInfo should render its lower gadget rows and status area";
}

TEST_F(SysInfoTest, MainWindowExposesTrackedGadgetsForPrimaryActions)
{
    int count = lxa_get_gadget_count(0);

    ASSERT_GE(count, 10)
        << "SysInfo should expose its main action gadgets through tracked Intuition gadgets";

    EXPECT_GE(FindGadgetIndexById(GADGET_ID_QUIT), 0);
    EXPECT_GE(FindGadgetIndexById(GADGET_ID_SPEED), 0);
    EXPECT_GE(FindGadgetIndexById(GADGET_ID_MEMORY), 0);
    EXPECT_GE(FindGadgetIndexById(GADGET_ID_BOARDS), 0);
    EXPECT_GE(FindGadgetIndexById(GADGET_ID_DRIVES), 0);
    EXPECT_GE(FindGadgetIndexById(GADGET_ID_LIBRARIES), 0);
    EXPECT_GE(FindGadgetIndexById(GADGET_ID_EXPAND), 0);
}

TEST_F(SysInfoTest, LibrariesGadgetIsTrackedViaScreenGadgetList)
{
    const int gadget_index = FindGadgetIndexById(GADGET_ID_LIBRARIES);

    ASSERT_GE(gadget_index, 0) << "SysInfo should expose the LIBRARIES gadget";
    EXPECT_GE(lxa_get_gadget_count(0), 15)
        << "SysInfo should expose both window gadgets and screen-level gadgets to host-side tests";
}

/* ===================================================================== */
/* ExecBase field verification                                           */
/*                                                                       */
/* Phase 108c: These tests read ExecBase fields directly from emulated   */
/* memory to confirm the ROM coldstart populates them correctly.  SysInfo */
/* (and other apps) depend on these fields for CPU/FPU detection,        */
/* PAL/NTSC detection, and memory reporting.                             */
/* ===================================================================== */

TEST_F(SysInfoTest, ExecBaseAttnFlagsReports68030)
{
    /* Read SysBase pointer from location 4 */
    uint32_t sysbase = lxa_peek32(4);
    ASSERT_NE(sysbase, 0u) << "SysBase (address 4) should be non-zero";

    uint16_t attn_flags = lxa_peek16(sysbase + EXECBASE_OFF_ATTNFLAGS);

    EXPECT_TRUE(attn_flags & AFF_68010)
        << "AttnFlags should have AFF_68010 set (implied by 68030)";
    EXPECT_TRUE(attn_flags & AFF_68020)
        << "AttnFlags should have AFF_68020 set (implied by 68030)";
    EXPECT_TRUE(attn_flags & AFF_68030)
        << "AttnFlags should have AFF_68030 set for the emulated CPU";
}

TEST_F(SysInfoTest, ExecBaseVBlankFrequencyIsPAL50Hz)
{
    uint32_t sysbase = lxa_peek32(4);
    ASSERT_NE(sysbase, 0u);

    uint8_t vblank_freq = lxa_peek8(sysbase + EXECBASE_OFF_VBLANKFREQUENCY);
    uint8_t power_freq  = lxa_peek8(sysbase + EXECBASE_OFF_POWERSUPPLYFREQ);

    EXPECT_EQ(vblank_freq, 50)
        << "VBlankFrequency should be 50 for PAL";
    EXPECT_EQ(power_freq, 50)
        << "PowerSupplyFrequency should be 50 for PAL";
}

TEST_F(SysInfoTest, ExecBaseMaxLocMemReportsChipMemTop)
{
    uint32_t sysbase = lxa_peek32(4);
    ASSERT_NE(sysbase, 0u);

    uint32_t max_loc_mem = lxa_peek32(sysbase + EXECBASE_OFF_MAXLOCMEM);

    /* lxa provides 10MB chip memory: 0x00010000 .. 0x009FFFFF,
     * so MaxLocMem should be 0x00A00000 (top of chip memory). */
    EXPECT_EQ(max_loc_mem, 0x00A00000u)
        << "MaxLocMem should report top of chip memory (10MB)";
}

TEST_F(SysInfoTest, ExecBaseEClockFrequencyIsPAL)
{
    uint32_t sysbase = lxa_peek32(4);
    ASSERT_NE(sysbase, 0u);

    uint32_t eclock = lxa_peek32(sysbase + EXECBASE_OFF_ECLOCKFREQUENCY);

    EXPECT_EQ(eclock, 709379u)
        << "ex_EClockFrequency should be 709379 Hz for PAL";
}

/* ===================================================================== */
/* Interaction tests (mutate display but do not exit the app)            */
/*                                                                       */
/* Order matters: MEMORY and BOARDS run first because SPEED performs a   */
/* lengthy benchmark that keeps the CPU busy.  After SPEED completes,   */
/* the app may need extra settling before gadget clicks are processed.  */
/* ===================================================================== */

TEST_F(SysInfoTest, MemoryGadgetRefreshesMemoryArea)
{
    const int gadget_index = FindGadgetIndexById(GADGET_ID_MEMORY);
    RgbImage before_image;
    RgbImage after_image;

    ASSERT_GE(gadget_index, 0) << "SysInfo should expose the MEMORY gadget";

    const std::string before_path = s_ram_dir + "/sysinfo-memory-before.png";
    const std::string after_path  = s_ram_dir + "/sysinfo-memory-after.png";

    ASSERT_TRUE(lxa_capture_window(0, before_path.c_str()));
    before_image = LoadPng(before_path);

    ASSERT_TRUE(ClickGadget(gadget_index, 0));
    RunCyclesWithVBlank(120, 50000);

    ASSERT_TRUE(lxa_capture_window(0, after_path.c_str()));
    after_image = LoadPng(after_path);

    /* SysInfo's main content area updates when switching to the MEMORY view.
     * The entire upper panel should show memory information. */
    const int diff = CountImageDifferences(before_image, after_image, 16, 20, 620, 200);
    EXPECT_GT(diff, 200)
        << "Clicking MEMORY should update the display with memory information";

    EXPECT_TRUE(lxa_is_running())
        << "SysInfo should remain running after clicking the MEMORY gadget";
}

TEST_F(SysInfoTest, BoardsGadgetRefreshesBoardsArea)
{
    const int gadget_index = FindGadgetIndexById(GADGET_ID_BOARDS);
    RgbImage before_image;
    RgbImage after_image;

    ASSERT_GE(gadget_index, 0) << "SysInfo should expose the BOARDS gadget";

    const std::string before_path = s_ram_dir + "/sysinfo-boards-before.png";
    const std::string after_path  = s_ram_dir + "/sysinfo-boards-after.png";

    ASSERT_TRUE(lxa_capture_window(0, before_path.c_str()));
    before_image = LoadPng(before_path);

    ASSERT_TRUE(ClickGadget(gadget_index, 0));
    RunCyclesWithVBlank(120, 50000);

    ASSERT_TRUE(lxa_capture_window(0, after_path.c_str()));
    after_image = LoadPng(after_path);

    /* BOARDS view replaces the main content area with expansion board info. */
    const int diff = CountImageDifferences(before_image, after_image, 16, 20, 620, 200);
    EXPECT_GT(diff, 200)
        << "Clicking BOARDS should update the display with board information";

    EXPECT_TRUE(lxa_is_running())
        << "SysInfo should remain running after clicking the BOARDS gadget";
}

TEST_F(SysInfoTest, LibrariesGadgetRefreshesContentArea)
{
    const int gadget_index = FindGadgetIndexById(GADGET_ID_LIBRARIES);
    RgbImage before_image;
    RgbImage after_image;

    ASSERT_GE(gadget_index, 0) << "SysInfo should expose the LIBRARIES gadget";

    const std::string before_path = s_ram_dir + "/sysinfo-libraries-before.png";
    const std::string after_path  = s_ram_dir + "/sysinfo-libraries-after.png";

    ASSERT_TRUE(lxa_capture_window(0, before_path.c_str()));
    before_image = LoadPng(before_path);

    ASSERT_TRUE(ClickGadget(gadget_index, 0));
    RunCyclesWithVBlank(120, 50000);

    ASSERT_TRUE(lxa_capture_window(0, after_path.c_str()));
    after_image = LoadPng(after_path);

    /* LIBRARIES view replaces the main content area with a list of loaded
     * libraries (exec, dos, intuition, graphics, etc.).  The upper portion
     * of the window should show significant pixel changes. */
    const int diff = CountImageDifferences(before_image, after_image, 16, 20, 620, 200);
    EXPECT_GT(diff, 200)
        << "Clicking LIBRARIES should update the display with loaded library information";

    EXPECT_TRUE(lxa_is_running())
        << "SysInfo should remain running after clicking the LIBRARIES gadget";
}

TEST_F(SysInfoTest, SpeedGadgetRefreshesComparisonArea)
{
    const int gadget_index = FindGadgetIndexById(GADGET_ID_SPEED);
    RgbImage before_image;
    RgbImage after_image;

    ASSERT_GE(gadget_index, 0) << "SysInfo should expose the SPEED gadget";

    const std::string before_path = s_ram_dir + "/sysinfo-speed-before.png";
    const std::string after_path  = s_ram_dir + "/sysinfo-speed-after.png";

    ASSERT_TRUE(lxa_capture_window(0, before_path.c_str()));
    before_image = LoadPng(before_path);

    ASSERT_TRUE(ClickGadget(gadget_index, 0));
    /* The SPEED benchmark performs intensive CPU computation.  Give it a
     * very generous cycle budget so it can finish the benchmark and
     * render the comparison bars. */
    RunCyclesWithVBlank(600, 200000);

    ASSERT_TRUE(lxa_capture_window(0, after_path.c_str()));
    after_image = LoadPng(after_path);

    /* Speed-comparison area covers the upper content panel.  Even a
     * partial benchmark result should change a substantial number of
     * pixels (status text, progress bars, numeric results). */
    const int diff = CountImageDifferences(before_image, after_image, 16, 20, 620, 200);
    EXPECT_GT(diff, 100)
        << "Clicking SPEED should refresh the display with benchmark results";
}

/* ===================================================================== */
/* Screenshot capture for visual review                                  */
/*                                                                       */
/* Captures the current state (after all interaction tests have run) to  */
/* /tmp/sysinfo_review/ for analysis with tools/screenshot_review.py.    */
/* ===================================================================== */

TEST_F(SysInfoTest, CaptureCurrentStateForVisualReview)
{
    /* Save a screenshot of the current SysInfo state to a persistent
     * directory for offline analysis with tools/screenshot_review.py.
     *
     * NOTE: This test runs after all interaction tests (MEMORY, BOARDS,
     * LIBRARIES, SPEED) in the persistent fixture, so the captured image
     * reflects whichever view was last active. */
    system("mkdir -p /tmp/sysinfo_review");
    EXPECT_TRUE(lxa_capture_window(0, "/tmp/sysinfo_review/sysinfo-current-state.png"));
}

/* ===================================================================== */
/* Destructive test last (close gadget)                                  */
/* ===================================================================== */

TEST_F(SysInfoTest, CloseGadgetLeavesApplicationRunning)
{
    ASSERT_TRUE(lxa_click_close_gadget(0));
    RunCyclesWithVBlank(120, 50000);

    EXPECT_TRUE(lxa_is_running())
        << "SysInfo should ignore the close gadget and keep its custom screen running";
    EXPECT_EQ(lxa_get_window_count(), 1)
        << "SysInfo should keep its main window open after a close-gadget click";
}

/* Phase 126: startup latency baseline sentinel */
TEST_F(SysInfoTest, ZStartupLatency)
{
    if (!s_setup_ok) GTEST_SKIP() << "Suite setup failed";
    ASSERT_GE(s_startup_ms, 0) << "Startup time was not recorded";
    /* Baseline: SysInfo must open its main window within 15 seconds of wall time */
    EXPECT_LE(s_startup_ms, 15000L)
        << "SysInfo startup latency " << s_startup_ms
        << " ms exceeds 15 s baseline";
    fprintf(stderr, "[LATENCY] SysInfo startup: %ld ms\n", s_startup_ms);
}

int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
