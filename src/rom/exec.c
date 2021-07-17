
#include <inttypes.h>

#include <exec/types.h>
#include <exec/memory.h>
#include <exec/libraries.h>
#include <exec/execbase.h>
#include <exec/resident.h>
#include <exec/initializers.h>

#include <proto/exec.h>

#include "util.h"

#define VERSION  1
#define REVISION 1

#define EXLIBNAME "exec"
#define EXLIBVER  " 1.1 (2021/06/13)"

char __aligned ExLibName [] = EXLIBNAME ".library";
char __aligned ExLibID   [] = EXLIBNAME EXLIBVER;
char __aligned Copyright [] = "(C)opyright 2021 by G. Bartsch. Licensed under the Apache License, Version 2.0.";

char __aligned VERSTRING [] = "\0$VER: " EXLIBNAME EXLIBVER;

extern ULONG     __saveds __stdargs  L_OpenLibs ( struct ExecBase *execBase);
extern void      __saveds __stdargs  L_CloseLibs( void);

struct ExecBase * __saveds           InitLib    ( register struct ExecBase  *sysbase  __asm("a6"),
                                                  register BPTR              seglist  __asm("a0"),
                                                  register struct ExecBase  *exb      __asm("d0"));
struct ExecBase * __saveds           OpenLib    ( register struct ExecBase  *ExecBase __asm("a6"));
BPTR             __saveds            CloseLib   ( register struct ExecBase  *ExecBase __asm("a6"));
BPTR             __saveds            ExpungeLib ( register struct ExecBase  *exb      __asm("a6"));
ULONG                                ExtFuncLib ( void );
ULONG                                f1         ( register ULONG             a        __asm("d0"));

LONG LibStart(void)
{
    return -1;
}

void __restore_a4(void)
{
    __asm volatile("\tlea ___a4_init, a4");
}

struct InitTable
{
    ULONG              LibBaseSize;
    APTR              *FunctionTable;
    struct MyDataInit *DataTable;
    APTR               InitLibTable;
};

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

extern APTR              FuncTab [];
extern struct MyDataInit DataTab;
extern struct InitTable  InitTab;
extern APTR              EndResident;

struct Resident __aligned ROMTag =
{							//                               offset
    RTC_MATCHWORD,			// UWORD rt_MatchWord;                0
    &ROMTag,				// struct Resident *rt_MatchTag;      2
    &EndResident,			// APTR  rt_EndSkip;                  6
    RTF_AUTOINIT,			// UBYTE rt_Flags;                    7
    VERSION,				// UBYTE rt_Version;                  8
    NT_LIBRARY,             // UBYTE rt_Type;                     9
    0,						// BYTE  rt_Pri;                     10
    &ExLibName[0],			// char  *rt_Name;                   14
    &ExLibID[0],			// char  *rt_IdString;               18
    &InitTab				// APTR  rt_Init;                    22
};

APTR EndResident;

struct InitTable InitTab =
{
    (ULONG)               sizeof(struct ExecBase),
    (APTR              *) &FuncTab[0],
    (struct MyDataInit *) &DataTab,
    (APTR)                InitLib
};

APTR FuncTab [] =
{
    OpenLib,
    CloseLib,
    ExpungeLib,
    ExtFuncLib,

    f1,

    (APTR) ((LONG)-1)
};

struct MyDataInit DataTab =
{
    /* ln_Type      */ INITBYTE(OFFSET(Node,         ln_Type),      NT_LIBRARY),
    /* ln_Name      */ 0x80, (UBYTE) (ULONG) OFFSET(Node,    ln_Name), (ULONG) &ExLibName[0],
    /* lib_Flags    */ INITBYTE(OFFSET(Library,      lib_Flags),    LIBF_SUMUSED|LIBF_CHANGED),
    /* lib_Version  */ INITWORD(OFFSET(Library,      lib_Version),  VERSION),
    /* lib_Revision */ INITWORD(OFFSET(Library,      lib_Revision), REVISION),
    /* lib_IdString */ 0x80, (UBYTE) (ULONG) OFFSET(Library, lib_IdString), (ULONG) &ExLibID[0],
    (ULONG) 0
};
//extern struct ExecBase *ExecBase;

#if 0
static void lputs (char *s)
{
    asm( "move.l    %0, a0\n\t"
         "trap #3"
        : /* no outputs */
        : "r" (s)
        : "cc", "a0"
        );
}
#endif

struct ExecBase * __saveds           InitLib    ( register struct ExecBase  *sysbase  __asm("a6"),
                                                  register BPTR              seglist  __asm("a0"),
                                                  register struct ExecBase  *exb      __asm("d0"))
{
#if 0
    ExecBase = exb;

    ExecBase->exb_ExecBase = sysbase;
    ExecBase->exb_SegList = seglist;

    if(L_OpenLibs(ExecBase)) return(ExecBase);

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


//    lputs ("Hello from the 68k side!\n");

//    asm("trap #3");

    return NULL;
}

struct ExecBase * __saveds OpenLib ( register struct ExecBase  *ExecBase __asm("a6"))
{
#if 0
    ExecBase->exb_LibNode.lib_OpenCnt++;
    ExecBase->exb_LibNode.lib_Flags &= ~LIBF_DELEXP;
    return ExecBase;
#endif
    return NULL;
}

BPTR __saveds CloseLib ( register struct ExecBase  *ExecBase __asm("a6"))
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

    return NULL;
}

BPTR __saveds ExpungeLib ( register struct ExecBase  *exb      __asm("a6"))
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
    return NULL;
}

ULONG ExtFuncLib(void)
{
    return NULL;
}

//struct ExecBase *ExecBase = NULL;

ULONG f1 ( register ULONG a __asm("d0"))
{
    return 0xdeadbeef;
}

/*
    // find ROMTAG structure
    uint32_t romtag_off = 0;
    uint16_t *p = (uint16_t *) &rom[0];
    while (true)
    {
        uint16_t w = ENDIAN_SWAP_16(*p);
        //printf ("0x%08x: 0x%04x\n", romtag_off, w);
        if (w == RTC_MATCHWORD)
        {
            break;
        }
        if (romtag_off >= ROM_SIZE)
        {
            fprintf (stderr, "failed to find romtag\n");
            exit(3);
        }
        p++;
        romtag_off += 2;
    }
    printf ("found RTC_MATCHWORD at offset %d\n", romtag_off);

*/

extern void handleTRAP3();

void coldstart (void)
{
    uint32_t *p = (void*) 0x00008c;
    *p = (uint32_t) handleTRAP3;

    __asm("andi.w  #0xdfff, sr\n");
    __asm("move.l  #0x9fff99, a7\n");

    lputs ("coldstart: init (puts test)\n");
    lprintf ("coldstart: init (printf test)\n");
    for (int i = 0; i<10; i++)
    {
        lprintf ("coldstart: i=%d, i*i=%d\n", i, i*i);
    }
    lprintf ("coldstart: init (printf test) done.\n");
    emu_stop();
}

