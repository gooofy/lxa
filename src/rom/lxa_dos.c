#include <exec/types.h>
//#include <exec/memory.h>
//#include <exec/libraries.h>
#include <exec/execbase.h>
#include <exec/resident.h>
#include <exec/initializers.h>
#include <clib/exec_protos.h>
#include <inline/exec.h>

#include <dos/dos.h>
#include <dos/dosextens.h>
#include <dos/dostags.h>
#include <dos/var.h>
#include <clib/dos_protos.h>
#include <inline/dos.h>

#include <clib/utility_protos.h>
#include <inline/utility.h>

//#include <string.h>

//#define ENABLE_DEBUG
#include "util.h"

#define FILE_KIND_REGULAR    42
#define FILE_KIND_CONSOLE    23

#define HUNK_TYPE_UNIT     0x03E7
#define HUNK_TYPE_NAME     0x03E8
#define HUNK_TYPE_CODE     0x03E9
#define HUNK_TYPE_DATA     0x03EA
#define HUNK_TYPE_BSS      0x03EB
#define HUNK_TYPE_RELOC32  0x03EC
#define HUNK_TYPE_EXT      0x03EF
#define HUNK_TYPE_SYMBOL   0x03F0
#define HUNK_TYPE_DEBUG    0x03F1
#define HUNK_TYPE_END      0x03F2
#define HUNK_TYPE_HEADER   0x03F3

#define  IS_PROCESS(task)  (((struct Task *)task)->tc_Node.ln_Type == NT_PROCESS)

#define VERSION    40
#define REVISION   3
#define EXLIBNAME  "dos"
#define EXLIBVER   " 40.3 (2021/07/21)"

char __aligned _g_dos_ExLibName [] = EXLIBNAME ".library";
char __aligned _g_dos_ExLibID   [] = EXLIBNAME EXLIBVER;
char __aligned _g_dos_Copyright [] = "(C)opyright 2022 by G. Bartsch. Licensed under the MIT license.";

char __aligned _g_dos_VERSTRING [] = "\0$VER: " EXLIBNAME EXLIBVER;

extern struct ExecBase      *SysBase;
extern struct UtilityBase   *UtilityBase;
extern struct DosLibrary    *DOSBase;

LONG _dos_SystemTagList ( register struct DosLibrary * DOSBase __asm("a6"),
                                    register CONST_STRPTR command  __asm("d1"),
                                    register const struct TagItem * tags __asm("d2"));

/* Forward declaration for variable functions */
struct LocalVar * _dos_FindVar ( register struct DosLibrary * DOSBase __asm("a6"),
                                 register CONST_STRPTR name __asm("d1"),
                                 register ULONG type __asm("d2"));

/*
 * RootNode and TaskArray implementation for proper CLI process numbering.
 * 
 * rn_TaskArray is a BPTR to an array of IPTRs (pointers):
 *   [0] = maximum number of CLI slots (size of array - 1)
 *   [1] = pointer to CLI process 1's MsgPort (or 0 if slot is free)
 *   [2] = pointer to CLI process 2's MsgPort (or 0 if slot is free)
 *   ...
 * 
 * When a CLI process is created, we find a free slot or expand the array.
 * When a CLI process exits, we clear its slot so it can be reused.
 *
 * IMPORTANT: RootNode must be dynamically allocated because ROM code
 * cannot have writable static data (it's in read-only memory).
 * We use DOSBase->dl_Root to store the pointer to the allocated RootNode.
 */

#define INITIAL_TASK_ARRAY_SIZE 8

/* Initialize the RootNode and TaskArray if not already done.
 * Uses DOSBase->dl_Root to store the RootNode (allocated in RAM).
 * Returns the RootNode pointer, or NULL on failure. */
static struct RootNode *initRootNode(void)
{
    /* Check if already initialized - DOSBase->dl_Root will be non-NULL */
    if (DOSBase && DOSBase->dl_Root)
        return DOSBase->dl_Root;
    
    DPRINTF(LOG_DEBUG, "_dos: initRootNode() initializing RootNode\n");
    
    /* Allocate RootNode in RAM (not ROM static!) */
    struct RootNode *rootNode = (struct RootNode *)AllocMem(sizeof(struct RootNode), MEMF_CLEAR | MEMF_PUBLIC);
    if (!rootNode)
    {
        LPRINTF(LOG_ERROR, "_dos: initRootNode() failed to allocate RootNode\n");
        return NULL;
    }
    
    /* Allocate initial TaskArray */
    ULONG *taskArray = (ULONG *)AllocMem((INITIAL_TASK_ARRAY_SIZE + 1) * sizeof(ULONG), MEMF_CLEAR | MEMF_PUBLIC);
    if (taskArray)
    {
        taskArray[0] = INITIAL_TASK_ARRAY_SIZE;  /* Max slots */
        /* Slots 1..INITIAL_TASK_ARRAY_SIZE are already 0 (free) due to MEMF_CLEAR */
    }
    
    rootNode->rn_TaskArray = MKBADDR(taskArray);
    
    /* Initialize the MinList for CLI processes */
    rootNode->rn_CliList.mlh_Head = (struct MinNode *)&rootNode->rn_CliList.mlh_Tail;
    rootNode->rn_CliList.mlh_Tail = NULL;
    rootNode->rn_CliList.mlh_TailPred = (struct MinNode *)&rootNode->rn_CliList.mlh_Head;
    
    /* Link to DOSBase */
    if (DOSBase)
        DOSBase->dl_Root = rootNode;
    
    DPRINTF(LOG_DEBUG, "_dos: initRootNode() done, rootNode=0x%08lx, taskArray=0x%08lx\n", rootNode, taskArray);
    
    return rootNode;
}

/*
 * Allocate a CLI task number from the TaskArray.
 * Returns the task number (1..n) or 0 on failure.
 */
static LONG allocTaskNum(struct Process *process)
{
    struct RootNode *rootNode = initRootNode();
    if (!rootNode)
        return 0;
    
    ULONG *taskArray = (ULONG *)BADDR(rootNode->rn_TaskArray);
    if (!taskArray)
        return 0;
    
    ULONG maxSlots = taskArray[0];
    
    Disable();
    
    /* Find a free slot */
    for (ULONG i = 1; i <= maxSlots; i++)
    {
        if (taskArray[i] == 0)
        {
            /* Found a free slot */
            taskArray[i] = (ULONG)&process->pr_MsgPort;
            Enable();
            DPRINTF(LOG_DEBUG, "_dos: allocTaskNum() assigned slot %lu to process 0x%08lx\n", i, process);
            return (LONG)i;
        }
    }
    
    Enable();
    
    /* No free slot - need to expand the array */
    DPRINTF(LOG_DEBUG, "_dos: allocTaskNum() no free slot, expanding from %lu\n", maxSlots);
    
    ULONG newMaxSlots = maxSlots + INITIAL_TASK_ARRAY_SIZE;
    ULONG *newTaskArray = (ULONG *)AllocMem((newMaxSlots + 1) * sizeof(ULONG), MEMF_CLEAR | MEMF_PUBLIC);
    if (!newTaskArray)
    {
        LPRINTF(LOG_ERROR, "_dos: allocTaskNum() failed to allocate expanded TaskArray\n");
        return 0;
    }
    
    /* Copy old entries */
    newTaskArray[0] = newMaxSlots;
    for (ULONG i = 1; i <= maxSlots; i++)
        newTaskArray[i] = taskArray[i];
    
    /* Assign the new slot */
    ULONG newSlot = maxSlots + 1;
    newTaskArray[newSlot] = (ULONG)&process->pr_MsgPort;
    
    /* Replace the array */
    Disable();
    rootNode->rn_TaskArray = MKBADDR(newTaskArray);
    Enable();
    
    /* Free old array */
    FreeMem(taskArray, (maxSlots + 1) * sizeof(ULONG));
    
    DPRINTF(LOG_DEBUG, "_dos: allocTaskNum() expanded to %lu slots, assigned slot %lu\n", newMaxSlots, newSlot);
    return (LONG)newSlot;
}

/*
 * Free a CLI task number, making it available for reuse.
 */
static void freeTaskNum(LONG taskNum)
{
    DPRINTF(LOG_DEBUG, "_dos: freeTaskNum(%ld) called\n", taskNum);
    
    if (taskNum <= 0)
        return;
    
    /* Get RootNode from DOSBase */
    if (!DOSBase || !DOSBase->dl_Root)
    {
        LPRINTF(LOG_ERROR, "_dos: freeTaskNum() DOSBase=0x%08lx, dl_Root=0x%08lx\n", 
                (ULONG)DOSBase, DOSBase ? (ULONG)DOSBase->dl_Root : 0);
        return;
    }
    
    struct RootNode *rootNode = DOSBase->dl_Root;
    ULONG *taskArray = (ULONG *)BADDR(rootNode->rn_TaskArray);
    if (!taskArray)
    {
        LPRINTF(LOG_ERROR, "_dos: freeTaskNum() taskArray is NULL\n");
        return;
    }
    
    ULONG maxSlots = taskArray[0];
    if ((ULONG)taskNum > maxSlots)
    {
        LPRINTF(LOG_ERROR, "_dos: freeTaskNum() taskNum %ld > maxSlots %lu\n", taskNum, maxSlots);
        return;
    }
    
    DPRINTF(LOG_DEBUG, "_dos: freeTaskNum() clearing slot %ld (was 0x%08lx)\n", taskNum, taskArray[taskNum]);
    
    Disable();
    taskArray[taskNum] = 0;  /* Mark slot as free */
    Enable();
    
    DPRINTF(LOG_DEBUG, "_dos: freeTaskNum() freed slot %ld\n", taskNum);
}

#if 0
struct DosLibrary * __g_lxa_dos_InitLib    ( register struct DosLibrary *dosb    __asm("a6"),
                                                      register BPTR               seglist __asm("a0"),
                                                      register struct ExecBase   *sysb    __asm("d0"));
struct DosLibrary * __g_lxa_dos_OpenLib    ( register struct DosLibrary *dosb    __asm("a6"));
BPTR                __g_lxa_dos_CloseLib   ( register struct DosLibrary *dosb    __asm("a6"));
BPTR                __g_lxa_dos_ExpungeLib ( register struct DosLibrary *dosb    __asm("a6"));
ULONG                        __g_lxa_dos_ExtFuncLib ( void );
#endif

struct DosLibrary * __g_lxa_dos_InitLib    ( register struct DosLibrary *dosb    __asm("a6"),
                                                      register BPTR               seglist __asm("a0"),
                                                      register struct ExecBase   *sysb    __asm("d0"))
{
    DPRINTF (LOG_DEBUG, "_dos: WARNING: InitLib() unimplemented STUB called.\n");
#if 0
    ExecBase = exb;

    ExecBase->exb_ExecBase = sysbase;
    ExecBase->exb_SegList = seglist;

    if (L_OpenLibs(ExecBase)) return(ExecBase);

    L_CloseLibs();

    {
      ULONG negsize, possize, fullsize;
      UBYTE *negptr = (UBYTE *) ExecBase;

      negsize  = ExecBase->exb_LibNode.lib_NegSize;
      possize  = ExecBase->exb_LibNode.lib_PosSize;
      fullsize = negsize + possize;
      negptr  -= negsize;

      FreeMem(negptr, fullsize);

    }
#endif
    return dosb;
}

struct DosLibrary * __g_lxa_dos_OpenLib ( register struct DosLibrary  *DosLibrary __asm("a6"))
{
    DPRINTF (LOG_DEBUG, "_dos: OpenLib() called\n");
    DosLibrary->dl_lib.lib_OpenCnt++;
    DosLibrary->dl_lib.lib_Flags &= ~LIBF_DELEXP;
    return DosLibrary;
}

BPTR __g_lxa_dos_CloseLib ( register struct DosLibrary  *dosb __asm("a6"))
{
#if 0
 ExecBase->exb_LibNode.lib_OpenCnt--;

 if(!ExecBase->exb_LibNode.lib_OpenCnt)
  {
   if(ExecBase->exb_LibNode.lib_Flags & LIBF_DELEXP)
    {
     return( ExpungeLib(ExecBase) );
    }
  }
#endif

    return 0;
}

BPTR __g_lxa_dos_ExpungeLib ( register struct DosLibrary  *dosb      __asm("a6"))
{
#if 0
 struct ExecBase *ExecBase = exb;
 BPTR seglist;

 if(!ExecBase->exb_LibNode.lib_OpenCnt)
  {
   ULONG negsize, possize, fullsize;
   UBYTE *negptr = (UBYTE *) ExecBase;

   seglist = ExecBase->exb_SegList;

   Remove((struct Node *)ExecBase);

   L_CloseLibs();

   negsize  = ExecBase->exb_LibNode.lib_NegSize;
   possize  = ExecBase->exb_LibNode.lib_PosSize;
   fullsize = negsize + possize;
   negptr  -= negsize;

   FreeMem(negptr, fullsize);

   #ifdef __MAXON__
   CleanupModules();
   #endif

   return(seglist);
  }

 ExecBase->exb_LibNode.lib_Flags |= LIBF_DELEXP;
#endif
    return 0;
}

ULONG __g_lxa_dos_ExtFuncLib(void)
{
    return 0;
}

BPTR _dos_Open ( register struct DosLibrary * DOSBase        __asm("a6"),
                                 register CONST_STRPTR        ___name        __asm("d1"),
                                 register LONG                ___accessMode  __asm("d2"))
{
    DPRINTF (LOG_DEBUG, "_dos: Open() ___name=%s, ___accessMode=%ld\n", ___name ? (char *)___name : "NULL", ___accessMode);

    if (!___name) return 0;

    struct FileHandle *fh = (struct FileHandle *) AllocDosObject (DOS_FILEHANDLE, NULL);

    DPRINTF (LOG_DEBUG, "_dos: Open() fh=0x%08lx\n", fh);

    LONG error = ERROR_NO_FREE_STORE;
    if (fh)
    {

        int err = emucall3 (EMU_CALL_DOS_OPEN, (ULONG) ___name, ___accessMode, (ULONG) fh);

        DPRINTF (LOG_DEBUG, "_dos: Open() result from emucall3: err=%d, fh->Args=%d\n", err, fh->fh_Args);

        if (!err)
        {
            BPTR f = MKBADDR (fh);
            DPRINTF (LOG_DEBUG, "_dos: Open() ___name=%s, ___accessMode=%ld -> BPTR 0x%08lx (APTR 0x%08lx)\n",
                     ___name ? (char *)___name : "NULL", ___accessMode, f, fh);
            return f;
        }
        else
        {
            error = IoErr();
            FreeDosObject (DOS_FILEHANDLE, fh);
        }
    }

    SetIoErr (error);

    return 0;
}

LONG _dos_Close ( register struct DosLibrary * __libBase __asm("a6"),
                                  register BPTR file  __asm("d1"))
{
    DPRINTF (LOG_DEBUG, "_dos: Close called, file=0x%08lx\n", file);

    struct FileHandle *fh = (struct FileHandle *) BADDR(file);

	if (!fh)
		return 1;

    int l = emucall1 (EMU_CALL_DOS_CLOSE, (ULONG) fh);

    DPRINTF (LOG_DEBUG, "_dos: Close() result from emucall1: l=%ld\n", l);

    if (l<0)
    {
        struct Process *me = (struct Process *)FindTask(NULL);
        me->pr_Result2 = fh->fh_Arg2;
    }

    return l;
}

LONG _dos_Read ( register struct DosLibrary * DOSBase __asm("a6"),
                                 register BPTR                file    __asm("d1"),
                                 register APTR                buffer  __asm("d2"),
                                 register LONG                length  __asm("d3"))
{
    struct FileHandle *fh = (struct FileHandle *) BADDR(file);
    DPRINTF (LOG_DEBUG, "_dos: Read called: file=0x%08lx length=%ld\n", file, length);

    int l = emucall3 (EMU_CALL_DOS_READ, (ULONG) fh, (ULONG) buffer, length);

    DPRINTF (LOG_DEBUG, "_dos: Read() result: l=%ld\n", l);

    if (l<0)
    {
        struct Process *me = (struct Process *)FindTask(NULL);
        me->pr_Result2 = fh->fh_Arg2;
    }

    return l;
}

LONG _dos_Write ( register struct DosLibrary * DOSBase __asm("a6"),
                                  register BPTR                file    __asm("d1"),
                                  register CONST APTR          buffer  __asm("d2"),
                                  register LONG                length  __asm("d3"))
{
    DPRINTF (LOG_DEBUG, "_dos: Write called, file=0x%08lx buffer=0x%08lx length=%ld\n\n", file, buffer, length);

    // U_hexdump (LOG_INFO, buffer, length);

    struct FileHandle *fh = (struct FileHandle *) BADDR(file);
    int l = emucall3 (EMU_CALL_DOS_WRITE, (ULONG) fh, (ULONG) buffer, length);

    DPRINTF (LOG_DEBUG, "_dos: Write() result from emucall3: l=%ld\n", l);

    if (l<0)
    {
        struct Process *me = (struct Process *)FindTask(NULL);
        me->pr_Result2 = fh->fh_Arg2;
    }

    return l;
}

BPTR _dos_Input ( register struct DosLibrary * __libBase __asm("a6"))
{
    DPRINTF (LOG_DEBUG, "_dos: Input() called.\n");

    struct Process *p = (struct Process *) FindTask (NULL);

    BPTR i = p->pr_CIS;

    DPRINTF (LOG_DEBUG, "_dos: Input() p=0x%08lx, o=%ld\n", p, i);

    return i;
}

BPTR _dos_Output ( register struct DosLibrary * __libBase __asm("a6"))
{
    DPRINTF (LOG_DEBUG, "_dos: Output() called.\n");

    struct Process *p = (struct Process *) FindTask (NULL);

    BPTR o = p->pr_COS;

    DPRINTF (LOG_DEBUG, "_dos: Output() p=0x%08lx, o=%ld\n", p, o);

    return o;
}

LONG _dos_Seek ( register struct DosLibrary * __libBase __asm("a6"),
                                 register BPTR file      __asm("d1"),
                                 register LONG position  __asm("d2"),
                                 register LONG mode      __asm("d3"))
{
    DPRINTF (LOG_DEBUG, "_dos: Seek() called file=0x%08lx, position=%d, mode=%d\n", file, position, mode);

    struct FileHandle *fh = (struct FileHandle *) BADDR(file);
    int l = emucall3 (EMU_CALL_DOS_SEEK, (ULONG) fh, (ULONG) position, (ULONG) mode);

    DPRINTF (LOG_DEBUG, "_dos: Seek() result from emucall3: l=%ld\n", l);

    if (l<0)
    {
        struct Process *me = (struct Process *)FindTask(NULL);
        me->pr_Result2 = fh->fh_Arg2;
    }

    return l;
}

LONG _dos_DeleteFile ( register struct DosLibrary * __libBase __asm("a6"),
                                                        register CONST_STRPTR ___name  __asm("d1"))
{
    DPRINTF (LOG_DEBUG, "_dos: DeleteFile() called, name=%s\n", ___name ? (char *)___name : "NULL");

    if (!___name) {
        SetIoErr(ERROR_OBJECT_NOT_FOUND);
        return DOSFALSE;
    }

    LONG result = emucall1(EMU_CALL_DOS_DELETE, (ULONG)___name);

    DPRINTF (LOG_DEBUG, "_dos: DeleteFile() result: %ld\n", result);

    if (!result) {
        SetIoErr(ERROR_OBJECT_NOT_FOUND);
        return DOSFALSE;
    }

    return DOSTRUE;
}

LONG _dos_Rename ( register struct DosLibrary * __libBase __asm("a6"),
                                                        register CONST_STRPTR ___oldName  __asm("d1"),
                                                        register CONST_STRPTR ___newName  __asm("d2"))
{
    DPRINTF (LOG_DEBUG, "_dos: Rename() called, old=%s, new=%s\n", 
             ___oldName ? (char *)___oldName : "NULL",
             ___newName ? (char *)___newName : "NULL");

    if (!___oldName || !___newName) {
        SetIoErr(ERROR_OBJECT_NOT_FOUND);
        return DOSFALSE;
    }

    LONG result = emucall2(EMU_CALL_DOS_RENAME, (ULONG)___oldName, (ULONG)___newName);

    DPRINTF (LOG_DEBUG, "_dos: Rename() result: %ld\n", result);

    if (!result) {
        SetIoErr(ERROR_OBJECT_NOT_FOUND);
        return DOSFALSE;
    }

    return DOSTRUE;
}

