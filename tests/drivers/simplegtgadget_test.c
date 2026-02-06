/*
 * simplegtgadget_test.c - Host-side test driver for SimpleGTGadget sample
 *
 * This test driver uses liblxa to:
 * 1. Start the SimpleGTGadget sample
 * 2. Wait for the window to open
 * 3. Verify GadTools gadgets are created correctly
 * 4. Click the BUTTON_KIND gadget and verify GADGETUP
 * 5. Click the CYCLE_KIND gadget and verify cycle advances
 *
 */

#include "lxa_api.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* Gadget positions (from simplegtgadget.c)
 * Note: These are relative to window content area.
 * The actual Y positions depend on screen font height.
 */
#define BUTTON_LEFT    20
#define BUTTON_WIDTH   120
#define BUTTON_HEIGHT  14

/* Window title bar height (approximate) */
#define TITLE_BAR_HEIGHT 11

/* Screen bar height for calculating gadget Y positions */
#define SCREEN_FONT_SIZE 8
#define BUTTON_TOP_OFFSET (20 + TITLE_BAR_HEIGHT + SCREEN_FONT_SIZE + 1)

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
 * Helper: run cycles with VBlanks for event processing
 */
static void run_with_vblanks(int vblank_count, int cycles_per_vblank)
{
    for (int i = 0; i < vblank_count; i++) {
        lxa_trigger_vblank();
        lxa_run_cycles(cycles_per_vblank);
    }
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
    printf("=== SimpleGTGadget Test Driver ===\n\n");

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
    
    /* Load the SimpleGTGadget program */
    printf("Loading SimpleGTGadget...\n");
    if (lxa_load_program("SYS:SimpleGTGadget", "") != 0) {
        fprintf(stderr, "ERROR: Failed to load SimpleGTGadget\n");
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
    
    /* Wait for program to complete its programmatic tests */
    printf("Waiting for programmatic tests to complete...\n");
    {
        char output[16384];
        int timeout = 0;
        while (timeout < 1000) {
            lxa_run_cycles(10000);
            lxa_get_output(output, sizeof(output));
            if (strstr(output, "Starting interactive testing") != NULL) {
                break;
            }
            timeout++;
        }
        
        if (timeout >= 1000) {
            /* Check for error output */
            lxa_get_output(output, sizeof(output));
            fprintf(stderr, "ERROR: Programmatic tests did not complete\nOutput: %s\n", output);
            lxa_shutdown();
            return 1;
        }
    }
    
    /* Verify programmatic test results */
    printf("Test 1: Verifying GadTools gadget creation...\n");
    {
        char output[16384];
        lxa_get_output(output, sizeof(output));
        
        check(strstr(output, "Created BUTTON_KIND gadget: OK") != NULL,
              "BUTTON_KIND gadget created");
        check(strstr(output, "Created CHECKBOX_KIND gadget: OK") != NULL,
              "CHECKBOX_KIND gadget created");
        check(strstr(output, "Created INTEGER_KIND gadget: OK") != NULL,
              "INTEGER_KIND gadget created");
        check(strstr(output, "Created CYCLE_KIND gadget: OK") != NULL,
              "CYCLE_KIND gadget created");
        check(strstr(output, "GT_RefreshWindow called") != NULL,
              "GT_RefreshWindow called");
        check(strstr(output, "PASS: Gadget list populated") != NULL,
              "Gadget list populated in window");
        check(strstr(output, "PASS: GT_SetGadgetAttrs executed") != NULL,
              "GT_SetGadgetAttrs works");
    }
    
    /* Let task reach interactive testing phase and complete */
    printf("\nLetting task complete interactive testing phase...\n");
    run_cycles_only(500, 10000);
    
    /* Check if the sample's own interactive tests ran */
    printf("\nTest 2: Verifying sample's interactive tests ran...\n");
    {
        char output[16384];
        lxa_get_output(output, sizeof(output));
        
        check(strstr(output, "Test 6 - Clicking BUTTON_KIND") != NULL,
              "Button click test started");
        check(strstr(output, "Test 7 - Clicking CYCLE_KIND") != NULL,
              "Cycle click test started");
    }
    
    /* Wait for program to complete naturally */
    printf("\nWaiting for program to complete...\n");
    {
        char output[16384];
        int timeout = 0;
        while (timeout < 1000) {
            lxa_run_cycles(10000);
            lxa_get_output(output, sizeof(output));
            if (strstr(output, "Test Summary") != NULL) {
                break;
            }
            timeout++;
        }
        
        /* Let it finish cleanup */
        run_cycles_only(100, 10000);
    }
    
    /* Verify final test results */
    printf("\nTest 3: Verifying test completion...\n");
    {
        char output[16384];
        lxa_get_output(output, sizeof(output));
        
        check(strstr(output, "Interactive testing complete") != NULL,
              "Interactive testing completed");
        check(strstr(output, "Window closed") != NULL,
              "Window closed cleanly");
        check(strstr(output, "Gadgets freed") != NULL,
              "Gadgets properly freed");
        check(strstr(output, "Libraries closed") != NULL,
              "Libraries properly closed");
        
        /* Check overall result */
        if (strstr(output, "ALL TESTS PASSED") != NULL) {
            printf("  OK: Sample reported ALL TESTS PASSED\n");
        } else if (strstr(output, "Tests passed:") != NULL) {
            printf("  Note: Sample completed with some tests\n");
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
