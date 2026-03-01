/*
 * lxa dopus.library implementation
 *
 * Provides a stub implementation of the Directory Opus 4 support library.
 * This allows Directory Opus to open the library and proceed with
 * initialization instead of immediately exiting.
 *
 * Function offsets derived from DOpus 4 GPL source code.
 */

#include <exec/types.h>
#include <exec/memory.h>
#include <exec/execbase.h>
#include <exec/resident.h>
#include <exec/initializers.h>
#include <clib/exec_protos.h>
#include <inline/exec.h>

#include <dos/dos.h>
#include <dos/dosextens.h>
#include <intuition/intuition.h>

#include <utility/tagitem.h>

#include "util.h"

#define VERSION    4
#define REVISION   12
#define EXLIBNAME  "dopus"
#define EXLIBVER   " 4.12 (2026/03/01)"

char __aligned _g_dopus_ExLibName [] = EXLIBNAME ".library";
char __aligned _g_dopus_ExLibID   [] = EXLIBNAME EXLIBVER;
char __aligned _g_dopus_Copyright [] = "(C)opyright 2026 by G. Bartsch. Licensed under the MIT license.";

char __aligned _g_dopus_VERSTRING [] = "\0$VER: " EXLIBNAME EXLIBVER;

extern struct ExecBase *SysBase;

/* DOpusBase structure — matches DOpus 4 source code layout */
struct DOpusBase {
    struct Library lib;
    UBYTE  Flags;
    UBYTE  pad;
    APTR   pdb_ExecBase;
    APTR   pdb_DOSBase;
    APTR   pdb_IntuitionBase;
    APTR   pdb_GfxBase;
    APTR   pdb_LayersBase;
    APTR   pdb_cycletop;
    APTR   pdb_cyclebot;
    APTR   pdb_check;
    ULONG  pad1, pad2, pad3, pad4;
    ULONG  pdb_Flags;
    BPTR   SegList;
};

/*
 * Library init/open/close/expunge
 */

struct DOpusBase * __g_lxa_dopus_InitLib ( register struct DOpusBase *dob __asm("d0"),
                                           register BPTR              seglist __asm("a0"),
                                           register struct ExecBase  *sysb    __asm("a6"))
{
    DPRINTF (LOG_DEBUG, "_dopus: InitLib() called\n");
    dob->SegList = seglist;
    return dob;
}

BPTR __g_lxa_dopus_ExpungeLib ( register struct DOpusBase *dob __asm("a6") )
{
    return 0;
}

struct DOpusBase * __g_lxa_dopus_OpenLib ( register struct DOpusBase *dob __asm("a6") )
{
    LPRINTF (LOG_INFO, "_dopus: OpenLib() called, dob=0x%08lx\n", (ULONG)dob);
    dob->lib.lib_OpenCnt++;
    dob->lib.lib_Flags &= ~LIBF_DELEXP;
    return dob;
}

BPTR __g_lxa_dopus_CloseLib ( register struct DOpusBase *dob __asm("a6") )
{
    LPRINTF (LOG_INFO, "_dopus: CloseLib() called, dob=0x%08lx\n", (ULONG)dob);
    dob->lib.lib_OpenCnt--;
    return 0;
}

ULONG __g_lxa_dopus_ExtFuncLib ( void )
{
    return 0;
}

/*
 * dopus.library API stubs
 *
 * All functions return NULL/0/FALSE to indicate "not available" or "no result".
 * This allows DOpus to proceed with initialization without crashing.
 */

/* -30: FileRequest */
static LONG _dopus_FileRequest ( void )
{
    DPRINTF (LOG_DEBUG, "_dopus: FileRequest() (stub)\n");
    return 0;
}

/* -36: Do3DBox */
static void _dopus_Do3DBox ( void )
{
    DPRINTF (LOG_DEBUG, "_dopus: Do3DBox() (stub)\n");
}

/* -42: Do3DStringBox */
static void _dopus_Do3DStringBox ( void )
{
    DPRINTF (LOG_DEBUG, "_dopus: Do3DStringBox() (stub)\n");
}