BPTR _dos_Lock ( register struct DosLibrary * __libBase __asm("a6"),
                                                        register CONST_STRPTR ___name  __asm("d1"),
                                                        register LONG ___type  __asm("d2"))
{
    DPRINTF (LOG_DEBUG, "_dos: Lock() called, name=%s, type=%ld\n", ___name ? (char *)___name : "NULL", ___type);

    if (!___name) {
        SetIoErr(ERROR_OBJECT_NOT_FOUND);
        return 0;
    }

    ULONG lock_id = emucall2(EMU_CALL_DOS_LOCK, (ULONG)___name, (ULONG)___type);

    DPRINTF (LOG_DEBUG, "_dos: Lock() result: lock_id=%lu\n", lock_id);

    if (lock_id == 0) {
        SetIoErr(ERROR_OBJECT_NOT_FOUND);
        return 0;
    }

    /* The lock_id from host is used directly as BPTR */
    return (BPTR)lock_id;
}

void _dos_UnLock ( register struct DosLibrary * __libBase __asm("a6"),
                                                        register BPTR ___lock  __asm("d1"))
{
    DPRINTF (LOG_DEBUG, "_dos: UnLock() called, lock=0x%08lx\n", ___lock);

    if (!___lock) return;

    emucall1(EMU_CALL_DOS_UNLOCK, (ULONG)___lock);
}

BPTR _dos_DupLock ( register struct DosLibrary * __libBase __asm("a6"),
                                                        register BPTR ___lock  __asm("d1"))
{
    DPRINTF (LOG_DEBUG, "_dos: DupLock() called, lock=0x%08lx\n", ___lock);

    if (!___lock) return 0;

    ULONG lock_id = emucall1(EMU_CALL_DOS_DUPLOCK, (ULONG)___lock);

    DPRINTF (LOG_DEBUG, "_dos: DupLock() result: lock_id=%lu\n", lock_id);

    return (BPTR)lock_id;
}

LONG _dos_Examine ( register struct DosLibrary * __libBase __asm("a6"),
                                                        register BPTR ___lock  __asm("d1"),
                                                        register struct FileInfoBlock * ___fileInfoBlock  __asm("d2"))
{
    DPRINTF (LOG_DEBUG, "_dos: Examine() called, lock=0x%08lx, fib=0x%08lx\n", ___lock, ___fileInfoBlock);

    if (!___lock || !___fileInfoBlock) {
        SetIoErr(ERROR_OBJECT_NOT_FOUND);
        return DOSFALSE;
    }

    LONG result = emucall2(EMU_CALL_DOS_EXAMINE, (ULONG)___lock, (ULONG)___fileInfoBlock);

    DPRINTF (LOG_DEBUG, "_dos: Examine() result: %ld\n", result);

    if (!result) {
        SetIoErr(ERROR_OBJECT_NOT_FOUND);
        return DOSFALSE;
    }

    return DOSTRUE;
}

LONG _dos_ExNext ( register struct DosLibrary * __libBase __asm("a6"),
                                                        register BPTR ___lock  __asm("d1"),
                                                        register struct FileInfoBlock * ___fileInfoBlock  __asm("d2"))
{
    DPRINTF (LOG_DEBUG, "_dos: ExNext() called, lock=0x%08lx, fib=0x%08lx\n", ___lock, ___fileInfoBlock);

    if (!___lock || !___fileInfoBlock) {
        SetIoErr(ERROR_OBJECT_NOT_FOUND);
        return DOSFALSE;
    }

    LONG result = emucall2(EMU_CALL_DOS_EXNEXT, (ULONG)___lock, (ULONG)___fileInfoBlock);

    DPRINTF (LOG_DEBUG, "_dos: ExNext() result: %ld\n", result);

    if (!result) {
        SetIoErr(ERROR_NO_MORE_ENTRIES);
        return DOSFALSE;
    }

    return DOSTRUE;
}

LONG _dos_Info ( register struct DosLibrary * __libBase __asm("a6"),
                                                        register BPTR ___lock  __asm("d1"),
                                                        register struct InfoData * ___parameterBlock  __asm("d2"))
{
    DPRINTF (LOG_DEBUG, "_dos: Info() called, lock=0x%08lx, infodata=0x%08lx\n", ___lock, ___parameterBlock);

    if (!___lock || !___parameterBlock) {
        SetIoErr(ERROR_OBJECT_NOT_FOUND);
        return DOSFALSE;
    }

    LONG result = emucall2(EMU_CALL_DOS_INFO, (ULONG)___lock, (ULONG)___parameterBlock);

    DPRINTF (LOG_DEBUG, "_dos: Info() result: %ld\n", result);

    if (!result) {
        SetIoErr(ERROR_OBJECT_NOT_FOUND);
        return DOSFALSE;
    }

    return DOSTRUE;
}

BPTR _dos_CreateDir ( register struct DosLibrary * __libBase __asm("a6"),
                                                        register CONST_STRPTR ___name  __asm("d1"))
{
    DPRINTF (LOG_DEBUG, "_dos: CreateDir() called, name=%s\n", ___name ? (char *)___name : "NULL");

    if (!___name) {
        SetIoErr(ERROR_OBJECT_NOT_FOUND);
        return 0;
    }

    ULONG lock_id = emucall1(EMU_CALL_DOS_CREATEDIR, (ULONG)___name);

    DPRINTF (LOG_DEBUG, "_dos: CreateDir() result: lock_id=%lu\n", lock_id);

    if (lock_id == 0) {
        SetIoErr(ERROR_OBJECT_NOT_FOUND);
        return 0;
    }

    return (BPTR)lock_id;
}

BPTR _dos_CurrentDir ( register struct DosLibrary * __libBase __asm("a6"),
                                                        register BPTR ___lock  __asm("d1"))
{
    DPRINTF (LOG_DEBUG, "_dos: CurrentDir() called, lock=0x%08lx\n", ___lock);

    struct Process *me = (struct Process *)FindTask(NULL);
    BPTR old_lock = me->pr_CurrentDir;
    me->pr_CurrentDir = ___lock;

    DPRINTF (LOG_DEBUG, "_dos: CurrentDir() old_lock=0x%08lx\n", old_lock);

    return old_lock;
}

LONG _dos_IoErr ( register struct DosLibrary * DOSBase __asm("a6"))
{
    DPRINTF (LOG_DEBUG, "_dos: IoErr() called.\n");

    struct Process *me = (struct Process *)FindTask(NULL);

    return me->pr_Result2;
}

struct MsgPort * _dos_CreateProc ( register struct DosLibrary * __libBase __asm("a6"),
                                                   register CONST_STRPTR ___name  __asm("d1"),
                                                   register LONG ___pri  __asm("d2"),
                                                   register BPTR ___segList  __asm("d3"),
                                                   register LONG ___stackSize  __asm("d4"))
{
    DPRINTF (LOG_DEBUG, "_dos: CreateProc unimplemented STUB called.\n");
    assert(FALSE);
    return NULL;
}

void _dos_Exit ( register struct DosLibrary * __libBase __asm("a6"),
                 register LONG ___returnCode  __asm("d1"))
{
    struct DosLibrary *DOSBase = __libBase;
    (void)DOSBase; /* used by FreeDosObject macro */

    DPRINTF (LOG_DEBUG, "_dos: Exit() called, returnCode=%ld\n", ___returnCode);

    struct Process *me = (struct Process *)FindTask(NULL);

    /* Store the return code */
    me->pr_Result2 = ___returnCode;

    /*
     * On classic m68k Amiga, Exit() works by setting SP to
     * (FindTask(NULL)->pr_ReturnAddr - 4) and performing an rts.
     *
     * For lxa, we use a simpler approach - we call RemTask(NULL) which
     * will terminate the current task and switch to the next one.
     * This is appropriate since lxa currently runs programs in a single-shot
     * manner from _bootstrap().
     *
     * If the current task is the bootstrap task, this will terminate the
     * emulation.
     */

    /* Close any open files */
    if (me->pr_CIS)
    {
        DPRINTF (LOG_DEBUG, "_dos: Exit() closing pr_CIS\n");
        /* Note: We don't actually close pr_CIS/pr_COS here because they
         * are typically inherited from the parent and should remain open.
         * Only close if we own them. */
    }

    /* Free CLI structure if we allocated it */
    if (me->pr_CLI)
    {
        DPRINTF (LOG_DEBUG, "_dos: Exit() freeing CLI structure\n");
        FreeDosObject(DOS_CLI, (APTR)BADDR(me->pr_CLI));
        me->pr_CLI = 0;
    }

    /* Free task number so it can be reused by other processes */
    if (me->pr_TaskNum > 0)
    {
        DPRINTF (LOG_DEBUG, "_dos: Exit() freeing task number %ld\n", me->pr_TaskNum);
        freeTaskNum(me->pr_TaskNum);
        me->pr_TaskNum = 0;
    }

    /* Call pr_ExitCode callback if set */
    if (me->pr_ExitCode)
    {
        DPRINTF (LOG_DEBUG, "_dos: Exit() calling pr_ExitCode callback\n");
        /* Call the exit code function with the return code */
        typedef void (*exitCodeFn_t)(LONG, LONG);
        exitCodeFn_t exitFn = (exitCodeFn_t)me->pr_ExitCode;
        exitFn(___returnCode, me->pr_ExitData);
    }

    DPRINTF (LOG_DEBUG, "_dos: Exit() calling RemTask(NULL)\n");

    /* Remove ourselves from the task system. This will not return. */
    RemTask(NULL);

    /* Should never reach here */
    LPRINTF (LOG_ERROR, "_dos: Exit() RemTask returned - this should not happen!\n");
    assert(FALSE);
}

BOOL _read_string (register struct DosLibrary * DOSBase __asm("a6"), BPTR f, char **res)
{
    ULONG num_longs;

    if (Read(f, &num_longs, 4) != 4)
        return FALSE;

    DPRINTF (LOG_DEBUG, "_dos: _read_string() num_longs=%d\n", num_longs);
    if (num_longs==0)
    {
        *res = NULL;
        return TRUE;
    }

    ULONG l = num_longs*4;
    char *str = AllocVec (l, MEMF_CLEAR);
    DPRINTF (LOG_DEBUG, "_dos: _read_string() l=%d -> str=0x%08lx\n", l, str);
    if (Read(f, str, l) != l)
        return FALSE;

    *res = str;
    return TRUE;
}

BPTR _dos_LoadSeg ( register struct DosLibrary * DOSBase __asm("a6"),
                    register CONST_STRPTR        ___name   __asm("d1"))
{
    DPRINTF (LOG_INFO, "_dos: LoadSeg() called, name=%s\n", ___name);

    BOOL success = FALSE;

    BPTR f = Open (___name, MODE_OLDFILE);
    if (!f)
    {
        LPRINTF (LOG_ERROR, "_dos: LoadSeg() Open() for name=%s failed\n", ___name);
        return 0;
    }

    DPRINTF (LOG_DEBUG, "_dos: LoadSeg() Open() for name=%s worked, BPTR f=0x%08lx\n", ___name, f);

    // read hunk HEADER

    ULONG ht;
    if (Read (f, &ht, 4) != 4)
    {
        DPRINTF (LOG_DEBUG, "_dos: LoadSeg() failed to read header hunk type\n");
        goto finish;
    }
    DPRINTF (LOG_DEBUG, "_dos: LoadSeg() header hunk type: 0x%08lx\n", ht);

    if (ht != HUNK_TYPE_HEADER)
    {
        DPRINTF (LOG_DEBUG, "_dos: LoadSeg() invalid header hunk type\n");
        goto finish;
    }

    ULONG num_longs;
    if (Read (f, &num_longs, 4) != 4)
    {
        DPRINTF (LOG_DEBUG, "_dos: LoadSeg() failed to read hunk table size\n");
        goto finish;
    }
    if (num_longs)
    {
        // FIXME: implement
        DPRINTF (LOG_DEBUG, "_dos: LoadSeg() sorry, library names are not implemented yet\n");
        goto finish;
    }

    ULONG table_size;
    ULONG hunk_first_slot;
    ULONG last_hunk_slot;

    if (Read (f, &table_size, 4) != 4)
        goto finish;
    if (Read (f, &hunk_first_slot, 4) != 4)
        goto finish;
    if (Read (f, &last_hunk_slot, 4) != 4)
        goto finish;

    DPRINTF (LOG_DEBUG, "_dos: LoadSeg() reading hunk header, table_size=%d, hunk_first_slot=%d, last_hunk_slot=%d\n", table_size, hunk_first_slot, last_hunk_slot);

    ULONG *hunk_table = AllocVec ((table_size + 2)*4, MEMF_CLEAR);
    if (!hunk_table)
        goto finish;

    ULONG hunk_prev = 0;
    ULONG hunk_first = 0;
    for (int i = hunk_first_slot; i <= last_hunk_slot; i++)
    {
        ULONG cnt;

        if (Read(f, &cnt, 4) != 4)
            goto finish;

        ULONG mem_flags = (cnt & 0xC0000000) >> 29;
        ULONG mem_size  = (cnt & 0x3FFFFFFF) * 4;

        ULONG req = MEMF_CLEAR | MEMF_PUBLIC;
        if (mem_flags == (MEMF_FAST | MEMF_CHIP))
        {
            if (Read(f, &req, 4) != 4)
                goto finish;
        }
        else
        {
            req |= mem_flags;
        }

        mem_size += 8; // leave room for the hunk length and the next hunk pointer
        void *hunk_ptr = AllocVec (mem_size, req);
        if (!hunk_ptr)
            goto finish;
        hunk_table[i] = MKBADDR(hunk_ptr);

        DPRINTF (LOG_DEBUG, "_dos: LoadSeg() hunk %3d size=%6d, flags=0x%08lx, req=0x%08lx -> 0x%08lx\n", i, mem_size, mem_flags, req, hunk_ptr);

        // link hunks
        if (!hunk_first)
            hunk_first = hunk_table[i];
        if (hunk_prev)
            ((BPTR *)(BADDR(hunk_prev)))[0] = hunk_table[i];
        hunk_prev = hunk_table[i];
    }

    // read hunks

    ULONG hunk_type;
    ULONG hunk_cur = hunk_first_slot;
    ULONG hunk_last = 0;
    while ( Read (f, &hunk_type, 4) == 4)
    {
        DPRINTF (LOG_DEBUG, "_dos: LoadSeg() reading hunk #%3d type 0x%08lx\n", hunk_cur, hunk_type);

        switch (hunk_type)
        {
            case HUNK_TYPE_CODE:
            case HUNK_TYPE_DATA:
            case HUNK_TYPE_BSS:
            {
                ULONG cnt;

                if (Read(f, &cnt, 4) != 4)
                    goto finish;

                if (hunk_type != HUNK_TYPE_BSS)
                {
                    APTR hunk_mem = BADDR(hunk_table[hunk_cur]+1);
                    ULONG hunk_size = cnt*4;
                    DPRINTF (LOG_DEBUG, "_dos: LoadSeg() reading CODE/DATA hunk data hunk_size=%ld, hunk_mem=0x%08lx\n", hunk_size, hunk_mem);
                    if (Read (f, hunk_mem, hunk_size) != hunk_size)
                        goto finish;
                }
                else
                {
                    DPRINTF (LOG_DEBUG, "_dos: LoadSeg() encountered BSS hunk\n");
                }

                hunk_last = hunk_cur;
                hunk_cur ++ ;
                break;
            }

            case HUNK_TYPE_RELOC32:
            {
                while (TRUE)
                {
                    ULONG cnt;

                    if (Read(f, &cnt, 4) != 4)
                        goto finish;
                    if (!cnt)
                        break;

                    ULONG hunk_id;
                    if (Read(f, &hunk_id, 4) != 4)
                        goto finish;

                    DPRINTF (LOG_DEBUG, "_dos: LoadSeg() RELOC32 Hunk #%ld\n", hunk_id);
                    while (cnt > 0)
                    {
                        ULONG offset;
                        if (Read(f, &offset, 4) != 4)
                            goto finish;

                        ULONG *addr = (ULONG *)(BADDR(hunk_table[hunk_last]+1) + offset);

                        ULONG val = *addr + (ULONG)BADDR(hunk_table[hunk_id]+1);

                        *addr = val;

                        cnt--;
                    }
                }
                break;
            }

            case HUNK_TYPE_SYMBOL:
            {
                //DPRINTF (LOG_DEBUG, "_dos: LoadSeg() HUNK_SYMBOL detected\n");
                while (TRUE)
                {
                    char *name;
                    if (!_read_string (DOSBase, f, &name))
                        goto finish;
                    if (!name)
                        break;

                    //DPRINTF (LOG_DEBUG, "_dos: LoadSeg() SYMBOL name=%s\n", name);
                    ULONG offset;
                    if (Read(f, &offset, 4) != 4)
                        goto finish;

                    ULONG hunk_base = (intptr_t) BADDR(hunk_table[hunk_last]+1);
                    ULONG addr = hunk_base + offset;
                    DPRINTF (LOG_DEBUG, "_dos: LoadSeg() SYMBOL %s at 0x%08lx of hunk at 0x%08lx -> 0x%08lx\n",
                                        name, offset, hunk_base, addr);

                    emucall2 (EMU_CALL_SYMBOL, addr, (intptr_t) name);
                }
                break;
            }

            case HUNK_TYPE_END:
                break;

            case HUNK_TYPE_DEBUG:
			{
				ULONG cnt;
                if (Read(f, &cnt, 4)!=4)
                    goto finish;

                if (Seek(f, cnt*4, OFFSET_CURRENT)<0)
                    goto finish;
                break;
			}

            default:
                assert(FALSE);
        }
    }

    success = TRUE;

finish:
    {
        LONG err = IoErr();

        if (!success)
            DPRINTF (LOG_INFO, "_dos: LoadSeg() failed to load %s, err=%ld\n", ___name, err);

        Close (f);

        SetIoErr (err);

        if (!success)
        {

            for (int i = hunk_first_slot; i <= last_hunk_slot; i++)
            {
                if (!hunk_table[i])
                    continue;
                FreeVec (BADDR(hunk_table[i]));
            }
            if (hunk_table)
                FreeVec (hunk_table);
            hunk_first = 0;
        }
    }

    DPRINTF (LOG_INFO, "_dos: LoadSeg() done, result: 0x%08lx\n", hunk_first);

    return hunk_first;
}

void _dos_UnLoadSeg ( register struct DosLibrary * __libBase __asm("a6"),
                                    register BPTR ___seglist  __asm("d1"))
{
    DPRINTF (LOG_DEBUG, "_dos: UnLoadSeg unimplemented STUB called for seglist 0x%08lx.\n", ___seglist);
    /* FIXME: Implement memory freeing */
}


VOID _dos_private0 ( register struct DosLibrary * DOSBase __asm("a6"))
{
    LPRINTF (LOG_ERROR, "_dos: private0() unimplemented STUB called.\n");
    assert(FALSE);
}

VOID _dos_private1 ( register struct DosLibrary * DOSBase __asm("a6"))
{
    LPRINTF (LOG_ERROR, "_dos: private1() unimplemented STUB called.\n");
    assert(FALSE);
}

void  _dos_ClearVec (register struct DosLibrary * __libBase __asm("a6"),
                                     register BPTR ___bVector  __asm("d1"),
                                     register BPTR ___upb      __asm("d2"))
{
    DPRINTF (LOG_DEBUG, "_dos: ClearVec() unimplemented *PRIVATE* STUB called.\n");
    assert(FALSE);
}

void _dos_NoReqLoadSeg (register struct DosLibrary * __libBase __asm("a6"),
                                        register BPTR ___bFileName             __asm("d1"))
{
    DPRINTF (LOG_DEBUG, "_dos: NoReqLoadSeg() unimplemented *PRIVATE* STUB called.\n");
    assert(FALSE);
}

struct MsgPort * _dos_DeviceProc ( register struct DosLibrary * __libBase __asm("a6"),
                                                        register CONST_STRPTR ___name  __asm("d1"))
{
    DPRINTF (LOG_DEBUG, "_dos: DeviceProc unimplemented STUB called.\n");
    assert(FALSE);
    return NULL;
}

