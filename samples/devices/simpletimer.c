/* SimpleTimer.c - Adapted from RKM Simple_Timer sample
 * Demonstrates the timer device:
 * - Creating and deleting timer requests
 * - TR_ADDREQUEST (time delays)
 * - TR_GETSYSTIME (reading system time)
 * Modified for automated testing (shorter delays, no Execute)
 */

#include <exec/types.h>
#include <exec/io.h>
#include <exec/memory.h>
#include <devices/timer.h>

#include <clib/exec_protos.h>
#include <clib/alib_protos.h>
#include <clib/dos_protos.h>

#include <stdio.h>

/* Timer sub-routines */
struct timerequest *create_timer(ULONG unit);
void delete_timer(struct timerequest *tr);
LONG get_sys_time(struct timeval *tv);
void wait_for_timer(struct timerequest *tr, struct timeval *tv);
LONG time_delay(struct timeval *tv, LONG unit);
void show_time(ULONG secs);

struct Library *TimerBase = NULL;

/* Time conversion constants */
#define SECSPERMIN   (60)
#define SECSPERHOUR  (60*60)
#define SECSPERDAY   (60*60*24)

struct timerequest *create_timer(ULONG unit)
{
    LONG error;
    struct MsgPort *timerport;
    struct timerequest *TimerIO;
    
    timerport = CreatePort(0, 0);
    if (timerport == NULL)
        return NULL;
    
    TimerIO = (struct timerequest *)CreateExtIO(timerport, sizeof(struct timerequest));
    if (TimerIO == NULL)
    {
        DeletePort(timerport);
        return NULL;
    }
    
    error = OpenDevice(TIMERNAME, unit, (struct IORequest *)TimerIO, 0L);
    if (error != 0)
    {
        DeletePort(timerport);
        DeleteExtIO((struct IORequest *)TimerIO);
        return NULL;
    }
    return TimerIO;
}

void delete_timer(struct timerequest *tr)
{
    struct MsgPort *tp;
    
    if (tr != NULL)
    {
        tp = tr->tr_node.io_Message.mn_ReplyPort;
        CloseDevice((struct IORequest *)tr);
        DeleteExtIO((struct IORequest *)tr);
        if (tp != NULL)
            DeletePort(tp);
    }
}

void wait_for_timer(struct timerequest *tr, struct timeval *tv)
{
    tr->tr_node.io_Command = TR_ADDREQUEST;
    tr->tr_time = *tv;
    DoIO((struct IORequest *)tr);
}

LONG time_delay(struct timeval *tv, LONG unit)
{
    struct timerequest *tr;
    
    tr = create_timer(unit);
    if (tr == NULL)
        return -1L;
    
    wait_for_timer(tr, tv);
    delete_timer(tr);
    return 0L;
}

LONG get_sys_time(struct timeval *tv)
{
    struct timerequest *tr;
    
    tr = create_timer(UNIT_MICROHZ);
    if (tr == NULL)
        return -1L;
    
    tr->tr_node.io_Command = TR_GETSYSTIME;
    DoIO((struct IORequest *)tr);
    
    *tv = tr->tr_time;
    delete_timer(tr);
    return 0L;
}

void show_time(ULONG secs)
{
    ULONG days, hrs, mins;
    
    mins = secs / 60;
    hrs = mins / 60;
    days = hrs / 24;
    secs = secs % 60;
    mins = mins % 60;
    hrs = hrs % 24;
    
    printf("  Time: %02ld:%02ld:%02ld (Days: %ld)\n", hrs, mins, secs, days);
}

