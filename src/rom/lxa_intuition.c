#include <exec/types.h>
//#include <exec/memory.h>
//#include <exec/libraries.h>
#include <exec/execbase.h>
#include <exec/resident.h>
#include <exec/initializers.h>
#include <clib/exec_protos.h>
#include <inline/exec.h>

#include <intuition/intuitionbase.h>

#include "util.h"

#define VERSION    40
#define REVISION   1
#define EXLIBNAME  "intuition"
#define EXLIBVER   " 40.1 (2022/03/21)"

char __aligned _g_intuition_ExLibName [] = EXLIBNAME ".library";
char __aligned _g_intuition_ExLibID   [] = EXLIBNAME EXLIBVER;
char __aligned _g_intuition_Copyright [] = "(C)opyright 2022 by G. Bartsch. Licensed under the MIT license.";

char __aligned _g_intuition_VERSTRING [] = "\0$VER: " EXLIBNAME EXLIBVER;

extern struct ExecBase      *SysBase;

// libBase: IntuitionBase
// baseType: struct IntuitionBase *
// libname: intuition.library

struct IntuitionBase * __g_lxa_intuition_InitLib    ( register struct IntuitionBase *intuitionb    __asm("a6"),
                                                      register BPTR               seglist __asm("a0"),
                                                      register struct ExecBase   *sysb    __asm("d0"))
{
    DPRINTF (LOG_DEBUG, "_intuition: WARNING: InitLib() unimplemented STUB called.\n");
    return intuitionb;
}

struct IntuitionBase * __g_lxa_intuition_OpenLib ( register struct IntuitionBase  *IntuitionBase __asm("a6"))
{
    DPRINTF (LOG_DEBUG, "_intuition: OpenLib() called\n");
    // FIXME IntuitionBase->dl_lib.lib_OpenCnt++;
    // FIXME IntuitionBase->dl_lib.lib_Flags &= ~LIBF_DELEXP;
    return IntuitionBase;
}

BPTR __g_lxa_intuition_CloseLib ( register struct IntuitionBase  *intuitionb __asm("a6"))
{
    return NULL;
}

BPTR __g_lxa_intuition_ExpungeLib ( register struct IntuitionBase  *intuitionb      __asm("a6"))
{
    return NULL;
}

ULONG __g_lxa_intuition_ExtFuncLib(void)
{
    return NULL;
}

VOID _intuition_OpenIntuition ( register struct IntuitionBase * IntuitionBase __asm("a6"))
{
    DPRINTF (LOG_ERROR, "_intuition: OpenIntuition() unimplemented STUB called.\n");
    assert(FALSE);
}

VOID _intuition_Intuition ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct InputEvent * iEvent __asm("a0"))
{
    DPRINTF (LOG_ERROR, "_intuition: Intuition() unimplemented STUB called.\n");
    assert(FALSE);
}

UWORD _intuition_AddGadget ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Window * window __asm("a0"),
                                                        register struct Gadget * gadget __asm("a1"),
                                                        register UWORD position __asm("d0"))
{
    DPRINTF (LOG_ERROR, "_intuition: AddGadget() unimplemented STUB called.\n");
    assert(FALSE);
}

BOOL _intuition_ClearDMRequest ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Window * window __asm("a0"))
{
    DPRINTF (LOG_ERROR, "_intuition: ClearDMRequest() unimplemented STUB called.\n");
    assert(FALSE);
}

VOID _intuition_ClearMenuStrip ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Window * window __asm("a0"))
{
    DPRINTF (LOG_ERROR, "_intuition: ClearMenuStrip() unimplemented STUB called.\n");
    assert(FALSE);
}

VOID _intuition_ClearPointer ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Window * window __asm("a0"))
{
    DPRINTF (LOG_ERROR, "_intuition: ClearPointer() unimplemented STUB called.\n");
    assert(FALSE);
}

BOOL _intuition_CloseScreen ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Screen * screen __asm("a0"))
{
    DPRINTF (LOG_ERROR, "_intuition: CloseScreen() unimplemented STUB called.\n");
    assert(FALSE);
}

VOID _intuition_CloseWindow ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Window * window __asm("a0"))
{
    DPRINTF (LOG_ERROR, "_intuition: CloseWindow() unimplemented STUB called.\n");
    assert(FALSE);
}

LONG _intuition_CloseWorkBench ( register struct IntuitionBase * IntuitionBase __asm("a6"))
{
    DPRINTF (LOG_ERROR, "_intuition: CloseWorkBench() unimplemented STUB called.\n");
    assert(FALSE);
}

VOID _intuition_CurrentTime ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register ULONG * seconds __asm("a0"),
                                                        register ULONG * micros __asm("a1"))
{
    DPRINTF (LOG_ERROR, "_intuition: CurrentTime() unimplemented STUB called.\n");
    assert(FALSE);
}

BOOL _intuition_DisplayAlert ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register ULONG alertNumber __asm("d0"),
                                                        register CONST_STRPTR string __asm("a0"),
                                                        register UWORD height __asm("d1"))
{
    DPRINTF (LOG_ERROR, "_intuition: DisplayAlert() unimplemented STUB called.\n");
    assert(FALSE);
}

VOID _intuition_DisplayBeep ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Screen * screen __asm("a0"))
{
    DPRINTF (LOG_ERROR, "_intuition: DisplayBeep() unimplemented STUB called.\n");
    assert(FALSE);
}

BOOL _intuition_DoubleClick ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register ULONG sSeconds __asm("d0"),
                                                        register ULONG sMicros __asm("d1"),
                                                        register ULONG cSeconds __asm("d2"),
                                                        register ULONG cMicros __asm("d3"))
{
    DPRINTF (LOG_ERROR, "_intuition: DoubleClick() unimplemented STUB called.\n");
    assert(FALSE);
}

VOID _intuition_DrawBorder ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct RastPort * rp __asm("a0"),
                                                        register const struct Border * border __asm("a1"),
                                                        register WORD leftOffset __asm("d0"),
                                                        register WORD topOffset __asm("d1"))
{
    DPRINTF (LOG_ERROR, "_intuition: DrawBorder() unimplemented STUB called.\n");
    assert(FALSE);
}

VOID _intuition_DrawImage ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct RastPort * rp __asm("a0"),
                                                        register struct Image * image __asm("a1"),
                                                        register WORD leftOffset __asm("d0"),
                                                        register WORD topOffset __asm("d1"))
{
    DPRINTF (LOG_ERROR, "_intuition: DrawImage() unimplemented STUB called.\n");
    assert(FALSE);
}

