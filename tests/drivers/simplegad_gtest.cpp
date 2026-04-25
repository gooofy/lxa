/**
 * simplegad_gtest.cpp - Google Test version of SimpleGad test
 *
 * This test driver uses Google Test + liblxa to:
 * 1. Start the SimpleGad sample (RKM original)
 * 2. Wait for the window to open
 * 3. Verify pixel-level correctness of window rendering (non-rootless mode)
 * 4. Click the button and verify GADGETDOWN + GADGETUP (ID 3)
 * 5. Click the close gadget to close the window
 * 6. Verify the program exits cleanly
 *
 * Phase 107: Both fixtures use SetUpTestSuite() to load SimpleGad once,
 * avoiding redundant emulator init + program load per test case.
 */

#include "lxa_test.h"

using namespace lxa::testing;

// Gadget positions (from simplegad.c RKM original)
constexpr int BUTTON_LEFT = 20;
constexpr int BUTTON_TOP = 20;
constexpr int BUTTON_WIDTH = 100;
constexpr int BUTTON_HEIGHT = 50;

// Window title bar height (approximate)
constexpr int TITLE_BAR_HEIGHT = 11;

// Amiga default palette pen indices
constexpr int PEN_GREY = 0;   // Background grey
constexpr int PEN_BLACK = 1;  // Black (0x00, 0x00, 0x00)
constexpr int PEN_WHITE = 2;  // White (0xFF, 0xFF, 0xFF)

// ============================================================================
// Behavioral tests (rootless mode - default)
// ============================================================================

