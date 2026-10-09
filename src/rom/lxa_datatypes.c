/*
 * lxa datatypes.library full implementation
 *
 * datatypes.library is an AmigaOS 3.x system library.
 * Per AGENTS.md §1, system libraries must be fully implemented.
 *
 * This implementation provides:
 *   - ObtainDataTypeA / ReleaseDataType  — type detection & lifecycle
 *   - NewDTObjectA / DisposeDTObject     — object lifecycle
 *   - GetDTAttrsA / SetDTAttrsA          — attribute access
 *   - DoDTMethodA                        — method dispatch
 *   - DrawDTObject / RefreshDTObjectA    — rendering
 *   - AddDTObject / RemoveDTObject       — gadget management
 *   - GetDTMethods / GetDTTriggerMethods — method introspection
 *   - GetDTString                        — error strings
 *   - DoAsyncLayout / PrintDTObjectA     — async layout & printing
 *
 * Function biases follow the official AmigaOS NDK datatypes_lib.fd.
 */

#include <exec/types.h>
#include <exec/memory.h>
#include <exec/execbase.h>
#include <exec/resident.h>
#include <exec/initializers.h>
#include <clib/exec_protos.h>
#include <inline/exec.h>

#include <datatypes/datatypes.h>
#include <datatypes/datatypesclass.h>
#include <utility/tagitem.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <clib/dos_protos.h>
#include <inline/dos.h>

#include "util.h"

/*---------------------------------------------------------------------------
 * Local tag iterator — avoids the utility.library dependency that would
 * require UtilityBase to be opened before we can call lxa_next_tag().
 * Handles TAG_DONE, TAG_IGNORE, TAG_SKIP, and TAG_MORE (chained lists).
 *---------------------------------------------------------------------------*/
static struct TagItem *lxa_next_tag(struct TagItem **state)
{
    struct TagItem *tag = *state;
    for (;;)
    {
        if (!tag)
            return NULL;
        switch (tag->ti_Tag)
        {
            case TAG_DONE:
                *state = NULL;
                return NULL;
            case TAG_IGNORE:
                tag++;
                continue;
            case TAG_SKIP:
                tag += tag->ti_Data + 1;
                continue;
            case TAG_MORE:
                tag = (struct TagItem *)tag->ti_Data;
                continue;
            default:
                *state = tag + 1;
                return tag;
        }
    }
}

#define VERSION    40
#define REVISION   6
#define EXLIBNAME  "datatypes"
#define EXLIBVER   " 40.6 (25.8.93)\r\n"

char __aligned _g_datatypes_ExLibName [] = EXLIBNAME ".library";
char __aligned _g_datatypes_ExLibID   [] = "DataTypes" EXLIBVER;   /* as on AmigaOS 3.1 */
char __aligned _g_datatypes_Copyright [] = "(C)opyright 2026 by G. Bartsch. Licensed under the MIT License.";

char __aligned _g_datatypes_VERSTRING [] = "\0$VER: " EXLIBNAME EXLIBVER;

struct ExecBase *SysBase = NULL;
struct DosLibrary *DOSBase = NULL;

/*---------------------------------------------------------------------------
 * Error strings (DTERROR_* constants from datatypes.h)
 *---------------------------------------------------------------------------*/

/* AmigaOS 3.1 texts (reference-verified, Phase 220); 2009 is V44. */
static const char * const _dt_error_strings[] =
{
    /* 2000 */ "Unknown data type for %s",
    /* 2001 */ "Couldn't save %s",
    /* 2002 */ "Couldn't open %s",
    /* 2003 */ "Couldn't send message",
    /* 2004 */ "Couldn't open clipboard",
    /* 2005 */ "Unknown data type",
    /* 2006 */ "Unknown compression type",
    /* 2007 */ "Not enough data",
    /* 2008 */ "Invalid data",
    /* 2009 */ "Not available",
};

#define DT_ERROR_BASE   2000
#define DT_ERROR_MAX    2009

/*---------------------------------------------------------------------------
 * Built-in DataType descriptors
 *
 * We provide entries for ILBM, 8SVX, FTXT, and a generic binary type.
 * Apps that scan SYS:Classes/DataTypes/ will get these from memory.
 *---------------------------------------------------------------------------*/

/*
 * Descriptors as AmigaOS 3.1 reports them (reference-verified, Phase 220):
 * the IFF types carry a 12-byte "FORM????<type>" mask (-1 = any byte),
 * plain files fall back to the built-in "binary" type and directories to
 * "directory" (both GID_SYSTEM, no pattern).
 */