VOID _intuition_EndRequest ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Requester * requester __asm("a0"),
                                                        register struct Window * window __asm("a1"))
{
    DPRINTF (LOG_ERROR, "_intuition: EndRequest() unimplemented STUB called.\n");
    assert(FALSE);
}

struct Preferences * _intuition_GetDefPrefs ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Preferences * preferences __asm("a0"),
                                                        register WORD size __asm("d0"))
{
    DPRINTF (LOG_ERROR, "_intuition: GetDefPrefs() unimplemented STUB called.\n");
    assert(FALSE);
}

struct Preferences * _intuition_GetPrefs ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Preferences * preferences __asm("a0"),
                                                        register WORD size __asm("d0"))
{
    DPRINTF (LOG_ERROR, "_intuition: GetPrefs() unimplemented STUB called.\n");
    assert(FALSE);
}

VOID _intuition_InitRequester ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Requester * requester __asm("a0"))
{
    DPRINTF (LOG_ERROR, "_intuition: InitRequester() unimplemented STUB called.\n");
    assert(FALSE);
}

struct MenuItem * _intuition_ItemAddress ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register const struct Menu * menuStrip __asm("a0"),
                                                        register UWORD menuNumber __asm("d0"))
{
    DPRINTF (LOG_ERROR, "_intuition: ItemAddress() unimplemented STUB called.\n");
    assert(FALSE);
}

BOOL _intuition_ModifyIDCMP ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Window * window __asm("a0"),
                                                        register ULONG flags __asm("d0"))
{
    DPRINTF (LOG_ERROR, "_intuition: ModifyIDCMP() unimplemented STUB called.\n");
    assert(FALSE);
}

VOID _intuition_ModifyProp ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Gadget * gadget __asm("a0"),
                                                        register struct Window * window __asm("a1"),
                                                        register struct Requester * requester __asm("a2"),
                                                        register UWORD flags __asm("d0"),
                                                        register UWORD horizPot __asm("d1"),
                                                        register UWORD vertPot __asm("d2"),
                                                        register UWORD horizBody __asm("d3"),
                                                        register UWORD vertBody __asm("d4"))
{
    DPRINTF (LOG_ERROR, "_intuition: ModifyProp() unimplemented STUB called.\n");
    assert(FALSE);
}

VOID _intuition_MoveScreen ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Screen * screen __asm("a0"),
                                                        register WORD dx __asm("d0"),
                                                        register WORD dy __asm("d1"))
{
    DPRINTF (LOG_ERROR, "_intuition: MoveScreen() unimplemented STUB called.\n");
    assert(FALSE);
}

VOID _intuition_MoveWindow ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Window * window __asm("a0"),
                                                        register WORD dx __asm("d0"),
                                                        register WORD dy __asm("d1"))
{
    DPRINTF (LOG_ERROR, "_intuition: MoveWindow() unimplemented STUB called.\n");
    assert(FALSE);
}

VOID _intuition_OffGadget ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Gadget * gadget __asm("a0"),
                                                        register struct Window * window __asm("a1"),
                                                        register struct Requester * requester __asm("a2"))
{
    DPRINTF (LOG_ERROR, "_intuition: OffGadget() unimplemented STUB called.\n");
    assert(FALSE);
}

VOID _intuition_OffMenu ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Window * window __asm("a0"),
                                                        register UWORD menuNumber __asm("d0"))
{
    DPRINTF (LOG_ERROR, "_intuition: OffMenu() unimplemented STUB called.\n");
    assert(FALSE);
}

VOID _intuition_OnGadget ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Gadget * gadget __asm("a0"),
                                                        register struct Window * window __asm("a1"),
                                                        register struct Requester * requester __asm("a2"))
{
    DPRINTF (LOG_ERROR, "_intuition: OnGadget() unimplemented STUB called.\n");
    assert(FALSE);
}

VOID _intuition_OnMenu ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Window * window __asm("a0"),
                                                        register UWORD menuNumber __asm("d0"))
{
    DPRINTF (LOG_ERROR, "_intuition: OnMenu() unimplemented STUB called.\n");
    assert(FALSE);
}

struct Screen * _intuition_OpenScreen ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register const struct NewScreen * newScreen __asm("a0"))
{
    DPRINTF (LOG_ERROR, "_intuition: OpenScreen() unimplemented STUB called.\n");
    assert(FALSE);
}

struct Window * _intuition_OpenWindow ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register const struct NewWindow * newWindow __asm("a0"))
{
    DPRINTF (LOG_ERROR, "_intuition: OpenWindow() unimplemented STUB called.\n");
    assert(FALSE);
}

ULONG _intuition_OpenWorkBench ( register struct IntuitionBase * IntuitionBase __asm("a6"))
{
    DPRINTF (LOG_ERROR, "_intuition: OpenWorkBench() unimplemented STUB called.\n");
    assert(FALSE);
}

VOID _intuition_PrintIText ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct RastPort * rp __asm("a0"),
                                                        register const struct IntuiText * iText __asm("a1"),
                                                        register WORD left __asm("d0"),
                                                        register WORD top __asm("d1"))
{
    DPRINTF (LOG_ERROR, "_intuition: PrintIText() unimplemented STUB called.\n");
    assert(FALSE);
}

VOID _intuition_RefreshGadgets ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Gadget * gadgets __asm("a0"),
                                                        register struct Window * window __asm("a1"),
                                                        register struct Requester * requester __asm("a2"))
{
    DPRINTF (LOG_ERROR, "_intuition: RefreshGadgets() unimplemented STUB called.\n");
    assert(FALSE);
}

UWORD _intuition_RemoveGadget ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Window * window __asm("a0"),
                                                        register struct Gadget * gadget __asm("a1"))
{
    DPRINTF (LOG_ERROR, "_intuition: RemoveGadget() unimplemented STUB called.\n");
    assert(FALSE);
}

VOID _intuition_ReportMouse ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register BOOL flag __asm("d0"),
                                                        register struct Window * window __asm("a0"))
{
    DPRINTF (LOG_ERROR, "_intuition: ReportMouse() unimplemented STUB called.\n");
    assert(FALSE);
}

BOOL _intuition_Request ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Requester * requester __asm("a0"),
                                                        register struct Window * window __asm("a1"))
{
    DPRINTF (LOG_ERROR, "_intuition: Request() unimplemented STUB called.\n");
    assert(FALSE);
}

VOID _intuition_ScreenToBack ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Screen * screen __asm("a0"))
{
    DPRINTF (LOG_ERROR, "_intuition: ScreenToBack() unimplemented STUB called.\n");
    assert(FALSE);
}

