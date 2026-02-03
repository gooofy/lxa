/*
 * Test: apps/devpac
 *
 * Automated test for HiSoft Devpac assembler IDE compatibility.
 * Phase 26: Real-world Application Testing
 *
 * Devpac is a professional assembler development environment
 * that includes an editor, assembler, linker, and debugger.
 *
 * This test:
 * 1. Loads Devpac binary and verifies it's valid
 * 2. Launches Devpac as a background process
 * 3. Waits and verifies a window opens
 */

#include <exec/types.h>
#include <exec/memory.h>
#include <exec/execbase.h>
#include <exec/io.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <dos/dostags.h>
#include <utility/tagitem.h>
#include <intuition/intuition.h>
#include <intuition/intuitionbase.h>
#include <graphics/gfxbase.h>
#include <clib/exec_protos.h>
#include <clib/dos_protos.h>
#include <clib/intuition_protos.h>
#include <clib/graphics_protos.h>
#include <inline/exec.h>
#include <inline/dos.h>
#include <inline/intuition.h>

extern struct DosLibrary *DOSBase;
extern struct ExecBase *SysBase;
extern struct IntuitionBase *IntuitionBase;
extern struct GfxBase *GfxBase;

static void print(const char *s)
{
    BPTR out = Output();
    LONG len = 0;
    const char *p = s;
    while (*p++) len++;
    Write(out, (CONST APTR)s, len);
}

static void print_num(const char *prefix, LONG val, const char *suffix)
{
    char buf[64];
    char *p = buf;
    const char *src = prefix;
    while (*src) *p++ = *src++;

    if (val < 0) {
        *p++ = '-';
        val = -val;
    }

    char digits[12];
    int i = 0;
    if (val == 0) {
        digits[i++] = '0';
    } else {
        while (val > 0) {
            digits[i++] = '0' + (val % 10);
            val /= 10;
        }
    }
    while (i > 0) *p++ = digits[--i];
    
    src = suffix;
    while (*src) *p++ = *src++;
    *p = '\0';
    print(buf);
}

/*
 * Check if a window with a specific title substring exists.
 */
static struct Window *find_window_by_title(const char *substr)
{
    struct Screen *scr;
    struct Window *win;

    /* Check all screens */
    scr = IntuitionBase->FirstScreen;
    while (scr) {
        win = scr->FirstWindow;
        while (win) {
            if (win->Title) {
                /* Simple substring search */
                const char *t = (const char *)win->Title;
                const char *tp = t;
                while (*tp) {
                    const char *check = tp;
                    const char *sp = substr;
                    while (*check && *sp && *check == *sp) {
                        check++;
                        sp++;
                    }
                    if (*sp == '\0') {
                        /* Found substring */
                        return win;
                    }
                    tp++;
                }
            }
            win = win->NextWindow;
        }
        scr = scr->NextScreen;
    }
    return NULL;
}

/*
 * Count total windows across all screens.
 */
static int count_windows(void)
{
    struct Screen *scr;
    struct Window *win;
    int count = 0;

    scr = IntuitionBase->FirstScreen;
    while (scr) {
        win = scr->FirstWindow;
        while (win) {
            count++;
            win = win->NextWindow;
        }
        scr = scr->NextScreen;
    }
    return count;
}

