
#include <inttypes.h>

#include <exec/types.h>
#include <exec/memory.h>
#include <exec/libraries.h>
#include <exec/execbase.h>
#include <exec/resident.h>
#include <exec/initializers.h>

//#include <proto/exec.h>

//#include <clib/alib_protos.h>
#include <clib/exec_protos.h>
//#include <clib/dos_protos.h>
//#include <clib/intuition_protos.h>
//#include <clib/graphics_protos.h>
//#include <clib/console_protos.h>
//#include <clib/diskfont_protos.h>

#include <inline/exec.h>
//#include <inline/dos.h>
//#include <inline/intuition.h>
//#include <inline/graphics.h>
//#include <inline/diskfont.h>

#include "util.h"
#include "lxa_dos.h"

#define NUM_EXEC_FUNCS 130
#define EXEC_FUNCTABLE_ENTRY(___off) (NUM_EXEC_FUNCS+(___off/6))

// memory map
#define EXCEPTION_VECTORS_START 0
#define EXCEPTION_VECTORS_SIZE  1024
#define EXCEPTION_VECTORS_END   EXCEPTION_VECTORS_START + EXCEPTION_VECTORS_SIZE - 1

#define EXEC_VECTORS_START      EXCEPTION_VECTORS_END + 1
#define EXEC_VECTORS_SIZE       NUM_EXEC_FUNCS * 6
#define EXEC_VECTORS_END        EXEC_VECTORS_START + EXEC_VECTORS_SIZE -1

#define EXEC_BASE_START         EXEC_VECTORS_END + 1
#define EXEC_BASE_SIZE          sizeof (struct ExecBase)
#define EXEC_BASE_END           EXEC_BASE_START + EXEC_BASE_SIZE -1

#define EXEC_MH_START           EXEC_BASE_END + 1
#define EXEC_MH_SIZE            sizeof (struct MemHeader)
#define EXEC_MH_END             EXEC_MH_START + EXEC_MH_SIZE -1

#define RAM_START               EXEC_MH_END + 1
#define RAM_SIZE                10 * 1024 * 1024
#define RAM_END                 RAM_START + RAM_SIZE - 1

#define VERSION  1
#define REVISION 1

struct JumpVec
{
    unsigned short jmp;
    void *vec;
};

static struct JumpVec  *g_ExecJumpTable = (struct JumpVec *) ((uint8_t *)EXEC_VECTORS_START);
struct ExecBase        *SysBase         = (struct ExecBase*) ((uint8_t *)EXEC_BASE_START);

#define JMPINSTR 0x4ef9

static void __saveds _exec_unimplemented_call ( register struct ExecBase  *exb __asm("a6") )
{
	assert (FALSE);
}

static ULONG __saveds _exec_Supervisor ( register struct ExecBase * __libBase __asm("a6"),
                                                        register VOID (*__fpt)()  __asm("d7"))
{
    lprintf ("_exec: Supervisor unimplemented STUB called.");
    assert(FALSE);
}

static void __saveds _exec_InitCode ( register struct ExecBase * __libBase __asm("a6"),
                                                        register ULONG ___startClass  __asm("d0"),
                                                        register ULONG ___version  __asm("d1"))
{
    lprintf ("_exec: InitCode unimplemented STUB called.");
    assert(FALSE);
}

static void __saveds _exec_InitStruct ( register struct ExecBase * __libBase __asm("a6"),
                                                        register const APTR ___initTable  __asm("a1"),
                                                        register APTR ___memory  __asm("a2"),
                                                        register ULONG ___size  __asm("d0"))
{
    lprintf ("_exec: InitStruct unimplemented STUB called.");
    assert(FALSE);
}

static struct Library * __saveds _exec_MakeLibrary ( register struct ExecBase * __libBase __asm("a6"),
                                                        register const APTR ___funcInit  __asm("a0"),
                                                        register const APTR ___structInit  __asm("a1"),
                                                        register VOID (*___libInit)()  __asm("a2"),
                                                        register ULONG ___dataSize  __asm("d0"),
                                                        register ULONG ___segList  __asm("d1"))
{
    lprintf ("exec: MakeLibrary called, ___funcInit=0x%08x, ___structInit=0x%08x, ___dataSize=%d\n", ___funcInit, ___structInit, ___dataSize);

    struct Library *library;
    ULONG negsize;
    ULONG count = 0;

    if (*(WORD *)___funcInit==-1)
    {
        WORD *fp=(WORD *)___funcInit+1;
        while (*fp++!=-1)
            count++;
    }
    else
    {
        void **fp=(void **)___funcInit;
        while (*fp++!=(void *)-1)
            count++;
    }
    lprintf ("exec: MakeLibrary count=%d\n", count);

    negsize = count * 6;

    char *mem = AllocMem (___dataSize+negsize, MEMF_PUBLIC|MEMF_CLEAR);

    if (mem)
    {
        library = (struct Library *)(mem+negsize);

        if(*(WORD *)___funcInit==-1)
            MakeFunctions (library, (WORD *)___funcInit+1, (WORD *)___funcInit);
        else
            MakeFunctions (library, ___funcInit, NULL);

        library->lib_NegSize = negsize;
        library->lib_PosSize = ___dataSize;

        if (___structInit)
            InitStruct (___structInit, library, 0);

        if (___libInit)
			assert (FALSE); // FIXME: implement
    }
	else
	{
		lprintf ("exec: MakeLibrary out of memory!\n");
		assert(FALSE);
	}

	return library;
}




static void __saveds _exec_MakeFunctions ( register struct ExecBase * __libBase __asm("a6"),
                                                        register APTR ___target  __asm("a0"),
                                                        register const APTR ___functionArray  __asm("a1"),
                                                        register const APTR ___funcDispBase  __asm("a2"))
{
    lprintf ("_exec: MakeFunctions unimplemented STUB called.");
    assert(FALSE);
}

static struct Resident * __saveds _exec_FindResident ( register struct ExecBase * __libBase __asm("a6"),
                                                        register CONST_STRPTR ___name  __asm("a1"))
{
    lprintf ("_exec: FindResident unimplemented STUB called.");
    assert(FALSE);
}

static APTR __saveds _exec_InitResident ( register struct ExecBase * __libBase __asm("a6"),
                                                        register const struct Resident * ___resident  __asm("a1"),
                                                        register ULONG ___segList  __asm("d1"))
{
    lprintf ("_exec: InitResident unimplemented STUB called.");
    assert(FALSE);
}

static void __saveds _exec_Alert ( register struct ExecBase * __libBase __asm("a6"),
                                                        register ULONG ___alertNum  __asm("d7"))
{
    lprintf ("_exec: Alert unimplemented STUB called.");
    assert(FALSE);
}

static void __saveds _exec_Debug ( register struct ExecBase * __libBase __asm("a6"),
                                                        register ULONG ___flags  __asm("d0"))
{
    lprintf ("_exec: Debug unimplemented STUB called.");
    assert(FALSE);
}

static void __saveds _exec_Disable ( register struct ExecBase * __libBase __asm("a6"))
{
    lprintf ("_exec: Disable unimplemented STUB called.");
    assert(FALSE);
}

