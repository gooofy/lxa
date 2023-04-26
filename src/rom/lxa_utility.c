#include <exec/types.h>
//#include <exec/memory.h>
//#include <exec/libraries.h>
#include <exec/execbase.h>
#include <exec/resident.h>
#include <exec/initializers.h>
#include <clib/exec_protos.h>
#include <inline/exec.h>

#include <utility/utility.h>
#include <clib/utility_protos.h>
#include <inline/utility.h>

#include "util.h"

#define VERSION    40
#define REVISION   1
#define EXLIBNAME  "utility"
#define EXLIBVER   " 40.1 (2022/03/02)"

char __aligned _g_utility_ExLibName [] = EXLIBNAME ".library";
char __aligned _g_utility_ExLibID   [] = EXLIBNAME EXLIBVER;
char __aligned _g_utility_Copyright [] = "(C)opyright 2022 by G. Bartsch. Licensed under the MIT license.";

char __aligned _g_utility_VERSTRING [] = "\0$VER: " EXLIBNAME EXLIBVER;

extern struct ExecBase      *SysBase;

// libBase: UtilityBase
// baseType: struct UtilityBase *
// libname: utility.library

struct UtilityBase * __g_lxa_utility_InitLib    ( register struct UtilityBase *utilityb    __asm("a6"),
                                                      register BPTR               seglist __asm("a0"),
                                                      register struct ExecBase   *sysb    __asm("d0"))
{
    DPRINTF (LOG_DEBUG, "_utility: WARNING: InitLib() unimplemented STUB called.\n");
    return utilityb;
}

struct UtilityBase * __g_lxa_utility_OpenLib ( register struct UtilityBase  *UtilityBase __asm("a6"))
{
    DPRINTF (LOG_DEBUG, "_utility: OpenLib() called\n");
    // FIXME UtilityBase->dl_lib.lib_OpenCnt++;
    // FIXME UtilityBase->dl_lib.lib_Flags &= ~LIBF_DELEXP;
    return UtilityBase;
}

BPTR __g_lxa_utility_CloseLib ( register struct UtilityBase  *utilityb __asm("a6"))
{
    return NULL;
}

BPTR __g_lxa_utility_ExpungeLib ( register struct UtilityBase  *utilityb      __asm("a6"))
{
    return NULL;
}

ULONG __g_lxa_utility_ExtFuncLib(void)
{
    return NULL;
}

static struct TagItem * _utility_FindTagItem ( register struct UtilityBase   *UtilityBase __asm("a6"),
                                                        register Tag                   tagVal      __asm("d0"),
                                                        register const struct TagItem *tagList     __asm("a0"))
{
    DPRINTF (LOG_DEBUG, "_utility: FindTagItem() called.\n");

    struct TagItem *tstate = (struct TagItem *)tagList;
    struct TagItem *tag;

    while ((tag = NextTagItem (&tstate)))
    {
        if ((ULONG)tag->ti_Tag == (ULONG)tagVal)
            return tag;
    }

    return NULL;
}

static ULONG _utility_GetTagData ( register struct UtilityBase   *UtilityBase __asm("a6"),
                                            register Tag                   tagValue    __asm("d0"),
                                            register ULONG                 defaultVal  __asm("d1"),
                                            register const struct TagItem *tagList     __asm("a0"))
{
    DPRINTF (LOG_DEBUG, "_utility: GetTagData() called.\n");

    struct TagItem *tag = FindTagItem (tagValue, tagList);

    return tag ? tag->ti_Data : defaultVal;
}

static ULONG _utility_PackBoolTags ( register struct UtilityBase * UtilityBase __asm("a6"),
                                                        register ULONG initialFlags __asm("d0"),
                                                        register const struct TagItem * tagList __asm("a0"),
                                                        register const struct TagItem * boolMap __asm("a1"))
{
    DPRINTF (LOG_ERROR, "_utility: PackBoolTags() unimplemented STUB called.\n");
    assert(FALSE);
}