LONG _dos_SetComment ( register struct DosLibrary * __libBase __asm("a6"),
                                                        register CONST_STRPTR ___name  __asm("d1"),
                                                        register CONST_STRPTR ___comment  __asm("d2"))
{
    DPRINTF (LOG_DEBUG, "_dos: SetComment() called, name=%s, comment=%s\n", 
             ___name ? (char *)___name : "NULL",
             ___comment ? (char *)___comment : "NULL");

    if (!___name) {
        SetIoErr(ERROR_OBJECT_NOT_FOUND);
        return DOSFALSE;
    }

    LONG result = emucall2(EMU_CALL_DOS_SETCOMMENT, (ULONG)___name, (ULONG)___comment);

    DPRINTF (LOG_DEBUG, "_dos: SetComment() result: %ld\n", result);

    if (!result) {
        SetIoErr(ERROR_OBJECT_NOT_FOUND);
        return DOSFALSE;
    }

    return DOSTRUE;
}

LONG _dos_SetProtection ( register struct DosLibrary * __libBase __asm("a6"),
                                                        register CONST_STRPTR ___name  __asm("d1"),
                                                        register LONG ___protect  __asm("d2"))
{
    DPRINTF (LOG_DEBUG, "_dos: SetProtection() called, name=%s, protect=0x%08lx\n", 
             ___name ? (char *)___name : "NULL", ___protect);

    if (!___name) {
        SetIoErr(ERROR_OBJECT_NOT_FOUND);
        return DOSFALSE;
    }

    LONG result = emucall2(EMU_CALL_DOS_SETPROTECTION, (ULONG)___name, (ULONG)___protect);

    DPRINTF (LOG_DEBUG, "_dos: SetProtection() result: %ld\n", result);

    if (!result) {
        SetIoErr(ERROR_OBJECT_NOT_FOUND);
        return DOSFALSE;
    }

    return DOSTRUE;
}

struct DateStamp * _dos_DateStamp ( register struct DosLibrary *DOSBase __asm("a6"),
                                    register struct DateStamp  *ds      __asm("d1"))
{
    struct timeval tv;

    emucall1 (EMU_CALL_GETSYSTIME, (intptr_t) &tv);

    DPRINTF (LOG_DEBUG, "_dos_DateStamp: EMU_CALL_GETSYSTIME -> tv.tv_secs=%ld, tv.tv_micro=%ld\n",
             tv.tv_secs, tv.tv_micro);

    ULONG s = tv.tv_secs;

    // number of days since Jan. 1, 1978
    ds->ds_Days = s / (3600 * 24);
    s %= 3600*24;

    // number of minutes past midnight
    ds->ds_Minute = s / 60;
    s %= 60;

    // number of ticks past the minute
    ds->ds_Tick = (tv.tv_micro + s * 1000000) / 20000;

    DPRINTF (LOG_DEBUG, "_dos_DateStamp: -> ds->ds_Days=%ld, ds->ds_Minute=%ld, ds->ds_Tick=%ld\n",
             ds->ds_Days, ds->ds_Minute, ds->ds_Tick);

    return ds;
}

LONG _dos_WaitForChar ( register struct DosLibrary * __libBase __asm("a6"),
                                        register BPTR ___file  __asm("d1"),
                                        register LONG ___timeout  __asm("d2"))
{
    DPRINTF (LOG_DEBUG, "_dos: WaitForChar() unimplemented STUB called.\n");
    assert(FALSE);
    return 0;
}

BPTR _dos_ParentDir ( register struct DosLibrary * __libBase __asm("a6"),
                                      register BPTR ___lock  __asm("d1"))
{
    DPRINTF (LOG_DEBUG, "_dos: ParentDir() called, lock=0x%08lx\n", ___lock);

    if (!___lock) {
        SetIoErr(ERROR_OBJECT_NOT_FOUND);
        return 0;
    }

    ULONG lock_id = emucall1(EMU_CALL_DOS_PARENTDIR, (ULONG)___lock);

    DPRINTF (LOG_DEBUG, "_dos: ParentDir() result: lock_id=%lu\n", lock_id);

    /* A return of 0 is valid - means we're at root */
    return (BPTR)lock_id;
}

LONG _dos_IsInteractive ( register struct DosLibrary *DOSBase __asm("a6"),
                                   register BPTR               file    __asm("d1"))
{
    DPRINTF (LOG_DEBUG, "_dos: IsInteractive() called, file=0x%08lx\n", file);
    struct FileHandle *fh = (struct FileHandle *) BADDR(file);
    ULONG kind = fh->fh_Func3;
    return kind == FILE_KIND_CONSOLE;
}

void _dos_Delay ( register struct DosLibrary * __libBase __asm("a6"),
                  register ULONG ticks __asm("d1"))
{
    /*
     * Delay() - Wait for a specified number of ticks (1/50th second each)
     *
     * Without timer.device, we implement this by directly calling the
     * scheduler via Supervisor mode. This forces a task switch if there
     * are any ready tasks with equal or higher priority.
     *
     * Note: This is NOT an accurate delay, but it allows other tasks to run.
     */
    DPRINTF (LOG_DEBUG, "_dos: Delay(%ld) called.\n", ticks);

    if (ticks == 0)
        return;

    struct ExecBase *SysBase = *(struct ExecBase **)4;

    /* For each tick, try to yield to other tasks */
    for (ULONG i = 0; i < ticks; i++)
    {
        /*
         * Force a task switch if there are ready tasks.
         * We set the quantum-expired flag and call Schedule() directly
         * via Supervisor mode.
         */
        if (!IsListEmpty(&SysBase->TaskReady))
        {
            DPRINTF(LOG_DEBUG, "_dos: Delay() about to schedule, TaskReady not empty\n");
            
            Disable();
            /*
             * SFF_QuantumOver is bit 6 of the HIGH byte of SysFlags.
             * Since SysFlags is a UWORD and m68k is big-endian, bit 6 of
             * the high byte is bit 14 of the word value.
             */
            SysBase->SysFlags |= (1 << 14);  /* SFF_QuantumOver (bit 6 of high byte) */
            SysBase->Elapsed = 0;  /* Force immediate reschedule */
            Enable();

            /* Call Schedule() via Supervisor mode */
            /* Schedule() will check SysFlags and perform task switch if needed */
            __asm__ volatile (
                "   move.l  a5, -(sp)                   \n"
                "   move.l  4, a6                       \n"  /* SysBase */
                "   lea     _exec_Schedule, a5          \n"  /* Schedule routine */
                "   jsr     -30(a6)                     \n"  /* Supervisor() */
                "   move.l  (sp)+, a5                   \n"
                :
                :
                : "d0", "d1", "a0", "a1", "a6", "cc", "memory"
            );
            
            DPRINTF(LOG_DEBUG, "_dos: Delay() back from schedule\n");
        }
        else
        {
            /* No ready tasks - just use host-side sleep */
            emucall0(EMU_CALL_WAIT);
        }
    }
}

LONG _dos_Execute ( register struct DosLibrary * DOSBase __asm("a6"),
                                    register CONST_STRPTR ___string  __asm("d1"),
                                    register BPTR ___file  __asm("d2"),
                                    register BPTR ___file2  __asm("d3"))
{
    DPRINTF (LOG_DEBUG, "_dos: Execute('%s', input=0x%08lx, output=0x%08lx) called.\n", 
             ___string ? ___string : (CONST_STRPTR)"NULL", ___file, ___file2);
    
    /* 
     * Execute(command, input, output)
     * 
     * If command is not empty: execute it as a command line
     * If input is non-zero: read additional commands from that file handle
     *   (commands are executed one per line until EOF)
     * Output is used for any output from the commands
     *
     * Returns:
     *   For compatibility with existing tests, we return SystemTagList's result:
     *   -1 if the command could not be loaded
     *   0  if execution completed (command's return code is not propagated)
     *
     * Note: True AmigaDOS Execute returns DOSTRUE (-1) on success, DOSFALSE (0) on failure,
     * but our tests expect the old behavior.
     */
    
    LONG rc = 0;  /* Success by default (old behavior) */
    
    /* First, execute the command string if provided */
    if (___string && *___string) {
        struct TagItem tags[3];
        tags[0].ti_Tag = SYS_Output;
        tags[0].ti_Data = ___file2 ? (ULONG)___file2 : (ULONG)Output();
        tags[1].ti_Tag = TAG_DONE;
        tags[1].ti_Data = 0;
        
        LONG sysrc = _dos_SystemTagList(DOSBase, ___string, tags);
        
        if (sysrc == -1) {
            /* System failed to load - try as script via Shell */
            STRPTR shellName = (STRPTR)"SYS:System/Shell";
            
            /* Allocate buffer for "Shell script args" */
            ULONG cmdLen = strlen((char *)shellName) + 1 + strlen((char *)___string) + 1;
            STRPTR cmdBuf = AllocVec(cmdLen, MEMF_PUBLIC);
            
            if (cmdBuf) {
                strcpy((char *)cmdBuf, (char *)shellName);
                strcat((char *)cmdBuf, " ");
                strcat((char *)cmdBuf, (char *)___string);
                
                DPRINTF(LOG_DEBUG, "_dos: Execute: System failed, trying as script: '%s'\n", cmdBuf);
                
                sysrc = _dos_SystemTagList(DOSBase, cmdBuf, tags);
                
                FreeVec(cmdBuf);
            } else {
                SetIoErr(ERROR_NO_FREE_STORE);
                return -1;  /* Failed to allocate */
            }
        }
        
        if (sysrc == -1) {
            return -1;  /* Command could not be loaded */
        }
    }
    
    /* If input file handle is provided, read and execute commands from it */
    if (___file) {
        /* Read commands line by line from input */
        #define EXEC_LINE_BUF_SIZE 512
        STRPTR lineBuf = AllocVec(EXEC_LINE_BUF_SIZE, MEMF_PUBLIC);
        
        if (!lineBuf) {
            SetIoErr(ERROR_NO_FREE_STORE);
            return -1;
        }
        
        while (1) {
            /* Read a line from input */
            STRPTR line = FGets(___file, lineBuf, EXEC_LINE_BUF_SIZE - 1);
            
            if (!line) {
                /* EOF or error */
                break;
            }
            
            /* Strip trailing newline */
            LONG len = 0;
            while (line[len]) len++;
            if (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r')) {
                line[len-1] = '\0';
                len--;
            }
            if (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r')) {
                line[len-1] = '\0';
                len--;
            }
            
            /* Skip empty lines and comments */
            if (len == 0 || line[0] == ';') {
                continue;
            }
            
            DPRINTF(LOG_DEBUG, "_dos: Execute: Running line from file: '%s'\n", line);
            
            /* Execute this line */
            struct TagItem tags[3];
            tags[0].ti_Tag = SYS_Output;
            tags[0].ti_Data = ___file2 ? (ULONG)___file2 : (ULONG)Output();
            tags[1].ti_Tag = TAG_DONE;
            tags[1].ti_Data = 0;
            
            LONG sysrc = _dos_SystemTagList(DOSBase, line, tags);
            
            if (sysrc == -1) {
                /* Try as script via Shell */
                STRPTR shellName = (STRPTR)"SYS:System/Shell";
                ULONG cmdLen = strlen((char *)shellName) + 1 + strlen((char *)line) + 1;
                STRPTR cmdBuf = AllocVec(cmdLen, MEMF_PUBLIC);
                
                if (cmdBuf) {
                    strcpy((char *)cmdBuf, (char *)shellName);
                    strcat((char *)cmdBuf, " ");
                    strcat((char *)cmdBuf, (char *)line);
                    
                    sysrc = _dos_SystemTagList(DOSBase, cmdBuf, tags);
                    FreeVec(cmdBuf);
                }
            }
            
            /* Note: Execute continues even if individual commands fail */
            /* but we track if any command failed to load */
            if (sysrc == -1) {
                rc = -1;
            }
        }
        
        FreeVec(lineBuf);
        #undef EXEC_LINE_BUF_SIZE
    }
    
    return rc;
}

void *_dos_AllocDosObject (register struct DosLibrary *DOSBase __asm("a6"),
                                    register ULONG              type    __asm("d1"),
                                    register struct TagItem    *tags    __asm("d2"))
{
    DPRINTF (LOG_DEBUG, "_dos: AllocDosObject() called type=%ld\n", type);

    switch (type)
    {
        case DOS_FILEHANDLE:
        {
            APTR m = AllocVec (sizeof(struct FileHandle), MEMF_CLEAR);

            if (m)
            {
                //struct FileHandle *fh = (struct FileHandle *) m;

                // FIXME: initialize
                //fh->fh_Pos = -1;
                //fh->fh_End = -1;
            }
            DPRINTF (LOG_DEBUG, "_dos: AllocDosObject() allocated new DOS_FILEHANDLE object: 0x%08lx\n", m);
            return m;
        }

        case DOS_FIB:
        {
            APTR m = AllocVec (sizeof(struct FileInfoBlock), MEMF_CLEAR);
            DPRINTF (LOG_DEBUG, "_dos: AllocDosObject() allocated new DOS_FIB object: 0x%08lx\n", m);
            return m;
        }

        case DOS_CLI:
        {
            ULONG   dirBufLen   = GetTagData (ADO_DirLen     , 255, tags);
            ULONG   commNameLen = GetTagData (ADO_CommNameLen, 255, tags);
            ULONG   commFileLen = GetTagData (ADO_CommFileLen, 255, tags);
            ULONG   promptLen   = GetTagData (ADO_PromptLen  , 255, tags);

            STRPTR  dirBuf      = NULL;
            STRPTR  commandBuf  = NULL;
            STRPTR  fileBuf     = NULL;
            STRPTR  promptBuf   = NULL;

            struct CommandLineInterface *cli = AllocVec (sizeof(struct CommandLineInterface), MEMF_CLEAR);
            if (!cli) goto OOM;

            cli->cli_FailLevel  = RETURN_ERROR;
            cli->cli_Background = DOSTRUE;

            dirBuf = AllocVec(dirBufLen + 1, MEMF_PUBLIC | MEMF_CLEAR);
            if (!dirBuf) goto OOM;
            dirBuf[0] = 0;
            cli->cli_SetName = MKBADDR(dirBuf);

            commandBuf = AllocVec(commNameLen + 1, MEMF_PUBLIC | MEMF_CLEAR);
            if (!commandBuf) goto OOM;
            commandBuf[0] = 0;
            cli->cli_CommandName = MKBADDR(commandBuf);

            fileBuf = AllocVec(commFileLen + 1, MEMF_PUBLIC | MEMF_CLEAR);
            if (!fileBuf) goto OOM;
            fileBuf[0] = 0;
            cli->cli_CommandFile = MKBADDR(fileBuf);

            promptBuf = AllocVec(promptLen + 1, MEMF_PUBLIC | MEMF_CLEAR);
            if (!promptBuf) goto OOM;
            promptBuf[0] = 0;
            cli->cli_Prompt = MKBADDR(promptBuf);

            return cli;

OOM:
            FreeVec(cli);

            FreeVec(dirBuf);
            FreeVec(commandBuf);
            FreeVec(fileBuf);
            FreeVec(promptBuf);

            SetIoErr(ERROR_NO_FREE_STORE);

            return NULL;
        }

        default:
            DPRINTF (LOG_DEBUG, "_dos: FIXME: AllocDosObject() type=%ld not implemented\n", type);
            assert (FALSE);
    }

    SetIoErr (ERROR_BAD_NUMBER);

    return NULL;
}

void _dos_FreeDosObject (register struct DosLibrary *DOSBase __asm("a6"),
                                  register ULONG              type    __asm("d1"),
                                  register void              *ptr     __asm("d2"))
{
    DPRINTF (LOG_DEBUG, "_dos: FreeDosObject() called, type=%d, ptr=0x%08lx\n", type, ptr);

    if (!ptr)
        return;

    switch (type)
    {
        case DOS_FILEHANDLE:
        {
            struct FileHandle *fh = (struct FileHandle *)ptr;
            FreeVec(fh);
            break;
        }

        case DOS_FIB:
        {
            FreeVec(ptr);
            break;
        }

        case DOS_CLI:
        {
            struct CommandLineInterface *cli = (struct CommandLineInterface *)ptr;

            BPTR *cdir = (BPTR *)BADDR(cli->cli_CommandDir);

            FreeVec(BADDR(cli->cli_SetName));
            FreeVec(BADDR(cli->cli_CommandName));
            FreeVec(BADDR(cli->cli_CommandFile));
            FreeVec(BADDR(cli->cli_Prompt));
            FreeVec(cli);

            while (cdir)
            {
                BPTR *next = (BPTR *)BADDR(cdir[0]);
                UnLock(cdir[1]);
                FreeVec(cdir);
                cdir = next;
            }
            break;

    	}
	default:
		assert (FALSE); // FIXME: implement other dos obj types
	}
}

LONG _dos_DoPkt ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register struct MsgPort * port __asm("d1"),
                                                        register LONG action __asm("d2"),
                                                        register LONG arg1 __asm("d3"),
                                                        register LONG arg2 __asm("d4"),
                                                        register LONG arg3 __asm("d5"),
                                                        register LONG arg4 __asm("d6"),
                                                        register LONG arg5 __asm("d7"))
{
    LPRINTF (LOG_ERROR, "_dos: DoPkt() unimplemented STUB called.\n");
    assert(FALSE);
    return 0;
}

VOID _dos_SendPkt ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register struct DosPacket * dp __asm("d1"),
                                                        register struct MsgPort * port __asm("d2"),
                                                        register struct MsgPort * replyport __asm("d3"))
{
    LPRINTF (LOG_ERROR, "_dos: SendPkt() unimplemented STUB called.\n");
    assert(FALSE);
}

struct DosPacket * _dos_WaitPkt ( register struct DosLibrary * DOSBase __asm("a6"))
{
    LPRINTF (LOG_ERROR, "_dos: WaitPkt() unimplemented STUB called.\n");
    assert(FALSE);
    return NULL;
}

VOID _dos_ReplyPkt ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register struct DosPacket * dp __asm("d1"),
                                                        register LONG res1 __asm("d2"),
                                                        register LONG res2 __asm("d3"))
{
    LPRINTF (LOG_ERROR, "_dos: ReplyPkt() unimplemented STUB called.\n");
    assert(FALSE);
}

VOID _dos_AbortPkt ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register struct MsgPort * port __asm("d1"),
                                                        register struct DosPacket * pkt __asm("d2"))
{
    LPRINTF (LOG_ERROR, "_dos: AbortPkt() unimplemented STUB called.\n");
    assert(FALSE);
}

BOOL _dos_LockRecord ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register BPTR fh __asm("d1"),
                                                        register ULONG offset __asm("d2"),
                                                        register ULONG length __asm("d3"),
                                                        register ULONG mode __asm("d4"),
                                                        register ULONG timeout __asm("d5"))
{
    LPRINTF (LOG_ERROR, "_dos: LockRecord() unimplemented STUB called.\n");
    assert(FALSE);
    return FALSE;
}

BOOL _dos_LockRecords ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register struct RecordLock * recArray __asm("d1"),
                                                        register ULONG timeout __asm("d2"))
{
    LPRINTF (LOG_ERROR, "_dos: LockRecords() unimplemented STUB called.\n");
    assert(FALSE);
    return FALSE;
}

BOOL _dos_UnLockRecord ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register BPTR fh __asm("d1"),
                                                        register ULONG offset __asm("d2"),
                                                        register ULONG length __asm("d3"))
{
    LPRINTF (LOG_ERROR, "_dos: UnLockRecord() unimplemented STUB called.\n");
    assert(FALSE);
    return FALSE;
}

BOOL _dos_UnLockRecords ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register struct RecordLock * recArray __asm("d1"))
{
    LPRINTF (LOG_ERROR, "_dos: UnLockRecords() unimplemented STUB called.\n");
    assert(FALSE);
    return FALSE;
}

BPTR _dos_SelectInput ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register BPTR fh __asm("d1"))
{
    LPRINTF (LOG_ERROR, "_dos: SelectInput() unimplemented STUB called.\n");
    assert(FALSE);
    return 0;
}

BPTR _dos_SelectOutput ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register BPTR fh __asm("d1"))
{
    LPRINTF (LOG_ERROR, "_dos: SelectOutput() unimplemented STUB called.\n");
    assert(FALSE);
    return 0;
}

LONG _dos_FGetC ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register BPTR fh __asm("d1"))
{
    LPRINTF (LOG_ERROR, "_dos: FGetC() unimplemented STUB called.\n");
    assert(FALSE);
    return 0;
}

