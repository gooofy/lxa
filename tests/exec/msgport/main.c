/*
 * Exec Message Port Tests
 *
 * Tests:
 *   - CreateMsgPort/DeleteMsgPort
 *   - AddPort/RemPort/FindPort
 *   - PutMsg/GetMsg
 *   - Message queue ordering
 *   - Port signal allocation
 */

#include <exec/types.h>
#include <exec/memory.h>
#include <exec/ports.h>
#include <exec/tasks.h>
#include <dos/dos.h>
#include <clib/exec_protos.h>
#include <clib/dos_protos.h>
#include <inline/exec.h>
#include <inline/dos.h>

extern struct ExecBase *SysBase;
extern struct DosLibrary *DOSBase;

static BPTR g_out;
static LONG test_pass = 0;
static LONG test_fail = 0;

/* Helper to print a string */
static void print(const char *s)
{
    LONG len = 0;
    const char *p = s;
    while (*p++) len++;
    Write(g_out, (CONST APTR)s, len);
}

/* Simple number to string for test output */
static void print_num(LONG n)
{
    char buf[16];
    char *p = buf + 15;
    BOOL neg = FALSE;
    
    *p = '\0';
    
    if (n < 0) {
        neg = TRUE;
        n = -n;
    }
    
    do {
        *--p = '0' + (n % 10);
        n /= 10;
    } while (n > 0);
    
    if (neg) *--p = '-';
    
    print(p);
}

static void test_ok(const char *name)
{
    print("  OK: ");
    print(name);
    print("\n");
    test_pass++;
}

static void test_fail_msg(const char *name)
{
    print("  FAIL: ");
    print(name);
    print("\n");
    test_fail++;
}

/* Custom message for testing */
struct TestMessage {
    struct Message msg;
    LONG value;
};

