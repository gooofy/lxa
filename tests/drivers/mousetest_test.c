/*
 * mousetest_test.c - Host-side test driver for MouseTest sample
 *
 * This test driver uses liblxa to:
 * 1. Start the MouseTest sample
 * 2. Wait for the window to open
 * 3. Move the mouse and verify MOUSEMOVE events
 * 4. Click left button and verify MOUSEBUTTONS events
 * 5. Click right button and verify MENUDOWN/MENUUP
 * 6. Click close gadget to exit
 */

#include "lxa_api.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

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
    printf("=== MouseTest Test Driver ===\n\n");

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
    
    printf("Loading MouseTest...\n");
    if (lxa_load_program("SYS:MouseTest", "") != 0) {
        fprintf(stderr, "ERROR: Failed to load MouseTest\n");
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
    
    /* ========== Test 1: Mouse Movement ========== */
    printf("Test 1: Testing mouse movement...\n");
    {
        int move_x = win_info.x + win_info.width / 2;
        int move_y = win_info.y + TITLE_BAR_HEIGHT + 10;
        
        printf("  Moving mouse to (%d, %d)\n", move_x, move_y);
        
        /* Inject mouse move */
        lxa_inject_mouse(move_x, move_y, 0, LXA_EVENT_MOUSEMOVE);
        
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
        
        check(strstr(output, "MouseMove") != NULL, "MOUSEMOVE event received");
    }
    
    /* ========== Test 2: Left Button Click ========== */
    printf("\nTest 2: Testing left button click...\n");
    {
        lxa_clear_output();
        
        int click_x = win_info.x + win_info.width / 2;
        int click_y = win_info.y + TITLE_BAR_HEIGHT + 20;
        
        printf("  Clicking left button at (%d, %d)\n", click_x, click_y);
        
        lxa_inject_mouse_click(click_x, click_y, LXA_MOUSE_LEFT);
        
        for (int i = 0; i < 20; i++) {
            lxa_trigger_vblank();
            lxa_run_cycles(50000);
        }
        
        for (int i = 0; i < 100; i++) {
            lxa_run_cycles(10000);
        }
        
        char output[4096];
        lxa_get_output(output, sizeof(output));
        
        check(strstr(output, "Left Button Down") != NULL, "Left button down received");
        check(strstr(output, "Left Button Up") != NULL, "Left button up received");
    }
    
    /* ========== Test 3: Right Button Click ========== */
    printf("\nTest 3: Testing right button click...\n");
    {
        lxa_clear_output();
        
        int click_x = win_info.x + win_info.width / 2;
        int click_y = win_info.y + TITLE_BAR_HEIGHT + 20;
        
        printf("  Clicking right button at (%d, %d)\n", click_x, click_y);
        
        lxa_inject_mouse_click(click_x, click_y, LXA_MOUSE_RIGHT);
        
        for (int i = 0; i < 20; i++) {
            lxa_trigger_vblank();
            lxa_run_cycles(50000);
        }
        
        for (int i = 0; i < 100; i++) {
            lxa_run_cycles(10000);
        }
        
        char output[4096];
        lxa_get_output(output, sizeof(output));
        
        check(strstr(output, "Right Button Down") != NULL, "Right button down received");
        check(strstr(output, "Right Button Up") != NULL, "Right button up received");
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