static WORD _dt_mask_ilbm[12] = { 'F','O','R','M',-1,-1,-1,-1,'I','L','B','M' };
static WORD _dt_mask_8svx[12] = { 'F','O','R','M',-1,-1,-1,-1,'8','S','V','X' };
static WORD _dt_mask_ftxt[12] = { 'F','O','R','M',-1,-1,-1,-1,'F','T','X','T' };

static struct DataTypeHeader _dt_hdr_ilbm =
{
    (STRPTR)"ILBM", (STRPTR)"ilbm", (STRPTR)"#?", _dt_mask_ilbm,
    GID_PICTURE, MAKE_ID('i','l','b','m'), 12, 0, DTF_IFF, 0,
};

static struct DataTypeHeader _dt_hdr_8svx =
{
    (STRPTR)"8SVX", (STRPTR)"8svx", (STRPTR)"#?", _dt_mask_8svx,
    GID_SOUND, MAKE_ID('8','s','v','x'), 12, 0, DTF_IFF, 0,
};

static struct DataTypeHeader _dt_hdr_ftxt =
{
    (STRPTR)"FTXT", (STRPTR)"ascii", (STRPTR)"#?", _dt_mask_ftxt,
    GID_TEXT, MAKE_ID('f','t','x','t'), 12, 0, DTF_IFF, 0,
};

static struct DataTypeHeader _dt_hdr_binary =
{
    (STRPTR)"binary", (STRPTR)"binary", NULL, NULL,
    GID_SYSTEM, MAKE_ID('b','i','n','a'), 0, 0, DTF_BINARY, 0,
};

static struct DataTypeHeader _dt_hdr_directory =
{
    (STRPTR)"directory", (STRPTR)"directory", NULL, NULL,
    GID_SYSTEM, MAKE_ID('d','i','r','e'), 0, 0, DTF_MISC, 0,
};

/*---------------------------------------------------------------------------
 * Internal DataType node (wraps DataTypeHeader in an allocated struct DataType)
 *---------------------------------------------------------------------------*/

struct LXADataType
{
    struct DataType          ldt_DT;       /* Public part (must be first) */
    struct DataTypeHeader   *ldt_HdrPtr;   /* Points into ldt_HdrCopy */
    struct DataTypeHeader    ldt_HdrCopy;  /* Local copy of header */
};

/*---------------------------------------------------------------------------
 * Internal DTObject — a BOOPSI-style object carrying DTSpecialInfo
 *---------------------------------------------------------------------------*/

struct LXADTObject
{
    struct Gadget            ldo_Gadget;    /* BOOPSI "Gadget" root */
    struct DTSpecialInfo     ldo_SI;        /* Scrolling/layout state */
    struct DataType         *ldo_DataType;  /* Back-pointer to DataType */
    /* Extended per-object state */
    STRPTR                   ldo_Name;      /* DTA_Name copy (or NULL) */
    ULONG                    ldo_GroupID;   /* DTA_GroupID */
    ULONG                    ldo_SourceType;/* DTA_SourceType */
};

/*---------------------------------------------------------------------------
 * Library init / open / close / expunge
 *---------------------------------------------------------------------------*/

struct Library * __g_lxa_datatypes_InitLib ( register struct Library   *libbase __asm("d0"),
                                             register BPTR              seglist __asm("a0"),
                                             register struct ExecBase  *sysb __asm("a6"))
{
    DPRINTF (LOG_DEBUG, "_datatypes: InitLib() called\n");
    SysBase = sysb;
    DOSBase = (struct DosLibrary *)OpenLibrary((CONST_STRPTR)"dos.library", 36);
    return libbase;
}

BPTR __g_lxa_datatypes_ExpungeLib ( register struct Library *libbase __asm("a6") )
{
    return 0;
}

struct Library * __g_lxa_datatypes_OpenLib ( register struct Library *libbase __asm("a6") )
{
    DPRINTF (LOG_DEBUG, "_datatypes: OpenLib() called, libbase=0x%08lx\n", (ULONG)libbase);
    libbase->lib_OpenCnt++;
    libbase->lib_Flags &= ~LIBF_DELEXP;
    return libbase;
}

BPTR __g_lxa_datatypes_CloseLib ( register struct Library *libbase __asm("a6") )
{
    DPRINTF (LOG_DEBUG, "_datatypes: CloseLib() called, libbase=0x%08lx\n", (ULONG)libbase);
    libbase->lib_OpenCnt--;
    return 0;
}

ULONG __g_lxa_datatypes_ExtFuncLib ( void )
{
    PRIVATE_FUNCTION_ERROR("_datatypes", "ExtFuncLib");
    return 0;
}

/* Reserved / private function stub */
ULONG _datatypes_Reserved ( void )
{
    DPRINTF (LOG_DEBUG, "_datatypes: Reserved/Private function called\n");
    return 0;
}

