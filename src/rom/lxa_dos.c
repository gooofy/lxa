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

#define ENABLE_DEBUG

#ifdef ENABLE_DEBUG
#define DPRINTF(lvl, ...) LPRINTF (lvl, __VA_ARGS__)
#else
#define DPRINTF(lvl, ...)
#endif

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

struct DosLibrary * __saveds __g_lxa_dos_OpenLib ( register struct DosLibrary  *DosLibrary __asm("a6"))
{
    DPRINTF (LOG_DEBUG, "_dos: OpenLib() called\n");
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

static LONG __saveds _dos_Close ( register struct DosLibrary * __libBase __asm("a6"),
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

static LONG __saveds _dos_Read ( register struct DosLibrary * __libBase __asm("a6"),
                                 register BPTR file  __asm("d1"),
                                 register APTR buffer  __asm("d2"),
                                 register LONG length  __asm("d3"))
{
    DPRINTF (LOG_DEBUG, "_dos: Read called: file=0x%08lx buffer=0x%08lx length=%ld\n", file, buffer, length);

    struct FileHandle *fh = (struct FileHandle *) BADDR(file);
    int l = emucall3 (EMU_CALL_DOS_READ, (ULONG) fh, (ULONG) buffer, length);

    DPRINTF (LOG_DEBUG, "_dos: Read() result from emucall3: l=%ld\n", l);

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
    DPRINTF (LOG_INFO, "_dos: Write unimplemented STUB called.\n");
    assert(FALSE);
}

static BPTR __saveds _dos_Input ( register struct DosLibrary * __libBase __asm("a6"))
{
    DPRINTF (LOG_DEBUG, "_dos: Input() called.\n");

    struct Process *p = (struct Process *) FindTask (NULL);

    BPTR i = p->pr_CIS;

    DPRINTF (LOG_DEBUG, "_dos: Input() p=0x%08lx, o=%ld\n", p, i);

    return i;
}

static BPTR __saveds _dos_Output ( register struct DosLibrary * __libBase __asm("a6"))
{
    DPRINTF (LOG_DEBUG, "_dos: Output() called.\n");

    struct Process *p = (struct Process *) FindTask (NULL);

    BPTR o = p->pr_COS;

    DPRINTF (LOG_DEBUG, "_dos: Output() p=0x%08lx, o=%ld\n", p, o);

    return o;
}

static LONG __saveds _dos_Seek ( register struct DosLibrary * __libBase __asm("a6"),
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

static LONG __saveds _dos_DeleteFile ( register struct DosLibrary * __libBase __asm("a6"),
                                                        register CONST_STRPTR ___name  __asm("d1"))
{
    DPRINTF (LOG_DEBUG, "_dos: DeleteFile unimplemented STUB called.\n");
    assert(FALSE);
}

static LONG __saveds _dos_Rename ( register struct DosLibrary * __libBase __asm("a6"),
                                                        register CONST_STRPTR ___oldName  __asm("d1"),
                                                        register CONST_STRPTR ___newName  __asm("d2"))
{
    DPRINTF (LOG_DEBUG, "_dos: Rename unimplemented STUB called.\n");
    assert(FALSE);
}

static BPTR __saveds _dos_Lock ( register struct DosLibrary * __libBase __asm("a6"),
                                                        register CONST_STRPTR ___name  __asm("d1"),
                                                        register LONG ___type  __asm("d2"))
{
    DPRINTF (LOG_DEBUG, "_dos: Lock unimplemented STUB called.\n");
    assert(FALSE);
}

static void __saveds _dos_UnLock ( register struct DosLibrary * __libBase __asm("a6"),
                                                        register BPTR ___lock  __asm("d1"))
{
    DPRINTF (LOG_DEBUG, "_dos: UnLock unimplemented STUB called.\n");
    assert(FALSE);
}

static BPTR __saveds _dos_DupLock ( register struct DosLibrary * __libBase __asm("a6"),
                                                        register BPTR ___lock  __asm("d1"))
{
    DPRINTF (LOG_DEBUG, "_dos: DupLock unimplemented STUB called.\n");
    assert(FALSE);
}

static LONG __saveds _dos_Examine ( register struct DosLibrary * __libBase __asm("a6"),
                                                        register BPTR ___lock  __asm("d1"),
                                                        register struct FileInfoBlock * ___fileInfoBlock  __asm("d2"))
{
    DPRINTF (LOG_DEBUG, "_dos: Examine unimplemented STUB called.\n");
    assert(FALSE);
}

static LONG __saveds _dos_ExNext ( register struct DosLibrary * __libBase __asm("a6"),
                                                        register BPTR ___lock  __asm("d1"),
                                                        register struct FileInfoBlock * ___fileInfoBlock  __asm("d2"))
{
    DPRINTF (LOG_DEBUG, "_dos: ExNext unimplemented STUB called.\n");
    assert(FALSE);
}

static LONG __saveds _dos_Info ( register struct DosLibrary * __libBase __asm("a6"),
                                                        register BPTR ___lock  __asm("d1"),
                                                        register struct InfoData * ___parameterBlock  __asm("d2"))
{
    DPRINTF (LOG_DEBUG, "_dos: Info unimplemented STUB called.\n");
    assert(FALSE);
}

static BPTR __saveds _dos_CreateDir ( register struct DosLibrary * __libBase __asm("a6"),
                                                        register CONST_STRPTR ___name  __asm("d1"))
{
    DPRINTF (LOG_DEBUG, "_dos: CreateDir unimplemented STUB called.\n");
    assert(FALSE);
}

static BPTR __saveds _dos_CurrentDir ( register struct DosLibrary * __libBase __asm("a6"),
                                                        register BPTR ___lock  __asm("d1"))
{
    DPRINTF (LOG_DEBUG, "_dos: CurrentDir unimplemented STUB called.\n");
    assert(FALSE);
}

static LONG __saveds _dos_IoErr ( register struct DosLibrary * DOSBase __asm("a6"))
{
    DPRINTF (LOG_DEBUG, "_dos: IoErr() called.\n");

    struct Process *me = (struct Process *)FindTask(NULL);

    return me->pr_Result2;
}

static struct MsgPort * __saveds _dos_CreateProc ( register struct DosLibrary * __libBase __asm("a6"),
                                                   register CONST_STRPTR ___name  __asm("d1"),
                                                   register LONG ___pri  __asm("d2"),
                                                   register BPTR ___segList  __asm("d3"),
                                                   register LONG ___stackSize  __asm("d4"))
{
    DPRINTF (LOG_DEBUG, "_dos: CreateProc unimplemented STUB called.\n");
    assert(FALSE);
}

static void __saveds _dos_Exit ( register struct DosLibrary * __libBase __asm("a6"),
                                                        register LONG ___returnCode  __asm("d1"))
{
    DPRINTF (LOG_DEBUG, "_dos: Exit unimplemented STUB called.\n");
    assert(FALSE);
}

static BPTR __saveds _dos_LoadSeg ( register struct DosLibrary * DOSBase __asm("a6"),
                                    register CONST_STRPTR        ___name   __asm("d1"))
{
    DPRINTF (LOG_INFO, "_dos: LoadSeg() called, name=%s\n", ___name);

    BOOL success = FALSE;

    BPTR f = Open (___name, MODE_OLDFILE);
    if (!f)
    {
        DPRINTF (LOG_INFO, "_dos: LoadSeg() Open() for name=%s failed\n", ___name);
        return 0;
    }

    DPRINTF (LOG_DEBUG, "_dos: LoadSeg() Open() for name=%s worked\n", ___name);

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

    ULONG hunk_prev = NULL;
    ULONG hunk_first = NULL;
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
                // skip
                ULONG cnt;
                while( (Read(f, &cnt, 4)==4) && cnt)
                {
                    if (Seek(f, (cnt+1)*4, OFFSET_CURRENT)<0)
						goto finish;
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

static void __saveds _dos_UnLoadSeg ( register struct DosLibrary * __libBase __asm("a6"),
                                                        register BPTR ___seglist  __asm("d1"))
{
    DPRINTF (LOG_DEBUG, "_dos: UnLoadSeg unimplemented STUB called.\n");
    assert(FALSE);
}

static VOID __saveds _dos_private0 ( register struct DosLibrary * DOSBase __asm("a6"))
{
    DPRINTF (LOG_ERROR, "_dos: private0() unimplemented STUB called.\n");
    assert(FALSE);
}

static VOID __saveds _dos_private1 ( register struct DosLibrary * DOSBase __asm("a6"))
{
    DPRINTF (LOG_ERROR, "_dos: private1() unimplemented STUB called.\n");
    assert(FALSE);
}

static void __saveds  _dos_ClearVec (register struct DosLibrary * __libBase __asm("a6"),
                                     register BPTR ___bVector  __asm("d1"),
                                     register BPTR ___upb      __asm("d2"))
{
    DPRINTF (LOG_DEBUG, "_dos: ClearVec() unimplemented *PRIVATE* STUB called.\n");
    assert(FALSE);
}

static void __saveds _dos_NoReqLoadSeg (register struct DosLibrary * __libBase __asm("a6"),
                                        register BPTR ___bFileName             __asm("d1"))
{
    DPRINTF (LOG_DEBUG, "_dos: NoReqLoadSeg() unimplemented *PRIVATE* STUB called.\n");
    assert(FALSE);
}

static struct MsgPort * __saveds _dos_DeviceProc ( register struct DosLibrary * __libBase __asm("a6"),
                                                        register CONST_STRPTR ___name  __asm("d1"))
{
    DPRINTF (LOG_DEBUG, "_dos: DeviceProc unimplemented STUB called.\n");
    assert(FALSE);
}

static LONG __saveds _dos_SetComment ( register struct DosLibrary * __libBase __asm("a6"),
                                                        register CONST_STRPTR ___name  __asm("d1"),
                                                        register CONST_STRPTR ___comment  __asm("d2"))
{
    DPRINTF (LOG_DEBUG, "_dos: SetComment unimplemented STUB called.\n");
    assert(FALSE);
}

static LONG __saveds _dos_SetProtection ( register struct DosLibrary * __libBase __asm("a6"),
                                                        register CONST_STRPTR ___name  __asm("d1"),
                                                        register LONG ___protect  __asm("d2"))
{
    DPRINTF (LOG_DEBUG, "_dos: SetProtection unimplemented STUB called.\n");
    assert(FALSE);
}

static struct DateStamp * __saveds _dos_DateStamp ( register struct DosLibrary * __libBase __asm("a6"),
                                                        register struct DateStamp * ___date  __asm("d1"))
{
    DPRINTF (LOG_DEBUG, "_dos: DateStamp unimplemented STUB called.\n");
    assert(FALSE);
}

static void __saveds _dos_Delay ( register struct DosLibrary * __libBase __asm("a6"),
                                                        register LONG ___timeout  __asm("d1"))
{
    DPRINTF (LOG_DEBUG, "_dos: Delay() unimplemented STUB called.\n");
    assert(FALSE);
}

static LONG __saveds _dos_WaitForChar ( register struct DosLibrary * __libBase __asm("a6"),
                                        register BPTR ___file  __asm("d1"),
                                        register LONG ___timeout  __asm("d2"))
{
    DPRINTF (LOG_DEBUG, "_dos: WaitForChar() unimplemented STUB called.\n");
    assert(FALSE);
}

static BPTR __saveds _dos_ParentDir ( register struct DosLibrary * __libBase __asm("a6"),
                                      register BPTR ___lock  __asm("d1"))
{
    DPRINTF (LOG_DEBUG, "_dos: ParentDir() unimplemented STUB called.\n");
    assert(FALSE);
}

static LONG __saveds _dos_IsInteractive ( register struct DosLibrary * __libBase __asm("a6"),
                                          register BPTR ___file  __asm("d1"))
{
    DPRINTF (LOG_DEBUG, "_dos: IsInteractive() unimplemented STUB called.\n");
    assert(FALSE);
}

static LONG __saveds _dos_Execute ( register struct DosLibrary * __libBase __asm("a6"),
                                    register CONST_STRPTR ___string  __asm("d1"),
                                    register BPTR ___file  __asm("d2"),
                                    register BPTR ___file2  __asm("d3"))
{
    DPRINTF (LOG_DEBUG, "_dos: Execute() unimplemented STUB called.\n");
    assert(FALSE);
}

static void __saveds *_dos_AllocDosObject (register struct DosLibrary * DOSBase __asm("a6"),
                                           register ULONG               ___type __asm("d1"),
                                           register struct TagItem    * ___tags __asm("d2"))
{
    DPRINTF (LOG_DEBUG, "_dos: AllocDosObject() called type=%ld\n", ___type);

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
            DPRINTF (LOG_DEBUG, "_dos: AllocDosObject() allocated new DOS_FILEHANDLE object: 0x%08lx\n", m);
            return m;
        }

        default:
            DPRINTF (LOG_DEBUG, "_dos: FIXME: AllocDosObject() type=%ld not implemented\n", ___type);
            assert (FALSE);
    }

    SetIoErr (ERROR_BAD_NUMBER);

    return NULL;
}

void __saveds _dos_FreeDosObject (register struct DosLibrary * DOSBase __asm("a6"),
                                  register ULONG               ___type __asm("d1"),
                                  register void              * ___ptr  __asm("d2"))
{
    DPRINTF (LOG_ERROR, "_dos: FreeDosObject() unimplemented STUB called.\n");
    assert (FALSE);
}

static LONG __saveds _dos_DoPkt ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register struct MsgPort * port __asm("d1"),
                                                        register LONG action __asm("d2"),
                                                        register LONG arg1 __asm("d3"),
                                                        register LONG arg2 __asm("d4"),
                                                        register LONG arg3 __asm("d5"),
                                                        register LONG arg4 __asm("d6"),
                                                        register LONG arg5 __asm("d7"))
{
    DPRINTF (LOG_ERROR, "_dos: DoPkt() unimplemented STUB called.\n");
    assert(FALSE);
}

static VOID __saveds _dos_SendPkt ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register struct DosPacket * dp __asm("d1"),
                                                        register struct MsgPort * port __asm("d2"),
                                                        register struct MsgPort * replyport __asm("d3"))
{
    DPRINTF (LOG_ERROR, "_dos: SendPkt() unimplemented STUB called.\n");
    assert(FALSE);
}

static struct DosPacket * __saveds _dos_WaitPkt ( register struct DosLibrary * DOSBase __asm("a6"))
{
    DPRINTF (LOG_ERROR, "_dos: WaitPkt() unimplemented STUB called.\n");
    assert(FALSE);
}

static VOID __saveds _dos_ReplyPkt ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register struct DosPacket * dp __asm("d1"),
                                                        register LONG res1 __asm("d2"),
                                                        register LONG res2 __asm("d3"))
{
    DPRINTF (LOG_ERROR, "_dos: ReplyPkt() unimplemented STUB called.\n");
    assert(FALSE);
}

