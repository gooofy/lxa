/**
 * lxa_test.h - Base class and utilities for lxa Google Test suites
 *
 * Provides common functionality for all lxa tests including:
 * - Automatic liblxa initialization and cleanup
 * - Common test fixtures and helpers
 * - Standard assertions and matchers
 */

#ifndef LXA_TEST_H
#define LXA_TEST_H

#include <gtest/gtest.h>
#include "lxa_api.h"
#include <png.h>
#include <cstdint>
#include <fstream>
#include <string>
#include <cstring>
#include <limits.h>
#include <unistd.h>
#include <cstdio>
#include <cstdlib>
#include <vector>

#ifndef LXA_TEST_PREFIX
#define LXA_TEST_PREFIX "/home/guenter/projects/amiga/lxa/usr"
#endif

#ifndef LXA_SCREENSHOTS_DIR
#define LXA_SCREENSHOTS_DIR "/home/guenter/projects/amiga/lxa/src/lxa/screenshots/lxa-tests"
#endif

namespace lxa {
namespace testing {

/**
 * Save a screenshot to screenshots/lxa-tests/<name>.png.
 * Creates the directory if it does not exist.
 * Always overwrites any existing file with the same name so the folder
 * always reflects the most recent test run.
 *
 * Uses lxa_capture_screen() which flushes the display before saving.
 * Returns true on success.
 */
inline bool SaveScreenshot(const std::string& name)
{
    std::string dir = LXA_SCREENSHOTS_DIR;
    std::string cmd = "mkdir -p " + dir;
    system(cmd.c_str());
    std::string path = dir + "/" + name + ".png";
    return lxa_capture_screen(path.c_str());
}

struct RgbImage {
    int width;
    int height;
    std::vector<uint8_t> pixels;
};

inline RgbImage LoadPng(const std::string& path) {
    std::ifstream capture(path, std::ios::binary);
    RgbImage image = {0, 0, {}};
    png_byte header[8];

    EXPECT_TRUE(capture.good()) << "Failed to open captured PNG image at " << path;
    if (!capture.good()) {
        return image;
    }

    capture.read(reinterpret_cast<char*>(header), sizeof(header));
    EXPECT_EQ(capture.gcount(), static_cast<std::streamsize>(sizeof(header)));
    if (capture.gcount() != static_cast<std::streamsize>(sizeof(header))) {
        return image;
    }
    EXPECT_EQ(png_sig_cmp(header, 0, sizeof(header)), 0)
        << "Captured image should use the PNG format";
    if (png_sig_cmp(header, 0, sizeof(header)) != 0) {
        return image;
    }

    png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
    EXPECT_NE(png, nullptr);
    if (png == nullptr) {
        return image;
    }
    png_infop info = png_create_info_struct(png);
    EXPECT_NE(info, nullptr);
    if (info == nullptr) {
        png_destroy_read_struct(&png, nullptr, nullptr);
        return image;
    }

    if (setjmp(png_jmpbuf(png))) {
        png_destroy_read_struct(&png, &info, nullptr);
        ADD_FAILURE() << "Failed to decode PNG image at " << path;
        return image;
    }

    png_set_read_fn(
        png,
        &capture,
        [](png_structp png_ptr, png_bytep data, png_size_t length) {
            std::istream* stream = static_cast<std::istream*>(png_get_io_ptr(png_ptr));
            stream->read(reinterpret_cast<char*>(data), static_cast<std::streamsize>(length));
            if (stream->gcount() != static_cast<std::streamsize>(length)) {
                png_error(png_ptr, "Unexpected end of PNG stream");
            }
        });
    png_set_sig_bytes(png, sizeof(header));
    png_read_info(png, info);

    if (png_get_bit_depth(png, info) == 16) {
        png_set_strip_16(png);
    }
    if (png_get_color_type(png, info) == PNG_COLOR_TYPE_PALETTE) {
        png_set_palette_to_rgb(png);
    }
    if (png_get_color_type(png, info) == PNG_COLOR_TYPE_GRAY &&
        png_get_bit_depth(png, info) < 8) {
        png_set_expand_gray_1_2_4_to_8(png);
    }
    if (png_get_valid(png, info, PNG_INFO_tRNS)) {
        png_set_tRNS_to_alpha(png);
    }
    if (png_get_color_type(png, info) == PNG_COLOR_TYPE_GRAY ||
        png_get_color_type(png, info) == PNG_COLOR_TYPE_GRAY_ALPHA) {
        png_set_gray_to_rgb(png);
    }
    if (png_get_color_type(png, info) & PNG_COLOR_MASK_ALPHA) {
        png_set_strip_alpha(png);
    }

    png_read_update_info(png, info);

    image.width = static_cast<int>(png_get_image_width(png, info));
    image.height = static_cast<int>(png_get_image_height(png, info));
    EXPECT_GT(image.width, 0);
    EXPECT_GT(image.height, 0);
    if (image.width <= 0 || image.height <= 0) {
        png_destroy_read_struct(&png, &info, nullptr);
        return image;
    }

    png_size_t rowbytes = png_get_rowbytes(png, info);
    EXPECT_EQ(rowbytes, static_cast<png_size_t>(image.width) * 3u)
        << "Captured PNG should decode to RGB rows";
    if (rowbytes != static_cast<png_size_t>(image.width) * 3u) {
        png_destroy_read_struct(&png, &info, nullptr);
        image = {0, 0, {}};
        return image;
    }

    image.pixels.resize(static_cast<size_t>(image.height) * static_cast<size_t>(rowbytes));
    std::vector<png_bytep> rows(static_cast<size_t>(image.height));
    for (int y = 0; y < image.height; ++y) {
        rows[static_cast<size_t>(y)] = image.pixels.data() + static_cast<size_t>(y) * rowbytes;
    }

    png_read_image(png, rows.data());
    png_read_end(png, nullptr);
    png_destroy_read_struct(&png, &info, nullptr);

    return image;
}

/**
 * Helper function to find ROM path.
 */
inline const char* FindRomPath() {
    static std::string rom_path;
    const char* locations[] = {
        "target/rom/lxa.rom",
        "../target/rom/lxa.rom",
        "../../target/rom/lxa.rom",
        "build/target/rom/lxa.rom",
        nullptr
    };
    
    for (int i = 0; locations[i]; i++) {
        if (access(locations[i], R_OK) == 0) {
            rom_path = locations[i];
            return rom_path.c_str();
        }
    }
    
    // Debug: print current directory to help diagnose
    char cwd[1024];
    if (getcwd(cwd, sizeof(cwd)) != nullptr) {
        fprintf(stderr, "Warning: Could not find ROM, current dir: %s\n", cwd);
    }
    
    return nullptr;
}

/**
 * Helper function to find samples path.
 */
inline const char* FindSamplesPath() {
    static std::string samples_path;
    const char* locations[] = {
        "target/samples/Samples",
        "../target/samples/Samples",
        "../../target/samples/Samples",
        "build/target/samples/Samples",
        nullptr
    };
    
    for (int i = 0; locations[i]; i++) {
        if (access(locations[i], F_OK) == 0) {
            samples_path = locations[i];
            return samples_path.c_str();
        }
    }
    
    return nullptr;
}

/**
 * Helper function to find system commands path.
 */
inline const char* FindCommandsPath() {
    static std::string commands_path;
    const char* locations[] = {
        "target/sys/C",
        "../target/sys/C",
        "../../target/sys/C",
        "build/target/sys/C",
        nullptr
    };
    
    for (int i = 0; locations[i]; i++) {
        if (access(locations[i], F_OK) == 0) {
            commands_path = locations[i];
            return commands_path.c_str();
        }
    }
    
    return nullptr;
}

/**
 * Helper function to find system commands path (System/Shell).
 */
inline const char* FindSystemPath() {
    static std::string system_path;
    const char* locations[] = {
        "target/sys/System",
        "../target/sys/System",
        "../../target/sys/System",
        "build/target/sys/System",
        nullptr
    };
    
    for (int i = 0; locations[i]; i++) {
        if (access(locations[i], F_OK) == 0) {
            system_path = locations[i];
            return system_path.c_str();
        }
    }
    
    return nullptr;
}

/**
 * Helper function to find system base path (containing System/ directory).
 */
inline const char* FindSystemBasePath() {
    static std::string system_base_path;
    const char* locations[] = {
        "target/sys",
        "../target/sys",
        "../../target/sys",
        "build/target/sys",
        nullptr
    };
    
    for (int i = 0; locations[i]; i++) {
        std::string check_path = std::string(locations[i]) + "/System";
        if (access(check_path.c_str(), F_OK) == 0) {
            system_base_path = locations[i];
            return system_base_path.c_str();
        }
    }
    
    return nullptr;
}

/**
 * Helper function to find system libraries path (Libs/).
 */
inline const char* FindSystemLibsPath() {
    static std::string system_libs_path;
    const char* base = FindSystemBasePath();
    if (base == nullptr) {
        return nullptr;
    }

    system_libs_path = std::string(base) + "/Libs";
    if (access(system_libs_path.c_str(), F_OK) == 0) {
        return system_libs_path.c_str();
    }

    return nullptr;
}

/**
 * Helper function to find the gadgets path (Libs/gadgets/).
 *
 * This is used to set up the GADGETS: assign so apps that load
 * gadget classes (e.g., BGUI gadgets, textfield.gadget) can find them.
 * The gadgets directory lives alongside the disk libraries in Libs/gadgets/.
 *
 * The installed location (share/lxa/System/Libs/gadgets/) is checked in
 * addition to the build-tree location.
 */
inline const char* FindGadgetsPath() {
    static std::string gadgets_path;

    /* Check share/lxa/System/Libs/gadgets (installed location, used by CI) */
    const char* candidates[] = {
        "share/lxa/System/Libs/gadgets",
        "../share/lxa/System/Libs/gadgets",
        "../../share/lxa/System/Libs/gadgets",
        nullptr
    };
    for (int i = 0; candidates[i]; i++) {
        if (access(candidates[i], F_OK) == 0) {
            char resolved[PATH_MAX];
            if (realpath(candidates[i], resolved)) {
                gadgets_path = resolved;
                return gadgets_path.c_str();
            }
        }
    }

    /* Fall back to build-tree sys/Libs/gadgets */
    const char* base = FindSystemLibsPath();
    if (base != nullptr) {
        std::string candidate = std::string(base) + "/gadgets";
        if (access(candidate.c_str(), F_OK) == 0) {
            gadgets_path = candidate;
            return gadgets_path.c_str();
        }
    }

    return nullptr;
}

/**
 * Helper function to find an optional third-party libraries path for tests.
 *
 * Third-party library binaries (req.library, reqtools.library, etc.) live in
 * share/lxa/System/Libs/ in the source tree.  This function auto-discovers
 * that directory so the LIBS: assign is set up correctly without requiring an
 * environment variable.
 *
 * The LXA_TEST_THIRD_PARTY_LIBS environment variable can still be used to
 * override the auto-detected path.
 */
inline const char* FindThirdPartyLibsPath() {
    static std::string third_party_libs_path;

    /* Explicit override via environment variable */
    const char* env_path = std::getenv("LXA_TEST_THIRD_PARTY_LIBS");
    if (env_path != nullptr && env_path[0] != '\0' && access(env_path, F_OK) == 0) {
        third_party_libs_path = env_path;
        return third_party_libs_path.c_str();
    }

    /* Auto-discover share/lxa/System/Libs relative to CWD (build tree) */
    const char* candidates[] = {
        "share/lxa/System/Libs",
        "../share/lxa/System/Libs",
        "../../share/lxa/System/Libs",
        nullptr
    };
    for (int i = 0; candidates[i]; i++) {
        if (access(candidates[i], F_OK) == 0) {
            char resolved[PATH_MAX];
            if (realpath(candidates[i], resolved)) {
                third_party_libs_path = resolved;
                return third_party_libs_path.c_str();
            }
        }
    }

    return nullptr;
}

/**
 * Helper function to find lxa-apps directory.
 */
inline const char* FindAppsPath() {
    static std::string apps_path;
    char resolved[PATH_MAX];
    const char* locations[] = {
        "../lxa-apps",
        "../../lxa-apps",
        "../../../lxa-apps",
        "lxa-apps",
        "../src/lxa-apps",
        "../../src/lxa-apps",
        nullptr
    };
    
    for (int i = 0; locations[i]; i++) {
        if (access(locations[i], F_OK) == 0) {
            if (realpath(locations[i], resolved) != nullptr) {
                apps_path = resolved;
            } else {
                apps_path = locations[i];
            }
            return apps_path.c_str();
        }
    }
    
    return nullptr;
}

/**
 * Base test fixture for all lxa tests.
 * Provides automatic setup/teardown of the lxa emulator.
 */
class LxaTest : public ::testing::Test {
protected:
    lxa_config_t config;
    bool initialized;
    std::string t_dir_path;
    std::string env_dir_path;
    std::string ram_dir_path;
    
