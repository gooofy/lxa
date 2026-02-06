/*
 * gadtoolsmenu_test.c - Host-side test driver for GadToolsMenu sample
 *
 * This test driver uses liblxa to:
 * 1. Start the GadToolsMenu sample (RKM original)
 * 2. Wait for the window to open
 * 3. Click the close gadget to exit
 * 4. Verify the program exits cleanly
 *
 * The RKM GadToolsMenu sample demonstrates creating menus using GadTools.
 * It has "Project" and "Edit" menus with standard items.
 * Quit is Project menu item 5 (after Open, Save, barlabel, Print, barlabel, Quit).
 */

#include "lxa_api.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int errors = 0;

static void check(int condition, const char *msg)
{
    if (condition) {
        printf("  OK: %s\n", msg);
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
    static char path[256];
    const char *locations[] = {
        "target/samples/Samples",
        "../target/samples/Samples",
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

int main(int argc, char **argv)
{
    printf("=== GadToolsMenu Test Driver ===\n\n");

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
    
    /* Load the GadToolsMenu program */
    printf("Loading GadToolsMenu...\n");
    if (lxa_load_program("SYS:GadToolsMenu", "") != 0) {
        fprintf(stderr, "ERROR: Failed to load GadToolsMenu\n");
        lxa_shutdown();
        return 1;
    }
    
    /* Run until window opens */
    printf("Waiting for window to open...\n");
    if (!lxa_wait_windows(1, 5000)) {
        fprintf(stderr, "ERROR: Window did not open within 5 seconds\n");
        lxa_shutdown();
        return 1;
    }
    
    printf("Window opened!\n\n");
    
    /* Get window info */
    lxa_window_info_t win_info;
    if (!lxa_get_window_info(0, &win_info)) {
        fprintf(stderr, "ERROR: Could not get window info\n");
        lxa_shutdown();
        return 1;
    }
    
    printf("Window at (%d, %d), size %dx%d\n\n", 
           win_info.x, win_info.y, win_info.width, win_info.height);
    
    check(win_info.width == 400, "Window width is 400");
    check(win_info.height == 100, "Window height is 100");
    
    /* CRITICAL: Run cycles to let task reach Wait() */
    printf("Letting task reach Wait()...\n");
    for (int i = 0; i < 200; i++) {
        lxa_run_cycles(10000);
    }
    lxa_clear_output();
    
    /* ========== Test: Click close gadget to exit ========== */
    printf("\nTest: Clicking close gadget...\n");
    {
        /* Close gadget is typically at top-left of window */
        int close_x = win_info.x + 10;
        int close_y = win_info.y + 5;
        
        printf("  Clicking close gadget at (%d, %d)\n", close_x, close_y);
        
        lxa_inject_mouse_click(close_x, close_y, LXA_MOUSE_LEFT);
        
        /* Run cycles */
        for (int i = 0; i < 20; i++) {
            lxa_trigger_vblank();
            lxa_run_cycles(50000);
        }
        
        /* Wait for exit */
        printf("  Waiting for program to exit...\n");
        if (!lxa_wait_exit(5000)) {
            printf("  WARNING: Program did not exit within 5 seconds\n");
            errors++;
        } else {
            check(1, "Program exited cleanly via close gadget");
        }
    }
    
    /* ========== Results ========== */
    printf("\n=== Test Results ===\n");
    if (errors == 0) {
        printf("PASS: All tests passed!\n");
    } else {
        printf("FAIL: %d test(s) failed\n", errors);
    }
    
    lxa_shutdown();
    
    return errors > 0 ? 1 : 0;
}