VOID _intuition_ScreenToFront ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Screen * screen __asm("a0"))
{
    DPRINTF (LOG_ERROR, "_intuition: ScreenToFront() unimplemented STUB called.\n");
    assert(FALSE);
}

BOOL _intuition_SetDMRequest ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Window * window __asm("a0"),
                                                        register struct Requester * requester __asm("a1"))
{
    DPRINTF (LOG_ERROR, "_intuition: SetDMRequest() unimplemented STUB called.\n");
    assert(FALSE);
}

BOOL _intuition_SetMenuStrip ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Window * window __asm("a0"),
                                                        register struct Menu * menu __asm("a1"))
{
    DPRINTF (LOG_ERROR, "_intuition: SetMenuStrip() unimplemented STUB called.\n");
    assert(FALSE);
}

VOID _intuition_SetPointer ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Window * window __asm("a0"),
                                                        register UWORD * pointer __asm("a1"),
                                                        register WORD height __asm("d0"),
                                                        register WORD width __asm("d1"),
                                                        register WORD xOffset __asm("d2"),
                                                        register WORD yOffset __asm("d3"))
{
    DPRINTF (LOG_ERROR, "_intuition: SetPointer() unimplemented STUB called.\n");
    assert(FALSE);
}

VOID _intuition_SetWindowTitles ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Window * window __asm("a0"),
                                                        register CONST_STRPTR windowTitle __asm("a1"),
                                                        register CONST_STRPTR screenTitle __asm("a2"))
{
    DPRINTF (LOG_ERROR, "_intuition: SetWindowTitles() unimplemented STUB called.\n");
    assert(FALSE);
}

VOID _intuition_ShowTitle ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Screen * screen __asm("a0"),
                                                        register BOOL showIt __asm("d0"))
{
    DPRINTF (LOG_ERROR, "_intuition: ShowTitle() unimplemented STUB called.\n");
    assert(FALSE);
}

VOID _intuition_SizeWindow ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Window * window __asm("a0"),
                                                        register WORD dx __asm("d0"),
                                                        register WORD dy __asm("d1"))
{
    DPRINTF (LOG_ERROR, "_intuition: SizeWindow() unimplemented STUB called.\n");
    assert(FALSE);
}

struct View * _intuition_ViewAddress ( register struct IntuitionBase * IntuitionBase __asm("a6"))
{
    DPRINTF (LOG_ERROR, "_intuition: ViewAddress() unimplemented STUB called.\n");
    assert(FALSE);
}

struct ViewPort * _intuition_ViewPortAddress ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register const struct Window * window __asm("a0"))
{
    DPRINTF (LOG_ERROR, "_intuition: ViewPortAddress() unimplemented STUB called.\n");
    assert(FALSE);
}

VOID _intuition_WindowToBack ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Window * window __asm("a0"))
{
    DPRINTF (LOG_ERROR, "_intuition: WindowToBack() unimplemented STUB called.\n");
    assert(FALSE);
}

VOID _intuition_WindowToFront ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Window * window __asm("a0"))
{
    DPRINTF (LOG_ERROR, "_intuition: WindowToFront() unimplemented STUB called.\n");
    assert(FALSE);
}

BOOL _intuition_WindowLimits ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Window * window __asm("a0"),
                                                        register LONG widthMin __asm("d0"),
                                                        register LONG heightMin __asm("d1"),
                                                        register ULONG widthMax __asm("d2"),
                                                        register ULONG heightMax __asm("d3"))
{
    DPRINTF (LOG_ERROR, "_intuition: WindowLimits() unimplemented STUB called.\n");
    assert(FALSE);
}

struct Preferences  * _intuition_SetPrefs ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register const struct Preferences * preferences __asm("a0"),
                                                        register LONG size __asm("d0"),
                                                        register BOOL inform __asm("d1"))
{
    DPRINTF (LOG_ERROR, "_intuition: SetPrefs() unimplemented STUB called.\n");
    assert(FALSE);
}

LONG _intuition_IntuiTextLength ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register const struct IntuiText * iText __asm("a0"))
{
    DPRINTF (LOG_ERROR, "_intuition: IntuiTextLength() unimplemented STUB called.\n");
    assert(FALSE);
}

BOOL _intuition_WBenchToBack ( register struct IntuitionBase * IntuitionBase __asm("a6"))
{
    DPRINTF (LOG_ERROR, "_intuition: WBenchToBack() unimplemented STUB called.\n");
    assert(FALSE);
}

BOOL _intuition_WBenchToFront ( register struct IntuitionBase * IntuitionBase __asm("a6"))
{
    DPRINTF (LOG_ERROR, "_intuition: WBenchToFront() unimplemented STUB called.\n");
    assert(FALSE);
}

BOOL _intuition_AutoRequest ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Window * window __asm("a0"),
                                                        register const struct IntuiText * body __asm("a1"),
                                                        register const struct IntuiText * posText __asm("a2"),
                                                        register const struct IntuiText * negText __asm("a3"),
                                                        register ULONG pFlag __asm("d0"),
                                                        register ULONG nFlag __asm("d1"),
                                                        register UWORD width __asm("d2"),
                                                        register UWORD height __asm("d3"))
{
    DPRINTF (LOG_ERROR, "_intuition: AutoRequest() unimplemented STUB called.\n");
    assert(FALSE);
}

VOID _intuition_BeginRefresh ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Window * window __asm("a0"))
{
    DPRINTF (LOG_ERROR, "_intuition: BeginRefresh() unimplemented STUB called.\n");
    assert(FALSE);
}

struct Window * _intuition_BuildSysRequest ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Window * window __asm("a0"),
                                                        register const struct IntuiText * body __asm("a1"),
                                                        register const struct IntuiText * posText __asm("a2"),
                                                        register const struct IntuiText * negText __asm("a3"),
                                                        register ULONG flags __asm("d0"),
                                                        register UWORD width __asm("d1"),
                                                        register UWORD height __asm("d2"))
{
    DPRINTF (LOG_ERROR, "_intuition: BuildSysRequest() unimplemented STUB called.\n");
    assert(FALSE);
}

VOID _intuition_EndRefresh ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Window * window __asm("a0"),
                                                        register LONG complete __asm("d0"))
{
    DPRINTF (LOG_ERROR, "_intuition: EndRefresh() unimplemented STUB called.\n");
    assert(FALSE);
}

VOID _intuition_FreeSysRequest ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Window * window __asm("a0"))
{
    DPRINTF (LOG_ERROR, "_intuition: FreeSysRequest() unimplemented STUB called.\n");
    assert(FALSE);
}