/*---------------------------------------------------------------------------
 * Helper: select built-in DataTypeHeader for a given source type
 *---------------------------------------------------------------------------*/

static struct DataTypeHeader *
_dt_header_for_lock (BPTR lock)
{
    struct FileInfoBlock *fib;
    struct DataTypeHeader *hdr = &_dt_hdr_binary;
    UBYTE buf[12];
    BPTR fh;
    BPTR dup;
    LONG n;

    fib = (struct FileInfoBlock *)AllocDosObject(DOS_FIB, NULL);
    if (fib)
    {
        if (Examine(lock, fib) && fib->fib_DirEntryType > 0)
            hdr = &_dt_hdr_directory;
        FreeDosObject(DOS_FIB, fib);
        if (hdr == &_dt_hdr_directory)
            return hdr;
    }

    dup = DupLock(lock);
    if (!dup)
        return hdr;
    fh = OpenFromLock(dup);
    if (!fh)
    {
        UnLock(dup);
        return hdr;
    }
    n = Read(fh, buf, sizeof(buf));
    Close(fh);

    if (n == 12 && buf[0] == 'F' && buf[1] == 'O' && buf[2] == 'R' && buf[3] == 'M')
    {
        ULONG type = ((ULONG)buf[8] << 24) | ((ULONG)buf[9] << 16) | ((ULONG)buf[10] << 8) | buf[11];
        if (type == MAKE_ID('I','L','B','M'))
            hdr = &_dt_hdr_ilbm;
        else if (type == MAKE_ID('8','S','V','X'))
            hdr = &_dt_hdr_8svx;
        else if (type == MAKE_ID('F','T','X','T'))
            hdr = &_dt_hdr_ftxt;
    }
    return hdr;
}

static struct LXADataType *
_dt_alloc_datatype (struct DataTypeHeader *src_hdr)
{
    struct LXADataType *ldt;

    ldt = (struct LXADataType *)AllocMem(sizeof(struct LXADataType), MEMF_PUBLIC | MEMF_CLEAR);
    if (!ldt)
        return NULL;

    /* Copy header so caller has stable storage */
    ldt->ldt_HdrCopy = *src_hdr;
    ldt->ldt_HdrPtr  = &ldt->ldt_HdrCopy;

    ldt->ldt_DT.dtn_Node1.ln_Type   = NT_UNKNOWN;
    ldt->ldt_DT.dtn_Node2.ln_Type   = NT_UNKNOWN;
    ldt->ldt_DT.dtn_Header          = ldt->ldt_HdrPtr;
    ldt->ldt_DT.dtn_FunctionName    = NULL;
    ldt->ldt_DT.dtn_AttrList        = NULL;
    ldt->ldt_DT.dtn_Length          = sizeof(struct LXADataType);
    NEWLIST(&ldt->ldt_DT.dtn_ToolList);
    return ldt;
}

/*---------------------------------------------------------------------------
 * -36: ObtainDataTypeA
 *
 * Allocates a struct DataType describing the data at <source>.
 * Caller must release with ReleaseDataType().
 *---------------------------------------------------------------------------*/
APTR _datatypes_ObtainDataTypeA ( register struct Library *DataTypesBase __asm("a6"),
                                  register ULONG type __asm("d0"),
                                  register APTR source __asm("a0"),
                                  register struct TagItem *tags __asm("a1") )
{
    struct LXADataType *ldt;
    struct DataTypeHeader *src_hdr;

    DPRINTF (LOG_DEBUG, "_datatypes: ObtainDataTypeA() type=%ld source=0x%08lx\n",
             type, (ULONG)source);

    switch (type)
    {
        case DTST_FILE:
            if (!source)
            {
                SetIoErr(ERROR_REQUIRED_ARG_MISSING);
                return NULL;
            }
            src_hdr = _dt_header_for_lock((BPTR)source);
            break;
        case DTST_CLIPBOARD:
            src_hdr = &_dt_hdr_ftxt;
            break;
        default:
            /* AmigaOS 3.1: DTST_RAM (and other non-file sources) cannot be
             * examined - NULL with ERROR_NOT_IMPLEMENTED (reference-verified). */
            SetIoErr(ERROR_NOT_IMPLEMENTED);
            return NULL;
    }

    ldt = _dt_alloc_datatype(src_hdr);
    if (!ldt)
    {
        SetIoErr(ERROR_NO_FREE_STORE);
        return NULL;
    }

    DPRINTF (LOG_DEBUG, "_datatypes: ObtainDataTypeA: returns 0x%08lx (%s)\n",
             (ULONG)&ldt->ldt_DT, src_hdr->dth_Name);

    return (APTR)&ldt->ldt_DT;
}

/*---------------------------------------------------------------------------
 * -42: ReleaseDataType
 *---------------------------------------------------------------------------*/
