
#include <inttypes.h>

#include <hardware/custom.h>

#include <exec/types.h>
#include <exec/memory.h>
#include <exec/libraries.h>
#include <exec/execbase.h>
#include <exec/resident.h>
#include <exec/initializers.h>
#include <exec/errors.h>

#include <clib/exec_protos.h>
#include <inline/exec.h>

#include <dos/dostags.h>
#include <clib/dos_protos.h>
#include <inline/dos.h>

#include <libraries/mathffp.h>

#include "util.h"
#include "mem_map.h"
#include "exceptions.h"

#define DEFAULT_SCHED_QUANTUM 4

#define DEFAULT_STACKSIZE 1<<16

#define EXEC_FUNCTABLE_ENTRY(___off) (NUM_EXEC_FUNCS+(___off/6))

#define VERSION  1
#define REVISION 1

typedef struct Library * __saveds (*libInitFn_t) ( register struct Library    *lib     __asm("a6"),
                                                   register BPTR               seglist __asm("a0"),
                                                   register struct ExecBase   *sysb    __asm("d0"));

typedef struct Library * __saveds (*libOpenFn_t) ( register struct Library    *lib     __asm("a6"));
typedef struct Library * __saveds (*libCloseFn_t)( register struct Library    *lib     __asm("a6"));

typedef void             __saveds (*devOpenFn_t) ( register struct Library    *dev     __asm("a6"),
                                                   register struct IORequest  *ioreq   __asm("a1"),
                                                   register ULONG              unitn   __asm("d0"),
                                                   register ULONG              flags   __asm("d1"));
typedef void             __saveds (*devCloseFn_t) ( register struct Library    *dev     __asm("a6"),
                                                    register struct IORequest  *ioreq   __asm("a1"));
typedef void             __saveds (*devBeginIOFn_t) ( register struct Library    *dev     __asm("a6"),
                                                      register struct IORequest  *ioreq   __asm("a1"));

struct JumpVec
{
    unsigned short jmp;
    void *vec;
};

static struct JumpVec  *g_ExecJumpTable   = (struct JumpVec *)      ((uint8_t *)EXEC_VECTORS_START);
struct ExecBase        *SysBase           = (struct ExecBase*)      ((uint8_t *)EXEC_BASE_START);
struct UtilityBase     *UtilityBase       = (struct UtilityBase*)   ((uint8_t *)UTILITY_BASE_START);
struct DosLibrary      *DOSBase           = (struct DosLibrary*)    ((uint8_t *)DOS_BASE_START);
struct Library         *MathBase          = (struct Library*)       ((uint8_t *)MATHFFP_BASE_START);
struct Library         *MathTransBase     = (struct Library*)       ((uint8_t *)MATHTRANS_BASE_START);
struct GfxBase         *GfxBase           = (struct GfxBase*)       ((uint8_t *)GRAPHICS_BASE_START);
struct IntuitionBase   *IntuitionBase     = (struct IntuitionBase*) ((uint8_t *)INTUITION_BASE_START);
struct ExpansionBase   *ExpansionBase     = (struct ExpansionBase*) ((uint8_t *)EXPANSION_BASE_START);
struct Library         *DeviceInputBase   = (struct Library*)       ((uint8_t *)DEVICE_INPUT_BASE_START);
struct Library         *DeviceConsoleBase = (struct Library*)       ((uint8_t *)DEVICE_CONSOLE_BASE_START);
static struct Custom   *custom            = (struct Custom*)        0xdff000;

#define JMPINSTR 0x4ef9

void __saveds _exec_unimplemented_call ( register struct ExecBase  *exb __asm("a6") )
{
    assert (FALSE);
}

void __saveds _exec_InitCode ( register struct ExecBase * __libBase __asm("a6"),
                                                        register ULONG ___startClass  __asm("d0"),
                                                        register ULONG ___version  __asm("d1"))
{
    DPRINTF (LOG_DEBUG, "_exec: InitCode unimplemented STUB called.\n");
    assert(FALSE);
}

void __saveds _exec_InitStruct ( register struct ExecBase * __libBase __asm("a6"),
                                        register const APTR ___initTable  __asm("a1"),
                                        register APTR ___memory  __asm("a2"),
                                        register ULONG ___size  __asm("d0"))
{
    DPRINTF (LOG_DEBUG, "_exec: InitStruct called initTable=0x%08lx, memory=0x%08lx, size=%ld\n", ___initTable, ___memory, ___size);

    U_hexdump (LOG_DEBUG, ___initTable, 32);

    ULONG size = ___size & 0xffff;

    if (size)
        memset (___memory, 0x00, size);

    UBYTE *it  = (UBYTE *) ___initTable;
    UBYTE *dst = (UBYTE *) ___memory;

    ULONG off=0;
    while (*it)
    {
        UBYTE action = *it>>6 & 3;

        UBYTE elementSize = *it>>4 & 3;

        WORD cnt = *it & 15; // BYTE is not enough here since it will run to -1 in the code below

        DPRINTF (LOG_DEBUG, "                  action=%d, elementSize=%d, cnt=%d\n", action, elementSize, cnt);

        switch (action)
        {
            case 0:
            case 1:
                it++;
                break;
            case 2:
                it++;
                off = *it++;
                break;
            case 3: // 24 bit off
                off = *(ULONG *)it & 0xffffff;
                it += 4;
                break;
        }

        // 68k alignment
        switch (elementSize)
        {
            case 0: // LONG
            case 1: // WORD
                it  = (UBYTE*) ALIGN ((ULONG)it , 2);
                dst = (UBYTE*) ALIGN ((ULONG)dst, 2);
                break;
            case 2: // BYTE
                break;
            case 3:
                // FIXME: alert
                assert(FALSE);
                break;
        }

        switch (action)
        {
            case 2:
            case 3:
                dst = (BYTE *)___memory + off;

                /* fall through */
            case 0:
                switch (elementSize)
                {
                    case 0: // LONG
                        do
                        {
                            *(LONG *)dst = *(LONG *)it;
                            dst += 4;
                            it  += 4;
                        } while (--cnt>=0);
                        break;

                    case 1: // WORD
                        do
                        {
                            *(WORD *)dst = *(WORD *)it;
                            dst += 2;
                            it  += 2;
                        } while (--cnt>=0);
                        break;

                    case 2: // BYTE
                        do
                        {
                            *dst++ = *it++;
                        }
                        while (--cnt>=0);
                        break;

                    case 3:
                        // FIXME: alert
                        assert(FALSE);
                        break;
                }
                break;

            case 1:
                switch (elementSize)
                {
                    case 0: // LONG
                    {
                        LONG src = *(LONG *)it;
                        it += 4;

                        do
                        {
                            *(LONG *)dst = src;
                            dst += 4;
                        } while (--cnt>=0);
                        break;
                    }

                    case 1: // WORD
                    {
                        WORD src = *(WORD *)it;
                        it += 2;
                        do
                        {
                            *(WORD *)dst = src;
                            dst += 2;
                        } while (--cnt>=0);
                        break;
                    }

                    case 2: // BYTE
                    {
                        UBYTE src = *it++;
                        do
                            *dst++ = src;
                        while (--cnt>=0);
                        break;
                    }

                    case 3:
                        // FIXME: alert
                        assert(FALSE);
                        break;
                }
                break;
        }

        it  = (UBYTE*) ALIGN ((ULONG)it, 2);
    }
    DPRINTF (LOG_DEBUG, "_exec: InitStruct done.\n");
}

void _makeLibrary ( struct Library *library,
                           const APTR ___funcInit,
                           const APTR ___structInit,
                           libInitFn_t ___libInitFn,
                           ULONG negsize,
                           ULONG ___dataSize,
                           BPTR ___segList)
{
    DPRINTF (LOG_DEBUG, "_exec: _makeLibrary library=0x%08lx, ___funcInit=0x%08lx, ___structInit=0x%08x, ___libInitFn=0x%08x, negsize=%ld, ___dataSize=%ld\n",
             library, ___funcInit, ___structInit, ___libInitFn, negsize, ___dataSize);

    if(*(WORD *)___funcInit==-1)
        MakeFunctions (library, (WORD *)___funcInit+1, (WORD *)___funcInit);
    else
        MakeFunctions (library, ___funcInit, NULL);

    library->lib_NegSize = negsize;
    library->lib_PosSize = ___dataSize;

    if (___structInit)
        InitStruct (___structInit, library, 0);
    if (___libInitFn)
    {
        DPRINTF (LOG_DEBUG, "_exec: calling ___libInitFn at 0x%08lx\n", ___libInitFn);

        library = ___libInitFn(library, ___segList, SysBase);
        DPRINTF (LOG_DEBUG, "_exec: ___libInitFn returned library=0x%08lx\n", library);
    }
}

struct Library * __saveds _exec_MakeLibrary ( register struct ExecBase * __libBase __asm("a6"),
                                                     register const APTR ___funcInit  __asm("a0"),
                                                     register const APTR ___structInit  __asm("a1"),
                                                     register libInitFn_t ___libInitFn  __asm("a2"),
                                                     register ULONG ___dataSize  __asm("d0"),
                                                     register BPTR ___segList  __asm("d1"))
{
    DPRINTF (LOG_DEBUG, "_exec: MakeLibrary called, ___funcInit=0x%08x, ___structInit=0x%08x, ___dataSize=%d\n", ___funcInit, ___structInit, ___dataSize);

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
    DPRINTF (LOG_DEBUG, "_exec: MakeLibrary count=%d\n", count);

    negsize = count * 6;

    char *mem = AllocMem (___dataSize+negsize, MEMF_PUBLIC|MEMF_CLEAR);

    if (mem)
    {
        library = (struct Library *)(mem+negsize);

        DPRINTF (LOG_DEBUG, "_exec: MakeLibrary mem=0x%08lx, library=0x%08lx\n", mem, library);

        _makeLibrary (library, ___funcInit, ___structInit, ___libInitFn, negsize, ___dataSize, ___segList);
    }
    else
    {
        DPRINTF (LOG_DEBUG, "exec: MakeLibrary out of memory!\n");
        assert(FALSE);
    }

    return library;
}

#define __AROS_GETJUMPVEC(lib,n)        (&(((struct JumpVec *)(lib))[-(n)]))