static void __saveds _exec_Enable ( register struct ExecBase * __libBase __asm("a6"))
{
    lprintf ("_exec: Enable unimplemented STUB called.");
    assert(FALSE);
}

static void __saveds _exec_Forbid ( register struct ExecBase * __libBase __asm("a6"))
{
    lprintf ("_exec: WARNING: Forbid(): unimplemented STUB called.\n");
    // FIXME
}

static void __saveds _exec_Permit ( register struct ExecBase * __libBase __asm("a6"))
{
    lprintf ("_exec: Permit unimplemented STUB called.");
    assert(FALSE);
}

static ULONG __saveds _exec_SetSR ( register struct ExecBase * __libBase __asm("a6"),
                                                        register ULONG ___newSR  __asm("d0"),
                                                        register ULONG ___mask  __asm("d1"))
{
    lprintf ("_exec: SetSR unimplemented STUB called.");
    assert(FALSE);
}

static APTR __saveds _exec_SuperState ( register struct ExecBase * __libBase __asm("a6"))
{
    lprintf ("_exec: SuperState unimplemented STUB called.");
    assert(FALSE);
}

static void __saveds _exec_UserState ( register struct ExecBase * __libBase __asm("a6"),
                                                        register APTR ___sysStack  __asm("d0"))
{
    lprintf ("_exec: UserState unimplemented STUB called.");
    assert(FALSE);
}

static struct Interrupt * __saveds _exec_SetIntVector ( register struct ExecBase * __libBase __asm("a6"),
                                                        register LONG ___intNumber  __asm("d0"),
                                                        register const struct Interrupt * ___interrupt  __asm("a1"))
{
    lprintf ("_exec: SetIntVector unimplemented STUB called.");
    assert(FALSE);
}

static void __saveds _exec_AddIntServer ( register struct ExecBase * __libBase __asm("a6"),
                                                        register LONG ___intNumber  __asm("d0"),
                                                        register struct Interrupt * ___interrupt  __asm("a1"))
{
    lprintf ("_exec: AddIntServer unimplemented STUB called.");
    assert(FALSE);
}

static void __saveds _exec_RemIntServer ( register struct ExecBase * __libBase __asm("a6"),
                                                        register LONG ___intNumber  __asm("d0"),
                                                        register struct Interrupt * ___interrupt  __asm("a1"))
{
    lprintf ("_exec: RemIntServer unimplemented STUB called.");
    assert(FALSE);
}

static void __saveds _exec_Cause ( register struct ExecBase * __libBase __asm("a6"),
                                                        register struct Interrupt * ___interrupt  __asm("a1"))
{
    lprintf ("_exec: Cause unimplemented STUB called.");
    assert(FALSE);
}

static APTR __saveds _exec_Allocate ( register struct ExecBase * __libBase __asm("a6"),
                                                        register struct MemHeader * ___freeList  __asm("a0"),
                                                        register ULONG ___byteSize  __asm("d0"))
{
    lprintf ("_exec: Allocate unimplemented STUB called.");
    assert(FALSE);
}

static void __saveds _exec_Deallocate ( register struct ExecBase * __libBase __asm("a6"),
                                                        register struct MemHeader * ___freeList  __asm("a0"),
                                                        register APTR ___memoryBlock  __asm("a1"),
                                                        register ULONG ___byteSize  __asm("d0"))
{
    lprintf ("_exec: Deallocate unimplemented STUB called.");
    assert(FALSE);
}

static APTR __saveds _exec_AllocMem ( register struct ExecBase * __libBase __asm("a6"),
                                      register ULONG ___byteSize  __asm("d0"),
                                      register ULONG ___requirements  __asm("d1"))
{
    lprintf ("_exec: AllocMem called, byteSize=%ld, requirements=0x%08lx\n",
             ___byteSize, ___requirements);

	if (!___byteSize)
		return NULL;

//OldAllocMem:
//	MOVEM.L D2/D3,-(SP)
//	MOVE.L  D0,D3		;D3=Raw number of bytes to allocate
//	BEQ.S	AllocMem_End	;Bozo case...
//	MOVE.L  D1,D2		;D2=Flags
//
//	;------ find a free list that matches the requirements
//	LEA	MemList(A6),A0
//
//	;---!!! protect whole func for now
//	FORBID
	Forbid();	

	struct MemHeader *mhCur = (struct MemHeader *)SysBase->MemList.lh_Head;

	while (mhCur->mh_Node.ln_Succ)
	{
		lprintf ("   MemHeader mh_Free=%d, mh_Attributes=0x%08lx\n",
                 mhCur->mh_Free, mhCur->mh_Attributes);

		UWORD req = (UWORD) ___requirements;
		if ((mhCur->mh_Attributes & req) == req)
		{
			if (mhCur->mh_Free >= ___byteSize)
			{
				// FIXME: Allocate
				assert (FALSE);
			}
			else
			{
				lprintf ("      *** too small ***");
			}
		}
		else
		{
			lprintf ("      *** does not meet requirements ***");
		}

		mhCur = (struct MemHeader *)mhCur->mh_Node.ln_Succ;
	}

	Permit();

	// FIXME: clear memory
    assert(FALSE);
}

static APTR __saveds _exec_AllocAbs ( register struct ExecBase * __libBase __asm("a6"),
                                                        register ULONG ___byteSize  __asm("d0"),
                                                        register APTR ___location  __asm("a1"))
{
    lprintf ("_exec: AllocAbs unimplemented STUB called.");
    assert(FALSE);
}

static void __saveds _exec_FreeMem ( register struct ExecBase * __libBase __asm("a6"),
                                                        register APTR ___memoryBlock  __asm("a1"),
                                                        register ULONG ___byteSize  __asm("d0"))
{
    lprintf ("_exec: FreeMem unimplemented STUB called.");
    assert(FALSE);
}

static ULONG __saveds _exec_AvailMem ( register struct ExecBase * __libBase __asm("a6"),
                                                        register ULONG ___requirements  __asm("d1"))
{
    lprintf ("_exec: AvailMem unimplemented STUB called.");
    assert(FALSE);
}

static struct MemList * __saveds _exec_AllocEntry ( register struct ExecBase * __libBase __asm("a6"),
                                                        register struct MemList * ___entry  __asm("a0"))
{
    lprintf ("_exec: AllocEntry unimplemented STUB called.");
    assert(FALSE);
}

static void __saveds _exec_FreeEntry ( register struct ExecBase * __libBase __asm("a6"),
                                                        register struct MemList * ___entry  __asm("a0"))
{
    lprintf ("_exec: FreeEntry unimplemented STUB called.");
    assert(FALSE);
}

static void __saveds _exec_Insert ( register struct ExecBase * __libBase __asm("a6"),
                                                        register struct List * ___list  __asm("a0"),
                                                        register struct Node * ___node  __asm("a1"),
                                                        register struct Node * ___pred  __asm("a2"))
{
    lprintf ("_exec: Insert unimplemented STUB called.");
    assert(FALSE);
}

