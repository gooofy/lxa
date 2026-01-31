/*
 * DATE command - Display or set the system date and time
 * Step 9.5 implementation for lxa
 * 
 * Template: DATE,TIME,TO=VER/K
 * 
 * Usage:
 *   DATE             - Display current date and time
 *   DATE date time   - Set date and time (not implemented - would need root)
 *   DATE TO file     - Write date to file
 */

#include <exec/types.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <dos/datetime.h>
#include <clib/exec_protos.h>
#include <clib/dos_protos.h>
#include <inline/exec.h>
#include <inline/dos.h>

#include <string.h>

/* External reference to library bases */
extern struct DosLibrary *DOSBase;
extern struct ExecBase *SysBase;

/* Command template */
#define TEMPLATE "DATE,TIME,TO=VER/K"

/* Argument array indices */
#define ARG_DATE   0
#define ARG_TIME   1
#define ARG_TO     2
#define ARG_COUNT  3

/* Helper: output a string */
static void out_str(BPTR fh, const char *str)
{
    Write(fh, (STRPTR)str, strlen(str));
}

/* Month names */
static const char *month_names[] = {
    "Jan", "Feb", "Mar", "Apr", "May", "Jun",
    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};

/* Day names */
static const char *day_names[] = {
    "Sunday", "Monday", "Tuesday", "Wednesday",
    "Thursday", "Friday", "Saturday"
};

/* Output a 2-digit number with leading zero */
static void out_num2(BPTR fh, int num)
{
    char buf[3];
    buf[0] = '0' + (num / 10);
    buf[1] = '0' + (num % 10);
    buf[2] = '\0';
    out_str(fh, buf);
}

/* Output a number */
static void out_num(BPTR fh, int num)
{
    char buf[16];
    char *p = buf + sizeof(buf) - 1;
    *p = '\0';
    
    do {
        *--p = '0' + (num % 10);
        num /= 10;
    } while (num);
    
    out_str(fh, p);
}

/* Display current date and time */
static void show_date_time(BPTR fh)
{
    struct DateStamp ds;
    
    /* Get current date/time */
    DateStamp(&ds);
    
    /* Convert DateStamp to readable format */
    /* DateStamp: days since Jan 1, 1978; minutes since midnight; ticks */
    
    /* Calculate year, month, day from days since 1978 */
    LONG days = ds.ds_Days;
    LONG year = 1978;
    LONG month, day;
    
    /* Days in each month (non-leap year) */
    static const int days_in_month[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    
    /* Calculate year */
    while (1) {
        LONG daysInYear = 365;
        /* Leap year check: divisible by 4, except centuries unless divisible by 400 */
        if ((year % 4 == 0 && year % 100 != 0) || (year % 400 == 0)) {
            daysInYear = 366;
        }
        if (days < daysInYear) break;
        days -= daysInYear;
        year++;
    }
    
    /* Calculate month and day */
    BOOL leap = ((year % 4 == 0 && year % 100 != 0) || (year % 400 == 0));
    month = 0;
    while (month < 12) {
        int dim = days_in_month[month];
        if (month == 1 && leap) dim = 29;  /* February in leap year */
        if (days < dim) break;
        days -= dim;
        month++;
    }
    day = days + 1;  /* Days are 1-based */
    
    /* Calculate day of week (Jan 1, 1978 was a Sunday = 0) */
    LONG totalDays = ds.ds_Days;
    int dayOfWeek = totalDays % 7;
    
    /* Calculate hours, minutes, seconds from ds_Minute and ds_Tick */
    LONG minutes = ds.ds_Minute;
    int hours = minutes / 60;
    int mins = minutes % 60;
    int secs = ds.ds_Tick / 50;  /* 50 ticks per second */
    
    /* Output in format: "Monday 31-Jan-2026 10:30:45" */
    out_str(fh, day_names[dayOfWeek]);
    out_str(fh, " ");
    out_num2(fh, day);
    out_str(fh, "-");
    out_str(fh, month_names[month]);
    out_str(fh, "-");
    out_num(fh, year);
    out_str(fh, " ");
    out_num2(fh, hours);
    out_str(fh, ":");
    out_num2(fh, mins);
    out_str(fh, ":");
    out_num2(fh, secs);
    out_str(fh, "\n");
}

int main(int argc, char **argv)
{
    struct RDArgs *rdargs;
    LONG args[ARG_COUNT] = {0};
    int rc = RETURN_OK;
    
    /* Parse arguments */
    rdargs = ReadArgs((STRPTR)TEMPLATE, args, NULL);
    if (!rdargs) {
        PrintFault(IoErr(), (STRPTR)"DATE");
        return RETURN_FAIL;
    }
    
    STRPTR dateStr = (STRPTR)args[ARG_DATE];
    STRPTR timeStr = (STRPTR)args[ARG_TIME];
    STRPTR toFile = (STRPTR)args[ARG_TO];
    
    /* If date or time strings provided, try to set (not fully implemented) */
    if (dateStr || timeStr) {
        out_str(Output(), "DATE: Setting date/time is not supported in lxa\n");
        out_str(Output(), "      (System time is derived from host)\n");
        rc = RETURN_WARN;
    }
    
    /* Determine output destination */
    BPTR outFh = Output();
    BOOL closeOut = FALSE;
    
    if (toFile) {
        outFh = Open(toFile, MODE_NEWFILE);
        if (!outFh) {
            PrintFault(IoErr(), (STRPTR)"DATE");
            FreeArgs(rdargs);
            return RETURN_ERROR;
        }
        closeOut = TRUE;
    }
    
    /* Display current date/time */
    show_date_time(outFh);
    
    if (closeOut) {
        Close(outFh);
    }
    
    FreeArgs(rdargs);
    return rc;
}