int main(int argc, char **argv)
{
    struct timerequest *tr;
    struct timeval tv1, tv2, tv3;
    LONG diff_micro;
    
    printf("SimpleTimer: Demonstrating timer.device\n\n");
    
    /* Test 1: Create timer with UNIT_VBLANK */
    printf("SimpleTimer: Test 1 - Creating timer (UNIT_VBLANK)...\n");
    tr = create_timer(UNIT_VBLANK);
    if (tr)
    {
        printf("  TimerRequest created: 0x%08lx\n", (ULONG)tr);
        printf("  Device: 0x%08lx\n", (ULONG)tr->tr_node.io_Device);
        printf("  Unit: 0x%08lx\n", (ULONG)tr->tr_node.io_Unit);
        delete_timer(tr);
        printf("  Timer deleted successfully\n");
    }
    else
    {
        printf("  ERROR: Failed to create timer!\n");
    }
    
    /* Test 2: Create timer with UNIT_MICROHZ */
    printf("\nSimpleTimer: Test 2 - Creating timer (UNIT_MICROHZ)...\n");
    tr = create_timer(UNIT_MICROHZ);
    if (tr)
    {
        printf("  TimerRequest created: 0x%08lx\n", (ULONG)tr);
        TimerBase = (struct Library *)tr->tr_node.io_Device;
        printf("  TimerBase set to: 0x%08lx\n", (ULONG)TimerBase);
        delete_timer(tr);
        printf("  Timer deleted successfully\n");
    }
    else
    {
        printf("  ERROR: Failed to create timer!\n");
    }
    
    /* Test 3: Get system time */
    printf("\nSimpleTimer: Test 3 - Getting system time (TR_GETSYSTIME)...\n");
    if (get_sys_time(&tv1) == 0)
    {
        printf("  System time retrieved successfully\n");
        printf("  Seconds: %ld, Microseconds: %ld\n", tv1.tv_secs, tv1.tv_micro);
        show_time(tv1.tv_secs);
    }
    else
    {
        printf("  ERROR: Failed to get system time!\n");
    }
    
    /* Test 4: Multiple time reads (microseconds should increase) */
    printf("\nSimpleTimer: Test 4 - Multiple TR_GETSYSTIME calls...\n");
    printf("  (Microseconds should increase between calls)\n");
    
    get_sys_time(&tv1);
    get_sys_time(&tv2);
    get_sys_time(&tv3);
    
    printf("  First:  %ld.%06ld\n", tv1.tv_secs, tv1.tv_micro);
    printf("  Second: %ld.%06ld\n", tv2.tv_secs, tv2.tv_micro);
    printf("  Third:  %ld.%06ld\n", tv3.tv_secs, tv3.tv_micro);
    
    /* Calculate if time increased */
    diff_micro = (tv3.tv_secs - tv1.tv_secs) * 1000000 + (tv3.tv_micro - tv1.tv_micro);
    if (diff_micro >= 0)
    {
        printf("  Time correctly increased by %ld microseconds\n", diff_micro);
    }
    else
    {
        printf("  WARNING: Time did not increase as expected!\n");
    }
    
    /* Test 5: Short delay using UNIT_MICROHZ */
    printf("\nSimpleTimer: Test 5 - Short delay (50ms using UNIT_MICROHZ)...\n");
    tv1.tv_secs = 0;
    tv1.tv_micro = 50000;  /* 50 milliseconds */
    
    get_sys_time(&tv2);
    printf("  Before delay: %ld.%06ld\n", tv2.tv_secs, tv2.tv_micro);
    
    if (time_delay(&tv1, UNIT_MICROHZ) == 0)
    {
        get_sys_time(&tv3);
        printf("  After delay:  %ld.%06ld\n", tv3.tv_secs, tv3.tv_micro);
        
        diff_micro = (tv3.tv_secs - tv2.tv_secs) * 1000000 + (tv3.tv_micro - tv2.tv_micro);
        printf("  Actual delay: %ld microseconds (requested 50000)\n", diff_micro);
        
        if (diff_micro >= 40000)  /* Allow some tolerance */
        {
            printf("  Delay completed successfully\n");
        }
        else
        {
            printf("  WARNING: Delay shorter than expected!\n");
        }
    }
    else
    {
        printf("  ERROR: time_delay() failed!\n");
    }
    
    /* Test 6: Short delay using UNIT_VBLANK */
    printf("\nSimpleTimer: Test 6 - Short delay (using UNIT_VBLANK)...\n");
    tv1.tv_secs = 0;
    tv1.tv_micro = 100000;  /* 100 milliseconds - VBLANK resolution */
    
    get_sys_time(&tv2);
    printf("  Before delay: %ld.%06ld\n", tv2.tv_secs, tv2.tv_micro);
    
    if (time_delay(&tv1, UNIT_VBLANK) == 0)
    {
        get_sys_time(&tv3);
        printf("  After delay:  %ld.%06ld\n", tv3.tv_secs, tv3.tv_micro);
        
        diff_micro = (tv3.tv_secs - tv2.tv_secs) * 1000000 + (tv3.tv_micro - tv2.tv_micro);
        printf("  Actual delay: %ld microseconds\n", diff_micro);
        printf("  Delay completed successfully\n");
    }
    else
    {
        printf("  ERROR: time_delay() failed!\n");
    }
    
    printf("\nSimpleTimer: Demo complete.\n");
    return 0;
}
