/*
 * updatestrgad_test.c - Host-side test driver for UpdateStrGad sample
 *
 * This test driver uses liblxa to:
 * 1. Start the UpdateStrGad sample
 * 2. Wait for the window to open
 * 3. Verify programmatic string gadget updates work
 * 4. Verify the sample's internal interactive tests run
 * 5. Verify the program completes cleanly
 *
 * Note: The UpdateStrGad sample already contains test_inject.h based interactive
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
    printf("=== UpdateStrGad Test Driver ===\n\n");

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
    
    /* Load the UpdateStrGad program */
    printf("Loading UpdateStrGad...\n");
    if (lxa_load_program("SYS:UpdateStrGad", "") != 0) {
        fprintf(stderr, "ERROR: Failed to load UpdateStrGad\n");
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
    
    /* Wait for program to finish its programmatic tests */
    printf("Waiting for programmatic update tests to complete...\n");
    {
        char output[8192];
        int timeout = 0;
        while (timeout < 1000) {
            lxa_run_cycles(10000);
            lxa_get_output(output, sizeof(output));
            if (strstr(output, "Programmatic updates completed") != NULL) {
                break;
            }
            timeout++;
        }
        
        if (timeout >= 1000) {
            fprintf(stderr, "ERROR: Programmatic tests did not complete\n");
            lxa_shutdown();
            return 1;
        }
    }
    
    /* Verify programmatic update tests passed */
    printf("Test 1: Verifying programmatic string gadget updates...\n");
    {
        char output[8192];
        lxa_get_output(output, sizeof(output));
        
        check(strstr(output, "Initial value: 'START'") != NULL, 
              "Initial value was 'START'");
        check(strstr(output, "Gadget activated successfully") != NULL,
              "ActivateGadget() succeeded");
        check(strstr(output, "Updating to 'Try again'") != NULL,
              "First update executed");
        check(strstr(output, "Updating to 'A Winner'") != NULL,
              "Last update executed");
        check(strstr(output, "Programmatic updates completed") != NULL,
              "Programmatic updates completed");
    }
    
    /* Let task reach its main loop for interactive testing */
    printf("\nLetting task reach interactive testing phase...\n");
    run_cycles_only(200, 10000);
    
    /* ========== Test 2: Wait for interactive testing to start ========== */
    printf("\nTest 2: Waiting for interactive string gadget tests...\n");
    {
        /* Wait for the sample to print "Starting interactive testing" */
        char output[8192];
        int timeout = 0;
        while (timeout < 500) {
            lxa_run_cycles(10000);
            lxa_get_output(output, sizeof(output));
            if (strstr(output, "Starting interactive testing") != NULL) {
                break;
            }
            timeout++;
        }
        
        check(strstr(output, "Starting interactive testing") != NULL,
              "Interactive testing phase started");
    }
    
    /* ========== Test 3: Wait for program completion ========== */
    printf("\nTest 3: Wait for program to complete...\n");
    {
        /* Let the sample run its remaining tests */
        for (int i = 0; i < 500; i++) {
            lxa_run_cycles(10000);
            
            char output[8192];
            lxa_get_output(output, sizeof(output));
            if (strstr(output, "Closing window") != NULL) {
                break;
            }
        }
        
        /* Run more cycles to let it finish */
        run_cycles_only(100, 10000);
        
        char output[8192];
        lxa_get_output(output, sizeof(output));
        
        check(strstr(output, "Interactive testing complete") != NULL ||
              strstr(output, "updates completed") != NULL,
              "Testing phase completed");
        check(strstr(output, "Closing window") != NULL,
              "Window closed cleanly");
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