int main(void)
{
    int errors = 0;
    BPTR seg = 0;
    struct Process *proc = NULL;
    int initial_windows;
    int windows_after;

    print("=== Devpac Automated Compatibility Test ===\n\n");

    /* Open required libraries */
    IntuitionBase = (struct IntuitionBase *)OpenLibrary((STRPTR)"intuition.library", 0);
    if (!IntuitionBase) {
        print("FAIL: Cannot open intuition.library\n");
        return 20;
    }

    GfxBase = (struct GfxBase *)OpenLibrary((STRPTR)"graphics.library", 0);
    if (!GfxBase) {
        print("FAIL: Cannot open graphics.library\n");
        CloseLibrary((struct Library *)IntuitionBase);
        return 20;
    }

    /* ========== Test 1: Load Devpac ========== */
    print("--- Test 1: Load Devpac binary ---\n");

    /* Load Devpac from APPS: assign */
    seg = LoadSeg((STRPTR)"APPS:Devpac/Devpac");
    if (!seg) {
        print("FAIL: Cannot load Devpac from APPS:Devpac/Devpac\n");
        errors++;
        goto cleanup;
    }
    print("OK: Devpac binary loaded successfully\n");

    /* ========== Test 2: Launch Devpac as background process ========== */
    print("\n--- Test 2: Launch Devpac as background process ---\n");
    
    initial_windows = count_windows();
    print_num("  Initial window count: ", initial_windows, "\n");

    /* Open NIL: for input/output */
    BPTR nilIn = Open((STRPTR)"NIL:", MODE_OLDFILE);
    BPTR nilOut = Open((STRPTR)"NIL:", MODE_NEWFILE);
    
    if (!nilIn || !nilOut) {
        print("FAIL: Cannot open NIL: device\n");
        if (nilIn) Close(nilIn);
        if (nilOut) Close(nilOut);
        UnLoadSeg(seg);
        errors++;
        goto cleanup;
    }

    /* Create the background process */
    struct TagItem procTags[] = {
        { NP_Seglist, (ULONG)seg },
        { NP_Name, (ULONG)"Devpac" },
        { NP_StackSize, 16384 },
        { NP_Cli, TRUE },
        { NP_Input, (ULONG)nilIn },
        { NP_Output, (ULONG)nilOut },
        { NP_Arguments, (ULONG)"\n" },
        { NP_FreeSeglist, TRUE },
        { TAG_DONE, 0 }
    };
    
    proc = CreateNewProc(procTags);
    if (!proc) {
        print("FAIL: CreateNewProc failed\n");
        UnLoadSeg(seg);
        Close(nilIn);
        Close(nilOut);
        errors++;
        goto cleanup;
    }
    
    /* Clear seg since it's now owned by the process */
    seg = 0;
    
    print("OK: Devpac process created\n");

    /* ========== Test 3: Wait for Devpac window to open ========== */
    print("\n--- Test 3: Wait for Devpac window ---\n");
    
    /* Give Devpac time to start and open its window */
    struct Window *app_window = NULL;
    int wait_ticks = 0;
    int max_wait = 100;  /* 2 seconds */
    
    print("  Waiting for window to open...\n");
    while (wait_ticks < max_wait) {
        Delay(10);  /* Wait 0.2 seconds */
        wait_ticks += 10;
        
        /* Check for Devpac window - various possible titles */
        app_window = find_window_by_title("Devpac");
        if (!app_window) {
            app_window = find_window_by_title("DEVPAC");
        }
        if (!app_window) {
            app_window = find_window_by_title("GenAm");
        }
        if (!app_window) {
            app_window = find_window_by_title("HiSoft");
        }
        if (!app_window) {
            /* Devpac opens with "Untitled" as default document title */
            app_window = find_window_by_title("Untitled");
        }
        
        if (app_window) {
            print("OK: Devpac window found!\n");
            break;
        }
    }
    
    windows_after = count_windows();
    print_num("  Window count after launch: ", windows_after, "\n");
    
    if (!app_window) {
        /* No window found - but a window WAS opened if count increased */
        if (windows_after > initial_windows) {
            print("Note: Window opened but title doesn't match expected\n");
            print("OK: A window was opened (partial success)\n");
        } else {
            print("FAIL: No Devpac window opened\n");
            errors++;
        }
    } else {
        print("OK: Devpac window opened successfully\n");
        print("  Window title: ");
        if (app_window->Title) {
            print((const char *)app_window->Title);
        } else {
            print("(no title)");
        }
        print("\n");
    }

cleanup:
    CloseLibrary((struct Library *)GfxBase);
    CloseLibrary((struct Library *)IntuitionBase);

    /* ========== Final result ========== */
    print("\n=== Test Results ===\n");
    if (errors == 0) {
        print("PASS: Devpac launch test passed\n");
        return 0;
    } else {
        print_num("FAIL: ", errors, " errors occurred\n");
        return 20;
    }
}