static VOID __saveds _dos_AbortPkt ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register struct MsgPort * port __asm("d1"),
                                                        register struct DosPacket * pkt __asm("d2"))
{
    DPRINTF (LOG_ERROR, "_dos: AbortPkt() unimplemented STUB called.\n");
    assert(FALSE);
}

static BOOL __saveds _dos_LockRecord ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register BPTR fh __asm("d1"),
                                                        register ULONG offset __asm("d2"),
                                                        register ULONG length __asm("d3"),
                                                        register ULONG mode __asm("d4"),
                                                        register ULONG timeout __asm("d5"))
{
    DPRINTF (LOG_ERROR, "_dos: LockRecord() unimplemented STUB called.\n");
    assert(FALSE);
}

static BOOL __saveds _dos_LockRecords ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register struct RecordLock * recArray __asm("d1"),
                                                        register ULONG timeout __asm("d2"))
{
    DPRINTF (LOG_ERROR, "_dos: LockRecords() unimplemented STUB called.\n");
    assert(FALSE);
}

static BOOL __saveds _dos_UnLockRecord ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register BPTR fh __asm("d1"),
                                                        register ULONG offset __asm("d2"),
                                                        register ULONG length __asm("d3"))
{
    DPRINTF (LOG_ERROR, "_dos: UnLockRecord() unimplemented STUB called.\n");
    assert(FALSE);
}

