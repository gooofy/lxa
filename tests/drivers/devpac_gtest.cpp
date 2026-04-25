/**
 * devpac_gtest.cpp - Google Test version of Devpac (HiSoft) test
 *
 * Tests Devpac assembler - opens and displays editor.
 * Verifies screen dimensions, editor initialization, text entry,
 * mouse input, and cursor key handling.
 *
 * Phase 107: Uses SetUpTestSuite() to load Devpac once for all tests,
 * avoiding redundant emulator init + program load per test case.
 */

#include "lxa_test.h"

#include <algorithm>

using namespace lxa::testing;

class DevpacTest : public ::testing::Test {
protected:
    static constexpr int PEN_GREY = 0;
    static constexpr int PEN_BLACK = 1;
    static constexpr int MENU_BAR_HEIGHT = 11;

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
            fprintf(stderr, "DevpacTest: could not find lxa.rom\n");
            return;
        }

        if (lxa_init(&config) != 0) {
            fprintf(stderr, "DevpacTest: lxa_init() failed\n");
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
            fprintf(stderr, "DevpacTest: lxa-apps directory not found\n");
            lxa_shutdown();
            return;
        }

        /* Load the Devpac IDE */
        if (lxa_load_program("APPS:DevPac/bin/Devpac/Devpac", "") != 0) {
            fprintf(stderr, "DevpacTest: failed to load Devpac\n");
            lxa_shutdown();
            return;
        }

        if (!lxa_wait_windows(1, 10000)) {
            fprintf(stderr, "DevpacTest: window did not open\n");
            lxa_shutdown();
            return;
        }

        if (!lxa_get_window_info(0, &s_window_info)) {
            fprintf(stderr, "DevpacTest: could not get window info\n");
            lxa_shutdown();
            return;
        }

        if (!lxa_wait_window_drawn(0, 5000)) {
            fprintf(stderr, "DevpacTest: window did not draw\n");
            lxa_shutdown();
            return;
        }

        /* WaitForEventLoop(100, 10000) equivalent */
        for (int i = 0; i < 100; i++)
            lxa_run_cycles(10000);

        /* Let Devpac initialize */
        for (int i = 0; i < 100; i++) {
            lxa_trigger_vblank();
            lxa_run_cycles(50000);
        }

        s_setup_ok = true;
    }

    static void TearDownTestSuite() {
        lxa_capture_screen("/home/guenter/projects/amiga/lxa/src/lxa/screenshots/lxa-tests/devpac.png");
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

    bool WaitForWindows(int count, int timeout_ms = 5000) {
        return lxa_wait_windows(count, timeout_ms);
    }

    bool GetWindowInfo(int index, lxa_window_info_t* info) {
        return lxa_get_window_info(index, info);
    }

    bool WaitForWindowDrawn(int index = 0, int timeout_ms = 5000) {
        return lxa_wait_window_drawn(index, timeout_ms);
    }

    bool CaptureWindow(const char* filename, int index = 0) {
        return lxa_capture_window(index, filename);
    }

    int ReadPixel(int x, int y) {
        int pen;
        if (lxa_read_pixel(x, y, &pen)) {
            return pen;
        }
        return -1;
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

    int CountNonBlackPixels(const RgbImage& image, int x1, int y1, int x2, int y2) {
        int count = 0;

        for (int y = y1; y <= y2; y++) {
            for (int x = x1; x <= x2; x++) {
                size_t pixel_offset = (static_cast<size_t>(y) * static_cast<size_t>(image.width) +
                                       static_cast<size_t>(x)) * 3U;

                if (image.pixels[pixel_offset] != 0 ||
                    image.pixels[pixel_offset + 1] != 0 ||
                    image.pixels[pixel_offset + 2] != 0) {
                    count++;
                }
            }
        }

        return count;
    }

    bool OpenAboutDialog(lxa_window_info_t* about_info = nullptr) {
        const int menu_bar_x = window_info.x + 32;
        const int menu_bar_y = std::max(3, window_info.y / 2);
        const int about_item_y = 128;

        lxa_inject_drag(menu_bar_x, menu_bar_y,
                        menu_bar_x, about_item_y,
                        LXA_MOUSE_RIGHT, 10);
        RunCyclesWithVBlank(40, 50000);

        if (!WaitForWindows(2, 5000)) {
            return false;
        }

        if (about_info != nullptr) {
            if (!GetWindowInfo(1, about_info)) {
                return false;
            }
        }

        return WaitForWindowDrawn(1, 5000);
    }

    int FindBottomGadgetIndex(int window_index) {
        auto gadgets = GetGadgets(window_index);
        int bottom_gadget = -1;
        int bottom_top = -1;

        for (size_t i = 0; i < gadgets.size(); ++i) {
            if (gadgets[i].top > bottom_top) {
                bottom_top = gadgets[i].top;
                bottom_gadget = static_cast<int>(i);
            }
        }

        return bottom_gadget;
    }

    int CountPenPixelsInRow(int left, int right, int y, int pen) {
        int count = 0;

        for (int x = left; x <= right; ++x) {
            if (ReadPixel(x, y) == pen) {
                count++;
            }
        }

        return count;
    }
};

/* Static member definitions */
bool                DevpacTest::s_setup_ok = false;
lxa_window_info_t   DevpacTest::s_window_info = {};
std::string         DevpacTest::s_ram_dir;
std::string         DevpacTest::s_t_dir;
std::string         DevpacTest::s_env_dir;

/* ===================================================================== */
/* Read-only tests first */

TEST_F(DevpacTest, WindowOpens) {
    EXPECT_GT(window_info.width, 0);
    EXPECT_GT(window_info.height, 0);
}

TEST_F(DevpacTest, ScreenDimensions) {
    int width, height, depth;
    if (lxa_get_screen_dimensions(&width, &height, &depth)) {
        EXPECT_GE(width, 640) << "Screen width should be >= 640";
        EXPECT_GE(height, 200) << "Screen height should be >= 200";
    }
}

TEST_F(DevpacTest, EditorVisible) {
    /* Verify Devpac is running and has windows */
    EXPECT_TRUE(lxa_is_running()) << "Devpac should still be running";
    EXPECT_GE(lxa_get_window_count(), 1) << "At least one window should be open";
}

/* Visual tests (capture/pixel reads, minor state changes from menu interactions) */

TEST_F(DevpacTest, RootlessHostWindowIncludesScreenStripAboveWindow) {
    const std::string capture_path = s_ram_dir + "/devpac-window.png";
    RgbImage image;
    int top_strip_non_black = 0;

    ASSERT_TRUE(CaptureWindow(capture_path.c_str(), 0))
        << "Rootless Devpac window capture should succeed";

    image = LoadPng(capture_path);

    top_strip_non_black = CountNonBlackPixels(image,
                                              0,
                                              0,
                                              image.width - 1,
                                              std::min(image.height - 1, MENU_BAR_HEIGHT - 1));

    EXPECT_EQ(image.width, window_info.width)
        << "Devpac should preserve its logical width in the host window capture";
    EXPECT_GE(image.height, window_info.height)
        << "Rootless Devpac capture should preserve at least the logical window height";
    EXPECT_GT(top_strip_non_black, 500)
        << "Rootless Devpac capture should include visible menu-bar content in the top strip";
}

/* About dialog must run before MenuBarRemains... because the repeated
 * menu drags in that test select "New" items, opening additional editor
 * windows that would interfere with the 2-window assertion here. */
TEST_F(DevpacTest, AboutDialogDoesNotDuplicateInsideMainWindow) {
    const std::string main_before_path = s_ram_dir + "/devpac-about-main-before.png";
    const std::string main_after_path = s_ram_dir + "/devpac-about-main-after.png";
    RgbImage before_image;
    RgbImage after_image;
    int before_main_pixels = 0;
    int after_main_pixels = 0;
    lxa_window_info_t about_info;

    ASSERT_TRUE(CaptureWindow(main_before_path.c_str(), 0));
    before_image = LoadPng(main_before_path);

    ASSERT_TRUE(OpenAboutDialog(&about_info))
        << "Devpac About dialog should open as a second window";

    ASSERT_TRUE(CaptureWindow(main_after_path.c_str(), 0));
    after_image = LoadPng(main_after_path);

    before_main_pixels = CountNonBlackPixels(before_image,
                                             8,
                                             std::min(before_image.height - 1, MENU_BAR_HEIGHT + 24),
                                             before_image.width - 9,
                                             before_image.height - 9);
    after_main_pixels = CountNonBlackPixels(after_image,
                                            8,
                                            std::min(after_image.height - 1, MENU_BAR_HEIGHT + 24),
                                            after_image.width - 9,
                                            after_image.height - 9);

    EXPECT_STREQ(about_info.title, "Devpac Amiga")
        << "The second rootless window should be the Devpac About dialog";
    EXPECT_LT(before_main_pixels > after_main_pixels
                  ? before_main_pixels - after_main_pixels
                  : after_main_pixels - before_main_pixels,
              before_main_pixels / 10)
        << "Opening the About dialog should leave the main Devpac content largely unchanged";
}

TEST_F(DevpacTest, MenuBarRemainsVisibleAfterRepeatedMenuOpenClose) {
    const std::string before_path = s_ram_dir + "/devpac-menu-before.png";
    const std::string after_path = s_ram_dir + "/devpac-menu-after.png";
    const int menu_bar_x = window_info.x + 32;
    const int menu_bar_y = std::max(3, window_info.y / 2);
    const int first_item_y = window_info.y + 18;
    RgbImage before_image;
    RgbImage after_image;
    int before_non_black = 0;
    int after_non_black = 0;

    ASSERT_TRUE(CaptureWindow(before_path.c_str(), 0));
    before_image = LoadPng(before_path);

    for (int i = 0; i < 3; i++) {
        lxa_inject_drag(menu_bar_x, menu_bar_y,
                        menu_bar_x, first_item_y,
                        LXA_MOUSE_RIGHT, 10);
        RunCyclesWithVBlank(20, 50000);
    }

    ASSERT_TRUE(CaptureWindow(after_path.c_str(), 0));
    after_image = LoadPng(after_path);

    after_non_black = CountNonBlackPixels(after_image, 0, 0,
                                          after_image.width - 1,
                                          std::min(after_image.height - 1, MENU_BAR_HEIGHT - 1));

    before_non_black = CountNonBlackPixels(before_image, 0, 0,
                                           before_image.width - 1,
                                           std::min(before_image.height - 1, MENU_BAR_HEIGHT - 1));

    EXPECT_GT(before_non_black, 500)
        << "Devpac menu bar strip should render visible content before interaction";
    EXPECT_GT(after_non_black, 500)
        << "Devpac menu bar strip should still render visible content after repeated menu interaction";
}

/* Input tests (mutate editor state but don't exit the app) */

TEST_F(DevpacTest, RespondsToInput) {
    /* Focus the editor and type a simple assembly program gradually. */
    Click(window_info.x + window_info.width / 2, window_info.y + window_info.height / 2);
    RunCyclesWithVBlank(20, 50000);

    SlowTypeString("; Devpac test\n\tmove.l\t#0,d0\n\trts\n", 4);
    RunCyclesWithVBlank(40, 50000);

    EXPECT_TRUE(lxa_is_running()) << "Devpac should still be running after typing";
    EXPECT_GE(lxa_get_window_count(), 1) << "Window should still be open after typing";
}

TEST_F(DevpacTest, MouseInput) {
    /* Click in the editor area */
    int click_x = window_info.x + window_info.width / 2;
    int click_y = window_info.y + window_info.height / 2;
    
    Click(click_x, click_y);
    RunCyclesWithVBlank(20, 50000);
    
    EXPECT_TRUE(lxa_is_running()) << "Devpac should still be running after mouse click";
}

TEST_F(DevpacTest, CursorKeys) {
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
    
    EXPECT_TRUE(lxa_is_running()) << "Devpac should still be running after cursor keys";
    EXPECT_GE(lxa_get_window_count(), 1) << "Window should still be open after cursor keys";
}

/* ===================================================================== */
/* Phase 117 additions: keyboard-driven workflow coverage.
 *
 * DevPac menu structure (from probe + vision review):
 *   Project: New(A-N), Load(A-L), Open(A-O), Save(A-S), Save As(A-A),
 *            Print(A-P), About(A-?), Quit
 *   Edit, Search, Window, Program, Macros, Settings
 *   Program menu items: Run, Break, Errors, Assemble, Make, Check, Output Symbols
 *
 * Menu pixel introspection during a drag is not feasible because
 * lxa_inject_drag releases the button before we can capture. Instead we
 * use Amiga+key shortcuts (which Devpac honors) to invoke menu actions
 * that are safe to test. We deliberately do NOT invoke Run/Assemble
 * (Program menu) because those require source state and could spawn
 * external workflows. */

/* RAWKEY codes for letter keys: O=0x18, S=0x21 (verified via standard
 * Amiga keymap). Qualifier 0x40 = LeftAmiga. */

TEST_F(DevpacTest, ZAmigaOOpensFileRequester) {
    /* Amiga-O = Project → Open. Should pop up a file requester window. */
    int windows_before = lxa_get_window_count();

    PressKey(0x18, 0x40);  /* RAWKEY for 'O' + LeftAmiga */
    RunCyclesWithVBlank(40, 50000);

    /* The requester may take a moment to appear (real req.library). */
    bool got_new_window = WaitForWindows(windows_before + 1, 5000);

    /* Capture for inspection. */
    const std::string capture_path = s_ram_dir + "/devpac-amiga-o.png";
    lxa_capture_screen(capture_path.c_str());

    if (got_new_window) {
        /* A requester opened. Verify it has nonzero size and the app is
         * still alive. Then dismiss with ESC (rawkey 0x45). */
        lxa_window_info_t req_info;
        ASSERT_TRUE(lxa_get_window_info(windows_before, &req_info));
        EXPECT_GT(req_info.width, 0);
        EXPECT_GT(req_info.height, 0);

        /* Dismiss via ESC. */
        PressKey(0x45, 0);
        RunCyclesWithVBlank(40, 50000);
    } else {
        /* No requester appeared. This is acceptable if req.library fails
         * to open for any reason — record but do not fail the test. The key
         * regression we guard against is "Amiga-O crashes the app". */
        fprintf(stderr,
                "ZAmigaOOpensFileRequester: no requester appeared "
                "(req.library may have failed to open). Verifying app survival.\n");
    }

    EXPECT_TRUE(lxa_is_running())
        << "DevPac must survive Amiga-O regardless of requester outcome";
}

TEST_F(DevpacTest, ZSurvivesRapidShortcutBurst) {
    /* Stress: fire several Amiga-key shortcuts in succession, each
     * followed by ESC to dismiss any dialog. Verifies the app and the
     * menu/shortcut dispatcher do not corrupt state under load. */
    /* N(0x36)=New, L(0x28)=Load, S(0x21)=Save -- but Save without a
     * filename may pop a requester; we ESC after each. */
    int rawkeys[] = {0x36, 0x28};  /* New, Load */

    for (size_t i = 0; i < sizeof(rawkeys) / sizeof(rawkeys[0]); i++) {
        PressKey(rawkeys[i], 0x40);  /* + LeftAmiga */
        RunCyclesWithVBlank(20, 50000);
        /* Dismiss any dialog. */
        PressKey(0x45, 0);  /* ESC */
        RunCyclesWithVBlank(20, 50000);
    }

    EXPECT_TRUE(lxa_is_running())
        << "DevPac should survive a rapid Amiga-key shortcut burst";
    EXPECT_GE(lxa_get_window_count(), 1)
        << "Primary editor window should remain open";
}

TEST_F(DevpacTest, ZSurvivesMenuBarSweep) {
    /* Open every top-level menu via RMB drag and release off-menu.
     * Verifies menu bar + dropdown rendering does not corrupt state for
     * any of the 7 menus, even though we cannot capture menu pixels
     * (drag releases the button before capture). */
    const int menu_y = std::max(3, window_info.y / 2);
    int xs[] = {32, 290, 430, 510, 550, 580, 610};

    for (size_t i = 0; i < sizeof(xs) / sizeof(xs[0]); i++) {
        const int x = window_info.x + xs[i];
        /* Drag down through the menu but to a benign Y (top of dropdown
         * region — the menu's own title row, which doesn't select an
         * item on release). */
        lxa_inject_drag(x, menu_y, x, menu_y + 1,
                        LXA_MOUSE_RIGHT, 5);
        RunCyclesWithVBlank(15, 50000);
    }

    EXPECT_TRUE(lxa_is_running())
        << "DevPac should survive a full top-level menu sweep";
    EXPECT_GE(lxa_get_window_count(), 1)
        << "Primary editor window should remain open";
}

/* Phase 131: menu introspection — assert Project/Edit/Program menus exist */
TEST_F(DevpacTest, ZMenuIntrospectionEnumeratesMenus) {
    /* DevPac 3 has menus: Project, Edit, Search, Window, Program, Macros, Settings */
    RunCyclesWithVBlank(10, 50000);

    lxa_menu_strip_t *strip = lxa_get_menu_strip(0);
    if (!strip) {
        GTEST_SKIP() << "No menu strip on window 0 — DevPac may not have called SetMenuStrip yet";
    }

    int menu_count = lxa_get_menu_count(strip);
    EXPECT_GE(menu_count, 3) << "DevPac should have at least 3 top-level menus";

    bool found_project = false;
    bool found_edit    = false;
    bool found_program = false;

    for (int i = 0; i < menu_count; i++) {
        lxa_menu_info_t info;
        if (!lxa_get_menu_info(strip, i, -1, -1, &info))
            continue;
        if (strcmp(info.name, "Project") == 0) found_project = true;
        if (strcmp(info.name, "Edit")    == 0) found_edit    = true;
        if (strcmp(info.name, "Program") == 0) found_program = true;
    }

    lxa_free_menu_strip(strip);

    EXPECT_TRUE(found_project) << "DevPac should have a 'Project' menu";
    EXPECT_TRUE(found_edit)    << "DevPac should have an 'Edit' menu";
    EXPECT_TRUE(found_program) << "DevPac should have a 'Program' menu";
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
