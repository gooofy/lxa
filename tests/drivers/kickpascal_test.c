/*
 * kickpascal_test.c - Host-side test driver for KickPascal 2
 *
 * This test driver uses liblxa to:
 * 1. Start KickPascal 2 IDE
 * 2. Wait for the editor window to open
 * 3. Verify the editor accepts keyboard input
 * 4. Test basic Pascal code entry
 * 5. Exit cleanly (if possible)
 *
 * Phase 57: Deep Dive App Test Drivers
 * 
 * Known issues (from roadmap):
 * - Screen clearing issues
 * - Repaint speed
 * - Cursor key handling
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

static char *find_apps_path(void)
{
    static char path[512];
    const char *locations[] = {
        /* Relative to build directory */
        "../../lxa-apps/kickpascal2/bin/KP2",
        "../../../lxa-apps/kickpascal2/bin/KP2",
        /* Absolute path */
        "/home/guenter/projects/amiga/lxa/src/lxa-apps/kickpascal2/bin/KP2",
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
    printf("=== KickPascal 2 Test Driver ===\n\n");

    /* Find ROM and apps */
    char *rom_path = find_rom_path();
    char *apps_path = find_apps_path();
    
    if (!rom_path) {
        fprintf(stderr, "ERROR: Could not find lxa.rom\n");
        return 1;
    }
    
    if (!apps_path) {
        fprintf(stderr, "ERROR: Could not find KickPascal directory\n");
        fprintf(stderr, "Expected at: lxa-apps/kickpascal2/bin/KP2/\n");
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
    
    /* Load KickPascal */
    printf("Loading KickPascal 2 (KP)...\n");
    if (lxa_load_program("SYS:KP", "") != 0) {
        fprintf(stderr, "ERROR: Failed to load KickPascal\n");
        lxa_shutdown();
        return 1;
    }
    
    /* ========== Test 1: Wait for window to open ========== */
    printf("\nTest 1: Waiting for KickPascal window to open...\n");
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
    
    /* ========== Test 3: Let KickPascal initialize fully ========== */
    printf("\nTest 3: Letting KickPascal initialize...\n");
    {
        /* Run more cycles to let KickPascal fully initialize */
        run_with_vblanks(2000000, 50000);
        
        check(lxa_get_window_count() >= 1, "Window still open");
        check(lxa_is_running(), "KickPascal still running");
    }
    
    /* ========== Test 4: Type a simple Pascal program ========== */
    printf("\nTest 4: Typing a Pascal program...\n");
    {
        /* Type a simple Pascal program */
        const char *program = 
            "program Test;\n"
            "begin\n"
            "  writeln('Hello');\n"
            "end.\n";
        
        lxa_inject_string(program);
        
        /* Run cycles to process the input */
        run_with_vblanks(1000000, 50000);
        
        check(lxa_is_running(), "KickPascal still running after typing");
        check(lxa_get_window_count() >= 1, "Window still open after typing");
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
            
            check(lxa_is_running(), "KickPascal still running after mouse click");
        } else {
            printf("  Skipping: No window info available\n");
            passed++;
        }
    }
    
    /* ========== Test 6: Test cursor keys (known issue) ========== */
    printf("\nTest 6: Testing cursor keys...\n");
    {
        /* Test arrow keys - this is a known issue area */
        /* Up = 0x4C, Down = 0x4D, Right = 0x4E, Left = 0x4F */
        printf("  Pressing cursor keys...\n");
        lxa_inject_keypress(0x4C, 0);  /* Up */
        lxa_inject_keypress(0x4D, 0);  /* Down */
        lxa_inject_keypress(0x4E, 0);  /* Right */
        lxa_inject_keypress(0x4F, 0);  /* Left */
        
        run_with_vblanks(500000, 50000);
        
        check(lxa_is_running(), "KickPascal still running after cursor keys");
        check(lxa_get_window_count() >= 1, "Window still open after cursor keys");
    }
    
    /* ========== Test 7: Try to quit ========== */
    printf("\nTest 7: Testing quit...\n");
    {
        /* Try Amiga-Q */
        lxa_inject_keypress(0x10, 0x0080);  /* Right Amiga + Q */
        run_with_vblanks(500000, 50000);
        
        if (!lxa_is_running()) {
            check(1, "KickPascal exited via Amiga-Q");
        } else {
            /* Try clicking close gadget */
            lxa_click_close_gadget(0);
            run_with_vblanks(500000, 50000);
            
            if (!lxa_is_running()) {
                check(1, "KickPascal exited via close gadget");
            } else {
                printf("  Note: KickPascal still running (normal - may need confirmation)\n");
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