static struct TagItem * _utility_NextTagItem ( register struct UtilityBase *  UtilityBase __asm("a6"),
                                                        register struct TagItem     ** tagListPtr  __asm("a0"))
{
    DPRINTF (LOG_DEBUG, "_utility: NextTagItem() called.\n");

    if (!(*tagListPtr))
        return NULL;

    while (TRUE)
    {
        switch (((*tagListPtr)->ti_Tag))
        {
            case TAG_MORE:
                if (!((*tagListPtr) = (struct TagItem *)(*tagListPtr)->ti_Data))
                    return NULL;
                continue;

            case TAG_END:
                (*tagListPtr) = NULL;
                return NULL;

            case TAG_SKIP:
                (*tagListPtr) += (*tagListPtr)->ti_Data + 1;
                continue;

            case TAG_IGNORE:
                break;

            default:
                return (*tagListPtr)++;
        }

        (*tagListPtr)++;
    }
}

static VOID _utility_FilterTagChanges ( register struct UtilityBase * UtilityBase __asm("a6"),
                                                        register struct TagItem * changeList __asm("a0"),
                                                        register struct TagItem * originalList __asm("a1"),
                                                        register ULONG apply __asm("d0"))
{
    DPRINTF (LOG_ERROR, "_utility: FilterTagChanges() unimplemented STUB called.\n");
    assert(FALSE);
}

static VOID _utility_MapTags ( register struct UtilityBase * UtilityBase __asm("a6"),
                                                        register struct TagItem * tagList __asm("a0"),
                                                        register const struct TagItem * mapList __asm("a1"),
                                                        register ULONG mapType __asm("d0"))
{
    DPRINTF (LOG_ERROR, "_utility: MapTags() unimplemented STUB called.\n");
    assert(FALSE);
}

static struct TagItem * _utility_AllocateTagItems ( register struct UtilityBase * UtilityBase __asm("a6"),
                                                        register ULONG numTags __asm("d0"))
{
    DPRINTF (LOG_ERROR, "_utility: AllocateTagItems() unimplemented STUB called.\n");
    assert(FALSE);
}

static struct TagItem * _utility_CloneTagItems ( register struct UtilityBase * UtilityBase __asm("a6"),
                                                        register const struct TagItem * tagList __asm("a0"))
{
    DPRINTF (LOG_ERROR, "_utility: CloneTagItems() unimplemented STUB called.\n");
    assert(FALSE);
}

static VOID _utility_FreeTagItems ( register struct UtilityBase * UtilityBase __asm("a6"),
                                                        register struct TagItem * tagList __asm("a0"))
{
    DPRINTF (LOG_ERROR, "_utility: FreeTagItems() unimplemented STUB called.\n");
    assert(FALSE);
}

static VOID _utility_RefreshTagItemClones ( register struct UtilityBase * UtilityBase __asm("a6"),
                                                        register struct TagItem * clone __asm("a0"),
                                                        register const struct TagItem * original __asm("a1"))
{
    DPRINTF (LOG_ERROR, "_utility: RefreshTagItemClones() unimplemented STUB called.\n");
    assert(FALSE);
}

static BOOL _utility_TagInArray ( register struct UtilityBase * UtilityBase __asm("a6"),
                                                        register Tag tagValue __asm("d0"),
                                                        register const Tag * tagArray __asm("a0"))
{
    DPRINTF (LOG_ERROR, "_utility: TagInArray() unimplemented STUB called.\n");
    assert(FALSE);
}

static ULONG _utility_FilterTagItems ( register struct UtilityBase * UtilityBase __asm("a6"),
                                                        register struct TagItem * tagList __asm("a0"),
                                                        register const Tag * filterArray __asm("a1"),
                                                        register ULONG logic __asm("d0"))
{
    DPRINTF (LOG_ERROR, "_utility: FilterTagItems() unimplemented STUB called.\n");
    assert(FALSE);
}