static void __saveds _exec_AddHead ( register struct ExecBase * __libBase __asm("a6"),
                                                        register struct List * ___list  __asm("a0"),
                                                        register struct Node * ___node  __asm("a1"))
{
    lprintf ("_exec: AddHead unimplemented STUB called.");
    assert(FALSE);
}

static void __saveds _exec_AddTail ( register struct ExecBase * __libBase __asm("a6"),
                                     register struct List * ___list  __asm("a0"),
                                     register struct Node * ___node  __asm("a1"))
{
    lprintf ("_exec: AddTail called.");

    ___node->ln_Succ              = (struct Node *)&___list->lh_Tail;
    ___node->ln_Pred              = ___list->lh_TailPred;

    ___list->lh_TailPred->ln_Succ = ___node;
    ___list->lh_TailPred          = ___node;
}

static void __saveds _exec_Remove ( register struct ExecBase * __libBase __asm("a6"),
                                                        register struct Node * ___node  __asm("a1"))
{
    lprintf ("_exec: Remove unimplemented STUB called.");
    assert(FALSE);
}

static struct Node * __saveds _exec_RemHead ( register struct ExecBase * __libBase __asm("a6"),
                                                        register struct List * ___list  __asm("a0"))
{
    lprintf ("_exec: RemHead unimplemented STUB called.");
    assert(FALSE);
}

static struct Node * __saveds _exec_RemTail ( register struct ExecBase * __libBase __asm("a6"),
                                                        register struct List * ___list  __asm("a0"))
{
    lprintf ("_exec: RemTail unimplemented STUB called.");
    assert(FALSE);
}

static void __saveds _exec_Enqueue ( register struct ExecBase * __libBase __asm("a6"),
                                                        register struct List * ___list  __asm("a0"),
                                                        register struct Node * ___node  __asm("a1"))
{
    lprintf ("_exec: Enqueue unimplemented STUB called.");
    assert(FALSE);
}

static struct Node * __saveds _exec_FindName ( register struct ExecBase * __libBase __asm("a6"),
                                                        register struct List * ___list  __asm("a0"),
                                                        register CONST_STRPTR ___name  __asm("a1"))
{
    lprintf ("_exec: FindName unimplemented STUB called.");
    assert(FALSE);
}

static APTR __saveds _exec_AddTask ( register struct ExecBase * __libBase __asm("a6"),
                                                        register struct Task * ___task  __asm("a1"),
                                                        register const APTR ___initPC  __asm("a2"),
                                                        register const APTR ___finalPC  __asm("a3"))
{
    lprintf ("_exec: AddTask unimplemented STUB called.");
    assert(FALSE);
}

static void __saveds _exec_RemTask ( register struct ExecBase * __libBase __asm("a6"),
                                                        register struct Task * ___task  __asm("a1"))
{
    lprintf ("_exec: RemTask unimplemented STUB called.");
    assert(FALSE);
}

static struct Task * __saveds _exec_FindTask ( register struct ExecBase * __libBase __asm("a6"),
                                                        register CONST_STRPTR ___name  __asm("a1"))
{
    lprintf ("_exec: FindTask unimplemented STUB called.");
    assert(FALSE);
}

static BYTE __saveds _exec_SetTaskPri ( register struct ExecBase * __libBase __asm("a6"),
                                                        register struct Task * ___task  __asm("a1"),
                                                        register LONG ___priority  __asm("d0"))
{
    lprintf ("_exec: SetTaskPri unimplemented STUB called.");
    assert(FALSE);
}

static ULONG __saveds _exec_SetSignal ( register struct ExecBase * __libBase __asm("a6"),
                                                        register ULONG ___newSignals  __asm("d0"),
                                                        register ULONG ___signalSet  __asm("d1"))
{
    lprintf ("_exec: SetSignal unimplemented STUB called.");
    assert(FALSE);
}

static ULONG __saveds _exec_SetExcept ( register struct ExecBase * __libBase __asm("a6"),
                                                        register ULONG ___newSignals  __asm("d0"),
                                                        register ULONG ___signalSet  __asm("d1"))
{
    lprintf ("_exec: SetExcept unimplemented STUB called.");
    assert(FALSE);
}

static ULONG __saveds _exec_Wait ( register struct ExecBase * __libBase __asm("a6"),
                                                        register ULONG ___signalSet  __asm("d0"))
{
    lprintf ("_exec: Wait unimplemented STUB called.");
    assert(FALSE);
}

static void __saveds _exec_Signal ( register struct ExecBase * __libBase __asm("a6"),
                                                        register struct Task * ___task  __asm("a1"),
                                                        register ULONG ___signalSet  __asm("d0"))
{
    lprintf ("_exec: Signal unimplemented STUB called.");
    assert(FALSE);
}

static BYTE __saveds _exec_AllocSignal ( register struct ExecBase * __libBase __asm("a6"),
                                                        register BYTE ___signalNum  __asm("d0"))
{
    lprintf ("_exec: AllocSignal unimplemented STUB called.");
    assert(FALSE);
}

static void __saveds _exec_FreeSignal ( register struct ExecBase * __libBase __asm("a6"),
                                                        register BYTE ___signalNum  __asm("d0"))
{
    lprintf ("_exec: FreeSignal unimplemented STUB called.");
    assert(FALSE);
}

static LONG __saveds _exec_AllocTrap ( register struct ExecBase * __libBase __asm("a6"),
                                                        register LONG ___trapNum  __asm("d0"))
{
    lprintf ("_exec: AllocTrap unimplemented STUB called.");
    assert(FALSE);
}

static void __saveds _exec_FreeTrap ( register struct ExecBase * __libBase __asm("a6"),
                                                        register LONG ___trapNum  __asm("d0"))
{
    lprintf ("_exec: FreeTrap unimplemented STUB called.");
    assert(FALSE);
}

static void __saveds _exec_AddPort ( register struct ExecBase * __libBase __asm("a6"),
                                                        register struct MsgPort * ___port  __asm("a1"))
{
    lprintf ("_exec: AddPort unimplemented STUB called.");
    assert(FALSE);
}

static void __saveds _exec_RemPort ( register struct ExecBase * __libBase __asm("a6"),
                                                        register struct MsgPort * ___port  __asm("a1"))
{
    lprintf ("_exec: RemPort unimplemented STUB called.");
    assert(FALSE);
}

static void __saveds _exec_PutMsg ( register struct ExecBase * __libBase __asm("a6"),
                                                        register struct MsgPort * ___port  __asm("a0"),
                                                        register struct Message * ___message  __asm("a1"))
{
    lprintf ("_exec: PutMsg unimplemented STUB called.");
    assert(FALSE);
}

static struct Message * __saveds _exec_GetMsg ( register struct ExecBase * __libBase __asm("a6"),
                                                        register struct MsgPort * ___port  __asm("a0"))
{
    lprintf ("_exec: GetMsg unimplemented STUB called.");
    assert(FALSE);
}

static void __saveds _exec_ReplyMsg ( register struct ExecBase * __libBase __asm("a6"),
                                                        register struct Message * ___message  __asm("a1"))
{
    lprintf ("_exec: ReplyMsg unimplemented STUB called.");
    assert(FALSE);
}

