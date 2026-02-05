/*
 * a2d.c - Utility library date conversion functions test
 *
 * Based on RKM Utility sample, adapted for automated testing.
 * Tests: Amiga2Date, CheckDate, Date2Amiga
 */

#include <exec/types.h>
#include <exec/memory.h>
#include <dos/datetime.h>
#include <devices/timer.h>
#include <stdio.h>

#include <clib/exec_protos.h>
#include <clib/timer_protos.h>
#include <clib/utility_protos.h>

struct Library *TimerBase = NULL;
struct Library *UtilityBase = NULL;

int main(void)
{
    struct ClockData *clockdata;
    struct timerequest *tr;
    struct timeval *tv;
    LONG seconds;
    ULONG result;

    printf("A2D: Utility library date conversion functions test\n\n");

    printf("Opening utility.library...\n");
    UtilityBase = OpenLibrary("utility.library", 37);
    if (UtilityBase == NULL)
    {
        printf("ERROR: Can't open utility.library v37\n");
        return 1;
    }
    printf("OK: utility.library opened (version %d)\n", UtilityBase->lib_Version);

    printf("Allocating memory structures...\n");
    tr = AllocMem(sizeof(struct timerequest), MEMF_CLEAR);
    if (tr == NULL)
    {
        printf("ERROR: Can't allocate timerequest\n");
        CloseLibrary(UtilityBase);
        return 1;
    }

    tv = AllocMem(sizeof(struct timeval), MEMF_CLEAR);
    if (tv == NULL)
    {
        printf("ERROR: Can't allocate timeval\n");
        FreeMem(tr, sizeof(struct timerequest));
        CloseLibrary(UtilityBase);
        return 1;
    }

    clockdata = AllocMem(sizeof(struct ClockData), MEMF_CLEAR);
    if (clockdata == NULL)
    {
        printf("ERROR: Can't allocate clockdata\n");
        FreeMem(tv, sizeof(struct timeval));
        FreeMem(tr, sizeof(struct timerequest));
        CloseLibrary(UtilityBase);
        return 1;
    }
    printf("OK: Memory structures allocated\n");

    printf("\nOpening timer.device...\n");
    result = OpenDevice("timer.device", UNIT_VBLANK, (struct IORequest *)tr, 0);
    if (result != 0)
    {
        printf("ERROR: Can't open timer.device (error %lu)\n", result);
        FreeMem(clockdata, sizeof(struct ClockData));
        FreeMem(tv, sizeof(struct timeval));
        FreeMem(tr, sizeof(struct timerequest));
        CloseLibrary(UtilityBase);
        return 1;
    }
    TimerBase = (struct Library *)tr->tr_node.io_Device;
    printf("OK: timer.device opened\n");

    /* Test 1: GetSysTime */
    printf("\n=== Test 1: GetSysTime ===\n");
    GetSysTime(tv);
    printf("GetSysTime(): Seconds=%ld, Microseconds=%ld\n", tv->tv_secs, tv->tv_micro);
    if (tv->tv_secs > 0)
        printf("OK: System time is valid\n");
    else
        printf("ERROR: System time is invalid\n");

    /* Test 2: Amiga2Date - Convert Amiga seconds to ClockData */
    printf("\n=== Test 2: Amiga2Date ===\n");
    Amiga2Date(tv->tv_secs, clockdata);
    printf("Amiga2Date(%ld):\n", tv->tv_secs);
    printf("  sec=%d, min=%d, hour=%d\n", clockdata->sec, clockdata->min, clockdata->hour);
    printf("  mday=%d, month=%d, year=%d, wday=%d\n", 
           clockdata->mday, clockdata->month, clockdata->year, clockdata->wday);
    
    if (clockdata->year >= 1978 && clockdata->month >= 1 && clockdata->month <= 12 &&
        clockdata->mday >= 1 && clockdata->mday <= 31)
        printf("OK: Date fields are valid\n");
    else
        printf("ERROR: Date fields are invalid\n");

    /* Test 3: CheckDate - Verify ClockData validity */
    printf("\n=== Test 3: CheckDate ===\n");
    seconds = CheckDate(clockdata);
    printf("CheckDate(): Result=%ld\n", seconds);
    if (seconds != 0)
        printf("OK: ClockData is valid (CheckDate returned %ld seconds)\n", seconds);
    else
        printf("ERROR: ClockData is invalid\n");

    /* Test 4: Date2Amiga - Convert ClockData back to seconds */
    printf("\n=== Test 4: Date2Amiga ===\n");
    seconds = Date2Amiga(clockdata);
    printf("Date2Amiga(): Seconds=%ld\n", seconds);
    if (seconds == tv->tv_secs)
        printf("OK: Round-trip conversion matches (seconds=%ld)\n", seconds);
    else
        printf("ERROR: Round-trip conversion mismatch (original=%ld, converted=%ld)\n",
               tv->tv_secs, seconds);

    /* Test 5: Known date conversion */
    printf("\n=== Test 5: Known Date Conversion ===\n");
    clockdata->sec = 0;
    clockdata->min = 0;
    clockdata->hour = 0;
    clockdata->mday = 1;
    clockdata->month = 1;
    clockdata->year = 1978;  /* Amiga epoch: Jan 1, 1978 */
    clockdata->wday = 0;

    printf("Testing Amiga epoch date: Jan 1, 1978 00:00:00\n");
    seconds = Date2Amiga(clockdata);
    printf("Date2Amiga(): Seconds=%ld\n", seconds);
    if (seconds == 0)
        printf("OK: Epoch date converts to 0 seconds\n");
    else
        printf("ERROR: Epoch date should be 0 seconds, got %ld\n", seconds);

    /* Cleanup */
    CloseDevice((struct IORequest *)tr);
    printf("\nOK: timer.device closed\n");

    FreeMem(clockdata, sizeof(struct ClockData));
    FreeMem(tv, sizeof(struct timeval));
    FreeMem(tr, sizeof(struct timerequest));
    printf("OK: Memory structures freed\n");

    CloseLibrary(UtilityBase);
    printf("OK: utility.library closed\n");

    printf("\nPASS: A2D tests completed successfully\n");
    return 0;
}