int main(void)
{
    struct MsgPort *port1, *port2;
    struct MsgPort *foundPort;
    struct Task *thisTask;
    struct TestMessage msgs[5];
    struct Message *receivedMsg;
    int i;
    
    g_out = Output();
    
    print("Exec Message Port Tests\n");
    print("=======================\n\n");
    
    thisTask = FindTask(NULL);
    
    /* Initialize test messages */
    for (i = 0; i < 5; i++) {
        msgs[i].msg.mn_Node.ln_Type = NT_MESSAGE;
        msgs[i].msg.mn_Length = sizeof(struct TestMessage);
        msgs[i].msg.mn_ReplyPort = NULL;
        msgs[i].value = i + 100;
    }
    
    /* Test 1: CreateMsgPort */
    print("Test 1: CreateMsgPort\n");
    port1 = CreateMsgPort();
    
    if (port1 != NULL) {
        test_ok("CreateMsgPort returned non-NULL");
        
        if (port1->mp_Node.ln_Type == NT_MSGPORT) {
            test_ok("Port type is NT_MSGPORT");
        } else {
            print("    Type is: ");
            print_num(port1->mp_Node.ln_Type);
            print("\n");
            test_fail_msg("Port type incorrect");
        }
        
        if (port1->mp_SigTask == thisTask) {
            test_ok("Port SigTask is current task");
        } else {
            test_fail_msg("Port SigTask not set to current task");
        }
        
        if (port1->mp_SigBit >= 0 && port1->mp_SigBit < 32) {
            print("    Signal bit: ");
            print_num(port1->mp_SigBit);
            print("\n");
            test_ok("Signal bit allocated");
        } else {
            test_fail_msg("Invalid signal bit");
        }
        
        if ((port1->mp_Flags & PF_ACTION) == PA_SIGNAL) {
            test_ok("Port flags set to PA_SIGNAL");
        } else {
            test_fail_msg("Port flags not PA_SIGNAL");
        }
        
        /* Verify message list is empty */
        if (port1->mp_MsgList.lh_TailPred == (struct Node *)&port1->mp_MsgList) {
            test_ok("Message list is empty");
        } else {
            test_fail_msg("Message list should be empty");
        }
    } else {
        test_fail_msg("CreateMsgPort returned NULL");
        print("FATAL: Cannot continue without port\n");
        return 20;
    }
    
    /* Test 2: Multiple CreateMsgPort */
    print("\nTest 2: Multiple CreateMsgPort\n");
    port2 = CreateMsgPort();
    
    if (port2 != NULL) {
        test_ok("Second CreateMsgPort succeeded");
        
        if (port2 != port1) {
            test_ok("Ports are distinct");
        } else {
            test_fail_msg("CreateMsgPort returned same port twice");
        }
        
        if (port2->mp_SigBit != port1->mp_SigBit) {
            test_ok("Different signal bits allocated");
        } else {
            test_fail_msg("Same signal bit for different ports");
        }
    } else {
        test_fail_msg("Second CreateMsgPort returned NULL");
    }
    
    /* Test 3: PutMsg/GetMsg */
    print("\nTest 3: PutMsg/GetMsg\n");
    
    /* Clear any pending signals on our port */
    SetSignal(0, 1 << port1->mp_SigBit);
    
    PutMsg(port1, (struct Message *)&msgs[0]);
    
    /* Message list should not be empty now */
    if (port1->mp_MsgList.lh_TailPred != (struct Node *)&port1->mp_MsgList) {
        test_ok("Message queued after PutMsg");
    } else {
        test_fail_msg("Message list still empty after PutMsg");
    }
    
    receivedMsg = GetMsg(port1);
    if (receivedMsg == (struct Message *)&msgs[0]) {
        test_ok("GetMsg returned correct message");
    } else if (receivedMsg == NULL) {
        test_fail_msg("GetMsg returned NULL");
    } else {
        test_fail_msg("GetMsg returned wrong message");
    }
    
    /* Message list should be empty again */
    if (port1->mp_MsgList.lh_TailPred == (struct Node *)&port1->mp_MsgList) {
        test_ok("Message list empty after GetMsg");
    } else {
        test_fail_msg("Message list not empty after GetMsg");
    }
    
    /* Test 4: GetMsg on empty port */
    print("\nTest 4: GetMsg on empty port\n");
    receivedMsg = GetMsg(port1);
    if (receivedMsg == NULL) {
        test_ok("GetMsg on empty port returns NULL");
    } else {
        test_fail_msg("GetMsg should return NULL for empty port");
    }
    
    /* Test 5: Multiple messages - FIFO order */
    print("\nTest 5: Message queue FIFO order\n");
    
    /* Queue 3 messages */
    PutMsg(port1, (struct Message *)&msgs[0]);
    PutMsg(port1, (struct Message *)&msgs[1]);
    PutMsg(port1, (struct Message *)&msgs[2]);
    
    /* Receive them - should be in FIFO order */
    receivedMsg = GetMsg(port1);
    if (receivedMsg == (struct Message *)&msgs[0]) {
        test_ok("First message is msgs[0]");
    } else {
        test_fail_msg("FIFO order violated - first message");
    }
    
    receivedMsg = GetMsg(port1);
    if (receivedMsg == (struct Message *)&msgs[1]) {
        test_ok("Second message is msgs[1]");
    } else {
        test_fail_msg("FIFO order violated - second message");
    }
    
    receivedMsg = GetMsg(port1);
    if (receivedMsg == (struct Message *)&msgs[2]) {
        test_ok("Third message is msgs[2]");
    } else {
        test_fail_msg("FIFO order violated - third message");
    }
    
    /* Test 6: AddPort/FindPort/RemPort */
    print("\nTest 6: AddPort/FindPort/RemPort\n");
    
    port1->mp_Node.ln_Name = (char *)"TestPort.Exec.001";
    AddPort(port1);
    test_ok("AddPort completed");
    
    foundPort = FindPort((CONST_STRPTR)"TestPort.Exec.001");
    if (foundPort == port1) {
        test_ok("FindPort found the port");
    } else if (foundPort == NULL) {
        test_fail_msg("FindPort returned NULL");
    } else {
        test_fail_msg("FindPort found wrong port");
    }
    
    /* Test FindPort with non-existent name */
    foundPort = FindPort((CONST_STRPTR)"NonExistentPort.XYZ");
    if (foundPort == NULL) {
        test_ok("FindPort returns NULL for non-existent port");
    } else {
        test_fail_msg("FindPort should return NULL for non-existent");
    }
    
    RemPort(port1);
    test_ok("RemPort completed");
    
    /* Verify port is no longer findable */
    foundPort = FindPort((CONST_STRPTR)"TestPort.Exec.001");
    if (foundPort == NULL) {
        test_ok("Port not findable after RemPort");
    } else {
        test_fail_msg("Port still findable after RemPort");
    }
    
    /* Test 7: ReplyMsg with reply port */
    print("\nTest 7: ReplyMsg\n");
    
    /* Set up reply port */
    msgs[0].msg.mn_ReplyPort = port2;
    
    /* Send message to port1 */
    PutMsg(port1, (struct Message *)&msgs[0]);
    
    /* Receive and reply */
    receivedMsg = GetMsg(port1);
    if (receivedMsg != NULL) {
        ReplyMsg(receivedMsg);
        
        /* Check if reply arrived at port2 */
        receivedMsg = GetMsg(port2);
        if (receivedMsg == (struct Message *)&msgs[0]) {
            test_ok("ReplyMsg sent message to reply port");
        } else if (receivedMsg == NULL) {
            test_fail_msg("Reply message not found on reply port");
        } else {
            test_fail_msg("Wrong message on reply port");
        }
    } else {
        test_fail_msg("Could not get message for ReplyMsg test");
    }
    
    /* Test 8: DeleteMsgPort */
    print("\nTest 8: DeleteMsgPort\n");
    
    /* Clear the port name before deleting (not required but cleaner) */
    port1->mp_Node.ln_Name = NULL;
    
    DeleteMsgPort(port1);
    test_ok("DeleteMsgPort on port1 completed");
    
    DeleteMsgPort(port2);
    test_ok("DeleteMsgPort on port2 completed");
    
    /* Test 9: DeleteMsgPort(NULL) should not crash */
    print("\nTest 9: DeleteMsgPort(NULL) safety\n");
    DeleteMsgPort(NULL);
    test_ok("DeleteMsgPort(NULL) did not crash");
    
    /* Summary */
    print("\n=============================\n");
    print("Tests passed: ");
    print_num(test_pass);
    print("\n");
    print("Tests failed: ");
    print_num(test_fail);
    print("\n");
    
    if (test_fail == 0) {
        print("\nPASS: All message port tests passed!\n");
        return 0;
    } else {
        print("\nFAIL: Some tests failed\n");
        return 20;
    }
}