static struct Message * __saveds _exec_WaitPort ( register struct ExecBase * __libBase __asm("a6"),
                                                        register struct MsgPort * ___port  __asm("a0"))
{
    lprintf ("_exec: WaitPort unimplemented STUB called.");
    assert(FALSE);
}

static struct MsgPort * __saveds _exec_FindPort ( register struct ExecBase * __libBase __asm("a6"),
                                                        register CONST_STRPTR ___name  __asm("a1"))
{
    lprintf ("_exec: FindPort unimplemented STUB called.");
    assert(FALSE);
}

static void __saveds _exec_AddLibrary ( register struct ExecBase * __libBase __asm("a6"),
                                                        register struct Library * ___library  __asm("a1"))
{
    lprintf ("_exec: AddLibrary unimplemented STUB called.");
    assert(FALSE);
}

static void __saveds _exec_RemLibrary ( register struct ExecBase * __libBase __asm("a6"),
                                                        register struct Library * ___library  __asm("a1"))
{
    lprintf ("_exec: RemLibrary unimplemented STUB called.");
    assert(FALSE);
}

static struct Library * __saveds _exec_OldOpenLibrary ( register struct ExecBase * __libBase __asm("a6"),
                                                        register CONST_STRPTR ___libName  __asm("a1"))
{
    lprintf ("_exec: OldOpenLibrary unimplemented STUB called.");
    assert(FALSE);
}

static void __saveds _exec_CloseLibrary ( register struct ExecBase * __libBase __asm("a6"),
                                                        register struct Library * ___library  __asm("a1"))
{
    lprintf ("_exec: CloseLibrary unimplemented STUB called.");
    assert(FALSE);
}

static APTR __saveds _exec_SetFunction ( register struct ExecBase * __libBase __asm("a6"),
                                                        register struct Library * ___library  __asm("a1"),
                                                        register LONG ___funcOffset  __asm("a0"),
                                                        register VOID (*___newFunction)()  __asm("d0"))
{
    lprintf ("_exec: SetFunction unimplemented STUB called.");
    assert(FALSE);
}

static void __saveds _exec_SumLibrary ( register struct ExecBase * __libBase __asm("a6"),
                                                        register struct Library * ___library  __asm("a1"))
{
    lprintf ("_exec: SumLibrary unimplemented STUB called.");
    assert(FALSE);
}

static void __saveds _exec_AddDevice ( register struct ExecBase * __libBase __asm("a6"),
                                                        register struct Device * ___device  __asm("a1"))
{
    lprintf ("_exec: AddDevice unimplemented STUB called.");
    assert(FALSE);
}

static void __saveds _exec_RemDevice ( register struct ExecBase * __libBase __asm("a6"),
                                                        register struct Device * ___device  __asm("a1"))
{
    lprintf ("_exec: RemDevice unimplemented STUB called.");
    assert(FALSE);
}

static BYTE __saveds _exec_OpenDevice ( register struct ExecBase * __libBase __asm("a6"),
                                                        register CONST_STRPTR ___devName  __asm("a0"),
                                                        register ULONG ___unit  __asm("d0"),
                                                        register struct IORequest * ___ioRequest  __asm("a1"),
                                                        register ULONG ___flags  __asm("d1"))
{
    lprintf ("_exec: OpenDevice unimplemented STUB called.");
    assert(FALSE);
}

static void __saveds _exec_CloseDevice ( register struct ExecBase * __libBase __asm("a6"),
                                                        register struct IORequest * ___ioRequest  __asm("a1"))
{
    lprintf ("_exec: CloseDevice unimplemented STUB called.");
    assert(FALSE);
}

static BYTE __saveds _exec_DoIO ( register struct ExecBase * __libBase __asm("a6"),
                                                        register struct IORequest * ___ioRequest  __asm("a1"))
{
    lprintf ("_exec: DoIO unimplemented STUB called.");
    assert(FALSE);
}

static void __saveds _exec_SendIO ( register struct ExecBase * __libBase __asm("a6"),
                                                        register struct IORequest * ___ioRequest  __asm("a1"))
{
    lprintf ("_exec: SendIO unimplemented STUB called.");
    assert(FALSE);
}

static struct IORequest * __saveds _exec_CheckIO ( register struct ExecBase * __libBase __asm("a6"),
                                                        register struct IORequest * ___ioRequest  __asm("a1"))
{
    lprintf ("_exec: CheckIO unimplemented STUB called.");
    assert(FALSE);
}

static BYTE __saveds _exec_WaitIO ( register struct ExecBase * __libBase __asm("a6"),
                                                        register struct IORequest * ___ioRequest  __asm("a1"))
{
    lprintf ("_exec: WaitIO unimplemented STUB called.");
    assert(FALSE);
}

static void __saveds _exec_AbortIO ( register struct ExecBase * __libBase __asm("a6"),
                                                        register struct IORequest * ___ioRequest  __asm("a1"))
{
    lprintf ("_exec: AbortIO unimplemented STUB called.");
    assert(FALSE);
}

static void __saveds _exec_AddResource ( register struct ExecBase * __libBase __asm("a6"),
                                                        register APTR ___resource  __asm("a1"))
{
    lprintf ("_exec: AddResource unimplemented STUB called.");
    assert(FALSE);
}

static void __saveds _exec_RemResource ( register struct ExecBase * __libBase __asm("a6"),
                                                        register APTR ___resource  __asm("a1"))
{
    lprintf ("_exec: RemResource unimplemented STUB called.");
    assert(FALSE);
}

static APTR __saveds _exec_OpenResource ( register struct ExecBase * __libBase __asm("a6"),
                                                        register CONST_STRPTR ___resName  __asm("a1"))
{
    lprintf ("_exec: OpenResource unimplemented STUB called.");
    assert(FALSE);
}

static APTR __saveds _exec_RawDoFmt ( register struct ExecBase * __libBase __asm("a6"),
                                                        register CONST_STRPTR ___formatString  __asm("a0"),
                                                        register const APTR ___dataStream  __asm("a1"),
                                                        register VOID (*___putChProc)()  __asm("a2"),
                                                        register APTR ___putChData  __asm("a3"))
{
    lprintf ("_exec: RawDoFmt unimplemented STUB called.");
    assert(FALSE);
}

static ULONG __saveds _exec_GetCC ( register struct ExecBase * __libBase __asm("a6"))
{
    lprintf ("_exec: GetCC unimplemented STUB called.");
    assert(FALSE);
}

static ULONG __saveds _exec_TypeOfMem ( register struct ExecBase * __libBase __asm("a6"),
                                                        register const APTR ___address  __asm("a1"))
{
    lprintf ("_exec: TypeOfMem unimplemented STUB called.");
    assert(FALSE);
}

static ULONG __saveds _exec_Procure ( register struct ExecBase * __libBase __asm("a6"),
                                                        register struct SignalSemaphore * ___sigSem  __asm("a0"),
                                                        register struct SemaphoreMessage * ___bidMsg  __asm("a1"))
{
    lprintf ("_exec: Procure unimplemented STUB called.");
    assert(FALSE);
}