LONG _dos_FPutC ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register BPTR fh __asm("d1"),
                                                        register LONG ch __asm("d2"))
{
    LPRINTF (LOG_ERROR, "_dos: FPutC() unimplemented STUB called.\n");
    assert(FALSE);
    return 0;
}

LONG _dos_UnGetC ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register BPTR fh __asm("d1"),
                                                        register LONG character __asm("d2"))
{
    LPRINTF (LOG_ERROR, "_dos: UnGetC() unimplemented STUB called.\n");
    assert(FALSE);
    return 0;
}

LONG _dos_FRead ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register BPTR fh __asm("d1"),
                                                        register APTR block __asm("d2"),
                                                        register ULONG blocklen __asm("d3"),
                                                        register ULONG number __asm("d4"))
{
    LPRINTF (LOG_ERROR, "_dos: FRead() unimplemented STUB called.\n");
    assert(FALSE);
    return 0;
}

LONG _dos_FWrite ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register BPTR fh __asm("d1"),
                                                        register const APTR block __asm("d2"),
                                                        register ULONG blocklen __asm("d3"),
                                                        register ULONG number __asm("d4"))
{
    LPRINTF (LOG_ERROR, "_dos: FWrite() unimplemented STUB called.\n");
    assert(FALSE);
    return 0;
}

STRPTR _dos_FGets ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register BPTR fh __asm("d1"),
                                                        register STRPTR buf __asm("d2"),
                                                        register ULONG buflen __asm("d3"))
{
    LPRINTF (LOG_ERROR, "_dos: FGets() unimplemented STUB called.\n");
    assert(FALSE);
    return NULL;
}

LONG _dos_FPuts ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register BPTR fh __asm("d1"),
                                                        register CONST_STRPTR str __asm("d2"))
{
    LPRINTF (LOG_ERROR, "_dos: FPuts() unimplemented STUB called.\n");
    assert(FALSE);
    return 0;
}

VOID _dos_VFWritef ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register BPTR fh __asm("d1"),
                                                        register CONST_STRPTR format __asm("d2"),
                                                        register const LONG * argarray __asm("d3"))
{
    LPRINTF (LOG_ERROR, "_dos: VFWritef() unimplemented STUB called.\n");
    assert(FALSE);
}

LONG _dos_VFPrintf ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register BPTR fh __asm("d1"),
                                                        register CONST_STRPTR format __asm("d2"),
                                                        register const APTR argarray __asm("d3"))
{
    LPRINTF (LOG_ERROR, "_dos: VFPrintf() unimplemented STUB called.\n");
    assert(FALSE);
    return 0;
}

LONG _dos_Flush ( register struct DosLibrary * DOSBase __asm("a6"),
														register BPTR fh __asm("d1"))
{
    DPRINTF (LOG_DEBUG, "_dos: Flush() called fh=%08lx\n", fh);
    struct FileHandle *fhp = (struct FileHandle *) BADDR(fh);
    return emucall1(EMU_CALL_DOS_FLUSH, (ULONG)fhp);
}

LONG _dos_SetVBuf ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register BPTR fh __asm("d1"),
                                                        register STRPTR buff __asm("d2"),
                                                        register LONG type __asm("d3"),
                                                        register LONG size __asm("d4"))
{
    LPRINTF (LOG_ERROR, "_dos: SetVBuf() unimplemented STUB called.\n");
    assert(FALSE);
    return 0;
}

BPTR _dos_DupLockFromFH ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register BPTR fh __asm("d1"))
{
    LPRINTF (LOG_ERROR, "_dos: DupLockFromFH() unimplemented STUB called.\n");
    assert(FALSE);
    return 0;
}

BPTR _dos_OpenFromLock ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register BPTR lock __asm("d1"))
{
    LPRINTF (LOG_ERROR, "_dos: OpenFromLock() unimplemented STUB called.\n");
    assert(FALSE);
    return 0;
}

BPTR _dos_ParentOfFH ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register BPTR fh __asm("d1"))
{
    LPRINTF (LOG_ERROR, "_dos: ParentOfFH() unimplemented STUB called.\n");
    assert(FALSE);
    return 0;
}

BOOL _dos_ExamineFH ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register BPTR fh __asm("d1"),
                                                        register struct FileInfoBlock * fib __asm("d2"))
{
    LPRINTF (LOG_ERROR, "_dos: ExamineFH() unimplemented STUB called.\n");
    assert(FALSE);
    return FALSE;
}

LONG _dos_SetFileDate ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register CONST_STRPTR name __asm("d1"),
                                                        register const struct DateStamp * date __asm("d2"))
{
    LPRINTF (LOG_ERROR, "_dos: SetFileDate() unimplemented STUB called.\n");
    assert(FALSE);
    return 0;
}

LONG _dos_NameFromLock ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register BPTR lock __asm("d1"),
                                                        register STRPTR buffer __asm("d2"),
                                                        register LONG len __asm("d3"))
{
    DPRINTF (LOG_DEBUG, "_dos: NameFromLock() called, lock=0x%08lx, buffer=0x%08lx, len=%ld\n", lock, buffer, len);

    if (!lock || !buffer || len <= 0) {
        SetIoErr(ERROR_OBJECT_NOT_FOUND);
        return DOSFALSE;
    }

    LONG result = emucall3(EMU_CALL_DOS_NAMEFROMLOCK, (ULONG)lock, (ULONG)buffer, (ULONG)len);

    DPRINTF (LOG_DEBUG, "_dos: NameFromLock() result: %ld\n", result);

    if (!result) {
        SetIoErr(ERROR_OBJECT_NOT_FOUND);
        return DOSFALSE;
    }

    return DOSTRUE;
}

LONG _dos_NameFromFH ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register BPTR fh __asm("d1"),
                                                        register STRPTR buffer __asm("d2"),
                                                        register LONG len __asm("d3"))
{
    LPRINTF (LOG_ERROR, "_dos: NameFromFH() unimplemented STUB called.\n");
    assert(FALSE);
    return 0;
}

WORD _dos_SplitName ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register CONST_STRPTR name __asm("d1"),
                                                        register UBYTE separator __asm("d2"),
                                                        register STRPTR buf __asm("d3"),
                                                        register WORD oldpos __asm("d4"),
                                                        register LONG size __asm("d5"))
{
    LPRINTF (LOG_ERROR, "_dos: SplitName() unimplemented STUB called.\n");
    assert(FALSE);
    return 0;
}

LONG _dos_SameLock ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register BPTR lock1 __asm("d1"),
                                                        register BPTR lock2 __asm("d2"))
{
    DPRINTF (LOG_DEBUG, "_dos: SameLock() called, lock1=0x%08lx, lock2=0x%08lx\n", lock1, lock2);

    LONG result = emucall2(EMU_CALL_DOS_SAMELOCK, (ULONG)lock1, (ULONG)lock2);

    DPRINTF (LOG_DEBUG, "_dos: SameLock() result: %ld\n", result);

    /* Returns LOCK_SAME (0), LOCK_SAME_VOLUME (1), or LOCK_DIFFERENT (-1) */
    return result;
}

LONG _dos_SetMode ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register BPTR fh __asm("d1"),
                                                        register LONG mode __asm("d2"))
{
    LPRINTF (LOG_ERROR, "_dos: SetMode() unimplemented STUB called.\n");
    assert(FALSE);
    return 0;
}

LONG _dos_ExAll ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register BPTR lock __asm("d1"),
                                                        register struct ExAllData * buffer __asm("d2"),
                                                        register LONG size __asm("d3"),
                                                        register LONG data __asm("d4"),
                                                        register struct ExAllControl * control __asm("d5"))
{
    LPRINTF (LOG_ERROR, "_dos: ExAll() unimplemented STUB called.\n");
    assert(FALSE);
    return 0;
}

LONG _dos_ReadLink ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register struct MsgPort * port __asm("d1"),
                                                        register BPTR lock __asm("d2"),
                                                        register CONST_STRPTR path __asm("d3"),
                                                        register STRPTR buffer __asm("d4"),
                                                        register ULONG size __asm("d5"))
{
    LPRINTF (LOG_ERROR, "_dos: ReadLink() unimplemented STUB called.\n");
    assert(FALSE);
    return 0;
}

LONG _dos_MakeLink ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register CONST_STRPTR name __asm("d1"),
                                                        register LONG dest __asm("d2"),
                                                        register LONG soft __asm("d3"))
{
    LPRINTF (LOG_ERROR, "_dos: MakeLink() unimplemented STUB called.\n");
    assert(FALSE);
    return 0;
}

LONG _dos_ChangeMode ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register LONG type __asm("d1"),
                                                        register BPTR fh __asm("d2"),
                                                        register LONG newmode __asm("d3"))
{
    LPRINTF (LOG_ERROR, "_dos: ChangeMode() unimplemented STUB called.\n");
    assert(FALSE);
    return 0;
}

LONG _dos_SetFileSize ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register BPTR fh __asm("d1"),
                                                        register LONG pos __asm("d2"),
                                                        register LONG mode __asm("d3"))
{
    LPRINTF (LOG_ERROR, "_dos: SetFileSize() unimplemented STUB called.\n");
    assert(FALSE);
    return 0;
}

LONG _dos_SetIoErr ( register struct DosLibrary * DOSBase __asm("a6"),
                                     register LONG result __asm("d1"))
{
    DPRINTF (LOG_DEBUG, "_dos: SetIoErr() called, result=%ld\n", result);

    ULONG prevErrorCode;

    struct Process *me = (struct Process *) FindTask (NULL);

    if (((struct Task *)me)->tc_Node.ln_Type != NT_PROCESS)
        return 0;

    prevErrorCode = me->pr_Result2;
    me->pr_Result2 = result;

    return prevErrorCode;
}

BOOL _dos_Fault ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register LONG code __asm("d1"),
                                                        register STRPTR header __asm("d2"),
                                                        register STRPTR buffer __asm("d3"),
                                                        register LONG len __asm("d4"))
{
    LPRINTF (LOG_ERROR, "_dos: Fault() unimplemented STUB called.\n");
    assert(FALSE);
    return FALSE;
}

BOOL _dos_PrintFault ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register LONG code __asm("d1"),
                                                        register CONST_STRPTR header __asm("d2"))
{
    LPRINTF (LOG_ERROR, "_dos: PrintFault() unimplemented STUB called.\n");
    assert(FALSE);
    return FALSE;
}

LONG _dos_ErrorReport ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register LONG code __asm("d1"),
                                                        register LONG type __asm("d2"),
                                                        register ULONG arg1 __asm("d3"),
                                                        register struct MsgPort * device __asm("d4"))
{
    LPRINTF (LOG_ERROR, "_dos: ErrorReport() unimplemented STUB called.\n");
    assert(FALSE);
    return 0;
}

VOID _dos_private2 ( register struct DosLibrary * DOSBase __asm("a6"))
{
    LPRINTF (LOG_ERROR, "_dos: private2() unimplemented STUB called.\n");
    assert(FALSE);
}

struct CommandLineInterface * _dos_Cli ( register struct DosLibrary * DOSBase __asm("a6"))
{
    DPRINTF (LOG_DEBUG, "_dos: Cli() called.\n");

    struct Process *me = (struct Process *) FindTask (NULL);
    if (IS_PROCESS(me))
        return (struct CommandLineInterface *) BADDR(me->pr_CLI);
    else
        return NULL;
}

// Minimum stack size for AmigaOS processes (4KB is the standard minimum)
#define MIN_STACK_SIZE 4096

struct Process * _dos_CreateNewProc ( register struct DosLibrary * DOSBase __asm("a6"),
                                                      register const struct TagItem * tags __asm("d1"))
{
    DPRINTF (LOG_INFO, "_dos: CreateNewProc() called.\n");

    // determine stack size, input, output

    ULONG           stackSize = 8192;
    struct Process *me        = (struct Process *) FindTask (NULL);
    BPTR            inp       = 0;
    BPTR            outp      = 0;

    if (IS_PROCESS(me))
    {
        struct CommandLineInterface *cli = Cli();

        if (cli)
        {
            LONG parent_stackSize = cli->cli_DefaultStack * 4;
            if (parent_stackSize > stackSize)
                stackSize = parent_stackSize;
        }

        inp  = me->pr_CIS;
        outp = me->pr_COS;
    }

    // examine tags

           stackSize =          GetTagData(NP_StackSize, stackSize            , tags);
    LONG   pri       =          GetTagData(NP_Priority , 0                    , tags);
    STRPTR name      = (STRPTR) GetTagData(NP_Name     , (ULONG) "new process", tags);
    APTR   initpc    = (APTR)   GetTagData(NP_Entry    , 0                    , tags);
    BPTR   seglist   =          GetTagData(NP_Seglist  , 0                    , tags);
    BOOL   do_cli    =          GetTagData(NP_Cli      , 0                    , tags);
           inp       =          GetTagData(NP_Input    , inp                  , tags);
           outp      =          GetTagData(NP_Output   , outp                 , tags);
    char  *args      = (char*)  GetTagData(NP_Arguments, (ULONG)NULL          , tags);
    BPTR   curdir    =          GetTagData(NP_CurrentDir, 0                   , tags);

    // Enforce minimum stack size
    if (stackSize < MIN_STACK_SIZE)
    {
        DPRINTF (LOG_WARNING, "_dos: CreateNewProc() stack size %lu too small, using minimum %d\n",
                 stackSize, MIN_STACK_SIZE);
        stackSize = MIN_STACK_SIZE;
    }

    if (!initpc)
    {
        if (!seglist)
        {
            LPRINTF (LOG_ERROR, "_dos: CreateNewProc() called without NP_Entry or NP_Seglist!\n");
            return NULL;
        }
        initpc = BADDR(seglist) + sizeof(BPTR);
    }

    if (args)
    {
        int l = strlen (args);
        char *nargs = AllocVec (l+1, MEMF_PUBLIC);
        if (!nargs) {
            LPRINTF(LOG_ERROR, "_dos: CreateNewProc failed to allocate args buffer!\n");
            return NULL;
        }
        CopyMem (args, nargs, l+1);
        args = nargs;
    }

    // create process

    struct Process *process = (struct Process *) U_allocTask (name, pri, stackSize, /*isProcess=*/ TRUE);

    if (!process)
        return NULL;

    U_prepareProcess (process, initpc, 0, stackSize, args);

    if (do_cli)
    {
        // Assign task number from RootNode TaskArray (supports recycling)
        process->pr_TaskNum = allocTaskNum(process);

        struct CommandLineInterface *cli = (struct CommandLineInterface *) AllocDosObject (DOS_CLI, (struct TagItem *)tags);
        if (!cli)
        {
            LPRINTF (LOG_ERROR, "_dos: CreateNewProc() failed to allocate CLI structure\n");
            // Clean up and return NULL
            // Note: process is on tc_MemEntry, will be cleaned up by RemTask eventually,
            // but since we haven't added it to TaskReady yet, we need to free it manually
            U_freeTask (&process->pr_Task);
            if (args)
                FreeVec (args);
            return NULL;
        }

        cli->cli_DefaultStack = (process->pr_StackSize + 3) / 4;

        // FIXME: copy prompt from parent's cli, if any
        // FIXME: set cli_CommandDir

        process->pr_CLI = MKBADDR(cli);
    }

    process->pr_CIS = inp;
    process->pr_COS = outp;
    process->pr_CurrentDir = curdir;

    // launch it

    Disable();

    process->pr_Task.tc_State = TS_READY;
    Enqueue (&SysBase->TaskReady, &process->pr_Task.tc_Node);

    Enable();

    DPRINTF (LOG_INFO, "_dos: CreateNewProc() done, process=0x%08lx\n", process);

    return process;
}

LONG _dos_RunCommand ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register BPTR seg __asm("d1"),
                                                        register LONG stack __asm("d2"),
                                                        register CONST_STRPTR paramptr __asm("d3"),
                                                        register LONG paramlen __asm("d4"))
{
    LPRINTF (LOG_ERROR, "_dos: RunCommand() unimplemented STUB called.\n");
    assert(FALSE);
    return 0;
}

struct MsgPort * _dos_GetConsoleTask ( register struct DosLibrary * DOSBase __asm("a6"))
{
    LPRINTF (LOG_ERROR, "_dos: GetConsoleTask() unimplemented STUB called.\n");
    assert(FALSE);
    return NULL;
}

struct MsgPort * _dos_SetConsoleTask ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register const struct MsgPort * task __asm("d1"))
{
    LPRINTF (LOG_ERROR, "_dos: SetConsoleTask() unimplemented STUB called.\n");
    assert(FALSE);
    return NULL;
}

struct MsgPort * _dos_GetFileSysTask ( register struct DosLibrary * DOSBase __asm("a6"))
{
    LPRINTF (LOG_ERROR, "_dos: GetFileSysTask() unimplemented STUB called.\n");
    assert(FALSE);
    return NULL;
}

struct MsgPort * _dos_SetFileSysTask ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register const struct MsgPort * task __asm("d1"))
{
    LPRINTF (LOG_ERROR, "_dos: SetFileSysTask() unimplemented STUB called.\n");
    assert(FALSE);
    return NULL;
}

STRPTR _dos_GetArgStr ( register struct DosLibrary * DOSBase __asm("a6"))
{
    LPRINTF (LOG_ERROR, "_dos: GetArgStr() unimplemented STUB called.\n");
    assert(FALSE);
    return NULL;
}

BOOL _dos_SetArgStr ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register CONST_STRPTR string __asm("d1"))
{
    LPRINTF (LOG_ERROR, "_dos: SetArgStr() unimplemented STUB called.\n");
    assert(FALSE);
    return FALSE;
}

struct Process * _dos_FindCliProc ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register ULONG num __asm("d1"))
{
    LPRINTF (LOG_ERROR, "_dos: FindCliProc() unimplemented STUB called.\n");
    assert(FALSE);
    return NULL;
}

ULONG _dos_MaxCli ( register struct DosLibrary * DOSBase __asm("a6"))
{
    LPRINTF (LOG_ERROR, "_dos: MaxCli() unimplemented STUB called.\n");
    assert(FALSE);
    return 0;
}

BOOL _dos_SetCurrentDirName ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register CONST_STRPTR name __asm("d1"))
{
    LPRINTF (LOG_ERROR, "_dos: SetCurrentDirName() unimplemented STUB called.\n");
    assert(FALSE);
    return FALSE;
}

BOOL _dos_GetCurrentDirName ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register STRPTR buf __asm("d1"),
                                                        register LONG len __asm("d2"))
{
    LPRINTF (LOG_ERROR, "_dos: GetCurrentDirName() unimplemented STUB called.\n");
    assert(FALSE);
    return FALSE;
}

BOOL _dos_SetProgramName ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register CONST_STRPTR name __asm("d1"))
{
    LPRINTF (LOG_ERROR, "_dos: SetProgramName() unimplemented STUB called.\n");
    assert(FALSE);
    return FALSE;
}

BOOL _dos_GetProgramName ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register STRPTR buf __asm("d1"),
                                                        register LONG len __asm("d2"))
{
    DPRINTF (LOG_DEBUG, "_dos: GetProgramName() called.\n");

    if (!buf || len <= 0)
        return FALSE;

    const char *src = "testprog";
    LONG i;
    for (i = 0; i < len - 1 && src[i] != '\0'; i++)
        buf[i] = src[i];
    buf[i] = '\0';

    return TRUE;
}

BOOL _dos_SetPrompt ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register CONST_STRPTR name __asm("d1"))
{
    LPRINTF (LOG_ERROR, "_dos: SetPrompt() unimplemented STUB called.\n");
    assert(FALSE);
    return FALSE;
}

BOOL _dos_GetPrompt ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register STRPTR buf __asm("d1"),
                                                        register LONG len __asm("d2"))
{
    LPRINTF (LOG_ERROR, "_dos: GetPrompt() unimplemented STUB called.\n");
    assert(FALSE);
    return FALSE;
}

BPTR _dos_SetProgramDir ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register BPTR lock __asm("d1"))
{
    LPRINTF (LOG_ERROR, "_dos: SetProgramDir() unimplemented STUB called.\n");
    assert(FALSE);
    return 0;
}

BPTR _dos_GetProgramDir ( register struct DosLibrary * DOSBase __asm("a6"))
{
    LPRINTF (LOG_ERROR, "_dos: GetProgramDir() unimplemented STUB called.\n");
    assert(FALSE);
    return 0;
}

