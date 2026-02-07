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
#include <string>
#include <cstring>
#include <unistd.h>
#include <cstdio>

namespace lxa {
namespace testing {

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
 * Helper function to find lxa-apps directory.
 */
inline const char* FindAppsPath() {
    static std::string apps_path;
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
            apps_path = locations[i];
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