LONG _intuition_MakeScreen ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Screen * screen __asm("a0"))
{
    DPRINTF (LOG_ERROR, "_intuition: MakeScreen() unimplemented STUB called.\n");
    assert(FALSE);
}

LONG _intuition_RemakeDisplay ( register struct IntuitionBase * IntuitionBase __asm("a6"))
{
    DPRINTF (LOG_ERROR, "_intuition: RemakeDisplay() unimplemented STUB called.\n");
    assert(FALSE);
}

LONG _intuition_RethinkDisplay ( register struct IntuitionBase * IntuitionBase __asm("a6"))
{
    DPRINTF (LOG_ERROR, "_intuition: RethinkDisplay() unimplemented STUB called.\n");
    assert(FALSE);
}

APTR _intuition_AllocRemember ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Remember ** rememberKey __asm("a0"),
                                                        register ULONG size __asm("d0"),
                                                        register ULONG flags __asm("d1"))
{
    DPRINTF (LOG_ERROR, "_intuition: AllocRemember() unimplemented STUB called.\n");
    assert(FALSE);
}

VOID _intuition_private0 ( register struct IntuitionBase * IntuitionBase __asm("a6"))
{
    DPRINTF (LOG_ERROR, "_intuition: private0() unimplemented STUB called.\n");
    assert(FALSE);
}

VOID _intuition_FreeRemember ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Remember ** rememberKey __asm("a0"),
                                                        register BOOL reallyForget __asm("d0"))
{
    DPRINTF (LOG_ERROR, "_intuition: FreeRemember() unimplemented STUB called.\n");
    assert(FALSE);
}

ULONG _intuition_LockIBase ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register ULONG dontknow __asm("d0"))
{
    DPRINTF (LOG_ERROR, "_intuition: LockIBase() unimplemented STUB called.\n");
    assert(FALSE);
}

VOID _intuition_UnlockIBase ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register ULONG ibLock __asm("a0"))
{
    DPRINTF (LOG_ERROR, "_intuition: UnlockIBase() unimplemented STUB called.\n");
    assert(FALSE);
}

LONG _intuition_GetScreenData ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register APTR buffer __asm("a0"),
                                                        register UWORD size __asm("d0"),
                                                        register UWORD type __asm("d1"),
                                                        register const struct Screen * screen __asm("a1"))
{
    DPRINTF (LOG_ERROR, "_intuition: GetScreenData() unimplemented STUB called.\n");
    assert(FALSE);
}

VOID _intuition_RefreshGList ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Gadget * gadgets __asm("a0"),
                                                        register struct Window * window __asm("a1"),
                                                        register struct Requester * requester __asm("a2"),
                                                        register WORD numGad __asm("d0"))
{
    DPRINTF (LOG_ERROR, "_intuition: RefreshGList() unimplemented STUB called.\n");
    assert(FALSE);
}

UWORD _intuition_AddGList ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Window * window __asm("a0"),
                                                        register struct Gadget * gadget __asm("a1"),
                                                        register UWORD position __asm("d0"),
                                                        register WORD numGad __asm("d1"),
                                                        register struct Requester * requester __asm("a2"))
{
    DPRINTF (LOG_ERROR, "_intuition: AddGList() unimplemented STUB called.\n");
    assert(FALSE);
}

UWORD _intuition_RemoveGList ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Window * remPtr __asm("a0"),
                                                        register struct Gadget * gadget __asm("a1"),
                                                        register WORD numGad __asm("d0"))
{
    DPRINTF (LOG_ERROR, "_intuition: RemoveGList() unimplemented STUB called.\n");
    assert(FALSE);
}

VOID _intuition_ActivateWindow ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Window * window __asm("a0"))
{
    DPRINTF (LOG_ERROR, "_intuition: ActivateWindow() unimplemented STUB called.\n");
    assert(FALSE);
}

VOID _intuition_RefreshWindowFrame ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Window * window __asm("a0"))
{
    DPRINTF (LOG_ERROR, "_intuition: RefreshWindowFrame() unimplemented STUB called.\n");
    assert(FALSE);
}

BOOL _intuition_ActivateGadget ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Gadget * gadgets __asm("a0"),
                                                        register struct Window * window __asm("a1"),
                                                        register struct Requester * requester __asm("a2"))
{
    DPRINTF (LOG_ERROR, "_intuition: ActivateGadget() unimplemented STUB called.\n");
    assert(FALSE);
}

VOID _intuition_NewModifyProp ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Gadget * gadget __asm("a0"),
                                                        register struct Window * window __asm("a1"),
                                                        register struct Requester * requester __asm("a2"),
                                                        register UWORD flags __asm("d0"),
                                                        register UWORD horizPot __asm("d1"),
                                                        register UWORD vertPot __asm("d2"),
                                                        register UWORD horizBody __asm("d3"),
                                                        register UWORD vertBody __asm("d4"),
                                                        register WORD numGad __asm("d5"))
{
    DPRINTF (LOG_ERROR, "_intuition: NewModifyProp() unimplemented STUB called.\n");
    assert(FALSE);
}

LONG _intuition_QueryOverscan ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register ULONG displayID __asm("a0"),
                                                        register struct Rectangle * rect __asm("a1"),
                                                        register WORD oScanType __asm("d0"))
{
    DPRINTF (LOG_ERROR, "_intuition: QueryOverscan() unimplemented STUB called.\n");
    assert(FALSE);
}

VOID _intuition_MoveWindowInFrontOf ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Window * window __asm("a0"),
                                                        register struct Window * behindWindow __asm("a1"))
{
    DPRINTF (LOG_ERROR, "_intuition: MoveWindowInFrontOf() unimplemented STUB called.\n");
    assert(FALSE);
}

VOID _intuition_ChangeWindowBox ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Window * window __asm("a0"),
                                                        register WORD left __asm("d0"),
                                                        register WORD top __asm("d1"),
                                                        register WORD width __asm("d2"),
                                                        register WORD height __asm("d3"))
{
    DPRINTF (LOG_ERROR, "_intuition: ChangeWindowBox() unimplemented STUB called.\n");
    assert(FALSE);
}

struct Hook * _intuition_SetEditHook ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Hook * hook __asm("a0"))
{
    DPRINTF (LOG_ERROR, "_intuition: SetEditHook() unimplemented STUB called.\n");
    assert(FALSE);
}

LONG _intuition_SetMouseQueue ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Window * window __asm("a0"),
                                                        register UWORD queueLength __asm("d0"))
{
    DPRINTF (LOG_ERROR, "_intuition: SetMouseQueue() unimplemented STUB called.\n");
    assert(FALSE);
}