/* -48: Do3DCycleBox */
static void _dopus_Do3DCycleBox ( void )
{
    DPRINTF (LOG_DEBUG, "_dopus: Do3DCycleBox() (stub)\n");
}

/* -54: DoArrow */
static void _dopus_DoArrow ( void )
{
    DPRINTF (LOG_DEBUG, "_dopus: DoArrow() (stub)\n");
}

/* -60: LSprintf (private) */
static void _dopus_LSprintf ( void )
{
    DPRINTF (LOG_DEBUG, "_dopus: LSprintf() (stub)\n");
}

/* -66: LCreateExtIO */
static APTR _dopus_LCreateExtIO ( void )
{
    DPRINTF (LOG_DEBUG, "_dopus: LCreateExtIO() (stub)\n");
    return NULL;
}

/* -72: LCreatePort */
static APTR _dopus_LCreatePort ( void )
{
    DPRINTF (LOG_DEBUG, "_dopus: LCreatePort() (stub)\n");
    return NULL;
}

/* -78: LDeleteExtIO */
static void _dopus_LDeleteExtIO ( void )
{
    DPRINTF (LOG_DEBUG, "_dopus: LDeleteExtIO() (stub)\n");
}

/* -84: LDeletePort */
static void _dopus_LDeletePort ( void )
{
    DPRINTF (LOG_DEBUG, "_dopus: LDeletePort() (stub)\n");
}

/* -90: LToUpper */
static UBYTE _dopus_LToUpper ( void )
{
    DPRINTF (LOG_DEBUG, "_dopus: LToUpper() (stub)\n");
    return 0;
}

/* -96: LToLower */
static UBYTE _dopus_LToLower ( void )
{
    DPRINTF (LOG_DEBUG, "_dopus: LToLower() (stub)\n");
    return 0;
}

/* -102: LStrCat */
static void _dopus_LStrCat ( void )
{
    DPRINTF (LOG_DEBUG, "_dopus: LStrCat() (stub)\n");
}

/* -108: LStrnCat */
static void _dopus_LStrnCat ( void )
{
    DPRINTF (LOG_DEBUG, "_dopus: LStrnCat() (stub)\n");
}

/* -114: LStrCpy */
static void _dopus_LStrCpy ( void )
{
    DPRINTF (LOG_DEBUG, "_dopus: LStrCpy() (stub)\n");
}

/* -120: LStrnCpy */
static void _dopus_LStrnCpy ( void )
{
    DPRINTF (LOG_DEBUG, "_dopus: LStrnCpy() (stub)\n");
}

/* -126: LStrCmp */
static LONG _dopus_LStrCmp ( void )
{
    DPRINTF (LOG_DEBUG, "_dopus: LStrCmp() (stub)\n");
    return 0;
}

/* -132: LStrnCmp */
static LONG _dopus_LStrnCmp ( void )
{
    DPRINTF (LOG_DEBUG, "_dopus: LStrnCmp() (stub)\n");
    return 0;
}

/* -138: LStrCmpI */
static LONG _dopus_LStrCmpI ( void )
{
    DPRINTF (LOG_DEBUG, "_dopus: LStrCmpI() (stub)\n");
    return 0;
}

/* -144: LStrnCmpI */
static LONG _dopus_LStrnCmpI ( void )
{
    DPRINTF (LOG_DEBUG, "_dopus: LStrnCmpI() (stub)\n");
    return 0;
}

/* -150: StrCombine */
static LONG _dopus_StrCombine ( void )
{
    DPRINTF (LOG_DEBUG, "_dopus: StrCombine() (stub)\n");
    return 0;
}

/* -156: StrConcat */
static LONG _dopus_StrConcat ( void )
{
    DPRINTF (LOG_DEBUG, "_dopus: StrConcat() (stub)\n");
    return 0;
}

/* -162: LParsePattern */
static LONG _dopus_LParsePattern ( void )
{
    DPRINTF (LOG_DEBUG, "_dopus: LParsePattern() (stub)\n");
    return -1;
}

/* -168: LMatchPattern */
static BOOL _dopus_LMatchPattern ( void )
{
    DPRINTF (LOG_DEBUG, "_dopus: LMatchPattern() (stub)\n");
    return FALSE;
}