static ULONG _utility_CallHookPkt ( register struct UtilityBase * UtilityBase __asm("a6"),
                                                        register struct Hook * hook __asm("a0"),
                                                        register APTR object __asm("a2"),
                                                        register APTR paramPacket __asm("a1"))
{
    DPRINTF (LOG_ERROR, "_utility: CallHookPkt() unimplemented STUB called.\n");
    assert(FALSE);
}

static VOID _utility_private0 ( register struct UtilityBase * UtilityBase __asm("a6"))
{
    DPRINTF (LOG_ERROR, "_utility: private0() unimplemented STUB called.\n");
    assert(FALSE);
}

static VOID _utility_private1 ( register struct UtilityBase * UtilityBase __asm("a6"))
{
    DPRINTF (LOG_ERROR, "_utility: private1() unimplemented STUB called.\n");
    assert(FALSE);
}

static VOID _utility_Amiga2Date ( register struct UtilityBase * UtilityBase __asm("a6"),
                                                        register ULONG seconds __asm("d0"),
                                                        register struct ClockData * result __asm("a0"))
{
    DPRINTF (LOG_ERROR, "_utility: Amiga2Date() unimplemented STUB called.\n");
    assert(FALSE);
}

static ULONG _utility_Date2Amiga ( register struct UtilityBase * UtilityBase __asm("a6"),
                                                        register const struct ClockData * date __asm("a0"))
{
    DPRINTF (LOG_ERROR, "_utility: Date2Amiga() unimplemented STUB called.\n");
    assert(FALSE);
}

static ULONG _utility_CheckDate ( register struct UtilityBase * UtilityBase __asm("a6"),
                                                        register const struct ClockData * date __asm("a0"))
{
    DPRINTF (LOG_ERROR, "_utility: CheckDate() unimplemented STUB called.\n");
    assert(FALSE);
}

static LONG _utility_SMult32 ( register struct UtilityBase * UtilityBase __asm("a6"),
                                                        register LONG arg1 __asm("d0"),
                                                        register LONG arg2 __asm("d1"))
{
    DPRINTF (LOG_ERROR, "_utility: SMult32() unimplemented STUB called.\n");
    assert(FALSE);
}

static ULONG _utility_UMult32 ( register struct UtilityBase * UtilityBase __asm("a6"),
                                                        register ULONG arg1 __asm("d0"),
                                                        register ULONG arg2 __asm("d1"))
{
    DPRINTF (LOG_ERROR, "_utility: UMult32() unimplemented STUB called.\n");
    assert(FALSE);
}

static LONG _utility_SDivMod32 ( register struct UtilityBase * UtilityBase __asm("a6"),
                                                        register LONG dividend __asm("d0"),
                                                        register LONG divisor __asm("d1"))
{
    DPRINTF (LOG_ERROR, "_utility: SDivMod32() unimplemented STUB called.\n");
    assert(FALSE);
}

static ULONG _utility_UDivMod32 ( register struct UtilityBase * UtilityBase __asm("a6"),
                                                        register ULONG dividend __asm("d0"),
                                                        register ULONG divisor __asm("d1"))
{
    DPRINTF (LOG_ERROR, "_utility: UDivMod32() unimplemented STUB called.\n");
    assert(FALSE);
}

static LONG _utility_Stricmp ( register struct UtilityBase * UtilityBase __asm("a6"),
                                                        register CONST_STRPTR string1 __asm("a0"),
                                                        register CONST_STRPTR string2 __asm("a1"))
{
    DPRINTF (LOG_ERROR, "_utility: Stricmp() unimplemented STUB called.\n");
    assert(FALSE);
}

static LONG _utility_Strnicmp ( register struct UtilityBase * UtilityBase __asm("a6"),
                                                        register CONST_STRPTR string1 __asm("a0"),
                                                        register CONST_STRPTR string2 __asm("a1"),
                                                        register LONG length __asm("d0"))
{
    DPRINTF (LOG_ERROR, "_utility: Strnicmp() unimplemented STUB called.\n");
    assert(FALSE);
}