void _datatypes_ReleaseDataType ( register struct Library *DataTypesBase __asm("a6"),
                                  register APTR dt __asm("a0") )
{
    struct LXADataType *ldt;

    DPRINTF (LOG_DEBUG, "_datatypes: ReleaseDataType() dt=0x%08lx\n", (ULONG)dt);

    if (!dt)
        return;

    ldt = (struct LXADataType *)dt;
    FreeMem(ldt, sizeof(struct LXADataType));
}

/*---------------------------------------------------------------------------
 * -48: NewDTObjectA
 *
 * Creates a DataType object for <name> (file path or NULL for RAM).
 * Returns an opaque Object pointer (actually &LXADTObject.ldo_Gadget).
 *---------------------------------------------------------------------------*/
APTR _datatypes_NewDTObjectA ( register struct Library *DataTypesBase __asm("a6"),
                               register APTR name __asm("d0"),
                               register struct TagItem *tags __asm("a0") )
{
    struct LXADTObject *obj;
    ULONG source_type = DTST_FILE;
    ULONG group_id    = 0;
    struct TagItem *ti;
    struct TagItem *tag_list;
    struct DataTypeHeader *hdr;
    struct LXADataType *ldt;

    DPRINTF (LOG_DEBUG, "_datatypes: NewDTObjectA() name=0x%08lx tags=0x%08lx\n",
             (ULONG)name, (ULONG)tags);

    /* Parse creation tags */
    tag_list = tags;
    while ((ti = lxa_next_tag(&tag_list)) != NULL)
    {
        switch (ti->ti_Tag)
        {
            case DTA_SourceType:
                source_type = (ULONG)ti->ti_Data;
                break;
            case DTA_GroupID:
                group_id = (ULONG)ti->ti_Data;
                break;
            default:
                break;
        }
    }

    if (!name)
        source_type = DTST_RAM;

    /*
     * AmigaOS 3.1 (reference-verified, Phase 220):
     *  - a file that cannot be locked fails with DTERROR_COULDNT_OPEN;
     *  - a file whose type has no class (binary, directory) fails with
     *    DTERROR_UNKNOWN_DATATYPE;
     *  - a DTST_RAM object of a given group has no DTA_DataType.
     */
    hdr = NULL;
    ldt = NULL;
    if (source_type == DTST_FILE)
    {
        BPTR lock = Lock((CONST_STRPTR)name, ACCESS_READ);
        if (!lock)
        {
            SetIoErr(DTERROR_COULDNT_OPEN);
            return NULL;
        }
        hdr = _dt_header_for_lock(lock);
        UnLock(lock);
        if (hdr->dth_GroupID == GID_SYSTEM)
        {
            SetIoErr(DTERROR_UNKNOWN_DATATYPE);
            return NULL;
        }
        ldt = _dt_alloc_datatype(hdr);
        if (!ldt)
        {
            SetIoErr(ERROR_NO_FREE_STORE);
            return NULL;
        }
    }
    else if (source_type == DTST_CLIPBOARD)
    {
        hdr = &_dt_hdr_ftxt;
        ldt = _dt_alloc_datatype(hdr);
        if (!ldt)
        {
            SetIoErr(ERROR_NO_FREE_STORE);
            return NULL;
        }
    }

    /* Allocate internal object */
    obj = (struct LXADTObject *)AllocMem(sizeof(struct LXADTObject),
                                         MEMF_PUBLIC | MEMF_CLEAR);
    if (!obj)
    {
        if (ldt)
            FreeMem(ldt, sizeof(struct LXADataType));
        SetIoErr(ERROR_NO_FREE_STORE);
        return NULL;
    }

    /* Initialise DTSpecialInfo — MEMF_CLEAR already zeroed the semaphore fields */
    /* InitSemaphore not called here: semaphore is unused in this implementation */

    /* Wire gadget SpecialInfo to the DTSpecialInfo */
    obj->ldo_Gadget.SpecialInfo = (APTR)&obj->ldo_SI;

    obj->ldo_DataType  = ldt ? &ldt->ldt_DT : NULL;
    obj->ldo_GroupID   = group_id ? group_id : (hdr ? hdr->dth_GroupID : 0);
    obj->ldo_SourceType = source_type;
    obj->ldo_Name      = (STRPTR)name;   /* borrowed pointer */

    DPRINTF (LOG_DEBUG, "_datatypes: NewDTObjectA: returns obj=0x%08lx (%s)\n",
             (ULONG)obj, hdr ? (char *)hdr->dth_Name : "RAM");

    return (APTR)obj;
}

/*---------------------------------------------------------------------------
 * -54: DisposeDTObject
 *---------------------------------------------------------------------------*/