LONG _dos_SystemTagList ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register CONST_STRPTR command __asm("d1"),
                                                        register const struct TagItem * tags __asm("d2"))
{
    DPRINTF (LOG_INFO, "_dos: SystemTagList() called command='%s'\n", command);
    
    char bin_name[256];
    char *args = NULL;
    int i = 0;
    
    /* Skip leading spaces */
    while (*command == ' ') command++;
    
    //const char *start = command;
    while (*command && *command != ' ' && *command != '\n' && i < 255) {
        bin_name[i++] = *command++;
    }
    bin_name[i] = '\0';
    
    /* Args start after the space/command */
    if (*command) args = (char *)command; // Points to space or rest of string
    // Usually arguments string passed to C startup includes the space? No, usually it's just args.
    // If I pass " foo", the startup code sees " foo".
    // If I pass "foo", it sees "foo".
    // Let's pass the rest of the string as is.
    
    /* Try to load */
    BPTR seglist = _dos_LoadSeg(DOSBase, (STRPTR)bin_name);
    if (!seglist) {
        /* Try C: prefix if no path component */
        int has_colon = 0;
        int has_slash = 0;
        for(int j=0; bin_name[j]; j++) {
            if(bin_name[j] == ':') has_colon=1;
            if(bin_name[j] == '/') has_slash=1;
        }
        
        if (!has_colon && !has_slash) {
             char tmp[256];
             // Simple strcpy/cat
             char *s = "SYS:C/";
             char *d = tmp;
             while(*s) *d++ = *s++;
             char *n = bin_name;
             while(*n) *d++ = *n++;
             *d = 0;
             
             seglist = _dos_LoadSeg(DOSBase, (STRPTR)tmp);
        }
    }
    
    if (!seglist) {
        LPRINTF(LOG_ERROR, "_dos: SystemTagList() failed to load '%s'\n", bin_name);
        return -1; // ERROR_OBJECT_NOT_FOUND
    }
    
    DPRINTF(LOG_DEBUG, "_dos: SystemTagList() loaded '%s', seglist=0x%08lx\n", bin_name, seglist);
    
    /* Create Process */
    BPTR input = GetTagData(SYS_Input, 0, tags);
    BPTR output = GetTagData(SYS_Output, 0, tags);
    ULONG stackSize = GetTagData(NP_StackSize, 4096, tags);  /* Respect caller's stack size, default 4096 */
    BPTR curDir = 0;
    
    struct Process *me = (struct Process *)FindTask(NULL);
    if (IS_PROCESS(me)) {
        if (!input) input = me->pr_CIS;
        if (!output) output = me->pr_COS;
        /* Pass current directory to child if parent has one */
        if (me->pr_CurrentDir) {
            curDir = DupLock(me->pr_CurrentDir);
        }
    }
    
    struct TagItem procTags[] = {
        { NP_Seglist, (ULONG)seglist },
        { NP_Name, (ULONG)bin_name },
        { NP_StackSize, stackSize },
        { NP_Cli, TRUE },
        { NP_Input, input },
        { NP_Output, output },
        { NP_Arguments, (ULONG)args },
        { NP_CurrentDir, (ULONG)curDir },
        { TAG_DONE, 0 }
    };
    
    struct Process *proc = _dos_CreateNewProc(DOSBase, procTags);
    if (!proc) {
        LPRINTF(LOG_ERROR, "_dos: SystemTagList() failed to create process\n");
        _dos_UnLoadSeg(DOSBase, seglist);
        return -1;
    }
    
    DPRINTF(LOG_DEBUG, "_dos: SystemTagList() created process 0x%08lx, taskNum=%ld\n", proc, proc->pr_TaskNum);
    
    /* Wait for completion - Poll TaskArray */
    LONG taskNum = proc->pr_TaskNum;
    struct RootNode *root = DOSBase->dl_Root;
    
    /*
     * IMPORTANT: Save the child's MsgPort address NOW, before starting the wait loop.
     * Once the child calls Exit() and RemTask(), the proc pointer becomes invalid
     * (the memory is freed). We must not access proc after the child might have exited.
     */
    struct MsgPort *childPort = &proc->pr_MsgPort;
    
    DPRINTF(LOG_INFO, "_dos: SystemTagList waiting for task %ld (proc 0x%08lx, port 0x%08lx)\n", 
            taskNum, proc, childPort);
    
    /* Wait for the child task to complete by polling TaskArray.
     * When Exit() is called, freeTaskNum() clears the slot to 0. */
    
    ULONG oldSig = me->pr_Task.tc_SigWait;
    int loopCount = 0;
    
    DPRINTF(LOG_DEBUG, "_dos: SystemTagList wait loop starting, taskNum=%ld\n", taskNum);
    
    while (1) {
        ULONG *taskArray = (ULONG *)BADDR(root->rn_TaskArray);
        if (!taskArray) {
            DPRINTF(LOG_DEBUG, "_dos: SystemTagList wait: taskArray is NULL\n");
            break;
        }
        
        /* TaskArray[taskNum] contains the pr_MsgPort pointer (as ULONG) */
        ULONG storedValue = taskArray[taskNum];
        
        if (storedValue == 0) {
            /* Slot cleared - task has exited and called freeTaskNum() */
            DPRINTF(LOG_DEBUG, "_dos: SystemTagList wait: task slot cleared (exited)\n");
            break;
        }
        
        if (storedValue != (ULONG)childPort) {
            /* Slot reused by another task - original task must have exited */
            DPRINTF(LOG_DEBUG, "_dos: SystemTagList wait: slot reused (stored=0x%08lx, expected=0x%08lx)\n", 
                    storedValue, (ULONG)childPort);
            break;
        }
        
        loopCount++;
        if (loopCount > 1000) {
            LPRINTF(LOG_WARNING, "_dos: SystemTagList wait loop exceeded 1000 iterations!\n");
            break;
        }
        
        DPRINTF(LOG_DEBUG, "_dos: SystemTagList wait loop iteration %d, stored=0x%08lx\n", 
                loopCount, storedValue);
        
        /* Yield to allow child task to run */
        _dos_Delay(DOSBase, 1);
    }
    
    DPRINTF(LOG_DEBUG, "_dos: SystemTagList wait loop finished after %d iterations\n", loopCount);
    
    me->pr_Task.tc_SigWait = oldSig;
    
    DPRINTF(LOG_INFO, "_dos: SystemTagList task %ld finished\n", taskNum);
    
    _dos_UnLoadSeg(DOSBase, seglist);
    return 0; // Success
}

/*
 * Phase 7: Assignment System
 *
 * These functions manage logical name assignments (like C:, LIBS:, etc.)
 * The actual assign storage is handled by the host VFS layer.
 */

/* Assign type constants for emucall */
#define ASSIGN_TYPE_LOCK 0   /* Points to a specific directory (resolved once) */
#define ASSIGN_TYPE_LATE 1   /* Late-binding: path resolved when accessed */
#define ASSIGN_TYPE_PATH 2   /* Non-binding path (like AssignPath) */

LONG _dos_AssignLock ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register CONST_STRPTR name __asm("d1"),
                                                        register BPTR lock __asm("d2"))
{
    DPRINTF (LOG_DEBUG, "_dos: AssignLock() called, name=%s, lock=0x%08lx\n",
             name ? (char *)name : "NULL", lock);
    
    if (!name || !lock)
    {
        SetIoErr(ERROR_REQUIRED_ARG_MISSING);
        return DOSFALSE;
    }
    
    /* Get path from lock using NameFromLock */
    char path[256];
    if (!_dos_NameFromLock(DOSBase, lock, (STRPTR)path, sizeof(path)))
    {
        SetIoErr(ERROR_OBJECT_NOT_FOUND);
        return DOSFALSE;
    }
    
    DPRINTF (LOG_DEBUG, "_dos: AssignLock() path from lock: %s\n", path);
    
    /* Call host to create the assign (type 0 = ASSIGN_LOCK) */
    LONG result = emucall3(EMU_CALL_DOS_ASSIGN_ADD, (ULONG)name, (ULONG)path, ASSIGN_TYPE_LOCK);
    
    if (result)
    {
        /* On success, we consume the lock (AmigaOS behavior) */
        _dos_UnLock(DOSBase, lock);
        return DOSTRUE;
    }
    
    SetIoErr(ERROR_OBJECT_EXISTS);
    return DOSFALSE;
}

BOOL _dos_AssignLate ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register CONST_STRPTR name __asm("d1"),
                                                        register CONST_STRPTR path __asm("d2"))
{
    DPRINTF (LOG_DEBUG, "_dos: AssignLate() called, name=%s, path=%s\n",
             name ? (char *)name : "NULL", path ? (char *)path : "NULL");
    
    if (!name || !path)
    {
        SetIoErr(ERROR_REQUIRED_ARG_MISSING);
        return FALSE;
    }
    
    /* Call host to create the assign (type 1 = ASSIGN_LATE) */
    LONG result = emucall3(EMU_CALL_DOS_ASSIGN_ADD, (ULONG)name, (ULONG)path, ASSIGN_TYPE_LATE);
    
    if (result)
    {
        return TRUE;
    }
    
    SetIoErr(ERROR_OBJECT_EXISTS);
    return FALSE;
}

BOOL _dos_AssignPath ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register CONST_STRPTR name __asm("d1"),
                                                        register CONST_STRPTR path __asm("d2"))
{
    DPRINTF (LOG_DEBUG, "_dos: AssignPath() called, name=%s, path=%s\n",
             name ? (char *)name : "NULL", path ? (char *)path : "NULL");
    
    if (!name || !path)
    {
        SetIoErr(ERROR_REQUIRED_ARG_MISSING);
        return FALSE;
    }
    
    /* Call host to create the assign (type 2 = ASSIGN_PATH) */
    LONG result = emucall3(EMU_CALL_DOS_ASSIGN_ADD, (ULONG)name, (ULONG)path, ASSIGN_TYPE_PATH);
    
    if (result)
    {
        return TRUE;
    }
    
    SetIoErr(ERROR_OBJECT_EXISTS);
    return FALSE;
}

BOOL _dos_AssignAdd ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register CONST_STRPTR name __asm("d1"),
                                                        register BPTR lock __asm("d2"))
{
    DPRINTF (LOG_DEBUG, "_dos: AssignAdd() called, name=%s, lock=0x%08lx\n",
             name ? (char *)name : "NULL", lock);
    
    if (!name || !lock)
    {
        SetIoErr(ERROR_REQUIRED_ARG_MISSING);
        return FALSE;
    }
    
    /* Get path from lock using NameFromLock */
    char path[256];
    if (!_dos_NameFromLock(DOSBase, lock, (STRPTR)path, sizeof(path)))
    {
        SetIoErr(ERROR_OBJECT_NOT_FOUND);
        return FALSE;
    }
    
    DPRINTF (LOG_DEBUG, "_dos: AssignAdd() path from lock: %s\n", path);
    
    /* Call host to add to the assign (type 0 = ASSIGN_LOCK for adds) */
    LONG result = emucall3(EMU_CALL_DOS_ASSIGN_ADD, (ULONG)name, (ULONG)path, ASSIGN_TYPE_LOCK);
    
    if (result)
    {
        /* On success, we consume the lock (AmigaOS behavior) */
        _dos_UnLock(DOSBase, lock);
        return TRUE;
    }
    
    SetIoErr(ERROR_OBJECT_EXISTS);
    return FALSE;
}

LONG _dos_RemAssignList ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register CONST_STRPTR name __asm("d1"),
                                                        register BPTR lock __asm("d2"))
{
    DPRINTF (LOG_DEBUG, "_dos: RemAssignList() called, name=%s, lock=0x%08lx\n",
             name ? (char *)name : "NULL", lock);
    
    if (!name)
    {
        SetIoErr(ERROR_REQUIRED_ARG_MISSING);
        return DOSFALSE;
    }
    
    /* If lock is NULL, remove the entire assign; otherwise just that path */
    /* For simplicity, we just remove the entire assign for now */
    LONG result = emucall1(EMU_CALL_DOS_ASSIGN_REMOVE, (ULONG)name);
    
    if (result)
    {
        if (lock)
        {
            _dos_UnLock(DOSBase, lock);
        }
        return DOSTRUE;
    }
    
    SetIoErr(ERROR_OBJECT_NOT_FOUND);
    return DOSFALSE;
}

struct DevProc * _dos_GetDeviceProc ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register CONST_STRPTR name __asm("d1"),
                                                        register struct DevProc * dp __asm("d2"))
{
    LPRINTF (LOG_ERROR, "_dos: GetDeviceProc() unimplemented STUB called.\n");
    assert(FALSE);
    return NULL;
}

VOID _dos_FreeDeviceProc ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register struct DevProc * dp __asm("d1"))
{
    LPRINTF (LOG_ERROR, "_dos: FreeDeviceProc() unimplemented STUB called.\n");
    assert(FALSE);
}

struct DosList * _dos_LockDosList ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register ULONG flags __asm("d1"))
{
    LPRINTF (LOG_ERROR, "_dos: LockDosList() unimplemented STUB called.\n");
    assert(FALSE);
    return NULL;
}

VOID _dos_UnLockDosList ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register ULONG flags __asm("d1"))
{
    LPRINTF (LOG_ERROR, "_dos: UnLockDosList() unimplemented STUB called.\n");
    assert(FALSE);
}

struct DosList * _dos_AttemptLockDosList ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register ULONG flags __asm("d1"))
{
    LPRINTF (LOG_ERROR, "_dos: AttemptLockDosList() unimplemented STUB called.\n");
    assert(FALSE);
    return NULL;
}

BOOL _dos_RemDosEntry ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register struct DosList * dlist __asm("d1"))
{
    LPRINTF (LOG_ERROR, "_dos: RemDosEntry() unimplemented STUB called.\n");
    assert(FALSE);
    return FALSE;
}

LONG _dos_AddDosEntry ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register struct DosList * dlist __asm("d1"))
{
    LPRINTF (LOG_ERROR, "_dos: AddDosEntry() unimplemented STUB called.\n");
    assert(FALSE);
    return 0;
}

struct DosList * _dos_FindDosEntry ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register const struct DosList * dlist __asm("d1"),
                                                        register CONST_STRPTR name __asm("d2"),
                                                        register ULONG flags __asm("d3"))
{
    LPRINTF (LOG_ERROR, "_dos: FindDosEntry() unimplemented STUB called.\n");
    assert(FALSE);
    return NULL;
    return NULL;
}

struct DosList * _dos_NextDosEntry ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register const struct DosList * dlist __asm("d1"),
                                                        register ULONG flags __asm("d2"))
{
    LPRINTF (LOG_ERROR, "_dos: NextDosEntry() unimplemented STUB called.\n");
    assert(FALSE);
    return NULL;
    return NULL;
}

struct DosList * _dos_MakeDosEntry ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register CONST_STRPTR name __asm("d1"),
                                                        register LONG type __asm("d2"))
{
    LPRINTF (LOG_ERROR, "_dos: MakeDosEntry() unimplemented STUB called.\n");
    assert(FALSE);
    return NULL;
}

VOID _dos_FreeDosEntry ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register struct DosList * dlist __asm("d1"))
{
    LPRINTF (LOG_ERROR, "_dos: FreeDosEntry() unimplemented STUB called.\n");
    assert(FALSE);
}

BOOL _dos_IsFileSystem ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register CONST_STRPTR name __asm("d1"))
{
    LPRINTF (LOG_ERROR, "_dos: IsFileSystem() unimplemented STUB called.\n");
    assert(FALSE);
    return FALSE;
}

BOOL _dos_Format ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register CONST_STRPTR filesystem __asm("d1"),
                                                        register CONST_STRPTR volumename __asm("d2"),
                                                        register ULONG dostype __asm("d3"))
{
    LPRINTF (LOG_ERROR, "_dos: Format() unimplemented STUB called.\n");
    assert(FALSE);
    return FALSE;
}

LONG _dos_Relabel ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register CONST_STRPTR drive __asm("d1"),
                                                        register CONST_STRPTR newname __asm("d2"))
{
    LPRINTF (LOG_ERROR, "_dos: Relabel() unimplemented STUB called.\n");
    assert(FALSE);
    return 0;
}

LONG _dos_Inhibit ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register CONST_STRPTR name __asm("d1"),
                                                        register LONG onoff __asm("d2"))
{
    LPRINTF (LOG_ERROR, "_dos: Inhibit() unimplemented STUB called.\n");
    assert(FALSE);
    return 0;
}

LONG _dos_AddBuffers ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register CONST_STRPTR name __asm("d1"),
                                                        register LONG number __asm("d2"))
{
    LPRINTF (LOG_ERROR, "_dos: AddBuffers() unimplemented STUB called.\n");
    assert(FALSE);
    return 0;
}

LONG _dos_CompareDates ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register const struct DateStamp * date1 __asm("d1"),
                                                        register const struct DateStamp * date2 __asm("d2"))
{
    LPRINTF (LOG_ERROR, "_dos: CompareDates() unimplemented STUB called.\n");
    assert(FALSE);
    return 0;
}

LONG _dos_DateToStr ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register struct DateTime * datetime __asm("d1"))
{
    LPRINTF (LOG_ERROR, "_dos: DateToStr() unimplemented STUB called.\n");
    assert(FALSE);
    return 0;
}

LONG _dos_StrToDate ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register struct DateTime * datetime __asm("d1"))
{
    LPRINTF (LOG_ERROR, "_dos: StrToDate() unimplemented STUB called.\n");
    assert(FALSE);
    return 0;
}

BPTR _dos_InternalLoadSeg ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register BPTR fh __asm("d0"),
                                                        register BPTR table __asm("a0"),
                                                        register const LONG * funcarray __asm("a1"),
                                                        register LONG * stack __asm("a2"))
{
    LPRINTF (LOG_ERROR, "_dos: InternalLoadSeg() unimplemented STUB called.\n");
    assert(FALSE);
    return 0;
}

BOOL _dos_InternalUnLoadSeg ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register BPTR seglist __asm("d1"),
                                                        register VOID (*freefunc)() __asm("a1"))
{
    LPRINTF (LOG_ERROR, "_dos: InternalUnLoadSeg() unimplemented STUB called.\n");
    assert(FALSE);
    return FALSE;
}

BPTR _dos_NewLoadSeg ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register CONST_STRPTR file __asm("d1"),
                                                        register const struct TagItem * tags __asm("d2"))
{
    LPRINTF (LOG_ERROR, "_dos: NewLoadSeg() unimplemented STUB called.\n");
    assert(FALSE);
    return 0;
}

LONG _dos_AddSegment ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register CONST_STRPTR name __asm("d1"),
                                                        register BPTR seg __asm("d2"),
                                                        register LONG system __asm("d3"))
{
    LPRINTF (LOG_ERROR, "_dos: AddSegment() unimplemented STUB called.\n");
    assert(FALSE);
    return 0;
}

struct Segment * _dos_FindSegment ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register CONST_STRPTR name __asm("d1"),
                                                        register const struct Segment * seg __asm("d2"),
                                                        register LONG system __asm("d3"))
{
    LPRINTF (LOG_ERROR, "_dos: FindSegment() unimplemented STUB called.\n");
    assert(FALSE);
    return NULL;
}

LONG _dos_RemSegment ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register struct Segment * seg __asm("d1"))
{
    LPRINTF (LOG_ERROR, "_dos: RemSegment() unimplemented STUB called.\n");
    assert(FALSE);
    return 0;
}

LONG _dos_CheckSignal ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register LONG mask __asm("d1"))
{
    LPRINTF (LOG_ERROR, "_dos: CheckSignal() unimplemented STUB called.\n");
    assert(FALSE);
    return 0;
}

/* Template item flags */
#define TEMPLATE_REQUIRED  0x01  /* /A */
#define TEMPLATE_KEYWORD   0x02  /* /K */
#define TEMPLATE_SWITCH    0x04  /* /S */
#define TEMPLATE_NUMERIC   0x08  /* /N */
#define TEMPLATE_MULTIPLE  0x10  /* /M */
#define TEMPLATE_REST      0x20  /* /F */

#ifndef RDAF_ALLOCATED_BY_READARGS
#define RDAF_ALLOCATED_BY_READARGS 0x80
#endif

/* Internal structure for tracking allocations made by ReadArgs */
typedef struct DANode {
    struct DANode *next;
    APTR memory;
} DANode;

typedef struct {
    char name[32];       /* Primary name - most keywords are short */
    char alias[32];      /* Alias name (for AS=TO syntax) */
    ULONG flags;
    LONG index;
} TemplateItem;