VOID _intuition_ZipWindow ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Window * window __asm("a0"))
{
    DPRINTF (LOG_ERROR, "_intuition: ZipWindow() unimplemented STUB called.\n");
    assert(FALSE);
}

struct Screen * _intuition_LockPubScreen ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register CONST_STRPTR name __asm("a0"))
{
    DPRINTF (LOG_ERROR, "_intuition: LockPubScreen() unimplemented STUB called.\n");
    assert(FALSE);
}

VOID _intuition_UnlockPubScreen ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register CONST_STRPTR name __asm("a0"),
                                                        register struct Screen * screen __asm("a1"))
{
    DPRINTF (LOG_ERROR, "_intuition: UnlockPubScreen() unimplemented STUB called.\n");
    assert(FALSE);
}

struct List * _intuition_LockPubScreenList ( register struct IntuitionBase * IntuitionBase __asm("a6"))
{
    DPRINTF (LOG_ERROR, "_intuition: LockPubScreenList() unimplemented STUB called.\n");
    assert(FALSE);
}

VOID _intuition_UnlockPubScreenList ( register struct IntuitionBase * IntuitionBase __asm("a6"))
{
    DPRINTF (LOG_ERROR, "_intuition: UnlockPubScreenList() unimplemented STUB called.\n");
    assert(FALSE);
}

STRPTR _intuition_NextPubScreen ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register const struct Screen * screen __asm("a0"),
                                                        register STRPTR namebuf __asm("a1"))
{
    DPRINTF (LOG_ERROR, "_intuition: NextPubScreen() unimplemented STUB called.\n");
    assert(FALSE);
}

VOID _intuition_SetDefaultPubScreen ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register CONST_STRPTR name __asm("a0"))
{
    DPRINTF (LOG_ERROR, "_intuition: SetDefaultPubScreen() unimplemented STUB called.\n");
    assert(FALSE);
}

UWORD _intuition_SetPubScreenModes ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register UWORD modes __asm("d0"))
{
    DPRINTF (LOG_ERROR, "_intuition: SetPubScreenModes() unimplemented STUB called.\n");
    assert(FALSE);
}

UWORD _intuition_PubScreenStatus ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Screen * screen __asm("a0"),
                                                        register UWORD statusFlags __asm("d0"))
{
    DPRINTF (LOG_ERROR, "_intuition: PubScreenStatus() unimplemented STUB called.\n");
    assert(FALSE);
}

struct RastPort	* _intuition_ObtainGIRPort ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct GadgetInfo * gInfo __asm("a0"))
{
    DPRINTF (LOG_ERROR, "_intuition: ObtainGIRPort() unimplemented STUB called.\n");
    assert(FALSE);
}

VOID _intuition_ReleaseGIRPort ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct RastPort * rp __asm("a0"))
{
    DPRINTF (LOG_ERROR, "_intuition: ReleaseGIRPort() unimplemented STUB called.\n");
    assert(FALSE);
}

VOID _intuition_GadgetMouse ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Gadget * gadget __asm("a0"),
                                                        register struct GadgetInfo * gInfo __asm("a1"),
                                                        register WORD * mousePoint __asm("a2"))
{
    DPRINTF (LOG_ERROR, "_intuition: GadgetMouse() unimplemented STUB called.\n");
    assert(FALSE);
}

VOID _intuition_private1 ( register struct IntuitionBase * IntuitionBase __asm("a6"))
{
    DPRINTF (LOG_ERROR, "_intuition: private1() unimplemented STUB called.\n");
    assert(FALSE);
}

VOID _intuition_GetDefaultPubScreen ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register STRPTR nameBuffer __asm("a0"))
{
    DPRINTF (LOG_ERROR, "_intuition: GetDefaultPubScreen() unimplemented STUB called.\n");
    assert(FALSE);
}

LONG _intuition_EasyRequestArgs ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Window * window __asm("a0"),
                                                        register const struct EasyStruct * easyStruct __asm("a1"),
                                                        register ULONG * idcmpPtr __asm("a2"),
                                                        register const APTR args __asm("a3"))
{
    DPRINTF (LOG_ERROR, "_intuition: EasyRequestArgs() unimplemented STUB called.\n");
    assert(FALSE);
}

struct Window * _intuition_BuildEasyRequestArgs ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Window * window __asm("a0"),
                                                        register const struct EasyStruct * easyStruct __asm("a1"),
                                                        register ULONG idcmp __asm("d0"),
                                                        register const APTR args __asm("a3"))
{
    DPRINTF (LOG_ERROR, "_intuition: BuildEasyRequestArgs() unimplemented STUB called.\n");
    assert(FALSE);
}

LONG _intuition_SysReqHandler ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Window * window __asm("a0"),
                                                        register ULONG * idcmpPtr __asm("a1"),
                                                        register BOOL waitInput __asm("d0"))
{
    DPRINTF (LOG_ERROR, "_intuition: SysReqHandler() unimplemented STUB called.\n");
    assert(FALSE);
}

struct Window * _intuition_OpenWindowTagList ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register const struct NewWindow * newWindow __asm("a0"),
                                                        register const struct TagItem * tagList __asm("a1"))
{
    DPRINTF (LOG_ERROR, "_intuition: OpenWindowTagList() unimplemented STUB called.\n");
    assert(FALSE);
}

struct Screen * _intuition_OpenScreenTagList ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register const struct NewScreen * newScreen __asm("a0"),
                                                        register const struct TagItem * tagList __asm("a1"))
{
    DPRINTF (LOG_ERROR, "_intuition: OpenScreenTagList() unimplemented STUB called.\n");
    assert(FALSE);
}

VOID _intuition_DrawImageState ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct RastPort * rp __asm("a0"),
                                                        register struct Image * image __asm("a1"),
                                                        register WORD leftOffset __asm("d0"),
                                                        register WORD topOffset __asm("d1"),
                                                        register ULONG state __asm("d2"),
                                                        register const struct DrawInfo * drawInfo __asm("a2"))
{
    DPRINTF (LOG_ERROR, "_intuition: DrawImageState() unimplemented STUB called.\n");
    assert(FALSE);
}

BOOL _intuition_PointInImage ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register ULONG point __asm("d0"),
                                                        register struct Image * image __asm("a0"))
{
    DPRINTF (LOG_ERROR, "_intuition: PointInImage() unimplemented STUB called.\n");
    assert(FALSE);
}

VOID _intuition_EraseImage ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct RastPort * rp __asm("a0"),
                                                        register struct Image * image __asm("a1"),
                                                        register WORD leftOffset __asm("d0"),
                                                        register WORD topOffset __asm("d1"))
{
    DPRINTF (LOG_ERROR, "_intuition: EraseImage() unimplemented STUB called.\n");
    assert(FALSE);
}

