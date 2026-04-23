/**
 * devpac_scrollbar_gtest.cpp — Phase 145 scrollbar and zoom gadget tests
 *
 * Verifies:
 *  A) A window opened with WA_Zoom has a GTYP_WZOOM system gadget
 *     (tested via the ZoomWindow sample, not Devpac which has no WA_Zoom on
 *     its editor window).
 *  B) The right border PropGadget knob of the Devpac editor renders
 *     non-background pixels (knob is visible).
 *  C) The scroll arrow BOOL gadgets at the bottom of the Devpac editor
 *     right border are rendered with non-background pixels.
 *
 * Uses two persistent test suite fixtures:
 *   ZoomWindowTest  — loads SYS:Samples/ZoomWindow
 *   DevpacBorderTest — loads Devpac (editor window only, no WA_Zoom)
 */

#include "lxa_test.h"

#include <algorithm>
#include <string>

using namespace lxa::testing;

/* ================================================================== */
/* Shared setup helpers                                                */
/* ================================================================== */

static void CommonAddPaths()
{
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
}

/* Count pixels in screen-coordinate region that differ from pen 0 */
static int CountNonBackgroundPixels(int x1, int y1, int x2, int y2)
{
    int count = 0;
    for (int y = y1; y <= y2; ++y) {
        for (int x = x1; x <= x2; ++x) {
            int pen = -1;
            if (lxa_read_pixel(x, y, &pen) && pen != 0)
                ++count;
        }
    }
    return count;
}

/* ================================================================== */
/* Fixture A: ZoomWindow sample — tests WZOOM gadget creation         */
/* ================================================================== */

class ZoomWindowTest : public ::testing::Test {
protected:
    static bool               s_setup_ok;
    static lxa_window_info_t  s_window_info;
    static std::string        s_tmp_dir;

    static void SetUpTestSuite() {
        s_setup_ok = false;
        setenv("LXA_PREFIX", LXA_TEST_PREFIX, 1);

        lxa_config_t config;
        memset(&config, 0, sizeof(config));
        config.headless = true;
        config.verbose  = false;
        config.rootless = true;
        config.rom_path = FindRomPath();
        if (!config.rom_path) return;
        if (lxa_init(&config) != 0) return;

        CommonAddPaths();

        char tdir[] = "/tmp/lxa_zoom_XXXXXX";
        if (mkdtemp(tdir)) {
            s_tmp_dir = tdir;
            lxa_add_drive("RAM", tdir);
            lxa_add_assign("T", tdir);
            lxa_add_assign("ENV", tdir);
            lxa_add_assign("ENVARC", tdir);
        }

        if (lxa_load_program("SYS:Samples/ZoomWindow", "") != 0) {
            fprintf(stderr, "ZoomWindowTest: failed to load ZoomWindow sample\n");
            lxa_shutdown(); return;
        }
        if (!lxa_wait_windows(1, 8000)) {
            fprintf(stderr, "ZoomWindowTest: window did not open\n");
            lxa_shutdown(); return;
        }
        if (!lxa_get_window_info(0, &s_window_info)) {
            fprintf(stderr, "ZoomWindowTest: could not get window info\n");
            lxa_shutdown(); return;
        }
        if (!lxa_wait_window_drawn(0, 5000)) {
            fprintf(stderr, "ZoomWindowTest: window not drawn\n");
            lxa_shutdown(); return;
        }
        for (int i = 0; i < 50; i++) {
            lxa_trigger_vblank();
            lxa_run_cycles(50000);
        }
        s_setup_ok = true;
    }

    static void TearDownTestSuite() {
        lxa_shutdown();
        if (!s_tmp_dir.empty()) {
            std::string cmd = "rm -rf " + s_tmp_dir;
            system(cmd.c_str());
            s_tmp_dir.clear();
        }
    }

    void SetUp() override {
        if (!s_setup_ok) GTEST_SKIP() << "ZoomWindow suite setup failed";
    }

    lxa_window_info_t& window_info = s_window_info;
};