static void __saveds _exec_Vacate ( register struct ExecBase * __libBase __asm("a6"),
                                                        register struct SignalSemaphore * ___sigSem  __asm("a0"),
                                                        register struct SemaphoreMessage * ___bidMsg  __asm("a1"))
{
    lprintf ("_exec: Vacate unimplemented STUB called.");
    assert(FALSE);
}

static struct Library * __saveds _exec_OpenLibrary ( register struct ExecBase * __libBase __asm("a6"),
                                                        register CONST_STRPTR ___libName  __asm("a1"),
                                                        register ULONG ___version  __asm("d0"))
{
    lprintf ("_exec: OpenLibrary unimplemented STUB called.");
    assert(FALSE);
}

static void __saveds _exec_InitSemaphore ( register struct ExecBase * __libBase __asm("a6"),
                                                        register struct SignalSemaphore * ___sigSem  __asm("a0"))
{
    lprintf ("_exec: InitSemaphore unimplemented STUB called.");
    assert(FALSE);
}

static void __saveds _exec_ObtainSemaphore ( register struct ExecBase * __libBase __asm("a6"),
                                                        register struct SignalSemaphore * ___sigSem  __asm("a0"))
{
    lprintf ("_exec: ObtainSemaphore unimplemented STUB called.");
    assert(FALSE);
}

static void __saveds _exec_ReleaseSemaphore ( register struct ExecBase * __libBase __asm("a6"),
                                                        register struct SignalSemaphore * ___sigSem  __asm("a0"))
{
    lprintf ("_exec: ReleaseSemaphore unimplemented STUB called.");
    assert(FALSE);
}

static ULONG __saveds _exec_AttemptSemaphore ( register struct ExecBase * __libBase __asm("a6"),
                                                        register struct SignalSemaphore * ___sigSem  __asm("a0"))
{
    lprintf ("_exec: AttemptSemaphore unimplemented STUB called.");
    assert(FALSE);
}

static void __saveds _exec_ObtainSemaphoreList ( register struct ExecBase * __libBase __asm("a6"),
                                                        register struct List * ___sigSem  __asm("a0"))
{
    lprintf ("_exec: ObtainSemaphoreList unimplemented STUB called.");
    assert(FALSE);
}

static void __saveds _exec_ReleaseSemaphoreList ( register struct ExecBase * __libBase __asm("a6"),
                                                        register struct List * ___sigSem  __asm("a0"))
{
    lprintf ("_exec: ReleaseSemaphoreList unimplemented STUB called.");
    assert(FALSE);
}

static struct SignalSemaphore * __saveds _exec_FindSemaphore ( register struct ExecBase * __libBase __asm("a6"),
                                                        register STRPTR ___name  __asm("a1"))
{
    lprintf ("_exec: FindSemaphore unimplemented STUB called.");
    assert(FALSE);
}

static void __saveds _exec_AddSemaphore ( register struct ExecBase * __libBase __asm("a6"),
                                                        register struct SignalSemaphore * ___sigSem  __asm("a1"))
{
    lprintf ("_exec: AddSemaphore unimplemented STUB called.");
    assert(FALSE);
}

static void __saveds _exec_RemSemaphore ( register struct ExecBase * __libBase __asm("a6"),
                                                        register struct SignalSemaphore * ___sigSem  __asm("a1"))
{
    lprintf ("_exec: RemSemaphore unimplemented STUB called.");
    assert(FALSE);
}

static ULONG __saveds _exec_SumKickData ( register struct ExecBase * __libBase __asm("a6"))
{
    lprintf ("_exec: SumKickData unimplemented STUB called.");
    assert(FALSE);
}

static void __saveds _exec_AddMemList ( register struct ExecBase * __libBase __asm("a6"),
                                                        register ULONG ___size  __asm("d0"),
                                                        register ULONG ___attributes  __asm("d1"),
                                                        register LONG ___pri  __asm("d2"),
                                                        register APTR ___base  __asm("a0"),
                                                        register CONST_STRPTR ___name  __asm("a1"))
{
    lprintf ("_exec: AddMemList unimplemented STUB called.");
    assert(FALSE);
}

static void __saveds _exec_CopyMem ( register struct ExecBase * __libBase __asm("a6"),
                                                        register const APTR ___source  __asm("a0"),
                                                        register APTR ___dest  __asm("a1"),
                                                        register ULONG ___size  __asm("d0"))
{
    lprintf ("_exec: CopyMem unimplemented STUB called.");
    assert(FALSE);
}

static void __saveds _exec_CopyMemQuick ( register struct ExecBase * __libBase __asm("a6"),
                                                        register const APTR ___source  __asm("a0"),
                                                        register APTR ___dest  __asm("a1"),
                                                        register ULONG ___size  __asm("d0"))
{
    lprintf ("_exec: CopyMemQuick unimplemented STUB called.");
    assert(FALSE);
}

static void __saveds _exec_CacheClearU ( register struct ExecBase * __libBase __asm("a6"))
{
    lprintf ("_exec: CacheClearU unimplemented STUB called.");
    assert(FALSE);
}

static void __saveds _exec_CacheClearE ( register struct ExecBase * __libBase __asm("a6"),
                                                        register APTR ___address  __asm("a0"),
                                                        register ULONG ___length  __asm("d0"),
                                                        register ULONG ___caches  __asm("d1"))
{
    lprintf ("_exec: CacheClearE unimplemented STUB called.");
    assert(FALSE);
}

static ULONG __saveds _exec_CacheControl ( register struct ExecBase * __libBase __asm("a6"),
                                                        register ULONG ___cacheBits  __asm("d0"),
                                                        register ULONG ___cacheMask  __asm("d1"))
{
    lprintf ("_exec: CacheControl unimplemented STUB called.");
    assert(FALSE);
}

static APTR __saveds _exec_CreateIORequest ( register struct ExecBase * __libBase __asm("a6"),
                                                        register const struct MsgPort * ___port  __asm("a0"),
                                                        register ULONG ___size  __asm("d0"))
{
    lprintf ("_exec: CreateIORequest unimplemented STUB called.");
    assert(FALSE);
}

static void __saveds _exec_DeleteIORequest ( register struct ExecBase * __libBase __asm("a6"),
                                                        register APTR ___iorequest  __asm("a0"))
{
    lprintf ("_exec: DeleteIORequest unimplemented STUB called.");
    assert(FALSE);
}

static struct MsgPort * __saveds _exec_CreateMsgPort ( register struct ExecBase * __libBase __asm("a6"))
{
    lprintf ("_exec: CreateMsgPort unimplemented STUB called.");
    assert(FALSE);
}

static void __saveds _exec_DeleteMsgPort ( register struct ExecBase * __libBase __asm("a6"),
                                                        register struct MsgPort * ___port  __asm("a0"))
{
    lprintf ("_exec: DeleteMsgPort unimplemented STUB called.");
    assert(FALSE);
}

static void __saveds _exec_ObtainSemaphoreShared ( register struct ExecBase * __libBase __asm("a6"),
                                                        register struct SignalSemaphore * ___sigSem  __asm("a0"))
{
    lprintf ("_exec: ObtainSemaphoreShared unimplemented STUB called.");
    assert(FALSE);
}