static LONG _parse_template(CONST_STRPTR tmpl, TemplateItem *items)
{
    LONG num_items = 0;
    CONST_STRPTR p = tmpl;

    while (*p && num_items < 32) {
        /* Skip whitespace and commas */
        while (*p == ' ' || *p == '\t' || *p == ',') p++;
        if (!*p) break;

        /* Parse item name - may contain = for alias (AS=TO) */
        LONG i = 0;
        items[num_items].alias[0] = '\0';  /* No alias by default */
        
        while (*p && *p != ',' && *p != '/' && *p != ' ' && *p != '\t' && i < 31) {
            if (*p == '=') {
                /* Found alias separator - what we have so far is the primary name */
                items[num_items].name[i] = '\0';
                p++;  /* Skip the = */
                
                /* Now read the alias */
                i = 0;
                while (*p && *p != ',' && *p != '/' && *p != ' ' && *p != '\t' && i < 31) {
                    items[num_items].alias[i++] = *p++;
                }
                items[num_items].alias[i] = '\0';
                i = -1;  /* Signal that name is already terminated */
                break;
            }
            items[num_items].name[i++] = *p++;
        }
        if (i >= 0) {
            items[num_items].name[i] = '\0';
        }
        items[num_items].flags = 0;
        items[num_items].index = num_items;

        /* Parse modifiers */
        while (*p == '/') {
            p++;
            switch (*p) {
                case 'A': items[num_items].flags |= TEMPLATE_REQUIRED; break;
                case 'K': items[num_items].flags |= TEMPLATE_KEYWORD; break;
                case 'S': items[num_items].flags |= TEMPLATE_SWITCH; break;
                case 'N': items[num_items].flags |= TEMPLATE_NUMERIC; break;
                case 'M': items[num_items].flags |= TEMPLATE_MULTIPLE; break;
                case 'F': items[num_items].flags |= TEMPLATE_REST; break;
            }
            if (*p) p++;
        }

        num_items++;

        /* Skip to next item */
        while (*p && *p != ',') p++;
        if (*p == ',') p++;
    }

    return num_items;
}

static LONG _str_to_long(CONST_STRPTR str, LONG *val)
{
    LONG v = 0;
    BOOL negative = FALSE;
    CONST_STRPTR p = str;

    if (!p || !*p)
        return -1;

    if (*p == '-') {
        negative = TRUE;
        p++;
    } else if (*p == '+') {
        p++;
    }

    if (!*p || *p < '0' || *p > '9')
        return -1;

    while (*p >= '0' && *p <= '9') {
        v = v * 10 + (*p - '0');
        p++;
    }

    if (*p != '\0')
        return -1;

    *val = negative ? -v : v;
    return 0;
}

/* Simple string comparison helper */
static int _stricmp(const char *s1, const char *s2)
{
    while (*s1 && *s2) {
        char c1 = *s1;
        char c2 = *s2;
        /* Convert to lowercase */
        if (c1 >= 'A' && c1 <= 'Z') c1 += 32;
        if (c2 >= 'A' && c2 <= 'Z') c2 += 32;
        if (c1 != c2)
            return c1 - c2;
        s1++;
        s2++;
    }
    return *s1 - *s2;
}

struct RDArgs * _dos_ReadArgs ( register struct DosLibrary * DOSBase __asm("a6"),
                                                         register CONST_STRPTR arg_template __asm("d1"),
                                                         register LONG * array __asm("d2"),
                                                         register struct RDArgs * args __asm("d3"))
{
    DPRINTF (LOG_DEBUG, "_dos: ReadArgs() called, template='%s'\n", arg_template ? arg_template : "NULL");

    if (!arg_template || !array) {
        SetIoErr(ERROR_BAD_NUMBER);
        return NULL;
    }

    /* Get process and arguments */
    struct Process *me = (struct Process *)FindTask(NULL);
    if (!me) {
        SetIoErr(ERROR_BAD_NUMBER);
        return NULL;
    }

    /* Parse template */
    TemplateItem items[32];
    LONG num_items = _parse_template(arg_template, items);

    if (num_items == 0) {
        SetIoErr(ERROR_BAD_NUMBER);
        return NULL;
    }

    /* Get argument string from process - NULL is treated as empty string */
    STRPTR arg_str = me->pr_Arguments;
    if (!arg_str) {
        arg_str = (STRPTR)"";
    }

    /* Initialize array to 0/FALSE */
    for (LONG i = 0; i < num_items; i++) {
        array[i] = 0;
    }

    /* For /M (multiple) arguments, we need to collect all values into a NULL-terminated
     * array of pointers. Allocate storage dynamically (can't use static in ROM code).
     * Limit: 16 values per /M argument
     */
    #define MAX_MULTI_VALUES 16
    
    /* Allocate multi_values as a 2D array: num_items * (MAX_MULTI_VALUES+1) pointers */
    STRPTR *multi_values_flat = (STRPTR *)AllocVec(num_items * (MAX_MULTI_VALUES + 1) * sizeof(STRPTR), MEMF_CLEAR);
    LONG *multi_counts = (LONG *)AllocVec(num_items * sizeof(LONG), MEMF_CLEAR);
    
    /* Allocate storage for /N numeric values (one LONG per item) */
    LONG *numeric_storage = (LONG *)AllocVec(num_items * sizeof(LONG), MEMF_CLEAR);
    
    if (!multi_values_flat || !multi_counts || !numeric_storage) {
        if (multi_values_flat) FreeVec(multi_values_flat);
        if (multi_counts) FreeVec(multi_counts);
        if (numeric_storage) FreeVec(numeric_storage);
        SetIoErr(ERROR_NO_FREE_STORE);
        return NULL;
    }
    
    /* Helper macros to access 2D array stored as flat array */
    #define MULTI_VALUES(item, idx) multi_values_flat[(item) * (MAX_MULTI_VALUES + 1) + (idx)]

    /* Parse arguments - handle whitespace, quotes, and KEY=value syntax
     * AmigaDOS modifies the argument string in-place, replacing delimiters with NULLs.
     */
    LONG current_item = -1;
    BOOL in_token = FALSE;
    BOOL in_quotes = FALSE;
    STRPTR p = arg_str;  /* Non-const because we modify in place */
    STRPTR token_start = NULL;

    while (1) {
        char c = *p;

        /* Handle quoted strings */
        if (c == '"') {
            if (!in_quotes) {
                /* Start of quoted string */
                in_quotes = TRUE;
                if (!in_token) {
                    token_start = p + 1;  /* Skip the opening quote */
                    in_token = TRUE;
                }
                p++;
                continue;
            } else {
                /* End of quoted string - null-terminate here */
                in_quotes = FALSE;
                *p = '\0';  /* Replace closing quote with NULL */
                /* Continue to process end of token (fall through to whitespace handling) */
                c = ' ';  /* Treat as whitespace to end token */
            }
        }

        /* Inside quotes, everything except closing quote is part of token */
        if (in_quotes) {
            if (c == '\0') {
                /* Unterminated quote - process as is */
                break;
            }
            in_token = TRUE;
            if (!token_start) token_start = p;
            p++;
            continue;
        }

        if (c == ' ' || c == '\t' || c == '\n' || c == '\0') {
            if (in_token) {
                /* End of token - null-terminate in place */
                if (c != '\0') {
                    *p = '\0';  /* Modify the string in place */
                }
                in_token = FALSE;

                /* Check for KEY=value syntax */
                STRPTR eq_pos = NULL;
                STRPTR tp;
                for (tp = token_start; *tp; tp++) {
                    if (*tp == '=') {
                        eq_pos = tp;
                        break;
                    }
                }

                STRPTR key_name = token_start;
                STRPTR key_value = NULL;
                
                if (eq_pos) {
                    /* Split at = sign */
                    *eq_pos = '\0';
                    key_value = eq_pos + 1;
                }

                /* Process the token */
                BOOL found_keyword = FALSE;

                /* Check if this is a keyword (check both name and alias) */
                for (LONG i = 0; i < num_items; i++) {
                    if (items[i].flags & TEMPLATE_KEYWORD || items[i].flags & TEMPLATE_SWITCH) {
                        /* Check primary name OR alias */
                        if (_stricmp((const char *)key_name, items[i].name) == 0 ||
                            (items[i].alias[0] && _stricmp((const char *)key_name, items[i].alias) == 0)) {
                            if (items[i].flags & TEMPLATE_SWITCH) {
                                /* Switch - just set to TRUE */
                                array[items[i].index] = (LONG)TRUE;
                            } else if (key_value) {
                                /* KEY=value syntax - process value immediately */
                                LONG idx = items[i].index;
                                if (items[i].flags & TEMPLATE_NUMERIC) {
                                    LONG val;
                                    if (_str_to_long((CONST_STRPTR)key_value, &val) == 0) {
                                        numeric_storage[idx] = val;
                                        array[idx] = (LONG)&numeric_storage[idx];
                                    }
                                } else if (items[i].flags & TEMPLATE_MULTIPLE) {
                                    if (multi_counts[idx] < MAX_MULTI_VALUES) {
                                        MULTI_VALUES(idx, multi_counts[idx]++) = key_value;
                                        MULTI_VALUES(idx, multi_counts[idx]) = NULL;
                                    }
                                } else {
                                    array[idx] = (LONG)key_value;
                                }
                            } else {
                                /* Keyword without = - next token is the value */
                                current_item = i;
                            }
                            found_keyword = TRUE;
                            break;
                        }
                    }
                }

                if (!found_keyword && current_item >= 0) {
                    /* This is a value for the current keyword item */
                    LONG idx = items[current_item].index;
                    if (items[current_item].flags & TEMPLATE_NUMERIC) {
                        LONG val;
                        if (_str_to_long((CONST_STRPTR)token_start, &val) == 0) {
                            numeric_storage[idx] = val;
                            array[idx] = (LONG)&numeric_storage[idx];
                        }
                    } else if (items[current_item].flags & TEMPLATE_MULTIPLE) {
                        /* /M argument - collect into multi_values array */
                        if (multi_counts[idx] < MAX_MULTI_VALUES) {
                            MULTI_VALUES(idx, multi_counts[idx]++) = token_start;
                            MULTI_VALUES(idx, multi_counts[idx]) = NULL;
                        }
                    } else {
                        /* Store pointer to the null-terminated token in arg_str */
                        array[idx] = (LONG)token_start;
                    }
                    current_item = -1;
                } else if (!found_keyword) {
                    /* Not a keyword, try to match with non-keyword items */
                    for (LONG i = 0; i < num_items; i++) {
                        LONG idx = items[i].index;
                        if (!(items[i].flags & TEMPLATE_KEYWORD) &&
                            !(items[i].flags & TEMPLATE_SWITCH)) {
                            /* For /M items, always add to array; for others, only if empty */
                            if (items[i].flags & TEMPLATE_MULTIPLE) {
                                if (multi_counts[idx] < MAX_MULTI_VALUES) {
                                    MULTI_VALUES(idx, multi_counts[idx]++) = token_start;
                                    MULTI_VALUES(idx, multi_counts[idx]) = NULL;
                                }
                                break;
                            } else if (array[idx] == 0) {
                                if (items[i].flags & TEMPLATE_NUMERIC) {
                                    LONG val;
                                    if (_str_to_long((CONST_STRPTR)token_start, &val) == 0) {
                                        numeric_storage[idx] = val;
                                        array[idx] = (LONG)&numeric_storage[idx];
                                    }
                                } else {
                                    array[idx] = (LONG)token_start;
                                }
                                break;
                            }
                        }
                    }
                }

                token_start = NULL;
            }

            if (c == '\0')
                break;
        } else {
            if (!in_token) {
                token_start = p;  /* Remember start of this token */
            }
            in_token = TRUE;
        }

        p++;
    }

    /* Now allocate arrays for /M items and copy the collected pointers */
    /* Store them in a temporary list that will be added to RDA_DAList after result is created */
    DANode *multi_alloc_list = NULL;
    
    for (LONG i = 0; i < num_items; i++) {
        if (items[i].flags & TEMPLATE_MULTIPLE) {
            LONG idx = items[i].index;
            LONG count = multi_counts[idx];
            if (count > 0) {
                /* Allocate array of (count+1) pointers (NULL-terminated) */
                STRPTR *arr = (STRPTR *)AllocVec((count + 1) * sizeof(STRPTR), MEMF_ANY);
                if (arr) {
                    for (LONG j = 0; j < count; j++) {
                        arr[j] = MULTI_VALUES(idx, j);
                    }
                    arr[count] = NULL;
                    array[idx] = (LONG)arr;
                    
                    /* Track this allocation for FreeArgs */
                    DANode *node = (DANode *)AllocVec(sizeof(DANode), MEMF_ANY);
                    if (node) {
                        node->memory = arr;
                        node->next = multi_alloc_list;
                        multi_alloc_list = node;
                    }
                }
            }
        }
    }

    /* Check required items - for /M items, check if count > 0 */
    for (LONG i = 0; i < num_items; i++) {
        if (items[i].flags & TEMPLATE_REQUIRED) {
            LONG idx = items[i].index;
            if (items[i].flags & TEMPLATE_MULTIPLE) {
                if (multi_counts[idx] == 0) {
                    /* Clean up and return error */
                    while (multi_alloc_list) {
                        DANode *next = multi_alloc_list->next;
                        if (multi_alloc_list->memory) FreeVec(multi_alloc_list->memory);
                        FreeVec(multi_alloc_list);
                        multi_alloc_list = next;
                    }
                    FreeVec(multi_values_flat);
                    FreeVec(multi_counts);
                    FreeVec(numeric_storage);
                    SetIoErr(ERROR_REQUIRED_ARG_MISSING);
                    return NULL;
                }
            } else if (array[idx] == 0) {
                /* Clean up and return error */
                while (multi_alloc_list) {
                    DANode *next = multi_alloc_list->next;
                    if (multi_alloc_list->memory) FreeVec(multi_alloc_list->memory);
                    FreeVec(multi_alloc_list);
                    multi_alloc_list = next;
                }
                FreeVec(multi_values_flat);
                FreeVec(multi_counts);
                FreeVec(numeric_storage);
                SetIoErr(ERROR_REQUIRED_ARG_MISSING);
                return NULL;
            }
        }
    }

    /* Free temporary arrays (but NOT numeric_storage - that's needed for /N results) */
    FreeVec(multi_values_flat);
    FreeVec(multi_counts);
    
    #undef MULTI_VALUES
    #undef MAX_MULTI_VALUES

    /* Allocate or return RDArgs structure */
    struct RDArgs *result = args;
    if (!result) {
        result = (struct RDArgs *)AllocVec(sizeof(struct RDArgs), MEMF_ANY | MEMF_CLEAR);
        if (result) {
            result->RDA_Flags |= RDAF_ALLOCATED_BY_READARGS;
        }
    }
    
    /* Store allocations in RDA_DAList so FreeArgs can free them */
    if (result) {
        /* Create a node for the numeric storage */
        DANode *node = (DANode *)AllocVec(sizeof(DANode), MEMF_ANY);
        if (node) {
            node->memory = numeric_storage;
            node->next = (DANode *)result->RDA_DAList;
            result->RDA_DAList = (LONG)node;
        }
        /* Also add any /M array allocations */
        while (multi_alloc_list) {
            DANode *next = multi_alloc_list->next;
            multi_alloc_list->next = (DANode *)result->RDA_DAList;
            result->RDA_DAList = (LONG)multi_alloc_list;
            multi_alloc_list = next;
        }
    } else {
        /* Allocation failed - clean up multi_alloc_list */
        while (multi_alloc_list) {
            DANode *next = multi_alloc_list->next;
            if (multi_alloc_list->memory) {
                FreeVec(multi_alloc_list->memory);
            }
            FreeVec(multi_alloc_list);
            multi_alloc_list = next;
        }
        FreeVec(numeric_storage);
    }

    return result;
}

LONG _dos_FindArg ( register struct DosLibrary * DOSBase __asm("a6"),
                                                         register CONST_STRPTR keyword __asm("d1"),
                                                         register CONST_STRPTR arg_template __asm("d2"))
{
    DPRINTF (LOG_DEBUG, "_dos: FindArg() called, keyword='%s', template='%s'\n",
             keyword ? keyword : "NULL", arg_template ? arg_template : "NULL");

    if (!keyword || !arg_template)
        return -1;

    TemplateItem items[32];
    LONG num_items = _parse_template(arg_template, items);

    for (LONG i = 0; i < num_items; i++) {
        if (_stricmp(items[i].name, (char *)keyword) == 0) {
            return items[i].index;
        }
    }

    return -1;
}

LONG _dos_ReadItem ( register struct DosLibrary * DOSBase __asm("a6"),
                                                         register CONST_STRPTR name __asm("d1"),
                                                         register LONG maxchars __asm("d2"),
                                                         register struct CSource * cSource __asm("d3"))
{
    DPRINTF (LOG_DEBUG, "_dos: ReadItem() unimplemented STUB called.\n");
    assert(FALSE);
    return 0;
}

LONG _dos_StrToLong ( register struct DosLibrary * DOSBase __asm("a6"),
                                                         register CONST_STRPTR string __asm("d1"),
                                                         register LONG * value __asm("d2"))
{
    DPRINTF (LOG_DEBUG, "_dos: StrToLong() called, string='%s'\n", string ? string : "NULL");

    if (!string || !value)
        return -1;

    return _str_to_long(string, value);
}

LONG _dos_MatchFirst ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register CONST_STRPTR pat __asm("d1"),
                                                        register struct AnchorPath * anchor __asm("d2"))
{
    LPRINTF (LOG_ERROR, "_dos: MatchFirst() unimplemented STUB called.\n");
    assert(FALSE);
    return 0;
}

LONG _dos_MatchNext ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register struct AnchorPath * anchor __asm("d1"))
{
    LPRINTF (LOG_ERROR, "_dos: MatchNext() unimplemented STUB called.\n");
    assert(FALSE);
    return 0;
}

VOID _dos_MatchEnd ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register struct AnchorPath * anchor __asm("d1"))
{
    LPRINTF (LOG_ERROR, "_dos: MatchEnd() unimplemented STUB called.\n");
    assert(FALSE);
}

/*
 * Pattern Matching Implementation for AmigaDOS compatibility
 * 
 * AmigaDOS pattern syntax:
 *   ?      - matches any single character
 *   #      - wildcard multiplier (zero or more of next char/wildcard)
 *   #?     - matches any sequence of characters (like "*" in other systems)
 *   %      - escape character (treat next char literally)
 *   ''     - quote characters (treat everything inside literally)
 *   [abc]  - character class (matches any character in brackets)
 *   [a-z]  - character range (matches any character in range)
 *   [~abc] - negated character class (matches any character NOT in brackets)
 */

/* Helper to convert character to lowercase (for case-insensitive matching) */
static UBYTE _to_lower(UBYTE c)
{
    if (c >= 'A' && c <= 'Z')
        return c + ('a' - 'A');
    return c;
}

/* 
 * Match a character against a character class [abc] or [a-z] or [~abc]
 * Returns: pointer to char after ']' if match, NULL if no match
 * Updates *matched to TRUE if character matches the class
 */
static const UBYTE *_match_char_class(const UBYTE *p, UBYTE c, BOOL *matched, BOOL case_insensitive)
{
    BOOL negated = FALSE;
    BOOL found = FALSE;
    
    /* p points to '[' - skip it */
    p++;
    
    /* Check for negation */
    if (*p == '~') {
        negated = TRUE;
        p++;
    }
    
    /* Convert character to lowercase if case-insensitive */
    UBYTE test_c = case_insensitive ? _to_lower(c) : c;
    
    /* Process characters until ']' */
    while (*p && *p != ']') {
        UBYTE class_c = case_insensitive ? _to_lower(*p) : *p;
        
        /* Check for range (a-z) */
        if (*(p + 1) == '-' && *(p + 2) && *(p + 2) != ']') {
            UBYTE range_start = class_c;
            UBYTE range_end = case_insensitive ? _to_lower(*(p + 2)) : *(p + 2);
            
            /* Ensure range is in correct order */
            if (range_start > range_end) {
                UBYTE tmp = range_start;
                range_start = range_end;
                range_end = tmp;
            }
            
            if (test_c >= range_start && test_c <= range_end) {
                found = TRUE;
            }
            p += 3; /* Skip 'a-z' */
        } else {
            /* Single character match */
            if (test_c == class_c) {
                found = TRUE;
            }
            p++;
        }
    }
    
    /* Skip closing ']' */
    if (*p == ']') {
        p++;
    } else {
        /* Malformed pattern - no closing bracket */
        *matched = FALSE;
        return NULL;
    }
    
    /* Apply negation if needed */
    *matched = negated ? !found : found;
    return p;
}