#define __AROS_SETVECADDR(lib,n,addr)   (__AROS_SET_VEC(__AROS_GETJUMPVEC(lib,n),(APTR)(addr)))
#define __AROS_INITVEC(lib,n)           __AROS_GETJUMPVEC(lib,n)->jmp = __AROS_ASMJMP, \
                                        __AROS_SETVECADDR(lib,n,_aros_empty_vector | (n << 8) | 1)



void __saveds _exec_MakeFunctions ( register struct ExecBase * __libBase __asm("a6"),
                                           register APTR ___target  __asm("a0"),
                                           register const APTR ___functionArray  __asm("a1"),
                                           register const APTR ___funcDispBase  __asm("a2"))
{
    DPRINTF (LOG_DEBUG, "_exec: MakeFunctions called target=0x%08lx, functionArray=0x%08x, funcDispBase=0x%08x\n", ___target, ___functionArray, ___funcDispBase);

    ULONG n = 1;
    APTR lastvec;

    if (___funcDispBase)
    {
        WORD *fp = (WORD *)___functionArray;

        while (*fp != -1)
        {
            struct JumpVec  *jv = &(((struct JumpVec *)(___target))[-n]);
            jv->jmp = JMPINSTR;
            jv->vec = (APTR)((ULONG)___funcDispBase + *fp);
            DPRINTF (LOG_DEBUG, "                  I  n=%4d jv=0x%08lx jv->jmp=0x%04x jv->vec=0x%08lx\n", n, jv, jv->jmp, jv->vec);
            fp++; n++;
        }
    }
    else
    {
        APTR *fp = (APTR *)___functionArray;

        while (*fp != (APTR)-1)
        {
            struct JumpVec  *jv = &(((struct JumpVec *)(___target))[-n]);
            jv->jmp = JMPINSTR;
            jv->vec = *fp;
            DPRINTF (LOG_DEBUG, "                  II n=%4d jv=0x%08lx jv->jmp=0x%04x jv->vec=0x%08lx\n", n, jv, jv->jmp, jv->vec);
            fp++; n++;
        }
    }
    DPRINTF (LOG_DEBUG, "_exec: MakeFunctions done, %d entries set.\n", n);
}

struct Resident * __saveds _exec_FindResident ( register struct ExecBase * __libBase __asm("a6"),
                                                        register CONST_STRPTR ___name  __asm("a1"))
{
    DPRINTF (LOG_DEBUG, "_exec: FindResident unimplemented STUB called.\n");
    assert(FALSE);
}

APTR __saveds _exec_InitResident ( register struct ExecBase * __libBase __asm("a6"),
                                                        register const struct Resident * ___resident  __asm("a1"),
                                                        register ULONG ___segList  __asm("d1"))
{
    DPRINTF (LOG_DEBUG, "_exec: InitResident unimplemented STUB called.\n");
    assert(FALSE);
}

void __saveds _exec_Alert ( register struct ExecBase * __libBase __asm("a6"),
                                                        register ULONG ___alertNum  __asm("d7"))
{
    DPRINTF (LOG_DEBUG, "_exec: Alert unimplemented STUB called.\n");
    assert(FALSE);
}

void __saveds _exec_Debug ( register struct ExecBase * __libBase __asm("a6"),
                                                        register ULONG ___flags  __asm("d0"))
{
    DPRINTF (LOG_DEBUG, "_exec: Debug unimplemented STUB called.\n");
    assert(FALSE);
}

void __saveds _exec_Disable ( register struct ExecBase * __libBase __asm("a6"))
{
    DPRINTF (LOG_DEBUG, "_exec: Disable() called.\n");

    custom->intena = 0x4000;

	SysBase->IDNestCnt++;
}

void __saveds _exec_Enable ( register struct ExecBase * __libBase __asm("a6"))
{
    DPRINTF (LOG_DEBUG, "_exec: Enable() called.\n");
	SysBase->IDNestCnt--;
	if (SysBase->IDNestCnt<0)
        custom->intena = 0xc000; // 1100 0000 0000 0000
}

void __saveds _exec_Forbid ( register struct ExecBase * __libBase __asm("a6"))
{
    DPRINTF (LOG_DEBUG, "_exec: Forbid(): called.\n");
    SysBase->TDNestCnt++;
    DPRINTF (LOG_DEBUG, "_exec: Forbid() --> SysBase->TDNestCnt=%d\n", SysBase->TDNestCnt);
}

void __saveds _exec_Permit ( register struct ExecBase * __libBase __asm("a6"))
{
    DPRINTF (LOG_DEBUG, "_exec: Permit() called.\n");
    SysBase->TDNestCnt--;
    DPRINTF (LOG_DEBUG, "_exec: Permit() --> SysBase->TDNestCnt=%d\n", SysBase->TDNestCnt);
}

APTR __saveds _exec_SuperState ( register struct ExecBase * __libBase __asm("a6"))
{
    DPRINTF (LOG_DEBUG, "_exec: SuperState unimplemented STUB called.\n");
    assert(FALSE);
}

void __saveds _exec_UserState ( register struct ExecBase * __libBase __asm("a6"),
                                                        register APTR ___sysStack  __asm("d0"))
{
    DPRINTF (LOG_DEBUG, "_exec: UserState unimplemented STUB called.\n");
    assert(FALSE);
}

struct Interrupt * __saveds _exec_SetIntVector ( register struct ExecBase * __libBase __asm("a6"),
                                                        register LONG ___intNumber  __asm("d0"),
                                                        register const struct Interrupt * ___interrupt  __asm("a1"))
{
    DPRINTF (LOG_DEBUG, "_exec: SetIntVector unimplemented STUB called.\n");
    assert(FALSE);
}

void __saveds _exec_AddIntServer ( register struct ExecBase * __libBase __asm("a6"),
                                                        register LONG ___intNumber  __asm("d0"),
                                                        register struct Interrupt * ___interrupt  __asm("a1"))
{
    DPRINTF (LOG_DEBUG, "_exec: AddIntServer unimplemented STUB called.\n");
    assert(FALSE);
}

void __saveds _exec_RemIntServer ( register struct ExecBase * __libBase __asm("a6"),
                                                        register LONG ___intNumber  __asm("d0"),
                                                        register struct Interrupt * ___interrupt  __asm("a1"))
{
    DPRINTF (LOG_DEBUG, "_exec: RemIntServer unimplemented STUB called.\n");
    assert(FALSE);
}

void __saveds _exec_Cause ( register struct ExecBase * __libBase __asm("a6"),
                                                        register struct Interrupt * ___interrupt  __asm("a1"))
{
    DPRINTF (LOG_DEBUG, "_exec: Cause unimplemented STUB called.\n");
    assert(FALSE);
}

APTR __saveds _exec_Allocate ( register struct ExecBase * __libBase __asm("a6"),
                                      register struct MemHeader * ___freeList  __asm("a0"),
                                      register ULONG ___byteSize  __asm("d0"))
{
    DPRINTF (LOG_DEBUG, "_exec: Allocate called, ___freeList=0x%08lx, ___byteSize=%d\n", (ULONG)___freeList, ___byteSize);
    DPRINTF (LOG_DEBUG, "       mh_First=0x%08lx, mh_Lower=0x%08lx, mh_Upper=0x%08lx, mh_Free=%ld, mh_Attributes=0x%08lx\n", 
             ___freeList->mh_First, ___freeList->mh_Lower, ___freeList->mh_Upper, ___freeList->mh_Free, ___freeList->mh_Attributes);

    ULONG byteSize = ALIGN (___byteSize, 4);

    DPRINTF (LOG_DEBUG, "       rounded up byte size is %d\n", byteSize);

    if (!byteSize)
        return NULL;

    DPRINTF (LOG_DEBUG, "       ___freeList->mh_Free=%ld\n", ___freeList->mh_Free);
    if (___freeList->mh_Free < byteSize)
        return NULL;

    struct MemChunk *mc=NULL;
    struct MemChunk *p1, *p2;

    p1 = (struct MemChunk *)&___freeList->mh_First;
    p2 = p1->mc_Next;

    while (p2)
    {
        DPRINTF (LOG_DEBUG, "       looking for mem chunk that is large enough, current chunk at 0x%08lx is %d bytes\n", (ULONG)p2, p2->mc_Bytes);
        if (p2->mc_Bytes >= byteSize)
        {
            mc = p1;
            break;
        }
        p1 = p2;
        p2 = p1->mc_Next;
    }

    if (mc)
    {
        p1 = mc;
        p2 = p1->mc_Next;

        if (p2->mc_Bytes == byteSize)
        {
            p1->mc_Next = p2->mc_Next;
            mc          = p2;
        }
        else
        {
            struct MemChunk *pp = p1;

            p1->mc_Next = (struct MemChunk *)((UBYTE *)p2+byteSize);
            mc = p2;

            p1 = p1->mc_Next;
            p1->mc_Next  = p2->mc_Next;
            p1->mc_Bytes = p2->mc_Bytes-byteSize;
        }

        ___freeList->mh_Free -= byteSize;
        DPRINTF (LOG_DEBUG, "       found a chunk at 0x%08lx\n", (ULONG)mc);
    }
    else
    {
        DPRINTF (LOG_DEBUG, "       no chunk found.\n");
    }

    return mc;
}