static BOOL __saveds _dos_UnLockRecords ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register struct RecordLock * recArray __asm("d1"))
{
    DPRINTF (LOG_ERROR, "_dos: UnLockRecords() unimplemented STUB called.\n");
    assert(FALSE);
}

static BPTR __saveds _dos_SelectInput ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register BPTR fh __asm("d1"))
{
    DPRINTF (LOG_ERROR, "_dos: SelectInput() unimplemented STUB called.\n");
    assert(FALSE);
}

static BPTR __saveds _dos_SelectOutput ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register BPTR fh __asm("d1"))
{
    DPRINTF (LOG_ERROR, "_dos: SelectOutput() unimplemented STUB called.\n");
    assert(FALSE);
}

static LONG __saveds _dos_FGetC ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register BPTR fh __asm("d1"))
{
    DPRINTF (LOG_ERROR, "_dos: FGetC() unimplemented STUB called.\n");
    assert(FALSE);
}

static LONG __saveds _dos_FPutC ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register BPTR fh __asm("d1"),
                                                        register LONG ch __asm("d2"))
{
    DPRINTF (LOG_ERROR, "_dos: FPutC() unimplemented STUB called.\n");
    assert(FALSE);
}

static LONG __saveds _dos_UnGetC ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register BPTR fh __asm("d1"),
                                                        register LONG character __asm("d2"))
{
    DPRINTF (LOG_ERROR, "_dos: UnGetC() unimplemented STUB called.\n");
    assert(FALSE);
}

static LONG __saveds _dos_FRead ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register BPTR fh __asm("d1"),
                                                        register APTR block __asm("d2"),
                                                        register ULONG blocklen __asm("d3"),
                                                        register ULONG number __asm("d4"))
{
    DPRINTF (LOG_ERROR, "_dos: FRead() unimplemented STUB called.\n");
    assert(FALSE);
}

static LONG __saveds _dos_FWrite ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register BPTR fh __asm("d1"),
                                                        register const APTR block __asm("d2"),
                                                        register ULONG blocklen __asm("d3"),
                                                        register ULONG number __asm("d4"))
{
    DPRINTF (LOG_ERROR, "_dos: FWrite() unimplemented STUB called.\n");
    assert(FALSE);
}

static STRPTR __saveds _dos_FGets ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register BPTR fh __asm("d1"),
                                                        register STRPTR buf __asm("d2"),
                                                        register ULONG buflen __asm("d3"))
{
    DPRINTF (LOG_ERROR, "_dos: FGets() unimplemented STUB called.\n");
    assert(FALSE);
}

static LONG __saveds _dos_FPuts ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register BPTR fh __asm("d1"),
                                                        register CONST_STRPTR str __asm("d2"))
{
    DPRINTF (LOG_ERROR, "_dos: FPuts() unimplemented STUB called.\n");
    assert(FALSE);
}

static VOID __saveds _dos_VFWritef ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register BPTR fh __asm("d1"),
                                                        register CONST_STRPTR format __asm("d2"),
                                                        register const LONG * argarray __asm("d3"))
{
    DPRINTF (LOG_ERROR, "_dos: VFWritef() unimplemented STUB called.\n");
    assert(FALSE);
}

static LONG __saveds _dos_VFPrintf ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register BPTR fh __asm("d1"),
                                                        register CONST_STRPTR format __asm("d2"),
                                                        register const APTR argarray __asm("d3"))
{
    DPRINTF (LOG_ERROR, "_dos: VFPrintf() unimplemented STUB called.\n");
    assert(FALSE);
}

static LONG __saveds _dos_Flush ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register BPTR fh __asm("d1"))
{
    DPRINTF (LOG_ERROR, "_dos: Flush() unimplemented STUB called.\n");
    assert(FALSE);
}

static LONG __saveds _dos_SetVBuf ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register BPTR fh __asm("d1"),
                                                        register STRPTR buff __asm("d2"),
                                                        register LONG type __asm("d3"),
                                                        register LONG size __asm("d4"))
{
    DPRINTF (LOG_ERROR, "_dos: SetVBuf() unimplemented STUB called.\n");
    assert(FALSE);
}