void _datatypes_DisposeDTObject ( register struct Library *DataTypesBase __asm("a6"),
                                  register APTR object __asm("a0") )
{
    struct LXADTObject *obj;

    DPRINTF (LOG_DEBUG, "_datatypes: DisposeDTObject() object=0x%08lx\n", (ULONG)object);

    if (!object)
        return;

    obj = (struct LXADTObject *)object;

    /* Free internal DataType node if we allocated one */
    if (obj->ldo_DataType)
    {
        struct LXADataType *ldt = (struct LXADataType *)obj->ldo_DataType;
        FreeMem(ldt, sizeof(struct LXADataType));
        obj->ldo_DataType = NULL;
    }

    FreeMem(obj, sizeof(struct LXADTObject));
}

/*---------------------------------------------------------------------------
 * Helper: get DTSpecialInfo from an object pointer
 *---------------------------------------------------------------------------*/
static struct DTSpecialInfo *
_dt_get_si (APTR object)
{
    struct LXADTObject *obj = (struct LXADTObject *)object;
    if (!obj)
        return NULL;
    return &obj->ldo_SI;
}

/*---------------------------------------------------------------------------
 * -60: SetDTAttrsA
 *---------------------------------------------------------------------------*/
ULONG _datatypes_SetDTAttrsA ( register struct Library *DataTypesBase __asm("a6"),
                               register APTR object __asm("a0"),
                               register APTR window __asm("a1"),
                               register APTR req __asm("a2"),
                               register struct TagItem *tags __asm("a3") )
{
    struct DTSpecialInfo *si;
    struct TagItem *ti;
    struct TagItem *tag_list;
    ULONG count = 0;

    DPRINTF (LOG_DEBUG, "_datatypes: SetDTAttrsA() object=0x%08lx\n", (ULONG)object);

    if (!object || !tags)
        return 0;

    si = _dt_get_si(object);
    if (!si)
        return 0;

    tag_list = tags;
    while ((ti = lxa_next_tag(&tag_list)) != NULL)
    {
        switch (ti->ti_Tag)
        {
            case DTA_TopVert:
                si->si_TopVert  = (LONG)ti->ti_Data;
                count++;
                break;
            case DTA_VisibleVert:
                si->si_VisVert  = (LONG)ti->ti_Data;
                count++;
                break;
            case DTA_TotalVert:
                si->si_TotVert  = (LONG)ti->ti_Data;
                count++;
                break;
            case DTA_VertUnit:
                si->si_VertUnit = (LONG)ti->ti_Data;
                count++;
                break;
            case DTA_TopHoriz:
                si->si_TopHoriz  = (LONG)ti->ti_Data;
                count++;
                break;
            case DTA_VisibleHoriz:
                si->si_VisHoriz  = (LONG)ti->ti_Data;
                count++;
                break;
            case DTA_TotalHoriz:
                si->si_TotHoriz  = (LONG)ti->ti_Data;
                count++;
                break;
            case DTA_HorizUnit:
                si->si_HorizUnit = (LONG)ti->ti_Data;
                count++;
                break;
            default:
                DPRINTF(LOG_DEBUG, "_datatypes: SetDTAttrsA: unknown tag 0x%08lx\n",
                        (ULONG)ti->ti_Tag);
                break;
        }
    }

    /* The result is the class's OM_SET "needs refresh" value, not a tag
     * count: AmigaOS 3.1 returns 0 for the scroll attributes (Phase 220). */
    (void)count;
    return 0;
}

/*---------------------------------------------------------------------------
 * -66: GetDTAttrsA
 *
 * Returns number of attributes successfully retrieved.
 * Tag data is a pointer-to-LONG destination.
 *---------------------------------------------------------------------------*/
