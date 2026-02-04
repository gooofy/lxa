/*
 * Signal/Message Ping-Pong Test
 *
 * This test validates the Exec signal and message passing implementation by:
 * 1. Creating a message port in the main task
 * 2. Spawning a child task that creates its own port
 * 3. Exchanging messages between the two tasks
 * 4. Verifying proper message delivery and reply
 */

#include <exec/types.h>
#include <exec/memory.h>
#include <exec/ports.h>
#include <exec/tasks.h>
#include <dos/dos.h>
#include <dos/dostags.h>
#include <clib/exec_protos.h>
#include <clib/dos_protos.h>
#include <inline/exec.h>
#include <inline/dos.h>

extern struct ExecBase *SysBase;
extern struct DosLibrary *DOSBase;

/* Custom message structure for ping-pong */
struct PingPongMsg {
    struct Message pp_Message;
    LONG           pp_Counter;  /* Counts ping-pong exchanges */
};

/* Global port for main task - child will look it up by name */
#define MAIN_PORT_NAME "PingPong.Main"

/* Number of ping-pong exchanges */
#define NUM_EXCHANGES 5

/* Child task entry point */
void ChildTask(void)
{
    struct MsgPort *myPort = NULL;
    struct MsgPort *mainPort = NULL;
    struct PingPongMsg *msg;
    BPTR out = Output();
    BOOL running = TRUE;

    Write(out, (CONST APTR)"Child: Started\n", 15);

    /* Create our own message port */
    myPort = CreateMsgPort();
    if (!myPort)
    {
        Write(out, (CONST APTR)"Child: Failed to create port!\n", 30);
        return;
    }

    /* Find the main task's port */
    Forbid();
    mainPort = FindPort((STRPTR)MAIN_PORT_NAME);
    Permit();

    if (!mainPort)
    {
        Write(out, (CONST APTR)"Child: Main port not found!\n", 28);
        DeleteMsgPort(myPort);
        return;
    }

    Write(out, (CONST APTR)"Child: Found main port\n", 23);

    /* Allocate a message to send */
    msg = (struct PingPongMsg *)AllocMem(sizeof(struct PingPongMsg), MEMF_PUBLIC | MEMF_CLEAR);
    if (!msg)
    {
        Write(out, (CONST APTR)"Child: Failed to alloc msg!\n", 28);
        DeleteMsgPort(myPort);
        return;
    }

    /* Initialize message */
    msg->pp_Message.mn_Node.ln_Type = NT_MESSAGE;
    msg->pp_Message.mn_Length = sizeof(struct PingPongMsg);
    msg->pp_Message.mn_ReplyPort = myPort;
    msg->pp_Counter = 0;

    /* Send initial ping to main */
    Write(out, (CONST APTR)"Child: Sending ping 0\n", 22);
    PutMsg(mainPort, (struct Message *)msg);

    /* Message loop */
    while (running)
    {
        /* Wait for reply */
        WaitPort(myPort);
        msg = (struct PingPongMsg *)GetMsg(myPort);
        if (msg)
        {
            /* Check if we've completed enough exchanges.
             * Main sends final pong with counter = NUM_EXCHANGES * 2 - 1 (9),
             * then exits. So child should exit when counter reaches this value.
             */
            if (msg->pp_Counter >= NUM_EXCHANGES * 2 - 1)
            {
                /* We're done, this was the last reply from main */
                Write(out, (CONST APTR)"Child: Done, exiting\n", 21);
                running = FALSE;
            }
            else
            {
                /* Increment counter and send back */
                msg->pp_Counter++;
                Write(out, (CONST APTR)"Child: Pong, sending\n", 21);
                PutMsg(mainPort, (struct Message *)msg);
            }
        }
    }

    /* Cleanup */
    FreeMem(msg, sizeof(struct PingPongMsg));
    DeleteMsgPort(myPort);

    Write(out, (CONST APTR)"Child: Finished\n", 16);
}

int main(void)
{
    struct MsgPort *mainPort = NULL;
    struct PingPongMsg *msg;
    struct Process *childProc;
    BPTR out = Output();
    LONG exchanges = 0;

    Write(out, (CONST APTR)"Main: Starting ping-pong test\n", 30);

    /* Create main port */
    mainPort = CreateMsgPort();
    if (!mainPort)
    {
        Write(out, (CONST APTR)"Main: Failed to create port!\n", 29);
        return 20;
    }

    /* Set port name so child can find it */
    mainPort->mp_Node.ln_Name = (char *)MAIN_PORT_NAME;
    AddPort(mainPort);

    Write(out, (CONST APTR)"Main: Port created\n", 19);

    /* Create child process */
    {
        struct TagItem tags[] = {
            { NP_Entry, (ULONG)ChildTask },
            { NP_Name, (ULONG)"PingPong.Child" },
            { NP_StackSize, 8192 },
            { NP_Input, Input() },
            { NP_Output, Output() },
            { TAG_DONE, 0 }
        };
        childProc = CreateNewProc(tags);
    }

    if (!childProc)
    {
        Write(out, (CONST APTR)"Main: Failed to create child!\n", 30);
        RemPort(mainPort);
        DeleteMsgPort(mainPort);
        return 20;
    }

    Write(out, (CONST APTR)"Main: Child created\n", 20);

    /* Message loop - receive pings, send pongs */
    while (exchanges < NUM_EXCHANGES)
    {
        /* Wait for message from child */
        WaitPort(mainPort);
        msg = (struct PingPongMsg *)GetMsg(mainPort);
        if (msg)
        {
            Write(out, (CONST APTR)"Main: Received message\n", 23);

            exchanges++;

            if (exchanges >= NUM_EXCHANGES)
            {
                /* Increment counter and reply - child will see we're done */
                msg->pp_Counter++;
                Write(out, (CONST APTR)"Main: Sending final pong\n", 25);
            }
            else
            {
                /* Increment counter and reply */
                msg->pp_Counter++;
                Write(out, (CONST APTR)"Main: Sending pong\n", 19);
            }

            /* Reply to the message (send it back to child's reply port) */
            ReplyMsg((struct Message *)msg);
        }
    }

    Write(out, (CONST APTR)"Main: Test complete!\n", 21);

    /* Cleanup */
    RemPort(mainPort);
    DeleteMsgPort(mainPort);

    Write(out, (CONST APTR)"PASS: Ping-pong test\n", 21);
    return 0;
}
