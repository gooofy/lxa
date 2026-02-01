/*
 * REQUESTCHOICE command - Display a text-based choice dialog
 * Phase 17 implementation for lxa
 * 
 * Template: TITLE/A,BODY/A,GADGETS/A/M
 * 
 * Usage:
 *   REQUESTCHOICE "Title" "Body text" "Yes|No|Cancel"
 *   
 * Returns the 1-based index of the selected gadget (0 for rightmost/cancel)
 */

#include <exec/types.h>
#include <dos/dos.h>
#include <clib/exec_protos.h>
#include <clib/dos_protos.h>
#include <inline/exec.h>
#include <inline/dos.h>

#include <string.h>
#include <stdlib.h>

/* External reference to library bases */
extern struct DosLibrary *DOSBase;
extern struct ExecBase *SysBase;

/* Command template - AmigaOS compatible */
#define TEMPLATE "TITLE/A,BODY/A,GADGETS/A/M"

/* Argument array indices */
#define ARG_TITLE   0
#define ARG_BODY    1
#define ARG_GADGETS 2
#define ARG_COUNT   3

/* Helper: output a string */
static void out_str(const char *str)
{
    Write(Output(), (STRPTR)str, strlen(str));
}

/* Helper: output a number */
static void out_num(LONG num)
{
    char buf[16];
    char *p = buf + sizeof(buf) - 1;
    BOOL neg = FALSE;
    
    *p = '\0';
    
    if (num < 0)
    {
        neg = TRUE;
        num = -num;
    }
    
    do
    {
        *--p = '0' + (num % 10);
        num /= 10;
    } while (num);
    
    if (neg)
    {
        *--p = '-';
    }
    
    out_str(p);
}

/* Parse gadget string and count gadgets */
static int count_gadgets(const char *gadgets)
{
    int count = 1;
    const char *p = gadgets;
    
    while (*p)
    {
        if (*p == '|')
        {
            count++;
        }
        p++;
    }
    
    return count;
}

/* Get gadget label by index (0-based) */
static void get_gadget_label(const char *gadgets, int index, char *buf, int buflen)
{
    const char *start = gadgets;
    const char *end;
    int i = 0;
    
    while (i < index && *start)
    {
        if (*start == '|')
        {
            i++;
        }
        start++;
    }
    
    end = start;
    while (*end && *end != '|')
    {
        end++;
    }
    
    int len = end - start;
    if (len >= buflen)
    {
        len = buflen - 1;
    }
    
    strncpy(buf, start, len);
    buf[len] = '\0';
}

int main(int argc, char **argv)
{
    struct RDArgs *rdargs;
    LONG args[ARG_COUNT] = {0};
    int choice = 0;
    
    /* Parse arguments using AmigaDOS template */
    rdargs = ReadArgs((STRPTR)TEMPLATE, args, NULL);
    if (!rdargs)
    {
        PrintFault(IoErr(), (STRPTR)"REQUESTCHOICE");
        return RETURN_FAIL;
    }
    
    STRPTR title = (STRPTR)args[ARG_TITLE];
    STRPTR body = (STRPTR)args[ARG_BODY];
    STRPTR *gadget_array = (STRPTR *)args[ARG_GADGETS];
    
    /* Build combined gadget string from array */
    char gadgets[512];
    gadgets[0] = '\0';
    
    if (gadget_array)
    {
        int first = 1;
        while (*gadget_array)
        {
            if (!first)
            {
                strncat(gadgets, "|", sizeof(gadgets) - strlen(gadgets) - 1);
            }
            strncat(gadgets, (char *)*gadget_array, sizeof(gadgets) - strlen(gadgets) - 1);
            gadget_array++;
            first = 0;
        }
    }
    
    int num_gadgets = count_gadgets(gadgets);
    
    /* Display the dialog */
    out_str("\n");
    
    /* Title */
    out_str("=== ");
    out_str((char *)title);
    out_str(" ===\n\n");
    
    /* Body */
    out_str((char *)body);
    out_str("\n\n");
    
    /* Gadgets */
    char label[64];
    for (int i = 0; i < num_gadgets; i++)
    {
        get_gadget_label(gadgets, i, label, sizeof(label));
        out_str("  ");
        out_num(i + 1);
        out_str(") ");
        out_str(label);
        out_str("\n");
    }
    
    out_str("\nYour choice (");
    out_num(1);
    out_str("-");
    out_num(num_gadgets);
    out_str("): ");
    
    /* Flush output */
    Flush(Output());
    
    /* Read user input */
    char buf[16];
    BPTR input = Input();
    LONG len = Read(input, (APTR)buf, sizeof(buf) - 1);
    
    if (len > 0)
    {
        buf[len] = '\0';
        choice = atoi(buf);
        
        /* Validate choice */
        if (choice < 1 || choice > num_gadgets)
        {
            /* Invalid choice - return 0 (cancel/rightmost) */
            choice = 0;
        }
    }
    else
    {
        /* No input - return 0 */
        choice = 0;
    }
    
    /* AmigaOS returns 1-based index, with 0 for rightmost gadget */
    /* If user selected the last gadget, return 0 */
    if (choice == num_gadgets)
    {
        choice = 0;
    }
    
    FreeArgs(rdargs);
    
    /* Return the choice as the exit code */
    return choice;
}