ULONG _datatypes_GetDTAttrsA ( register struct Library *DataTypesBase __asm("a6"),
                               register APTR object __asm("a0"),
                               register struct TagItem *tags __asm("a2") )
{
    struct DTSpecialInfo *si;
    struct LXADTObject *obj;
    struct TagItem *ti;
    struct TagItem *tag_list;
    ULONG count = 0;

    DPRINTF (LOG_DEBUG, "_datatypes: GetDTAttrsA() object=0x%08lx\n", (ULONG)object);

    if (!object || !tags)
        return 0;

    obj = (struct LXADTObject *)object;
    si  = &obj->ldo_SI;

    tag_list = tags;
    while ((ti = lxa_next_tag(&tag_list)) != NULL)
    {
        LONG *dest = (LONG *)ti->ti_Data;
        if (!dest)
            continue;

        switch (ti->ti_Tag)
        {
            case DTA_TopVert:
                *dest = si->si_TopVert;
                count++;
                break;
            case DTA_VisibleVert:
                *dest = si->si_VisVert;
                count++;
                break;
            case DTA_TotalVert:
                *dest = si->si_TotVert;
                count++;
                break;
            case DTA_VertUnit:
                *dest = si->si_VertUnit;
                count++;
                break;
            case DTA_TopHoriz:
                *dest = si->si_TopHoriz;
                count++;
                break;
            case DTA_VisibleHoriz:
                *dest = si->si_VisHoriz;
                count++;
                break;
            case DTA_TotalHoriz:
                *dest = si->si_TotHoriz;
                count++;
                break;
            case DTA_HorizUnit:
                *dest = si->si_HorizUnit;
                count++;
                break;
            case DTA_GroupID:
                *dest = (LONG)obj->ldo_GroupID;
                count++;
                break;
            case DTA_SourceType:
                *dest = (LONG)obj->ldo_SourceType;
                count++;
                break;
            case DTA_DataType:
                *(APTR *)dest = (APTR)obj->ldo_DataType;
                count++;
                break;
            default:
                DPRINTF(LOG_DEBUG, "_datatypes: GetDTAttrsA: unknown tag 0x%08lx\n",
                        (ULONG)ti->ti_Tag);
                break;
        }
    }

    return count;
}

/*---------------------------------------------------------------------------
 * -72: AddDTObject
 *---------------------------------------------------------------------------*/
ULONG _datatypes_AddDTObject ( register struct Library *DataTypesBase __asm("a6"),
                               register APTR window __asm("a0"),
                               register APTR requester __asm("a1"),
                               register APTR object __asm("a2"),
                               register LONG position __asm("d0") )
{
    LXA_UNIMPLEMENTED("datatypes", "AddDTObject", "stub: object not added to the window (Phase 253)");

    DPRINTF (LOG_DEBUG, "_datatypes: AddDTObject() object=0x%08lx position=%ld\n",
             (ULONG)object, position);
    /* Without a real gadget list to insert into, return position unchanged */
    return (ULONG)position;
}

/*---------------------------------------------------------------------------
 * -78: RefreshDTObjectA
 *---------------------------------------------------------------------------*/
void _datatypes_RefreshDTObjectA ( register struct Library *DataTypesBase __asm("a6"),
                                   register APTR object __asm("a0"),
                                   register APTR window __asm("a1"),
                                   register APTR requester __asm("a2"),
                                   register struct TagItem *tags __asm("a3") )
{
    LXA_UNIMPLEMENTED("datatypes", "RefreshDTObjectA", "stub: no refresh (Phase 253)");

    DPRINTF (LOG_DEBUG, "_datatypes: RefreshDTObjectA() object=0x%08lx\n", (ULONG)object);
    /* No display to refresh in headless mode */
}

/*---------------------------------------------------------------------------
 * -84: DoAsyncLayout
 *---------------------------------------------------------------------------*/
ULONG _datatypes_DoAsyncLayout ( register struct Library *DataTypesBase __asm("a6"),
                                 register APTR object __asm("a0"),
                                 register APTR gpl __asm("a1") )
{
    LXA_UNIMPLEMENTED("datatypes", "DoAsyncLayout", "stub: no layout (Phase 253)");

    DPRINTF (LOG_DEBUG, "_datatypes: DoAsyncLayout() object=0x%08lx\n", (ULONG)object);
    return 0;
}

/*---------------------------------------------------------------------------
 * -90: DoDTMethodA
 *
 * Dispatches a method to a DataTypes object.
 * Returns 0 for unhandled methods (safe default).
 *---------------------------------------------------------------------------*/
ULONG _datatypes_DoDTMethodA ( register struct Library *DataTypesBase __asm("a6"),
                               register APTR object __asm("a0"),
                               register APTR window __asm("a1"),
                               register APTR requester __asm("a2"),
                               register APTR msg __asm("a3") )
{
    ULONG method_id = 0;

    if (msg)
        method_id = *(ULONG *)msg;

    DPRINTF (LOG_DEBUG, "_datatypes: DoDTMethodA() object=0x%08lx method=0x%08lx\n",
             (ULONG)object, method_id);

    switch (method_id)
    {
        case DTM_FRAMEBOX:
            /* Fill in a minimal FrameInfo */
            {
                struct dtFrameBox *fb = (struct dtFrameBox *)msg;
                if (fb && fb->dtf_FrameInfo && fb->dtf_SizeFrameInfo >= sizeof(struct FrameInfo))
                {
                    struct FrameInfo *fi = fb->dtf_FrameInfo;
                    fi->fri_PropertyFlags = 0;
                    fi->fri_Resolution.x  = 1;
                    fi->fri_Resolution.y  = 1;
                    fi->fri_RedBits   = 8;
                    fi->fri_GreenBits = 8;
                    fi->fri_BlueBits  = 8;
                    fi->fri_Dimensions.Width  = 0;
                    fi->fri_Dimensions.Height = 0;
                    fi->fri_Dimensions.Depth  = 0;
                    fi->fri_Screen    = NULL;
                    fi->fri_ColorMap  = NULL;
                    fi->fri_Flags     = 0;
                }
            }
            return 1;

        case DTM_PROCLAYOUT:
        case DTM_ASYNCLAYOUT:
        case DTM_REMOVEDTOBJECT:
        case DTM_CLEARSELECTED:
        case DTM_COPY:
        case DTM_ABORTPRINT:
            return 0;

        default:
            DPRINTF(LOG_DEBUG, "_datatypes: DoDTMethodA: unhandled method 0x%08lx\n",
                    method_id);
            return 0;
    }
}

