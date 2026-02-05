/* uptime.c - System uptime calculator
 * Demonstrates:
 * - timer.device for getting system time
 * - DateStamp() for current date/time
 * - Time calculation and formatting
 * 
 * This version uses timer.device TR_GETSYSTIME to get system uptime.
 * On real Amiga, you'd compare RAM: volume date with current time.
 * For lxa, system time starts from 0 at boot.
 */

#include <exec/types.h>
#include <exec/memory.h>
#include <exec/io.h>
#include <dos/dos.h>
#include <dos/datetime.h>
#include <devices/timer.h>

#include <clib/exec_protos.h>
#include <clib/dos_protos.h>
#include <clib/alib_protos.h>

#include <stdio.h>
#include <stdlib.h>

/* Constants */
#define SECS_PER_MIN   60
#define SECS_PER_HOUR  3600
#define SECS_PER_DAY   86400

int main(void)
{
    struct MsgPort *timerport = NULL;
    struct timerequest *tr = NULL;
    LONG error;
    LONG uptime_secs;
    LONG days, hours, mins, secs;
    LONG remainder;
    LONG rc = RETURN_FAIL;

    printf("Uptime: System uptime calculator\n\n");

    /* Create message port for timer */
    timerport = CreatePort(0, 0);
    if (!timerport)
    {
        printf("ERROR: Could not create message port\n");
        goto cleanup;
    }
    printf("Uptime: Created message port\n");

    /* Create timer IO request */
    tr = (struct timerequest *)CreateExtIO(timerport, sizeof(struct timerequest));
    if (!tr)
    {
        printf("ERROR: Could not create timer request\n");
        goto cleanup;
    }
    printf("Uptime: Created timer request\n");

    /* Open timer.device */
    error = OpenDevice(TIMERNAME, UNIT_MICROHZ, (struct IORequest *)tr, 0L);
    if (error != 0)
    {
        printf("ERROR: Could not open timer.device (error %ld)\n", error);
        tr = NULL;  /* Don't try to close it */
        goto cleanup;
    }
    printf("Uptime: Opened timer.device\n");

    /* Get system time - this represents uptime since boot */
    tr->tr_node.io_Command = TR_GETSYSTIME;
    DoIO((struct IORequest *)tr);

    uptime_secs = tr->tr_time.tv_secs;
    
    printf("Uptime: System time = %ld.%06ld seconds\n", 
           tr->tr_time.tv_secs, tr->tr_time.tv_micro);

    /* Calculate days, hours, minutes, seconds */
    days = uptime_secs / SECS_PER_DAY;
    remainder = uptime_secs % SECS_PER_DAY;
    
    hours = remainder / SECS_PER_HOUR;
    remainder = remainder % SECS_PER_HOUR;
    
    mins = remainder / SECS_PER_MIN;
    secs = remainder % SECS_PER_MIN;

    printf("\nUptime: System has been up for %ld days, %ld hours, %ld minutes, %ld seconds\n",
           days, hours, mins, secs);

    /* Also show current date from DateStamp for reference */
    {
        struct DateStamp ds;
        DateStamp(&ds);
        printf("\nUptime: DateStamp - Days=%ld, Minute=%ld, Tick=%ld\n",
               ds.ds_Days, ds.ds_Minute, ds.ds_Tick);
    }

    rc = RETURN_OK;

cleanup:
    if (tr)
    {
        CloseDevice((struct IORequest *)tr);
        DeleteExtIO((struct IORequest *)tr);
    }
    if (timerport)
        DeletePort(timerport);

    printf("\nUptime: Done (rc=%ld)\n", rc);
    return rc;
}
