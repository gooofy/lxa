
#include <inttypes.h>

#include <hardware/custom.h>

#include <exec/types.h>
#include <exec/memory.h>
#include <exec/libraries.h>
#include <exec/execbase.h>
#include <exec/resident.h>
#include <exec/initializers.h>
#include <exec/errors.h>
#include <exec/semaphores.h>

#include <clib/exec_protos.h>
#include <inline/exec.h>

#include <dos/dosextens.h>
#include <dos/dostags.h>
#include <clib/dos_protos.h>
#include <inline/dos.h>

#include <libraries/mathffp.h>

#include <graphics/gfxbase.h>

#include <intuition/intuitionbase.h>

#include <libraries/expansionbase.h>

#include <utility/utility.h>

//#define ENABLE_DEBUG

#include "util.h"
#include "exceptions.h"

#define DEFAULT_SCHED_QUANTUM 4

#define DEFAULT_STACKSIZE 1<<16

#define EXEC_FUNCTABLE_ENTRY(___off) (NUM_EXEC_FUNCS+(___off/6))

#define VERSION  1
#define REVISION 1

typedef struct Library * (*libInitFn_t)   ( register struct Library    *lib     __asm("a6"),
                                            register BPTR               seglist __asm("a0"),
                                            register struct ExecBase   *sysb    __asm("d0"));

typedef struct Library * (*libOpenFn_t)   ( register struct Library    *lib     __asm("a6"));
typedef struct Library * (*libCloseFn_t)  ( register struct Library    *lib     __asm("a6"));

typedef void             (*devOpenFn_t)   ( register struct Library    *dev     __asm("a6"),
                                            register struct IORequest  *ioreq   __asm("a1"),
                                            register ULONG              unitn   __asm("d0"),
                                            register ULONG              flags   __asm("d1"));
typedef void             (*devCloseFn_t)  ( register struct Library    *dev     __asm("a6"),
                                            register struct IORequest  *ioreq   __asm("a1"));
typedef void             (*devBeginIOFn_t) (register struct Library    *dev     __asm("a6"),
                                            register struct IORequest  *ioreq   __asm("a1"));

struct JumpVec
{
    unsigned short jmp;
    void *vec;
};

#define NUM_EXEC_FUNCS               130
#define NUM_UTILITY_FUNCS            (45+4)
#define NUM_DOS_FUNCS                (162+4)
#define NUM_MATHFFP_FUNCS            (12+4)
#define NUM_MATHTRANS_FUNCS          (17+4)
#define NUM_GRAPHICS_FUNCS           (172+4)
#define NUM_INTUITION_FUNCS          (134+4)
#define NUM_EXPANSION_FUNCS          (21+4)
#define NUM_DEVICE_INPUT_FUNCS       (0+6)
#define NUM_DEVICE_CONSOLE_FUNCS     (0+6)

#define RAM_START                    0x00010000
#define RAM_END                      0x009fffff

extern struct Resident *__lxa_dos_ROMTag;
extern struct Resident *__lxa_utility_ROMTag;
extern struct Resident *__lxa_mathffp_ROMTag;
extern struct Resident *__lxa_mathtrans_ROMTag;
extern struct Resident *__lxa_graphics_ROMTag;
extern struct Resident *__lxa_intuition_ROMTag;
extern struct Resident *__lxa_expansion_ROMTag;
extern struct Resident *__lxa_input_ROMTag;
extern struct Resident *__lxa_console_ROMTag;

static struct JumpVec   g_ExecJumpTable[NUM_EXEC_FUNCS];
static struct ExecBase  g_SysBase;
static struct MemHeader g_MemHeader;

struct ExecBase        *SysBase;
struct UtilityBase     *UtilityBase;
struct DosLibrary      *DOSBase;
struct Library         *MathBase;
struct Library         *MathTransBase;
struct GfxBase         *GfxBase;
struct IntuitionBase   *IntuitionBase;
struct ExpansionBase   *ExpansionBase;
struct Library         *DeviceInputBase;
struct Library         *DeviceConsoleBase;

static struct Custom   *custom            = (struct Custom*)        0xdff000;

#define JMPINSTR 0x4ef9

void _exec_unimplemented_call ( register struct ExecBase  *exb __asm("a6") )
{
    assert (FALSE);
}

void _exec_InitCode ( register struct ExecBase * SysBase __asm("a6"),
                      register ULONG ___startClass  __asm("d0"),
                      register ULONG ___version  __asm("d1"))
{
    DPRINTF (LOG_DEBUG, "_exec: InitCode unimplemented STUB called.\n");
    assert(FALSE);
}

void _exec_InitStruct ( register struct ExecBase * SysBase __asm("a6"),
                        register const APTR        ___initTable  __asm("a1"),
                        register APTR              ___memory  __asm("a2"),
                        register ULONG             ___size  __asm("d0"))
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
                dst = (UBYTE *)___memory + off;

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

    //VOID  __stdargs MakeFunctions( APTR target, CONST_APTR functionArray, ULONG funcDispBase );

    if(*(WORD *)___funcInit==-1)
        MakeFunctions (library, (CONST_APTR)___funcInit+1, (ULONG)___funcInit);
    else
        MakeFunctions (library, ___funcInit, NULL);

    library->lib_NegSize = negsize;
    library->lib_PosSize = ___dataSize;

    if (___structInit)
    {
        DPRINTF (LOG_DEBUG, "_exec: _makeLibrary InitStruct()...\n");
        InitStruct (___structInit, library, 0);
    }
    if (___libInitFn)
    {
        DPRINTF (LOG_DEBUG, "_exec: calling ___libInitFn at 0x%08lx\n", ___libInitFn);

        library = ___libInitFn(library, ___segList, SysBase);
        DPRINTF (LOG_DEBUG, "_exec: ___libInitFn returned library=0x%08lx\n", library);
    }
    DPRINTF (LOG_DEBUG, "_exec: _makeLibrary done.\n");
}

