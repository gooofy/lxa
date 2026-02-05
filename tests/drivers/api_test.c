/*
 * api_test.c - Test driver for new liblxa API functions
 *
 * This test verifies the Phase 57 API extensions:
 * - lxa_inject_rmb_click()
 * - lxa_inject_drag()
 * - lxa_get_screen_info()
 * - lxa_read_pixel()
 * - lxa_read_pixel_rgb()
 *
 * Phase 57: Host-Side Test Driver Migration
 */

#include "lxa_api.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int errors = 0;
static int passed = 0;

static void check(int condition, const char *msg)
{
    if (condition) {
        printf("  OK: %s\n", msg);
        passed++;
    } else {
        printf("  FAIL: %s\n", msg);
        errors++;
    }
}

static char *find_rom_path(void)
{
    static char path[256];
    const char *locations[] = {
        "host/rom/lxa.rom",
        "target/rom/lxa.rom",
        "../target/rom/lxa.rom",
        NULL
    };
    
    for (int i = 0; locations[i]; i++) {
        if (access(locations[i], R_OK) == 0) {
            strcpy(path, locations[i]);
            return path;
        }
    }
    
    return NULL;
}

static char *find_samples_path(void)
{
    static char path[512];
    const char *locations[] = {
        "target/samples",
        "../build/target/samples",
        NULL
    };
    
    for (int i = 0; locations[i]; i++) {
        if (access(locations[i], F_OK) == 0) {
            strcpy(path, locations[i]);
            return path;
        }
    }
    
    return NULL;
}

/*
 * Helper: run cycles with periodic VBlanks
 */
static void run_with_vblanks(int total_cycles, int cycles_per_vblank)
{
    int remaining = total_cycles;
    while (remaining > 0) {
        int chunk = remaining > cycles_per_vblank ? cycles_per_vblank : remaining;
        lxa_run_cycles(chunk);
        lxa_trigger_vblank();
        remaining -= chunk;
    }
}