    LxaTest() : initialized(false) {
        // Default configuration
        memset(&config, 0, sizeof(config));
        config.headless = true;  // All tests run headless by default
        config.verbose = false;
        config.rootless = true;
    }
    
    virtual ~LxaTest() = default;
    
    /**
     * Set up the test fixture.
     * Override this to customize initialization, but remember to call
     * LxaTest::SetUp() from your override.
     */
    void SetUp() override {
        setenv("LXA_PREFIX", LXA_TEST_PREFIX, 1);

        // Set default paths if not already configured
        if (config.rom_path == nullptr) {
            config.rom_path = FindRomPath();
            if (config.rom_path == nullptr) {
                FAIL() << "Could not find lxa.rom in standard locations";
            }
        }
        
        // Initialize lxa (without setting sys_drive in config to avoid drive/assign conflict)
        int result = lxa_init(&config);
        ASSERT_EQ(result, 0) << "Failed to initialize lxa emulator";
        
        // Map SYS: as a multi-assign
        const char* samples_path = FindSamplesPath();
        if (samples_path) {
            lxa_add_assign("SYS", samples_path);
        } else {
            FAIL() << "Could not find samples directory in standard locations";
        }

        // Add system base to SYS: multi-assign if found
        const char* system_base = FindSystemBasePath();
        if (system_base) {
            lxa_add_assign_path("SYS", system_base);
        }

        const char* third_party_libs_path = FindThirdPartyLibsPath();
        if (third_party_libs_path) {
            lxa_add_assign("LIBS", third_party_libs_path);
        }

        const char* libs_path = FindSystemLibsPath();
        if (libs_path) {
            lxa_add_assign_path("LIBS", libs_path);
        }

        // Map RAM: to a temporary directory
        char ram_dir[] = "/tmp/lxa_test_RAM_XXXXXX";
        if (mkdtemp(ram_dir)) {
            ram_dir_path = ram_dir;
            lxa_add_drive("RAM", ram_dir);
        }

        // Map APPS: to lxa-apps directory if found
        const char* apps_path = FindAppsPath();
        if (apps_path) {
            lxa_add_assign("APPS", apps_path);
        }

        // Map GADGETS: to the gadget classes directory if found
        const char* gadgets_path = FindGadgetsPath();
        if (gadgets_path) {
            lxa_add_assign("GADGETS", gadgets_path);
        }

        // Map C: to system commands if found
        const char* commands_path = FindCommandsPath();
        if (commands_path) {
            lxa_add_assign("C", commands_path);
        }

        // Map System: to system binaries if found
        const char* system_path = FindSystemPath();
        if (system_path) {
            lxa_add_assign("System", system_path);
        }

        // Map T: to a temporary directory
        char t_dir[] = "/tmp/lxa_test_T_XXXXXX";
        if (mkdtemp(t_dir)) {
            t_dir_path = t_dir;
            lxa_add_assign("T", t_dir);
        }

        // Map ENV: and ENVARC: to a temporary directory
        char env_dir[] = "/tmp/lxa_test_ENV_XXXXXX";
        if (mkdtemp(env_dir)) {
            env_dir_path = env_dir;
            lxa_add_assign("ENV", env_dir);
            lxa_add_assign("ENVARC", env_dir);
        }

        initialized = true;
    }
    