class SimpleGadTest : public ::testing::Test {
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
            fprintf(stderr, "SimpleGadTest: could not find lxa.rom\n");
            return;
        }

        if (lxa_init(&config) != 0) {
            fprintf(stderr, "SimpleGadTest: lxa_init() failed\n");
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

        /* Load SimpleGad */
        if (lxa_load_program("SYS:SimpleGad", "") != 0) {
            fprintf(stderr, "SimpleGadTest: failed to load SimpleGad\n");
            lxa_shutdown();
            return;
        }

        if (!lxa_wait_windows(1, 5000)) {
            fprintf(stderr, "SimpleGadTest: window did not open\n");
            lxa_shutdown();
            return;
        }

        if (!lxa_get_window_info(0, &s_window_info)) {
            fprintf(stderr, "SimpleGadTest: could not get window info\n");
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
        lxa_capture_screen("/home/guenter/projects/amiga/lxa/src/lxa/screenshots/lxa-tests/simplegad-behavior.png");
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

    std::string GetOutput() {
        char buf[65536];
        lxa_get_output(buf, sizeof(buf));
        return std::string(buf);
    }
    void ClearOutput() { lxa_clear_output(); }

    bool WaitForWindowDrawn(int index = 0, int timeout_ms = 5000) {
        return lxa_wait_window_drawn(index, timeout_ms);
    }

    bool CaptureWindow(const char* filename, int index = 0) {
        return lxa_capture_window(index, filename);
    }
};

/* Static member definitions */
bool                SimpleGadTest::s_setup_ok = false;
lxa_window_info_t   SimpleGadTest::s_window_info = {};
std::string         SimpleGadTest::s_ram_dir;
std::string         SimpleGadTest::s_t_dir;
std::string         SimpleGadTest::s_env_dir;

/* ----- Tests: read-only first, destructive (CloseWindow) last ----- */

TEST_F(SimpleGadTest, WindowOpens) {
    // Just verify the window opened successfully
    EXPECT_GT(window_info.width, 0);
    EXPECT_GT(window_info.height, 0);
}

TEST_F(SimpleGadTest, RootlessWindowShowsGadgetBorder) {
    const std::string capture_path = s_ram_dir + "/simplegad-window.png";
    RgbImage image;
    int top_edge_pixels = 0;
    int left_edge_pixels = 0;
    int right_edge_pixels = 0;
    int bottom_edge_pixels = 0;
    int bx0 = 19;
    int by0 = 19;
    int bx1 = 19 + BUTTON_WIDTH + 1;
    int by1 = 19 + BUTTON_HEIGHT + 1;

    ASSERT_TRUE(WaitForWindowDrawn(0, 5000))
        << "Rootless SimpleGad window did not draw";
    RunCyclesWithVBlank(20, 50000);

    ASSERT_TRUE(CaptureWindow(capture_path.c_str(), 0))
        << "Rootless SimpleGad window capture should succeed";

    image = LoadPng(capture_path);
    ASSERT_GE(image.width, bx1 + 1);
    ASSERT_GE(image.height, by1 + 1);

    auto is_black = [&](int x, int y) {
        size_t idx = (static_cast<size_t>(y) * static_cast<size_t>(image.width) +
                      static_cast<size_t>(x)) * 3u;
        return image.pixels[idx] == 0 && image.pixels[idx + 1] == 0 && image.pixels[idx + 2] == 0;
    };

    for (int x = bx0; x <= bx1; x++) {
        if (is_black(x, by0)) top_edge_pixels++;
        if (is_black(x, by1)) bottom_edge_pixels++;
    }
    for (int y = by0; y <= by1; y++) {
        if (is_black(bx0, y)) left_edge_pixels++;
        if (is_black(bx1, y)) right_edge_pixels++;
    }

    EXPECT_GT(top_edge_pixels, 90)
        << "Rootless top edge of gadget border should contain black pixels (got "
        << top_edge_pixels << ")";
    EXPECT_GT(bottom_edge_pixels, 90)
        << "Rootless bottom edge of gadget border should contain black pixels (got "
        << bottom_edge_pixels << ")";
    EXPECT_GT(left_edge_pixels, 45)
        << "Rootless left edge of gadget border should contain black pixels (got "
        << left_edge_pixels << ")";
    EXPECT_GT(right_edge_pixels, 45)
        << "Rootless right edge of gadget border should contain black pixels (got "
        << right_edge_pixels << ")";
}

TEST_F(SimpleGadTest, ButtonClick) {
    // Calculate button center
    int btn_x = window_info.x + BUTTON_LEFT + (BUTTON_WIDTH / 2);
    int btn_y = window_info.y + TITLE_BAR_HEIGHT + BUTTON_TOP + (BUTTON_HEIGHT / 2);
    
    ClearOutput();

    // Click the button
    Click(btn_x, btn_y);
    
    // Process event through VBlanks
    RunCyclesWithVBlank(20, 50000);
    
    // Check output
    std::string output = GetOutput();
    EXPECT_NE(output.find("IDCMP_GADGETDOWN"), std::string::npos) 
        << "Expected GADGETDOWN event. Output was: " << output;
    EXPECT_NE(output.find("IDCMP_GADGETUP"), std::string::npos) 
        << "Expected GADGETUP event. Output was: " << output;
    EXPECT_NE(output.find("gadget number 3"), std::string::npos) 
        << "Expected gadget ID 3. Output was: " << output;
}

/* CloseWindow MUST be the last test — it terminates the emulated program. */
TEST_F(SimpleGadTest, CloseWindow) {
    ClearOutput();
    
    // Click close gadget (top-left of window)
    int close_x = window_info.x + 10;
    int close_y = window_info.y + 5;
    
    Click(close_x, close_y);
    
    // Give additional VBlanks for the close event to propagate
    RunCyclesWithVBlank(20, 50000);
    
    // Wait for exit
    ASSERT_TRUE(lxa_wait_exit(5000)) 
        << "Program did not exit within 5 seconds";
    
    // Verify CLOSEWINDOW event
    std::string output = GetOutput();
    EXPECT_NE(output.find("IDCMP_CLOSEWINDOW"), std::string::npos) 
        << "Expected CLOSEWINDOW event. Output was: " << output;
}

// ============================================================================
// Pixel verification tests (non-rootless mode)
//
// In rootless mode, Intuition skips window frame rendering since the host
// window manager provides decorations. For pixel-level verification of
// Amiga-side rendering (borders, gadgets, title bars), we must use
// non-rootless mode so everything is drawn to the screen bitmap.
// ============================================================================

class SimpleGadPixelTest : public ::testing::Test {
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
            fprintf(stderr, "SimpleGadPixelTest: could not find lxa.rom\n");
            return;
        }

        if (lxa_init(&config) != 0) {
            fprintf(stderr, "SimpleGadPixelTest: lxa_init() failed\n");
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

        /* Load SimpleGad */
        if (lxa_load_program("SYS:SimpleGad", "") != 0) {
            fprintf(stderr, "SimpleGadPixelTest: failed to load SimpleGad\n");
            lxa_shutdown();
            return;
        }

        if (!lxa_wait_windows(1, 5000)) {
            fprintf(stderr, "SimpleGadPixelTest: window did not open\n");
            lxa_shutdown();
            return;
        }

        if (!lxa_get_window_info(0, &s_window_info)) {
            fprintf(stderr, "SimpleGadPixelTest: could not get window info\n");
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

        s_setup_ok = true;
    }

    static void TearDownTestSuite() {
        lxa_capture_screen("/home/guenter/projects/amiga/lxa/src/lxa/screenshots/lxa-tests/simplegad-pixels.png");
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

    int ReadPixel(int x, int y) {
        int pen;
        if (lxa_read_pixel(x, y, &pen)) {
            return pen;
        }
        return -1;
    }

    bool ReadPixelRGB(int x, int y, uint8_t* r, uint8_t* g, uint8_t* b) {
        return lxa_read_pixel_rgb(x, y, r, g, b);
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
bool                SimpleGadPixelTest::s_setup_ok = false;
lxa_window_info_t   SimpleGadPixelTest::s_window_info = {};
std::string         SimpleGadPixelTest::s_ram_dir;
std::string         SimpleGadPixelTest::s_t_dir;
std::string         SimpleGadPixelTest::s_env_dir;

/* ----- Pixel tests ----- */

TEST_F(SimpleGadPixelTest, WindowBorderRendered) {
    // Verify that the window title bar has non-background pixels
    int title_content = CountContentPixels(
        window_info.x + 1,
        window_info.y + 1,
        window_info.x + window_info.width - 2,
        window_info.y + TITLE_BAR_HEIGHT - 1,
        PEN_GREY
    );
    EXPECT_GT(title_content, 0)
        << "Title bar should contain non-background pixels (close gadget, title text)";
    
    // Verify the window border - top edge should be non-grey
    int top_border_pen = ReadPixel(window_info.x, window_info.y);
    EXPECT_NE(top_border_pen, -1) << "Should be able to read pixel at window top-left corner";
    EXPECT_NE(top_border_pen, PEN_GREY)
        << "Window border should not be background color (pen=" << top_border_pen << ")";
}

TEST_F(SimpleGadPixelTest, GadgetBorderRendered) {
    int bx0 = 19;
    int by0 = 19;
    int bx1 = 19 + BUTTON_WIDTH + 1;
    int by1 = 19 + BUTTON_HEIGHT + 1;
    
    // Verify top edge
    int top_edge_pixels = 0;
    for (int x = bx0; x <= bx1; x++) {
        int pen = ReadPixel(window_info.x + x, window_info.y + by0);
        if (pen == PEN_BLACK) top_edge_pixels++;
    }
    EXPECT_GT(top_edge_pixels, 90)
        << "Top edge of gadget border should have pen-1 pixels (got " << top_edge_pixels << ")";
    
    // Verify left edge
    int left_edge_pixels = 0;
    for (int y = by0; y <= by1; y++) {
        int pen = ReadPixel(window_info.x + bx0, window_info.y + y);
        if (pen == PEN_BLACK) left_edge_pixels++;
    }
    EXPECT_GT(left_edge_pixels, 45)
        << "Left edge of gadget border should have pen-1 pixels (got " << left_edge_pixels << ")";
    
    // Verify right edge
    int right_edge_pixels = 0;
    for (int y = by0; y <= by1; y++) {
        int pen = ReadPixel(window_info.x + bx1, window_info.y + y);
        if (pen == PEN_BLACK) right_edge_pixels++;
    }
    EXPECT_GT(right_edge_pixels, 45)
        << "Right edge of gadget border should have pen-1 pixels (got " << right_edge_pixels << ")";
    
    // Verify bottom edge
    int bottom_edge_pixels = 0;
    for (int x = bx0; x <= bx1; x++) {
        int pen = ReadPixel(window_info.x + x, window_info.y + by1);
        if (pen == PEN_BLACK) bottom_edge_pixels++;
    }
    EXPECT_GT(bottom_edge_pixels, 90)
        << "Bottom edge of gadget border should have pen-1 pixels (got " << bottom_edge_pixels << ")";
    
    // Verify interior is pen 0
    int interior_pen = ReadPixel(window_info.x + BUTTON_LEFT + 10,
                                 window_info.y + BUTTON_TOP + 10);
    EXPECT_EQ(interior_pen, PEN_GREY)
        << "Gadget interior should be background color (pen 0)";
}

TEST_F(SimpleGadPixelTest, WindowInteriorColor) {
    int check_x = window_info.x + BUTTON_LEFT + BUTTON_WIDTH + 20;
    int check_y = window_info.y + TITLE_BAR_HEIGHT + 10;
    
    if (check_x < window_info.x + window_info.width - 2 &&
        check_y < window_info.y + window_info.height - 2) {
        int pen = ReadPixel(check_x, check_y);
        EXPECT_NE(pen, -1) << "Should be able to read pixel in window interior";
        EXPECT_EQ(pen, PEN_GREY)
            << "Window interior (away from gadgets) should be grey background (pen 0)";
    }
    
    uint8_t r, g, b;
    if (ReadPixelRGB(check_x, check_y, &r, &g, &b)) {
        EXPECT_EQ(r, 0xAA) << "Background red component should be 0xAA (Workbench grey)";
        EXPECT_EQ(g, 0xAA) << "Background green component should be 0xAA (Workbench grey)";
        EXPECT_EQ(b, 0xAA) << "Background blue component should be 0xAA (Workbench grey)";
    }
}

TEST_F(SimpleGadPixelTest, NoDepthGadgetInTopRight) {
    constexpr int SYS_GADGET_WIDTH = 18;
    constexpr int BORDER_RIGHT = 2;
    
    int gadget_area_x0 = window_info.width - BORDER_RIGHT - SYS_GADGET_WIDTH;
    int gadget_area_x1 = window_info.width - BORDER_RIGHT - 1;
    int gadget_area_y0 = 1;
    int gadget_area_y1 = TITLE_BAR_HEIGHT - 1;
    
    int shine_count = 0;
    for (int y = gadget_area_y0; y <= gadget_area_y1; y++) {
        int pen = ReadPixel(window_info.x + gadget_area_x0, window_info.y + y);
        if (pen == PEN_WHITE) shine_count++;
    }
    
    int gadget_height = gadget_area_y1 - gadget_area_y0 + 1;
    EXPECT_LT(shine_count, gadget_height - 1)
        << "Left edge of potential depth gadget area should NOT have a full vertical "
           "shine line (pen 2). shine_count=" << shine_count << " out of " << gadget_height;
    
    int top_row_shine = 0;
    for (int x = gadget_area_x0; x <= gadget_area_x1; x++) {
        int pen = ReadPixel(window_info.x + x, window_info.y + gadget_area_y0);
        if (pen == PEN_WHITE) top_row_shine++;
    }
    
    EXPECT_LT(top_row_shine, SYS_GADGET_WIDTH / 2)
        << "Top row of potential depth gadget area should NOT have a shine line. "
           "top_row_shine=" << top_row_shine << " out of " << SYS_GADGET_WIDTH;
}

TEST_F(SimpleGadPixelTest, GadghcompHighlightOnClick) {
    struct { int x; int y; } sample_points[] = {
        { BUTTON_LEFT + 10, BUTTON_TOP + 10 },
        { BUTTON_LEFT + 50, BUTTON_TOP + 25 },
        { BUTTON_LEFT + 80, BUTTON_TOP + 40 },
    };
    constexpr int NUM_SAMPLES = sizeof(sample_points) / sizeof(sample_points[0]);
    
    // 1. Capture pre-click pen values
    int pre_click_pens[NUM_SAMPLES];
    for (int i = 0; i < NUM_SAMPLES; i++) {
        pre_click_pens[i] = ReadPixel(window_info.x + sample_points[i].x,
                                       window_info.y + sample_points[i].y);
        ASSERT_NE(pre_click_pens[i], -1) 
            << "Should be able to read pixel at sample point " << i;
    }
    
    // 2. Inject mouse-down only
    int btn_x = window_info.x + BUTTON_LEFT + (BUTTON_WIDTH / 2);
    int btn_y = window_info.y + BUTTON_TOP + (BUTTON_HEIGHT / 2);
    
    lxa_inject_mouse(btn_x, btn_y, 0, LXA_EVENT_MOUSEMOVE);
    lxa_trigger_vblank();
    lxa_run_cycles(10000);
    
    lxa_inject_mouse(btn_x, btn_y, LXA_MOUSE_LEFT, LXA_EVENT_MOUSEBUTTON);
    
    for (int i = 0; i < 10; i++) {
        lxa_trigger_vblank();
        lxa_run_cycles(50000);
    }
    lxa_flush_display();
    
    // 3. Read pixels while button is held — should be complemented
    int pressed_pens[NUM_SAMPLES];
    int changed_count = 0;
    for (int i = 0; i < NUM_SAMPLES; i++) {
        pressed_pens[i] = ReadPixel(window_info.x + sample_points[i].x,
                                     window_info.y + sample_points[i].y);
        ASSERT_NE(pressed_pens[i], -1)
            << "Should be able to read pixel at sample point " << i << " while pressed";
        if (pressed_pens[i] != pre_click_pens[i]) {
            changed_count++;
        }
    }
    
    EXPECT_EQ(changed_count, NUM_SAMPLES)
        << "GADGHCOMP: All interior pixels should change when button is pressed. "
           "Pre-click pens: " << pre_click_pens[0] << "," << pre_click_pens[1] << "," << pre_click_pens[2]
        << " Pressed pens: " << pressed_pens[0] << "," << pressed_pens[1] << "," << pressed_pens[2];
    
    // 4. Release button
    lxa_inject_mouse(btn_x, btn_y, 0, LXA_EVENT_MOUSEBUTTON);
    
    for (int i = 0; i < 10; i++) {
        lxa_trigger_vblank();
        lxa_run_cycles(50000);
    }
    lxa_flush_display();
    
    // 5. Read pixels after release — should be restored
    int released_pens[NUM_SAMPLES];
    int restored_count = 0;
    for (int i = 0; i < NUM_SAMPLES; i++) {
        released_pens[i] = ReadPixel(window_info.x + sample_points[i].x,
                                      window_info.y + sample_points[i].y);
        if (released_pens[i] == pre_click_pens[i]) {
            restored_count++;
        }
    }
    
    EXPECT_EQ(restored_count, NUM_SAMPLES)
        << "GADGHCOMP: All pixels should be restored after button release. "
           "Pre: " << pre_click_pens[0] << "," << pre_click_pens[1] << "," << pre_click_pens[2]
        << " Released: " << released_pens[0] << "," << released_pens[1] << "," << released_pens[2];
}

// Main function for Google Test
int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
