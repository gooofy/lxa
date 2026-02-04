/* Port1.c - Adapted from RKM sample
 * Demonstrates Message Port creation and message passing
 * Port1 sends messages to Port2
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
    struct MsgPort *replyport = NULL;
    struct MyMessage *mymsg = NULL;
    ULONG i;

    printf("Port1: Starting message port example\n");

    /* Create a reply port for receiving replies */
    replyport = CreatePort(NULL, 0);
    if (!replyport) {
        printf("Port1: Can't create reply port\n");
        return RETURN_FAIL;
    }
    printf("Port1: Created reply port\n");

    /* Wait a bit for Port2 to start and create its port */
    printf("Port1: Waiting for Port2 to create its message port...\n");
    Delay(50);  /* Wait 1 second */

    /* Try to find the public port created by Port2 */
    Forbid();
    myport = FindPort((CONST_STRPTR)PORTNAME);
    Permit();

    if (!myport) {
        printf("Port1: Can't find port '%s' - make sure Port2 is running!\n", PORTNAME);
        DeletePort(replyport);
        return RETURN_FAIL;
    }
    printf("Port1: Found message port '%s' at 0x%lx\n", PORTNAME, (ULONG)myport);

    /* Send 5 messages to Port2 */
    for (i = 1; i <= 5; i++) {
        mymsg = (struct MyMessage *)AllocMem(sizeof(struct MyMessage), MEMF_PUBLIC | MEMF_CLEAR);
        if (!mymsg) {
            printf("Port1: Can't allocate message %lu\n", i);
            break;
        }

        /* Initialize the message */
        mymsg->mm_Message.mn_Node.ln_Type = NT_MESSAGE;
        mymsg->mm_Message.mn_Length = sizeof(struct MyMessage);
        mymsg->mm_Message.mn_ReplyPort = replyport;
        mymsg->mm_Number = i;

        printf("Port1: Sending message %lu\n", i);
        PutMsg(myport, (struct Message *)mymsg);

        /* Wait for reply */
        WaitPort(replyport);
        mymsg = (struct MyMessage *)GetMsg(replyport);
        
        if (mymsg) {
            printf("Port1: Got reply for message %lu\n", mymsg->mm_Number);
            FreeMem(mymsg, sizeof(struct MyMessage));
        }
    }

    printf("Port1: All messages sent and replied. Exiting.\n");
    
    /* Clean up */
    DeletePort(replyport);
    
    return RETURN_OK;
}