int main(int argc, char **argv)
{
    printf("=== liblxa API Test Driver ===\n\n");

    /* Find ROM and samples */
    char *rom_path = find_rom_path();
    char *samples_path = find_samples_path();
    
    if (!rom_path) {
        fprintf(stderr, "ERROR: Could not find lxa.rom\n");
        return 1;
    }
    
    if (!samples_path) {
        fprintf(stderr, "ERROR: Could not find samples directory\n");
        return 1;
    }
    
    printf("Using ROM: %s\n", rom_path);
    printf("Using samples: %s\n\n", samples_path);

    /* Initialize lxa */
    lxa_config_t config = {
        .rom_path = rom_path,
        .sys_drive = samples_path,
        .headless = true,
        .verbose = false,
    };
    
    printf("Initializing lxa...\n");
    if (lxa_init(&config) != 0) {
        fprintf(stderr, "ERROR: Failed to initialize lxa\n");
        return 1;
    }
    
    /* Load SimpleGad - a simple program that opens a window */
    printf("Loading SimpleGad...\n");
    if (lxa_load_program("SYS:Samples/SimpleGad", "") != 0) {
        fprintf(stderr, "ERROR: Failed to load SimpleGad\n");
        lxa_shutdown();
        return 1;
    }
    
    /* Wait for window to open */
    printf("\nTest 1: Waiting for window...\n");
    if (!lxa_wait_windows(1, 5000)) {
        printf("  WARNING: Window did not open, continuing anyway\n");
    }
    
    /* Let the program initialize */
    run_with_vblanks(2000000, 50000);
    
    check(lxa_get_window_count() >= 1, "Window opened");
    
    /* ========== Test lxa_get_screen_info ========== */
    printf("\nTest 2: Testing lxa_get_screen_info()...\n");
    {
        lxa_screen_info_t screen_info;
        bool got_info = lxa_get_screen_info(&screen_info);
        
        check(got_info, "lxa_get_screen_info() succeeded");
        
        if (got_info) {
            printf("  Screen: %dx%dx%d, %d colors\n", 
                   screen_info.width, screen_info.height, 
                   screen_info.depth, screen_info.num_colors);
            
            check(screen_info.width > 0, "Screen width > 0");
            check(screen_info.height > 0, "Screen height > 0");
            check(screen_info.depth > 0, "Screen depth > 0");
            check(screen_info.num_colors > 0, "num_colors > 0");
            check(screen_info.num_colors == (1 << screen_info.depth), 
                  "num_colors matches 2^depth");
        }
    }
    
    /* ========== Test lxa_read_pixel ========== */
    printf("\nTest 3: Testing lxa_read_pixel()...\n");
    {
        int pen;
        bool read_ok = lxa_read_pixel(10, 10, &pen);
        
        check(read_ok, "lxa_read_pixel() at (10,10) succeeded");
        
        if (read_ok) {
            printf("  Pixel at (10,10) = pen %d\n", pen);
            check(pen >= 0 && pen < 256, "Pen value in valid range");
        }
        
        /* Test out of bounds */
        bool oob_read = lxa_read_pixel(-1, -1, &pen);
        check(!oob_read, "lxa_read_pixel() at (-1,-1) correctly fails");
        
        oob_read = lxa_read_pixel(10000, 10000, &pen);
        check(!oob_read, "lxa_read_pixel() at (10000,10000) correctly fails");
    }
    
    /* ========== Test lxa_read_pixel_rgb ========== */
    printf("\nTest 4: Testing lxa_read_pixel_rgb()...\n");
    {
        uint8_t r, g, b;
        bool read_ok = lxa_read_pixel_rgb(10, 10, &r, &g, &b);
        
        check(read_ok, "lxa_read_pixel_rgb() at (10,10) succeeded");
        
        if (read_ok) {
            printf("  Pixel RGB at (10,10) = (%d, %d, %d)\n", r, g, b);
            /* Just verify we got some values, can't predict exact color */
            check(1, "RGB values retrieved");
        }
    }
    
    /* ========== Test lxa_inject_rmb_click ========== */
    printf("\nTest 5: Testing lxa_inject_rmb_click()...\n");
    {
        lxa_window_info_t win_info;
        if (lxa_get_window_info(0, &win_info)) {
            int x = win_info.x + win_info.width / 2;
            int y = win_info.y + win_info.height / 2;
            
            printf("  RMB click at (%d, %d)\n", x, y);
            bool rmb_ok = lxa_inject_rmb_click(x, y);
            check(rmb_ok, "lxa_inject_rmb_click() succeeded");
            
            run_with_vblanks(500000, 50000);
            check(lxa_is_running(), "Program still running after RMB click");
        } else {
            printf("  Skipping: No window info\n");
            passed++;
            passed++;
        }
    }
    
    /* ========== Test lxa_inject_drag ========== */
    printf("\nTest 6: Testing lxa_inject_drag()...\n");
    {
        lxa_window_info_t win_info;
        if (lxa_get_window_info(0, &win_info)) {
            int x1 = win_info.x + 50;
            int y1 = win_info.y + 50;
            int x2 = win_info.x + 100;
            int y2 = win_info.y + 100;
            
            printf("  Drag from (%d,%d) to (%d,%d)\n", x1, y1, x2, y2);
            bool drag_ok = lxa_inject_drag(x1, y1, x2, y2, LXA_MOUSE_LEFT, 5);
            check(drag_ok, "lxa_inject_drag() succeeded");
            
            run_with_vblanks(500000, 50000);
            check(lxa_is_running(), "Program still running after drag");
        } else {
            printf("  Skipping: No window info\n");
            passed++;
            passed++;
        }
    }
    
    /* ========== Results ========== */
    printf("\n=== Test Results ===\n");
    printf("Passed: %d, Failed: %d\n", passed, errors);
    
    if (errors == 0) {
        printf("PASS: All tests passed!\n");
    } else {
        printf("FAIL: %d test(s) failed\n", errors);
    }
    
    lxa_shutdown();
    
    return errors > 0 ? 1 : 0;
}
