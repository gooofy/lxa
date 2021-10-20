#include <exec/types.h>
//#include <exec/memory.h>
//#include <exec/libraries.h>
#include <exec/execbase.h>
#include <exec/resident.h>
#include <exec/initializers.h>

#include <clib/dos_protos.h>

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

#if 0
struct DosLibrary * __saveds __g_lxa_dos_InitLib    ( register struct DosLibrary *dosb    __asm("a6"),
                                                      register BPTR               seglist __asm("a0"),
                                                      register struct ExecBase   *sysb    __asm("d0"));
struct DosLibrary * __saveds __g_lxa_dos_OpenLib    ( register struct DosLibrary *dosb    __asm("a6"));
BPTR                __saveds __g_lxa_dos_CloseLib   ( register struct DosLibrary *dosb    __asm("a6"));
BPTR                __saveds __g_lxa_dos_ExpungeLib ( register struct DosLibrary *dosb    __asm("a6"));
ULONG                        __g_lxa_dos_ExtFuncLib ( void );
#endif

struct DosLibrary * __saveds __g_lxa_dos_InitLib    ( register struct DosLibrary *dosb    __asm("a6"),
                                                      register BPTR               seglist __asm("a0"),
                                                      register struct ExecBase   *sysb    __asm("d0"))
{
    lprintf ("_dos: WARNING: InitLib() unimplemented STUB called.\n");
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
    lprintf ("_dos: OpenLib() called\n");
    DosLibrary->dl_lib.lib_OpenCnt++;
    DosLibrary->dl_lib.lib_Flags &= ~LIBF_DELEXP;
    return DosLibrary;
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

static BPTR __saveds _dos_Open ( register struct DosLibrary * __libBase __asm("a6"),
                                                        register CONST_STRPTR ___name  __asm("d1"),
                                                        register LONG ___accessMode  __asm("d2"))
{
    lprintf ("_dos: Open unimplemented STUB called.\n");
    assert(FALSE);
}

static LONG __saveds _dos_Close ( register struct DosLibrary * __libBase __asm("a6"),
                                                        register BPTR ___file  __asm("d1"))
{
    lprintf ("_dos: Close unimplemented STUB called.\n");
    assert(FALSE);
}

static LONG __saveds _dos_Read ( register struct DosLibrary * __libBase __asm("a6"),
                                                        register BPTR ___file  __asm("d1"),
                                                        register APTR ___buffer  __asm("d2"),
                                                        register LONG ___length  __asm("d3"))
{
    lprintf ("_dos: Read unimplemented STUB called.\n");
    assert(FALSE);
}

static LONG __saveds _dos_Write ( register struct DosLibrary * __libBase __asm("a6"),
                                                        register BPTR ___file  __asm("d1"),
                                                        register CONST APTR ___buffer  __asm("d2"),
                                                        register LONG ___length  __asm("d3"))
{
    lprintf ("_dos: Write unimplemented STUB called.\n");
    assert(FALSE);
}

static BPTR __saveds _dos_Input ( register struct DosLibrary * __libBase __asm("a6"))
{
    lprintf ("_dos: Input unimplemented STUB called.\n");
    assert(FALSE);
}

static BPTR __saveds _dos_Output ( register struct DosLibrary * __libBase __asm("a6"))
{
    lprintf ("_dos: Output unimplemented STUB called.\n");
    assert(FALSE);
}

static LONG __saveds _dos_Seek ( register struct DosLibrary * __libBase __asm("a6"),
                                                        register BPTR ___file  __asm("d1"),
                                                        register LONG ___position  __asm("d2"),
                                                        register LONG ___offset  __asm("d3"))
{
    lprintf ("_dos: Seek unimplemented STUB called.\n");
    assert(FALSE);
}

static LONG __saveds _dos_DeleteFile ( register struct DosLibrary * __libBase __asm("a6"),
                                                        register CONST_STRPTR ___name  __asm("d1"))
{
    lprintf ("_dos: DeleteFile unimplemented STUB called.\n");
    assert(FALSE);
}

static LONG __saveds _dos_Rename ( register struct DosLibrary * __libBase __asm("a6"),
                                                        register CONST_STRPTR ___oldName  __asm("d1"),
                                                        register CONST_STRPTR ___newName  __asm("d2"))
{
    lprintf ("_dos: Rename unimplemented STUB called.\n");
    assert(FALSE);
}

static BPTR __saveds _dos_Lock ( register struct DosLibrary * __libBase __asm("a6"),
                                                        register CONST_STRPTR ___name  __asm("d1"),
                                                        register LONG ___type  __asm("d2"))
{
    lprintf ("_dos: Lock unimplemented STUB called.\n");
    assert(FALSE);
}

static void __saveds _dos_UnLock ( register struct DosLibrary * __libBase __asm("a6"),
                                                        register BPTR ___lock  __asm("d1"))
{
    lprintf ("_dos: UnLock unimplemented STUB called.\n");
    assert(FALSE);
}

static BPTR __saveds _dos_DupLock ( register struct DosLibrary * __libBase __asm("a6"),
                                                        register BPTR ___lock  __asm("d1"))
{
    lprintf ("_dos: DupLock unimplemented STUB called.\n");
    assert(FALSE);
}

static LONG __saveds _dos_Examine ( register struct DosLibrary * __libBase __asm("a6"),
                                                        register BPTR ___lock  __asm("d1"),
                                                        register struct FileInfoBlock * ___fileInfoBlock  __asm("d2"))
{
    lprintf ("_dos: Examine unimplemented STUB called.\n");
    assert(FALSE);
}

static LONG __saveds _dos_ExNext ( register struct DosLibrary * __libBase __asm("a6"),
                                                        register BPTR ___lock  __asm("d1"),
                                                        register struct FileInfoBlock * ___fileInfoBlock  __asm("d2"))
{
    lprintf ("_dos: ExNext unimplemented STUB called.\n");
    assert(FALSE);
}

static LONG __saveds _dos_Info ( register struct DosLibrary * __libBase __asm("a6"),
                                                        register BPTR ___lock  __asm("d1"),
                                                        register struct InfoData * ___parameterBlock  __asm("d2"))
{
    lprintf ("_dos: Info unimplemented STUB called.\n");
    assert(FALSE);
}

static BPTR __saveds _dos_CreateDir ( register struct DosLibrary * __libBase __asm("a6"),
                                                        register CONST_STRPTR ___name  __asm("d1"))
{
    lprintf ("_dos: CreateDir unimplemented STUB called.\n");
    assert(FALSE);
}

static BPTR __saveds _dos_CurrentDir ( register struct DosLibrary * __libBase __asm("a6"),
                                                        register BPTR ___lock  __asm("d1"))
{
    lprintf ("_dos: CurrentDir unimplemented STUB called.\n");
    assert(FALSE);
}

static LONG __saveds _dos_IoErr ( register struct DosLibrary * __libBase __asm("a6"))
{
    lprintf ("_dos: IoErr unimplemented STUB called.\n");
    assert(FALSE);
}

static struct MsgPort * __saveds _dos_CreateProc ( register struct DosLibrary * __libBase __asm("a6"),
                                                        register CONST_STRPTR ___name  __asm("d1"),
                                                        register LONG ___pri  __asm("d2"),
                                                        register BPTR ___segList  __asm("d3"),
                                                        register LONG ___stackSize  __asm("d4"))
{
    lprintf ("_dos: CreateProc unimplemented STUB called.\n");
    assert(FALSE);
}

static void __saveds _dos_Exit ( register struct DosLibrary * __libBase __asm("a6"),
                                                        register LONG ___returnCode  __asm("d1"))
{
    lprintf ("_dos: Exit unimplemented STUB called.\n");
    assert(FALSE);
}

static BPTR __saveds _dos_LoadSeg ( register struct DosLibrary * __libBase __asm("a6"),
                                                        register CONST_STRPTR ___name  __asm("d1"))
{
    lprintf ("_dos: LoadSeg unimplemented STUB called.\n");
    assert(FALSE);
}

static void __saveds _dos_UnLoadSeg ( register struct DosLibrary * __libBase __asm("a6"),
                                                        register BPTR ___seglist  __asm("d1"))
{
    lprintf ("_dos: UnLoadSeg unimplemented STUB called.\n");
    assert(FALSE);
}

static struct MsgPort * __saveds _dos_DeviceProc ( register struct DosLibrary * __libBase __asm("a6"),
                                                        register CONST_STRPTR ___name  __asm("d1"))
{
    lprintf ("_dos: DeviceProc unimplemented STUB called.\n");
    assert(FALSE);
}

static LONG __saveds _dos_SetComment ( register struct DosLibrary * __libBase __asm("a6"),
                                                        register CONST_STRPTR ___name  __asm("d1"),
                                                        register CONST_STRPTR ___comment  __asm("d2"))
{
    lprintf ("_dos: SetComment unimplemented STUB called.\n");
    assert(FALSE);
}

static LONG __saveds _dos_SetProtection ( register struct DosLibrary * __libBase __asm("a6"),
                                                        register CONST_STRPTR ___name  __asm("d1"),
                                                        register LONG ___protect  __asm("d2"))
{
    lprintf ("_dos: SetProtection unimplemented STUB called.\n");
    assert(FALSE);
}

static struct DateStamp * __saveds _dos_DateStamp ( register struct DosLibrary * __libBase __asm("a6"),
                                                        register struct DateStamp * ___date  __asm("d1"))
{
    lprintf ("_dos: DateStamp unimplemented STUB called.\n");
    assert(FALSE);
}

static void __saveds _dos_Delay ( register struct DosLibrary * __libBase __asm("a6"),
                                                        register LONG ___timeout  __asm("d1"))
{
    lprintf ("_dos: Delay unimplemented STUB called.\n");
    assert(FALSE);
}

static LONG __saveds _dos_WaitForChar ( register struct DosLibrary * __libBase __asm("a6"),
                                                        register BPTR ___file  __asm("d1"),
                                                        register LONG ___timeout  __asm("d2"))
{
    lprintf ("_dos: WaitForChar unimplemented STUB called.\n");
    assert(FALSE);
}

static BPTR __saveds _dos_ParentDir ( register struct DosLibrary * __libBase __asm("a6"),
                                                        register BPTR ___lock  __asm("d1"))
{
    lprintf ("_dos: ParentDir unimplemented STUB called.\n");
    assert(FALSE);
}

static LONG __saveds _dos_IsInteractive ( register struct DosLibrary * __libBase __asm("a6"),
                                                        register BPTR ___file  __asm("d1"))
{
    lprintf ("_dos: IsInteractive unimplemented STUB called.\n");
    assert(FALSE);
}

static LONG __saveds _dos_Execute ( register struct DosLibrary * __libBase __asm("a6"),
                                                        register CONST_STRPTR ___string  __asm("d1"),
                                                        register BPTR ___file  __asm("d2"),
                                                        register BPTR ___file2  __asm("d3"))
{
    lprintf ("_dos: Execute unimplemented STUB called.\n");
    assert(FALSE);
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
    _dos_Open,
    _dos_Close,
    _dos_Read,
    _dos_Write,
    _dos_Input,
    _dos_Output,
    _dos_Seek,
    _dos_DeleteFile,
    _dos_Rename,
    _dos_Lock,
    _dos_UnLock,
    _dos_DupLock,
    _dos_Examine,
    _dos_ExNext,
    _dos_Info,
    _dos_CreateDir,
    _dos_CurrentDir,
    _dos_IoErr,
    _dos_CreateProc,
    _dos_Exit,
    _dos_LoadSeg,
    _dos_UnLoadSeg,
    _dos_DeviceProc,
    _dos_SetComment,
    _dos_SetProtection,
    _dos_DateStamp,
    _dos_Delay,
    _dos_WaitForChar,
    _dos_ParentDir,
    _dos_IsInteractive,
    _dos_Execute,

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

