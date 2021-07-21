
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

#define RAM_START               EXEC_BASE_END + 1
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

static struct Library* __saveds _exec_OpenLibrary ( register struct ExecBase  *exb __asm("a6"),
                                                    register STRPTR libName __asm("a1"),
                                                    register ULONG  version __asm("d0"))
{
    lprintf ("exec: OpenLibrary called: libName=%s, version=%d\n", libName, version);
	return NULL;
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

    g_ExecJumpTable[EXEC_FUNCTABLE_ENTRY(-552)].vec = _exec_OpenLibrary;

    SysBase->SoftVer = VERSION;

	OpenLibrary ("dos.library", 0);

    assert(FALSE);

    emu_stop();
}