void __saveds _exec_Deallocate ( register struct ExecBase  *SysBase     __asm("a6"),
                                        register struct MemHeader *freeList    __asm("a0"),
                                        register APTR              memoryBlock __asm("a1"),
                                        register ULONG             byteSize    __asm("d0"))
{
    DPRINTF (LOG_INFO, "_exec: Deallocate called, freeList=0x%08lx, memoryBlock=0x%08lx, byteSize=%ld\n", freeList, memoryBlock, byteSize);

	if(!byteSize || !memoryBlock)
		return;

	// alignment

    byteSize = ALIGN (byteSize, 4);

	struct MemChunk *pNext     = freeList->mh_First;
	struct MemChunk *pPrev     = (struct MemChunk *)&freeList->mh_First;
	struct MemChunk *pCurStart = (struct MemChunk *)memoryBlock;
	struct MemChunk *pCurEnd   = (struct MemChunk *) ((UBYTE *)pCurStart + byteSize);

	if (!pNext)	// empty list?
	{
		pCurStart->mc_Bytes = byteSize;
		pCurStart->mc_Next  = NULL;
		pPrev->mc_Next      = pCurStart;

		freeList->mh_Free  += byteSize;

		return;
	}

	// not empty -> find correct spot where to insert our block
	do
	{
		if (pNext >= pCurStart)
		{
			break;
		}
		pPrev = pNext;
		pNext = pNext->mc_Next;

	} while (pNext);

	// if we found a prev block, see if we can merge
	if (pPrev != (struct MemChunk *)&freeList->mh_First)
	{
		if ((UBYTE *)pPrev + pPrev->mc_Bytes == (UBYTE *)pCurStart)
			pCurStart = pPrev;
		else
			pPrev->mc_Next = pCurStart;
	}
	else
	{
		pPrev->mc_Next = pCurStart;
	}

	// if we have a next block, try to merge with it as well
	if (pNext && (pCurEnd == pNext))
	{
		pCurEnd += pNext->mc_Bytes;
		pNext = pNext->mc_Next;
	}

	pCurStart->mc_Next   = pNext;
	pCurStart->mc_Bytes  = (UBYTE *)pCurEnd - (UBYTE *)pCurStart;
	freeList->mh_Free  += byteSize;
}

APTR __saveds _exec_AllocMem ( register struct ExecBase *SysBase __asm("a6"),
                                      register ULONG ___byteSize  __asm("d0"),
                                      register ULONG ___requirements  __asm("d1"))
{
    DPRINTF (LOG_DEBUG, "_exec: AllocMem called, byteSize=%ld, requirements=0x%08lx\n",
             ___byteSize, ___requirements);

    if (!___byteSize)
        return NULL;

    Forbid();

    struct MemHeader *mhCur = (struct MemHeader *)SysBase->MemList.lh_Head;

    APTR mem = NULL;

    while (mhCur->mh_Node.ln_Succ)
    {
        DPRINTF (LOG_DEBUG, "                MemHeader mh_Free=%d, mh_Attributes=0x%08lx\n",
                 mhCur->mh_Free, mhCur->mh_Attributes);

        UWORD req = (UWORD) ___requirements;
        if ((mhCur->mh_Attributes & req) == req)
        {
            if (mhCur->mh_Free >= ___byteSize)
            {
                mem = Allocate (mhCur, ___byteSize);
                if (mem)
                    break;
            }
            else
            {
                DPRINTF (LOG_DEBUG, "      *** too small ***");
            }
        }
        else
        {
            DPRINTF (LOG_DEBUG, "      *** does not meet requirements ***");
        }

        mhCur = (struct MemHeader *)mhCur->mh_Node.ln_Succ;
    }

    Permit();

    if (___requirements & MEMF_CLEAR)
        memset(mem, 0, ___byteSize);

    DPRINTF (LOG_DEBUG, "_exec: AllocMem returning with mem=0x%08lx\n", (ULONG)mem);

    return mem;
}

APTR __saveds _exec_AllocAbs ( register struct ExecBase * __libBase __asm("a6"),
                                                        register ULONG ___byteSize  __asm("d0"),
                                                        register APTR ___location  __asm("a1"))
{
    DPRINTF (LOG_DEBUG, "_exec: AllocAbs unimplemented STUB called.\n");
    assert(FALSE);
}

void __saveds _exec_FreeMem ( register struct ExecBase * __libBase __asm("a6"),
                                     register APTR  memoryBlock  __asm("a1"),
                                     register ULONG byteSize     __asm("d0"))
{
    DPRINTF (LOG_INFO, "_exec: FreeMem called, memoryBlock=0x%08lx, byteSize=%ld\n", memoryBlock, byteSize);

    if (!byteSize || !memoryBlock)
		return;


    APTR blockEnd = memoryBlock + byteSize;

	Forbid();

	for ( struct Node *node = SysBase->MemList.lh_Head ; node->ln_Succ != NULL ; node = node->ln_Succ )
    {
		struct MemHeader *mh = (struct MemHeader *)node;

		if (mh->mh_Lower > memoryBlock || mh->mh_Upper < blockEnd)
			continue;

		Deallocate(mh, memoryBlock, byteSize);
		break;
    }

	Permit();
}

ULONG __saveds _exec_AvailMem ( register struct ExecBase * __libBase __asm("a6"),
                                       register ULONG ___requirements  __asm("d1"))
{
    DPRINTF (LOG_DEBUG, "_exec: AvailMem unimplemented STUB called.\n");
    assert(FALSE);
}

struct MemList * __saveds _exec_AllocEntry ( register struct ExecBase * __libBase __asm("a6"),
                                                    register struct MemList * entry  __asm("a0"))
{

    DPRINTF (LOG_DEBUG, "_exec: AllocEntry called, entry=0x%08lx, entry->ml_NumEntries=%d\n", entry, entry->ml_NumEntries);

    for (int i = 0; i < entry->ml_NumEntries; i++)
    {
        DPRINTF (LOG_DEBUG, "       # %3d me_Reqs=0x%08lx me_Length=%d\n", i, entry->ml_ME[i].me_Reqs, entry->ml_ME[i].me_Length);
    }

    ULONG size = sizeof(struct MemList) + sizeof(struct MemEntry) * (entry->ml_NumEntries-1);
    struct MemList *res = (struct MemList *) AllocMem (size, MEMF_PUBLIC);

    if (!res)
    {
        res =  (struct MemList*) (0x80000000 | MEMF_PUBLIC);
        return res;
    }

    res->ml_NumEntries   = entry->ml_NumEntries;
    res->ml_Node.ln_Type = 0;
    res->ml_Node.ln_Pri  = 0;
    res->ml_Node.ln_Name = NULL;

    for (int i = 0; i < entry->ml_NumEntries; i++)
    {
        if (entry->ml_ME[i].me_Length)
        {
            res->ml_ME[i].me_Addr = AllocMem(entry->ml_ME[i].me_Length, entry->ml_ME[i].me_Reqs);
            if (!res->ml_ME[i].me_Addr)
            {
                for (int j=0; j<i; j++)
                    FreeMem (res->ml_ME[j].me_Addr, res->ml_ME[j].me_Length);

                FreeMem (res, size);

                res =  (struct MemList*) (0x80000000 | entry->ml_ME[i].me_Reqs);
                return res;
            }
        }
        else
        {
            res->ml_ME[i].me_Addr = NULL;
        }

        res->ml_ME[i].me_Length = entry->ml_ME[i].me_Length;
    }

    return res;
}

void __saveds _exec_FreeEntry ( register struct ExecBase * __libBase __asm("a6"),
                                       register struct MemList * entry  __asm("a0"))
{
    DPRINTF (LOG_INFO, "_exec: FreeEntry called, entry=0x%08lx\n", entry);

    for (ULONG i=0; i<entry->ml_NumEntries; i++)
        FreeMem (entry->ml_ME[i].me_Addr, entry->ml_ME[i].me_Length);

    FreeMem (entry, sizeof(struct MemList) - sizeof(struct MemEntry) + (sizeof(struct MemEntry)*entry->ml_NumEntries));
}

void __saveds _exec_Insert ( register struct ExecBase * __libBase __asm("a6"),
                                                        register struct List * ___list  __asm("a0"),
                                                        register struct Node * ___node  __asm("a1"),
                                                        register struct Node * ___pred  __asm("a2"))
{
    DPRINTF (LOG_DEBUG, "_exec: Insert unimplemented STUB called.\n");
    assert(FALSE);
}

void __saveds _exec_AddHead ( register struct ExecBase * __libBase __asm("a6"),
                                     register struct List * list  __asm("a0"),
                                     register struct Node * node  __asm("a1"))
{
    DPRINTF (LOG_DEBUG, "_exec: AddHead called, list=0x%08lx, node=0x%08lx\n", list, node);

    node->ln_Succ = list->lh_Head;
    node->ln_Pred = (struct Node *) &list->lh_Head;

    list->lh_Head->ln_Pred = node;
    list->lh_Head          = node;
}

void __saveds _exec_AddTail ( register struct ExecBase * __libBase __asm("a6"),
                                     register struct List * ___list  __asm("a0"),
                                     register struct Node * ___node  __asm("a1"))
{
    DPRINTF (LOG_DEBUG, "_exec: AddTail called ___list=0x%08lx ___node=0x%08lx\n", ___list, ___node);

    ___node->ln_Succ              = (struct Node *)&___list->lh_Tail;
    ___node->ln_Pred              = ___list->lh_TailPred;

    ___list->lh_TailPred->ln_Succ = ___node;
    ___list->lh_TailPred          = ___node;
}

void __saveds _exec_Remove ( register struct ExecBase * __libBase __asm("a6"),
                                    register struct Node * node  __asm("a1"))
{
    DPRINTF (LOG_DEBUG, "_exec: Remove called, node=0x%08lx\n", node);

    assert (node);

    node->ln_Pred->ln_Succ = node->ln_Succ;
    node->ln_Succ->ln_Pred = node->ln_Pred;
}

struct Node * __saveds _exec_RemHead ( register struct ExecBase *__libBase __asm("a6"),
                                              register struct List     *list  __asm("a0"))
{
    DPRINTF (LOG_INFO, "_exec: RemHead called, list=0x%08lx\n", list);

    assert (list);

    struct Node *node = list->lh_Head->ln_Succ;
    if (node)
    {
        node->ln_Pred = (struct Node *)list;
        node          = list->lh_Head;
        list->lh_Head = node->ln_Succ;
    }

    return node;
}

struct Node * __saveds _exec_RemTail ( register struct ExecBase * __libBase __asm("a6"),
                                                        register struct List * ___list  __asm("a0"))
{
    DPRINTF (LOG_DEBUG, "_exec: RemTail unimplemented STUB called.\n");
    assert(FALSE);
}

void __saveds _exec_Enqueue ( register struct ExecBase * __libBase __asm("a6"),
                                     register struct List * list  __asm("a0"),
                                     register struct Node * node  __asm("a1"))
{
    DPRINTF (LOG_DEBUG, "_exec: Enqueue() called: list=0x%08lx, node=0x%08lx\n", list, node);

    struct Node * next;

    for ( next = list->lh_Head ; next->ln_Succ != NULL ; next = next->ln_Succ )
    {
        if (node->ln_Pri > next->ln_Pri)
            break;
    }

    node->ln_Pred          = next->ln_Pred;
    node->ln_Succ          = next;
    next->ln_Pred->ln_Succ = node;
    next->ln_Pred          = node;
}