    /**
     * Tear down the test fixture.
     * Override this to customize cleanup, but remember to call
     * LxaTest::TearDown() from your override.
     */
    void TearDown() override {
        if (initialized) {
            lxa_shutdown();
            initialized = false;
        }

        // Cleanup temporary directories
        if (!ram_dir_path.empty()) {
            std::string cmd = "rm -rf " + ram_dir_path;
            system(cmd.c_str());
            ram_dir_path.clear();
        }
        if (!t_dir_path.empty()) {
            std::string cmd = "rm -rf " + t_dir_path;
            system(cmd.c_str());
            t_dir_path.clear();
        }
        if (!env_dir_path.empty()) {
            std::string cmd = "rm -rf " + env_dir_path;
            system(cmd.c_str());
            env_dir_path.clear();
        }
    }
    
    /**
     * Load and run a program with the given arguments.
     * Returns the exit code of the program.
     */
    int RunProgram(const char* path, const char* args = "", int timeout_ms = 5000) {
        EXPECT_TRUE(initialized) << "lxa not initialized";
        
        int result = lxa_load_program(path, args);
        if (result != 0) {
            return result;
        }
        
        return lxa_run_until_exit(timeout_ms);
    }
    
    /**
     * Get program output as a string.
     */
    std::string GetOutput() {
        char buffer[65536];
        lxa_get_output(buffer, sizeof(buffer));
        return std::string(buffer);
    }
    