struct Library * _exec_MakeLibrary ( register struct ExecBase * SysBase __asm("a6"),
                                     register const APTR ___funcInit  __asm("a0"),
                                     register const APTR ___structInit  __asm("a1"),
                                     register libInitFn_t ___libInitFn  __asm("a2"),
                                     register ULONG ___dataSize  __asm("d0"),
                                     register BPTR ___segList  __asm("d1"))
{
    DPRINTF (LOG_DEBUG, "_exec: MakeLibrary called, ___funcInit=0x%08x, ___structInit=0x%08x, ___dataSize=%d\n", ___funcInit, ___structInit, ___dataSize);

    struct Library *library=NULL;
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



void _exec_MakeFunctions ( register struct ExecBase * SysBase __asm("a6"),
                           register APTR ___target  __asm("a0"),
                           register const APTR ___functionArray  __asm("a1"),
                           register const APTR ___funcDispBase  __asm("a2"))
{
    DPRINTF (LOG_DEBUG, "_exec: MakeFunctions called target=0x%08lx, functionArray=0x%08x, funcDispBase=0x%08x\n", ___target, ___functionArray, ___funcDispBase);

    ULONG n = 1;
    //APTR lastvec;

    if (___funcDispBase)
    {
        WORD *fp = (WORD *)___functionArray;

        while (*fp != -1)
        {
            struct JumpVec  *jv = &(((struct JumpVec *)(___target))[-n]);
            jv->jmp = JMPINSTR;
            jv->vec = (APTR)((ULONG)___funcDispBase + *fp);
            //DPRINTF (LOG_DEBUG, "                  I  n=%4d jv=0x%08lx jv->jmp=0x%04x jv->vec=0x%08lx\n", n, jv, jv->jmp, jv->vec);
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
            //DPRINTF (LOG_DEBUG, "                  II n=%4d jv=0x%08lx jv->jmp=0x%04x jv->vec=0x%08lx\n", n, jv, jv->jmp, jv->vec);
            fp++; n++;
        }
    }
    DPRINTF (LOG_DEBUG, "_exec: MakeFunctions done, %d entries set.\n", n);
}

struct Resident * _exec_FindResident ( register struct ExecBase * SysBase __asm("a6"),
                                                        register CONST_STRPTR ___name  __asm("a1"))
{
    DPRINTF (LOG_DEBUG, "_exec: FindResident unimplemented STUB called.\n");
    assert(FALSE);
    return NULL;
}

APTR _exec_InitResident ( register struct ExecBase * SysBase __asm("a6"),
                                                        register const struct Resident * ___resident  __asm("a1"),
                                                        register ULONG ___segList  __asm("d1"))
{
    DPRINTF (LOG_DEBUG, "_exec: InitResident unimplemented STUB called.\n");
    assert(FALSE);
    return NULL;
}

void _exec_Alert ( register struct ExecBase *SysBase   __asm("a6"),
                            register ULONG            alertNum  __asm("d7"))
{
    LPRINTF (LOG_ERROR, "_exec: Alert called, alertNum=0x%08lx\n", alertNum);
    assert(FALSE);
}

void _exec_Debug ( register struct ExecBase * SysBase __asm("a6"),
                                                        register ULONG ___flags  __asm("d0"))
{
    DPRINTF (LOG_DEBUG, "_exec: Debug unimplemented STUB called.\n");
    assert(FALSE);
}

void _exec_Disable ( register struct ExecBase * SysBase __asm("a6"))
{
    DPRINTF (LOG_DEBUG, "_exec: Disable() called.\n");

    custom->intena = 0x4000;

    SysBase->IDNestCnt++;
}

void _exec_Enable ( register struct ExecBase * SysBase __asm("a6"))
{
    DPRINTF (LOG_DEBUG, "_exec: Enable() called.\n");
    SysBase->IDNestCnt--;
    if (SysBase->IDNestCnt<0)
        custom->intena = 0xc000; // 1100 0000 0000 0000
}

void _exec_Forbid ( register struct ExecBase * SysBase __asm("a6"))
{
    DPRINTF (LOG_DEBUG, "_exec: Forbid(): called.\n");
    SysBase->TDNestCnt++;
    DPRINTF (LOG_DEBUG, "_exec: Forbid() --> SysBase->TDNestCnt=%d\n", SysBase->TDNestCnt);
}

void _exec_Permit ( register struct ExecBase * SysBase __asm("a6"))
{
    DPRINTF (LOG_DEBUG, "_exec: Permit() called.\n");
    SysBase->TDNestCnt--;
    DPRINTF (LOG_DEBUG, "_exec: Permit() --> SysBase->TDNestCnt=%d\n", SysBase->TDNestCnt);
}

APTR _exec_SuperState ( register struct ExecBase * SysBase __asm("a6"))
{
    DPRINTF (LOG_DEBUG, "_exec: SuperState unimplemented STUB called.\n");
    assert(FALSE);
    return NULL;
}

void _exec_UserState ( register struct ExecBase * SysBase __asm("a6"),
                                                        register APTR ___sysStack  __asm("d0"))
{
    DPRINTF (LOG_DEBUG, "_exec: UserState unimplemented STUB called.\n");
    assert(FALSE);
}

struct Interrupt * _exec_SetIntVector ( register struct ExecBase * SysBase __asm("a6"),
                                                        register LONG ___intNumber  __asm("d0"),
                                                        register const struct Interrupt * ___interrupt  __asm("a1"))
{
    DPRINTF (LOG_DEBUG, "_exec: SetIntVector unimplemented STUB called.\n");
    assert(FALSE);
    return NULL;
}

void _exec_AddIntServer ( register struct ExecBase * SysBase __asm("a6"),
                                                        register LONG ___intNumber  __asm("d0"),
                                                        register struct Interrupt * ___interrupt  __asm("a1"))
{
    DPRINTF (LOG_DEBUG, "_exec: AddIntServer unimplemented STUB called.\n");
    assert(FALSE);
}

void _exec_RemIntServer ( register struct ExecBase * SysBase __asm("a6"),
                                                        register LONG ___intNumber  __asm("d0"),
                                                        register struct Interrupt * ___interrupt  __asm("a1"))
{
    DPRINTF (LOG_DEBUG, "_exec: RemIntServer unimplemented STUB called.\n");
    assert(FALSE);
}

void _exec_Cause ( register struct ExecBase * SysBase __asm("a6"),
                                                        register struct Interrupt * ___interrupt  __asm("a1"))
{
    DPRINTF (LOG_DEBUG, "_exec: Cause unimplemented STUB called.\n");
    assert(FALSE);
}

#ifdef ENABLE_DEBUG

static void _exec_dump_memh (struct MemHeader * freeList)
{
    struct MemChunk *p1 = (struct MemChunk *)&freeList->mh_First;
    struct MemChunk *p2 = p1->mc_Next;

    while (p2)
    {
        DPRINTF (LOG_DEBUG, "       MEMH chunk at 0x%08lx-0x%08lx is %8d bytes, next=0x%08lx\n",
                (ULONG)p2, (ULONG)p2+p2->mc_Bytes, p2->mc_Bytes, (ULONG)p2->mc_Next);
        p1 = p2;
        assert (p1 != p1->mc_Next);
        p2 = p1->mc_Next;
    }
}

#else

static inline void _exec_dump_memh (struct MemHeader * freeList)
{
}

#endif

APTR _exec_Allocate ( register struct ExecBase * SysBase __asm("a6"),
                      register struct MemHeader * freeList  __asm("a0"),
                      register ULONG ___byteSize  __asm("d0"))
{
    DPRINTF (LOG_DEBUG, "_exec: Allocate called, freeList=0x%08lx, ___byteSize=%d\n", (ULONG)freeList, ___byteSize);
    DPRINTF (LOG_DEBUG, "       mh_First=0x%08lx, mh_Lower=0x%08lx, mh_Upper=0x%08lx, mh_Free=%ld, mh_Attributes=0x%08lx\n", 
             freeList->mh_First, freeList->mh_Lower, freeList->mh_Upper, freeList->mh_Free, freeList->mh_Attributes);

    ULONG byteSize = ALIGN (___byteSize, 4);

    DPRINTF (LOG_DEBUG, "       rounded up byte size is %d\n", byteSize);

    if (!byteSize)
        return NULL;

    DPRINTF (LOG_DEBUG, "       freeList->mh_Free=%ld\n", freeList->mh_Free);
    if (freeList->mh_Free < byteSize)
        return NULL;

    _exec_dump_memh (freeList);

    struct MemChunk *mc_prev = (struct MemChunk *)&freeList->mh_First;
    struct MemChunk *mc_cur  = mc_prev->mc_Next;

    while (mc_cur)
    {
        DPRINTF (LOG_DEBUG, "       looking for mem chunk that is large enough, current chunk at 0x%08lx is %d bytes\n",
                 (ULONG)mc_cur, mc_cur->mc_Bytes);
        if (mc_cur->mc_Bytes >= byteSize)
        {
            break;
        }
        mc_prev = mc_cur;
        mc_cur  = mc_cur->mc_Next;
    }

    if (mc_cur)
    {
        DPRINTF (LOG_DEBUG, "       found a chunk at 0x%08lx, mc_Bytes=%ld, prev=0x%08lx\n",
                 (ULONG)mc_cur, (ULONG)mc_cur->mc_Bytes, (ULONG)mc_prev);

        if (mc_cur->mc_Bytes == byteSize)
        {
            DPRINTF (LOG_DEBUG, "       *** EXACT MATCH ***\n");
            mc_prev->mc_Next = mc_cur->mc_Next;
        }
        else
        {
            struct MemChunk *mc_new = (struct MemChunk *)((UBYTE *)mc_cur+byteSize);
            mc_new->mc_Next  = mc_cur->mc_Next;
            mc_new->mc_Bytes = mc_cur->mc_Bytes-byteSize;

            mc_prev->mc_Next = mc_new;

            DPRINTF (LOG_DEBUG, "       created a new chunk at 0x%08lx size=%ld\n", (ULONG)mc_new, mc_new->mc_Bytes);
        }

        freeList->mh_Free -= byteSize;
    }
    else
    {
        DPRINTF (LOG_DEBUG, "       no chunk found.\n");
    }

    _exec_dump_memh (freeList);

    return mc_cur;
}

void _exec_Deallocate ( register struct ExecBase  *SysBase     __asm("a6"),
                        register struct MemHeader *freeList    __asm("a0"),
                        register APTR              memoryBlock __asm("a1"),
                        register ULONG             byteSize    __asm("d0"))
{
    DPRINTF (LOG_DEBUG, "_exec: Deallocate called, freeList=0x%08lx, memoryBlock=0x%08lx, byteSize=%ld\n", freeList, memoryBlock, byteSize);

    if(!byteSize || !memoryBlock)
        return;

    _exec_dump_memh (freeList);

    // alignment

    byteSize = ALIGN (byteSize, 4);

    struct MemChunk *pNext     = freeList->mh_First;
    struct MemChunk *pPrev     = (struct MemChunk *)&freeList->mh_First;
    struct MemChunk *pCurStart = (struct MemChunk *)memoryBlock;
    struct MemChunk *pCurEnd   = (struct MemChunk *) ((UBYTE *)pCurStart + byteSize);

    if (!pNext) // empty list?
    {
        DPRINTF (LOG_DEBUG, "       empty list\n");
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

    DPRINTF (LOG_DEBUG, "       will insert, pPrev=0x%08lx, pNext=0x%08lx, pCurStart=0x%08lx, pCurEnd=0x%08lx\n",
                        pPrev, pNext, pCurStart, pCurEnd);

    // if we found a prev block, see if we can merge
    if (pPrev != (struct MemChunk *)&freeList->mh_First)
    {
        if ((UBYTE *)pPrev + pPrev->mc_Bytes == (UBYTE *)pCurStart)
        {
            DPRINTF (LOG_DEBUG, "       merge with prev\n");
            pCurStart = pPrev;
        }
        else
        {
            pPrev->mc_Next = pCurStart;
        }
    }
    else
    {
        pPrev->mc_Next = pCurStart;
    }

    // if we have a next block, try to merge with it as well
    if (pNext && (pCurEnd == pNext))
    {
        DPRINTF (LOG_DEBUG, "       merge with next\n");
        pCurEnd = (struct MemChunk *) (((UBYTE*)pCurEnd) + pNext->mc_Bytes);
        pNext = pNext->mc_Next;
    }

    pCurStart->mc_Next   = pNext;
    pCurStart->mc_Bytes  = (UBYTE *)pCurEnd - (UBYTE *)pCurStart;
    freeList->mh_Free  += byteSize;

    _exec_dump_memh (freeList);
}

APTR _exec_AllocMem ( register struct ExecBase *SysBase __asm("a6"),
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
    {
        DPRINTF (LOG_DEBUG, "_exec: AllocMem -> memset %d bytes at 0x%08lx\n", ___byteSize, (ULONG)mem);
        memset(mem, 0, ___byteSize);
    }

    DPRINTF (LOG_DEBUG, "_exec: AllocMem returning with mem=0x%08lx\n", (ULONG)mem);

    return mem;
}

APTR _exec_AllocAbs ( register struct ExecBase * SysBase __asm("a6"),
                                                        register ULONG ___byteSize  __asm("d0"),
                                                        register APTR ___location  __asm("a1"))
{
    DPRINTF (LOG_DEBUG, "_exec: AllocAbs unimplemented STUB called.\n");
    assert(FALSE);
    return NULL;
}

void _exec_FreeMem ( register struct ExecBase * SysBase __asm("a6"),
                     register APTR  memoryBlock  __asm("a1"),
                     register ULONG byteSize     __asm("d0"))
{
    DPRINTF (LOG_DEBUG, "_exec: FreeMem called, memoryBlock=0x%08lx, byteSize=%ld\n", memoryBlock, byteSize);

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

ULONG _exec_AvailMem ( register struct ExecBase * SysBase __asm("a6"),
                                       register ULONG ___requirements  __asm("d1"))
{
    DPRINTF (LOG_DEBUG, "_exec: AvailMem unimplemented STUB called.\n");
    assert(FALSE);
    return 0;
}

struct MemList * _exec_AllocEntry ( register struct ExecBase * SysBase __asm("a6"),
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

void _exec_FreeEntry ( register struct ExecBase * SysBase __asm("a6"),
                       register struct MemList * entry  __asm("a0"))
{
    DPRINTF (LOG_DEBUG, "_exec: FreeEntry called, entry=0x%08lx\n", entry);

    for (ULONG i=0; i<entry->ml_NumEntries; i++)
        FreeMem (entry->ml_ME[i].me_Addr, entry->ml_ME[i].me_Length);

    FreeMem (entry, sizeof(struct MemList) - sizeof(struct MemEntry) + (sizeof(struct MemEntry)*entry->ml_NumEntries));
}

void _exec_Insert ( register struct ExecBase * SysBase __asm("a6"),
                             register struct List *list  __asm("a0"),
                             register struct Node *node  __asm("a1"),
                             register struct Node *pred  __asm("a2"))
{
    DPRINTF (LOG_DEBUG, "_exec: Insert unimplemented STUB called.\n");

    if (pred)
    {
		// last one?
        if (pred->ln_Succ)
        {
            node->ln_Succ          = pred->ln_Succ;
            node->ln_Pred          = pred;
            pred->ln_Succ->ln_Pred = node;
            pred->ln_Succ          = node;
        }
        else // pred is last node in this list
        {
            node->ln_Succ              = (struct Node *) &list->lh_Tail;
            node->ln_Pred              = list->lh_TailPred;
            list->lh_TailPred->ln_Succ = node;
            list->lh_TailPred          = node;
        }
    }
    else
    {
		// same as AddHead
        node->ln_Succ          = list->lh_Head;
        node->ln_Pred          = (struct Node *) &list->lh_Head;
        list->lh_Head->ln_Pred = node;
        list->lh_Head          = node;
    }
}

void _exec_AddHead ( register struct ExecBase * SysBase __asm("a6"),
                                     register struct List * list  __asm("a0"),
                                     register struct Node * node  __asm("a1"))
{
    DPRINTF (LOG_DEBUG, "_exec: AddHead called, list=0x%08lx, node=0x%08lx\n", list, node);

    node->ln_Succ = list->lh_Head;
    node->ln_Pred = (struct Node *) &list->lh_Head;

    list->lh_Head->ln_Pred = node;
    list->lh_Head          = node;
}

void _exec_AddTail ( register struct ExecBase *SysBase __asm("a6"),
                     register struct List     *list    __asm("a0"),
                     register struct Node     *node    __asm("a1"))
{
    DPRINTF (LOG_DEBUG, "_exec: AddTail called list=0x%08lx node=0x%08lx\n", list, node);

    node->ln_Succ              = (struct Node *)&list->lh_Tail;
    node->ln_Pred              = list->lh_TailPred;

    list->lh_TailPred->ln_Succ = node;
    list->lh_TailPred          = node;

    // DPRINTF (LOG_DEBUG, "_exec: AddTail done\n");
}

void _exec_Remove ( register struct ExecBase * SysBase __asm("a6"),
                                    register struct Node * node  __asm("a1"))
{
    DPRINTF (LOG_DEBUG, "_exec: Remove called, node=0x%08lx\n", node);

    assert (node);

    node->ln_Pred->ln_Succ = node->ln_Succ;
    node->ln_Succ->ln_Pred = node->ln_Pred;
}

struct Node * _exec_RemHead ( register struct ExecBase *SysBase __asm("a6"),
                                              register struct List     *list  __asm("a0"))
{
    DPRINTF (LOG_DEBUG, "_exec: RemHead called, list=0x%08lx\n", list);

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

struct Node * _exec_RemTail ( register struct ExecBase * SysBase __asm("a6"),
                                                        register struct List * ___list  __asm("a0"))
{
    DPRINTF (LOG_DEBUG, "_exec: RemTail unimplemented STUB called.\n");
    assert(FALSE);
    return NULL;
}

void _exec_Enqueue ( register struct ExecBase * SysBase __asm("a6"),
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

struct Node * _exec_FindName ( register struct ExecBase * SysBase __asm("a6"),
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
            if (!strcmp (node->ln_Name, (char *) name))
                return node;
        }
    }

    return NULL;
}

APTR _exec_AddTask ( register struct ExecBase *SysBase __asm("a6"),
                                     register struct Task     *task    __asm("a1"),
                                     register const APTR       initPC  __asm("a2"),
                                     register const APTR       finalPC __asm("a3"))
{
    DPRINTF (LOG_DEBUG, "_exec: AddTask called task=0x%08lx, initPC=0x%08lx, finalPC=0x%08lx\n", task, initPC, finalPC);

    assert(task);

    U_prepareTask (task, initPC, finalPC, /*args=*/NULL);

    // launch it

    Disable();

    task->tc_State = TS_READY;
    Enqueue (&SysBase->TaskReady, &task->tc_Node);

    Enable();

    return task;
}

void _defaultTaskExit (void)
{
    DPUTS (LOG_DEBUG, "_exec: _defaultTaskExit() called\n");

    asm(
        "    move.l  4, a6      \n" // SysBase -> a6
        "    suba.l  a1, a1     \n" // #0 -> a1
        "    jsr    -288(a6)    \n" // RemTask(0)
        : /* no outputs */
        : /* no inputs */
        : "cc", "d0", "d1", "a0", "a1", "a6"
    );
}

void _exec_RemTask ( register struct ExecBase * SysBase __asm("a6"),
                                     register struct Task * task  __asm("a1"))
{
    DPRINTF (LOG_DEBUG, "_exec: RemTask called, task=0x%08lx\n", task);

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
        DPUTS (LOG_DEBUG, "_exec: RemTask calling Dispatch() via Supervisor()\n");
        asm(
            "   move.l  a5, -(a7)               \n"     // save a5
            "   move.l  4, a6                   \n"     // SysBase -> a6
            "   move.l  #_exec_Dispatch, a5     \n"     // #exec_Dispatch -> a5
            "   jsr     -30(a6)                 \n"     // Supervisor()
            "   move.l  (a7)+, a5               \n"     // restore a5
            : /* no outputs */
            : /* no inputs */
            : "cc", "d0", "d1", "a0", "a1", "a6"
        );
    }
}

struct Task * _exec_FindTask ( register struct ExecBase *SysBase __asm("a6"),
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
            const char *s2 = (char *) name;
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

BYTE _exec_SetTaskPri ( register struct ExecBase * SysBase __asm("a6"),
                                                        register struct Task * ___task  __asm("a1"),
                                                        register LONG ___priority  __asm("d0"))
{
    LPRINTF (LOG_ERROR, "_exec: SetTaskPri unimplemented STUB called.\n");
    assert(FALSE);
    return 0;
}

ULONG _exec_SetSignal ( register struct ExecBase *SysBase     __asm("a6"),
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

ULONG _exec_SetExcept ( register struct ExecBase * SysBase    __asm("a6"),
                                        register ULONG             newSignals __asm("d0"),
                                        register ULONG             signalSet  __asm("d1"))
{
    DPRINTF (LOG_DEBUG, "_exec: SetExcept called, newSignals=0x%08lx, signalSet=0x%08lx\n", newSignals, signalSet);

    struct Task *me = SysBase->ThisTask;

    Disable();

    ULONG oldsigs = me->tc_SigExcept;

    /* Update tc_SigExcept: clear bits in signalSet, then set bits from newSignals */
    me->tc_SigExcept = (oldsigs & ~signalSet) | (newSignals & signalSet);

    DPRINTF (LOG_DEBUG, "_exec: SetExcept: old=0x%08lx, new=0x%08lx\n", oldsigs, me->tc_SigExcept);

    /* Check if any of the newly enabled exception signals are already received */
    if (me->tc_SigExcept & me->tc_SigRecvd)
    {
        DPRINTF (LOG_DEBUG, "_exec: SetExcept: exception signals pending, setting TF_EXCEPT\n");
        me->tc_Flags |= TF_EXCEPT;
        /* Note: Exception processing will occur when the scheduler runs.
         * We don't call Reschedule() here as it's not implemented and
         * the exception will be processed on the next context switch. */
    }

    Enable();

    return oldsigs;
}

ULONG _exec_Wait ( register struct ExecBase * SysBase __asm("a6"),
                                                        register ULONG ___signalSet  __asm("d0"))
{
    DPRINTF (LOG_DEBUG, "_exec: Wait() called, signalSet=0x%08lx\n", ___signalSet);

    struct Task *thisTask = SysBase->ThisTask;
    ULONG rcvd;

    Disable();

    /* If at least one of the signals is already set do not wait. */
    while (!(thisTask->tc_SigRecvd & ___signalSet))
    {
        /* Set the wait signal mask */
        thisTask->tc_SigWait = ___signalSet;

        DPRINTF (LOG_DEBUG, "_exec: Wait() moving task '%s' @ 0x%08lx to TaskWait\n",
                 thisTask->tc_Node.ln_Name, thisTask);

        /*
         * Clear TDNestCnt (because Switch() will not care about it),
         * but memorize it first. IDNestCnt is handled by Switch().
         */
        thisTask->tc_TDNestCnt = SysBase->TDNestCnt;
        SysBase->TDNestCnt = -1;

        thisTask->tc_State = TS_WAIT;

        /* Move current task to the waiting list. */
        Enqueue(&SysBase->TaskWait, &thisTask->tc_Node);

        /* And switch to the next ready task via Supervisor call */
        asm volatile (
            "   move.l  a5, -(a7)               \n"     // save a5
            "   move.l  4, a6                   \n"     // SysBase -> a6
            "   move.l  #_exec_Switch, a5       \n"     // #exec_Switch -> a5
            "   jsr     -30(a6)                 \n"     // Supervisor()
            "   move.l  (a7)+, a5               \n"     // restore a5
            : /* no outputs */
            : /* no inputs */
            : "cc", "d0", "d1", "a0", "a1", "a6", "memory"
        );

        /*
         * OK. Somebody awakened us. This means that either the
         * signals are there or it's just a finished task exception.
         * Test again to be sure (see above).
         */
        DPRINTF (LOG_DEBUG, "_exec: Wait() task '%s' awoken\n", thisTask->tc_Node.ln_Name);

        /* Restore TDNestCnt. */
        SysBase->TDNestCnt = thisTask->tc_TDNestCnt;
    }

    /* Get active signals. */
    rcvd = (thisTask->tc_SigRecvd & ___signalSet);

    /* And clear them. */
    thisTask->tc_SigRecvd &= ~___signalSet;

    Enable();

    DPRINTF (LOG_DEBUG, "_exec: Wait() returning rcvd=0x%08lx\n", rcvd);

    /* All done. */
    return rcvd;
}

void _exec_Signal ( register struct ExecBase * SysBase __asm("a6"),
                                                        register struct Task * ___task  __asm("a1"),
                                                        register ULONG ___signalSet  __asm("d0"))
{
    DPRINTF (LOG_DEBUG, "_exec: Signal() called, task=0x%08lx '%s', signalSet=0x%08lx\n",
             ___task, ___task->tc_Node.ln_Name, ___signalSet);

    struct Task *thisTask = SysBase->ThisTask;

    Disable();

    /* Set the signals in the task structure. */
    ___task->tc_SigRecvd |= ___signalSet;

    DPRINTF (LOG_DEBUG, "_exec: Signal() target task state=%d, SigRecvd=0x%08lx\n",
             ___task->tc_State, ___task->tc_SigRecvd);

    /* Do those bits raise exceptions? */
    if (___task->tc_SigRecvd & ___task->tc_SigExcept)
    {
        /* Yes. Set the exception flag. */
        ___task->tc_Flags |= TF_EXCEPT;
        DPRINTF (LOG_DEBUG, "_exec: Signal() TF_EXCEPT set\n");

        /*
         * If the target task is running (called from within interrupt handler),
         * we need to schedule a reschedule.
         */
        if (___task->tc_State == TS_RUN)
        {
            /* Order a reschedule - set SysFlags to trigger scheduler */
            SysBase->SysFlags |= (1 << 6); /* SFF_QuantumOver */
            Enable();
            return;
        }
    }

    /* Does the target task have signals to process? */
    if (___task->tc_SigRecvd & (___task->tc_SigWait | ___task->tc_SigExcept))
    {
        if (___task->tc_State == TS_WAIT)
        {
            DPRINTF (LOG_DEBUG, "_exec: Signal() waking up WAIT task '%s'\n", ___task->tc_Node.ln_Name);

            /* Yes. Move it to the ready list. */
            Remove(&___task->tc_Node);
            ___task->tc_State = TS_READY;
            Enqueue(&SysBase->TaskReady, &___task->tc_Node);
        }

        if (___task->tc_State == TS_READY)
        {
            /* Has it a higher priority than the running task? */
            if (___task->tc_Node.ln_Pri > thisTask->tc_Node.ln_Pri)
            {
                DPRINTF (LOG_DEBUG, "_exec: Signal() task has higher priority, rescheduling\n");

                /*
                 * Yes. A taskswitch is necessary. Prepare one if possible.
                 * (If the current task is not running it is already moved)
                 */
                if (thisTask->tc_State == TS_RUN)
                {
                    /* Set SysFlags to trigger scheduler on next opportunity */
                    SysBase->SysFlags |= (1 << 6); /* SFF_QuantumOver */
                }
            }
        }
    }

    Enable();

    DPRINTF (LOG_DEBUG, "_exec: Signal() done\n");
}

BYTE _exec_AllocSignal ( register struct ExecBase * SysBase    __asm("a6"),
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

void _exec_FreeSignal ( register struct ExecBase *SysBase   __asm("a6"),
                                        register BYTE             signalNum __asm("d0"))
{
    DPRINTF (LOG_DEBUG, "_exec: FreeSignal() called.\n");

    if (signalNum != -1)
    {
        struct Task *me = SysBase->ThisTask;
        me->tc_SigAlloc &= ~(1 << signalNum);
    }
}

LONG _exec_AllocTrap ( register struct ExecBase * SysBase __asm("a6"),
                                                        register LONG ___trapNum  __asm("d0"))
{
    LPRINTF (LOG_ERROR, "_exec: AllocTrap unimplemented STUB called.\n");
    assert(FALSE);
    return 0;
}

void _exec_FreeTrap ( register struct ExecBase * SysBase __asm("a6"),
                                                        register LONG ___trapNum  __asm("d0"))
{
    LPRINTF (LOG_ERROR, "_exec: FreeTrap unimplemented STUB called.\n");
    assert(FALSE);
}

void _exec_AddPort ( register struct ExecBase * SysBase __asm("a6"),
                                                        register struct MsgPort * ___port  __asm("a1"))
{
    DPRINTF (LOG_DEBUG, "_exec: AddPort() called, port=0x%08lx, name=%s\n",
             ___port, ___port->mp_Node.ln_Name ? ___port->mp_Node.ln_Name : "NULL");

    /* Arbitrate for the list of messageports. */
    Forbid();

    /* Yes, this is a messageport */
    ___port->mp_Node.ln_Type = NT_MSGPORT;

    /* Clear the list of messages */
    NEWLIST(&___port->mp_MsgList);

    /* And add the actual port */
    Enqueue(&SysBase->PortList, &___port->mp_Node);

    /* All done */
    Permit();
}

void _exec_RemPort ( register struct ExecBase * SysBase __asm("a6"),
                                                        register struct MsgPort * ___port  __asm("a1"))
{
    DPRINTF (LOG_DEBUG, "_exec: RemPort() called, port=0x%08lx\n", ___port);

    /* Arbitrate for the list of message ports.*/
    Forbid();

    /* Remove the current port. */
    Remove(&___port->mp_Node);

    /* All done. */
    Permit();
}

void _exec_PutMsg ( register struct ExecBase * SysBase __asm("a6"),
                                                        register struct MsgPort * ___port  __asm("a0"),
                                                        register struct Message * ___message  __asm("a1"))
{
    DPRINTF (LOG_DEBUG, "_exec: PutMsg() called, port=0x%08lx, message=0x%08lx\n", ___port, ___message);

    /* Set the node type to NT_MESSAGE == sent message. */
    ___message->mn_Node.ln_Type = NT_MESSAGE;

    /*
     * Add a message to the ports list.
     * NB: Messages may be sent from interrupts, therefore
     * the message list of the message port must be protected
     * with Disable().
     */
    Disable();
    AddTail(&___port->mp_MsgList, &___message->mn_Node);
    Enable();

    if (___port->mp_SigTask)
    {
        /* And trigger the action. */
        switch(___port->mp_Flags & PF_ACTION)
        {
            case PA_SIGNAL:
                DPRINTF (LOG_DEBUG, "_exec: PutMsg() PA_SIGNAL -> Task 0x%08lx, Signal 0x%08lx\n",
                         ___port->mp_SigTask, (1 << ___port->mp_SigBit));
                /* Send the signal */
                Signal((struct Task *)___port->mp_SigTask, (1 << ___port->mp_SigBit));
                break;

            case PA_SOFTINT:
                DPRINTF (LOG_DEBUG, "_exec: PutMsg() PA_SOFTINT\n");
                /* Raise a software interrupt */
                Cause((struct Interrupt *)___port->mp_SoftInt);
                break;

            case PA_IGNORE:
                /* Do nothing. */
                break;
        }
    }

    DPRINTF (LOG_DEBUG, "_exec: PutMsg() done\n");
}

struct Message * _exec_GetMsg ( register struct ExecBase * SysBase __asm("a6"),
                                                        register struct MsgPort * ___port  __asm("a0"))
{
    DPRINTF (LOG_DEBUG, "_exec: GetMsg() called, port=0x%08lx\n", ___port);

    struct Message *msg;

    /*
     * Protect the message list, and get the first node.
     */
    Disable();
    msg = (struct Message *)RemHead(&___port->mp_MsgList);
    Enable();

    DPRINTF (LOG_DEBUG, "_exec: GetMsg() returning msg=0x%08lx\n", msg);

    /* All done. */
    return msg;
}

void _exec_ReplyMsg ( register struct ExecBase * SysBase __asm("a6"),
                                                        register struct Message * ___message  __asm("a1"))
{
    DPRINTF (LOG_DEBUG, "_exec: ReplyMsg() called, message=0x%08lx\n", ___message);

    struct MsgPort *port;

    /* Get replyport */
    port = ___message->mn_ReplyPort;

    /* Not set? Only mark the message as no longer sent. */
    if (port == NULL)
    {
        ___message->mn_Node.ln_Type = NT_FREEMSG;
    }
    else
    {
        /* Mark the message as replied */
        ___message->mn_Node.ln_Type = NT_REPLYMSG;

        /* Send it back using the same logic as PutMsg */
        Disable();
        AddTail(&port->mp_MsgList, &___message->mn_Node);
        Enable();

        if (port->mp_SigTask)
        {
            /* And trigger the action. */
            switch(port->mp_Flags & PF_ACTION)
            {
                case PA_SIGNAL:
                    DPRINTF (LOG_DEBUG, "_exec: ReplyMsg() PA_SIGNAL -> Task 0x%08lx, Signal 0x%08lx\n",
                             port->mp_SigTask, (1 << port->mp_SigBit));
                    Signal((struct Task *)port->mp_SigTask, (1 << port->mp_SigBit));
                    break;

                case PA_SOFTINT:
                    Cause((struct Interrupt *)port->mp_SoftInt);
                    break;

                case PA_IGNORE:
                    break;
            }
        }
    }

    DPRINTF (LOG_DEBUG, "_exec: ReplyMsg() done\n");
}

struct Message * _exec_WaitPort ( register struct ExecBase * SysBase __asm("a6"),
                                                  register struct MsgPort * ___port  __asm("a0"))
{
    DPRINTF (LOG_DEBUG, "_exec: WaitPort() called, port=0x%08lx\n", ___port);

    /*
     * On uniprocessor systems, Disable() is not necessary since emptiness
     * can be checked without it - and nobody is allowed to change the signal
     * bit as soon as the current task entered WaitPort().
     */

    /* Is messageport empty? */
    while (___port->mp_MsgList.lh_TailPred == (struct Node *)&___port->mp_MsgList)
    {
        DPRINTF (LOG_DEBUG, "_exec: WaitPort() msg list empty, waiting for signal...\n");

        /*
         * Yes. Wait for the signal to arrive. Remember that signals may
         * arrive without a message so check again.
         */
        Wait(1 << ___port->mp_SigBit);

        DPRINTF (LOG_DEBUG, "_exec: WaitPort() signal received\n");
    }

    DPRINTF (LOG_DEBUG, "_exec: WaitPort() returning first message 0x%08lx\n",
             ___port->mp_MsgList.lh_Head);

    /* Return the first node in the list. */
    return (struct Message *)___port->mp_MsgList.lh_Head;
}

struct MsgPort * _exec_FindPort ( register struct ExecBase * SysBase __asm("a6"),
                                                        register CONST_STRPTR ___name  __asm("a1"))
{
    DPRINTF (LOG_DEBUG, "_exec: FindPort() called, name=%s\n", ___name ? (char *)___name : "NULL");

    /* Nothing spectacular - just look for that name. */
    struct MsgPort *retVal = (struct MsgPort *)FindName(&SysBase->PortList, ___name);

    DPRINTF (LOG_DEBUG, "_exec: FindPort() returning 0x%08lx\n", retVal);

    return retVal;
}

void _exec_AddLibrary ( register struct ExecBase * SysBase __asm("a6"),
                                                        register struct Library * ___library  __asm("a1"))
{
    LPRINTF (LOG_ERROR, "_exec: AddLibrary unimplemented STUB called.\n");
    assert(FALSE);
}

void _exec_RemLibrary ( register struct ExecBase * SysBase __asm("a6"),
                                                        register struct Library * ___library  __asm("a1"))
{
    LPRINTF (LOG_ERROR, "_exec: RemLibrary unimplemented STUB called.\n");
    assert(FALSE);
}

struct Library * _exec_OldOpenLibrary ( register struct ExecBase *SysBase __asm("a6"),
                                                 register CONST_STRPTR     libName __asm("a1"))
{
    DPRINTF (LOG_DEBUG, "_exec: OldOpenLibrary called, liName=%s\n", libName);
    return OpenLibrary (libName, 0);
}

void _exec_CloseLibrary ( register struct ExecBase *SysBase  __asm("a6"),
                                          register struct Library  *library  __asm("a1"))
{
    DPRINTF (LOG_DEBUG, "_exec: CloseLibrary() called, library = 0x%08lx\n", library);

    //BPTR seglist;

    if (library)
    {
        Forbid();

        struct JumpVec *jv = &(((struct JumpVec *)(library))[-2]);
        libCloseFn_t closefn = jv->vec;

        closefn(library);

        Permit();
    }
}

APTR _exec_SetFunction ( register struct ExecBase * SysBase __asm("a6"),
                                                        register struct Library * ___library  __asm("a1"),
                                                        register LONG ___funcOffset  __asm("a0"),
                                                        register VOID (*___newFunction)()  __asm("d0"))
{
    LPRINTF (LOG_ERROR, "_exec: SetFunction unimplemented STUB called.\n");
    assert(FALSE);
    return NULL;
}

void _exec_SumLibrary ( register struct ExecBase * SysBase __asm("a6"),
                                                        register struct Library * ___library  __asm("a1"))
{
    LPRINTF (LOG_ERROR, "_exec: SumLibrary unimplemented STUB called.\n");
    assert(FALSE);
}

void _exec_AddDevice ( register struct ExecBase * SysBase __asm("a6"),
                                                        register struct Device * ___device  __asm("a1"))
{
    LPRINTF (LOG_ERROR, "_exec: AddDevice unimplemented STUB called.\n");
    assert(FALSE);
}

void _exec_RemDevice ( register struct ExecBase * SysBase __asm("a6"),
                                                        register struct Device * ___device  __asm("a1"))
{
    LPRINTF (LOG_ERROR, "_exec: RemDevice unimplemented STUB called.\n");
    assert(FALSE);
}

BYTE _exec_OpenDevice ( register struct ExecBase  *SysBase    __asm("a6"),
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
        LPRINTF (LOG_ERROR, "_exec: OpenDevice() couldn't find device '%s'\n", devName);
    }

    Permit();

    return ioRequest->io_Error;
}

void _exec_CloseDevice ( register struct ExecBase  *SysBase   __asm("a6"),
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

BYTE _exec_DoIO ( register struct ExecBase  *SysBase    __asm("a6"),
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

void _exec_SendIO ( register struct ExecBase * SysBase __asm("a6"),
                                                        register struct IORequest * ___ioRequest  __asm("a1"))
{
    LPRINTF (LOG_ERROR, "_exec: SendIO unimplemented STUB called.\n");
    assert(FALSE);
}

struct IORequest * _exec_CheckIO ( register struct ExecBase * SysBase __asm("a6"),
                                                        register struct IORequest * ___ioRequest  __asm("a1"))
{
    LPRINTF (LOG_ERROR, "_exec: CheckIO unimplemented STUB called.\n");
    assert(FALSE);
    return NULL;
}

BYTE _exec_WaitIO ( register struct ExecBase * SysBase __asm("a6"),
                                                        register struct IORequest * ___ioRequest  __asm("a1"))
{
    LPRINTF (LOG_ERROR, "_exec: WaitIO unimplemented STUB called.\n");
    assert(FALSE);
    return 0;
}

void _exec_AbortIO ( register struct ExecBase * SysBase __asm("a6"),
                                                        register struct IORequest * ___ioRequest  __asm("a1"))
{
    LPRINTF (LOG_ERROR, "_exec: AbortIO unimplemented STUB called.\n");
    assert(FALSE);
}

void _exec_AddResource ( register struct ExecBase * SysBase __asm("a6"),
                                                        register APTR ___resource  __asm("a1"))
{
    LPRINTF (LOG_ERROR, "_exec: AddResource unimplemented STUB called.\n");
    assert(FALSE);
}

void _exec_RemResource ( register struct ExecBase * SysBase __asm("a6"),
                                                        register APTR ___resource  __asm("a1"))
{
    LPRINTF (LOG_ERROR, "_exec: RemResource unimplemented STUB called.\n");
    assert(FALSE);
}

APTR _exec_OpenResource ( register struct ExecBase * SysBase __asm("a6"),
                                                        register CONST_STRPTR ___resName  __asm("a1"))
{
    LPRINTF (LOG_ERROR, "_exec: OpenResource unimplemented STUB called.\n");
    assert(FALSE);
    return NULL;
}

APTR _exec_RawDoFmt ( register struct ExecBase * SysBase __asm("a6"),
                                                        register CONST_STRPTR ___formatString  __asm("a0"),
                                                        register const APTR ___dataStream  __asm("a1"),
                                                        register VOID (*___putChProc)()  __asm("a2"),
                                                        register APTR ___putChData  __asm("a3"))
{
    LPRINTF (LOG_ERROR, "_exec: RawDoFmt unimplemented STUB called.\n");
    assert(FALSE);
    return NULL;
}

ULONG exec_GetCC ( register struct ExecBase * SysBase __asm("a6"));

asm(
"_exec_GetCC:                    \n"
"       move.w      sr, d0       \n"
"       rts                      \n");

ULONG _exec_TypeOfMem ( register struct ExecBase * SysBase __asm("a6"),
                                                        register const APTR ___address  __asm("a1"))
{
    LPRINTF (LOG_ERROR, "_exec: TypeOfMem unimplemented STUB called.\n");
    assert(FALSE);
    return 0;
}

ULONG _exec_Procure ( register struct ExecBase * SysBase __asm("a6"),
                                                        register struct SignalSemaphore * ___sigSem  __asm("a0"),
                                                        register struct SemaphoreMessage * ___bidMsg  __asm("a1"))
{
    LPRINTF (LOG_ERROR, "_exec: Procure unimplemented STUB called.\n");
    assert(FALSE);
    return 0;
}

void _exec_Vacate ( register struct ExecBase * SysBase __asm("a6"),
                                                        register struct SignalSemaphore * ___sigSem  __asm("a0"),
                                                        register struct SemaphoreMessage * ___bidMsg  __asm("a1"))
{
    LPRINTF (LOG_ERROR, "_exec: Vacate unimplemented STUB called.\n");
    assert(FALSE);
}

struct Library * _exec_OpenLibrary ( register struct ExecBase *SysBase __asm("a6"),
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
        LPRINTF (LOG_ERROR, "\n_exec: OpenLibrary: *** ERROR: requested library %s was not found.\n\n", libName);
    }

    return lib;
}

void _exec_InitSemaphore ( register struct ExecBase * SysBase __asm("a6"),
                                                         register struct SignalSemaphore * ___sigSem  __asm("a0"))
{
    DPRINTF (LOG_DEBUG, "_exec: InitSemaphore called, sigSem=0x%08lx\n", ___sigSem);

    if (!___sigSem)
        return;

    /* Clear list of wait messages */
    ___sigSem->ss_WaitQueue.mlh_Head     = (struct MinNode *)&___sigSem->ss_WaitQueue.mlh_Tail;
    ___sigSem->ss_WaitQueue.mlh_Tail     = NULL;
    ___sigSem->ss_WaitQueue.mlh_TailPred = (struct MinNode *)&___sigSem->ss_WaitQueue.mlh_Head;

    /* Set type of Semaphore */
    ___sigSem->ss_Link.ln_Type = NT_SIGNALSEM;

    /* Semaphore is currently unused */
    ___sigSem->ss_NestCount = 0;

    /* Semaphore has no owner yet */
    ___sigSem->ss_Owner = NULL;

    /* Semaphore has no queue */
    ___sigSem->ss_QueueCount = -1;
}

void _exec_ObtainSemaphore ( register struct ExecBase * SysBase __asm("a6"),
                                                         register struct SignalSemaphore * ___sigSem  __asm("a0"))
{
    DPRINTF (LOG_DEBUG, "_exec: ObtainSemaphore called, sigSem=0x%08lx\n", ___sigSem);

    if (!___sigSem)
        return;

    struct Task *ThisTask = SysBase->ThisTask;

    /* If there's no ThisTask, we're in single-threaded init code */
    if (!ThisTask)
        return;

    /* If task is being removed, don't wait */
    if (ThisTask->tc_State == TS_REMOVED)
        return;

    Forbid();

    /* Increment queue count */
    ___sigSem->ss_QueueCount++;

    if (___sigSem->ss_QueueCount == 0)
    {
        /* We now own the semaphore */
        ___sigSem->ss_Owner = ThisTask;
        ___sigSem->ss_NestCount = 1;
    }
    else if (___sigSem->ss_Owner == ThisTask)
    {
        /* We already own it, increment nest count */
        ___sigSem->ss_NestCount++;
    }
    else
    {
        /* Semaphore is owned by another task, we need to wait */
        struct SemaphoreRequest sr;
        sr.sr_Waiter = ThisTask;

        /* Clear SIGF_SINGLE to ensure we wait */
        ThisTask->tc_SigRecvd &= ~SIGF_SINGLE;

        /* Add to wait queue */
        AddTail((struct List *)&___sigSem->ss_WaitQueue, (struct Node *)&sr);

        /* Wait for the semaphore */
        Wait(SIGF_SINGLE);
    }

    Permit();
}

void _exec_ReleaseSemaphore ( register struct ExecBase * SysBase __asm("a6"),
                                                         register struct SignalSemaphore * ___sigSem  __asm("a0"))
{
    DPRINTF (LOG_DEBUG, "_exec: ReleaseSemaphore called, sigSem=0x%08lx\n", ___sigSem);

    if (!___sigSem)
        return;

    struct Task *ThisTask = SysBase->ThisTask;

    /* Skip if no task context */
    if (!ThisTask)
        return;

    if (ThisTask->tc_State == TS_REMOVED)
        return;

    Forbid();

    /* Decrement nest count and queue count */
    ___sigSem->ss_NestCount--;
    ___sigSem->ss_QueueCount--;

    if (___sigSem->ss_NestCount == 0)
    {
        /* Check if there are waiters */
        if (___sigSem->ss_QueueCount >= 0 &&
            ___sigSem->ss_WaitQueue.mlh_Head->mln_Succ != NULL)
        {
            struct SemaphoreRequest *sr;

            /* Get first waiter */
            sr = (struct SemaphoreRequest *)___sigSem->ss_WaitQueue.mlh_Head;

            /* Remove from wait queue */
            Remove((struct Node *)sr);

            /* Set new owner */
            ___sigSem->ss_NestCount = 1;
            ___sigSem->ss_Owner = sr->sr_Waiter;

            /* Signal the waiter */
            if (sr->sr_Waiter)
            {
                Signal(sr->sr_Waiter, SIGF_SINGLE);
            }
        }
        else
        {
            /* No waiters, clear owner */
            ___sigSem->ss_Owner = NULL;
            ___sigSem->ss_QueueCount = -1;
        }
    }

    Permit();
}

ULONG _exec_AttemptSemaphore ( register struct ExecBase * SysBase __asm("a6"),
                                                         register struct SignalSemaphore * ___sigSem  __asm("a0"))
{
    DPRINTF (LOG_DEBUG, "_exec: AttemptSemaphore called, sigSem=0x%08lx\n", ___sigSem);

    if (!___sigSem)
        return FALSE;

    struct Task *ThisTask = SysBase->ThisTask;
    ULONG retval = TRUE;

    /* If no task context, succeed */
    if (!ThisTask)
        return TRUE;

    Forbid();

    /* Increment queue count */
    ___sigSem->ss_QueueCount++;

    if (___sigSem->ss_QueueCount == 0)
    {
        /* Semaphore is free, take it */
        ___sigSem->ss_Owner = ThisTask;
        ___sigSem->ss_NestCount = 1;
    }
    else if (___sigSem->ss_Owner == ThisTask)
    {
        /* We already own it */
        ___sigSem->ss_NestCount++;
    }
    else
    {
        /* Can't get it, decrement queue count and return FALSE */
        ___sigSem->ss_QueueCount--;
        retval = FALSE;
    }

    Permit();

    return retval;
}

void _exec_ObtainSemaphoreList ( register struct ExecBase * SysBase __asm("a6"),
                                                        register struct List * ___sigSem  __asm("a0"))
{
    DPRINTF (LOG_DEBUG, "_exec: ObtainSemaphoreList unimplemented STUB called.\n");
    assert(FALSE);
}

void _exec_ReleaseSemaphoreList ( register struct ExecBase * SysBase __asm("a6"),
                                                        register struct List * ___sigSem  __asm("a0"))
{
    DPRINTF (LOG_DEBUG, "_exec: ReleaseSemaphoreList unimplemented STUB called.\n");
    assert(FALSE);
}

struct SignalSemaphore * _exec_FindSemaphore ( register struct ExecBase * SysBase __asm("a6"),
                                                        register STRPTR ___name  __asm("a1"))
{
    DPRINTF (LOG_DEBUG, "_exec: FindSemaphore unimplemented STUB called.\n");
    assert(FALSE);
    return NULL;
}

void _exec_AddSemaphore ( register struct ExecBase * SysBase __asm("a6"),
                                                        register struct SignalSemaphore * ___sigSem  __asm("a1"))
{
    DPRINTF (LOG_DEBUG, "_exec: AddSemaphore unimplemented STUB called.\n");
    assert(FALSE);
}

void _exec_RemSemaphore ( register struct ExecBase * SysBase __asm("a6"),
                                                        register struct SignalSemaphore * ___sigSem  __asm("a1"))
{
    DPRINTF (LOG_DEBUG, "_exec: RemSemaphore unimplemented STUB called.\n");
    assert(FALSE);
}

ULONG _exec_SumKickData ( register struct ExecBase * SysBase __asm("a6"))
{
    DPRINTF (LOG_DEBUG, "_exec: SumKickData unimplemented STUB called.\n");
    assert(FALSE);
    return 0;
}

void _exec_AddMemList ( register struct ExecBase * SysBase __asm("a6"),
                                                        register ULONG ___size  __asm("d0"),
                                                        register ULONG ___attributes  __asm("d1"),
                                                        register LONG ___pri  __asm("d2"),
                                                        register APTR ___base  __asm("a0"),
                                                        register CONST_STRPTR ___name  __asm("a1"))
{
    LPRINTF (LOG_ERROR, "_exec: AddMemList unimplemented STUB called.\n");
    assert(FALSE);
}

void _exec_CopyMem ( register struct ExecBase *SysBase __asm("a6"),
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

void _exec_CopyMemQuick ( register struct ExecBase * SysBase __asm("a6"),
                                                        register const APTR ___source  __asm("a0"),
                                                        register APTR ___dest  __asm("a1"),
                                                        register ULONG ___size  __asm("d0"))
{
    LPRINTF (LOG_ERROR, "_exec: CopyMemQuick unimplemented STUB called.\n");
    assert(FALSE);
}

void _exec_CacheClearU ( register struct ExecBase * SysBase __asm("a6"))
{
    LPRINTF (LOG_ERROR, "_exec: CacheClearU unimplemented STUB called.\n");
    assert(FALSE);
}

void _exec_CacheClearE ( register struct ExecBase * SysBase __asm("a6"),
                                                        register APTR ___address  __asm("a0"),
                                                        register ULONG ___length  __asm("d0"),
                                                        register ULONG ___caches  __asm("d1"))
{
    LPRINTF (LOG_ERROR, "_exec: CacheClearE unimplemented STUB called.\n");
    assert(FALSE);
}

ULONG _exec_CacheControl ( register struct ExecBase * SysBase __asm("a6"),
                                                        register ULONG ___cacheBits  __asm("d0"),
                                                        register ULONG ___cacheMask  __asm("d1"))
{
    LPRINTF (LOG_ERROR, "_exec: CacheControl unimplemented STUB called.\n");
    assert(FALSE);
    return 0;
}

APTR _exec_CreateIORequest ( register struct ExecBase * SysBase __asm("a6"),
                                                        register const struct MsgPort * ___port  __asm("a0"),
                                                        register ULONG ___size  __asm("d0"))
{
    DPRINTF (LOG_DEBUG, "_exec: CreateIORequest() called, port=0x%08lx, size=%ld\n", ___port, ___size);

    struct IORequest *ret = NULL;

    /* A NULL ioReplyPort is legal but has no effect */
    if (___port == NULL)
        return NULL;

    /* Allocate the memory */
    ret = (struct IORequest *)AllocMem(___size, MEMF_PUBLIC|MEMF_CLEAR);

    if (ret != NULL)
    {
        /* Initialize it. */
        ret->io_Message.mn_ReplyPort = (struct MsgPort *)___port;

        /* This size is needed to free the memory at DeleteIORequest() time. */
        ret->io_Message.mn_Length = ___size;
    }

    DPRINTF (LOG_DEBUG, "_exec: CreateIORequest() returning 0x%08lx\n", ret);

    /* All done. */
    return ret;
}

void _exec_DeleteIORequest ( register struct ExecBase * SysBase __asm("a6"),
                                                        register APTR ___iorequest  __asm("a0"))
{
    DPRINTF (LOG_DEBUG, "_exec: DeleteIORequest() called, iorequest=0x%08lx\n", ___iorequest);

    if (___iorequest != NULL)
    {
        /* Just free the memory */
        FreeMem(___iorequest, ((struct Message *)___iorequest)->mn_Length);
    }
}

struct MsgPort * _exec_CreateMsgPort ( register struct ExecBase * SysBase __asm("a6"))
{
    DPRINTF (LOG_DEBUG, "_exec: CreateMsgPort() called\n");

    struct MsgPort *ret;

    /* Allocate memory for struct MsgPort */
    ret = (struct MsgPort *)AllocMem(sizeof(struct MsgPort), MEMF_PUBLIC|MEMF_CLEAR);
    if (ret != NULL)
    {
        BYTE sb;

        /* Allocate a signal bit */
        sb = AllocSignal(-1);
        if (sb != -1)
        {
            /* Initialize messageport structure */
            ret->mp_Node.ln_Type = NT_MSGPORT;
            ret->mp_Flags        = PA_SIGNAL;
            NEWLIST(&ret->mp_MsgList);

            /* Set signal bit. */
            ret->mp_SigBit = sb;

            /* Set task to send the signal to. */
            ret->mp_SigTask = SysBase->ThisTask;

            DPRINTF (LOG_DEBUG, "_exec: CreateMsgPort() returning 0x%08lx, sigbit=%d\n", ret, sb);

            /* Now the port is ready for use. */
            return ret;
        }

        /* Couldn't get the signal bit. Free the memory. */
        FreeMem(ret, sizeof(struct MsgPort));
    }

    DPRINTF (LOG_DEBUG, "_exec: CreateMsgPort() failed\n");

    /* function failed */
    return NULL;
}

void _exec_DeleteMsgPort ( register struct ExecBase * SysBase __asm("a6"),
                                                        register struct MsgPort * ___port  __asm("a0"))
{
    DPRINTF (LOG_DEBUG, "_exec: DeleteMsgPort() called, port=0x%08lx\n", ___port);

    /* Only if there is something to free */
    if (___port != NULL)
    {
        /* Free signal bit */
        FreeSignal(___port->mp_SigBit);

        /* And memory */
        FreeMem(___port, sizeof(struct MsgPort));
    }
}

void _exec_ObtainSemaphoreShared ( register struct ExecBase * SysBase __asm("a6"),
                                                        register struct SignalSemaphore * ___sigSem  __asm("a0"))
{
    LPRINTF (LOG_ERROR, "_exec: ObtainSemaphoreShared unimplemented STUB called.\n");
    assert(FALSE);
}

APTR _exec_AllocVec ( register struct ExecBase * SysBase        __asm("a6"),
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

void _exec_FreeVec ( register struct ExecBase *SysBase     __asm("a6"),
                              register APTR             memoryBlock __asm("a1"))
{
    DPRINTF (LOG_DEBUG, "_exec: FreeVec called, memoryBlock=0x%08lx\n", memoryBlock);

    if (!memoryBlock)
        return;

    memoryBlock -= 4;
    FreeMem (memoryBlock, *((ULONG *) memoryBlock));
}

APTR _exec_CreatePool ( register struct ExecBase * SysBase __asm("a6"),
                                                        register ULONG ___requirements  __asm("d0"),
                                                        register ULONG ___puddleSize  __asm("d1"),
                                                        register ULONG ___threshSize  __asm("d2"))
{
    DPRINTF (LOG_DEBUG, "_exec: CreatePool unimplemented STUB called.\n");
    assert(FALSE);
    return NULL;
}

void _exec_DeletePool ( register struct ExecBase * SysBase __asm("a6"),
                                                        register APTR ___poolHeader  __asm("a0"))
{
    DPRINTF (LOG_DEBUG, "_exec: DeletePool unimplemented STUB called.\n");
    assert(FALSE);
}

APTR _exec_AllocPooled ( register struct ExecBase * SysBase __asm("a6"),
                                                        register APTR ___poolHeader  __asm("a0"),
                                                        register ULONG ___memSize  __asm("d0"))
{
    DPRINTF (LOG_DEBUG, "_exec: AllocPooled unimplemented STUB called.\n");
    assert(FALSE);
    return NULL;
}

void _exec_FreePooled ( register struct ExecBase * SysBase __asm("a6"),
                                                        register APTR ___poolHeader  __asm("a0"),
                                                        register APTR ___memory  __asm("a1"),
                                                        register ULONG ___memSize  __asm("d0"))
{
    DPRINTF (LOG_DEBUG, "_exec: FreePooled unimplemented STUB called.\n");
    assert(FALSE);
}

ULONG _exec_AttemptSemaphoreShared ( register struct ExecBase * SysBase __asm("a6"),
                                                        register struct SignalSemaphore * ___sigSem  __asm("a0"))
{
    DPRINTF (LOG_DEBUG, "_exec: AttemptSemaphoreShared unimplemented STUB called.\n");
    assert(FALSE);
    return 0;
}

void _exec_ColdReboot ( register struct ExecBase * SysBase __asm("a6"))
{
    DPRINTF (LOG_DEBUG, "_exec: ColdReboot unimplemented STUB called.\n");
    assert(FALSE);
}

void exec_StackSwap ( register struct ExecBase        *SysBase   __asm("a6"),
                               register struct StackSwapStruct *newStack  __asm("a0"));

asm(
".set INTENA, 0xdff09a                                                                      \n"
"_exec_StackSwap:                                                                           \n"
"        move.l     276(a6), a1                     | ThisTask -> a1                        \n"
"        move.w     #0x04000, INTENA                | disable interrupts                    \n"
"        addq.b     #1, 294(A6)                     | IDNestCnt++                           \n"
"        /* swap stk_Upper */                                                               \n"
"        move.l     62(a1), d0                      | tc_SPUpper -> d0                      \n"
"        move.l     4(a0), 62(a1)                   | newStack->stk_Upper -> tc_SPUpper     \n"
"        move.l     d0, 4(a0)                       | d0 -> newStack->stk_Upper             \n"
"        /* swap stk_Lower */                                                               \n"
"        move.l     58(a1), d0                      | tc_SPLower -> d0                      \n"
"        move.l     0(a0), 58(a1)                   | newStack->stk_SPLower -> tc_SPLower   \n"
"        move.l     d0, 0(a0)                       | d0 -> newStack->stk_Lower             \n"
"                                                                                           \n"
"        move.l     8(a0), 54(a1)                   | newStack->stk_Pointer -> tc_SPReg     \n"
"                                                                                           \n"
"        /* actual stack swap */                                                            \n"
"        move.l     8(a0), a1                       | newStack->stk_Pointer -> a1           \n"
"        move.l     (sp)+, d0                       | old stack's return address -> d0      \n"
"        move.l     sp, 8(a0)                       | old stack -> newStack->stk_Pointer    \n"
"        move.l     d0, -(a1)                       | push return address onto new stack    \n"
"        move.l     a1, sp                          | and switch to it                      \n"
"        subq.b     #1, 294(A6)                     | IDNestCnt--                           \n"
"        bge.s      1f                              | >=0 ?                                 \n"
"        move.w     #0x0c000, INTENA                | enable                                \n"
"1:      rts                                        | done                                  \n"
);


APTR _exec_CachePreDMA ( register struct ExecBase * SysBase __asm("a6"),
                                                        register const APTR ___address  __asm("a0"),
                                                        register ULONG * ___length  __asm("a1"),
                                                        register ULONG ___flags  __asm("d0"))
{
    DPRINTF (LOG_DEBUG, "_exec: CachePreDMA unimplemented STUB called.\n");
    assert(FALSE);
    return NULL;
}

void _exec_CachePostDMA ( register struct ExecBase * SysBase __asm("a6"),
                                                        register const APTR ___address  __asm("a0"),
                                                        register ULONG * ___length  __asm("a1"),
                                                        register ULONG ___flags  __asm("d0"))
{
    DPRINTF (LOG_DEBUG, "_exec: CachePostDMA unimplemented STUB called.\n");
    assert(FALSE);
}

void _exec_AddMemHandler ( register struct ExecBase * SysBase __asm("a6"),
                                                        register struct Interrupt * ___memhand  __asm("a1"))
{
    LPRINTF (LOG_ERROR, "_exec: AddMemHandler unimplemented STUB called.\n");
    assert(FALSE);
}

void _exec_RemMemHandler ( register struct ExecBase * SysBase __asm("a6"),
                                                        register struct Interrupt * ___memhand  __asm("a1"))
{
    LPRINTF (LOG_ERROR, "_exec: RemMemHandler unimplemented STUB called.\n");
    assert(FALSE);
}

struct Library *registerBuiltInLib (ULONG dSize, struct Resident *romTAG)
{
    DPRINTF (LOG_DEBUG, "_exec: registerBuiltInLib dSize=%ld, romTAG rt_Name=%s rt_IdString=%s\n",
             dSize, romTAG->rt_Name, romTAG->rt_IdString);

    struct InitTable *initTab = romTAG->rt_Init;
    struct Library *libBase = MakeLibrary (initTab->FunctionTable, initTab->DataTable, initTab->InitLibFn, dSize, initTab->LibBaseSize);

    AddTail (&SysBase->LibList, (struct Node*) libBase);

    return libBase;
}

struct Library *registerBuiltInDev (ULONG dSize, struct Resident *romTAG)
{
    DPRINTF (LOG_DEBUG, "_exec: registerBuiltInDev dSize=%ld, romTAG rt_Name=%s rt_IdString=%s\n",
             dSize, romTAG->rt_Name, romTAG->rt_IdString);

    struct InitTable *initTab = romTAG->rt_Init;
    struct Library *libBase = MakeLibrary (initTab->FunctionTable, initTab->DataTable, initTab->InitLibFn, dSize, initTab->LibBaseSize);

    AddTail (&SysBase->DeviceList, (struct Node*) libBase);

    return libBase;
}

#if 0
static void _myTestTask(void)
{
    for (int i = 0; i<10000; i++)
    {
        DPRINTF (LOG_INFO, "_exec: test task iter SysBase->Elapsed=%d, SysBase->Quantum=%d\n", SysBase->Elapsed, SysBase->Quantum);
    }
    DPUTS (LOG_INFO, "_exec: test task done\n");
}
#endif

typedef ULONG (*cliChildFn_t) ( register ULONG  arglen __asm("d0"),
                                register STRPTR args   __asm("a0"));
void _bootstrap(void)
{
    DPRINTF (LOG_INFO, "_exec: _bootstrap() called\n");

    // just for testing purposes add another task
    // FIXME: remove _createTask ("exec test task", 0, _myTestTask, 8192);

    // load our program

    char binfn[1024];
    emucall1 (EMU_CALL_LOADFILE, (ULONG) &binfn);
    DPRINTF (LOG_INFO, "_exec: _bootstrap(): loadfile: %s\n", binfn);

    OpenLibrary ((STRPTR)"dos.library", 0);
    OpenLibrary ((STRPTR)"utility.library", 0);
    BPTR segs = LoadSeg ((STRPTR)binfn);

    DPRINTF (LOG_INFO, "_exec: _bootstrap(): segs=0x%08lx\n", segs);

    if (!segs)
    {
        LPRINTF (LOG_ERROR, "_exec: _bootstrap(): failed to load %s\n", binfn);
        emu_stop(1);
    }

    emucall0 (EMU_CALL_LOADED);

    // inject initPC pointing to our loaded code into our task's stack

    APTR initPC = BADDR(segs+1);
    DPRINTF (LOG_INFO, "_exec: _bootstrap(): initPC is 0x%08lx\n", initPC);
    U_hexdump (LOG_DEBUG, initPC, 32);

    /* Get command line arguments */
    char args_buf[4096];
    emucall1 (EMU_CALL_GETARGS, (ULONG) args_buf);
    int args_len = strlen(args_buf);
    if (args_len == 0) {
        args_buf[0] = '\n';
        args_buf[1] = '\0';
        args_len = 1;
    }
    DPRINTF (LOG_INFO, "_exec: _bootstrap(): args='%s' len=%d\n", args_buf, args_len);

    /* simply JSR() into our child process */

    cliChildFn_t childfn = initPC;
    ULONG rv = childfn (args_len, (STRPTR)args_buf);

    DPRINTF (LOG_INFO, "_exec: _bootstrap(): childfn() returned, rv=%ld\n", rv);
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

    DPRINTF (LOG_INFO, "_exec: _bootstrap(): calling emu_stop(%ld)...\n", rv);
    emu_stop(rv);
    DPRINTF (LOG_INFO, "_exec: _bootstrap(): emu_stop... done.\n");
}

void coldstart (void)
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

    DPRINTF (LOG_INFO, "coldstart: g_ExecJumpTable    = 0x%08lx\n", g_ExecJumpTable   );
    DPRINTF (LOG_INFO, "           RAM_START          = 0x%08lx\n", RAM_START         );
    DPRINTF (LOG_INFO, "           RAM_END            = 0x%08lx\n", RAM_END           );

    // coldstart is at 0x00f801be, &SysBase at 0x00f80c90, SysBase at 0x0009eed0, f1 at 0x00f80028
    DPRINTF (LOG_DEBUG, "coldstart: locations in RAM: coldstart is at 0x%08x, &SysBase at 0x%08x, SysBase at 0x%08x\n",
             coldstart, &SysBase, SysBase);
    // g_ExecJumpTable at 0x00001800, &g_ExecJumpTable[0].vec at 0x00001802
    DPRINTF (LOG_DEBUG, "coldstart: g_ExecJumpTable at 0x%08x, &g_ExecJumpTable[0].vec at 0x%08x\n", g_ExecJumpTable, &g_ExecJumpTable[0].vec);

    /* set up execbase */

    SysBase = &g_SysBase;
    *(APTR *)4L = SysBase;

    DPRINTF (LOG_INFO, "          &SysBase            = 0x%08lx\n", &SysBase          );
    DPRINTF (LOG_INFO, "           SysBase            = 0x%08lx\n", SysBase           );

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
    g_ExecJumpTable[EXEC_FUNCTABLE_ENTRY(-732)].vec =  exec_StackSwap;
    g_ExecJumpTable[EXEC_FUNCTABLE_ENTRY(-762)].vec = _exec_CachePreDMA;
    g_ExecJumpTable[EXEC_FUNCTABLE_ENTRY(-768)].vec = _exec_CachePostDMA;
    g_ExecJumpTable[EXEC_FUNCTABLE_ENTRY(-774)].vec = _exec_AddMemHandler;
    g_ExecJumpTable[EXEC_FUNCTABLE_ENTRY(-780)].vec = _exec_RemMemHandler;

    SysBase->SoftVer = VERSION;

    // set up memory management

    NEWLIST (&SysBase->MemList);
    SysBase->MemList.lh_Type = NT_MEMORY;

    struct MemChunk *mc = (struct MemChunk *) RAM_START;

    mc->mc_Next  = NULL;
    mc->mc_Bytes = RAM_END-RAM_START+1;

    g_MemHeader.mh_Node.ln_Type = NT_MEMORY;
    g_MemHeader.mh_Node.ln_Pri  = 0;
    g_MemHeader.mh_Node.ln_Name = NULL;
    g_MemHeader.mh_Attributes   = MEMF_CHIP | MEMF_PUBLIC;
    g_MemHeader.mh_First        = mc;
    g_MemHeader.mh_Lower        = (APTR) RAM_START;
    g_MemHeader.mh_Upper        = (APTR) (RAM_END + 1);
    g_MemHeader.mh_Free         = RAM_END-RAM_START+1;

    DPRINTF (LOG_DEBUG, "coldstart: setting up first struct MemHeader at 0x%08lx:\n", &g_MemHeader);
    DPRINTF (LOG_DEBUG, "           g_MemHeader.mh_First=0x%08lx, g_MemHeader.mh_Lower=0x%08lx, g_MemHeader.mh_Upper=0x%08lx,\n",
             g_MemHeader.mh_First, g_MemHeader.mh_Lower, g_MemHeader.mh_Upper);
    DPRINTF (LOG_DEBUG, "           g_MemHeader.mh_Free=%ld, g_MemHeader.mh_Attributes=0x%08lx\n",
             g_MemHeader.mh_Free, g_MemHeader.mh_Attributes);

    AddTail (&SysBase->MemList, &g_MemHeader.mh_Node);

    // init and register built-in libraries

    DPRINTF (LOG_DEBUG, "coldstart: registering built-in libraries\n");

    NEWLIST (&SysBase->LibList);
    SysBase->LibList.lh_Type = NT_LIBRARY;

    DOSBase       = (struct DosLibrary    *) registerBuiltInLib (sizeof(*DOSBase)       , __lxa_dos_ROMTag       );
    UtilityBase   = (struct UtilityBase   *) registerBuiltInLib (sizeof(*UtilityBase)   , __lxa_utility_ROMTag   );
    MathBase      = (struct Library       *) registerBuiltInLib (sizeof(*MathBase)      , __lxa_mathffp_ROMTag   );
    MathTransBase = (struct Library       *) registerBuiltInLib (sizeof(*MathTransBase) , __lxa_mathtrans_ROMTag );
    GfxBase       = (struct GfxBase       *) registerBuiltInLib (sizeof(*GfxBase)       , __lxa_graphics_ROMTag  );
    IntuitionBase = (struct IntuitionBase *) registerBuiltInLib (sizeof(*IntuitionBase) , __lxa_intuition_ROMTag );
    ExpansionBase = (struct ExpansionBase *) registerBuiltInLib (sizeof(*ExpansionBase) , __lxa_expansion_ROMTag );

    DPRINTF (LOG_DEBUG, "coldstart: done registering built-in libraries\n");

    // init and register built-in devices

    DPRINTF (LOG_DEBUG, "coldstart: registering built-in devices\n");

    NEWLIST (&SysBase->DeviceList);
    SysBase->DeviceList.lh_Type = NT_DEVICE;

    DeviceInputBase   = (struct Library *) registerBuiltInDev (sizeof (*DeviceInputBase)  , __lxa_input_ROMTag  );
    DeviceConsoleBase = (struct Library *) registerBuiltInDev (sizeof (*DeviceConsoleBase), __lxa_console_ROMTag);

    DPRINTF (LOG_DEBUG, "coldstart: done registering built-in devices\n");

    // init multitasking
    NEWLIST (&SysBase->TaskReady);
    SysBase->TaskReady.lh_Type = NT_TASK;
    NEWLIST (&SysBase->TaskWait);
    SysBase->TaskWait.lh_Type = NT_TASK;

    // init port list for AddPort/RemPort/FindPort
    NEWLIST (&SysBase->PortList);
    SysBase->PortList.lh_Type = NT_MSGPORT;

    SysBase->TaskExitCode = _defaultTaskExit;
    SysBase->Quantum      = DEFAULT_SCHED_QUANTUM;
    SysBase->Elapsed      = DEFAULT_SCHED_QUANTUM;
    SysBase->SysFlags     = 0;
    SysBase->IDNestCnt    = -1;
    SysBase->TDNestCnt    = -1;

    // create a bootstrap process
    struct Process *rootProc = (struct Process *) U_allocTask ((STRPTR)"exec bootstrap", 0, DEFAULT_STACKSIZE, /*isProcess=*/ TRUE);
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

    //BPTR oldpath = 0;

    struct CommandLineInterface *cli = (struct CommandLineInterface *) AllocDosObject (DOS_CLI, (struct TagItem *)NULL);
    cli->cli_DefaultStack = (rootProc->pr_StackSize + 3) / 4;

    // FIXME: set cli_CommandDir
    char *binfn = AllocVec (1024, MEMF_CLEAR);
    emucall1 (EMU_CALL_LOADFILE, (ULONG) binfn);
    cli->cli_CommandName = MKBADDR(binfn);

    // Get command line arguments
    char *args = AllocVec (4096, MEMF_CLEAR);
    emucall1 (EMU_CALL_GETARGS, (ULONG) args);
    cli->cli_CommandFile = MKBADDR(args);

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
        : "cc", "a2"                      // doesn't really matter since will never exit from these instructions anyway (a5 is technically missing from this list)
        );

    LPRINTF (LOG_ERROR, "coldstart: this shouldn't happen\n");
    emu_stop(255);
}