static BPTR __saveds _dos_DupLockFromFH ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register BPTR fh __asm("d1"))
{
    DPRINTF (LOG_ERROR, "_dos: DupLockFromFH() unimplemented STUB called.\n");
    assert(FALSE);
}

static BPTR __saveds _dos_OpenFromLock ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register BPTR lock __asm("d1"))
{
    DPRINTF (LOG_ERROR, "_dos: OpenFromLock() unimplemented STUB called.\n");
    assert(FALSE);
}

static BPTR __saveds _dos_ParentOfFH ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register BPTR fh __asm("d1"))
{
    DPRINTF (LOG_ERROR, "_dos: ParentOfFH() unimplemented STUB called.\n");
    assert(FALSE);
}

static BOOL __saveds _dos_ExamineFH ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register BPTR fh __asm("d1"),
                                                        register struct FileInfoBlock * fib __asm("d2"))
{
    DPRINTF (LOG_ERROR, "_dos: ExamineFH() unimplemented STUB called.\n");
    assert(FALSE);
}

static LONG __saveds _dos_SetFileDate ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register CONST_STRPTR name __asm("d1"),
                                                        register const struct DateStamp * date __asm("d2"))
{
    DPRINTF (LOG_ERROR, "_dos: SetFileDate() unimplemented STUB called.\n");
    assert(FALSE);
}

static LONG __saveds _dos_NameFromLock ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register BPTR lock __asm("d1"),
                                                        register STRPTR buffer __asm("d2"),
                                                        register LONG len __asm("d3"))
{
    DPRINTF (LOG_ERROR, "_dos: NameFromLock() unimplemented STUB called.\n");
    assert(FALSE);
}

static LONG __saveds _dos_NameFromFH ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register BPTR fh __asm("d1"),
                                                        register STRPTR buffer __asm("d2"),
                                                        register LONG len __asm("d3"))
{
    DPRINTF (LOG_ERROR, "_dos: NameFromFH() unimplemented STUB called.\n");
    assert(FALSE);
}

static WORD __saveds _dos_SplitName ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register CONST_STRPTR name __asm("d1"),
                                                        register UBYTE separator __asm("d2"),
                                                        register STRPTR buf __asm("d3"),
                                                        register WORD oldpos __asm("d4"),
                                                        register LONG size __asm("d5"))
{
    DPRINTF (LOG_ERROR, "_dos: SplitName() unimplemented STUB called.\n");
    assert(FALSE);
}

static LONG __saveds _dos_SameLock ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register BPTR lock1 __asm("d1"),
                                                        register BPTR lock2 __asm("d2"))
{
    DPRINTF (LOG_ERROR, "_dos: SameLock() unimplemented STUB called.\n");
    assert(FALSE);
}

static LONG __saveds _dos_SetMode ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register BPTR fh __asm("d1"),
                                                        register LONG mode __asm("d2"))
{
    DPRINTF (LOG_ERROR, "_dos: SetMode() unimplemented STUB called.\n");
    assert(FALSE);
}

static LONG __saveds _dos_ExAll ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register BPTR lock __asm("d1"),
                                                        register struct ExAllData * buffer __asm("d2"),
                                                        register LONG size __asm("d3"),
                                                        register LONG data __asm("d4"),
                                                        register struct ExAllControl * control __asm("d5"))
{
    DPRINTF (LOG_ERROR, "_dos: ExAll() unimplemented STUB called.\n");
    assert(FALSE);
}

static LONG __saveds _dos_ReadLink ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register struct MsgPort * port __asm("d1"),
                                                        register BPTR lock __asm("d2"),
                                                        register CONST_STRPTR path __asm("d3"),
                                                        register STRPTR buffer __asm("d4"),
                                                        register ULONG size __asm("d5"))
{
    DPRINTF (LOG_ERROR, "_dos: ReadLink() unimplemented STUB called.\n");
    assert(FALSE);
}

static LONG __saveds _dos_MakeLink ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register CONST_STRPTR name __asm("d1"),
                                                        register LONG dest __asm("d2"),
                                                        register LONG soft __asm("d3"))
{
    DPRINTF (LOG_ERROR, "_dos: MakeLink() unimplemented STUB called.\n");
    assert(FALSE);
}

static LONG __saveds _dos_ChangeMode ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register LONG type __asm("d1"),
                                                        register BPTR fh __asm("d2"),
                                                        register LONG newmode __asm("d3"))
{
    DPRINTF (LOG_ERROR, "_dos: ChangeMode() unimplemented STUB called.\n");
    assert(FALSE);
}

static LONG __saveds _dos_SetFileSize ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register BPTR fh __asm("d1"),
                                                        register LONG pos __asm("d2"),
                                                        register LONG mode __asm("d3"))
{
    DPRINTF (LOG_ERROR, "_dos: SetFileSize() unimplemented STUB called.\n");
    assert(FALSE);
}

static LONG __saveds _dos_SetIoErr ( register struct DosLibrary * DOSBase __asm("a6"),
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

static BOOL __saveds _dos_Fault ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register LONG code __asm("d1"),
                                                        register STRPTR header __asm("d2"),
                                                        register STRPTR buffer __asm("d3"),
                                                        register LONG len __asm("d4"))
{
    DPRINTF (LOG_ERROR, "_dos: Fault() unimplemented STUB called.\n");
    assert(FALSE);
}

static BOOL __saveds _dos_PrintFault ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register LONG code __asm("d1"),
                                                        register CONST_STRPTR header __asm("d2"))
{
    DPRINTF (LOG_ERROR, "_dos: PrintFault() unimplemented STUB called.\n");
    assert(FALSE);
}

static LONG __saveds _dos_ErrorReport ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register LONG code __asm("d1"),
                                                        register LONG type __asm("d2"),
                                                        register ULONG arg1 __asm("d3"),
                                                        register struct MsgPort * device __asm("d4"))
{
    DPRINTF (LOG_ERROR, "_dos: ErrorReport() unimplemented STUB called.\n");
    assert(FALSE);
}

static VOID __saveds _dos_private2 ( register struct DosLibrary * DOSBase __asm("a6"))
{
    DPRINTF (LOG_ERROR, "_dos: private2() unimplemented STUB called.\n");
    assert(FALSE);
}

static struct CommandLineInterface * __saveds _dos_Cli ( register struct DosLibrary * DOSBase __asm("a6"))
{
    DPRINTF (LOG_ERROR, "_dos: Cli() unimplemented STUB called.\n");
    assert(FALSE);
}

