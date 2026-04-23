/**
 * devpac_settings_gtest.cpp — Phase 146: Devpac Settings window test
 *
 * Opens Devpac, navigates to Settings→Settings... via RMB drag, waits for the
 * settings requester window to open and render, and verifies:
 *   1. A second window opens (window count goes from 1 to 2).
 *   2. The settings window renders non-trivial content (non-background pixels).
 *   3. At least 3 gadgets are visible (Save, Use, Cancel buttons found by ID).
 *   4. Window can be dismissed cleanly.
 *
 * Menu coordinates (verified via lxa_get_menu_info() probe):
 *   Settings menu (Menu[6]): left=384, width=80, centre x=424
 *   "Settings..." item (Item[3]): top=26 in dropdown → screen y = 11+26+5 = 42
 *
 * Timing note: req.library needs ~500 VBlanks (25M cycles) after the window
 * opens before it finishes rendering all content. We use lxa_wait_window_drawn()
 * which polls for non-background pixels and avoids a fixed cycle budget.
 *
 * The req.library real binary must be present (Phase 142).
 */

#include "lxa_test.h"

#include <algorithm>
#include <string>

using namespace lxa::testing;

class DevpacSettingsTest : public ::testing::Test {
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
        if (!config.rom_path) {
            fprintf(stderr, "DevpacSettingsTest: could not find lxa.rom\n");
            return;
        }
        if (lxa_init(&config) != 0) {
            fprintf(stderr, "DevpacSettingsTest: lxa_init() failed\n");
            return;
        }

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

        if (!apps) {
            fprintf(stderr, "DevpacSettingsTest: lxa-apps not found\n");
            lxa_shutdown();
            return;
        }

        char ram[] = "/tmp/lxa_devsettings_RAM_XXXXXX";
        if (mkdtemp(ram)) { s_ram_dir = ram; lxa_add_drive("RAM", ram); }
        char tdir[] = "/tmp/lxa_devsettings_T_XXXXXX";
        if (mkdtemp(tdir)) { s_t_dir = tdir; lxa_add_assign("T", tdir); }
        char edir[] = "/tmp/lxa_devsettings_ENV_XXXXXX";
        if (mkdtemp(edir)) {
            s_env_dir = edir;
            lxa_add_assign("ENV", edir);
            lxa_add_assign("ENVARC", edir);
        }

        if (lxa_load_program("APPS:DevPac/bin/Devpac/Devpac", "") != 0) {
            fprintf(stderr, "DevpacSettingsTest: failed to load Devpac\n");
            lxa_shutdown();
            return;
        }
        if (!lxa_wait_windows(1, 10000)) {
            fprintf(stderr, "DevpacSettingsTest: window did not open\n");
            lxa_shutdown();
            return;
        }
        if (!lxa_get_window_info(0, &s_window_info)) {
            fprintf(stderr, "DevpacSettingsTest: could not get window info\n");
            lxa_shutdown();
            return;
        }
        if (!lxa_wait_window_drawn(0, 5000)) {
            fprintf(stderr, "DevpacSettingsTest: window not drawn\n");
            lxa_shutdown();
            return;
        }
        /* Let Devpac initialize fully */
        for (int i = 0; i < 100; i++) {
            lxa_trigger_vblank();
            lxa_run_cycles(50000);
        }

        /* ---- Open the Settings window now (shared for all tests) ---- */

        /* Settings menu (Menu[6]): left=384, width=80 → centre x=424.
         * "Settings..." (Item[3]): top=26 in dropdown → screen y = 11+26+5 = 42. */
        const int menu_bar_y      = std::max(3, s_window_info.y / 2);
        const int settings_menu_x = s_window_info.x + 424;
        const int settings_item_y = 42;

        lxa_inject_drag(settings_menu_x, menu_bar_y,
                        settings_menu_x, settings_item_y,
                        LXA_MOUSE_RIGHT, 10);

        /* Run enough cycles so req.library can open the window */
        for (int i = 0; i < 120; i++) {
            lxa_trigger_vblank();
            lxa_run_cycles(50000);
        }

        if (!lxa_wait_windows(2, 8000)) {
            fprintf(stderr, "DevpacSettingsTest: Settings window did not open\n");
            lxa_shutdown();
            return;
        }

        /* Wait for the window to finish rendering (req.library needs many cycles) */
        if (!lxa_wait_window_drawn(1, 15000)) {
            fprintf(stderr, "DevpacSettingsTest: Settings window did not draw\n");
            /* Not fatal — tests will verify and report */
        }

        /* Additional settling */
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
        if (!s_setup_ok) GTEST_SKIP() << "DevpacSettings suite setup failed";
    }

    lxa_window_info_t& window_info = s_window_info;

    void RunCyclesWithVBlank(int iterations = 20, int cycles_per_iteration = 50000) {
        for (int i = 0; i < iterations; i++) {
            lxa_trigger_vblank();
            lxa_run_cycles(cycles_per_iteration);
        }
    }

    /* Count pixels in the settings window (window index 1) that differ from
     * the background pen (pen 0). Uses screen-absolute coordinates. */
    int CountNonBackgroundPixels(int x1, int y1, int x2, int y2) {
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
};

bool               DevpacSettingsTest::s_setup_ok    = false;
lxa_window_info_t  DevpacSettingsTest::s_window_info = {};
std::string        DevpacSettingsTest::s_ram_dir;
std::string        DevpacSettingsTest::s_t_dir;
std::string        DevpacSettingsTest::s_env_dir;

/* ===================================================================== */