APTR _intuition_NewObjectA ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct IClass * classPtr __asm("a0"),
                                                        register CONST_STRPTR classID __asm("a1"),
                                                        register const struct TagItem * tagList __asm("a2"))
{
    DPRINTF (LOG_ERROR, "_intuition: NewObjectA() unimplemented STUB called.\n");
    assert(FALSE);
}

VOID _intuition_DisposeObject ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register APTR object __asm("a0"))
{
    DPRINTF (LOG_ERROR, "_intuition: DisposeObject() unimplemented STUB called.\n");
    assert(FALSE);
}

ULONG _intuition_SetAttrsA ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register APTR object __asm("a0"),
                                                        register const struct TagItem * tagList __asm("a1"))
{
    DPRINTF (LOG_ERROR, "_intuition: SetAttrsA() unimplemented STUB called.\n");
    assert(FALSE);
}

ULONG _intuition_GetAttr ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register ULONG attrID __asm("d0"),
                                                        register APTR object __asm("a0"),
                                                        register ULONG * storagePtr __asm("a1"))
{
    DPRINTF (LOG_ERROR, "_intuition: GetAttr() unimplemented STUB called.\n");
    assert(FALSE);
}

ULONG _intuition_SetGadgetAttrsA ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Gadget * gadget __asm("a0"),
                                                        register struct Window * window __asm("a1"),
                                                        register struct Requester * requester __asm("a2"),
                                                        register const struct TagItem * tagList __asm("a3"))
{
    DPRINTF (LOG_ERROR, "_intuition: SetGadgetAttrsA() unimplemented STUB called.\n");
    assert(FALSE);
}

APTR _intuition_NextObject ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register APTR objectPtrPtr __asm("a0"))
{
    DPRINTF (LOG_ERROR, "_intuition: NextObject() unimplemented STUB called.\n");
    assert(FALSE);
}

VOID _intuition_private2 ( register struct IntuitionBase * IntuitionBase __asm("a6"))
{
    DPRINTF (LOG_ERROR, "_intuition: private2() unimplemented STUB called.\n");
    assert(FALSE);
}

struct IClass * _intuition_MakeClass ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register CONST_STRPTR classID __asm("a0"),
                                                        register CONST_STRPTR superClassID __asm("a1"),
                                                        register const struct IClass * superClassPtr __asm("a2"),
                                                        register UWORD instanceSize __asm("d0"),
                                                        register ULONG flags __asm("d1"))
{
    DPRINTF (LOG_ERROR, "_intuition: MakeClass() unimplemented STUB called.\n");
    assert(FALSE);
}

VOID _intuition_AddClass ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct IClass * classPtr __asm("a0"))
{
    DPRINTF (LOG_ERROR, "_intuition: AddClass() unimplemented STUB called.\n");
    assert(FALSE);
}

struct DrawInfo * _intuition_GetScreenDrawInfo ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Screen * screen __asm("a0"))
{
    DPRINTF (LOG_ERROR, "_intuition: GetScreenDrawInfo() unimplemented STUB called.\n");
    assert(FALSE);
}

VOID _intuition_FreeScreenDrawInfo ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Screen * screen __asm("a0"),
                                                        register struct DrawInfo * drawInfo __asm("a1"))
{
    DPRINTF (LOG_ERROR, "_intuition: FreeScreenDrawInfo() unimplemented STUB called.\n");
    assert(FALSE);
}

BOOL _intuition_ResetMenuStrip ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Window * window __asm("a0"),
                                                        register struct Menu * menu __asm("a1"))
{
    DPRINTF (LOG_ERROR, "_intuition: ResetMenuStrip() unimplemented STUB called.\n");
    assert(FALSE);
}

VOID _intuition_RemoveClass ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct IClass * classPtr __asm("a0"))
{
    DPRINTF (LOG_ERROR, "_intuition: RemoveClass() unimplemented STUB called.\n");
    assert(FALSE);
}

BOOL _intuition_FreeClass ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct IClass * classPtr __asm("a0"))
{
    DPRINTF (LOG_ERROR, "_intuition: FreeClass() unimplemented STUB called.\n");
    assert(FALSE);
}

VOID _intuition_private3 ( register struct IntuitionBase * IntuitionBase __asm("a6"))
{
    DPRINTF (LOG_ERROR, "_intuition: private3() unimplemented STUB called.\n");
    assert(FALSE);
}

VOID _intuition_private4 ( register struct IntuitionBase * IntuitionBase __asm("a6"))
{
    DPRINTF (LOG_ERROR, "_intuition: private4() unimplemented STUB called.\n");
    assert(FALSE);
}

VOID _intuition_private5 ( register struct IntuitionBase * IntuitionBase __asm("a6"))
{
    DPRINTF (LOG_ERROR, "_intuition: private5() unimplemented STUB called.\n");
    assert(FALSE);
}

VOID _intuition_private6 ( register struct IntuitionBase * IntuitionBase __asm("a6"))
{
    DPRINTF (LOG_ERROR, "_intuition: private6() unimplemented STUB called.\n");
    assert(FALSE);
}

VOID _intuition_private7 ( register struct IntuitionBase * IntuitionBase __asm("a6"))
{
    DPRINTF (LOG_ERROR, "_intuition: private7() unimplemented STUB called.\n");
    assert(FALSE);
}

VOID _intuition_private8 ( register struct IntuitionBase * IntuitionBase __asm("a6"))
{
    DPRINTF (LOG_ERROR, "_intuition: private8() unimplemented STUB called.\n");
    assert(FALSE);
}

VOID _intuition_private9 ( register struct IntuitionBase * IntuitionBase __asm("a6"))
{
    DPRINTF (LOG_ERROR, "_intuition: private9() unimplemented STUB called.\n");
    assert(FALSE);
}

VOID _intuition_private10 ( register struct IntuitionBase * IntuitionBase __asm("a6"))
{
    DPRINTF (LOG_ERROR, "_intuition: private10() unimplemented STUB called.\n");
    assert(FALSE);
}

struct ScreenBuffer * _intuition_AllocScreenBuffer ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Screen * sc __asm("a0"),
                                                        register struct BitMap * bm __asm("a1"),
                                                        register ULONG flags __asm("d0"))
{
    DPRINTF (LOG_ERROR, "_intuition: AllocScreenBuffer() unimplemented STUB called.\n");
    assert(FALSE);
}