static UBYTE _utility_ToUpper ( register struct UtilityBase *UtilityBase __asm("a6"),
                                         register UBYTE               c           __asm("d0"))
{
    DPRINTF (LOG_DEBUG, "_utility: ToUpper() called.\n");

    // FIXME: take locale into account

    if ((c>96) && (c<123)) return c ^ 0x20;
    if ((c >= 0xe0) && (c <= 0xee) && (c != 0xe7)) return c-0x20;

    return c;
}

static UBYTE _utility_ToLower ( register struct UtilityBase *UtilityBase __asm("a6"),
                                         register UBYTE               c           __asm("d0"))
{
    DPRINTF (LOG_DEBUG, "_utility: ToLower() called.\n");

    // FIXME: take locale into account

    if ((c>0x40) && (c<0x5b)) return c | 0x60;
    if ((c>=0xc0) && (c<=0xde) && (c!=0xd7)) return c + 0x20;
    return c;
}

static VOID _utility_ApplyTagChanges ( register struct UtilityBase * UtilityBase __asm("a6"),
                                                        register struct TagItem * list __asm("a0"),
                                                        register const struct TagItem * changeList __asm("a1"))
{
    DPRINTF (LOG_ERROR, "_utility: ApplyTagChanges() unimplemented STUB called.\n");
    assert(FALSE);
}

static VOID _utility_private2 ( register struct UtilityBase * UtilityBase __asm("a6"))
{
    DPRINTF (LOG_ERROR, "_utility: private2() unimplemented STUB called.\n");
    assert(FALSE);
}

static LONG _utility_SMult64 ( register struct UtilityBase * UtilityBase __asm("a6"),
                                                        register LONG arg1 __asm("d0"),
                                                        register LONG arg2 __asm("d1"))
{
    DPRINTF (LOG_ERROR, "_utility: SMult64() unimplemented STUB called.\n");
    assert(FALSE);
}

static ULONG _utility_UMult64 ( register struct UtilityBase * UtilityBase __asm("a6"),
                                                        register ULONG arg1 __asm("d0"),
                                                        register ULONG arg2 __asm("d1"))
{
    DPRINTF (LOG_ERROR, "_utility: UMult64() unimplemented STUB called.\n");
    assert(FALSE);
}

static ULONG _utility_PackStructureTags ( register struct UtilityBase * UtilityBase __asm("a6"),
                                                        register APTR pack __asm("a0"),
                                                        register const ULONG * packTable __asm("a1"),
                                                        register const struct TagItem * tagList __asm("a2"))
{
    DPRINTF (LOG_ERROR, "_utility: PackStructureTags() unimplemented STUB called.\n");
    assert(FALSE);
}

static ULONG _utility_UnpackStructureTags ( register struct UtilityBase * UtilityBase __asm("a6"),
                                                        register const APTR pack __asm("a0"),
                                                        register const ULONG * packTable __asm("a1"),
                                                        register struct TagItem * tagList __asm("a2"))
{
    DPRINTF (LOG_ERROR, "_utility: UnpackStructureTags() unimplemented STUB called.\n");
    assert(FALSE);
}

static BOOL _utility_AddNamedObject ( register struct UtilityBase * UtilityBase __asm("a6"),
                                                        register struct NamedObject * nameSpace __asm("a0"),
                                                        register struct NamedObject * object __asm("a1"))
{
    DPRINTF (LOG_ERROR, "_utility: AddNamedObject() unimplemented STUB called.\n");
    assert(FALSE);
}

static struct NamedObject * _utility_AllocNamedObjectA ( register struct UtilityBase * UtilityBase __asm("a6"),
                                                        register CONST_STRPTR name __asm("a0"),
                                                        register const struct TagItem * tagList __asm("a1"))
{
    DPRINTF (LOG_ERROR, "_utility: AllocNamedObjectA() unimplemented STUB called.\n");
    assert(FALSE);
}

static LONG _utility_AttemptRemNamedObject ( register struct UtilityBase * UtilityBase __asm("a6"),
                                                        register struct NamedObject * object __asm("a0"))
{
    DPRINTF (LOG_ERROR, "_utility: AttemptRemNamedObject() unimplemented STUB called.\n");
    assert(FALSE);
}