static struct Process * __saveds _dos_CreateNewProc ( register struct DosLibrary * DOSBase __asm("a6"),
                                                      register const struct TagItem * tags __asm("d1"))
{
    DPRINTF (LOG_INFO, "_dos: CreateNewProc() called.\n");
    assert(FALSE);

#if 0

    struct newMemList  nml;
    struct MemList    *ml;
    struct Task       *newtask;
    APTR               task2;

    DPRINTF (LOG_DEBUG, "_exec: _createTask() called, name=%s, pri=%ld, initpc=0x%08lx, stacksize=%ld\n",
             name ? (char *) name : "NULL", pri, initpc, stacksize);


    stacksize = (stacksize+3)&~3;

    nml.nml_Node.ln_Succ = NULL;
    nml.nml_Node.ln_Pred = NULL;
    nml.nml_Node.ln_Type = NT_MEMORY;
    nml.nml_Node.ln_Pri  = 0;
    nml.nml_Node.ln_Name = NULL;

    nml.nml_NumEntries = 2;

    nml.nml_ME[0].me_Un.meu_Reqs = MEMF_CLEAR|MEMF_PUBLIC;
    nml.nml_ME[0].me_Length      = sizeof(struct Task);
    nml.nml_ME[1].me_Un.meu_Reqs = MEMF_CLEAR;
    nml.nml_ME[1].me_Length      = stacksize;

    ml = AllocEntry ((struct MemList *)&nml);
    if (!(((unsigned int)ml) & (1<<31)))
    {
        newtask = ml->ml_ME[0].me_Addr;
        newtask->tc_Node.ln_Type = NT_TASK;
        newtask->tc_Node.ln_Pri  = pri;
        newtask->tc_Node.ln_Name = name;
        newtask->tc_SPReg        = (APTR)((ULONG)ml->ml_ME[1].me_Addr+stacksize);
        newtask->tc_SPLower      = ml->ml_ME[1].me_Addr;
        newtask->tc_SPUpper      = newtask->tc_SPReg;

        DPRINTF (LOG_INFO, "_exec: _createTask() newtask->tc_SPReg=0x%08lx, newtask->tc_SPLower=0x%08lx, newtask->tc_SPUpper=0x%08lx, initPC=0x%08lx\n",
                 newtask->tc_SPReg, newtask->tc_SPLower, newtask->tc_SPUpper, initpc);

        NEWLIST (&newtask->tc_MemEntry);
        AddHead (&newtask->tc_MemEntry,&ml->ml_Node);
        task2 = AddTask (newtask, initpc, 0);
        if (!task2)
        {
            DPRINTF (LOG_ERROR, "_exec: _createTask() AddTask() failed\n");
            FreeEntry (ml);
            newtask = NULL;
        }
    }
    else
    {
        DPRINTF (LOG_ERROR, "_exec: _createTask() failed to allocate memory\n");
        newtask = NULL;
    }

    return newtask;
    #endif
}

static LONG __saveds _dos_RunCommand ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register BPTR seg __asm("d1"),
                                                        register LONG stack __asm("d2"),
                                                        register CONST_STRPTR paramptr __asm("d3"),
                                                        register LONG paramlen __asm("d4"))
{
    DPRINTF (LOG_ERROR, "_dos: RunCommand() unimplemented STUB called.\n");
    assert(FALSE);
}

static struct MsgPort * __saveds _dos_GetConsoleTask ( register struct DosLibrary * DOSBase __asm("a6"))
{
    DPRINTF (LOG_ERROR, "_dos: GetConsoleTask() unimplemented STUB called.\n");
    assert(FALSE);
}

static struct MsgPort * __saveds _dos_SetConsoleTask ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register const struct MsgPort * task __asm("d1"))
{
    DPRINTF (LOG_ERROR, "_dos: SetConsoleTask() unimplemented STUB called.\n");
    assert(FALSE);
}

static struct MsgPort * __saveds _dos_GetFileSysTask ( register struct DosLibrary * DOSBase __asm("a6"))
{
    DPRINTF (LOG_ERROR, "_dos: GetFileSysTask() unimplemented STUB called.\n");
    assert(FALSE);
}

static struct MsgPort * __saveds _dos_SetFileSysTask ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register const struct MsgPort * task __asm("d1"))
{
    DPRINTF (LOG_ERROR, "_dos: SetFileSysTask() unimplemented STUB called.\n");
    assert(FALSE);
}

static STRPTR __saveds _dos_GetArgStr ( register struct DosLibrary * DOSBase __asm("a6"))
{
    DPRINTF (LOG_ERROR, "_dos: GetArgStr() unimplemented STUB called.\n");
    assert(FALSE);
}

static BOOL __saveds _dos_SetArgStr ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register CONST_STRPTR string __asm("d1"))
{
    DPRINTF (LOG_ERROR, "_dos: SetArgStr() unimplemented STUB called.\n");
    assert(FALSE);
}

static struct Process * __saveds _dos_FindCliProc ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register ULONG num __asm("d1"))
{
    DPRINTF (LOG_ERROR, "_dos: FindCliProc() unimplemented STUB called.\n");
    assert(FALSE);
}

static ULONG __saveds _dos_MaxCli ( register struct DosLibrary * DOSBase __asm("a6"))
{
    DPRINTF (LOG_ERROR, "_dos: MaxCli() unimplemented STUB called.\n");
    assert(FALSE);
}

static BOOL __saveds _dos_SetCurrentDirName ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register CONST_STRPTR name __asm("d1"))
{
    DPRINTF (LOG_ERROR, "_dos: SetCurrentDirName() unimplemented STUB called.\n");
    assert(FALSE);
}

static BOOL __saveds _dos_GetCurrentDirName ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register STRPTR buf __asm("d1"),
                                                        register LONG len __asm("d2"))
{
    DPRINTF (LOG_ERROR, "_dos: GetCurrentDirName() unimplemented STUB called.\n");
    assert(FALSE);
}

static BOOL __saveds _dos_SetProgramName ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register CONST_STRPTR name __asm("d1"))
{
    DPRINTF (LOG_ERROR, "_dos: SetProgramName() unimplemented STUB called.\n");
    assert(FALSE);
}

static BOOL __saveds _dos_GetProgramName ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register STRPTR buf __asm("d1"),
                                                        register LONG len __asm("d2"))
{
    DPRINTF (LOG_ERROR, "_dos: GetProgramName() unimplemented STUB called.\n");
    assert(FALSE);
}

static BOOL __saveds _dos_SetPrompt ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register CONST_STRPTR name __asm("d1"))
{
    DPRINTF (LOG_ERROR, "_dos: SetPrompt() unimplemented STUB called.\n");
    assert(FALSE);
}

static BOOL __saveds _dos_GetPrompt ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register STRPTR buf __asm("d1"),
                                                        register LONG len __asm("d2"))
{
    DPRINTF (LOG_ERROR, "_dos: GetPrompt() unimplemented STUB called.\n");
    assert(FALSE);
}

static BPTR __saveds _dos_SetProgramDir ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register BPTR lock __asm("d1"))
{
    DPRINTF (LOG_ERROR, "_dos: SetProgramDir() unimplemented STUB called.\n");
    assert(FALSE);
}