VOID _intuition_FreeScreenBuffer ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Screen * sc __asm("a0"),
                                                        register struct ScreenBuffer * sb __asm("a1"))
{
    DPRINTF (LOG_ERROR, "_intuition: FreeScreenBuffer() unimplemented STUB called.\n");
    assert(FALSE);
}

ULONG _intuition_ChangeScreenBuffer ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Screen * sc __asm("a0"),
                                                        register struct ScreenBuffer * sb __asm("a1"))
{
    DPRINTF (LOG_ERROR, "_intuition: ChangeScreenBuffer() unimplemented STUB called.\n");
    assert(FALSE);
}

VOID _intuition_ScreenDepth ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Screen * screen __asm("a0"),
                                                        register ULONG flags __asm("d0"),
                                                        register APTR reserved __asm("a1"))
{
    DPRINTF (LOG_ERROR, "_intuition: ScreenDepth() unimplemented STUB called.\n");
    assert(FALSE);
}

VOID _intuition_ScreenPosition ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Screen * screen __asm("a0"),
                                                        register ULONG flags __asm("d0"),
                                                        register LONG x1 __asm("d1"),
                                                        register LONG y1 __asm("d2"),
                                                        register LONG x2 __asm("d3"),
                                                        register LONG y2 __asm("d4"))
{
    DPRINTF (LOG_ERROR, "_intuition: ScreenPosition() unimplemented STUB called.\n");
    assert(FALSE);
}

VOID _intuition_ScrollWindowRaster ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Window * win __asm("a1"),
                                                        register WORD dx __asm("d0"),
                                                        register WORD dy __asm("d1"),
                                                        register WORD xMin __asm("d2"),
                                                        register WORD yMin __asm("d3"),
                                                        register WORD xMax __asm("d4"),
                                                        register WORD yMax __asm("d5"))
{
    DPRINTF (LOG_ERROR, "_intuition: ScrollWindowRaster() unimplemented STUB called.\n");
    assert(FALSE);
}

VOID _intuition_LendMenus ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Window * fromwindow __asm("a0"),
                                                        register struct Window * towindow __asm("a1"))
{
    DPRINTF (LOG_ERROR, "_intuition: LendMenus() unimplemented STUB called.\n");
    assert(FALSE);
}

ULONG _intuition_DoGadgetMethodA ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Gadget * gad __asm("a0"),
                                                        register struct Window * win __asm("a1"),
                                                        register struct Requester * req __asm("a2"),
                                                        register Msg message __asm("a3"))
{
    DPRINTF (LOG_ERROR, "_intuition: DoGadgetMethodA() unimplemented STUB called.\n");
    assert(FALSE);
}

VOID _intuition_SetWindowPointerA ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Window * win __asm("a0"),
                                                        register const struct TagItem * taglist __asm("a1"))
{
    DPRINTF (LOG_ERROR, "_intuition: SetWindowPointerA() unimplemented STUB called.\n");
    assert(FALSE);
}

BOOL _intuition_TimedDisplayAlert ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register ULONG alertNumber __asm("d0"),
                                                        register CONST_STRPTR string __asm("a0"),
                                                        register UWORD height __asm("d1"),
                                                        register ULONG time __asm("a1"))
{
    DPRINTF (LOG_ERROR, "_intuition: TimedDisplayAlert() unimplemented STUB called.\n");
    assert(FALSE);
}