/* -174: LParsePatternI */
static LONG _dopus_LParsePatternI ( void )
{
    DPRINTF (LOG_DEBUG, "_dopus: LParsePatternI() (stub)\n");
    return -1;
}

/* -180: LMatchPatternI */
static BOOL _dopus_LMatchPatternI ( void )
{
    DPRINTF (LOG_DEBUG, "_dopus: LMatchPatternI() (stub)\n");
    return FALSE;
}

/* -186: BtoCStr */
static void _dopus_BtoCStr ( void )
{
    DPRINTF (LOG_DEBUG, "_dopus: BtoCStr() (stub)\n");
}

/* -192: Assign */
static BOOL _dopus_Assign ( void )
{
    DPRINTF (LOG_DEBUG, "_dopus: Assign() (stub)\n");
    return FALSE;
}

/* -198: BaseName */
static STRPTR _dopus_BaseName ( void )
{
    DPRINTF (LOG_DEBUG, "_dopus: BaseName() (stub)\n");
    return NULL;
}

/* -204: CompareLock */
static LONG _dopus_CompareLock ( void )
{
    DPRINTF (LOG_DEBUG, "_dopus: CompareLock() (stub)\n");
    return -1;
}

/* -210: PathName */
static LONG _dopus_PathName ( void )
{
    DPRINTF (LOG_DEBUG, "_dopus: PathName() (stub)\n");
    return 0;
}

/* -216: SendPacket */
static LONG _dopus_SendPacket ( void )
{
    DPRINTF (LOG_DEBUG, "_dopus: SendPacket() (stub)\n");
    return 0;
}

/* -222: TackOn */
static void _dopus_TackOn ( void )
{
    DPRINTF (LOG_DEBUG, "_dopus: TackOn() (stub)\n");
}

/* -228: StampToStr */
static void _dopus_StampToStr ( void )
{
    DPRINTF (LOG_DEBUG, "_dopus: StampToStr() (stub)\n");
}

/* -234: StrToStamp */
static BOOL _dopus_StrToStamp ( void )
{
    DPRINTF (LOG_DEBUG, "_dopus: StrToStamp() (stub)\n");
    return FALSE;
}

/* -240: AddListView */
static LONG _dopus_AddListView ( void )
{
    DPRINTF (LOG_DEBUG, "_dopus: AddListView() (stub)\n");
    return 0;
}

/* -246: ListViewIDCMP */
static LONG _dopus_ListViewIDCMP ( void )
{
    DPRINTF (LOG_DEBUG, "_dopus: ListViewIDCMP() (stub)\n");
    return -1;
}

/* -252: RefreshListView */
static void _dopus_RefreshListView ( void )
{
    DPRINTF (LOG_DEBUG, "_dopus: RefreshListView() (stub)\n");
}

/* -258: RemoveListView */
static void _dopus_RemoveListView ( void )
{
    DPRINTF (LOG_DEBUG, "_dopus: RemoveListView() (stub)\n");
}

/* -264: DrawCheckMark */
static void _dopus_DrawCheckMark ( void )
{
    DPRINTF (LOG_DEBUG, "_dopus: DrawCheckMark() (stub)\n");
}

/* -270: FixSliderBody */
static void _dopus_FixSliderBody ( void )
{
    DPRINTF (LOG_DEBUG, "_dopus: FixSliderBody() (stub)\n");
}

/* -276: FixSliderPot */
static void _dopus_FixSliderPot ( void )
{
    DPRINTF (LOG_DEBUG, "_dopus: FixSliderPot() (stub)\n");
}

/* -282: GetSliderPos */
static LONG _dopus_GetSliderPos ( void )
{
    DPRINTF (LOG_DEBUG, "_dopus: GetSliderPos() (stub)\n");
    return 0;
}

/* -288: LAllocRemember */
static APTR _dopus_LAllocRemember ( void )
{
    DPRINTF (LOG_DEBUG, "_dopus: LAllocRemember() (stub)\n");
    return NULL;
}