    /**
     * Clear output buffer.
     */
    void ClearOutput() {
        lxa_clear_output();
    }

    /**
     * Force IDCMP_REFRESHWINDOW to all open windows.
     *
     * Sets a flag that the next VBlank handler will consume; triggers
     * deferred-paint apps that wait for a REFRESHWINDOW before drawing.
     * Always follow this call with RunCyclesWithVBlank() to let the
     * redraw happen.
     */
    void ForceFullRedraw() {
        lxa_force_full_redraw();
    }

    /**
     * Run CPU cycles without triggering VBlank.
     * Useful for letting tasks reach WaitPort().
     */
    void RunCycles(int count = 10000) {
        lxa_run_cycles(count);
    }
    
    /**
     * Run cycles with VBlank interrupts.
     * Useful for processing input events through Intuition.
     */
    void RunCyclesWithVBlank(int iterations = 20, int cycles_per_iteration = 50000) {
        for (int i = 0; i < iterations; i++) {
            lxa_trigger_vblank();
            lxa_run_cycles(cycles_per_iteration);
        }
    }
    
    /**
     * Wait for a specific number of windows to open.
     */
    bool WaitForWindows(int count, int timeout_ms = 5000) {
        return lxa_wait_windows(count, timeout_ms);
    }
    
    /**
     * Get window information.
     */
    bool GetWindowInfo(int index, lxa_window_info_t* info) {
        return lxa_get_window_info(index, info);
    }

