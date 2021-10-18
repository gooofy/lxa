#include <exec/types.h>
//#include <exec/memory.h>
//#include <exec/libraries.h>
#include <exec/execbase.h>
#include <exec/resident.h>
#include <exec/initializers.h>

#include "dos/dos.h"
#include "dos/dosextens.h"

#include "util.h"
#include "lxa_dos.h"

#define VERSION    1
#define REVISION   1
#define EXLIBNAME  "dos"
#define EXLIBVER   " 1.1 (2021/07/21)"

char __aligned ExLibName [] = EXLIBNAME ".library";
char __aligned ExLibID   [] = EXLIBNAME EXLIBVER;
char __aligned Copyright [] = "(C)opyright 2021 by G. Bartsch. Licensed under the Apache License, Version 2.0.";

char __aligned VERSTRING [] = "\0$VER: " EXLIBNAME EXLIBVER;

//extern ULONG     __saveds __stdargs  L_OpenLibs ( struct ExecBase *execBase);
//extern void      __saveds __stdargs  L_CloseLibs( void);

struct DosLibrary * __saveds __g_lxa_dos_InitLib    ( register struct DosLibrary *dosb    __asm("a6"),
                                                      register BPTR               seglist __asm("a0"),
                                                      register struct ExecBase   *sysb    __asm("d0"));
struct DosLibrary * __saveds __g_lxa_dos_OpenLib    ( register struct DosLibrary *dosb    __asm("a6"));
BPTR                __saveds __g_lxa_dos_CloseLib   ( register struct DosLibrary *dosb    __asm("a6"));
BPTR                __saveds __g_lxa_dos_ExpungeLib ( register struct DosLibrary *dosb    __asm("a6"));
ULONG                        __g_lxa_dos_ExtFuncLib ( void );

#if 0
LONG LibStart(void)
{
    return -1;
}

void __restore_a4(void)
{
    __asm volatile("\tlea ___a4_init, a4");
}
#endif

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
{							    //                               offset
    RTC_MATCHWORD,			    // UWORD rt_MatchWord;                0
    &ROMTag,				    // struct Resident *rt_MatchTag;      2
    &__g_lxa_dos_EndResident,	// APTR  rt_EndSkip;                  6
    RTF_AUTOINIT,			    // UBYTE rt_Flags;                    7
    VERSION,				    // UBYTE rt_Version;                  8
    NT_LIBRARY,                 // UBYTE rt_Type;                     9
    0,						    // BYTE  rt_Pri;                     10
    &ExLibName[0],			    // char  *rt_Name;                   14
    &ExLibID[0],			    // char  *rt_IdString;               18
    &__g_lxa_dos_InitTab	    // APTR  rt_Init;                    22
};

APTR __g_lxa_dos_EndResident;
struct Resident *__lxa_dos_ROMTag = &ROMTag;

struct InitTable __g_lxa_dos_InitTab =
{
    (ULONG)               sizeof(struct DosLibrary), 	// LibBaseSize
    (APTR              *) &__g_lxa_dos_FuncTab[0],		// FunctionTable
    (APTR)                &__g_lxa_dos_DataTab,			// DataTable
    (APTR)                __g_lxa_dos_InitLib			// InitLibFn
};

APTR __g_lxa_dos_FuncTab [] =
{
    __g_lxa_dos_OpenLib,
    __g_lxa_dos_CloseLib,
    __g_lxa_dos_ExpungeLib,
    __g_lxa_dos_ExtFuncLib,

    // FIXME f1,

    (APTR) ((LONG)-1)
};

struct MyDataInit __g_lxa_dos_DataTab =
{
    /* ln_Type      */ INITBYTE(OFFSET(Node,         ln_Type),      NT_LIBRARY),
    /* ln_Name      */ 0x80, (UBYTE) (ULONG) OFFSET(Node,    ln_Name), (ULONG) &ExLibName[0],
    /* lib_Flags    */ INITBYTE(OFFSET(Library,      lib_Flags),    LIBF_SUMUSED|LIBF_CHANGED),
    /* lib_Version  */ INITWORD(OFFSET(Library,      lib_Version),  VERSION),
    /* lib_Revision */ INITWORD(OFFSET(Library,      lib_Revision), REVISION),
    /* lib_IdString */ 0x80, (UBYTE) (ULONG) OFFSET(Library, lib_IdString), (ULONG) &ExLibID[0],
    (ULONG) 0
};

struct DosLibrary * __saveds __g_lxa_dos_InitLib    ( register struct DosLibrary *dosb    __asm("a6"),
                                                      register BPTR               seglist __asm("a0"),
                                                      register struct ExecBase   *sysb    __asm("d0"))
{
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

struct DosLibrary * __saveds __g_lxa_dos_OpenLib ( register struct DosLibrary  *DosLibrary __asm("a6"))
{
#if 0
    ExecBase->exb_LibNode.lib_OpenCnt++;
    ExecBase->exb_LibNode.lib_Flags &= ~LIBF_DELEXP;
    return ExecBase;
#endif
    return NULL;
}

BPTR __saveds __g_lxa_dos_CloseLib ( register struct DosLibrary  *dosb __asm("a6"))
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

BPTR __saveds __g_lxa_dos_ExpungeLib ( register struct DosLibrary  *dosb      __asm("a6"))
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

ULONG __g_lxa_dos_ExtFuncLib(void)
{
    return NULL;
}