/* -294: LFreeRemember */
static void _dopus_LFreeRemember ( void )
{
    DPRINTF (LOG_DEBUG, "_dopus: LFreeRemember() (stub)\n");
}

/* -300: SetBusyPointer */
static void _dopus_SetBusyPointer ( void )
{
    DPRINTF (LOG_DEBUG, "_dopus: SetBusyPointer() (stub)\n");
}

/* -306: GetWBScreen */
static LONG _dopus_GetWBScreen ( void )
{
    DPRINTF (LOG_DEBUG, "_dopus: GetWBScreen() (stub)\n");
    return 0;
}

/* -312: SearchPathList */
static LONG _dopus_SearchPathList ( void )
{
    DPRINTF (LOG_DEBUG, "_dopus: SearchPathList() (stub)\n");
    return 0;
}

/* -318: CheckExist */
static LONG _dopus_CheckExist ( void )
{
    DPRINTF (LOG_DEBUG, "_dopus: CheckExist() (stub)\n");
    return 0;
}

/* -324: CompareDate */
static LONG _dopus_CompareDate ( void )
{
    DPRINTF (LOG_DEBUG, "_dopus: CompareDate() (stub)\n");
    return 0;
}

/* -330: Seed */
static void _dopus_Seed ( void )
{
    DPRINTF (LOG_DEBUG, "_dopus: Seed() (stub)\n");
}

/* -336: Random */
static LONG _dopus_Random ( void )
{
    DPRINTF (LOG_DEBUG, "_dopus: Random() (stub)\n");
    return 0;
}

/* -342: StrToUpper */
static void _dopus_StrToUpper ( void )
{
    DPRINTF (LOG_DEBUG, "_dopus: StrToUpper() (stub)\n");
}

/* -348: StrToLower */
static void _dopus_StrToLower ( void )
{
    DPRINTF (LOG_DEBUG, "_dopus: StrToLower() (stub)\n");
}

/* -354: RawkeyToStr */
static LONG _dopus_RawkeyToStr ( void )
{
    DPRINTF (LOG_DEBUG, "_dopus: RawkeyToStr() (stub)\n");
    return 0;
}

/* -360: DoRMBGadget */
static LONG _dopus_DoRMBGadget ( void )
{
    DPRINTF (LOG_DEBUG, "_dopus: DoRMBGadget() (stub)\n");
    return -1;
}

/* -366: AddGadgets */
static void _dopus_AddGadgets ( void )
{
    DPRINTF (LOG_DEBUG, "_dopus: AddGadgets() (stub)\n");
}

/* -372: ActivateStrGad */
static void _dopus_ActivateStrGad ( void )
{
    DPRINTF (LOG_DEBUG, "_dopus: ActivateStrGad() (stub)\n");
}

/* -378: RefreshStrGad */
static void _dopus_RefreshStrGad ( void )
{
    DPRINTF (LOG_DEBUG, "_dopus: RefreshStrGad() (stub)\n");
}

/* -384: CheckNumGad */
static LONG _dopus_CheckNumGad ( void )
{
    DPRINTF (LOG_DEBUG, "_dopus: CheckNumGad() (stub)\n");
    return 0;
}

/* -390: CheckHexGad */
static LONG _dopus_CheckHexGad ( void )
{
    DPRINTF (LOG_DEBUG, "_dopus: CheckHexGad() (stub)\n");
    return 0;
}

/* -396: Atoh */
static ULONG _dopus_Atoh ( void )
{
    DPRINTF (LOG_DEBUG, "_dopus: Atoh() (stub)\n");
    return 0;
}

/* -402: HiliteGad */
static void _dopus_HiliteGad ( void )
{
    DPRINTF (LOG_DEBUG, "_dopus: HiliteGad() (stub)\n");
}

/* -408: DoSimpleRequest */
static LONG _dopus_DoSimpleRequest ( void )
{
    DPRINTF (LOG_DEBUG, "_dopus: DoSimpleRequest() (stub)\n");
    return 0;
}

/* -414: ReadConfig */
static LONG _dopus_ReadConfig ( void )
{
    DPRINTF (LOG_DEBUG, "_dopus: ReadConfig() (stub)\n");
    return 0;
}