    bool WaitForWindowDrawn(int index = 0, int timeout_ms = 5000) {
        return lxa_wait_window_drawn(index, timeout_ms);
    }

    bool CaptureWindow(const char* filename, int index = 0) {
        return lxa_capture_window(index, filename);
    }

    /**
     * Switch lxa_read_pixel() / CountScreenContent() to read from the given
     * rootless window (0 = first/main window).  In rootless mode each window
     * has its own pixel buffer; after menu operations the active display may
     * have shifted away from the editor window — call this before pixel sampling.
     */
    bool SetActiveWindow(int index = 0) {
        return lxa_set_active_window(index);
    }
    
    /**
     * Inject a mouse click at the given coordinates.
     */
    void Click(int x, int y, int button = LXA_MOUSE_LEFT) {
        lxa_inject_mouse_click(x, y, button);
    }
    
    /**
     * Inject a key press.
     */
    void PressKey(int rawkey, int qualifier = 0) {
        lxa_inject_keypress(rawkey, qualifier);
    }
    
    /**
     * Type a string.
     */
    void TypeString(const char* str) {
        lxa_inject_string(str);
    }

    int GetGadgetCount(int window_index = 0) {
        return lxa_get_gadget_count(window_index);
    }

    bool GetGadgetInfo(int gadget_index, lxa_gadget_info_t* info, int window_index = 0) {
        return lxa_get_gadget_info(window_index, gadget_index, info);
    }

    std::vector<lxa_gadget_info_t> GetGadgets(int window_index = 0) {
        std::vector<lxa_gadget_info_t> gadgets;
        int count = GetGadgetCount(window_index);

        for (int i = 0; i < count; i++) {
            lxa_gadget_info_t info;
            if (GetGadgetInfo(i, &info, window_index)) {
                gadgets.push_back(info);
            }
        }

        return gadgets;
    }