struct Node * __saveds _exec_FindName ( register struct ExecBase * __libBase __asm("a6"),
                                               register struct List     * list      __asm("a0"),
                                               register CONST_STRPTR      name      __asm("a1"))
{
    DPRINTF (LOG_DEBUG, "_exec: FindName called list=0x%08lx, name=%s\n", list, name);

    if (!list)
        list = &SysBase->PortList;

    for (struct Node *node=list->lh_Head; node->ln_Succ; node=node->ln_Succ)
    {
        DPRINTF (LOG_DEBUG, "_exec: FindName node=0x%08lx, node->ln_Name=%s\n", node, node->ln_Name);
        if (node->ln_Name)
        {
            if (!strcmp (node->ln_Name, name))
                return node;
        }
    }

    return NULL;
}

APTR __saveds _exec_AddTask ( register struct ExecBase *SysBase __asm("a6"),
                                     register struct Task     *task    __asm("a1"),
                                     register const APTR       initPC  __asm("a2"),
                                     register const APTR       finalPC __asm("a3"))
{
    DPRINTF (LOG_INFO, "_exec: AddTask called task=0x%08lx, initPC=0x%08lx, finalPC=0x%08lx\n", task, initPC, finalPC);

    assert(task);

    U_prepareTask (task, initPC, finalPC, /*args=*/NULL);

    // launch it

    Disable();

    task->tc_State = TS_READY;
    Enqueue (&SysBase->TaskReady, &task->tc_Node);

    Enable();

    return task;
}

void __saveds _defaultTaskExit (void)
{
    DPUTS (LOG_INFO, "_exec: _defaultTaskExit() called\n");

    asm(
        "    move.l  4, a6      \n" // SysBase -> a6
        "    suba.l  a1, a1     \n" // #0 -> a1
        "    jsr    -288(a6)    \n" // RemTask(0)
	    : /* no outputs */
		: /* no inputs */
		: "cc", "d0", "d1", "a0", "a1", "a6"
    );
}

void __saveds _exec_RemTask ( register struct ExecBase * __libBase __asm("a6"),
                                     register struct Task * task  __asm("a1"))
{
    DPRINTF (LOG_INFO, "_exec: RemTask called, task=0x%08lx\n", task);

    struct Task    *me = SysBase->ThisTask;

    if (!task)
        task = me;

	if (task != me)
	{
		Disable();
		Remove(&task->tc_Node);
		Enable();
	}

	task->tc_State = TS_REMOVED;

	if (task == me)
		Forbid();

    struct MemList *ml;
	while ((ml = (struct MemList *) RemHead (&task->tc_MemEntry)) != NULL)
		FreeEntry (ml);

	if (task == me)
	{
        DPUTS (LOG_INFO, "_exec: RemTask calling Dispatch() via Supervisor()\n");
		asm(
            "   move.l  4, a6                   \n"     // SysBase -> a6
			"	move.l	#_exec_Dispatch, a5		\n"		// #exec_Dispatch -> a5
			"   jsr     -30(a6)					\n"		// Supervisor()
			: /* no outputs */
			: /* no inputs */
			: "cc", "d0", "d1", "a0", "a1", "a5", "a6"
		);
	}
}

struct Task * __saveds _exec_FindTask ( register struct ExecBase *SysBase __asm("a6"),
                                               register CONST_STRPTR     name    __asm("a1"))
{
    DPRINTF (LOG_DEBUG, "_exec: FindTask() called name=%s\n", name ? (char*) name : "NULL");

    struct Task *ret, *thisTask = SysBase->ThisTask;

    if (!name)
        return thisTask;

    Disable();

    ret = (struct Task *) FindName (&SysBase->TaskReady, name);
    if (!ret)
    {
        ret = (struct Task *) FindName (&SysBase->TaskWait, name);
        if (!ret)
        {
            char       *s1 = thisTask->tc_Node.ln_Name;
            const char *s2 = name;
            while (*s1 == *s2)
            {
                if (!*s2)
                {
                    ret = thisTask;
                    break;
                }
                s1++; s2++;
            }
        }
    }

    Enable();

    return ret;
}

BYTE __saveds _exec_SetTaskPri ( register struct ExecBase * __libBase __asm("a6"),
                                                        register struct Task * ___task  __asm("a1"),
                                                        register LONG ___priority  __asm("d0"))
{
    DPRINTF (LOG_ERROR, "_exec: SetTaskPri unimplemented STUB called.\n");
    assert(FALSE);
}

ULONG __saveds _exec_SetSignal ( register struct ExecBase *SysBase     __asm("a6"),
                                        register ULONG            newSignals  __asm("d0"),
                                        register ULONG            signalSet   __asm("d1"))
{
    DPRINTF (LOG_DEBUG, "_exec: SetSignal called, newSignals=0x%08lx, signalSet=0x%08lx\n", newSignals, signalSet);

    struct Task *me = SysBase->ThisTask;
    ULONG oldsigs = 0;

    if (me)
    {
        Disable();

        oldsigs = me->tc_SigRecvd;
        me->tc_SigRecvd = (oldsigs & ~signalSet) | (newSignals & signalSet);

        Enable();
    }

    return oldsigs;
}

ULONG __saveds _exec_SetExcept ( register struct ExecBase * SysBase    __asm("a6"),
                                        register ULONG             newSignals __asm("d0"),
                                        register ULONG             signalSet  __asm("d1"))
{
    DPRINTF (LOG_DEBUG, "_exec: SetExcept called, newSignals=0x%08lx, signalSet=0x%08lx (STUB ONLY)\n", newSignals, signalSet);

    struct Task *me = SysBase->ThisTask;

    Disable();

    ULONG oldsigs = me->tc_SigExcept;

    me->tc_SigExcept = (oldsigs & ~signalSet) | (newSignals & signalSet);

    // FIXME
    // if (me->tc_SigExcept & me->tc_SigRecvd)
    // {
    //     me->tc_Flags |= TF_EXCEPT;
    //     Reschedule();
    // }

    Enable();

    return oldsigs;
}

ULONG __saveds _exec_Wait ( register struct ExecBase * __libBase __asm("a6"),
                                                        register ULONG ___signalSet  __asm("d0"))
{
    DPRINTF (LOG_ERROR, "_exec: Wait unimplemented STUB called.\n");
    assert(FALSE);
}

void __saveds _exec_Signal ( register struct ExecBase * __libBase __asm("a6"),
                                                        register struct Task * ___task  __asm("a1"),
                                                        register ULONG ___signalSet  __asm("d0"))
{
    DPRINTF (LOG_ERROR, "_exec: Signal unimplemented STUB called.\n");
    assert(FALSE);
}

BYTE __saveds _exec_AllocSignal ( register struct ExecBase * SysBase    __asm("a6"),
                                         register BYTE              signalNum  __asm("d0"))
{
    DPRINTF (LOG_DEBUG, "_exec: AllocSignal called, signalNum=%d\n", signalNum);

    struct Task *me = SysBase->ThisTask;

    ULONG oldmask = me->tc_SigAlloc;

    if (signalNum < 0)
    {
		// find a free signal
		signalNum = 31;
		while (signalNum && ((1 << signalNum) & oldmask))
			signalNum--;

		if (!signalNum)
			return -1;

		DPRINTF (LOG_DEBUG, "_exec: AllocSignal -> auto selected signalNum=%d\n", signalNum);
    }

	ULONG newmask = 1 << signalNum;
	if (me->tc_SigAlloc & newmask)
		return -1;

    me->tc_SigAlloc  |=  newmask;
    me->tc_SigExcept &= ~newmask;
    me->tc_SigWait   &= ~newmask;

    Disable();
    me->tc_SigRecvd  &= ~newmask;
    Enable();

    return signalNum;
}

void __saveds _exec_FreeSignal ( register struct ExecBase *SysBase   __asm("a6"),
                                        register BYTE             signalNum __asm("d0"))
{
    DPRINTF (LOG_DEBUG, "_exec: FreeSignal() called.\n");

    if (signalNum != -1)
    {
        struct Task *me = SysBase->ThisTask;
        me->tc_SigAlloc &= ~(1 << signalNum);
    }
}

LONG __saveds _exec_AllocTrap ( register struct ExecBase * __libBase __asm("a6"),
                                                        register LONG ___trapNum  __asm("d0"))
{
    DPRINTF (LOG_ERROR, "_exec: AllocTrap unimplemented STUB called.\n");
    assert(FALSE);
}

void __saveds _exec_FreeTrap ( register struct ExecBase * __libBase __asm("a6"),
                                                        register LONG ___trapNum  __asm("d0"))
{
    DPRINTF (LOG_ERROR, "_exec: FreeTrap unimplemented STUB called.\n");
    assert(FALSE);
}

void __saveds _exec_AddPort ( register struct ExecBase * __libBase __asm("a6"),
                                                        register struct MsgPort * ___port  __asm("a1"))
{
    DPRINTF (LOG_ERROR, "_exec: AddPort unimplemented STUB called.\n");
    assert(FALSE);
}

void __saveds _exec_RemPort ( register struct ExecBase * __libBase __asm("a6"),
                                                        register struct MsgPort * ___port  __asm("a1"))
{
    DPRINTF (LOG_ERROR, "_exec: RemPort unimplemented STUB called.\n");
    assert(FALSE);
}

void __saveds _exec_PutMsg ( register struct ExecBase * __libBase __asm("a6"),
                                                        register struct MsgPort * ___port  __asm("a0"),
                                                        register struct Message * ___message  __asm("a1"))
{
    DPRINTF (LOG_ERROR, "_exec: PutMsg unimplemented STUB called.\n");
    assert(FALSE);
}

struct Message * __saveds _exec_GetMsg ( register struct ExecBase * __libBase __asm("a6"),
                                                        register struct MsgPort * ___port  __asm("a0"))
{
    DPRINTF (LOG_ERROR, "_exec: GetMsg unimplemented STUB called.\n");
    assert(FALSE);
}

