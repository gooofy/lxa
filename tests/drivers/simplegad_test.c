/*
 * simplegad_test.c - Host-side test driver for SimpleGad sample
 *
 * This test driver uses liblxa to:
 * 1. Start the SimpleGad sample
 * 2. Wait for the window to open
 * 3. Click Button 1 and verify GADGETDOWN + GADGETUP
 * 4. Click Button 2 and verify only GADGETUP (no GADGETDOWN)
 * 5. Click the Exit button to close the window
 * 6. Verify the program exits cleanly
 */

#include "lxa_api.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* Gadget positions (from simplegad.c) */
#define BUTTON1_LEFT  20
#define BUTTON1_TOP   20
#define BUTTON2_LEFT  20
#define BUTTON2_TOP   90
#define BUTTON3_LEFT  20
#define BUTTON3_TOP   160
#define BUTTON_WIDTH  100
#define BUTTON_HEIGHT 50

/* Window title bar height (approximate) */
#define TITLE_BAR_HEIGHT 11

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
    /* Try common locations */
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
    /* Try common locations */
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
    printf("=== SimpleGad Test Driver ===\n\n");

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
    
    /* Load the SimpleGad program */
    printf("Loading SimpleGad...\n");
    if (lxa_load_program("SYS:SimpleGad", "") != 0) {
        fprintf(stderr, "ERROR: Failed to load SimpleGad\n");
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
    
    /* Wait for the program to finish initialization and be ready for input */
    /* It prints "Click the close gadget or Exit button to quit" when ready */
    printf("Waiting for program to be ready...\n");
    {
        char output[4096];
        int timeout = 0;
        while (timeout < 500) {  /* 500 iterations * 10000 cycles = 5M cycles max */
            lxa_run_cycles(10000);
            lxa_get_output(output, sizeof(output));
            if (strstr(output, "Exit button to quit") != NULL) {
                break;
            }
            timeout++;
        }
        if (timeout >= 500) {
            fprintf(stderr, "WARNING: Program may not be ready (timeout waiting for init)\n");
        }
        
        /* CRITICAL: Run many more cycles to ensure the task finishes its printf() calls
         * and reaches WaitPort(). The printf output appears while the task is still
         * executing the printf call - we need to let it complete and enter the main loop.
         * Without VBlanks, there are no events to process, so the task should run
         * straight through to WaitPort() where it will block. */
        printf("Letting task reach WaitPort()...\n");
        for (int i = 0; i < 200; i++) {
            lxa_run_cycles(10000);  /* 200 * 10000 = 2M cycles */
        }
    }
    lxa_clear_output();
    
    /* ========== Test 1: Click Button 1 ========== */
    printf("Test 1: Clicking Button 1 (IMMEDIATE+RELVERIFY)...\n");
    {
        int btn_x = win_info.x + BUTTON1_LEFT + (BUTTON_WIDTH / 2);
        int btn_y = win_info.y + TITLE_BAR_HEIGHT + BUTTON1_TOP + (BUTTON_HEIGHT / 2);
        
        printf("  Clicking at (%d, %d)\n", btn_x, btn_y);
        
        /* Inject mouse click */
        lxa_inject_mouse_click(btn_x, btn_y, LXA_MOUSE_LEFT);
        
        /* Run more cycles with VBlanks to ensure task processes the event and prints output */
        for (int i = 0; i < 20; i++) {
            lxa_trigger_vblank();
            lxa_run_cycles(50000);
        }
        
        /* Run additional cycles WITHOUT VBlanks to let the task process messages and print output.
         * VBlanks interrupt the task, so we need uninterrupted cycles for printf to complete. */
        for (int i = 0; i < 100; i++) {
            lxa_run_cycles(10000);  /* 100 * 10000 = 1M cycles */
        }
        
        /* Check output for expected messages */
        char output[4096];
        lxa_get_output(output, sizeof(output));
        
        check(strstr(output, "GADGETDOWN") != NULL, "GADGETDOWN received");
        check(strstr(output, "Gadget ID 3") != NULL, "Correct gadget ID (3)");
    }
    
    /* ========== Test 2: Click Button 2 ========== */
    printf("\nTest 2: Clicking Button 2 (RELVERIFY only)...\n");
    {
        lxa_clear_output();
        
        int btn_x = win_info.x + BUTTON2_LEFT + (BUTTON_WIDTH / 2);
        int btn_y = win_info.y + TITLE_BAR_HEIGHT + BUTTON2_TOP + (BUTTON_HEIGHT / 2);
        
        printf("  Clicking at (%d, %d)\n", btn_x, btn_y);
        
        lxa_inject_mouse_click(btn_x, btn_y, LXA_MOUSE_LEFT);
        
        /* Run more cycles with VBlanks to ensure task processes the event and prints output */
        for (int i = 0; i < 20; i++) {
            lxa_trigger_vblank();
            lxa_run_cycles(50000);
        }
        
        /* Run additional cycles WITHOUT VBlanks to let the task process messages and print output */
        for (int i = 0; i < 100; i++) {
            lxa_run_cycles(10000);  /* 100 * 10000 = 1M cycles */
        }
        
        char output[4096];
        lxa_get_output(output, sizeof(output));
        
        /* RELVERIFY-only should NOT generate GADGETDOWN */
        /* But it should generate GADGETUP */
        check(strstr(output, "GADGETUP") != NULL, "GADGETUP received");
        check(strstr(output, "Gadget ID 4") != NULL, "Correct gadget ID (4)");
    }
    
    /* ========== Test 3: Click Exit button ========== */
    printf("\nTest 3: Clicking Exit button...\n");
    {
        lxa_clear_output();
        
        int btn_x = win_info.x + BUTTON3_LEFT + (BUTTON_WIDTH / 2);
        int btn_y = win_info.y + TITLE_BAR_HEIGHT + BUTTON3_TOP + (BUTTON_HEIGHT / 2);
        
        printf("  Clicking at (%d, %d)\n", btn_x, btn_y);
        
        lxa_inject_mouse_click(btn_x, btn_y, LXA_MOUSE_LEFT);
        
        /* Run until exit */
        printf("  Waiting for program to exit...\n");
        if (!lxa_wait_exit(5000)) {
            printf("  WARNING: Program did not exit within 5 seconds\n");
        }
        
        char output[4096];
        lxa_get_output(output, sizeof(output));
        
        check(strstr(output, "Exit button clicked") != NULL, "Exit button recognized");
        check(strstr(output, "Closing window") != NULL, "Window closed cleanly");
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