static struct NamedObject * _utility_FindNamedObject ( register struct UtilityBase * UtilityBase __asm("a6"),
                                                        register struct NamedObject * nameSpace __asm("a0"),
                                                        register CONST_STRPTR name __asm("a1"),
                                                        register struct NamedObject * lastObject __asm("a2"))
{
    DPRINTF (LOG_ERROR, "_utility: FindNamedObject() unimplemented STUB called.\n");
    assert(FALSE);
}

static VOID _utility_FreeNamedObject ( register struct UtilityBase * UtilityBase __asm("a6"),
                                                        register struct NamedObject * object __asm("a0"))
{
    DPRINTF (LOG_ERROR, "_utility: FreeNamedObject() unimplemented STUB called.\n");
    assert(FALSE);
}

static STRPTR _utility_NamedObjectName ( register struct UtilityBase * UtilityBase __asm("a6"),
                                                        register struct NamedObject * object __asm("a0"))
{
    DPRINTF (LOG_ERROR, "_utility: NamedObjectName() unimplemented STUB called.\n");
    assert(FALSE);
}

static VOID _utility_ReleaseNamedObject ( register struct UtilityBase * UtilityBase __asm("a6"),
                                                        register struct NamedObject * object __asm("a0"))
{
    DPRINTF (LOG_ERROR, "_utility: ReleaseNamedObject() unimplemented STUB called.\n");
    assert(FALSE);
}

static VOID _utility_RemNamedObject ( register struct UtilityBase * UtilityBase __asm("a6"),
                                                        register struct NamedObject * object __asm("a0"),
                                                        register struct Message * message __asm("a1"))
{
    DPRINTF (LOG_ERROR, "_utility: RemNamedObject() unimplemented STUB called.\n");
    assert(FALSE);
}

static ULONG _utility_GetUniqueID ( register struct UtilityBase * UtilityBase __asm("a6"))
{
    DPRINTF (LOG_ERROR, "_utility: GetUniqueID() unimplemented STUB called.\n");
    assert(FALSE);
}

static VOID _utility_private3 ( register struct UtilityBase * UtilityBase __asm("a6"))
{
    DPRINTF (LOG_ERROR, "_utility: private3() unimplemented STUB called.\n");
    assert(FALSE);
}

static VOID _utility_private4 ( register struct UtilityBase * UtilityBase __asm("a6"))
{
    DPRINTF (LOG_ERROR, "_utility: private4() unimplemented STUB called.\n");
    assert(FALSE);
}

static VOID _utility_private5 ( register struct UtilityBase * UtilityBase __asm("a6"))
{
    DPRINTF (LOG_ERROR, "_utility: private5() unimplemented STUB called.\n");
    assert(FALSE);
}