static APTR __saveds _exec_AllocVec ( register struct ExecBase * __libBase __asm("a6"),
                                                        register ULONG ___byteSize  __asm("d0"),
                                                        register ULONG ___requirements  __asm("d1"))
{
    lprintf ("_exec: AllocVec unimplemented STUB called.");
    assert(FALSE);
}

static void __saveds _exec_FreeVec ( register struct ExecBase * __libBase __asm("a6"),
                                                        register APTR ___memoryBlock  __asm("a1"))
{
    lprintf ("_exec: FreeVec unimplemented STUB called.");
    assert(FALSE);
}

static APTR __saveds _exec_CreatePool ( register struct ExecBase * __libBase __asm("a6"),
                                                        register ULONG ___requirements  __asm("d0"),
                                                        register ULONG ___puddleSize  __asm("d1"),
                                                        register ULONG ___threshSize  __asm("d2"))
{
    lprintf ("_exec: CreatePool unimplemented STUB called.");
    assert(FALSE);
}

static void __saveds _exec_DeletePool ( register struct ExecBase * __libBase __asm("a6"),
                                                        register APTR ___poolHeader  __asm("a0"))
{
    lprintf ("_exec: DeletePool unimplemented STUB called.");
    assert(FALSE);
}

static APTR __saveds _exec_AllocPooled ( register struct ExecBase * __libBase __asm("a6"),
                                                        register APTR ___poolHeader  __asm("a0"),
                                                        register ULONG ___memSize  __asm("d0"))
{
    lprintf ("_exec: AllocPooled unimplemented STUB called.");
    assert(FALSE);
}

static void __saveds _exec_FreePooled ( register struct ExecBase * __libBase __asm("a6"),
                                                        register APTR ___poolHeader  __asm("a0"),
                                                        register APTR ___memory  __asm("a1"),
                                                        register ULONG ___memSize  __asm("d0"))
{
    lprintf ("_exec: FreePooled unimplemented STUB called.");
    assert(FALSE);
}

static ULONG __saveds _exec_AttemptSemaphoreShared ( register struct ExecBase * __libBase __asm("a6"),
                                                        register struct SignalSemaphore * ___sigSem  __asm("a0"))
{
    lprintf ("_exec: AttemptSemaphoreShared unimplemented STUB called.");
    assert(FALSE);
}

static void __saveds _exec_ColdReboot ( register struct ExecBase * __libBase __asm("a6"))
{
    lprintf ("_exec: ColdReboot unimplemented STUB called.");
    assert(FALSE);
}

static void __saveds _exec_StackSwap ( register struct ExecBase * __libBase __asm("a6"),
                                                        register struct StackSwapStruct * ___newStack  __asm("a0"))
{
    lprintf ("_exec: StackSwap unimplemented STUB called.");
    assert(FALSE);
}

static APTR __saveds _exec_CachePreDMA ( register struct ExecBase * __libBase __asm("a6"),
                                                        register const APTR ___address  __asm("a0"),
                                                        register ULONG * ___length  __asm("a1"),
                                                        register ULONG ___flags  __asm("d0"))
{
    lprintf ("_exec: CachePreDMA unimplemented STUB called.");
    assert(FALSE);
}

static void __saveds _exec_CachePostDMA ( register struct ExecBase * __libBase __asm("a6"),
                                                        register const APTR ___address  __asm("a0"),
                                                        register ULONG * ___length  __asm("a1"),
                                                        register ULONG ___flags  __asm("d0"))
{
    lprintf ("_exec: CachePostDMA unimplemented STUB called.");
    assert(FALSE);
}

static void __saveds _exec_AddMemHandler ( register struct ExecBase * __libBase __asm("a6"),
                                                        register struct Interrupt * ___memhand  __asm("a1"))
{
    lprintf ("_exec: AddMemHandler unimplemented STUB called.");
    assert(FALSE);
}

static void __saveds _exec_RemMemHandler ( register struct ExecBase * __libBase __asm("a6"),
                                                        register struct Interrupt * ___memhand  __asm("a1"))
{
    lprintf ("_exec: RemMemHandler unimplemented STUB called.");
    assert(FALSE);
}

static void registerBuiltInLib (struct Resident *romTAG)
{
    struct InitTable *initTab = romTAG->rt_Init;
    MakeLibrary(initTab->FunctionTable, initTab->DataTable, initTab->InitLibFn, initTab->LibBaseSize, /* segList=*/NULL);
}