/* -420: SaveConfig */
static LONG _dopus_SaveConfig ( void )
{
    DPRINTF (LOG_DEBUG, "_dopus: SaveConfig() (stub)\n");
    return 0;
}

/* -426: DefaultConfig */
static void _dopus_DefaultConfig ( void )
{
    DPRINTF (LOG_DEBUG, "_dopus: DefaultConfig() (stub)\n");
}

/* -432: GetDevices */
static LONG _dopus_GetDevices ( void )
{
    DPRINTF (LOG_DEBUG, "_dopus: GetDevices() (stub)\n");
    return 0;
}

/* -438: AssignGadget */
static void _dopus_AssignGadget ( void )
{
    DPRINTF (LOG_DEBUG, "_dopus: AssignGadget() (stub)\n");
}

/* -444: AssignMenu */
static void _dopus_AssignMenu ( void )
{
    DPRINTF (LOG_DEBUG, "_dopus: AssignMenu() (stub)\n");
}

/* -450: FindSystemFile */
static LONG _dopus_FindSystemFile ( void )
{
    DPRINTF (LOG_DEBUG, "_dopus: FindSystemFile() (stub)\n");
    return 0;
}

/* -456: Do3DFrame */
static void _dopus_Do3DFrame ( void )
{
    DPRINTF (LOG_DEBUG, "_dopus: Do3DFrame() (stub)\n");
}

/* -462: FreeConfig */
static void _dopus_FreeConfig ( void )
{
    DPRINTF (LOG_DEBUG, "_dopus: FreeConfig() (stub)\n");
}

/* -468: DoCycleGadget */
static LONG _dopus_DoCycleGadget ( void )
{
    DPRINTF (LOG_DEBUG, "_dopus: DoCycleGadget() (stub)\n");
    return 0;
}

/* -474: UScoreText */
static void _dopus_UScoreText ( void )
{
    DPRINTF (LOG_DEBUG, "_dopus: UScoreText() (stub)\n");
}

/* -480: DisableGadget */
static void _dopus_DisableGadget ( void )
{
    DPRINTF (LOG_DEBUG, "_dopus: DisableGadget() (stub)\n");
}

/* -486: EnableGadget */
static void _dopus_EnableGadget ( void )
{
    DPRINTF (LOG_DEBUG, "_dopus: EnableGadget() (stub)\n");
}

/* -492: GhostGadget */
static void _dopus_GhostGadget ( void )
{
    DPRINTF (LOG_DEBUG, "_dopus: GhostGadget() (stub)\n");
}

/* -498: DrawRadioButton */
static void _dopus_DrawRadioButton ( void )
{
    DPRINTF (LOG_DEBUG, "_dopus: DrawRadioButton() (stub)\n");
}

/* -504: GetButtonImage */
static APTR _dopus_GetButtonImage ( void )
{
    DPRINTF (LOG_DEBUG, "_dopus: GetButtonImage() (stub)\n");
    return NULL;
}

/* -510: ShowSlider */
static void _dopus_ShowSlider ( void )
{
    DPRINTF (LOG_DEBUG, "_dopus: ShowSlider() (stub)\n");
}

/* -516: CheckConfig */
static LONG _dopus_CheckConfig ( void )
{
    DPRINTF (LOG_DEBUG, "_dopus: CheckConfig() (stub)\n");
    return 0;
}

/* -522: GetCheckImage */
static APTR _dopus_GetCheckImage ( void )
{
    DPRINTF (LOG_DEBUG, "_dopus: GetCheckImage() (stub)\n");
    return NULL;
}

/* -528: OpenRequester */
static APTR _dopus_OpenRequester ( void )
{
    DPRINTF (LOG_DEBUG, "_dopus: OpenRequester() (stub)\n");
    return NULL;
}

/* -534: CloseRequester */
static void _dopus_CloseRequester ( void )
{
    DPRINTF (LOG_DEBUG, "_dopus: CloseRequester() (stub)\n");
}

/* -540: AddRequesterObject */
static void _dopus_AddRequesterObject ( void )
{
    DPRINTF (LOG_DEBUG, "_dopus: AddRequesterObject() (stub)\n");
}