void __saveds _exec_ReplyMsg ( register struct ExecBase * __libBase __asm("a6"),
                                                        register struct Message * ___message  __asm("a1"))
{
    DPRINTF (LOG_ERROR, "_exec: ReplyMsg unimplemented STUB called.\n");
    assert(FALSE);
}

struct Message * __saveds _exec_WaitPort ( register struct ExecBase * __libBase __asm("a6"),
                                                  register struct MsgPort * ___port  __asm("a0"))
{
    DPRINTF (LOG_ERROR, "_exec: WaitPort() unimplemented STUB called.\n");
    assert(FALSE);
}

struct MsgPort * __saveds _exec_FindPort ( register struct ExecBase * __libBase __asm("a6"),
                                                        register CONST_STRPTR ___name  __asm("a1"))
{
    DPRINTF (LOG_ERROR, "_exec: FindPort unimplemented STUB called.\n");
    assert(FALSE);
}

void __saveds _exec_AddLibrary ( register struct ExecBase * __libBase __asm("a6"),
                                                        register struct Library * ___library  __asm("a1"))
{
    DPRINTF (LOG_ERROR, "_exec: AddLibrary unimplemented STUB called.\n");
    assert(FALSE);
}

void __saveds _exec_RemLibrary ( register struct ExecBase * __libBase __asm("a6"),
                                                        register struct Library * ___library  __asm("a1"))
{
    DPRINTF (LOG_ERROR, "_exec: RemLibrary unimplemented STUB called.\n");
    assert(FALSE);
}

struct Library * __saveds _exec_OldOpenLibrary ( register struct ExecBase *SysBase __asm("a6"),
                                                 register CONST_STRPTR     libName __asm("a1"))
{
    DPRINTF (LOG_DEBUG, "_exec: OldOpenLibrary called, liName=%s\n", libName);
    return OpenLibrary (libName, 0);
}

void __saveds _exec_CloseLibrary ( register struct ExecBase *SysBase  __asm("a6"),
                                          register struct Library  *library  __asm("a1"))
{
    DPRINTF (LOG_INFO, "_exec: CloseLibrary() called, library = 0x%08lx\n", library);

    BPTR seglist;

    if (library)
    {
        Forbid();

        struct JumpVec *jv = &(((struct JumpVec *)(library))[-2]);
        libCloseFn_t closefn = jv->vec;

        closefn(library);

        Permit();
    }
}

APTR __saveds _exec_SetFunction ( register struct ExecBase * __libBase __asm("a6"),
                                                        register struct Library * ___library  __asm("a1"),
                                                        register LONG ___funcOffset  __asm("a0"),
                                                        register VOID (*___newFunction)()  __asm("d0"))
{
    DPRINTF (LOG_ERROR, "_exec: SetFunction unimplemented STUB called.\n");
    assert(FALSE);
}

void __saveds _exec_SumLibrary ( register struct ExecBase * __libBase __asm("a6"),
                                                        register struct Library * ___library  __asm("a1"))
{
    DPRINTF (LOG_ERROR, "_exec: SumLibrary unimplemented STUB called.\n");
    assert(FALSE);
}

void __saveds _exec_AddDevice ( register struct ExecBase * __libBase __asm("a6"),
                                                        register struct Device * ___device  __asm("a1"))
{
    DPRINTF (LOG_ERROR, "_exec: AddDevice unimplemented STUB called.\n");
    assert(FALSE);
}

void __saveds _exec_RemDevice ( register struct ExecBase * __libBase __asm("a6"),
                                                        register struct Device * ___device  __asm("a1"))
{
    DPRINTF (LOG_ERROR, "_exec: RemDevice unimplemented STUB called.\n");
    assert(FALSE);
}

BYTE __saveds _exec_OpenDevice ( register struct ExecBase  *SysBase    __asm("a6"),
                                        register CONST_STRPTR      devName    __asm("a0"),
                                        register ULONG             unit       __asm("d0"),
                                        register struct IORequest *ioRequest  __asm("a1"),
                                        register ULONG             flags      __asm("d1"))
{
    DPRINTF (LOG_DEBUG, "_exec: OpenDevice() called, devName=%s, unit=%d\n", devName, unit);

	Forbid();

    ioRequest->io_Unit   = NULL;
    ioRequest->io_Device = (struct Device *)FindName (&SysBase->DeviceList, devName);

    if (ioRequest->io_Device)
    {
        ioRequest->io_Error = 0;

        struct JumpVec *jv = &(((struct JumpVec *)(ioRequest->io_Device))[-1]);
        devOpenFn_t openfn = jv->vec;

        openfn (&ioRequest->io_Device->dd_Library, ioRequest, unit, flags);

        if (ioRequest->io_Error)
            ioRequest->io_Device = NULL;
    }
    else
	{
        ioRequest->io_Error = IOERR_OPENFAIL;
		DPRINTF (LOG_ERROR, "_exec: OpenDevice() couldn't find device '%s'\n", devName);
	}

    Permit();

    return ioRequest->io_Error;
}

void __saveds _exec_CloseDevice ( register struct ExecBase  *SysBase   __asm("a6"),
                                         register struct IORequest *ioRequest __asm("a1"))
{
    DPRINTF (LOG_DEBUG, "_exec: CloseDevice() called.\n");

    Forbid();

    if (ioRequest->io_Device)
    {
		struct JumpVec *jv = &(((struct JumpVec *)(ioRequest->io_Device))[-2]);
		devCloseFn_t closefn = jv->vec;

		closefn (&ioRequest->io_Device->dd_Library, ioRequest);

		// FIXME: expunge

        ioRequest->io_Device = NULL;
    }

    Permit();
}

BYTE __saveds _exec_DoIO ( register struct ExecBase  *SysBase    __asm("a6"),
                                  register struct IORequest *ioRequest  __asm("a1"))
{
    DPRINTF (LOG_DEBUG, "_exec: DoIO() called, ioRequest=0x%08lx, command: %d\n", ioRequest, ioRequest->io_Command);

    if (!ioRequest || !ioRequest->io_Device)
		return -1;

    ioRequest->io_Flags                   = IOF_QUICK;
    ioRequest->io_Message.mn_Node.ln_Type = 0;

	struct JumpVec *jv = &(((struct JumpVec *)(ioRequest->io_Device))[-5]);
	devBeginIOFn_t beginiofn = jv->vec;

	beginiofn (&ioRequest->io_Device->dd_Library, ioRequest);

    if (! (ioRequest->io_Flags & IOF_QUICK))
        WaitIO(ioRequest);

    return ioRequest->io_Error;
}

void __saveds _exec_SendIO ( register struct ExecBase * __libBase __asm("a6"),
                                                        register struct IORequest * ___ioRequest  __asm("a1"))
{
    DPRINTF (LOG_ERROR, "_exec: SendIO unimplemented STUB called.\n");
    assert(FALSE);
}

struct IORequest * __saveds _exec_CheckIO ( register struct ExecBase * __libBase __asm("a6"),
                                                        register struct IORequest * ___ioRequest  __asm("a1"))
{
    DPRINTF (LOG_ERROR, "_exec: CheckIO unimplemented STUB called.\n");
    assert(FALSE);
}

BYTE __saveds _exec_WaitIO ( register struct ExecBase * __libBase __asm("a6"),
                                                        register struct IORequest * ___ioRequest  __asm("a1"))
{
    DPRINTF (LOG_ERROR, "_exec: WaitIO unimplemented STUB called.\n");
    assert(FALSE);
}

void __saveds _exec_AbortIO ( register struct ExecBase * __libBase __asm("a6"),
                                                        register struct IORequest * ___ioRequest  __asm("a1"))
{
    DPRINTF (LOG_ERROR, "_exec: AbortIO unimplemented STUB called.\n");
    assert(FALSE);
}

void __saveds _exec_AddResource ( register struct ExecBase * __libBase __asm("a6"),
                                                        register APTR ___resource  __asm("a1"))
{
    DPRINTF (LOG_ERROR, "_exec: AddResource unimplemented STUB called.\n");
    assert(FALSE);
}

void __saveds _exec_RemResource ( register struct ExecBase * __libBase __asm("a6"),
                                                        register APTR ___resource  __asm("a1"))
{
    DPRINTF (LOG_ERROR, "_exec: RemResource unimplemented STUB called.\n");
    assert(FALSE);
}

APTR __saveds _exec_OpenResource ( register struct ExecBase * __libBase __asm("a6"),
                                                        register CONST_STRPTR ___resName  __asm("a1"))
{
    DPRINTF (LOG_ERROR, "_exec: OpenResource unimplemented STUB called.\n");
    assert(FALSE);
}

APTR __saveds _exec_RawDoFmt ( register struct ExecBase * __libBase __asm("a6"),
                                                        register CONST_STRPTR ___formatString  __asm("a0"),
                                                        register const APTR ___dataStream  __asm("a1"),
                                                        register VOID (*___putChProc)()  __asm("a2"),
                                                        register APTR ___putChData  __asm("a3"))
{
    DPRINTF (LOG_ERROR, "_exec: RawDoFmt unimplemented STUB called.\n");
    assert(FALSE);
}

ULONG __saveds exec_GetCC ( register struct ExecBase * __libBase __asm("a6"));

asm(
"_exec_GetCC:                    \n"
"       move.w      sr, d0       \n"
"       rts                      \n");

ULONG __saveds _exec_TypeOfMem ( register struct ExecBase * __libBase __asm("a6"),
                                                        register const APTR ___address  __asm("a1"))
{
    DPRINTF (LOG_ERROR, "_exec: TypeOfMem unimplemented STUB called.\n");
    assert(FALSE);
}

ULONG __saveds _exec_Procure ( register struct ExecBase * __libBase __asm("a6"),
                                                        register struct SignalSemaphore * ___sigSem  __asm("a0"),
                                                        register struct SemaphoreMessage * ___bidMsg  __asm("a1"))
{
    DPRINTF (LOG_ERROR, "_exec: Procure unimplemented STUB called.\n");
    assert(FALSE);
}

void __saveds _exec_Vacate ( register struct ExecBase * __libBase __asm("a6"),
                                                        register struct SignalSemaphore * ___sigSem  __asm("a0"),
                                                        register struct SemaphoreMessage * ___bidMsg  __asm("a1"))
{
    DPRINTF (LOG_ERROR, "_exec: Vacate unimplemented STUB called.\n");
    assert(FALSE);
}