static VOID _utility_private6 ( register struct UtilityBase * UtilityBase __asm("a6"))
{
    DPRINTF (LOG_ERROR, "_utility: private6() unimplemented STUB called.\n");
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

extern APTR              __g_lxa_utility_FuncTab [];
extern struct MyDataInit __g_lxa_utility_DataTab;
extern struct InitTable  __g_lxa_utility_InitTab;
extern APTR              __g_lxa_utility_EndResident;

static struct Resident __aligned ROMTag =
{                                 //                               offset
    RTC_MATCHWORD,                // UWORD rt_MatchWord;                0
    &ROMTag,                      // struct Resident *rt_MatchTag;      2
    &__g_lxa_utility_EndResident, // APTR  rt_EndSkip;                  6
    RTF_AUTOINIT,                 // UBYTE rt_Flags;                    7
    VERSION,                      // UBYTE rt_Version;                  8
    NT_LIBRARY,                   // UBYTE rt_Type;                     9
    0,                            // BYTE  rt_Pri;                     10
    &_g_utility_ExLibName[0],     // char  *rt_Name;                   14
    &_g_utility_ExLibID[0],       // char  *rt_IdString;               18
    &__g_lxa_utility_InitTab      // APTR  rt_Init;                    22
};

APTR __g_lxa_utility_EndResident;
struct Resident *__lxa_utility_ROMTag = &ROMTag;

struct InitTable __g_lxa_utility_InitTab =
{
    (ULONG)               sizeof(struct UtilityBase),   // LibBaseSize
    (APTR              *) &__g_lxa_utility_FuncTab[0],  // FunctionTable
    (APTR)                &__g_lxa_utility_DataTab,     // DataTable
    (APTR)                __g_lxa_utility_InitLib       // InitLibFn
};

APTR __g_lxa_utility_FuncTab [] =
{
    __g_lxa_utility_OpenLib,
    __g_lxa_utility_CloseLib,
    __g_lxa_utility_ExpungeLib,
    __g_lxa_utility_ExtFuncLib,
    _utility_FindTagItem, // offset = -30
    _utility_GetTagData, // offset = -36
    _utility_PackBoolTags, // offset = -42
    _utility_NextTagItem, // offset = -48
    _utility_FilterTagChanges, // offset = -54
    _utility_MapTags, // offset = -60
    _utility_AllocateTagItems, // offset = -66
    _utility_CloneTagItems, // offset = -72
    _utility_FreeTagItems, // offset = -78
    _utility_RefreshTagItemClones, // offset = -84
    _utility_TagInArray, // offset = -90
    _utility_FilterTagItems, // offset = -96
    _utility_CallHookPkt, // offset = -102
    _utility_private0, // offset = -108
    _utility_private1, // offset = -114
    _utility_Amiga2Date, // offset = -120
    _utility_Date2Amiga, // offset = -126
    _utility_CheckDate, // offset = -132
    _utility_SMult32, // offset = -138
    _utility_UMult32, // offset = -144
    _utility_SDivMod32, // offset = -150
    _utility_UDivMod32, // offset = -156
    _utility_Stricmp, // offset = -162
    _utility_Strnicmp, // offset = -168
    _utility_ToUpper, // offset = -174
    _utility_ToLower, // offset = -180
    _utility_ApplyTagChanges, // offset = -186
    _utility_private2, // offset = -192
    _utility_SMult64, // offset = -198
    _utility_UMult64, // offset = -204
    _utility_PackStructureTags, // offset = -210
    _utility_UnpackStructureTags, // offset = -216
    _utility_AddNamedObject, // offset = -222
    _utility_AllocNamedObjectA, // offset = -228
    _utility_AttemptRemNamedObject, // offset = -234
    _utility_FindNamedObject, // offset = -240
    _utility_FreeNamedObject, // offset = -246
    _utility_NamedObjectName, // offset = -252
    _utility_ReleaseNamedObject, // offset = -258
    _utility_RemNamedObject, // offset = -264
    _utility_GetUniqueID, // offset = -270
    _utility_private3, // offset = -276
    _utility_private4, // offset = -282
    _utility_private5, // offset = -288
    _utility_private6, // offset = -294
    (APTR) ((LONG)-1)
};

struct MyDataInit __g_lxa_utility_DataTab =
{
    /* ln_Type      */ INITBYTE(OFFSET(Node,         ln_Type),      NT_LIBRARY),
    /* ln_Name      */ 0x80, (UBYTE) (ULONG) OFFSET(Node,    ln_Name), (ULONG) &_g_utility_ExLibName[0],
    /* lib_Flags    */ INITBYTE(OFFSET(Library,      lib_Flags),    LIBF_SUMUSED|LIBF_CHANGED),
    /* lib_Version  */ INITWORD(OFFSET(Library,      lib_Version),  VERSION),
    /* lib_Revision */ INITWORD(OFFSET(Library,      lib_Revision), REVISION),
    /* lib_IdString */ 0x80, (UBYTE) (ULONG) OFFSET(Library, lib_IdString), (ULONG) &_g_utility_ExLibID[0],
    (ULONG) 0
};