/* -546: RefreshRequesterObject */
static void _dopus_RefreshRequesterObject ( void )
{
    DPRINTF (LOG_DEBUG, "_dopus: RefreshRequesterObject() (stub)\n");
}

/* -552: ObjectText */
static void _dopus_ObjectText ( void )
{
    DPRINTF (LOG_DEBUG, "_dopus: ObjectText() (stub)\n");
}

/* -558: DoGlassImage */
static void _dopus_DoGlassImage ( void )
{
    DPRINTF (LOG_DEBUG, "_dopus: DoGlassImage() (stub)\n");
}

/* -564: Decode_RLE */
static LONG _dopus_Decode_RLE ( void )
{
    DPRINTF (LOG_DEBUG, "_dopus: Decode_RLE() (stub)\n");
    return 0;
}

/* -570: ReadStringFile */
static LONG _dopus_ReadStringFile ( void )
{
    DPRINTF (LOG_DEBUG, "_dopus: ReadStringFile() (stub)\n");
    return 0;
}

/* -576: FreeStringFile */
static void _dopus_FreeStringFile ( void )
{
    DPRINTF (LOG_DEBUG, "_dopus: FreeStringFile() (stub)\n");
}

/* -582: LFreeRemEntry */
static void _dopus_LFreeRemEntry ( void )
{
    DPRINTF (LOG_DEBUG, "_dopus: LFreeRemEntry() (stub)\n");
}

/* -588: AddGadgetBorders */
static void _dopus_AddGadgetBorders ( void )
{
    DPRINTF (LOG_DEBUG, "_dopus: AddGadgetBorders() (stub)\n");
}

/* -594: CreateGadgetBorders */
static LONG _dopus_CreateGadgetBorders ( void )
{
    DPRINTF (LOG_DEBUG, "_dopus: CreateGadgetBorders() (stub)\n");
    return 0;
}

/* -600: SelectGadget */
static void _dopus_SelectGadget ( void )
{
    DPRINTF (LOG_DEBUG, "_dopus: SelectGadget() (stub)\n");
}