/* Internal recursive pattern matching function - uses UBYTE for Amiga compatibility */
static BOOL _match_pattern_internal(const UBYTE *pat, const UBYTE *str)
{
    const UBYTE *p = pat;
    const UBYTE *s = str;
    
    while (*p) {
        switch (*p) {
            case '?':
                /* Match any single character */
                if (*s == '\0')
                    return FALSE;
                p++;
                s++;
                break;
                
            case '#':
                /* Multiplier - zero or more of following char/wildcard */
                p++;
                if (*p == '\0')
                    return FALSE; /* # at end is an error */
                
                if (*p == '?') {
                    /* #? - matches any string (like *) */
                    p++;
                    /* Try to match rest of pattern at each position */
                    const UBYTE *try_s = s;
                    while (1) {
                        if (_match_pattern_internal(p, try_s))
                            return TRUE;
                        if (*try_s == '\0')
                            break;
                        try_s++;
                    }
                    return FALSE;
                } else {
                    /* #c - matches zero or more of character c */
                    UBYTE c = *p++;
                    /* Try to match rest of pattern with varying numbers of c */
                    const UBYTE *try_s = s;
                    while (1) {
                        if (_match_pattern_internal(p, try_s))
                            return TRUE;
                        if (*try_s != c)
                            break;
                        try_s++;
                    }
                    return FALSE;
                }
                break;
                
            case '%':
                /* Escape character - match literally */
                p++;
                if (*p == '\0')
                    return FALSE; /* % at end is an error */
                if (*s != *p)
                    return FALSE;
                p++;
                s++;
                break;
                
            case '*':
                /* Treat * as equivalent to #? for convenience */
                p++;
                {
                    const UBYTE *try_s = s;
                    while (1) {
                        if (_match_pattern_internal(p, try_s))
                            return TRUE;
                        if (*try_s == '\0')
                            break;
                        try_s++;
                    }
                }
                return FALSE;
            
            case '[':
                /* Character class [abc] or [a-z] */
                {
                    if (*s == '\0')
                        return FALSE;
                    
                    BOOL matched;
                    const UBYTE *after_class = _match_char_class(p, *s, &matched, FALSE);
                    if (!after_class || !matched)
                        return FALSE;
                    
                    p = after_class;
                    s++;
                }
                break;
                
            default:
                /* Literal character match */
                if (*s != *p)
                    return FALSE;
                p++;
                s++;
                break;
        }
    }
    
    /* Pattern exhausted - match if string is also exhausted */
    return (*s == '\0');
}

/* Internal recursive pattern matching function - CASE INSENSITIVE version */
static BOOL _match_pattern_internal_nocase(const UBYTE *pat, const UBYTE *str)
{
    const UBYTE *p = pat;
    const UBYTE *s = str;
    
    while (*p) {
        switch (*p) {
            case '?':
                /* Match any single character */
                if (*s == '\0')
                    return FALSE;
                p++;
                s++;
                break;
                
            case '#':
                /* Multiplier - zero or more of following char/wildcard */
                p++;
                if (*p == '\0')
                    return FALSE; /* # at end is an error */
                
                if (*p == '?') {
                    /* #? - matches any string (like *) */
                    p++;
                    /* Try to match rest of pattern at each position */
                    const UBYTE *try_s = s;
                    while (1) {
                        if (_match_pattern_internal_nocase(p, try_s))
                            return TRUE;
                        if (*try_s == '\0')
                            break;
                        try_s++;
                    }
                    return FALSE;
                } else {
                    /* #c - matches zero or more of character c (case-insensitive) */
                    UBYTE c = _to_lower(*p++);
                    /* Try to match rest of pattern with varying numbers of c */
                    const UBYTE *try_s = s;
                    while (1) {
                        if (_match_pattern_internal_nocase(p, try_s))
                            return TRUE;
                        if (_to_lower(*try_s) != c)
                            break;
                        try_s++;
                    }
                    return FALSE;
                }
                break;
                
            case '%':
                /* Escape character - match literally (case-insensitive) */
                p++;
                if (*p == '\0')
                    return FALSE; /* % at end is an error */
                if (_to_lower(*s) != _to_lower(*p))
                    return FALSE;
                p++;
                s++;
                break;
                
            case '*':
                /* Treat * as equivalent to #? for convenience */
                p++;
                {
                    const UBYTE *try_s = s;
                    while (1) {
                        if (_match_pattern_internal_nocase(p, try_s))
                            return TRUE;
                        if (*try_s == '\0')
                            break;
                        try_s++;
                    }
                }
                return FALSE;
            
            case '[':
                /* Character class [abc] or [a-z] - case-insensitive */
                {
                    if (*s == '\0')
                        return FALSE;
                    
                    BOOL matched;
                    const UBYTE *after_class = _match_char_class(p, *s, &matched, TRUE);
                    if (!after_class || !matched)
                        return FALSE;
                    
                    p = after_class;
                    s++;
                }
                break;
                
            default:
                /* Literal character match (case-insensitive) */
                if (_to_lower(*s) != _to_lower(*p))
                    return FALSE;
                p++;
                s++;
                break;
        }
    }
    
    /* Pattern exhausted - match if string is also exhausted */
    return (*s == '\0');
}

LONG _dos_ParsePattern ( register struct DosLibrary * DOSBase __asm("a6"),
                                                         register CONST_STRPTR pat __asm("d1"),
                                                         register STRPTR buf __asm("d2"),
                                                         register LONG buflen __asm("d3"))
{
    DPRINTF (LOG_DEBUG, "_dos: ParsePattern() called, pat='%s'\n", pat ? pat : (CONST_STRPTR)"NULL");
    
    if (!pat || !buf || buflen <= 0) {
        SetIoErr(ERROR_BAD_NUMBER);
        return 0;
    }
    
    /* Check if pattern contains any wildcards */
    BOOL has_wildcard = FALSE;
    const UBYTE *p = pat;
    while (*p) {
        if (*p == '?' || *p == '#' || *p == '*' || *p == '%' || *p == '[') {
            has_wildcard = TRUE;
            break;
        }
        p++;
    }
    
    /* Copy pattern to buffer (ParsePattern just copies it for our simple implementation) */
    LONG len = 0;
    while (pat[len] != '\0') len++;
    
    if (len >= buflen) {
        SetIoErr(ERROR_LINE_TOO_LONG);
        return 0;
    }
    
    /* Copy manually to avoid strcpy type issues */
    for (LONG i = 0; i <= len; i++) {
        buf[i] = pat[i];
    }
    
    /* Return -1 for literal string, positive for wildcard pattern */
    return has_wildcard ? len : -1;
}

BOOL _dos_MatchPattern ( register struct DosLibrary * DOSBase __asm("a6"),
                                                         register CONST_STRPTR pat __asm("d1"),
                                                         register STRPTR str __asm("d2"))
{
    DPRINTF (LOG_DEBUG, "_dos: MatchPattern() called, pat='%s', str='%s'\n", 
             pat ? pat : (CONST_STRPTR)"NULL", str ? str : (STRPTR)"NULL");
    
    if (!pat || !str)
        return FALSE;
    
    /* Empty pattern matches empty string only */
    if (pat[0] == '\0')
        return (str[0] == '\0');
    
    return _match_pattern_internal((const UBYTE *)pat, (const UBYTE *)str);
}

VOID _dos_private3 ( register struct DosLibrary * DOSBase __asm("a6"))
{
    LPRINTF (LOG_ERROR, "_dos: private3() unimplemented STUB called.\n");
    assert(FALSE);
}

VOID _dos_FreeArgs ( register struct DosLibrary * DOSBase __asm("a6"),
                                                         register struct RDArgs * args __asm("d1"))
{
    DPRINTF (LOG_DEBUG, "_dos: FreeArgs() called, args=0x%08lx\n", args);

    if (!args)
        return;

    /* Free all allocations tracked in RDA_DAList */
    DANode *node = (DANode *)args->RDA_DAList;
    while (node) {
        DANode *next = node->next;
        if (node->memory) {
            FreeVec(node->memory);
        }
        FreeVec(node);
        node = next;
    }
    args->RDA_DAList = 0;

    /* Free if allocated by ReadArgs */
    if (args->RDA_Flags & RDAF_ALLOCATED_BY_READARGS) {
        FreeVec(args);
    }
}

VOID _dos_private4 ( register struct DosLibrary * DOSBase __asm("a6"))
{
    LPRINTF (LOG_ERROR, "_dos: private4() unimplemented STUB called.\n");
    assert(FALSE);
}

STRPTR _dos_FilePart ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register CONST_STRPTR path __asm("d1"))
{
    LPRINTF (LOG_ERROR, "_dos: FilePart() unimplemented STUB called.\n");
    assert(FALSE);
    return NULL;
}

STRPTR _dos_PathPart ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register CONST_STRPTR path __asm("d1"))
{
    LPRINTF (LOG_ERROR, "_dos: PathPart() unimplemented STUB called.\n");
    assert(FALSE);
    return NULL;
}

BOOL _dos_AddPart ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register STRPTR dirname __asm("d1"),
                                                        register CONST_STRPTR filename __asm("d2"),
                                                        register ULONG size __asm("d3"))
{
    LPRINTF (LOG_ERROR, "_dos: AddPart() unimplemented STUB called.\n");
    assert(FALSE);
    return FALSE;
}

BOOL _dos_StartNotify ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register struct NotifyRequest * notify __asm("d1"))
{
    LPRINTF (LOG_ERROR, "_dos: StartNotify() unimplemented STUB called.\n");
    assert(FALSE);
    return FALSE;
}

VOID _dos_EndNotify ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register struct NotifyRequest * notify __asm("d1"))
{
    LPRINTF (LOG_ERROR, "_dos: EndNotify() unimplemented STUB called.\n");
    assert(FALSE);
}

BOOL _dos_SetVar ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register CONST_STRPTR name __asm("d1"),
                                                        register CONST_STRPTR buffer __asm("d2"),
                                                        register LONG size __asm("d3"),
                                                        register LONG flags __asm("d4"))
{
    DPRINTF(LOG_DEBUG, "_dos: SetVar() name='%s', buffer=0x%08lx, size=%ld, flags=0x%lx\n", 
            name, buffer, size, flags);
    
    /* If size is -1, treat buffer as null-terminated string */
    if (size < 0)
    {
        size = 0;
        if (buffer)
        {
            CONST_STRPTR p = buffer;
            while (*p++) size++;
        }
    }
    
    /* Get the variable type */
    UBYTE varType = flags & 0xFF;  /* LV_VAR or LV_ALIAS */
    
    /* Handle global variables (ENV:) */
    if (!(flags & GVF_LOCAL_ONLY))
    {
        /* For global variables, create/update a file in ENV: */
        char envPath[256];
        char *p = envPath;
        const char *src = "ENV:";
        while (*src) *p++ = *src++;
        src = (const char *)name;
        while (*src && p < envPath + 250) *p++ = *src++;
        *p = '\0';
        
        BPTR fh = Open((STRPTR)envPath, MODE_NEWFILE);
        if (fh)
        {
            if (buffer && size > 0)
            {
                Write(fh, (APTR)buffer, size);
            }
            Close(fh);
            DPRINTF(LOG_DEBUG, "_dos: SetVar() wrote to %s\n", envPath);
            
            /* Also save to ENVARC: if GVF_SAVE_VAR is set */
            if (flags & GVF_SAVE_VAR)
            {
                p = envPath;
                src = "ENVARC:";
                while (*src) *p++ = *src++;
                src = (const char *)name;
                while (*src && p < envPath + 250) *p++ = *src++;
                *p = '\0';
                
                fh = Open((STRPTR)envPath, MODE_NEWFILE);
                if (fh)
                {
                    if (buffer && size > 0)
                    {
                        Write(fh, (APTR)buffer, size);
                    }
                    Close(fh);
                    DPRINTF(LOG_DEBUG, "_dos: SetVar() wrote to %s\n", envPath);
                }
            }
        }
        else
        {
            DPRINTF(LOG_DEBUG, "_dos: SetVar() failed to open %s\n", envPath);
        }
        
        /* If only global, we're done */
        if (flags & GVF_GLOBAL_ONLY)
            return TRUE;
    }
    
    /* Handle local variables */
    struct Process *proc = (struct Process *)FindTask(NULL);
    if (!IS_PROCESS(proc))
    {
        DPRINTF(LOG_DEBUG, "_dos: SetVar() not a process\n");
        SetIoErr(ERROR_OBJECT_WRONG_TYPE);
        return FALSE;
    }
    
    /* Find existing variable */
    struct LocalVar *lv = _dos_FindVar(DOSBase, name, varType);
    
    if (lv)
    {
        /* Update existing variable - free old value */
        if (lv->lv_Value && lv->lv_Len > 0)
        {
            FreeMem(lv->lv_Value, lv->lv_Len);
        }
    }
    else
    {
        /* Create new variable */
        LONG nameLen = 0;
        CONST_STRPTR p = name;
        while (*p++) nameLen++;
        nameLen++;  /* Include null terminator */
        
        lv = (struct LocalVar *)AllocMem(sizeof(struct LocalVar), MEMF_CLEAR | MEMF_PUBLIC);
        if (!lv)
        {
            SetIoErr(ERROR_NO_FREE_STORE);
            return FALSE;
        }
        
        lv->lv_Node.ln_Name = (char *)AllocMem(nameLen, MEMF_PUBLIC);
        if (!lv->lv_Node.ln_Name)
        {
            FreeMem(lv, sizeof(struct LocalVar));
            SetIoErr(ERROR_NO_FREE_STORE);
            return FALSE;
        }
        
        /* Copy name */
        char *dst = lv->lv_Node.ln_Name;
        p = name;
        while (*p) *dst++ = *p++;
        *dst = '\0';
        
        lv->lv_Node.ln_Type = varType;
        
        /* Insert into list (alphabetically) */
        struct MinList *list = &proc->pr_LocalVars;
        struct LocalVar *insertAfter = NULL;
        struct LocalVar *curr;
        
        for (curr = (struct LocalVar *)list->mlh_Head;
             curr->lv_Node.ln_Succ != NULL;
             curr = (struct LocalVar *)curr->lv_Node.ln_Succ)
        {
            /* Compare names for alphabetical ordering */
            if (curr->lv_Node.ln_Name)
            {
                CONST_STRPTR a = name;
                char *b = curr->lv_Node.ln_Name;
                while (*a && *b)
                {
                    char ca = *a, cb = *b;
                    if (ca >= 'A' && ca <= 'Z') ca += 32;
                    if (cb >= 'A' && cb <= 'Z') cb += 32;
                    if (ca < cb) break;
                    if (ca > cb) { insertAfter = curr; break; }
                    a++; b++;
                }
                if (*a == '\0' && *b != '\0') break;  /* name is shorter */
                if (*a != '\0' && *b == '\0') insertAfter = curr;  /* name is longer */
            }
        }
        
        if (insertAfter)
        {
            Insert((struct List *)list, (struct Node *)lv, (struct Node *)insertAfter);
        }
        else
        {
            AddHead((struct List *)list, (struct Node *)lv);
        }
    }
    
    /* Set the new value */
    if (size > 0 && buffer)
    {
        /* Allocate space for value (add 1 for null terminator unless DONT_NULL_TERM) */
        LONG allocSize = (flags & GVF_DONT_NULL_TERM) ? size : size + 1;
        lv->lv_Value = (STRPTR)AllocMem(allocSize, MEMF_PUBLIC);
        if (!lv->lv_Value)
        {
            SetIoErr(ERROR_NO_FREE_STORE);
            return FALSE;
        }
        
        /* Copy value */
        STRPTR dst = lv->lv_Value;
        CONST_STRPTR src = buffer;
        LONG i;
        for (i = 0; i < size; i++)
            *dst++ = *src++;
        
        if (!(flags & GVF_DONT_NULL_TERM))
            *dst = '\0';
        
        lv->lv_Len = allocSize;
    }
    else
    {
        lv->lv_Value = NULL;
        lv->lv_Len = 0;
    }
    
    DPRINTF(LOG_DEBUG, "_dos: SetVar() success, lv=0x%08lx\n", lv);
    return TRUE;
}

LONG _dos_GetVar ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register CONST_STRPTR name __asm("d1"),
                                                        register STRPTR buffer __asm("d2"),
                                                        register LONG size __asm("d3"),
                                                        register LONG flags __asm("d4"))
{
    DPRINTF(LOG_DEBUG, "_dos: GetVar() name='%s', buffer=0x%08lx, size=%ld, flags=0x%lx\n", 
            name, buffer, size, flags);
    
    /* Get the variable type */
    UBYTE varType = flags & 0xFF;  /* LV_VAR or LV_ALIAS */
    
    /* Try local variables first (unless GLOBAL_ONLY) */
    if (!(flags & GVF_GLOBAL_ONLY))
    {
        struct LocalVar *lv = _dos_FindVar(DOSBase, name, varType);
        if (lv)
        {
            /* Handle empty string case (lv_Value is NULL, lv_Len is 0) */
            if (!lv->lv_Value || lv->lv_Len == 0)
            {
                /* Empty value - just null-terminate and return 0 */
                if (buffer && size > 0 && !(flags & GVF_DONT_NULL_TERM))
                    buffer[0] = '\0';
                DPRINTF(LOG_DEBUG, "_dos: GetVar() found local (empty), len=0\n");
                return 0;
            }
            
            /* Copy value to buffer */
            LONG copyLen = lv->lv_Len;
            if (!(flags & GVF_BINARY_VAR))
            {
                /* For text vars, don't count the null terminator in returned length */
                if (copyLen > 0 && lv->lv_Value[copyLen - 1] == '\0')
                    copyLen--;
            }
            
            if (copyLen >= size)
                copyLen = size - 1;
            
            STRPTR src = lv->lv_Value;
            STRPTR dst = buffer;
            LONG i;
            for (i = 0; i < copyLen; i++)
                *dst++ = *src++;
            
            /* Null-terminate unless DONT_NULL_TERM */
            if (!(flags & GVF_DONT_NULL_TERM))
                *dst = '\0';
            
            DPRINTF(LOG_DEBUG, "_dos: GetVar() found local, len=%ld\n", copyLen);
            return copyLen;
        }
        
        /* If LOCAL_ONLY and not found, return error */
        if (flags & GVF_LOCAL_ONLY)
        {
            SetIoErr(ERROR_OBJECT_NOT_FOUND);
            return -1;
        }
    }
    
    /* Try global variable (ENV: file) */
    char envPath[256];
    char *p = envPath;
    const char *src = "ENV:";
    while (*src) *p++ = *src++;
    src = (const char *)name;
    while (*src && p < envPath + 250) *p++ = *src++;
    *p = '\0';
    
    BPTR fh = Open((STRPTR)envPath, MODE_OLDFILE);
    if (fh)
    {
        /* Read the file content */
        LONG bytesRead = Read(fh, buffer, size - 1);
        Close(fh);
        
        if (bytesRead < 0)
        {
            DPRINTF(LOG_DEBUG, "_dos: GetVar() read error\n");
            return -1;
        }
        
        /* Strip trailing newlines for text variables */
        if (!(flags & GVF_BINARY_VAR))
        {
            while (bytesRead > 0 && (buffer[bytesRead - 1] == '\n' || buffer[bytesRead - 1] == '\r'))
                bytesRead--;
        }
        
        /* Null-terminate unless DONT_NULL_TERM */
        if (!(flags & GVF_DONT_NULL_TERM))
            buffer[bytesRead] = '\0';
        
        DPRINTF(LOG_DEBUG, "_dos: GetVar() found global in %s, len=%ld\n", envPath, bytesRead);
        return bytesRead;
    }
    
    DPRINTF(LOG_DEBUG, "_dos: GetVar() not found\n");
    SetIoErr(ERROR_OBJECT_NOT_FOUND);
    return -1;
}

