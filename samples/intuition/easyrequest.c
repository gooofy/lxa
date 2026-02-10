/* easyrequest.c - Adapted from RKM sample
 * Shows the use of an easy requester.
 *
 * Original: rkm/libraries/intuition/requesters_alerts/easyrequest.c
 * Copyright (c) 1992 Commodore-Amiga, Inc.
 */

#define INTUI_V36_NAMES_ONLY

#include <exec/types.h>
#include <intuition/intuition.h>

#include <clib/exec_protos.h>
#include <clib/intuition_protos.h>

#include <stdio.h>

struct Library *IntuitionBase;

/* Declare the easy request structure.
 * This uses many features of EasyRequest(), including:
 *     multiple lines of body text separated by '\n'.
 *     variable substitution of a string (%s) in the body text.
 *     multiple button gadgets separated by '|'.
 *     variable substitution in a gadget (long decimal '%ld').
 */
struct EasyStruct myES =
{
    sizeof(struct EasyStruct),
    0,
    "Request Window Name",
    "Text for the request\nSecond line of %s text\nThird line of text for the request",
    "Yes|%ld|No",
};

int main(int argc, char **argv)
{
    LONG answer;
    LONG number;

    number = 3125794;  /* for use in the middle button */

    if (IntuitionBase = OpenLibrary("intuition.library", 37))
    {
        /* Note in the variable substitution:
         *     the string goes in the first open variable (in body text).
         *     the number goes in the second open (gadget text).
         */
        answer = EasyRequest(NULL, &myES, NULL, "(Variable)", number);

        /* Process the answer.  Note that the buttons are numbered in
         * a strange order.  This is because the rightmost button is
         * always a negative reply.  The code can use this if it chooses,
         * with a construct like:
         *
         *     if (EasyRequest())
         *          positive_response();
         */
        switch (answer)
        {
            case 1:
                printf("selected 'Yes'\n");
                break;
            case 2:
                printf("selected '%ld'\n", number);
                break;
            case 0:
                printf("selected 'No'\n");
                break;
        }

        CloseLibrary(IntuitionBase);
    }
    return 0;
}
