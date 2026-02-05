/*
 * simplemenu_test.c - Host-side test driver for SimpleMenu sample
 *
 * This test driver uses liblxa to:
 * 1. Start the SimpleMenu sample
 * 2. Wait for the window to open
 * 3. Let the sample run its own interactive tests
 * 4. Verify the program completes cleanly
 *
 * Note: The SimpleMenu sample already contains test_inject.h based interactive
 * testing. This driver verifies the sample runs and completes successfully.
 *
 * Phase 57: Host-side driver migration from test_inject.h
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

/*
 * Helper: run cycles without VBlanks for output processing
 */
static void run_cycles_only(int iterations, int cycles_per_iteration)
{
    for (int i = 0; i < iterations; i++) {
        lxa_run_cycles(cycles_per_iteration);
    }
}

int main(int argc, char **argv)
{
    printf("=== SimpleMenu Test Driver ===\n\n");

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
    
    /* Load the SimpleMenu program */
    printf("Loading SimpleMenu...\n");
    if (lxa_load_program("SYS:SimpleMenu", "") != 0) {
        fprintf(stderr, "ERROR: Failed to load SimpleMenu\n");
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
    
    /* Let the sample run its tests - it uses test_inject.h internally.
     * The sample uses WaitTOF() which requires VBlank signals. */
    printf("Letting sample run its internal tests...\n");
    {
        char output[16384];
        int timeout = 0;
        while (timeout < 5000) {  /* Increased timeout for interactive tests */
            /* Trigger VBlank periodically to let WaitTOF() complete */
            if (timeout % 50 == 0) {
                lxa_trigger_vblank();
            }
            lxa_run_cycles(10000);
            lxa_get_output(output, sizeof(output));
            if (strstr(output, "Demo complete") != NULL) {
                break;
            }
            timeout++;
        }
        
        /* Run a few more cycles for final output */
        run_cycles_only(100, 10000);
    }
    
    /* Verify all expected output */
    {
        char output[16384];
        lxa_get_output(output, sizeof(output));
        
        printf("Test 1: Verifying menu initialization...\n");
        check(strstr(output, "Menu strip attached successfully") != NULL, 
              "Menu strip attached");
        check(strstr(output, "Window->MenuStrip: Correctly set") != NULL, 
              "MenuStrip pointer set correctly");
        
        printf("\nTest 2: Verifying ItemAddress functionality...\n");
        check(strstr(output, "Menu 0, Item 0: Open...") != NULL,
              "ItemAddress finds Open...");
        check(strstr(output, "Menu 0, Item 3: Quit") != NULL,
              "ItemAddress finds Quit");
        check(strstr(output, "Menu 0, Item 2, Sub 0: Draft") != NULL,
              "ItemAddress finds Draft submenu");
        check(strstr(output, "Menu 0, Item 2, Sub 1: NLQ") != NULL,
              "ItemAddress finds NLQ submenu");
        
        printf("\nTest 3: Verifying interactive testing ran...\n");
        check(strstr(output, "Starting interactive menu testing") != NULL,
              "Interactive testing started");
        check(strstr(output, "Test 1 - Selecting 'Open...'") != NULL,
              "Test 1 ran");
        check(strstr(output, "Test 2 - Selecting 'Quit'") != NULL,
              "Test 2 ran");
        check(strstr(output, "Interactive testing complete") != NULL,
              "Interactive testing completed");
        
        printf("\nTest 4: Verifying cleanup...\n");
        check(strstr(output, "Menu strip cleared") != NULL,
              "Menu strip cleared");
        check(strstr(output, "Window->MenuStrip: Correctly cleared") != NULL,
              "MenuStrip pointer cleared correctly");
        check(strstr(output, "Demo complete") != NULL,
              "Demo completed");
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