static BPTR __saveds _dos_GetProgramDir ( register struct DosLibrary * DOSBase __asm("a6"))
{
    DPRINTF (LOG_ERROR, "_dos: GetProgramDir() unimplemented STUB called.\n");
    assert(FALSE);
}

static LONG __saveds _dos_SystemTagList ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register CONST_STRPTR command __asm("d1"),
                                                        register const struct TagItem * tags __asm("d2"))
{
    DPRINTF (LOG_ERROR, "_dos: SystemTagList() unimplemented STUB called.\n");
    assert(FALSE);
}

static LONG __saveds _dos_AssignLock ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register CONST_STRPTR name __asm("d1"),
                                                        register BPTR lock __asm("d2"))
{
    DPRINTF (LOG_ERROR, "_dos: AssignLock() unimplemented STUB called.\n");
    assert(FALSE);
}

static BOOL __saveds _dos_AssignLate ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register CONST_STRPTR name __asm("d1"),
                                                        register CONST_STRPTR path __asm("d2"))
{
    DPRINTF (LOG_ERROR, "_dos: AssignLate() unimplemented STUB called.\n");
    assert(FALSE);
}

static BOOL __saveds _dos_AssignPath ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register CONST_STRPTR name __asm("d1"),
                                                        register CONST_STRPTR path __asm("d2"))
{
    DPRINTF (LOG_ERROR, "_dos: AssignPath() unimplemented STUB called.\n");
    assert(FALSE);
}

static BOOL __saveds _dos_AssignAdd ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register CONST_STRPTR name __asm("d1"),
                                                        register BPTR lock __asm("d2"))
{
    DPRINTF (LOG_ERROR, "_dos: AssignAdd() unimplemented STUB called.\n");
    assert(FALSE);
}

static LONG __saveds _dos_RemAssignList ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register CONST_STRPTR name __asm("d1"),
                                                        register BPTR lock __asm("d2"))
{
    DPRINTF (LOG_ERROR, "_dos: RemAssignList() unimplemented STUB called.\n");
    assert(FALSE);
}

static struct DevProc * __saveds _dos_GetDeviceProc ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register CONST_STRPTR name __asm("d1"),
                                                        register struct DevProc * dp __asm("d2"))
{
    DPRINTF (LOG_ERROR, "_dos: GetDeviceProc() unimplemented STUB called.\n");
    assert(FALSE);
}

static VOID __saveds _dos_FreeDeviceProc ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register struct DevProc * dp __asm("d1"))
{
    DPRINTF (LOG_ERROR, "_dos: FreeDeviceProc() unimplemented STUB called.\n");
    assert(FALSE);
}

static struct DosList * __saveds _dos_LockDosList ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register ULONG flags __asm("d1"))
{
    DPRINTF (LOG_ERROR, "_dos: LockDosList() unimplemented STUB called.\n");
    assert(FALSE);
}

static VOID __saveds _dos_UnLockDosList ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register ULONG flags __asm("d1"))
{
    DPRINTF (LOG_ERROR, "_dos: UnLockDosList() unimplemented STUB called.\n");
    assert(FALSE);
}

static struct DosList * __saveds _dos_AttemptLockDosList ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register ULONG flags __asm("d1"))
{
    DPRINTF (LOG_ERROR, "_dos: AttemptLockDosList() unimplemented STUB called.\n");
    assert(FALSE);
}

static BOOL __saveds _dos_RemDosEntry ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register struct DosList * dlist __asm("d1"))
{
    DPRINTF (LOG_ERROR, "_dos: RemDosEntry() unimplemented STUB called.\n");
    assert(FALSE);
}

static LONG __saveds _dos_AddDosEntry ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register struct DosList * dlist __asm("d1"))
{
    DPRINTF (LOG_ERROR, "_dos: AddDosEntry() unimplemented STUB called.\n");
    assert(FALSE);
}

static struct DosList * __saveds _dos_FindDosEntry ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register const struct DosList * dlist __asm("d1"),
                                                        register CONST_STRPTR name __asm("d2"),
                                                        register ULONG flags __asm("d3"))
{
    DPRINTF (LOG_ERROR, "_dos: FindDosEntry() unimplemented STUB called.\n");
    assert(FALSE);
}

static struct DosList * __saveds _dos_NextDosEntry ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register const struct DosList * dlist __asm("d1"),
                                                        register ULONG flags __asm("d2"))
{
    DPRINTF (LOG_ERROR, "_dos: NextDosEntry() unimplemented STUB called.\n");
    assert(FALSE);
}

static struct DosList * __saveds _dos_MakeDosEntry ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register CONST_STRPTR name __asm("d1"),
                                                        register LONG type __asm("d2"))
{
    DPRINTF (LOG_ERROR, "_dos: MakeDosEntry() unimplemented STUB called.\n");
    assert(FALSE);
}

static VOID __saveds _dos_FreeDosEntry ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register struct DosList * dlist __asm("d1"))
{
    DPRINTF (LOG_ERROR, "_dos: FreeDosEntry() unimplemented STUB called.\n");
    assert(FALSE);
}

static BOOL __saveds _dos_IsFileSystem ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register CONST_STRPTR name __asm("d1"))
{
    DPRINTF (LOG_ERROR, "_dos: IsFileSystem() unimplemented STUB called.\n");
    assert(FALSE);
}

static BOOL __saveds _dos_Format ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register CONST_STRPTR filesystem __asm("d1"),
                                                        register CONST_STRPTR volumename __asm("d2"),
                                                        register ULONG dostype __asm("d3"))
{
    DPRINTF (LOG_ERROR, "_dos: Format() unimplemented STUB called.\n");
    assert(FALSE);
}

static LONG __saveds _dos_Relabel ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register CONST_STRPTR drive __asm("d1"),
                                                        register CONST_STRPTR newname __asm("d2"))
{
    DPRINTF (LOG_ERROR, "_dos: Relabel() unimplemented STUB called.\n");
    assert(FALSE);
}

static LONG __saveds _dos_Inhibit ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register CONST_STRPTR name __asm("d1"),
                                                        register LONG onoff __asm("d2"))
{
    DPRINTF (LOG_ERROR, "_dos: Inhibit() unimplemented STUB called.\n");
    assert(FALSE);
}

static LONG __saveds _dos_AddBuffers ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register CONST_STRPTR name __asm("d1"),
                                                        register LONG number __asm("d2"))
{
    DPRINTF (LOG_ERROR, "_dos: AddBuffers() unimplemented STUB called.\n");
    assert(FALSE);
}

static LONG __saveds _dos_CompareDates ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register const struct DateStamp * date1 __asm("d1"),
                                                        register const struct DateStamp * date2 __asm("d2"))
{
    DPRINTF (LOG_ERROR, "_dos: CompareDates() unimplemented STUB called.\n");
    assert(FALSE);
}

static LONG __saveds _dos_DateToStr ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register struct DateTime * datetime __asm("d1"))
{
    DPRINTF (LOG_ERROR, "_dos: DateToStr() unimplemented STUB called.\n");
    assert(FALSE);
}