bool               ZoomWindowTest::s_setup_ok    = false;
lxa_window_info_t  ZoomWindowTest::s_window_info = {};
std::string        ZoomWindowTest::s_tmp_dir;

/* ------------------------------------------------------------------ */
/* Test A: WZOOM gadget appears when WA_Zoom is used                  */
/* ------------------------------------------------------------------ */
TEST_F(ZoomWindowTest, ZoomGadgetExists) {
    /* The ZoomWindow sample opens a window with WA_Zoom.  Verify that
     * a GTYP_WZOOM (gadget_id == 0x0060) system gadget is present and
     * has sensible geometry (non-zero dimensions in the title bar). */
    int count = lxa_get_gadget_count(0);
    ASSERT_GT(count, 0) << "ZoomWindow should have at least one gadget";

    bool found_wzoom = false;
    for (int i = 0; i < count; ++i) {
        lxa_gadget_info_t info;
        if (lxa_get_gadget_info(0, i, &info)) {
            /* GTYP_WZOOM system gadget has GadgetID == 0x0060 */
            if (info.gadget_id == 0x0060) {
                found_wzoom = true;
                /* Geometry sanity: must have non-zero width and height */
                EXPECT_GT(info.width, 0) << "WZOOM gadget width should be > 0";
                EXPECT_GT(info.height, 0) << "WZOOM gadget height should be > 0";
                /* Must be within the window bounds */
                EXPECT_GE(info.left, window_info.x)
                    << "WZOOM gadget left should be >= window left";
                EXPECT_LT(info.left, window_info.x + window_info.width)
                    << "WZOOM gadget left should be within window width";
                /* Must be in the title bar (top of window) */
                EXPECT_EQ(info.top, window_info.y)
                    << "WZOOM gadget should be in the title bar (top=window.y)";
                break;
            }
        }
    }
    EXPECT_TRUE(found_wzoom)
        << "No GTYP_WZOOM gadget found. gadget count=" << count;
}

/* ================================================================== */
/* Fixture B: Devpac editor — right border scrollbar tests            */
/* ================================================================== */

class DevpacBorderTest : public ::testing::Test {
protected:
    static bool               s_setup_ok;
    static lxa_window_info_t  s_window_info;
    static std::string        s_ram_dir;
    static std::string        s_t_dir;
    static std::string        s_env_dir;

    static void SetUpTestSuite() {
        s_setup_ok = false;
        setenv("LXA_PREFIX", LXA_TEST_PREFIX, 1);

        lxa_config_t config;
        memset(&config, 0, sizeof(config));
        config.headless = true;
        config.verbose  = false;
        config.rootless = true;
        config.rom_path = FindRomPath();
        if (!config.rom_path) return;
        if (lxa_init(&config) != 0) return;

        CommonAddPaths();

        const char* apps = FindAppsPath();
        if (!apps) {
            fprintf(stderr, "DevpacBorderTest: lxa-apps not found\n");
            lxa_shutdown(); return;
        }

        char ram[] = "/tmp/lxa_devborder_RAM_XXXXXX";
        if (mkdtemp(ram)) { s_ram_dir = ram; lxa_add_drive("RAM", ram); }
        char tdir[] = "/tmp/lxa_devborder_T_XXXXXX";
        if (mkdtemp(tdir)) { s_t_dir = tdir; lxa_add_assign("T", tdir); }
        char edir[] = "/tmp/lxa_devborder_ENV_XXXXXX";
        if (mkdtemp(edir)) {
            s_env_dir = edir;
            lxa_add_assign("ENV", edir);
            lxa_add_assign("ENVARC", edir);
        }

        if (lxa_load_program("APPS:DevPac/bin/Devpac/Devpac", "") != 0) {
            fprintf(stderr, "DevpacBorderTest: failed to load Devpac\n");
            lxa_shutdown(); return;
        }
        if (!lxa_wait_windows(1, 10000)) {
            fprintf(stderr, "DevpacBorderTest: window did not open\n");
            lxa_shutdown(); return;
        }
        if (!lxa_get_window_info(0, &s_window_info)) {
            fprintf(stderr, "DevpacBorderTest: could not get window info\n");
            lxa_shutdown(); return;
        }
        if (!lxa_wait_window_drawn(0, 5000)) {
            fprintf(stderr, "DevpacBorderTest: window not drawn\n");
            lxa_shutdown(); return;
        }
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
        rm(s_ram_dir); rm(s_t_dir); rm(s_env_dir);
    }

