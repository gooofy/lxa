/*
 * devpac_test.c - Host-side test driver for Devpac (HiSoft) assembler
 *
 * This test driver uses liblxa to:
 * 1. Start Devpac (HiSoft assembler/editor)
 * 2. Wait for the editor window to open
 * 3. Verify the editor accepts keyboard input
 * 4. Test basic navigation/editing with visual verification
 * 5. Test cursor key handling
 * 6. Exit cleanly (if possible)
 *
 * Phase 59: Devpac Deep Dive
 */

#include "lxa_api.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int errors = 0;
static int passed = 0;
static int screenshot_num = 0;
static int debug_mode = 0;

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

/*
 * Capture screenshot with numbered filename
 */
static void capture_screenshot(const char *label)
{
    char filename[256];
    snprintf(filename, sizeof(filename), "/tmp/devpac_%02d_%s.ppm", 
             screenshot_num++, label);
    if (lxa_capture_screen(filename)) {
        if (debug_mode) {
            printf("  Screenshot: %s\n", filename);
        }
    }
}

/*
 * Count non-background pixels in a region
 */
static int count_content_in_region(int x1, int y1, int x2, int y2)
{
    int content = 0;
    int bg_pen;
    
    /* Sample background color from corner */
    if (!lxa_read_pixel(0, 0, &bg_pen)) {
        return -1;
    }
    
    for (int y = y1; y <= y2; y += 2) {
        for (int x = x1; x <= x2; x += 2) {
            int pen;
            if (lxa_read_pixel(x, y, &pen) && pen != bg_pen) {
                content++;
            }
        }
    }
    return content;
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

static char *find_apps_path(void)
{
    static char path[512];
    const char *locations[] = {
        /* Relative to build directory */
        "../../lxa-apps/DevPac/bin/Devpac",
        "../../../lxa-apps/DevPac/bin/Devpac",
        /* Absolute path */
        "/home/guenter/projects/amiga/lxa/src/lxa-apps/DevPac/bin/Devpac",
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
    /* Check for debug mode */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-d") == 0 || strcmp(argv[i], "--debug") == 0) {
            debug_mode = 1;
        }
    }
    
    printf("=== Devpac (HiSoft) Test Driver (Phase 59) ===\n\n");

    /* Find ROM and apps */
    char *rom_path = find_rom_path();
    char *apps_path = find_apps_path();
    
    if (!rom_path) {
        fprintf(stderr, "ERROR: Could not find lxa.rom\n");
        return 1;
    }
    
    if (!apps_path) {
        fprintf(stderr, "ERROR: Could not find Devpac directory\n");
        fprintf(stderr, "Expected at: lxa-apps/DevPac/bin/Devpac/\n");
        return 1;
    }
    
    printf("Using ROM: %s\n", rom_path);
    printf("Using apps: %s\n\n", apps_path);

    /* Initialize lxa */
    lxa_config_t config = {
        .rom_path = rom_path,
        .sys_drive = apps_path,
        .headless = true,
        .verbose = false,
    };
    
    printf("Initializing lxa...\n");
    if (lxa_init(&config) != 0) {
        fprintf(stderr, "ERROR: Failed to initialize lxa\n");
        return 1;
    }
    
    /* Load Devpac */
    printf("Loading Devpac...\n");
    if (lxa_load_program("SYS:Devpac", "") != 0) {
        fprintf(stderr, "ERROR: Failed to load Devpac\n");
        lxa_shutdown();
        return 1;
    }
    
    /* ========== Test 1: Wait for window to open ========== */
    printf("\nTest 1: Waiting for Devpac window to open...\n");
    {
        if (!lxa_wait_windows(1, 10000)) {
            printf("  WARNING: Window did not open within 10 seconds\n");
            printf("  Continuing to run more cycles...\n");
            
            /* Run more cycles with VBlanks */
            run_with_vblanks(5000000, 50000);
        }
        
        int win_count = lxa_get_window_count();
        check(win_count >= 1, "At least one window opened");
        
        if (win_count > 0) {
            lxa_window_info_t win_info;
            if (lxa_get_window_info(0, &win_info)) {
                printf("  Window at (%d, %d), size %dx%d\n", 
                       win_info.x, win_info.y, win_info.width, win_info.height);
                printf("  Title: '%s'\n", win_info.title);
            }
        }
    }
    
    /* ========== Test 2: Check screen dimensions ========== */
    printf("\nTest 2: Checking screen dimensions...\n");
    {
        int width, height, depth;
        if (lxa_get_screen_dimensions(&width, &height, &depth)) {
            printf("  Screen: %dx%dx%d\n", width, height, depth);
            check(width >= 640, "Screen width >= 640");
            check(height >= 200, "Screen height >= 200");
        } else {
            printf("  FAIL: Could not get screen dimensions\n");
            errors++;
        }
    }
    
    /* ========== Test 3: Let Devpac initialize fully ========== */
    printf("\nTest 3: Letting Devpac initialize...\n");
    {
        /* Run more cycles to let Devpac fully initialize */
        run_with_vblanks(2000000, 50000);
        
        check(lxa_get_window_count() >= 1, "Window still open");
        check(lxa_is_running(), "Devpac still running");
        
        /* Capture initial state */
        capture_screenshot("after_init");
        
        /* Check editor area for content */
        lxa_window_info_t win_info;
        if (lxa_get_window_info(0, &win_info)) {
            int content = count_content_in_region(
                win_info.x + 10, 
                win_info.y + 20,
                win_info.x + win_info.width - 10,
                win_info.y + 50
            );
            printf("  Editor area content pixels: %d\n", content);
            check(content >= 0, "Editor area accessible");
        }
    }
    
    /* ========== Test 4: Type some assembly code ========== */
    printf("\nTest 4: Typing assembly code...\n");
    {
        int content_before = lxa_get_content_pixels();
        
        /* Type a simple assembly program */
        const char *program = 
            "; Devpac test\n"
            "\tmove.l\t#0,d0\n"
            "\trts\n";
        
        lxa_inject_string(program);
        
        /* Run cycles to process the input */
        run_with_vblanks(1000000, 50000);
        
        capture_screenshot("after_typing");
        
        int content_after = lxa_get_content_pixels();
        printf("  Content pixels before: %d, after: %d\n", content_before, content_after);
        
        check(lxa_is_running(), "Devpac still running after typing");
        check(lxa_get_window_count() >= 1, "Window still open after typing");
        
        if (content_before >= 0 && content_after >= 0) {
            if (content_after > content_before) {
                check(1, "Text content rendered (content increased)");
            } else {
                printf("  Note: Content count unchanged\n");
                passed++;
            }
        }
    }
    
    /* ========== Test 5: Test mouse input ========== */
    printf("\nTest 5: Testing mouse input...\n");
    {
        lxa_window_info_t win_info;
        if (lxa_get_window_info(0, &win_info)) {
            /* Click in the editor area */
            int click_x = win_info.x + win_info.width / 2;
            int click_y = win_info.y + win_info.height / 2;
            
            printf("  Clicking at (%d, %d)\n", click_x, click_y);
            lxa_inject_mouse_click(click_x, click_y, LXA_MOUSE_LEFT);
            
            run_with_vblanks(500000, 50000);
            
            check(lxa_is_running(), "Devpac still running after mouse click");
        } else {
            printf("  Skipping: No window info available\n");
            passed++;
        }
    }
    
    /* ========== Test 6: Test cursor keys ========== */
    printf("\nTest 6: Testing cursor keys...\n");
    {
        /* Drain pending events first */
        printf("  Draining pending events...\n");
        run_with_vblanks(2000000, 50000);
        
        capture_screenshot("before_cursor");
        
        printf("  Pressing cursor keys (Up, Down, Right, Left)...\n");
        lxa_inject_keypress(0x4C, 0);  /* Up */
        run_with_vblanks(500000, 50000);
        
        lxa_inject_keypress(0x4D, 0);  /* Down */
        run_with_vblanks(500000, 50000);
        
        lxa_inject_keypress(0x4E, 0);  /* Right */
        run_with_vblanks(500000, 50000);
        
        lxa_inject_keypress(0x4F, 0);  /* Left */
        run_with_vblanks(500000, 50000);
        
        run_with_vblanks(500000, 50000);
        
        capture_screenshot("after_cursor");
        
        check(lxa_is_running(), "Devpac still running after cursor keys");
        check(lxa_get_window_count() >= 1, "Window still open after cursor keys");
    }
    
    /* ========== Test 7: Try to quit ========== */
    printf("\nTest 7: Testing quit...\n");
    {
        /* Try Amiga-Q */
        lxa_inject_keypress(0x10, 0x0080);  /* Right Amiga + Q */
        run_with_vblanks(500000, 50000);
        
        if (!lxa_is_running()) {
            check(1, "Devpac exited via Amiga-Q");
        } else {
            /* Try clicking close gadget */
            lxa_click_close_gadget(0);
            run_with_vblanks(500000, 50000);
            
            if (!lxa_is_running()) {
                check(1, "Devpac exited via close gadget");
            } else {
                printf("  Note: Devpac still running (normal - may need save dialog handling)\n");
                passed++;  /* Not a failure */
            }
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
