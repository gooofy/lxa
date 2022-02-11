#include <exec/types.h>
//#include <exec/memory.h>
//#include <exec/libraries.h>
#include <exec/execbase.h>
#include <exec/resident.h>
#include <exec/initializers.h>
#include <clib/exec_protos.h>
#include <inline/exec.h>

#include "dos/dos.h"
#include "dos/dosextens.h"
#include <clib/dos_protos.h>
#include <inline/dos.h>

#include "util.h"
#include "lxa_dos.h"

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

#define VERSION    1
#define REVISION   1
#define EXLIBNAME  "dos"
#define EXLIBVER   " 1.1 (2021/07/21)"

char __aligned ExLibName [] = EXLIBNAME ".library";
char __aligned ExLibID   [] = EXLIBNAME EXLIBVER;
char __aligned Copyright [] = "(C)opyright 2022 by G. Bartsch. Licensed under the MIT license.";

char __aligned VERSTRING [] = "\0$VER: " EXLIBNAME EXLIBVER;

extern struct ExecBase *SysBase;

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

static BPTR __saveds _dos_Open ( register struct DosLibrary * DOSBase        __asm("a6"),
                                 register CONST_STRPTR        ___name        __asm("d1"),
                                 register LONG                ___accessMode  __asm("d2"))
{
    lprintf ("_dos: Open() ___name=%s, ___accessMode=%ld\n", ___name ? (char *)___name : "NULL", ___accessMode);

    if (!___name) return 0;

    struct FileHandle *fh = (struct FileHandle *) AllocDosObject (DOS_FILEHANDLE, NULL);

    lprintf ("_dos: Open() fh=0x%08lx\n", fh);

    LONG error = ERROR_NO_FREE_STORE;
    if (fh)
    {

        int err = emucall3 (EMU_CALL_DOS_OPEN, (ULONG) ___name, ___accessMode, (ULONG) fh);

        lprintf ("_dos: Open() result from emucall3: err=%d, fh->Args=%d\n", err, fh->fh_Args);

        if (!err)
        {
            return MKBADDR (fh);
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

static LONG __saveds _dos_Close ( register struct DosLibrary * __libBase __asm("a6"),
                                                        register BPTR ___file  __asm("d1"))
{
    lprintf ("_dos: Close unimplemented STUB called.\n");
    assert(FALSE);
}

static LONG __saveds _dos_Read ( register struct DosLibrary * __libBase __asm("a6"),
                                 register BPTR file  __asm("d1"),
                                 register APTR buffer  __asm("d2"),
                                 register LONG length  __asm("d3"))
{
    lprintf ("_dos: Read called: file=0x%08lx buffer=0x%08lx length=%ld\n", file, buffer, length);

    struct FileHandle *fh = (struct FileHandle *) BADDR(file);
    int l = emucall3 (EMU_CALL_DOS_READ, (ULONG) fh, (ULONG) buffer, length);

    lprintf ("_dos: Read() result from emucall3: l=%ld\n", l);

    if (l<0)
    {
        struct Process *me = (struct Process *)FindTask(NULL);
        me->pr_Result2 = fh->fh_Arg2;
    }

    return l;
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
    lprintf ("_dos: Output() called.\n");

    struct Process *p = (struct Process *) FindTask (NULL);

    BPTR o = p->pr_COS;

    lprintf ("_dos: Output() p=0x%08lx, o=%ld\n", p, o);

    return o;
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

static LONG __saveds _dos_IoErr ( register struct DosLibrary * DOSBase __asm("a6"))
{
    lprintf ("_dos: IoErr() called.\n");

    struct Process *me = (struct Process *)FindTask(NULL);

    return me->pr_Result2;
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

static BPTR __saveds _dos_LoadSeg ( register struct DosLibrary * DOSBase __asm("a6"),
                                    register CONST_STRPTR        ___name   __asm("d1"))
{
    lprintf ("_dos: LoadSeg() called, name=%s\n", ___name);

    BPTR f=0, segs=0;

    f = Open (___name, MODE_OLDFILE);

    if (f)
    {
		lprintf ("_dos: LoadSeg() Open() for name=%s worked\n", ___name);

        // read hunk HEADER

        ULONG ht;
        if (Read (f, &ht, 4) != 4)
        {
            lprintf ("_dos: LoadSeg() failed to read header hunk type\n");
            goto fail;
        }
        lprintf ("_dos: LoadSeg() header hunk type: 0x%08lx\n", ht);

        if (ht != HUNK_TYPE_HEADER)
        {
            lprintf ("_dos: LoadSeg() invalid header hunk type\n");
            goto fail;
        }

        ULONG num_longs;
        if (Read (f, &num_longs, 4) != 4)
		{
            lprintf ("_dos: LoadSeg() failed to read hunk table size\n");
            goto fail;
		}
        if (num_longs)
        {
            // FIXME: implement
            lprintf ("_dos: LoadSeg() sorry, library names are not implemented yet\n");
            goto fail;
        }

        ULONG table_size;
        ULONG first_hunk_slot;
        ULONG last_hunk_slot;

        if (Read (f, &table_size, 4) != 4)
            goto fail;
        if (Read (f, &first_hunk_slot, 4) != 4)
            goto fail;
        if (Read (f, &last_hunk_slot, 4) != 4)
            goto fail;

        lprintf ("_dos: LoadSeg() reading hunk header, table_size=%d, first_hunk_slot=%d, last_hunk_slot=%d\n", table_size, first_hunk_slot, last_hunk_slot);

    	ULONG *hunk_table = AllocVec ((table_size + 2)*4, MEMF_CLEAR);
		if (!hunk_table)
			goto fail;

		ULONG prev_hunk = NULL;
		ULONG first_hunk = NULL;
        for (int i = first_hunk_slot; i <= last_hunk_slot; i++)
		{
			ULONG cnt;

            if (Read(f, &cnt, 4) != 4)
              goto fail;

			ULONG mem_flags = (cnt & 0xC0000000) >> 29;
			ULONG mem_size  = (cnt & 0x3FFFFFFF) * 4;

            ULONG req = MEMF_CLEAR | MEMF_PUBLIC;
            if (mem_flags == (MEMF_FAST | MEMF_CHIP))
			{
                if (Read(f, &req, 4) != 4)
                    goto fail;
            }
			else
			{
                req |= mem_flags;
            }

			mem_size += 8; // we also room for the hunk length and the next hunk pointer
            void *hunk_ptr = AllocVec (mem_size, req);
            if (!hunk_ptr)
				goto fail;
            hunk_table[i] = MKBADDR(hunk_ptr);

            lprintf ("_dos: LoadSeg() hunk %3d size=%d, flags=0x%08lx, req=0x%08lx -> 0x%08lx\n", i, mem_size, mem_flags, req, hunk_ptr);

			// link hunks
            if (!first_hunk)
                first_hunk = hunk_table[i];
            if (prev_hunk)
                ((BPTR *)(BADDR(prev_hunk)))[0] = hunk_table[i];
            prev_hunk = hunk_table[i];
        }

		// FIXME: read hunks
		assert(FALSE);

		// FIXME: relocate
		assert(FALSE);


fail:
        {
            LONG err = IoErr();

            if (!segs)
                lprintf ("_dos: LoadSeg() failed to load %s, err=%ld\n", ___name, err);

            Close (f);

            SetIoErr (err);
        }
    }

    return segs;
}

static void __saveds _dos_UnLoadSeg ( register struct DosLibrary * __libBase __asm("a6"),
                                                        register BPTR ___seglist  __asm("d1"))
{
    lprintf ("_dos: UnLoadSeg unimplemented STUB called.\n");
    assert(FALSE);
}

static void __saveds  _dos_ClearVec (register struct DosLibrary * __libBase __asm("a6"),
									 register BPTR ___bVector  __asm("d1"),
									 register BPTR ___upb      __asm("d2"))
{
    lprintf ("_dos: ClearVec() unimplemented *PRIVATE* STUB called.\n");
    assert(FALSE);
}

static void __saveds _dos_NoReqLoadSeg (register struct DosLibrary * __libBase __asm("a6"),
                                        register BPTR ___bFileName             __asm("d1"))
{
    lprintf ("_dos: NoReqLoadSeg() unimplemented *PRIVATE* STUB called.\n");
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
    lprintf ("_dos: Delay() unimplemented STUB called.\n");
    assert(FALSE);
}

static LONG __saveds _dos_WaitForChar ( register struct DosLibrary * __libBase __asm("a6"),
                                        register BPTR ___file  __asm("d1"),
                                        register LONG ___timeout  __asm("d2"))
{
    lprintf ("_dos: WaitForChar() unimplemented STUB called.\n");
    assert(FALSE);
}

static BPTR __saveds _dos_ParentDir ( register struct DosLibrary * __libBase __asm("a6"),
                                      register BPTR ___lock  __asm("d1"))
{
    lprintf ("_dos: ParentDir() unimplemented STUB called.\n");
    assert(FALSE);
}

static LONG __saveds _dos_IsInteractive ( register struct DosLibrary * __libBase __asm("a6"),
                                          register BPTR ___file  __asm("d1"))
{
    lprintf ("_dos: IsInteractive() unimplemented STUB called.\n");
    assert(FALSE);
}

static LONG __saveds _dos_Execute ( register struct DosLibrary * __libBase __asm("a6"),
                                    register CONST_STRPTR ___string  __asm("d1"),
                                    register BPTR ___file  __asm("d2"),
                                    register BPTR ___file2  __asm("d3"))
{
    lprintf ("_dos: Execute() unimplemented STUB called.\n");
    assert(FALSE);
}

static void __saveds *_dos_AllocDosObject (register struct DosLibrary * DOSBase __asm("a6"),
										   register ULONG               ___type __asm("d1"),
										   register struct TagItem    * ___tags __asm("d2"))
{
	lprintf ("_dos: AllocDosObject() called type=%ld\n", ___type);

    switch (___type)
    {
		case DOS_FILEHANDLE:
		{
			APTR m = AllocVec (sizeof(struct FileHandle), MEMF_CLEAR);

			if (m)
			{
				struct FileHandle *fh = (struct FileHandle *) m;

				// FIXME: initialize
				//fh->fh_Pos = -1;
				//fh->fh_End = -1;
			}
            lprintf ("_dos: AllocDosObject() allocated new DOS_FILEHANDLE object: 0x%08lx\n", m);
			return m;
		}

		default:
			lprintf ("_dos: FIXME: AllocDosObject() type=%ld not implemented\n", ___type);
			assert (FALSE);
    }

    SetIoErr (ERROR_BAD_NUMBER);

    return NULL;
}

void __saveds _dos_FreeDosObject (register struct DosLibrary * DOSBase __asm("a6"),
								  register ULONG               ___type __asm("d1"),
								  register void              * ___ptr  __asm("d2"))
{
	lprintf ("_dos: FreeDosObject() unimplemented STUB called.\n");
	assert (FALSE);
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
    _dos_ClearVec,
    _dos_NoReqLoadSeg,
    _dos_DeviceProc,
    _dos_SetComment,
    _dos_SetProtection,
    _dos_DateStamp,
    _dos_Delay,
    _dos_WaitForChar,
    _dos_ParentDir,
    _dos_IsInteractive,
    _dos_Execute,

	// V36

	_dos_AllocDosObject,
	_dos_FreeDosObject,

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