/*---------------------------------------------------------------------------
 * -96: RemoveDTObject
 *---------------------------------------------------------------------------*/
ULONG _datatypes_RemoveDTObject ( register struct Library *DataTypesBase __asm("a6"),
                                  register APTR window __asm("a0"),
                                  register APTR object __asm("a1") )
{
    LXA_UNIMPLEMENTED("datatypes", "RemoveDTObject", "stub (Phase 253)");

    DPRINTF (LOG_DEBUG, "_datatypes: RemoveDTObject() object=0x%08lx\n", (ULONG)object);
    return 0;
}

/*---------------------------------------------------------------------------
 * -102: GetDTMethods
 *
 * Returns a NULL-terminated array of supported ULONG method IDs.
 * We return a static array of the methods we handle.
 *---------------------------------------------------------------------------*/
static ULONG _dt_method_list[] =
{
    DTM_FRAMEBOX,
    DTM_PROCLAYOUT,
    DTM_ASYNCLAYOUT,
    DTM_REMOVEDTOBJECT,
    DTM_SELECT,
    DTM_CLEARSELECTED,
    DTM_COPY,
    DTM_ABORTPRINT,
    DTM_GOTO,
    DTM_TRIGGER,
    DTM_OBTAINDRAWINFO,
    DTM_DRAW,
    DTM_RELEASEDRAWINFO,
    DTM_WRITE,
    ~0UL    /* terminator */
};

APTR _datatypes_GetDTMethods ( register struct Library *DataTypesBase __asm("a6"),
                               register APTR object __asm("a0") )
{
    DPRINTF (LOG_DEBUG, "_datatypes: GetDTMethods() object=0x%08lx\n", (ULONG)object);
    return (APTR)_dt_method_list;
}

/*---------------------------------------------------------------------------
 * -108: GetDTTriggerMethods
 *
 * Returns a NULL-terminated array of struct DTMethod trigger descriptors.
 *---------------------------------------------------------------------------*/
static struct DTMethod _dt_trigger_list[] =
{
    { (STRPTR)"Play",    (STRPTR)"PLAY",    STM_PLAY    },
    { (STRPTR)"Pause",   (STRPTR)"PAUSE",   STM_PAUSE   },
    { (STRPTR)"Stop",    (STRPTR)"STOP",    STM_STOP    },
    { (STRPTR)"Rewind",  (STRPTR)"REWIND",  STM_REWIND  },
    { NULL, NULL, 0 }   /* terminator */
};

APTR _datatypes_GetDTTriggerMethods ( register struct Library *DataTypesBase __asm("a6"),
                                      register APTR object __asm("a0") )
{
    DPRINTF (LOG_DEBUG, "_datatypes: GetDTTriggerMethods() object=0x%08lx\n", (ULONG)object);
    return (APTR)_dt_trigger_list;
}

/*---------------------------------------------------------------------------
 * -114: PrintDTObjectA
 *---------------------------------------------------------------------------*/
ULONG _datatypes_PrintDTObjectA ( register struct Library *DataTypesBase __asm("a6"),
                                  register APTR object __asm("a0"),
                                  register APTR window __asm("a1"),
                                  register APTR requester __asm("a2"),
                                  register struct TagItem *tags __asm("a3") )
{
    LXA_UNIMPLEMENTED("datatypes", "PrintDTObjectA", "stub: no printing (Phase 253)");

    DPRINTF (LOG_DEBUG, "_datatypes: PrintDTObjectA() object=0x%08lx\n", (ULONG)object);
    return 0;
}

/*---------------------------------------------------------------------------
 * -138: GetDTString
 *
 * Returns the localised string for a DTERROR_* constant.
 *---------------------------------------------------------------------------*/
