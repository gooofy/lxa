/* Port2.c - Adapted from RKM sample
 * Demonstrates Message Port creation and message receiving
 * Port2 receives messages from Port1 and replies to them
 */
#include <exec/types.h>
#include <exec/memory.h>
#include <exec/ports.h>
#include <libraries/dos.h>

#include <clib/exec_protos.h>
#include <clib/dos_protos.h>

#include <stdio.h>
#include <string.h>

#define PORTNAME "RKM_Port_Example"

struct MyMessage {
    struct Message mm_Message;
    ULONG mm_Number;
};

int main(int argc, char **argv)
{
    struct MsgPort *myport = NULL;
    struct MyMessage *mymsg = NULL;
    ULONG msgcount = 0;

    printf("Port2: Starting message receiver\n");

    /* Create a public message port */
    myport = CreatePort((CONST_STRPTR)PORTNAME, 0);
    if (!myport) {
        printf("Port2: Can't create message port\n");
        return RETURN_FAIL;
    }
    printf("Port2: Created public port '%s' at 0x%lx\n", PORTNAME, (ULONG)myport);

    printf("Port2: Waiting for messages (will process 5 messages)...\n");

    /* Process 5 messages then exit */
    while (msgcount < 5) {
        /* Wait for a message to arrive */
        WaitPort(myport);
        
        /* Get the message */
        mymsg = (struct MyMessage *)GetMsg(myport);
        if (mymsg) {
            msgcount++;
            printf("Port2: Received message %lu (total: %lu)\n", 
                   mymsg->mm_Number, msgcount);
            
            /* Reply to the message */
            ReplyMsg((struct Message *)mymsg);
            printf("Port2: Replied to message %lu\n", mymsg->mm_Number);
        }
    }

    printf("Port2: Processed all messages. Cleaning up...\n");

    /* Clean up any remaining messages */
    while ((mymsg = (struct MyMessage *)GetMsg(myport))) {
        ReplyMsg((struct Message *)mymsg);
    }

    /* Remove the public port */
    DeletePort(myport);
    
    printf("Port2: Exiting.\n");
    return RETURN_OK;
}