    bool ClickGadget(int gadget_index, int window_index = 0, int button = LXA_MOUSE_LEFT) {
        lxa_gadget_info_t info;
        if (!GetGadgetInfo(gadget_index, &info, window_index)) {
            return false;
        }

        int click_x = info.left + info.width / 2;
        int click_y = info.top + info.height / 2;
        return lxa_inject_mouse_click(click_x, click_y, button);
    }

    /* Phase 131: drain all pending Intuition events from the circular log */
    std::vector<lxa_intui_event_t> DrainEvents() {
        lxa_intui_event_t buf[LXA_EVENT_LOG_SIZE];
        int n = lxa_drain_intui_events(buf, LXA_EVENT_LOG_SIZE);
        return std::vector<lxa_intui_event_t>(buf, buf + n);
    }

    /* Phase 131: return number of pending events without draining */
    int PendingEventCount() {
        return lxa_intui_event_count();
    }

    /* Phase 131: obtain a snapshot of the MenuStrip attached to a window */
    lxa_menu_strip_t *GetMenuStrip(int window_index = 0) {
        return lxa_get_menu_strip(window_index);
    }

    /* Phase 131: convenience — return top-level menu names for a window */
    std::vector<std::string> GetMenuNames(int window_index = 0) {
        std::vector<std::string> names;
        lxa_menu_strip_t *strip = lxa_get_menu_strip(window_index);
        if (!strip) return names;
        int count = lxa_get_menu_count(strip);
        for (int i = 0; i < count; i++) {
            lxa_menu_info_t info;
            if (lxa_get_menu_info(strip, i, -1, -1, &info)) {
                names.push_back(std::string(info.name));
            }
        }
        lxa_free_menu_strip(strip);
        return names;
    }
};

/**
 * Test fixture for window-based tests.
 * Automatically waits for a window to open during setup.
 */
class LxaWindowTest : public LxaTest {
protected:
    lxa_window_info_t window_info;
    
    void SetUp() override {
        LxaTest::SetUp();
        // Subclasses should load their program and wait for windows
    }

    /**
     * Auto-capture the current screen state after every test.
     * The file is named <TestSuite>-<TestName>.png and written to
     * LXA_SCREENSHOTS_DIR.  It always overwrites the previous run so
     * the folder stays current.
     */
    void TearDown() override {
        if (initialized) {
            const ::testing::TestInfo* ti =
                ::testing::UnitTest::GetInstance()->current_test_info();
            if (ti) {
                std::string suite = ti->test_suite_name();
                std::string test  = ti->name();
                // Replace characters that are awkward in file names
                for (char& c : suite) if (c == '/' || c == ' ') c = '_';
                for (char& c : test)  if (c == '/' || c == ' ') c = '_';
                SaveScreenshot(suite + "-" + test);
            }
        }
        LxaTest::TearDown();
    }
};

/**
 * Test fixture for interactive UI tests.
 * Provides additional helpers for UI interaction.
 */
class LxaUITest : public LxaWindowTest {
protected:
    /**
     * Wait for task to reach WaitPort() before injecting events.
     */
    void WaitForEventLoop(int cycles = 200, int cycles_per_iteration = 10000) {
        for (int i = 0; i < cycles; i++) {
            RunCycles(cycles_per_iteration);
        }
    }
    
    /**
     * Read pixel color at given coordinates.
     */
    int ReadPixel(int x, int y) {
        int pen;
        if (lxa_read_pixel(x, y, &pen)) {
            return pen;
        }
        return -1;
    }
    
    /**
     * Read RGB pixel values at given coordinates.
     */
    bool ReadPixelRGB(int x, int y, uint8_t* r, uint8_t* g, uint8_t* b) {
        return lxa_read_pixel_rgb(x, y, r, g, b);
    }
    
    /**
     * Count non-background pixels in a region.
     */
    int CountContentPixels(int x1, int y1, int x2, int y2, int bg_color = 0) {
        lxa_flush_display();  /* sync planar→chunky before reading */
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

} // namespace testing
} // namespace lxa

#endif // LXA_TEST_H