struct Library * __saveds _exec_OpenLibrary ( register struct ExecBase *SysBase __asm("a6"),
                                                     register CONST_STRPTR     libName  __asm("a1"),
                                                     register ULONG            version  __asm("d0"))
{
    DPRINTF (LOG_DEBUG, "_exec: OpenLibrary called libName=%s, version=%ld\n", libName, version);

    Forbid();

    struct Library *lib = (struct Library *) FindName (&SysBase->LibList, libName);

    Permit();

    if (lib)
    {
        if (lib->lib_Version < version)
        {
            DPRINTF (LOG_DEBUG, "_exec: OpenLibrary version is too old: lib->lib_Version=%ld, version=%ld\n", lib->lib_Version, version);
            return NULL;
        }

        struct JumpVec *jv = &(((struct JumpVec *)(lib))[-1]);
        libOpenFn_t openfn = jv->vec;

        lib = openfn(lib);

        DPRINTF (LOG_DEBUG, "_exec: OpenLibrary done libName=%s, version=%ld -> lib=0x%08lx\n", libName, version, lib);
    }
    else
    {
        DPRINTF (LOG_ERROR, "\n_exec: OpenLibrary: *** ERROR: requested library %s was not found.\n\n", libName);
    }

    return lib;
}

void __saveds _exec_InitSemaphore ( register struct ExecBase * __libBase __asm("a6"),
                                                        register struct SignalSemaphore * ___sigSem  __asm("a0"))
{
    DPRINTF (LOG_DEBUG, "_exec: InitSemaphore unimplemented STUB called.\n");
    assert(FALSE);
}

void __saveds _exec_ObtainSemaphore ( register struct ExecBase * __libBase __asm("a6"),
                                                        register struct SignalSemaphore * ___sigSem  __asm("a0"))
{
    DPRINTF (LOG_DEBUG, "_exec: ObtainSemaphore unimplemented STUB called.\n");
    assert(FALSE);
}

void __saveds _exec_ReleaseSemaphore ( register struct ExecBase * __libBase __asm("a6"),
                                                        register struct SignalSemaphore * ___sigSem  __asm("a0"))
{
    DPRINTF (LOG_DEBUG, "_exec: ReleaseSemaphore unimplemented STUB called.\n");
    assert(FALSE);
}

ULONG __saveds _exec_AttemptSemaphore ( register struct ExecBase * __libBase __asm("a6"),
                                                        register struct SignalSemaphore * ___sigSem  __asm("a0"))
{
    DPRINTF (LOG_DEBUG, "_exec: AttemptSemaphore unimplemented STUB called.\n");
    assert(FALSE);
}

void __saveds _exec_ObtainSemaphoreList ( register struct ExecBase * __libBase __asm("a6"),
                                                        register struct List * ___sigSem  __asm("a0"))
{
    DPRINTF (LOG_DEBUG, "_exec: ObtainSemaphoreList unimplemented STUB called.\n");
    assert(FALSE);
}

void __saveds _exec_ReleaseSemaphoreList ( register struct ExecBase * __libBase __asm("a6"),
                                                        register struct List * ___sigSem  __asm("a0"))
{
    DPRINTF (LOG_DEBUG, "_exec: ReleaseSemaphoreList unimplemented STUB called.\n");
    assert(FALSE);
}

struct SignalSemaphore * __saveds _exec_FindSemaphore ( register struct ExecBase * __libBase __asm("a6"),
                                                        register STRPTR ___name  __asm("a1"))
{
    DPRINTF (LOG_DEBUG, "_exec: FindSemaphore unimplemented STUB called.\n");
    assert(FALSE);
}

void __saveds _exec_AddSemaphore ( register struct ExecBase * __libBase __asm("a6"),
                                                        register struct SignalSemaphore * ___sigSem  __asm("a1"))
{
    DPRINTF (LOG_DEBUG, "_exec: AddSemaphore unimplemented STUB called.\n");
    assert(FALSE);
}

void __saveds _exec_RemSemaphore ( register struct ExecBase * __libBase __asm("a6"),
                                                        register struct SignalSemaphore * ___sigSem  __asm("a1"))
{
    DPRINTF (LOG_DEBUG, "_exec: RemSemaphore unimplemented STUB called.\n");
    assert(FALSE);
}

ULONG __saveds _exec_SumKickData ( register struct ExecBase * __libBase __asm("a6"))
{
    DPRINTF (LOG_DEBUG, "_exec: SumKickData unimplemented STUB called.\n");
    assert(FALSE);
}

void __saveds _exec_AddMemList ( register struct ExecBase * __libBase __asm("a6"),
                                                        register ULONG ___size  __asm("d0"),
                                                        register ULONG ___attributes  __asm("d1"),
                                                        register LONG ___pri  __asm("d2"),
                                                        register APTR ___base  __asm("a0"),
                                                        register CONST_STRPTR ___name  __asm("a1"))
{
    DPRINTF (LOG_ERROR, "_exec: AddMemList unimplemented STUB called.\n");
    assert(FALSE);
}

void __saveds _exec_CopyMem ( register struct ExecBase *SysBase __asm("a6"),
                                     register const APTR       source  __asm("a0"),
                                     register APTR             dest    __asm("a1"),
                                     register ULONG            size    __asm("d0"))
{
    DPRINTF (LOG_DEBUG, "_exec: CopyMem called source=0x%08lx, dest=0x%08lx, size=%d\n", source, dest, size);

    if (!size)
        return;

    // FIXME: optimize for speed

    UBYTE *src = (UBYTE *)source;
    UBYTE *dst = (UBYTE *)dest;

    for (ULONG i = 0; i<size; i++)
        *dst++ = *src++;

}

void __saveds _exec_CopyMemQuick ( register struct ExecBase * __libBase __asm("a6"),
                                                        register const APTR ___source  __asm("a0"),
                                                        register APTR ___dest  __asm("a1"),
                                                        register ULONG ___size  __asm("d0"))
{
    DPRINTF (LOG_ERROR, "_exec: CopyMemQuick unimplemented STUB called.\n");
    assert(FALSE);
}

void __saveds _exec_CacheClearU ( register struct ExecBase * __libBase __asm("a6"))
{
    DPRINTF (LOG_ERROR, "_exec: CacheClearU unimplemented STUB called.\n");
    assert(FALSE);
}

void __saveds _exec_CacheClearE ( register struct ExecBase * __libBase __asm("a6"),
                                                        register APTR ___address  __asm("a0"),
                                                        register ULONG ___length  __asm("d0"),
                                                        register ULONG ___caches  __asm("d1"))
{
    DPRINTF (LOG_ERROR, "_exec: CacheClearE unimplemented STUB called.\n");
    assert(FALSE);
}

ULONG __saveds _exec_CacheControl ( register struct ExecBase * __libBase __asm("a6"),
                                                        register ULONG ___cacheBits  __asm("d0"),
                                                        register ULONG ___cacheMask  __asm("d1"))
{
    DPRINTF (LOG_ERROR, "_exec: CacheControl unimplemented STUB called.\n");
    assert(FALSE);
}

APTR __saveds _exec_CreateIORequest ( register struct ExecBase * __libBase __asm("a6"),
                                                        register const struct MsgPort * ___port  __asm("a0"),
                                                        register ULONG ___size  __asm("d0"))
{
    DPRINTF (LOG_ERROR, "_exec: CreateIORequest unimplemented STUB called.\n");
    assert(FALSE);
}

void __saveds _exec_DeleteIORequest ( register struct ExecBase * __libBase __asm("a6"),
                                                        register APTR ___iorequest  __asm("a0"))
{
    DPRINTF (LOG_ERROR, "_exec: DeleteIORequest unimplemented STUB called.\n");
    assert(FALSE);
}

struct MsgPort * __saveds _exec_CreateMsgPort ( register struct ExecBase * __libBase __asm("a6"))
{
    DPRINTF (LOG_ERROR, "_exec: CreateMsgPort unimplemented STUB called.\n");
    assert(FALSE);
}

void __saveds _exec_DeleteMsgPort ( register struct ExecBase * __libBase __asm("a6"),
                                                        register struct MsgPort * ___port  __asm("a0"))
{
    DPRINTF (LOG_ERROR, "_exec: DeleteMsgPort unimplemented STUB called.\n");
    assert(FALSE);
}

void __saveds _exec_ObtainSemaphoreShared ( register struct ExecBase * __libBase __asm("a6"),
                                                        register struct SignalSemaphore * ___sigSem  __asm("a0"))
{
    DPRINTF (LOG_ERROR, "_exec: ObtainSemaphoreShared unimplemented STUB called.\n");
    assert(FALSE);
}

APTR __saveds _exec_AllocVec ( register struct ExecBase * __libBase        __asm("a6"),
                                      register ULONG             ___byteSize      __asm("d0"),
                                      register ULONG             ___requirements  __asm("d1"))
{
    DPRINTF (LOG_DEBUG, "_exec: AllocVec() called ___byteSize=%ld, ___requirements=0x%08lx\n", ___byteSize, ___requirements);

    if (!___byteSize)
        return NULL;

    ULONG *m = (ULONG*) AllocMem (___byteSize+4, ___requirements);
    if (!m)
        return NULL;

    *m++ = ___byteSize;

    return m;
}

void __saveds _exec_FreeVec ( register struct ExecBase *SysBase     __asm("a6"),
                              register APTR             memoryBlock __asm("a1"))
{
    DPRINTF (LOG_DEBUG, "_exec: FreeVec called, memoryBlock=0x%08lx\n", memoryBlock);

    if (!memoryBlock)
        return;

    memoryBlock -= 4;
    FreeMem (memoryBlock, *((ULONG *) memoryBlock));
}

APTR __saveds _exec_CreatePool ( register struct ExecBase * __libBase __asm("a6"),
                                                        register ULONG ___requirements  __asm("d0"),
                                                        register ULONG ___puddleSize  __asm("d1"),
                                                        register ULONG ___threshSize  __asm("d2"))
{
    DPRINTF (LOG_DEBUG, "_exec: CreatePool unimplemented STUB called.\n");
    assert(FALSE);
}