static LONG __saveds _dos_StrToDate ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register struct DateTime * datetime __asm("d1"))
{
    DPRINTF (LOG_ERROR, "_dos: StrToDate() unimplemented STUB called.\n");
    assert(FALSE);
}

static BPTR __saveds _dos_InternalLoadSeg ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register BPTR fh __asm("d0"),
                                                        register BPTR table __asm("a0"),
                                                        register const LONG * funcarray __asm("a1"),
                                                        register LONG * stack __asm("a2"))
{
    DPRINTF (LOG_ERROR, "_dos: InternalLoadSeg() unimplemented STUB called.\n");
    assert(FALSE);
}

static BOOL __saveds _dos_InternalUnLoadSeg ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register BPTR seglist __asm("d1"),
                                                        register VOID (*freefunc)() __asm("a1"))
{
    DPRINTF (LOG_ERROR, "_dos: InternalUnLoadSeg() unimplemented STUB called.\n");
    assert(FALSE);
}

static BPTR __saveds _dos_NewLoadSeg ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register CONST_STRPTR file __asm("d1"),
                                                        register const struct TagItem * tags __asm("d2"))
{
    DPRINTF (LOG_ERROR, "_dos: NewLoadSeg() unimplemented STUB called.\n");
    assert(FALSE);
}

static LONG __saveds _dos_AddSegment ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register CONST_STRPTR name __asm("d1"),
                                                        register BPTR seg __asm("d2"),
                                                        register LONG system __asm("d3"))
{
    DPRINTF (LOG_ERROR, "_dos: AddSegment() unimplemented STUB called.\n");
    assert(FALSE);
}

static struct Segment * __saveds _dos_FindSegment ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register CONST_STRPTR name __asm("d1"),
                                                        register const struct Segment * seg __asm("d2"),
                                                        register LONG system __asm("d3"))
{
    DPRINTF (LOG_ERROR, "_dos: FindSegment() unimplemented STUB called.\n");
    assert(FALSE);
}

static LONG __saveds _dos_RemSegment ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register struct Segment * seg __asm("d1"))
{
    DPRINTF (LOG_ERROR, "_dos: RemSegment() unimplemented STUB called.\n");
    assert(FALSE);
}

static LONG __saveds _dos_CheckSignal ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register LONG mask __asm("d1"))
{
    DPRINTF (LOG_ERROR, "_dos: CheckSignal() unimplemented STUB called.\n");
    assert(FALSE);
}

static struct RDArgs * __saveds _dos_ReadArgs ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register CONST_STRPTR arg_template __asm("d1"),
                                                        register LONG * array __asm("d2"),
                                                        register struct RDArgs * args __asm("d3"))
{
    DPRINTF (LOG_ERROR, "_dos: ReadArgs() unimplemented STUB called.\n");
    assert(FALSE);
}

static LONG __saveds _dos_FindArg ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register CONST_STRPTR keyword __asm("d1"),
                                                        register CONST_STRPTR arg_template __asm("d2"))
{
    DPRINTF (LOG_ERROR, "_dos: FindArg() unimplemented STUB called.\n");
    assert(FALSE);
}

static LONG __saveds _dos_ReadItem ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register CONST_STRPTR name __asm("d1"),
                                                        register LONG maxchars __asm("d2"),
                                                        register struct CSource * cSource __asm("d3"))
{
    DPRINTF (LOG_ERROR, "_dos: ReadItem() unimplemented STUB called.\n");
    assert(FALSE);
}

static LONG __saveds _dos_StrToLong ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register CONST_STRPTR string __asm("d1"),
                                                        register LONG * value __asm("d2"))
{
    DPRINTF (LOG_ERROR, "_dos: StrToLong() unimplemented STUB called.\n");
    assert(FALSE);
}

static LONG __saveds _dos_MatchFirst ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register CONST_STRPTR pat __asm("d1"),
                                                        register struct AnchorPath * anchor __asm("d2"))
{
    DPRINTF (LOG_ERROR, "_dos: MatchFirst() unimplemented STUB called.\n");
    assert(FALSE);
}

static LONG __saveds _dos_MatchNext ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register struct AnchorPath * anchor __asm("d1"))
{
    DPRINTF (LOG_ERROR, "_dos: MatchNext() unimplemented STUB called.\n");
    assert(FALSE);
}

static VOID __saveds _dos_MatchEnd ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register struct AnchorPath * anchor __asm("d1"))
{
    DPRINTF (LOG_ERROR, "_dos: MatchEnd() unimplemented STUB called.\n");
    assert(FALSE);
}

static LONG __saveds _dos_ParsePattern ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register CONST_STRPTR pat __asm("d1"),
                                                        register STRPTR buf __asm("d2"),
                                                        register LONG buflen __asm("d3"))
{
    DPRINTF (LOG_ERROR, "_dos: ParsePattern() unimplemented STUB called.\n");
    assert(FALSE);
}

static BOOL __saveds _dos_MatchPattern ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register CONST_STRPTR pat __asm("d1"),
                                                        register STRPTR str __asm("d2"))
{
    DPRINTF (LOG_ERROR, "_dos: MatchPattern() unimplemented STUB called.\n");
    assert(FALSE);
}

static VOID __saveds _dos_private3 ( register struct DosLibrary * DOSBase __asm("a6"))
{
    DPRINTF (LOG_ERROR, "_dos: private3() unimplemented STUB called.\n");
    assert(FALSE);
}

static VOID __saveds _dos_FreeArgs ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register struct RDArgs * args __asm("d1"))
{
    DPRINTF (LOG_ERROR, "_dos: FreeArgs() unimplemented STUB called.\n");
    assert(FALSE);
}

static VOID __saveds _dos_private4 ( register struct DosLibrary * DOSBase __asm("a6"))
{
    DPRINTF (LOG_ERROR, "_dos: private4() unimplemented STUB called.\n");
    assert(FALSE);
}

static STRPTR __saveds _dos_FilePart ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register CONST_STRPTR path __asm("d1"))
{
    DPRINTF (LOG_ERROR, "_dos: FilePart() unimplemented STUB called.\n");
    assert(FALSE);
}

static STRPTR __saveds _dos_PathPart ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register CONST_STRPTR path __asm("d1"))
{
    DPRINTF (LOG_ERROR, "_dos: PathPart() unimplemented STUB called.\n");
    assert(FALSE);
}

static BOOL __saveds _dos_AddPart ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register STRPTR dirname __asm("d1"),
                                                        register CONST_STRPTR filename __asm("d2"),
                                                        register ULONG size __asm("d3"))
{
    DPRINTF (LOG_ERROR, "_dos: AddPart() unimplemented STUB called.\n");
    assert(FALSE);
}