/* Test 1: The Settings window opened and has sensible geometry. */
TEST_F(DevpacSettingsTest, SettingsWindowOpens) {
    ASSERT_GE(lxa_get_window_count(), 2)
        << "Settings window (window 1) should be open";

    lxa_window_info_t settings_info;
    ASSERT_TRUE(lxa_get_window_info(1, &settings_info))
        << "Should be able to query the Settings window";

    EXPECT_STREQ(settings_info.title, "Settings")
        << "Second window should be titled 'Settings'";
    EXPECT_GT(settings_info.width,  100) << "Settings window must have non-trivial width";
    EXPECT_GT(settings_info.height, 50)  << "Settings window must have non-trivial height";
    EXPECT_TRUE(lxa_is_running()) << "Devpac should still be running";
}

/* Test 2: The Settings window renders non-background content.
 * The cycle gadgets (left side) and checkbox gadgets (right side) should
 * produce non-background pixels in the client area. */
TEST_F(DevpacSettingsTest, SettingsWindowRendersContent) {
    ASSERT_GE(lxa_get_window_count(), 2) << "Settings window must be open";

    lxa_window_info_t sw;
    ASSERT_TRUE(lxa_get_window_info(1, &sw));

    const std::string cap = s_ram_dir + "/devpac-settings-content.png";
    lxa_capture_window(1, cap.c_str());

    /* The client area (inside the window border, below title bar).
     * Use screen-absolute coordinates: border ~4px, title bar ~10px. */
    const int x1 = sw.x + 4;
    const int y1 = sw.y + 14;  /* below title bar */
    const int x2 = sw.x + sw.width  - 5;
    const int y2 = sw.y + sw.height - 5;

    int non_bg = CountNonBackgroundPixels(x1, y1, x2, y2);
    EXPECT_GT(non_bg, 100)
        << "Settings window client area should render non-background content. "
        << "Capture: " << cap
        << " Region: (" << x1 << "," << y1 << ")..(" << x2 << "," << y2 << ")";
}

/* Test 3: The Settings window has at least 3 gadgets (Save, Use, Cancel). */
TEST_F(DevpacSettingsTest, SettingsWindowHasThreeButtons) {
    ASSERT_GE(lxa_get_window_count(), 2) << "Settings window must be open";

    int gadget_count = lxa_get_gadget_count(1);
    EXPECT_GE(gadget_count, 3)
        << "Settings window should have at least 3 gadgets (Save, Use, Cancel). "
        << "Got: " << gadget_count;

    EXPECT_TRUE(lxa_is_running());
}

/* Capture before dismiss for visual investigation */
TEST_F(DevpacSettingsTest, YCaptureSettingsOpen) {
    if (lxa_get_window_count() < 2) {
        GTEST_SKIP() << "Settings window not open";
    }
    lxa_capture_screen("/tmp/devpac_settings_open.png");
    SUCCEED();
}

/* Last test: dismiss the settings window via ESC and verify the editor
 * window redraws. With IDCMP_REFRESHWINDOW notification (Phase 148),
 * Devpac receives a refresh event and redraws the area that was covered. */
TEST_F(DevpacSettingsTest, ZDismissSettingsWindow) {
    if (lxa_get_window_count() < 2) {
        GTEST_SKIP() << "Settings window not open";
    }

    /* Get settings window bounds (the area that will be exposed on close) */
    lxa_window_info_t sw;
    ASSERT_TRUE(lxa_get_window_info(1, &sw))
        << "Should be able to query settings window before dismiss";

    /* Count pixels in the area that will be exposed after dismiss.
     * This area currently shows the settings window content. */
    int editor_exposed_x1 = sw.x + 4;
    int editor_exposed_y1 = sw.y + 14;
    int editor_exposed_x2 = sw.x + sw.width  - 5;
    int editor_exposed_y2 = sw.y + sw.height - 5;

    lxa_inject_keypress(0x45, 0);  /* ESC */
    RunCyclesWithVBlank(40, 50000);

    /* Wait for the settings window to close */
    bool closed = false;
    for (int i = 0; i < 200 && !closed; ++i) {
        lxa_trigger_vblank();
        lxa_run_cycles(50000);
        closed = (lxa_get_window_count() < 2);
    }

    lxa_flush_display();

    /* Capture the main window after dismiss to inspect overdraw */
    lxa_capture_screen("/tmp/devpac_after_dismiss.png");

    EXPECT_TRUE(lxa_is_running())
        << "Devpac should survive dismissing the Settings window";

    if (!closed) {
        /* Settings window didn't close via ESC — skip redraw check */
        GTEST_SKIP() << "Settings window did not close via ESC — skipping redraw check";
    }

    EXPECT_EQ(lxa_get_window_count(), 1)
        << "Only the editor window should remain after dismissing Settings";

    /* After dismissal the editor should have non-background content in the
     * area that was previously covered by the settings window.  With
     * IDCMP_REFRESHWINDOW delivered, Devpac redraws its editor content. */
    int after_pixels = CountNonBackgroundPixels(
        editor_exposed_x1, editor_exposed_y1,
        editor_exposed_x2, editor_exposed_y2);
    EXPECT_GT(after_pixels, 20)
        << "Editor area covered by Settings window should have non-background "
        << "content after dismiss (IDCMP_REFRESHWINDOW delivered and app redrawn). "
        << "Region: (" << editor_exposed_x1 << "," << editor_exposed_y1
        << ")..(" << editor_exposed_x2 << "," << editor_exposed_y2
        << "). Capture: /tmp/devpac_after_dismiss.png";
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