void __saveds _exec_DeletePool ( register struct ExecBase * __libBase __asm("a6"),
                                                        register APTR ___poolHeader  __asm("a0"))
{
    DPRINTF (LOG_DEBUG, "_exec: DeletePool unimplemented STUB called.\n");
    assert(FALSE);
}

APTR __saveds _exec_AllocPooled ( register struct ExecBase * __libBase __asm("a6"),
                                                        register APTR ___poolHeader  __asm("a0"),
                                                        register ULONG ___memSize  __asm("d0"))
{
    DPRINTF (LOG_DEBUG, "_exec: AllocPooled unimplemented STUB called.\n");
    assert(FALSE);
}

void __saveds _exec_FreePooled ( register struct ExecBase * __libBase __asm("a6"),
                                                        register APTR ___poolHeader  __asm("a0"),
                                                        register APTR ___memory  __asm("a1"),
                                                        register ULONG ___memSize  __asm("d0"))
{
    DPRINTF (LOG_DEBUG, "_exec: FreePooled unimplemented STUB called.\n");
    assert(FALSE);
}

ULONG __saveds _exec_AttemptSemaphoreShared ( register struct ExecBase * __libBase __asm("a6"),
                                                        register struct SignalSemaphore * ___sigSem  __asm("a0"))
{
    DPRINTF (LOG_DEBUG, "_exec: AttemptSemaphoreShared unimplemented STUB called.\n");
    assert(FALSE);
}

void __saveds _exec_ColdReboot ( register struct ExecBase * __libBase __asm("a6"))
{
    DPRINTF (LOG_DEBUG, "_exec: ColdReboot unimplemented STUB called.\n");
    assert(FALSE);
}

void __saveds _exec_StackSwap ( register struct ExecBase * __libBase __asm("a6"),
                                                        register struct StackSwapStruct * ___newStack  __asm("a0"))
{
    DPRINTF (LOG_DEBUG, "_exec: StackSwap unimplemented STUB called.\n");
    assert(FALSE);
}

APTR __saveds _exec_CachePreDMA ( register struct ExecBase * __libBase __asm("a6"),
                                                        register const APTR ___address  __asm("a0"),
                                                        register ULONG * ___length  __asm("a1"),
                                                        register ULONG ___flags  __asm("d0"))
{
    DPRINTF (LOG_DEBUG, "_exec: CachePreDMA unimplemented STUB called.\n");
    assert(FALSE);
}

void __saveds _exec_CachePostDMA ( register struct ExecBase * __libBase __asm("a6"),
                                                        register const APTR ___address  __asm("a0"),
                                                        register ULONG * ___length  __asm("a1"),
                                                        register ULONG ___flags  __asm("d0"))
{
    DPRINTF (LOG_DEBUG, "_exec: CachePostDMA unimplemented STUB called.\n");
    assert(FALSE);
}

void __saveds _exec_AddMemHandler ( register struct ExecBase * __libBase __asm("a6"),
                                                        register struct Interrupt * ___memhand  __asm("a1"))
{
    DPRINTF (LOG_ERROR, "_exec: AddMemHandler unimplemented STUB called.\n");
    assert(FALSE);
}

void __saveds _exec_RemMemHandler ( register struct ExecBase * __libBase __asm("a6"),
                                                        register struct Interrupt * ___memhand  __asm("a1"))
{
    DPRINTF (LOG_ERROR, "_exec: RemMemHandler unimplemented STUB called.\n");
    assert(FALSE);
}

static void registerBuiltInLib (struct Library *libBase, ULONG num_funcs, struct Resident *romTAG)
{
    DPRINTF (LOG_DEBUG, "_exec: registerBuiltInLib libBase=0x%08lx, num_funcs=%ld, romTAG rt_Name=%s rt_IdString=%s\n", libBase, num_funcs, romTAG->rt_Name, romTAG->rt_IdString);
    struct InitTable *initTab = romTAG->rt_Init;
    _makeLibrary(libBase, initTab->FunctionTable, initTab->DataTable, initTab->InitLibFn, num_funcs*6, initTab->LibBaseSize, /* segList=*/NULL);
    AddTail (&SysBase->LibList, (struct Node*) libBase);
}

static void registerBuiltInDev (struct Library *libBase, ULONG num_funcs, struct Resident *romTAG)
{
    DPRINTF (LOG_DEBUG, "_exec: registerBuiltInDev libBase=0x%08lx, num_funcs=%ld, romTAG rt_Name=%s rt_IdString=%s\n", libBase, num_funcs, romTAG->rt_Name, romTAG->rt_IdString);
    struct InitTable *initTab = romTAG->rt_Init;
    _makeLibrary(libBase, initTab->FunctionTable, initTab->DataTable, initTab->InitLibFn, num_funcs*6, initTab->LibBaseSize, /* segList=*/NULL);
    AddTail (&SysBase->DeviceList, (struct Node*) libBase);
}

#if 0
static void __saveds _myTestTask(void)
{
    for (int i = 0; i<10000; i++)
    {
        DPRINTF (LOG_INFO, "_exec: test task iter SysBase->Elapsed=%d, SysBase->Quantum=%d\n", SysBase->Elapsed, SysBase->Quantum);
    }
    DPUTS (LOG_INFO, "_exec: test task done\n");
}
#endif

typedef ULONG __saveds (*cliChildFn_t) ( register ULONG  arglen __asm("d0"),
                                         register STRPTR args   __asm("a0"));
void __saveds _bootstrap(void)
{
    DPRINTF (LOG_INFO, "_exec: _bootstrap() called\n");

    // just for testing purposes add another task
    // FIXME: remove _createTask ("exec test task", 0, _myTestTask, 8192);

    // load our program

    char binfn[1024];
    emucall1 (EMU_CALL_LOADFILE, (ULONG) &binfn);
    DPRINTF (LOG_INFO, "_exec: _bootstrap(): loadfile: %s\n", binfn);

    OpenLibrary ("dos.library", 0);
    OpenLibrary ("utility.library", 0);
    BPTR segs = LoadSeg (binfn);

    DPRINTF (LOG_INFO, "_exec: _bootstrap(): segs=0x%08lx\n", segs);

    if (!segs)
    {
        LPRINTF (LOG_ERROR, "_exec: _bootstrap(): failed to load %s\n", binfn);
        emu_stop(1);
    }

    // inject initPC pointing to our loaded code into our task's stack

    APTR initPC = BADDR(segs+1);
    DPRINTF (LOG_INFO, "_exec: _bootstrap(): initPC is 0x%08lx\n", initPC);
    U_hexdump (LOG_DEBUG, initPC, 32);

    /* simply JSR() into our child process */

    cliChildFn_t childfn = initPC;
    ULONG rv = childfn (/*arglen=*/1, /*args=*/(STRPTR)"\n");

#if 0
    //*((APTR*) SysBase->ThisTask->tc_SPReg) = initPC;

    DPRINTF (LOG_INFO, "_exec: about to launch a new process for %s ...\n", binfn);

    //emucall1 (EMU_CALL_TRACE, TRUE); // FIXME: remove/disable

    // FIXME childHomeDirLock = Lock ((STRPTR)env->dirbuf, ACCESS_READ);

    struct Process *child = CreateNewProcTags(NP_Seglist     , (ULONG) segs,
                                              NP_FreeSeglist , FALSE,
                                              NP_Input       , Input(),
                                              NP_Output      , Output(),
                                              NP_CloseInput  , FALSE,
                                              NP_CloseOutput , FALSE,
                                              NP_StackSize   , DEFAULT_STACKSIZE,
                                              NP_Name        , (ULONG) binfn,
                                              //NP_WindowPtr , 0l,
                                              /* FIXME: NP_HomeDir,     env->childHomeDirLock, */
                                              NP_CopyVars    , FALSE,
                                              NP_Cli         , TRUE,
                                              NP_Arguments   , (ULONG) "\n",
                                              TAG_DONE);

    DPRINTF (LOG_INFO, "_exec: new process for %s created: 0x%08lx\n", binfn, child);

    DPUTS (LOG_INFO, "_exec: _bootstrap() DONE -> endless loop\n");

    while (TRUE);
    //    DPRINTF (LOG_INFO, "bootstrap() loop, SysBase->TDNestCnt=%d\n", SysBase->TDNestCnt);
#endif

    emu_stop(rv);
}

