/*
 * custompointer_test.c - Host-side test driver for CustomPointer sample
 *
 * This test driver uses liblxa to:
 * 1. Start the CustomPointer sample (RKM original)
 * 2. Wait for the window to open
 * 3. Wait for the program to complete its Delay() sequences
 * 4. Verify the program exits cleanly
 *
 * The RKM CustomPointer sample demonstrates SetPointer/ClearPointer
 * with a busy wait pointer. It uses Delay() calls to pause:
 *   - Delay(50)  after opening window
 *   - Delay(100) while busy pointer is active
 *   - Delay(100) after clearing pointer
 * Then it closes and exits.
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
    printf("=== CustomPointer Test Driver ===\n\n");

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
    
    /* Load the CustomPointer program */
    printf("Loading CustomPointer...\n");
    if (lxa_load_program("SYS:CustomPointer", "") != 0) {
        fprintf(stderr, "ERROR: Failed to load CustomPointer\n");
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
    
    check(win_info.width > 0, "Window has positive width");
    check(win_info.height > 0, "Window has positive height");
    
    /* ========== Test 1: Wait for program to complete ========== */
    printf("\nTest 1: Waiting for program to complete Delay() sequences...\n");
    {
        /* The program does:
         *   Delay(50)   = 1 second
         *   Delay(100)  = 2 seconds
         *   Delay(100)  = 2 seconds
         * Total: 5 seconds plus some startup time
         * We'll give it 10 seconds to be safe
         */
        printf("  (CustomPointer uses Delay() calls totaling ~5 seconds)\n");
        
        if (!lxa_wait_exit(15000)) {
            printf("  WARNING: Program did not exit within 15 seconds\n");
            printf("  This may indicate a timer or Delay() implementation issue\n");
            errors++;
        } else {
            check(1, "Program completed and exited normally");
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