VOID _intuition_HelpControl ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Window * win __asm("a0"),
                                                        register ULONG flags __asm("d0"))
{
    DPRINTF (LOG_ERROR, "_intuition: HelpControl() unimplemented STUB called.\n");
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

extern APTR              __g_lxa_intuition_FuncTab [];
extern struct MyDataInit __g_lxa_intuition_DataTab;
extern struct InitTable  __g_lxa_intuition_InitTab;
extern APTR              __g_lxa_intuition_EndResident;

static struct Resident __aligned ROMTag =
{                                 //                               offset
    RTC_MATCHWORD,                // UWORD rt_MatchWord;                0
    &ROMTag,                      // struct Resident *rt_MatchTag;      2
    &__g_lxa_intuition_EndResident, // APTR  rt_EndSkip;                  6
    RTF_AUTOINIT,                 // UBYTE rt_Flags;                    7
    VERSION,                      // UBYTE rt_Version;                  8
    NT_LIBRARY,                   // UBYTE rt_Type;                     9
    0,                            // BYTE  rt_Pri;                     10
    &_g_intuition_ExLibName[0],     // char  *rt_Name;                   14
    &_g_intuition_ExLibID[0],       // char  *rt_IdString;               18
    &__g_lxa_intuition_InitTab      // APTR  rt_Init;                    22
};

APTR __g_lxa_intuition_EndResident;
struct Resident *__lxa_intuition_ROMTag = &ROMTag;

struct InitTable __g_lxa_intuition_InitTab =
{
    (ULONG)               sizeof(struct IntuitionBase),        // LibBaseSize
    (APTR              *) &__g_lxa_intuition_FuncTab[0],  // FunctionTable
    (APTR)                &__g_lxa_intuition_DataTab,     // DataTable
    (APTR)                __g_lxa_intuition_InitLib       // InitLibFn
};

APTR __g_lxa_intuition_FuncTab [] =
{
    __g_lxa_intuition_OpenLib,
    __g_lxa_intuition_CloseLib,
    __g_lxa_intuition_ExpungeLib,
    __g_lxa_intuition_ExtFuncLib,
    _intuition_OpenIntuition, // offset = -30
    _intuition_Intuition, // offset = -36
    _intuition_AddGadget, // offset = -42
    _intuition_ClearDMRequest, // offset = -48
    _intuition_ClearMenuStrip, // offset = -54
    _intuition_ClearPointer, // offset = -60
    _intuition_CloseScreen, // offset = -66
    _intuition_CloseWindow, // offset = -72
    _intuition_CloseWorkBench, // offset = -78
    _intuition_CurrentTime, // offset = -84
    _intuition_DisplayAlert, // offset = -90
    _intuition_DisplayBeep, // offset = -96
    _intuition_DoubleClick, // offset = -102
    _intuition_DrawBorder, // offset = -108
    _intuition_DrawImage, // offset = -114
    _intuition_EndRequest, // offset = -120
    _intuition_GetDefPrefs, // offset = -126
    _intuition_GetPrefs, // offset = -132
    _intuition_InitRequester, // offset = -138
    _intuition_ItemAddress, // offset = -144
    _intuition_ModifyIDCMP, // offset = -150
    _intuition_ModifyProp, // offset = -156
    _intuition_MoveScreen, // offset = -162
    _intuition_MoveWindow, // offset = -168
    _intuition_OffGadget, // offset = -174
    _intuition_OffMenu, // offset = -180
    _intuition_OnGadget, // offset = -186
    _intuition_OnMenu, // offset = -192
    _intuition_OpenScreen, // offset = -198
    _intuition_OpenWindow, // offset = -204
    _intuition_OpenWorkBench, // offset = -210
    _intuition_PrintIText, // offset = -216
    _intuition_RefreshGadgets, // offset = -222
    _intuition_RemoveGadget, // offset = -228
    _intuition_ReportMouse, // offset = -234
    _intuition_Request, // offset = -240
    _intuition_ScreenToBack, // offset = -246
    _intuition_ScreenToFront, // offset = -252
    _intuition_SetDMRequest, // offset = -258
    _intuition_SetMenuStrip, // offset = -264
    _intuition_SetPointer, // offset = -270
    _intuition_SetWindowTitles, // offset = -276
    _intuition_ShowTitle, // offset = -282
    _intuition_SizeWindow, // offset = -288
    _intuition_ViewAddress, // offset = -294
    _intuition_ViewPortAddress, // offset = -300
    _intuition_WindowToBack, // offset = -306
    _intuition_WindowToFront, // offset = -312
    _intuition_WindowLimits, // offset = -318
    _intuition_SetPrefs, // offset = -324
    _intuition_IntuiTextLength, // offset = -330
    _intuition_WBenchToBack, // offset = -336
    _intuition_WBenchToFront, // offset = -342
    _intuition_AutoRequest, // offset = -348
    _intuition_BeginRefresh, // offset = -354
    _intuition_BuildSysRequest, // offset = -360
    _intuition_EndRefresh, // offset = -366
    _intuition_FreeSysRequest, // offset = -372
    _intuition_MakeScreen, // offset = -378
    _intuition_RemakeDisplay, // offset = -384
    _intuition_RethinkDisplay, // offset = -390
    _intuition_AllocRemember, // offset = -396
    _intuition_private0, // offset = -402
    _intuition_FreeRemember, // offset = -408
    _intuition_LockIBase, // offset = -414
    _intuition_UnlockIBase, // offset = -420
    _intuition_GetScreenData, // offset = -426
    _intuition_RefreshGList, // offset = -432
    _intuition_AddGList, // offset = -438
    _intuition_RemoveGList, // offset = -444
    _intuition_ActivateWindow, // offset = -450
    _intuition_RefreshWindowFrame, // offset = -456
    _intuition_ActivateGadget, // offset = -462
    _intuition_NewModifyProp, // offset = -468
    _intuition_QueryOverscan, // offset = -474
    _intuition_MoveWindowInFrontOf, // offset = -480
    _intuition_ChangeWindowBox, // offset = -486
    _intuition_SetEditHook, // offset = -492
    _intuition_SetMouseQueue, // offset = -498
    _intuition_ZipWindow, // offset = -504
    _intuition_LockPubScreen, // offset = -510
    _intuition_UnlockPubScreen, // offset = -516
    _intuition_LockPubScreenList, // offset = -522
    _intuition_UnlockPubScreenList, // offset = -528
    _intuition_NextPubScreen, // offset = -534
    _intuition_SetDefaultPubScreen, // offset = -540
    _intuition_SetPubScreenModes, // offset = -546
    _intuition_PubScreenStatus, // offset = -552
    _intuition_ObtainGIRPort, // offset = -558
    _intuition_ReleaseGIRPort, // offset = -564
    _intuition_GadgetMouse, // offset = -570
    _intuition_private1, // offset = -576
    _intuition_GetDefaultPubScreen, // offset = -582
    _intuition_EasyRequestArgs, // offset = -588
    _intuition_BuildEasyRequestArgs, // offset = -594
    _intuition_SysReqHandler, // offset = -600
    _intuition_OpenWindowTagList, // offset = -606
    _intuition_OpenScreenTagList, // offset = -612
    _intuition_DrawImageState, // offset = -618
    _intuition_PointInImage, // offset = -624
    _intuition_EraseImage, // offset = -630
    _intuition_NewObjectA, // offset = -636
    _intuition_DisposeObject, // offset = -642
    _intuition_SetAttrsA, // offset = -648
    _intuition_GetAttr, // offset = -654
    _intuition_SetGadgetAttrsA, // offset = -660
    _intuition_NextObject, // offset = -666
    _intuition_private2, // offset = -672
    _intuition_MakeClass, // offset = -678
    _intuition_AddClass, // offset = -684
    _intuition_GetScreenDrawInfo, // offset = -690
    _intuition_FreeScreenDrawInfo, // offset = -696
    _intuition_ResetMenuStrip, // offset = -702
    _intuition_RemoveClass, // offset = -708
    _intuition_FreeClass, // offset = -714
    _intuition_private3, // offset = -720
    _intuition_private4, // offset = -726
    _intuition_private5, // offset = -732
    _intuition_private6, // offset = -738
    _intuition_private7, // offset = -744
    _intuition_private8, // offset = -750
    _intuition_private9, // offset = -756
    _intuition_private10, // offset = -762
    _intuition_AllocScreenBuffer, // offset = -768
    _intuition_FreeScreenBuffer, // offset = -774
    _intuition_ChangeScreenBuffer, // offset = -780
    _intuition_ScreenDepth, // offset = -786
    _intuition_ScreenPosition, // offset = -792
    _intuition_ScrollWindowRaster, // offset = -798
    _intuition_LendMenus, // offset = -804
    _intuition_DoGadgetMethodA, // offset = -810
    _intuition_SetWindowPointerA, // offset = -816
    _intuition_TimedDisplayAlert, // offset = -822
    _intuition_HelpControl, // offset = -828
    (APTR) ((LONG)-1)
};

struct MyDataInit __g_lxa_intuition_DataTab =
{
    /* ln_Type      */ INITBYTE(OFFSET(Node,         ln_Type),      NT_LIBRARY),
    /* ln_Name      */ 0x80, (UBYTE) (ULONG) OFFSET(Node,    ln_Name), (ULONG) &_g_intuition_ExLibName[0],
    /* lib_Flags    */ INITBYTE(OFFSET(Library,      lib_Flags),    LIBF_SUMUSED|LIBF_CHANGED),
    /* lib_Version  */ INITWORD(OFFSET(Library,      lib_Version),  VERSION),
    /* lib_Revision */ INITWORD(OFFSET(Library,      lib_Revision), REVISION),
    /* lib_IdString */ 0x80, (UBYTE) (ULONG) OFFSET(Library, lib_IdString), (ULONG) &_g_intuition_ExLibID[0],
    (ULONG) 0
};