void __saveds coldstart (void)
{

    // setup exceptions, traps, interrupts
    uint32_t *p;

    p = (uint32_t*) 0x00000008; *p = (uint32_t) handleVec02; // bus error
    p = (uint32_t*) 0x0000000c; *p = (uint32_t) handleVec03; // address error
    p = (uint32_t*) 0x00000010; *p = (uint32_t) handleVec04; // illegal instruction
    p = (uint32_t*) 0x00000014; *p = (uint32_t) handleVec05; // divide by 0
    p = (uint32_t*) 0x00000018; *p = (uint32_t) handleVec06; // chk
    p = (uint32_t*) 0x0000001c; *p = (uint32_t) handleVec07; // trap v
    p = (uint32_t*) 0x00000020; *p = (uint32_t) handleVec08; // privilege violation
    p = (uint32_t*) 0x00000024; *p = (uint32_t) handleVec09; // trace
    p = (uint32_t*) 0x00000028; *p = (uint32_t) handleVec10; // line 1010
    p = (uint32_t*) 0x0000002c; *p = (uint32_t) handleVec11; // line 1111

    p = (uint32_t*) 0x0000006c; *p = (uint32_t) handleIRQ3;

    //__asm("    ori.w  #0x0700, sr;\n");   // disable interrupts
    //__asm("andi.w  #0xdfff, sr\n");   // disable supervisor bit
    //__asm("andi.w  #0xdfff, sr\n");   // disable supervisor bit
    __asm("move.l  #0x009ffff0, a7\n"); // setup initial stack

    DPRINTF (LOG_INFO, "coldstart: EXEC_VECTORS_START = 0x%08lx\n", EXEC_VECTORS_START);
    DPRINTF (LOG_INFO, "           EXEC_BASE_START    = 0x%08lx\n", EXEC_BASE_START   );
    DPRINTF (LOG_INFO, "           EXEC_MH_START      = 0x%08lx\n", EXEC_MH_START     );
    DPRINTF (LOG_INFO, "           DOS_VECTORS_START  = 0x%08lx\n", DOS_VECTORS_START );
    DPRINTF (LOG_INFO, "           DOS_BASE_START     = 0x%08lx\n", DOS_BASE_START    );
    DPRINTF (LOG_INFO, "           RAM_START          = 0x%08lx\n", RAM_START         );
    DPRINTF (LOG_INFO, "           RAM_END            = 0x%08lx\n", RAM_END           );

    // coldstart is at 0x00f801be, &SysBase at 0x00f80c90, SysBase at 0x0009eed0, f1 at 0x00f80028
    DPRINTF (LOG_DEBUG, "coldstart: locations in RAM: coldstart is at 0x%08x, &SysBase at 0x%08x, SysBase at 0x%08x\n",
             coldstart, &SysBase, SysBase);
    // g_ExecJumpTable at 0x00001800, &g_ExecJumpTable[0].vec at 0x00001802
    DPRINTF (LOG_DEBUG, "coldstart: g_ExecJumpTable at 0x%08x, &g_ExecJumpTable[0].vec at 0x%08x\n", g_ExecJumpTable, &g_ExecJumpTable[0].vec);

    /* set up execbase */

    *(APTR *)4L = SysBase;

    for (int i = 0; i<NUM_EXEC_FUNCS; i++)
    {
        g_ExecJumpTable[i].vec = _exec_unimplemented_call;
        g_ExecJumpTable[i].jmp = JMPINSTR;
    }


    g_ExecJumpTable[EXEC_FUNCTABLE_ENTRY( -30)].vec = exec_Supervisor;
    //g_ExecJumpTable[EXEC_FUNCTABLE_ENTRY(-36)].vec = _exec_ExitIntr;
    g_ExecJumpTable[EXEC_FUNCTABLE_ENTRY( -42)].vec = exec_Schedule;
    //g_ExecJumpTable[EXEC_FUNCTABLE_ENTRY(-48)].vec = _exec_Reschedule;
    g_ExecJumpTable[EXEC_FUNCTABLE_ENTRY( -54)].vec = exec_Switch;
    g_ExecJumpTable[EXEC_FUNCTABLE_ENTRY( -60)].vec = exec_Dispatch;
    //g_ExecJumpTable[EXEC_FUNCTABLE_ENTRY(-66)].vec = _exec_Exception;
    g_ExecJumpTable[EXEC_FUNCTABLE_ENTRY( -72)].vec = _exec_InitCode;
    g_ExecJumpTable[EXEC_FUNCTABLE_ENTRY( -78)].vec = _exec_InitStruct;
    g_ExecJumpTable[EXEC_FUNCTABLE_ENTRY( -84)].vec = _exec_MakeLibrary;
    g_ExecJumpTable[EXEC_FUNCTABLE_ENTRY( -90)].vec = _exec_MakeFunctions;
    g_ExecJumpTable[EXEC_FUNCTABLE_ENTRY( -96)].vec = _exec_FindResident;
    g_ExecJumpTable[EXEC_FUNCTABLE_ENTRY(-102)].vec = _exec_InitResident;
    g_ExecJumpTable[EXEC_FUNCTABLE_ENTRY(-108)].vec = _exec_Alert;
    g_ExecJumpTable[EXEC_FUNCTABLE_ENTRY(-114)].vec = _exec_Debug;
    g_ExecJumpTable[EXEC_FUNCTABLE_ENTRY(-120)].vec = _exec_Disable;
    g_ExecJumpTable[EXEC_FUNCTABLE_ENTRY(-126)].vec = _exec_Enable;
    g_ExecJumpTable[EXEC_FUNCTABLE_ENTRY(-132)].vec = _exec_Forbid;
    g_ExecJumpTable[EXEC_FUNCTABLE_ENTRY(-138)].vec = _exec_Permit;
    g_ExecJumpTable[EXEC_FUNCTABLE_ENTRY(-144)].vec =  exec_SetSR;
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
    g_ExecJumpTable[EXEC_FUNCTABLE_ENTRY(-528)].vec = exec_GetCC;
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

    NEWLIST (&SysBase->MemList);
    SysBase->MemList.lh_Type = NT_MEMORY;

    struct MemChunk *myc = (struct MemChunk*)((uint8_t *)RAM_START);
    myc->mc_Next  = NULL;
    myc->mc_Bytes = RAM_SIZE;

    struct MemHeader *myh = (struct MemHeader*) ((uint8_t *)EXEC_MH_START);
    myh->mh_Node.ln_Type = NT_MEMORY;
    myh->mh_Node.ln_Pri  = 0;
    myh->mh_Node.ln_Name = NULL;
    myh->mh_Attributes   = MEMF_CHIP | MEMF_PUBLIC;
    myh->mh_First        = myc;
    myh->mh_Lower        = (APTR) RAM_START;
    myh->mh_Upper        = (APTR) (RAM_END + 1);
    myh->mh_Free         = RAM_SIZE;

    DPRINTF (LOG_DEBUG, "coldstart: setting up first struct MemHeader *myh at 0x%08lx:\n", myh);
    DPRINTF (LOG_DEBUG, "           myh->mh_First=0x%08lx, myh->mh_Lower=0x%08lx, myh->mh_Upper=0x%08lx, myh->mh_Free=%ld, myh->mh_Attributes=0x%08lx\n", 
             myh->mh_First, myh->mh_Lower, myh->mh_Upper, myh->mh_Free, myh->mh_Attributes);

    AddTail (&SysBase->MemList, &myh->mh_Node);

    // init and register built-in libraries
    NEWLIST (&SysBase->LibList);
    SysBase->LibList.lh_Type = NT_LIBRARY;

    registerBuiltInLib ((struct Library *) DOSBase      , NUM_DOS_FUNCS       , __lxa_dos_ROMTag       );
    registerBuiltInLib ((struct Library *) UtilityBase  , NUM_UTILITY_FUNCS   , __lxa_utility_ROMTag   );
    registerBuiltInLib ((struct Library *) MathBase     , NUM_MATHFFP_FUNCS   , __lxa_mathffp_ROMTag   );
    registerBuiltInLib ((struct Library *) MathTransBase, NUM_MATHTRANS_FUNCS , __lxa_mathtrans_ROMTag );
    registerBuiltInLib ((struct Library *) GfxBase      , NUM_GRAPHICS_FUNCS  , __lxa_graphics_ROMTag  );
    registerBuiltInLib ((struct Library *) IntuitionBase, NUM_INTUITION_FUNCS , __lxa_intuition_ROMTag );
    registerBuiltInLib ((struct Library *) ExpansionBase, NUM_EXPANSION_FUNCS , __lxa_expansion_ROMTag );

    // init and register built-in devices
    NEWLIST (&SysBase->DeviceList);
    SysBase->DeviceList.lh_Type = NT_DEVICE;

    registerBuiltInDev ((struct Library *) DeviceInputBase  , NUM_DEVICE_INPUT_FUNCS  , __lxa_input_ROMTag  );
    registerBuiltInDev ((struct Library *) DeviceConsoleBase, NUM_DEVICE_CONSOLE_FUNCS, __lxa_console_ROMTag);

    // init multitasking
    NEWLIST (&SysBase->TaskReady);
    SysBase->TaskReady.lh_Type = NT_TASK;
    NEWLIST (&SysBase->TaskWait);
    SysBase->TaskWait.lh_Type = NT_TASK;

    SysBase->TaskExitCode = _defaultTaskExit;
    SysBase->Quantum      = DEFAULT_SCHED_QUANTUM;
    SysBase->Elapsed      = DEFAULT_SCHED_QUANTUM;
    SysBase->SysFlags     = 0;
    SysBase->IDNestCnt    = -1;
    SysBase->TDNestCnt    = -1;

    // create a bootstrap process
    struct Process *rootProc = (struct Process *) U_allocTask ("exec bootstrap", 0, DEFAULT_STACKSIZE, /*isProcess=*/ TRUE);
    U_prepareProcess (rootProc, _bootstrap, 0, DEFAULT_STACKSIZE, /*args=*/NULL);

    // stdin/stdout
    struct FileHandle *fh = (struct FileHandle *) AllocDosObject (DOS_FILEHANDLE, NULL);
    emucall1 (EMU_CALL_DOS_INPUT, (ULONG) fh);
    rootProc->pr_CIS = MKBADDR (fh);

    fh = (struct FileHandle *) AllocDosObject (DOS_FILEHANDLE, NULL);
    emucall1 (EMU_CALL_DOS_OUTPUT, (ULONG) fh);
    rootProc->pr_COS = MKBADDR (fh);

    // cli
    rootProc->pr_TaskNum = 1; // FIXME

    BPTR oldpath = 0;

    struct CommandLineInterface *cli = (struct CommandLineInterface *) AllocDosObject (DOS_CLI, (struct TagItem *)NULL);
    cli->cli_DefaultStack = (rootProc->pr_StackSize + 3) / 4;

    // FIXME: set cli_CommandDir
    char *binfn = AllocVec (1024, MEMF_CLEAR);
    emucall1 (EMU_CALL_LOADFILE, (ULONG) binfn);
    cli->cli_CommandName = MKBADDR(binfn);

    rootProc->pr_CLI = MKBADDR(cli);

    // launch it

    DPRINTF (LOG_INFO, "coldstart: about to launch _bootstrap process, pc=0x%08lx\n",
             *((ULONG *)rootProc->pr_Task.tc_SPReg));

    SysBase->ThisTask = &rootProc->pr_Task;
    SysBase->ThisTask->tc_State = TS_RUN;

    asm( "    move.l    %0, a5            \n"

         // restore usp
         "    lea.l     2+16*4(a5),a2     \n"
         "    move.l    a2, usp           \n"

         // prepare stack for for rte
         "    move.l    (a5)+, -(a7)      \n" // pc
         "    move.w    (a5)+, -(a7)      \n" // sr

         // restore registers
         "    movem.l   (a5), d0-d7/a0-a6 \n"

         // return from supervisor mode, jump into our task
         "    rte                         \n"
        : /* no outputs */
        : "r" (SysBase->ThisTask->tc_SPReg)
        : "cc", "a5", "a2"                   // doesn't really matter since will never exit from these instructions anyway
        );

    DPRINTF (LOG_ERROR, "coldstart: this shouldn't happen\n");

    emu_stop(255);
}