static BOOL __saveds _dos_StartNotify ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register struct NotifyRequest * notify __asm("d1"))
{
    DPRINTF (LOG_ERROR, "_dos: StartNotify() unimplemented STUB called.\n");
    assert(FALSE);
}

static VOID __saveds _dos_EndNotify ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register struct NotifyRequest * notify __asm("d1"))
{
    DPRINTF (LOG_ERROR, "_dos: EndNotify() unimplemented STUB called.\n");
    assert(FALSE);
}

static BOOL __saveds _dos_SetVar ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register CONST_STRPTR name __asm("d1"),
                                                        register CONST_STRPTR buffer __asm("d2"),
                                                        register LONG size __asm("d3"),
                                                        register LONG flags __asm("d4"))
{
    DPRINTF (LOG_ERROR, "_dos: SetVar() unimplemented STUB called.\n");
    assert(FALSE);
}

static LONG __saveds _dos_GetVar ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register CONST_STRPTR name __asm("d1"),
                                                        register STRPTR buffer __asm("d2"),
                                                        register LONG size __asm("d3"),
                                                        register LONG flags __asm("d4"))
{
    DPRINTF (LOG_ERROR, "_dos: GetVar() unimplemented STUB called.\n");
    assert(FALSE);
}

static LONG __saveds _dos_DeleteVar ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register CONST_STRPTR name __asm("d1"),
                                                        register ULONG flags __asm("d2"))
{
    DPRINTF (LOG_ERROR, "_dos: DeleteVar() unimplemented STUB called.\n");
    assert(FALSE);
}

static struct LocalVar * __saveds _dos_FindVar ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register CONST_STRPTR name __asm("d1"),
                                                        register ULONG type __asm("d2"))
{
    DPRINTF (LOG_ERROR, "_dos: FindVar() unimplemented STUB called.\n");
    assert(FALSE);
}

static VOID __saveds _dos_private5 ( register struct DosLibrary * DOSBase __asm("a6"))
{
    DPRINTF (LOG_ERROR, "_dos: private5() unimplemented STUB called.\n");
    assert(FALSE);
}

static LONG __saveds _dos_CliInitNewcli ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register struct DosPacket * dp __asm("a0"))
{
    DPRINTF (LOG_ERROR, "_dos: CliInitNewcli() unimplemented STUB called.\n");
    assert(FALSE);
}

static LONG __saveds _dos_CliInitRun ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register struct DosPacket * dp __asm("a0"))
{
    DPRINTF (LOG_ERROR, "_dos: CliInitRun() unimplemented STUB called.\n");
    assert(FALSE);
}

static LONG __saveds _dos_WriteChars ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register CONST_STRPTR buf __asm("d1"),
                                                        register ULONG buflen __asm("d2"))
{
    DPRINTF (LOG_ERROR, "_dos: WriteChars() unimplemented STUB called.\n");
    assert(FALSE);
}

static LONG __saveds _dos_PutStr ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register CONST_STRPTR str __asm("d1"))
{
    DPRINTF (LOG_ERROR, "_dos: PutStr() unimplemented STUB called.\n");
    assert(FALSE);
}

static LONG __saveds _dos_VPrintf ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register CONST_STRPTR format __asm("d1"),
                                                        register const APTR argarray __asm("d2"))
{
    DPRINTF (LOG_ERROR, "_dos: VPrintf() unimplemented STUB called.\n");
    assert(FALSE);
}

static VOID __saveds _dos_private6 ( register struct DosLibrary * DOSBase __asm("a6"))
{
    DPRINTF (LOG_ERROR, "_dos: private6() unimplemented STUB called.\n");
    assert(FALSE);
}

static LONG __saveds _dos_ParsePatternNoCase ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register CONST_STRPTR pat __asm("d1"),
                                                        register UBYTE * buf __asm("d2"),
                                                        register LONG buflen __asm("d3"))
{
    DPRINTF (LOG_ERROR, "_dos: ParsePatternNoCase() unimplemented STUB called.\n");
    assert(FALSE);
}

static BOOL __saveds _dos_MatchPatternNoCase ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register CONST_STRPTR pat __asm("d1"),
                                                        register STRPTR str __asm("d2"))
{
    DPRINTF (LOG_ERROR, "_dos: MatchPatternNoCase() unimplemented STUB called.\n");
    assert(FALSE);
}

static VOID __saveds _dos_private7 ( register struct DosLibrary * DOSBase __asm("a6"))
{
    DPRINTF (LOG_ERROR, "_dos: private7() unimplemented STUB called.\n");
    assert(FALSE);
}

static BOOL __saveds _dos_SameDevice ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register BPTR lock1 __asm("d1"),
                                                        register BPTR lock2 __asm("d2"))
{
    DPRINTF (LOG_ERROR, "_dos: SameDevice() unimplemented STUB called.\n");
    assert(FALSE);
}

static VOID __saveds _dos_ExAllEnd ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register BPTR lock __asm("d1"),
                                                        register struct ExAllData * buffer __asm("d2"),
                                                        register LONG size __asm("d3"),
                                                        register LONG data __asm("d4"),
                                                        register struct ExAllControl * control __asm("d5"))
{
    DPRINTF (LOG_ERROR, "_dos: ExAllEnd() unimplemented STUB called.\n");
    assert(FALSE);
}

static BOOL __saveds _dos_SetOwner ( register struct DosLibrary * DOSBase __asm("a6"),
                                                        register CONST_STRPTR name __asm("d1"),
                                                        register LONG owner_info __asm("d2"))
{
    DPRINTF (LOG_ERROR, "_dos: SetOwner() unimplemented STUB called.\n");
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
{                               //                               offset
    RTC_MATCHWORD,              // UWORD rt_MatchWord;                0
    &ROMTag,                    // struct Resident *rt_MatchTag;      2
    &__g_lxa_dos_EndResident,   // APTR  rt_EndSkip;                  6
    RTF_AUTOINIT,               // UBYTE rt_Flags;                    7
    VERSION,                    // UBYTE rt_Version;                  8
    NT_LIBRARY,                 // UBYTE rt_Type;                     9
    0,                          // BYTE  rt_Pri;                     10
    &ExLibName[0],              // char  *rt_Name;                   14
    &ExLibID[0],                // char  *rt_IdString;               18
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
    /* ln_Name      */ 0x80, (UBYTE) (ULONG) OFFSET(Node,    ln_Name), (ULONG) &ExLibName[0],
    /* lib_Flags    */ INITBYTE(OFFSET(Library,      lib_Flags),    LIBF_SUMUSED|LIBF_CHANGED),
    /* lib_Version  */ INITWORD(OFFSET(Library,      lib_Version),  VERSION),
    /* lib_Revision */ INITWORD(OFFSET(Library,      lib_Revision), REVISION),
    /* lib_IdString */ 0x80, (UBYTE) (ULONG) OFFSET(Library, lib_IdString), (ULONG) &ExLibID[0],
    (ULONG) 0
};

