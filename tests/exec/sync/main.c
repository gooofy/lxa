/*
 * Exec Semaphore Tests
 *
 * Tests:
 *   - InitSemaphore
 *   - ObtainSemaphore/ReleaseSemaphore
 *   - AttemptSemaphore
 *   - Nested semaphore acquisition
 *   - Basic semaphore semantics
 */

#include <exec/types.h>
#include <exec/memory.h>
#include <exec/ports.h>
#include <exec/semaphores.h>
#include <exec/tasks.h>
#include <dos/dos.h>
#include <dos/dostags.h>
#include <clib/exec_protos.h>
#include <clib/dos_protos.h>
#include <inline/exec.h>
#include <inline/dos.h>

extern struct ExecBase *SysBase;
extern struct DosLibrary *DOSBase;

static BPTR out;
static LONG test_pass = 0;
static LONG test_fail = 0;

#define QUEUE_TEST_PORT_NAME "Sync.Queue.Port"
#define QUEUE_TEST_SEM_NAME  "Sync.Queue.Sem"

#define QUEUE_MSG_READY    1
#define QUEUE_MSG_ACQUIRED 2
#define QUEUE_MSG_RELEASED 3

struct QueueMessage
{
    struct Message msg;
    LONG code;
};

/* Helper to print a string */
static void print(const char *s)
{
    LONG len = 0;
    const char *p = s;
    while (*p++) len++;
    Write(out, (CONST APTR)s, len);
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

static void queue_test_send(struct MsgPort *port, LONG code)
{
    struct QueueMessage *msg;

    msg = (struct QueueMessage *)AllocMem(sizeof(struct QueueMessage), MEMF_PUBLIC | MEMF_CLEAR);
    if (msg == NULL)
        return;

    msg->msg.mn_Node.ln_Type = NT_MESSAGE;
    msg->msg.mn_Length = sizeof(struct QueueMessage);
    msg->code = code;
    PutMsg(port, (struct Message *)msg);
}

static void SemaphoreQueueChild(void)
{
    struct MsgPort *port;
    struct SignalSemaphore *sem;

    Forbid();
    port = FindPort((STRPTR)QUEUE_TEST_PORT_NAME);
    Permit();

    if (port == NULL)
        return;

    queue_test_send(port, QUEUE_MSG_READY);

    sem = FindSemaphore((STRPTR)QUEUE_TEST_SEM_NAME);
    if (sem == NULL)
        return;

    ObtainSemaphore(sem);
    queue_test_send(port, QUEUE_MSG_ACQUIRED);

    Wait(SIGBREAKF_CTRL_C);

    ReleaseSemaphore(sem);
    queue_test_send(port, QUEUE_MSG_RELEASED);
}

int main(void)
{
    struct SignalSemaphore *sem;
    struct Task *thisTask;
    ULONG result;
    
    out = Output();
    
    print("Exec Semaphore Tests\n");
    print("====================\n\n");
    
    thisTask = FindTask(NULL);
    
    /* Allocate a semaphore */
    sem = (struct SignalSemaphore *)AllocMem(sizeof(struct SignalSemaphore), MEMF_PUBLIC | MEMF_CLEAR);
    if (sem == NULL) {
        print("FATAL: Could not allocate semaphore\n");
        return 20;
    }
    
    /* Test 1: InitSemaphore */
    print("Test 1: InitSemaphore\n");
    InitSemaphore(sem);
    
    /* Verify initialization */
    if (sem->ss_Link.ln_Type == NT_SIGNALSEM) {
        test_ok("Semaphore type set to NT_SIGNALSEM");
    } else {
        print("    Type is: ");
        print_num(sem->ss_Link.ln_Type);
        print(" (expected ");
        print_num(NT_SIGNALSEM);
        print(")\n");
        test_fail_msg("Semaphore type not set correctly");
    }
    
    if (sem->ss_NestCount == 0) {
        test_ok("NestCount initialized to 0");
    } else {
        test_fail_msg("NestCount not initialized to 0");
    }
    
    if (sem->ss_Owner == NULL) {
        test_ok("Owner initialized to NULL");
    } else {
        test_fail_msg("Owner not initialized to NULL");
    }
    
    if (sem->ss_QueueCount == -1) {
        test_ok("QueueCount initialized to -1");
    } else {
        print("    QueueCount is: ");
        print_num(sem->ss_QueueCount);
        print(" (expected -1)\n");
        test_fail_msg("QueueCount not initialized to -1");
    }
    
    /* Test 2: ObtainSemaphore */
    print("\nTest 2: ObtainSemaphore\n");
    ObtainSemaphore(sem);
    
    if (sem->ss_Owner == thisTask) {
        test_ok("Owner set to current task after Obtain");
    } else {
        test_fail_msg("Owner not set correctly");
    }
    
    if (sem->ss_NestCount == 1) {
        test_ok("NestCount is 1 after first Obtain");
    } else {
        print("    NestCount is: ");
        print_num(sem->ss_NestCount);
        print("\n");
        test_fail_msg("NestCount not incremented correctly");
    }
    
    if (sem->ss_QueueCount == 0) {
        test_ok("QueueCount is 0 after first Obtain");
    } else {
        print("    QueueCount is: ");
        print_num(sem->ss_QueueCount);
        print("\n");
        test_fail_msg("QueueCount not updated correctly");
    }
    
    /* Test 3: Nested ObtainSemaphore (same task) */
    print("\nTest 3: Nested ObtainSemaphore\n");
    ObtainSemaphore(sem);
    
    if (sem->ss_NestCount == 2) {
        test_ok("NestCount is 2 after nested Obtain");
    } else {
        print("    NestCount is: ");
        print_num(sem->ss_NestCount);
        print("\n");
        test_fail_msg("Nested Obtain did not increment NestCount");
    }
    
    if (sem->ss_Owner == thisTask) {
        test_ok("Owner still current task after nested Obtain");
    } else {
        test_fail_msg("Owner changed during nested Obtain");
    }
    
    /* Test 4: ReleaseSemaphore (nested) */
    print("\nTest 4: ReleaseSemaphore (nested)\n");
    ReleaseSemaphore(sem);
    
    if (sem->ss_NestCount == 1) {
        test_ok("NestCount decremented to 1");
    } else {
        print("    NestCount is: ");
        print_num(sem->ss_NestCount);
        print("\n");
        test_fail_msg("NestCount not decremented correctly");
    }
    
    if (sem->ss_Owner == thisTask) {
        test_ok("Owner still set (not fully released)");
    } else {
        test_fail_msg("Owner cleared too early");
    }
    
    /* Test 5: Final ReleaseSemaphore */
    print("\nTest 5: Final ReleaseSemaphore\n");
    ReleaseSemaphore(sem);
    
    if (sem->ss_NestCount == 0) {
        test_ok("NestCount decremented to 0");
    } else {
        test_fail_msg("NestCount not 0 after full release");
    }
    
    if (sem->ss_Owner == NULL) {
        test_ok("Owner cleared after full release");
    } else {
        test_fail_msg("Owner not cleared after full release");
    }
    
    if (sem->ss_QueueCount == -1) {
        test_ok("QueueCount back to -1");
    } else {
        print("    QueueCount is: ");
        print_num(sem->ss_QueueCount);
        print("\n");
        test_fail_msg("QueueCount not reset");
    }
    
    /* Test 6: AttemptSemaphore on free semaphore */
    print("\nTest 6: AttemptSemaphore (free semaphore)\n");
    result = AttemptSemaphore(sem);
    
    if (result != 0) {
        test_ok("AttemptSemaphore returned TRUE for free semaphore");
        
        if (sem->ss_Owner == thisTask) {
            test_ok("Semaphore now owned by current task");
        } else {
            test_fail_msg("Semaphore not owned after successful Attempt");
        }
        
        ReleaseSemaphore(sem);
        test_ok("Released semaphore after Attempt");
    } else {
        test_fail_msg("AttemptSemaphore returned FALSE for free semaphore");
    }
    
    /* Test 7: AttemptSemaphore on same-task-owned semaphore */
    print("\nTest 7: AttemptSemaphore (self-owned)\n");
    ObtainSemaphore(sem);
    
    result = AttemptSemaphore(sem);
    if (result != 0) {
        test_ok("AttemptSemaphore succeeds when same task owns it");
        
        if (sem->ss_NestCount == 2) {
            test_ok("NestCount incremented to 2");
        } else {
            test_fail_msg("NestCount not 2");
        }
        
        ReleaseSemaphore(sem);
    } else {
        test_fail_msg("AttemptSemaphore failed on self-owned semaphore");
    }
    
    ReleaseSemaphore(sem);
    
    /* Test 8: Shared semaphore semantics */
    print("\nTest 8: Shared semaphore semantics\n");
    ObtainSemaphoreShared(sem);

    if (sem->ss_Owner == NULL) {
        test_ok("Shared obtain leaves Owner as NULL");
    } else {
        test_fail_msg("Shared obtain should leave Owner as NULL");
    }

    if (sem->ss_NestCount == 1) {
        test_ok("Shared obtain sets NestCount to 1");
    } else {
        print("    NestCount is: ");
        print_num(sem->ss_NestCount);
        print("\n");
        test_fail_msg("Shared obtain did not set NestCount to 1");
    }

    if (sem->ss_QueueCount == 0) {
        test_ok("Shared obtain sets QueueCount to 0");
    } else {
        print("    QueueCount is: ");
        print_num(sem->ss_QueueCount);
        print("\n");
        test_fail_msg("Shared obtain did not set QueueCount to 0");
    }

    result = AttemptSemaphoreShared(sem);
    if (result != 0) {
        test_ok("AttemptSemaphoreShared succeeds on shared-owned semaphore");
    } else {
        test_fail_msg("AttemptSemaphoreShared failed on shared-owned semaphore");
    }

    if (sem->ss_NestCount == 2) {
        test_ok("Shared attempt increments NestCount");
    } else {
        print("    NestCount is: ");
        print_num(sem->ss_NestCount);
        print("\n");
        test_fail_msg("Shared attempt did not increment NestCount");
    }

    result = AttemptSemaphore(sem);
    if (result == 0) {
        test_ok("AttemptSemaphore fails while semaphore is shared-owned");
    } else {
        test_fail_msg("AttemptSemaphore should fail while shared-owned");
        ReleaseSemaphore(sem);
    }

    ReleaseSemaphore(sem);
    if (sem->ss_NestCount == 1 && sem->ss_Owner == NULL) {
        test_ok("Releasing one shared lock keeps semaphore shared-owned");
    } else {
        test_fail_msg("Releasing one shared lock left wrong semaphore state");
    }

    ReleaseSemaphore(sem);
    if (sem->ss_NestCount == 0 && sem->ss_Owner == NULL && sem->ss_QueueCount == -1) {
        test_ok("Final shared release resets semaphore state");
    } else {
        test_fail_msg("Final shared release did not reset semaphore state");
    }

    /* Test 9: AddSemaphore/FindSemaphore/RemSemaphore */
    print("\nTest 9: AddSemaphore/FindSemaphore/RemSemaphore\n");
    {
        static STRPTR semName = (STRPTR)"Sync.Public.Semaphore";
        struct SignalSemaphore *pubSem;

        pubSem = (struct SignalSemaphore *)AllocMem(sizeof(struct SignalSemaphore), MEMF_PUBLIC | MEMF_CLEAR);
        if (pubSem == NULL) {
            test_fail_msg("Could not allocate public semaphore");
        } else {
            pubSem->ss_Link.ln_Name = (char *)semName;
            pubSem->ss_NestCount = 7;
            pubSem->ss_Owner = thisTask;
            pubSem->ss_QueueCount = 3;

            AddSemaphore(pubSem);

            if (pubSem->ss_NestCount == 0 && pubSem->ss_Owner == NULL && pubSem->ss_QueueCount == -1) {
                test_ok("AddSemaphore initializes semaphore state");
            } else {
                test_fail_msg("AddSemaphore did not initialize semaphore state");
            }

            if (FindSemaphore(semName) == pubSem) {
                test_ok("FindSemaphore found added semaphore");
            } else {
                test_fail_msg("FindSemaphore did not find added semaphore");
            }

            RemSemaphore(pubSem);
            test_ok("RemSemaphore completed");

            if (FindSemaphore(semName) == NULL) {
                test_ok("FindSemaphore returns NULL after removal");
            } else {
                test_fail_msg("FindSemaphore still found removed semaphore");
            }

            FreeMem(pubSem, sizeof(struct SignalSemaphore));
        }
    }

    /* Test 10: Semaphore wait queue handoff */
    print("\nTest 10: Semaphore wait queue handoff\n");
    {
        struct SignalSemaphore *pubSem;
        struct MsgPort *queuePort;
        struct Process *childProc;
        struct QueueMessage *queueMsg;
        struct TagItem tags[] = {
            { NP_Entry, (ULONG)SemaphoreQueueChild },
            { NP_Name, (ULONG)"Sync.Queue.Child" },
            { NP_StackSize, 8192 },
            { TAG_DONE, 0 }
        };

        pubSem = (struct SignalSemaphore *)AllocMem(sizeof(struct SignalSemaphore), MEMF_PUBLIC | MEMF_CLEAR);
        queuePort = CreateMsgPort();

        if (pubSem == NULL || queuePort == NULL) {
            test_fail_msg("Could not allocate queue handoff resources");
            if (queuePort != NULL)
                DeleteMsgPort(queuePort);
            if (pubSem != NULL)
                FreeMem(pubSem, sizeof(struct SignalSemaphore));
        } else {
            pubSem->ss_Link.ln_Name = (char *)QUEUE_TEST_SEM_NAME;
            AddSemaphore(pubSem);
            ObtainSemaphore(pubSem);

            queuePort->mp_Node.ln_Name = (char *)QUEUE_TEST_PORT_NAME;
            AddPort(queuePort);

            childProc = CreateNewProc(tags);
            if (childProc == NULL) {
                test_fail_msg("CreateNewProc failed for queue handoff child");
            } else {
                WaitPort(queuePort);
                queueMsg = (struct QueueMessage *)GetMsg(queuePort);
                if (queueMsg != NULL && queueMsg->code == QUEUE_MSG_READY) {
                    test_ok("Child reached semaphore wait point");
                } else {
                    test_fail_msg("Child did not report ready state");
                }
                if (queueMsg != NULL)
                    FreeMem(queueMsg, sizeof(struct QueueMessage));

                if (pubSem->ss_QueueCount == 1) {
                    test_ok("Wait queue count increments for blocked child");
                } else {
                    test_fail_msg("Wait queue count did not increment for blocked child");
                }

                ReleaseSemaphore(pubSem);

                WaitPort(queuePort);
                queueMsg = (struct QueueMessage *)GetMsg(queuePort);
                if (queueMsg != NULL && queueMsg->code == QUEUE_MSG_ACQUIRED) {
                    test_ok("Blocked child acquired semaphore after release");
                } else {
                    test_fail_msg("Blocked child did not acquire semaphore");
                }
                if (queueMsg != NULL)
                    FreeMem(queueMsg, sizeof(struct QueueMessage));

                if (pubSem->ss_Owner == (struct Task *)childProc && pubSem->ss_NestCount == 1) {
                    test_ok("Semaphore ownership transferred to waiting child");
                } else {
                    test_fail_msg("Semaphore ownership did not transfer correctly");
                }

                Signal((struct Task *)childProc, SIGBREAKF_CTRL_C);

                WaitPort(queuePort);
                queueMsg = (struct QueueMessage *)GetMsg(queuePort);
                if (queueMsg != NULL && queueMsg->code == QUEUE_MSG_RELEASED) {
                    test_ok("Child released semaphore after signal");
                } else {
                    test_fail_msg("Child did not report semaphore release");
                }
                if (queueMsg != NULL)
                    FreeMem(queueMsg, sizeof(struct QueueMessage));

                if (pubSem->ss_Owner == NULL && pubSem->ss_NestCount == 0 && pubSem->ss_QueueCount == -1) {
                    test_ok("Semaphore reset after waiting child released it");
                } else {
                    test_fail_msg("Semaphore state not reset after waiting child release");
                }
            }

            RemPort(queuePort);
            DeleteMsgPort(queuePort);
            RemSemaphore(pubSem);
            FreeMem(pubSem, sizeof(struct SignalSemaphore));
        }
    }

    /* Test 11: Multiple obtain/release cycles */
    print("\nTest 11: Multiple obtain/release cycles\n");
    {
        int i;
        BOOL success = TRUE;
        
        for (i = 0; i < 10; i++) {
            ObtainSemaphore(sem);
            if (sem->ss_Owner != thisTask || sem->ss_NestCount != 1) {
                success = FALSE;
                break;
            }
            ReleaseSemaphore(sem);
            if (sem->ss_Owner != NULL || sem->ss_NestCount != 0) {
                success = FALSE;
                break;
            }
        }
        
        if (success) {
            test_ok("10 obtain/release cycles completed correctly");
        } else {
            test_fail_msg("Obtain/release cycle failed");
        }
    }
    
    /* Test 12: InitSemaphore(NULL) should not crash */
    print("\nTest 12: InitSemaphore(NULL) safety\n");
    InitSemaphore(NULL);
    test_ok("InitSemaphore(NULL) did not crash");
    
    /* Test 13: ObtainSemaphore(NULL) should not crash */
    print("\nTest 13: ObtainSemaphore(NULL) safety\n");
    ObtainSemaphore(NULL);
    test_ok("ObtainSemaphore(NULL) did not crash");
    
    /* Test 14: ReleaseSemaphore(NULL) should not crash */
    print("\nTest 14: ReleaseSemaphore(NULL) safety\n");
    ReleaseSemaphore(NULL);
    test_ok("ReleaseSemaphore(NULL) did not crash");
    
    /* Test 15: Shared semaphore NULL safety */
    print("\nTest 15: Shared semaphore NULL safety\n");
    ObtainSemaphoreShared(NULL);
    test_ok("ObtainSemaphoreShared(NULL) did not crash");

    result = AttemptSemaphoreShared(NULL);
    if (result == 0) {
        test_ok("AttemptSemaphoreShared(NULL) returns FALSE");
    } else {
        test_fail_msg("AttemptSemaphoreShared(NULL) should return FALSE");
    }

    /* Test 16: AttemptSemaphore(NULL) should return FALSE */
    print("\nTest 16: AttemptSemaphore(NULL)\n");
    result = AttemptSemaphore(NULL);
    if (result == 0) {
        test_ok("AttemptSemaphore(NULL) returns FALSE");
    } else {
        test_fail_msg("AttemptSemaphore(NULL) should return FALSE");
    }
    
    /* Cleanup */
    FreeMem(sem, sizeof(struct SignalSemaphore));
    
    /* Summary */
    print("\n=============================\n");
    print("Tests passed: ");
    print_num(test_pass);
    print("\n");
    print("Tests failed: ");
    print_num(test_fail);
    print("\n");
    
    if (test_fail == 0) {
        print("\nPASS: All semaphore tests passed!\n");
        return 0;
    } else {
        print("\nFAIL: Some tests failed\n");
        return 20;
    }
}
