/*
 * updatestrgad_test.c - Host-side test driver for UpdateStrGad sample
 *
 * This test driver uses liblxa to:
 * 1. Start the UpdateStrGad sample
 * 2. Wait for the window to open
 * 3. Verify the window opened and initial string value is correct
 * 4. Type into the string gadget and verify GADGETUP
 * 5. Close the window and verify clean exit
 *
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
 * Helper: run cycles with VBlanks for event processing
 */
static void run_with_vblanks(int vblank_count, int cycles_per_vblank)
{
    for (int i = 0; i < vblank_count; i++) {
        lxa_trigger_vblank();
        lxa_run_cycles(cycles_per_vblank);
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

    /* Let the program initialize fully - need many VBlank cycles for Printf to flush */
    run_with_vblanks(100, 50000);
    
    /* Additional cycles for the m68k program to process messages and produce output */
    run_with_vblanks(50, 50000);

    /* ========== Test 1: Verify initial output ========== */
    printf("Test 1: Verifying window opened with correct initial state...\n");
    {
        char output[8192];
        lxa_get_output(output, sizeof(output));
        
        check(strstr(output, "Window opened with string gadget") != NULL, 
              "Window opened message present");
        check(strstr(output, "Initial value: 'START'") != NULL,
              "Initial string gadget value is 'START'");
    }
    
    /* ========== Test 2: Type into the string gadget ========== */
    printf("\nTest 2: Typing into string gadget...\n");
    {
        /* Click in the string gadget to activate it using convenience wrapper */
        int clickX = win_info.x + 20 + 100;
        int clickY = win_info.y + 20 + 4;
        
        lxa_inject_mouse_click(clickX, clickY, LXA_MOUSE_LEFT);
        run_with_vblanks(10, 50000);
        
        /* Clear output and type "Hello" followed by Return */
        lxa_clear_output();
        lxa_inject_string("Hello\n");
        run_with_vblanks(50, 50000);
        
        /* Additional cycles for the m68k program to wake up, GetMsg(), and printf() */
        run_with_vblanks(30, 50000);
        
        char output[8192];
        lxa_get_output(output, sizeof(output));
        
        check(strstr(output, "IDCMP_GADGETUP") != NULL,
              "GADGETUP received after typing");
        check(strstr(output, "string is 'STARTHello'") != NULL,
              "String gadget contains 'STARTHello' (START from initial buffer + Hello typed)");
    }
    
    /* ========== Test 3: Close the window ========== */
    printf("\nTest 3: Closing window...\n");
    {
        lxa_click_close_gadget(0);
        run_with_vblanks(20, 50000);
        
        check(lxa_wait_exit(3000), "Program exited cleanly after close window");
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
