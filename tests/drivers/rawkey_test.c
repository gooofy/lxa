/*
 * rawkey_test.c - Host-side test driver for RawKey sample
 *
 * This test driver uses liblxa to:
 * 1. Start the RawKey sample
 * 2. Wait for the window to open
 * 3. Inject keyboard events (key presses)
 * 4. Verify RAWKEY events are received and converted correctly
 * 5. Click close gadget to exit
 */

#include "lxa_api.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* Amiga raw key codes */
#define RAWKEY_A        0x20
#define RAWKEY_B        0x35
#define RAWKEY_SPACE    0x40
#define RAWKEY_RETURN   0x44
#define RAWKEY_1        0x01
#define RAWKEY_LSHIFT   0x60

/* Qualifier bits */
#define IEQUALIFIER_LSHIFT  0x0001

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
    printf("=== RawKey Test Driver ===\n\n");

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
    
    printf("Loading RawKey...\n");
    if (lxa_load_program("SYS:RawKey", "") != 0) {
        fprintf(stderr, "ERROR: Failed to load RawKey\n");
        lxa_shutdown();
        return 1;
    }
    
    printf("Waiting for window to open...\n");
    if (!lxa_wait_windows(1, 5000)) {
        fprintf(stderr, "ERROR: Window did not open within 5 seconds\n");
        lxa_shutdown();
        return 1;
    }
    
    printf("Window opened!\n\n");
    
    lxa_window_info_t win_info;
    if (!lxa_get_window_info(0, &win_info)) {
        fprintf(stderr, "ERROR: Could not get window info\n");
        lxa_shutdown();
        return 1;
    }
    
    printf("Window at (%d, %d), size %dx%d\n\n", 
           win_info.x, win_info.y, win_info.width, win_info.height);
    
    /* Wait for program to initialize */
    printf("Waiting for program to be ready...\n");
    {
        char output[4096];
        int timeout = 0;
        while (timeout < 500) {
            lxa_run_cycles(10000);
            lxa_get_output(output, sizeof(output));
            if (strstr(output, "close gadget to exit") != NULL) {
                break;
            }
            timeout++;
        }
        
        /* Let task reach WaitPort() */
        printf("Letting task reach WaitPort()...\n");
        for (int i = 0; i < 200; i++) {
            lxa_run_cycles(10000);
        }
    }
    lxa_clear_output();
    
    /* ========== Test 1: Simple Key Press ('a') ========== */
    printf("Test 1: Pressing 'a' key...\n");
    {
        printf("  Injecting key 'a' (rawkey 0x%02x)\n", RAWKEY_A);
        
        lxa_inject_keypress(RAWKEY_A, 0);
        
        /* Process events */
        for (int i = 0; i < 20; i++) {
            lxa_trigger_vblank();
            lxa_run_cycles(50000);
        }
        
        for (int i = 0; i < 100; i++) {
            lxa_run_cycles(10000);
        }
        
        char output[4096];
        lxa_get_output(output, sizeof(output));
        
        check(strstr(output, "Key Down") != NULL, "Key down event received");
        check(strstr(output, "Key Up") != NULL, "Key up event received");
        /* The 'a' key should map to ASCII 'a' (97 = 0x61) */
        check(strstr(output, "'a'") != NULL || strstr(output, "= 'a'") != NULL, 
              "Key converted to 'a'");
    }
    
    /* ========== Test 2: Shifted Key Press ('A') ========== */
    printf("\nTest 2: Pressing Shift+'a' key...\n");
    {
        lxa_clear_output();
        
        printf("  Injecting Shift + 'a'\n");
        
        /* Press with shift qualifier */
        lxa_inject_keypress(RAWKEY_A, IEQUALIFIER_LSHIFT);
        
        for (int i = 0; i < 20; i++) {
            lxa_trigger_vblank();
            lxa_run_cycles(50000);
        }
        
        for (int i = 0; i < 100; i++) {
            lxa_run_cycles(10000);
        }
        
        char output[4096];
        lxa_get_output(output, sizeof(output));
        
        check(strstr(output, "LShift") != NULL || strstr(output, "RShift") != NULL, 
              "Shift qualifier detected");
        /* Shift+a should produce 'A' (65 = 0x41) */
        check(strstr(output, "'A'") != NULL || strstr(output, "= 'A'") != NULL, 
              "Key converted to 'A'");
    }
    
    /* ========== Test 3: Space Key ========== */
    printf("\nTest 3: Pressing space key...\n");
    {
        lxa_clear_output();
        
        printf("  Injecting space key (rawkey 0x%02x)\n", RAWKEY_SPACE);
        
        lxa_inject_keypress(RAWKEY_SPACE, 0);
        
        for (int i = 0; i < 20; i++) {
            lxa_trigger_vblank();
            lxa_run_cycles(50000);
        }
        
        for (int i = 0; i < 100; i++) {
            lxa_run_cycles(10000);
        }
        
        char output[4096];
        lxa_get_output(output, sizeof(output));
        
        check(strstr(output, "Key Down") != NULL, "Space key down received");
        /* Space is ASCII 32 = 0x20, shown as ' ' */
        check(strstr(output, "code= 32") != NULL || strstr(output, "($20)") != NULL,
              "Space key code correct (32/0x20)");
    }
    
    /* ========== Test 4: Close Window ========== */
    printf("\nTest 4: Closing window via close gadget...\n");
    {
        lxa_clear_output();
        
        printf("  Clicking close gadget\n");
        lxa_click_close_gadget(0);
        
        if (!lxa_wait_exit(5000)) {
            printf("  WARNING: Program did not exit within 5 seconds\n");
        }
        
        char output[4096];
        lxa_get_output(output, sizeof(output));
        
        check(strstr(output, "Close gadget clicked") != NULL, "Close event recognized");
        check(strstr(output, "Window closed") != NULL, "Window closed cleanly");
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