    void SetUp() override {
        if (!s_setup_ok) GTEST_SKIP() << "DevpacBorder suite setup failed";
    }

    lxa_window_info_t& window_info = s_window_info;
};

bool               DevpacBorderTest::s_setup_ok    = false;
lxa_window_info_t  DevpacBorderTest::s_window_info = {};
std::string        DevpacBorderTest::s_ram_dir;
std::string        DevpacBorderTest::s_t_dir;
std::string        DevpacBorderTest::s_env_dir;

/* ------------------------------------------------------------------ */
/* Test B: Right border PropGadget knob renders non-background pixels  */
/* ------------------------------------------------------------------ */
TEST_F(DevpacBorderTest, RightBorderPropKnob) {
    /* Gadget 0 is the PropGadget (vertical scrollbar) at the right border.
     * After the window is drawn the knob should produce non-background pixels
     * in the middle half of its height to avoid boundary lines. */
    lxa_gadget_info_t prop;
    ASSERT_TRUE(lxa_get_gadget_info(0, 0, &prop))
        << "Could not read gadget 0 (PropGadget)";

    int x1     = prop.left + 1;
    int x2     = prop.left + prop.width - 2;
    int mid_h  = prop.height / 2;
    int y1     = prop.top + prop.height / 4;
    int y2     = prop.top + prop.height / 4 + mid_h;

    const std::string cap = s_ram_dir + "/devpac-propknob.png";
    lxa_capture_screen(cap.c_str());

    int non_bg = CountNonBackgroundPixels(x1, y1, x2, y2);
    EXPECT_GT(non_bg, 0)
        << "Right border PropGadget middle region should have non-background pixels."
        << " x=" << x1 << ".." << x2 << " y=" << y1 << ".." << y2
        << ". Capture: " << cap;
}

/* ------------------------------------------------------------------ */
/* Test C: Scroll arrow BOOL gadgets render non-background pixels      */
/* ------------------------------------------------------------------ */
TEST_F(DevpacBorderTest, RightBorderScrollArrows) {
    /* Gadgets 1 and 2 are BOOLGADGET scroll arrows at the bottom of the
     * right border.  Use their reported geometry to check pixel content. */
    int count = lxa_get_gadget_count(0);
    ASSERT_GE(count, 3) << "Expected at least 3 gadgets (prop + 2 arrows)";

    /* Find the two BOOL gadgets in the right border (not SYSGADGET).
     * info.left is screen-absolute; threshold relative to window right edge. */
    const int win_right = window_info.x + window_info.width;
    const int right_border_x_threshold = win_right - 22;
    int arrow_pixels = 0;

    for (int i = 0; i < count; ++i) {
        lxa_gadget_info_t g;
        if (!lxa_get_gadget_info(0, i, &g)) continue;

        /* Skip SYSGADGET (0x8000) — we want the app's own arrow gadgets */
        if (g.gadget_type & 0x8000) continue;
        /* Must be in the right border area (screen-absolute) */
        if (g.left < right_border_x_threshold) continue;

        int x1 = g.left + 1;
        int x2 = g.left + g.width - 2;
        int y1 = g.top + 1;
        int y2 = g.top + g.height - 2;

        arrow_pixels += CountNonBackgroundPixels(x1, y1, x2, y2);
    }

    const std::string cap = s_ram_dir + "/devpac-arrows.png";
    lxa_capture_screen(cap.c_str());

    EXPECT_GT(arrow_pixels, 0)
        << "Right-border scroll arrow gadgets should render non-background pixels."
        << " Capture: " << cap;
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