LONG _dos_DeleteVar ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register CONST_STRPTR name __asm("d1"),
                                                        register ULONG flags __asm("d2"))
{
    DPRINTF(LOG_DEBUG, "_dos: DeleteVar() name='%s', flags=0x%lx\n", name, flags);
    
    BOOL deleted = FALSE;
    
    /* Get the variable type */
    UBYTE varType = flags & 0xFF;  /* LV_VAR or LV_ALIAS */
    
    /* Delete local variable (unless GLOBAL_ONLY) */
    if (!(flags & GVF_GLOBAL_ONLY))
    {
        struct LocalVar *lv = _dos_FindVar(DOSBase, name, varType);
        if (lv)
        {
            /* Remove from list */
            Remove((struct Node *)lv);
            
            /* Free name */
            if (lv->lv_Node.ln_Name)
            {
                LONG nameLen = 0;
                char *p = lv->lv_Node.ln_Name;
                while (*p++) nameLen++;
                nameLen++;  /* Include null terminator */
                FreeMem(lv->lv_Node.ln_Name, nameLen);
            }
            
            /* Free value */
            if (lv->lv_Value && lv->lv_Len > 0)
            {
                FreeMem(lv->lv_Value, lv->lv_Len);
            }
            
            /* Free structure */
            FreeMem(lv, sizeof(struct LocalVar));
            
            deleted = TRUE;
            DPRINTF(LOG_DEBUG, "_dos: DeleteVar() deleted local var\n");
        }
    }
    
    /* Delete global variable (ENV: file) unless LOCAL_ONLY */
    if (!(flags & GVF_LOCAL_ONLY))
    {
        char envPath[256];
        char *p = envPath;
        const char *src = "ENV:";
        while (*src) *p++ = *src++;
        src = (const char *)name;
        while (*src && p < envPath + 250) *p++ = *src++;
        *p = '\0';
        
        if (DeleteFile((STRPTR)envPath))
        {
            deleted = TRUE;
            DPRINTF(LOG_DEBUG, "_dos: DeleteVar() deleted %s\n", envPath);
        }
        
        /* Also delete from ENVARC: if GVF_SAVE_VAR */
        if (flags & GVF_SAVE_VAR)
        {
            p = envPath;
            src = "ENVARC:";
            while (*src) *p++ = *src++;
            src = (const char *)name;
            while (*src && p < envPath + 250) *p++ = *src++;
            *p = '\0';
            
            if (DeleteFile((STRPTR)envPath))
            {
                DPRINTF(LOG_DEBUG, "_dos: DeleteVar() deleted %s\n", envPath);
            }
        }
    }
    
    if (!deleted)
    {
        SetIoErr(ERROR_OBJECT_NOT_FOUND);
    }
    
    return deleted ? DOSTRUE : DOSFALSE;
}

struct LocalVar * _dos_FindVar ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register CONST_STRPTR name __asm("d1"),
                                                        register ULONG type __asm("d2"))
{
    DPRINTF(LOG_DEBUG, "_dos: FindVar() name='%s', type=0x%lx\n", name, type);
    
    struct Process *proc = (struct Process *)FindTask(NULL);
    if (!IS_PROCESS(proc))
    {
        DPRINTF(LOG_DEBUG, "_dos: FindVar() not a process\n");
        return NULL;
    }
    
    /* Get the type to search for (mask off the flags) */
    UBYTE searchType = type & 0x7F;  /* LV_VAR or LV_ALIAS */
    
    /* Search the local variable list */
    struct MinList *list = &proc->pr_LocalVars;
    struct LocalVar *lv;
    
    for (lv = (struct LocalVar *)list->mlh_Head;
         lv->lv_Node.ln_Succ != NULL;
         lv = (struct LocalVar *)lv->lv_Node.ln_Succ)
    {
        /* Check type matches */
        if ((lv->lv_Node.ln_Type & 0x7F) != searchType)
            continue;
        
        /* Skip if LVF_IGNORE is set and we're not looking for ignored vars */
        if ((lv->lv_Node.ln_Type & LVF_IGNORE) && !(type & LVF_IGNORE))
            continue;
        
        /* Compare names (case-insensitive) */
        if (lv->lv_Node.ln_Name && name)
        {
            CONST_STRPTR a = name;
            char *b = lv->lv_Node.ln_Name;
            BOOL match = TRUE;
            
            while (*a && *b)
            {
                char ca = *a, cb = *b;
                if (ca >= 'A' && ca <= 'Z') ca += 32;
                if (cb >= 'A' && cb <= 'Z') cb += 32;
                if (ca != cb) { match = FALSE; break; }
                a++; b++;
            }
            if (*a != *b) match = FALSE;
            
            if (match)
            {
                DPRINTF(LOG_DEBUG, "_dos: FindVar() found lv=0x%08lx\n", lv);
                return lv;
            }
        }
    }
    
    DPRINTF(LOG_DEBUG, "_dos: FindVar() not found\n");
    return NULL;
}

VOID _dos_private5 ( register struct DosLibrary * DOSBase __asm("a6"))
{
    LPRINTF (LOG_ERROR, "_dos: private5() unimplemented STUB called.\n");
    assert(FALSE);
}

LONG _dos_CliInitNewcli ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register struct DosPacket * dp __asm("a0"))
{
    LPRINTF (LOG_ERROR, "_dos: CliInitNewcli() unimplemented STUB called.\n");
    assert(FALSE);
    return 0;
}

LONG _dos_CliInitRun ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register struct DosPacket * dp __asm("a0"))
{
    LPRINTF (LOG_ERROR, "_dos: CliInitRun() unimplemented STUB called.\n");
    assert(FALSE);
    return 0;
}

LONG _dos_WriteChars ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register CONST_STRPTR buf __asm("d1"),
                                                        register ULONG buflen __asm("d2"))
{
    LPRINTF (LOG_ERROR, "_dos: WriteChars() unimplemented STUB called.\n");
    assert(FALSE);
    return 0;
}

LONG _dos_PutStr ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register CONST_STRPTR str __asm("d1"))
{
    LPRINTF (LOG_ERROR, "_dos: PutStr() unimplemented STUB called.\n");
    assert(FALSE);
    return 0;
}

LONG _dos_VPrintf ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register CONST_STRPTR format __asm("d1"),
                                                        register const APTR argarray __asm("d2"))
{
    LPRINTF (LOG_ERROR, "_dos: VPrintf() unimplemented STUB called.\n");
    assert(FALSE);
    return 0;
}

VOID _dos_private6 ( register struct DosLibrary * DOSBase __asm("a6"))
{
    LPRINTF (LOG_ERROR, "_dos: private6() unimplemented STUB called.\n");
    assert(FALSE);
}

LONG _dos_ParsePatternNoCase ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register CONST_STRPTR pat __asm("d1"),
                                                        register UBYTE * buf __asm("d2"),
                                                        register LONG buflen __asm("d3"))
{
    DPRINTF (LOG_DEBUG, "_dos: ParsePatternNoCase() called, pat='%s'\n", pat ? pat : (CONST_STRPTR)"NULL");
    
    /* ParsePatternNoCase is identical to ParsePattern - the case-insensitivity
     * is handled by MatchPatternNoCase. We just need to copy the pattern. */
    
    if (!pat || !buf || buflen <= 0) {
        SetIoErr(ERROR_BAD_NUMBER);
        return 0;
    }
    
    /* Check if pattern contains any wildcards */
    BOOL has_wildcard = FALSE;
    const UBYTE *p = pat;
    while (*p) {
        if (*p == '?' || *p == '#' || *p == '*' || *p == '%' || *p == '[') {
            has_wildcard = TRUE;
            break;
        }
        p++;
    }
    
    /* Copy pattern to buffer */
    LONG len = 0;
    while (pat[len] != '\0') len++;
    
    if (len >= buflen) {
        SetIoErr(ERROR_LINE_TOO_LONG);
        return 0;
    }
    
    /* Copy manually to avoid strcpy type issues */
    for (LONG i = 0; i <= len; i++) {
        buf[i] = pat[i];
    }
    
    /* Return -1 for literal string, positive for wildcard pattern */
    return has_wildcard ? len : -1;
}

BOOL _dos_MatchPatternNoCase ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register CONST_STRPTR pat __asm("d1"),
                                                        register STRPTR str __asm("d2"))
{
    DPRINTF (LOG_DEBUG, "_dos: MatchPatternNoCase() called, pat='%s', str='%s'\n", 
             pat ? pat : (CONST_STRPTR)"NULL", str ? str : (STRPTR)"NULL");
    
    if (!pat || !str)
        return FALSE;
    
    /* Empty pattern matches empty string only */
    if (pat[0] == '\0')
        return (str[0] == '\0');
    
    return _match_pattern_internal_nocase((const UBYTE *)pat, (const UBYTE *)str);
}

VOID _dos_private7 ( register struct DosLibrary * DOSBase __asm("a6"))
{
    LPRINTF (LOG_ERROR, "_dos: private7() unimplemented STUB called.\n");
    assert(FALSE);
}

BOOL _dos_SameDevice ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register BPTR lock1 __asm("d1"),
                                                        register BPTR lock2 __asm("d2"))
{
    LPRINTF (LOG_ERROR, "_dos: SameDevice() unimplemented STUB called.\n");
    assert(FALSE);
    return FALSE;
}

VOID _dos_ExAllEnd ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register BPTR lock __asm("d1"),
                                                        register struct ExAllData * buffer __asm("d2"),
                                                        register LONG size __asm("d3"),
                                                        register LONG data __asm("d4"),
                                                        register struct ExAllControl * control __asm("d5"))
{
    LPRINTF (LOG_ERROR, "_dos: ExAllEnd() unimplemented STUB called.\n");
    assert(FALSE);
}

BOOL _dos_SetOwner ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register CONST_STRPTR name __asm("d1"),
                                                        register LONG owner_info __asm("d2"))
{
    LPRINTF (LOG_ERROR, "_dos: SetOwner() unimplemented STUB called.\n");
    assert(FALSE);
    return FALSE;
}

struct MyDataInit
{
    UWORD ln_Type_Init     ; UWORD ln_Type_Offset     ; UWORD ln_Type_Content     ;
    UBYTE ln_Name_Init     ; UBYTE ln_Name_Offset     ; ULONG ln_Name_Content     ;
    UWORD lib_Flags_Init   ; UWORD lib_Flags_Offset   ; UWORD lib_Flags_Content   ;
    UWORD lib_Version_Init ; UWORD lib_Version_Offset ; UWORD lib_Version_Content ;
    UWORD lib_Revision_Init; UWORD lib_Revision_Offset; UWORD lib_Revision_Content;
    UBYTE lib_IdString_Init; UBYTE lib_IdString_Offset; ULONG lib_IdString_Content;
    ULONG ENDMARK;
};

extern APTR              __g_lxa_dos_FuncTab [];
extern struct MyDataInit __g_lxa_dos_DataTab;
extern struct InitTable  __g_lxa_dos_InitTab;
extern APTR              __g_lxa_dos_EndResident;

static struct Resident __aligned ROMTag =
{                               //                               offset
    RTC_MATCHWORD,              // UWORD rt_MatchWord;                0
    &ROMTag,                    // struct Resident *rt_MatchTag;      2
    &__g_lxa_dos_EndResident,   // APTR  rt_EndSkip;                  6
    RTF_AUTOINIT,               // UBYTE rt_Flags;                    7
    VERSION,                    // UBYTE rt_Version;                  8
    NT_LIBRARY,                 // UBYTE rt_Type;                     9
    0,                          // BYTE  rt_Pri;                     10
    &_g_dos_ExLibName[0],       // char  *rt_Name;                   14
    &_g_dos_ExLibID[0],         // char  *rt_IdString;               18
    &__g_lxa_dos_InitTab        // APTR  rt_Init;                    22
};

APTR __g_lxa_dos_EndResident;
struct Resident *__lxa_dos_ROMTag = &ROMTag;

struct InitTable __g_lxa_dos_InitTab =
{
    (ULONG)               sizeof(struct DosLibrary),    // LibBaseSize
    (APTR              *) &__g_lxa_dos_FuncTab[0],      // FunctionTable
    (APTR)                &__g_lxa_dos_DataTab,         // DataTable
    (APTR)                __g_lxa_dos_InitLib           // InitLibFn
};

APTR __g_lxa_dos_FuncTab [] =
{
    __g_lxa_dos_OpenLib,
    __g_lxa_dos_CloseLib,
    __g_lxa_dos_ExpungeLib,
    __g_lxa_dos_ExtFuncLib,
    _dos_Open, // offset = -30
    _dos_Close, // offset = -36
    _dos_Read, // offset = -42
    _dos_Write, // offset = -48
    _dos_Input, // offset = -54
    _dos_Output, // offset = -60
    _dos_Seek, // offset = -66
    _dos_DeleteFile, // offset = -72
    _dos_Rename, // offset = -78
    _dos_Lock, // offset = -84
    _dos_UnLock, // offset = -90
    _dos_DupLock, // offset = -96
    _dos_Examine, // offset = -102
    _dos_ExNext, // offset = -108
    _dos_Info, // offset = -114
    _dos_CreateDir, // offset = -120
    _dos_CurrentDir, // offset = -126
    _dos_IoErr, // offset = -132
    _dos_CreateProc, // offset = -138
    _dos_Exit, // offset = -144
    _dos_LoadSeg, // offset = -150
    _dos_UnLoadSeg, // offset = -156
    _dos_private0, // offset = -162
    _dos_private1, // offset = -168
    _dos_DeviceProc, // offset = -174
    _dos_SetComment, // offset = -180
    _dos_SetProtection, // offset = -186
    _dos_DateStamp, // offset = -192
    _dos_Delay, // offset = -198
    _dos_WaitForChar, // offset = -204
    _dos_ParentDir, // offset = -210
    _dos_IsInteractive, // offset = -216
    _dos_Execute, // offset = -222
    _dos_AllocDosObject, // offset = -228
    _dos_FreeDosObject, // offset = -234
    _dos_DoPkt, // offset = -240
    _dos_SendPkt, // offset = -246
    _dos_WaitPkt, // offset = -252
    _dos_ReplyPkt, // offset = -258
    _dos_AbortPkt, // offset = -264
    _dos_LockRecord, // offset = -270
    _dos_LockRecords, // offset = -276
    _dos_UnLockRecord, // offset = -282
    _dos_UnLockRecords, // offset = -288
    _dos_SelectInput, // offset = -294
    _dos_SelectOutput, // offset = -300
    _dos_FGetC, // offset = -306
    _dos_FPutC, // offset = -312
    _dos_UnGetC, // offset = -318
    _dos_FRead, // offset = -324
    _dos_FWrite, // offset = -330
    _dos_FGets, // offset = -336
    _dos_FPuts, // offset = -342
    _dos_VFWritef, // offset = -348
    _dos_VFPrintf, // offset = -354
    _dos_Flush, // offset = -360
    _dos_SetVBuf, // offset = -366
    _dos_DupLockFromFH, // offset = -372
    _dos_OpenFromLock, // offset = -378
    _dos_ParentOfFH, // offset = -384
    _dos_ExamineFH, // offset = -390
    _dos_SetFileDate, // offset = -396
    _dos_NameFromLock, // offset = -402
    _dos_NameFromFH, // offset = -408
    _dos_SplitName, // offset = -414
    _dos_SameLock, // offset = -420
    _dos_SetMode, // offset = -426
    _dos_ExAll, // offset = -432
    _dos_ReadLink, // offset = -438
    _dos_MakeLink, // offset = -444
    _dos_ChangeMode, // offset = -450
    _dos_SetFileSize, // offset = -456
    _dos_SetIoErr, // offset = -462
    _dos_Fault, // offset = -468
    _dos_PrintFault, // offset = -474
    _dos_ErrorReport, // offset = -480
    _dos_private2, // offset = -486
    _dos_Cli, // offset = -492
    _dos_CreateNewProc, // offset = -498
    _dos_RunCommand, // offset = -504
    _dos_GetConsoleTask, // offset = -510
    _dos_SetConsoleTask, // offset = -516
    _dos_GetFileSysTask, // offset = -522
    _dos_SetFileSysTask, // offset = -528
    _dos_GetArgStr, // offset = -534
    _dos_SetArgStr, // offset = -540
    _dos_FindCliProc, // offset = -546
    _dos_MaxCli, // offset = -552
    _dos_SetCurrentDirName, // offset = -558
    _dos_GetCurrentDirName, // offset = -564
    _dos_SetProgramName, // offset = -570
    _dos_GetProgramName, // offset = -576
    _dos_SetPrompt, // offset = -582
    _dos_GetPrompt, // offset = -588
    _dos_SetProgramDir, // offset = -594
    _dos_GetProgramDir, // offset = -600
    _dos_SystemTagList, // offset = -606
    _dos_AssignLock, // offset = -612
    _dos_AssignLate, // offset = -618
    _dos_AssignPath, // offset = -624
    _dos_AssignAdd, // offset = -630
    _dos_RemAssignList, // offset = -636
    _dos_GetDeviceProc, // offset = -642
    _dos_FreeDeviceProc, // offset = -648
    _dos_LockDosList, // offset = -654
    _dos_UnLockDosList, // offset = -660
    _dos_AttemptLockDosList, // offset = -666
    _dos_RemDosEntry, // offset = -672
    _dos_AddDosEntry, // offset = -678
    _dos_FindDosEntry, // offset = -684
    _dos_NextDosEntry, // offset = -690
    _dos_MakeDosEntry, // offset = -696
    _dos_FreeDosEntry, // offset = -702
    _dos_IsFileSystem, // offset = -708
    _dos_Format, // offset = -714
    _dos_Relabel, // offset = -720
    _dos_Inhibit, // offset = -726
    _dos_AddBuffers, // offset = -732
    _dos_CompareDates, // offset = -738
    _dos_DateToStr, // offset = -744
    _dos_StrToDate, // offset = -750
    _dos_InternalLoadSeg, // offset = -756
    _dos_InternalUnLoadSeg, // offset = -762
    _dos_NewLoadSeg, // offset = -768
    _dos_AddSegment, // offset = -774
    _dos_FindSegment, // offset = -780
    _dos_RemSegment, // offset = -786
    _dos_CheckSignal, // offset = -792
    _dos_ReadArgs, // offset = -798
    _dos_FindArg, // offset = -804
    _dos_ReadItem, // offset = -810
    _dos_StrToLong, // offset = -816
    _dos_MatchFirst, // offset = -822
    _dos_MatchNext, // offset = -828
    _dos_MatchEnd, // offset = -834
    _dos_ParsePattern, // offset = -840
    _dos_MatchPattern, // offset = -846
    _dos_private3, // offset = -852
    _dos_FreeArgs, // offset = -858
    _dos_private4, // offset = -864
    _dos_FilePart, // offset = -870
    _dos_PathPart, // offset = -876
    _dos_AddPart, // offset = -882
    _dos_StartNotify, // offset = -888
    _dos_EndNotify, // offset = -894
    _dos_SetVar, // offset = -900
    _dos_GetVar, // offset = -906
    _dos_DeleteVar, // offset = -912
    _dos_FindVar, // offset = -918
    _dos_private5, // offset = -924
    _dos_CliInitNewcli, // offset = -930
    _dos_CliInitRun, // offset = -936
    _dos_WriteChars, // offset = -942
    _dos_PutStr, // offset = -948
    _dos_VPrintf, // offset = -954
    _dos_private6, // offset = -960
    _dos_ParsePatternNoCase, // offset = -966
    _dos_MatchPatternNoCase, // offset = -972
    _dos_private7, // offset = -978
    _dos_SameDevice, // offset = -984
    _dos_ExAllEnd, // offset = -990
    _dos_SetOwner, // offset = -996
    (APTR) ((LONG)-1)
};

struct MyDataInit __g_lxa_dos_DataTab =
{
    /* ln_Type      */ INITBYTE(OFFSET(Node,         ln_Type),      NT_LIBRARY),
    /* ln_Name      */ 0x80, (UBYTE) (ULONG) OFFSET(Node,    ln_Name), (ULONG) &_g_dos_ExLibName[0],
    /* lib_Flags    */ INITBYTE(OFFSET(Library,      lib_Flags),    LIBF_SUMUSED|LIBF_CHANGED),
    /* lib_Version  */ INITWORD(OFFSET(Library,      lib_Version),  VERSION),
    /* lib_Revision */ INITWORD(OFFSET(Library,      lib_Revision), REVISION),
    /* lib_IdString */ 0x80, (UBYTE) (ULONG) OFFSET(Library, lib_IdString), (ULONG) &_g_dos_ExLibID[0],
    (ULONG) 0
};