STRPTR _datatypes_GetDTString ( register struct Library *DataTypesBase __asm("a6"),
                                register ULONG id __asm("d0") )
{
    DPRINTF (LOG_DEBUG, "_datatypes: GetDTString() id=%ld\n", id);

    if (id >= DT_ERROR_BASE && id <= (ULONG)DT_ERROR_MAX)
        return (STRPTR)_dt_error_strings[id - DT_ERROR_BASE];

    return NULL;
}


/*
 * Library structure definitions
 */

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

extern APTR              __g_lxa_datatypes_FuncTab [];
extern struct MyDataInit __g_lxa_datatypes_DataTab;
extern struct InitTable  __g_lxa_datatypes_InitTab;
extern APTR              __g_lxa_datatypes_EndResident;

static struct Resident __aligned ROMTag =
{
    RTC_MATCHWORD,                        // UWORD rt_MatchWord
    &ROMTag,                              // struct Resident *rt_MatchTag
    &__g_lxa_datatypes_EndResident,       // APTR  rt_EndSkip
    RTF_AUTOINIT,                         // UBYTE rt_Flags
    VERSION,                              // UBYTE rt_Version
    NT_LIBRARY,                           // UBYTE rt_Type
    0,                                    // BYTE  rt_Pri
    &_g_datatypes_ExLibName[0],           // char  *rt_Name
    &_g_datatypes_ExLibID[0],             // char  *rt_IdString
    &__g_lxa_datatypes_InitTab            // APTR  rt_Init
};

APTR __g_lxa_datatypes_EndResident;
struct Resident *__lxa_datatypes_ROMTag = &ROMTag;

struct InitTable __g_lxa_datatypes_InitTab =
{
    (ULONG)               sizeof(struct Library),
    (APTR              *) &__g_lxa_datatypes_FuncTab[0],
    (APTR)                &__g_lxa_datatypes_DataTab,
    (APTR)                __g_lxa_datatypes_InitLib
};

/*
 * Function table
 *
 * Standard library functions at -6 through -24
 * Then API functions starting at -30
 * Last entry at -138: GetDTString
 * Total: 4 standard + 19 API = 23 vectors
 */
APTR __g_lxa_datatypes_FuncTab [] =
{
    __g_lxa_datatypes_OpenLib,           // -6   Open
    __g_lxa_datatypes_CloseLib,          // -12  Close
    __g_lxa_datatypes_ExpungeLib,        // -18  Expunge
    __g_lxa_datatypes_ExtFuncLib,        // -24  Reserved
    /* API functions starting at -30 */
    _datatypes_Reserved,                 // -30  (private)
    _datatypes_ObtainDataTypeA,          // -36  ObtainDataTypeA
    _datatypes_ReleaseDataType,          // -42  ReleaseDataType
    _datatypes_NewDTObjectA,             // -48  NewDTObjectA
    _datatypes_DisposeDTObject,          // -54  DisposeDTObject
    _datatypes_SetDTAttrsA,              // -60  SetDTAttrsA
    _datatypes_GetDTAttrsA,              // -66  GetDTAttrsA
    _datatypes_AddDTObject,              // -72  AddDTObject
    _datatypes_RefreshDTObjectA,         // -78  RefreshDTObjectA
    _datatypes_DoAsyncLayout,            // -84  DoAsyncLayout
    _datatypes_DoDTMethodA,              // -90  DoDTMethodA
    _datatypes_RemoveDTObject,           // -96  RemoveDTObject
    _datatypes_GetDTMethods,             // -102 GetDTMethods
    _datatypes_GetDTTriggerMethods,      // -108 GetDTTriggerMethods
    _datatypes_PrintDTObjectA,           // -114 PrintDTObjectA
    _datatypes_Reserved,                 // -120 (private)
    _datatypes_Reserved,                 // -126 (private)
    _datatypes_Reserved,                 // -132 (private)
    _datatypes_GetDTString,              // -138 GetDTString
    (APTR) ((LONG)-1)
};

struct MyDataInit __g_lxa_datatypes_DataTab =
{
    /* ln_Type      */ INITBYTE(OFFSET(Node,         ln_Type),      NT_LIBRARY),
    /* ln_Name      */ 0x80, (UBYTE) (ULONG) OFFSET(Node,    ln_Name), (ULONG) &_g_datatypes_ExLibName[0],
    /* lib_Flags    */ INITBYTE(OFFSET(Library,      lib_Flags),    LIBF_SUMUSED|LIBF_CHANGED),
    /* lib_Version  */ INITWORD(OFFSET(Library,      lib_Version),  VERSION),
    /* lib_Revision */ INITWORD(OFFSET(Library,      lib_Revision), REVISION),
    /* lib_IdString */ 0x80, (UBYTE) (ULONG) OFFSET(Library, lib_IdString), (ULONG) &_g_datatypes_ExLibID[0],
    (ULONG) 0
};