/* -606: FSSetMenuStrip */
static void _dopus_FSSetMenuStrip ( void )
{
    DPRINTF (LOG_DEBUG, "_dopus: FSSetMenuStrip() (stub)\n");
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

extern APTR              __g_lxa_dopus_FuncTab [];
extern struct MyDataInit __g_lxa_dopus_DataTab;
extern struct InitTable  __g_lxa_dopus_InitTab;
extern APTR              __g_lxa_dopus_EndResident;

static struct Resident __aligned ROMTag =
{
    RTC_MATCHWORD,                        /* UWORD rt_MatchWord */
    &ROMTag,                              /* struct Resident *rt_MatchTag */
    &__g_lxa_dopus_EndResident,           /* APTR  rt_EndSkip */
    RTF_AUTOINIT,                         /* UBYTE rt_Flags */
    VERSION,                              /* UBYTE rt_Version */
    NT_LIBRARY,                           /* UBYTE rt_Type */
    0,                                    /* BYTE  rt_Pri */
    &_g_dopus_ExLibName[0],               /* char  *rt_Name */
    &_g_dopus_ExLibID[0],                 /* char  *rt_IdString */
    &__g_lxa_dopus_InitTab                /* APTR  rt_Init */
};

APTR __g_lxa_dopus_EndResident;
struct Resident *__lxa_dopus_ROMTag = &ROMTag;

struct InitTable __g_lxa_dopus_InitTab =
{
    (ULONG)               sizeof(struct DOpusBase),
    (APTR              *) &__g_lxa_dopus_FuncTab[0],
    (APTR)                &__g_lxa_dopus_DataTab,
    (APTR)                __g_lxa_dopus_InitLib
};

/* Function table — 97 functions at LVO offsets -30 through -606
 * Standard library functions at -6 through -24 (4 entries)
 * DOpus API functions at -30 through -606 (97 entries)
 */
APTR __g_lxa_dopus_FuncTab [] =
{
    __g_lxa_dopus_OpenLib,               /* -6   Standard */
    __g_lxa_dopus_CloseLib,              /* -12  Standard */
    __g_lxa_dopus_ExpungeLib,            /* -18  Standard */
    __g_lxa_dopus_ExtFuncLib,            /* -24  Standard (reserved) */
    _dopus_FileRequest,                  /* -30  FileRequest */
    _dopus_Do3DBox,                      /* -36  Do3DBox */
    _dopus_Do3DStringBox,                /* -42  Do3DStringBox */
    _dopus_Do3DCycleBox,                 /* -48  Do3DCycleBox */
    _dopus_DoArrow,                      /* -54  DoArrow */
    _dopus_LSprintf,                     /* -60  LSprintf (private) */
    _dopus_LCreateExtIO,                 /* -66  LCreateExtIO */
    _dopus_LCreatePort,                  /* -72  LCreatePort */
    _dopus_LDeleteExtIO,                 /* -78  LDeleteExtIO */
    _dopus_LDeletePort,                  /* -84  LDeletePort */
    _dopus_LToUpper,                     /* -90  LToUpper */
    _dopus_LToLower,                     /* -96  LToLower */
    _dopus_LStrCat,                      /* -102 LStrCat */
    _dopus_LStrnCat,                     /* -108 LStrnCat */
    _dopus_LStrCpy,                      /* -114 LStrCpy */
    _dopus_LStrnCpy,                     /* -120 LStrnCpy */
    _dopus_LStrCmp,                      /* -126 LStrCmp */
    _dopus_LStrnCmp,                     /* -132 LStrnCmp */
    _dopus_LStrCmpI,                     /* -138 LStrCmpI */
    _dopus_LStrnCmpI,                    /* -144 LStrnCmpI */
    _dopus_StrCombine,                   /* -150 StrCombine */
    _dopus_StrConcat,                    /* -156 StrConcat */
    _dopus_LParsePattern,                /* -162 LParsePattern */
    _dopus_LMatchPattern,                /* -168 LMatchPattern */
    _dopus_LParsePatternI,               /* -174 LParsePatternI */
    _dopus_LMatchPatternI,               /* -180 LMatchPatternI */
    _dopus_BtoCStr,                      /* -186 BtoCStr */
    _dopus_Assign,                       /* -192 Assign */
    _dopus_BaseName,                     /* -198 BaseName */
    _dopus_CompareLock,                  /* -204 CompareLock */
    _dopus_PathName,                     /* -210 PathName */
    _dopus_SendPacket,                   /* -216 SendPacket */
    _dopus_TackOn,                       /* -222 TackOn */
    _dopus_StampToStr,                   /* -228 StampToStr */
    _dopus_StrToStamp,                   /* -234 StrToStamp */
    _dopus_AddListView,                  /* -240 AddListView */
    _dopus_ListViewIDCMP,                /* -246 ListViewIDCMP */
    _dopus_RefreshListView,              /* -252 RefreshListView */
    _dopus_RemoveListView,               /* -258 RemoveListView */
    _dopus_DrawCheckMark,                /* -264 DrawCheckMark */
    _dopus_FixSliderBody,                /* -270 FixSliderBody */
    _dopus_FixSliderPot,                 /* -276 FixSliderPot */
    _dopus_GetSliderPos,                 /* -282 GetSliderPos */
    _dopus_LAllocRemember,               /* -288 LAllocRemember */
    _dopus_LFreeRemember,                /* -294 LFreeRemember */
    _dopus_SetBusyPointer,               /* -300 SetBusyPointer */
    _dopus_GetWBScreen,                  /* -306 GetWBScreen */
    _dopus_SearchPathList,               /* -312 SearchPathList */
    _dopus_CheckExist,                   /* -318 CheckExist */
    _dopus_CompareDate,                  /* -324 CompareDate */
    _dopus_Seed,                         /* -330 Seed */
    _dopus_Random,                       /* -336 Random */
    _dopus_StrToUpper,                   /* -342 StrToUpper */
    _dopus_StrToLower,                   /* -348 StrToLower */
    _dopus_RawkeyToStr,                  /* -354 RawkeyToStr */
    _dopus_DoRMBGadget,                  /* -360 DoRMBGadget */
    _dopus_AddGadgets,                   /* -366 AddGadgets */
    _dopus_ActivateStrGad,               /* -372 ActivateStrGad */
    _dopus_RefreshStrGad,                /* -378 RefreshStrGad */
    _dopus_CheckNumGad,                  /* -384 CheckNumGad */
    _dopus_CheckHexGad,                  /* -390 CheckHexGad */
    _dopus_Atoh,                         /* -396 Atoh */
    _dopus_HiliteGad,                    /* -402 HiliteGad */
    _dopus_DoSimpleRequest,              /* -408 DoSimpleRequest */
    _dopus_ReadConfig,                   /* -414 ReadConfig */
    _dopus_SaveConfig,                   /* -420 SaveConfig */
    _dopus_DefaultConfig,                /* -426 DefaultConfig */
    _dopus_GetDevices,                   /* -432 GetDevices */
    _dopus_AssignGadget,                 /* -438 AssignGadget */
    _dopus_AssignMenu,                   /* -444 AssignMenu */
    _dopus_FindSystemFile,               /* -450 FindSystemFile */
    _dopus_Do3DFrame,                    /* -456 Do3DFrame */
    _dopus_FreeConfig,                   /* -462 FreeConfig */
    _dopus_DoCycleGadget,                /* -468 DoCycleGadget */
    _dopus_UScoreText,                   /* -474 UScoreText */
    _dopus_DisableGadget,                /* -480 DisableGadget */
    _dopus_EnableGadget,                 /* -486 EnableGadget */
    _dopus_GhostGadget,                  /* -492 GhostGadget */
    _dopus_DrawRadioButton,              /* -498 DrawRadioButton */
    _dopus_GetButtonImage,               /* -504 GetButtonImage */
    _dopus_ShowSlider,                   /* -510 ShowSlider */
    _dopus_CheckConfig,                  /* -516 CheckConfig */
    _dopus_GetCheckImage,                /* -522 GetCheckImage */
    _dopus_OpenRequester,                /* -528 OpenRequester */
    _dopus_CloseRequester,               /* -534 CloseRequester */
    _dopus_AddRequesterObject,           /* -540 AddRequesterObject */
    _dopus_RefreshRequesterObject,       /* -546 RefreshRequesterObject */
    _dopus_ObjectText,                   /* -552 ObjectText */
    _dopus_DoGlassImage,                 /* -558 DoGlassImage */
    _dopus_Decode_RLE,                   /* -564 Decode_RLE */
    _dopus_ReadStringFile,               /* -570 ReadStringFile */
    _dopus_FreeStringFile,               /* -576 FreeStringFile */
    _dopus_LFreeRemEntry,                /* -582 LFreeRemEntry */
    _dopus_AddGadgetBorders,             /* -588 AddGadgetBorders */
    _dopus_CreateGadgetBorders,          /* -594 CreateGadgetBorders */
    _dopus_SelectGadget,                 /* -600 SelectGadget */
    _dopus_FSSetMenuStrip,               /* -606 FSSetMenuStrip */
    (APTR) ((LONG)-1)
};

struct MyDataInit __g_lxa_dopus_DataTab =
{
    /* ln_Type      */ INITBYTE(OFFSET(Node,         ln_Type),      NT_LIBRARY),
    /* ln_Name      */ 0x80, (UBYTE) (ULONG) OFFSET(Node,    ln_Name), (ULONG) &_g_dopus_ExLibName[0],
    /* lib_Flags    */ INITBYTE(OFFSET(Library,      lib_Flags),    LIBF_SUMUSED|LIBF_CHANGED),
    /* lib_Version  */ INITWORD(OFFSET(Library,      lib_Version),  VERSION),
    /* lib_Revision */ INITWORD(OFFSET(Library,      lib_Revision), REVISION),
    /* lib_IdString */ 0x80, (UBYTE) (ULONG) OFFSET(Library, lib_IdString), (ULONG) &_g_dopus_ExLibID[0],
    (ULONG) 0
};