void __saveds coldstart (void)
{
    //uint32_t *p = (void*) 0x00008c;
    //*p = (uint32_t) handleTRAP3;

    __asm("andi.w  #0xdfff, sr\n");
    __asm("move.l  #0x9fff99, a7\n");

    lprintf ("coldstart: EXEC_VECTORS_START=0x%08x, EXEC_BASE_START=0x%08x\n", EXEC_VECTORS_START, EXEC_BASE_START);

    // coldstart is at 0x00f801be, &SysBase at 0x00f80c90, SysBase at 0x0009eed0, f1 at 0x00f80028
    lprintf ("coldstart: locations in RAM: coldstart is at 0x%08x, &SysBase at 0x%08x, SysBase at 0x%08x\n",
             coldstart, &SysBase, SysBase);
    // g_ExecJumpTable at 0x00001800, &g_ExecJumpTable[0].vec at 0x00001802
    lprintf ("coldstart: g_ExecJumpTable at 0x%08x, &g_ExecJumpTable[0].vec at 0x%08x\n", g_ExecJumpTable, &g_ExecJumpTable[0].vec);

    /* set up execbase */

    *(APTR *)4L = SysBase;

	for (int i = 0; i<NUM_EXEC_FUNCS; i++)
	{
		g_ExecJumpTable[i].vec = _exec_unimplemented_call;
		g_ExecJumpTable[i].jmp = JMPINSTR;
	}

    g_ExecJumpTable[EXEC_FUNCTABLE_ENTRY(-30)].vec = _exec_Supervisor;
    g_ExecJumpTable[EXEC_FUNCTABLE_ENTRY(-72)].vec = _exec_InitCode;
    g_ExecJumpTable[EXEC_FUNCTABLE_ENTRY(-78)].vec = _exec_InitStruct;
    g_ExecJumpTable[EXEC_FUNCTABLE_ENTRY(-84)].vec = _exec_MakeLibrary;
    g_ExecJumpTable[EXEC_FUNCTABLE_ENTRY(-90)].vec = _exec_MakeFunctions;
    g_ExecJumpTable[EXEC_FUNCTABLE_ENTRY(-96)].vec = _exec_FindResident;
    g_ExecJumpTable[EXEC_FUNCTABLE_ENTRY(-102)].vec = _exec_InitResident;
    g_ExecJumpTable[EXEC_FUNCTABLE_ENTRY(-108)].vec = _exec_Alert;
    g_ExecJumpTable[EXEC_FUNCTABLE_ENTRY(-114)].vec = _exec_Debug;
    g_ExecJumpTable[EXEC_FUNCTABLE_ENTRY(-120)].vec = _exec_Disable;
    g_ExecJumpTable[EXEC_FUNCTABLE_ENTRY(-126)].vec = _exec_Enable;
    g_ExecJumpTable[EXEC_FUNCTABLE_ENTRY(-132)].vec = _exec_Forbid;
    g_ExecJumpTable[EXEC_FUNCTABLE_ENTRY(-138)].vec = _exec_Permit;
    g_ExecJumpTable[EXEC_FUNCTABLE_ENTRY(-144)].vec = _exec_SetSR;
    g_ExecJumpTable[EXEC_FUNCTABLE_ENTRY(-150)].vec = _exec_SuperState;
    g_ExecJumpTable[EXEC_FUNCTABLE_ENTRY(-156)].vec = _exec_UserState;
    g_ExecJumpTable[EXEC_FUNCTABLE_ENTRY(-162)].vec = _exec_SetIntVector;
    g_ExecJumpTable[EXEC_FUNCTABLE_ENTRY(-168)].vec = _exec_AddIntServer;
    g_ExecJumpTable[EXEC_FUNCTABLE_ENTRY(-174)].vec = _exec_RemIntServer;
    g_ExecJumpTable[EXEC_FUNCTABLE_ENTRY(-180)].vec = _exec_Cause;
    g_ExecJumpTable[EXEC_FUNCTABLE_ENTRY(-186)].vec = _exec_Allocate;
    g_ExecJumpTable[EXEC_FUNCTABLE_ENTRY(-192)].vec = _exec_Deallocate;
    g_ExecJumpTable[EXEC_FUNCTABLE_ENTRY(-198)].vec = _exec_AllocMem;
    g_ExecJumpTable[EXEC_FUNCTABLE_ENTRY(-204)].vec = _exec_AllocAbs;
    g_ExecJumpTable[EXEC_FUNCTABLE_ENTRY(-210)].vec = _exec_FreeMem;
    g_ExecJumpTable[EXEC_FUNCTABLE_ENTRY(-216)].vec = _exec_AvailMem;
    g_ExecJumpTable[EXEC_FUNCTABLE_ENTRY(-222)].vec = _exec_AllocEntry;
    g_ExecJumpTable[EXEC_FUNCTABLE_ENTRY(-228)].vec = _exec_FreeEntry;
    g_ExecJumpTable[EXEC_FUNCTABLE_ENTRY(-234)].vec = _exec_Insert;
    g_ExecJumpTable[EXEC_FUNCTABLE_ENTRY(-240)].vec = _exec_AddHead;
    g_ExecJumpTable[EXEC_FUNCTABLE_ENTRY(-246)].vec = _exec_AddTail;
    g_ExecJumpTable[EXEC_FUNCTABLE_ENTRY(-252)].vec = _exec_Remove;
    g_ExecJumpTable[EXEC_FUNCTABLE_ENTRY(-258)].vec = _exec_RemHead;
    g_ExecJumpTable[EXEC_FUNCTABLE_ENTRY(-264)].vec = _exec_RemTail;
    g_ExecJumpTable[EXEC_FUNCTABLE_ENTRY(-270)].vec = _exec_Enqueue;
    g_ExecJumpTable[EXEC_FUNCTABLE_ENTRY(-276)].vec = _exec_FindName;
    g_ExecJumpTable[EXEC_FUNCTABLE_ENTRY(-282)].vec = _exec_AddTask;
    g_ExecJumpTable[EXEC_FUNCTABLE_ENTRY(-288)].vec = _exec_RemTask;
    g_ExecJumpTable[EXEC_FUNCTABLE_ENTRY(-294)].vec = _exec_FindTask;
    g_ExecJumpTable[EXEC_FUNCTABLE_ENTRY(-300)].vec = _exec_SetTaskPri;
    g_ExecJumpTable[EXEC_FUNCTABLE_ENTRY(-306)].vec = _exec_SetSignal;
    g_ExecJumpTable[EXEC_FUNCTABLE_ENTRY(-312)].vec = _exec_SetExcept;
    g_ExecJumpTable[EXEC_FUNCTABLE_ENTRY(-318)].vec = _exec_Wait;
    g_ExecJumpTable[EXEC_FUNCTABLE_ENTRY(-324)].vec = _exec_Signal;
    g_ExecJumpTable[EXEC_FUNCTABLE_ENTRY(-330)].vec = _exec_AllocSignal;
    g_ExecJumpTable[EXEC_FUNCTABLE_ENTRY(-336)].vec = _exec_FreeSignal;
    g_ExecJumpTable[EXEC_FUNCTABLE_ENTRY(-342)].vec = _exec_AllocTrap;
    g_ExecJumpTable[EXEC_FUNCTABLE_ENTRY(-348)].vec = _exec_FreeTrap;
    g_ExecJumpTable[EXEC_FUNCTABLE_ENTRY(-354)].vec = _exec_AddPort;
    g_ExecJumpTable[EXEC_FUNCTABLE_ENTRY(-360)].vec = _exec_RemPort;
    g_ExecJumpTable[EXEC_FUNCTABLE_ENTRY(-366)].vec = _exec_PutMsg;
    g_ExecJumpTable[EXEC_FUNCTABLE_ENTRY(-372)].vec = _exec_GetMsg;
    g_ExecJumpTable[EXEC_FUNCTABLE_ENTRY(-378)].vec = _exec_ReplyMsg;
    g_ExecJumpTable[EXEC_FUNCTABLE_ENTRY(-384)].vec = _exec_WaitPort;
    g_ExecJumpTable[EXEC_FUNCTABLE_ENTRY(-390)].vec = _exec_FindPort;
    g_ExecJumpTable[EXEC_FUNCTABLE_ENTRY(-396)].vec = _exec_AddLibrary;
    g_ExecJumpTable[EXEC_FUNCTABLE_ENTRY(-402)].vec = _exec_RemLibrary;
    g_ExecJumpTable[EXEC_FUNCTABLE_ENTRY(-408)].vec = _exec_OldOpenLibrary;
    g_ExecJumpTable[EXEC_FUNCTABLE_ENTRY(-414)].vec = _exec_CloseLibrary;
    g_ExecJumpTable[EXEC_FUNCTABLE_ENTRY(-420)].vec = _exec_SetFunction;
    g_ExecJumpTable[EXEC_FUNCTABLE_ENTRY(-426)].vec = _exec_SumLibrary;
    g_ExecJumpTable[EXEC_FUNCTABLE_ENTRY(-432)].vec = _exec_AddDevice;
    g_ExecJumpTable[EXEC_FUNCTABLE_ENTRY(-438)].vec = _exec_RemDevice;
    g_ExecJumpTable[EXEC_FUNCTABLE_ENTRY(-444)].vec = _exec_OpenDevice;
    g_ExecJumpTable[EXEC_FUNCTABLE_ENTRY(-450)].vec = _exec_CloseDevice;
    g_ExecJumpTable[EXEC_FUNCTABLE_ENTRY(-456)].vec = _exec_DoIO;
    g_ExecJumpTable[EXEC_FUNCTABLE_ENTRY(-462)].vec = _exec_SendIO;
    g_ExecJumpTable[EXEC_FUNCTABLE_ENTRY(-468)].vec = _exec_CheckIO;
    g_ExecJumpTable[EXEC_FUNCTABLE_ENTRY(-474)].vec = _exec_WaitIO;
    g_ExecJumpTable[EXEC_FUNCTABLE_ENTRY(-480)].vec = _exec_AbortIO;
    g_ExecJumpTable[EXEC_FUNCTABLE_ENTRY(-486)].vec = _exec_AddResource;
    g_ExecJumpTable[EXEC_FUNCTABLE_ENTRY(-492)].vec = _exec_RemResource;
    g_ExecJumpTable[EXEC_FUNCTABLE_ENTRY(-498)].vec = _exec_OpenResource;
    g_ExecJumpTable[EXEC_FUNCTABLE_ENTRY(-522)].vec = _exec_RawDoFmt;
    g_ExecJumpTable[EXEC_FUNCTABLE_ENTRY(-528)].vec = _exec_GetCC;
    g_ExecJumpTable[EXEC_FUNCTABLE_ENTRY(-534)].vec = _exec_TypeOfMem;
    g_ExecJumpTable[EXEC_FUNCTABLE_ENTRY(-540)].vec = _exec_Procure;
    g_ExecJumpTable[EXEC_FUNCTABLE_ENTRY(-546)].vec = _exec_Vacate;
    g_ExecJumpTable[EXEC_FUNCTABLE_ENTRY(-552)].vec = _exec_OpenLibrary;
    g_ExecJumpTable[EXEC_FUNCTABLE_ENTRY(-558)].vec = _exec_InitSemaphore;
    g_ExecJumpTable[EXEC_FUNCTABLE_ENTRY(-564)].vec = _exec_ObtainSemaphore;
    g_ExecJumpTable[EXEC_FUNCTABLE_ENTRY(-570)].vec = _exec_ReleaseSemaphore;
    g_ExecJumpTable[EXEC_FUNCTABLE_ENTRY(-576)].vec = _exec_AttemptSemaphore;
    g_ExecJumpTable[EXEC_FUNCTABLE_ENTRY(-582)].vec = _exec_ObtainSemaphoreList;
    g_ExecJumpTable[EXEC_FUNCTABLE_ENTRY(-588)].vec = _exec_ReleaseSemaphoreList;
    g_ExecJumpTable[EXEC_FUNCTABLE_ENTRY(-594)].vec = _exec_FindSemaphore;
    g_ExecJumpTable[EXEC_FUNCTABLE_ENTRY(-600)].vec = _exec_AddSemaphore;
    g_ExecJumpTable[EXEC_FUNCTABLE_ENTRY(-606)].vec = _exec_RemSemaphore;
    g_ExecJumpTable[EXEC_FUNCTABLE_ENTRY(-612)].vec = _exec_SumKickData;
    g_ExecJumpTable[EXEC_FUNCTABLE_ENTRY(-618)].vec = _exec_AddMemList;
    g_ExecJumpTable[EXEC_FUNCTABLE_ENTRY(-624)].vec = _exec_CopyMem;
    g_ExecJumpTable[EXEC_FUNCTABLE_ENTRY(-630)].vec = _exec_CopyMemQuick;
    g_ExecJumpTable[EXEC_FUNCTABLE_ENTRY(-636)].vec = _exec_CacheClearU;
    g_ExecJumpTable[EXEC_FUNCTABLE_ENTRY(-642)].vec = _exec_CacheClearE;
    g_ExecJumpTable[EXEC_FUNCTABLE_ENTRY(-648)].vec = _exec_CacheControl;
    g_ExecJumpTable[EXEC_FUNCTABLE_ENTRY(-654)].vec = _exec_CreateIORequest;
    g_ExecJumpTable[EXEC_FUNCTABLE_ENTRY(-660)].vec = _exec_DeleteIORequest;
    g_ExecJumpTable[EXEC_FUNCTABLE_ENTRY(-666)].vec = _exec_CreateMsgPort;
    g_ExecJumpTable[EXEC_FUNCTABLE_ENTRY(-672)].vec = _exec_DeleteMsgPort;
    g_ExecJumpTable[EXEC_FUNCTABLE_ENTRY(-678)].vec = _exec_ObtainSemaphoreShared;
    g_ExecJumpTable[EXEC_FUNCTABLE_ENTRY(-684)].vec = _exec_AllocVec;
    g_ExecJumpTable[EXEC_FUNCTABLE_ENTRY(-690)].vec = _exec_FreeVec;
    g_ExecJumpTable[EXEC_FUNCTABLE_ENTRY(-696)].vec = _exec_CreatePool;
    g_ExecJumpTable[EXEC_FUNCTABLE_ENTRY(-702)].vec = _exec_DeletePool;
    g_ExecJumpTable[EXEC_FUNCTABLE_ENTRY(-708)].vec = _exec_AllocPooled;
    g_ExecJumpTable[EXEC_FUNCTABLE_ENTRY(-714)].vec = _exec_FreePooled;
    g_ExecJumpTable[EXEC_FUNCTABLE_ENTRY(-720)].vec = _exec_AttemptSemaphoreShared;
    g_ExecJumpTable[EXEC_FUNCTABLE_ENTRY(-726)].vec = _exec_ColdReboot;
    g_ExecJumpTable[EXEC_FUNCTABLE_ENTRY(-732)].vec = _exec_StackSwap;
    g_ExecJumpTable[EXEC_FUNCTABLE_ENTRY(-762)].vec = _exec_CachePreDMA;
    g_ExecJumpTable[EXEC_FUNCTABLE_ENTRY(-768)].vec = _exec_CachePostDMA;
    g_ExecJumpTable[EXEC_FUNCTABLE_ENTRY(-774)].vec = _exec_AddMemHandler;
    g_ExecJumpTable[EXEC_FUNCTABLE_ENTRY(-780)].vec = _exec_RemMemHandler;

    SysBase->SoftVer = VERSION;

	// set up memory management

	SysBase->MemList.lh_Head = NULL;
	SysBase->MemList.lh_Tail = NULL;
	SysBase->MemList.lh_TailPred = (struct Node *)&SysBase->MemList.lh_Head;
	SysBase->MemList.lh_Type = NT_MEMORY;

	struct MemHeader *myh = (struct MemHeader*) ((uint8_t *)EXEC_MH_START);
    myh->mh_Node.ln_Type = NT_MEMORY;
    myh->mh_Node.ln_Pri  = 0;
    myh->mh_Node.ln_Name = NULL;
	myh->mh_Attributes   = MEMF_CHIP | MEMF_PUBLIC;
    myh->mh_First        = NULL;      // FIXME struct  MemChunk *
    myh->mh_Lower        = (APTR) RAM_START;
    myh->mh_Upper        = (APTR) (RAM_END + 1);
    myh->mh_Free         = RAM_SIZE;

	AddTail (&SysBase->MemList, &myh->mh_Node);

    // init and register built-in libraries
    registerBuiltInLib (__lxa_dos_ROMTag);

	OpenLibrary ("dos.library", 0);

    assert(FALSE);

    emu_stop();
}

