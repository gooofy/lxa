#include <exec/types.h>
#include <exec/memory.h>
//#include <exec/libraries.h>
#include <exec/execbase.h>
#include <exec/resident.h>
#include <exec/initializers.h>
#include <clib/exec_protos.h>
#include <inline/exec.h>
#include <inline/alib.h>

#include <devices/inputevent.h>

#include <intuition/intuition.h>
#include <intuition/intuitionbase.h>
#include <intuition/screens.h>
#include <intuition/sghooks.h>
#include <intuition/preferences.h>
#include <intuition/classes.h>
#include <intuition/classusr.h>
#include <intuition/imageclass.h>
#include <intuition/gadgetclass.h>
#include <intuition/icclass.h>
#include <intuition/cghooks.h>

#include <graphics/gfx.h>
#include <graphics/rastport.h>
#include <graphics/gfxmacros.h>
#include <graphics/view.h>
#include <graphics/clip.h>
#include <graphics/layers.h>
#include <graphics/layersext.h>
#include <clib/graphics_protos.h>
#include <graphics/videocontrol.h>
#include <graphics/gfxnodes.h>
#include <inline/graphics.h>
#include <clib/layers_protos.h>
#include <inline/layers.h>
#include <clib/utility_protos.h>
#include <inline/utility.h>

#include "util.h"
#include "lxa_images.h"
#include "lxa_iprefs.h"

#include <graphics/displayinfo.h>
#include <graphics/modeid.h>

extern void _input_device_dispatch_event(struct InputEvent *event);
extern void _keyboard_device_record_event(UWORD rawkey, UWORD qualifier);
extern VOID _gadtools_UpdateSliderLevelDisplay(register struct Gadget *gad __asm("a0"),
                                               register LONG level __asm("d0"));
extern BOOL _gadtools_IsCheckbox(register struct Gadget *gad __asm("a0"));
extern BOOL _gadtools_GetCheckboxState(register struct Gadget *gad __asm("a0"));
extern VOID _gadtools_SetCheckboxState(register struct Gadget *gad __asm("a0"),
                                       register BOOL checked __asm("d0"));
extern BOOL _gadtools_IsCycle(register struct Gadget *gad __asm("a0"));
extern UWORD _gadtools_GetCycleState(register struct Gadget *gad __asm("a0"));
extern UWORD _gadtools_AdvanceCycleState(register struct Gadget *gad __asm("a0"));
extern STRPTR _gadtools_GetCycleLabel(register struct Gadget *gad __asm("a0"));
extern BOOL _gadtools_IsGadTools(register struct Gadget *gad __asm("a0"));
extern VOID _gadtools_RefreshPass(register LONG begin __asm("d0"));
extern LONG _gadtools_HandleClick(register struct Gadget *gad __asm("a0"),
                                  register LONG relx __asm("d0"),
                                  register LONG rely __asm("d1"));
extern BOOL _gadtools_RenderGadget(register struct Window *win __asm("a0"),
                                   register struct Gadget *gad __asm("a1"),
                                   register struct RastPort *rp __asm("a2"),
                                   register LONG gl __asm("d0"),
                                   register LONG gt __asm("d1"));
extern VOID graphics_screen_sync_viewport_bitmap(struct Screen *screen);

/*
 * Minimum usable screen/window height threshold.
 * When applications pass suspiciously small height values (< 50 pixels),
 * we expand them to the display mode's default height. This handles older
 * apps that relied on ViewModes-based height expansion (e.g., GFA Basic).
 */

/* LACE flag for interlaced display modes */
#define LACE_FLAG 0x0004

#define DEFAULT_MOUSEQUEUE 5
#define IDCMPUPDATE_TAG_LIMIT 256
#define LXA_WMF_IDCMP_USERPORT_OWNED  0x80000000UL
#define LXA_WMF_IDCMP_WINDOWPORT_OWNED 0x40000000UL
#define LXA_WMF_GADGETHELP            0x20000000UL

#define LXA_GADTOOLS_CONTEXT_MAGIC    0x47544358UL

extern struct GfxBase *GfxBase;
extern struct Library *LayersBase;
extern struct UtilityBase *UtilityBase;
extern struct ExecBase *SysBase;

/* Global IntuitionBase (defined in exec.c) - used to avoid OpenLibrary from interrupt context */
extern struct IntuitionBase *IntuitionBase;

struct Window * _intuition_OpenWindowTagList ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register const struct NewWindow * newWindow __asm("a0"),
                                                        register const struct TagItem * tagList __asm("a1"));

/* Helper functions to bypass broken macros */
static inline void _call_MoveLayer(struct Library *base, struct Layer *layer, LONG dx, LONG dy) {
    register struct Library * _base __asm("a6") = base;
    register LONG _dummy __asm("a0") = 0;
    register struct Layer * _layer __asm("a1") = layer;
    register LONG _dx __asm("d0") = dx;
    register LONG _dy __asm("d1") = dy;
    __asm volatile ("jsr -60(%%a6)"
        :
        : "r"(_base), "r"(_dummy), "r"(_layer), "r"(_dx), "r"(_dy)
        : "d0", "d1", "a0", "a1", "memory", "cc");
}

static inline LONG _call_SizeLayer(struct Library *base, struct Layer *layer, LONG dx, LONG dy) {
    register struct Library * _base __asm("a6") = base;
    register LONG _dummy __asm("a0") = 0;
    register struct Layer * _layer __asm("a1") = layer;
    register LONG _dx __asm("d0") = dx;
    register LONG _dy __asm("d1") = dy;
    register LONG _res __asm("d0");
    __asm volatile ("jsr -66(%%a6)"
        : "=r"(_res)
        : "r"(_base), "r"(_dummy), "r"(_layer), "r"(_dx), "r"(_dy)
        : "d1", "a0", "a1", "memory", "cc");
    return _res;
}

static inline LONG _call_UpfrontLayer(struct Library *base, struct Layer *layer) {
    register struct Library * _base __asm("a6") = base;
    register LONG _dummy __asm("a0") = 0;
    register struct Layer * _layer __asm("a1") = layer;
    register LONG _res __asm("d0");
    __asm volatile ("jsr -48(%%a6)"
        : "=r"(_res)
        : "r"(_base), "r"(_dummy), "r"(_layer)
        : "d1", "a0", "a1", "memory", "cc");
    return _res;
}

static inline LONG _call_BehindLayer(struct Library *base, struct Layer *layer) {
    register struct Library * _base __asm("a6") = base;
    register LONG _dummy __asm("a0") = 0;
    register struct Layer * _layer __asm("a1") = layer;
    register LONG _res __asm("d0");
    __asm volatile ("jsr -54(%%a6)"
        : "=r"(_res)
        : "r"(_base), "r"(_dummy), "r"(_layer)
        : "d1", "a0", "a1", "memory", "cc");
    return _res;
}

static inline LONG _call_MoveLayerInFrontOf(struct Library *base,
                                            struct Layer *layer,
                                            struct Layer *other)
{
    register struct Library * _base __asm("a6") = base;
    register struct Layer * _layer __asm("a0") = layer;
    register struct Layer * _other __asm("a1") = other;
    register LONG _res __asm("d0");
    __asm volatile ("jsr -168(%%a6)"
        : "=r"(_res)
        : "r"(_base), "r"(_layer), "r"(_other)
        : "d1", "a0", "a1", "memory", "cc");
    return _res;
}

struct LXAWindowState {
    struct Node node;
    struct Window *window;
    ULONG host_window_handle;
    UWORD mouse_queue;
    UWORD pending_mousemoves;
    UBYTE prev1_down_code;
    UBYTE prev1_down_qual;
    UBYTE prev2_down_code;
    UBYTE prev2_down_qual;
    struct Window *lend_menus_to; /* LendMenus(): menus of this window are used */
    WORD open_left, open_top;     /* position at OpenWindow() time */
    WORD zip_box[4];              /* ZipWindow() alternate box (no WA_Zoom) */
    BOOL zip_valid;
    struct MsgPort *console_port;       /* attached console.device unit's input port */
    struct MsgPort *console_reply_port; /* Intuition-owned reply port for it */
    struct Window *prev_active;   /* active when this window opened active: active again when it closes */
};

/* the input events Intuition passes on to an attached console.device unit */
#define LXA_CONSOLE_INPUT_CLASSES (IDCMP_RAWKEY | IDCMP_MOUSEBUTTONS | IDCMP_MOUSEMOVE | \
                                   IDCMP_GADGETDOWN | IDCMP_GADGETUP | IDCMP_CLOSEWINDOW | \
                                   IDCMP_NEWSIZE | IDCMP_REFRESHWINDOW | IDCMP_ACTIVEWINDOW | \
                                   IDCMP_INACTIVEWINDOW | IDCMP_CHANGEWINDOW)

struct LXAIntuiMessage {
    struct IntuiMessage msg;
    APTR rawkey_prev_code_quals;
};

struct LXAGadgetContext {
    ULONG magic;
    struct Gadget *last;
};

struct LXAIntuitionBase {
    struct IntuitionBase ib;
    struct List ClassList;      /* List of public classes */
    struct IClass *RootClass;   /* Pointer to rootclass */
    struct IClass *ImageClass;  /* Pointer to imageclass */
    struct IClass *SysIClass;   /* Pointer to sysiclass */
    struct IClass *ICClass;     /* Pointer to icclass */
    struct IClass *ModelClass;  /* Pointer to modelclass */
    struct IClass *GadgetClass; /* Pointer to gadgetclass */
    struct IClass *ButtonGClass;/* Pointer to buttongclass */
    struct IClass *PropGClass;  /* Pointer to propgclass */
    struct IClass *StrGClass;   /* Pointer to strgclass */
    struct List PubScreenList;  /* List of public screens */
    struct List WindowStateList;/* Private per-window state */
    struct Screen *DefaultPubScreen;
    struct Preferences DefaultPrefs;
    struct Preferences ActivePrefs;
    struct Hook *EditHook;
    /* Phase 236: preferences installed by IPrefs through SetIPrefs() */
    ULONG PrefColors[11][3];        /* 0-7 Workbench colours, 8-10 pointer
                                     * colours 17-19 (32 bit components) */
    UWORD Pens4[NUMDRIPENS + 1];    /* Workbench pens below 8 colours */
    UWORD Pens8[NUMDRIPENS + 1];    /* Workbench pens from 8 colours on */
    struct LxaIScreenModePrefs ScreenModePrefs;
    BOOL HasScreenModePrefs;
    struct TextFont *ScreenFont;    /* screen font prefs (NULL: topaz 8) */
    struct LxaIIControlPrefs IControlPrefs;
    struct LxaIPointerPrefs PointerPrefs[2];   /* normal, busy (image copied) */
    UWORD *PointerData[2];
    struct MinList ScreenDataList; /* per-screen records (LXAPubScreenNode.all_node) */
};

/* Forward declarations */
VOID _intuition_RefreshGList ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Gadget * gadgets __asm("a0"),
                                                        register struct Window * window __asm("a1"),
                                                        register struct Requester * requester __asm("a2"),
                                                        register WORD numGad __asm("d0"));
static struct LXAWindowState *_intuition_find_window_state(struct LXAIntuitionBase *base,
                                                           const struct Window *window);
static struct Gadget *_intuition_public_gadget_list(struct Gadget *gadgets);
static ULONG _intuition_get_host_window_handle(struct LXAIntuitionBase *base,
                                               const struct Window *window);
static VOID _intuition_reset_runtime_state(VOID);
static VOID _intuition_discard_menu_runtime_state(VOID);
static VOID _intuition_clear_window_runtime_state(struct Window *window);
static VOID _intuition_clear_screen_runtime_state(struct IntuitionBase *IntuitionBase,
                                                  struct Screen *screen);
static volatile BOOL g_processing_events;
/* DisplayAlert() waits for a mouse button (Phase 237): set while an alert
 * is up, the button (SELECTDOWN/MENUDOWN) that answered it */
static volatile BOOL g_alert_waiting;
static volatile UWORD g_alert_button;
static BOOL g_screen_from_tags;     /* OpenScreen() called by OpenScreenTagList() */
static ULONG g_screen_display_id;   /* OpenScreenTagList(): SA_DisplayID + 1 (0: none) */
static BYTE g_screen_sysfont;       /* OpenScreenTagList(): SA_SysFont (-1: none) */
static BOOL g_screen_full_palette;  /* OpenScreenTagList(): SA_FullPalette */
static BOOL g_sysreq_layout;        /* BuildSysRequest(): body extent below */
static WORD g_sysreq_w, g_sysreq_h;
static void _create_screen_sys_gadgets(struct Screen *screen);
static void _free_screen_sys_gadgets(struct Screen *screen);

/* Forward declaration for internal string gadget key handling */
static BOOL _handle_string_gadget_key(struct Gadget *gad, struct Window *window, 
                                       UWORD rawkey, UWORD qualifier);

UWORD _intuition_RemoveGList ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Window * remPtr __asm("a0"),
                                                        register struct Gadget * gadget __asm("a1"),
                                                        register WORD numGad __asm("d0"));

VOID _intuition_MoveWindow ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Window * window __asm("a0"),
                                                        register WORD dx __asm("d0"),
                                                        register WORD dy __asm("d1"));
VOID _intuition_ScreenToBack ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Screen * screen __asm("a0"));
VOID _intuition_ScreenToFront ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                         register struct Screen * screen __asm("a0"));
VOID _intuition_ZipWindow ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Window * window __asm("a0"));

static ULONG _idcmp_update_payload_size(APTR payload)
{
    const struct TagItem *tags = (const struct TagItem *)payload;
    ULONG count = 0;

    if (!tags)
    {
        return 0;
    }

    while (count < IDCMPUPDATE_TAG_LIMIT)
    {
        if (tags[count].ti_Tag == TAG_END)
        {
            return (count + 1) * sizeof(struct TagItem);
        }
        count++;
    }

    return 0;
}

static VOID _dispose_idcmp_message(struct IntuiMessage *msg)
{
    struct LXAWindowState *state;
    ULONG msg_size;

    if (!msg)
    {
        return;
    }

    if (msg->Class == IDCMP_MOUSEMOVE && msg->IDCMPWindow)
    {
        state = _intuition_find_window_state((struct LXAIntuitionBase *)IntuitionBase, msg->IDCMPWindow);
        if (state && state->pending_mousemoves > 0)
            state->pending_mousemoves--;
    }

    /* AmigaOS 3.1: WFLG_WINDOWTICKED stays set from the moment a tick is
     * sent until Intuition reaps its reply (tests/probes/intuition/ticks). */
    if (msg->Class == IDCMP_INTUITICKS && msg->IDCMPWindow)
        msg->IDCMPWindow->Flags &= ~WFLG_WINDOWTICKED;

    if (msg->Class == IDCMP_IDCMPUPDATE && msg->IAddress)
    {
        ULONG payload_size = _idcmp_update_payload_size(msg->IAddress);

        if (payload_size)
        {
            FreeMem(msg->IAddress, payload_size);
        }
    }

    msg_size = sizeof(struct IntuiMessage);
    if (msg->ExecMessage.mn_Length >= sizeof(struct IntuiMessage))
        msg_size = msg->ExecMessage.mn_Length;

    FreeMem(msg, msg_size);
}

static VOID _flush_idcmp_port(struct MsgPort *port)
{
    struct IntuiMessage *msg;

    if (!port)
    {
        return;
    }

    while ((msg = (struct IntuiMessage *)GetMsg(port)) != NULL)
    {
        _dispose_idcmp_message(msg);
    }
}

static VOID _dispose_window_idcmp_ports(struct Window *window)
{
    struct LXAWindowState *state;
    struct MsgPort *user_port;
    struct MsgPort *reply_port;
    ULONG owned_flags;

    if (!window)
    {
        return;
    }

    user_port = window->UserPort;
    reply_port = window->WindowPort;
    owned_flags = window->MoreFlags;
    state = _intuition_find_window_state((struct LXAIntuitionBase *)IntuitionBase, window);

    window->UserPort = NULL;
    window->WindowPort = NULL;
    window->MoreFlags &= ~(LXA_WMF_IDCMP_USERPORT_OWNED | LXA_WMF_IDCMP_WINDOWPORT_OWNED);

    if (state)
        state->pending_mousemoves = 0;

    if (owned_flags & LXA_WMF_IDCMP_WINDOWPORT_OWNED)
    {
        _flush_idcmp_port(reply_port);
        FreeMem(reply_port, sizeof(struct MsgPort));
    }

    if (owned_flags & LXA_WMF_IDCMP_USERPORT_OWNED)
    {
        _flush_idcmp_port(user_port);
        DeleteMsgPort(user_port);
    }
}

static BOOL _ensure_window_idcmp_ports(struct Window *window)
{
    if (!window)
    {
        return FALSE;
    }

    if (!window->UserPort)
    {
        window->UserPort = CreateMsgPort();
        if (!window->UserPort)
        {
            return FALSE;
        }
        window->MoreFlags |= LXA_WMF_IDCMP_USERPORT_OWNED;
    }

    if (!window->WindowPort)
    {
        /* The reply port belongs to Intuition, not to the window's task:
         * it must not take a signal bit from the application (AmigaOS 3.1
         * opens 11+ IDCMP windows per task).  Replies are reaped by polling
         * (_reap_window_idcmp_replies), so the port never signals. */
        window->WindowPort = (struct MsgPort *)AllocMem(sizeof(struct MsgPort), MEMF_PUBLIC | MEMF_CLEAR);
        if (window->WindowPort)
        {
            window->WindowPort->mp_Node.ln_Type = NT_MSGPORT;
            window->WindowPort->mp_Flags = PA_IGNORE;
            NewList(&window->WindowPort->mp_MsgList);
        }
        if (!window->WindowPort)
        {
            if (window->MoreFlags & LXA_WMF_IDCMP_USERPORT_OWNED)
            {
                DeleteMsgPort(window->UserPort);
                window->UserPort = NULL;
                window->MoreFlags &= ~LXA_WMF_IDCMP_USERPORT_OWNED;
            }
            return FALSE;
        }
        window->MoreFlags |= LXA_WMF_IDCMP_WINDOWPORT_OWNED;
    }

    return TRUE;
}

static VOID _reap_window_idcmp_replies(struct Window *window)
{
    if (!window || !(window->MoreFlags & LXA_WMF_IDCMP_WINDOWPORT_OWNED))
    {
        return;
    }

    _flush_idcmp_port(window->WindowPort);
}

struct LXAClassNode {
    struct Node node;
    struct IClass *class_ptr;
};

/*
 * Per-screen record.  Every screen has one (linked through all_node into
 * ScreenDataList); only public screens (Workbench, SA_PubName screens) are
 * also linked through pub.psn_Node into the public screen list that
 * LockPubScreenList() exposes.
 */
struct LXAPubScreenNode {
    struct PubScreenNode pub;
    UWORD pens[NUMDRIPENS + 1];  /* Per-screen pen array (terminated with ~0) */
    struct DrawInfo drawInfo;     /* Pre-built DrawInfo for GetScreenDrawInfo() */
    BOOL has_custom_pens;         /* TRUE if SA_Pens was provided */
    struct MinNode all_node;      /* ScreenDataList link */
    BOOL is_public;               /* pub.psn_Node is in PubScreenList */
    BOOL default_font;            /* no NewScreen.Font/SA_Font: windows draw in
                                   * the system default font */
};

/* AmigaOS 3.1 reference: a custom screen opened without SA_Pens keeps the
 * pre-V36 pens and has DRIF_NEWLOOK clear. */
static VOID _intuition_set_oldlook_pens(struct LXAPubScreenNode *entry)
{
    /* AmigaOS 3.1 (tests/probes/intuition/screens): the old look pens come
     * from the screen's DetailPen D and BlockPen B: DETAILPEN = D,
     * BLOCKPEN = B, BACKGROUNDPEN = 0, FILLTEXTPEN and BARDETAILPEN = D and
     * every other pen = B - unless B is 0, then those two groups swap. */
    struct Screen *s = entry->pub.psn_Screen;
    UWORD d = s ? s->DetailPen : 0, b = s ? s->BlockPen : 1;
    UWORD fg = b ? b : d, detail = b ? d : b;
    UWORD i;

    for (i = 0; i < NUMDRIPENS; i++)
        entry->pens[i] = fg;
    entry->pens[DETAILPEN] = d;
    entry->pens[BLOCKPEN] = b;
    entry->pens[BACKGROUNDPEN] = 0;
    entry->pens[FILLTEXTPEN] = detail;
    entry->pens[BARDETAILPEN] = detail;
    entry->pens[NUMDRIPENS] = (UWORD)~0;
    entry->drawInfo.dri_Flags &= ~DRIF_NEWLOOK;
}

#define LXA_PUB_FROM_ALL_NODE(n) \
    ((struct LXAPubScreenNode *)((UBYTE *)(n) - (ULONG)&((struct LXAPubScreenNode *)0)->all_node))

static struct IClass *_intuition_find_class(struct LXAIntuitionBase *base, CONST_STRPTR classID);
static ULONG _intuition_dispatch_method(struct IClass *cl, Object *obj, Msg msg);

/*
 * Intuition calls a GTYP_CUSTOMGADGET through the hook in its MutualExclude
 * field (a0 = hook, a2 = gadget, a1 = message); for a BOOPSI gadget that is
 * its class (AmigaOS 3.1, probe intuition/customhook).  Programs build custom
 * gadgets by hand with their own hook and no BOOPSI object header (Fish
 * JukeBox).  Returns FALSE when the gadget has no hook to call.
 */
static BOOL _custom_gadget_call(struct Gadget *gad, Msg msg, ULONG *result)
{
    typedef ULONG (*HookEntry)(register struct Hook *h __asm("a0"),
                               register Object *obj __asm("a2"),
                               register Msg msg __asm("a1"));
    struct Hook *h = (struct Hook *)gad->MutualExclude;
    ULONG r;

    if (!h)
    {
        /* gadgets lxa created without the hook: dispatch on the class */
        struct IClass *cl = OCLASS((Object *)gad);
        if (!cl)
            return FALSE;
        h = &cl->cl_Dispatcher;
    }
    if (!h->h_Entry)
        return FALSE;
    r = ((HookEntry)h->h_Entry)(h, (Object *)gad, msg);
    if (result)
        *result = r;
    return TRUE;
}
VOID _intuition_DrawImageState ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct RastPort * rp __asm("a0"),
                                                        register struct Image * image __asm("a1"),
                                                        register WORD leftOffset __asm("d0"),
                                                        register WORD topOffset __asm("d1"),
                                                        register ULONG state __asm("d2"),
                                                        register const struct DrawInfo * drawInfo __asm("a2"));

static struct LXAWindowState *_intuition_find_window_state(struct LXAIntuitionBase *base,
                                                           const struct Window *window)
{
    struct Node *node;

    if (!base || !window)
        return NULL;

    for (node = base->WindowStateList.lh_Head; node && node->ln_Succ; node = node->ln_Succ)
    {
        struct LXAWindowState *state = (struct LXAWindowState *)node;

        if (state->window == window)
            return state;
    }

    return NULL;
}

static struct Gadget *_intuition_public_gadget_list(struct Gadget *gadgets)
{
    struct LXAGadgetContext *context;

    if (!gadgets || gadgets->GadgetType != GTYP_CUSTOMGADGET)
        return gadgets;

    context = (struct LXAGadgetContext *)gadgets->SpecialInfo;
    if (!context || context->magic != LXA_GADTOOLS_CONTEXT_MAGIC)
        return gadgets;

    return gadgets->NextGadget;
}

static ULONG _intuition_get_host_window_handle(struct LXAIntuitionBase *base,
                                               const struct Window *window)
{
    struct LXAWindowState *state;

    state = _intuition_find_window_state(base, window);
    if (!state)
        return 0;

    return state->host_window_handle;
}

static VOID _intuition_note_rawkey(struct LXAWindowState *state,
                                   UWORD rawkey,
                                   UWORD qualifier)
{
    if (!state || (rawkey & IECODE_UP_PREFIX))
        return;

    state->prev2_down_code = state->prev1_down_code;
    state->prev2_down_qual = state->prev1_down_qual;
    state->prev1_down_code = (UBYTE)(rawkey & ~IECODE_UP_PREFIX);
    state->prev1_down_qual = (UBYTE)(qualifier & 0xff);
}

static struct LXAWindowState *_intuition_ensure_window_state(struct LXAIntuitionBase *base,
                                                             struct Window *window)
{
    struct LXAWindowState *state;

    state = _intuition_find_window_state(base, window);
    if (state)
        return state;

    state = (struct LXAWindowState *)AllocMem(sizeof(*state), MEMF_PUBLIC | MEMF_CLEAR);
    if (!state)
        return NULL;

    state->window = window;
    state->mouse_queue = DEFAULT_MOUSEQUEUE;
    AddTail(&base->WindowStateList, &state->node);

    return state;
}

/*
 * LendMenus(): when the menu button is pressed in a window that lends its
 * menu activation to another window, that other window's menu strip is used.
 */
static struct Window *_intuition_menu_window(struct LXAIntuitionBase *base, struct Window *window)
{
    struct LXAWindowState *state = _intuition_find_window_state(base, window);

    if (state && state->lend_menus_to)
        return state->lend_menus_to;
    return window;
}

static VOID _intuition_remove_window_state(struct LXAIntuitionBase *base, const struct Window *window)
{
    struct LXAWindowState *state;
    struct Node *node;

    /* nobody may keep lending menus to a window that goes away */
    for (node = base->WindowStateList.lh_Head; node && node->ln_Succ; node = node->ln_Succ)
    {
        if (((struct LXAWindowState *)node)->lend_menus_to == window)
            ((struct LXAWindowState *)node)->lend_menus_to = NULL;
    }

    state = _intuition_find_window_state(base, window);
    if (!state)
        return;

    Remove(&state->node);
    if (state->console_reply_port)
    {
        _flush_idcmp_port(state->console_reply_port);
        FreeMem(state->console_reply_port, sizeof(struct MsgPort));
    }
    FreeMem(state, sizeof(*state));
}

static LONG _intuition_set_mouse_queue_value(struct LXAIntuitionBase *base,
                                             struct Window *window,
                                             UWORD queue_length)
{
    struct LXAWindowState *state;
    LONG old_value;

    state = _intuition_find_window_state(base, window);
    if (!state)
        return -1;

    old_value = state->mouse_queue;
    state->mouse_queue = queue_length;
    return old_value;
}

/* Forward declarations for internal calls */
struct DrawInfo * _intuition_GetScreenDrawInfo(
    register struct IntuitionBase * IntuitionBase __asm("a6"),
    register struct Screen * screen __asm("a0"));
VOID _intuition_FreeScreenDrawInfo(
    register struct IntuitionBase * IntuitionBase __asm("a6"),
    register struct Screen * screen __asm("a0"),
    register struct DrawInfo * drawInfo __asm("a1"));

/* Safe bounded string copy (avoids string.h dependency for strncpy) */
static void _intuition_strncpy(char *dst, const char *src, LONG maxchars)
{
    LONG i;
    for (i = 0; i < maxchars - 1 && src[i] != '\0'; i++)
        dst[i] = src[i];
    dst[i] = '\0';
}

/* The pointer of the AmigaOS 3.1 Preferences (PointerMatrix, two words
 * per row between the control words; reference: tests/gallery/prefs.c) */
static const UWORD _intuition_default_pointer[POINTERSIZE] = {
    0x0000, 0x0000,
    0xc000, 0x4000, 0x7000, 0xb000, 0x3c00, 0x4c00, 0x3f00, 0x4300,
    0x1fc0, 0x20c0, 0x1fc0, 0x2000, 0x0f00, 0x1100, 0x0d80, 0x1280,
    0x04c0, 0x0940, 0x0460, 0x08a0, 0x0020, 0x0040
};

/* Preferences of a freshly booted AmigaOS 3.1 (reference-verified, Phase
 * 236: tests/scenarios/gallery-prefs-default.yaml); GetDefPrefs() reports
 * a 9 pixel font and a 1.5 s double-click time, GetPrefs() 8 and 1.02 s. */
static VOID _intuition_init_preferences(struct Preferences *prefs)
{
    if (!prefs)
        return;

    memset(prefs, 0, sizeof(*prefs));

    prefs->FontHeight = 8;
    prefs->PrinterPort = 0;
    prefs->BaudRate = 5;

    prefs->KeyRptDelay.tv_secs = 0;
    prefs->KeyRptDelay.tv_micro = 600000;
    prefs->KeyRptSpeed.tv_secs = 0;
    prefs->KeyRptSpeed.tv_micro = 50000;

    prefs->DoubleClick.tv_secs = 1;
    prefs->DoubleClick.tv_micro = 20000;

    CopyMem((APTR)_intuition_default_pointer, prefs->PointerMatrix, sizeof(_intuition_default_pointer));
    prefs->XOffset = -1;
    prefs->YOffset = 0;
    prefs->PointerTicks = 1;

    prefs->color0 = 0x0AAA;
    prefs->color1 = 0x0000;
    prefs->color2 = 0x0FFF;
    prefs->color3 = 0x068B;

    prefs->color17 = 0x0E44;
    prefs->color18 = 0x0000;
    prefs->color19 = 0x0EEC;

    prefs->ViewXOffset = 0;
    prefs->ViewYOffset = 0;
    prefs->ViewInitX = 0x0081;
    prefs->ViewInitY = 0x002C;

    prefs->EnableCLI = TRUE | (1 << 14);

    prefs->PrinterType = 0x07;
    prefs->PrintPitch = 0;
    prefs->PrintQuality = 0;
    prefs->PrintSpacing = 0;
    prefs->PrintLeftMargin = 5;
    prefs->PrintRightMargin = 75;
    prefs->PrintImage = 0;
    prefs->PrintAspect = 0;
    prefs->PrintShade = 1;
    prefs->PrintThreshold = 7;

    prefs->PaperSize = 0;
    prefs->PaperLength = 66;
    prefs->PaperType = 0;

    prefs->SerRWBits = 0;
    prefs->SerStopBuf = 0x01;
    prefs->SerParShk = 0x02;

    prefs->LaceWB = 0;
}

static struct Preferences *_intuition_copy_prefs(struct Preferences *dst,
                                                 WORD size,
                                                 const struct Preferences *src)
{
    ULONG copy_size;

    if (!dst)
        return NULL;

    if (size <= 0 || !src)
        return dst;

    copy_size = (ULONG)size;
    if (copy_size > sizeof(struct Preferences))
        copy_size = sizeof(struct Preferences);

    CopyMem((APTR)src, dst, copy_size);
    return dst;
}

static const UWORD _intuition_busy_pointer[] = {
    0x0000, 0x0000,
    0x0400, 0x07C0,
    0x0000, 0x07C0,
    0x0100, 0x0380,
    0x0000, 0x07E0,
    0x07C0, 0x1FF8,
    0x1FF0, 0x3FEC,
    0x3FF8, 0x7FDE,
    0x3FF8, 0x7FBE,
    0x7FFC, 0xFF7F,
    0x7EFC, 0xFFFF,
    0x7FFC, 0xFFFF,
    0x3FF8, 0x7FFE,
    0x3FF8, 0x7FFE,
    0x1FF0, 0x3FFC,
    0x07C0, 0x1FF8,
    0x0000, 0x07E0,
    0x0000, 0x0000
};

static int _intuition_ascii_casecmp(const char *a, const char *b)
{
    while (*a && *b)
    {
        char ca = *a;
        char cb = *b;

        if (ca >= 'A' && ca <= 'Z')
            ca = (char)(ca - 'A' + 'a');
        if (cb >= 'A' && cb <= 'Z')
            cb = (char)(cb - 'A' + 'a');

        if (ca != cb)
            return (int)((unsigned char)ca - (unsigned char)cb);

        a++;
        b++;
    }

    return (int)((unsigned char)*a - (unsigned char)*b);
}

static const char *_intuition_pubscreen_name_for_screen(const struct Screen *screen)
{
    if (!screen)
        return "Workbench";

    if ((screen->Flags & SCREENTYPE) == WBENCHSCREEN)
        return "Workbench";

    if (screen->Title && screen->Title[0] != '\0')
        return (const char *)screen->Title;

    if (screen->DefaultTitle && screen->DefaultTitle[0] != '\0')
        return (const char *)screen->DefaultTitle;

    return "Screen";
}

static struct Screen *_intuition_find_workbench_screen(struct IntuitionBase *IntuitionBase)
{
    struct Screen *screen;

    if (!IntuitionBase)
        return NULL;

    for (screen = IntuitionBase->FirstScreen; screen; screen = screen->NextScreen)
    {
        if ((screen->Flags & SCREENTYPE) == WBENCHSCREEN)
            return screen;
    }

    return NULL;
}

/* per-screen record of any screen (public or not) */
/* AmigaOS 3.1 reference (tests/probes/graphics/vpextra): every screen's
 * ColorMap has a ViewPortExtra whose DisplayClip is the OSCAN_TEXT
 * rectangle of the screen's mode (reqtools sizes its requesters from it) */
static void _intuition_update_display_clip(struct Screen *screen)
{
    struct ColorMap *cm = screen->ViewPort.ColorMap;
    struct ViewPortExtra *vpe;
    struct DimensionInfo dims;

    if (!cm)
        return;
    vpe = cm->cm_vpe;
    if (!vpe)
    {
        struct TagItem tags[2];

        vpe = (struct ViewPortExtra *)GfxNew(VIEWPORT_EXTRA_TYPE);
        if (!vpe)
            return;
        tags[0].ti_Tag = VTAG_VIEWPORTEXTRA_SET;
        tags[0].ti_Data = (ULONG)vpe;
        tags[1].ti_Tag = TAG_END;
        VideoControl(cm, tags);
    }
    if (GetDisplayInfoData(NULL, (UBYTE *)&dims, sizeof(dims), DTAG_DIMS, GetVPModeID(&screen->ViewPort)))
        vpe->DisplayClip = dims.TxtOScan;
}

static struct PubScreenNode *_intuition_find_pubscreen_by_screen(struct LXAIntuitionBase *base,
                                                                  const struct Screen *screen)
{
    struct MinNode *node;

    if (!base || !screen)
        return NULL;

    for (node = base->ScreenDataList.mlh_Head; node && node->mln_Succ; node = node->mln_Succ)
    {
        struct LXAPubScreenNode *entry = LXA_PUB_FROM_ALL_NODE(node);
        if (entry->pub.psn_Screen == screen)
            return &entry->pub;
    }

    return NULL;
}

/* record of a screen that is in the public screen list */
static struct PubScreenNode *_intuition_find_public_screen(struct LXAIntuitionBase *base,
                                                           const struct Screen *screen)
{
    struct PubScreenNode *pub = _intuition_find_pubscreen_by_screen(base, screen);

    if (pub && ((struct LXAPubScreenNode *)pub)->is_public)
        return pub;
    return NULL;
}

static struct PubScreenNode *_intuition_find_pubscreen_by_name(struct LXAIntuitionBase *base,
                                                                CONST_STRPTR name)
{
    struct Node *node;

    if (!base || !name)
        return NULL;

    for (node = base->PubScreenList.lh_Head; node && node->ln_Succ; node = node->ln_Succ)
    {
        struct PubScreenNode *pub = (struct PubScreenNode *)node;
        if (pub->psn_Node.ln_Name && _intuition_ascii_casecmp(pub->psn_Node.ln_Name, (const char *)name) == 0)
            return pub;
    }

    return NULL;
}

static struct PubScreenNode *_intuition_default_pubscreen_node(struct LXAIntuitionBase *base)
{
    struct PubScreenNode *pub;
    struct Node *node;

    if (!base)
        return NULL;

    if (base->DefaultPubScreen)
    {
        pub = _intuition_find_public_screen(base, base->DefaultPubScreen);
        if (pub && !(pub->psn_Flags & PSNF_PRIVATE))
            return pub;
    }

    pub = _intuition_find_pubscreen_by_name(base, (CONST_STRPTR)"Workbench");
    if (pub && !(pub->psn_Flags & PSNF_PRIVATE))
        return pub;

    for (node = base->PubScreenList.lh_Head; node && node->ln_Succ; node = node->ln_Succ)
    {
        pub = (struct PubScreenNode *)node;
        if (!(pub->psn_Flags & PSNF_PRIVATE))
            return pub;
    }

    return NULL;
}

struct LXAPubScreenNode;
static VOID _intuition_set_oldlook_pens(struct LXAPubScreenNode *entry);

/* ------------------------------------------------------------------------
 * Preferences installed by IPrefs (Phase 236, reference-verified with
 * tests/scenarios/gallery-prefs-*.yaml)
 * ------------------------------------------------------------------------ */

static const UWORD _intuition_newlook_pens[NUMDRIPENS] = {
    0, 1, 1, 2, 1, 3, 1, 0, 2, 1, 2, 1
};

static VOID _intuition_init_iprefs(struct LXAIntuitionBase *base)
{
    /* the Workbench palette of AmigaOS 3.1: colours 0-3, the four colours
     * the Workbench puts at the end of a palette of 8 or more, and the
     * pointer colours */
    static const UBYTE rgb[11][3] = {
        { 0xaa, 0xaa, 0xaa }, { 0x00, 0x00, 0x00 }, { 0xff, 0xff, 0xff }, { 0x66, 0x88, 0xbb },
        { 0xee, 0x44, 0x44 }, { 0x55, 0xdd, 0x55 }, { 0x00, 0x44, 0xdd }, { 0xee, 0x99, 0x00 },
        { 0xee, 0x44, 0x44 }, { 0x00, 0x00, 0x00 }, { 0xee, 0xee, 0xcc }
    };
    UWORD i, c;

    for (i = 0; i < 11; i++)
        for (c = 0; c < 3; c++)
            base->PrefColors[i][c] = rgb[i][c] * 0x01010101UL;
    for (i = 0; i < NUMDRIPENS; i++)
    {
        base->Pens4[i] = _intuition_newlook_pens[i];
        base->Pens8[i] = _intuition_newlook_pens[i];
    }
    base->Pens4[NUMDRIPENS] = (UWORD)~0;
    base->Pens8[NUMDRIPENS] = (UWORD)~0;
    base->HasScreenModePrefs = FALSE;
    base->ScreenFont = NULL;
    /* IControl defaults as written by the 3.1 IControl editor */
    base->IControlPrefs.ic_TimeOut = 50;
    base->IControlPrefs.ic_MetaDrag = IEQUALIFIER_LCOMMAND;
    base->IControlPrefs.ic_Flags = 0x1e;
    base->IControlPrefs.ic_WBtoFront = 'N';
    base->IControlPrefs.ic_FrontToBack = 'M';
    base->IControlPrefs.ic_ReqTrue = 'V';
    base->IControlPrefs.ic_ReqFalse = 'B';
}

/* one colour into a ColorMap and the host palette */
static VOID _intuition_set_screen_color(struct Screen *screen, ULONG index, const ULONG *rgb)
{
    ULONG display_handle = (ULONG)screen->ExtData;

    if (!screen->ViewPort.ColorMap || index >= (ULONG)screen->ViewPort.ColorMap->Count)
        return;
    SetRGB32CM(screen->ViewPort.ColorMap, index, rgb[0], rgb[1], rgb[2]);
    if (display_handle)
        emucall3(EMU_CALL_GFX_SET_COLOR, display_handle, index,
                 ((rgb[0] >> 8) & 0xff0000) | ((rgb[1] >> 16) & 0xff00) | (rgb[2] >> 24));
}

/* The preferences colours of a screen (AmigaOS 3.1 reference): colours 0-3
 * on every screen, the pointer colours 17-19 on screens of 32 colours or
 * more, and on the Workbench (SA_FullPalette) colours 4-7 as the last four
 * colours of a palette of 8 or more. */
static VOID _intuition_apply_pref_colors(struct Screen *screen, BOOL full_palette)
{
    struct LXAIntuitionBase *base = (struct LXAIntuitionBase *)IntuitionBase;
    ULONG ncolors;
    ULONG i;

    if (!screen->ViewPort.ColorMap)
        return;
    ncolors = 1UL << screen->BitMap.Depth;
    for (i = 0; i < 4 && i < ncolors; i++)
        _intuition_set_screen_color(screen, i, base->PrefColors[i]);
    if (full_palette && ncolors >= 8)
        for (i = 0; i < 4; i++)
            _intuition_set_screen_color(screen, ncolors - 4 + i, base->PrefColors[4 + i]);
    if (ncolors >= 32)
        for (i = 0; i < 3; i++)
            _intuition_set_screen_color(screen, 17 + i, base->PrefColors[8 + i]);
}

static struct Screen *_intuition_find_workbench_screen(struct IntuitionBase *IntuitionBase);
static VOID _intuition_register_pubscreen(struct IntuitionBase *IntuitionBase, struct Screen *screen);
ULONG _intuition_OpenWorkBench ( register struct IntuitionBase * IntuitionBase __asm("a6"));
LONG _intuition_CloseWorkBench ( register struct IntuitionBase * IntuitionBase __asm("a6"));

/* graphics.library SetDisplayInfoData() (-750, system private) */
static ULONG _intuition_set_display_info(APTR buf, ULONG size, ULONG tagID, ULONG displayID)
{
    register ULONG d0 __asm("d0") = size;
    register ULONG d1 __asm("d1") = tagID;
    register ULONG d2 __asm("d2") = displayID;
    register APTR a0 __asm("a0") = NULL;
    register APTR a1 __asm("a1") = buf;
    register struct GfxBase *a6 __asm("a6") = GfxBase;

    __asm__ __volatile__ ("jsr -750(%%a6)"
                          : "+r" (d0), "+r" (d1), "+r" (a0), "+r" (a1)
                          : "r" (d2), "r" (a6)
                          : "cc", "memory");
    return d0;
}

static VOID _intuition_set_wb_pens(struct LXAPubScreenNode *entry, struct Screen *screen)
{
    struct LXAIntuitionBase *base = (struct LXAIntuitionBase *)IntuitionBase;
    const UWORD *src = (screen->BitMap.Depth >= 3) ? base->Pens8 : base->Pens4;
    UWORD i;

    for (i = 0; i < NUMDRIPENS; i++)
        entry->pens[i] = src[i];
    entry->pens[NUMDRIPENS] = (UWORD)~0;
}

/* SetIPrefs() (-576, private): IPrefs installs the preferences that belong
 * to Intuition (lxa_iprefs.h).  The values apply to screens opened from now
 * on; the Workbench screen, if open, gets the new palette and pens at once,
 * and a new screen mode reopens it when no windows are open on it. */
ULONG _intuition_SetIPrefs ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                             register APTR data __asm("a0"),
                             register ULONG length __asm("d0"),
                             register ULONG type __asm("d1"))
{
    struct LXAIntuitionBase *base = (struct LXAIntuitionBase *)IntuitionBase;
    struct Screen *wb = _intuition_find_workbench_screen(IntuitionBase);

    DPRINTF(LOG_DEBUG, "_intuition: SetIPrefs() data=0x%08lx length=%lu type=%lu\n",
            (ULONG)data, length, type);
    if (!data)
        return FALSE;

    switch (type)
    {
        case LXA_IPREFS_SCREENMODE:
        {
            const struct LxaIScreenModePrefs *sm = (const struct LxaIScreenModePrefs *)data;

            if (base->HasScreenModePrefs &&
                base->ScreenModePrefs.smp_DisplayID == sm->smp_DisplayID &&
                base->ScreenModePrefs.smp_Width == sm->smp_Width &&
                base->ScreenModePrefs.smp_Height == sm->smp_Height &&
                base->ScreenModePrefs.smp_Depth == sm->smp_Depth &&
                base->ScreenModePrefs.smp_Control == sm->smp_Control)
                return TRUE;
            if (wb && wb->FirstWindow)
                return FALSE;           /* cannot reset the Workbench now */
            base->ScreenModePrefs = *sm;
            base->HasScreenModePrefs = TRUE;
            if (wb && _intuition_CloseWorkBench(IntuitionBase))
                _intuition_OpenWorkBench(IntuitionBase);
            return TRUE;
        }

        case LXA_IPREFS_FONT:
        {
            const struct LxaIFontPrefs *fp = (const struct LxaIFontPrefs *)data;
            struct TextAttr ta = fp->fp_TextAttr;
            struct TextFont *font;

            ta.ta_Name = (STRPTR)fp->fp_Name;
            font = OpenFont(&ta);
            if (!font)
                return FALSE;
            if (!fp->fp_ScrFont)
            {
                /* the system default font stays open for good */
                GfxBase->DefaultFont = font;
            }
            else
            {
                if (base->ScreenFont)
                    CloseFont(base->ScreenFont);
                base->ScreenFont = font;
            }
            return TRUE;
        }

        case LXA_IPREFS_OVERSCAN:
        {
            const struct LxaIOverscanPrefs *op = (const struct LxaIOverscanPrefs *)data;
            struct DimensionInfo dims;

            if (!GetDisplayInfoData(NULL, (UBYTE *)&dims, sizeof(dims), DTAG_DIMS, op->os_DisplayID))
                return FALSE;
            dims.TxtOScan.MinX = 0;
            dims.TxtOScan.MinY = 0;
            dims.TxtOScan.MaxX = op->os_Text.x - 1;
            dims.TxtOScan.MaxY = op->os_Text.y - 1;
            dims.StdOScan = op->os_Standard;
            return _intuition_set_display_info(&dims, sizeof(dims), DTAG_DIMS,
                                               op->os_DisplayID) ? TRUE : FALSE;
        }

        case LXA_IPREFS_ICONTROL:
            CopyMem(data, &base->IControlPrefs,
                    length < sizeof(base->IControlPrefs) ? length : sizeof(base->IControlPrefs));
            return TRUE;

        case LXA_IPREFS_POINTER:
        {
            const struct LxaIPointerPrefs *pp = (const struct LxaIPointerPrefs *)data;
            UWORD which = pp->Which ? 1 : 0;
            ULONG rows = pp->BitMap ? pp->BitMap->Rows : 0;
            ULONG words = pp->BitMap ? pp->BitMap->BytesPerRow / 2 : 0;
            UWORD *copy;
            ULONG r, w, pl;

            /* keep a copy of the image (both planes, row by row) */
            copy = (rows && words) ? AllocVec(rows * words * 2 * sizeof(UWORD), MEMF_PUBLIC) : NULL;
            if (copy)
            {
                for (pl = 0; pl < 2; pl++)
                    for (r = 0; r < rows; r++)
                        for (w = 0; w < words; w++)
                            copy[(pl * rows + r) * words + w] = pp->BitMap->Depth > pl
                                ? ((UWORD *)pp->BitMap->Planes[pl])[r * words + w] : 0;
            }
            if (base->PointerData[which])
                FreeVec(base->PointerData[which]);
            base->PointerData[which] = copy;
            base->PointerPrefs[which] = *pp;
            base->PointerPrefs[which].BitMap = NULL;
            /* the AmigaOS 3.x convention for this call */
            return (ULONG)-1;
        }

        case LXA_IPREFS_PALETTE:
        {
            const struct ColorSpec *cs = (const struct ColorSpec *)data;

            for (; cs->ColorIndex != -1; cs++)
            {
                if (cs->ColorIndex < 0 || cs->ColorIndex > 10)
                    continue;
                base->PrefColors[cs->ColorIndex][0] = (ULONG)cs->Red * 0x10001UL;
                base->PrefColors[cs->ColorIndex][1] = (ULONG)cs->Green * 0x10001UL;
                base->PrefColors[cs->ColorIndex][2] = (ULONG)cs->Blue * 0x10001UL;
            }
            if (wb)
                _intuition_apply_pref_colors(wb, TRUE);
            return TRUE;
        }

        case LXA_IPREFS_PENS:
        {
            const struct LxaIPenPrefs *pp = (const struct LxaIPenPrefs *)data;
            UWORD *dst = pp->Type ? base->Pens8 : base->Pens4;
            UWORD i;

            for (i = 0; i < NUMDRIPENS && pp->PenTable[i] != (UWORD)~0; i++)
                dst[i] = pp->PenTable[i];
            if (wb && ((wb->BitMap.Depth >= 3) == (pp->Type != 0)))
            {
                struct PubScreenNode *pub = _intuition_find_pubscreen_by_screen(base, wb);
                if (pub)
                    _intuition_set_wb_pens((struct LXAPubScreenNode *)pub, wb);
            }
            return TRUE;
        }

        default:
            return FALSE;
    }
}

static VOID _intuition_register_pubscreen(struct IntuitionBase *IntuitionBase, struct Screen *screen)
{
    struct LXAIntuitionBase *base = (struct LXAIntuitionBase *)IntuitionBase;
    struct LXAPubScreenNode *entry;
    const char *name;
    char *namebuf;
    ULONG size;
    ULONG len;

    if (!base || !screen || _intuition_find_pubscreen_by_screen(base, screen))
        return;

    name = _intuition_pubscreen_name_for_screen(screen);
    len = strlen(name);
    size = sizeof(struct LXAPubScreenNode) + len + 1;

    entry = (struct LXAPubScreenNode *)AllocMem(size, MEMF_PUBLIC | MEMF_CLEAR);
    if (!entry)
        return;

    namebuf = (char *)(entry + 1);
    strcpy(namebuf, name);

    entry->pub.psn_Node.ln_Name = namebuf;
    entry->pub.psn_Screen = screen;
    entry->pub.psn_Flags = 0;
    entry->pub.psn_Size = (WORD)size;
    entry->pub.psn_VisitorCount = 0;
    entry->pub.psn_SigTask = NULL;
    entry->pub.psn_SigBit = 0;

    /* Initialize default pen array.  AmigaOS 3.1 reference: the Workbench
     * and every screen opened with SA_Pens get the 3D "new look" pens; a
     * custom screen without SA_Pens keeps the pre-V36 pens (DRIF_NEWLOOK
     * clear, dri_Pens 0 1 1 1 1 1 0 0 1 0 1 1). */
    entry->pens[DETAILPEN]        = 0;
    entry->pens[BLOCKPEN]         = 1;
    entry->pens[TEXTPEN]          = 1;
    entry->pens[SHINEPEN]         = 2;
    entry->pens[SHADOWPEN]        = 1;
    entry->pens[FILLPEN]          = 3;
    entry->pens[FILLTEXTPEN]      = 1;
    entry->pens[BACKGROUNDPEN]    = 0;
    entry->pens[HIGHLIGHTTEXTPEN] = 2;
    entry->pens[BARDETAILPEN]     = 1;
    entry->pens[BARBLOCKPEN]      = 2;
    entry->pens[BARTRIMPEN]       = 1;
    entry->pens[BARCONTOURPEN]    = 1;
    entry->pens[NUMDRIPENS]       = (UWORD)~0;  /* Terminator */
    entry->has_custom_pens = FALSE;

    /* Pre-build DrawInfo for GetScreenDrawInfo() */
    entry->drawInfo.dri_Version    = 2;  /* V39 compatible */
    entry->drawInfo.dri_NumPens    = 12;     /* AmigaOS 3.1 (V40) */
    entry->drawInfo.dri_Pens       = entry->pens;
    entry->drawInfo.dri_Font       = screen->RastPort.Font;
    entry->drawInfo.dri_Depth      = screen->RastPort.BitMap ? screen->RastPort.BitMap->Depth : 2;
    /* reference: 22:44 on hires screens, 44:44 on lores screens */
    entry->drawInfo.dri_Resolution.X = (screen->Flags & SCREENHIRES) ? 22 : 44;
    entry->drawInfo.dri_Resolution.Y = (screen->ViewPort.Modes & LACE) ? 22 : 44;
    entry->drawInfo.dri_Flags      = DRIF_NEWLOOK;
    if ((screen->Flags & SCREENTYPE) != WBENCHSCREEN)
        _intuition_set_oldlook_pens(entry);
    else
        _intuition_set_wb_pens(entry, screen);
    entry->drawInfo.dri_CheckMark  = NULL;
    entry->drawInfo.dri_AmigaKey   = NULL;

    AddTail((struct List *)&base->ScreenDataList, (struct Node *)&entry->all_node);

    /* Only the Workbench screen is public by itself; a custom screen becomes
     * public through SA_PubName (AmigaOS 3.1 reference). */
    if ((screen->Flags & SCREENTYPE) == WBENCHSCREEN)
    {
        AddTail(&base->PubScreenList, &entry->pub.psn_Node);
        entry->is_public = TRUE;
    }
}

/* SA_PubName: enter the screen into the public screen list, private until
 * PubScreenStatus() makes it public. */
static VOID _intuition_publish_screen(struct LXAIntuitionBase *base, struct LXAPubScreenNode *entry)
{
    if (!base || !entry || entry->is_public)
        return;

    entry->pub.psn_Flags = PSNF_PRIVATE;
    AddTail(&base->PubScreenList, &entry->pub.psn_Node);
    entry->is_public = TRUE;
}

static VOID _intuition_unregister_pubscreen(struct IntuitionBase *IntuitionBase, struct Screen *screen)
{
    struct LXAIntuitionBase *base = (struct LXAIntuitionBase *)IntuitionBase;
    struct PubScreenNode *pub;

    if (!base || !screen)
        return;

    pub = _intuition_find_pubscreen_by_screen(base, screen);
    if (!pub)
        return;

    Remove((struct Node *)&((struct LXAPubScreenNode *)pub)->all_node);
    if (((struct LXAPubScreenNode *)pub)->is_public)
        Remove(&pub->psn_Node);

    if (base->DefaultPubScreen == screen)
        base->DefaultPubScreen = NULL;

    FreeMem(pub, pub->psn_Size);

    if (!base->DefaultPubScreen)
    {
        struct PubScreenNode *default_pub = _intuition_default_pubscreen_node(base);
        if (default_pub)
            base->DefaultPubScreen = default_pub->psn_Screen;
    }
}

/* Rootclass dispatcher */
static ULONG rootclass_dispatch(
    register struct IClass *cl __asm("a0"),
    register Object *obj __asm("a2"),
    register Msg msg __asm("a1"))
{
    switch (msg->MethodID) {
        case OM_NEW:
        {
            /* AmigaOS convention (RKRM Libraries, BOOPSI): NewObject()
             * sends OM_NEW with the *true class* in place of the object.
             * rootclass allocates the instance for the whole class chain
             * and returns the new object; subclasses initialise their data
             * after DoSuperMethod() returned it.  (MUI walks the chain from
             * that true class itself.) */
            struct IClass *true_class = (struct IClass *)obj;
            ULONG size = SIZEOF_INSTANCE(true_class);
            struct _Object *o = AllocMem(size, MEMF_PUBLIC | MEMF_CLEAR);

            (void)cl;
            if (!o)
                return 0;
            o->o_Class = true_class;
            true_class->cl_ObjectCount++;
            return (ULONG)BASEOBJECT(o);
        }

        case OM_ADDTAIL:
        {
            struct opAddTail *opat = (struct opAddTail *)msg;
            if (!opat->opat_List)
                return 0;
            AddTail(opat->opat_List, (struct Node *)&_OBJECT(obj)->o_Node);
            return 1;
        }

        case OM_REMOVE:
            Remove((struct Node *)&_OBJECT(obj)->o_Node);
            return 1;
            
        case OM_DISPOSE:
        {
            /* rootclass frees the instance that its OM_NEW allocated */
            struct _Object *o = _OBJECT(obj);
            struct IClass *true_class = o->o_Class;

            if (true_class)
            {
                if (true_class->cl_ObjectCount > 0)
                    true_class->cl_ObjectCount--;
                FreeMem(o, SIZEOF_INSTANCE(true_class));
            }
            return 0;
        }

        case OM_SET:
        case OM_GET:
        case OM_UPDATE:
        case OM_NOTIFY:
            return 0;
    }
    return 0;
}

/* Forward declarations for BOOPSI class dispatchers */
static ULONG icclass_dispatch(register struct IClass *cl __asm("a0"),
                               register Object *obj __asm("a2"),
                               register Msg msg __asm("a1"));
static ULONG modelclass_dispatch(register struct IClass *cl __asm("a0"),
                                  register Object *obj __asm("a2"),
                                  register Msg msg __asm("a1"));
static ULONG gadgetclass_dispatch(register struct IClass *cl __asm("a0"),
                                  register Object *obj __asm("a2"),
                                  register Msg msg __asm("a1"));
static ULONG buttongclass_dispatch(register struct IClass *cl __asm("a0"),
                                   register Object *obj __asm("a2"),
                                   register Msg msg __asm("a1"));
static ULONG propgclass_dispatch(register struct IClass *cl __asm("a0"),
                                 register Object *obj __asm("a2"),
                                 register Msg msg __asm("a1"));
static ULONG strgclass_dispatch(register struct IClass *cl __asm("a0"),
                                register Object *obj __asm("a2"),
                                register Msg msg __asm("a1"));

#define VERSION    40
#define REVISION   85
#define EXLIBNAME  "intuition"
#define EXLIBVER   " 40.85 (5.5.93)\r\n"

char __aligned _g_intuition_ExLibName [] = EXLIBNAME ".library";
char __aligned _g_intuition_ExLibID   [] = EXLIBNAME EXLIBVER;
char __aligned _g_intuition_Copyright [] = "(C)opyright 2022 by G. Bartsch. Licensed under the MIT License.";

char __aligned _g_intuition_VERSTRING [] = "\0$VER: " EXLIBNAME EXLIBVER;

extern struct ExecBase      *SysBase;

/* Forward declarations */
ULONG _intuition_OpenWorkBench ( register struct IntuitionBase * IntuitionBase __asm("a6"));
struct Screen * _intuition_OpenScreen ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                        register const struct NewScreen * newScreen __asm("a0"));

/* Forward declaration for internal helper */
static BOOL _post_idcmp_message(struct Window *window, ULONG class, UWORD code, 
                                 UWORD qualifier, APTR iaddress, WORD mouseX, WORD mouseY);

static void _draw_bevel_box(struct RastPort *rp, WORD left, WORD top, WORD width, WORD height, 
                           ULONG flags, const struct DrawInfo *drInfo);

/*
 * Proportional gadget geometry, as AmigaOS 3.1 lays it out
 * (tests/probes/intuition/propgad.ref.out):
 *  - the container is the whole gadget box (PropInfo CWidth/CHeight);
 *  - the knob moves inside it, inset by LeftBorder/TopBorder on each side:
 *    0/0 with PROPBORDERLESS, 1/1 with PROPNEWLOOK, 4/2 otherwise;
 *  - along a free axis the knob is range * Body / MAXBODY + 1 pixels
 *    (at least KNOBHMIN/KNOBVMIN, at most the range), otherwise the range;
 *  - its offset is (range - knob) * Pot / MAXPOT.
 */
struct PropLayout
{
    WORD lb, tb;        /* knob range inset */
    WORD rw, rh;        /* knob range */
    WORD kx, ky;        /* knob offset in the range */
    WORD kw, kh;        /* knob size */
};

static WORD _prop_knob_size(WORD range, UWORD body, BOOL free_axis, WORD minimum)
{
    WORD k;

    if (range <= 0)
        return 0;
    if (!free_axis)
        return range;
    k = (WORD)(((ULONG)range * (ULONG)body) / MAXBODY + 1);
    if (k < minimum)
        k = minimum;
    if (k > range)
        k = range;
    return k;
}

static void _prop_layout(const struct PropInfo *pi, WORD width, WORD height, struct PropLayout *pl)
{
    if (pi->Flags & PROPBORDERLESS)
        pl->lb = pl->tb = 0;
    else if (pi->Flags & PROPNEWLOOK)
        pl->lb = pl->tb = 1;
    else
    {
        pl->lb = 4;
        pl->tb = 2;
    }
    pl->rw = width - 2 * pl->lb;
    pl->rh = height - 2 * pl->tb;
    if (pl->rw < 0)
        pl->rw = 0;
    if (pl->rh < 0)
        pl->rh = 0;
    pl->kw = _prop_knob_size(pl->rw, pi->HorizBody, (pi->Flags & FREEHORIZ) != 0, KNOBHMIN);
    pl->kh = _prop_knob_size(pl->rh, pi->VertBody, (pi->Flags & FREEVERT) != 0, KNOBVMIN);
    pl->kx = (WORD)(((ULONG)(pl->rw - pl->kw) * (ULONG)pi->HorizPot) / MAXPOT);
    pl->ky = (WORD)(((ULONG)(pl->rh - pl->kh) * (ULONG)pi->VertPot) / MAXPOT);
}

/* the level range of a GadTools slider (lxa_gadtools.c); FALSE for other gadgets */
extern BOOL _gadtools_GetSliderRange(register struct Gadget *gad __asm("a0"),
                                     register LONG *min __asm("a1"),
                                     register LONG *max __asm("a2"));

/* the level a horizontal slider reports for its current HorizPot */
static LONG _prop_slider_level(struct Gadget *gad, const struct PropInfo *pi)
{
    LONG sl_min = 0, sl_max = 0;

    if (!_gadtools_GetSliderRange(gad, &sl_min, &sl_max))
        return 0;
    if (sl_max > sl_min)
        return sl_min + ((LONG)pi->HorizPot * (sl_max - sl_min) + 0x7FFF) / 0xFFFF;
    return sl_min;
}

/* Forward declarations for gadget rendering helpers */
static void _complement_gadget_area(struct Window *window, struct Requester *req, struct Gadget *gad);
static void _render_gadget(struct Window *window, struct Requester *req, struct Gadget *gad);
static void _render_window_user_gadgets(struct Window *window);
static void _free_window_sys_gadgets(struct Window *window);
static BOOL _is_sys_gadget(const struct Gadget *gad);
static void _render_sys_gadget(struct Window *window, struct Gadget *gad);
static void _render_requester(struct Window *window, struct Requester *req);
static void _calculate_requester_box(struct Window *window, struct Requester *req,
                                     LONG *left, LONG *top,
                                     LONG *width, LONG *height);
static void _calculate_gadget_box(struct Window *window, struct Requester *req,
                                  struct Gadget *gad,
                                  LONG *left, LONG *top,
                                  LONG *width, LONG *height);
static void _rerender_requester_stack(struct Window *window);
static struct Window * _find_window_at_pos(struct Screen *screen, WORD x, WORD y);
static struct Gadget * _find_gadget_at_pos(struct Window *window, WORD relX, WORD relY);
static BOOL _point_in_gadget(struct Window *window, struct Gadget *gad, WORD relX, WORD relY);
static struct Menu * _find_menu_at_x(struct Window *window, WORD screenX);
static struct MenuItem * _find_item_at_pos(struct Menu *menu, WORD x, WORD y);
static struct MenuItem * _find_item_in_chain_at_pos(struct MenuItem *firstItem, WORD x, WORD y);
static void _menu_item_origin(struct Screen *screen, struct Menu *menu, WORD *ox, WORD *oy);
static BOOL _get_menu_submenu_origin(struct Window *window, struct Menu *menu,
                                     struct MenuItem *item, WORD *ox, WORD *oy);
VOID _intuition_PrintIText ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                             register struct RastPort * rp __asm("a0"),
                             register const struct IntuiText * iText __asm("a1"),
                             register WORD left __asm("d0"),
                             register WORD top __asm("d1"));
static BOOL _get_active_submenu_box(struct Window *window,
                                    WORD *submenuX,
                                    WORD *submenuY,
                                    WORD *submenuWidth,
                                    WORD *submenuHeight);
static BOOL _menu_hover_redraw_can_repaint_in_place(struct Window *window,
                                                    struct Menu *oldMenu,
                                                    struct MenuItem *oldItem);
static void _restore_menu_dropdown_area(struct Screen *screen);
static void _save_dropdown_for_menu(struct Window *window, struct Menu *menu);
static void _render_menu_bar(struct Window *window);
static const UWORD *_intuition_screen_pens(struct Screen *screen);
static void _render_menu_items(struct Window *window);
static void _render_menu_items_in_place(struct Window *window);
static void _enter_menu_mode(struct Window *window, struct Screen *screen, WORD mouseX, WORD mouseY);
static void _exit_menu_mode(struct Window *window, WORD mouseX, WORD mouseY);
static UWORD _find_menu_commkey(struct Menu *strip, char key);
static void _menu_check_item(struct MenuItem *chain, struct MenuItem *item, BOOL mark_toggled);
static void _menu_sync_drawn_flags(void);
static void _handle_sys_gadget_verify(struct Window *window, struct Gadget *gadget);
static void _compute_idcmp_mouse_coords(struct Window *window, ULONG class,
                                         WORD absX, WORD absY,
                                         WORD *outX, WORD *outY);

VOID _intuition_SizeWindow(register struct IntuitionBase *IntuitionBase __asm("a6"),
                           register struct Window *window __asm("a0"),
                           register WORD dx __asm("d0"),
                           register WORD dy __asm("d1"));

static struct Gadget *g_active_gadget;
static struct Window *g_active_window;
static WORD g_prop_click_offset;

/* screen x of the knob range and the layout of an AUTOKNOB prop gadget */
static void _prop_screen_layout(struct Window *window, struct Gadget *gad, struct PropInfo *pi,
                                WORD *range_left, struct PropLayout *pl)
{
    LONG l, t, w, h;

    _calculate_gadget_box(window, NULL, gad, &l, &t, &w, &h);
    _prop_layout(pi, (WORD)w, (WORD)h, pl);
    *range_left = (WORD)(window->LeftEdge + l + pl->lb);
}

/* set HorizPot so that the knob's left edge lies at screen x knob_left */
static void _prop_set_hpot(struct PropInfo *pi, const struct PropLayout *pl, WORD range_left, WORD knob_left)
{
    WORD max_move = pl->rw - pl->kw;

    if (knob_left < range_left)
        knob_left = range_left;
    if (knob_left > range_left + max_move)
        knob_left = range_left + max_move;
    pi->HorizPot = max_move > 0 ? (UWORD)(((ULONG)(knob_left - range_left) * MAXPOT) / (ULONG)max_move) : 0;
}

/* SELECTDOWN on an AUTOKNOB prop gadget: remember where the knob was
 * grabbed; a click beside the knob centres it on the pointer */
static void _prop_select_down(struct Window *window, struct Gadget *gad, WORD mouseX)
{
    struct PropInfo *pi = (struct PropInfo *)gad->SpecialInfo;
    struct PropLayout pl;
    WORD range_left, knob_left;

    if (!pi || !(pi->Flags & AUTOKNOB))
        return;
    _prop_screen_layout(window, gad, pi, &range_left, &pl);
    knob_left = range_left + pl.kx;
    if (mouseX >= knob_left && mouseX < knob_left + pl.kw)
    {
        g_prop_click_offset = mouseX - knob_left;
        return;
    }
    g_prop_click_offset = pl.kw / 2;
    _prop_set_hpot(pi, &pl, range_left, mouseX - g_prop_click_offset);
    _gadtools_UpdateSliderLevelDisplay(gad, _prop_slider_level(gad, pi));
    _render_gadget(window, NULL, gad);
}

/* pointer moved while a horizontal AUTOKNOB prop gadget is held: TRUE when
 * the pot changed (the gadget is then redrawn and *level is set) */
static BOOL _prop_drag(struct Window *window, struct Gadget *gad, WORD mouseX, LONG *level)
{
    struct PropInfo *pi = (struct PropInfo *)gad->SpecialInfo;
    struct PropLayout pl;
    WORD range_left;
    UWORD old_pot;

    if (!pi || !(pi->Flags & AUTOKNOB) || !(pi->Flags & FREEHORIZ))
        return FALSE;
    old_pot = pi->HorizPot;
    _prop_screen_layout(window, gad, pi, &range_left, &pl);
    _prop_set_hpot(pi, &pl, range_left, mouseX - g_prop_click_offset);
    if (pi->HorizPot == old_pot)
        return FALSE;
    _render_gadget(window, NULL, gad);
    *level = _prop_slider_level(gad, pi);
    _gadtools_UpdateSliderLevelDisplay(gad, *level);
    return TRUE;
}

static LONG g_gt_down_code;     /* GADGETDOWN code of a GadTools MX click */
static UWORD g_intuitick_counter;   /* VBlank counter for IDCMP_INTUITICKS (fires every 10th VBlank) */
static UWORD g_current_qualifier;   /* last known input qualifier (for INTUITICKS messages) */
static BOOL g_menu_mode;
static struct Window *g_menu_window;
static struct Menu *g_active_menu;
static struct MenuItem *g_active_item;
static struct MenuItem *g_active_subitem;
/* the menu, item and sub-item whose MIDRAWN/HIGHITEM/ISDRAWN flags this
 * session set (only valid while g_menu_mode; see _menu_sync_drawn_flags) */
static struct Menu *g_flagged_menu;
static struct MenuItem *g_flagged_item;
static struct MenuItem *g_flagged_subitem;
static BOOL g_dragging_window;
static struct Window *g_drag_window;
static WORD g_drag_start_x;
static WORD g_drag_start_y;
static WORD g_drag_window_x;
static WORD g_drag_window_y;
static BOOL g_sizing_window;
static struct Window *g_size_window;
static WORD g_size_start_x;
static WORD g_size_start_y;
static WORD g_size_orig_w;
static WORD g_size_orig_h;
static WORD g_prev_abs_mouse_x;  /* previous absolute mouse X for IDCMP_DELTAMOVE */
static WORD g_prev_abs_mouse_y;  /* previous absolute mouse Y for IDCMP_DELTAMOVE */

/* Forward declarations for EasyRequest infrastructure */
struct Window * _intuition_BuildEasyRequestArgs ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                   register struct Window * window __asm("a0"),
                                                  register const struct EasyStruct * easyStruct __asm("a1"),
                                                  register ULONG idcmp __asm("d0"),
                                                  register const APTR args __asm("a3"));
LONG _intuition_SysReqHandler ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                register struct Window * window __asm("a0"),
                                register ULONG * idcmpFlags __asm("a1"),
                                register LONG waitInput __asm("d0"));
VOID _intuition_FreeSysRequest ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                 register struct Window * window __asm("a0"));

/* Data stored in window->UserData for cleanup by FreeSysRequest */
struct EasyReqData {
    APTR   gadget_mem;
    LONG   gadget_mem_size;
    APTR   text_mem;
    LONG   text_mem_size;
    APTR   gadget_text_mem;
    LONG   gadget_text_mem_size;
    APTR   border_mem;
    LONG   border_mem_size;
    APTR   itext_mem;
    LONG   itext_mem_size;
    APTR   border_xy_mem;
    LONG   border_xy_mem_size;
    WORD   num_gadgets;
};

/******************************************************************************
 * BOOPSI Inter-Object Communication (IC) Data Structures
 *
 * ICData is embedded in gadgetclass and icclass instances to support
 * BOOPSI notification forwarding via ICA_TARGET / ICA_MAP.
 ******************************************************************************/

struct ICData {
    Object          *ic_Target;     /* Target object for OM_UPDATE forwarding */
    struct TagItem  *ic_Mapping;    /* Tag mapping list (shared, NOT cloned) */
    ULONG            ic_LoopCounter; /* Loop prevention counter */
};

/* Instance data for propgclass (allocated as part of the BOOPSI object) */
struct PropGData {
    struct PropInfo  propinfo;       /* Embedded PropInfo structure */
    UWORD            top;            /* Logical top (scrollbar) */
    UWORD            visible;        /* Logical visible (scrollbar) */
    UWORD            total;          /* Logical total (scrollbar) */
};

/* Instance data for strgclass (allocated as part of the BOOPSI object) */
struct StrGData {
    struct StringInfo strinfo;       /* Embedded StringInfo structure */
    UBYTE            buffer[128];    /* Default text buffer */
    UBYTE            undobuffer[128];/* Default undo buffer */
    LONG             longval;        /* Integer value (for STRINGA_LongVal) */
};

/* Instance data for modelclass (allocated as part of icclass + model extension) */
struct ModelData {
    struct MinList   memberlist;     /* List of member objects */
};

/*
 * ZipWindow zoom data — stored via window->ExtData.
 * Holds the "alternate" position/size for ZipWindow toggling,
 * and a flag indicating which state the window is currently in.
 */
struct ZoomData {
    WORD   zd_Left;         /* Alternate left edge */
    WORD   zd_Top;          /* Alternate top edge */
    WORD   zd_Width;        /* Alternate width */
    WORD   zd_Height;       /* Alternate height */
    BOOL   zd_IsZoomed;     /* TRUE if currently in zoomed (alternate) state */
};

/*
 * Access the embedded ICData for a gadgetclass object.
 * In gadgetclass, cl_InstSize = sizeof(struct Gadget) + sizeof(struct ICData).
 * The Gadget struct starts at offset 0 (the public obj pointer), and the
 * ICData is located immediately after it. Subclasses (propgclass, strgclass)
 * do NOT contain their own ICData — they inherit it from gadgetclass.
 */
#define GADGET_ICDATA(obj) ((struct ICData *)((UBYTE *)(obj) + sizeof(struct Gadget)))

/*
 * _boopsi_do_notify - Forward OM_NOTIFY/OM_UPDATE to the IC target.
 *
 * This is the heart of BOOPSI inter-object communication.
 * It clones the tag list, maps tags via ICA_MAP, and sends
 * OM_UPDATE to the target object (or IDCMP_IDCMPUPDATE to the window).
 *
 * Loop prevention: ic_LoopCounter is incremented before forwarding
 * and decremented after. If counter > 0 on entry, notification is skipped.
 */
static void _boopsi_do_notify(struct IClass *cl, Object *obj, struct ICData *ic, struct opUpdate *msg)
{
    struct TagItem *tags;
    struct TagItem *clone;
    ULONG numTags;
    
    if (!ic || !ic->ic_Target || !msg->opu_AttrList)
        return;
    
    /* Loop prevention check */
    if (ic->ic_LoopCounter > 0)
        return;
    
    ic->ic_LoopCounter++;
    
    /* Count tags for cloning */
    numTags = 0;
    tags = msg->opu_AttrList;
    while (tags->ti_Tag != TAG_END) {
        if (tags->ti_Tag == TAG_IGNORE) {
            tags++;
            continue;
        }
        if (tags->ti_Tag == TAG_SKIP) {
            tags += 1 + tags->ti_Data;
            continue;
        }
        if (tags->ti_Tag == TAG_MORE) {
            tags = (struct TagItem *)tags->ti_Data;
            continue;
        }
        numTags++;
        tags++;
    }
    
    if (numTags == 0) {
        ic->ic_LoopCounter--;
        return;
    }
    
    /* Clone the tag list */
    clone = AllocMem((numTags + 1) * sizeof(struct TagItem), MEMF_PUBLIC);
    if (!clone) {
        ic->ic_LoopCounter--;
        return;
    }
    
    {
        ULONG i = 0;
        tags = msg->opu_AttrList;
        while (tags->ti_Tag != TAG_END && i < numTags) {
            if (tags->ti_Tag == TAG_IGNORE) {
                tags++;
                continue;
            }
            if (tags->ti_Tag == TAG_SKIP) {
                tags += 1 + tags->ti_Data;
                continue;
            }
            if (tags->ti_Tag == TAG_MORE) {
                tags = (struct TagItem *)tags->ti_Data;
                continue;
            }
            clone[i].ti_Tag = tags->ti_Tag;
            clone[i].ti_Data = tags->ti_Data;
            i++;
            tags++;
        }
        clone[i].ti_Tag = TAG_END;
        clone[i].ti_Data = 0;
    }
    
    /* Apply tag mapping if we have one */
    if (ic->ic_Mapping) {
        MapTags(clone, ic->ic_Mapping, MAP_KEEP_NOT_FOUND);
    }
    
    /* Forward to target */
    if ((ULONG)ic->ic_Target == ICTARGET_IDCMP) {
        /* Send as IDCMP_IDCMPUPDATE to the window from GadgetInfo */
        struct GadgetInfo *gi = msg->opu_GInfo;
        if (gi && gi->gi_Window) {
            _post_idcmp_message(gi->gi_Window, IDCMP_IDCMPUPDATE, 0, 0, 
                               (APTR)clone, 0, 0);
            /* Note: clone is freed by the IDCMP message handler */
            ic->ic_LoopCounter--;
            return;
        }
    } else {
        /* Send OM_UPDATE to target object */
        struct opUpdate update;
        update.MethodID = OM_UPDATE;
        update.opu_AttrList = clone;
        update.opu_GInfo = msg->opu_GInfo;
        update.opu_Flags = msg->opu_Flags;
        
        struct _Object *target_obj = _OBJECT(ic->ic_Target);
        if (target_obj && target_obj->o_Class) {
            _intuition_dispatch_method(target_obj->o_Class, ic->ic_Target, (Msg)&update);
        }
    }
    
    FreeMem(clone, (numTags + 1) * sizeof(struct TagItem));
    ic->ic_LoopCounter--;
}

/*
 * _boopsi_set_icdata - Process ICA_TARGET and ICA_MAP tags.
 * Returns TRUE if any IC attributes were changed.
 */
static BOOL _boopsi_set_icdata(struct ICData *ic, struct TagItem *tags)
{
    struct TagItem *tag;
    BOOL changed = FALSE;
    
    if (!ic || !tags)
        return FALSE;
    
    while ((tag = NextTagItem(&tags))) {
        switch (tag->ti_Tag) {
            case ICA_TARGET:
                ic->ic_Target = (Object *)tag->ti_Data;
                changed = TRUE;
                break;
            case ICA_MAP:
                ic->ic_Mapping = (struct TagItem *)tag->ti_Data;
                changed = TRUE;
                break;
        }
    }
    return changed;
}

/*
 * _boopsi_free_icdata - Clean up IC data (reset loop counter).
 */
static void _boopsi_free_icdata(struct ICData *ic)
{
    if (!ic) return;
    ic->ic_LoopCounter = 0;
    ic->ic_Target = NULL;
    ic->ic_Mapping = NULL;
}

/******************************************************************************
 * BOOPSI icclass dispatcher — Interconnection class
 *
 * icclass is a subclass of rootclass that adds ICA_TARGET/ICA_MAP support.
 * It is the base class for inter-object communication.
 ******************************************************************************/

static ULONG icclass_dispatch(
    register struct IClass *cl __asm("a0"),
    register Object *obj __asm("a2"),
    register Msg msg __asm("a1"))
{
    struct ICData *ic = (struct ICData *)INST_DATA(cl, obj);
    
    switch (msg->MethodID) {
        case OM_NEW: {
            /* Call superclass first (rootclass) */
            struct IClass *super = cl->cl_Super;
            if (super && super->cl_Dispatcher.h_Entry) {
                typedef ULONG (*DispatchEntry)(register struct IClass *cl __asm("a0"),
                                                register Object *obj __asm("a2"),
                                                register Msg msg __asm("a1"));
                DispatchEntry entry = (DispatchEntry)super->cl_Dispatcher.h_Entry;
                /* AmigaOS: OM_NEW is sent with the true class as the
                 * object; rootclass allocates and returns the object */
                obj = (Object *)entry(super, obj, msg);
                if (!obj)
                    return 0;
                ic = (struct ICData *)INST_DATA(cl, obj);
            }
            
            /* Initialize IC data */
            ic->ic_Target = NULL;
            ic->ic_Mapping = NULL;
            ic->ic_LoopCounter = 0;
            
            /* Process ICA_TARGET/ICA_MAP from tags */
            {
                struct opSet *ops = (struct opSet *)msg;
                _boopsi_set_icdata(ic, ops->ops_AttrList);
            }
            
            return (ULONG)obj;
        }
        
        case OM_DISPOSE:
            _boopsi_free_icdata(ic);
            /* Call superclass */
            {
                struct IClass *super = cl->cl_Super;
                if (super && super->cl_Dispatcher.h_Entry) {
                    typedef ULONG (*DispatchEntry)(register struct IClass *cl __asm("a0"),
                                                    register Object *obj __asm("a2"),
                                                    register Msg msg __asm("a1"));
                    DispatchEntry entry = (DispatchEntry)super->cl_Dispatcher.h_Entry;
                    return entry(super, obj, msg);
                }
            }
            return 0;
        
        case OM_SET: {
            struct opSet *ops = (struct opSet *)msg;
            _boopsi_set_icdata(ic, ops->ops_AttrList);
            return 0;
        }
        
        case OM_NOTIFY:
        case OM_UPDATE:
            _boopsi_do_notify(cl, obj, ic, (struct opUpdate *)msg);
            return 0;
        
        case ICM_SETLOOP:
            ic->ic_LoopCounter++;
            return 0;
        
        case ICM_CLEARLOOP:
            if (ic->ic_LoopCounter > 0)
                ic->ic_LoopCounter--;
            return 0;
        
        case ICM_CHECKLOOP:
            return (ic->ic_LoopCounter > 0) ? TRUE : FALSE;
    }
    
    /* Call superclass for unhandled methods */
    {
        struct IClass *super = cl->cl_Super;
        if (super && super->cl_Dispatcher.h_Entry) {
            typedef ULONG (*DispatchEntry)(register struct IClass *cl __asm("a0"),
                                            register Object *obj __asm("a2"),
                                            register Msg msg __asm("a1"));
            DispatchEntry entry = (DispatchEntry)super->cl_Dispatcher.h_Entry;
            return entry(super, obj, msg);
        }
    }
    
    return 0;
}

/******************************************************************************
 * BOOPSI modelclass dispatcher — Broadcast model class
 *
 * modelclass is a subclass of icclass that broadcasts OM_UPDATE to a list
 * of member objects. Each member can be added/removed via OM_ADDMEMBER/OM_REMMEMBER.
 ******************************************************************************/

static ULONG modelclass_dispatch(
    register struct IClass *cl __asm("a0"),
    register Object *obj __asm("a2"),
    register Msg msg __asm("a1"))
{
    struct ModelData *md = (struct ModelData *)INST_DATA(cl, obj);
    
    switch (msg->MethodID) {
        case OM_NEW: {
            /* Call superclass first (icclass) */
            struct IClass *super = cl->cl_Super;
            if (super && super->cl_Dispatcher.h_Entry) {
                typedef ULONG (*DispatchEntry)(register struct IClass *cl __asm("a0"),
                                                register Object *obj __asm("a2"),
                                                register Msg msg __asm("a1"));
                DispatchEntry entry = (DispatchEntry)super->cl_Dispatcher.h_Entry;
                /* AmigaOS: OM_NEW is sent with the true class as the
                 * object; rootclass allocates and returns the object */
                obj = (Object *)entry(super, obj, msg);
                if (!obj)
                    return 0;
                md = (struct ModelData *)INST_DATA(cl, obj);
            }
            
            /* Initialize member list */
            NewList((struct List *)&md->memberlist);
            
            return (ULONG)obj;
        }
        
        case OM_DISPOSE: {
            /* Dispose all members first */
            struct MinNode *node = md->memberlist.mlh_Head;
            while (node->mln_Succ) {
                struct MinNode *next = node->mln_Succ;
                /* Remove and dispose member object */
                Remove((struct Node *)node);
                {
                    struct { ULONG MethodID; } dispose_msg;
                    dispose_msg.MethodID = OM_DISPOSE;
                    Object *member = (Object *)((UBYTE *)node + sizeof(struct _Object));
                    struct _Object *mobj = _OBJECT(member);
                    if (mobj && mobj->o_Class) {
                        _intuition_dispatch_method(mobj->o_Class, member, (Msg)&dispose_msg);
                    }
                }
                node = next;
            }
            /* Call superclass */
            {
                struct IClass *super = cl->cl_Super;
                if (super && super->cl_Dispatcher.h_Entry) {
                    typedef ULONG (*DispatchEntry)(register struct IClass *cl __asm("a0"),
                                                    register Object *obj __asm("a2"),
                                                    register Msg msg __asm("a1"));
                    DispatchEntry entry = (DispatchEntry)super->cl_Dispatcher.h_Entry;
                    return entry(super, obj, msg);
                }
            }
            return 0;
        }
        
        case OM_ADDMEMBER: {
            struct opMember *opm = (struct opMember *)msg;
            if (opm->opam_Object) {
                /* Use OM_ADDTAIL to add the member's node to our list */
                struct opAddTail at;
                at.MethodID = OM_ADDTAIL;
                at.opat_List = (struct List *)&md->memberlist;
                struct _Object *mobj = _OBJECT(opm->opam_Object);
                if (mobj && mobj->o_Class) {
                    /* AmigaOS 3.1 reference: returns the OM_ADDTAIL result (1) */
                    return _intuition_dispatch_method(mobj->o_Class, opm->opam_Object, (Msg)&at);
                }
            }
            return 0;
        }
        
        case OM_REMMEMBER: {
            struct opMember *opm = (struct opMember *)msg;
            if (opm->opam_Object) {
                /* Use OM_REMOVE to remove the member's node */
                struct { ULONG MethodID; } rm;
                rm.MethodID = OM_REMOVE;
                struct _Object *mobj = _OBJECT(opm->opam_Object);
                if (mobj && mobj->o_Class) {
                    /* AmigaOS 3.1 reference: returns the OM_REMOVE result (1) */
                    return _intuition_dispatch_method(mobj->o_Class, opm->opam_Object, (Msg)&rm);
                }
            }
            return 0;
        }
        
        case OM_NOTIFY:
        case OM_UPDATE: {
            /* Broadcast to all members first */
            struct opUpdate *opu = (struct opUpdate *)msg;
            struct MinNode *node;
            
            for (node = md->memberlist.mlh_Head; node->mln_Succ; node = node->mln_Succ) {
                /* Convert MinNode back to Object:
                 * In AmigaOS, the object's _Object header contains the node,
                 * so the Object pointer is after the _Object header */
                Object *member = (Object *)((UBYTE *)node + sizeof(struct _Object));
                struct _Object *mobj = _OBJECT(member);
                if (mobj && mobj->o_Class) {
                    struct opUpdate update;
                    update.MethodID = OM_UPDATE;
                    update.opu_AttrList = opu->opu_AttrList;
                    update.opu_GInfo = opu->opu_GInfo;
                    update.opu_Flags = opu->opu_Flags;
                    _intuition_dispatch_method(mobj->o_Class, member, (Msg)&update);
                }
            }
            
            /* Then pass to superclass (icclass) for primary target */
            {
                struct IClass *super = cl->cl_Super;
                if (super && super->cl_Dispatcher.h_Entry) {
                    typedef ULONG (*DispatchEntry)(register struct IClass *cl __asm("a0"),
                                                    register Object *obj __asm("a2"),
                                                    register Msg msg __asm("a1"));
                    DispatchEntry entry = (DispatchEntry)super->cl_Dispatcher.h_Entry;
                    entry(super, obj, msg);
                }
            }
            /* AmigaOS 3.1 reference: the broadcast returns 1 */
            return 1;
        }
    }
    
    /* Call superclass for unhandled methods */
    {
        struct IClass *super = cl->cl_Super;
        if (super && super->cl_Dispatcher.h_Entry) {
            typedef ULONG (*DispatchEntry)(register struct IClass *cl __asm("a0"),
                                            register Object *obj __asm("a2"),
                                            register Msg msg __asm("a1"));
            DispatchEntry entry = (DispatchEntry)super->cl_Dispatcher.h_Entry;
            return entry(super, obj, msg);
        }
    }
    
    return 0;
}

/******************************************************************************
 * BOOPSI Gadget Class Dispatchers
 ******************************************************************************/

/* GadgetClass dispatcher - base class for all gadgets */

/*
 * gadgetclass attributes (OM_NEW / OM_SET): the GA_* tags of
 * intuition/gadgetclass.h map onto the Gadget fields and flags.
 */
static ULONG _gadgetclass_set_attrs(Object *obj, struct Gadget *gadget, struct TagItem *taglist, BOOL init)
{
    struct TagItem *tags = taglist;
    struct TagItem *tag;
    ULONG changed = 0;

#define GC_FLAG(field, bit) do { if (tag->ti_Data) gadget->field |= (bit); else gadget->field &= ~(bit); } while (0)
    while ((tag = NextTagItem(&tags)))
    {
        switch (tag->ti_Tag)
        {
            case GA_Left:      gadget->LeftEdge = (WORD)tag->ti_Data; gadget->Flags &= ~GFLG_RELRIGHT; break;
            case GA_RelRight:  gadget->LeftEdge = (WORD)tag->ti_Data; gadget->Flags |= GFLG_RELRIGHT; break;
            case GA_Top:       gadget->TopEdge = (WORD)tag->ti_Data; gadget->Flags &= ~GFLG_RELBOTTOM; break;
            case GA_RelBottom: gadget->TopEdge = (WORD)tag->ti_Data; gadget->Flags |= GFLG_RELBOTTOM; break;
            case GA_Width:     gadget->Width = (WORD)tag->ti_Data; gadget->Flags &= ~GFLG_RELWIDTH; break;
            case GA_RelWidth:  gadget->Width = (WORD)tag->ti_Data; gadget->Flags |= GFLG_RELWIDTH; break;
            case GA_Height:    gadget->Height = (WORD)tag->ti_Data; gadget->Flags &= ~GFLG_RELHEIGHT; break;
            case GA_RelHeight: gadget->Height = (WORD)tag->ti_Data; gadget->Flags |= GFLG_RELHEIGHT; break;
            case GA_RelSpecial: GC_FLAG(Flags, GFLG_RELSPECIAL); break;
            case GA_ID:        gadget->GadgetID = (UWORD)tag->ti_Data; break;
            case GA_UserData:  gadget->UserData = (APTR)tag->ti_Data; break;
            case GA_SpecialInfo: gadget->SpecialInfo = (APTR)tag->ti_Data; break;
            case GA_Disabled:  GC_FLAG(Flags, GFLG_DISABLED); break;
            case GA_Selected:  GC_FLAG(Flags, GFLG_SELECTED); break;
            case GA_Highlight:
                gadget->Flags = (gadget->Flags & ~GFLG_GADGHIGHBITS) | ((UWORD)tag->ti_Data & GFLG_GADGHIGHBITS);
                break;
            case GA_Image:
                gadget->GadgetRender = (APTR)tag->ti_Data;
                GC_FLAG(Flags, GFLG_GADGIMAGE);
                break;
            case GA_Border:
                gadget->GadgetRender = (APTR)tag->ti_Data;
                if (tag->ti_Data)
                    gadget->Flags &= ~GFLG_GADGIMAGE;
                break;
            case GA_SelectRender:
                gadget->SelectRender = (APTR)tag->ti_Data;
                if (tag->ti_Data)
                    gadget->Flags = (gadget->Flags & ~GFLG_GADGHIGHBITS) | GFLG_GADGHIMAGE;
                break;
            case GA_IntuiText:
                gadget->GadgetText = (struct IntuiText *)tag->ti_Data;
                gadget->Flags &= ~GFLG_LABELMASK;
                break;
            case GA_Text:
                gadget->GadgetText = (struct IntuiText *)tag->ti_Data;
                gadget->Flags = (gadget->Flags & ~GFLG_LABELMASK) | GFLG_LABELSTRING;
                break;
            case GA_LabelImage:
                gadget->GadgetText = (struct IntuiText *)tag->ti_Data;
                gadget->Flags = (gadget->Flags & ~GFLG_LABELMASK) | GFLG_LABELIMAGE;
                break;
            case GA_TabCycle:  GC_FLAG(Flags, GFLG_TABCYCLE); break;
            case GA_Immediate: GC_FLAG(Activation, GACT_IMMEDIATE); break;
            case GA_RelVerify: GC_FLAG(Activation, GACT_RELVERIFY); break;
            case GA_FollowMouse: GC_FLAG(Activation, GACT_FOLLOWMOUSE); break;
            case GA_RightBorder: GC_FLAG(Activation, GACT_RIGHTBORDER); break;
            case GA_LeftBorder: GC_FLAG(Activation, GACT_LEFTBORDER); break;
            case GA_TopBorder: GC_FLAG(Activation, GACT_TOPBORDER); break;
            case GA_BottomBorder: GC_FLAG(Activation, GACT_BOTTOMBORDER); break;
            case GA_ToggleSelect: GC_FLAG(Activation, GACT_TOGGLESELECT); break;
            case GA_EndGadget: GC_FLAG(Activation, GACT_ENDGADGET); break;
            case GA_GZZGadget: GC_FLAG(GadgetType, GTYP_GZZGADGET); break;
            case GA_SysGadget: GC_FLAG(GadgetType, GTYP_SYSGADGET); break;
            case GA_SysGType:
                gadget->GadgetType = (gadget->GadgetType & ~GTYP_SYSTYPEMASK) | ((UWORD)tag->ti_Data & GTYP_SYSTYPEMASK);
                break;
            case GA_Previous:
                if (init && tag->ti_Data)
                {
                    /* link the new gadget after this one (OM_NEW only) */
                    struct Gadget *prev = (struct Gadget *)tag->ti_Data;
                    gadget->NextGadget = prev->NextGadget;
                    prev->NextGadget = gadget;
                }
                break;
            default:
                continue;
        }
        changed = 1;
    }
#undef GC_FLAG
    (void)obj;
    return changed;
}

static ULONG gadgetclass_dispatch(
    register struct IClass *cl __asm("a0"),
    register Object *obj __asm("a2"),
    register Msg msg __asm("a1"))
{
    struct Gadget *gadget = (struct Gadget *)obj;
    
    switch (msg->MethodID) {
        case OM_NEW: {
            /* Call superclass first (rootclass) */
            struct IClass *super = cl->cl_Super;
            if (super && super->cl_Dispatcher.h_Entry)
            {
                typedef ULONG (*DispatchEntry)(register struct IClass *cl __asm("a0"),
                                                register Object *obj __asm("a2"),
                                                register Msg msg __asm("a1"));
                DispatchEntry entry = (DispatchEntry)super->cl_Dispatcher.h_Entry;
                /* AmigaOS: OM_NEW is sent with the true class as the
                 * object; rootclass allocates and returns the object */
                obj = (Object *)entry(super, obj, msg);
                if (!obj)
                    return 0;
                gadget = (struct Gadget *)obj;
            }
            
            /* Initialize gadget structure */
            gadget->GadgetType = GTYP_CUSTOMGADGET;
            /* AmigaOS 3.1: MutualExclude holds the hook Intuition calls for
             * a custom gadget - the object's class (probe intuition/customhook) */
            gadget->MutualExclude = (ULONG)OCLASS(obj);
            /* AmigaOS 3.1: gadgetclass objects are ExtGadgets, GADGHCOMP */
            gadget->Flags = GFLG_EXTENDED;
            gadget->Activation = 0;
            gadget->GadgetID = 0;
            gadget->UserData = NULL;
            gadget->SpecialInfo = NULL;
            gadget->NextGadget = NULL;
            
            /* Initialize embedded ICData */
            struct ICData *ic = GADGET_ICDATA(obj);
            ic->ic_Target = NULL;
            ic->ic_Mapping = NULL;
            ic->ic_LoopCounter = 0;
            
            /* Process tags from opSet */
            {
                struct opSet *ops = (struct opSet *)msg;
                struct TagItem *tags = ops->ops_AttrList;
                struct TagItem *tag;

                _gadgetclass_set_attrs(obj, gadget, ops->ops_AttrList, TRUE);
                while ((tag = NextTagItem(&tags)))
                {
                    if (tag->ti_Tag == ICA_TARGET)
                        ic->ic_Target = (Object *)tag->ti_Data;
                    else if (tag->ti_Tag == ICA_MAP)
                        ic->ic_Mapping = (struct TagItem *)tag->ti_Data;
                }
            }

            return (ULONG)obj;
        }
            
        case OM_DISPOSE:
        {
            /* Free embedded ICData resources */
            struct ICData *ic = GADGET_ICDATA(obj);
            _boopsi_free_icdata(ic);

            /* Call superclass */
            struct IClass *super = cl->cl_Super;
            if (super && super->cl_Dispatcher.h_Entry)
            {
                typedef ULONG (*DispatchEntry)(register struct IClass *cl __asm("a0"),
                                                register Object *obj __asm("a2"),
                                                register Msg msg __asm("a1"));
                DispatchEntry entry = (DispatchEntry)super->cl_Dispatcher.h_Entry;
                return entry(super, obj, msg);
            }
            return 0;
        }
            
        case OM_SET:
        case OM_UPDATE:
        {
            struct opSet *ops = (struct opSet *)msg;
            struct TagItem *tags = ops->ops_AttrList;
            struct TagItem *tag;
            ULONG changed = 0;

            /* Process ICA_TARGET/ICA_MAP for embedded IC support */
            struct ICData *ic = GADGET_ICDATA(obj);
            if (_boopsi_set_icdata(ic, ops->ops_AttrList))
                changed = 1;
            
            if (_gadgetclass_set_attrs(obj, gadget, ops->ops_AttrList, FALSE))
                changed = 1;
            (void)tags;
            (void)tag;
            return changed;
        }
            
        case OM_GET:
        {
            struct opGet *opg = (struct opGet *)msg;
            /* AmigaOS 3.1 reference: gadgetclass OM_GET does not support the
             * GA_* attributes (GA_Left/Top/Width/Height, GA_ID, GA_UserData,
             * GA_Disabled, GA_Selected all fail). */
            switch (opg->opg_AttrID)
            {
                case ICA_TARGET:
                {
                    struct ICData *ic = GADGET_ICDATA(obj);
                    *(opg->opg_Storage) = (ULONG)ic->ic_Target;
                    return 1;
                }
                case ICA_MAP:
                {
                    struct ICData *ic = GADGET_ICDATA(obj);
                    *(opg->opg_Storage) = (ULONG)ic->ic_Mapping;
                    return 1;
                }
                default:
                    return 0;
            }
        }
        
        case OM_NOTIFY:
        {
            /* Forward notification via embedded ICData */
            struct ICData *ic = GADGET_ICDATA(obj);
            _boopsi_do_notify(cl, obj, ic, (struct opUpdate *)msg);
            return 0;
        }
        
        case ICM_SETLOOP:
        {
            struct ICData *ic = GADGET_ICDATA(obj);
            ic->ic_LoopCounter++;
            return 0;
        }
        
        case ICM_CLEARLOOP:
        {
            struct ICData *ic = GADGET_ICDATA(obj);
            ic->ic_LoopCounter--;
            return 0;
        }
        
        case ICM_CHECKLOOP:
        {
            struct ICData *ic = GADGET_ICDATA(obj);
            return (ic->ic_LoopCounter > 0) ? TRUE : FALSE;
        }
        
        case GM_RENDER:
            /* Default: no rendering */
            return 0;
            
        case GM_HITTEST:
            /* Default: always hit if in bounds */
            return GMR_GADGETHIT;
            
        case GM_GOACTIVE:
            /* Default: become active immediately */
            return GMR_MEACTIVE;
            
        case GM_HANDLEINPUT:
            /* Default: stay active */
            return GMR_MEACTIVE;
            
        case GM_GOINACTIVE:
            /* Default: go inactive */
            return 0;
    }
    
    /* Call superclass for unhandled methods */
    {
        struct IClass *super = cl->cl_Super;
        if (super && super->cl_Dispatcher.h_Entry)
        {
            typedef ULONG (*DispatchEntry)(register struct IClass *cl __asm("a0"),
                                            register Object *obj __asm("a2"),
                                            register Msg msg __asm("a1"));
            DispatchEntry entry = (DispatchEntry)super->cl_Dispatcher.h_Entry;
            return entry(super, obj, msg);
        }
    }
    
    return 0;
}

/* ButtonGClass dispatcher - button gadget class */
static ULONG buttongclass_dispatch(
    register struct IClass *cl __asm("a0"),
    register Object *obj __asm("a2"),
    register Msg msg __asm("a1"))
{
    struct Gadget *gadget = (struct Gadget *)obj;
    
    switch (msg->MethodID) {
        case OM_NEW: {
            /* Call superclass first (gadgetclass) */
            struct IClass *super = cl->cl_Super;
            if (super && super->cl_Dispatcher.h_Entry) {
                typedef ULONG (*DispatchEntry)(register struct IClass *cl __asm("a0"),
                                               register Object *obj __asm("a2"),
                                               register Msg msg __asm("a1"));
                DispatchEntry entry = (DispatchEntry)super->cl_Dispatcher.h_Entry;
                /* AmigaOS: OM_NEW is sent with the true class as the
                 * object; rootclass allocates and returns the object */
                obj = (Object *)entry(super, obj, msg);
                if (!obj)
                    return 0;
                gadget = (struct Gadget *)obj;
            }
            
            /* Set button-specific defaults */
            gadget->GadgetType = GTYP_BOOLGADGET;
            gadget->Flags = GFLG_GADGHCOMP;
            gadget->Activation = GACT_RELVERIFY;
            
            return (ULONG)obj;
        }
        
        case GM_RENDER: {
            struct gpRender *gpr = (struct gpRender *)msg;
            struct RastPort *rp = gpr->gpr_RPort;
            struct DrawInfo *dri = gpr->gpr_GInfo ? gpr->gpr_GInfo->gi_DrInfo : NULL;
            ULONG state = 0;
            
            if (!rp) return 0;

            /* Resolve gadget box through _calculate_gadget_box so that
             * GFLG_REL* flags are handled correctly. */
            LONG gbox_left = gadget->LeftEdge;
            LONG gbox_top  = gadget->TopEdge;
            LONG gbox_w    = gadget->Width;
            LONG gbox_h    = gadget->Height;
            if (gpr->gpr_GInfo && gpr->gpr_GInfo->gi_Window)
            {
                _calculate_gadget_box(gpr->gpr_GInfo->gi_Window,
                                      gpr->gpr_GInfo->gi_Requester,
                                      gadget,
                                      &gbox_left, &gbox_top, &gbox_w, &gbox_h);
            }

            /* Determine state for visual feedback */
            if (gadget->Flags & GFLG_SELECTED)
                state |= IDS_SELECTED;
                
            /* Draw the button frame */
            _draw_bevel_box(rp, gbox_left, gbox_top, gbox_w, gbox_h, state, dri);
            
            /* Draw Label (GadgetText) */
            if (gadget->GadgetText) {
                struct IntuiText *it = gadget->GadgetText;
                
                SetAPen(rp, dri ? dri->dri_Pens[TEXTPEN] : 1);
                SetBPen(rp, dri ? dri->dri_Pens[BACKGROUNDPEN] : 0);
                
                /* Simple centering calculation */
                /* For now, just draw at offsets */
                Move(rp, gbox_left + it->LeftEdge + 4, gbox_top + it->TopEdge + gbox_h/2 + 2); // Approximate centering offset
                
                if (it->IText) {
                   Text(rp, it->IText, strlen((char *)it->IText));
                }
            }
            return 0;
        }
        
        case GM_HANDLEINPUT: {
            struct gpInput *gpi = (struct gpInput *)msg;
            struct InputEvent *ie = gpi->gpi_IEvent;
            
            /* Simple button behavior: release to verify */
            if (ie->ie_Class == IECLASS_RAWMOUSE) {
                if (ie->ie_Code == SELECTUP) {
                    if (gpi->gpi_Termination)
                        *gpi->gpi_Termination = gadget->GadgetID;
                    return GMR_NOREUSE | GMR_VERIFY;
                }
            }
            return GMR_MEACTIVE;
        }
    }
    
    /* Call superclass */
    {
        struct IClass *super = cl->cl_Super;
        if (super && super->cl_Dispatcher.h_Entry) {
            typedef ULONG (*DispatchEntry)(register struct IClass *cl __asm("a0"),
                                           register Object *obj __asm("a2"),
                                           register Msg msg __asm("a1"));
            DispatchEntry entry = (DispatchEntry)super->cl_Dispatcher.h_Entry;
            return entry(super, obj, msg);
        }
    }
    
    return 0;
}

/* PropGClass dispatcher - proportional gadget class */
static ULONG propgclass_dispatch(
    register struct IClass *cl __asm("a0"),
    register Object *obj __asm("a2"),
    register Msg msg __asm("a1"))
{
    struct Gadget *gadget = (struct Gadget *)obj;
    
    switch (msg->MethodID)
    {
        case OM_NEW:
        {
            /* Call superclass first (gadgetclass) */
            struct IClass *super = cl->cl_Super;
            if (super && super->cl_Dispatcher.h_Entry)
            {
                typedef ULONG (*DispatchEntry)(register struct IClass *cl __asm("a0"),
                                                register Object *obj __asm("a2"),
                                                register Msg msg __asm("a1"));
                DispatchEntry entry = (DispatchEntry)super->cl_Dispatcher.h_Entry;
                /* AmigaOS: OM_NEW is sent with the true class as the
                 * object; rootclass allocates and returns the object */
                obj = (Object *)entry(super, obj, msg);
                if (!obj)
                    return 0;
                gadget = (struct Gadget *)obj;
            }
            
            /* Initialize PropGData instance data */
            struct PropGData *data = (struct PropGData *)INST_DATA(cl, obj);
            
            /* Set up PropInfo defaults */
            data->propinfo.Flags = PROPNEWLOOK | AUTOKNOB | FREEVERT;
            data->propinfo.HorizPot = 0;
            data->propinfo.VertPot = 0;
            data->propinfo.HorizBody = MAXBODY;
            data->propinfo.VertBody = MAXBODY;
            data->top = 0;
            data->visible = 1;
            data->total = 1;
            
            /* Link PropInfo to gadget */
            gadget->SpecialInfo = &data->propinfo;
            gadget->GadgetType = GTYP_PROPGADGET;
            gadget->Flags = GFLG_GADGHCOMP;
            gadget->Activation = GACT_RELVERIFY | GACT_IMMEDIATE;
            
            /* Process PGA tags */
            struct opSet *ops = (struct opSet *)msg;
            struct TagItem *tags = ops->ops_AttrList;
            struct TagItem *tag;
            
            while ((tag = NextTagItem(&tags)))
            {
                switch (tag->ti_Tag)
                {
                    case PGA_Freedom:
                        if (tag->ti_Data == FREEHORIZ)
                        {
                            data->propinfo.Flags &= ~FREEVERT;
                            data->propinfo.Flags |= FREEHORIZ;
                        }
                        else
                        {
                            data->propinfo.Flags &= ~FREEHORIZ;
                            data->propinfo.Flags |= FREEVERT;
                        }
                        break;
                    case PGA_Top:
                        data->top = (UWORD)tag->ti_Data;
                        break;
                    case PGA_Visible:
                        data->visible = (UWORD)tag->ti_Data;
                        break;
                    case PGA_Total:
                        data->total = (UWORD)tag->ti_Data;
                        break;
                    case PGA_NewLook:
                        if (tag->ti_Data)
                            data->propinfo.Flags |= PROPNEWLOOK;
                        else
                            data->propinfo.Flags &= ~PROPNEWLOOK;
                        break;
                }
            }
            
            /* Convert top/visible/total to Pot/Body values */
            if (data->total > data->visible)
            {
                if (data->propinfo.Flags & FREEVERT)
                {
                    data->propinfo.VertBody = (data->visible * MAXBODY) / data->total;
                    if (data->total > data->visible)
                        data->propinfo.VertPot = (data->top * MAXPOT) / (data->total - data->visible);
                }
                else
                {
                    data->propinfo.HorizBody = (data->visible * MAXBODY) / data->total;
                    if (data->total > data->visible)
                        data->propinfo.HorizPot = (data->top * MAXPOT) / (data->total - data->visible);
                }
            }
            
            return (ULONG)obj;
        }
        
        case OM_SET:
        case OM_UPDATE:
        {
            /* Let superclass handle GA_* and ICA_* tags */
            struct IClass *super = cl->cl_Super;
            ULONG retval = 0;
            if (super && super->cl_Dispatcher.h_Entry)
            {
                typedef ULONG (*DispatchEntry)(register struct IClass *cl __asm("a0"),
                                                register Object *obj __asm("a2"),
                                                register Msg msg __asm("a1"));
                DispatchEntry entry = (DispatchEntry)super->cl_Dispatcher.h_Entry;
                retval = entry(super, obj, msg);
            }
            
            /* Process PGA tags */
            struct PropGData *data = (struct PropGData *)INST_DATA(cl, obj);
            struct opSet *ops = (struct opSet *)msg;
            struct TagItem *tags = ops->ops_AttrList;
            struct TagItem *tag;
            BOOL changed = FALSE;
            
            while ((tag = NextTagItem(&tags)))
            {
                switch (tag->ti_Tag)
                {
                    case PGA_Freedom:
                        if (tag->ti_Data == FREEHORIZ)
                        {
                            data->propinfo.Flags &= ~FREEVERT;
                            data->propinfo.Flags |= FREEHORIZ;
                        }
                        else
                        {
                            data->propinfo.Flags &= ~FREEHORIZ;
                            data->propinfo.Flags |= FREEVERT;
                        }
                        changed = TRUE;
                        break;
                    case PGA_Top:
                        data->top = (UWORD)tag->ti_Data;
                        changed = TRUE;
                        break;
                    case PGA_Visible:
                        data->visible = (UWORD)tag->ti_Data;
                        changed = TRUE;
                        break;
                    case PGA_Total:
                        data->total = (UWORD)tag->ti_Data;
                        changed = TRUE;
                        break;
                    case PGA_NewLook:
                        if (tag->ti_Data)
                            data->propinfo.Flags |= PROPNEWLOOK;
                        else
                            data->propinfo.Flags &= ~PROPNEWLOOK;
                        changed = TRUE;
                        break;
                }
            }
            
            /* Recalculate Pot/Body if changed */
            if (changed)
            {
                if (data->total > data->visible)
                {
                    if (data->propinfo.Flags & FREEVERT)
                    {
                        data->propinfo.VertBody = (data->visible * MAXBODY) / data->total;
                        if (data->total > data->visible)
                            data->propinfo.VertPot = (data->top * MAXPOT) / (data->total - data->visible);
                    }
                    else
                    {
                        data->propinfo.HorizBody = (data->visible * MAXBODY) / data->total;
                        if (data->total > data->visible)
                            data->propinfo.HorizPot = (data->top * MAXPOT) / (data->total - data->visible);
                    }
                }
                retval = 1;
            }
            
            return retval;
        }
        
        case OM_GET:
        {
            struct PropGData *data = (struct PropGData *)INST_DATA(cl, obj);
            struct opGet *opg = (struct opGet *)msg;
            switch (opg->opg_AttrID)
            {
                /* AmigaOS 3.1 reference: PGA_Top and PGA_Freedom are gettable,
                 * PGA_Visible and PGA_Total are not. */
                case PGA_Top:
                    *(opg->opg_Storage) = data->top;
                    return 1;
                case PGA_Freedom:
                    *(opg->opg_Storage) = data->propinfo.Flags & (FREEHORIZ | FREEVERT);
                    return 1;
                default:
                {
                    /* Let superclass handle GA_* queries */
                    struct IClass *super = cl->cl_Super;
                    if (super && super->cl_Dispatcher.h_Entry)
                    {
                        typedef ULONG (*DispatchEntry)(register struct IClass *cl __asm("a0"),
                                                        register Object *obj __asm("a2"),
                                                        register Msg msg __asm("a1"));
                        DispatchEntry entry = (DispatchEntry)super->cl_Dispatcher.h_Entry;
                        return entry(super, obj, msg);
                    }
                    return 0;
                }
            }
        }
        
        case GM_RENDER:
        {
            struct gpRender *gpr = (struct gpRender *)msg;
            struct RastPort *rp = gpr->gpr_RPort;
            struct DrawInfo *dri = gpr->gpr_GInfo ? gpr->gpr_GInfo->gi_DrInfo : NULL;
            struct PropInfo *pi = (struct PropInfo *)gadget->SpecialInfo;
            
            if (!rp) return 0;

            /* Resolve gadget box through _calculate_gadget_box so that
             * GFLG_RELRIGHT / GFLG_RELBOTTOM / GFLG_RELWIDTH /
             * GFLG_RELHEIGHT are handled correctly. */
            LONG gbox_left = gadget->LeftEdge;
            LONG gbox_top  = gadget->TopEdge;
            LONG gbox_w    = gadget->Width;
            LONG gbox_h    = gadget->Height;
            if (gpr->gpr_GInfo && gpr->gpr_GInfo->gi_Window)
            {
                _calculate_gadget_box(gpr->gpr_GInfo->gi_Window,
                                      gpr->gpr_GInfo->gi_Requester,
                                      gadget,
                                      &gbox_left, &gbox_top, &gbox_w, &gbox_h);
            }

            /* Draw container (Recessed) */
            _draw_bevel_box(rp, gbox_left, gbox_top, gbox_w, gbox_h, IDS_SELECTED, dri);
            
            /* Draw Knob if we have PropInfo */
            if (pi)
            {
                WORD knobW, knobH, knobX, knobY;
                
                /* Autoknob */
                WORD containerW = gbox_w - 4;
                WORD containerH = gbox_h - 4;
                
                if (pi->Flags & AUTOKNOB)
                {
                    /* Calculate knob size based on Body */
                    if (pi->Flags & FREEHORIZ)
                        knobW = (containerW * (ULONG)pi->HorizBody) / 0xFFFF;
                    else
                        knobW = containerW;
                        
                    if (pi->Flags & FREEVERT)
                        knobH = (containerH * (ULONG)pi->VertBody) / 0xFFFF;
                    else
                        knobH = containerH;
                        
                    /* Min size */
                    if (knobW < 4) knobW = 4;
                    if (knobH < 4) knobH = 4;
                     
                    /* Calculate knob position based on Pot */
                    WORD maxMoveX = containerW - knobW;
                    WORD maxMoveY = containerH - knobH;
                     
                    knobX = gbox_left + 2 + ((maxMoveX * (ULONG)pi->HorizPot) / 0xFFFF);
                    knobY = gbox_top + 2 + ((maxMoveY * (ULONG)pi->VertPot) / 0xFFFF);
                     
                    /* Draw Knob (Raised) */
                    _draw_bevel_box(rp, knobX, knobY, knobW, knobH, 0, dri);
                }
            }
            return 0;
        }
        
        case GM_HANDLEINPUT:
        {
            struct gpInput *gpi = (struct gpInput *)msg;
            struct InputEvent *ie = gpi->gpi_IEvent;
            struct PropGData *data = (struct PropGData *)INST_DATA(cl, obj);
            struct PropInfo *pi = &data->propinfo;
            
            if (!ie || !pi)
                return GMR_MEACTIVE;
            
            DPRINTF(LOG_DEBUG, "_intuition: propgclass GM_HANDLEINPUT ie_Class=%d ie_Code=0x%x mouse=%d,%d\n",
                    ie->ie_Class, ie->ie_Code, gpi->gpi_Mouse.X, gpi->gpi_Mouse.Y);
            
            /* Handle mouse button release */
            if (ie->ie_Class == IECLASS_RAWMOUSE)
            {
                if (ie->ie_Code == (IECODE_LBUTTON | IECODE_UP_PREFIX))
                {
                    /* Final notification with OPUF_INTERIM cleared */
                    /* Update top from pot values */
                    UWORD pot = (pi->Flags & FREEVERT) ? pi->VertPot : pi->HorizPot;
                    UWORD newtop = 0;
                    if (data->total > data->visible)
                        newtop = (pot * (ULONG)(data->total - data->visible)) / MAXPOT;
                    data->top = newtop;
                    
                    /* Send final notification */
                    struct TagItem notifyattrs[3];
                    notifyattrs[0].ti_Tag = PGA_Top;
                    notifyattrs[0].ti_Data = data->top;
                    notifyattrs[1].ti_Tag = GA_ID;
                    notifyattrs[1].ti_Data = gadget->GadgetID;
                    notifyattrs[2].ti_Tag = TAG_END;
                    
                    struct opUpdate notifymsg;
                    notifymsg.MethodID = OM_NOTIFY;
                    notifymsg.opu_AttrList = notifyattrs;
                    notifymsg.opu_GInfo = gpi->gpi_GInfo;
                    notifymsg.opu_Flags = 0; /* final */
                    
                    struct IClass *super = cl->cl_Super;
                    if (super && super->cl_Dispatcher.h_Entry)
                    {
                        typedef ULONG (*DispatchEntry)(register struct IClass *cl __asm("a0"),
                                                        register Object *obj __asm("a2"),
                                                        register Msg msg __asm("a1"));
                        DispatchEntry entry = (DispatchEntry)super->cl_Dispatcher.h_Entry;
                        entry(super, obj, (Msg)&notifymsg);
                    }
                    
                    if (gpi->gpi_Termination)
                        *gpi->gpi_Termination = gadget->GadgetID;
                    return GMR_NOREUSE | GMR_VERIFY;
                }
            }
            
            /* Handle mouse movement while dragging */
            if (ie->ie_Class == IECLASS_RAWMOUSE || ie->ie_Class == 0)
            {
                WORD containerW = gadget->Width - 4;
                WORD containerH = gadget->Height - 4;
                WORD knobW, knobH;
                WORD maxMoveX, maxMoveY;
                WORD mouseX = gpi->gpi_Mouse.X - gadget->LeftEdge - 2;
                WORD mouseY = gpi->gpi_Mouse.Y - gadget->TopEdge - 2;
                
                /* Calculate knob size */
                if (pi->Flags & AUTOKNOB)
                {
                    if (pi->Flags & FREEHORIZ)
                        knobW = (containerW * (ULONG)pi->HorizBody) / 0xFFFF;
                    else
                        knobW = containerW;
                        
                    if (pi->Flags & FREEVERT)
                        knobH = (containerH * (ULONG)pi->VertBody) / 0xFFFF;
                    else
                        knobH = containerH;
                        
                    if (knobW < 4) knobW = 4;
                    if (knobH < 4) knobH = 4;
                }
                else
                {
                    knobW = containerW;
                    knobH = containerH;
                }
                
                maxMoveX = containerW - knobW;
                maxMoveY = containerH - knobH;
                
                /* Update pot values based on mouse position */
                if ((pi->Flags & FREEHORIZ) && maxMoveX > 0)
                {
                    if (mouseX < 0) mouseX = 0;
                    if (mouseX > maxMoveX) mouseX = maxMoveX;
                    pi->HorizPot = (mouseX * 0xFFFF) / maxMoveX;
                }
                
                if ((pi->Flags & FREEVERT) && maxMoveY > 0)
                {
                    if (mouseY < 0) mouseY = 0;
                    if (mouseY > maxMoveY) mouseY = maxMoveY;
                    pi->VertPot = (mouseY * 0xFFFF) / maxMoveY;
                }
                
                DPRINTF(LOG_DEBUG, "_intuition: propgclass updated HorizPot=%d VertPot=%d\n",
                        pi->HorizPot, pi->VertPot);
                
                /* Update top from pot and send interim notification */
                UWORD pot = (pi->Flags & FREEVERT) ? pi->VertPot : pi->HorizPot;
                UWORD newtop = 0;
                if (data->total > data->visible)
                    newtop = (pot * (ULONG)(data->total - data->visible)) / MAXPOT;
                
                if (newtop != data->top)
                {
                    data->top = newtop;
                    
                    /* Send interim notification */
                    struct TagItem notifyattrs[3];
                    notifyattrs[0].ti_Tag = PGA_Top;
                    notifyattrs[0].ti_Data = data->top;
                    notifyattrs[1].ti_Tag = GA_ID;
                    notifyattrs[1].ti_Data = gadget->GadgetID;
                    notifyattrs[2].ti_Tag = TAG_END;
                    
                    struct opUpdate notifymsg;
                    notifymsg.MethodID = OM_NOTIFY;
                    notifymsg.opu_AttrList = notifyattrs;
                    notifymsg.opu_GInfo = gpi->gpi_GInfo;
                    notifymsg.opu_Flags = OPUF_INTERIM;
                    
                    struct IClass *super = cl->cl_Super;
                    if (super && super->cl_Dispatcher.h_Entry)
                    {
                        typedef ULONG (*DispatchEntry)(register struct IClass *cl __asm("a0"),
                                                        register Object *obj __asm("a2"),
                                                        register Msg msg __asm("a1"));
                        DispatchEntry entry = (DispatchEntry)super->cl_Dispatcher.h_Entry;
                        entry(super, obj, (Msg)&notifymsg);
                    }
                }
            }
            
            return GMR_MEACTIVE;
        }
    }
    
    /* Call superclass */
    {
        struct IClass *super = cl->cl_Super;
        if (super && super->cl_Dispatcher.h_Entry)
        {
            typedef ULONG (*DispatchEntry)(register struct IClass *cl __asm("a0"),
                                            register Object *obj __asm("a2"),
                                            register Msg msg __asm("a1"));
            DispatchEntry entry = (DispatchEntry)super->cl_Dispatcher.h_Entry;
            return entry(super, obj, msg);
        }
    }
    
    return 0;
}

/* StrGClass dispatcher - string gadget class */
static ULONG strgclass_dispatch(
    register struct IClass *cl __asm("a0"),
    register Object *obj __asm("a2"),
    register Msg msg __asm("a1"))
{
    struct Gadget *gadget = (struct Gadget *)obj;
    
    switch (msg->MethodID)
    {
        case OM_NEW:
        {
            /* Call superclass first (gadgetclass) */
            struct IClass *super = cl->cl_Super;
            if (super && super->cl_Dispatcher.h_Entry)
            {
                typedef ULONG (*DispatchEntry)(register struct IClass *cl __asm("a0"),
                                                register Object *obj __asm("a2"),
                                                register Msg msg __asm("a1"));
                DispatchEntry entry = (DispatchEntry)super->cl_Dispatcher.h_Entry;
                /* AmigaOS: OM_NEW is sent with the true class as the
                 * object; rootclass allocates and returns the object */
                obj = (Object *)entry(super, obj, msg);
                if (!obj)
                    return 0;
                gadget = (struct Gadget *)obj;
            }
            
            /* Initialize StrGData instance data */
            struct StrGData *data = (struct StrGData *)INST_DATA(cl, obj);
            
            /* Set defaults */
            data->strinfo.MaxChars = 128;
            data->strinfo.Buffer = data->buffer;
            data->strinfo.UndoBuffer = data->undobuffer;
            data->strinfo.BufferPos = 0;
            data->strinfo.NumChars = 0;
            data->strinfo.DispPos = 0;
            data->strinfo.UndoPos = 0;
            data->strinfo.LongInt = 0;
            data->longval = 0;
            
            /* Clear buffers */
            data->buffer[0] = '\0';
            data->undobuffer[0] = '\0';
            
            /* Link StringInfo to gadget */
            gadget->SpecialInfo = &data->strinfo;
            gadget->GadgetType = GTYP_STRGADGET;
            gadget->Flags = GFLG_GADGHCOMP;
            gadget->Activation = GACT_RELVERIFY;
            
            /* Process STRINGA tags */
            struct opSet *ops = (struct opSet *)msg;
            struct TagItem *tags = ops->ops_AttrList;
            struct TagItem *tag;
            
            while ((tag = NextTagItem(&tags)))
            {
                switch (tag->ti_Tag)
                {
                    case STRINGA_MaxChars:
                    {
                        LONG maxchars = (LONG)tag->ti_Data;
                        if (maxchars > 0 && maxchars <= 128)
                            data->strinfo.MaxChars = maxchars;
                        break;
                    }
                    case STRINGA_Buffer:
                        /* Use externally-provided buffer */
                        if (tag->ti_Data)
                        {
                            data->strinfo.Buffer = (UBYTE *)tag->ti_Data;
                        }
                        break;
                    case STRINGA_UndoBuffer:
                        if (tag->ti_Data)
                        {
                            data->strinfo.UndoBuffer = (UBYTE *)tag->ti_Data;
                        }
                        break;
                    case STRINGA_TextVal:
                        if (tag->ti_Data)
                        {
                            _intuition_strncpy((char *)data->strinfo.Buffer, 
                                    (const char *)tag->ti_Data, 
                                    data->strinfo.MaxChars);
                            data->strinfo.NumChars = strlen((char *)data->strinfo.Buffer);
                            data->strinfo.BufferPos = data->strinfo.NumChars;
                        }
                        break;
                    case STRINGA_LongVal:
                        data->longval = (LONG)tag->ti_Data;
                        data->strinfo.LongInt = data->longval;
                        gadget->Activation |= GACT_LONGINT;
                        /* Convert the long value to a string representation */
                        {
                            char tmpbuf[16];
                            LONG val = data->longval;
                            LONG i = 0;
                            BOOL negative = FALSE;
                            
                            if (val < 0)
                            {
                                negative = TRUE;
                                val = -val;
                            }
                            
                            /* Build digits in reverse */
                            if (val == 0)
                            {
                                tmpbuf[i++] = '0';
                            }
                            else
                            {
                                while (val > 0 && i < 14)
                                {
                                    tmpbuf[i++] = '0' + (val % 10);
                                    val /= 10;
                                }
                            }
                            if (negative)
                                tmpbuf[i++] = '-';
                            
                            /* Reverse into buffer */
                            LONG j;
                            for (j = 0; j < i; j++)
                            {
                                data->strinfo.Buffer[j] = tmpbuf[i - 1 - j];
                            }
                            data->strinfo.Buffer[i] = '\0';
                            data->strinfo.NumChars = i;
                            data->strinfo.BufferPos = i;
                        }
                        break;
                    case STRINGA_Justification:
                        /* Store in Extension if available */
                        break;
                }
            }
            
            return (ULONG)obj;
        }
        
        case OM_SET:
        case OM_UPDATE:
        {
            /* Let superclass handle GA_* and ICA_* tags */
            struct IClass *super = cl->cl_Super;
            ULONG retval = 0;
            if (super && super->cl_Dispatcher.h_Entry)
            {
                typedef ULONG (*DispatchEntry)(register struct IClass *cl __asm("a0"),
                                                register Object *obj __asm("a2"),
                                                register Msg msg __asm("a1"));
                DispatchEntry entry = (DispatchEntry)super->cl_Dispatcher.h_Entry;
                retval = entry(super, obj, msg);
            }
            
            /* Process STRINGA tags */
            struct StrGData *data = (struct StrGData *)INST_DATA(cl, obj);
            struct opSet *ops = (struct opSet *)msg;
            struct TagItem *tags = ops->ops_AttrList;
            struct TagItem *tag;
            
            while ((tag = NextTagItem(&tags)))
            {
                switch (tag->ti_Tag)
                {
                    case STRINGA_TextVal:
                        if (tag->ti_Data)
                        {
                            _intuition_strncpy((char *)data->strinfo.Buffer,
                                    (const char *)tag->ti_Data,
                                    data->strinfo.MaxChars);
                            data->strinfo.NumChars = strlen((char *)data->strinfo.Buffer);
                            data->strinfo.BufferPos = data->strinfo.NumChars;
                            retval = 1;
                        }
                        break;
                    case STRINGA_LongVal:
                        data->longval = (LONG)tag->ti_Data;
                        data->strinfo.LongInt = data->longval;
                        /* Convert long to string */
                        {
                            char tmpbuf[16];
                            LONG val = data->longval;
                            LONG i = 0;
                            BOOL negative = FALSE;
                            
                            if (val < 0)
                            {
                                negative = TRUE;
                                val = -val;
                            }
                            
                            if (val == 0)
                            {
                                tmpbuf[i++] = '0';
                            }
                            else
                            {
                                while (val > 0 && i < 14)
                                {
                                    tmpbuf[i++] = '0' + (val % 10);
                                    val /= 10;
                                }
                            }
                            if (negative)
                                tmpbuf[i++] = '-';
                            
                            LONG j;
                            for (j = 0; j < i; j++)
                            {
                                data->strinfo.Buffer[j] = tmpbuf[i - 1 - j];
                            }
                            data->strinfo.Buffer[i] = '\0';
                            data->strinfo.NumChars = i;
                            data->strinfo.BufferPos = i;
                        }
                        retval = 1;
                        break;
                }
            }
            
            return retval;
        }
        
        case OM_GET:
        {
            struct StrGData *data = (struct StrGData *)INST_DATA(cl, obj);
            struct opGet *opg = (struct opGet *)msg;
            switch (opg->opg_AttrID)
            {
                case STRINGA_TextVal:
                    *(opg->opg_Storage) = (ULONG)data->strinfo.Buffer;
                    return 1;
                case STRINGA_LongVal:
                    *(opg->opg_Storage) = (ULONG)data->strinfo.LongInt;
                    return 1;
                default:
                {
                    /* Let superclass handle GA_* queries */
                    struct IClass *super = cl->cl_Super;
                    if (super && super->cl_Dispatcher.h_Entry)
                    {
                        typedef ULONG (*DispatchEntry)(register struct IClass *cl __asm("a0"),
                                                        register Object *obj __asm("a2"),
                                                        register Msg msg __asm("a1"));
                        DispatchEntry entry = (DispatchEntry)super->cl_Dispatcher.h_Entry;
                        return entry(super, obj, msg);
                    }
                    return 0;
                }
            }
        }
        
        case GM_RENDER:
        {
            struct gpRender *gpr = (struct gpRender *)msg;
            struct RastPort *rp = gpr->gpr_RPort;
            struct DrawInfo *dri = gpr->gpr_GInfo ? gpr->gpr_GInfo->gi_DrInfo : NULL;
            struct StringInfo *si = (struct StringInfo *)gadget->SpecialInfo;
            UBYTE bgColor = dri ? dri->dri_Pens[BACKGROUNDPEN] : 0;
            UBYTE txtColor = dri ? dri->dri_Pens[TEXTPEN] : 1;
            WORD textY;
            
            if (!rp) return 0;

            /* Resolve gadget box through _calculate_gadget_box so that
             * GFLG_REL* flags are handled correctly. */
            LONG gbox_left = gadget->LeftEdge;
            LONG gbox_top  = gadget->TopEdge;
            LONG gbox_w    = gadget->Width;
            LONG gbox_h    = gadget->Height;
            if (gpr->gpr_GInfo && gpr->gpr_GInfo->gi_Window)
            {
                _calculate_gadget_box(gpr->gpr_GInfo->gi_Window,
                                      gpr->gpr_GInfo->gi_Requester,
                                      gadget,
                                      &gbox_left, &gbox_top, &gbox_w, &gbox_h);
            }

            /* Draw container (Recessed) */
            _draw_bevel_box(rp, gbox_left, gbox_top, gbox_w, gbox_h, IDS_SELECTED, dri);
            
            /* Clear background inside */
            SetAPen(rp, bgColor);
            RectFill(rp, gbox_left + 2, gbox_top + 2, 
                         gbox_left + gbox_w - 3, gbox_top + gbox_h - 3);
            
            /* Calculate text Y position (vertically centered) */
            textY = gbox_top + (gbox_h / 2) + 3;
            
            /* Draw Text */
            if (si && si->Buffer)
            {
                LONG len = strlen((char *)si->Buffer);
                
                SetAPen(rp, txtColor);
                SetBPen(rp, bgColor);
                SetDrMd(rp, JAM2);
                
                Move(rp, gbox_left + 4, textY);
                if (len > 0)
                {
                    Text(rp, si->Buffer, len);
                }
                
                /* Draw cursor if gadget is selected (active) */
                if (gadget->Flags & GFLG_SELECTED)
                {
                    WORD cursorX;
                    WORD cursorPos = si->BufferPos;
                    
                    /* Calculate cursor X position */
                    if (cursorPos > 0 && rp->Font)
                    {
                        cursorX = gbox_left + 4 + TextLength(rp, si->Buffer, cursorPos);
                    }
                    else
                    {
                        cursorX = gbox_left + 4;
                    }
                    
                    /* Draw cursor as a vertical bar (XOR mode) */
                    SetAPen(rp, txtColor);
                    SetDrMd(rp, COMPLEMENT);
                    RectFill(rp, cursorX, gbox_top + 3, 
                                 cursorX + 1, gbox_top + gbox_h - 4);
                    SetDrMd(rp, JAM2);
                }
            }
            return 0;
        }
        
        case GM_HANDLEINPUT:
        {
            struct gpInput *gpi = (struct gpInput *)msg;
            struct InputEvent *ie = gpi->gpi_IEvent;
            struct StringInfo *si = (struct StringInfo *)gadget->SpecialInfo;
            
            if (!ie || !si || !si->Buffer)
                return GMR_MEACTIVE;
            
            DPRINTF(LOG_DEBUG, "_intuition: strgclass GM_HANDLEINPUT ie_Class=%d ie_Code=0x%x\n",
                    ie->ie_Class, ie->ie_Code);
            
            /* Handle mouse button release */
            if (ie->ie_Class == IECLASS_RAWMOUSE)
            {
                if (ie->ie_Code == (IECODE_LBUTTON | IECODE_UP_PREFIX))
                {
                    /* Click - don't deactivate, stay active for editing */
                    return GMR_MEACTIVE;
                }
            }
            
            /* Handle keyboard input */
            if (ie->ie_Class == IECLASS_RAWKEY)
            {
                UWORD code = ie->ie_Code;
                BOOL isUpKey = (code & IECODE_UP_PREFIX) != 0;
                
                if (isUpKey)
                {
                    /* Key release - ignore */
                    return GMR_MEACTIVE;
                }
                
                code &= ~IECODE_UP_PREFIX;
                
                /* Check for special keys */
                switch (code)
                {
                    case 0x44: /* Return key */
                        if (gpi->gpi_Termination)
                            *gpi->gpi_Termination = gadget->GadgetID;
                        gadget->Flags &= ~GFLG_SELECTED;
                        return GMR_NOREUSE | GMR_VERIFY;
                        
                    case 0x45: /* Escape key */
                        gadget->Flags &= ~GFLG_SELECTED;
                        return GMR_NOREUSE;
                        
                    case 0x41: /* Backspace */
                        if (si->BufferPos > 0 && si->NumChars > 0)
                        {
                            WORD i;
                            for (i = si->BufferPos - 1; i < si->NumChars - 1; i++)
                            {
                                si->Buffer[i] = si->Buffer[i + 1];
                            }
                            si->Buffer[si->NumChars - 1] = '\0';
                            si->BufferPos--;
                            si->NumChars--;
                        }
                        return GMR_MEACTIVE;
                        
                    case 0x46: /* Delete */
                        if (si->BufferPos < si->NumChars)
                        {
                            WORD i;
                            for (i = si->BufferPos; i < si->NumChars - 1; i++)
                            {
                                si->Buffer[i] = si->Buffer[i + 1];
                            }
                            si->Buffer[si->NumChars - 1] = '\0';
                            si->NumChars--;
                        }
                        return GMR_MEACTIVE;
                        
                    case 0x4F: /* Cursor Left */
                        if (si->BufferPos > 0)
                            si->BufferPos--;
                        return GMR_MEACTIVE;
                        
                    case 0x4E: /* Cursor Right */
                        if (si->BufferPos < si->NumChars)
                            si->BufferPos++;
                        return GMR_MEACTIVE;
                        
                    default:
                        break;
                }
                
                /* Try to convert the raw key to ASCII */
                {
                    UBYTE ch = 0;
                    
                    if (code >= 0x00 && code <= 0x09)
                    {
                        static const UBYTE numRow[] = "1234567890";
                        static const UBYTE numRowShift[] = "!@#$%^&*()";
                        if (ie->ie_Qualifier & (IEQUALIFIER_LSHIFT | IEQUALIFIER_RSHIFT))
                            ch = numRowShift[code];
                        else
                            ch = numRow[code];
                    }
                    else if (code >= 0x10 && code <= 0x19)
                    {
                        static const UBYTE qRow[] = "qwertyuiop";
                        ch = qRow[code - 0x10];
                        if (ie->ie_Qualifier & (IEQUALIFIER_LSHIFT | IEQUALIFIER_RSHIFT | IEQUALIFIER_CAPSLOCK))
                            ch = ch - 'a' + 'A';
                    }
                    else if (code >= 0x20 && code <= 0x28)
                    {
                        static const UBYTE aRow[] = "asdfghjkl";
                        ch = aRow[code - 0x20];
                        if (ie->ie_Qualifier & (IEQUALIFIER_LSHIFT | IEQUALIFIER_RSHIFT | IEQUALIFIER_CAPSLOCK))
                            ch = ch - 'a' + 'A';
                    }
                    else if (code >= 0x31 && code <= 0x39)
                    {
                        static const UBYTE zRow[] = "zxcvbnm,.";
                        ch = zRow[code - 0x31];
                        if (ie->ie_Qualifier & (IEQUALIFIER_LSHIFT | IEQUALIFIER_RSHIFT | IEQUALIFIER_CAPSLOCK))
                            ch = ch - 'a' + 'A';
                    }
                    else if (code == 0x40)
                    {
                        ch = ' ';
                    }
                    else if (code == 0x0A)
                    {
                        ch = (ie->ie_Qualifier & (IEQUALIFIER_LSHIFT | IEQUALIFIER_RSHIFT)) ? '_' : '-';
                    }
                    else if (code == 0x0B)
                    {
                        ch = (ie->ie_Qualifier & (IEQUALIFIER_LSHIFT | IEQUALIFIER_RSHIFT)) ? '+' : '=';
                    }
                    
                    /* Insert character if we got one */
                    if (ch != 0 && si->NumChars < si->MaxChars - 1)
                    {
                        WORD i;
                        for (i = si->NumChars; i > si->BufferPos; i--)
                        {
                            si->Buffer[i] = si->Buffer[i - 1];
                        }
                        si->Buffer[si->BufferPos] = ch;
                        si->BufferPos++;
                        si->NumChars++;
                        si->Buffer[si->NumChars] = '\0';
                    }
                }
            }
            
            return GMR_MEACTIVE;
        }
        
        case GM_GOINACTIVE:
        {
            /* Send notification when gadget goes inactive */
            struct StrGData *data = (struct StrGData *)INST_DATA(cl, obj);
            struct gpGoInactive *gpgi = (struct gpGoInactive *)msg;
            
            /* Update LongInt if integer mode */
            if (gadget->Activation & GACT_LONGINT)
            {
                /* Parse the buffer as a number */
                LONG val = 0;
                BOOL negative = FALSE;
                UBYTE *p = data->strinfo.Buffer;
                
                if (*p == '-')
                {
                    negative = TRUE;
                    p++;
                }
                while (*p >= '0' && *p <= '9')
                {
                    val = val * 10 + (*p - '0');
                    p++;
                }
                if (negative)
                    val = -val;
                data->strinfo.LongInt = val;
                data->longval = val;
            }
            
            /* Send OM_NOTIFY to superclass */
            struct TagItem notifyattrs[3];
            
            if (gadget->Activation & GACT_LONGINT)
            {
                notifyattrs[0].ti_Tag = STRINGA_LongVal;
                notifyattrs[0].ti_Data = (ULONG)data->strinfo.LongInt;
            }
            else
            {
                notifyattrs[0].ti_Tag = STRINGA_TextVal;
                notifyattrs[0].ti_Data = (ULONG)data->strinfo.Buffer;
            }
            notifyattrs[1].ti_Tag = GA_ID;
            notifyattrs[1].ti_Data = gadget->GadgetID;
            notifyattrs[2].ti_Tag = TAG_END;
            
            struct opUpdate notifymsg;
            notifymsg.MethodID = OM_NOTIFY;
            notifymsg.opu_AttrList = notifyattrs;
            notifymsg.opu_GInfo = gpgi->gpgi_GInfo;
            notifymsg.opu_Flags = 0; /* final */
            
            struct IClass *super = cl->cl_Super;
            if (super && super->cl_Dispatcher.h_Entry)
            {
                typedef ULONG (*DispatchEntry)(register struct IClass *cl __asm("a0"),
                                                register Object *obj __asm("a2"),
                                                register Msg msg __asm("a1"));
                DispatchEntry entry = (DispatchEntry)super->cl_Dispatcher.h_Entry;
                entry(super, obj, (Msg)&notifymsg);
            }
            
            return 0;
        }
    }
    
    /* Call superclass */
    {
        struct IClass *super = cl->cl_Super;
        if (super && super->cl_Dispatcher.h_Entry)
        {
            typedef ULONG (*DispatchEntry)(register struct IClass *cl __asm("a0"),
                                            register Object *obj __asm("a2"),
                                            register Msg msg __asm("a1"));
            DispatchEntry entry = (DispatchEntry)super->cl_Dispatcher.h_Entry;
            return entry(super, obj, msg);
        }
    }
    
    return 0;
}

// libBase: IntuitionBase
// baseType: struct IntuitionBase *
// libname: intuition.library

struct IntuitionBase * __g_lxa_intuition_InitLib    ( register struct IntuitionBase *intuitionb    __asm("d0"),
                                                      register BPTR               seglist __asm("a0"),
                                                      register struct ExecBase   *sysb    __asm("a6"))
{
    struct LXAIntuitionBase *base = (struct LXAIntuitionBase *)intuitionb;
    DPRINTF (LOG_DEBUG, "_intuition: InitLib() called.\n");

    _intuition_reset_runtime_state();

    /* Initialize ClassList (NewList inline) */
    NewList(&base->ClassList);
    NewList(&base->PubScreenList);
    NewList((struct List *)&base->ScreenDataList);
    NewList(&base->WindowStateList);
    base->DefaultPubScreen = NULL;
    _intuition_init_preferences(&base->ActivePrefs);
    base->DefaultPrefs = base->ActivePrefs;
    base->DefaultPrefs.FontHeight = 9;
    base->DefaultPrefs.DoubleClick.tv_secs = 1;
    base->DefaultPrefs.DoubleClick.tv_micro = 500000;
    base->EditHook = NULL;
    _intuition_init_iprefs(base);

    /* Create rootclass */
    struct IClass *root = AllocMem(sizeof(struct IClass) + sizeof("rootclass"), MEMF_PUBLIC | MEMF_CLEAR);
    if (root) {
        UBYTE *id = (UBYTE *)(root + 1);
        strcpy((char *)id, "rootclass");
        
        root->cl_ID = (ClassID)id;
        root->cl_Dispatcher.h_Entry = (ULONG (*)())rootclass_dispatch;
        root->cl_Dispatcher.h_Data = NULL;
        root->cl_Dispatcher.h_SubEntry = NULL; 
        root->cl_Reserved = 0;
        root->cl_InstOffset = 0;
        root->cl_InstSize = 0;
        
        /* Add to ClassList so it can be found */
        {
            struct LXAClassNode *node = AllocMem(sizeof(struct LXAClassNode), MEMF_PUBLIC | MEMF_CLEAR);
            if (node) {
                node->class_ptr = root;
                node->node.ln_Type = NT_UNKNOWN;
                node->node.ln_Name = (char *)id;
                AddTail(&base->ClassList, &node->node);
                root->cl_Flags |= CLF_INLIST;
            }
        }
        
        base->RootClass = root;
        DPRINTF(LOG_DEBUG, "_intuition: rootclass created at 0x%08lx\n", (ULONG)root);
    } else {
        DPRINTF(LOG_ERROR, "_intuition: Failed to allocate rootclass!\n");
    }

    /* Create imageclass (subclass of rootclass) */
    {
        struct IClass *imageclass = AllocMem(sizeof(struct IClass) + sizeof(IMAGECLASS), MEMF_PUBLIC | MEMF_CLEAR);
        if (imageclass && base->RootClass) {
            UBYTE *id = (UBYTE *)(imageclass + 1);
            strcpy((char *)id, IMAGECLASS);

            imageclass->cl_ID = (ClassID)id;
            imageclass->cl_Super = base->RootClass;
            imageclass->cl_Dispatcher.h_Entry = (ULONG (*)())lxa_imageclass_dispatch;
            imageclass->cl_Dispatcher.h_Data = NULL;
            imageclass->cl_Dispatcher.h_SubEntry = NULL;
            imageclass->cl_Reserved = 0;
            imageclass->cl_InstOffset = base->RootClass->cl_InstOffset + base->RootClass->cl_InstSize;
            imageclass->cl_InstSize = sizeof(struct Image);

            base->RootClass->cl_SubclassCount++;

            {
                struct LXAClassNode *node = AllocMem(sizeof(struct LXAClassNode), MEMF_PUBLIC | MEMF_CLEAR);
                if (node) {
                    node->class_ptr = imageclass;
                    node->node.ln_Type = NT_UNKNOWN;
                    node->node.ln_Name = (char *)id;
                    AddTail(&base->ClassList, &node->node);
                    imageclass->cl_Flags |= CLF_INLIST;
                }
            }

            base->ImageClass = imageclass;
            DPRINTF(LOG_DEBUG, "_intuition: imageclass created at 0x%08lx\n", (ULONG)imageclass);
        } else {
            DPRINTF(LOG_ERROR, "_intuition: Failed to allocate imageclass!\n");
        }
    }

    /* Create sysiclass (subclass of imageclass) */
    {
        struct IClass *sysiclass = AllocMem(sizeof(struct IClass) + sizeof(SYSICLASS), MEMF_PUBLIC | MEMF_CLEAR);
        if (sysiclass && base->ImageClass) {
            UBYTE *id = (UBYTE *)(sysiclass + 1);
            strcpy((char *)id, SYSICLASS);

            sysiclass->cl_ID = (ClassID)id;
            sysiclass->cl_Super = base->ImageClass;
            sysiclass->cl_Dispatcher.h_Entry = (ULONG (*)())lxa_sysiclass_dispatch;
            sysiclass->cl_Dispatcher.h_Data = NULL;
            sysiclass->cl_Dispatcher.h_SubEntry = NULL;
            sysiclass->cl_Reserved = 0;
            sysiclass->cl_InstOffset = base->ImageClass->cl_InstOffset + base->ImageClass->cl_InstSize;
            sysiclass->cl_InstSize = lxa_sysiclass_instsize;

            base->ImageClass->cl_SubclassCount++;

            {
                struct LXAClassNode *node = AllocMem(sizeof(struct LXAClassNode), MEMF_PUBLIC | MEMF_CLEAR);
                if (node) {
                    node->class_ptr = sysiclass;
                    node->node.ln_Type = NT_UNKNOWN;
                    node->node.ln_Name = (char *)id;
                    AddTail(&base->ClassList, &node->node);
                    sysiclass->cl_Flags |= CLF_INLIST;
                }
            }

            base->SysIClass = sysiclass;
            DPRINTF(LOG_DEBUG, "_intuition: sysiclass created at 0x%08lx\n", (ULONG)sysiclass);
        } else {
            DPRINTF(LOG_ERROR, "_intuition: Failed to allocate sysiclass!\n");
        }
    }

    /* Create frameiclass (subclass of imageclass) */
    {
        struct IClass *frameiclass = AllocMem(sizeof(struct IClass) + sizeof(FRAMEICLASS), MEMF_PUBLIC | MEMF_CLEAR);
        if (frameiclass && base->ImageClass) {
            UBYTE *id = (UBYTE *)(frameiclass + 1);
            strcpy((char *)id, FRAMEICLASS);

            frameiclass->cl_ID = (ClassID)id;
            frameiclass->cl_Super = base->ImageClass;
            frameiclass->cl_Dispatcher.h_Entry = (ULONG (*)())lxa_frameiclass_dispatch;
            frameiclass->cl_InstOffset = base->ImageClass->cl_InstOffset + base->ImageClass->cl_InstSize;
            frameiclass->cl_InstSize = lxa_frameiclass_instsize;
            base->ImageClass->cl_SubclassCount++;
            {
                struct LXAClassNode *node = AllocMem(sizeof(struct LXAClassNode), MEMF_PUBLIC | MEMF_CLEAR);
                if (node) {
                    node->class_ptr = frameiclass;
                    node->node.ln_Type = NT_UNKNOWN;
                    node->node.ln_Name = (char *)id;
                    AddTail(&base->ClassList, &node->node);
                    frameiclass->cl_Flags |= CLF_INLIST;
                }
            }
        }
    }

    /* Create icclass (subclass of rootclass) */
    struct IClass *icclass = AllocMem(sizeof(struct IClass) + sizeof("icclass"), MEMF_PUBLIC | MEMF_CLEAR);
    if (icclass && base->RootClass)
    {
        UBYTE *id = (UBYTE *)(icclass + 1);
        strcpy((char *)id, "icclass");
        
        icclass->cl_ID = (ClassID)id;
        icclass->cl_Super = base->RootClass;
        icclass->cl_Dispatcher.h_Entry = (ULONG (*)())icclass_dispatch;
        icclass->cl_Dispatcher.h_Data = NULL;
        icclass->cl_Dispatcher.h_SubEntry = NULL;
        icclass->cl_Reserved = 0;
        icclass->cl_InstOffset = base->RootClass->cl_InstOffset + base->RootClass->cl_InstSize;
        icclass->cl_InstSize = sizeof(struct ICData);
        
        base->RootClass->cl_SubclassCount++;
        
        /* Add to ClassList */
        {
            struct LXAClassNode *node = AllocMem(sizeof(struct LXAClassNode), MEMF_PUBLIC | MEMF_CLEAR);
            if (node)
            {
                node->class_ptr = icclass;
                node->node.ln_Type = NT_UNKNOWN;
                node->node.ln_Name = (char *)id;
                AddTail(&base->ClassList, &node->node);
                icclass->cl_Flags |= CLF_INLIST;
            }
        }
        
        base->ICClass = icclass;
        DPRINTF(LOG_DEBUG, "_intuition: icclass created at 0x%08lx\n", (ULONG)icclass);
    }
    else
    {
        DPRINTF(LOG_ERROR, "_intuition: Failed to allocate icclass!\n");
    }

    /* Create modelclass (subclass of icclass) */
    struct IClass *modelclass = AllocMem(sizeof(struct IClass) + sizeof("modelclass"), MEMF_PUBLIC | MEMF_CLEAR);
    if (modelclass && base->ICClass)
    {
        UBYTE *id = (UBYTE *)(modelclass + 1);
        strcpy((char *)id, "modelclass");
        
        modelclass->cl_ID = (ClassID)id;
        modelclass->cl_Super = base->ICClass;
        modelclass->cl_Dispatcher.h_Entry = (ULONG (*)())modelclass_dispatch;
        modelclass->cl_Dispatcher.h_Data = NULL;
        modelclass->cl_Dispatcher.h_SubEntry = NULL;
        modelclass->cl_Reserved = 0;
        modelclass->cl_InstOffset = base->ICClass->cl_InstOffset + base->ICClass->cl_InstSize;
        modelclass->cl_InstSize = sizeof(struct ModelData);
        
        base->ICClass->cl_SubclassCount++;
        
        /* Add to ClassList */
        {
            struct LXAClassNode *node = AllocMem(sizeof(struct LXAClassNode), MEMF_PUBLIC | MEMF_CLEAR);
            if (node)
            {
                node->class_ptr = modelclass;
                node->node.ln_Type = NT_UNKNOWN;
                node->node.ln_Name = (char *)id;
                AddTail(&base->ClassList, &node->node);
                modelclass->cl_Flags |= CLF_INLIST;
            }
        }
        
        base->ModelClass = modelclass;
        DPRINTF(LOG_DEBUG, "_intuition: modelclass created at 0x%08lx\n", (ULONG)modelclass);
    }
    else
    {
        DPRINTF(LOG_ERROR, "_intuition: Failed to allocate modelclass!\n");
    }

    /* Create gadgetclass */
    struct IClass *gadgetclass = AllocMem(sizeof(struct IClass) + sizeof("gadgetclass"), MEMF_PUBLIC | MEMF_CLEAR);
    if (gadgetclass && base->RootClass) {
        UBYTE *id = (UBYTE *)(gadgetclass + 1);
        strcpy((char *)id, "gadgetclass");
        
        gadgetclass->cl_ID = (ClassID)id;
        gadgetclass->cl_Super = base->RootClass;
        gadgetclass->cl_Dispatcher.h_Entry = (ULONG (*)())gadgetclass_dispatch;
        gadgetclass->cl_Dispatcher.h_Data = NULL;
        gadgetclass->cl_Dispatcher.h_SubEntry = NULL;
        gadgetclass->cl_Reserved = 0;
        gadgetclass->cl_InstOffset = base->RootClass->cl_InstOffset + base->RootClass->cl_InstSize;
        gadgetclass->cl_InstSize = sizeof(struct Gadget) + sizeof(struct ICData);
        
        base->RootClass->cl_SubclassCount++;
        
        /* Add to ClassList */
        {
            struct LXAClassNode *node = AllocMem(sizeof(struct LXAClassNode), MEMF_PUBLIC | MEMF_CLEAR);
            if (node) {
                node->class_ptr = gadgetclass;
                node->node.ln_Type = NT_UNKNOWN;
                node->node.ln_Name = (char *)id;
                AddTail(&base->ClassList, &node->node);
                gadgetclass->cl_Flags |= CLF_INLIST;
            }
        }
        
        base->GadgetClass = gadgetclass;
        DPRINTF(LOG_DEBUG, "_intuition: gadgetclass created at 0x%08lx\n", (ULONG)gadgetclass);
    } else {
        DPRINTF(LOG_ERROR, "_intuition: Failed to allocate gadgetclass!\n");
    }

    /* Create buttongclass */
    struct IClass *buttongclass = AllocMem(sizeof(struct IClass) + sizeof("buttongclass"), MEMF_PUBLIC | MEMF_CLEAR);
    if (buttongclass && base->GadgetClass) {
        UBYTE *id = (UBYTE *)(buttongclass + 1);
        strcpy((char *)id, "buttongclass");
        
        buttongclass->cl_ID = (ClassID)id;
        buttongclass->cl_Super = base->GadgetClass;
        buttongclass->cl_Dispatcher.h_Entry = (ULONG (*)())buttongclass_dispatch;
        buttongclass->cl_Dispatcher.h_Data = NULL;
        buttongclass->cl_Dispatcher.h_SubEntry = NULL;
        buttongclass->cl_Reserved = 0;
        buttongclass->cl_InstOffset = base->GadgetClass->cl_InstOffset + base->GadgetClass->cl_InstSize;
        buttongclass->cl_InstSize = 0; /* No additional instance data beyond Gadget */
        
        base->GadgetClass->cl_SubclassCount++;
        
        /* Add to ClassList */
        {
            struct LXAClassNode *node = AllocMem(sizeof(struct LXAClassNode), MEMF_PUBLIC | MEMF_CLEAR);
            if (node) {
                node->class_ptr = buttongclass;
                node->node.ln_Type = NT_UNKNOWN;
                node->node.ln_Name = (char *)id;
                AddTail(&base->ClassList, &node->node);
                buttongclass->cl_Flags |= CLF_INLIST;
            }
        }
        
        base->ButtonGClass = buttongclass;
        DPRINTF(LOG_DEBUG, "_intuition: buttongclass created at 0x%08lx\n", (ULONG)buttongclass);
    } else {
        DPRINTF(LOG_ERROR, "_intuition: Failed to allocate buttongclass!\n");
    }

    /* Create propgclass */
    struct IClass *propgclass = AllocMem(sizeof(struct IClass) + sizeof("propgclass"), MEMF_PUBLIC | MEMF_CLEAR);
    if (propgclass && base->GadgetClass) {
        UBYTE *id = (UBYTE *)(propgclass + 1);
        strcpy((char *)id, "propgclass");
        
        propgclass->cl_ID = (ClassID)id;
        propgclass->cl_Super = base->GadgetClass;
        propgclass->cl_Dispatcher.h_Entry = (ULONG (*)())propgclass_dispatch;
        propgclass->cl_Dispatcher.h_Data = NULL;
        propgclass->cl_Dispatcher.h_SubEntry = NULL;
        propgclass->cl_Reserved = 0;
        propgclass->cl_InstOffset = base->GadgetClass->cl_InstOffset + base->GadgetClass->cl_InstSize;
        propgclass->cl_InstSize = sizeof(struct PropGData);
        
        base->GadgetClass->cl_SubclassCount++;
        
        /* Add to ClassList */
        {
            struct LXAClassNode *node = AllocMem(sizeof(struct LXAClassNode), MEMF_PUBLIC | MEMF_CLEAR);
            if (node) {
                node->class_ptr = propgclass;
                node->node.ln_Type = NT_UNKNOWN;
                node->node.ln_Name = (char *)id;
                AddTail(&base->ClassList, &node->node);
                propgclass->cl_Flags |= CLF_INLIST;
            }
        }
        
        base->PropGClass = propgclass;
        DPRINTF(LOG_DEBUG, "_intuition: propgclass created at 0x%08lx\n", (ULONG)propgclass);
    } else {
        DPRINTF(LOG_ERROR, "_intuition: Failed to allocate propgclass!\n");
    }

    /* Create strgclass */
    struct IClass *strgclass = AllocMem(sizeof(struct IClass) + sizeof("strgclass"), MEMF_PUBLIC | MEMF_CLEAR);
    if (strgclass && base->GadgetClass) {
        UBYTE *id = (UBYTE *)(strgclass + 1);
        strcpy((char *)id, "strgclass");
        
        strgclass->cl_ID = (ClassID)id;
        strgclass->cl_Super = base->GadgetClass;
        strgclass->cl_Dispatcher.h_Entry = (ULONG (*)())strgclass_dispatch;
        strgclass->cl_Dispatcher.h_Data = NULL;
        strgclass->cl_Dispatcher.h_SubEntry = NULL;
        strgclass->cl_Reserved = 0;
        strgclass->cl_InstOffset = base->GadgetClass->cl_InstOffset + base->GadgetClass->cl_InstSize;
        strgclass->cl_InstSize = sizeof(struct StrGData);
        
        base->GadgetClass->cl_SubclassCount++;
        
        /* Add to ClassList */
        {
            struct LXAClassNode *node = AllocMem(sizeof(struct LXAClassNode), MEMF_PUBLIC | MEMF_CLEAR);
            if (node) {
                node->class_ptr = strgclass;
                node->node.ln_Type = NT_UNKNOWN;
                node->node.ln_Name = (char *)id;
                AddTail(&base->ClassList, &node->node);
                strgclass->cl_Flags |= CLF_INLIST;
            }
        }
        
        base->StrGClass = strgclass;
        DPRINTF(LOG_DEBUG, "_intuition: strgclass created at 0x%08lx\n", (ULONG)strgclass);
    } else {
        DPRINTF(LOG_ERROR, "_intuition: Failed to allocate strgclass!\n");
    }

    return intuitionb;
}

struct IntuitionBase * __g_lxa_intuition_OpenLib ( register struct IntuitionBase  *IntuitionBase __asm("a6"))
{
    DPRINTF (LOG_DEBUG, "_intuition: OpenLib() called\n");
    /* AmigaOS 3.1 keeps lib_OpenCnt of this never-expunged ROM library
     * constant at 1 (reference-verified, Phase 220) */
    IntuitionBase->LibNode.lib_OpenCnt = 1;
    IntuitionBase->LibNode.lib_Flags &= ~LIBF_DELEXP;
    return IntuitionBase;
}

BPTR __g_lxa_intuition_CloseLib ( register struct IntuitionBase  *intuitionb __asm("a6"))
{
    (void)intuitionb;   /* lib_OpenCnt stays constant, see OpenLib */
    return (BPTR)0;
}

BPTR __g_lxa_intuition_ExpungeLib ( register struct IntuitionBase  *intuitionb      __asm("a6"))
{
    return (BPTR)0;
}

ULONG __g_lxa_intuition_ExtFuncLib(void)
{
    PRIVATE_FUNCTION_ERROR("_intuition", "ExtFuncLib");
    return 0;
}

VOID _intuition_OpenIntuition ( register struct IntuitionBase * IntuitionBase __asm("a6"))
{
    DPRINTF (LOG_DEBUG, "_intuition: OpenIntuition() called\n");

    if (!IntuitionBase)
        return;

    if (_intuition_find_workbench_screen(IntuitionBase))
        return;

    (void)_intuition_OpenWorkBench(IntuitionBase);
}

static VOID _intuition_update_input_snapshot(struct IntuitionBase *IntuitionBase,
                                             WORD mouseX,
                                             WORD mouseY)
{
    struct timeval tv;

    if (!IntuitionBase)
        return;

    U_getSysTime(&tv);
    IntuitionBase->MouseX = mouseX;
    IntuitionBase->MouseY = mouseY;
    {
        /* every screen tracks the pointer in its own coordinates */
        struct Screen *scr;
        for (scr = IntuitionBase->FirstScreen; scr; scr = scr->NextScreen)
        {
            scr->MouseX = mouseX - scr->LeftEdge;
            scr->MouseY = mouseY - scr->TopEdge;
        }
    }
    IntuitionBase->Seconds = tv.tv_secs;
    IntuitionBase->Micros = tv.tv_micro;
}

VOID _intuition_ActivateWindow ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Window * window __asm("a0"));

static struct Window *_intuition_resolve_input_window(struct IntuitionBase *IntuitionBase,
                                                      struct Screen *screen,
                                                      WORD mouseX,
                                                      WORD mouseY)
{
    struct Window *window = NULL;

    if (screen)
        window = _find_window_at_pos(screen, mouseX, mouseY);

    if (!window && g_active_window && (!screen || g_active_window->WScreen == screen))
        window = g_active_window;

    if (!window && IntuitionBase && IntuitionBase->ActiveWindow &&
        (!screen || IntuitionBase->ActiveWindow->WScreen == screen))
    {
        window = IntuitionBase->ActiveWindow;
    }

    if (!window && screen)
        window = screen->FirstWindow;

    return window;
}

static struct Screen *_intuition_resolve_input_screen(struct IntuitionBase *IntuitionBase,
                                                      const struct InputEvent *iEvent,
                                                      WORD mouseX,
                                                      WORD mouseY)
{
    struct Screen *screen;

    if (!IntuitionBase)
        return NULL;

    if (iEvent && iEvent->ie_Class == IECLASS_RAWKEY &&
        IntuitionBase->ActiveWindow && IntuitionBase->ActiveWindow->WScreen)
    {
        return IntuitionBase->ActiveWindow->WScreen;
    }

    for (screen = IntuitionBase->FirstScreen; screen; screen = screen->NextScreen)
    {
        if (mouseX >= screen->LeftEdge &&
            mouseX < screen->LeftEdge + screen->Width &&
            mouseY >= screen->TopEdge &&
            mouseY < screen->TopEdge + screen->Height)
        {
            return screen;
        }
    }

    if (IntuitionBase->ActiveWindow && IntuitionBase->ActiveWindow->WScreen)
        return IntuitionBase->ActiveWindow->WScreen;

    if (IntuitionBase->ActiveScreen)
        return IntuitionBase->ActiveScreen;

    return IntuitionBase->FirstScreen;
}

static VOID _intuition_handle_mouse_button_event(struct IntuitionBase *IntuitionBase,
                                                 struct Screen *screen,
                                                 struct Window *window,
                                                 WORD mouseX,
                                                 WORD mouseY,
                                                 UWORD code,
                                                 UWORD qualifier,
                                                 BOOL dispatch_input_device)
{
    struct InputEvent input_event;

    input_event.ie_NextEvent = NULL;
    input_event.ie_Class = IECLASS_RAWMOUSE;
    input_event.ie_SubClass = 0;
    input_event.ie_Code = code;
    input_event.ie_Qualifier = qualifier;
    input_event.ie_X = mouseX;
    input_event.ie_Y = mouseY;
    input_event.ie_EventAddress = NULL;

    if (dispatch_input_device)
        _input_device_dispatch_event(&input_event);

    DPRINTF(LOG_DEBUG, "_intuition: MouseButton code=0x%02x qual=0x%04x at (%d,%d)\n",
            code, qualifier, mouseX, mouseY);

    if (!window)
        return;

    {
        WORD relX = mouseX - window->LeftEdge;
        WORD relY = mouseY - window->TopEdge;

        DPRINTF(LOG_DEBUG, "_intuition: MouseButton: window=0x%08lx at (%d,%d) size=(%d,%d) rel=(%d,%d)\n",
                (ULONG)window, window->LeftEdge, window->TopEdge, window->Width, window->Height, relX, relY);

        if (code == SELECTDOWN)
        {
            if (!(window->Flags & WFLG_WINDOWACTIVE))
            {
                _intuition_ActivateWindow(IntuitionBase, window);
            }

            struct Gadget *gad = _find_gadget_at_pos(window, relX, relY);
            DPRINTF(LOG_DEBUG, "_intuition: SELECTDOWN relX=%d relY=%d gad=0x%08lx firstGad=0x%08lx type=0x%04x\n",
                    relX, relY, (ULONG)gad, (ULONG)window->FirstGadget,
                    gad ? gad->GadgetType : 0xFFFF);
            if (gad)
            {
                DPRINTF(LOG_DEBUG, "_intuition: SELECTDOWN on gadget type=0x%04x\n",
                        gad->GadgetType);

                g_active_gadget = gad;
                g_active_window = window;

                gad->Flags |= GFLG_SELECTED;

                if (_gadtools_IsGadTools(gad))
                {
                    if (gad->Activation & GACT_IMMEDIATE)
                    {
                        LONG gbl, gbt, gbw, gbh;
                        _calculate_gadget_box(window, NULL, gad, &gbl, &gbt, &gbw, &gbh);
                        g_gt_down_code = _gadtools_HandleClick(gad, relX - gbl, relY - gbt);
                        if (g_gt_down_code < 0)
                            g_gt_down_code = 0;
                    }
                    _render_gadget(window, NULL, gad);
                }

                if ((gad->GadgetType & GTYP_SYSGADGET) &&
                    (gad->GadgetType & GTYP_SYSTYPEMASK) == GTYP_SIZING)
                {
                    DPRINTF(LOG_DEBUG, "_intuition: Starting window resize: window=0x%08lx size=(%d,%d)\n",
                            (ULONG)window, window->Width, window->Height);

                    /* Per RKRM: Post IDCMP_SIZEVERIFY before allowing the resize
                     * to proceed.  In the real Amiga, Intuition would block until
                     * the app replies.  In lxa's cooperative emulator the app gets
                     * the notification and can process it on the next event-loop
                     * iteration (before the first SizeWindow() call during drag). */
                    _post_idcmp_message(window, IDCMP_SIZEVERIFY, 0, qualifier,
                                        NULL, relX, relY);

                    g_sizing_window = TRUE;
                    g_size_window = window;
                    g_size_start_x = mouseX;
                    g_size_start_y = mouseY;
                    g_size_orig_w = window->Width;
                    g_size_orig_h = window->Height;
                }

                if ((gad->GadgetType & GTYP_GTYPEMASK) == GTYP_STRGADGET)
                {
                    struct StringInfo *si = (struct StringInfo *)gad->SpecialInfo;
                    if (si && si->Buffer)
                    {
                        si->BufferPos = si->NumChars;
                    }

                    _render_gadget(window, NULL, gad);
                }

                if ((gad->GadgetType & GTYP_GTYPEMASK) == GTYP_PROPGADGET)
                    _prop_select_down(window, gad, mouseX);

                if (gad->Activation & GACT_IMMEDIATE)
                {
                    _post_idcmp_message(window, IDCMP_GADGETDOWN, (UWORD)g_gt_down_code,
                                       qualifier, gad, relX, relY);
                }

                if ((gad->Flags & GFLG_GADGHIGHBITS) == GFLG_GADGHCOMP &&
                    (gad->GadgetType & GTYP_GTYPEMASK) != GTYP_CUSTOMGADGET &&
                    (gad->GadgetType & GTYP_GTYPEMASK) != GTYP_PROPGADGET &&
                    (gad->GadgetType & GTYP_GTYPEMASK) != GTYP_STRGADGET)
                {
                    _complement_gadget_area(window, NULL, gad);
                }
            }
            else
            {
                if ((window->Flags & WFLG_DRAGBAR) &&
                    relY >= 0 && relY < window->BorderTop &&
                    relX >= 0 && relX < window->Width)
                {
                    DPRINTF(LOG_DEBUG, "_intuition: Starting window drag: window=0x%08lx at (%d,%d)\n",
                            (ULONG)window, window->LeftEdge, window->TopEdge);
                    g_dragging_window = TRUE;
                    g_drag_window = window;
                    g_drag_start_x = mouseX;
                    g_drag_start_y = mouseY;
                    g_drag_window_x = window->LeftEdge;
                    g_drag_window_y = window->TopEdge;
                }
            }
        }
        else if (code == SELECTUP)
        {
            if (g_active_gadget && g_active_window)
            {
                struct Gadget *gad = g_active_gadget;
                struct Window *activeWin = g_active_window;
                WORD activeRelX = mouseX - activeWin->LeftEdge;
                WORD activeRelY = mouseY - activeWin->TopEdge;
                BOOL inside = _point_in_gadget(activeWin, gad, activeRelX, activeRelY);

                DPRINTF(LOG_DEBUG, "_intuition: SELECTUP gadtype=0x%04x inside=%d isSys=%d isStr=%d\n",
                        gad->GadgetType, inside,
                        (gad->GadgetType & GTYP_SYSGADGET) ? 1 : 0,
                        ((gad->GadgetType & GTYP_GTYPEMASK) == GTYP_STRGADGET) ? 1 : 0);

                DPRINTF(LOG_DEBUG, "_intuition: SELECTUP on gadget type=0x%04x inside=%d\n",
                        gad->GadgetType, inside);

                if ((gad->GadgetType & GTYP_GTYPEMASK) == GTYP_STRGADGET)
                {
                    DPRINTF(LOG_DEBUG, "_intuition: String gadget remains active for keyboard input\n");

                    if (inside)
                    {
                        if (gad->Activation & GACT_IMMEDIATE)
                        {
                            _post_idcmp_message(activeWin, IDCMP_GADGETDOWN, 0,
                                               qualifier, gad, activeRelX, activeRelY);
                        }
                    }

                    _render_gadget(activeWin, NULL, gad);
                }
                else if ((gad->GadgetType & GTYP_GTYPEMASK) == GTYP_PROPGADGET)
                {
                    struct PropInfo *pi = (struct PropInfo *)gad->SpecialInfo;
                    UWORD level_code = 0;

                    gad->Flags &= ~GFLG_SELECTED;

                    if (pi)
                    {
                        LONG level = _prop_slider_level(gad, pi);
                        _gadtools_UpdateSliderLevelDisplay(gad, level);
                        level_code = (UWORD)level;

                        DPRINTF(LOG_DEBUG, "_intuition: Prop SELECTUP: pot=%u level=%ld\n", pi->HorizPot, level);
                    }

                    if (gad->Activation & GACT_RELVERIFY)
                    {
                        _post_idcmp_message(activeWin, IDCMP_GADGETUP, level_code,
                                           qualifier, gad, activeRelX, activeRelY);
                    }

                    g_active_gadget = NULL;
                    g_active_window = NULL;
                }
                else
                {
                    if (_gadtools_IsCheckbox(gad))
                    {
                        if (inside)
                        {
                            _gadtools_SetCheckboxState(gad,
                                                       _gadtools_GetCheckboxState(gad) ? FALSE : TRUE);
                        }
                        /* a checked GadTools checkbox stays GFLG_SELECTED (AmigaOS 3.1) */
                        if (_gadtools_GetCheckboxState(gad))
                            gad->Flags |= GFLG_SELECTED;
                        else
                            gad->Flags &= ~GFLG_SELECTED;
                    }
                    else if (_gadtools_IsCycle(gad))
                    {
                        UWORD active = _gadtools_GetCycleState(gad);

                        if (inside)
                            active = _gadtools_AdvanceCycleState(gad);

                        gad->Flags &= ~GFLG_SELECTED;

                        if (inside && (gad->Activation & GACT_RELVERIFY))
                        {
                            _post_idcmp_message(activeWin, IDCMP_GADGETUP, active,
                                               qualifier, gad, activeRelX, activeRelY);
                        }
                    }
                    else if ((gad->Activation & GACT_TOGGLESELECT) && inside)
                    {
                        gad->Flags ^= GFLG_SELECTED;
                    }
                    else
                    {
                        gad->Flags &= ~GFLG_SELECTED;
                    }

                    if (inside)
                    {
                        LONG up_code = 0;
                        if (_gadtools_IsGadTools(gad) && !(gad->Activation & GACT_IMMEDIATE))
                        {
                            LONG gbl, gbt, gbw, gbh;
                            _calculate_gadget_box(activeWin, NULL, gad, &gbl, &gbt, &gbw, &gbh);
                            up_code = _gadtools_HandleClick(gad, activeRelX - gbl, activeRelY - gbt);
                            if (up_code < 0)
                                up_code = 0;
                        }
                        if (gad->GadgetType & GTYP_SYSGADGET)
                        {
                            _handle_sys_gadget_verify(activeWin, gad);
                        }
                        else if (gad->Activation & GACT_RELVERIFY)
                        {
                            _post_idcmp_message(activeWin, IDCMP_GADGETUP, (UWORD)up_code,
                                               qualifier, gad, activeRelX, activeRelY);
                        }
                    }

                    if (_gadtools_IsGadTools(gad))
                    {
                        _render_gadget(activeWin, NULL, gad);
                    }
                    else if ((gad->Flags & GFLG_GADGHIGHBITS) == GFLG_GADGHCOMP &&
                             (gad->GadgetType & GTYP_GTYPEMASK) != GTYP_CUSTOMGADGET)
                    {
                        _complement_gadget_area(activeWin, NULL, gad);
                    }

                    g_active_gadget = NULL;
                    g_active_window = NULL;
                }
            }

            if (g_dragging_window)
            {
                DPRINTF(LOG_DEBUG, "_intuition: Stopping window drag\n");
                g_dragging_window = FALSE;
                g_drag_window = NULL;
            }

            if (g_sizing_window)
            {
                DPRINTF(LOG_DEBUG, "_intuition: Stopping window resize: final size=(%d,%d)\n",
                        g_size_window ? g_size_window->Width : 0,
                        g_size_window ? g_size_window->Height : 0);
                g_sizing_window = FALSE;
                g_size_window = NULL;
            }
        }
        else if (code == MENUDOWN)
        {
            struct Window *menuWin = NULL;
            BOOL in_title_bar = (screen && mouseY <= screen->BarHeight);

            DPRINTF(LOG_DEBUG, "_intuition: MENUDOWN at (%d,%d), window=0x%08lx MenuStrip=0x%08lx in_title_bar=%d BarHeight=%d\n",
                    mouseX, mouseY, (ULONG)window, window ? (ULONG)window->MenuStrip : 0, (int)in_title_bar, screen ? (int)screen->BarHeight : -1);

            if (window && !(window->Flags & WFLG_RMBTRAP) &&
                _intuition_menu_window((struct LXAIntuitionBase *)IntuitionBase, window)->MenuStrip)
            {
                menuWin = _intuition_menu_window((struct LXAIntuitionBase *)IntuitionBase, window);
            }
            else if (in_title_bar)
            {
                /* Title bar is system territory — ignore WFLG_RMBTRAP */
                struct Window *w;
                for (w = screen->FirstWindow; w; w = w->NextWindow)
                {
                    if (w->MenuStrip)
                    {
                        menuWin = w;
                        break;
                    }
                }
            }
            else if (screen)
            {
                struct Window *w;
                for (w = screen->FirstWindow; w; w = w->NextWindow)
                {
                    if (w->MenuStrip && !(w->Flags & WFLG_RMBTRAP))
                    {
                        menuWin = w;
                        break;
                    }
                }
            }

            if (menuWin && menuWin->MenuStrip && screen)
            {
                DPRINTF(LOG_DEBUG, "_intuition: MENUDOWN calling _enter_menu_mode at (%d,%d) menuWin=0x%08lx\n",
                        mouseX, mouseY, (ULONG)menuWin);

                /* Per RKRM: Post IDCMP_MENUVERIFY with MENUHOT to the active
                 * window before activating menus.  The app can set Code to
                 * MENUCANCEL before replying to suppress menu activation.
                 * In lxa's cooperative emulator we cannot block for the reply,
                 * so we post the notification and proceed immediately. */
                _post_idcmp_message(menuWin, IDCMP_MENUVERIFY, MENUHOT,
                                    qualifier, NULL, relX, relY);

                /* Post MENUWAITING to all other MENUVERIFY-interested windows */
                if (screen)
                {
                    struct Window *w;
                    for (w = screen->FirstWindow; w; w = w->NextWindow)
                    {
                        if (w != menuWin && (w->IDCMPFlags & IDCMP_MENUVERIFY))
                        {
                            WORD wRelX = mouseX - w->LeftEdge;
                            WORD wRelY = mouseY - w->TopEdge;
                            _post_idcmp_message(w, IDCMP_MENUVERIFY, MENUWAITING,
                                                qualifier, NULL, wRelX, wRelY);
                        }
                    }
                }

                _enter_menu_mode(menuWin, screen, mouseX, mouseY);
            }
        }
        else if (code == MENUUP)
        {
             DPRINTF(LOG_DEBUG, "_intuition: MENUUP at (%d,%d), g_menu_mode=%d, g_active_menu=0x%08lx, g_active_item=0x%08lx\n",
                     mouseX, mouseY, g_menu_mode, (ULONG)g_active_menu, (ULONG)g_active_item);

            if (g_menu_mode && g_menu_window)
            {
                _exit_menu_mode(g_menu_window, mouseX, mouseY);
            }
        }

        {
            WORD btnMsgX, btnMsgY;
            _compute_idcmp_mouse_coords(window, IDCMP_MOUSEBUTTONS, mouseX, mouseY, &btnMsgX, &btnMsgY);
            _post_idcmp_message(window, IDCMP_MOUSEBUTTONS, code,
                               qualifier, NULL, btnMsgX, btnMsgY);
        }
    }
}

/* Menu tracking while the menu button is held: pointer at screen (mouseX, mouseY). */
static void _menu_track_pointer(struct Screen *screen, WORD mouseX, WORD mouseY)
{
    BOOL needRedraw = FALSE;
    struct Menu *oldMenu = g_active_menu;
    struct MenuItem *oldItem = g_active_item;

    DPRINTF(LOG_DEBUG, "_intuition: pointerpos in menu_mode: mouse=(%d,%d) BarH=%d g_active_menu=0x%08lx g_active_item=0x%08lx\n",
            mouseX, mouseY, (int)screen->BarHeight, (ULONG)g_active_menu, (ULONG)g_active_item);

    if (mouseY < screen->BarHeight + 1)
    {
        struct Menu *newMenu = _find_menu_at_x(g_menu_window, mouseX);
        /* AmigaOS 3.1 keeps the last menu open while the pointer is
         * over a part of the bar without a menu title */
        if (!newMenu)
        {
            newMenu = g_active_menu;
            if (g_active_item || g_active_subitem)
            {
                g_active_item = NULL;
                g_active_subitem = NULL;
                needRedraw = TRUE;
            }
        }
        if (newMenu != g_active_menu)
        {
            g_active_menu = newMenu;
            g_active_item = NULL;
            g_active_subitem = NULL;
            needRedraw = TRUE;
        }
    }
    else if (g_active_menu && g_active_menu->FirstItem)
    {
        BOOL handledSubmenu = FALSE;

        if (g_active_item && g_active_item->SubItem)
        {
            WORD submenuX;
            WORD submenuY;
            WORD submenuWidth;
            WORD submenuHeight;

            if (_get_active_submenu_box(g_menu_window, &submenuX, &submenuY,
                                        &submenuWidth, &submenuHeight) &&
                mouseX >= submenuX && mouseX < submenuX + submenuWidth &&
                mouseY >= submenuY && mouseY < submenuY + submenuHeight)
            {
                struct MenuItem *newSubItem;

                handledSubmenu = TRUE;
                {
                                                        WORD sox = 0, soy = 0;
                                                        _get_menu_submenu_origin(g_menu_window, g_active_menu, g_active_item, &sox, &soy);
                                                        newSubItem = _find_item_in_chain_at_pos(g_active_item->SubItem,
                                                                                                mouseX - sox, mouseY - soy);
                                                        }
                if (newSubItem != g_active_subitem)
                {
                    g_active_subitem = newSubItem;
                    needRedraw = TRUE;
                }
            }
        }

        if (!handledSubmenu)
        {
            WORD menuTop, menuLeft;
            WORD itemX, itemY;
            _menu_item_origin(screen, g_active_menu, &menuLeft, &menuTop);
            itemX = mouseX - menuLeft;
            itemY = mouseY - menuTop;
            struct MenuItem *newItem = _find_item_at_pos(g_active_menu, itemX, itemY);

            DPRINTF(LOG_DEBUG, "_intuition: menu_item_hit: menuTop=%d menuLeft=%d itemX=%d itemY=%d newItem=0x%08lx BarHBorder=%d LeftEdge=%d\n",
                    (int)menuTop, (int)menuLeft, (int)itemX, (int)itemY, (ULONG)newItem, (int)screen->BarHBorder, (int)g_active_menu->LeftEdge);

            if (newItem != g_active_item)
            {
                g_active_item = newItem;
                g_active_subitem = NULL;
                needRedraw = TRUE;
            }
            else if (g_active_subitem)
            {
                g_active_subitem = NULL;
                needRedraw = TRUE;
            }
        }
    }
    else
    {
        if (g_active_item || g_active_subitem)
        {
            g_active_item = NULL;
            g_active_subitem = NULL;
            needRedraw = TRUE;
        }
    }

    if (needRedraw)
    {
        _menu_sync_drawn_flags();
        if (_menu_hover_redraw_can_repaint_in_place(g_menu_window,
                                                    oldMenu,
                                                    oldItem))
        {
            _render_menu_items_in_place(g_menu_window);
        }
        else
        {
            if (oldMenu || g_active_menu)
                _restore_menu_dropdown_area(g_menu_window->WScreen);

            _render_menu_bar(g_menu_window);

            if (g_active_menu)
            {
                _save_dropdown_for_menu(g_menu_window, g_active_menu);
                _render_menu_items(g_menu_window);
            }
        }
    }
}

static VOID _intuition_handle_pointerpos_event(struct IntuitionBase *IntuitionBase,
                                               struct Screen *screen,
                                               struct Window *window,
                                               WORD mouseX,
                                               WORD mouseY,
                                               UWORD qualifier,
                                               BOOL dispatch_input_device)
{
    struct InputEvent input_event;

    input_event.ie_NextEvent = NULL;
    input_event.ie_Class = IECLASS_POINTERPOS;
    input_event.ie_SubClass = 0;
    input_event.ie_Code = 0;
    input_event.ie_Qualifier = qualifier;
    input_event.ie_X = mouseX;
    input_event.ie_Y = mouseY;
    input_event.ie_EventAddress = NULL;

    if (dispatch_input_device)
        _input_device_dispatch_event(&input_event);

    if (g_menu_mode && g_menu_window && screen)
        _menu_track_pointer(screen, mouseX, mouseY);

    if (g_dragging_window && g_drag_window)
    {
        WORD dx = mouseX - g_drag_start_x;
        WORD dy = mouseY - g_drag_start_y;
        WORD newX = g_drag_window_x + dx;
        WORD newY = g_drag_window_y + dy;
        struct Screen *wscreen = g_drag_window->WScreen;

        if (wscreen)
        {
            if (newX < -g_drag_window->Width + 20)
                newX = -g_drag_window->Width + 20;
            if (newX > wscreen->Width - 20)
                newX = wscreen->Width - 20;
            if (newY < 0)
                newY = 0;
            if (newY > wscreen->Height - g_drag_window->BorderTop)
                newY = wscreen->Height - g_drag_window->BorderTop;
        }

        if (newX != g_drag_window->LeftEdge || newY != g_drag_window->TopEdge)
        {
            WORD move_dx = newX - g_drag_window->LeftEdge;
            WORD move_dy = newY - g_drag_window->TopEdge;

            DPRINTF(LOG_DEBUG, "_intuition: Dragging window to (%d,%d) delta=(%d,%d)\n",
                    newX, newY, move_dx, move_dy);

            _intuition_MoveWindow(IntuitionBase, g_drag_window, move_dx, move_dy);
        }
    }

    if (g_sizing_window && g_size_window)
    {
        WORD dx = mouseX - g_size_start_x;
        WORD dy = mouseY - g_size_start_y;
        WORD newW = g_size_orig_w + dx;
        WORD newH = g_size_orig_h + dy;
        /* sizing with the mouse respects the window limits */
        if (newW < g_size_window->MinWidth) newW = g_size_window->MinWidth;
        if ((UWORD)newW > g_size_window->MaxWidth) newW = g_size_window->MaxWidth;
        if (newH < g_size_window->MinHeight) newH = g_size_window->MinHeight;
        if ((UWORD)newH > g_size_window->MaxHeight) newH = g_size_window->MaxHeight;
        WORD size_dx = newW - g_size_window->Width;
        WORD size_dy = newH - g_size_window->Height;

        if (size_dx != 0 || size_dy != 0)
        {
            DPRINTF(LOG_DEBUG, "_intuition: Sizing window: delta=(%d,%d) target=(%d,%d)\n",
                    size_dx, size_dy, newW, newH);

            _intuition_SizeWindow(IntuitionBase, g_size_window, size_dx, size_dy);
        }
    }

    if (g_active_gadget && g_active_window &&
        (g_active_gadget->GadgetType & GTYP_GTYPEMASK) == GTYP_PROPGADGET)
    {
        struct Gadget *gad = g_active_gadget;
        struct Window *activeWin = g_active_window;
        LONG level;

        if (_prop_drag(activeWin, gad, mouseX, &level))
            _post_idcmp_message(activeWin, IDCMP_MOUSEMOVE, (UWORD)level, qualifier, gad,
                                mouseX - activeWin->LeftEdge, mouseY - activeWin->TopEdge);
    }

    if (window)
    {
        WORD relX = mouseX - window->LeftEdge;
        WORD relY = mouseY - window->TopEdge;
        WORD msgX, msgY;

        window->MouseX = relX;
        window->MouseY = relY;

        _compute_idcmp_mouse_coords(window, IDCMP_MOUSEMOVE, mouseX, mouseY, &msgX, &msgY);
        _post_idcmp_message(window, IDCMP_MOUSEMOVE, 0,
                           qualifier, NULL, msgX, msgY);
    }

    /* Update previous absolute position for DELTAMOVE calculations */
    g_prev_abs_mouse_x = mouseX;
    g_prev_abs_mouse_y = mouseY;
}

static VOID _intuition_handle_rawkey_event(struct IntuitionBase *IntuitionBase,
                                           struct Window *window,
                                           WORD mouseX,
                                           WORD mouseY,
                                           UWORD rawkey,
                                           UWORD qualifier,
                                           BOOL dispatch_input_device)
{
    struct InputEvent input_event;

    input_event.ie_NextEvent = NULL;
    input_event.ie_Class = IECLASS_RAWKEY;
    input_event.ie_SubClass = 0;
    input_event.ie_Code = rawkey;
    input_event.ie_Qualifier = qualifier;
    input_event.ie_X = mouseX;
    input_event.ie_Y = mouseY;
    input_event.ie_EventAddress = NULL;

    if (dispatch_input_device)
        _input_device_dispatch_event(&input_event);
    _keyboard_device_record_event(rawkey, qualifier);

    if (g_active_gadget && g_active_window &&
        (g_active_gadget->GadgetType & GTYP_GTYPEMASK) == GTYP_STRGADGET)
    {
        BOOL stillActive = _handle_string_gadget_key(g_active_gadget, g_active_window,
                                                     rawkey, qualifier);
        if (!stillActive)
        {
            g_active_gadget = NULL;
            g_active_window = NULL;
        }
    }
    else if (window)
    {
        struct LXAWindowState *state = _intuition_ensure_window_state(
            (struct LXAIntuitionBase *)IntuitionBase, window);
        WORD relX = mouseX - window->LeftEdge;
        WORD relY = mouseY - window->TopEdge;
        BOOL posted = FALSE;

        /* Menu keyboard shortcuts (CommKey / Right-Amiga + key) */
        if ((qualifier & IEQUALIFIER_RCOMMAND) &&
            !(rawkey & IECODE_UP_PREFIX) &&
            window->MenuStrip &&
            !(window->Flags & WFLG_RMBTRAP))
        {
            char ascii = U_rawkeyToVanilla(rawkey,
                             qualifier & ~IEQUALIFIER_RCOMMAND);
            if (ascii)
            {
                UWORD menuCode = _find_menu_commkey(window->MenuStrip, ascii);
                if (menuCode != MENUNULL)
                {
                    DPRINTF(LOG_DEBUG,
                        "_intuition: CommKey '%c' -> menuCode=0x%04x (rawkey handler)\n",
                        ascii, menuCode);
                    posted = _post_idcmp_message(window, IDCMP_MENUPICK,
                                 menuCode, qualifier, NULL, relX, relY);
                }
            }
        }

        if (!posted && (window->IDCMPFlags & IDCMP_VANILLAKEY))
        {
            char ascii = U_rawkeyToVanilla(rawkey, qualifier);
            if (ascii)
            {
                posted = _post_idcmp_message(window, IDCMP_VANILLAKEY,
                                             (UWORD)ascii, qualifier, NULL, relX, relY);
            }
        }

        if (!posted)
        {
            _post_idcmp_message(window, IDCMP_RAWKEY, rawkey,
                               qualifier, NULL, relX, relY);
        }

        _intuition_note_rawkey(state, rawkey, qualifier);
    }
}

static VOID _intuition_handle_event_class_event(struct IntuitionBase *IntuitionBase,
                                                struct Window *window,
                                                WORD mouseX,
                                                WORD mouseY,
                                                UWORD code,
                                                BOOL dispatch_input_device)
{
    struct InputEvent input_event;

    input_event.ie_NextEvent = NULL;
    input_event.ie_Class = IECLASS_EVENT;
    input_event.ie_SubClass = 0;
    input_event.ie_Code = code;
    input_event.ie_Qualifier = 0;
    input_event.ie_X = mouseX;
    input_event.ie_Y = mouseY;
    input_event.ie_EventAddress = NULL;

    if (dispatch_input_device)
        _input_device_dispatch_event(&input_event);

    if (window)
    {
        WORD relX = mouseX - window->LeftEdge;
        WORD relY = mouseY - window->TopEdge;
        _post_idcmp_message(window, IDCMP_CLOSEWINDOW, 0,
                           0, window, relX, relY);
    }
}

static VOID _intuition_process_input_events(struct IntuitionBase *IntuitionBase,
                                            struct InputEvent *iEvent,
                                            BOOL dispatch_input_device)
{
    while (IntuitionBase && iEvent)
    {
        struct InputEvent current_event = *iEvent;
        struct Screen *screen;
        struct Window *window;
        struct InputEvent *next_event = iEvent->ie_NextEvent;
        WORD mouseX = IntuitionBase->MouseX;
        WORD mouseY = IntuitionBase->MouseY;

        current_event.ie_NextEvent = NULL;

        if (current_event.ie_Class == IECLASS_RAWMOUSE &&
            (current_event.ie_Qualifier & IEQUALIFIER_RELATIVEMOUSE))
        {
            /* mouse driver events carry deltas */
            mouseX = IntuitionBase->MouseX + current_event.ie_X;
            mouseY = IntuitionBase->MouseY + current_event.ie_Y;
        }
        else if (current_event.ie_Class == IECLASS_RAWMOUSE ||
                 current_event.ie_Class == IECLASS_POINTERPOS)
        {
            mouseX = current_event.ie_X;
            mouseY = current_event.ie_Y;
        }
        else if (current_event.ie_Class == IECLASS_NEWPOINTERPOS)
        {
            /* V36 pointer positioning: IESUBCLASS_PIXEL gives a position on
             * a screen (ie_EventAddress -> IEPointerPixel) */
            struct IEPointerPixel *pp = (struct IEPointerPixel *)current_event.ie_EventAddress;
            if (current_event.ie_SubClass == IESUBCLASS_PIXEL && pp)
            {
                mouseX = pp->iepp_Position.X + (pp->iepp_Screen ? pp->iepp_Screen->LeftEdge : 0);
                mouseY = pp->iepp_Position.Y + (pp->iepp_Screen ? pp->iepp_Screen->TopEdge : 0);
            }
            current_event.ie_Class = IECLASS_POINTERPOS;
        }
        else if (current_event.ie_X != 0 || current_event.ie_Y != 0)
        {
            mouseX = current_event.ie_X;
            mouseY = current_event.ie_Y;
        }

        screen = _intuition_resolve_input_screen(IntuitionBase, &current_event, mouseX, mouseY);
        _intuition_update_input_snapshot(IntuitionBase, mouseX, mouseY);
        window = _intuition_resolve_input_window(IntuitionBase, screen, mouseX, mouseY);

        switch (current_event.ie_Class)
        {
            case IECLASS_RAWMOUSE:
                if (current_event.ie_Code == IECODE_NOBUTTON)
                {
                    /* pure mouse motion */
                    _intuition_handle_pointerpos_event(IntuitionBase, screen, window,
                                                       mouseX, mouseY,
                                                       current_event.ie_Qualifier,
                                                       dispatch_input_device);
                }
                else
                {
                    _intuition_handle_mouse_button_event(IntuitionBase, screen, window,
                                                         mouseX, mouseY,
                                                         current_event.ie_Code,
                                                         current_event.ie_Qualifier,
                                                         dispatch_input_device);
                }
                break;

            case IECLASS_POINTERPOS:
                _intuition_handle_pointerpos_event(IntuitionBase, screen, window,
                                                   mouseX, mouseY,
                                                   current_event.ie_Qualifier,
                                                   dispatch_input_device);
                break;

            case IECLASS_RAWKEY:
                _intuition_handle_rawkey_event(IntuitionBase, window,
                                               mouseX, mouseY,
                                               current_event.ie_Code,
                                               current_event.ie_Qualifier,
                                               dispatch_input_device);
                break;

            case IECLASS_EVENT:
                if (current_event.ie_Code == IECODE_NEWACTIVE)
                {
                    _intuition_handle_event_class_event(IntuitionBase, window,
                                                        mouseX, mouseY,
                                                        current_event.ie_Code,
                                                        dispatch_input_device);
                }
                break;

            default:
                break;
        }

        iEvent = next_event;
    }
}

VOID _intuition_Intuition ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct InputEvent * iEvent __asm("a0"))
{
    DPRINTF (LOG_DEBUG, "_intuition: Intuition() called iEvent=0x%08lx\n", (ULONG)iEvent);

    _intuition_process_input_events(IntuitionBase, iEvent, TRUE);
}

/*
 * Events written to input.device (IND_WRITEEVENT / IND_ADDEVENT) by
 * applications or mouse/keyboard drivers reach Intuition's input handler
 * after the input.device handler chain has seen them (as on AmigaOS, where
 * Intuition is a handler in that chain).
 */
VOID _intuition_input_device_events(struct InputEvent *iEvent)
{
    _intuition_process_input_events(IntuitionBase, iEvent, FALSE);
}

UWORD _intuition_AddGadget ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Window * window __asm("a0"),
                                                        register struct Gadget * gadget __asm("a1"),
                                                        register UWORD position __asm("d0"))
{
    DPRINTF (LOG_DEBUG, "_intuition: AddGadget() window=0x%08lx gadget=0x%08lx pos=%d\n",
             (ULONG)window, (ULONG)gadget, position);

    if (!window || !gadget)
        return 0;

    /* Ensure gadget is not linked to another list */
    gadget->NextGadget = NULL;

    /* Count existing gadgets and find insertion point */
    UWORD count = 0;
    struct Gadget *prev = NULL;
    struct Gadget *curr = window->FirstGadget;
    
    while (curr && count < position) {
        count++;
        prev = curr;
        curr = curr->NextGadget;
    }
    
    /* Insert the gadget */
    if (!prev) {
        /* Insert at beginning */
        gadget->NextGadget = window->FirstGadget;
        window->FirstGadget = gadget;
    } else {
        /* Insert after prev */
        gadget->NextGadget = prev->NextGadget;
        prev->NextGadget = gadget;
    }
    
    /* Return the actual position where gadget was inserted */
    return count;
}

BOOL _intuition_ClearDMRequest ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Window * window __asm("a0"))
{
    DPRINTF (LOG_DEBUG, "_intuition: ClearDMRequest() window=0x%08lx\n", (ULONG)window);

    if (!window)
        return FALSE;

    if (window->DMRequest && (window->DMRequest->Flags & REQACTIVE))
        return FALSE;

    window->DMRequest = NULL;
    return TRUE;
}

VOID _intuition_ClearMenuStrip ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Window * window __asm("a0"))
{
    DPRINTF (LOG_DEBUG, "_intuition: ClearMenuStrip() window=0x%08lx\n", (ULONG)window);

    if (!window)
    {
        LPRINTF (LOG_ERROR, "_intuition: ClearMenuStrip() called with NULL window\n");
        return;
    }

    window->MenuStrip = NULL;
}

VOID _intuition_ClearPointer ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Window * window __asm("a0"))
{
    /*
     * ClearPointer() clears a custom pointer image and restores the default.
     * Since we don't support custom pointers yet, this is a no-op.
     */
    DPRINTF (LOG_DEBUG, "_intuition: ClearPointer() window=0x%08lx (no-op)\n", (ULONG)window);

    if (!window)
        return;

    window->Pointer = NULL;
    window->PtrHeight = 0;
    window->PtrWidth = 0;
    window->XOffset = 0;
    window->YOffset = 0;
}

BOOL _intuition_CloseScreen ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Screen * screen __asm("a0"))
{
    ULONG display_handle;
    UBYTE i;
    struct LXAIntuitionBase *base = (struct LXAIntuitionBase *)IntuitionBase;
    struct PubScreenNode *pub;
    BOOL was_processing_events;

    DPRINTF (LOG_DEBUG, "_intuition: CloseScreen() screen=0x%08lx\n", (ULONG)screen);

    if (!screen)
    {
        LPRINTF (LOG_ERROR, "_intuition: CloseScreen() called with NULL screen\n");
        return FALSE;
    }

    if (screen->FirstWindow)
    {
        DPRINTF (LOG_DEBUG, "_intuition: CloseScreen() refusing to close screen with open windows\n");
        return FALSE;
    }

    pub = _intuition_find_pubscreen_by_screen(base, screen);
    if (pub && pub->psn_VisitorCount > 0)
    {
        DPRINTF (LOG_DEBUG, "_intuition: CloseScreen() refusing to close screen with visitor count %d\n",
                 pub->psn_VisitorCount);
        return FALSE;
    }

    was_processing_events = g_processing_events;
    g_processing_events = TRUE;

    _intuition_clear_screen_runtime_state(IntuitionBase, screen);

    /* Delete the BarLayer before tearing down the rest of the screen */
    if (screen->BarLayer)
    {
        DeleteLayer(0, screen->BarLayer);
        screen->BarLayer = NULL;
    }

    /* Get the display handle */
    display_handle = (ULONG)screen->ExtData;

    /* Close host display */
    if (display_handle)
    {
        emucall1(EMU_CALL_INT_CLOSE_SCREEN, display_handle);
    }

    /* Free bitplanes */
    for (i = 0; i < screen->BitMap.Depth; i++)
    {
        if (screen->BitMap.Planes[i])
        {
            FreeRaster(screen->BitMap.Planes[i], screen->Width, screen->Height);
        }
    }

    /* Unlink its ViewPort from Intuition's View */
    {
        struct ViewPort **vpp = &IntuitionBase->ViewLord.ViewPort;
        while (*vpp && *vpp != &screen->ViewPort)
            vpp = &(*vpp)->Next;
        if (*vpp)
            *vpp = screen->ViewPort.Next;
        screen->ViewPort.Next = NULL;
    }

    /* Unlink screen from IntuitionBase screen list */
    if (IntuitionBase->FirstScreen == screen)
    {
        IntuitionBase->FirstScreen = screen->NextScreen;
    }
    else
    {
        struct Screen *prev = IntuitionBase->FirstScreen;
        while (prev && prev->NextScreen != screen)
        {
            prev = prev->NextScreen;
        }
        if (prev)
        {
            prev->NextScreen = screen->NextScreen;
        }
    }
    
    /* Update active screen if necessary */
    if (IntuitionBase->ActiveScreen == screen)
    {
        IntuitionBase->ActiveScreen = IntuitionBase->FirstScreen;
    }
    
    _free_screen_sys_gadgets(screen);

    /* Free the RasInfo if allocated */
    if (screen->ViewPort.RasInfo)
    {
        FreeMem(screen->ViewPort.RasInfo, sizeof(struct RasInfo));
    }

    if (screen->ViewPort.ColorMap)
    {
        if (screen->ViewPort.ColorMap->cm_vpe)
            GfxFree((struct ExtendedNode *)screen->ViewPort.ColorMap->cm_vpe);
        FreeColorMap(screen->ViewPort.ColorMap);
    }

    _intuition_unregister_pubscreen(IntuitionBase, screen);

    /* Free the Screen structure */
    FreeMem(screen, sizeof(struct Screen));

    g_processing_events = was_processing_events;

    DPRINTF (LOG_DEBUG, "_intuition: CloseScreen() done\n");

    return TRUE;
}

VOID _intuition_CloseWindow ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Window * window __asm("a0"))
{
    struct LXAWindowState *state;
    struct Layer *border_layer = NULL;
    struct Layer *content_layer = NULL;
    BOOL was_processing_events;
    struct Window *reactivate = NULL;

    DPRINTF (LOG_DEBUG, "_intuition: CloseWindow() window=0x%08lx\n", (ULONG)window);

    if (!window)
    {
        LPRINTF (LOG_ERROR, "_intuition: CloseWindow() called with NULL window\n");
        return;
    }

    was_processing_events = g_processing_events;
    g_processing_events = TRUE;

    _intuition_clear_window_runtime_state(window);

    state = _intuition_find_window_state((struct LXAIntuitionBase *)IntuitionBase, window);

    /* AmigaOS 3.1 (tests/probes/intuition/activation): closing the active
     * window activates the window that was active when it opened (with
     * WFLG_ACTIVATE), if that one is still open.  Forget this window as
     * the predecessor of others. */
    {
        struct Node *node;
        struct LXAIntuitionBase *lb = (struct LXAIntuitionBase *)IntuitionBase;

        if (state && IntuitionBase->ActiveWindow == window)
            reactivate = state->prev_active;
        for (node = lb->WindowStateList.lh_Head; node && node->ln_Succ; node = node->ln_Succ)
            if (((struct LXAWindowState *)node)->prev_active == window)
                ((struct LXAWindowState *)node)->prev_active = NULL;
    }

    /* Close the tracked host window if one exists. */
    if (state && state->host_window_handle)
    {
        DPRINTF (LOG_DEBUG, "_intuition: CloseWindow() closing host window_handle=0x%08lx\n",
                 state->host_window_handle);
        emucall1(EMU_CALL_INT_CLOSE_WINDOW, state->host_window_handle);
        state->host_window_handle = 0;
    }

    /* Dispose any pending and replied IDCMP messages, then remove both ports. */
    _dispose_window_idcmp_ports(window);

    /* requesters still up: their layers go with the window */
    {
        struct Requester *rq;
        for (rq = window->FirstRequest; rq; rq = rq->OlderRequest)
            if (rq->ReqLayer && rq->ReqLayer != window->WLayer)
            {
                DeleteLayer(0, rq->ReqLayer);
                rq->ReqLayer = NULL;
            }
    }

    if (window->BorderRPort)
        border_layer = window->BorderRPort->Layer;
    content_layer = window->WLayer;

    if (content_layer)
    {
        DeleteLayer(0, content_layer);
        window->WLayer = NULL;
    }

    if (border_layer && border_layer != content_layer)
        DeleteLayer(0, border_layer);

    window->RPort = NULL;
    window->BorderRPort = NULL;

    /* Free system gadgets we created */
    _free_window_sys_gadgets(window);
    window->FirstGadget = NULL;

    _intuition_remove_window_state((struct LXAIntuitionBase *)IntuitionBase, window);

    /* Unlink window from screen's window list */
    if (window->WScreen)
    {
        struct Window **wp = &window->WScreen->FirstWindow;
        while (*wp)
        {
            if (*wp == window)
            {
                *wp = window->NextWindow;
                break;
            }
            wp = &(*wp)->NextWindow;
        }
    }

    /* Free ZoomData if we allocated it (via WA_Zoom / ExtData) */
    if (window->ExtData)
    {
        FreeMem(window->ExtData, sizeof(struct ZoomData));
        window->ExtData = NULL;
    }

    /* If this was the active window, clear ActiveWindow */
    if (IntuitionBase->ActiveWindow == window)
    {
        IntuitionBase->ActiveWindow = NULL;
    }

    /* Free the Window structure */
    FreeMem(window, sizeof(struct Window));

    if (reactivate && reactivate != window && !IntuitionBase->ActiveWindow &&
        _intuition_find_window_state((struct LXAIntuitionBase *)IntuitionBase, reactivate))
        _intuition_ActivateWindow(IntuitionBase, reactivate);

    g_processing_events = was_processing_events;

    DPRINTF (LOG_DEBUG, "_intuition: CloseWindow() done\n");
}

LONG _intuition_CloseWorkBench ( register struct IntuitionBase * IntuitionBase __asm("a6"))
{
    struct Screen *wbscreen;

    DPRINTF (LOG_DEBUG, "_intuition: CloseWorkBench()\n");

    wbscreen = _intuition_find_workbench_screen(IntuitionBase);
    if (!wbscreen)
        return FALSE;

    return _intuition_CloseScreen(IntuitionBase, wbscreen) ? TRUE : FALSE;
}

VOID _intuition_CurrentTime ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register ULONG * seconds __asm("a0"),
                                                        register ULONG * micros __asm("a1"))
{
    struct timeval tv;
    emucall1(EMU_CALL_GETSYSTIME, (ULONG)&tv);
    
    if (seconds)
        *seconds = tv.tv_secs;
    if (micros)
        *micros = tv.tv_micro;
    
    DPRINTF(LOG_DEBUG, "_intuition: CurrentTime() -> seconds=%lu, micros=%lu\n",
            tv.tv_secs, tv.tv_micro);
}

BOOL _intuition_DisplayAlert ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register ULONG alertNumber __asm("d0"),
                                                        register CONST_STRPTR string __asm("a0"),
                                                        register UWORD height __asm("d1"))
{
    /*
     * DisplayAlert() shows an alert and waits until the user presses a
     * mouse button: left = TRUE (continue), right = FALSE.  A dead-end
     * alert returns FALSE (AmigaOS reboots).  AmigaOS 3.1 blocks until the
     * click (Fish VMK on the reference); lxa used to answer "continue" at
     * once, which made VMK and DirWork reboot through Supervisor().
     * The string is a list of (x.w, y.b, text, continuation.b) entries.
     */
    LXA_UNIMPLEMENTED("intuition", "DisplayAlert", "partial: the alert box is not drawn (logged), waits for a mouse button");
    LPRINTF (LOG_WARNING, "_intuition: DisplayAlert() alertNumber=0x%08lx height=%u\n",
             alertNumber, (unsigned)height);
    if (string)
    {
        const UBYTE *p = (const UBYTE *)string;
        for (;;)
        {
            p += 3;                         /* x (WORD), y (BYTE) */
            LPRINTF (LOG_WARNING, "_intuition: DisplayAlert(): %s\n", (const char *)p);
            while (*p)
                p++;
            p++;
            if (!*p++)                      /* continuation byte */
                break;
        }
    }

    g_alert_button = 0;
    g_alert_waiting = TRUE;
    while (!g_alert_button)
        WaitTOF();
    g_alert_waiting = FALSE;

    if (alertNumber & DEADEND_ALERT)
        return FALSE;
    return g_alert_button == SELECTDOWN;
}

VOID _intuition_DisplayBeep ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Screen * screen __asm("a0"))
{
    LXA_UNIMPLEMENTED("intuition", "DisplayBeep", "stub: no screen flash or bell (Phase 256)");

    /*
     * DisplayBeep() flashes the screen colors as an audio/visual alert.
     * We just log it as a no-op.
     */
    DPRINTF (LOG_DEBUG, "_intuition: DisplayBeep() screen=0x%08lx (no-op)\n", (ULONG)screen);
}

BOOL _intuition_DoubleClick ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register ULONG sSeconds __asm("d0"),
                                                        register ULONG sMicros __asm("d1"),
                                                        register ULONG cSeconds __asm("d2"),
                                                        register ULONG cMicros __asm("d3"))
{
    struct LXAIntuitionBase *base = (struct LXAIntuitionBase *)IntuitionBase;
    /* Check if two times are within the double-click interval.
     * Based on AROS implementation.
     * 
     * sSeconds, sMicros = first (start) click time
     * cSeconds, cMicros = second (current) click time
     */
    ULONG sTotal, cTotal;
    ULONG diff;
    ULONG doubleClickTime;
    
    DPRINTF (LOG_DEBUG, "_intuition: DoubleClick(s=%lu.%06lu c=%lu.%06lu)\n",
             sSeconds, sMicros, cSeconds, cMicros);
    
    /* If times are more than 4 seconds apart, definitely not a double-click */
    if (sSeconds > cSeconds) {
        if (sSeconds - cSeconds > 4)
            return FALSE;
    } else {
        if (cSeconds - sSeconds > 4)
            return FALSE;
    }
    
    /* Convert to microseconds relative to the minimum second */
    ULONG baseSeconds = (sSeconds < cSeconds) ? sSeconds : cSeconds;
    sTotal = (sSeconds - baseSeconds) * 1000000 + sMicros;
    cTotal = (cSeconds - baseSeconds) * 1000000 + cMicros;
    
    /* Calculate absolute difference */
    diff = (sTotal > cTotal) ? (sTotal - cTotal) : (cTotal - sTotal);
    
    doubleClickTime = base->ActivePrefs.DoubleClick.tv_secs * 1000000UL +
                      base->ActivePrefs.DoubleClick.tv_micro;
    if (doubleClickTime == 0)
        doubleClickTime = 500000;
    
    DPRINTF (LOG_DEBUG, "_intuition: DoubleClick diff=%lu threshold=%lu\n", diff, doubleClickTime);
    
    return (diff <= doubleClickTime);
}

VOID _intuition_DrawBorder ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct RastPort * rp __asm("a0"),
                                                        register const struct Border * border __asm("a1"),
                                                        register WORD leftOffset __asm("d0"),
                                                        register WORD topOffset __asm("d1"))
{
    /* Draw one or more borders in the specified RastPort.
     * Based on AROS implementation.
     */
    UBYTE savedAPen, savedBPen, savedDrMd;
    WORD *ptr;
    WORD x, y;
    int t;
    
    DPRINTF (LOG_DEBUG, "_intuition: DrawBorder(rp=%p, border=%p, off=%d,%d)\n",
             rp, border, (int)leftOffset, (int)topOffset);
    
    if (!rp || !border)
        return;
    
    /* Save current RastPort state */
    savedAPen = rp->FgPen;
    savedBPen = rp->BgPen;
    savedDrMd = rp->DrawMode;
    
    /* Lock layer if present */
    if (rp->Layer)
        LockLayerRom(rp->Layer);
    
    /* For all borders in the chain... */
    for ( ; border; border = border->NextBorder)
    {
        /* Change RastPort to the colors/mode specified */
        SetAPen(rp, border->FrontPen);
        SetBPen(rp, border->BackPen);
        SetDrMd(rp, border->DrawMode);
        
        /* Get base coords */
        x = border->LeftEdge + leftOffset;
        y = border->TopEdge + topOffset;
        
        /* Start of vector offsets */
        ptr = border->XY;
        
        if (!ptr || border->Count <= 0)
            continue;
        
        for (t = 0; t < border->Count; t++)
        {
            /* Get vector offset */
            WORD xoff = *ptr++;
            WORD yoff = *ptr++;
            
            if (t == 0)
            {
                /* First point - just move */
                Move(rp, x + xoff, y + yoff);
            }
            else
            {
                /* Draw line to this point */
                Draw(rp, x + xoff, y + yoff);
            }
        }
    }
    
    /* Restore RastPort state */
    SetAPen(rp, savedAPen);
    SetBPen(rp, savedBPen);
    SetDrMd(rp, savedDrMd);
    
    /* Unlock layer if present */
    if (rp->Layer)
        UnlockLayerRom(rp->Layer);
}

VOID _intuition_DrawImage ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct RastPort * rp __asm("a0"),
                                                        register struct Image * image __asm("a1"),
                                                        register WORD leftOffset __asm("d0"),
                                                        register WORD topOffset __asm("d1"))
{
    DPRINTF (LOG_DEBUG, "_intuition: DrawImage() rp=0x%08lx image=0x%08lx at %d,%d\n",
             (ULONG)rp, (ULONG)image, (int)leftOffset, (int)topOffset);

    _intuition_DrawImageState(IntuitionBase, rp, image, leftOffset, topOffset,
                              IDS_NORMAL, NULL);
}

VOID _intuition_EndRequest ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Requester * requester __asm("a0"),
                                                        register struct Window * window __asm("a1"))
{
    struct Requester *curr, *prev = NULL;
    LONG left, top, width, height;

    DPRINTF (LOG_DEBUG, "_intuition: EndRequest() req=0x%08lx win=0x%08lx\n", (ULONG)requester, (ULONG)window);
    
    if (!requester || !window) return;
    
    /* Find and unlink */
    curr = window->FirstRequest;
    while (curr && curr != requester)
    {
        prev = curr;
        curr = curr->OlderRequest;
    }
    
    if (!curr) return; /* Not found */
    
    if (prev)
    {
        prev->OlderRequest = requester->OlderRequest;
    }
    else
    {
        window->FirstRequest = requester->OlderRequest;
    }
    
    requester->Flags &= ~REQACTIVE;
    requester->OlderRequest = NULL;
    if (window->ReqCount > 0)
        window->ReqCount--;
    if (!window->ReqCount)
        window->Flags &= ~WFLG_INREQUEST;

    if (requester->ReqLayer && requester->ReqLayer != window->WLayer)
    {
        /* deleting the layer uncovers the window: its backing store (smart
         * refresh) or a refresh message (simple refresh) restores it */
        DeleteLayer(0, requester->ReqLayer);
        requester->ReqLayer = NULL;
        _rerender_requester_stack(window);
    }
    else if (window->RPort)
    {
        _calculate_requester_box(window, requester, &left, &top, &width, &height);
        UBYTE save_fg = window->RPort->FgPen;

        SetAPen(window->RPort, 0);
        RectFill(window->RPort, left, top, left + width - 1, top + height - 1);
        SetAPen(window->RPort, save_fg);
        _rerender_requester_stack(window);
    }
    /* AmigaOS 3.1 clears ReqLayer but keeps RWindow */
    requester->ReqLayer = NULL;

    /* Post IDCMP_REQCLEAR to notify the window that a requester was removed.
     * Per RKRM: one REQCLEAR is sent for each requester closed in the window. */
    _post_idcmp_message(window, IDCMP_REQCLEAR, 0, 0, NULL,
                        window->MouseX, window->MouseY);
}

struct Preferences * _intuition_GetDefPrefs ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Preferences * preferences __asm("a0"),
                                                        register WORD size __asm("d0"))
{
    struct LXAIntuitionBase *base = (struct LXAIntuitionBase *)IntuitionBase;

    DPRINTF (LOG_DEBUG, "_intuition: GetDefPrefs() preferences=0x%08lx size=%d\n",
             (ULONG)preferences, (int)size);

    if (!preferences)
        return NULL;

    return _intuition_copy_prefs(preferences, size, &base->DefaultPrefs);
}

struct Preferences * _intuition_GetPrefs ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Preferences * preferences __asm("a0"),
                                                        register WORD size __asm("d0"))
{
    struct LXAIntuitionBase *base = (struct LXAIntuitionBase *)IntuitionBase;

    DPRINTF (LOG_DEBUG, "_intuition: GetPrefs() preferences=0x%08lx size=%d\n",
             (ULONG)preferences, (int)size);

    if (!preferences)
        return NULL;

    return _intuition_copy_prefs(preferences, size, &base->ActivePrefs);
}

VOID _intuition_InitRequester ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Requester * requester __asm("a0"))
{
    DPRINTF (LOG_DEBUG, "_intuition: InitRequester() req=0x%08lx\n", (ULONG)requester);
    
    if (!requester) return;

    memset(requester, 0, sizeof(*requester));
}

struct MenuItem * _intuition_ItemAddress ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register const struct Menu * menuStrip __asm("a0"),
                                                        register UWORD menuNumber __asm("d0"))
{
    struct Menu *menu;
    struct MenuItem *item = NULL;
    WORD i;
    WORD menuNum;
    WORD itemNum;
    WORD subNum;

    DPRINTF (LOG_DEBUG, "_intuition: ItemAddress() menuStrip=0x%08lx menuNumber=0x%04x\n",
             (ULONG)menuStrip, (UWORD)menuNumber);

    /* MENUNULL means no menu selected */
    if (menuNumber == MENUNULL)
    {
        return NULL;
    }

    if (!menuStrip)
    {
        LPRINTF (LOG_ERROR, "_intuition: ItemAddress() called with NULL menuStrip\n");
        return NULL;
    }

    /* Extract menu, item, and sub-item numbers from packed value */
    menuNum = MENUNUM(menuNumber);
    itemNum = ITEMNUM(menuNumber);
    subNum = SUBNUM(menuNumber);

    DPRINTF (LOG_DEBUG, "_intuition: ItemAddress() menu=%d item=%d sub=%d\n",
             menuNum, itemNum, subNum);

    /* Navigate to the correct Menu */
    menu = (struct Menu *)menuStrip;
    for (i = 0; menu && i < menuNum; i++)
    {
        menu = menu->NextMenu;
    }

    if (!menu)
    {
        DPRINTF (LOG_DEBUG, "_intuition: ItemAddress() menu not found\n");
        return NULL;
    }

    /* Navigate to the correct MenuItem */
    item = menu->FirstItem;
    for (i = 0; item && i < itemNum; i++)
    {
        item = item->NextItem;
    }

    if (!item)
    {
        DPRINTF (LOG_DEBUG, "_intuition: ItemAddress() item not found\n");
        return NULL;
    }

    /* If there's a sub-item and it's specified, navigate to it */
    if (subNum != NOSUB && item->SubItem)
    {
        item = item->SubItem;
        for (i = 0; item && i < subNum; i++)
        {
            item = item->NextItem;
        }
    }

    DPRINTF (LOG_DEBUG, "_intuition: ItemAddress() returning 0x%08lx\n", (ULONG)item);
    return item;
}

BOOL _intuition_ModifyIDCMP ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Window * window __asm("a0"),
                                                        register ULONG flags __asm("d0"))
{
    DPRINTF (LOG_DEBUG, "_intuition: ModifyIDCMP() window=0x%08lx flags=0x%08lx\n",
             (ULONG)window, (ULONG)flags);

    if (!window)
    {
        LPRINTF (LOG_ERROR, "_intuition: ModifyIDCMP() called with NULL window\n");
        return FALSE;
    }

    if (flags != 0)
    {
        if (!_ensure_window_idcmp_ports(window))
        {
            LPRINTF (LOG_ERROR, "_intuition: ModifyIDCMP() failed to create port\n");
            return FALSE;
        }
    }
    else
    {
        _dispose_window_idcmp_ports(window);
    }

    /* Update the flags */
    window->IDCMPFlags = flags;

    return TRUE;
}

/*
 * Find the window at a given screen position
 * Returns the topmost window containing the point, or NULL if none
 */
static struct Window * _find_window_at_pos(struct Screen *screen, WORD x, WORD y)
{
    struct Window *window;
    struct Window *found = NULL;
    
    if (!screen)
        return NULL;
    
    /* Walk through windows - first window is frontmost due to our list order */
    for (window = screen->FirstWindow; window; window = window->NextWindow)
    {
        /* Check if point is inside window bounds */
        if (x >= window->LeftEdge && 
            x < window->LeftEdge + window->Width &&
            y >= window->TopEdge &&
            y < window->TopEdge + window->Height)
        {
            /* Found a window - since we iterate front-to-back, take the first match */
            if (!found)
                found = window;
        }
    }
    
    return found;
}

/*
 * Find a gadget at a given position within a window
 * Coordinates are window-relative
 * Returns the gadget or NULL if none found
 */
static void _calculate_gadget_box(struct Window *window, struct Requester *req,
                                  struct Gadget *gad,
                                  LONG *left, LONG *top,
                                  LONG *width, LONG *height)
{
    LONG calc_left;
    LONG calc_top;
    LONG calc_width;
    LONG calc_height;

    if (!window || !gad)
    {
        if (left)
            *left = 0;
        if (top)
            *top = 0;
        if (width)
            *width = 0;
        if (height)
            *height = 0;
        return;
    }

    calc_left = gad->LeftEdge;
    calc_top = gad->TopEdge;
    calc_width = gad->Width;
    calc_height = gad->Height;

    if (req)
    {
        LONG req_left;
        LONG req_top;
        LONG req_width;
        LONG req_height;

        _calculate_requester_box(window, req, &req_left, &req_top, &req_width, &req_height);
        calc_left += req_left;
        calc_top += req_top;
    }

    if (gad->Flags & GFLG_RELRIGHT)
    {
        LONG container_width;
        if (req)
            container_width = req->Width;
        else if ((window->Flags & WFLG_GIMMEZEROZERO) &&
                 !(gad->GadgetType & (GTYP_GZZGADGET | GTYP_SYSGADGET)))
            container_width = window->GZZWidth;
        else
            container_width = window->Width;
        calc_left += container_width - 1;
    }

    if (gad->Flags & GFLG_RELBOTTOM)
    {
        LONG container_height;
        if (req)
            container_height = req->Height;
        else if ((window->Flags & WFLG_GIMMEZEROZERO) &&
                 !(gad->GadgetType & (GTYP_GZZGADGET | GTYP_SYSGADGET)))
            container_height = window->GZZHeight;
        else
            container_height = window->Height;
        calc_top += container_height - 1;
    }

    if (gad->Flags & GFLG_RELWIDTH)
    {
        LONG container_width;
        if (req)
            container_width = req->Width;
        else if ((window->Flags & WFLG_GIMMEZEROZERO) &&
                 !(gad->GadgetType & (GTYP_GZZGADGET | GTYP_SYSGADGET)))
            container_width = window->GZZWidth;
        else
            container_width = window->Width;
        calc_width += container_width;
    }

    if (gad->Flags & GFLG_RELHEIGHT)
    {
        LONG container_height;
        if (req)
            container_height = req->Height;
        else if ((window->Flags & WFLG_GIMMEZEROZERO) &&
                 !(gad->GadgetType & (GTYP_GZZGADGET | GTYP_SYSGADGET)))
            container_height = window->GZZHeight;
        else
            container_height = window->Height;
        calc_height += container_height;
    }

    if (left)
        *left = calc_left;
    if (top)
        *top = calc_top;
    if (width)
        *width = calc_width;
    if (height)
        *height = calc_height;
}

static void _clear_relative_gadget_trails(struct Window *window, WORD dx, WORD dy)
{
    struct Gadget *gad;
    struct RastPort *rp;

    if (!window || !window->RPort)
        return;

    rp = window->RPort;

    for (gad = window->FirstGadget; gad; gad = gad->NextGadget)
    {
        LONG new_left;
        LONG new_top;
        LONG new_width;
        LONG new_height;
        LONG old_left;
        LONG old_top;
        LONG old_width;
        LONG old_height;

        if (!(gad->Flags & (GFLG_RELRIGHT | GFLG_RELBOTTOM | GFLG_RELWIDTH | GFLG_RELHEIGHT)))
            continue;

        _calculate_gadget_box(window, NULL, gad, &new_left, &new_top, &new_width, &new_height);

        old_left = new_left - ((gad->Flags & GFLG_RELRIGHT) ? dx : 0);
        old_top = new_top - ((gad->Flags & GFLG_RELBOTTOM) ? dy : 0);
        old_width = new_width - ((gad->Flags & GFLG_RELWIDTH) ? dx : 0);
        old_height = new_height - ((gad->Flags & GFLG_RELHEIGHT) ? dy : 0);

        if (old_width <= 0 || old_height <= 0)
            continue;

        if (old_left == new_left && old_top == new_top &&
            old_width == new_width && old_height == new_height)
            continue;

        SetAPen(rp, 0);
        RectFill(rp,
                 old_left,
                 old_top,
                 old_left + old_width - 1,
                 old_top + old_height - 1);
    }
}

static void _clear_resized_window_exposed_areas(struct Window *window,
                                                WORD old_width,
                                                WORD old_height)
{
    struct RastPort *rp;
    WORD old_inner_right;
    WORD new_inner_right;
    WORD old_inner_bottom;
    WORD new_inner_bottom;

    if (!window || !window->RPort)
        return;

    rp = window->RPort;
    SetAPen(rp, 0);

    old_inner_right = old_width - window->BorderRight - 1;
    new_inner_right = window->Width - window->BorderRight - 1;
    old_inner_bottom = old_height - window->BorderBottom - 1;
    new_inner_bottom = window->Height - window->BorderBottom - 1;

    if (new_inner_right > old_inner_right &&
        window->BorderTop <= new_inner_bottom)
    {
        RectFill(rp,
                 old_inner_right + 1,
                 window->BorderTop,
                 new_inner_right,
                 new_inner_bottom);
    }

    if (new_inner_bottom > old_inner_bottom &&
        window->BorderLeft <= new_inner_right)
    {
        RectFill(rp,
                 window->BorderLeft,
                 old_inner_bottom + 1,
                 new_inner_right,
                 new_inner_bottom);
    }
}

static struct Gadget *_find_gadget_at_pos_in_list(struct Window *window,
                                                  struct Gadget *first_gadget,
                                                  WORD relX,
                                                  WORD relY)
{
    struct Gadget *gad;
    LONG gx0, gy0, width, height;

    for (gad = first_gadget; gad; gad = gad->NextGadget)
    {
        if (gad->Flags & GFLG_DISABLED)
            continue;

        /* screen title bar gadgets are handled by the screen code */
        if ((gad->GadgetType & (GTYP_SYSGADGET | GTYP_SCRGADGET)) == (GTYP_SYSGADGET | GTYP_SCRGADGET))
            continue;

        /* the drag bar gadget is handled by the window drag code */
        if (_is_sys_gadget(gad) && (gad->GadgetType & GTYP_SYSTYPEMASK) == GTYP_WDRAGGING)
            continue;

        _calculate_gadget_box(window, NULL, gad, &gx0, &gy0, &width, &height);
        
        DPRINTF(LOG_DEBUG, "_intuition: _find_gadget_at_pos() checking gad=0x%08lx type=0x%04x bounds=(%d,%d)-(%d,%d) point=(%d,%d)\n",
                (ULONG)gad, gad->GadgetType,
                (int)gx0, (int)gy0, (int)(gx0 + width), (int)(gy0 + height),
                relX, relY);
        
        /* Check if point is inside gadget bounds */
        if (relX >= gx0 && relX < gx0 + width && relY >= gy0 && relY < gy0 + height)
        {
            DPRINTF(LOG_DEBUG, "_intuition: _find_gadget_at_pos() hit gadget type=0x%04x at (%d,%d)\n",
                    gad->GadgetType, (int)relX, (int)relY);
            return gad;
        }
    }

    return NULL;
}

static struct Gadget * _find_gadget_at_pos(struct Window *window, WORD relX, WORD relY)
{
    struct Gadget *gad;

    if (!window)
        return NULL;

    gad = _find_gadget_at_pos_in_list(window, window->FirstGadget, relX, relY);
    if (gad)
        return gad;

    if (window->WScreen && window->WScreen->FirstGadget != window->FirstGadget)
        return _find_gadget_at_pos_in_list(window, window->WScreen->FirstGadget, relX, relY);

    return NULL;
}

/*
 * Check if a point is inside a gadget (for RELVERIFY validation)
 * Used when mouse button is released to verify we're still inside the gadget
 */
static BOOL _point_in_gadget(struct Window *window, struct Gadget *gad, WORD relX, WORD relY)
{
    LONG gx0, gy0, width, height;
    
    if (!window || !gad)
        return FALSE;
    
    _calculate_gadget_box(window, NULL, gad, &gx0, &gy0, &width, &height);
    
    return (relX >= gx0 && relX < gx0 + width && relY >= gy0 && relY < gy0 + height);
}

static void _rerender_requester_stack(struct Window *window)
{
    struct Requester *stack[10];
    struct Requester *req;
    WORD count;
    WORD i;

    if (!window)
        return;

    if (window->FirstGadget)
        _intuition_RefreshGList(IntuitionBase, window->FirstGadget, window, NULL, -1);

    count = 0;
    req = window->FirstRequest;
    while (req && count < 10)
    {
        stack[count++] = req;
        req = req->OlderRequest;
    }

    for (i = count - 1; i >= 0; i--)
        _render_requester(window, stack[i]);
}

/* Forward declarations for WindowToBack/WindowToFront (used by depth gadget handler) */
VOID _intuition_WindowToBack(register struct IntuitionBase *IntuitionBase __asm("a6"),
                             register struct Window *window __asm("a0"));
VOID _intuition_WindowToFront(register struct IntuitionBase *IntuitionBase __asm("a6"),
                              register struct Window *window __asm("a0"));

/*
 * Handle system gadget action when mouse is released inside the gadget
 * This is called after RELVERIFY confirms the click
 */
static void _handle_sys_gadget_verify(struct Window *window, struct Gadget *gadget)
{
    UWORD sysType;
    
    if (!window || !gadget)
        return;
    
    /* Must be a system gadget */
    if (!(gadget->GadgetType & GTYP_SYSGADGET))
        return;
    
    sysType = gadget->GadgetType & GTYP_SYSTYPEMASK;
    
    DPRINTF(LOG_DEBUG, "_intuition: _handle_sys_gadget_verify() sysType=0x%04x\n", sysType);
    
    switch (sysType)
    {
        case GTYP_CLOSE:
            /* Fire IDCMP_CLOSEWINDOW message */
            DPRINTF(LOG_DEBUG, "_intuition: Close gadget clicked - posting IDCMP_CLOSEWINDOW\n");
            _post_idcmp_message(window, IDCMP_CLOSEWINDOW, 0, 0, window, 
                               window->MouseX, window->MouseY);
            break;
        
        case GTYP_WDEPTH:
        {
            /* Toggle window depth: if frontmost, send to back; else bring to front.
             * Per RKRM, clicking the depth gadget cycles the window z-order.
             * WindowToBack/WindowToFront handle layers, rootless, and IDCMP. */
            struct Screen *scr = window->WScreen;
            if (scr && scr->FirstWindow == window && window->NextWindow)
            {
                /* Window is frontmost and there are others — send to back */
                struct Window *last;

                /* Unlink from front of list */
                scr->FirstWindow = window->NextWindow;

                /* Append to end of list */
                for (last = scr->FirstWindow; last->NextWindow; last = last->NextWindow)
                    ;
                last->NextWindow = window;
                window->NextWindow = NULL;

                _intuition_WindowToBack(IntuitionBase, window);
                _post_idcmp_message(window, IDCMP_CHANGEWINDOW, CWCODE_DEPTH, 0,
                                    window, window->MouseX, window->MouseY);
                DPRINTF(LOG_DEBUG, "_intuition: Depth gadget - WindowToBack()\n");
            }
            else if (scr)
            {
                /* Window is not frontmost — bring to front */
                struct Window **wp;

                /* Unlink from current position */
                for (wp = &scr->FirstWindow; *wp; wp = &(*wp)->NextWindow)
                {
                    if (*wp == window)
                    {
                        *wp = window->NextWindow;
                        break;
                    }
                }

                /* Prepend to front of list */
                window->NextWindow = scr->FirstWindow;
                scr->FirstWindow = window;

                _intuition_WindowToFront(IntuitionBase, window);
                _post_idcmp_message(window, IDCMP_CHANGEWINDOW, CWCODE_DEPTH, 0,
                                    window, window->MouseX, window->MouseY);
                DPRINTF(LOG_DEBUG, "_intuition: Depth gadget - WindowToFront()\n");
            }
            break;
        }
        
        case GTYP_WDRAGGING:
            /* Drag bar - handled separately during mouse move */
            break;
        
        case GTYP_SIZING:
            /* Resize gadget - handled separately during mouse move */
            break;
        
        case GTYP_WZOOM:
            /* Zoom gadget — toggle window between normal and zoomed size */
            DPRINTF(LOG_DEBUG, "_intuition: Zoom gadget clicked - ZipWindow\n");
            _intuition_ZipWindow(IntuitionBase, window);
            break;
    }
}

static void _calculate_requester_box(struct Window *window, struct Requester *req,
                                     LONG *left, LONG *top,
                                     LONG *width, LONG *height)
{
    LONG calc_left;
    LONG calc_top;
    LONG calc_width;
    LONG calc_height;

    if (!window || !req)
    {
        if (left)
            *left = 0;
        if (top)
            *top = 0;
        if (width)
            *width = 0;
        if (height)
            *height = 0;
        return;
    }

    calc_left = req->LeftEdge;
    calc_top = req->TopEdge;
    calc_width = req->Width;
    calc_height = req->Height;

    if (calc_width < 0)
        calc_width = 0;
    if (calc_height < 0)
        calc_height = 0;

    if (req->Flags & POINTREL)
    {
        calc_left = ((LONG)window->Width - calc_width) / 2 + req->RelLeft;
        calc_top = ((LONG)window->Height - calc_height) / 2 + req->RelTop;
    }

    if (calc_left < 0)
        calc_left = 0;
    if (calc_top < 0)
        calc_top = 0;

    if (calc_width > window->Width)
        calc_left = 0;
    else if (calc_left + calc_width > window->Width)
        calc_left = window->Width - calc_width;

    if (calc_height > window->Height)
        calc_top = 0;
    else if (calc_top + calc_height > window->Height)
        calc_top = window->Height - calc_height;

    if (left)
        *left = calc_left;
    if (top)
        *top = calc_top;
    if (width)
        *width = calc_width;
    if (height)
        *height = calc_height;
}

static UWORD g_menu_selection;              /* Encoded menu selection */

/* Menu drop-down save-behind and off-screen composition bitmaps.
 *
 * Phase 112 (v0.8.77) replaced the byte-aligned planar save/restore with a
 * pair of off-screen BitMaps allocated via AllocBitMap():
 *
 *   g_menu_save_bm      Saves the screen content beneath the dropdown so it
 *                       can be restored verbatim when the menu closes.
 *                       BltBitMap() handles non-byte-aligned X coordinates
 *                       correctly via shifting, eliminating the previous
 *                       1-7 pixel edge artifact.
 *
 * NOTE: No initializers — .bss must be in RAM, not ROM .data section. */
static struct BitMap *g_menu_save_bm;       /* save-behind, sized w x h */
static WORD   g_menu_save_x;               /* Left edge of saved area on screen */
static WORD   g_menu_save_y;               /* Top edge of saved area on screen */
static WORD   g_menu_save_w;               /* Width of saved area in pixels */
static WORD   g_menu_save_h;               /* Height of saved area in pixels */

/* Phase 133: off-screen compose BitMap for atomic menu rendering.
 *
 * Menu bar and drop-down chains are rendered into this off-screen
 * BitMap with origin (0,0); the result is BltBitMap'd onto the screen
 * in a single operation, eliminating the visible "blank then redraw"
 * flicker that direct-to-screen rendering produced.
 *
 * The bitmap is allocated lazily on first menu activation and grown
 * (re-allocated) when a render request exceeds its current size. It
 * is freed when menu mode exits via _intuition_discard_menu_runtime_state()
 * or _exit_menu_mode().
 *
 * .bss must be in RAM, not ROM .data section — no initializers. */
static struct BitMap *g_menu_compose_bm;
static WORD   g_menu_compose_w;
static WORD   g_menu_compose_h;
static WORD   g_menu_compose_depth;

static VOID _intuition_discard_menu_runtime_state(VOID)
{
    if (g_menu_save_bm)
    {
        FreeBitMap(g_menu_save_bm);
        g_menu_save_bm = NULL;
    }

    if (g_menu_compose_bm)
    {
        FreeBitMap(g_menu_compose_bm);
        g_menu_compose_bm = NULL;
    }
    g_menu_compose_w = 0;
    g_menu_compose_h = 0;
    g_menu_compose_depth = 0;

    g_menu_mode = FALSE;
    g_menu_window = NULL;
    g_active_menu = NULL;
    g_active_item = NULL;
    g_active_subitem = NULL;
    g_flagged_menu = NULL;
    g_flagged_item = NULL;
    g_flagged_subitem = NULL;
    g_menu_selection = MENUNULL;
    g_menu_save_x = 0;
    g_menu_save_y = 0;
    g_menu_save_w = 0;
    g_menu_save_h = 0;
}

static VOID _intuition_clear_window_runtime_state(struct Window *window)
{
    if (!window)
        return;

    if (g_active_window == window)
    {
        g_active_window = NULL;
        g_active_gadget = NULL;
        g_prop_click_offset = 0;
    }

    if (g_drag_window == window)
    {
        g_dragging_window = FALSE;
        g_drag_window = NULL;
        g_drag_start_x = 0;
        g_drag_start_y = 0;
        g_drag_window_x = 0;
        g_drag_window_y = 0;
    }

    if (g_size_window == window)
    {
        g_sizing_window = FALSE;
        g_size_window = NULL;
        g_size_start_x = 0;
        g_size_start_y = 0;
        g_size_orig_w = 0;
        g_size_orig_h = 0;
    }

    if (g_menu_window == window)
        _intuition_discard_menu_runtime_state();
}

static VOID _intuition_clear_screen_runtime_state(struct IntuitionBase *IntuitionBase,
                                                  struct Screen *screen)
{
    struct Window *window;

    if (!screen)
        return;

    if (g_menu_window && g_menu_window->WScreen == screen)
        _intuition_discard_menu_runtime_state();

    for (window = screen->FirstWindow; window; window = window->NextWindow)
        _intuition_clear_window_runtime_state(window);

    if (IntuitionBase && IntuitionBase->ActiveWindow && IntuitionBase->ActiveWindow->WScreen == screen)
        IntuitionBase->ActiveWindow = NULL;
}

static VOID _intuition_reset_runtime_state(VOID)
{
    _intuition_discard_menu_runtime_state();

    g_active_gadget = NULL;
    g_active_window = NULL;
    g_prop_click_offset = 0;
    g_dragging_window = FALSE;
    g_drag_window = NULL;
    g_drag_start_x = 0;
    g_drag_start_y = 0;
    g_drag_window_x = 0;
    g_drag_window_y = 0;
    g_sizing_window = FALSE;
    g_size_window = NULL;
    g_size_start_x = 0;
    g_size_start_y = 0;
    g_size_orig_w = 0;
    g_size_orig_h = 0;
    g_processing_events = FALSE;
}

/* Forward declarations for functions used by the input event handler */
static void _render_window_frame(struct Window *window);
VOID _intuition_SizeWindow(register struct IntuitionBase *IntuitionBase __asm("a6"),
                           register struct Window *window __asm("a0"),
                           register WORD dx __asm("d0"),
                            register WORD dy __asm("d1"));

/*
 * Initialize StringInfo fields for string gadgets in a gadget list.
 * Per RKRM, Intuition initializes NumChars when the gadget is added to a window.
 * This must be called when gadgets are first linked to a window (OpenWindow, AddGList).
 */
static VOID _init_string_gadget_info(struct Gadget *gadget)
{
    if (!gadget)
        return;
    
    if ((gadget->GadgetType & GTYP_GTYPEMASK) == GTYP_STRGADGET)
    {
        struct StringInfo *si = (struct StringInfo *)gadget->SpecialInfo;
        if (si && si->Buffer)
        {
            /* AmigaOS 3.1 leaves NumChars alone here; rendering the gadget
             * recomputes it (tests/probes/intuition/strgad) */
            WORD len = 0;
            while (si->Buffer[len] != '\0' && len < si->MaxChars)
                len++;

            if (si->BufferPos < 0)
                si->BufferPos = 0;
            else if (si->BufferPos > len)
                si->BufferPos = len;

            if (si->DispPos < 0)
                si->DispPos = 0;
            else if (si->DispPos > len)
                si->DispPos = len;
        }
    }
}

/*
 * gi_Domain of the GadgetInfo Intuition passes to BOOPSI gadgets (AmigaOS
 * 3.1, tests/probes/intuition/gadinfo): the whole window, or the inner
 * area of a GIMMEZEROZERO window - for every method (GM_LAYOUT, GM_RENDER,
 * SetGadgetAttrsA's OM_SET, DoGadgetMethodA).
 */
static VOID _intuition_gadget_domain(struct Window *window, struct IBox *domain)
{
    if (window->Flags & WFLG_GIMMEZEROZERO)
    {
        domain->Left = window->BorderLeft;
        domain->Top = window->BorderTop;
        domain->Width = window->Width - window->BorderLeft - window->BorderRight;
        domain->Height = window->Height - window->BorderTop - window->BorderBottom;
    }
    else
    {
        domain->Left = 0;
        domain->Top = 0;
        domain->Width = window->Width;
        domain->Height = window->Height;
    }
}

/*
 * GM_LAYOUT for BOOPSI gadgets whose size depends on the window
 * (GFLG_REL*): sent when the gadget joins a window and after every size
 * change (gpl_Initial FALSE), as Intuition V39 does.
 */
static VOID _layout_custom_gadget(struct Window *window, struct Requester *req, struct Gadget *gad,
                                  BOOL initial)
{
    struct IClass *cl;
    struct GadgetInfo gi;
    struct gpLayout gpl;

    if (!window || !gad || (gad->GadgetType & GTYP_GTYPEMASK) != GTYP_CUSTOMGADGET ||
        (gad->GadgetType & GTYP_SYSGADGET) ||
        !(gad->Flags & (GFLG_RELRIGHT | GFLG_RELBOTTOM | GFLG_RELWIDTH | GFLG_RELHEIGHT | GFLG_RELSPECIAL)))
        return;
    (void)cl;
    memset(&gi, 0, sizeof(gi));
    gi.gi_Screen = window->WScreen;
    gi.gi_Window = window;
    gi.gi_Requester = req;
    gi.gi_RastPort = window->RPort;
    gi.gi_Layer = window->WLayer;
    _intuition_gadget_domain(window, &gi.gi_Domain);
    gi.gi_DrInfo = _intuition_GetScreenDrawInfo(IntuitionBase, window->WScreen);
    gpl.MethodID = GM_LAYOUT;
    gpl.gpl_GInfo = &gi;
    gpl.gpl_Initial = initial;
    _custom_gadget_call(gad, (Msg)&gpl, NULL);
    if (gi.gi_DrInfo)
        _intuition_FreeScreenDrawInfo(IntuitionBase, window->WScreen, gi.gi_DrInfo);
}

/*
 * AmigaOS 3.1 marks every gadget that reaches into the window border with
 * GACT_BORDERSNIFF when it joins a window (reference: dopus-startup and
 * devpac-edit goldens): border gadgets (GACT_*BORDER) and gadgets whose box
 * leaves the window's inner area.
 */
static VOID _sniff_border_gadget(struct Window *window, struct Requester *req, struct Gadget *gad)
{
    LONG l, t, w, h;

    if (!window || req || !gad || (gad->GadgetType & GTYP_SYSGADGET) ||
        (window->Flags & WFLG_GIMMEZEROZERO))
        return;
    if (gad->Activation & (GACT_RIGHTBORDER | GACT_LEFTBORDER | GACT_TOPBORDER | GACT_BOTTOMBORDER))
    {
        gad->Activation |= GACT_BORDERSNIFF;
        return;
    }
    _calculate_gadget_box(window, NULL, gad, &l, &t, &w, &h);
    if (w > 0 && h > 0 &&
        (l < window->BorderLeft || t < window->BorderTop ||
         l + w > window->Width - window->BorderRight ||
         t + h > window->Height - window->BorderBottom))
        gad->Activation |= GACT_BORDERSNIFF;
}

/*
 * Find a menu item with a matching CommKey (command key shortcut).
 *
 * Scans the entire menu strip for items/subitems that have the COMMSEQ
 * flag set and whose Command field matches the given ASCII character
 * (case-insensitive).  When found, returns the encoded FULLMENUNUM
 * code.  If no match is found, returns MENUNULL.
 *
 * Per AmigaOS convention keyboard shortcuts fire even when the menu or
 * item is disabled, so MENUENABLED/ITEMENABLED are NOT checked here.
 * CHECKIT toggle / mutual-exclude handling is applied when a match is
 * found (matching AROS behaviour).
 */
static UWORD _find_menu_commkey(struct Menu *strip, char key)
{
    struct Menu     *menu;
    struct MenuItem *item;
    struct MenuItem *sub;
    WORD             menuNum, itemNum, subNum;
    char             ukey;

    if (!strip || !key)
        return MENUNULL;

    /* Case-insensitive comparison */
    ukey = key;
    if (ukey >= 'a' && ukey <= 'z')
        ukey -= ('a' - 'A');

    for (menu = strip, menuNum = 0; menu; menu = menu->NextMenu, menuNum++)
    {
        for (item = menu->FirstItem, itemNum = 0; item;
             item = item->NextItem, itemNum++)
        {
            /* Check sub-items first (subitems take precedence if any) */
            if (item->SubItem)
            {
                for (sub = item->SubItem, subNum = 0; sub;
                     sub = sub->NextItem, subNum++)
                {
                    if (sub->Flags & COMMSEQ)
                    {
                        char cmd = sub->Command;
                        if (cmd >= 'a' && cmd <= 'z')
                            cmd -= ('a' - 'A');
                        if (cmd == ukey)
                        {
                            _menu_check_item(item->SubItem, sub, FALSE);
                            sub->NextSelect = MENUNULL;
                            return (UWORD)((menuNum & 0x1F) |
                                          ((itemNum & 0x3F) << 5) |
                                          ((subNum & 0x1F) << 11));
                        }
                    }
                }
            }

            /* Check the item itself */
            if (item->Flags & COMMSEQ)
            {
                char cmd = item->Command;
                if (cmd >= 'a' && cmd <= 'z')
                    cmd -= ('a' - 'A');
                if (cmd == ukey)
                {
                    _menu_check_item(menu->FirstItem, item, FALSE);
                    item->NextSelect = MENUNULL;
                    return (UWORD)((menuNum & 0x1F) |
                                  ((itemNum & 0x3F) << 5) |
                                  ((WORD)NOSUB << 11));
                }
            }
        }
    }

    return MENUNULL;
}

/*
 * Encode menu selection into FULLMENUNUM format
 * menuNum: 0-31, itemNum: 0-63, subNum: 0-31
 */
static UWORD _encode_menu_selection(WORD menuNum, WORD itemNum, WORD subNum)
{
    if (menuNum == NOMENU || itemNum == NOITEM)
        return MENUNULL;
    
    /* Encode using the same layout as FULLMENUNUM():
     * Bits 0-4:   menu number (SHIFTMENU)
     * Bits 5-10:  item number (SHIFTITEM)
     * Bits 11-15: sub-item number (SHIFTSUB)
     * When subNum is NOSUB (0x1F), bits 11-15 are all 1s.
     */
    UWORD code = (menuNum & 0x1F);              /* Bits 0-4: menu number */
    code |= ((itemNum & 0x3F) << 5);            /* Bits 5-10: item number */
    code |= ((subNum & 0x1F) << 11);            /* Bits 11-15: sub-item number */
    
    return code;
}

static BOOL _is_separator_menu_item(const struct MenuItem *item)
{
    struct IntuiText *it;

    if (!item || !item->ItemFill)
        return FALSE;

    /* GadTools NM_BARLABEL: an image item without image data */
    if (!(item->Flags & ITEMTEXT))
    {
        const struct Image *im = (const struct Image *)item->ItemFill;
        return im->Depth == 0 && im->ImageData == NULL && im->PlanePick == 0 &&
               (item->Flags & HIGHFLAGS) == HIGHNONE;
    }

    it = (struct IntuiText *)item->ItemFill;
    if (!it || !it->IText)
        return FALSE;

    return it->IText[0] == '-' && it->IText[1] == '-';
}



/*
 * Find which menu title is at a given screen X position
 * Returns the menu or NULL if no menu at that position
 */
static struct Menu * _find_menu_at_x(struct Window *window, WORD screenX)
{
    struct Menu *menu;
    struct Screen *screen;
    WORD barHBorder;
    
    if (!window || !window->MenuStrip || !window->WScreen)
        return NULL;
    
    screen = window->WScreen;
    barHBorder = screen->BarHBorder;
    
    for (menu = window->MenuStrip; menu; menu = menu->NextMenu)
    {
        /* Menu positions are stored relative to screen left edge */
        WORD menuLeft = barHBorder + menu->LeftEdge;
        WORD menuRight = menuLeft + menu->Width;
        
        if (screenX >= menuLeft && screenX < menuRight)
            return menu;
    }
    
    return NULL;
}

/*
 * Find which menu item is at a given position within the menu drop-down
 * x, y are relative to the menu drop-down origin
 */
static struct MenuItem * _find_item_at_pos(struct Menu *menu, WORD x, WORD y)
{
    /* x, y relative to the menu's item origin (_menu_item_origin) */
    if (!menu || !(menu->Flags & MENUENABLED))
        return NULL;
    return _find_item_in_chain_at_pos(menu->FirstItem, x, y);
}

static struct MenuItem * _find_item_in_chain_at_pos(struct MenuItem *firstItem, WORD x, WORD y)
{
    struct MenuItem *item;

    for (item = firstItem; item; item = item->NextItem)
    {
        if (x >= item->LeftEdge && x < item->LeftEdge + item->Width &&
            y >= item->TopEdge && y < item->TopEdge + item->Height)
            return (item->Flags & ITEMENABLED) ? item : NULL;
    }

    return NULL;
}

/*
 * AmigaOS 3.1 drop-down geometry (reference: tests/scenarios/gallery-menus.yaml).
 * The items of a menu are placed relative to the "item origin"
 *   x = Menu.LeftEdge + 1 + MenuHBorder,  y = BarHeight + MenuVBorder - 1;
 * the box around them has a 2 pixel frame left and right and a 1 pixel
 * frame at the top and the bottom, MenuHBorder/2 of padding left and right,
 * MenuVBorder - 1 at the top and 1 at the bottom.  The items of a sub-menu are
 * relative to the origin of their parent item.
 */
/* pens of the menu imagery: NewLook menus use the screen's bar pens, the
 * classic look the window's DetailPen (text, frame) and BlockPen (fill) */
static void _menu_pens(struct Window *window, UWORD *pens)
{
    const UWORD *sp = _intuition_screen_pens(window->WScreen);
    UWORD i;

    for (i = 0; i < NUMDRIPENS; i++)
        pens[i] = sp[i];
    if (!(window->Flags & WFLG_NEWLOOKMENUS))
    {
        pens[BARDETAILPEN] = window->DetailPen;
        pens[BARBLOCKPEN] = window->BlockPen;
        pens[BARTRIMPEN] = window->DetailPen;
    }
}

/* the classic look box has 8 more pixels of padding on the right */
static WORD _menu_extra_right(struct Window *window)
{
    return (window && !(window->Flags & WFLG_NEWLOOKMENUS)) ? 8 : 0;
}

static void _menu_item_origin(struct Screen *screen, struct Menu *menu, WORD *ox, WORD *oy)
{
    *ox = menu->LeftEdge + 1 + screen->MenuHBorder;
    *oy = screen->BarHeight + screen->MenuVBorder - 1;
}

/*
 * Get the index of a menu in the menu strip (0-based)
 */
static WORD _get_menu_index(struct Window *window, struct Menu *targetMenu)
{
    struct Menu *menu;
    WORD index = 0;
    
    if (!window || !window->MenuStrip)
        return NOMENU;
    
    for (menu = window->MenuStrip; menu; menu = menu->NextMenu, index++)
    {
        if (menu == targetMenu)
            return index;
    }
    
    return NOMENU;
}

/*
 * Get the index of an item in a menu (0-based)
 */
static WORD _get_item_index(struct Menu *menu, struct MenuItem *targetItem)
{
    struct MenuItem *item;
    WORD index = 0;
    
    if (!menu || !menu->FirstItem)
        return NOITEM;
    
    for (item = menu->FirstItem; item; item = item->NextItem, index++)
    {
        if (item == targetItem)
            return index;
    }
    
    return NOITEM;
}

static WORD _get_item_chain_index(struct MenuItem *firstItem, struct MenuItem *targetItem)
{
    struct MenuItem *item;
    WORD index;

    if (!firstItem || !targetItem)
        return NOITEM;

    index = 0;
    for (item = firstItem; item; item = item->NextItem, index++)
    {
        if (item == targetItem)
            return index;
    }

    return NOITEM;
}

static void _get_menu_item_chain_box(struct Screen *screen, struct MenuItem *firstItem,
                                     WORD originX, WORD originY, WORD extra,
                                     WORD *boxX, WORD *boxY,
                                     WORD *boxWidth, WORD *boxHeight)
{
    struct MenuItem *item;
    WORD minx = 0, miny = 0, maxx = 0, maxy = 0;
    WORD hb = screen ? screen->MenuHBorder : 4;
    WORD vb = screen ? screen->MenuVBorder : 2;

    for (item = firstItem; item; item = item->NextItem)
    {
        if (item == firstItem || item->LeftEdge < minx)
            minx = item->LeftEdge;
        if (item == firstItem || item->TopEdge < miny)
            miny = item->TopEdge;
        if (item->LeftEdge + item->Width > maxx)
            maxx = item->LeftEdge + item->Width;
        if (item->TopEdge + item->Height > maxy)
            maxy = item->TopEdge + item->Height;
    }
    if (minx > 0)
        minx = 0;
    if (miny > 0)
        miny = 0;

    if (boxX)
        *boxX = originX + minx - hb;
    if (boxY)
        *boxY = originY + miny - vb;
    if (boxWidth)
        *boxWidth = (firstItem ? (maxx - minx) : 0) + 2 * hb + extra;
    if (boxHeight)
        *boxHeight = (firstItem ? (maxy - miny) : 0) + vb + 2;
}

static BOOL _get_menu_submenu_origin(struct Window *window, struct Menu *menu,
                                     struct MenuItem *item, WORD *ox, WORD *oy)
{
    if (!window || !window->WScreen || !menu || !item || !item->SubItem)
        return FALSE;
    _menu_item_origin(window->WScreen, menu, ox, oy);
    *ox += item->LeftEdge;
    *oy += item->TopEdge;
    return TRUE;
}

static BOOL _get_menu_submenu_box(struct Window *window,
                                  struct Menu *menu,
                                  struct MenuItem *item,
                                  WORD *boxX, WORD *boxY,
                                  WORD *boxWidth, WORD *boxHeight)
{
    WORD ox, oy;

    if (boxX)
        *boxX = 0;
    if (boxY)
        *boxY = 0;
    if (boxWidth)
        *boxWidth = 0;
    if (boxHeight)
        *boxHeight = 0;

    if (!_get_menu_submenu_origin(window, menu, item, &ox, &oy))
        return FALSE;

    _get_menu_item_chain_box(window->WScreen, item->SubItem, ox, oy, _menu_extra_right(window),
                             boxX, boxY, boxWidth, boxHeight);
    return TRUE;
}

static BOOL _get_active_submenu_box(struct Window *window,
                                    WORD *boxX, WORD *boxY,
                                    WORD *boxWidth, WORD *boxHeight)
{
    return _get_menu_submenu_box(window,
                                 g_active_menu,
                                 g_active_item,
                                 boxX,
                                 boxY,
                                 boxWidth,
                                 boxHeight);
}

static BOOL _menu_hover_redraw_can_repaint_in_place(struct Window *window,
                                                    struct Menu *oldMenu,
                                                    struct MenuItem *oldItem)
{
    WORD oldX;
    WORD oldY;
    WORD oldWidth;
    WORD oldHeight;
    WORD newX;
    WORD newY;
    WORD newWidth;
    WORD newHeight;
    BOOL hadOldSubmenu;
    BOOL hasNewSubmenu;

    if (!window || !oldMenu || oldMenu != g_active_menu)
        return FALSE;

    hadOldSubmenu = _get_menu_submenu_box(window, oldMenu, oldItem,
                                          &oldX, &oldY, &oldWidth, &oldHeight);
    hasNewSubmenu = _get_active_submenu_box(window,
                                            &newX, &newY, &newWidth, &newHeight);

    if (hadOldSubmenu != hasNewSubmenu)
        return FALSE;

    if (!hadOldSubmenu)
        return TRUE;

    return oldX == newX && oldY == newY &&
           oldWidth == newWidth && oldHeight == newHeight;
}

/*
 * Save a rectangular area of the screen's BitMap into an off-screen
 * BitMap (g_menu_save_bm) using BltBitMap. Used to save the area under a
 * menu drop-down before drawing it, so it can be restored when the menu
 * closes.
 *
 * Pixel-accurate: BltBitMap handles arbitrary X coordinates correctly via
 * shifting, eliminating the byte-aligned 0-7 pixel fringe artifact present
 * in the previous planar memcpy implementation.
 */
static void _save_menu_dropdown_area(struct Screen *screen, WORD x, WORD y, WORD w, WORD h)
{
    struct BitMap *bm;
    WORD depth;

    if (!screen)
        return;

    bm = &screen->BitMap;
    if (!bm->Planes[0])
        return;

    /* Clamp to screen bounds */
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > screen->Width) w = screen->Width - x;
    if (y + h > screen->Height) h = screen->Height - y;
    if (w <= 0 || h <= 0)
        return;

    /* Free any previous save BitMap */
    if (g_menu_save_bm)
    {
        FreeBitMap(g_menu_save_bm);
        g_menu_save_bm = NULL;
    }

    depth = bm->Depth;

    g_menu_save_bm = AllocBitMap(w, h, depth, BMF_CLEAR, NULL);
    if (!g_menu_save_bm)
    {
        DPRINTF(LOG_ERROR, "_intuition: _save_menu_dropdown_area() AllocBitMap(save) failed\n");
        return;
    }

    g_menu_save_x = x;
    g_menu_save_y = y;
    g_menu_save_w = w;
    g_menu_save_h = h;

    /* Save: screen (x,y) -> save BitMap (0,0). Plain copy minterm 0xC0. */
    BltBitMap(bm, x, y, g_menu_save_bm, 0, 0, w, h, 0xC0, 0xFF, NULL);
}

/*
 * Restore the previously saved menu drop-down area to the screen's BitMap
 * via BltBitMap. Called when the menu closes or switches.
 *
 * Also frees the off-screen save BitMap.
 */
static void _restore_menu_dropdown_area(struct Screen *screen)
{
    struct BitMap *bm;

    if (!screen || !g_menu_save_bm)
        return;

    bm = &screen->BitMap;
    if (!bm->Planes[0])
        return;

    /* Restore: save BitMap (0,0) -> screen (g_menu_save_x, g_menu_save_y). */
    BltBitMap(g_menu_save_bm, 0, 0, bm, g_menu_save_x, g_menu_save_y,
              g_menu_save_w, g_menu_save_h, 0xC0, 0xFF, NULL);

    DPRINTF(LOG_DEBUG, "_intuition: _restore_menu_dropdown_area() restored %dx%d at (%d,%d)\n",
            (int)g_menu_save_w, (int)g_menu_save_h,
            (int)g_menu_save_x, (int)g_menu_save_y);

    FreeBitMap(g_menu_save_bm);
    g_menu_save_bm = NULL;
    g_menu_save_w = 0;
    g_menu_save_h = 0;
}

/*
 * Render the screen title bar (shown when not in menu mode)
 * This restores the title bar after menu mode ends
 */
static const UWORD *_intuition_screen_pens(struct Screen *screen);
static UWORD _screen_sysi_size(struct Screen *screen);

static void _render_screen_title_bar(struct Screen *screen)
{
    struct RastPort *rp;
    const UWORD *pens;
    UWORD dw = 23;
    UBYTE oldpen, olddm;

    if (!screen)
        return;

    rp = &screen->RastPort;

    /* Validate RastPort has a valid BitMap */
    if (!rp->BitMap || !rp->BitMap->Planes[0])
        return;

    /* AmigaOS 3.1 reference: BARBLOCKPEN bar, BARTRIMPEN line below it,
     * the title in BARDETAILPEN, the screen depth gadget at the right */
    pens = _intuition_screen_pens(screen);
    oldpen = rp->FgPen;
    olddm = rp->DrawMode;
    SetAPen(rp, pens[BARBLOCKPEN]);
    RectFill(rp, 0, 0, screen->Width - 1, screen->BarHeight - 1);
    SetAPen(rp, pens[BARTRIMPEN]);
    RectFill(rp, 0, screen->BarHeight, screen->Width - 1, screen->BarHeight);

    lxa_sysi_dims(SDEPTHIMAGE, _screen_sysi_size(screen), &dw, NULL);
    if (screen->Title)
    {
        WORD len = strlen((const char *)screen->Title);
        WORD avail = screen->Width - dw - screen->BarHBorder;
        WORD fit = (avail > 0) ? lxa_text_fit(rp, (STRPTR)screen->Title, len, avail) : 0;

        SetAPen(rp, pens[BARDETAILPEN]);
        SetDrMd(rp, JAM1);
        Move(rp, screen->BarHBorder, screen->BarVBorder + rp->TxBaseline);
        if (fit > 0)
            Text(rp, (STRPTR)screen->Title, fit);
    }
    lxa_sysi_draw(rp, SDEPTHIMAGE, _screen_sysi_size(screen), screen->Width - dw, 0,
                  IDS_NORMAL, pens);
    SetAPen(rp, oldpen);
    SetDrMd(rp, olddm);

    DPRINTF(LOG_DEBUG, "_intuition: _render_screen_title_bar() screen=0x%08lx title='%s'\n",
            (ULONG)screen, screen->Title ? (const char *)screen->Title : "(none)");
}



/*
 * Render the menu bar background on the screen's title bar area
 */
static void _render_menu_bar(struct Window *window)
{
    struct Screen *screen;
    struct RastPort *rp;
    const UWORD *pens;
    UWORD mpens[NUMDRIPENS + 1];
    struct Menu *menu;
    UBYTE oldpen, olddm;

    if (!window || !window->WScreen)
        return;

    screen = window->WScreen;
    rp = &screen->RastPort;
    if (!rp->BitMap || !rp->BitMap->Planes[0])
        return;

    /* AmigaOS 3.1: the screen bar shows the menu titles (BARDETAILPEN on
     * BARBLOCKPEN, no depth gadget); the selected title is inverted over
     * Menu.LeftEdge+1 .. LeftEdge+Width; disabled titles are ghosted */
    _menu_pens(window, mpens);
    pens = mpens;
    oldpen = rp->FgPen;
    olddm = rp->DrawMode;
    SetAPen(rp, pens[BARBLOCKPEN]);
    RectFill(rp, 0, 0, screen->Width - 1, screen->BarHeight - 1);
    if (window->Flags & WFLG_NEWLOOKMENUS)
    {
        SetAPen(rp, pens[BARTRIMPEN]);
        RectFill(rp, 0, screen->BarHeight, screen->Width - 1, screen->BarHeight);
    }
    SetDrMd(rp, JAM1);

    for (menu = window->MenuStrip; menu; menu = menu->NextMenu)
    {
        WORD tx = screen->BarHBorder + menu->LeftEdge;
        BOOL active = (menu == g_active_menu);

        if (menu->MenuName)
        {
            SetAPen(rp, pens[BARDETAILPEN]);
            Move(rp, tx, screen->BarVBorder + rp->TxBaseline);
            Text(rp, (STRPTR)menu->MenuName, strlen((const char *)menu->MenuName));
        }
        if (!(menu->Flags & MENUENABLED))
            lxa_ghost_rect_pen(rp, tx, screen->BarVBorder, tx + menu->Width - 1,
                               screen->BarVBorder + rp->TxHeight - 1, pens[BARBLOCKPEN],
                               screen->BarVBorder);   /* pattern phase: 3.1 reference */
        if (active && (menu->Flags & MENUENABLED))
        {
            /* the selected title is complemented */
            SetDrMd(rp, COMPLEMENT);
            RectFill(rp, menu->LeftEdge + 1, 0, menu->LeftEdge + menu->Width, screen->BarHeight - 2);
            SetDrMd(rp, JAM1);
        }
    }
    SetAPen(rp, oldpen);
    SetDrMd(rp, olddm);
}

/*
 * Save the screen area that would be covered by the drop-down for the given menu.
 * Must be called BEFORE _render_menu_items() when opening a new drop-down.
 */
static void _save_dropdown_for_menu(struct Window *window, struct Menu *menu)
{
    struct Screen *screen;
    WORD ox, oy, bx, by, bw, bh;
    WORD sx, sy, sw, sh;
    WORD saveRight, saveBottom;

    if (!window || !window->WScreen || !menu || !menu->FirstItem)
        return;

    screen = window->WScreen;
    _menu_item_origin(screen, menu, &ox, &oy);
    _get_menu_item_chain_box(screen, menu->FirstItem, ox, oy, _menu_extra_right(window),
                             &bx, &by, &bw, &bh);
    saveRight = bx + bw;
    saveBottom = by + bh;

    if (_get_active_submenu_box(window, &sx, &sy, &sw, &sh))
    {
        if (sx < bx)
            bx = sx;
        if (sy < by)
            by = sy;
        if (sx + sw > saveRight)
            saveRight = sx + sw;
        if (sy + sh > saveBottom)
            saveBottom = sy + sh;
    }

    _save_menu_dropdown_area(screen, bx, by, saveRight - bx, saveBottom - by);
}

static void _render_menu_item_chain(struct Screen *screen, struct RastPort *rp,
                                    struct MenuItem *firstItem,
                                    struct MenuItem *highlightedItem,
                                    WORD ox, WORD oy, struct Window *window, BOOL all_ghosted)
{
    struct MenuItem *item;
    UWORD pens[NUMDRIPENS + 1];
    WORD bx, by, bw, bh, extra = _menu_extra_right(window);
    UBYTE oldpen, olddm;

    if (!rp || !firstItem || !window)
        return;

    _menu_pens(window, pens);
    oldpen = rp->FgPen;
    olddm = rp->DrawMode;
    _get_menu_item_chain_box(screen, firstItem, ox, oy, extra, &bx, &by, &bw, &bh);

    /* box: BARBLOCKPEN inside, BARDETAILPEN frame (2 pixels left/right) */
    SetAPen(rp, pens[BARBLOCKPEN]);
    RectFill(rp, bx, by, bx + bw - 1, by + bh - 1);
    SetAPen(rp, pens[BARDETAILPEN]);
    RectFill(rp, bx, by, bx + bw - 1, by);
    RectFill(rp, bx, by + bh - 1, bx + bw - 1, by + bh - 1);
    RectFill(rp, bx, by, bx + 1, by + bh - 1);
    RectFill(rp, bx + bw - 2, by, bx + bw - 1, by + bh - 1);

    for (item = firstItem; item; item = item->NextItem)
    {
        WORD ix = ox + item->LeftEdge;
        WORD iy = oy + item->TopEdge;
        BOOL highlighted = (item == highlightedItem && (item->Flags & ITEMENABLED));

        if (_is_separator_menu_item(item))
        {
            /* GadTools bar: two dotted BARDETAILPEN rows */
            WORD r = ix + item->Width - 3 + extra;
            SetAPen(rp, pens[BARDETAILPEN]);
            RectFill(rp, ix + 2, iy + 2, r, iy + 3);
            lxa_ghost_rect_pen(rp, ix + 2, iy + 2, r, iy + 3, pens[BARBLOCKPEN], screen->BarHeight);
            continue;
        }

        if (highlighted && (item->Flags & HIGHFLAGS) == HIGHIMAGE && item->SelectFill)
        {
            if (item->Flags & ITEMTEXT)
                _intuition_PrintIText(IntuitionBase, rp, (struct IntuiText *)item->SelectFill, ix, iy);
            else
                _intuition_DrawImage(IntuitionBase, rp, (struct Image *)item->SelectFill, ix, iy);
        }
        else if (item->ItemFill)
        {
            if (item->Flags & ITEMTEXT)
                _intuition_PrintIText(IntuitionBase, rp, (struct IntuiText *)item->ItemFill, ix, iy);
            else
                _intuition_DrawImage(IntuitionBase, rp, (struct Image *)item->ItemFill, ix, iy);
        }

        if ((item->Flags & CHECKIT) && (item->Flags & CHECKED))
        {
            struct Image *cm = window ? window->CheckMark : NULL;
            if (cm)
                _intuition_DrawImage(IntuitionBase, rp, cm, ix, iy + 1);
            else
                lxa_sysi_draw(rp, MENUCHECK, SYSISIZE_MEDRES, ix, iy + 1, IDS_NORMAL, pens);
        }

        if (item->Flags & COMMSEQ)
        {
            UBYTE c = (UBYTE)item->Command;
            WORD cw = TextLength(rp, (STRPTR)&c, 1);
            UWORD aw = 23;
            WORD cx = ix + item->Width - cw;

            lxa_sysi_dims(AMIGAKEY, SYSISIZE_MEDRES, &aw, NULL);
            lxa_sysi_draw(rp, AMIGAKEY, SYSISIZE_MEDRES, cx - aw - 4, iy + 1, IDS_NORMAL, pens);
            SetAPen(rp, pens[BARDETAILPEN]);
            SetDrMd(rp, JAM1);
            /* 3.1 reference: the key ends 2 px inside the item, 2 px
             * right of the Amiga key image */
            Move(rp, cx - 2, iy + 1 + rp->TxBaseline);
            Text(rp, (STRPTR)&c, 1);
        }

        if (!(item->Flags & ITEMENABLED) || all_ghosted)
            lxa_ghost_rect_pen(rp, ix, iy, ix + item->Width - 1, iy + item->Height - 1, pens[BARBLOCKPEN],
                               screen->BarHeight);
        else if (highlighted)
        {
            switch (item->Flags & HIGHFLAGS)
            {
                case HIGHCOMP:
                    SetDrMd(rp, COMPLEMENT);
                    RectFill(rp, ix, iy, ix + item->Width - 1, iy + item->Height - 1);
                    SetDrMd(rp, JAM1);
                    break;
                case HIGHBOX:
                    SetDrMd(rp, COMPLEMENT);
                    RectFill(rp, ix - 2, iy - 1, ix + item->Width + 1, iy - 1);
                    RectFill(rp, ix - 2, iy + item->Height, ix + item->Width + 1, iy + item->Height);
                    RectFill(rp, ix - 2, iy, ix - 1, iy + item->Height - 1);
                    RectFill(rp, ix + item->Width, iy, ix + item->Width + 1, iy + item->Height - 1);
                    SetDrMd(rp, JAM1);
                    break;
                default:
                    break;
            }
        }
    }
    SetAPen(rp, oldpen);
    SetDrMd(rp, olddm);
}

/*
 * Render the drop-down menu for the active menu.
 *
 * Phase 133 (v0.9.7): off-screen composition. Each item chain (main +
 * optional submenu) is rendered into a shared off-screen compose BitMap
 * (allocated lazily by _menu_ensure_compose_bitmap()), then BltBitMap'd
 * onto the screen in a single operation. This eliminates the visible
 * "blank then redraw" flash that direct-to-screen rendering produced
 * on every hover transition (the symptom previously documented for
 * ASM-One and MaxonBASIC).
 *
 * Earlier history: Phase 112 (v0.8.77) attempted compose but reverted
 * due to per-event cycle cost concerns; Phase 133 keeps the BitMap
 * allocated for the lifetime of the menu session so the only per-event
 * cost is the BltBitMap itself, which is acceptable.
 *
 * Falls back to direct screen rendering if the compose BitMap cannot
 * be allocated.
 */
static void _render_menu_items(struct Window *window)
{
    struct Screen *screen;
    struct Menu *menu;
    WORD ox, oy, sx, sy;

    if (!window || !window->WScreen || !g_active_menu)
        return;

    screen = window->WScreen;
    if (!screen->RastPort.BitMap || !screen->RastPort.BitMap->Planes[0])
        return;

    menu = g_active_menu;
    _menu_item_origin(screen, menu, &ox, &oy);
    _render_menu_item_chain(screen, &screen->RastPort, menu->FirstItem, g_active_item, ox, oy, window,
                            !(menu->Flags & MENUENABLED));

    if (g_active_item && g_active_item->SubItem &&
        _get_menu_submenu_origin(window, menu, g_active_item, &sx, &sy))
        _render_menu_item_chain(screen, &screen->RastPort, g_active_item->SubItem,
                                g_active_subitem, sx, sy, window, FALSE);
}

/*
 * Render the drop-down menu for the active menu.
 *
 * Phase 133 (v0.9.7): off-screen composition restored. Each item chain
 * (main + optional submenu) is rendered into a shared off-screen
 * compose BitMap (allocated lazily by _menu_ensure_compose_bitmap()),
 * then BltBitMap'd onto the screen in a single operation. This
 * eliminates the visible "blank then redraw" flash that direct-to-
 * screen rendering produced on every hover transition.
 *
 * Falls back to direct screen rendering if the compose BitMap cannot
 * be allocated.
 *
 * The hover-redraw fast path (_render_menu_items_in_place) and the
 * full-repaint path both come through here; with composition active
 * the cost difference is small enough that no separate fast path is
 * needed.
 */
static void _render_menu_items_in_place(struct Window *window)
{
    _render_menu_items(window);
}

/*
 * Enter menu mode - called on MENUDOWN
 */
/*
 * Menu drawn-state flags, as AmigaOS 3.1 keeps them (tests/scenarios/
 * gallery-menus-select.yaml): the menu whose items are shown has MIDRAWN,
 * the highlighted (enabled) item or sub-item has HIGHITEM, an item whose
 * sub-items are shown has ISDRAWN.  When the menu button is released the
 * flags of the final state stay set; the next menu session clears them,
 * together with MENUTOGGLED, on the whole strip.
 */
static void _menu_clear_session_flags(struct Menu *strip)
{
    struct Menu *menu;
    struct MenuItem *item, *sub;

    for (menu = strip; menu; menu = menu->NextMenu)
    {
        menu->Flags &= ~MIDRAWN;
        for (item = menu->FirstItem; item; item = item->NextItem)
        {
            item->Flags &= ~(HIGHITEM | ISDRAWN | MENUTOGGLED);
            for (sub = item->SubItem; sub; sub = sub->NextItem)
                sub->Flags &= ~(HIGHITEM | ISDRAWN | MENUTOGGLED);
        }
    }
    g_flagged_menu = NULL;
    g_flagged_item = NULL;
    g_flagged_subitem = NULL;
}

static void _menu_sync_drawn_flags(void)
{
    if (g_flagged_menu)
        g_flagged_menu->Flags &= ~MIDRAWN;
    if (g_flagged_item)
        g_flagged_item->Flags &= ~(HIGHITEM | ISDRAWN);
    if (g_flagged_subitem)
        g_flagged_subitem->Flags &= ~HIGHITEM;

    g_flagged_menu = g_active_menu;
    g_flagged_item = g_active_menu ? g_active_item : NULL;
    g_flagged_subitem = g_flagged_item && g_flagged_item->SubItem ? g_active_subitem : NULL;

    if (g_flagged_menu)
        g_flagged_menu->Flags |= MIDRAWN;
    if (g_flagged_item)
    {
        if (g_flagged_item->Flags & ITEMENABLED)
            g_flagged_item->Flags |= HIGHITEM;
        if (g_flagged_item->SubItem)
            g_flagged_item->Flags |= ISDRAWN;
    }
    if (g_flagged_subitem && (g_flagged_subitem->Flags & ITEMENABLED))
        g_flagged_subitem->Flags |= HIGHITEM;
}

/*
 * A selected CHECKIT item: MENUTOGGLE items toggle, others become checked;
 * MutualExclude unchecks the checked CHECKIT items of the same chain
 * (bit n = n-th item of the menu, or sub-item of the parent item).
 * mark_toggled: the mouse path flags the item MENUTOGGLED (AmigaOS 3.1).
 */
static void _menu_check_item(struct MenuItem *chain, struct MenuItem *item, BOOL mark_toggled)
{
    struct MenuItem *other;
    WORD i;

    if (!(item->Flags & CHECKIT))
        return;

    if (item->Flags & MENUTOGGLE)
        item->Flags ^= CHECKED;
    else
        item->Flags |= CHECKED;
    if (mark_toggled)
        item->Flags |= MENUTOGGLED;

    if (item->MutualExclude)
    {
        for (other = chain, i = 0; other && i < 32; other = other->NextItem, i++)
        {
            if (other != item && (item->MutualExclude & (1UL << i)) &&
                (other->Flags & (CHECKIT | CHECKED)) == (CHECKIT | CHECKED))
                other->Flags &= ~CHECKED;
        }
    }
}

static void _enter_menu_mode(struct Window *window, struct Screen *screen, WORD mouseX, WORD mouseY)
{
    if (!window || !window->MenuStrip || !screen)
        return;
    
    /*
     * Note: WFLG_RMBTRAP filtering is done by the callers — the
     * MENUDOWN handler already decides based on whether the mouse
     * is in the screen title bar (system territory, RMBTRAP ignored)
     * or over a window (RMBTRAP respected).  We do not re-check it
     * here so that title-bar menu activation works for programs like
     * BlitzBasic2 that use RMBTRAP + SetMenuStrip together.
     */
    
    g_menu_mode = TRUE;
    g_menu_window = window;
    g_active_menu = NULL;
    g_active_item = NULL;
    g_active_subitem = NULL;
    g_menu_selection = MENUNULL;
    _menu_clear_session_flags(window->MenuStrip);
    
    /* Render the menu bar */
    _render_menu_bar(window);
    
    /* Check if mouse is already over a menu */
    if (mouseY < screen->BarHeight + 1)
    {
        struct Menu *menu = _find_menu_at_x(window, mouseX);
        /* AmigaOS 3.1 opens disabled menus too (all items ghosted) */
        if (menu)
        {
            g_active_menu = menu;
            _render_menu_bar(window);
            _save_dropdown_for_menu(window, menu);
            _render_menu_items(window);
        }
    }
    _menu_sync_drawn_flags();
    DPRINTF(LOG_DEBUG, "_intuition: _enter_menu_mode completed, g_menu_mode=%d g_active_menu=0x%08lx\n",
            g_menu_mode, (ULONG)g_active_menu);
}

/*
 * Exit menu mode - called on MENUUP
 */
static void _exit_menu_mode(struct Window *window, WORD mouseX, WORD mouseY)
{
    UWORD menuCode = MENUNULL;
    struct Screen *screen = NULL;
    
    if (!g_menu_mode)
        return;
    
    /* Save screen pointer before we clear g_menu_window */
    if (g_menu_window)
        screen = g_menu_window->WScreen;
    
    DPRINTF(LOG_DEBUG, "_intuition: Exiting menu mode, active_menu=0x%08lx active_item=0x%08lx mouseXY=(%d,%d)\n",
            (ULONG)g_active_menu, (ULONG)g_active_item, (int)mouseX, (int)mouseY);
    
    /* Calculate menu selection code if we have a valid selection */
    if (g_active_menu && g_active_item)
    {
        WORD menuNum = _get_menu_index(g_menu_window, g_active_menu);
        WORD itemNum = _get_item_index(g_active_menu, g_active_item);
        WORD subNum = NOSUB;
        struct MenuItem *selected_item = g_active_item;

        if (g_active_subitem && (g_active_subitem->Flags & ITEMENABLED))
        {
            subNum = _get_item_chain_index(g_active_item->SubItem, g_active_subitem);
            selected_item = g_active_subitem;
        }

        if (selected_item->Flags & ITEMENABLED)
        {
            _menu_check_item(subNum != NOSUB ? g_active_item->SubItem : g_active_menu->FirstItem,
                             selected_item, TRUE);
            menuCode = _encode_menu_selection(menuNum, itemNum, subNum);
            selected_item->NextSelect = MENUNULL;

            DPRINTF(LOG_DEBUG, "_intuition: Menu selection: menu=%d item=%d sub=%d code=0x%04x enabled=0x%04x\n",
                    (int)menuNum, (int)itemNum, (int)subNum, menuCode, selected_item->Flags);
        }
        else
        {
            DPRINTF(LOG_DEBUG, "_intuition: Menu item NOT enabled: flags=0x%04x\n", selected_item->Flags);
        }
    }
    
    /* Post IDCMP_MENUPICK if we have a valid selection */
    if (g_menu_window && menuCode != MENUNULL)
    {
        WORD relX = mouseX - g_menu_window->LeftEdge;
        WORD relY = mouseY - g_menu_window->TopEdge;
        DPRINTF(LOG_DEBUG, "_intuition: posting MENUPICK code=0x%04x to window 0x%08lx IDCMPFlags=0x%08lx UserPort=0x%08lx SigBit=%d\n",
                menuCode, (ULONG)g_menu_window, g_menu_window->IDCMPFlags,
                (ULONG)g_menu_window->UserPort,
                g_menu_window->UserPort ? (int)g_menu_window->UserPort->mp_SigBit : -1);
        _post_idcmp_message(g_menu_window, IDCMP_MENUPICK, menuCode,
                           IEQUALIFIER_RBUTTON, NULL, relX, relY);
    }
    
    /* Clear menu mode state (the drawn-state flags of the final state
     * stay set, as on AmigaOS 3.1) */
    g_menu_mode = FALSE;
    g_menu_window = NULL;
    g_active_menu = NULL;
    g_active_item = NULL;
    g_active_subitem = NULL;
    g_flagged_menu = NULL;
    g_flagged_item = NULL;
    g_flagged_subitem = NULL;
    g_menu_selection = MENUNULL;
    
    /* Restore the menu drop-down area and redraw screen title bar */
    if (screen)
    {
        _restore_menu_dropdown_area(screen);
        _render_screen_title_bar(screen);
    }
    else
    {
        LPRINTF(LOG_WARNING, "_intuition: NO screen to restore menu area!\n");
    }

    /* Phase 133: free the off-screen compose BitMap; it will be lazily
     * re-allocated next time menu mode is entered. */
    if (g_menu_compose_bm)
    {
        FreeBitMap(g_menu_compose_bm);
        g_menu_compose_bm = NULL;
        g_menu_compose_w = 0;
        g_menu_compose_h = 0;
        g_menu_compose_depth = 0;
    }
}

/*
 * Handle keyboard input for an active string gadget
 * Returns TRUE if the gadget handled the input and remains active
 * Returns FALSE if the gadget deactivated (Return/Escape pressed or focus lost)
 * 
 * NOTE: This function may be called from interrupt context (via VBlank hook)
 * so it uses the global IntuitionBase instead of calling OpenLibrary().
 */
/* A GACT_LONGINT gadget's StringInfo->LongInt follows its text when the
 * gadget is left (Return, Tab, a click elsewhere): asl.library reads the
 * value from LongInt (AmigaOS 3.1, scenario gallery-asl-screenmode). */
static VOID _string_update_longint(struct Gadget *gad)
{
    struct StringInfo *si;
    LONG v = 0;
    WORD i = 0;
    BOOL neg = FALSE;

    if (!gad || !(gad->Activation & GACT_LONGINT) ||
        (gad->GadgetType & GTYP_GTYPEMASK) != GTYP_STRGADGET)
        return;
    si = (struct StringInfo *)gad->SpecialInfo;
    if (!si || !si->Buffer)
        return;
    while (si->Buffer[i] == ' ')
        i++;
    if (si->Buffer[i] == '-')
    {
        neg = TRUE;
        i++;
    }
    else if (si->Buffer[i] == '+')
        i++;
    while (si->Buffer[i] >= '0' && si->Buffer[i] <= '9')
        v = v * 10 + (si->Buffer[i++] - '0');
    si->LongInt = neg ? -v : v;
}

/* the active string gadget loses the input focus (a click elsewhere) */
static VOID _string_gadget_leave(struct Window *window, struct Gadget *gad)
{
    if (!gad || (gad->GadgetType & GTYP_GTYPEMASK) != GTYP_STRGADGET)
        return;
    gad->Flags &= ~GFLG_SELECTED;
    _string_update_longint(gad);
    if (window && IntuitionBase)
        _intuition_RefreshGList(IntuitionBase, gad, window, NULL, 1);
}

static BOOL _handle_string_gadget_key(struct Gadget *gad, struct Window *window,
                                       UWORD rawkey, UWORD qualifier)
{
    struct StringInfo *si = (struct StringInfo *)gad->SpecialInfo;
    BOOL keyUp = (rawkey & IECODE_UP_PREFIX) != 0;
    UWORD code = rawkey & ~IECODE_UP_PREFIX;
    BOOL needsRefresh = FALSE;
    
    if (!si || !si->Buffer)
        return TRUE;  /* No StringInfo, stay active anyway */
    
    if (keyUp)
        return TRUE;  /* Ignore key release events */
    
    DPRINTF(LOG_DEBUG, "_intuition: _handle_string_gadget_key code=0x%02x qual=0x%04x buf='%s' pos=%d numch=%d\n",
            code, qualifier, si->Buffer ? (char*)si->Buffer : "(null)", (int)si->BufferPos, (int)si->NumChars);
    
    /* Right-Amiga editing commands (RKRM Libraries, string gadget editing
     * keys): Amiga-X clears the buffer, Amiga-Q restores the undo buffer;
     * other Right-Amiga keys insert nothing.  Directory Opus clears its path
     * gadget with Amiga-X (scenario dopus-select, AmigaOS 3.1). */
    if (qualifier & IEQUALIFIER_RCOMMAND)
    {
        if (code == 0x32) /* X */
        {
            si->Buffer[0] = '\0';
            si->NumChars = 0;
            si->BufferPos = 0;
            si->DispPos = 0;
            needsRefresh = TRUE;
        }
        else if (code == 0x10 && si->UndoBuffer) /* Q */
        {
            WORD len = 0;
            while (len < si->MaxChars - 1 && si->UndoBuffer[len])
            {
                si->Buffer[len] = si->UndoBuffer[len];
                len++;
            }
            si->Buffer[len] = '\0';
            si->NumChars = len;
            si->BufferPos = len;
            needsRefresh = TRUE;
        }
        if (needsRefresh && IntuitionBase)
            _intuition_RefreshGList(IntuitionBase, gad, window, NULL, 1);
        return TRUE;
    }

    /* Check for special keys */
    switch (code) {
        case 0x44: /* Return key */
            gad->Flags &= ~GFLG_SELECTED;
            _string_update_longint(gad);
            /* Post IDCMP_GADGETUP */
            if (gad->Activation & GACT_RELVERIFY) {
                _post_idcmp_message(window, IDCMP_GADGETUP, 0, qualifier, gad,
                                    window->MouseX, window->MouseY);
            }
            return FALSE;  /* Deactivate gadget */
            
        case 0x45: /* Escape key */
            gad->Flags &= ~GFLG_SELECTED;
            return FALSE;  /* Deactivate gadget */
            
        case 0x41: /* Backspace */
            if (si->BufferPos > 0 && si->NumChars > 0) {
                /* Delete character before cursor */
                WORD i;
                for (i = si->BufferPos - 1; i < si->NumChars - 1; i++) {
                    si->Buffer[i] = si->Buffer[i + 1];
                }
                si->Buffer[si->NumChars - 1] = '\0';
                si->BufferPos--;
                si->NumChars--;
                needsRefresh = TRUE;
            }
            break;
            
        case 0x46: /* Delete */
            if (si->BufferPos < si->NumChars) {
                /* Delete character at cursor */
                WORD i;
                for (i = si->BufferPos; i < si->NumChars - 1; i++) {
                    si->Buffer[i] = si->Buffer[i + 1];
                }
                si->Buffer[si->NumChars - 1] = '\0';
                si->NumChars--;
                needsRefresh = TRUE;
            }
            break;
            
        case 0x4F: /* Cursor Left */
            if (si->BufferPos > 0) {
                si->BufferPos--;
                needsRefresh = TRUE;
            }
            break;
            
        case 0x4E: /* Cursor Right */
            if (si->BufferPos < si->NumChars) {
                si->BufferPos++;
                needsRefresh = TRUE;
            }
            break;
            
        case 0x42: /* TAB - cycle to next/previous string gadget */
        {
            /* Per RKRM, TAB cycles forward and Shift-TAB cycles backward
             * through the string gadgets in the window's gadget list. */
            struct Gadget *nextStr;
            struct Gadget *firstStr;
            struct Gadget *lastStr;
            struct Gadget *g;
            BOOL forward;

            forward = !(qualifier & (IEQUALIFIER_LSHIFT | IEQUALIFIER_RSHIFT));
            nextStr = NULL;
            firstStr = NULL;
            lastStr = NULL;

            /* Scan all string gadgets in the window list */
            for (g = window->FirstGadget; g; g = g->NextGadget)
            {
                if ((g->GadgetType & GTYP_GTYPEMASK) == GTYP_STRGADGET && g != gad)
                {
                    if (!firstStr)
                        firstStr = g;
                    lastStr = g;
                }
            }

            if (forward)
            {
                /* Find the next string gadget after the current one */
                BOOL found_current;

                found_current = FALSE;
                for (g = window->FirstGadget; g; g = g->NextGadget)
                {
                    if (g == gad)
                    {
                        found_current = TRUE;
                        continue;
                    }
                    if (found_current &&
                        (g->GadgetType & GTYP_GTYPEMASK) == GTYP_STRGADGET)
                    {
                        nextStr = g;
                        break;
                    }
                }
                /* Wrap around to first string gadget */
                if (!nextStr)
                    nextStr = firstStr;
            }
            else
            {
                /* Shift-TAB: find the previous string gadget */
                struct Gadget *prev;

                prev = NULL;
                for (g = window->FirstGadget; g; g = g->NextGadget)
                {
                    if (g == gad)
                        break;
                    if ((g->GadgetType & GTYP_GTYPEMASK) == GTYP_STRGADGET)
                        prev = g;
                }
                nextStr = prev;
                /* Wrap around to last string gadget */
                if (!nextStr)
                    nextStr = lastStr;
            }

            if (nextStr)
            {
                /* Deactivate current gadget */
                gad->Flags &= ~GFLG_SELECTED;
                _string_update_longint(gad);
                needsRefresh = TRUE;

                /* Activate the new string gadget.
                 * Set globals directly (ActivateGadget is defined later in file). */
                nextStr->Flags |= GFLG_SELECTED;
                g_active_gadget = nextStr;
                g_active_window = window;

                /* Recompute NumChars and position cursor at end */
                {
                    struct StringInfo *nsi = (struct StringInfo *)nextStr->SpecialInfo;
                    if (nsi && nsi->Buffer)
                    {
                        WORD len = 0;
                        while (nsi->Buffer[len] != '\0' && len < nsi->MaxChars)
                            len++;
                        nsi->NumChars = len;
                        nsi->BufferPos = len;
                    }
                }

                /* Refresh both gadgets to update cursor display */
                if (IntuitionBase)
                {
                    _intuition_RefreshGList(IntuitionBase, gad, window, NULL, 1);
                    _intuition_RefreshGList(IntuitionBase, nextStr, window, NULL, 1);
                }

                DPRINTF(LOG_DEBUG, "_intuition: TAB cycling to gadget 0x%08lx\n", (ULONG)nextStr);
                return TRUE;  /* New gadget is now active */
            }
            break;
        }
            
        default:
            /* Try to convert raw key to ASCII character */
            {
                UBYTE ch = 0;
                
                /* Simple ASCII mapping for common keys
                 * Based on the authoritative rawkey table in lxa_dev_console.c */
                if (code == 0x00) {
                    /* Grave accent / tilde */
                    ch = (qualifier & (IEQUALIFIER_LSHIFT | IEQUALIFIER_RSHIFT)) ? '~' : '`';
                } else if (code >= 0x01 && code <= 0x09) {
                    /* Numbers 1-9 on main keyboard */
                    static const UBYTE numRow[] = "123456789";
                    static const UBYTE numRowShift[] = "!@#$%^&*(";
                    if (qualifier & (IEQUALIFIER_LSHIFT | IEQUALIFIER_RSHIFT))
                        ch = numRowShift[code - 0x01];
                    else
                        ch = numRow[code - 0x01];
                } else if (code == 0x0A) {
                    /* 0 / ) */
                    ch = (qualifier & (IEQUALIFIER_LSHIFT | IEQUALIFIER_RSHIFT)) ? ')' : '0';
                } else if (code == 0x0B) {
                    /* Minus / underscore */
                    ch = (qualifier & (IEQUALIFIER_LSHIFT | IEQUALIFIER_RSHIFT)) ? '_' : '-';
                } else if (code == 0x0C) {
                    /* Equals / plus */
                    ch = (qualifier & (IEQUALIFIER_LSHIFT | IEQUALIFIER_RSHIFT)) ? '+' : '=';
                } else if (code == 0x0D) {
                    /* Backslash / pipe */
                    ch = (qualifier & (IEQUALIFIER_LSHIFT | IEQUALIFIER_RSHIFT)) ? '|' : '\\';
                } else if (code >= 0x10 && code <= 0x19) {
                    /* QWERTYUIOP */
                    static const UBYTE qRow[] = "qwertyuiop";
                    ch = qRow[code - 0x10];
                    if (qualifier & (IEQUALIFIER_LSHIFT | IEQUALIFIER_RSHIFT | IEQUALIFIER_CAPSLOCK))
                        ch = ch - 'a' + 'A';
                } else if (code >= 0x20 && code <= 0x28) {
                    /* ASDFGHJKL */
                    static const UBYTE aRow[] = "asdfghjkl";
                    ch = aRow[code - 0x20];
                    if (qualifier & (IEQUALIFIER_LSHIFT | IEQUALIFIER_RSHIFT | IEQUALIFIER_CAPSLOCK))
                        ch = ch - 'a' + 'A';
                } else if (code >= 0x31 && code <= 0x37) {
                    /* ZXCVBNM (letters only, not punctuation) */
                    static const UBYTE zRow[] = "zxcvbnm";
                    ch = zRow[code - 0x31];
                    if (qualifier & (IEQUALIFIER_LSHIFT | IEQUALIFIER_RSHIFT | IEQUALIFIER_CAPSLOCK))
                        ch = ch - 'a' + 'A';
                } else if (code == 0x40) {
                    /* Space */
                    ch = ' ';
                } else if (code == 0x1A) {
                    /* Left bracket */
                    ch = (qualifier & (IEQUALIFIER_LSHIFT | IEQUALIFIER_RSHIFT)) ? '{' : '[';
                } else if (code == 0x1B) {
                    /* Right bracket */
                    ch = (qualifier & (IEQUALIFIER_LSHIFT | IEQUALIFIER_RSHIFT)) ? '}' : ']';
                } else if (code == 0x29) {
                    /* Semicolon/colon */
                    ch = (qualifier & (IEQUALIFIER_LSHIFT | IEQUALIFIER_RSHIFT)) ? ':' : ';';
                } else if (code == 0x2A) {
                    /* Quote */
                    ch = (qualifier & (IEQUALIFIER_LSHIFT | IEQUALIFIER_RSHIFT)) ? '"' : '\'';
                } else if (code == 0x38) {
                    /* Comma/less-than */
                    ch = (qualifier & (IEQUALIFIER_LSHIFT | IEQUALIFIER_RSHIFT)) ? '<' : ',';
                } else if (code == 0x39) {
                    /* Period/greater-than */
                    ch = (qualifier & (IEQUALIFIER_LSHIFT | IEQUALIFIER_RSHIFT)) ? '>' : '.';
                } else if (code == 0x3A) {
                    /* Slash/question */
                    ch = (qualifier & (IEQUALIFIER_LSHIFT | IEQUALIFIER_RSHIFT)) ? '?' : '/';
                }
                
                /* Insert character if we got one and there's room */
                if (ch != 0 && si->NumChars < si->MaxChars - 1) {
                    /* Make room at cursor position */
                    WORD i;
                    for (i = si->NumChars; i > si->BufferPos; i--) {
                        si->Buffer[i] = si->Buffer[i - 1];
                    }
                    si->Buffer[si->BufferPos] = ch;
                    si->BufferPos++;
                    si->NumChars++;
                    si->Buffer[si->NumChars] = '\0';
                    needsRefresh = TRUE;
                }
            }
            break;
    }
    
    /* Refresh the gadget display if needed */
    if (needsRefresh && IntuitionBase) {
        DPRINTF(LOG_DEBUG, "_intuition: _handle_string_gadget_key: about to refresh buf='%s' numch=%d pos=%d\n",
                si->Buffer ? (char*)si->Buffer : "(null)", (int)si->NumChars, (int)si->BufferPos);
        _intuition_RefreshGList(IntuitionBase, gad, window, NULL, 1);
        DPRINTF(LOG_DEBUG, "_intuition: _handle_string_gadget_key: refresh done\n");
    }
    
    return TRUE;  /* Stay active */
}

/*
 * Compute mouse coordinates for IDCMP messages, respecting IDCMP_DELTAMOVE.
 * When IDCMP_DELTAMOVE is set on the window, MOUSEMOVE and MOUSEBUTTONS
 * messages carry relative dx/dy instead of absolute window-relative coords.
 * Per RKRM: "IDCMP_DELTAMOVE is not a message type" -- it modifies how
 * MOUSEMOVE/MOUSEBUTTONS report coordinates.
 */
static void _compute_idcmp_mouse_coords(struct Window *window, ULONG class,
                                         WORD absX, WORD absY,
                                         WORD *outX, WORD *outY)
{
    if ((class == IDCMP_MOUSEMOVE || class == IDCMP_MOUSEBUTTONS) &&
        (window->IDCMPFlags & IDCMP_DELTAMOVE))
    {
        /* Delta mode: report change from previous absolute position, in
         * Intuition's internal (hires interlace) resolution - AmigaOS 3.1
         * reports 20/10 for a 10/5 pixel move on a lores screen
         * (tests/scenarios/interactive/IDCMPDeltaMove.yaml) */
        UWORD modes = window->WScreen ? window->WScreen->ViewPort.Modes : HIRES;
        *outX = (WORD)((absX - g_prev_abs_mouse_x) * ((modes & (HIRES | SUPERHIRES)) ? 1 : 2));
        *outY = (WORD)((absY - g_prev_abs_mouse_y) * ((modes & LACE) ? 1 : 2));
    }
    else
    {
        /* Normal mode: window-relative coordinates */
        *outX = absX - window->LeftEdge;
        *outY = absY - window->TopEdge;
    }
}

/*
 * lxa_notify_window_refresh - Send IDCMP_REFRESHWINDOW to a window.
 *
 * Called from lxa_layers.c when a layer is damaged (e.g. after a covering
 * window is deleted).  Mirrors AROS WindowNeedsRefresh() in
 * inputhandler_support.c.  'win' is APTR to avoid a circular include
 * dependency between layers.c and intuition.c.
 */
void lxa_notify_window_refresh(APTR win)
{
    struct Window *window = (struct Window *)win;

    if (!window)
        return;

    /* Only send if the app asked for REFRESHWINDOW events, or to the
     * console.device unit attached to the window (it repairs CON: windows,
     * which are simple refresh) */
    if (!(window->IDCMPFlags & IDCMP_REFRESHWINDOW))
    {
        struct LXAWindowState *state =
            _intuition_find_window_state((struct LXAIntuitionBase *)IntuitionBase, window);
        if (!state || !state->console_port)
            return;
    }

    /* Do not send for SuperBitMap or NoCareRefresh windows */
    if (window->Flags & (WFLG_SUPER_BITMAP | WFLG_NOCAREREFRESH))
        return;

    DPRINTF(LOG_DEBUG, "_intuition: lxa_notify_window_refresh() window=0x%08lx\n",
            (ULONG)window);

    _post_idcmp_message(window, IDCMP_REFRESHWINDOW, 0, 0, NULL,
                        window->MouseX, window->MouseY);
}

/*
 * lxa_force_full_redraw_all - Send IDCMP_REFRESHWINDOW to every open window.
 *
 * Called from the host side via EMU_CALL_INT_FORCE_FULL_REDRAW to trigger
 * deferred-paint apps that wait for an IDCMP_REFRESHWINDOW before doing their
 * initial render.  Iterates all screens and all windows on each screen.
 * Returns the number of windows notified.
 */
LONG lxa_force_full_redraw_all(void)
{
    struct Screen *screen;
    struct Window *window;
    LONG count = 0;

    if (!IntuitionBase)
        return 0;

    for (screen = IntuitionBase->FirstScreen; screen; screen = screen->NextScreen)
    {
        for (window = screen->FirstWindow; window; window = window->NextWindow)
        {
            lxa_notify_window_refresh(window);
            count++;
        }
    }

    DPRINTF(LOG_DEBUG, "_intuition: lxa_force_full_redraw_all() notified %ld windows\n", count);
    return count;
}

/*
 * Internal function to post an IDCMP message to a window
 * Returns TRUE if message was posted, FALSE if window not interested
 */
/*
 * console.device (lxa_dev_console.c) registers the port on which it wants
 * the input events of a window that the window's own IDCMP does not take;
 * NULL detaches.  Unreplied messages stay valid: the reply port is
 * Intuition's and lives until the window closes.
 */
VOID _intuition_set_console_port(struct Window *window, struct MsgPort *port, BOOL attach)
{
    struct LXAWindowState *state;

    if (attach)
    {
        state = _intuition_ensure_window_state((struct LXAIntuitionBase *)IntuitionBase, window);
        if (state)
            state->console_port = port;
    }
    else
    {
        state = _intuition_find_window_state((struct LXAIntuitionBase *)IntuitionBase, window);
        if (state && state->console_port == port)
            state->console_port = NULL;
    }
}

static BOOL _post_idcmp_message(struct Window *window, ULONG class, UWORD code,
                                 UWORD qualifier, APTR iaddress, WORD mouseX, WORD mouseY)
{
    struct LXAWindowState *state;
    struct LXAIntuiMessage *rawkey_msg;
    struct IntuiMessage *imsg;
    struct MsgPort *target, *reply;
    ULONG msg_size;
    
    if (!window) {
        return FALSE;
    }

    state = _intuition_find_window_state((struct LXAIntuitionBase *)IntuitionBase, window);
    if (!state)
    {
        DPRINTF(LOG_DEBUG,
                "_intuition: skipping IDCMP 0x%08lx for unregistered window 0x%08lx\n",
                class, (ULONG)window);
        return FALSE;
    }

    if (window->IDCMPFlags & class)
    {
        if (!window->UserPort || !window->WindowPort)
            return FALSE;
        target = window->UserPort;
        reply = window->WindowPort;
        _reap_window_idcmp_replies(window);
    }
    else
    {
        /* An input event the window's IDCMP does not take goes on to the
         * console.device unit attached to the window, if any - with
         * IDCMP_RAWKEY/VANILLAKEY/MOUSEBUTTONS set, the console sees none
         * of those events (AmigaOS 3.1, tests/console/idcmp_console). */
        if (!state->console_port || !(class & LXA_CONSOLE_INPUT_CLASSES))
            return FALSE;
        if (!state->console_reply_port)
        {
            state->console_reply_port = (struct MsgPort *)AllocMem(sizeof(struct MsgPort), MEMF_PUBLIC | MEMF_CLEAR);
            if (!state->console_reply_port)
                return FALSE;
            state->console_reply_port->mp_Node.ln_Type = NT_MSGPORT;
            state->console_reply_port->mp_Flags = PA_IGNORE;
            NewList(&state->console_reply_port->mp_MsgList);
        }
        _flush_idcmp_port(state->console_reply_port);
        target = state->console_port;
        reply = state->console_reply_port;
    }

    if (class == IDCMP_MOUSEMOVE)
    {
        if (state->pending_mousemoves >= state->mouse_queue)
            return FALSE;
    }

    msg_size = sizeof(struct IntuiMessage);
    if (class == IDCMP_RAWKEY)
        msg_size = sizeof(struct LXAIntuiMessage);

    /* Allocate IntuiMessage */
    imsg = (struct IntuiMessage *)AllocMem(msg_size, MEMF_PUBLIC | MEMF_CLEAR);
    if (!imsg)
    {
        LPRINTF(LOG_ERROR, "_intuition: _post_idcmp_message() out of memory\n");
        return FALSE;
    }
    
    /* Fill in the message */
    imsg->ExecMessage.mn_Node.ln_Type = NT_MESSAGE;
    imsg->ExecMessage.mn_Length = msg_size;
    imsg->ExecMessage.mn_ReplyPort = reply;
    
    imsg->Class = class;
    imsg->Code = code;
    imsg->Qualifier = qualifier;
    if (class == IDCMP_RAWKEY)
    {
        UBYTE *prev;

        rawkey_msg = (struct LXAIntuiMessage *)imsg;
        prev = (UBYTE *)&rawkey_msg->rawkey_prev_code_quals;
        prev[0] = state ? state->prev1_down_code : 0;
        prev[1] = state ? state->prev1_down_qual : 0;
        prev[2] = state ? state->prev2_down_code : 0;
        prev[3] = state ? state->prev2_down_qual : 0;
        imsg->IAddress = &rawkey_msg->rawkey_prev_code_quals;
    }
    else
    {
        imsg->IAddress = iaddress;
    }
    imsg->MouseX = mouseX;
    imsg->MouseY = mouseY;
    imsg->IDCMPWindow = window;
    
    /* Get current time via emucall */
    struct timeval tv;
    emucall1(EMU_CALL_GETSYSTIME, (ULONG)&tv);
    imsg->Seconds = tv.tv_secs;
    imsg->Micros = tv.tv_micro;
    
    /* Update window's mouse position */
    window->MouseX = mouseX;
    window->MouseY = mouseY;
    
    /* Post the message to the window's UserPort (or the console's port) */
    PutMsg(target, (struct Message *)imsg);

    if (class == IDCMP_MOUSEMOVE && state)
        state->pending_mousemoves++;
    
    DPRINTF(LOG_DEBUG, "_intuition: Posted IDCMP 0x%08lx to window 0x%08lx\n",
            class, (ULONG)window);
    
    return TRUE;
}

/*
 * Internal function: Process pending input events from the host
 * This should be called periodically (e.g., from WaitTOF or the scheduler)
 * 
 * The function polls for SDL events via emucalls and posts appropriate
 * IDCMP messages to windows that have requested them.
 */
/* Prevent re-entry to event processing (e.g., VBlank firing during WaitTOF) */

VOID _intuition_ProcessInputEvents(struct Screen *hint_screen)
{
    ULONG event_type;
    ULONG mouse_pos;
    ULONG button_code;
    ULONG key_data;
    WORD mouseX, mouseY;
    struct Window *window;
    DPRINTF(LOG_DEBUG, "_intuition: ProcessInputEvents called hint_screen=0x%08lx\n", (ULONG)hint_screen);
    struct InputEvent input_event;
    struct Screen *screen;
    
    /* Prevent re-entry - if already processing events, skip */
    if (g_processing_events)
    {
        DPRINTF(LOG_DEBUG, "_intuition: ProcessInputEvents SKIPPED (re-entry guard)\n");
        return;
    }
    g_processing_events = TRUE;
    
    /*
     * Use the global IntuitionBase (defined in exec.c) instead of calling
     * OpenLibrary(). This function can be called from VBlank interrupt
     * context, where calling OpenLibrary/CloseLibrary causes reentrancy
     * issues and crashes.
     */
    
    /* Poll for input events */
    while (1)
    {
        event_type = emucall0(EMU_CALL_INT_POLL_INPUT);
        if (event_type == 0)
        {
            DPRINTF(LOG_DEBUG, "_intuition: ProcessInputEvents: no more events, breaking\n");
            break;  /* No more events */
        }
        
        /* Get mouse position for all events */
        mouse_pos = emucall0(EMU_CALL_INT_GET_MOUSE_POS);
        mouseX = (WORD)(mouse_pos >> 16);
        mouseY = (WORD)(mouse_pos & 0xFFFF);

        /* an alert is up (DisplayAlert): mouse buttons answer it and
         * nothing reaches the screens */
        if (g_alert_waiting && event_type == 1)
        {
            UWORD code = (UWORD)(emucall0(EMU_CALL_INT_GET_MOUSE_BTN) & 0xFF);
            if (code == SELECTDOWN || code == MENUDOWN)
                g_alert_button = code;
            continue;
        }

        DPRINTF(LOG_DEBUG, "_intuition: ProcessInputEvents: event_type=%ld mouse=(%d,%d)\n",
                event_type, (int)mouseX, (int)mouseY);
        
        /*
         * Resolve the correct screen for this event based on mouse
         * coordinates.  The event queue is global, so we must route
         * each event to the screen that actually contains the pointer
         * position — otherwise the first screen in the chain would
         * consume events intended for other screens.
         */
        screen = NULL;
        if (IntuitionBase)
        {
            struct Screen *s;
            for (s = IntuitionBase->FirstScreen; s; s = s->NextScreen)
            {
                if (mouseX >= s->LeftEdge &&
                    mouseX < s->LeftEdge + s->Width &&
                    mouseY >= s->TopEdge &&
                    mouseY < s->TopEdge + s->Height)
                {
                    screen = s;
                    break;
                }
            }
        }
        /* Fallback to the caller-supplied hint or ActiveScreen */
        if (!screen)
            screen = hint_screen;
        if (!screen && IntuitionBase)
            screen = IntuitionBase->ActiveScreen;
        if (!screen && IntuitionBase)
            screen = IntuitionBase->FirstScreen;
        if (!screen)
        {
            /* No screen at all — discard event */
            continue;
        }
        
        DPRINTF(LOG_DEBUG, "_intuition: ProcessInputEvents: got event type=%ld screen=0x%08lx\n",
                event_type, (ULONG)screen);
        
        /* Update IntuitionBase and every screen's MouseX/MouseY with the
         * current mouse position, and the timestamp (Cluster2 polls
         * Screen->MouseX/Y to find the toolbar button it is released on) */
        _intuition_update_input_snapshot(IntuitionBase, mouseX, mouseY);
        
        /* Find the window at the mouse position */
        window = _find_window_at_pos(screen, mouseX, mouseY);

        input_event.ie_NextEvent = NULL;
        input_event.ie_SubClass = 0;
        input_event.ie_Code = 0;
        input_event.ie_Qualifier = 0;
        input_event.ie_X = mouseX;
        input_event.ie_Y = mouseY;
        input_event.ie_EventAddress = NULL;
        
        DPRINTF(LOG_DEBUG, "_intuition: ProcessInputEvents: mouse=(%d,%d) window=0x%08lx firstWin=0x%08lx\n",
                mouseX, mouseY, (ULONG)window, (ULONG)screen->FirstWindow);
        
        /* If no window under mouse but we have an active gadget, use that window */
        if (!window && g_active_window)
            window = g_active_window;
        
        /* Fallback to first window if still none found */
        if (!window)
            window = screen->FirstWindow;
        
        switch (event_type)
        {
            case 1:  /* Mouse button */
            {
                button_code = emucall0(EMU_CALL_INT_GET_MOUSE_BTN);
                UWORD code = (UWORD)(button_code & 0xFF);
                UWORD qualifier = (UWORD)((button_code >> 8) & 0xFFFF);
                g_current_qualifier = qualifier;

                DPRINTF(LOG_DEBUG, "_intuition: PIE MouseButton raw=0x%08lx code=0x%02x qual=0x%04x at (%d,%d) window=0x%08lx\n",
                        button_code, (int)code, (int)qualifier, (int)mouseX, (int)mouseY, (ULONG)window);

                input_event.ie_Class = IECLASS_RAWMOUSE;
                input_event.ie_Code = code;
                input_event.ie_Qualifier = qualifier;
                _input_device_dispatch_event(&input_event);
                
                DPRINTF(LOG_DEBUG, "_intuition: MouseButton code=0x%02x qual=0x%04x at (%d,%d)\n",
                        code, qualifier, mouseX, mouseY);

                /*
                 * The menu button pressed or released with no window under
                 * the pointer (e.g. on the screen's title bar): the menus of
                 * the active window (else the first window with a strip) are
                 * shown at once, including the drop-down of the title under
                 * the pointer; releasing ends menu mode.
                 */
                if (!window && code == MENUDOWN && screen && mouseY <= screen->BarHeight)
                {
                    struct Window *menuWin = IntuitionBase->ActiveWindow;
                    struct Window *w;

                    if (!menuWin || menuWin->WScreen != screen || !menuWin->MenuStrip)
                    {
                        menuWin = NULL;
                        for (w = screen->FirstWindow; w; w = w->NextWindow)
                        {
                            if (w->MenuStrip)
                            {
                                menuWin = w;
                                break;
                            }
                        }
                    }

                    if (menuWin)
                    {
                        _post_idcmp_message(menuWin, IDCMP_MENUVERIFY, MENUHOT, qualifier, NULL,
                                            mouseX - menuWin->LeftEdge, mouseY - menuWin->TopEdge);
                        for (w = screen->FirstWindow; w; w = w->NextWindow)
                        {
                            if (w != menuWin && (w->IDCMPFlags & IDCMP_MENUVERIFY))
                                _post_idcmp_message(w, IDCMP_MENUVERIFY, MENUWAITING, qualifier, NULL,
                                                    mouseX - w->LeftEdge, mouseY - w->TopEdge);
                        }
                        _enter_menu_mode(menuWin, screen, mouseX, mouseY);
                    }
                }
                else if (!window && code == MENUUP && g_menu_mode && g_menu_window)
                {
                    _exit_menu_mode(g_menu_window, mouseX, mouseY);
                }
                
                if (window)
                {
                    /* Convert to window-relative coordinates */
                    WORD relX = mouseX - window->LeftEdge;
                    WORD relY = mouseY - window->TopEdge;
                    
                    DPRINTF(LOG_DEBUG, "_intuition: MouseButton: window=0x%08lx at (%d,%d) size=(%d,%d) rel=(%d,%d)\n",
                            (ULONG)window, window->LeftEdge, window->TopEdge, window->Width, window->Height, relX, relY);
                    
                    /* Check for gadget hit on mouse down (SELECTDOWN) */
                    DPRINTF(LOG_DEBUG, "_intuition: MouseButton code=0x%02x at (%d,%d) relXY=(%d,%d) window=0x%08lx\n",
                            code, mouseX, mouseY, relX, relY, (ULONG)window);
                    DPRINTF(LOG_DEBUG, "_intuition: MouseButton MENUDOWN=0x%02x SELECTDOWN=0x%02x SELECTUP=0x%02x MENUUP=0x%02x\n",
                            (UWORD)MENUDOWN, (UWORD)SELECTDOWN, (UWORD)SELECTUP, (UWORD)MENUUP);
                    if (code == SELECTDOWN)
                    {
                        if (!(window->Flags & WFLG_WINDOWACTIVE))
                        {
                            _intuition_ActivateWindow(IntuitionBase, window);
                        }

                        struct Gadget *gad = _find_gadget_at_pos(window, relX, relY);
                        DPRINTF(LOG_DEBUG, "_intuition: SELECTDOWN relX=%d relY=%d gad=0x%08lx type=0x%04x\n",
                                relX, relY, (ULONG)gad, gad ? gad->GadgetType : 0xFFFF);
                        /* a click anywhere else ends the active string gadget */
                        if (g_active_gadget && g_active_gadget != gad &&
                            (g_active_gadget->GadgetType & GTYP_GTYPEMASK) == GTYP_STRGADGET)
                        {
                            _string_gadget_leave(g_active_window, g_active_gadget);
                            g_active_gadget = NULL;
                            g_active_window = NULL;
                        }
                        if (gad)
                        {
                            DPRINTF(LOG_DEBUG, "_intuition: SELECTDOWN on gadget type=0x%04x\n",
                                    gad->GadgetType);
                            
                            /* Set as active gadget for tracking */
                            g_active_gadget = gad;
                            g_active_window = window;
                            
                            /* Set gadget as selected */
                            gad->Flags |= GFLG_SELECTED;

                            if (_gadtools_IsGadTools(gad))
                            {
                                if (gad->Activation & GACT_IMMEDIATE)
                                {
                                    LONG gbl, gbt, gbw, gbh;
                                    _calculate_gadget_box(window, NULL, gad, &gbl, &gbt, &gbw, &gbh);
                                    g_gt_down_code = _gadtools_HandleClick(gad, relX - gbl, relY - gbt);
                                    if (g_gt_down_code < 0)
                                        g_gt_down_code = 0;
                                }
                                _render_gadget(window, NULL, gad);
                            }
                            
                            /* For sizing system gadget, start resize drag immediately */
                            if ((gad->GadgetType & GTYP_SYSGADGET) &&
                                (gad->GadgetType & GTYP_SYSTYPEMASK) == GTYP_SIZING)
                            {
                                /* Per RKRM: Post IDCMP_SIZEVERIFY before allowing the resize */
                                _post_idcmp_message(window, IDCMP_SIZEVERIFY, 0, qualifier,
                                                    NULL, relX, relY);

                                g_sizing_window = TRUE;
                                g_size_window = window;
                                g_size_start_x = mouseX;
                                g_size_start_y = mouseY;
                                g_size_orig_w = window->Width;
                                g_size_orig_h = window->Height;
                            }
                            
                            /* For string gadgets, position cursor at end of text on click */
                            if ((gad->GadgetType & GTYP_GTYPEMASK) == GTYP_STRGADGET)
                            {
                                struct StringInfo *si = (struct StringInfo *)gad->SpecialInfo;
                                if (si && si->Buffer)
                                {
                                    /* the cursor goes to the character under the
                                     * pointer, else to the end of the text
                                     * (AmigaOS 3.1, scenario gallery-asl-screenmode) */
                                    LONG gbl, gbt, gbw, gbh;
                                    struct TextFont *tf = window->IFont ? window->IFont : (window->RPort ? window->RPort->Font : NULL);
                                    WORD cw;

                                    if ((gad->Flags & GFLG_STRINGEXTEND || gad->Activation & GACT_STRINGEXTEND) &&
                                        si->Extension && si->Extension->Font)
                                        tf = si->Extension->Font;
                                    cw = tf && tf->tf_XSize ? tf->tf_XSize : 8;
                                    LONG pos;

                                    _calculate_gadget_box(window, NULL, gad, &gbl, &gbt, &gbw, &gbh);
                                    pos = si->DispPos + (relX - gbl) / cw;
                                    if (pos < 0)
                                        pos = 0;
                                    if (pos > si->NumChars)
                                        pos = si->NumChars;
                                    si->BufferPos = (WORD)pos;
                                }

                                _render_gadget(window, NULL, gad);
                            }
                            
                            /* For prop gadgets, compute drag offset for smooth knob tracking */
                            if ((gad->GadgetType & GTYP_GTYPEMASK) == GTYP_PROPGADGET)
                                _prop_select_down(window, gad, mouseX);
                            
                            /* Post IDCMP_GADGETDOWN if it's a GADGIMMEDIATE gadget */
                            if (gad->Activation & GACT_IMMEDIATE)
                            {
                                _post_idcmp_message(window, IDCMP_GADGETDOWN, (UWORD)g_gt_down_code,
                                                   qualifier, gad, relX, relY);
                            }
                            
                            /* Render selected state highlight (but not for prop gadgets - they draw their own knob) */
                            if ((gad->Flags & GFLG_GADGHIGHBITS) == GFLG_GADGHCOMP &&
                                (gad->GadgetType & GTYP_GTYPEMASK) != GTYP_PROPGADGET &&
                                (gad->GadgetType & GTYP_GTYPEMASK) != GTYP_STRGADGET)
                            {
                                _complement_gadget_area(window, NULL, gad);
                            }
                        }
                        else
                        {
                            /* No gadget hit - check for title bar drag */
                            if ((window->Flags & WFLG_DRAGBAR) && 
                                relY >= 0 && relY < window->BorderTop &&
                                relX >= 0 && relX < window->Width)
                            {
                                /* Click is in title bar area - start dragging */
                                DPRINTF(LOG_DEBUG, "_intuition: Starting window drag: window=0x%08lx at (%d,%d)\n",
                                        (ULONG)window, window->LeftEdge, window->TopEdge);
                                g_dragging_window = TRUE;
                                g_drag_window = window;
                                g_drag_start_x = mouseX;
                                g_drag_start_y = mouseY;
                                g_drag_window_x = window->LeftEdge;
                                g_drag_window_y = window->TopEdge;
                            }
                        }
                    }
                    /* Check for gadget release on mouse up (SELECTUP) */
                    else if (code == SELECTUP)
                    {
                        if (g_active_gadget && g_active_window)
                        {
                            struct Gadget *gad = g_active_gadget;
                            struct Window *activeWin = g_active_window;
                            
                            /* Calculate relative position in the active window */
                            WORD activeRelX = mouseX - activeWin->LeftEdge;
                            WORD activeRelY = mouseY - activeWin->TopEdge;
                            
                            /* Check if still inside gadget (RELVERIFY) */
                            BOOL inside = _point_in_gadget(activeWin, gad, activeRelX, activeRelY);
                            
                            DPRINTF(LOG_DEBUG, "_intuition: SELECTUP gadtype=0x%04x inside=%d isSys=%d isStr=%d\n",
                                    gad->GadgetType, inside,
                                    (gad->GadgetType & GTYP_SYSGADGET) ? 1 : 0,
                                    ((gad->GadgetType & GTYP_GTYPEMASK) == GTYP_STRGADGET) ? 1 : 0);
                            
                            DPRINTF(LOG_DEBUG, "_intuition: SELECTUP on gadget type=0x%04x inside=%d\n",
                                    gad->GadgetType, inside);
                            
                            /* For string gadgets, keep them active to receive keyboard input */
                            if ((gad->GadgetType & GTYP_GTYPEMASK) == GTYP_STRGADGET)
                            {
                                /* String gadget stays active until Return/Escape */
                                /* Keep GFLG_SELECTED set and g_active_gadget pointing to it */
                                DPRINTF(LOG_DEBUG, "_intuition: String gadget remains active for keyboard input\n");
                                
                                if (inside)
                                {
                                    /* Post GADGETDOWN if not already (it's now the active string gadget) */
                                    if (gad->Activation & GACT_IMMEDIATE)
                                    {
                                        _post_idcmp_message(activeWin, IDCMP_GADGETDOWN, 0,
                                                           qualifier, gad, activeRelX, activeRelY);
                                    }
                                }

                                _render_gadget(activeWin, NULL, gad);
                                /* Don't clear g_active_gadget or g_active_window */
                            }
                            /* For prop gadgets: finalize pot, compute level, post GADGETUP */
                            else if ((gad->GadgetType & GTYP_GTYPEMASK) == GTYP_PROPGADGET)
                            {
                                struct PropInfo *pi = (struct PropInfo *)gad->SpecialInfo;
                                UWORD level_code = 0;

                                gad->Flags &= ~GFLG_SELECTED;

                                if (pi)
                                {
                                    LONG level = _prop_slider_level(gad, pi);
                                    _gadtools_UpdateSliderLevelDisplay(gad, level);
                                    level_code = (UWORD)level;

                                    DPRINTF(LOG_DEBUG, "_intuition: Prop SELECTUP: pot=%u level=%ld\n", pi->HorizPot, level);
                                }

                                /* Post IDCMP_GADGETUP with level as Code, gadget as IAddress */
                                if (gad->Activation & GACT_RELVERIFY)
                                {
                                    _post_idcmp_message(activeWin, IDCMP_GADGETUP, level_code,
                                                       qualifier, gad, activeRelX, activeRelY);
                                }

                                /* Clear active gadget */
                                g_active_gadget = NULL;
                                g_active_window = NULL;
                            }
                            else
                            {
                                /* Non-string gadgets: update selected state */
                                if (_gadtools_IsCheckbox(gad))
                                {
                                    if (inside)
                                    {
                                        _gadtools_SetCheckboxState(gad,
                                                                   _gadtools_GetCheckboxState(gad) ? FALSE : TRUE);
                                    }
                                    /* a checked GadTools checkbox stays GFLG_SELECTED (AmigaOS 3.1) */
                                    if (_gadtools_GetCheckboxState(gad))
                                        gad->Flags |= GFLG_SELECTED;
                                    else
                                        gad->Flags &= ~GFLG_SELECTED;
                                }
                                else if (_gadtools_IsCycle(gad))
                                {
                                    UWORD active = _gadtools_GetCycleState(gad);

                                    if (inside)
                                        active = _gadtools_AdvanceCycleState(gad);

                                    gad->Flags &= ~GFLG_SELECTED;

                                    if (inside && (gad->Activation & GACT_RELVERIFY))
                                    {
                                        _post_idcmp_message(activeWin, IDCMP_GADGETUP, active,
                                                           qualifier, gad, activeRelX, activeRelY);
                                    }
                                }
                                else if ((gad->Activation & GACT_TOGGLESELECT) && inside)
                                {
                                    /* Toggle gadgets: flip SELECTED state on release inside gadget */
                                    gad->Flags ^= GFLG_SELECTED;
                                }
                                else
                                {
                                    /* Normal gadgets: clear selected state */
                                    gad->Flags &= ~GFLG_SELECTED;
                                }
                                
                                if (inside)
                                {
                                    LONG up_code = 0;
                                    if (_gadtools_IsGadTools(gad) && !(gad->Activation & GACT_IMMEDIATE))
                                    {
                                        LONG gbl, gbt, gbw, gbh;
                                        _calculate_gadget_box(activeWin, NULL, gad, &gbl, &gbt, &gbw, &gbh);
                                        up_code = _gadtools_HandleClick(gad, activeRelX - gbl, activeRelY - gbt);
                                        if (up_code < 0)
                                            up_code = 0;
                                    }
                                    if (gad->GadgetType & GTYP_SYSGADGET)
                                    {
                                        _handle_sys_gadget_verify(activeWin, gad);
                                    }
                                    /* Post IDCMP_GADGETUP for RELVERIFY gadgets */
                                    else if (gad->Activation & GACT_RELVERIFY)
                                    {
                                        _post_idcmp_message(activeWin, IDCMP_GADGETUP, (UWORD)up_code,
                                                           qualifier, gad, activeRelX, activeRelY);
                                    }
                                }
                                
                                /* Render normal state - undo GADGHCOMP highlight */
                                if (_gadtools_IsGadTools(gad))
                                {
                                    _render_gadget(activeWin, NULL, gad);
                                }
                                else if ((gad->Flags & GFLG_GADGHIGHBITS) == GFLG_GADGHCOMP)
                                {
                                    _complement_gadget_area(activeWin, NULL, gad);
                                }
                                
                                /* Clear active gadget */
                                g_active_gadget = NULL;
                                g_active_window = NULL;
                            }
                        }
                        
                        /* Stop window dragging on mouse up */
                        if (g_dragging_window)
                        {
                            DPRINTF(LOG_DEBUG, "_intuition: Stopping window drag\n");
                            g_dragging_window = FALSE;
                            g_drag_window = NULL;
                        }
                        
                        /* Stop window sizing on mouse up */
                        if (g_sizing_window)
                        {
                            DPRINTF(LOG_DEBUG, "_intuition: Stopping window resize: final size=(%d,%d)\n",
                                    g_size_window ? g_size_window->Width : 0,
                                    g_size_window ? g_size_window->Height : 0);
                            g_sizing_window = FALSE;
                            g_size_window = NULL;
                        }
                    }
                    /* Right mouse button press - enter menu mode */
                    else if (code == MENUDOWN)
                    {
                        DPRINTF(LOG_DEBUG, "_intuition: MENUDOWN(PIE) ENTERED at (%d,%d), window=0x%08lx MenuStrip=0x%08lx\n",
                                mouseX, mouseY, (ULONG)window, window ? (ULONG)window->MenuStrip : 0);
                        /* 
                         * On real AmigaOS, right-clicking activates the menu bar.
                         * When the mouse is in the screen title bar (above all
                         * windows), the menu strip of the active window is used
                         * regardless of WFLG_RMBTRAP, because the title bar is a
                         * system-level area owned by Intuition, not by any window.
                         *
                         * When the mouse is over a window that has WFLG_RMBTRAP,
                         * the raw MOUSEBUTTONS event is delivered to that window
                         * and the menu system is NOT activated.
                         *
                         * When the mouse is over a window without RMBTRAP and
                         * with a MenuStrip, that window's menus are shown.
                         */
                        struct Window *menuWin = NULL;
                        BOOL in_title_bar = (screen && mouseY <= screen->BarHeight);
                        DPRINTF(LOG_DEBUG, "_intuition: MENUDOWN in_title_bar=%d BarHeight=%d\n",
                                (int)in_title_bar, screen ? (int)screen->BarHeight : -1);

                        /* First, check the window under the mouse */
                        if (window && !(window->Flags & WFLG_RMBTRAP) &&
                            _intuition_menu_window((struct LXAIntuitionBase *)IntuitionBase, window)->MenuStrip)
                        {
                            menuWin = _intuition_menu_window((struct LXAIntuitionBase *)IntuitionBase, window);
                        }
                        else if (in_title_bar)
                        {
                            /*
                             * Mouse is in the screen title bar — this is system
                             * territory.  Find the first window with a MenuStrip,
                             * ignoring WFLG_RMBTRAP.  The title bar click belongs
                             * to the system, not to any window, so RMBTRAP does
                             * not suppress menu activation here.
                             */
                            struct Window *w;
                            for (w = screen->FirstWindow; w; w = w->NextWindow)
                            {
                                DPRINTF(LOG_DEBUG, "_intuition: title_bar walk: w=0x%08lx MenuStrip=0x%08lx\n",
                                        (ULONG)w, (ULONG)w->MenuStrip);
                                if (w->MenuStrip)
                                {
                                    menuWin = w;
                                    break;
                                }
                            }
                            DPRINTF(LOG_DEBUG, "_intuition: title_bar resolved menuWin=0x%08lx\n", (ULONG)menuWin);
                        }
                        else
                        {
                            /* Mouse is over a window area but no direct match.
                             * Walk the window list and respect WFLG_RMBTRAP. */
                            struct Window *w;
                            for (w = screen->FirstWindow; w; w = w->NextWindow)
                            {
                                if (w->MenuStrip && !(w->Flags & WFLG_RMBTRAP))
                                {
                                    menuWin = w;
                                    break;
                                }
                            }
                        }

                        DPRINTF(LOG_DEBUG, "_intuition: MENUDOWN final menuWin=0x%08lx MenuStrip=0x%08lx\n",
                                (ULONG)menuWin, menuWin ? (ULONG)menuWin->MenuStrip : 0);
                        if (menuWin && menuWin->MenuStrip)
                        {
                            /* Per RKRM: Post IDCMP_MENUVERIFY with MENUHOT to the
                             * active window and MENUWAITING to others before
                             * activating menus.  See non-PIE handler for full comment. */
                            _post_idcmp_message(menuWin, IDCMP_MENUVERIFY, MENUHOT,
                                                qualifier, NULL, relX, relY);
                            if (screen)
                            {
                                struct Window *w;
                                for (w = screen->FirstWindow; w; w = w->NextWindow)
                                {
                                    if (w != menuWin && (w->IDCMPFlags & IDCMP_MENUVERIFY))
                                    {
                                        WORD wRelX = mouseX - w->LeftEdge;
                                        WORD wRelY = mouseY - w->TopEdge;
                                        _post_idcmp_message(w, IDCMP_MENUVERIFY, MENUWAITING,
                                                            qualifier, NULL, wRelX, wRelY);
                                    }
                                }
                            }

                            _enter_menu_mode(menuWin, screen, mouseX, mouseY);
                        }
                    }
                    /* Right mouse button release - exit menu mode and select item */
                    else if (code == MENUUP)
                    {
                        DPRINTF(LOG_DEBUG, "_intuition: MENUUP(PIE) at (%d,%d), g_menu_mode=%d, g_active_menu=0x%08lx, g_active_item=0x%08lx\n",
                                mouseX, mouseY, g_menu_mode, (ULONG)g_active_menu, (ULONG)g_active_item);
                        if (g_menu_mode && g_menu_window)
                        {
                            _exit_menu_mode(g_menu_window, mouseX, mouseY);
                        }
                    }
                    
                    /* Post IDCMP_MOUSEBUTTONS for general notification.
                     * Per RKRM: do NOT post for RMB when menus are being
                     * activated (MENUDOWN) or released (MENUUP), unless
                     * WFLG_RMBTRAP. The menu system consumes these events. */
                    BOOL postMouseBtn = TRUE;
                    if ((code == MENUDOWN || code == MENUUP) &&
                        !(window && (window->Flags & WFLG_RMBTRAP)))
                    {
                        postMouseBtn = FALSE;
                    }
                    if (postMouseBtn)
                    {
                        WORD btnMsgX, btnMsgY;
                        _compute_idcmp_mouse_coords(window, IDCMP_MOUSEBUTTONS, mouseX, mouseY, &btnMsgX, &btnMsgY);
                        _post_idcmp_message(window, IDCMP_MOUSEBUTTONS, code,
                                           qualifier, NULL, btnMsgX, btnMsgY);
                    }
                }
                break;
            }
            
            case 2:  /* Mouse move */
            {
                key_data = emucall0(EMU_CALL_INT_GET_KEY);  /* Get qualifier */
                UWORD qualifier = (UWORD)(key_data >> 16);
                g_current_qualifier = qualifier;

                DPRINTF(LOG_DEBUG, "_intuition: PIE case2 mouse_move at (%d,%d) g_menu_mode=%d\n",
                        mouseX, mouseY, g_menu_mode);

                input_event.ie_Class = IECLASS_POINTERPOS;
                input_event.ie_Qualifier = qualifier;
                _input_device_dispatch_event(&input_event);
                
                /* Handle menu tracking when in menu mode */
                if (g_menu_mode && g_menu_window && screen)
                    _menu_track_pointer(screen, mouseX, mouseY);
                
                /* Handle window dragging */
                if (g_dragging_window && g_drag_window)
                {
                    /* Calculate new window position based on mouse delta */
                    WORD dx = mouseX - g_drag_start_x;
                    WORD dy = mouseY - g_drag_start_y;
                    WORD newX = g_drag_window_x + dx;
                    WORD newY = g_drag_window_y + dy;
                    
                    /* Clamp to screen bounds */
                    struct Screen *wscreen = g_drag_window->WScreen;
                    if (wscreen)
                    {
                        /* Ensure at least part of the window title bar remains visible */
                        if (newX < -g_drag_window->Width + 20)
                            newX = -g_drag_window->Width + 20;
                        if (newX > wscreen->Width - 20)
                            newX = wscreen->Width - 20;
                        if (newY < 0)
                            newY = 0;
                        if (newY > wscreen->Height - g_drag_window->BorderTop)
                            newY = wscreen->Height - g_drag_window->BorderTop;
                    }
                    
                    /* Move window to new position */
                    if (newX != g_drag_window->LeftEdge || newY != g_drag_window->TopEdge)
                    {
                        WORD move_dx = newX - g_drag_window->LeftEdge;
                        WORD move_dy = newY - g_drag_window->TopEdge;
                        
                        DPRINTF(LOG_DEBUG, "_intuition: Dragging window to (%d,%d) delta=(%d,%d)\n",
                                newX, newY, move_dx, move_dy);
                        
                        /* Use MoveWindow for proper layer handling */
                        _intuition_MoveWindow(IntuitionBase, g_drag_window, move_dx, move_dy);
                    }
                }
                
                /* Handle window sizing */
                if (g_sizing_window && g_size_window)
                {
                    /* Calculate new window size based on mouse delta from start */
                    WORD dx = mouseX - g_size_start_x;
                    WORD dy = mouseY - g_size_start_y;
                    WORD newW = g_size_orig_w + dx;
                    WORD newH = g_size_orig_h + dy;
                    /* sizing with the mouse respects the window limits */
                    if (newW < g_size_window->MinWidth) newW = g_size_window->MinWidth;
                    if ((UWORD)newW > g_size_window->MaxWidth) newW = g_size_window->MaxWidth;
                    if (newH < g_size_window->MinHeight) newH = g_size_window->MinHeight;
                    if ((UWORD)newH > g_size_window->MaxHeight) newH = g_size_window->MaxHeight;

                    /* SizeWindow will enforce min/max limits */
                    WORD size_dx = newW - g_size_window->Width;
                    WORD size_dy = newH - g_size_window->Height;
                    
                    if (size_dx != 0 || size_dy != 0)
                    {
                        DPRINTF(LOG_DEBUG, "_intuition: Sizing window: delta=(%d,%d) target=(%d,%d)\n",
                                size_dx, size_dy, newW, newH);
                        
                        _intuition_SizeWindow(IntuitionBase, g_size_window, size_dx, size_dy);
                    }
                }
                
                /* Handle prop gadget dragging */
                if (g_active_gadget && g_active_window &&
                    (g_active_gadget->GadgetType & GTYP_GTYPEMASK) == GTYP_PROPGADGET)
                {
                    struct Gadget *gad = g_active_gadget;
                    struct Window *activeWin = g_active_window;
                    LONG level;

                    if (_prop_drag(activeWin, gad, mouseX, &level))
                        _post_idcmp_message(activeWin, IDCMP_MOUSEMOVE, (UWORD)level, qualifier, gad,
                                            mouseX - activeWin->LeftEdge, mouseY - activeWin->TopEdge);
                }
                
                if (window && !g_menu_mode)
                {
                    /* Update window mouse position (always absolute) */
                    WORD relX = mouseX - window->LeftEdge;
                    WORD relY = mouseY - window->TopEdge;
                    WORD msgX, msgY;
                    window->MouseX = relX;
                    window->MouseY = relY;
                    
                    /* Post IDCMP_MOUSEMOVE if requested (respects DELTAMOVE) */
                    _compute_idcmp_mouse_coords(window, IDCMP_MOUSEMOVE, mouseX, mouseY, &msgX, &msgY);
                    _post_idcmp_message(window, IDCMP_MOUSEMOVE, 0, 
                                       qualifier, NULL, msgX, msgY);
                }

        /* Update previous absolute position for DELTAMOVE calculations */
        g_prev_abs_mouse_x = mouseX;
        g_prev_abs_mouse_y = mouseY;
        DPRINTF(LOG_DEBUG, "_intuition: PIE case2 DONE at (%d,%d), looping\n", (int)mouseX, (int)mouseY);
        break;
            }
            
            case 3:  /* Key */
            {
                key_data = emucall0(EMU_CALL_INT_GET_KEY);
                UWORD rawkey = (UWORD)(key_data & 0xFFFF);
                UWORD qualifier = (UWORD)(key_data >> 16);
                g_current_qualifier = qualifier;

                input_event.ie_Class = IECLASS_RAWKEY;
                input_event.ie_Code = rawkey;
                input_event.ie_Qualifier = qualifier;
                _input_device_dispatch_event(&input_event);
                _keyboard_device_record_event(rawkey, qualifier);

                /*
                 * Phase 135: Keyboard events go to the ACTIVE window, not
                 * the window under the mouse pointer.  This matches real
                 * Amiga behaviour and is required by apps like BlitzBasic 2
                 * whose IDE waits on a different window's UserPort than the
                 * one the cursor happens to hover over.
                 *
                 * Resolution order:
                 *   1. IntuitionBase->ActiveWindow (canonical)
                 *   2. g_active_window (string-gadget/menu activation tracker)
                 *   3. window under mouse (legacy fallback)
                 *   4. screen->FirstWindow (last resort)
                 */
                struct Window *kbd_window = NULL;
                if (IntuitionBase && IntuitionBase->ActiveWindow &&
                    IntuitionBase->ActiveWindow->WScreen == screen)
                {
                    kbd_window = IntuitionBase->ActiveWindow;
                }
                if (!kbd_window && g_active_window &&
                    g_active_window->WScreen == screen)
                {
                    kbd_window = g_active_window;
                }
                if (!kbd_window)
                    kbd_window = window;
                if (!kbd_window && screen)
                    kbd_window = screen->FirstWindow;

                DPRINTF(LOG_DEBUG, "RAWKEY route: rawkey=0x%04x qual=0x%04x active=0x%08lx mouse_win=0x%08lx chosen=0x%08lx\n",
                        rawkey, qualifier,
                        IntuitionBase ? (ULONG)IntuitionBase->ActiveWindow : 0,
                        (ULONG)window, (ULONG)kbd_window);

                /* Check if there's an active string gadget that should receive keyboard input */
                if (g_active_gadget && g_active_window &&
                    (g_active_gadget->GadgetType & GTYP_GTYPEMASK) == GTYP_STRGADGET)
                {
                    /* Route keyboard input to the string gadget */
                    BOOL stillActive = _handle_string_gadget_key(g_active_gadget, g_active_window, 
                                                                  rawkey, qualifier);
                    if (!stillActive)
                    {
                        /* Gadget deactivated (Return/Escape pressed) */
                        g_active_gadget = NULL;
                        g_active_window = NULL;
                    }
                }
                else if (kbd_window)
                {
                    struct Window *window = kbd_window;  /* shadow for existing code below */
                    struct LXAWindowState *state = _intuition_find_window_state(
                        (struct LXAIntuitionBase *)IntuitionBase, window);
                    WORD relX = mouseX - window->LeftEdge;
                    WORD relY = mouseY - window->TopEdge;
                    BOOL posted = FALSE;

                    /*
                     * Menu keyboard shortcuts (CommKey / Right-Amiga + key).
                     *
                     * When the Right Amiga qualifier is held and the window
                     * has a menu strip, convert the raw key to ASCII and
                     * scan the strip for a matching Command entry.  If found,
                     * post IDCMP_MENUPICK with the encoded FULLMENUNUM code
                     * instead of the normal RAWKEY/VANILLAKEY message.
                     *
                     * Matches AROS behaviour: shortcuts fire even for
                     * disabled items, and WFLG_RMBTRAP suppresses them.
                     */
                    if ((qualifier & IEQUALIFIER_RCOMMAND) &&
                        !(rawkey & IECODE_UP_PREFIX) &&
                        window->MenuStrip &&
                        !(window->Flags & WFLG_RMBTRAP))
                    {
                        char ascii = U_rawkeyToVanilla(rawkey,
                                         qualifier & ~IEQUALIFIER_RCOMMAND);
                        if (ascii)
                        {
                            UWORD menuCode = _find_menu_commkey(
                                                 window->MenuStrip, ascii);
                            if (menuCode != MENUNULL)
                            {
                                DPRINTF(LOG_DEBUG,
                                    "_intuition: CommKey '%c' -> menuCode=0x%04x\n",
                                    ascii, menuCode);
                                posted = _post_idcmp_message(window,
                                             IDCMP_MENUPICK, menuCode,
                                             qualifier, NULL, relX, relY);
                            }
                        }
                    }

                    /* If window wants VANILLAKEY, try to convert rawkey to ASCII */
                    if (!posted && (window->IDCMPFlags & IDCMP_VANILLAKEY))
                    {
                        char ascii = U_rawkeyToVanilla(rawkey, qualifier);
                        if (ascii)
                        {
                            posted = _post_idcmp_message(window, IDCMP_VANILLAKEY,
                                         (UWORD)ascii, qualifier, NULL, relX, relY);
                        }
                    }

                    /* Fall through to RAWKEY if no VANILLAKEY posted */
                    if (!posted)
                    {
                        _post_idcmp_message(window, IDCMP_RAWKEY, rawkey,
                                           qualifier, NULL, relX, relY);
                    }

                    _intuition_note_rawkey(state, rawkey, qualifier);
                }
                break;
            }
            
            case 4:  /* Close window request (from host window manager) */
            {
                input_event.ie_Class = IECLASS_EVENT;
                input_event.ie_Code = IECODE_NEWACTIVE;
                _input_device_dispatch_event(&input_event);

                if (window)
                {
                    WORD relX = mouseX - window->LeftEdge;
                    WORD relY = mouseY - window->TopEdge;
                    _post_idcmp_message(window, IDCMP_CLOSEWINDOW, 0, 
                                       0, window, relX, relY);
                }
                break;
            }
            
            case 5:  /* Quit */
            {
                /* System quit requested - post close to all windows on ALL screens */
                struct Screen *qs;
                if (IntuitionBase)
                {
                    for (qs = IntuitionBase->FirstScreen; qs; qs = qs->NextScreen)
                    {
                        struct Window *qw;
                        for (qw = qs->FirstWindow; qw; qw = qw->NextWindow)
                        {
                            _post_idcmp_message(qw, IDCMP_CLOSEWINDOW, 0, 0, qw, 0, 0);
                        }
                    }
                }
                break;
            }
        }
    }
    
    /* Release re-entry guard */
    g_processing_events = FALSE;
}

/*
 * VBlank hook for input processing.
 * Called from the VBlank interrupt handler to ensure input events
 * are processed even when the app doesn't call WaitTOF().
 * 
 * This function must be called from a context where interrupts are safe
 * (e.g., at the end of VBlank processing before scheduling).
 * 
 * IMPORTANT: This function is called from interrupt context. It MUST NOT
 * call OpenLibrary/CloseLibrary as this can cause reentrancy issues when
 * the interrupted code is also doing library calls. Instead, we use the
 * global IntuitionBase which is set up at ROM initialization time.
 */

VOID _intuition_VBlankInputHook(void)
{
    /* Use global IntuitionBase - no OpenLibrary calls from interrupt context! */
    if (!IntuitionBase)
    {
        LPRINTF(LOG_WARNING, "_intuition: VBlankInputHook: IntuitionBase is NULL!\n");
        return;
    }
    
    if (!IntuitionBase->FirstScreen)
    {
        /* No screen yet - this is normal during startup */
        return;
    }
    
    DPRINTF(LOG_DEBUG, "_intuition: VBlankInputHook calling PIE, g_processing_events=%d g_menu_mode=%d\n",
            g_processing_events, g_menu_mode);
    
    /* Reset re-entry guard here rather than relying on ProcessInputEvents
     * to clear it. If the previous cycle budget expired mid-loop, the flag
     * would remain set and block all future VBlank processing. Resetting
     * at the top of the VBlank hook is safe because VBlankInputHook is
     * the sole caller of ProcessInputEvents from interrupt context. */
    g_processing_events = FALSE;
    
    /*
     * Process all pending input events.  The function resolves the
     * correct target screen for each event internally using mouse
     * coordinates, so we only need a single call.  The hint_screen
     * parameter provides a fallback when the coordinates don't match
     * any screen viewport.
     */
    _intuition_ProcessInputEvents(IntuitionBase->FirstScreen);

    /* Phase 149: deferred-paint trigger.  Poll the host flag once per VBlank.
     * If set, the host has called lxa_force_full_redraw() — send
     * IDCMP_REFRESHWINDOW to all open windows so deferred-paint apps finally
     * render.  The EMU_CALL clears the host flag atomically. */
    if (emucall0(EMU_CALL_INT_FORCE_FULL_REDRAW))
    {
        lxa_force_full_redraw_all();
    }

    /*
     * IDCMP_INTUITICKS: fire approximately every 10th VBlank (~5 Hz on PAL).
     * AmigaOS 3.1 (tests/probes/intuition/ticks): only the active window
     * gets ticks, and never a second one while WFLG_WINDOWTICKED is set;
     * the flag is cleared when Intuition reaps the tick's reply, which it
     * does for the active window on every tick and for any window when it
     * posts another message to it.
     */
    g_intuitick_counter++;
    if (g_intuitick_counter >= 10)
    {
        struct Window *win = IntuitionBase->ActiveWindow;

        g_intuitick_counter = 0;
        if (win && _intuition_find_window_state((struct LXAIntuitionBase *)IntuitionBase, win))
        {
            _reap_window_idcmp_replies(win);
            if ((win->IDCMPFlags & IDCMP_INTUITICKS) && !(win->Flags & WFLG_WINDOWTICKED))
            {
                if (_post_idcmp_message(win, IDCMP_INTUITICKS, 0,
                                        g_current_qualifier,
                                        NULL, win->MouseX, win->MouseY))
                    win->Flags |= WFLG_WINDOWTICKED;
            }
        }
    }
}

/*
 * ReplyIntuiMsg - Reply to an IntuiMessage and free it
 * This is a convenience function for applications
 */
VOID _intuition_ReplyIntuiMsg(struct IntuiMessage *imsg)
{
    if (imsg)
    {
        ULONG msg_size = sizeof(struct IntuiMessage);

        if (imsg->ExecMessage.mn_Length >= sizeof(struct IntuiMessage))
            msg_size = imsg->ExecMessage.mn_Length;

        FreeMem(imsg, msg_size);
    }
}

/*
 * ModifyProp - Modify proportional gadget properties
 *
 * This function updates the values of a proportional gadget and optionally
 * refreshes its visual appearance. Used for scrollbars and sliders.
 *
 * Current implementation is a stub that updates the PropInfo structure
 * but doesn't refresh the visual.
 */
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
    LXA_UNIMPLEMENTED("intuition", "ModifyProp", "partial: updates PropInfo but does not re-render the gadget (Phase 256)");

    DPRINTF (LOG_DEBUG, "_intuition: ModifyProp() called, gadget=0x%08lx window=0x%08lx\n",
             (ULONG)gadget, (ULONG)window);

    if (!gadget)
        return;

    /* Check if this is a proportional gadget */
    if ((gadget->GadgetType & GTYP_GTYPEMASK) != GTYP_PROPGADGET) {
        DPRINTF (LOG_WARNING, "_intuition: ModifyProp() called on non-prop gadget\n");
        return;
    }

    /* Get the PropInfo structure from the gadget */
    struct PropInfo *pi = (struct PropInfo *)gadget->SpecialInfo;
    if (!pi)
        return;

    /* Update the PropInfo fields */
    pi->Flags = flags;
    pi->HorizPot = horizPot;
    pi->VertPot = vertPot;
    pi->HorizBody = horizBody;
    pi->VertBody = vertBody;

    DPRINTF (LOG_DEBUG, "_intuition: ModifyProp() updated: HorizPot=%u VertPot=%u HorizBody=%u VertBody=%u\n",
             horizPot, vertPot, horizBody, vertBody);

    /* TODO: Refresh the gadget visual (requires gadget rendering) */
}

VOID _intuition_MoveScreen ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Screen * screen __asm("a0"),
                                                        register WORD dx __asm("d0"),
                                                        register WORD dy __asm("d1"))
{
    LXA_UNIMPLEMENTED("intuition", "MoveScreen", "partial: screen position changes, display does not (Phase 256)");

    DPRINTF (LOG_DEBUG, "_intuition: MoveScreen() screen=0x%08lx dx=%d dy=%d\n", (ULONG)screen, dx, dy);
    
    if (!screen) return;
    
    /* AmigaOS 3.1 reference: screens move vertically; a horizontal delta
     * leaves LeftEdge unchanged (no autoscroll/overscan model yet). */
    (void)dx;
    screen->TopEdge += dy;
    
    /* TODO: Update display hardware/host window if applicable */
}

static VOID _intuition_move_window_impl(struct IntuitionBase *IntuitionBase, struct Window *window,
                                        WORD dx, WORD dy, BOOL notify);

VOID _intuition_MoveWindow ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Window * window __asm("a0"),
                                                        register WORD dx __asm("d0"),
                                                        register WORD dy __asm("d1"))
{
    DPRINTF(LOG_DEBUG, "_intuition: MoveWindow() window=0x%08lx dx=%d dy=%d\n", (ULONG)window, dx, dy);

    _intuition_move_window_impl(IntuitionBase, window, dx, dy, TRUE);
}

static VOID _intuition_move_window_impl(struct IntuitionBase *IntuitionBase, struct Window *window,
                                        WORD dx, WORD dy, BOOL notify)
{
    LONG new_x, new_y;
    BOOL rootless_mode;

    if (!window) return;

    /* Update internal coordinates */
    new_x = window->LeftEdge + dx;
    new_y = window->TopEdge + dy;
    
    /* Update Window structure */
    window->LeftEdge = new_x;
    window->TopEdge = new_y;

    /* Update Layer if present */
    if (window->WLayer && LayersBase)
    {
        _call_MoveLayer(LayersBase, window->WLayer, dx, dy);
    }
    
    /* Check for rootless mode */
    rootless_mode = emucall0(EMU_CALL_INT_GET_ROOTLESS);
    
    if (rootless_mode)
    {
        ULONG window_handle = _intuition_get_host_window_handle((struct LXAIntuitionBase *)IntuitionBase, window);

        if (window_handle)
        {
            /* Update host window - emucall expects absolute coordinates */
            emucall3(EMU_CALL_INT_MOVE_WINDOW, window_handle, (ULONG)new_x, (ULONG)new_y);
        }
    }

    if (notify)
        _post_idcmp_message(window, IDCMP_CHANGEWINDOW, CWCODE_MOVESIZE, 0,
                            window, window->MouseX, window->MouseY);
}

VOID _intuition_OffGadget ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Gadget * gadget __asm("a0"),
                                                        register struct Window * window __asm("a1"),
                                                        register struct Requester * requester __asm("a2"))
{
    DPRINTF (LOG_DEBUG, "_intuition: OffGadget() gad=0x%08lx\n", (ULONG)gadget);
    
    if (gadget)
    {
        gadget->Flags |= GFLG_DISABLED;
        _intuition_RefreshGList(IntuitionBase, gadget, window, requester, 1);
    }
}

VOID _intuition_OffMenu ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Window * window __asm("a0"),
                                                        register UWORD menuNumber __asm("d0"))
{
    struct Menu *menu;
    struct MenuItem *item;
    WORD index;

    DPRINTF (LOG_DEBUG, "_intuition: OffMenu() window=0x%08lx menuNumber=0x%04x\n",
             (ULONG)window, (unsigned)menuNumber);

    if (!window || !window->MenuStrip || MENUNUM(menuNumber) == NOMENU)
        return;

    menu = window->MenuStrip;
    for (index = 0; menu && index < MENUNUM(menuNumber); index++)
        menu = menu->NextMenu;

    if (!menu)
        return;

    if (ITEMNUM(menuNumber) == NOITEM)
    {
        menu->Flags &= ~MENUENABLED;
        return;
    }

    item = menu->FirstItem;
    for (index = 0; item && index < ITEMNUM(menuNumber); index++)
        item = item->NextItem;

    if (!item)
        return;

    if (SUBNUM(menuNumber) != NOSUB && item->SubItem)
    {
        item = item->SubItem;
        for (index = 0; item && index < SUBNUM(menuNumber); index++)
            item = item->NextItem;
    }

    if (item)
        item->Flags &= ~ITEMENABLED;
}

VOID _intuition_OnGadget ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Gadget * gadget __asm("a0"),
                                                        register struct Window * window __asm("a1"),
                                                        register struct Requester * requester __asm("a2"))
{
    DPRINTF (LOG_DEBUG, "_intuition: OnGadget() gad=0x%08lx\n", (ULONG)gadget);
    
    if (gadget)
    {
        gadget->Flags &= ~GFLG_DISABLED;
        _intuition_RefreshGList(IntuitionBase, gadget, window, requester, 1);
    }
}

VOID _intuition_OnMenu ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Window * window __asm("a0"),
                                                        register UWORD menuNumber __asm("d0"))
{
    struct Menu *menu;
    struct MenuItem *item;
    WORD index;

    DPRINTF (LOG_DEBUG, "_intuition: OnMenu() window=0x%08lx menuNumber=0x%04x\n",
             (ULONG)window, (unsigned)menuNumber);

    if (!window || !window->MenuStrip || MENUNUM(menuNumber) == NOMENU)
        return;

    menu = window->MenuStrip;
    for (index = 0; menu && index < MENUNUM(menuNumber); index++)
        menu = menu->NextMenu;

    if (!menu)
        return;

    if (ITEMNUM(menuNumber) == NOITEM)
    {
        menu->Flags |= MENUENABLED;
        return;
    }

    item = menu->FirstItem;
    for (index = 0; item && index < ITEMNUM(menuNumber); index++)
        item = item->NextItem;

    if (!item)
        return;

    if (SUBNUM(menuNumber) != NOSUB && item->SubItem)
    {
        item = item->SubItem;
        for (index = 0; item && index < SUBNUM(menuNumber); index++)
            item = item->NextItem;
    }

    if (item)
        item->Flags |= ITEMENABLED;
}

/*
 * Phase 147a — Set TRUE while OpenWorkBench() is calling
 * _intuition_OpenScreen() to create the Workbench screen, so the
 * EMU_CALL_INT_OPEN_SCREEN site below can tell the host this is the
 * Workbench screen (and therefore must NOT receive its own SDL host
 * window in rootless mode — its windows become native host windows
 * individually instead).
 */
static BOOL g_opening_workbench_screen = FALSE;

/*
 * The screen's PaletteExtra (AmigaOS 3.1, tests/probes/intuition/screenpens):
 * Intuition attaches one to every screen, obtains the pens its DrawInfo
 * uses as shared pens (in pen order), and - unless the screen is PENSHARED
 * (SA_SharePens) - every other pen of the screen exclusively, so that
 * ObtainBestPenA() only shares the DrawInfo pens.  Called once the screen's
 * pens are final.
 */
static void _intuition_setup_palextra(struct Screen *screen)
{
    struct ColorMap *cm = screen->ViewPort.ColorMap;
    const UWORD *pens = _intuition_screen_pens(screen);
    UBYTE used[256];
    ULONG ncol, p;
    int i;

    if (!cm || cm->PalExtra || AttachPalExtra(cm, &screen->ViewPort) || !cm->PalExtra)
        return;
    ncol = 1UL << (screen->BitMap.Depth > 8 ? 8 : screen->BitMap.Depth);
    if (ncol > (ULONG)cm->Count)
        ncol = (ULONG)cm->Count;
    memset(used, 0, sizeof(used));
    for (i = 0; i < NUMDRIPENS; i++)
        if (pens[i] < ncol)
            used[pens[i]] = 1;
    for (p = 0; p < ncol; p++)
        if (used[p])
            ObtainPen(cm, p, 0, 0, 0, PENF_NO_SETCOLOR);
    /* the other pens are taken by count: 3.1 leaves them linked in the
     * (then empty, pe_NFree 0) free list - pe_FirstFree keeps its value */
    if (!(screen->Flags & PENSHARED))
    {
        struct PaletteExtra *pe = cm->PalExtra;

        ObtainSemaphore(&pe->pe_Semaphore);
        for (p = 0; p < ncol; p++)
            if (!used[p])
                ((UWORD *)pe->pe_RefCnt)[p]++;
        pe->pe_NFree = 0;
        ReleaseSemaphore(&pe->pe_Semaphore);
    }
}

struct Screen * _intuition_OpenScreen ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register const struct NewScreen * newScreen __asm("a0"))
{
    LXA_UNIMPLEMENTED("intuition", "OpenScreen", "partial: fixed bar/border sizes, screen font height ignored (Phase 256)");

    struct Screen *screen;
    ULONG display_handle;
    WORD requested_width, requested_height;
    UWORD width, height;
    UBYTE depth;
    UBYTE i;

    DPRINTF (LOG_DEBUG, "_intuition: OpenScreen() newScreen=0x%08lx\n", (ULONG)newScreen);

    if (!newScreen)
    {
        LPRINTF (LOG_ERROR, "_intuition: OpenScreen() called with NULL newScreen\n");
        return NULL;
    }

    struct Screen * _intuition_OpenScreenTagList ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                   register const struct NewScreen * newScreen __asm("a0"),
                                                   register const struct TagItem * tagList __asm("a1"));

    /* ExtNewScreen: the extension tags are processed like
     * OpenScreenTagList(newScreen, Extension) */
    if ((newScreen->Type & NS_EXTENDED) &&
        ((const struct ExtNewScreen *)newScreen)->Extension)
    {
        return _intuition_OpenScreenTagList(IntuitionBase, newScreen,
                                            ((const struct ExtNewScreen *)newScreen)->Extension);
    }

    /* Per RKRM: apps must not call OpenScreen() with Type=WBENCHSCREEN.
     * Only Intuition itself opens the Workbench screen via OpenWorkbench().
     * Return NULL as real Intuition does when an app tries this. */
    if ((newScreen->Type & SCREENTYPE) == WBENCHSCREEN)
    {
        DPRINTF (LOG_DEBUG, "_intuition: OpenScreen() rejecting WBENCHSCREEN request (use LockPubScreen instead)\n");
        return NULL;
    }

    /* Get screen dimensions */
    requested_width = newScreen->Width;
    requested_height = newScreen->Height;
    depth = (UBYTE)newScreen->Depth;

    /*
     * Width/height are WORDs in NewScreen. Keep them signed until after
     * validation so sentinel values such as STDSCREENHEIGHT (-1) and other
     * negative compatibility values do not wrap to huge unsigned sizes.
     */
    /* AmigaOS 3.1 refuses a zero width or height
     * (tests/probes/intuition/screens) */
    if (requested_width == 0 || requested_height == 0)
    {
        DPRINTF (LOG_DEBUG, "_intuition: OpenScreen() zero size %dx%d\n",
                 (int)requested_width, (int)requested_height);
        return NULL;
    }

    /* STDSCREENWIDTH/HEIGHT: the text overscan of the display mode
     * (AmigaOS 3.1: lores 320, hires 640, interlace doubles the height;
     * the overscan preferences change it - Phase 236) */
    {
        struct DimensionInfo dims;
        ULONG mode_id = g_screen_display_id ? g_screen_display_id - 1
                      : (ULONG)(newScreen->ViewModes & (HIRES | LACE));
        BOOL have_dims = (requested_width <= 0 || requested_height <= 0) &&
                         GetDisplayInfoData(NULL, (UBYTE *)&dims, sizeof(dims), DTAG_DIMS, mode_id);

        if (requested_width > 0)
            width = (UWORD)requested_width;
        else if (have_dims)
            width = (UWORD)(dims.TxtOScan.MaxX - dims.TxtOScan.MinX + 1);
        else
            width = (g_screen_display_id && (newScreen->ViewModes & SUPERHIRES)) ? 1280
                  : (newScreen->ViewModes & HIRES) ? 640 : 320;

        if (requested_height > 0)
            height = (UWORD)requested_height;
        else if (have_dims)
            height = (UWORD)(dims.TxtOScan.MaxY - dims.TxtOScan.MinY + 1);
        else
            height = (newScreen->ViewModes & LACE) ? 512 : 256;
    }

    if (depth == 0)
        depth = 2;
    
    /* Small heights are kept as requested (AmigaOS 3.1 reference: a 40 line
     * screen opens 40 lines high; no expansion to the display height). */

    DPRINTF (LOG_DEBUG, "_intuition: OpenScreen() tags=%d req=%d,%d %dx%dx%d ViewModes=0x%04x\n",
             (int)g_screen_from_tags, (int)requested_width, (int)requested_height, (int)width, (int)height, (int)depth, (UWORD)newScreen->ViewModes);

    /* Allocate Screen structure */
    screen = (struct Screen *)AllocMem(sizeof(struct Screen), MEMF_PUBLIC | MEMF_CLEAR);
    if (!screen)
    {
        LPRINTF (LOG_ERROR, "_intuition: OpenScreen() out of memory for Screen\n");
        return NULL;
    }

    /* Open host display via emucall */
    /* Pack width/height into d1, depth into d2, title into d3,
     * flags into d4 (Phase 147a: bit 0 = is_workbench_screen). */
    {
        ULONG screen_flags = g_opening_workbench_screen ? 1UL : 0UL;
        display_handle = emucall4(EMU_CALL_INT_OPEN_SCREEN,
                                  ((ULONG)width << 16) | (ULONG)height,
                                  (ULONG)depth,
                                  (ULONG)newScreen->DefaultTitle,
                                  screen_flags);
    }

    if (display_handle == 0)
    {
        LPRINTF (LOG_ERROR, "_intuition: OpenScreen() host display_open failed\n");
        FreeMem(screen, sizeof(struct Screen));
        return NULL;
    }

    /* Store display handle in ExtData (we'll use this to identify the screen) */
    screen->ExtData = (UBYTE *)display_handle;

    /* Initialize screen fields */
    screen->LeftEdge = newScreen->LeftEdge;
    screen->TopEdge = newScreen->TopEdge;
    screen->Width = width;
    screen->Height = height;
    /* NS_EXTENDED only describes the NewScreen structure (AmigaOS 3.1
     * reference: dopus-startup) */
    screen->Flags = newScreen->Type & ~NS_EXTENDED;
    /* OpenScreen() always shows the title bar (AmigaOS 3.1); only
     * SA_ShowTitle FALSE hides it */
    if (!g_screen_from_tags)
        screen->Flags |= SHOWTITLE;
    screen->Title = newScreen->DefaultTitle;
    screen->DefaultTitle = newScreen->DefaultTitle;
    screen->DetailPen = newScreen->DetailPen;
    screen->BlockPen = newScreen->BlockPen;

    /* Initialize the embedded BitMap */
    InitBitMap(&screen->BitMap, depth, width, height);

    /* Allocate bitplanes */
    for (i = 0; i < depth; i++)
    {
        screen->BitMap.Planes[i] = AllocRaster(width, height);
        if (!screen->BitMap.Planes[i])
        {
            /* Cleanup on failure */
            LPRINTF (LOG_ERROR, "_intuition: OpenScreen() out of memory for plane %d\n", (int)i);
            while (i > 0)
            {
                i--;
                FreeRaster(screen->BitMap.Planes[i], width, height);
            }
            emucall1(EMU_CALL_INT_CLOSE_SCREEN, display_handle);
            FreeMem(screen, sizeof(struct Screen));
            return NULL;
        }
    }

    /* Initialize the embedded RastPort */
    InitRastPort(&screen->RastPort);
    screen->RastPort.BitMap = &screen->BitMap;

    /* Tell the host where the screen's bitmap lives so it can auto-refresh
     * from planar RAM at VBlank time.
     * Pack bpr and depth into single parameter: (bpr << 16) | depth
     */
    ULONG bpr_depth = ((ULONG)screen->BitMap.BytesPerRow << 16) | (ULONG)depth;
    emucall3(EMU_CALL_INT_SET_SCREEN_BITMAP, display_handle,
             (ULONG)&screen->BitMap.Planes[0], bpr_depth);

    /* Initialize ViewPort (minimal) */
    screen->ViewPort.DWidth = width;
    screen->ViewPort.DHeight = height;
    {
        /* On real Amiga, a 640-wide screen requires HIRES (ViewModes & HIRES).
         * Some apps (e.g. PPaint) open a 640-wide screen with ViewModes=0x0000
         * and then read screen->ViewPort.Modes to determine the display class.
         * Apply the same auto-correction that real Intuition performs: if the
         * physical width is >= 640, set the HIRES bit. */
        /* AmigaOS 3.1 keeps the requested mode: a 640 pixel wide screen
         * with ViewModes 0 is a (wide) lores screen
         * (tests/probes/intuition/screens) */
        UWORD adjModes = newScreen->ViewModes;
        /* a plain NewScreen cannot ask for SUPERHIRES: 3.1 drops the bit
         * (HIRES|SUPERHIRES is a hires screen, SysInfo opens one;
         * tests/probes/intuition/screens) */
        if (!g_screen_display_id)
            adjModes &= ~SUPERHIRES;
        /* AmigaOS 3.1 screens always have SPRITES set */
        screen->ViewPort.Modes = adjModes | SPRITES;
        /* a screen opened behind the others is hidden */
        if (newScreen->Type & SCREENBEHIND)
            screen->ViewPort.Modes |= VP_HIDE;
    }
    
    /* Allocate and initialize RasInfo for the ViewPort.
     * This is required for double-buffering and other ViewPort operations.
     * Without a valid RasInfo, code like screen->ViewPort.RasInfo->BitMap = xxx
     * would write to address 4 (NULL + 4) and corrupt SysBase!
     */
    struct RasInfo *rasinfo = (struct RasInfo *)AllocMem(sizeof(struct RasInfo), MEMF_PUBLIC | MEMF_CLEAR);
    if (rasinfo)
    {
        rasinfo->Next = NULL;
        rasinfo->BitMap = &screen->BitMap;
        rasinfo->RxOffset = 0;
        rasinfo->RyOffset = 0;
    }
    screen->ViewPort.RasInfo = rasinfo;

    /* Allocate and initialize ColorMap for the ViewPort.
     * This is required for GetRGB4(), SetRGB4(), and other color operations.
     * The number of entries is 2^depth (e.g., 4 for 2-bit depth, 32 for 5-bit depth).
     */
    /* AmigaOS 3.1: at least 32 entries (the sprite colours 16-31 included),
     * whatever the depth (tests/probes/intuition/screenpens) */
    ULONG num_colors = 1UL << depth;
    if (num_colors < 32)
        num_colors = 32;
    screen->ViewPort.ColorMap = GetColorMap(num_colors);
    if (screen->ViewPort.ColorMap)
    {
        /* attached to the screen ViewPort, as by VTAG_ATTACH_CM_SET:
         * VideoControl() changes then happen immediately (reference) */
        screen->ViewPort.ColorMap->cm_vp = &screen->ViewPort;
    }
    if (screen->ViewPort.ColorMap)
    {
        /* AmigaOS 3.1 reference: GetVPModeID() of a screen is the mode the
         * application asked for, without a monitor ID unless it asked for one
         * (Workbench and a ViewModes HIRES screen: 0x8000; SA_DisplayID
         * overrides this in OpenScreenTagList()). */
        {
            ULONG modeKey = screen->ViewPort.Modes &
                            (HIRES | SUPERHIRES | LACE | HAM | EXTRA_HALFBRITE);
            screen->ViewPort.ColorMap->VPModeID = modeKey;
        }
        /* The palette: the ColorMap's defaults with the preferences colours
         * (AmigaOS 3.1 reference: gallery-prefs-*).  The screen is not
         * linked into IntuitionBase yet, so the host palette is set
         * directly. */
        {
            ULONG c, rgb[3];

            _intuition_apply_pref_colors(screen, g_opening_workbench_screen || g_screen_full_palette);
            for (c = 0; c < num_colors && c < 256; c++)
            {
                GetRGB32(screen->ViewPort.ColorMap, c, 1, rgb);
                emucall3(EMU_CALL_GFX_SET_COLOR, display_handle, c,
                         ((rgb[0] >> 8) & 0xff0000) | ((rgb[1] >> 16) & 0xff00) | (rgb[2] >> 24));
            }
        }
        _intuition_update_display_clip(screen);
    }

    /* The screen font: NewScreen.Font / SA_Font, else the Workbench and
     * SA_SysFont 1 screens get the screen font of the preferences and all
     * others the system default font GfxBase->DefaultFont (AmigaOS 3.1
     * reference: gallery-prefs-fontpal). */
    {
        struct TextAttr *ta = NULL;
        struct TextFont *deffont = NULL;

        if (newScreen->Font)
        {
            ta = newScreen->Font;
        }
        else
        {
            struct LXAIntuitionBase *ibase = (struct LXAIntuitionBase *)IntuitionBase;

            if (g_opening_workbench_screen || g_screen_sysfont == 1)
                deffont = ibase->ScreenFont;
            else
                deffont = GfxBase->DefaultFont;
            ta = (struct TextAttr *)AllocMem(sizeof(struct TextAttr), MEMF_PUBLIC | MEMF_CLEAR);
            if (ta)
            {
                if (deffont)
                {
                    ta->ta_Name = (STRPTR)deffont->tf_Message.mn_Node.ln_Name;
                    ta->ta_YSize = deffont->tf_YSize;
                    ta->ta_Style = deffont->tf_Style;
                    ta->ta_Flags = deffont->tf_Flags;
                }
                else
                {
                    ta->ta_Name = (STRPTR)"topaz.font";
                    ta->ta_YSize = 8;
                }
            }
        }
        screen->Font = ta;
    }

    /* Bar height: font height + 2 (AmigaOS 3.1 reference: 10 for topaz 8,
     * 13 for topaz 11) */
    screen->BarHeight = ((screen->Font && screen->Font->ta_YSize > 0)
                         ? screen->Font->ta_YSize : 8) + 2;
    screen->BarVBorder = 1;
    /* AmigaOS 3.1 reference: 5 on hires screens, 2 on lores screens */
    screen->BarHBorder = (screen->ViewPort.Modes & (HIRES | SUPERHIRES)) ? 5 : 2;
    /* AmigaOS 3.1 reference: WBorTop is the border above the window title
     * (the title bar height is WBorTop + font height + 1), MenuHBorder 4 and
     * MenuVBorder 2 on hires, 4 on lores screens. */
    screen->WBorTop = 2;
    screen->MenuHBorder = 4;
    screen->MenuVBorder = (screen->BarHBorder == 5) ? 2 : 4;
    if (screen->BarHBorder == 5)
        screen->Flags |= SCREENHIRES;
    screen->WBorLeft = 4;
    screen->WBorRight = 4;
    screen->WBorBottom = 2;


    /* Clear the screen to color 0 */
    SetRast(&screen->RastPort, 0);

    /* Open the screen font and set it on the screen's RastPort.
     * On real Amiga, OpenScreen always sets screen->RastPort.Font to an
     * opened TextFont. Apps like PPaint check RastPort.Font != NULL as a
     * screen validity check and exit if it is NULL. */
    {
        struct TextFont *screenFont = NULL;
        if (screen->Font)
        {
            screenFont = OpenFont(screen->Font);
        }
        if (!screenFont)
        {
            /* Fall back to topaz.font if no font specified or open failed */
            struct TextAttr ta;
            ta.ta_Name  = (STRPTR)"topaz.font";
            ta.ta_YSize = 8;
            ta.ta_Style = 0;
            ta.ta_Flags = 0;
            screenFont = OpenFont(&ta);
        }
        if (screenFont)
        {
            SetFont(&screen->RastPort, screenFont);
        }
    }


    /* Initialize Layer_Info for this screen */
    InitLayers(&screen->LayerInfo);
    screen->LayerInfo.top_layer = NULL;

    /*
     * Create the BarLayer — a LAYERSIMPLE|LAYERBACKDROP layer that spans
     * the full screen width and covers the title bar area (0..BarHeight).
     * Real AmigaOS always creates this; applications (e.g. PPaint) read
     * screen->BarLayer->rp to obtain a RastPort for the screen bar area.
     * Without it, BarLayer is NULL and dereferencing ->rp reads garbage
     * from the exception vector table at address 0x0C.
     */
    if (LayersBase)
    {
        screen->BarLayer = CreateUpfrontHookLayer(
            &screen->LayerInfo,
            &screen->BitMap,
            0,                            /* x0 */
            0,                            /* y0 */
            screen->Width - 1,            /* x1 */
            screen->BarHeight,            /* y1 (inclusive) */
            LAYERSIMPLE | LAYERBACKDROP,  /* flags */
            LAYERS_NOBACKFILL,            /* backfill hook */
            NULL);                        /* shape hook */

        if (screen->BarLayer)
        {
            /* Set the screen's font on the BarLayer RastPort so that
             * apps reading BarLayer->rp->Font get the correct font. */
            if (screen->Font)
            {
                struct TextFont *tf = OpenFont(screen->Font);
                if (tf)
                {
                    SetFont(screen->BarLayer->rp, tf);
                }
            }
        }
    }

    /* Link screen into IntuitionBase screen list (at front) */
    screen->NextScreen = IntuitionBase->FirstScreen;
    IntuitionBase->FirstScreen = screen;
    /* ... and its ViewPort into Intuition's View (ViewAddress()): newest
     * screen first, the order does not follow depth arrangement (AmigaOS
     * 3.1, probe intuition/viewaddress; Fish EOMS reads ViewPort->ColorMap) */
    screen->ViewPort.Next = IntuitionBase->ViewLord.ViewPort;
    IntuitionBase->ViewLord.ViewPort = &screen->ViewPort;
    _intuition_register_pubscreen(IntuitionBase, screen);
    {
        struct PubScreenNode *pub = _intuition_find_pubscreen_by_screen(
            (struct LXAIntuitionBase *)IntuitionBase, screen);
        if (pub)
            ((struct LXAPubScreenNode *)pub)->default_font = newScreen->Font ? FALSE : TRUE;
    }

    _create_screen_sys_gadgets(screen);

    /* OpenScreenTagList() does this once SA_Pens/SA_SharePens are applied */
    if (!g_screen_from_tags)
        _intuition_setup_palextra(screen);

    if (screen->Flags & SHOWTITLE)
        _render_screen_title_bar(screen);

    DPRINTF(LOG_DEBUG, "[ROM] OpenScreen: IntuitionBase=0x%08lx, FirstScreen set to 0x%08lx\n",
            (ULONG)IntuitionBase, (ULONG)screen);
    
    /* If this is the first screen, make it the active screen */
    if (!IntuitionBase->ActiveScreen)
    {
        IntuitionBase->ActiveScreen = screen;
    }

    DPRINTF (LOG_DEBUG, "_intuition: OpenScreen() -> 0x%08lx, display_handle=0x%08lx\n",
             (ULONG)screen, display_handle);

    return screen;
}

APTR _intuition_NewObjectA ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                             register struct IClass * classPtr __asm("a0"),
                             register CONST_STRPTR classID __asm("a1"),
                             register const struct TagItem * tagList __asm("a2"));
VOID _intuition_DisposeObject ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                register APTR object __asm("a0"));
static void _render_window_user_gadgets(struct Window *window);

/*
 * Window chrome (Phase 223).
 *
 * Geometry, gadget structures and pixels follow real AmigaOS 3.1
 * (tests/scenarios/gallery-chrome.yaml):
 *  - the title bar is WBorTop + font height + 1 high;
 *  - system gadgets are sysiclass images in GTYP_SYSGADGET|GTYP_CUSTOMGADGET
 *    gadgets that come first in the gadget list, in the order depth, zoom,
 *    sizing, close, drag bar;
 *  - a sizeable window with a depth gadget always gets a zoom gadget;
 *  - the sizing gadget widens the right border unless WFLG_SIZEBBOTTOM is set
 *    alone;
 *  - the active window fills its border with FILLPEN, the title in
 *    FILLTEXTPEN; inactive windows use BACKGROUNDPEN/TEXTPEN.
 */

static const UWORD *_intuition_screen_pens(struct Screen *screen)
{
    struct PubScreenNode *pub = _intuition_find_pubscreen_by_screen(
        (struct LXAIntuitionBase *)IntuitionBase, screen);

    if (pub)
        return ((struct LXAPubScreenNode *)pub)->pens;
    return lxa_default_pens();
}

static UWORD _screen_sysi_size(struct Screen *screen)
{
    return (screen && !(screen->Flags & SCREENHIRES)) ? SYSISIZE_LOWRES : SYSISIZE_MEDRES;
}

static WORD _screen_font_height(struct Screen *screen)
{
    return (screen && screen->Font && screen->Font->ta_YSize > 0) ? screen->Font->ta_YSize : 8;
}

static BOOL _window_has_title_bar(ULONG flags, const UBYTE *title)
{
    return title != NULL ||
           (flags & (WFLG_DRAGBAR | WFLG_CLOSEGADGET | WFLG_DEPTHGADGET | WFLG_HASZOOM)) != 0;
}

/* AmigaOS 3.1: a sizeable window with a depth gadget always gets a zoom gadget */
static ULONG _window_effective_flags(ULONG flags)
{
    if ((flags & WFLG_SIZEGADGET) && (flags & WFLG_DEPTHGADGET))
        flags |= WFLG_HASZOOM;
    return flags;
}

static void _window_compute_borders(struct Screen *screen, ULONG flags, const UBYTE *title,
                                    WORD *left, WORD *top, WORD *right, WORD *bottom)
{
    WORD l, t, r, b;
    BOOL titlebar = _window_has_title_bar(flags, title);

    if (flags & WFLG_BORDERLESS)
    {
        l = r = b = 0;
        t = titlebar ? _screen_font_height(screen) + 1 : 0;
    }
    else
    {
        l = screen->WBorLeft;
        r = screen->WBorRight;
        b = screen->WBorBottom;
        t = screen->WBorTop + (titlebar ? _screen_font_height(screen) + 1 : 0);
    }

    /* the sizing gadget widens the border, borderless or not (AmigaOS
     * 3.1, probe dos/conwindow: a borderless CON: window has 0/9/18/0) */
    {
        if (flags & WFLG_SIZEGADGET)
        {
            UWORD sw = 18, sh = 10;

            lxa_sysi_dims(SIZEIMAGE, _screen_sysi_size(screen), &sw, &sh);
            if (flags & WFLG_SIZEBBOTTOM)
            {
                if (b < (WORD)sh)
                    b = sh;
            }
            if ((flags & WFLG_SIZEBRIGHT) || !(flags & WFLG_SIZEBBOTTOM))
            {
                if (r < (WORD)sw)
                    r = sw;
            }
        }
    }
    *left = l;
    *top = t;
    *right = r;
    *bottom = b;
}

/* sysgadgets carry a BOOPSI header with o_Class == NULL, so OCLASS() is
 * valid but never dispatched; the image is a real sysiclass object */
static struct Hook g_sysgadget_hook;

static struct Gadget *_create_sys_gadget(struct Window *window, UWORD systype,
                                         WORD left, WORD top, WORD width, WORD height,
                                         UWORD flags, UWORD activation, LONG which)
{
    UBYTE *mem;
    struct Gadget *gad;

    mem = (UBYTE *)AllocMem(sizeof(struct _Object) + sizeof(struct ExtGadget), MEMF_PUBLIC | MEMF_CLEAR);
    if (!mem)
        return NULL;
    gad = (struct Gadget *)(mem + sizeof(struct _Object));
    /* AmigaOS 3.1: system gadgets are gadget-help capable custom gadgets
     * whose MutualExclude holds their dispatcher hook */
    ((struct ExtGadget *)gad)->MoreFlags = GMORE_GADGETHELP;
    gad->MutualExclude = (ULONG)&g_sysgadget_hook;

    gad->LeftEdge = left;
    gad->TopEdge = top;
    gad->Width = width;
    gad->Height = height;
    gad->Flags = flags;
    gad->Activation = activation;
    gad->GadgetType = GTYP_SYSGADGET | systype | GTYP_CUSTOMGADGET;
    if (window->Flags & WFLG_GIMMEZEROZERO)
        gad->GadgetType |= GTYP_GZZGADGET;
    gad->UserData = (APTR)window;

    if (which >= 0)
    {
        struct TagItem tags[] = {
            { SYSIA_Which, (ULONG)which },
            { SYSIA_Size, _screen_sysi_size(window->WScreen) },
            { TAG_DONE, 0 }
        };
        gad->GadgetRender = _intuition_NewObjectA(IntuitionBase, NULL, (CONST_STRPTR)SYSICLASS, tags);
    }
    return gad;
}

static void _free_sys_gadget(struct Gadget *gad)
{
    if (!gad)
        return;
    if (gad->GadgetRender)
        _intuition_DisposeObject(IntuitionBase, gad->GadgetRender);
    FreeMem((UBYTE *)gad - sizeof(struct _Object), sizeof(struct _Object) + sizeof(struct ExtGadget));
}

static BOOL _is_sys_gadget(const struct Gadget *gad)
{
    return gad && (gad->GadgetType & GTYP_SYSGADGET) &&
           (gad->GadgetType & GTYP_GTYPEMASK) == GTYP_CUSTOMGADGET &&
           OCLASS((Object *)gad) == NULL;
}

/*
 * Create the system gadgets and put them in front of the window's gadget
 * list (AmigaOS 3.1 order: depth, zoom, sizing, close, drag bar).
 */
static void _create_window_sys_gadgets(struct Window *window)
{
    struct Gadget *list[5];
    WORD n = 0, i;
    UWORD size = _screen_sysi_size(window->WScreen);
    UWORD dw = 24, zw = 24, sw = 18, sh = 10, cw = 20, h, gh;
    WORD right_left = 0;
    BOOL borderless = (window->Flags & WFLG_BORDERLESS) != 0;

    lxa_sysi_dims(DEPTHIMAGE, size, &dw, NULL);
    lxa_sysi_dims(ZOOMIMAGE, size, &zw, NULL);
    lxa_sysi_dims(SIZEIMAGE, size, &sw, &sh);
    lxa_sysi_dims(CLOSEIMAGE, size, &cw, NULL);
    h = window->BorderTop;
    /* a borderless window keeps its system gadgets; depth, zoom and close
     * are as high as on a bordered window, the drag bar spans the
     * (thinner) title bar (AmigaOS 3.1, probe dos/conwindow) */
    gh = borderless ? window->WScreen->WBorTop + _screen_font_height(window->WScreen) + 1 : h;

    if (window->Flags & WFLG_DEPTHGADGET)
    {
        right_left = -(WORD)(dw - 2);
        list[n] = _create_sys_gadget(window, GTYP_WDEPTH, right_left, 0, dw, gh,
                                     GFLG_EXTENDED | GFLG_RELRIGHT | GFLG_GADGIMAGE,
                                     GACT_BORDERSNIFF | GACT_RELVERIFY, DEPTHIMAGE);
        if (list[n]) n++;
    }
    if (window->Flags & WFLG_HASZOOM)
    {
        right_left = right_left ? right_left - (WORD)(zw - 1) : -(WORD)(zw - 2);
        list[n] = _create_sys_gadget(window, GTYP_WZOOM, right_left, 0, zw, gh,
                                     GFLG_EXTENDED | GFLG_RELRIGHT | GFLG_GADGIMAGE,
                                     GACT_BORDERSNIFF | GACT_RELVERIFY, ZOOMIMAGE);
        if (list[n]) n++;
    }
    if (window->Flags & WFLG_SIZEGADGET)
    {
        list[n] = _create_sys_gadget(window, GTYP_SIZING, -(WORD)(sw - 1), -(WORD)(sh - 1), sw, sh,
                                     GFLG_EXTENDED | GFLG_RELRIGHT | GFLG_RELBOTTOM | GFLG_GADGIMAGE,
                                     GACT_BORDERSNIFF | GACT_RELVERIFY, SIZEIMAGE);
        if (list[n]) n++;
    }
    if (window->Flags & WFLG_CLOSEGADGET)
    {
        list[n] = _create_sys_gadget(window, GTYP_CLOSE, 0, 0, cw, gh,
                                     GFLG_EXTENDED | GFLG_GADGIMAGE,
                                     GACT_BORDERSNIFF | GACT_RELVERIFY, CLOSEIMAGE);
        if (list[n]) n++;
    }
    if (_window_has_title_bar(window->Flags, window->Title) || (window->Flags & WFLG_DRAGBAR))
    {
        if (window->Flags & WFLG_DRAGBAR)
        {
            list[n] = _create_sys_gadget(window, GTYP_WDRAGGING, 0, 0, 0, h - 1,
                                         GFLG_EXTENDED | GFLG_RELWIDTH | GFLG_GADGIMAGE,
                                         GACT_BORDERSNIFF, -1);
            if (list[n]) n++;
        }
    }

    for (i = n - 1; i >= 0; i--)
    {
        list[i]->NextGadget = window->FirstGadget;
        window->FirstGadget = list[i];
    }
}

/* The screen's depth and drag gadgets (AmigaOS 3.1: Screen->FirstGadget) */
static void _create_screen_sys_gadgets(struct Screen *screen)
{
    UBYTE *mem;
    struct Gadget *depth, *drag;
    UWORD dw = 24, size = _screen_sysi_size(screen);
    WORD h = screen->BarHeight + 1;

    lxa_sysi_dims(DEPTHIMAGE, size, &dw, NULL);
    mem = (UBYTE *)AllocMem(2 * (sizeof(struct _Object) + sizeof(struct ExtGadget)), MEMF_PUBLIC | MEMF_CLEAR);
    if (!mem)
        return;
    depth = (struct Gadget *)(mem + sizeof(struct _Object));
    drag = (struct Gadget *)(mem + 2 * sizeof(struct _Object) + sizeof(struct ExtGadget));

    depth->LeftEdge = -(WORD)(dw - 2);
    depth->Width = dw - 1;
    depth->Height = h;
    depth->Flags = GFLG_EXTENDED | GFLG_RELRIGHT | GFLG_GADGIMAGE;
    depth->Activation = GACT_RELVERIFY;
    depth->GadgetType = GTYP_SYSGADGET | GTYP_SCRGADGET | GTYP_SDEPTH | GTYP_CUSTOMGADGET;
    if (!(screen->Flags & SCREENQUIET))
    {
        struct TagItem tags[] = {
            { SYSIA_Which, DEPTHIMAGE },
            { SYSIA_Size, size },
            { TAG_DONE, 0 }
        };
        depth->GadgetRender = _intuition_NewObjectA(IntuitionBase, NULL, (CONST_STRPTR)SYSICLASS, tags);
    }
    drag->Height = h;
    drag->Flags = GFLG_EXTENDED | GFLG_RELWIDTH | GFLG_GADGIMAGE;
    drag->GadgetType = GTYP_SYSGADGET | GTYP_SCRGADGET | GTYP_SDRAGGING | GTYP_CUSTOMGADGET;
    ((struct ExtGadget *)depth)->MoreFlags = GMORE_GADGETHELP;
    ((struct ExtGadget *)drag)->MoreFlags = GMORE_GADGETHELP;
    depth->MutualExclude = (ULONG)&g_sysgadget_hook;
    drag->MutualExclude = (ULONG)&g_sysgadget_hook;
    depth->UserData = (APTR)screen;
    drag->UserData = (APTR)screen;
    depth->NextGadget = drag;
    drag->NextGadget = screen->FirstGadget;
    screen->FirstGadget = depth;
}

static void _free_screen_sys_gadgets(struct Screen *screen)
{
    struct Gadget *depth = screen->FirstGadget;

    if (!depth || !(depth->GadgetType & GTYP_SCRGADGET) ||
        (depth->GadgetType & GTYP_SYSTYPEMASK) != GTYP_SDEPTH || depth->UserData != (APTR)screen)
        return;
    screen->FirstGadget = depth->NextGadget ? depth->NextGadget->NextGadget : NULL;
    if (depth->GadgetRender)
        _intuition_DisposeObject(IntuitionBase, depth->GadgetRender);
    FreeMem((UBYTE *)depth - sizeof(struct _Object), 2 * (sizeof(struct _Object) + sizeof(struct ExtGadget)));
}

/* free the system gadgets (they are always the first gadgets of the list) */
static void _free_window_sys_gadgets(struct Window *window)
{
    struct Gadget **pp = &window->FirstGadget;

    while (*pp)
    {
        struct Gadget *gad = *pp;
        if (_is_sys_gadget(gad))
        {
            *pp = gad->NextGadget;
            _free_sys_gadget(gad);
        }
        else
            pp = &gad->NextGadget;
    }
}

static ULONG _window_image_state(struct Window *window, struct Gadget *gad)
{
    BOOL active = (window->Flags & WFLG_WINDOWACTIVE) != 0;

    if (gad && (gad->Flags & GFLG_SELECTED))
        return active ? IDS_SELECTED : IDS_INACTIVESELECTED;
    return active ? IDS_NORMAL : IDS_INACTIVENORMAL;
}

static void _render_sys_gadget(struct Window *window, struct Gadget *gad)
{
    struct RastPort *rp;
    LONG x, y, w, h;
    struct Image *im;

    if (!window || !gad || !gad->GadgetRender)
        return;
    rp = window->BorderRPort ? window->BorderRPort : window->RPort;
    if (!rp)
        return;
    im = (struct Image *)gad->GadgetRender;
    _calculate_gadget_box(window, NULL, gad, &x, &y, &w, &h);
    {
        /* the screen's pens (preferences pens on the Workbench, Phase 236) */
        struct DrawInfo dri;

        memset(&dri, 0, sizeof(dri));
        dri.dri_Version = 2;
        dri.dri_NumPens = NUMDRIPENS;
        dri.dri_Pens = (UWORD *)_intuition_screen_pens(window->WScreen);
        dri.dri_Font = window->WScreen ? window->WScreen->RastPort.Font : NULL;
        dri.dri_Depth = window->WScreen ? window->WScreen->BitMap.Depth : 2;
        dri.dri_Flags = DRIF_NEWLOOK;
        {
            /* old-look screens keep the built-in imagery (Phase 252) */
            struct PubScreenNode *pub = window->WScreen ? _intuition_find_pubscreen_by_screen(
                (struct LXAIntuitionBase *)IntuitionBase, window->WScreen) : NULL;
            BOOL newlook = pub && (((struct LXAPubScreenNode *)pub)->drawInfo.dri_Flags & DRIF_NEWLOOK);

            _intuition_DrawImageState(IntuitionBase, rp, im, (WORD)x, (WORD)y,
                                      _window_image_state(window, gad), newlook ? &dri : NULL);
        }
    }
}

#define LXA_FRAME_NO_GADGETS      0
#define LXA_FRAME_BORDER_GADGETS  1
#define LXA_FRAME_ALL_GADGETS     2
static void _render_window_frame_impl(struct Window *window, UBYTE user_gadgets);
static void _render_window_user_gadgets_ex(struct Window *window, BOOL border_only);

/*
 * Intuition renders the frame without leaving traces in the window's
 * RastPort: on AmigaOS 3.1 a window's RPort keeps its InitRastPort()
 * attributes (FgPen -1, BgPen 0, JAM2) - verified in Phase 220.
 */
static void _render_window_frame_ex(struct Window *window, UBYTE user_gadgets)
{
    struct RastPort *rp = window ? (window->BorderRPort ? window->BorderRPort : window->RPort) : NULL;
    UBYTE fg, bg, ol, dm;

    if (!rp)
    {
        _render_window_frame_impl(window, user_gadgets);
        return;
    }

    fg = rp->FgPen;
    bg = rp->BgPen;
    ol = rp->AOlPen;
    dm = rp->DrawMode;
    _render_window_frame_impl(window, user_gadgets);
    SetAPen(rp, fg);
    SetBPen(rp, bg);
    rp->AOlPen = ol;    /* not SetOPen(): that also sets AREAOUTLINE */
    SetDrMd(rp, dm);
}

static void _render_window_frame(struct Window *window)
{
    _render_window_frame_ex(window, LXA_FRAME_ALL_GADGETS);
}

static void _render_window_frame_impl(struct Window *window, UBYTE user_gadgets)
{
    struct RastPort *rp;
    const UWORD *pens;
    struct Gadget *gad;
    BOOL active;
    WORD W, H, bl, bt, br, bb;
    WORD title_left, title_right;
    UWORD fill;
    UBYTE oldpen, olddm;

    if (!window || (!window->RPort && !window->BorderRPort))
        return;
    rp = window->BorderRPort ? window->BorderRPort : window->RPort;
    pens = _intuition_screen_pens(window->WScreen);
    active = (window->Flags & WFLG_WINDOWACTIVE) != 0;
    fill = active ? pens[FILLPEN] : pens[BACKGROUNDPEN];
    W = window->Width;
    H = window->Height;
    bl = window->BorderLeft;
    bt = window->BorderTop;
    br = window->BorderRight;
    bb = window->BorderBottom;
    oldpen = rp->FgPen;
    olddm = rp->DrawMode;

    /* extent of the title text area: between the close gadget and the
     * leftmost gadget on the right */
    title_left = 1;
    title_right = W - 2;
    for (gad = window->FirstGadget; gad; gad = gad->NextGadget)
    {
        LONG gx, gy, gw, gh;
        UWORD st;

        if (!_is_sys_gadget(gad))
            continue;
        st = gad->GadgetType & GTYP_SYSTYPEMASK;
        _calculate_gadget_box(window, NULL, gad, &gx, &gy, &gw, &gh);
        if (st == GTYP_CLOSE)
            title_left = gx + gw;
        else if (st == GTYP_WDEPTH || st == GTYP_WZOOM)
        {
            if (gx - 2 < title_right)
                title_right = gx - 2;
        }
    }

    if (!(window->Flags & WFLG_BORDERLESS))
    {
        /* border fill */
        SetAPen(rp, fill);
        if (bt > 2)
            RectFill(rp, title_left, 1, title_right, bt - 2);
        if (bl > 2)
            RectFill(rp, 1, 1, bl - 2, H - 2);
        if (br > 2)
            RectFill(rp, W - br + 1, bt > 0 ? bt - 1 : 1, W - 2, H - 2);
        if (bb > 2)
            RectFill(rp, 1, H - bb + 1, W - 2, H - 2);

        /* outer edge: shine top/left, shadow right/bottom */
        SetAPen(rp, pens[SHINEPEN]);
        RectFill(rp, 0, 0, W - 1, 0);
        RectFill(rp, 0, 0, 0, H - 1);
        SetAPen(rp, pens[SHADOWPEN]);
        RectFill(rp, W - 1, 1, W - 1, H - 1);
        RectFill(rp, 1, H - 1, W - 1, H - 1);

        /* inner edge around the contents: shadow top/left, shine right/bottom */
        if (bt > 0 && bl > 0)
        {
            SetAPen(rp, pens[SHADOWPEN]);
            RectFill(rp, bl - 1, bt - 1, W - br, bt - 1);
            RectFill(rp, bl - 1, bt - 1, bl - 1, H - bb);
            SetAPen(rp, pens[SHINEPEN]);
            RectFill(rp, W - br, bt, W - br, H - bb);
            RectFill(rp, bl, H - bb, W - br, H - bb);
        }

        /* separator left of the right title bar gadgets */
        if (title_right < W - 2 && bt > 2)
        {
            SetAPen(rp, pens[SHADOWPEN]);
            RectFill(rp, title_right + 1, 1, title_right + 1, bt - 2);
        }
    }

    /* title */
    if (window->Title && bt > 0)
    {
        /* 3.1 reference: the title starts 10 px right of the close
         * gadget and keeps 2 px clear of the separator (gallery-chrome) */
        WORD tx = (title_left > 1) ? title_left + 10 : 4;
        WORD avail = title_right - tx - 1;
        WORD len = strlen((const char *)window->Title);
        WORD fit;
        /* the title is in the screen font (window->IFont), which may
         * differ from the window's own font (Phase 236) */
        struct TextFont *rpfont = rp->Font;

        if (window->IFont && window->IFont != rpfont)
            SetFont(rp, window->IFont);
        fit = (avail > 0) ? lxa_text_fit(rp, (STRPTR)window->Title, len, avail) : 0;
        if (fit > 0)
        {
            SetAPen(rp, active ? pens[FILLTEXTPEN] : pens[TEXTPEN]);
            SetDrMd(rp, JAM1);
            Move(rp, tx, 1 + rp->TxBaseline);
            Text(rp, (STRPTR)window->Title, fit);
        }
        if (rpfont && rp->Font != rpfont)
            SetFont(rp, rpfont);
    }

    /* system gadget imagery */
    for (gad = window->FirstGadget; gad; gad = gad->NextGadget)
    {
        if (_is_sys_gadget(gad) && gad->GadgetRender)
            _render_sys_gadget(window, gad);
    }

    SetAPen(rp, oldpen);
    SetDrMd(rp, olddm);

    /* Render user gadgets after the frame/system gadgets. */
    if (user_gadgets != LXA_FRAME_NO_GADGETS)
        _render_window_user_gadgets_ex(window, user_gadgets == LXA_FRAME_BORDER_GADGETS);
}

/*
 * Render n gadgets starting at first, last one first: AmigaOS 3.1 draws a
 * gadget list back to front, so the first gadget ends up on top
 * (tests/probes/intuition/gadinfo).  border_only: only GACT_*BORDER
 * gadgets (window activation, tests/probes/intuition/gadinfo); sys_too:
 * system gadgets are drawn as well.
 */
static void _render_gadget_range_reverse(struct Window *window, struct Requester *req,
                                         struct Gadget *first, WORD n,
                                         BOOL border_only, BOOL sys_too)
{
    WORD count = 0, i, k;
    struct Gadget *gad;

    for (gad = first; gad && (n == -1 || count < n); gad = gad->NextGadget)
        count++;
    for (i = count - 1; i >= 0; i--)
    {
        gad = first;
        for (k = 0; k < i; k++)
            gad = gad->NextGadget;
        if (!sys_too && (gad->GadgetType & GTYP_SYSGADGET))
            continue;
        if (border_only &&
            !(gad->Activation & (GACT_RIGHTBORDER | GACT_LEFTBORDER | GACT_TOPBORDER | GACT_BOTTOMBORDER)))
            continue;
        _render_gadget(window, req, gad);
    }
}

static void _render_window_user_gadgets_ex(struct Window *window, BOOL border_only)
{
    if (!window)
        return;

    _gadtools_RefreshPass(TRUE);
    _render_gadget_range_reverse(window, NULL, window->FirstGadget, -1, border_only, FALSE);
    _gadtools_RefreshPass(FALSE);
}

static void _render_window_user_gadgets(struct Window *window)
{
    _render_window_user_gadgets_ex(window, FALSE);
}

/* Phase 147a: Decide whether a window should get its own native host SDL
 * window, or render into its screen's bitmap.
 *
 *  - rootless mode + window opens on Workbench screen: native host (no host
 *    window for the WB screen itself, each window is its own X11 window).
 *  - rootless mode + window opens on a custom screen: NO native host (the
 *    custom screen owns one host window; the child window renders inside).
 *  - non-rootless mode: NO native host (everything renders into the screen
 *    host window).
 */
static BOOL _window_uses_native_host(struct Screen *screen, BOOL rootless_mode)
{
    if (!rootless_mode || !screen)
        return FALSE;
    return ((screen->Flags & SCREENTYPE) == WBENCHSCREEN);
}

struct Window * _intuition_OpenWindow ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register const struct NewWindow * newWindow __asm("a0"))
{
    LXA_UNIMPLEMENTED("intuition", "OpenWindow", "partial: title bar height ignores screen font height (Phase 256)");

    struct Window *window;
    struct Screen *screen;
    struct Layer *border_layer = NULL;
    struct Layer *content_layer = NULL;
    WORD width, height;
    WORD requested_width, requested_height;
    ULONG rootless_mode;
    ULONG host_window_handle = 0;

    DPRINTF (LOG_DEBUG, "_intuition: OpenWindow() newWindow=0x%08lx\n", (ULONG)newWindow);

    if (!newWindow)
    {
        LPRINTF (LOG_ERROR, "_intuition: OpenWindow() called with NULL newWindow\n");
        return NULL;
    }

    if ((newWindow->Flags & WFLG_NW_EXTENDED) != 0)
    {
        const struct ExtNewWindow *ext_new_window = (const struct ExtNewWindow *)newWindow;
        const struct TagItem *extension = ext_new_window->Extension;

        if (extension != NULL && (((ULONG)extension & 1) == 0) && TypeOfMem((APTR)extension) != 0)
            return _intuition_OpenWindowTagList(IntuitionBase,
                                                newWindow,
                                                extension);

        if (extension != NULL)
        {
            LPRINTF(LOG_WARNING,
                    "_intuition: OpenWindow() ignoring invalid NW_EXTENDED taglist 0x%08lx\n",
                    (ULONG)extension);
        }
    }

    /* Debug dump of NewWindow structure for KP2 investigation */
    DPRINTF (LOG_DEBUG, "_intuition: OpenWindow() NewWindow dump:\n");
    U_hexdump(LOG_DEBUG, (void *)newWindow, 48);
    DPRINTF (LOG_DEBUG, "  LeftEdge=%d TopEdge=%d Width=%d Height=%d\n",
             (int)newWindow->LeftEdge, (int)newWindow->TopEdge,
             (int)newWindow->Width, (int)newWindow->Height);
    DPRINTF (LOG_DEBUG, "  DetailPen=%d BlockPen=%d\n",
             (int)newWindow->DetailPen, (int)newWindow->BlockPen);
    DPRINTF (LOG_DEBUG, "  IDCMPFlags=0x%08lx Flags=0x%08lx\n",
             (ULONG)newWindow->IDCMPFlags, (ULONG)newWindow->Flags);
    DPRINTF (LOG_DEBUG, "  FirstGadget=0x%08lx CheckMark=0x%08lx\n",
             (ULONG)newWindow->FirstGadget, (ULONG)newWindow->CheckMark);
    DPRINTF (LOG_DEBUG, "  Title=0x%08lx Screen=0x%08lx BitMap=0x%08lx\n",
             (ULONG)newWindow->Title, (ULONG)newWindow->Screen, (ULONG)newWindow->BitMap);
    DPRINTF (LOG_DEBUG, "  MinWidth=%d MinHeight=%d MaxWidth=%u MaxHeight=%u Type=%u\n",
             (int)newWindow->MinWidth, (int)newWindow->MinHeight,
             (unsigned)newWindow->MaxWidth, (unsigned)newWindow->MaxHeight,
             (unsigned)newWindow->Type);
    if (newWindow->Title)
    {
        DPRINTF (LOG_DEBUG, "  Title string: '%s'\n", (char *)newWindow->Title);
    }

    /* Get the target screen */
    screen = newWindow->Screen;
    if (!screen)
    {
        /* No screen specified - use the Workbench screen (open it if needed) */
        DPRINTF (LOG_DEBUG, "_intuition: OpenWindow() no screen specified, using Workbench\n");
        
        /* Try to find existing Workbench screen */
        screen = IntuitionBase->FirstScreen;
        while (screen)
        {
            if ((screen->Flags & SCREENTYPE) == WBENCHSCREEN)
                break;
            screen = screen->NextScreen;
        }
        
        /* If no Workbench screen, open one */
        if (!screen)
        {
            DPRINTF (LOG_DEBUG, "_intuition: OpenWindow() opening Workbench screen (called from OpenWindow)\n");
            if (!_intuition_OpenWorkBench(IntuitionBase))
            {
                LPRINTF (LOG_ERROR, "_intuition: OpenWindow() failed to open Workbench screen\n");
                return NULL;
            }
            /* Find the newly created Workbench screen */
            screen = IntuitionBase->FirstScreen;
            while (screen)
            {
                if ((screen->Flags & SCREENTYPE) == WBENCHSCREEN)
                    break;
                screen = screen->NextScreen;
            }
        }
        
        if (!screen)
        {
            LPRINTF (LOG_ERROR, "_intuition: OpenWindow() Workbench screen not found after creation\n");
            return NULL;
        }
    }

    /* Calculate window dimensions */
    requested_width = newWindow->Width;
    requested_height = newWindow->Height;
    width = requested_width;
    height = requested_height;

    /*
     * Width/height are signed WORDs. Keep them signed while validating so
     * compatibility sentinels and other negative values do not wrap into huge
     * unsigned host sizes when the tracking window is created.
     */
    {
        WORD max_width = screen->Width;
        WORD max_height = screen->Height;

        if (newWindow->LeftEdge > 0 && newWindow->LeftEdge < screen->Width)
            max_width = screen->Width - newWindow->LeftEdge;
        if (newWindow->TopEdge > 0 && newWindow->TopEdge < screen->Height)
            max_height = screen->Height - newWindow->TopEdge;

        if (requested_width <= 0)
            width = max_width;
        if (requested_height <= 0)
            height = max_height;

        if (width <= 0 || width > max_width)
            width = max_width;
        if (height <= 0 || height > max_height)
            height = max_height;

        if (requested_width != width || requested_height != height)
        {
            DPRINTF(LOG_DEBUG,
                    "_intuition: OpenWindow() sanitized %dx%d to %dx%d on screen %dx%d at (%d,%d)\n",
                    (int)requested_width,
                    (int)requested_height,
                    (int)width,
                    (int)height,
                    (int)screen->Width,
                    (int)screen->Height,
                    (int)newWindow->LeftEdge,
                    (int)newWindow->TopEdge);
        }

    }
    

    DPRINTF (LOG_DEBUG, "_intuition: OpenWindow() %dx%d at (%d,%d) flags=0x%08lx\n",
             (int)width, (int)height, (int)newWindow->LeftEdge, (int)newWindow->TopEdge,
             (ULONG)newWindow->Flags);

    /* Allocate Window structure */
    window = (struct Window *)AllocMem(sizeof(struct Window), MEMF_PUBLIC | MEMF_CLEAR);
    if (!window)
    {
        LPRINTF (LOG_ERROR, "_intuition: OpenWindow() out of memory for Window\n");
        return NULL;
    }

    /* Initialize window fields — clamp position to screen bounds.
     * Some apps (e.g. BlitzBasic2) pass garbage sentinel values in
     * LeftEdge/TopEdge; the host side uses these to compute the tracked
     * window extent, so out-of-range values cause oversized host windows.
     *
     * Phase 135: When the requested position would place the window
     * largely off-screen, CENTER the window instead of clipping it to a
     * single pixel at the screen corner.  Old apps (BlitzBasic2 in
     * particular) hand us garbage LE/TE for popup-style windows and
     * expect Intuition to "do something reasonable" — clipping the
     * window to (639,255) on a 640x256 screen meant the user could
     * never click it and the app's main loop, which waits on that
     * window's UserPort, hung forever.
     */
    {
        WORD clampedLeft = newWindow->LeftEdge;
        WORD clampedTop  = newWindow->TopEdge;
        BOOL recenter = FALSE;

        /* If the requested position would put the window mostly or fully
         * off the visible screen area, recentre it. */
        if (clampedLeft < 0 || clampedLeft + (WORD)width > screen->Width ||
            clampedTop  < 0 || clampedTop  + (WORD)height > screen->Height)
        {
            recenter = TRUE;
        }

        if (recenter)
        {
            if ((WORD)width  < screen->Width)
                clampedLeft = (screen->Width  - (WORD)width)  / 2;
            else
                clampedLeft = 0;
            if ((WORD)height < screen->Height)
                clampedTop  = (screen->Height - (WORD)height) / 2;
            else
                clampedTop  = 0;
        }

        if (clampedLeft != newWindow->LeftEdge || clampedTop != newWindow->TopEdge)
        {
            DPRINTF(LOG_DEBUG,
                    "_intuition: OpenWindow() repositioned (%d,%d) to (%d,%d) for %dx%d on screen %dx%d (recenter=%d)\n",
                    (int)newWindow->LeftEdge, (int)newWindow->TopEdge,
                    (int)clampedLeft, (int)clampedTop,
                    (int)width, (int)height,
                    (int)screen->Width, (int)screen->Height,
                    (int)recenter);
        }

        window->LeftEdge = clampedLeft;
        window->TopEdge  = clampedTop;
    }
    window->Width = width;
    window->Height = height;
    window->MinWidth = newWindow->MinWidth ? newWindow->MinWidth : width;
    window->MinHeight = newWindow->MinHeight ? newWindow->MinHeight : height;
    /* AmigaOS 3.1 reference: 0 means "the window's initial size" */
    window->MaxWidth = newWindow->MaxWidth ? newWindow->MaxWidth : width;
    window->MaxHeight = newWindow->MaxHeight ? newWindow->MaxHeight : height;
    /* the limits always include the initial size (AmigaOS 3.1) */
    if (window->MinWidth > width) window->MinWidth = width;
    if (window->MinHeight > height) window->MinHeight = height;
    if ((UWORD)window->MaxWidth < (UWORD)width) window->MaxWidth = width;
    if ((UWORD)window->MaxHeight < (UWORD)height) window->MaxHeight = height;
    window->Flags = newWindow->Flags;
    window->IDCMPFlags = newWindow->IDCMPFlags;
    window->Title = newWindow->Title;
    window->WScreen = screen;
    
    /* Handle DetailPen and BlockPen: 0xFF (~0) means use screen defaults (per RKRM) */
    window->DetailPen = (newWindow->DetailPen == 0xFF) ? screen->DetailPen : newWindow->DetailPen;
    window->BlockPen = (newWindow->BlockPen == 0xFF) ? screen->BlockPen : newWindow->BlockPen;
    
    window->FirstGadget = newWindow->FirstGadget;
    window->CheckMark = newWindow->CheckMark;

    /* Set up border dimensions based on flags (AmigaOS 3.1 rules) */
    window->Flags = _window_effective_flags(window->Flags);
    {
        struct PubScreenNode *pub = _intuition_find_pubscreen_by_screen(
            (struct LXAIntuitionBase *)IntuitionBase, screen);
        /* AmigaOS 3.1 reference (dopus-startup, gadtoolsgadgets): a window
         * opened on a public screen through the public screen mechanism
         * (WBENCHSCREEN/PUBLICSCREEN type, WA_PubScreen[Name]) is a
         * visitor; a CUSTOMSCREEN window on the opener's own screen is not */
        if (pub && ((struct LXAPubScreenNode *)pub)->is_public &&
            (!newWindow->Screen ||
             (newWindow->Type & SCREENTYPE) != CUSTOMSCREEN))
            window->Flags |= WFLG_VISITOR;
    }
    {
        WORD bl, bt, br, bb;

        _window_compute_borders(screen, window->Flags, window->Title, &bl, &bt, &br, &bb);
        window->BorderLeft = bl;
        window->BorderTop = bt;
        window->BorderRight = br;
        window->BorderBottom = bb;
    }

    /* AmigaOS 3.1 sets the inner size for every window, GZZ or not */
    window->GZZWidth = window->Width - window->BorderLeft - window->BorderRight;
    window->GZZHeight = window->Height - window->BorderTop - window->BorderBottom;
    /* without WA_ScreenTitle a window shows the screen's default title */
    if (!window->ScreenTitle)
        window->ScreenTitle = screen->DefaultTitle;

    /* Create the window layer(s). */
    {
        LONG layer_flags;
        LONG x0 = window->LeftEdge;
        LONG y0 = window->TopEdge;
        LONG x1 = window->LeftEdge + width - 1;
        LONG y1 = window->TopEdge + height - 1;

        if (window->Flags & WFLG_SIMPLE_REFRESH)
            layer_flags = LAYERSIMPLE;
        else if ((window->Flags & WFLG_SUPER_BITMAP) && newWindow->BitMap)
            layer_flags = LAYERSUPER;
        else
            layer_flags = LAYERSMART;

        if (window->Flags & WFLG_BACKDROP)
            layer_flags |= LAYERBACKDROP;

        if (window->Flags & WFLG_GIMMEZEROZERO)
        {
            struct TagItem content_tags[] = {
                { LA_BackfillHook, (ULONG)LAYERS_BACKFILL },
                { (layer_flags & LAYERSUPER) ? LA_SuperBitMap : TAG_IGNORE, (ULONG)newWindow->BitMap },
                { LA_WindowPtr, (ULONG)window },
                { TAG_DONE, 0 }
            };
            LONG inner_x0 = x0 + window->BorderLeft;
            LONG inner_y0 = y0 + window->BorderTop;
            LONG inner_x1 = inner_x0 + window->GZZWidth - 1;
            LONG inner_y1 = inner_y0 + window->GZZHeight - 1;

            border_layer = CreateLayerTagList(&screen->LayerInfo, &screen->BitMap,
                                              x0, y0, x1, y1,
                                              LAYERSIMPLE | (layer_flags & LAYERBACKDROP),
                                              NULL);
            if (border_layer)
            {
                content_layer = CreateLayerTagList(&screen->LayerInfo, &screen->BitMap,
                                                   inner_x0, inner_y0, inner_x1, inner_y1,
                                                   layer_flags,
                                                   content_tags);
                if (!content_layer)
                {
                    DeleteLayer(0, border_layer);
                    border_layer = NULL;
                }
            }
        }
        else
        {
            struct TagItem content_tags[] = {
                { LA_BackfillHook, (ULONG)LAYERS_BACKFILL },
                { (layer_flags & LAYERSUPER) ? LA_SuperBitMap : TAG_IGNORE, (ULONG)newWindow->BitMap },
                { LA_WindowPtr, (ULONG)window },
                { TAG_DONE, 0 }
            };

            content_layer = CreateLayerTagList(&screen->LayerInfo, &screen->BitMap,
                                               x0, y0, x1, y1,
                                               layer_flags,
                                               content_tags);
        }

        if (content_layer)
        {
            window->WLayer = content_layer;
            window->RPort = content_layer->rp;
            /* windows render in the screen font when the screen was
             * opened with one, else in the system default font - also on
             * the Workbench and SA_SysFont 1 screens, whose font is the
             * screen font of the preferences (AmigaOS 3.1 reference:
             * gallery-menus-topaz11, gallery-prefs-fontpal) */
            if (screen->RastPort.Font)
            {
                struct PubScreenNode *pub = _intuition_find_pubscreen_by_screen(
                    (struct LXAIntuitionBase *)IntuitionBase, screen);
                struct TextFont *wfont = screen->RastPort.Font;

                if (pub && ((struct LXAPubScreenNode *)pub)->default_font && GfxBase->DefaultFont)
                    wfont = GfxBase->DefaultFont;
                SetFont(window->RPort, wfont);
                if (border_layer)
                    SetFont(border_layer->rp, screen->RastPort.Font);
                window->IFont = screen->RastPort.Font;
            }
            /* only GZZ windows have a separate border RastPort (3.1) */
            window->BorderRPort = border_layer ? border_layer->rp : NULL;
            DPRINTF(LOG_DEBUG,
                    "_intuition: OpenWindow() created border_layer=0x%08lx content_layer=0x%08lx rp=0x%08lx bounds=[%ld,%ld]-[%ld,%ld]\n",
                    (ULONG)border_layer,
                    (ULONG)content_layer,
                    (ULONG)content_layer->rp,
                    x0,
                    y0,
                    x1,
                    y1);
        }
        else
        {
            /* Fallback to screen's RastPort if layer creation fails */
            DPRINTF(LOG_WARNING, "_intuition: OpenWindow() layer creation failed, using screen RastPort\n");
            window->RPort = &screen->RastPort;
            window->BorderRPort = &screen->RastPort;
            window->WLayer = NULL;
        }
    }

    /* Check if rootless mode is enabled */
    rootless_mode = emucall0(EMU_CALL_INT_GET_ROOTLESS);

    {
        /* Always register window with host for tracking (window count, info queries).
         * In rootless mode this also creates a separate host window with pixel buffer.
         * In non-rootless mode the slot is used for tracking only. */
        ULONG window_handle;
        ULONG screen_handle = (ULONG)screen->ExtData;
        ULONG window_flags = _window_uses_native_host(screen, rootless_mode) ? 1UL : 0UL;

        window_handle = emucall5(EMU_CALL_INT_OPEN_WINDOW,
                                 screen_handle,
                                 ((ULONG)(WORD)window->LeftEdge << 16) | ((ULONG)(WORD)window->TopEdge & 0xFFFF),
                                 ((ULONG)width << 16) | ((ULONG)height & 0xFFFF),
                                 (ULONG)newWindow->Title,
                                 window_flags);

        if (window_handle == 0)
        {
            LPRINTF (LOG_WARNING, "_intuition: OpenWindow() window registration failed, continuing without\n");
        }

        struct LXAWindowState *state = _intuition_ensure_window_state((struct LXAIntuitionBase *)IntuitionBase,
                                                                      window);

        if (!state)
        {
            if (window_handle)
                emucall1(EMU_CALL_INT_CLOSE_WINDOW, window_handle);
            FreeMem(window, sizeof(struct Window));
            return NULL;
        }

        state->host_window_handle = window_handle;
        state->open_left = window->LeftEdge;
        state->open_top = window->TopEdge;
        state->prev_active = (newWindow->Flags & WFLG_ACTIVATE) ? IntuitionBase->ActiveWindow : NULL;
        host_window_handle = window_handle;

        if (window_handle)
            emucall2(EMU_CALL_INT_ATTACH_WINDOW, window_handle, (ULONG)window);

        DPRINTF (LOG_DEBUG, "_intuition: OpenWindow() window_handle=0x%08lx rootless=%d\n",
                 window_handle, (int)rootless_mode);
    }

    /* Create IDCMP message ports if IDCMP flags are set */
    if (newWindow->IDCMPFlags)
    {
        if (!_ensure_window_idcmp_ports(window))
        {
            LPRINTF (LOG_ERROR, "_intuition: OpenWindow() failed to create IDCMP port\n");
            if (host_window_handle)
            {
                emucall1(EMU_CALL_INT_CLOSE_WINDOW, host_window_handle);
            }
            FreeMem(window, sizeof(struct Window));
            return NULL;
        }
    }

    /* Link window into screen's window list */
    window->NextWindow = screen->FirstWindow;
    screen->FirstWindow = window;

    /* Activate window if requested */
    if (newWindow->Flags & WFLG_ACTIVATE)
    {
        struct Window *prevActive = IntuitionBase->ActiveWindow;
        if (prevActive && prevActive != window)
        {
            prevActive->Flags &= ~WFLG_WINDOWACTIVE;
            _render_window_frame_ex(prevActive, LXA_FRAME_BORDER_GADGETS);
            _post_idcmp_message(prevActive, IDCMP_INACTIVEWINDOW, 0, 0,
                                prevActive, 0, 0);
        }
        IntuitionBase->ActiveWindow = window;
        IntuitionBase->ActiveScreen = window->WScreen;  /* screen of the active window */
        window->Flags |= WFLG_WINDOWACTIVE;

        if (window->IDCMPFlags & IDCMP_ACTIVEWINDOW)
        {
            _post_idcmp_message(window, IDCMP_ACTIVEWINDOW, 0, 0,
                                window, 0, 0);
        }
    }

    window->FirstGadget = _intuition_public_gadget_list(window->FirstGadget);

    /* Create system gadgets based on window flags (in front of the list) */
    _create_window_sys_gadgets(window);

    /* Clear the window interior to pen 0 before rendering gadgets/chrome.
     * Real AmigaOS Intuition always clears a new window's background — this
     * prevents ghost pixels from previously-rendered content at the same
     * screen coordinates from showing through (Phase 150 backfill fix).
     * We use RectFill on the full window area (including borders) at pen 0
     * so even the border region starts clean; _render_window_frame will
     * overdraw it correctly afterwards.  It comes before GM_LAYOUT: a
     * BOOPSI gadget may draw while it lays itself out (BGUI's groups do). */
    if (window->RPort)
    {
        UBYTE save_fg = window->RPort->FgPen;

        SetAPen(window->RPort, 0);
        RectFill(window->RPort, window->LeftEdge, window->TopEdge,
                 window->LeftEdge + window->Width - 1,
                 window->TopEdge  + window->Height - 1);
        SetAPen(window->RPort, save_fg);
    }

    /* Initialize string gadget NumChars for user gadgets (per RKRM, Intuition does this) */
    {
        struct Gadget *gad = window->FirstGadget;
        while (gad)
        {
            _init_string_gadget_info(gad);
            _sniff_border_gadget(window, NULL, gad);
            _layout_custom_gadget(window, NULL, gad, TRUE);
            gad = gad->NextGadget;
        }
    }
    
    /* Render initial visuals.
     * Windows that use a native host SDL window let the host render the
     * outer frame; we still draw user gadgets into the backing bitmap.
     * All other windows (custom screens, non-rootless) need the full
     * Amiga chrome rendered into the screen bitmap. */
    if (!_window_uses_native_host(screen, rootless_mode))
    {
        _render_window_frame(window);
    }
    else
    {
        _render_window_user_gadgets(window);
    }

    /* No initial IDCMP_REFRESHWINDOW and no LAYERREFRESH: a new window has
     * no damage on AmigaOS 3.1, smart or simple refresh
     * (tests/probes/intuition/openrefresh) */

    DPRINTF (LOG_DEBUG, "_intuition: OpenWindow() -> 0x%08lx\n", (ULONG)window);

    return window;
}

ULONG _intuition_OpenWorkBench ( register struct IntuitionBase * IntuitionBase __asm("a6"))
{
    struct Screen *wbscreen;
    struct NewScreen ns;
    
    DPRINTF (LOG_DEBUG, "_intuition: OpenWorkBench() called, FirstScreen=0x%08lx\n", 
             (ULONG)IntuitionBase->FirstScreen);
    
    /* Check if Workbench screen already exists */
    wbscreen = IntuitionBase->FirstScreen;
    while (wbscreen)
    {
        if ((wbscreen->Flags & SCREENTYPE) == WBENCHSCREEN)
        {
            DPRINTF (LOG_DEBUG, "_intuition: OpenWorkBench() - Workbench already open at 0x%08lx\n", (ULONG)wbscreen);
            return (ULONG)wbscreen;
        }
        wbscreen = wbscreen->NextScreen;
    }
    
    /* Create a new Workbench screen.
     * Note: We use CUSTOMSCREEN here as the internal type because
     * OpenScreen() rejects WBENCHSCREEN from public callers.
     * We set Flags to WBENCHSCREEN after creation so the screen
     * is properly identified as the Workbench screen. */
    /* The screen mode preferences choose mode, size (~0: the mode's text
     * overscan) and depth; without them the Workbench is a 4 colour hires
     * screen of the text overscan size (AmigaOS 3.1 reference:
     * gallery-prefs-sys). */
    struct LXAIntuitionBase *ibase = (struct LXAIntuitionBase *)IntuitionBase;
    memset(&ns, 0, sizeof(ns));
    ns.LeftEdge = 0;
    ns.TopEdge = 0;
    ns.Width = STDSCREENWIDTH;
    ns.Height = STDSCREENHEIGHT;
    ns.Depth = 2;
    ns.DetailPen = 0;
    ns.BlockPen = 1;
    ns.Type = CUSTOMSCREEN;
    ns.ViewModes = HIRES;           /* the Workbench is a hires screen */
    ns.DefaultTitle = (UBYTE *)"Workbench Screen";
    if (ibase->HasScreenModePrefs)
    {
        const struct LxaIScreenModePrefs *sm = &ibase->ScreenModePrefs;

        ns.ViewModes = (UWORD)(sm->smp_DisplayID & 0xffff);
        if (sm->smp_Width != (UWORD)~0 && sm->smp_Width)
            ns.Width = (WORD)sm->smp_Width;
        if (sm->smp_Height != (UWORD)~0 && sm->smp_Height)
            ns.Height = (WORD)sm->smp_Height;
        if (sm->smp_Depth)
            ns.Depth = sm->smp_Depth;
        if (sm->smp_Control & 1)        /* SMF_AUTOSCROLL */
            ns.Type |= AUTOSCROLL;
        g_screen_display_id = sm->smp_DisplayID + 1;
    }

    /* Phase 147a: tell the EMU_CALL_INT_OPEN_SCREEN site that this is
     * the Workbench screen so the host can suppress the screen's own
     * SDL window in rootless mode. */
    g_opening_workbench_screen = TRUE;
    wbscreen = _intuition_OpenScreen(IntuitionBase, &ns);
    g_opening_workbench_screen = FALSE;
    g_screen_display_id = 0;
    if (wbscreen && ibase->HasScreenModePrefs && wbscreen->ViewPort.ColorMap)
        wbscreen->ViewPort.ColorMap->VPModeID = ibase->ScreenModePrefs.smp_DisplayID;
    
    if (wbscreen)
    {
        struct LXAIntuitionBase *base = (struct LXAIntuitionBase *)IntuitionBase;
        struct PubScreenNode *stale_pub;

        /* Mark as Workbench screen (reference flags: WBENCHSCREEN |
         * SHOWTITLE | SCREENHIRES | PENSHARED) */
        wbscreen->Flags = (wbscreen->Flags & ~SCREENTYPE) | WBENCHSCREEN | SHOWTITLE | PENSHARED;

        /* OpenScreen registered the pubscreen entry while the screen was
         * still flagged CUSTOMSCREEN, so the entry was named after the
         * DefaultTitle ("Workbench Screen") instead of the canonical
         * "Workbench" required by LockPubScreen("Workbench"). Drop the
         * stale entry and re-register so the WBENCHSCREEN flag drives
         * the name. */
        stale_pub = _intuition_find_pubscreen_by_screen(base, wbscreen);
        if (stale_pub)
            _intuition_unregister_pubscreen(IntuitionBase, wbscreen);
        _intuition_register_pubscreen(IntuitionBase, wbscreen);
        stale_pub = _intuition_find_pubscreen_by_screen(base, wbscreen);
        if (stale_pub)
            ((struct LXAPubScreenNode *)stale_pub)->default_font = TRUE;
        _render_screen_title_bar(wbscreen);

        DPRINTF (LOG_DEBUG, "_intuition: OpenWorkBench() - opened at 0x%08lx, Width=%d Height=%d\n", 
                 (ULONG)wbscreen, (int)wbscreen->Width, (int)wbscreen->Height);

        return (ULONG)wbscreen;
    }
    
    LPRINTF (LOG_ERROR, "_intuition: OpenWorkBench() failed to create screen\n");
    return 0;
}

VOID _intuition_PrintIText ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct RastPort * rp __asm("a0"),
                                                        register const struct IntuiText * iText __asm("a1"),
                                                        register WORD left __asm("d0"),
                                                        register WORD top __asm("d1"))
{
    LXA_UNIMPLEMENTED("intuition", "PrintIText", "partial: ignores IntuiText ITextFont, uses RastPort font (Phase 256)");

    DPRINTF (LOG_DEBUG, "_intuition: PrintIText() rp=0x%08lx iText=0x%08lx at %d,%d\n",
             (ULONG)rp, (ULONG)iText, (int)left, (int)top);
    
    if (!rp || !iText)
        return;
    
    /* Walk the IntuiText chain and render each text */
    while (iText)
    {
        if (iText->IText)
        {
            BYTE oldAPen = rp->FgPen;
            BYTE oldBPen = rp->BgPen;
            BYTE oldDrMd = rp->DrawMode;
            
            /* Set colors and drawmode from IntuiText */
            SetAPen(rp, iText->FrontPen);
            SetBPen(rp, iText->BackPen);
            SetDrMd(rp, iText->DrawMode);
            
            /* Calculate position */
            WORD x = left + iText->LeftEdge;
            WORD y = top + iText->TopEdge;
            
            DPRINTF (LOG_DEBUG, "_intuition: PrintIText() text='%s' leftEdge=%d topEdge=%d -> x=%d y=%d\n",
                     (const char *)iText->IText, (int)iText->LeftEdge, (int)iText->TopEdge, (int)x, (int)y);
            
            /* If a font is specified, try to use it */
            /* For now, just use the rastport's current font */
            
            /* Move to position and render text.
             * TopEdge is the top of the character cell (per RKRM),
             * but Text() renders at the baseline.  Add tf_Baseline
             * to convert cell-top to baseline coordinate. */
            {
                WORD baseline = rp->Font ? rp->Font->tf_Baseline : 6;
                Move(rp, x, y + baseline);
            }
            
            /* Calculate text length and render */
            STRPTR txt = iText->IText;
            UWORD len = 0;
            while (txt[len]) len++;
            
            if (len > 0)
            {
                Text(rp, txt, len);
            }
            
            /* Restore original colors/mode */
            SetAPen(rp, oldAPen);
            SetBPen(rp, oldBPen);
            SetDrMd(rp, oldDrMd);
        }
        
        iText = iText->NextText;
    }
}

VOID _intuition_RefreshGadgets ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Gadget * gadgets __asm("a0"),
                                                        register struct Window * window __asm("a1"),
                                                        register struct Requester * requester __asm("a2"))
{
    /* RefreshGadgets() refreshes all gadgets from 'gadgets' to the end of the list.
     * Per RKRM: "This routine refreshes (redraws) the imagery of every gadget in
     * the gadget list starting from and including the specified gadget."
     * Implemented by calling RefreshGList with numGad=-1 (all gadgets).
     */
    DPRINTF (LOG_DEBUG, "_intuition: RefreshGadgets(gadgets=%p, window=%p, req=%p)\n",
             gadgets, window, requester);
    
    _intuition_RefreshGList(IntuitionBase, gadgets, window, requester, -1);
}

UWORD _intuition_RemoveGadget ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Window * window __asm("a0"),
                                                        register struct Gadget * gadget __asm("a1"))
{
    /* Remove a single gadget from a window's gadget list.
     * Returns the ordinal position or 0xFFFF if not found.
     * Based on AROS implementation - simply calls RemoveGList with count=1.
     */
    DPRINTF (LOG_DEBUG, "_intuition: RemoveGadget(window=%p, gadget=%p)\n", window, gadget);
    
    if (!window || !gadget)
        return 0xFFFF;
    
    /* Find the gadget's position first */
    struct Gadget *curr = window->FirstGadget;
    UWORD pos = 0;
    
    while (curr && curr != gadget)
    {
        pos++;
        curr = curr->NextGadget;
    }
    
    if (!curr)
        return 0xFFFF;  /* Gadget not found */
    
    /* Remove it using RemoveGList */
    _intuition_RemoveGList(IntuitionBase, window, gadget, 1);
    
    return pos;
}

VOID _intuition_ReportMouse ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register BOOL flag __asm("d0"),
                                                        register struct Window * window __asm("a0"))
{
    /* Enable or disable REPORTMOUSE flag for a window.
     * When enabled, window receives mouse movement IDCMP events.
     * Based on AROS implementation.
     * Note: Arguments are twisted (flag in D0, window in A0).
     */
    DPRINTF (LOG_DEBUG, "_intuition: ReportMouse(flag=%d, window=%p)\n", (int)flag, window);
    
    if (!window)
        return;
    
    if (flag)
        window->Flags |= WFLG_REPORTMOUSE;
    else
        window->Flags &= ~WFLG_REPORTMOUSE;
}

/* Helper to render a requester */
static void _render_requester(struct Window *window, struct Requester *req)
{
    struct RastPort *rp;
    LONG left, top, width, height;
    
    if (!window || !req || !window->RPort) return;
    
    rp = window->RPort;
    
    _calculate_requester_box(window, req, &left, &top, &width, &height);
    /* the requester renders into its own layer (AmigaOS 3.1) */
    if (req->ReqLayer && req->ReqLayer != window->WLayer && req->ReqLayer->rp)
    {
        rp = req->ReqLayer->rp;
        left = 0;
        top = 0;
    }

    /* Draw background */
    if (!(req->Flags & NOREQBACKFILL))
    {
        SetAPen(rp, req->BackFill);
        RectFill(rp, left, top, left + width - 1, top + height - 1);
    }
    
    /* Draw Border if present */
    if (req->ReqBorder)
        _intuition_DrawBorder(IntuitionBase, rp, req->ReqBorder, left, top);
    
    /* Draw Text if present */
    if (req->ReqText)
        _intuition_PrintIText(IntuitionBase, rp, req->ReqText, left, top);

    if ((req->Flags & USEREQIMAGE) && req->ReqImage)
        _intuition_DrawImage(IntuitionBase, rp, req->ReqImage, left, top);
    
    /* Draw Gadgets */
    if (req->ReqGadget)
        _intuition_RefreshGList(IntuitionBase, req->ReqGadget, window, req, -1);
}

BOOL _intuition_Request ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Requester * requester __asm("a0"),
                                                        register struct Window * window __asm("a1"))
{
    DPRINTF (LOG_DEBUG, "_intuition: Request() req=0x%08lx win=0x%08lx\n", (ULONG)requester, (ULONG)window);
    
    if (!requester || !window) return FALSE;
    
    /* Link into window's requester list (LIFO) */
    requester->OlderRequest = window->FirstRequest;
    window->FirstRequest = requester;
    
    requester->Flags |= REQACTIVE;
    requester->RWindow = window;
    /* AmigaOS 3.1: a requester is a layer of its own in front of the
     * window, inside the window's bounds */
    requester->ReqLayer = NULL;
    if (window->WScreen && LayersBase)
    {
        LONG l, t, w, h;
        _calculate_requester_box(window, requester, &l, &t, &w, &h);
        if (w > window->Width - l)
            w = window->Width - l;
        if (h > window->Height - t)
            h = window->Height - t;
        if (w > 0 && h > 0)
            requester->ReqLayer = CreateUpfrontLayer(&window->WScreen->LayerInfo,
                                                     &window->WScreen->BitMap,
                                                     window->LeftEdge + l, window->TopEdge + t,
                                                     window->LeftEdge + l + w - 1,
                                                     window->TopEdge + t + h - 1,
                                                     LAYERSMART, NULL);
        if (requester->ReqLayer && window->RPort && window->RPort->Font)
            SetFont(requester->ReqLayer->rp, window->RPort->Font);
    }
    if (!requester->ReqLayer)
        requester->ReqLayer = window->WLayer;
    window->ReqCount++;
    window->Flags |= WFLG_INREQUEST;
    
    /* Render */
    _render_requester(window, requester);

    /* Post IDCMP_REQSET to notify the window that a requester was opened.
     * Per RKRM: one REQSET is sent for each requester opened in the window. */
    _post_idcmp_message(window, IDCMP_REQSET, 0, 0, NULL,
                        window->MouseX, window->MouseY);

    return TRUE;
}

VOID _intuition_ScreenToBack ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Screen * screen __asm("a0"))
{
    LXA_UNIMPLEMENTED("intuition", "ScreenToBack", "partial: reorders screen list, display not updated (Phase 256)");

    struct Screen *curr, *prev = NULL;
    
    DPRINTF (LOG_DEBUG, "_intuition: ScreenToBack() screen=0x%08lx\n", (ULONG)screen);
    
    if (!screen || !IntuitionBase->FirstScreen) return;
    
    /* Find screen in list */
    curr = IntuitionBase->FirstScreen;
    while (curr && curr != screen)
    {
        prev = curr;
        curr = curr->NextScreen;
    }
    
    if (!curr) return; /* Not found */
    
    if (!screen->NextScreen) return; /* Already at back */
    
    /* Unlink */
    if (prev)
    {
        prev->NextScreen = screen->NextScreen;
    }
    else
    {
        /* Was first */
        IntuitionBase->FirstScreen = screen->NextScreen;
    }
    
    /* Find tail */
    curr = IntuitionBase->FirstScreen;
    while (curr->NextScreen)
    {
        curr = curr->NextScreen;
    }
    
    /* Link at tail */
    curr->NextScreen = screen;
    screen->NextScreen = NULL;
    
    /* TODO: RethinkDisplay / Update View */
}

VOID _intuition_ScreenToFront ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Screen * screen __asm("a0"))
{
    LXA_UNIMPLEMENTED("intuition", "ScreenToFront", "partial: reorders screen list, display not updated (Phase 256)");

    struct Screen *curr, *prev = NULL;

    DPRINTF (LOG_DEBUG, "_intuition: ScreenToFront() screen=0x%08lx\n", (ULONG)screen);
    
    if (!screen || !IntuitionBase->FirstScreen) return;

    screen->ViewPort.Modes &= ~VP_HIDE;
    
    if (screen == IntuitionBase->FirstScreen) return; /* Already at front */

    /* Find screen in list */
    curr = IntuitionBase->FirstScreen;
    while (curr && curr != screen)
    {
        prev = curr;
        curr = curr->NextScreen;
    }
    
    if (!curr) return; /* Not found */
    
    /* Unlink */
    if (prev)
    {
        prev->NextScreen = screen->NextScreen;
    }
    
    /* Link at front */
    screen->NextScreen = IntuitionBase->FirstScreen;
    IntuitionBase->FirstScreen = screen;
    
    /* TODO: RethinkDisplay / Update View */
}

BOOL _intuition_SetDMRequest ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Window * window __asm("a0"),
                                                        register struct Requester * requester __asm("a1"))
{
    DPRINTF (LOG_DEBUG, "_intuition: SetDMRequest() window=0x%08lx requester=0x%08lx\n",
             (ULONG)window, (ULONG)requester);

    if (!window)
        return FALSE;

    if (window->DMRequest && (window->DMRequest->Flags & REQACTIVE))
        return FALSE;

    window->DMRequest = requester;
    return TRUE;
}

BOOL _intuition_SetMenuStrip ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Window * window __asm("a0"),
                                                        register struct Menu * menu __asm("a1"))
{
    DPRINTF (LOG_DEBUG, "_intuition: SetMenuStrip() window=0x%08lx menu=0x%08lx\n",
             (ULONG)window, (ULONG)menu);

    if (!window)
    {
        LPRINTF (LOG_ERROR, "_intuition: SetMenuStrip() called with NULL window\n");
        return FALSE;
    }

    window->MenuStrip = menu;

    /* Intuition computes each menu's drop-down box (JazzX/JazzY - BeatX/
     * BeatY, relative to the items' coordinate origin): the items' bounding
     * box extended by 4 pixels left/up and 3 pixels right/down (AmigaOS 3.1
     * reference). */
    for (; menu; menu = menu->NextMenu)
    {
        struct MenuItem *item = menu->FirstItem;
        WORD minx, miny, maxx, maxy;

        if (!item)
            continue;

        minx = item->LeftEdge;
        miny = item->TopEdge;
        maxx = item->LeftEdge + item->Width;
        maxy = item->TopEdge + item->Height;
        for (item = item->NextItem; item; item = item->NextItem)
        {
            if (item->LeftEdge < minx) minx = item->LeftEdge;
            if (item->TopEdge < miny) miny = item->TopEdge;
            if (item->LeftEdge + item->Width > maxx) maxx = item->LeftEdge + item->Width;
            if (item->TopEdge + item->Height > maxy) maxy = item->TopEdge + item->Height;
        }
        menu->JazzX = minx - 4;
        menu->JazzY = miny - 4;
        menu->BeatX = maxx + 3;
        menu->BeatY = maxy + 3;
    }

    return TRUE;
}

VOID _intuition_SetPointer ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Window * window __asm("a0"),
                                                        register UWORD * pointer __asm("a1"),
                                                        register WORD height __asm("d0"),
                                                        register WORD width __asm("d1"),
                                                        register WORD xOffset __asm("d2"),
                                                        register WORD yOffset __asm("d3"))
{
    /*
     * SetPointer() sets a custom mouse pointer image for the window.
     * Since we use the system cursor, this is a no-op.
     */
    DPRINTF (LOG_DEBUG, "_intuition: SetPointer() window=0x%08lx pointer=0x%08lx %dx%d offset=(%d,%d) (no-op)\n",
             (ULONG)window, (ULONG)pointer, width, height, xOffset, yOffset);

    if (!window)
        return;

    window->Pointer = pointer;
    window->PtrHeight = height;
    window->PtrWidth = width;
    window->XOffset = xOffset;
    window->YOffset = yOffset;
}

VOID _intuition_SetWindowTitles ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Window * window __asm("a0"),
                                                        register CONST_STRPTR windowTitle __asm("a1"),
                                                        register CONST_STRPTR screenTitle __asm("a2"))
{
    /*
     * SetWindowTitles() changes the window and/or screen title.
     * A value of (UBYTE *)-1 means "don't change this title".
     */
    DPRINTF (LOG_DEBUG, "_intuition: SetWindowTitles() window=0x%08lx\n", (ULONG)window);

    if (!window)
        return;

    /* Update window title if requested */
    if (windowTitle != (CONST_STRPTR)-1) {
        ULONG handle = _intuition_get_host_window_handle((struct LXAIntuitionBase *)IntuitionBase, window);
        window->Title = (UBYTE *)windowTitle;
        /* AmigaOS 3.1 redraws no application gadget here, not even a
         * border gadget (tests/probes/intuition/gadinfo) */
        _render_window_frame_ex(window, LXA_FRAME_NO_GADGETS);
        /* the host (rootless window, liblxa's tracked title) follows */
        if (handle)
            emucall2(EMU_CALL_INT_SET_TITLE, handle, (ULONG)windowTitle);
    }

    /* Update screen title if requested */
    if (screenTitle != (CONST_STRPTR)-1 && window->WScreen) {
        window->WScreen->Title = (UBYTE *)screenTitle;
        if (window->WScreen->Flags & SHOWTITLE)
            _render_screen_title_bar(window->WScreen);
    }
}

VOID _intuition_ShowTitle ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Screen * screen __asm("a0"),
                                                        register BOOL showIt __asm("d0"))
{
    DPRINTF (LOG_DEBUG, "_intuition: ShowTitle() screen=0x%08lx showIt=%d\n", screen, showIt);

    if (!screen)
        return;

    if (showIt)
        screen->Flags |= SHOWTITLE;
    else
        screen->Flags &= ~SHOWTITLE;

    if (showIt)
        _render_screen_title_bar(screen);
    else
    {
        SetAPen(&screen->RastPort, 0);
        RectFill(&screen->RastPort, 0, 0, screen->Width - 1, screen->BarHeight);
    }
}

VOID _intuition_SizeWindow ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Window * window __asm("a0"),
                                                        register WORD dx __asm("d0"),
                                                        register WORD dy __asm("d1"))
{
    LONG new_w, new_h;
    WORD old_width, old_height;

    DPRINTF(LOG_DEBUG, "_intuition: SizeWindow() window=0x%08lx dx=%d dy=%d\n", (ULONG)window, dx, dy);

    if (!window) return;

    old_width = window->Width;
    old_height = window->Height;

    new_w = window->Width + dx;
    new_h = window->Height + dy;

    /* Enforce limits (AmigaOS 3.1 reference: tests/intuition/window_manipulation) */
    if (new_w < window->MinWidth) new_w = window->MinWidth;
    if ((UWORD)new_w > window->MaxWidth) new_w = window->MaxWidth;
    if (new_h < window->MinHeight) new_h = window->MinHeight;
    if ((UWORD)new_h > window->MaxHeight) new_h = window->MaxHeight;

    /* Recalculate deltas in case limits were hit */
    dx = new_w - window->Width;
    dy = new_h - window->Height;

    if (dx == 0 && dy == 0) return;

    /* Update Window structure */
    window->Width = new_w;
    window->Height = new_h;

    window->GZZWidth = window->Width - window->BorderLeft - window->BorderRight;
    window->GZZHeight = window->Height - window->BorderTop - window->BorderBottom;

    /* Update Layer if present */
    if (window->WLayer && LayersBase)
    {
        _call_SizeLayer(LayersBase, window->WLayer, dx, dy);
    }

    /* Always notify the host so tracked window dimensions stay up to date.
     * In rootless mode this also resizes the host-side SDL2 window;
     * in non-rootless mode it only updates the tracked width/height. */
    {
        ULONG window_handle = _intuition_get_host_window_handle((struct LXAIntuitionBase *)IntuitionBase, window);

        if (window_handle)
        {
            emucall3(EMU_CALL_INT_SIZE_WINDOW, window_handle, (ULONG)new_w, (ULONG)new_h);
        }
    }

    /* GM_LAYOUT (gpl_Initial FALSE) to the window's GREL_ gadgets before
     * the window is redrawn (AmigaOS 3.1, tests/probes/intuition/gadinfo) */
    {
        struct Gadget *gad;

        for (gad = window->FirstGadget; gad; gad = gad->NextGadget)
            _layout_custom_gadget(window, NULL, gad, FALSE);
    }

    _clear_relative_gadget_trails(window, dx, dy);
    _clear_resized_window_exposed_areas(window, old_width, old_height);

    _render_window_frame(window);

    _post_idcmp_message(window, IDCMP_NEWSIZE, 0, 0,
                        window, window->MouseX, window->MouseY);
    _post_idcmp_message(window, IDCMP_CHANGEWINDOW, CWCODE_MOVESIZE, 0,
                        window, window->MouseX, window->MouseY);
}

struct View * _intuition_ViewAddress ( register struct IntuitionBase * IntuitionBase __asm("a6"))
{
    /* Return a pointer to the Intuition View structure.
     * This is the master View for all screens.
     */
    DPRINTF (LOG_DEBUG, "_intuition: ViewAddress() returning &IntuitionBase->ViewLord=%p\n",
             &IntuitionBase->ViewLord);
    
    return &IntuitionBase->ViewLord;
}

struct ViewPort * _intuition_ViewPortAddress ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register const struct Window * window __asm("a0"))
{
    /*
     * ViewPortAddress() returns a pointer to the ViewPort associated with
     * the window's screen. This is used for graphics functions that need
     * to work with the screen's color map and display settings.
     */
    DPRINTF (LOG_DEBUG, "_intuition: ViewPortAddress() window=0x%08lx\n", (ULONG)window);
    
    if (!window || !window->WScreen) {
        DPRINTF (LOG_WARNING, "_intuition: ViewPortAddress() - invalid window or no screen\n");
        return NULL;
    }
    
    /* Return the ViewPort from the window's screen */
    return &window->WScreen->ViewPort;
}

VOID _intuition_WindowToBack ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Window * window __asm("a0"))
{
    BOOL rootless_mode;
    struct Window **link;
    struct Window *tail;

    DPRINTF (LOG_DEBUG, "_intuition: WindowToBack() window=0x%08lx\n", (ULONG)window);
    
    if (!window) return;

    if (window->WScreen && window->WScreen->FirstWindow != window)
    {
        link = &window->WScreen->FirstWindow;
        while (*link && *link != window)
            link = &(*link)->NextWindow;

        if (*link == window)
        {
            *link = window->NextWindow;
            tail = window->WScreen->FirstWindow;
            while (tail && tail->NextWindow)
                tail = tail->NextWindow;

            if (tail)
                tail->NextWindow = window;
            else
                window->WScreen->FirstWindow = window;

            window->NextWindow = NULL;
        }
    }
    else if (window->WScreen && window->NextWindow)
    {
        window->WScreen->FirstWindow = window->NextWindow;
        tail = window->WScreen->FirstWindow;
        while (tail->NextWindow)
            tail = tail->NextWindow;
        tail->NextWindow = window;
        window->NextWindow = NULL;
    }

    /* Update internal structures */
    if (window->WLayer && LayersBase)
    {
        _call_BehindLayer(LayersBase, window->WLayer);
    }

    /* Check for rootless mode */
    rootless_mode = emucall0(EMU_CALL_INT_GET_ROOTLESS);
    
    if (rootless_mode)
    {
        ULONG window_handle = _intuition_get_host_window_handle((struct LXAIntuitionBase *)IntuitionBase, window);

        if (window_handle)
            emucall1(EMU_CALL_INT_WINDOW_TOBACK, window_handle);
    }

    /* AmigaOS 3.1 reference: no IDCMP_CHANGEWINDOW for programmatic depth
     * changes (the depth gadget handler posts CWCODE_DEPTH itself) */
}

VOID _intuition_WindowToFront ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Window * window __asm("a0"))
{
    BOOL rootless_mode;
    struct Window **link;

    DPRINTF (LOG_DEBUG, "_intuition: WindowToFront() window=0x%08lx\n", (ULONG)window);
    
    if (!window) return;

    if (window->WScreen && window->WScreen->FirstWindow != window)
    {
        link = &window->WScreen->FirstWindow;
        while (*link && *link != window)
            link = &(*link)->NextWindow;

        if (*link == window)
        {
            *link = window->NextWindow;
            window->NextWindow = window->WScreen->FirstWindow;
            window->WScreen->FirstWindow = window;
        }
    }

    /* Update internal structures */
    if (window->WLayer && LayersBase)
    {
        _call_UpfrontLayer(LayersBase, window->WLayer);
    }

    /* Check for rootless mode */
    rootless_mode = emucall0(EMU_CALL_INT_GET_ROOTLESS);
    
    if (rootless_mode)
    {
        ULONG window_handle = _intuition_get_host_window_handle((struct LXAIntuitionBase *)IntuitionBase, window);

        if (window_handle)
            emucall1(EMU_CALL_INT_WINDOW_TOFRONT, window_handle);
    }

    /* AmigaOS 3.1 reference: no IDCMP_CHANGEWINDOW for programmatic depth
     * changes (the depth gadget handler posts CWCODE_DEPTH itself) */
}

BOOL _intuition_WindowLimits ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Window * window __asm("a0"),
                                                        register LONG widthMin __asm("d0"),
                                                        register LONG heightMin __asm("d1"),
                                                        register ULONG widthMax __asm("d2"),
                                                        register ULONG heightMax __asm("d3"))
{
    /* GCC m68k move.w fix: sign-extend LONG params, zero-extend ULONG params */
    widthMin  = (LONG)(WORD)widthMin;
    heightMin = (LONG)(WORD)heightMin;
    widthMax  = (ULONG)(UWORD)widthMax;
    heightMax = (ULONG)(UWORD)heightMax;

    DPRINTF(LOG_DEBUG, "_intuition: WindowLimits() window=0x%08lx min=%ldx%ld max=%ldx%ld\n", 
            (ULONG)window, widthMin, heightMin, widthMax, heightMax);
            
    if (!window) return FALSE;

    if (widthMin) window->MinWidth = widthMin;
    if (heightMin) window->MinHeight = heightMin;
    if (widthMax) window->MaxWidth = widthMax;
    if (heightMax) window->MaxHeight = heightMax;

    /* Calling SizeWindow with 0,0 will enforce new limits */
    _intuition_SizeWindow(IntuitionBase, window, 0, 0);

    return TRUE;
}

struct Preferences  * _intuition_SetPrefs ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register const struct Preferences * preferences __asm("a0"),
                                                        register LONG size __asm("d0"),
                                                        register BOOL inform __asm("d1"))
{
    struct LXAIntuitionBase *base = (struct LXAIntuitionBase *)IntuitionBase;
    struct Screen *screen;

    /* GCC m68k move.w fix: sign-extend d-register LONG param */
    size = (LONG)(WORD)size;

    DPRINTF (LOG_DEBUG, "_intuition: SetPrefs() preferences=0x%08lx size=%ld inform=%d\n",
             (ULONG)preferences, size, (int)inform);

    if (!preferences || size <= 0)
        return (struct Preferences *)preferences;

    CopyMem((APTR)preferences,
            &base->ActivePrefs,
            (ULONG)size > sizeof(struct Preferences) ? sizeof(struct Preferences) : (ULONG)size);

    if (inform)
    {
        for (screen = IntuitionBase->FirstScreen; screen; screen = screen->NextScreen)
        {
            struct Window *window;

            for (window = screen->FirstWindow; window; window = window->NextWindow)
            {
                if (window->IDCMPFlags & IDCMP_NEWPREFS)
                    _post_idcmp_message(window, IDCMP_NEWPREFS, 0, 0, NULL, window->MouseX, window->MouseY);
            }
        }
    }

    return (struct Preferences *)preferences;
}

LONG _intuition_IntuiTextLength ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register const struct IntuiText * iText __asm("a0"))
{
    LXA_UNIMPLEMENTED("intuition", "IntuiTextLength", "partial: assumes 8 pixel Topaz width, ignores ITextFont (Phase 256)");

    /*
     * IntuiTextLength() returns the pixel width of an IntuiText string.
     * This is used for layout calculations before rendering.
     */
    DPRINTF (LOG_DEBUG, "_intuition: IntuiTextLength() iText=0x%08lx\n", (ULONG)iText);
    
    if (!iText || !iText->IText) {
        return 0;
    }
    
    /* Count string length */
    const char *s = (const char *)iText->IText;
    int len = 0;
    while (s[len]) len++;
    
    /* Default to 8 pixels per char (Topaz 8) - ITextFont is a TextAttr, not TextFont */
    UWORD char_width = 8;
    
    /* Could look up font from TextAttr if needed, but for now use default */
    (void)iText->ITextFont;  /* Unused - would need to open font to get metrics */
    
    return (LONG)(len * char_width);
}

BOOL _intuition_WBenchToBack ( register struct IntuitionBase * IntuitionBase __asm("a6"))
{
    struct Screen *wbscreen;

    DPRINTF (LOG_DEBUG, "_intuition: WBenchToBack() called.\n");

    wbscreen = _intuition_find_workbench_screen(IntuitionBase);
    if (!wbscreen)
        return FALSE;

    _intuition_ScreenToBack(IntuitionBase, wbscreen);
    return TRUE;
}

BOOL _intuition_WBenchToFront ( register struct IntuitionBase * IntuitionBase __asm("a6"))
{
    struct Screen *wbscreen;
    
    DPRINTF (LOG_DEBUG, "_intuition: WBenchToFront() called.\n");
    
    wbscreen = _intuition_find_workbench_screen(IntuitionBase);
    if (!wbscreen)
    {
        /* Lazily open the Workbench screen.  Apps such as KickPascal 2
         * call WBenchToFront() (or just expect WB to exist) before
         * directly dereferencing IntuitionBase->ActiveScreen->Width/
         * Height to size their own windows.  On real Amiga, WB is
         * always open by the time apps run; in lxa it is opened
         * lazily, so do it here when first requested. */
        DPRINTF (LOG_DEBUG, "_intuition: WBenchToFront() lazy-opening Workbench screen.\n");
        (void)_intuition_OpenWorkBench(IntuitionBase);
        wbscreen = _intuition_find_workbench_screen(IntuitionBase);
        if (!wbscreen)
            return FALSE;
    }

    _intuition_ScreenToFront(IntuitionBase, wbscreen);
    return TRUE;
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
    /*
     * AutoRequest() displays a modal requester with positive/negative buttons.
     * Per RKRM, it opens a window on the same screen as the reference window,
     * renders body text, creates two gadgets, and blocks until the user clicks
     * one of them or pFlag/nFlag IDCMP events arrive at the original window.
     *
     * Returns TRUE for positive (left) button, FALSE for negative (right) button.
     */
    struct Screen *screen;
    struct NewWindow nw;
    struct Window *reqWin;
    struct Gadget *posGad = NULL, *negGad = NULL;
    struct IntuiMessage *msg;
    BOOL result = TRUE;
    WORD gad_width, gad_height, gad_y;

    DPRINTF (LOG_DEBUG, "_intuition: AutoRequest() window=0x%08lx pFlag=0x%08lx nFlag=0x%08lx %dx%d\n",
             (ULONG)window, pFlag, nFlag, (int)width, (int)height);

    /* Print the body text chain to debug output */
    {
        const struct IntuiText *it = body;
        while (it)
        {
            if (it->IText)
            {
                DPRINTF (LOG_DEBUG, "_intuition: AutoRequest body: %s\n", (char *)it->IText);
            }
            it = it->NextText;
        }
    }

    /* Determine which screen to use */
    if (window)
        screen = window->WScreen;
    else
        screen = IntuitionBase->FirstScreen;

    if (!screen)
    {
        LPRINTF(LOG_WARNING, "_intuition: AutoRequest() no screen available\n");
        return TRUE;
    }

    /* Ensure minimum dimensions */
    if (width < 100) width = 100;
    if (height < 50) height = 50;

    /* Clamp to screen dimensions */
    if (width > screen->Width) width = screen->Width;
    if (height > screen->Height) height = screen->Height;

    /* Create the requester window */
    memset(&nw, 0, sizeof(nw));
    nw.LeftEdge = (screen->Width - width) / 2;
    nw.TopEdge = (screen->Height - height) / 2;
    nw.Width = width;
    nw.Height = height;
    nw.DetailPen = 0;
    nw.BlockPen = 1;
    nw.IDCMPFlags = IDCMP_GADGETUP;
    nw.Flags = WFLG_DRAGBAR | WFLG_ACTIVATE | WFLG_SMART_REFRESH;
    nw.Title = (STRPTR)"Request";
    nw.Screen = screen;
    nw.Type = CUSTOMSCREEN;

    reqWin = _intuition_OpenWindow(IntuitionBase, &nw);
    if (!reqWin)
    {
        LPRINTF(LOG_WARNING, "_intuition: AutoRequest() failed to open window\n");
        return TRUE;
    }

    /* Create positive and negative gadgets */
    gad_width = (width - 40) / 2;
    if (gad_width < 40) gad_width = 40;
    if (gad_width > 120) gad_width = 120;
    gad_height = 12;
    gad_y = height - reqWin->BorderBottom - gad_height - 4;

    if (posText)
    {
        posGad = (struct Gadget *)AllocMem(sizeof(struct Gadget), MEMF_PUBLIC | MEMF_CLEAR);
        if (posGad)
        {
            posGad->LeftEdge = 10;
            posGad->TopEdge = gad_y;
            posGad->Width = gad_width;
            posGad->Height = gad_height;
            posGad->Flags = GFLG_GADGHCOMP;
            posGad->Activation = GACT_RELVERIFY | GACT_ENDGADGET;
            posGad->GadgetType = GTYP_BOOLGADGET;
            posGad->GadgetText = (struct IntuiText *)posText;
            posGad->GadgetID = 1;  /* Positive */
            _intuition_AddGadget(IntuitionBase, reqWin, posGad, -1);
        }
    }
    if (negText)
    {
        negGad = (struct Gadget *)AllocMem(sizeof(struct Gadget), MEMF_PUBLIC | MEMF_CLEAR);
        if (negGad)
        {
            negGad->LeftEdge = width - gad_width - 10;
            negGad->TopEdge = gad_y;
            negGad->Width = gad_width;
            negGad->Height = gad_height;
            negGad->Flags = GFLG_GADGHCOMP;
            negGad->Activation = GACT_RELVERIFY | GACT_ENDGADGET;
            negGad->GadgetType = GTYP_BOOLGADGET;
            negGad->GadgetText = (struct IntuiText *)negText;
            negGad->GadgetID = 0;  /* Negative */
            _intuition_AddGadget(IntuitionBase, reqWin, negGad, -1);
        }
    }

    /* Refresh gadgets so they're rendered */
    _intuition_RefreshGadgets(IntuitionBase, reqWin->FirstGadget, reqWin, NULL);

    /* Render body text */
    if (body && reqWin->RPort)
    {
        _intuition_PrintIText(IntuitionBase, reqWin->RPort, (struct IntuiText *)body,
                   reqWin->BorderLeft + 8, reqWin->BorderTop + 4);
    }

    /* Event loop: wait for gadget click */
    {
        BOOL done = FALSE;
        while (!done)
        {
            WaitPort(reqWin->UserPort);
            while ((msg = (struct IntuiMessage *)GetMsg(reqWin->UserPort)))
            {
                ULONG cls = msg->Class;
                APTR iaddr = msg->IAddress;
                ReplyMsg((struct Message *)msg);

                if (cls == IDCMP_GADGETUP)
                {
                    struct Gadget *gad = (struct Gadget *)iaddr;
                    result = (gad->GadgetID == 1) ? TRUE : FALSE;
                    done = TRUE;
                    break;
                }
            }

            /* Also check the original window for pFlag/nFlag events */
            if (!done && window && window->UserPort)
            {
                while ((msg = (struct IntuiMessage *)GetMsg(window->UserPort)))
                {
                    ULONG cls = msg->Class;
                    ReplyMsg((struct Message *)msg);

                    if (cls & pFlag)
                    {
                        result = TRUE;
                        done = TRUE;
                        break;
                    }
                    if (cls & nFlag)
                    {
                        result = FALSE;
                        done = TRUE;
                        break;
                    }
                }
            }
        }
    }

    /* Clean up: remove gadgets, close window */
    if (posGad)
    {
        _intuition_RemoveGadget(IntuitionBase, reqWin, posGad);
        FreeMem(posGad, sizeof(struct Gadget));
    }
    if (negGad)
    {
        _intuition_RemoveGadget(IntuitionBase, reqWin, negGad);
        FreeMem(negGad, sizeof(struct Gadget));
    }
    _intuition_CloseWindow(IntuitionBase, reqWin);

    DPRINTF(LOG_DEBUG, "_intuition: AutoRequest() -> %s\n", result ? "TRUE" : "FALSE");
    return result;
}

/*
 * BeginRefresh - Begin refresh cycle for a WFLG_SIMPLE_REFRESH window
 *
 * This is called in response to an IDCMP_REFRESHWINDOW message.
 * It sets up the layer for optimized refresh drawing (only damaged areas).
 */
VOID _intuition_BeginRefresh ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                               register struct Window * window __asm("a0"))
{
    struct Layer *layer;

    DPRINTF(LOG_DEBUG, "_intuition: BeginRefresh() window=0x%08lx\n", (ULONG)window);

    if (!window)
        return;

    layer = window->WLayer;
    if (!layer)
    {
        DPRINTF(LOG_DEBUG, "_intuition: BeginRefresh() window has no layer\n");
        return;
    }

    LockLayerInfo(&window->WScreen->LayerInfo);
    LockLayer(0, layer);

    if (!BeginUpdate(layer))
    {
        EndUpdate(layer, FALSE);
        UnlockLayer(layer);
        UnlockLayerInfo(&window->WScreen->LayerInfo);
        return;
    }

    window->Flags |= WFLG_WINDOWREFRESH;

    /* Optionally: Clear the damaged areas to the background color
     * For WFLG_SIMPLE_REFRESH windows, the app is responsible for redrawing */
}

struct Window * _intuition_BuildSysRequest ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Window * window __asm("a0"),
                                                        register struct IntuiText * bodyText __asm("a1"),
                                                        register struct IntuiText * posText __asm("a2"),
                                                        register struct IntuiText * negText __asm("a3"),
                                                        register ULONG flags __asm("d0"),
                                                        register UWORD width __asm("d1"),
                                                        register UWORD height __asm("d2"))
{
    struct EasyStruct easy;
    struct Window *reqWindow;
    char *body_buf;
    char *gad_buf;
    LONG body_len;
    LONG pos_len;
    LONG neg_len;
    const struct IntuiText *it;
    char *dst;
    
    DPRINTF (LOG_DEBUG, "_intuition: BuildSysRequest() body=%s\n", 
             (bodyText && bodyText->IText) ? (char *)bodyText->IText : "NULL");

    body_len = 0;
    for (it = bodyText; it; it = it->NextText)
    {
        if (it->IText)
            body_len += strlen((char *)it->IText);
        if (it->NextText)
            body_len++;
    }

    body_buf = (char *)AllocMem(body_len + 1, MEMF_PUBLIC | MEMF_CLEAR);
    if (!body_buf)
        return NULL;

    dst = body_buf;
    for (it = bodyText; it; it = it->NextText)
    {
        if (it->IText)
        {
            strcpy(dst, (char *)it->IText);
            dst += strlen(dst);
        }
        if (it->NextText)
            *dst++ = '\n';
    }
    *dst = '\0';

    pos_len = (posText && posText->IText) ? strlen((char *)posText->IText) : 0;
    neg_len = (negText && negText->IText) ? strlen((char *)negText->IText) : strlen("Cancel");
    gad_buf = (char *)AllocMem(pos_len + neg_len + 2, MEMF_PUBLIC | MEMF_CLEAR);
    if (!gad_buf)
    {
        FreeMem(body_buf, body_len + 1);
        return NULL;
    }

    /* without a positive text only the negative button (AmigaOS 3.1) */
    dst = gad_buf;
    if (posText && posText->IText)
    {
        strcpy(dst, (char *)posText->IText);
        dst += pos_len;
        *dst++ = '|';
    }
    if (negText && negText->IText)
        strcpy(dst, (char *)negText->IText);
    else
        strcpy(dst, "Cancel");

    /* the body's extent: the texts' right and bottom edges plus their
     * smallest LeftEdge/TopEdge (AmigaOS 3.1 reference) */
    {
        struct Screen *scr = (window && window->WScreen) ? window->WScreen
                                                         : _intuition_find_workbench_screen(IntuitionBase);
        struct RastPort *srp = scr ? &scr->RastPort : NULL;
        WORD fh = (srp && srp->TxHeight) ? (WORD)srp->TxHeight : 8;
        WORD ew = 0, eh = 0, ml = 0x7fff, mt = 0x7fff;

        /* the texts' box with the same margin on both sides */
        for (it = bodyText; it; it = it->NextText)
        {
            WORD tw = (it->IText && srp) ? (WORD)TextLength(srp, it->IText, (UWORD)strlen((char *)it->IText)) : 0;
            if (it->LeftEdge + tw > ew)
                ew = it->LeftEdge + tw;
            if (it->TopEdge + fh > eh)
                eh = it->TopEdge + fh;
            if (it->LeftEdge < ml)
                ml = it->LeftEdge;
            if (it->TopEdge < mt)
                mt = it->TopEdge;
        }
        if (bodyText)
        {
            ew += ml;
            eh += mt;
        }
        g_sysreq_w = ew;
        g_sysreq_h = eh;
    }

    easy.es_StructSize = sizeof(easy);
    easy.es_Flags = 0;
    /* NULL: the reference window's title, else "System Request" */
    easy.es_Title = NULL;
    easy.es_TextFormat = (UBYTE *)body_buf;
    easy.es_GadgetFormat = (UBYTE *)gad_buf;

    /* AmigaOS 3.1 sizes the requester from its texts: width and height
     * are ignored */
    (void)width;
    (void)height;
    g_sysreq_layout = TRUE;
    reqWindow = _intuition_BuildEasyRequestArgs(IntuitionBase, window, &easy, flags, NULL);
    g_sysreq_layout = FALSE;

    FreeMem(gad_buf, pos_len + neg_len + 2);
    FreeMem(body_buf, body_len + 1);

    return reqWindow;
}

/*
 * EndRefresh - End refresh cycle for a WFLG_SIMPLE_REFRESH window
 *
 * This is called after the application has redrawn damaged areas.
 * If 'complete' is TRUE, the damage list is cleared. Otherwise,
 * more IDCMP_REFRESHWINDOW messages may follow.
 */
VOID _intuition_EndRefresh ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                             register struct Window * window __asm("a0"),
                             register LONG complete __asm("d0"))
{
    struct Layer *layer;

    /* GCC m68k move.w fix: sign-extend d-register LONG param (boolean) */
    complete = (LONG)(WORD)complete;

    DPRINTF(LOG_DEBUG, "_intuition: EndRefresh() window=0x%08lx complete=%ld\n",
            (ULONG)window, complete);

    if (!window)
        return;

    layer = window->WLayer;
    if (!layer)
    {
        DPRINTF(LOG_DEBUG, "_intuition: EndRefresh() window has no layer\n");
        return;
    }

    if (window->Flags & WFLG_WINDOWREFRESH)
    {
        EndUpdate(layer, complete ? TRUE : FALSE);
    }

    window->Flags &= ~WFLG_WINDOWREFRESH;

    if (complete)
    {
        layer->Flags &= ~LAYERREFRESH;
    }

    UnlockLayer(layer);
    UnlockLayerInfo(&window->WScreen->LayerInfo);
}

VOID _intuition_FreeSysRequest ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Window * window __asm("a0"))
{
    DPRINTF (LOG_DEBUG, "_intuition: FreeSysRequest() window=0x%08lx\n", (ULONG)window);
    
    if (window)
    {
        /* Free EasyReqData allocations if present */
        struct EasyReqData *erd = (struct EasyReqData *)window->UserData;
        if (erd)
        {
            /* Clear UserData before freeing to avoid double-free */
            window->UserData = NULL;
            /* unlink the requester's buttons, keep the system gadgets for
             * CloseWindow() */
            if (erd->gadget_mem)
                _intuition_RemoveGList(IntuitionBase, window,
                                       (struct Gadget *)erd->gadget_mem,
                                       erd->num_gadgets);

            if (erd->gadget_mem)
                FreeMem(erd->gadget_mem, erd->gadget_mem_size);
            if (erd->text_mem)
                FreeMem(erd->text_mem, erd->text_mem_size);
            if (erd->gadget_text_mem)
                FreeMem(erd->gadget_text_mem, erd->gadget_text_mem_size);
            if (erd->border_mem)
                FreeMem(erd->border_mem, erd->border_mem_size);
            if (erd->itext_mem)
                FreeMem(erd->itext_mem, erd->itext_mem_size);
            if (erd->border_xy_mem)
                FreeMem(erd->border_xy_mem, erd->border_xy_mem_size);
            FreeMem(erd, sizeof(struct EasyReqData));
        }

        _intuition_CloseWindow(IntuitionBase, window);
    }
}

LONG _intuition_MakeScreen ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Screen * screen __asm("a0"))
{
    DPRINTF (LOG_DEBUG, "_intuition: MakeScreen() screen=0x%08lx\n", (ULONG)screen);
    
    /*
     * MakeScreen() builds the Copper list for a screen's ViewPort.
     * In lxa, we don't have real Copper hardware. The bitmap pointer
     * changes made by the caller (e.g., for double buffering) will
     * be picked up on the next VBlank display refresh.
     *
     * We just return success here - the actual update happens via
     * the subsequent RethinkDisplay() or WaitTOF() call.
     */
    
    graphics_screen_sync_viewport_bitmap(screen);

    return 0;  /* Success */
}

LONG _intuition_RemakeDisplay ( register struct IntuitionBase * IntuitionBase __asm("a6"))
{
    DPRINTF (LOG_DEBUG, "_intuition: RemakeDisplay() called\n");
    
    /*
     * RemakeDisplay() rebuilds the entire display from all screens.
     * In lxa, we just wait for the next VBlank which will refresh
     * the display with current screen/viewport bitmaps.
     */
    WaitTOF();
    
    return 0;  /* Success */
}

LONG _intuition_RethinkDisplay ( register struct IntuitionBase * IntuitionBase __asm("a6"))
{
    struct Screen *screen;

    DPRINTF (LOG_DEBUG, "_intuition: RethinkDisplay() called\n");
    
    /*
     * RethinkDisplay() is the Intuition-compatible way to merge
     * all screen Copper lists and load the View. In lxa this is
     * equivalent to waiting for VBlank which refreshes the SDL
     * display with the current bitmap contents.
     *
     * Per RKRM: "This call also does a WaitTOF()"
     */
    for (screen = IntuitionBase->FirstScreen; screen; screen = screen->NextScreen)
        graphics_screen_sync_viewport_bitmap(screen);

    WaitTOF();
    
    return 0;  /* Success */
}

APTR _intuition_AllocRemember ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Remember ** rememberKey __asm("a0"),
                                                        register ULONG size __asm("d0"),
                                                        register ULONG flags __asm("d1"))
{
    struct Remember *rem;
    APTR mem;
    
    DPRINTF (LOG_DEBUG, "_intuition: AllocRemember() rememberKey=0x%08lx, size=%ld, flags=0x%08lx\n",
             (ULONG)rememberKey, size, flags);
    
    if (!rememberKey)
        return NULL;
    
    /* Allocate the memory */
    mem = AllocMem(size, flags);
    if (!mem)
        return NULL;
    
    /* Allocate a Remember node to track this allocation */
    rem = AllocMem(sizeof(struct Remember), MEMF_CLEAR | MEMF_PUBLIC);
    if (!rem) {
        FreeMem(mem, size);
        return NULL;
    }
    
    /* Set up the Remember node */
    rem->Memory = mem;
    rem->RememberSize = size;
    
    /* Link it into the list */
    rem->NextRemember = *rememberKey;
    *rememberKey = rem;
    
    DPRINTF (LOG_DEBUG, "_intuition: AllocRemember() -> mem=0x%08lx\n", (ULONG)mem);
    
    return mem;
}

VOID _intuition_private0 ( register struct IntuitionBase * IntuitionBase __asm("a6"))
{
    PRIVATE_FUNCTION_ERROR("_intuition", "private0");
}

VOID _intuition_FreeRemember ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Remember ** rememberKey __asm("a0"),
                                                        register BOOL reallyForget __asm("d0"))
{
    struct Remember *rem, *next;
    
    DPRINTF (LOG_DEBUG, "_intuition: FreeRemember() rememberKey=0x%08lx, reallyForget=%d\n",
             (ULONG)rememberKey, reallyForget);
    
    if (!rememberKey)
        return;
    
    rem = *rememberKey;
    
    while (rem) {
        next = rem->NextRemember;
        
        if (reallyForget && rem->Memory) {
            /* Free the allocated memory */
            FreeMem(rem->Memory, rem->RememberSize);
        }
        
        /* Free the Remember node itself */
        FreeMem(rem, sizeof(struct Remember));
        
        rem = next;
    }
    
    *rememberKey = NULL;
}

ULONG _intuition_LockIBase ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register ULONG dontknow __asm("d0"))
{
    /*
     * LockIBase() locks access to IntuitionBase for safe reading.
     * Per RKRM: The return value is a ULONG lock token that must be
     * passed to UnlockIBase().  On real AmigaOS 2.0+, this value is
     * used internally but the convention is that 0 is returned when
     * there is no contention.  Many applications (e.g. PPaint) check
     * the return value with tst.w/bne and treat non-zero as an error.
     * We therefore return 0 to stay compatible.
     */
    DPRINTF (LOG_DEBUG, "_intuition: LockIBase() dontknow=0x%08lx\n", dontknow);
    if (!IntuitionBase->ActiveScreen)
    {
        DPRINTF (LOG_DEBUG, "_intuition: LockIBase() opening Workbench (ActiveScreen was NULL)\n");
        (void)_intuition_OpenWorkBench(IntuitionBase);
    }
    return 0;  /* Return 0 — apps treat non-zero as failure */
}

VOID _intuition_UnlockIBase ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register ULONG ibLock __asm("a0"))
{
    /*
     * UnlockIBase() releases the lock obtained from LockIBase().
     * No-op since we don't actually lock anything.
     */
    DPRINTF (LOG_DEBUG, "_intuition: UnlockIBase() ibLock=0x%08lx\n", ibLock);
}

LONG _intuition_GetScreenData ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register APTR buffer __asm("a0"),
                                                        register UWORD size __asm("d0"),
                                                        register UWORD type __asm("d1"),
                                                        register const struct Screen * screen __asm("a1"))
{
    /*
     * GetScreenData() copies data from a screen into a buffer.
     * If 'screen' is NULL, it uses the default screen based on 'type'.
     * type: WBENCHSCREEN (1) = Workbench screen, CUSTOMSCREEN (15) = specific screen.
     * Returns TRUE on success, FALSE on failure.
     */
    DPRINTF (LOG_DEBUG, "_intuition: GetScreenData() buffer=0x%08lx size=%u type=%u screen=0x%08lx\n",
             (ULONG)buffer, (unsigned)size, (unsigned)type, (ULONG)screen);
    
    if (!buffer || size == 0) {
        return FALSE;
    }
    
    const struct Screen *src = screen;
    
    /* If screen is NULL, get screen based on type */
    if (!src) {
        if (type == 1) {  /* WBENCHSCREEN */
            /* the Workbench screen; AmigaOS 3.1 always has it open (an
             * application sizing its screen from it - BlitzBasic2 - needs
             * real data) */
            src = _intuition_find_workbench_screen(IntuitionBase);
            if (!src && _intuition_OpenWorkBench(IntuitionBase))
                src = _intuition_find_workbench_screen(IntuitionBase);
            if (!src)
                src = IntuitionBase->FirstScreen;
        }
    }
    
    if (!src) {
        DPRINTF (LOG_WARNING, "_intuition: GetScreenData() - no screen available\n");
        return FALSE;
    }
    
    /* Copy screen data to buffer (limited by size) */
    UWORD copy_size = size < sizeof(struct Screen) ? size : sizeof(struct Screen);
    const char *sp = (const char *)src;
    char *dp = (char *)buffer;
    for (UWORD i = 0; i < copy_size; i++) {
        dp[i] = sp[i];
    }
    
    return TRUE;
}

/*
 * Helper to complement (XOR) a gadget's area for GADGHCOMP highlight.
 * Per RKRM: When GFLG_GADGHCOMP is set, Intuition highlights the gadget
 * by complementing (XOR'ing) the select box area. Since XOR is self-inverting,
 * calling this twice restores the original appearance.
 */
static void _complement_gadget_area(struct Window *window, struct Requester *req, struct Gadget *gad)
{
    struct RastPort *rp;
    LONG left, top, width, height;
    
    if (!window || !gad || !window->RPort) return;

    /* system gadgets show their IDS_SELECTED image instead */
    if (_is_sys_gadget(gad))
    {
        _render_sys_gadget(window, gad);
        return;
    }

    /* a BOOPSI gadget draws its own selected state */
    if ((gad->GadgetType & GTYP_GTYPEMASK) == GTYP_CUSTOMGADGET)
        return;

    /* For GZZ windows, border/system gadgets use BorderRPort */
    if ((window->Flags & WFLG_GIMMEZEROZERO) && window->BorderRPort &&
        (gad->GadgetType & (GTYP_GZZGADGET | GTYP_SYSGADGET)))
    {
        rp = window->BorderRPort;
    }
    else
    {
        rp = window->RPort;
    }
    
    _calculate_gadget_box(window, req, gad, &left, &top, &width, &height);
    
    DPRINTF(LOG_DEBUG, "_intuition: _complement_gadget_area() gad=0x%08lx at (%ld,%ld) %ldx%ld flags=0x%04x\n",
            (ULONG)gad, left, top, width, height, (unsigned)gad->Flags);
    
    /* Complement the gadget select box area using XOR draw mode */
    SetDrMd(rp, COMPLEMENT);
    SetAPen(rp, 0xFF);  /* All planes */
    RectFill(rp, left, top, left + width - 1, top + height - 1);
    SetDrMd(rp, JAM2);  /* Restore normal draw mode */
}

/* Draw Intuition-style disabled ghosting over an already-rendered gadget.
 * Pattern matches the classic 0x5555/0xAAAA checker mask. */
static void _render_gadget_disabled_overlay(struct RastPort *rp,
                                            LONG left,
                                            LONG top,
                                            LONG width,
                                            LONG height)
{
    WORD x0, y0, x1, y1, x, y;

    if (!rp || width <= 0 || height <= 0)
        return;

    x0 = (WORD)left;
    y0 = (WORD)top;
    x1 = (WORD)(left + width - 1);
    y1 = (WORD)(top + height - 1);

    SetDrMd(rp, JAM1);
    SetAPen(rp, 1);

    /* Classic ghosting checker: draw pen 1 on every other pixel. */
    for (y = y0; y <= y1; y++)
    {
        WORD parity = (WORD)(y & 1);
        for (x = (WORD)(x0 + parity); x <= x1; x += 2)
            WritePixel(rp, x, y);
    }

    SetDrMd(rp, JAM2);
}

/* Helper to render a single gadget */
static void _render_gadget(struct Window *window, struct Requester *req, struct Gadget *gad)
{
    struct RastPort *rp;
    LONG left, top, width, height;
    
    if (!window || !gad || !window->RPort) return;

    if (_is_sys_gadget(gad))
    {
        _render_sys_gadget(window, gad);
        return;
    }

    DPRINTF(LOG_DEBUG, "_intuition: _render_gadget() gad=0x%08lx type=0x%04x flags=0x%04x GadgetRender=0x%08lx\n",
            (ULONG)gad, (unsigned)gad->GadgetType, (unsigned)gad->Flags, (ULONG)gad->GadgetRender);

    /* BOOPSI custom gadget: dispatch GM_RENDER on its class.
     * The gadget object was created by NewObjectA() with a proper _Object
     * header preceding it, so OCLASS() is valid.
     */
    if ((gad->GadgetType & GTYP_GTYPEMASK) == GTYP_CUSTOMGADGET)
    {
        struct Hook *ch = (struct Hook *)gad->MutualExclude;
        if (ch ? ch->h_Entry != NULL : OCLASS((Object *)gad) != NULL)
        {
            /* For GZZ windows, route system/gzz gadgets to the border RastPort. */
            struct RastPort *brp = window->RPort;
            if ((window->Flags & WFLG_GIMMEZEROZERO) && window->BorderRPort &&
                (gad->GadgetType & (GTYP_GZZGADGET | GTYP_SYSGADGET)))
                brp = window->BorderRPort;

            struct GadgetInfo gi;
            memset(&gi, 0, sizeof(gi));
            gi.gi_Screen = window->WScreen;
            gi.gi_Window = window;
            gi.gi_Requester = req;
            gi.gi_RastPort = brp;
            gi.gi_Layer = brp ? brp->Layer : NULL;
            /* gi_Domain is the coordinate domain for GFLG_REL* gadget geometry.
             * Per AROS/Intuition: for normal window gadgets (non-GZZ), the domain
             * is {0, 0, window->Width, window->Height}.  The GZZ inner-layer case
             * (WFLG_GIMMEZEROZERO) uses the inner dimensions instead.
             * Using the inner dimensions for non-GZZ windows causes BGUI to
             * double-subtract the border widths when resolving GA_RelWidth /
             * GA_RelHeight on the master group gadget, resulting in collapsed layout. */
            _intuition_gadget_domain(window, &gi.gi_Domain);
            gi.gi_DrInfo = _intuition_GetScreenDrawInfo((struct IntuitionBase *)NULL, window->WScreen);

            struct gpRender gpr;
            gpr.MethodID = GM_RENDER;
            gpr.gpr_GInfo = &gi;
            gpr.gpr_RPort = brp;
            gpr.gpr_Redraw = GREDRAW_REDRAW;

            _custom_gadget_call(gad, (Msg)&gpr, NULL);

            if (gad->Flags & GFLG_DISABLED)
            {
                _calculate_gadget_box(window, req, gad, &left, &top, &width, &height);
                _render_gadget_disabled_overlay(brp, left, top, width, height);
            }

            if (gi.gi_DrInfo)
                _intuition_FreeScreenDrawInfo((struct IntuitionBase *)NULL, window->WScreen, gi.gi_DrInfo);
        }
        return;
    }
    
    /* For GZZ windows, gadgets with GTYP_GZZGADGET or GTYP_SYSGADGET
     * belong to the border layer and must use BorderRPort. */
    if ((window->Flags & WFLG_GIMMEZEROZERO) && window->BorderRPort &&
        (gad->GadgetType & (GTYP_GZZGADGET | GTYP_SYSGADGET)))
    {
        rp = window->BorderRPort;
    }
    else
    {
        rp = window->RPort;
    }
    
    _calculate_gadget_box(window, req, gad, &left, &top, &width, &height);

    /* a requester's gadgets live in the requester's own layer */
    if (req && req->ReqLayer && req->ReqLayer != window->WLayer && req->ReqLayer->rp)
    {
        LONG rl, rt;
        _calculate_requester_box(window, req, &rl, &rt, NULL, NULL);
        rp = req->ReqLayer->rp;
        left -= rl;
        top -= rt;
    }

    /* GadTools draws its own gadgets (frames, labels, images) */
    if (_gadtools_IsGadTools(gad) && _gadtools_RenderGadget(window, gad, rp, left, top))
        return;
    
    /* Add window border offset if not using a layer that handles it?
     * Standard Gfx: RPort is usually window's UserPort or similar.
     * If using window->RPort (which is typically the Layer's RP), coordinate (0,0) is window top-left (excluding borders usually? No, Layer includes borders if simple layer).
     * Wait, standard Window RPort (0,0) is at (BorderLeft, BorderTop).
     * Gadget coordinates are relative to (BorderLeft, BorderTop) if GFLG_REL... or just standard?
     * RKRM: "Gadget coordinates are relative to the top-left of the window (including borders)."
     * So if RPort origin is (BorderLeft, BorderTop), we must subtract borders to draw at (0,0)?
     * Or does RPort origin match Window (0,0)?
     * In lxa `OpenWindow`: `CreateUpfrontLayer` uses `window->LeftEdge` ...
     * The Layer covers the whole window.
     * So (0,0) in Layer RP is (0,0) of Window (Top-Left corner of border).
     * So we don't need to add `BorderLeft/Top` unless we are drawing into screen RP.
     */
     
    /* Draw gadget imagery.
     * Per RKRM: GadgetRender determines the gadget's visual appearance.
     * - If GFLG_GADGIMAGE is set, GadgetRender points to an Image.
     * - If GFLG_GADGIMAGE is NOT set, GadgetRender points to a Border.
     * The highlight flags (GFLG_GADGHCOMP, GFLG_GADGHBOX, GFLG_GADGHIMAGE)
     * control how the gadget looks when SELECTED, not the initial rendering.
     */
    if ((gad->GadgetType & GTYP_GTYPEMASK) == GTYP_PROPGADGET && gad->SpecialInfo)
    {
        /* GadgetRender is the knob: drawn with the container below */
    }
    else if (gad->Flags & GFLG_GADGIMAGE)
    {
        if (gad->GadgetRender)
        {
            /* Use SelectRender if selected and GFLG_GADGHIMAGE highlight mode */
            struct Image *img = (struct Image *)gad->GadgetRender;
            if ((gad->Flags & GFLG_SELECTED) && (gad->Flags & GFLG_GADGHIMAGE) && gad->SelectRender)
            {
                img = (struct Image *)gad->SelectRender;
            }
             
            _intuition_DrawImage((struct IntuitionBase *)NULL, rp, img, left, top);
        }
    }
    else
    {
        /* GFLG_GADGIMAGE is NOT set: GadgetRender is a Border* */
        if (gad->GadgetRender)
        {
            _intuition_DrawBorder((struct IntuitionBase *)NULL, rp,
                                  (struct Border *)gad->GadgetRender, left, top);
        }
    }
    
    /* Cycle gadget chrome: spec-correct implementation per cycle_gadget_spec.md.
     *
     * Layout (all offsets relative to gadget left/top):
     *   CYCLEGLYPHWIDTH = 20 px: glyph occupies left section
     *   Glyph border: LeftEdge = LEFTTRIM+2 = 6, vertically centred
     *   Divider: shadow at x=20, shine at x=21, from y=2 to y=height-3
     *   Label text: centred in [x=22 .. width-1, y=0 .. height-1]
     *
     * Glyph polygon (16 vertices, §8 of spec):
     *   height_g = max(gadHeight-5, 9)
     *   Vertices (relative to glyph LeftEdge/TopEdge):
     *     (7,0)(7,5)(5,3)(10,3)(8,5)(8,1)(7,0)
     *     (1,0)(0,1)(0,h-2)(1,h-1)(1,1)(1,h-1)(7,h-1)(7,h-2)(8,h-2)
     *   where h = height_g
     *
     * Pen: TEXTPEN (pen 1) for the glyph (DESIGNTEXT);
     *      SHADOWPEN (pen 1) for dark stroke, SHINEPEN (pen 2) for light stroke.
     *
     * Note: lxa pen map — pen 0 = BACKGROUNDPEN, pen 1 = TEXTPEN/SHADOWPEN,
     *       pen 2 = SHINEPEN. On standard 4-colour Workbench these differ, but
     *       for the glyph the spec uses DESIGNTEXT which maps to TEXTPEN = pen 1.
     */
    if (_gadtools_IsCycle(gad) && width >= 22 && height >= 6)
    {
        /* Glyph geometry */
        WORD gh     = (WORD)(height - 5);
        if (gh < 9) gh = 9;
        WORD g_left = (WORD)(left + 6);                        /* LEFTTRIM+2 */
        WORD g_top  = (WORD)(top  + (height - gh) / 2);       /* vertically centred */

        /* Divider geometry */
        WORD div_dark  = (WORD)(left + 20);   /* CYCLEGLYPHWIDTH */
        WORD div_light = (WORD)(left + 21);
        WORD div_y1    = (WORD)(top  + 2);
        WORD div_y2    = (WORD)(top  + height - 3);

        /* Clear glyph+divider region */
        SetAPen(rp, 0);
        SetDrMd(rp, JAM1);
        RectFill(rp, (WORD)(left + 1), (WORD)(top + 1),
                     div_light, (WORD)(top + height - 2));

        /* --- Draw glyph polyline (15 segments, 16 vertices) --- */
        /* All coordinates are relative to (g_left, g_top). */
        SetAPen(rp, 1);   /* TEXTPEN (DESIGNTEXT) */

#define GX(dx) ((WORD)(g_left + (dx)))
#define GY(dy) ((WORD)(g_top  + (dy)))

        /* Vertex 0: (7,0) */
        Move(rp, GX(7), GY(0));
        /* Segment 0→1: stem down */
        Draw(rp, GX(7), GY(5));
        /* Segment 1→2: left barb */
        Draw(rp, GX(5), GY(3));
        /* Segment 2→3: horizontal body to right tip */
        Draw(rp, GX(10), GY(3));
        /* Segment 3→4: right shoulder */
        Draw(rp, GX(8), GY(5));
        /* Segment 4→5: right side back up */
        Draw(rp, GX(8), GY(1));
        /* Segment 5→6: close arrow back to top */
        Draw(rp, GX(7), GY(0));
        /* Segment 6→7: top edge leftward */
        Draw(rp, GX(1), GY(0));
        /* Segment 7→8: top-left bevel */
        Draw(rp, GX(0), GY(1));
        /* Segment 8→9: full left edge down */
        Draw(rp, GX(0), GY(gh - 2));
        /* Segment 9→10: bottom-left bevel */
        Draw(rp, GX(1), GY(gh - 1));
        /* Segment 10→11: inner left up (double-thickness border) */
        Draw(rp, GX(1), GY(1));
        /* Segment 11→12: inner left back down */
        Draw(rp, GX(1), GY(gh - 1));
        /* Segment 12→13: bottom edge rightward */
        Draw(rp, GX(7), GY(gh - 1));
        /* Segment 13→14: partial right side */
        Draw(rp, GX(7), GY(gh - 2));
        /* Segment 14→15: end of glyph outline */
        Draw(rp, GX(8), GY(gh - 2));

#undef GX
#undef GY

        /* --- Vertical divider: shadow (left line) then shine (right line) --- */
        SetAPen(rp, 1);   /* SHADOWPEN */
        Move(rp, div_dark, div_y1);
        Draw(rp, div_dark, div_y2);

        SetAPen(rp, 2);   /* SHINEPEN */
        Move(rp, div_light, div_y1);
        Draw(rp, div_light, div_y2);
    }

    /* Cycle gadget right-side dropdown arrow removed.
     * RKRM specifies the icon+divider on the LEFT (drawn above).
     * The old AROS-style right-side dropdown is not correct for AmigaOS. */

    /* GadTools CHECKBOX_KIND artwork — spec §11.4/§11.5.
     *
     * The checkbox image is drawn as:
     *   1. Interior fill (BACKGROUNDPEN, pen 0): x=2..w-3, y=1..h-2
     *   2. VIB_THICK3D double-pixel bevel (JOINS_ANGLED):
     *        SHINEPEN  (pen 2): top row y=0 x=0..w-2, left cols x=0..1 y=1..h-2
     *        SHADOWPEN (pen 1): right cols x=w-2..w-1 y=0..h-2, bottom row y=h-1 x=1..w-1
     *        Angled corners: top-right (w-1,0) = SHADOW; bottom-left (0,h-1) = SHINE
     *   3. Checkmark polygon (TEXTPEN, pen 1) if GFLG_SELECTED (§11.4)
     *      Vertices on 26×11 design grid; scale linearly for other sizes.
     */
    if (_gadtools_IsCheckbox(gad))
    {
        WORD bx = (WORD)left;    /* absolute screen coords of checkbox top-left */
        WORD by = (WORD)top;
        WORD bw = (WORD)width;   /* should be 26 (CHECKBOX_WIDTH) */
        WORD bh = (WORD)height;  /* should be 11 (CHECKBOX_HEIGHT) */

        /* 1. Interior clear */
        SetAPen(rp, 0);   /* BACKGROUNDPEN */
        SetDrMd(rp, JAM1);
        RectFill(rp, (WORD)(bx + 2), (WORD)(by + 1),
                     (WORD)(bx + bw - 3), (WORD)(by + bh - 2));

        /* 2a. SHINE (pen 2): top row + left two columns */
        SetAPen(rp, 2);   /* SHINEPEN */
        /* top row: x = 0..w-2  (top-right corner belongs to SHADOW) */
        Move(rp, bx,               by);
        Draw(rp, (WORD)(bx + bw - 2), by);
        /* left col 0: y = 1..h-1  (bottom-left corner belongs to SHINE) */
        Move(rp, bx, (WORD)(by + 1));
        Draw(rp, bx, (WORD)(by + bh - 1));
        /* left col 1: y = 1..h-2 */
        Move(rp, (WORD)(bx + 1), (WORD)(by + 1));
        Draw(rp, (WORD)(bx + 1), (WORD)(by + bh - 2));

        /* 2b. SHADOW (pen 1): right two columns + bottom row */
        SetAPen(rp, 1);   /* SHADOWPEN */
        /* right col w-1: y = 0..h-2 (top-right corner = SHADOW) */
        Move(rp, (WORD)(bx + bw - 1), by);
        Draw(rp, (WORD)(bx + bw - 1), (WORD)(by + bh - 2));
        /* right col w-2: y = 1..h-2 */
        Move(rp, (WORD)(bx + bw - 2), (WORD)(by + 1));
        Draw(rp, (WORD)(bx + bw - 2), (WORD)(by + bh - 2));
        /* bottom row: x = 1..w-1 (bottom-left corner = SHINE, covered above) */
        Move(rp, (WORD)(bx + 1),      (WORD)(by + bh - 1));
        Draw(rp, (WORD)(bx + bw - 1), (WORD)(by + bh - 1));

        /* 3. Checkmark polygon — only when GFLG_SELECTED */
        if (gad->Flags & GFLG_SELECTED)
        {
            /* Scale factors: design grid is 26 wide × 11 tall.
             * For the standard 26×11 size the scale is 1:1.
             * For scaled mode use linear interpolation per spec §11.4. */
#define CBX(cx) ((WORD)(bx + (bw <= 1 ? 0 : ((LONG)(cx) * (bw - 1) + 12) / 25)))
#define CBY(cy) ((WORD)(by + (bh <= 1 ? 0 : ((LONG)(cy) * (bh - 1) +  5) / 10)))
            SetAPen(rp, 1);   /* TEXTPEN */
            /* Scanline fill per spec §11.4 scanline table: */
            /* Row y=2: x=17..18  (2 px, top of right arm) */
            Move(rp, CBX(17), CBY(2)); Draw(rp, CBX(18), CBY(2));
            /* Row y=3: x=16..17 */
            Move(rp, CBX(16), CBY(3)); Draw(rp, CBX(17), CBY(3));
            /* Row y=4: x=15..16 */
            Move(rp, CBX(15), CBY(4)); Draw(rp, CBX(16), CBY(4));
            /* Row y=5: x=7..9 (left arm) and x=14..15 (right arm) */
            Move(rp, CBX(7), CBY(5)); Draw(rp, CBX(9), CBY(5));
            Move(rp, CBX(14), CBY(5)); Draw(rp, CBX(15), CBY(5));
            /* Row y=6: x=8..10, x=13..14 */
            Move(rp, CBX(8), CBY(6)); Draw(rp, CBX(10), CBY(6));
            Move(rp, CBX(13), CBY(6)); Draw(rp, CBX(14), CBY(6));
            /* Row y=7: x=9..13 (arms merge) */
            Move(rp, CBX(9), CBY(7)); Draw(rp, CBX(13), CBY(7));
            /* Row y=8: x=10..12 (bottom apex) */
            Move(rp, CBX(10), CBY(8)); Draw(rp, CBX(12), CBY(8));
#undef CBX
#undef CBY
        }
    }

    /* Draw Text — walk the IntuiText chain (NextText) so that
     * GT_Underscore underline IntuiTexts are rendered too.
     * Per RKRM: IntuiText.TopEdge is the top edge of the character cell,
     * not the baseline.  Text() renders at the baseline, so we must add
     * tf_Baseline to convert from cell-top to baseline coordinates. */
    if (gad->GadgetText)
    {
        struct IntuiText *it = gad->GadgetText;
        BOOL has_underline = FALSE;
        
        /* Phase 157: Check if there's an underline marker (secondary IntuiText with "_") */
        if (it && it->NextText && it->NextText->IText && 
            it->NextText->IText[0] == '_' && it->NextText->IText[1] == '\0')
        {
            has_underline = TRUE;
        }
        
        while (it)
        {
            if (it->IText)
            {
                /* Skip the underscore marker IntuiText */
                if (it->IText[0] == '_' && it->IText[1] == '\0')
                {
                    it = it->NextText;
                    continue;
                }
                
                LONG tx = left + it->LeftEdge;
                LONG ty = top + it->TopEdge;
                WORD baseline = rp->Font ? rp->Font->tf_Baseline : 6;
                SetAPen(rp, it->FrontPen);
                SetBPen(rp, it->BackPen);
                SetDrMd(rp, it->DrawMode);
                Move(rp, tx, ty + baseline);
                
                /* Phase 157: Set FSF_UNDERLINED if this text should be underlined */
                if (has_underline && it == gad->GadgetText)
                {
                    SetSoftStyle(rp, FSF_UNDERLINED, 1);
                    Text(rp, (STRPTR)it->IText, strlen((char *)it->IText));
                    SetSoftStyle(rp, 0, FSF_UNDERLINED);   /* enable 0 would change nothing */
                }
                else
                {
                    Text(rp, (STRPTR)it->IText, strlen((char *)it->IText));
                }
            }
            it = it->NextText;
        }
    }
    
    /* Proportional gadget: container and knob as AmigaOS 3.1 draws them
     * (tests/probes/intuition/propgad.ref.out).  The layout is in
     * _prop_layout(); Intuition stores the container size and knob inset in
     * the PropInfo and the knob box in the AUTOKNOB Image.
     *
     *  - old look: a pen 1 frame (1 pixel top/bottom, 2 pixels left/right)
     *    around a pen 0 container, a solid pen 1 knob; PROPBORDERLESS
     *    drops the frame;
     *  - PROPNEWLOOK: a 1 pixel pen 1 frame around a pen 1/0 dither
     *    (pen 1 where x + y is odd), a pen 2 knob with pen 1 ends along each
     *    free axis; PROPBORDERLESS drops the frame and the knob gets a pen 1
     *    right column and bottom row instead.
     */
    if ((gad->GadgetType & GTYP_GTYPEMASK) == GTYP_PROPGADGET && gad->SpecialInfo &&
        width > 0 && height > 0)
    {
        struct PropInfo *pi = (struct PropInfo *)gad->SpecialInfo;
        struct PropLayout pl;
        WORD x0 = (WORD)left, y0 = (WORD)top;
        WORD x1 = (WORD)(left + width - 1), y1 = (WORD)(top + height - 1);
        BOOL newlook = (pi->Flags & PROPNEWLOOK) != 0;
        BOOL borderless = (pi->Flags & PROPBORDERLESS) != 0;

        _prop_layout(pi, (WORD)width, (WORD)height, &pl);
        pi->CWidth = (UWORD)width;
        pi->CHeight = (UWORD)height;
        pi->LeftBorder = (UWORD)pl.lb;
        pi->TopBorder = (UWORD)pl.tb;

        SetDrMd(rp, JAM2);
        if (!borderless)
        {
            SetAPen(rp, 1);
            RectFill(rp, x0, y0, x1, y1);
            x0 += newlook ? 1 : 2;
            x1 -= newlook ? 1 : 2;
            y0++;
            y1--;
        }
        if (x0 <= x1 && y0 <= y1)
        {
            if (newlook)
            {
                static const UWORD dither[2] = { 0x5555, 0xAAAA };
                SetAfPt(rp, (UWORD *)dither, 1);
                SetAPen(rp, 1);
                SetBPen(rp, 0);
                RectFill(rp, x0, y0, x1, y1);
                SetAfPt(rp, NULL, 0);
            }
            else
            {
                SetAPen(rp, 0);
                RectFill(rp, x0, y0, x1, y1);
            }
        }

        if (pi->Flags & AUTOKNOB)
        {
            struct Image *knob = (struct Image *)gad->GadgetRender;
            WORD kx0 = (WORD)(left + pl.lb + pl.kx), ky0 = (WORD)(top + pl.tb + pl.ky);
            WORD kx1 = kx0 + pl.kw - 1, ky1 = ky0 + pl.kh - 1;

            if (knob)
            {
                knob->LeftEdge = pl.kx;
                knob->TopEdge = pl.ky;
                knob->Width = pl.kw;
                knob->Height = pl.kh;
            }
            if (pl.kw > 0 && pl.kh > 0)
            {
                if (!newlook)
                {
                    SetAPen(rp, 1);
                    RectFill(rp, kx0, ky0, kx1, ky1);
                }
                else
                {
                    SetAPen(rp, 2);
                    RectFill(rp, kx0, ky0, kx1, ky1);
                    SetAPen(rp, 1);
                    if (borderless)
                    {
                        RectFill(rp, kx1, ky0, kx1, ky1);
                        RectFill(rp, kx0, ky1, kx1, ky1);
                    }
                    else
                    {
                        if (pi->Flags & FREEVERT)
                        {
                            RectFill(rp, kx0, ky0, kx1, ky0);
                            RectFill(rp, kx0, ky1, kx1, ky1);
                        }
                        if (pi->Flags & FREEHORIZ)
                        {
                            RectFill(rp, kx0, ky0, kx0, ky1);
                            RectFill(rp, kx1, ky0, kx1, ky1);
                        }
                    }
                }
            }
        }
        else if (gad->GadgetRender)
        {
            /* a custom knob image at the knob position */
            _intuition_DrawImage((struct IntuitionBase *)NULL, rp, (struct Image *)gad->GadgetRender,
                                 (WORD)(left + pl.lb + pl.kx), (WORD)(top + pl.tb + pl.ky));
        }
    }

    if (_gadtools_IsCheckbox(gad))
    {
        WORD box_left = left + 2;
        WORD box_top = top + 2;
        WORD box_right = left + width - 3;
        WORD box_bottom = top + height - 3;

        if (box_left <= box_right && box_top <= box_bottom)
        {
            SetAPen(rp, 0);
            RectFill(rp, box_left, box_top, box_right, box_bottom);

            if (_gadtools_GetCheckboxState(gad))
            {
                WORD mid_y = box_top + ((box_bottom - box_top) / 2);

                SetAPen(rp, 1);
                Move(rp, box_left + 1, mid_y);
                Draw(rp, box_left + 3, box_bottom);
                Draw(rp, box_right, box_top);
            }
        }
    }

    /* Cycle gadget right-side dropdown arrow removed.
     * RKRM specifies the icon+divider on the LEFT (drawn above at line ~11659).
     * The old AROS-style right-side dropdown is not correct for AmigaOS. */

    /* Render string gadget buffer contents.
     * Per RKRM, Intuition renders the string contents inside the gadget area.
     * The border/image (GadgetRender) provides the visual frame, and Intuition
     * draws the actual editable text buffer on top.
     */
    if ((gad->GadgetType & GTYP_GTYPEMASK) == GTYP_STRGADGET)
    {
        struct StringInfo *si = (struct StringInfo *)gad->SpecialInfo;
        if (si && si->Buffer)
        {
            /* AmigaOS 3.1 (tests/probes/intuition/strgad): rendering takes
             * the buffer as it is now - NumChars is recomputed, a DispPos
             * that is not needed to show the cursor is reset - and draws
             * the text from the gadget's top left corner (baseline at top
             * + font baseline), justified per GACT_STRINGCENTER/RIGHT, with
             * a full-height block cursor (COMPLEMENT) when the gadget is
             * selected. */
            LONG len = 0;
            WORD disp, fit, tw, textX;

            while (si->Buffer[len] != '\0' && len < si->MaxChars)
                len++;
            si->NumChars = (WORD)len;
            if (si->BufferPos > len)
                si->BufferPos = (WORD)len;
            disp = si->DispPos;
            if (disp < 0 || disp > len || TextLength(rp, si->Buffer, (UWORD)len) <= width)
                disp = 0;
            if (si->BufferPos < disp)
                disp = si->BufferPos;
            si->DispPos = disp;

            /* Clear gadget interior with background pen */
            SetAPen(rp, 0);
            SetDrMd(rp, JAM2);
            RectFill(rp, left, top, left + width - 1, top + height - 1);

            fit = lxa_text_fit(rp, si->Buffer + disp, (WORD)(len - disp), (WORD)width);
            tw = fit > 0 ? TextLength(rp, si->Buffer + disp, fit) : 0;
            textX = left;
            if (gad->Activation & GACT_STRINGCENTER)
                textX = left + (width - tw) / 2;
            else if (gad->Activation & GACT_STRINGRIGHT)
                textX = left + width - tw;

            SetAPen(rp, 1);  /* Text pen */
            SetBPen(rp, 0);  /* Background pen */
            Move(rp, textX, top + rp->TxBaseline);
            if (fit > 0)
                Text(rp, si->Buffer + disp, fit);

            /* Draw cursor if gadget is active (selected) */
            if (gad->Flags & GFLG_SELECTED)
            {
                WORD pos = si->BufferPos - disp;
                WORD cx = textX + (pos > 0 ? TextLength(rp, si->Buffer + disp, pos) : 0);
                WORD cw = (si->BufferPos < len) ? TextLength(rp, si->Buffer + si->BufferPos, 1)
                                                : TextLength(rp, (STRPTR)" ", 1);
                WORD ch = rp->TxHeight ? rp->TxHeight : 8;

                if (ch > height)
                    ch = height;
                SetAPen(rp, 1);
                SetDrMd(rp, COMPLEMENT);
                RectFill(rp, cx, top, cx + cw - 1, top + ch - 1);
                SetDrMd(rp, JAM2);
            }
        }
    }

    if (gad->Flags & GFLG_DISABLED)
    {
        _render_gadget_disabled_overlay(rp, left, top, width, height);
    }
    
}

VOID _intuition_RefreshGList ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Gadget * gadgets __asm("a0"),
                                                        register struct Window * window __asm("a1"),
                                                        register struct Requester * requester __asm("a2"),
                                                        register WORD numGad __asm("d0"))
{
    struct Gadget *gad = gadgets;
    
    DPRINTF (LOG_DEBUG, "_intuition: RefreshGList() gadgets=0x%08lx win=0x%08lx req=0x%08lx num=%d\n",
             (ULONG)gadgets, (ULONG)window, (ULONG)requester, numGad);
             
    if (!window || !gadgets) return;

    _gadtools_RefreshPass(TRUE);
    _render_gadget_range_reverse(window, requester, gad, numGad, FALSE, TRUE);
    _gadtools_RefreshPass(FALSE);
}

UWORD _intuition_AddGList ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Window * window __asm("a0"),
                                                        register struct Gadget * gadget __asm("a1"),
                                                        register UWORD position __asm("d0"),
                                                        register WORD numGad __asm("d1"),
                                                        register struct Requester * requester __asm("a2"))
{
    struct Gadget **insert_link;
    struct Gadget *scan;
    struct Gadget *last;
    WORD remaining;
    UWORD actual_position;

    DPRINTF (LOG_DEBUG, "_intuition: AddGList() window=0x%08lx gadget=0x%08lx pos=%d numGad=%d req=0x%08lx\n",
             (ULONG)window, (ULONG)gadget, position, numGad, (ULONG)requester);

    if (!window || !gadget || numGad == 0)
        return (UWORD)-1;

    last = gadget;
    remaining = numGad;
    while (last->NextGadget && remaining != 1)
    {
        last = last->NextGadget;
        if (remaining > 0)
            remaining--;
    }

    insert_link = &window->FirstGadget;
    actual_position = 0;

    if (position == (UWORD)-1)
    {
        while (*insert_link)
        {
            insert_link = &(*insert_link)->NextGadget;
            actual_position++;
        }
    }
    else
    {
        while (*insert_link && actual_position < position)
        {
            insert_link = &(*insert_link)->NextGadget;
            actual_position++;
        }
    }

    scan = *insert_link;
    *insert_link = gadget;
    last->NextGadget = scan;

    {
        struct Gadget *init = gadget;
        WORD init_remaining = numGad;

        while (init && (init_remaining == -1 || init_remaining > 0))
        {
            _init_string_gadget_info(init);
            _sniff_border_gadget(window, requester, init);
            _layout_custom_gadget(window, requester, init, TRUE);
            init = init->NextGadget;
            if (init_remaining > 0)
                init_remaining--;
        }
    }

    /* AmigaOS 3.1 does not draw added gadgets: the application calls
     * RefreshGList() (tests/probes/intuition/strgad: NumChars stays 0) */

    return actual_position;
}

UWORD _intuition_RemoveGList ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Window * remPtr __asm("a0"),
                                                        register struct Gadget * gadget __asm("a1"),
                                                        register WORD numGad __asm("d0"))
{
    struct Gadget **link;
    struct Gadget *last;
    WORD remaining;
    UWORD position;

    DPRINTF (LOG_DEBUG, "_intuition: RemoveGList() window=0x%08lx gadget=0x%08lx numGad=%d\n",
             (ULONG)remPtr, (ULONG)gadget, numGad);

    if (!remPtr || !gadget || numGad == 0)
        return (UWORD)-1;

    link = &remPtr->FirstGadget;
    position = 0;
    while (*link && *link != gadget)
    {
        link = &(*link)->NextGadget;
        position++;
    }

    if (!*link)
        return (UWORD)-1;

    last = gadget;
    remaining = numGad;
    while (last->NextGadget && remaining != 1)
    {
        last = last->NextGadget;
        if (remaining > 0)
            remaining--;
    }

    *link = last->NextGadget;

    return position;
}

VOID _intuition_ActivateWindow ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Window * window __asm("a0"))
{
    DPRINTF (LOG_DEBUG, "_intuition: ActivateWindow() window=0x%08lx\n", (ULONG)window);

    if (!window)
        return;

    struct Window *prevActive = IntuitionBase->ActiveWindow;

    /* AmigaOS 3.1 reference (probe intuition/activate): activating the
     * already active window still sends it IDCMP_ACTIVEWINDOW (AmigaOberon's
     * OEd sets its IDCMP after OpenWindow and waits for that message) */
    if (prevActive == window)
    {
        _post_idcmp_message(window, IDCMP_ACTIVEWINDOW, 0, 0, window, 0, 0);
        return;
    }

    /* Deactivate the previously active window */
    if (prevActive)
    {
        prevActive->Flags &= ~WFLG_WINDOWACTIVE;
        _post_idcmp_message(prevActive, IDCMP_INACTIVEWINDOW, 0, 0,
                            prevActive, 0, 0);
        /* Re-render frame to show inactive title bar colors; of the
         * application's gadgets 3.1 redraws only border gadgets
         * (tests/probes/intuition/gadinfo) */
        _render_window_frame_ex(prevActive, LXA_FRAME_BORDER_GADGETS);
    }

    /* Activate the new window; ActiveScreen is the screen of the active
     * window (intuition/intuitionbase.h) */
    IntuitionBase->ActiveWindow = window;
    if (window->WScreen)
        IntuitionBase->ActiveScreen = window->WScreen;
    window->Flags |= WFLG_WINDOWACTIVE;
    _post_idcmp_message(window, IDCMP_ACTIVEWINDOW, 0, 0,
                        window, 0, 0);
    /* Re-render frame to show active title bar colors */
    _render_window_frame_ex(window, LXA_FRAME_BORDER_GADGETS);
}

VOID _intuition_RefreshWindowFrame ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Window * window __asm("a0"))
{
    DPRINTF (LOG_DEBUG, "_intuition: RefreshWindowFrame() window=0x%08lx\n", (ULONG)window);
    
    if (window)
    {
        _render_window_frame(window);
    }
}

BOOL _intuition_ActivateGadget ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Gadget * gadget __asm("a0"),
                                                        register struct Window * window __asm("a1"),
                                                        register struct Requester * requester __asm("a2"))
{
    UWORD gadget_type;

    DPRINTF (LOG_DEBUG, "_intuition: ActivateGadget() gad=0x%08lx win=0x%08lx type=0x%04x\n",
             (ULONG)gadget, (ULONG)window, gadget ? gadget->GadgetType : 0);
    
    if (!gadget || (!window && !requester))
        return FALSE;

    if (gadget->Flags & GFLG_DISABLED)
        return FALSE;

    gadget_type = gadget->GadgetType & GTYP_GTYPEMASK;
    /* AmigaOS 3.1 reference: ActivateGadget() on a boolean gadget succeeds
     * (returns TRUE) but does not select it; only string and custom gadgets
     * become the active input gadget. */
    if (gadget_type != GTYP_STRGADGET && gadget_type != GTYP_CUSTOMGADGET)
        return TRUE;

    if (gadget_type == GTYP_CUSTOMGADGET && (gadget->Activation & GACT_ACTIVEGADGET))
        return FALSE;
    
    /* Per RKRM, ActivateGadget() is primarily used for string gadgets.
     * It makes the gadget the active input gadget so it receives keyboard input.
     * If there's already an active gadget, deactivate it first.
     */
    
    /* Deactivate previous active gadget if any */
    if (g_active_gadget && g_active_gadget != gadget)
    {
        g_active_gadget->Flags &= ~GFLG_SELECTED;
    }
    
    /* Set the new active gadget */
    g_active_gadget = gadget;
    g_active_window = window;
    
    /* Mark gadget as selected (active) */
    gadget->Flags |= GFLG_SELECTED;
    
    /* For string gadgets, recompute NumChars from current buffer contents.
     * The caller may have modified the buffer (e.g., updateStrGad pattern:
     * RemoveGList + strcpy + AddGList + ActivateGadget). We don't touch
     * BufferPos since the caller may have set it intentionally. */
    if ((gadget->GadgetType & GTYP_GTYPEMASK) == GTYP_STRGADGET)
    {
        struct StringInfo *si = (struct StringInfo *)gadget->SpecialInfo;
        if (si && si->Buffer)
        {
            WORD len = 0;
            while (si->Buffer[len] != '\0' && len < si->MaxChars)
                len++;
            si->NumChars = len;
        }
        /* the active string gadget shows its cursor (AmigaOS 3.1) */
        _render_gadget(window, requester, gadget);
    }

    return TRUE;
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
    DPRINTF (LOG_DEBUG, "_intuition: NewModifyProp() gadget=0x%08lx window=0x%08lx req=0x%08lx numGad=%d\n",
             (ULONG)gadget, (ULONG)window, (ULONG)requester, (int)numGad);

    _intuition_ModifyProp(IntuitionBase, gadget, window, requester,
                          flags, horizPot, vertPot, horizBody, vertBody);

    if (window && gadget)
        _intuition_RefreshGList(IntuitionBase, gadget, window, requester, numGad);
}

LONG _intuition_QueryOverscan ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register ULONG displayID __asm("a0"),
                                                        register struct Rectangle * rect __asm("a1"),
                                                        register WORD oScanType __asm("d0"))
{
    struct DimensionInfo dims;

    if (!rect)
        return FALSE;

    /* the overscan rectangles of the display database (AmigaOS 3.1
     * reference: tests/probes/graphics/colormap) */
    if (!GetDisplayInfoData(NULL, (UBYTE *)&dims, sizeof(dims), DTAG_DIMS, displayID))
        return FALSE;
    switch (oScanType) {
        case OSCAN_TEXT:     *rect = dims.TxtOScan;   break;
        case OSCAN_STANDARD: *rect = dims.StdOScan;   break;
        case OSCAN_MAX:      *rect = dims.MaxOScan;   break;
        case OSCAN_VIDEO:    *rect = dims.VideoOScan; break;
        default:             return FALSE;
    }
    return TRUE;
}

VOID _intuition_MoveWindowInFrontOf ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Window * window __asm("a0"),
                                                        register struct Window * behindWindow __asm("a1"))
{
    struct Window **link;

    DPRINTF (LOG_DEBUG, "_intuition: MoveWindowInFrontOf() window=0x%08lx behind=0x%08lx\n",
             (ULONG)window, (ULONG)behindWindow);

    if (!window || !window->WScreen)
        return;

    if (!behindWindow)
    {
        _intuition_WindowToFront(IntuitionBase, window);
        return;
    }

    if (window == behindWindow || behindWindow->WScreen != window->WScreen)
        return;

    link = &window->WScreen->FirstWindow;
    while (*link && *link != window)
        link = &(*link)->NextWindow;
    if (*link != window)
        return;

    *link = window->NextWindow;

    link = &window->WScreen->FirstWindow;
    while (*link && *link != behindWindow)
        link = &(*link)->NextWindow;

    window->NextWindow = *link;
    *link = window;

    if (window->WLayer && behindWindow->WLayer && LayersBase)
        _call_MoveLayerInFrontOf(LayersBase, window->WLayer, behindWindow->WLayer);

    /* AmigaOS 3.1 reference: no IDCMP_CHANGEWINDOW for programmatic depth
     * changes */
}

VOID _intuition_ChangeWindowBox ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Window * window __asm("a0"),
                                                        register LONG left __asm("d0"),
                                                        register LONG top __asm("d1"),
                                                        register LONG width __asm("d2"),
                                                        register LONG height __asm("d3"))
{
    /* GCC m68k move.w fix: sign-extend d-register LONG params from 16 bits */
    left   = (LONG)(WORD)left;
    top    = (LONG)(WORD)top;
    width  = (LONG)(WORD)width;
    height = (LONG)(WORD)height;

    DPRINTF(LOG_DEBUG, "_intuition: ChangeWindowBox() window=0x%08lx pos=%ld,%ld size=%ldx%ld\n", 
            (ULONG)window, left, top, width, height);

    if (!window) return;

    /* clamp the size like SizeWindow() does */
    /* AmigaOS 3.1 reference: ChangeWindowBox() ignores the window limits */
    if (width < 1) width = 1;
    if (height < 1) height = 1;

    /* AmigaOS 3.1 reference: one IDCMP_NEWSIZE + IDCMP_CHANGEWINDOW for a
     * box change (no separate CHANGEWINDOW for the move part) */
    if (width != window->Width || height != window->Height)
    {
        /* ChangeWindowBox() (and ZipWindow() through it) is not bound by the
         * window's size limits, unlike SizeWindow() (AmigaOS 3.1, Phase 220) */
        WORD min_w = window->MinWidth, min_h = window->MinHeight;
        UWORD max_w = window->MaxWidth, max_h = window->MaxHeight;

        _intuition_move_window_impl(IntuitionBase, window, left - window->LeftEdge, top - window->TopEdge, FALSE);
        window->MinWidth = 1;
        window->MinHeight = 1;
        window->MaxWidth = (UWORD)-1;
        window->MaxHeight = (UWORD)-1;
        _intuition_SizeWindow(IntuitionBase, window, width - window->Width, height - window->Height);
        window->MinWidth = min_w;
        window->MinHeight = min_h;
        window->MaxWidth = max_w;
        window->MaxHeight = max_h;
    }
    else if (left != window->LeftEdge || top != window->TopEdge)
    {
        _intuition_move_window_impl(IntuitionBase, window, left - window->LeftEdge, top - window->TopEdge, TRUE);
    }
}

struct Hook * _intuition_SetEditHook ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Hook * hook __asm("a0"))
{
    struct LXAIntuitionBase *base = (struct LXAIntuitionBase *)IntuitionBase;
    struct Hook *old_hook = base->EditHook;

    DPRINTF (LOG_DEBUG, "_intuition: SetEditHook() hook=0x%08lx old=0x%08lx\n",
             (ULONG)hook, (ULONG)old_hook);

    base->EditHook = hook;
    return old_hook;
}

LONG _intuition_SetMouseQueue ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Window * window __asm("a0"),
                                                        register UWORD queueLength __asm("d0"))
{
    LONG old_value;

    DPRINTF (LOG_DEBUG, "_intuition: SetMouseQueue() window=0x%08lx queueLength=%u\n",
             (ULONG)window, (unsigned)queueLength);

    old_value = _intuition_set_mouse_queue_value((struct LXAIntuitionBase *)IntuitionBase,
                                                 window, queueLength);
    return old_value;
}

VOID _intuition_ZipWindow ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Window * window __asm("a0"))
{
    DPRINTF (LOG_DEBUG, "_intuition: ZipWindow() window=0x%08lx\n", (ULONG)window);

    if (!window) return;

    struct ZoomData *zd = (struct ZoomData *)window->ExtData;

    /* the window is in its alternate (zoomed) box (AmigaOS 3.1: zooming
     * an asl requester sets WFLG_ZOOMED, scenario gallery-asl-screenmode) */
    window->Flags ^= WFLG_ZOOMED;

    if (zd)
    {
        /*
         * WA_Zoom was specified: toggle between normal and alternate
         * position/size. Save current pos/size into the "alternate" slot,
         * then switch to the previously stored alternate pos/size.
         */
        WORD cur_left   = window->LeftEdge;
        WORD cur_top    = window->TopEdge;
        WORD cur_width  = window->Width;
        WORD cur_height = window->Height;

        WORD alt_left   = zd->zd_Left;
        WORD alt_top    = zd->zd_Top;
        WORD alt_width  = zd->zd_Width;
        WORD alt_height = zd->zd_Height;

        /* Store current pos/size as the new alternate */
        zd->zd_Left   = cur_left;
        zd->zd_Top    = cur_top;
        zd->zd_Width  = cur_width;
        zd->zd_Height = cur_height;
        zd->zd_IsZoomed = !zd->zd_IsZoomed;

        DPRINTF(LOG_DEBUG, "_intuition: ZipWindow() toggling from %d,%d %dx%d -> %d,%d %dx%d\n",
                (int)cur_left, (int)cur_top, (int)cur_width, (int)cur_height,
                (int)alt_left, (int)alt_top, (int)alt_width, (int)alt_height);

        _intuition_ChangeWindowBox(IntuitionBase, window,
                                    alt_left, alt_top, alt_width, alt_height);
    }
    else
    {
        /*
         * No WA_Zoom data.  AmigaOS 3.1 reference: the alternate box starts
         * as the window's open position with its current minimum size;
         * every ZipWindow() swaps the current box with the alternate one.
         */
        struct LXAWindowState *state = _intuition_ensure_window_state(
            (struct LXAIntuitionBase *)IntuitionBase, window);
        WORD alt_left, alt_top, alt_width, alt_height;

        if (!state)
            return;

        if (state->zip_valid)
        {
            alt_left   = state->zip_box[0];
            alt_top    = state->zip_box[1];
            alt_width  = state->zip_box[2];
            alt_height = state->zip_box[3];
        }
        else
        {
            alt_left   = state->open_left;
            alt_top    = state->open_top;
            alt_width  = window->MinWidth;
            alt_height = window->MinHeight;
        }

        state->zip_box[0] = window->LeftEdge;
        state->zip_box[1] = window->TopEdge;
        state->zip_box[2] = window->Width;
        state->zip_box[3] = window->Height;
        state->zip_valid = TRUE;

        _intuition_ChangeWindowBox(IntuitionBase, window,
                                    alt_left, alt_top, alt_width, alt_height);
    }
}

struct Screen * _intuition_LockPubScreen ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register CONST_STRPTR name __asm("a0"))
{
    struct LXAIntuitionBase *base = (struct LXAIntuitionBase *)IntuitionBase;
    struct PubScreenNode *pub;
    struct Screen *screen;
    
    DPRINTF (LOG_DEBUG, "_intuition: LockPubScreen() name='%s', FirstScreen=0x%08lx\n", 
             name ? (char *)name : "(null/default)", (ULONG)IntuitionBase->FirstScreen);
    
    /* If name is NULL, return the default public screen. */
    if (!name)
    {
        pub = _intuition_default_pubscreen_node(base);
        if (pub)
        {
            pub->psn_VisitorCount++;
            DPRINTF (LOG_DEBUG, "_intuition: LockPubScreen() returning default screen=0x%08lx visitors=%d\n",
                     (ULONG)pub->psn_Screen, pub->psn_VisitorCount);
            return pub->psn_Screen;
        }

        /* No screen exists - auto-open Workbench screen */
        DPRINTF (LOG_INFO, "_intuition: LockPubScreen() no screen, auto-opening Workbench\n");
        if (_intuition_OpenWorkBench(IntuitionBase))
        {
            pub = _intuition_default_pubscreen_node(base);
            if (pub)
            {
                pub->psn_VisitorCount++;
                DPRINTF (LOG_DEBUG, "_intuition: LockPubScreen() returning new default screen=0x%08lx visitors=%d\n",
                         (ULONG)pub->psn_Screen, pub->psn_VisitorCount);
                return pub->psn_Screen;
            }
        }
        DPRINTF (LOG_ERROR, "_intuition: LockPubScreen() failed to auto-open Workbench\n");
        return NULL;
    }

    if (_intuition_ascii_casecmp((const char *)name, "Workbench") == 0)
    {
        pub = _intuition_find_pubscreen_by_name(base, (CONST_STRPTR)"Workbench");
        if (!pub && _intuition_OpenWorkBench(IntuitionBase))
            pub = _intuition_find_pubscreen_by_name(base, (CONST_STRPTR)"Workbench");

        if (pub && !(pub->psn_Flags & PSNF_PRIVATE))
        {
            pub->psn_VisitorCount++;
            return pub->psn_Screen;
        }

        return NULL;
    }

    pub = _intuition_find_pubscreen_by_name(base, name);
    if (pub && !(pub->psn_Flags & PSNF_PRIVATE))
    {
        pub->psn_VisitorCount++;
        screen = pub->psn_Screen;
        DPRINTF (LOG_DEBUG, "_intuition: LockPubScreen() returning named screen '%s' 0x%08lx visitors=%d\n",
                 (const char *)name, (ULONG)screen, pub->psn_VisitorCount);
        return screen;
    }

    DPRINTF (LOG_DEBUG, "_intuition: LockPubScreen() named screen '%s' not found\n", (char *)name);
    return NULL;
}

VOID _intuition_UnlockPubScreen ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register CONST_STRPTR name __asm("a0"),
                                                        register struct Screen * screen __asm("a1"))
{
    struct LXAIntuitionBase *base = (struct LXAIntuitionBase *)IntuitionBase;
    struct PubScreenNode *pub = NULL;

    DPRINTF (LOG_DEBUG, "_intuition: UnlockPubScreen() name='%s', screen=0x%08lx\n",
             name ? (char *)name : "(null)", (ULONG)screen);

    if (name)
        pub = _intuition_find_pubscreen_by_name(base, name);
    if (!pub && screen)
        pub = _intuition_find_pubscreen_by_screen(base, screen);

    if (pub && pub->psn_VisitorCount > 0)
        pub->psn_VisitorCount--;
}

struct List * _intuition_LockPubScreenList ( register struct IntuitionBase * IntuitionBase __asm("a6"))
{
    LXA_UNIMPLEMENTED("intuition", "LockPubScreenList", "partial: list is returned without locking (Phase 256)");

    struct LXAIntuitionBase *base = (struct LXAIntuitionBase *)IntuitionBase;

    DPRINTF (LOG_DEBUG, "_intuition: LockPubScreenList() -> 0x%08lx\n", (ULONG)&base->PubScreenList);
    return &base->PubScreenList;
}

VOID _intuition_UnlockPubScreenList ( register struct IntuitionBase * IntuitionBase __asm("a6"))
{
    LXA_UNIMPLEMENTED("intuition", "UnlockPubScreenList", "partial: no-op, list is never locked (Phase 256)");

    /* Unlock the public screen list after LockPubScreenList.
     * In our simplified implementation, this is a no-op.
     */
    DPRINTF (LOG_DEBUG, "_intuition: UnlockPubScreenList()\n");
    
    /* In a full implementation, this would ReleaseSemaphore on the pub screen list. */
}

STRPTR _intuition_NextPubScreen ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register const struct Screen * screen __asm("a0"),
                                                        register STRPTR namebuf __asm("a1"))
{
    LXA_UNIMPLEMENTED("intuition", "NextPubScreen", "partial: only ever reports Workbench (Phase 256)");

    /* Return the name of the next public screen.
     * If screen is NULL, returns the first public screen name.
     * Returns NULL if there are no more public screens.
     * 
     * In our simplified implementation, we only have Workbench as public.
     */
    DPRINTF (LOG_DEBUG, "_intuition: NextPubScreen(screen=%p, namebuf=%p)\n", screen, namebuf);
    
    if (!namebuf)
        return NULL;
    
    /* If screen is NULL, return "Workbench" as the first/only public screen */
    if (screen == NULL)
    {
        strcpy((char *)namebuf, "Workbench");
        return namebuf;
    }
    
    /* No more public screens after Workbench */
    return NULL;
}

VOID _intuition_SetDefaultPubScreen ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register CONST_STRPTR name __asm("a0"))
{
    LXA_UNIMPLEMENTED("intuition", "SetDefaultPubScreen", "stub: default public screen not changed (Phase 256)");

    /* Set the default public screen.
     * If name is NULL, Workbench becomes the default.
     * In our simplified implementation, this is a no-op since Workbench is always default.
     */
    DPRINTF (LOG_DEBUG, "_intuition: SetDefaultPubScreen(name='%s')\n",
             name ? (const char *)name : "(null=Workbench)");
    
    /* No-op: Workbench is always the default in our implementation */
}

UWORD _intuition_SetPubScreenModes ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register UWORD modes __asm("d0"))
{
    LXA_UNIMPLEMENTED("intuition", "SetPubScreenModes", "partial: modes stored but SHANGHAI/POPPUBSCREEN ignored (Phase 256)");

    /* Set the public screen modes.
     * Returns the old modes value.
     * In our simplified implementation, we store but largely ignore modes.
     */
    static UWORD currentModes = 0;
    UWORD oldModes = currentModes;
    
    DPRINTF (LOG_DEBUG, "_intuition: SetPubScreenModes(modes=0x%04x) old=0x%04x\n",
             (unsigned)modes, (unsigned)oldModes);
    
    currentModes = modes;
    return oldModes;
}

UWORD _intuition_PubScreenStatus ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Screen * screen __asm("a0"),
                                                        register UWORD statusFlags __asm("d0"))
{
    struct LXAIntuitionBase *base = (struct LXAIntuitionBase *)IntuitionBase;
    struct PubScreenNode *pub;
    UWORD oldFlags;

    DPRINTF (LOG_DEBUG, "_intuition: PubScreenStatus() screen=0x%08lx statusFlags=0x%04x\n",
             (ULONG)screen, statusFlags);

    if (!screen)
        return 0;

    pub = _intuition_find_public_screen(base, screen);
    if (!pub)
        return 0;               /* not a public screen */

    oldFlags = pub->psn_Flags;
    (void)oldFlags;

    if ((statusFlags & PSNF_PRIVATE) && pub->psn_VisitorCount > 0)
        return 0;               /* cannot privatize a locked screen */

    pub->psn_Flags = statusFlags & PSNF_PRIVATE;

    if ((pub->psn_Flags & PSNF_PRIVATE) && base->DefaultPubScreen == screen)
        base->DefaultPubScreen = NULL;

    /* AmigaOS 3.1 reference: bit 0 of the result is set when the change
     * succeeded (both for making a screen private and public). */
    return PSNF_PRIVATE;
}

struct RastPort	* _intuition_ObtainGIRPort ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct GadgetInfo * gInfo __asm("a0"))
{
    LXA_UNIMPLEMENTED("intuition", "ObtainGIRPort", "partial: returns the shared gadget RastPort instead of a clone (Phase 256)");

    /* Obtain a RastPort for rendering a gadget.
     * Returns NULL if unsuccessful.
     * The GadgetInfo contains screen/window/layer info.
     */
    DPRINTF (LOG_DEBUG, "_intuition: ObtainGIRPort(gInfo=%p)\n", gInfo);
    
    if (!gInfo)
        return NULL;
    
    /* Return the RastPort from the GadgetInfo.
     * In a full implementation, we'd clone and lock the rastport.
     * For now, just return the one from gInfo.
     */
    if (gInfo->gi_RastPort)
    {
        /* Lock the layer if present */
        if (gInfo->gi_Layer)
            LockLayerRom(gInfo->gi_Layer);
        
        return gInfo->gi_RastPort;
    }
    
    /* If no RastPort in gInfo, try to get one from window or screen */
    if (gInfo->gi_Window && gInfo->gi_Window->RPort)
        return gInfo->gi_Window->RPort;
    
    if (gInfo->gi_Screen)
        return &gInfo->gi_Screen->RastPort;
    
    return NULL;
}

VOID _intuition_ReleaseGIRPort ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct RastPort * rp __asm("a0"))
{
    /* Release a RastPort obtained via ObtainGIRPort.
     * This unlocks the layer if it was locked.
     */
    DPRINTF (LOG_DEBUG, "_intuition: ReleaseGIRPort(rp=%p)\n", rp);
    
    if (!rp)
        return;
    
    /* Unlock the layer if present */
    if (rp->Layer)
        UnlockLayerRom(rp->Layer);
}

VOID _intuition_GadgetMouse ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Gadget * gadget __asm("a0"),
                                                        register struct GadgetInfo * gInfo __asm("a1"),
                                                        register WORD * mousePoint __asm("a2"))
{
    /* Get the current mouse position relative to the gadget.
     * Stores the x,y coordinates in the mousePoint array.
     */
    DPRINTF (LOG_DEBUG, "_intuition: GadgetMouse(gadget=%p, gInfo=%p, mousePoint=%p)\n",
             gadget, gInfo, mousePoint);
    
    if (!mousePoint)
        return;
    
    /* Get mouse position relative to gadget's domain */
    if (gInfo && gInfo->gi_Window)
    {
        /* Get window-relative mouse position and adjust for domain/gadget offset */
        mousePoint[0] = gInfo->gi_Window->MouseX - gInfo->gi_Domain.Left;
        mousePoint[1] = gInfo->gi_Window->MouseY - gInfo->gi_Domain.Top;
        
        /* Further adjust for gadget position if gadget is provided */
        if (gadget)
        {
            mousePoint[0] -= gadget->LeftEdge;
            mousePoint[1] -= gadget->TopEdge;
        }
    }
    else
    {
        /* No window context - return 0,0 */
        mousePoint[0] = 0;
        mousePoint[1] = 0;
    }
}


VOID _intuition_GetDefaultPubScreen ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register STRPTR nameBuffer __asm("a0"))
{
    struct LXAIntuitionBase *base = (struct LXAIntuitionBase *)IntuitionBase;
    struct PubScreenNode *pub;

    /* Get the name of the default public screen.
     * Copies the name into nameBuffer (which must be at least MAXPUBSCREENNAME+1 bytes).
     */
    DPRINTF (LOG_DEBUG, "_intuition: GetDefaultPubScreen(nameBuffer=%p)\n", nameBuffer);

    if (!nameBuffer)
        return;

    pub = _intuition_default_pubscreen_node(base);
    if (pub && pub->psn_Node.ln_Name)
        strcpy((char *)nameBuffer, pub->psn_Node.ln_Name);
    else
        strcpy((char *)nameBuffer, "Workbench");
}

/* ============================================================================
 * EasyRequest implementation
 *
 * This implements the three-function pattern:
 *   BuildEasyRequestArgs() - builds the requester window
 *   SysReqHandler()        - event loop (already implemented)
 *   FreeSysRequest()       - cleanup (already implemented)
 *   EasyRequestArgs()      - blocking wrapper using the above three
 *
 * The implementation formats text using a simple % substitution engine,
 * splits gadget labels on '|', calculates layout, creates gadgets,
 * and opens a centered window on the reference window's screen.
 * ============================================================================ */

/* Simple format string processor for Amiga %s, %ld, %lu, %lx patterns.
 * Returns length of formatted output (not counting NUL).
 * If buf is NULL, just counts the length.
 */
static LONG _easy_format_string(const char *fmt, const UWORD *args, char *buf, LONG bufsize,
                                const UWORD **next_args)
{
    LONG pos = 0;
    const UWORD *ap = args;

    if (!fmt)
    {
        if (buf && bufsize > 0) buf[0] = '\0';
        if (next_args) *next_args = ap;
        return 0;
    }

    while (*fmt)
    {
        if (*fmt != '%')
        {
            if (buf && pos < bufsize - 1) buf[pos] = *fmt;
            pos++;
            fmt++;
            continue;
        }

        fmt++; /* skip '%' */

        if (*fmt == '%')
        {
            if (buf && pos < bufsize - 1) buf[pos] = '%';
            pos++;
            fmt++;
            continue;
        }

        /* Parse optional 'l' modifier */
        int is_long = 0;
        if (*fmt == 'l')
        {
            is_long = 1;
            fmt++;
        }

        if (*fmt == 's')
        {
            /* String: 32-bit pointer on stack */
            ULONG ptr_val = ((ULONG)ap[0] << 16) | ap[1];
            ap += 2;
            const char *s = (const char *)ptr_val;
            if (!s) s = "(null)";
            while (*s)
            {
                if (buf && pos < bufsize - 1) buf[pos] = *s;
                pos++;
                s++;
            }
            fmt++;
        }
        else if (*fmt == 'd' || *fmt == 'u' || *fmt == 'x' || *fmt == 'X')
        {
            char specifier = *fmt;
            fmt++;

            LONG val;
            if (is_long)
            {
                /* 32-bit value */
                val = (LONG)(((ULONG)ap[0] << 16) | ap[1]);
                ap += 2;
            }
            else
            {
                /* 16-bit value */
                val = (WORD)ap[0];
                ap += 1;
            }

            /* Convert number to string */
            char numbuf[20];
            int ni = 0;
            ULONG uval;

            if (specifier == 'x' || specifier == 'X')
            {
                uval = (ULONG)val;
                if (uval == 0)
                {
                    numbuf[ni++] = '0';
                }
                else
                {
                    char hexbuf[16];
                    int hi = 0;
                    while (uval > 0)
                    {
                        int digit = uval & 0xF;
                        hexbuf[hi++] = (digit < 10) ? ('0' + digit) : 
                                       ((specifier == 'X') ? ('A' + digit - 10) : ('a' + digit - 10));
                        uval >>= 4;
                    }
                    while (hi > 0) numbuf[ni++] = hexbuf[--hi];
                }
            }
            else if (specifier == 'u')
            {
                uval = (ULONG)val;
                if (uval == 0)
                {
                    numbuf[ni++] = '0';
                }
                else
                {
                    char revbuf[16];
                    int ri = 0;
                    while (uval > 0)
                    {
                        revbuf[ri++] = '0' + (uval % 10);
                        uval /= 10;
                    }
                    while (ri > 0) numbuf[ni++] = revbuf[--ri];
                }
            }
            else /* 'd' */
            {
                if (val < 0)
                {
                    if (buf && pos < bufsize - 1) buf[pos] = '-';
                    pos++;
                    val = -val;
                }
                uval = (ULONG)val;
                if (uval == 0)
                {
                    numbuf[ni++] = '0';
                }
                else
                {
                    char revbuf[16];
                    int ri = 0;
                    while (uval > 0)
                    {
                        revbuf[ri++] = '0' + (uval % 10);
                        uval /= 10;
                    }
                    while (ri > 0) numbuf[ni++] = revbuf[--ri];
                }
            }

            int j;
            for (j = 0; j < ni; j++)
            {
                if (buf && pos < bufsize - 1) buf[pos] = numbuf[j];
                pos++;
            }
        }
        else
        {
            /* Unknown format spec — output literal */
            if (buf && pos < bufsize - 1) buf[pos] = '%';
            pos++;
            if (is_long)
            {
                if (buf && pos < bufsize - 1) buf[pos] = 'l';
                pos++;
            }
            if (*fmt)
            {
                if (buf && pos < bufsize - 1) buf[pos] = *fmt;
                pos++;
                fmt++;
            }
        }
    }

    if (buf)
    {
        if (pos < bufsize)
            buf[pos] = '\0';
        else if (bufsize > 0)
            buf[bufsize - 1] = '\0';
    }

    if (next_args) *next_args = ap;
    return pos;
}

LONG _intuition_EasyRequestArgs ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Window * window __asm("a0"),
                                                        register const struct EasyStruct * easyStruct __asm("a1"),
                                                        register ULONG * idcmpPtr __asm("a2"),
                                                        register const APTR args __asm("a3"))
{
    /* EasyRequestArgs() - blocking requester using BuildEasyRequestArgs / SysReqHandler / FreeSysRequest.
     *
     * Return values:
     *  0 = rightmost gadget (negative/cancel)
     *  1 = leftmost gadget (positive/ok)
     *  n = nth gadget from left
     * -1 = IDCMP message received (if idcmpPtr != NULL)
     */
    struct Window *req;
    LONG result;
    ULONG idcmp_user = idcmpPtr ? *idcmpPtr : 0;

    DPRINTF (LOG_DEBUG, "_intuition: EasyRequestArgs() window=0x%08lx easyStruct=0x%08lx args=0x%08lx\n",
             (ULONG)window, (ULONG)easyStruct, (ULONG)args);

    req = _intuition_BuildEasyRequestArgs(IntuitionBase, window, easyStruct, idcmp_user, args);

    /* BuildEasyRequestArgs may return special values:
     *  0 -> out of memory, treat as if rightmost gadget was selected
     *  1 -> could not open window, treat as if leftmost gadget was selected
     */
    if (req == NULL || req == (struct Window *)0)
        return 0;
    if (req == (struct Window *)1)
        return 1;

    /* Event loop: keep calling SysReqHandler until we get a result */
    do {
        result = _intuition_SysReqHandler(IntuitionBase, req, idcmpPtr, TRUE);
    } while (result == -2);

    _intuition_FreeSysRequest(IntuitionBase, req);

    return result;
}

struct Window * _intuition_BuildEasyRequestArgs ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Window * window __asm("a0"),
                                                        register const struct EasyStruct * easyStruct __asm("a1"),
                                                        register ULONG idcmp __asm("d0"),
                                                        register const APTR args __asm("a3"))
{
    struct NewWindow nw;
    struct Window *reqWindow;
    struct EasyReqData *erd;
    const UWORD *ap = (const UWORD *)args;
    const UWORD *next_ap;
    LONG body_len, gad_len;
    char *body_buf, *gad_buf;
    WORD num_gadgets;
    WORD i;

    DPRINTF(LOG_DEBUG, "_intuition: BuildEasyRequestArgs() window=0x%08lx easyStruct=0x%08lx args=0x%08lx\n",
            (ULONG)window, (ULONG)easyStruct, (ULONG)args);

    if (!easyStruct)
        return NULL;

    /* ---- Step 1: Format body text (two-pass: measure, then fill) ---- */
    body_len = _easy_format_string((const char *)easyStruct->es_TextFormat, ap, NULL, 0, &next_ap);
    body_buf = (char *)AllocMem(body_len + 1, MEMF_PUBLIC | MEMF_CLEAR);
    if (!body_buf)
        return NULL; /* 0 = out of memory */
    _easy_format_string((const char *)easyStruct->es_TextFormat, ap, body_buf, body_len + 1, &next_ap);

    /* ---- Step 2: Format gadget text (chained from body args) ---- */
    gad_len = _easy_format_string((const char *)easyStruct->es_GadgetFormat, next_ap, NULL, 0, NULL);
    gad_buf = (char *)AllocMem(gad_len + 1, MEMF_PUBLIC | MEMF_CLEAR);
    if (!gad_buf)
    {
        FreeMem(body_buf, body_len + 1);
        return NULL;
    }
    _easy_format_string((const char *)easyStruct->es_GadgetFormat, next_ap, gad_buf, gad_len + 1, NULL);

    /* ---- Step 3: Split gadget labels on '|', count gadgets ---- */
    num_gadgets = 1;
    for (i = 0; gad_buf[i]; i++)
    {
        if (gad_buf[i] == '|')
        {
            gad_buf[i] = '\0';
            num_gadgets++;
        }
    }

    DPRINTF(LOG_DEBUG, "_intuition: BuildEasyRequestArgs() body='%s' gadgets=%d\n",
            body_buf, num_gadgets);

    /* Build array of pointers to each gadget label */
    /* Use a small local array — max 10 gadgets is safe on stack */
    char *gad_labels[10];
    {
        char *p = gad_buf;
        WORD gi = 0;
        for (gi = 0; gi < num_gadgets && gi < 10; gi++)
        {
            gad_labels[gi] = p;
            while (*p) p++;
            p++; /* skip NUL separator */
        }
    }

    /* ---- Step 4: Calculate layout ----
     * AmigaOS 3.1 reference (tests/scenarios/gallery-easyrequest.yaml):
     *   - the window is filled with a SHINEPEN/BACKGROUNDPEN dither
     *   - the body text sits in a recessed one-pixel frame 4 pixels inside
     *     the window borders and 2 below the title bar; the text block is
     *     centred, 24 pixels from the frame sides and fontY-1 above and
     *     below, lines fontY+1 apart, left aligned within the block
     *   - one row below the frame follow the buttons: raised one-pixel
     *     frames, label width + 24 wide, fontY + 6 high, label centred; the
     *     first button is left aligned with the text frame, the last one
     *     right aligned, the others spread evenly in between
     *   - 2 dither rows separate the buttons from the bottom border */
    struct Screen *scr = NULL;
    if (window && window->WScreen)
        scr = window->WScreen;
    if (!scr)
        scr = _intuition_find_workbench_screen(IntuitionBase);
    if (!scr && _intuition_OpenWorkBench(IntuitionBase))
        scr = _intuition_find_workbench_screen(IntuitionBase);
    if (!scr)
    {
        FreeMem(body_buf, body_len + 1);
        FreeMem(gad_buf, gad_len + 1);
        return NULL;
    }

    struct RastPort *srp = &scr->RastPort;
    WORD char_h = srp->TxHeight ? (WORD)srp->TxHeight : 8;
    WORD line_h = char_h + 1;

    /* Measure body text: split on \n, find max width and line count.
     * AmigaOS 3.1 reference (tests/probes/intuition/reqlayout): lines are
     * fontY high, empty ones fontY-2, one pixel apart; an empty first line
     * adds another fontY-2. */
    WORD body_lines = 1;
    WORD body_pixel_w = 0;
    WORD body_pixel_h = 0;
    {
        char *p = body_buf;
        char *ls = p;
        for (;;)
        {
            if (*p == '\n' || *p == '\0')
            {
                WORD w = (WORD)TextLength(srp, (STRPTR)ls, (UWORD)(p - ls));
                if (w > body_pixel_w) body_pixel_w = w;
                body_pixel_h += (p == ls) ? char_h - 2 : char_h;
                if (p == ls && ls == body_buf)
                    body_pixel_h += char_h - 2;
                if (*p == '\0')
                    break;
                body_lines++;
                body_pixel_h++;
                ls = p + 1;
            }
            p++;
        }
    }
    (void)line_h;

    WORD gad_height = char_h + 6;
    WORD total_gad_width = 0;
    WORD gad_widths[10];
    WORD gad_text_w[10];

    for (i = 0; i < num_gadgets && i < 10; i++)
    {
        gad_text_w[i] = (WORD)TextLength(srp, (STRPTR)gad_labels[i], (UWORD)strlen(gad_labels[i]));
        gad_widths[i] = gad_text_w[i] + 24;
        total_gad_width += gad_widths[i];
    }

    WORD border_left = scr->WBorLeft;
    WORD border_right = scr->WBorRight;
    WORD border_top = scr->WBorTop + char_h + 1;
    WORD border_bottom = scr->WBorBottom;

    WORD box_w = body_pixel_w + 50;
    WORD box_h = body_pixel_h + 2 * char_h;
    /* AutoRequest(): the body texts' own extent (BuildSysRequest) */
    if (g_sysreq_layout)
    {
        box_w = g_sysreq_w + 44;
        box_h = g_sysreq_h + 4;
    }
    /* AmigaOS 3.1 reference: at least 12 pixels between buttons */
    if (box_w < total_gad_width + (num_gadgets - 1) * 12)
        box_w = total_gad_width + (num_gadgets - 1) * 12;
    WORD box_x = border_left + 4;
    WORD box_y = border_top + 2;
    WORD gad_row_y = box_y + box_h + 1;

    WORD win_w = border_left + 4 + box_w + 4 + border_right;
    WORD win_h = gad_row_y + gad_height + 2 + border_bottom;

    /* ---- Step 5: Position: AmigaOS 3.1 opens requesters at the top left
     * corner of the screen, wherever the pointer is
     * (tests/probes/intuition/requesters) ---- */
    WORD win_x = 0;
    WORD win_y = 0;

    /* ---- Step 6: Resolve title ---- */
    const char *title = (const char *)easyStruct->es_Title;
    if (!title)
    {
        if (window && window->Title)
            title = (const char *)window->Title;
        else
            title = "System Request";
    }
    /* the title (plus the bar's gadgets) fits into the window */
    {
        WORD tw = (WORD)TextLength(srp, (STRPTR)title, (UWORD)strlen(title)) + 40;
        if (win_w < tw)
        {
            box_w += tw - win_w;
            win_w = tw;
        }
    }

    /* ---- Step 7: Allocate gadgets, borders, and IntuiText ---- */
    LONG gadget_alloc = num_gadgets * sizeof(struct Gadget);
    LONG border_alloc = num_gadgets * 2 * sizeof(struct Border);
    LONG itext_alloc = num_gadgets * sizeof(struct IntuiText);
    LONG border_xy_alloc = num_gadgets * 2 * 10 * sizeof(SHORT);

    struct Gadget *gadgets = (struct Gadget *)AllocMem(gadget_alloc, MEMF_PUBLIC | MEMF_CLEAR);
    struct Border *borders = (struct Border *)AllocMem(border_alloc, MEMF_PUBLIC | MEMF_CLEAR);
    struct IntuiText *itexts = (struct IntuiText *)AllocMem(itext_alloc, MEMF_PUBLIC | MEMF_CLEAR);
    SHORT *border_xy = (SHORT *)AllocMem(border_xy_alloc, MEMF_PUBLIC | MEMF_CLEAR);

    if (!gadgets || !borders || !itexts || !border_xy)
    {
        if (gadgets) FreeMem(gadgets, gadget_alloc);
        if (borders) FreeMem(borders, border_alloc);
        if (itexts) FreeMem(itexts, itext_alloc);
        if (border_xy) FreeMem(border_xy, border_xy_alloc);
        FreeMem(body_buf, body_len + 1);
        FreeMem(gad_buf, gad_len + 1);
        return NULL;
    }

    const UWORD *rpens = _intuition_screen_pens(scr);

    /* ---- Step 8: Set up gadgets ---- */
    {
        WORD free_w = box_w - total_gad_width;
        WORD used = 0;

        for (i = 0; i < num_gadgets; i++)
        {
            struct Gadget *g = &gadgets[i];
            struct Border *b_shine = &borders[i * 2];
            struct Border *b_shadow = &borders[i * 2 + 1];
            struct IntuiText *it = &itexts[i];
            SHORT *xy_shine = &border_xy[i * 2 * 10];
            SHORT *xy_shadow = &border_xy[(i * 2 + 1) * 10];
            WORD bw = gad_widths[i];
            WORD bh = gad_height;
            WORD gx;

            if (num_gadgets == 1)
                gx = box_x + free_w / 2;
            else
                gx = box_x + used + (WORD)(((LONG)free_w * i) / (num_gadgets - 1));
            used += bw;

            /* Gadget ID: last gadget = 0, others count from 1 left-to-right */
            g->GadgetID = (i == num_gadgets - 1) ? 0 : i + 1;
            g->LeftEdge = gx;
            g->TopEdge = gad_row_y;
            g->Width = bw;
            g->Height = bh;
            g->Flags = GFLG_GADGHCOMP;
            g->Activation = GACT_RELVERIFY;
            g->GadgetType = GTYP_BOOLGADGET;
            g->GadgetRender = (APTR)b_shine;
            g->GadgetText = it;
            g->NextGadget = (i < num_gadgets - 1) ? &gadgets[i + 1] : NULL;

            /* raised one-pixel frame, corners left out */
            xy_shine[0] = 0;        xy_shine[1] = bh - 2;
            xy_shine[2] = 0;        xy_shine[3] = 0;
            xy_shine[4] = bw - 2;   xy_shine[5] = 0;
            b_shine->FrontPen = (UBYTE)rpens[SHINEPEN];
            b_shine->DrawMode = JAM1;
            b_shine->Count = 3;
            b_shine->XY = xy_shine;
            b_shine->NextBorder = b_shadow;

            xy_shadow[0] = bw - 1;  xy_shadow[1] = 1;
            xy_shadow[2] = bw - 1;  xy_shadow[3] = bh - 1;
            xy_shadow[4] = 1;       xy_shadow[5] = bh - 1;
            b_shadow->FrontPen = (UBYTE)rpens[SHADOWPEN];
            b_shadow->DrawMode = JAM1;
            b_shadow->Count = 3;
            b_shadow->XY = xy_shadow;
            b_shadow->NextBorder = NULL;

            it->FrontPen = (UBYTE)rpens[TEXTPEN];
            it->BackPen = (UBYTE)rpens[BACKGROUNDPEN];
            it->DrawMode = JAM1;
            it->LeftEdge = (bw - gad_text_w[i]) / 2;
            it->TopEdge = (bh - char_h) / 2;
            it->ITextFont = NULL;
            it->IText = (UBYTE *)gad_labels[i];
            it->NextText = NULL;
        }
    }

    /* ---- Step 9: Allocate EasyReqData for cleanup ---- */
    erd = (struct EasyReqData *)AllocMem(sizeof(struct EasyReqData), MEMF_PUBLIC | MEMF_CLEAR);
    if (!erd)
    {
        FreeMem(gadgets, gadget_alloc);
        FreeMem(borders, border_alloc);
        FreeMem(itexts, itext_alloc);
        FreeMem(border_xy, border_xy_alloc);
        FreeMem(body_buf, body_len + 1);
        FreeMem(gad_buf, gad_len + 1);
        return NULL;
    }
    erd->gadget_mem = gadgets;
    erd->gadget_mem_size = gadget_alloc;
    erd->text_mem = body_buf;
    erd->text_mem_size = body_len + 1;
    erd->gadget_text_mem = gad_buf;
    erd->gadget_text_mem_size = gad_len + 1;
    erd->border_mem = borders;
    erd->border_mem_size = border_alloc;
    erd->itext_mem = itexts;
    erd->itext_mem_size = itext_alloc;
    erd->border_xy_mem = border_xy;
    erd->border_xy_mem_size = border_xy_alloc;
    erd->num_gadgets = num_gadgets;

    /* ---- Step 10: Open the requester window ---- */
    memset(&nw, 0, sizeof(nw));
    nw.LeftEdge = win_x;
    nw.TopEdge = win_y;
    nw.Width = win_w;
    nw.Height = win_h;
    nw.DetailPen = 0;
    nw.BlockPen = 1;
    nw.Title = (UBYTE *)title;
    nw.Flags = WFLG_DRAGBAR | WFLG_DEPTHGADGET | WFLG_ACTIVATE | WFLG_RMBTRAP |
               WFLG_SIMPLE_REFRESH | WFLG_NOCAREREFRESH;
    /* VANILLAKEY for the keyboard shortcuts (AmigaOS 3.1) */
    nw.IDCMPFlags = IDCMP_GADGETUP | IDCMP_VANILLAKEY | idcmp;
    nw.FirstGadget = NULL;
    nw.Type = CUSTOMSCREEN;
    nw.Screen = scr;

    reqWindow = _intuition_OpenWindow(IntuitionBase, &nw);
    if (!reqWindow)
    {
        FreeMem(erd, sizeof(struct EasyReqData));
        FreeMem(gadgets, gadget_alloc);
        FreeMem(borders, border_alloc);
        FreeMem(itexts, itext_alloc);
        FreeMem(border_xy, border_xy_alloc);
        FreeMem(body_buf, body_len + 1);
        FreeMem(gad_buf, gad_len + 1);
        return (struct Window *)1; /* 1 = could not open window */
    }

    /* Store cleanup data in UserData */
    reqWindow->UserData = (BYTE *)erd;

    /* ---- Step 11: Render the requester body ---- */
    {
        struct RastPort *rp = reqWindow->RPort;
        WORD ix0 = border_left, iy0 = border_top;
        WORD ix1 = win_w - border_right - 1, iy1 = win_h - border_bottom - 1;
        WORD y;
        UWORD row_pat;
        UWORD *old_pat = rp->AreaPtrn;
        BYTE old_sz = rp->AreaPtSz;
        WORD text_x = box_x + (box_w - body_pixel_w) / 2;
        WORD text_y = box_y + char_h;
        char *p = body_buf;
        WORD line = 0;

        /* SHINEPEN/BACKGROUNDPEN dither, (x + y) odd -> SHINEPEN in window
         * coordinates; the pattern bits are aligned to bitmap columns */
        SetAPen(rp, rpens[SHINEPEN]);
        SetBPen(rp, rpens[BACKGROUNDPEN]);
        SetDrMd(rp, JAM2);
        for (y = iy0; y <= iy1; y++)
        {
            /* bit 15 of the pattern is bitmap column 0 (even) */
            BOOL shine_even = (((reqWindow->LeftEdge + y + 1) & 1) == 0);
            row_pat = shine_even ? 0xAAAA : 0x5555;
            rp->AreaPtrn = &row_pat; rp->AreaPtSz = 0;
            RectFill(rp, ix0, y, ix1, y);
        }
        rp->AreaPtrn = old_pat; rp->AreaPtSz = old_sz;

        /* recessed text frame with a BACKGROUNDPEN interior */
        SetAPen(rp, rpens[BACKGROUNDPEN]);
        RectFill(rp, box_x + 1, box_y + 1, box_x + box_w - 2, box_y + box_h - 2);
        SetAPen(rp, rpens[SHADOWPEN]);
        Move(rp, box_x, box_y + box_h - 2);
        Draw(rp, box_x, box_y);
        Draw(rp, box_x + box_w - 2, box_y);
        SetAPen(rp, rpens[SHINEPEN]);
        Move(rp, box_x + box_w - 1, box_y + 1);
        Draw(rp, box_x + box_w - 1, box_y + box_h - 1);
        Draw(rp, box_x + 1, box_y + box_h - 1);

        /* button interiors */
        SetAPen(rp, rpens[BACKGROUNDPEN]);
        for (i = 0; i < num_gadgets; i++)
            RectFill(rp, gadgets[i].LeftEdge + 1, gadgets[i].TopEdge + 1,
                     gadgets[i].LeftEdge + gadgets[i].Width - 2,
                     gadgets[i].TopEdge + gadgets[i].Height - 2);

        SetAPen(rp, rpens[TEXTPEN]);
        SetDrMd(rp, JAM1);
        while (*p)
        {
            char *line_start = p;
            WORD line_len = 0;
            while (*p && *p != '\n')
            {
                line_len++;
                p++;
            }

            if (line_len > 0)
            {
                Move(rp, text_x, text_y + line * line_h + rp->TxBaseline);
                Text(rp, (STRPTR)line_start, line_len);
            }

            if (*p == '\n') p++;
            line++;
        }
    }

    /* ---- Step 12: Add and render the buttons ---- */
    _intuition_AddGList(IntuitionBase, reqWindow, gadgets, (UWORD)~0, num_gadgets, NULL);
    _intuition_RefreshGList(IntuitionBase, gadgets, reqWindow, NULL, num_gadgets);

    DPRINTF(LOG_DEBUG, "_intuition: BuildEasyRequestArgs() created window 0x%08lx (%dx%d) with %d gadgets\n",
            (ULONG)reqWindow, win_w, win_h, num_gadgets);

    return reqWindow;
}

LONG _intuition_SysReqHandler ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Window * window __asm("a0"),
                                                        register ULONG * idcmpFlags __asm("a1"),
                                                        register LONG waitInput __asm("d0"))
{
    struct IntuiMessage *msg;
    LONG result = -2; /* No result yet */

    /* GCC m68k move.w fix: sign-extend d-register LONG param (boolean) */
    waitInput = (LONG)(WORD)waitInput;
    
    DPRINTF (LOG_DEBUG, "_intuition: SysReqHandler() window=0x%08lx wait=%ld\n", (ULONG)window, waitInput);

    if (window == NULL || window == (struct Window *)1)
        return (LONG)window;
    
    if (!window || !window->UserPort) return -1;
    
    /* Consume messages */
    while (1)
    {
        /* If waitInput is TRUE, wait for a message */
        if (waitInput && IsMsgPortEmpty(window->UserPort))
        {
            WaitPort(window->UserPort);
        }
        
        while ((msg = (struct IntuiMessage *)GetMsg(window->UserPort)))
        {
            ULONG class = msg->Class;
            /* UWORD code = msg->Code; */
            APTR iaddress = msg->IAddress;
            
            ReplyMsg((struct Message *)msg);
            
            /* AmigaOS 3.1: the class is stored only for an event the caller
             * has to handle - a gadget that ends the requester leaves it
             * untouched (reference: RequesterBasic) */
            if (idcmpFlags && class != IDCMP_GADGETUP) *idcmpFlags = class;
            
            if (class == IDCMP_GADGETUP)
            {
                struct Gadget *gad = (struct Gadget *)iaddress;
                /* Assuming GadgetID is set to 1 for Pos, 0 for Neg (or similar) */
                result = gad->GadgetID;
                return result;
            }
            else if (class == IDCMP_CLOSEWINDOW)
            {
                return 0;
            }
            else if (class == IDCMP_DISKINSERTED)
            {
                return -1;
            }
            /* any other message leaves the requester open (-2) */
        }
        
        if (!waitInput) break;
    }
    
    return result;
}

struct Window * _intuition_OpenWindowTagList ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register const struct NewWindow * newWindow __asm("a0"),
                                                        register const struct TagItem * tagList __asm("a1"))
{
    LXA_UNIMPLEMENTED("intuition", "OpenWindowTagList", "partial: ignores WA_BackFill, WA_RptQueue, WA_Pointer, WA_BusyPointer, WA_Checkmark, WA_HelpGroup (Phase 256)");

    BOOL auto_adjust = FALSE;
    BOOL pubname_missing = FALSE;   /* WA_PubScreenName not found / private */
    BOOL pubname_fallback = FALSE;  /* WA_PubScreenFallBack */
    BOOL top_specified = FALSE;
    UWORD mouse_queue = DEFAULT_MOUSEQUEUE;
    struct NewWindow nw;
    struct TagItem *tstate;
    struct TagItem *tag;
    LONG inner_width = -1;
    LONG inner_height = -1;
    WORD *zoom_coords = NULL;  /* WA_Zoom: pointer to WORD[4] {left,top,w,h} */
    
    DPRINTF(LOG_DEBUG, "_intuition: OpenWindowTagList() called, newWindow=0x%08lx, tagList=0x%08lx\n",
            (ULONG)newWindow, (ULONG)tagList);
    
    /* Start with defaults from NewWindow if provided, else use sensible defaults */
    if (newWindow)
    {
        /* Copy the NewWindow structure */
        nw = *newWindow;
    }
    else
    {
        /* Initialize with defaults matching AmigaOS/AROS behavior:
         * When newWindow is NULL, Flags start at 0.
         * Only the WA_* tags should set flags.
         * Width/Height default to ~0 (sentinel meaning "use screen dimensions").
         * DetailPen/BlockPen default to 0xFF (use screen defaults) per AROS,
         * but we use 0/1 for backward compatibility.
         */
        memset(&nw, 0, sizeof(nw));
        nw.Width = (WORD)~0;
        nw.Height = (WORD)~0;
        nw.DetailPen = 0;
        nw.BlockPen = 1;
        nw.Flags = 0;  /* Tags will set the flags */
        nw.Type = WBENCHSCREEN;  /* Default to opening on Workbench */
    }
    
    /* Process tags to override NewWindow fields */
    if (tagList)
    {
        tstate = (struct TagItem *)tagList;
        while ((tag = NextTagItem(&tstate)))
        {
            switch (tag->ti_Tag)
            {
                case WA_Left:
                    nw.LeftEdge = (WORD)tag->ti_Data;
                    break;
                case WA_Top:
                    nw.TopEdge = (WORD)tag->ti_Data;
                    top_specified = TRUE;
                    break;
                case WA_Width:
                    nw.Width = (WORD)tag->ti_Data;
                    break;
                case WA_Height:
                    nw.Height = (WORD)tag->ti_Data;
                    break;
                case WA_DetailPen:
                    nw.DetailPen = (UBYTE)tag->ti_Data;
                    break;
                case WA_BlockPen:
                    nw.BlockPen = (UBYTE)tag->ti_Data;
                    break;
                case WA_IDCMP:
                    nw.IDCMPFlags = (ULONG)tag->ti_Data;
                    break;
                case WA_Flags:
                    nw.Flags = (ULONG)tag->ti_Data;
                    break;
                case WA_Gadgets:
                    nw.FirstGadget = (struct Gadget *)tag->ti_Data;
                    break;
                case WA_Title:
                    nw.Title = (UBYTE *)tag->ti_Data;
                    break;
                case WA_CustomScreen:
                    nw.Screen = (struct Screen *)tag->ti_Data;
                    nw.Type = CUSTOMSCREEN;
                    break;
                case WA_SuperBitMap:
                    nw.BitMap = (struct BitMap *)tag->ti_Data;
                    if (tag->ti_Data)
                        nw.Flags |= WFLG_SUPER_BITMAP;
                    else
                        nw.Flags &= ~WFLG_SUPER_BITMAP;
                    break;
                case WA_MinWidth:
                    nw.MinWidth = (WORD)tag->ti_Data;
                    break;
                case WA_MinHeight:
                    nw.MinHeight = (WORD)tag->ti_Data;
                    break;
                case WA_MaxWidth:
                    nw.MaxWidth = (UWORD)tag->ti_Data;
                    break;
                case WA_MaxHeight:
                    nw.MaxHeight = (UWORD)tag->ti_Data;
                    break;
                /* Boolean flags - set or clear flags based on ti_Data */
                case WA_SizeGadget:
                    if (tag->ti_Data)
                        nw.Flags |= WFLG_SIZEGADGET;
                    else
                        nw.Flags &= ~WFLG_SIZEGADGET;
                    break;
                case WA_SizeBRight:
                    if (tag->ti_Data)
                        nw.Flags |= WFLG_SIZEBRIGHT;
                    else
                        nw.Flags &= ~WFLG_SIZEBRIGHT;
                    break;
                case WA_SizeBBottom:
                    if (tag->ti_Data)
                        nw.Flags |= WFLG_SIZEBBOTTOM;
                    else
                        nw.Flags &= ~WFLG_SIZEBBOTTOM;
                    break;
                case WA_DragBar:
                    if (tag->ti_Data)
                        nw.Flags |= WFLG_DRAGBAR;
                    else
                        nw.Flags &= ~WFLG_DRAGBAR;
                    break;
                case WA_DepthGadget:
                    if (tag->ti_Data)
                        nw.Flags |= WFLG_DEPTHGADGET;
                    else
                        nw.Flags &= ~WFLG_DEPTHGADGET;
                    break;
                case WA_CloseGadget:
                    if (tag->ti_Data)
                        nw.Flags |= WFLG_CLOSEGADGET;
                    else
                        nw.Flags &= ~WFLG_CLOSEGADGET;
                    break;
                case WA_Backdrop:
                    if (tag->ti_Data)
                        nw.Flags |= WFLG_BACKDROP;
                    else
                        nw.Flags &= ~WFLG_BACKDROP;
                    break;
                case WA_ReportMouse:
                    if (tag->ti_Data)
                        nw.Flags |= WFLG_REPORTMOUSE;
                    else
                        nw.Flags &= ~WFLG_REPORTMOUSE;
                    break;
                case WA_NoCareRefresh:
                    if (tag->ti_Data)
                        nw.Flags |= WFLG_NOCAREREFRESH;
                    else
                        nw.Flags &= ~WFLG_NOCAREREFRESH;
                    break;
                case WA_Borderless:
                    if (tag->ti_Data)
                        nw.Flags |= WFLG_BORDERLESS;
                    else
                        nw.Flags &= ~WFLG_BORDERLESS;
                    break;
                case WA_Activate:
                    if (tag->ti_Data)
                        nw.Flags |= WFLG_ACTIVATE;
                    else
                        nw.Flags &= ~WFLG_ACTIVATE;
                    break;
                case WA_RMBTrap:
                    if (tag->ti_Data)
                        nw.Flags |= WFLG_RMBTRAP;
                    else
                        nw.Flags &= ~WFLG_RMBTRAP;
                    break;
                case WA_SimpleRefresh:
                    if (tag->ti_Data)
                    {
                        nw.Flags &= ~(WFLG_SMART_REFRESH | WFLG_SUPER_BITMAP);
                        nw.Flags |= WFLG_SIMPLE_REFRESH;
                    }
                    break;
                case WA_SmartRefresh:
                    if (tag->ti_Data)
                    {
                        nw.Flags &= ~(WFLG_SIMPLE_REFRESH | WFLG_SUPER_BITMAP);
                        nw.Flags |= WFLG_SMART_REFRESH;
                    }
                    break;
                case WA_GimmeZeroZero:
                    if (tag->ti_Data)
                        nw.Flags |= WFLG_GIMMEZEROZERO;
                    else
                        nw.Flags &= ~WFLG_GIMMEZEROZERO;
                    break;
                case WA_InnerWidth:
                    inner_width = (LONG)tag->ti_Data;
                    break;
                case WA_InnerHeight:
                    inner_height = (LONG)tag->ti_Data;
                    break;
                case WA_Zoom:
                    zoom_coords = (WORD *)tag->ti_Data;
                    nw.Flags |= WFLG_HASZOOM;
                    break;
                case WA_MouseQueue:
                    mouse_queue = (UWORD)tag->ti_Data;
                    break;
                case WA_PubScreenName:
                {
                    /* Look up the named public screen and open on it.
                     * Per RKRM: WA_PubScreenName provides the name of a
                     * public screen to open the window on.  We use
                     * LockPubScreen() to find it; if not found the window
                     * falls back to the default public screen.
                     */
                    const char *pub_name = (const char *)tag->ti_Data;
                    struct Screen *pub_screen = NULL;

                    DPRINTF(LOG_DEBUG, "_intuition: OpenWindowTagList() WA_PubScreenName='%s'\n",
                            pub_name ? pub_name : "(null)");

                    if (pub_name)
                    {
                        pub_screen = _intuition_LockPubScreen(IntuitionBase, (CONST_STRPTR)pub_name);
                        if (pub_screen)
                        {
                            nw.Screen = pub_screen;
                            nw.Type = PUBLICSCREEN;
                            _intuition_UnlockPubScreen(IntuitionBase, NULL, pub_screen);
                        }
                    }

                    if (!pub_screen)
                    {
                        /* Only with WA_PubScreenFallBack (checked after
                         * the tag loop) the default public screen is used */
                        pubname_missing = TRUE;
                        pub_screen = _intuition_LockPubScreen(IntuitionBase, NULL);
                        if (pub_screen)
                        {
                            nw.Screen = pub_screen;
                            nw.Type = PUBLICSCREEN;
                            _intuition_UnlockPubScreen(IntuitionBase, NULL, pub_screen);
                        }
                    }
                    break;
                }
                case WA_PubScreen:
                    /* WA_PubScreen provides a direct pointer to a
                     * Screen obtained from LockPubScreen().  Per RKRM
                     * this directs the window to open on that screen.
                     */
                    DPRINTF(LOG_DEBUG, "_intuition: OpenWindowTagList() WA_PubScreen=0x%08lx\n",
                            (ULONG)tag->ti_Data);
                    if (tag->ti_Data)
                    {
                        nw.Screen = (struct Screen *)tag->ti_Data;
                        nw.Type = PUBLICSCREEN;
                    }
                    else
                    {
                        /* NULL WA_PubScreen means default public screen */
                        struct Screen *def_screen = _intuition_LockPubScreen(IntuitionBase, NULL);
                        if (def_screen)
                        {
                            nw.Screen = def_screen;
                            nw.Type = PUBLICSCREEN;
                            _intuition_UnlockPubScreen(IntuitionBase, NULL, def_screen);
                        }
                    }
                    break;
                case WA_PubScreenFallBack:
                    /* Fall back to the default public screen if the
                     * WA_PubScreenName screen cannot be locked. */
                    DPRINTF(LOG_DEBUG, "_intuition: OpenWindowTagList() WA_PubScreenFallBack=%ld\n",
                            (LONG)tag->ti_Data);
                    pubname_fallback = (tag->ti_Data != 0);
                    break;
                /* Tags we recognize but don't fully implement yet */
                case WA_BackFill:
                case WA_RptQueue:
                    DPRINTF(LOG_DEBUG, "_intuition: OpenWindowTagList() ignoring tag 0x%08lx (not yet implemented)\n",
                            tag->ti_Tag);
                    break;
                case WA_AutoAdjust:
                    auto_adjust = (tag->ti_Data != 0);
                    break;
                case WA_NewLookMenus:
                    if (tag->ti_Data)
                        nw.Flags |= WFLG_NEWLOOKMENUS;
                    else
                        nw.Flags &= ~WFLG_NEWLOOKMENUS;
                    break;
                case WA_ScreenTitle:
                case WA_Checkmark:
                case WA_MenuHelp:
                case WA_NotifyDepth:
                case WA_Pointer:
                case WA_BusyPointer:
                case WA_PointerDelay:
                case WA_TabletMessages:
                case WA_HelpGroup:
                case WA_HelpGroupWindow:
                    DPRINTF(LOG_DEBUG, "_intuition: OpenWindowTagList() ignoring tag 0x%08lx (not yet implemented)\n",
                            tag->ti_Tag);
                    break;
                default:
                    DPRINTF(LOG_DEBUG, "_intuition: OpenWindowTagList() unknown tag 0x%08lx\n",
                            tag->ti_Tag);
                    break;
            }
        }
    }
    
    /* Handle WA_InnerWidth / WA_InnerHeight:
     * Convert inner dimensions to outer dimensions by adding border sizes.
     * We pre-compute borders using the same logic as _intuition_OpenWindow().
     */
    if (inner_width >= 0 || inner_height >= 0)
    {
        struct Screen *screen;
        WORD border_left, border_right, border_top, border_bottom;

        /* Find the screen to get WBor* values */
        screen = nw.Screen;
        if (!screen)
        {
            /* Default to Workbench screen */
            screen = IntuitionBase->FirstScreen;
            while (screen)
            {
                if ((screen->Flags & SCREENTYPE) == WBENCHSCREEN)
                    break;
                screen = screen->NextScreen;
            }

            if (!screen && _intuition_OpenWorkBench(IntuitionBase))
            {
                screen = IntuitionBase->FirstScreen;
                while (screen)
                {
                    if ((screen->Flags & SCREENTYPE) == WBENCHSCREEN)
                        break;
                    screen = screen->NextScreen;
                }
            }
        }

        if (screen)
        {
            _window_compute_borders(screen, _window_effective_flags(nw.Flags), nw.Title,
                                    &border_left, &border_top, &border_right, &border_bottom);

            if (inner_width >= 0)
            {
                nw.Width = (WORD)(inner_width + border_left + border_right);
                DPRINTF(LOG_DEBUG, "_intuition: OpenWindowTagList() WA_InnerWidth=%ld -> Width=%d (borders: %d+%d)\n",
                        inner_width, (int)nw.Width, (int)border_left, (int)border_right);
            }
            if (inner_height >= 0)
            {
                nw.Height = (WORD)(inner_height + border_top + border_bottom);
                DPRINTF(LOG_DEBUG, "_intuition: OpenWindowTagList() WA_InnerHeight=%ld -> Height=%d (borders: %d+%d)\n",
                        inner_height, (int)nw.Height, (int)border_top, (int)border_bottom);
            }
        }
        else
        {
            LPRINTF(LOG_WARNING, "_intuition: OpenWindowTagList() WA_InnerWidth/Height ignored - no screen found\n");
        }
    }

    if (pubname_missing && !pubname_fallback)
    {
        /* AmigaOS 3.1 reference: a missing or private WA_PubScreenName
         * screen makes OpenWindowTagList() fail */
        DPRINTF(LOG_DEBUG, "_intuition: OpenWindowTagList() WA_PubScreenName screen not available\n");
        return NULL;
    }

    /* AmigaOS 3.1 reference (simplegad, gadtoolsgadgets, simplegtgadget
     * goldens): without a NewWindow and WA_Top the window opens just below
     * the screen title bar */
    if (!newWindow && !top_specified)
    {
        struct Screen *def_screen = nw.Screen;

        if (!def_screen)
            def_screen = _intuition_find_workbench_screen(IntuitionBase);
        if (!def_screen && _intuition_OpenWorkBench(IntuitionBase))
            def_screen = _intuition_find_workbench_screen(IntuitionBase);
        if (def_screen && nw.Height > 0 &&
            def_screen->BarHeight + 1 + nw.Height <= def_screen->Height)
            nw.TopEdge = def_screen->BarHeight + 1;
    }

    if (auto_adjust)
    {
        struct Screen *adjust_screen = nw.Screen;
        WORD adjust_width;
        WORD adjust_height;

        if (!adjust_screen)
        {
            adjust_screen = IntuitionBase->FirstScreen;
            while (adjust_screen)
            {
                if ((adjust_screen->Flags & SCREENTYPE) == WBENCHSCREEN)
                    break;
                adjust_screen = adjust_screen->NextScreen;
            }

            if (!adjust_screen && _intuition_OpenWorkBench(IntuitionBase))
                adjust_screen = _intuition_find_workbench_screen(IntuitionBase);
        }

        if (adjust_screen)
        {
            adjust_width = nw.Width;
            adjust_height = nw.Height;

            if (adjust_width <= 0 || adjust_width > adjust_screen->Width)
                adjust_width = adjust_screen->Width;
            if (adjust_height <= 0 || adjust_height > adjust_screen->Height)
                adjust_height = adjust_screen->Height;

            /* AmigaOS 3.1 reference (gadtoolsgadgets golden): WA_AutoAdjust
             * only moves the window into the screen, it does not place it
             * at the mouse pointer */
            if (nw.LeftEdge < 0)
                nw.LeftEdge = 0;
            if (nw.TopEdge < 0)
                nw.TopEdge = 0;

            if (nw.LeftEdge + adjust_width > adjust_screen->Width)
                nw.LeftEdge = adjust_screen->Width - adjust_width;
            if (nw.TopEdge + adjust_height > adjust_screen->Height)
                nw.TopEdge = adjust_screen->Height - adjust_height;

            if (nw.LeftEdge < 0)
                nw.LeftEdge = 0;
            if (nw.TopEdge < 0)
                nw.TopEdge = 0;
        }
    }

    nw.Flags &= ~WFLG_NW_EXTENDED;



    /* Call our existing OpenWindow with the assembled NewWindow */
    struct Window *win = _intuition_OpenWindow(IntuitionBase, &nw);

    /* AmigaOS 3.1 reference (dopus-startup): an ExtNewWindow keeps
     * WFLG_NW_EXTENDED in Window->Flags */
    if (win && newWindow && (newWindow->Flags & WFLG_NW_EXTENDED))
        win->Flags |= WFLG_NW_EXTENDED;

    if (win)
        _intuition_set_mouse_queue_value((struct LXAIntuitionBase *)IntuitionBase, win, mouse_queue);

    /* If WA_Zoom was specified, allocate and attach ZoomData */
    if (win && zoom_coords)
    {
        struct ZoomData *zd = (struct ZoomData *)AllocMem(sizeof(struct ZoomData),
                                                          MEMF_PUBLIC | MEMF_CLEAR);
        if (zd)
        {
            zd->zd_Left   = zoom_coords[0];
            zd->zd_Top    = zoom_coords[1];
            zd->zd_Width  = zoom_coords[2];
            zd->zd_Height = zoom_coords[3];
            zd->zd_IsZoomed = FALSE;
            win->ExtData = (UBYTE *)zd;
            /* the alternate box is always reachable, whatever the limits */
            if (zd->zd_Width > 0 && (UWORD)zd->zd_Width > win->MaxWidth)
                win->MaxWidth = zd->zd_Width;
            if (zd->zd_Height > 0 && (UWORD)zd->zd_Height > win->MaxHeight)
                win->MaxHeight = zd->zd_Height;
            DPRINTF(LOG_DEBUG, "_intuition: OpenWindowTagList() WA_Zoom set: alt pos=%d,%d size=%dx%d\n",
                    (int)zd->zd_Left, (int)zd->zd_Top, (int)zd->zd_Width, (int)zd->zd_Height);
        }
    }

    return win;
}

struct Screen * _intuition_OpenScreenTagList ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register const struct NewScreen * newScreen __asm("a0"),
                                                        register const struct TagItem * tagList __asm("a1"))
{
    LXA_UNIMPLEMENTED("intuition", "OpenScreenTagList", "partial: ignores SA_DClip, SA_Overscan, SA_Colors; SA_ErrorCode only for mode errors (Phase 256)");

    struct NewScreen ns;
    struct TagItem *tstate;
    struct TagItem *tag;
    ULONG sa_display_id = (ULONG)INVALID_ID;  /* Track SA_DisplayID for VPModeID override */
    ULONG *sa_error = NULL;
    
    DPRINTF(LOG_DEBUG, "_intuition: OpenScreenTagList() called, newScreen=0x%08lx, tagList=0x%08lx\n",
            (ULONG)newScreen, (ULONG)tagList);
    
    const struct TagItem *tag_lists[2];
    int tl;
    BOOL share_pens = FALSE;
    BOOL pub_name = FALSE;
    BYTE sys_font = -1;
    BOOL full_palette = FALSE;

    tag_lists[0] = NULL;
    tag_lists[1] = tagList;

    /* Start with defaults from NewScreen if provided, else use sensible defaults */
    if (newScreen)
    {
        /* Copy the NewScreen structure; when OpenScreen() hands over an
         * ExtNewScreen with its Extension as the tag list, the structure
         * itself has been consumed */
        ns = *newScreen;
        ns.Type &= ~NS_EXTENDED;
        ns.Type |= SHOWTITLE;               /* SA_ShowTitle defaults to TRUE */
        if ((newScreen->Type & NS_EXTENDED) &&
            ((const struct ExtNewScreen *)newScreen)->Extension &&
            ((const struct ExtNewScreen *)newScreen)->Extension != tagList)
            tag_lists[0] = ((const struct ExtNewScreen *)newScreen)->Extension;
    }
    else
    {
        /* Initialize with defaults */
        memset(&ns, 0, sizeof(ns));
        ns.Width = STDSCREENWIDTH;          /* the display mode's size */
        ns.Height = STDSCREENHEIGHT;
        ns.Depth = 2;
        ns.Type = CUSTOMSCREEN | SHOWTITLE;   /* SA_ShowTitle defaults to TRUE */
        ns.DetailPen = 0;
        ns.BlockPen = 1;
    }
    
    /* Process tags to override NewScreen fields */
    for (tl = 0; tl < 2; tl++)
    {
        if (!tag_lists[tl])
            continue;
        tstate = (struct TagItem *)tag_lists[tl];
        while ((tag = NextTagItem(&tstate)))
        {
            switch (tag->ti_Tag)
            {
                case SA_Left:
                    ns.LeftEdge = (WORD)tag->ti_Data;
                    break;
                case SA_Top:
                    ns.TopEdge = (WORD)tag->ti_Data;
                    break;
                case SA_Width:
                    ns.Width = (WORD)tag->ti_Data;
                    break;
                case SA_Height:
                    ns.Height = (WORD)tag->ti_Data;
                    break;
                case SA_Depth:
                    ns.Depth = (WORD)tag->ti_Data;
                    break;
                case SA_DetailPen:
                    ns.DetailPen = (UBYTE)tag->ti_Data;
                    break;
                case SA_BlockPen:
                    ns.BlockPen = (UBYTE)tag->ti_Data;
                    break;
                case SA_Title:
                    ns.DefaultTitle = (UBYTE *)tag->ti_Data;
                    break;
                case SA_Type:
                    ns.Type &= ~SCREENTYPE;
                    ns.Type |= ((UWORD)tag->ti_Data & SCREENTYPE);
                    break;
                case SA_Behind:
                    if (tag->ti_Data)
                        ns.Type |= SCREENBEHIND;
                    else
                        ns.Type &= ~SCREENBEHIND;
                    break;
                case SA_Quiet:
                    if (tag->ti_Data)
                        ns.Type |= SCREENQUIET;
                    else
                        ns.Type &= ~SCREENQUIET;
                    break;
                case SA_ShowTitle:
                    if (tag->ti_Data)
                        ns.Type |= SHOWTITLE;
                    else
                        ns.Type &= ~SHOWTITLE;
                    break;
                case SA_AutoScroll:
                    if (tag->ti_Data)
                        ns.Type |= AUTOSCROLL;
                    else
                        ns.Type &= ~AUTOSCROLL;
                    break;
                case SA_BitMap:
                    ns.CustomBitMap = (struct BitMap *)tag->ti_Data;
                    if (tag->ti_Data)
                        ns.Type |= CUSTOMBITMAP;
                    else
                        ns.Type &= ~CUSTOMBITMAP;
                    break;
                case SA_Font:
                    ns.Font = (struct TextAttr *)tag->ti_Data;
                    break;
                case SA_DisplayID:
                    /* Store the display ID; use it to set ViewPort.Modes
                     * after screen creation.  For now we extract the
                     * HIRES and LACE bits which affect resolution/height.
                     * We also preserve the full ID for VPModeID so that
                     * apps like PPaint that read VPModeID directly get
                     * back exactly what they requested.
                     */
                    sa_display_id = (ULONG)tag->ti_Data;
                    ns.ViewModes = (UWORD)(tag->ti_Data & 0xFFFF);
                    break;
                case SA_Pens:
                    /* SA_Pens handled in second pass after screen is open */
                    DPRINTF(LOG_DEBUG, "_intuition: OpenScreenTagList() SA_Pens=0x%08lx\n",
                            (ULONG)tag->ti_Data);
                    break;
                case SA_SharePens:
                    share_pens = tag->ti_Data ? TRUE : FALSE;
                    break;
                case SA_LikeWorkbench:
                    /* AmigaOS 3.1 reference (gallery-menus): a screen like
                     * the Workbench shares its pens and has its mode */
                    if (tag->ti_Data)
                    {
                        share_pens = TRUE;
                        ns.ViewModes |= HIRES;
                    }
                    break;
                case SA_PubName:
                    pub_name = tag->ti_Data ? TRUE : FALSE;
                    /* SA_PubName handled after screen registration */
                    DPRINTF(LOG_DEBUG, "_intuition: OpenScreenTagList() SA_PubName='%s'\n",
                            tag->ti_Data ? (const char *)tag->ti_Data : "(null)");
                    break;
                case SA_PubSig:
                case SA_PubTask:
                    DPRINTF(LOG_DEBUG, "_intuition: OpenScreenTagList() storing pub signal tag 0x%08lx\n",
                            tag->ti_Tag);
                    break;
                case SA_Colors32:
                    /* SA_Colors32 handled after screen is open */
                    DPRINTF(LOG_DEBUG, "_intuition: OpenScreenTagList() SA_Colors32=0x%08lx\n",
                            (ULONG)tag->ti_Data);
                    break;
                case SA_SysFont:
                    sys_font = (BYTE)tag->ti_Data;
                    break;
                case SA_FullPalette:
                    full_palette = tag->ti_Data ? TRUE : FALSE;
                    break;
                /* Tags we recognize but don't fully implement yet */
                case SA_ErrorCode:
                    sa_error = (ULONG *)tag->ti_Data;
                    break;
                case SA_DClip:
                case SA_Overscan:
                case SA_Colors:
                    DPRINTF(LOG_DEBUG, "_intuition: OpenScreenTagList() ignoring tag 0x%08lx (not yet implemented)\n",
                            tag->ti_Tag);
                    break;
                default:
                    DPRINTF(LOG_DEBUG, "_intuition: OpenScreenTagList() unknown tag 0x%08lx\n",
                            tag->ti_Tag);
                    break;
            }
        }
    }
    
    /* AmigaOS 3.1 reference (tests/probes/intuition/screenmodes): a
     * display ID the machine does not have fails the open - IDs of an
     * unknown monitor (RTG, DblPAL without DEVS:Monitors) with
     * OSERR_UNKNOWNMODE, NTSC modes without ntsc.monitor with
     * OSERR_NOMONITOR.  (ASM-One asks for a screen mode only when its
     * default RTG screen fails to open.) */
    if (sa_display_id != (ULONG)INVALID_ID)
    {
        ULONG na = (ULONG)ModeNotAvailable(sa_display_id);
        if (na)
        {
            if (sa_error)
                *sa_error = (na == 0xffffffffUL) ? OSERR_UNKNOWNMODE :
                            (na & DI_AVAIL_NOMONITOR) ? OSERR_NOMONITOR : OSERR_NOCHIPS;
            return NULL;
        }
    }

    /* Call our existing OpenScreen with the assembled NewScreen */
    g_screen_from_tags = TRUE;
    g_screen_display_id = (sa_display_id != (ULONG)INVALID_ID) ? sa_display_id + 1 : 0;
    g_screen_sysfont = sys_font;
    g_screen_full_palette = full_palette;
    struct Screen *screen = _intuition_OpenScreen(IntuitionBase, &ns);
    g_screen_from_tags = FALSE;
    g_screen_display_id = 0;
    g_screen_sysfont = -1;
    g_screen_full_palette = FALSE;
    if (!screen)
    {
        if (sa_error)
            *sa_error = OSERR_NOMEM;
        return NULL;
    }

    /* AmigaOS 3.1 reference (dopus-startup, gallery-menus): a screen opened
     * with SA_PubName is a PUBLICSCREEN, SA_SharePens sets PENSHARED */
    if (pub_name)
        screen->Flags = (screen->Flags & ~SCREENTYPE) | PUBLICSCREEN;
    if (share_pens)
        screen->Flags |= PENSHARED;

    /* If SA_DisplayID was provided, override VPModeID in the ColorMap with the
     * exact requested display ID.  Apps like PPaint's CloantoScreenManager read
     * VPModeID back after OpenScreen and compare it to what they requested.
     * OpenScreen sets VPModeID from width (PAL_MONITOR_ID|key), but apps may
     * have requested just the raw key (e.g. 0x8000 for HIRES) without a monitor
     * prefix.  Storing back the requested ID preserves that expectation.
     */
    if (sa_display_id != (ULONG)INVALID_ID && screen->ViewPort.ColorMap)
    {
        DPRINTF(LOG_DEBUG, "_intuition: OpenScreenTagList() overriding VPModeID 0x%08lx -> 0x%08lx\n",
                (ULONG)screen->ViewPort.ColorMap->VPModeID, sa_display_id);
        screen->ViewPort.ColorMap->VPModeID = sa_display_id;
        _intuition_update_display_clip(screen);
    }

    /* Second pass: apply tags that require a live screen */
    if (tag_lists[0] || tag_lists[1])
    {
        struct LXAIntuitionBase *base = (struct LXAIntuitionBase *)IntuitionBase;
        struct LXAPubScreenNode *lxa_pub = NULL;
        struct PubScreenNode *pub_node;
        const UWORD *sa_pens = NULL;
        const char *sa_pub_name = NULL;
        const ULONG *sa_colors32 = NULL;
        APTR sa_pub_task = NULL;
        BYTE sa_pub_sig = -1;

        pub_node = _intuition_find_pubscreen_by_screen(base, screen);
        if (pub_node)
            lxa_pub = (struct LXAPubScreenNode *)pub_node;

        for (tl = 0; tl < 2; tl++)
        {
        if (!tag_lists[tl])
            continue;
        tstate = (struct TagItem *)tag_lists[tl];
        while ((tag = NextTagItem(&tstate)))
        {
            switch (tag->ti_Tag)
            {
                case SA_Pens:
                    sa_pens = (const UWORD *)tag->ti_Data;
                    break;
                case SA_PubName:
                    sa_pub_name = (const char *)tag->ti_Data;
                    break;
                case SA_PubTask:
                    sa_pub_task = (APTR)tag->ti_Data;
                    break;
                case SA_PubSig:
                    sa_pub_sig = (BYTE)tag->ti_Data;
                    break;
                case SA_Colors32:
                    sa_colors32 = (const ULONG *)tag->ti_Data;
                    break;
                default:
                    break;
            }
        }
        }

        /* Apply SA_Pens: merge caller's pens with defaults.
         * Per RKRM: providing SA_Pens (even with just a ~0 terminator)
         * enables NewLook (DRIF_NEWLOOK).  Each pen in the caller's
         * array overrides the corresponding default, up to the ~0
         * terminator.
         */
        if (lxa_pub && sa_pens)
        {
            UWORD i;
            const UWORD *def = lxa_default_pens();

            /* SA_Pens selects the new look: the Workbench default pens,
             * overridden by the caller's pens up to the ~0 terminator */
            lxa_pub->has_custom_pens = TRUE;
            lxa_pub->drawInfo.dri_Flags |= DRIF_NEWLOOK;
            for (i = 0; i < NUMDRIPENS; i++)
                lxa_pub->pens[i] = def[i];
            /* AmigaOS 3.1: DETAILPEN/BLOCKPEN default to the screen's
             * DetailPen/BlockPen (tests/probes/intuition/screens) */
            lxa_pub->pens[DETAILPEN] = screen->DetailPen;
            lxa_pub->pens[BLOCKPEN] = screen->BlockPen;
            for (i = 0; i < NUMDRIPENS; i++)
            {
                if (sa_pens[i] == (UWORD)~0)
                    break;
                lxa_pub->pens[i] = sa_pens[i];
            }

            /* a V37-style array (1 to 9 pens, no bar pens): the bar keeps
             * the screen's DetailPen/BlockPen (tests/probes/intuition/screens) */
            if (i > 0 && i <= BARDETAILPEN)
            {
                lxa_pub->pens[BARDETAILPEN] = screen->DetailPen;
                lxa_pub->pens[BARBLOCKPEN] = screen->BlockPen;
                lxa_pub->pens[BARTRIMPEN] = screen->BlockPen;
            }
            DPRINTF(LOG_DEBUG, "_intuition: OpenScreenTagList() applied %d custom pens\n", (int)i);
            if (screen->Flags & SHOWTITLE)
                _render_screen_title_bar(screen);
        }

        /* Apply SA_PubName: re-register the screen with the requested
         * public name so LockPubScreen() can find it.
         */
        if (sa_pub_name && sa_pub_name[0] != '\0' && lxa_pub)
        {
            ULONG name_len = strlen(sa_pub_name);
            char *new_name;

            /* Allocate a separate name buffer and update the node */
            new_name = (char *)AllocMem(name_len + 1, MEMF_PUBLIC);
            if (new_name)
            {
                strcpy(new_name, sa_pub_name);
                /* The old name was inline after the struct, we can't free
                 * it separately, but we can point away from it.
                 */
                lxa_pub->pub.psn_Node.ln_Name = new_name;

                DPRINTF(LOG_DEBUG, "_intuition: OpenScreenTagList() registered pub screen name='%s'\n",
                        sa_pub_name);
            }

            _intuition_publish_screen(base, lxa_pub);

            if (sa_pub_task)
                lxa_pub->pub.psn_SigTask = (struct Task *)sa_pub_task;
            if (sa_pub_sig >= 0)
                lxa_pub->pub.psn_SigBit = (UBYTE)sa_pub_sig;
        }

        /* Apply SA_Colors32: load a palette from a LoadRGB32-style array.
         * Format: { numcolors_high16 | first_color_low16,
         *           r32, g32, b32, r32, g32, b32, ..., 0 }
         */
        if (sa_colors32 && screen->ViewPort.ColorMap)
        {
            ULONG display_handle = (ULONG)screen->ExtData;
            const ULONG *ptr = sa_colors32;

            while (*ptr != 0)
            {
                UWORD num_colors = (UWORD)(*ptr >> 16);
                UWORD first_color = (UWORD)(*ptr & 0xFFFF);

                ptr++;  /* skip header word */

                for (UWORD c = 0; c < num_colors; c++)
                {
                    UWORD color_index = first_color + c;
                    UBYTE r8 = (UBYTE)(ptr[0] >> 24);
                    UBYTE g8 = (UBYTE)(ptr[1] >> 24);
                    UBYTE b8 = (UBYTE)(ptr[2] >> 24);
                    ULONG rgb24 = ((ULONG)r8 << 16) | ((ULONG)g8 << 8) | (ULONG)b8;
                    UBYTE r4 = r8 >> 4;
                    UBYTE g4 = g8 >> 4;
                    UBYTE b4 = b8 >> 4;

                    SetRGB4CM(screen->ViewPort.ColorMap, color_index, r4, g4, b4);

                    if (display_handle)
                        emucall3(EMU_CALL_GFX_SET_COLOR, display_handle, color_index, rgb24);

                    ptr += 3;
                }
            }

            DPRINTF(LOG_DEBUG, "_intuition: OpenScreenTagList() applied SA_Colors32 palette\n");
        }
    }

    _intuition_setup_palextra(screen);

    return screen;
}

/* Helper to check if a pointer is likely a BOOPSI object */
static BOOL _is_object(struct LXAIntuitionBase *base, APTR ptr)
{
    struct _Object *obj;
    struct Node *node;
    struct IClass *cls;
    
    if (!ptr || !base) return FALSE;
    
    /* Check alignment */
    if ((ULONG)ptr & 3) return FALSE;
    
    /* Peek at potential object header */
    obj = _OBJECT(ptr);
    
    /* Check if memory is readable (hard to do portably, but we can check basic validity) */
    /* Check if o_Class looks like a pointer */
    if ((ULONG)obj->o_Class & 1) return FALSE;
    
    cls = obj->o_Class;
    if (!cls) return FALSE;
    
    /* Verify if cls is in our known class list */
    node = base->ClassList.lh_Head;
    while (node && node->ln_Succ) {
        struct LXAClassNode *entry = (struct LXAClassNode *)node;
        if (entry->class_ptr == cls) {
            return TRUE;
        }
        node = node->ln_Succ;
    }
    
    /* Also check internal classes that might not be in list yet? 
     * They are added to list in InitLib.
     */
     
    return FALSE;
}

/* Helper to draw a bevel box (simple 3D frame) */
static void _draw_bevel_box(struct RastPort *rp, WORD left, WORD top, WORD width, WORD height, 
                           ULONG flags, const struct DrawInfo *drInfo)
{
    UBYTE shiPen = 2; /* Default Shine */
    UBYTE shaPen = 1; /* Default Shadow */
    
    if (drInfo) {
        shiPen = drInfo->dri_Pens[SHINEPEN];
        shaPen = drInfo->dri_Pens[SHADOWPEN];
    }
    
    WORD x0 = left;
    WORD y0 = top;
    WORD x1 = left + width - 1;
    WORD y1 = top + height - 1;
    
    /* Flags could indicate recessed vs raised. 
     * Default to raised.
     * IDS_SELECTED or similar might invert.
     */
    BOOL recessed = (flags & IDS_SELECTED) ? TRUE : FALSE;
    
    UBYTE topPen = recessed ? shaPen : shiPen;
    UBYTE botPen = recessed ? shiPen : shaPen;
    
    /* Top/Left */
    SetAPen(rp, topPen);
    Move(rp, x0, y1);
    Draw(rp, x0, y0);
    Draw(rp, x1, y0);
    
    /* Bottom/Right */
    SetAPen(rp, botPen);
    Draw(rp, x1, y1);
    Draw(rp, x0, y1);
    
    /* Fill? usually caller handles content */
}

VOID _intuition_DrawImageState ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct RastPort * rp __asm("a0"),
                                                        register struct Image * image __asm("a1"),
                                                        register WORD leftOffset __asm("d0"),
                                                        register WORD topOffset __asm("d1"),
                                                        register ULONG state __asm("d2"),
                                                        register const struct DrawInfo * drawInfo __asm("a2"))
{
    struct LXAIntuitionBase *base = (struct LXAIntuitionBase *)IntuitionBase;
    UBYTE savedAPen;
    UBYTE savedBPen;
    UBYTE savedDrMd;
    
    DPRINTF (LOG_DEBUG, "_intuition: DrawImageState() image=0x%08lx state=0x%08lx\n", (ULONG)image, state);
    
    if (!image || !rp) return;

    savedAPen = rp->FgPen;
    savedBPen = rp->BgPen;
    savedDrMd = rp->DrawMode;

    if (rp->Layer)
        LockLayerRom(rp->Layer);

    while (image)
    {
        BOOL is_boopsi = (image->Depth == CUSTOMIMAGEDEPTH) || _is_object(base, image);

        if (is_boopsi) {
            struct impDraw imp;

            imp.MethodID = IM_DRAW;
            imp.imp_RPort = rp;
            imp.imp_Offset.X = leftOffset;
            imp.imp_Offset.Y = topOffset;
            imp.imp_State = state;
            imp.imp_DrInfo = (struct DrawInfo *)drawInfo;

            _intuition_dispatch_method(_OBJECT(image)->o_Class, (Object *)image, (Msg)&imp);
        } else if (image->Width > 0 && image->Height > 0) {
            ULONG planepick = image->PlanePick;
            ULONG planeonoff = image->PlaneOnOff & ~planepick;

            if (planepick == 0) {
                SetAPen(rp, planeonoff);
                RectFill(rp,
                         leftOffset + image->LeftEdge,
                         topOffset + image->TopEdge,
                         leftOffset + image->LeftEdge + image->Width - 1,
                         topOffset + image->TopEdge + image->Height - 1);
            } else if (image->ImageData) {
                struct BitMap bitmap;
                WORD plane_size = ((image->Width + 15) >> 4) * image->Height;
                WORD depth = 1;
                WORD image_depth = image->Depth;
                WORD plane;
                WORD image_plane_index = 0;
                ULONG shift = 1;

                if (rp->BitMap && rp->BitMap->Depth > 0)
                    depth = rp->BitMap->Depth;
                if (depth > 8)
                    depth = 8;

                InitBitMap(&bitmap, depth, image->Width, image->Height);

                for (plane = 0; plane < depth; plane++)
                {
                    if ((image_depth > 0) && (planepick & shift))
                    {
                        image_depth--;
                        bitmap.Planes[plane] = (PLANEPTR)(image->ImageData + (plane_size * image_plane_index));
                        image_plane_index++;
                    }
                    else
                    {
                        bitmap.Planes[plane] = (planeonoff & shift) ? (PLANEPTR)-1 : NULL;
                    }

                    shift <<= 1;
                }

                BltBitMapRastPort(&bitmap,
                                  0,
                                  0,
                                  rp,
                                  leftOffset + image->LeftEdge,
                                  topOffset + image->TopEdge,
                                  image->Width,
                                  image->Height,
                                  0xC0);   /* AmigaOS 3.1: state is ignored for standard images */
            }
        }

        image = image->NextImage;
    }

    SetAPen(rp, savedAPen);
    SetBPen(rp, savedBPen);
    SetDrMd(rp, savedDrMd);

    if (rp->Layer)
        UnlockLayerRom(rp->Layer);
}

BOOL _intuition_PointInImage ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register ULONG point __asm("d0"),
                                                        register struct Image * image __asm("a0"))
{
    LXA_UNIMPLEMENTED("intuition", "PointInImage", "stub: always TRUE (Phase 256)");

    /* point is packed Y | X<<16 ? No, Amiga Point is struct { WORD x, y }.
     * But passed in d0? D0 is 32-bit. Usually "point" args are passed as separate registers or a pointer.
     * Stub says "ULONG point".
     * Let's assume standard calling convention for PointInImage: d0 = point (y | x<<16) ?
     * RKRM: "ULONG point". "The point is relative to the image's top-left."
     */
    
    /* Stub for now */
    return TRUE; 
}

VOID _intuition_EraseImage ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct RastPort * rp __asm("a0"),
                                                        register struct Image * image __asm("a1"),
                                                        register WORD leftOffset __asm("d0"),
                                                        register WORD topOffset __asm("d1"))
{
    struct LXAIntuitionBase *base = (struct LXAIntuitionBase *)IntuitionBase;

    DPRINTF (LOG_DEBUG, "_intuition: EraseImage() image=0x%08lx\n", (ULONG)image);
    
    if (!image || !rp) return;
    
    if ((image->Depth == CUSTOMIMAGEDEPTH) || _is_object(base, image)) {
        /* Dispatch IM_ERASE */
        struct impErase imp;
        imp.MethodID = IM_ERASE;
        imp.imp_RPort = rp;
        imp.imp_Offset.X = leftOffset;
        imp.imp_Offset.Y = topOffset;
        /* impErase doesn't have dimensions? It should. 
         * Checking include/intuition/imageclass.h...
         * struct impErase { ULONG MethodID; struct RastPort *imp_RPort; 
         *                   struct { WORD X; WORD Y; } imp_Offset; };
         * No dimensions. Object implies size.
         */
         
        _intuition_dispatch_method(_OBJECT(image)->o_Class, (Object *)image, (Msg)&imp);
    } else {
        /* Standard Image: erase the box of every image in the NextImage
         * chain (AmigaOS 3.1 reference) through the layer backfill,
         * without touching the RastPort's pens. */
        for (; image; image = image->NextImage)
        {
            if (image->Width <= 0 || image->Height <= 0)
                continue;
            EraseRect(rp, leftOffset + image->LeftEdge,
                          topOffset + image->TopEdge,
                          leftOffset + image->LeftEdge + image->Width - 1,
                          topOffset + image->TopEdge + image->Height - 1);
        }
    }
}

static struct IClass *_intuition_find_class(struct LXAIntuitionBase *base, CONST_STRPTR classID)
{
    struct Node *node;

    if (!base || !classID)
        return NULL;

    node = base->ClassList.lh_Head;
    while (node && node->ln_Succ) {
        struct LXAClassNode *entry = (struct LXAClassNode *)node;
        if (entry->class_ptr && entry->class_ptr->cl_ID &&
            strcmp((const char *)entry->class_ptr->cl_ID, (const char *)classID) == 0) {
            return entry->class_ptr;
        }
        node = node->ln_Succ;
    }

    return NULL;
}

/* TRUE if cl is, or is derived from, one of the ROM's gadget classes
 * that draw themselves on OM_SET on AmigaOS 3.1 (buttongclass, propgclass,
 * strgclass) */
static BOOL _intuition_is_builtin_gadget_class(struct IClass *cl)
{
    for (; cl; cl = cl->cl_Super)
    {
        ULONG (*entry)() = cl->cl_Dispatcher.h_Entry;
        if (entry == (ULONG (*)())buttongclass_dispatch ||
            entry == (ULONG (*)())propgclass_dispatch ||
            entry == (ULONG (*)())strgclass_dispatch)
            return TRUE;
    }
    return FALSE;
}

static ULONG _intuition_dispatch_method(struct IClass *cl, Object *obj, Msg msg)
{
    if (!cl || !cl->cl_Dispatcher.h_Entry) {
        return 0;
    }

    {
        typedef ULONG (*DispatchEntry)(register struct IClass *cl __asm("a0"),
                                       register Object *obj __asm("a2"),
                                       register Msg msg __asm("a1"));
        DispatchEntry entry = (DispatchEntry)cl->cl_Dispatcher.h_Entry;
        return entry(cl, obj, msg);
    }
}

APTR _intuition_NewObjectA ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct IClass * classPtr __asm("a0"),
                                                        register CONST_STRPTR classID __asm("a1"),
                                                        register const struct TagItem * tagList __asm("a2"))
{
    struct LXAIntuitionBase *base = (struct LXAIntuitionBase *)IntuitionBase;
    struct IClass *use_class = classPtr;
    struct opSet op;

    /*
     * NewObjectA() creates a new BOOPSI object.
     * 
     * We provide minimal support for sysiclass to allow apps that need
     * system imagery (menu checkmarks, Amiga key, etc.) to continue.
     */
    DPRINTF (LOG_DEBUG, "_intuition: NewObjectA() classPtr=0x%08lx classID='%s'\n",
             (ULONG)classPtr, classID ? (const char*)classID : "(null)");

    if (!use_class && classID)
        use_class = _intuition_find_class(base, classID);

    if (use_class) {
        /* OM_NEW goes to the class with the class itself as the object;
         * rootclass allocates (see rootclass_dispatch) */
        op.MethodID = OM_NEW;
        op.ops_AttrList = (struct TagItem *)tagList;
        op.ops_GInfo = NULL;
        return (APTR)_intuition_dispatch_method(use_class, (Object *)use_class, (Msg)&op);
    }

    /* Unknown class - return NULL */
    DPRINTF (LOG_DEBUG, "_intuition: NewObjectA() unknown class, returning NULL\n");
    return NULL;
}

VOID _intuition_DisposeObject ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register APTR object __asm("a0"))
{
    /* DisposeObject() sends OM_DISPOSE to the object's class; it travels
     * up the class chain and rootclass frees the instance. */
    DPRINTF (LOG_DEBUG, "_intuition: DisposeObject() object=0x%08lx\n", (ULONG)object);

    if (!object || !_OBJECT(object)->o_Class)
        return;

    {
        struct { ULONG MethodID; } dispose_msg;
        dispose_msg.MethodID = OM_DISPOSE;
        _intuition_dispatch_method(_OBJECT(object)->o_Class, (Object *)object, (Msg)&dispose_msg);
    }
}

ULONG _intuition_SetAttrsA ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register APTR object __asm("a0"),
                                                        register const struct TagItem * tagList __asm("a1"))
{
    struct _Object *obj_data;
    struct IClass *cl;
    struct opSet op;

    if (!object)
        return 0;

    obj_data = _OBJECT(object);
    cl = obj_data->o_Class;
    if (!cl)
        return 0;

    op.MethodID = OM_SET;
    op.ops_AttrList = (struct TagItem *)tagList;
    op.ops_GInfo = NULL;

    return _intuition_dispatch_method(cl, (Object *)object, (Msg)&op);
}

ULONG _intuition_GetAttr ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register ULONG attrID __asm("d0"),
                                                        register APTR object __asm("a0"),
                                                        register ULONG * storagePtr __asm("a1"))
{
    struct _Object *obj_data;
    struct IClass *cl;
    struct opGet op;

    if (!object || !storagePtr)
        return FALSE;

    obj_data = _OBJECT(object);
    cl = obj_data->o_Class;
    if (!cl)
        return FALSE;

    op.MethodID = OM_GET;
    op.opg_AttrID = attrID;
    op.opg_Storage = storagePtr;

    return _intuition_dispatch_method(cl, (Object *)object, (Msg)&op);
}

ULONG _intuition_SetGadgetAttrsA ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Gadget * gadget __asm("a0"),
                                                        register struct Window * window __asm("a1"),
                                                        register struct Requester * requester __asm("a2"),
                                                        register const struct TagItem * tagList __asm("a3"))
{
    /*
     * SetGadgetAttrsA() sets attributes on a BOOPSI gadget by dispatching
     * OM_SET with a GadgetInfo derived from the window/requester context.
     */
    DPRINTF (LOG_DEBUG, "_intuition: SetGadgetAttrsA() gadget=0x%08lx window=0x%08lx\n",
             (ULONG)gadget, (ULONG)window);
    
    if (!gadget || !tagList)
        return 0;
    
    /* Build a GadgetInfo from window context */
    struct GadgetInfo gi;
    memset(&gi, 0, sizeof(gi));
    
    if (window)
    {
        gi.gi_Screen = window->WScreen;
        gi.gi_Window = window;
        gi.gi_Requester = requester;
        gi.gi_RastPort = window->RPort;
        gi.gi_Layer = window->WLayer;
        _intuition_gadget_domain(window, &gi.gi_Domain);
        gi.gi_DrInfo = _intuition_GetScreenDrawInfo(IntuitionBase, window->WScreen);
    }
    
    /* Dispatch OM_SET to the gadget */
    struct opSet ops;
    ops.MethodID = OM_SET;
    ops.ops_AttrList = (struct TagItem *)tagList;
    ops.ops_GInfo = &gi;
    
    struct IClass *cl = OCLASS((Object *)gadget);
    if (!cl)
        return 0;
    
    ULONG result = _intuition_dispatch_method(cl, (Object *)gadget, (Msg)&ops);

    /* SetGadgetAttrsA() itself renders nothing on AmigaOS 3.1: a class
     * redraws itself in OM_SET when it gets a GadgetInfo
     * (tests/probes/intuition/gadinfo: a gadgetclass subclass sees no
     * GM_RENDER).  lxa's built-in classes do not, so they are re-rendered
     * here on their behalf (and then report no further refresh need). */
    if (result && window && _intuition_is_builtin_gadget_class(cl))
    {
        struct RastPort *rp = window->RPort;
        if (rp)
        {
            struct gpRender gpr;
            gpr.MethodID = GM_RENDER;
            gpr.gpr_GInfo = &gi;
            gpr.gpr_RPort = rp;
            gpr.gpr_Redraw = GREDRAW_UPDATE;
            _intuition_dispatch_method(cl, (Object *)gadget, (Msg)&gpr);
            result = 0;
        }
    }
    
    if (window && gi.gi_DrInfo)
        _intuition_FreeScreenDrawInfo(IntuitionBase, window->WScreen, gi.gi_DrInfo);
    
    return result;
}

APTR _intuition_NextObject ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register APTR objectPtrPtr __asm("a0"))
{
    struct _Object *nextobject;
    APTR oldobject;

    DPRINTF (LOG_DEBUG, "_intuition: NextObject() objectPtrPtr=0x%08lx\n", (ULONG)objectPtrPtr);

    IntuitionBase = IntuitionBase;

    if (!objectPtrPtr)
        return NULL;

    if (!*((struct _Object **)objectPtrPtr))
        return NULL;

    nextobject = (struct _Object *)(*((struct _Object **)objectPtrPtr))->o_Node.mln_Succ;
    if (nextobject)
    {
        oldobject = BASEOBJECT(*((struct _Object **)objectPtrPtr));
        *((struct _Object **)objectPtrPtr) = nextobject;
    }
    else
    {
        oldobject = NULL;
    }

    return oldobject;
}

VOID _intuition_private2 ( register struct IntuitionBase * IntuitionBase __asm("a6"))
{
    PRIVATE_FUNCTION_ERROR("_intuition", "private2");
}

struct IClass * _intuition_MakeClass ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                       register CONST_STRPTR classID __asm("a0"),
                                       register CONST_STRPTR superClassID __asm("a1"),
                                       register struct IClass * superClassPtr __asm("a2"),
                                       register UWORD instanceSize __asm("d0"),
                                       register ULONG flags __asm("d1"))
{
    struct LXAIntuitionBase *base = (struct LXAIntuitionBase *)IntuitionBase;
    struct IClass *superClass = superClassPtr;
    
    DPRINTF(LOG_DEBUG, "_intuition: MakeClass('%s', superId='%s', super=0x%08lx, size=%d flags=0x%08lx)\n",
            classID ? (char *)classID : "NULL",
            superClassID ? (char *)superClassID : "NULL",
            (ULONG)superClass, instanceSize, flags);

    if (!base)
        return NULL;

    /* Determine superclass. Public superclasses must resolve by name, while
     * private classes must be provided explicitly via superClassPtr.
     */
    if (superClassID)
    {
        superClass = _intuition_find_class(base, superClassID);
        if (!superClass)
            return NULL;
    }

    /* AmigaOS 3.1 reference: without any superclass MakeClass() still
     * creates a (root) class; cl_Super stays NULL. */

    if (classID && _intuition_find_class(base, classID))
        return NULL;
    
    /* Calculate allocation size */
    /* We allocate: IClass + ClassID string */
    ULONG nameLen = classID ? strlen((char *)classID) + 1 : 0;
    struct IClass *cl = AllocMem(sizeof(struct IClass) + nameLen, MEMF_PUBLIC | MEMF_CLEAR);
    if (!cl) return NULL;
    
    /* Initialize class */
    /* cl->cl_Next is NULL */
    
    if (classID) {
        UBYTE *id = (UBYTE *)(cl + 1);
        strcpy((char *)id, (char *)classID);
        cl->cl_ID = (ClassID)id;
    }
    
    cl->cl_Super = superClass;
    cl->cl_Reserved = 0;
    cl->cl_InstSize = instanceSize;
    if (superClass)
        cl->cl_InstOffset = superClass->cl_InstOffset + superClass->cl_InstSize;
    else
        cl->cl_InstOffset = 0;
    
    if (superClass)
        superClass->cl_SubclassCount++;
    
    DPRINTF(LOG_DEBUG, "_intuition: MakeClass created 0x%08lx, instance size=%d\n", (ULONG)cl, cl->cl_InstSize);
    return cl;
}


VOID _intuition_AddClass ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct IClass * classPtr __asm("a0"))
{
    struct LXAIntuitionBase *base = (struct LXAIntuitionBase *)IntuitionBase;

    if (!classPtr || !classPtr->cl_ID)
        return;

    if (classPtr->cl_Flags & CLF_INLIST)
        return;

    {
        struct LXAClassNode *node = AllocMem(sizeof(struct LXAClassNode), MEMF_PUBLIC | MEMF_CLEAR);
        if (!node)
            return;

        node->class_ptr = classPtr;
        node->node.ln_Type = NT_UNKNOWN;
        node->node.ln_Name = (char *)classPtr->cl_ID;
        AddTail(&base->ClassList, &node->node);
        classPtr->cl_Flags |= CLF_INLIST;
    }
}

struct DrawInfo * _intuition_GetScreenDrawInfo ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Screen * screen __asm("a0"))
{
    struct LXAIntuitionBase *base = (struct LXAIntuitionBase *)IntuitionBase;
    struct PubScreenNode *pub;
    struct LXAPubScreenNode *lxa_pub;
    struct DrawInfo *drawInfo;
    UWORD *pens;

    DPRINTF (LOG_DEBUG, "_intuition: GetScreenDrawInfo() screen=0x%08lx\n", (ULONG)screen);

    if (!screen)
        return NULL;

    /*
     * GetScreenDrawInfo returns information about the screen's drawing pens
     * and font.  We allocate a fresh DrawInfo and populate it from the
     * per-screen pens stored in the LXAPubScreenNode (if one exists).
     */

    drawInfo = AllocMem(sizeof(struct DrawInfo), MEMF_CLEAR | MEMF_PUBLIC);
    if (!drawInfo)
        return NULL;

    pens = AllocMem((NUMDRIPENS + 2) * sizeof(UWORD), MEMF_PUBLIC);
    if (!pens) {
        FreeMem(drawInfo, sizeof(struct DrawInfo));
        return NULL;
    }

    /* Check if this screen has a PubScreenNode with custom pens */
    pub = _intuition_find_pubscreen_by_screen(base, screen);
    if (pub)
    {
        UWORD i;
        lxa_pub = (struct LXAPubScreenNode *)pub;

        /* Copy pens from the per-screen storage */
        for (i = 0; i < NUMDRIPENS; i++)
            pens[i] = lxa_pub->pens[i];
        pens[NUMDRIPENS] = (UWORD)~0;  /* Terminator */

        drawInfo->dri_Flags = lxa_pub->drawInfo.dri_Flags;
    }
    else
    {
        /* No PubScreenNode: use defaults */
        pens[DETAILPEN]        = 0;
        pens[BLOCKPEN]         = 1;
        pens[TEXTPEN]          = 1;
        pens[SHINEPEN]         = 2;
        pens[SHADOWPEN]        = 1;
        pens[FILLPEN]          = 3;
        pens[FILLTEXTPEN]      = 1;
        pens[BACKGROUNDPEN]    = 0;
        pens[HIGHLIGHTTEXTPEN] = 2;
        pens[BARDETAILPEN]     = 1;
        pens[BARBLOCKPEN]      = 2;
        pens[BARTRIMPEN]       = 1;
        pens[BARCONTOURPEN]    = 1;
        pens[NUMDRIPENS]       = (UWORD)~0;
        drawInfo->dri_Flags = DRIF_NEWLOOK;
    }

    drawInfo->dri_Version    = 2;
    drawInfo->dri_NumPens    = 12;     /* AmigaOS 3.1 (V40) */
    drawInfo->dri_Pens       = pens;
    drawInfo->dri_Font       = screen->RastPort.Font;
    drawInfo->dri_Depth      = screen->RastPort.BitMap ? screen->RastPort.BitMap->Depth : 2;
    drawInfo->dri_Resolution.X = (screen->Flags & SCREENHIRES) ? 22 : 44;
    drawInfo->dri_Resolution.Y = (screen->ViewPort.Modes & LACE) ? 22 : 44;
    drawInfo->dri_CheckMark  = NULL;
    drawInfo->dri_AmigaKey   = NULL;

    DPRINTF (LOG_DEBUG, "_intuition: GetScreenDrawInfo() -> drawInfo=0x%08lx, Version=%d, NumPens=%d, Font=0x%08lx, Depth=%d, Pens=0x%08lx, Flags=0x%lx\n",
             (ULONG)drawInfo, drawInfo->dri_Version, drawInfo->dri_NumPens,
             (ULONG)drawInfo->dri_Font, drawInfo->dri_Depth, (ULONG)drawInfo->dri_Pens, drawInfo->dri_Flags);
    return drawInfo;
}

VOID _intuition_FreeScreenDrawInfo ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Screen * screen __asm("a0"),
                                                        register struct DrawInfo * drawInfo __asm("a1"))
{
    DPRINTF (LOG_DEBUG, "_intuition: FreeScreenDrawInfo() screen=0x%08lx drawInfo=0x%08lx\n",
             (ULONG)screen, (ULONG)drawInfo);
    /*
     * FreeScreenDrawInfo releases a DrawInfo obtained from GetScreenDrawInfo.
     * Since we always allocate a fresh copy, we always free it here.
     */
    if (drawInfo) {
        if (drawInfo->dri_Pens)
            FreeMem(drawInfo->dri_Pens, (NUMDRIPENS + 2) * sizeof(UWORD));
        FreeMem(drawInfo, sizeof(struct DrawInfo));
    }
}

BOOL _intuition_ResetMenuStrip ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Window * window __asm("a0"),
                                                        register struct Menu * menu __asm("a1"))
{
    DPRINTF (LOG_DEBUG, "_intuition: ResetMenuStrip() window=0x%08lx menu=0x%08lx\n",
             (ULONG)window, (ULONG)menu);

    if (!window)
    {
        LPRINTF (LOG_ERROR, "_intuition: ResetMenuStrip() called with NULL window\n");
        return TRUE;  /* Always returns TRUE per RKRM */
    }

    /* ResetMenuStrip is a "fast" SetMenuStrip - it just re-attaches the menu
     * without recalculating internal values. Use only when the menu was
     * previously attached via SetMenuStrip and only CHECKED/ITEMENABLED
     * flags have changed.
     */
    window->MenuStrip = menu;

    return TRUE;
}

VOID _intuition_RemoveClass ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct IClass * classPtr __asm("a0"))
{
    struct LXAIntuitionBase *base = (struct LXAIntuitionBase *)IntuitionBase;
    struct Node *node;

    if (!classPtr)
        return;

    node = base->ClassList.lh_Head;
    while (node && node->ln_Succ) {
        struct LXAClassNode *entry = (struct LXAClassNode *)node;
        if (entry->class_ptr == classPtr) {
            Remove(node);
            FreeMem(entry, sizeof(struct LXAClassNode));
            classPtr->cl_Flags &= ~CLF_INLIST;
            return;
        }
        node = node->ln_Succ;
    }
}

BOOL _intuition_FreeClass ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct IClass * classPtr __asm("a0"))
{
    if (!classPtr)
        return FALSE;

    if (classPtr->cl_Flags & CLF_INLIST)
        _intuition_RemoveClass(IntuitionBase, classPtr);

    if (classPtr->cl_SubclassCount != 0)
        return FALSE;

    if (classPtr->cl_ObjectCount != 0)
        return FALSE;

    if (classPtr->cl_Super && classPtr->cl_Super->cl_SubclassCount > 0)
        classPtr->cl_Super->cl_SubclassCount--;

    FreeMem(classPtr, sizeof(struct IClass) + (classPtr->cl_ID ? strlen((char *)classPtr->cl_ID) + 1 : 0));
    return TRUE;
}

VOID _intuition_private3 ( register struct IntuitionBase * IntuitionBase __asm("a6"))
{
    PRIVATE_FUNCTION_ERROR("_intuition", "private3");
}

VOID _intuition_private4 ( register struct IntuitionBase * IntuitionBase __asm("a6"))
{
    PRIVATE_FUNCTION_ERROR("_intuition", "private4");
}

VOID _intuition_private5 ( register struct IntuitionBase * IntuitionBase __asm("a6"))
{
    PRIVATE_FUNCTION_ERROR("_intuition", "private5");
}

VOID _intuition_private6 ( register struct IntuitionBase * IntuitionBase __asm("a6"))
{
    PRIVATE_FUNCTION_ERROR("_intuition", "private6");
}

VOID _intuition_private7 ( register struct IntuitionBase * IntuitionBase __asm("a6"))
{
    PRIVATE_FUNCTION_ERROR("_intuition", "private7");
}

VOID _intuition_private8 ( register struct IntuitionBase * IntuitionBase __asm("a6"))
{
    PRIVATE_FUNCTION_ERROR("_intuition", "private8");
}

VOID _intuition_private9 ( register struct IntuitionBase * IntuitionBase __asm("a6"))
{
    PRIVATE_FUNCTION_ERROR("_intuition", "private9");
}

VOID _intuition_private10 ( register struct IntuitionBase * IntuitionBase __asm("a6"))
{
    PRIVATE_FUNCTION_ERROR("_intuition", "private10");
}

struct ScreenBuffer * _intuition_AllocScreenBuffer ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Screen * sc __asm("a0"),
                                                        register struct BitMap * bm __asm("a1"),
                                                        register ULONG flags __asm("d0"))
{
    LXA_UNIMPLEMENTED("intuition", "AllocScreenBuffer", "partial: sb_DBufInfo is NULL, no double-buffer sync messages (Phase 256)");

    struct ScreenBuffer *sb;

    DPRINTF (LOG_DEBUG, "_intuition: AllocScreenBuffer() sc=0x%08lx bm=0x%08lx flags=0x%08lx\n", 
             (ULONG)sc, (ULONG)bm, flags);
    
    if (!sc) return NULL;
    
    sb = AllocMem(sizeof(struct ScreenBuffer), MEMF_PUBLIC | MEMF_CLEAR);
    if (!sb) return NULL;
    
    if (flags & SB_SCREEN_BITMAP) {
        /* Use the screen's embedded bitmap (address of it) */
        sb->sb_BitMap = &sc->BitMap;
    } else {
        sb->sb_BitMap = bm;
    }
    
    /* Initialize DBufInfo to NULL for now */
    sb->sb_DBufInfo = NULL;
    
    return sb;
}

VOID _intuition_FreeScreenBuffer ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Screen * sc __asm("a0"),
                                                        register struct ScreenBuffer * sb __asm("a1"))
{
    DPRINTF (LOG_DEBUG, "_intuition: FreeScreenBuffer() sc=0x%08lx sb=0x%08lx\n", 
             (ULONG)sc, (ULONG)sb);
             
    if (sb) {
        /* We don't free the bitmap, as we didn't allocate it (unless we add logic for that later) */
        /* Also Free DBufInfo if present? */
        FreeMem(sb, sizeof(struct ScreenBuffer));
    }
}

ULONG _intuition_ChangeScreenBuffer ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Screen * sc __asm("a0"),
                                                        register struct ScreenBuffer * sb __asm("a1"))
{
    DPRINTF (LOG_DEBUG, "_intuition: ChangeScreenBuffer() sc=0x%08lx sb=0x%08lx\n", 
             (ULONG)sc, (ULONG)sb);
             
    if (!sc || !sb || !sb->sb_BitMap) return 0;
    
    /* Update the Screen's embedded BitMap planes to point to the new buffer.
     * AmigaOS does this to switch the display.
     */
    struct BitMap *newBm = sb->sb_BitMap;
    int i;
    
    /* Copy planes */
    for (i = 0; i < 8; i++) {
        if (i < newBm->Depth)
            sc->BitMap.Planes[i] = newBm->Planes[i];
        else
            sc->BitMap.Planes[i] = NULL;
    }
    
    /* Copy depth (should be same, but just in case) */
    sc->BitMap.Depth = newBm->Depth;
    
    /* Update Host Display? 
     * The host display loop reads sc->BitMap.Planes directly.
     * So this change should be visible on next frame.
     * We might need to ensure memory visibility/barriers if MP, but for now this is fine.
     */
     
    return 1; /* Success */
}

VOID _intuition_ScreenDepth ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Screen * screen __asm("a0"),
                                                        register ULONG flags __asm("d0"),
                                                        register APTR reserved __asm("a1"))
{
    DPRINTF (LOG_DEBUG, "_intuition: ScreenDepth() screen=0x%08lx flags=0x%08lx\n", (ULONG)screen, flags);

    if (!screen)
        return;

    if ((UWORD)flags == SDEPTH_TOBACK)
    {
        _intuition_ScreenToBack(IntuitionBase, screen);
    }
    else
    {
        _intuition_ScreenToFront(IntuitionBase, screen);
    }
}

VOID _intuition_ScreenPosition ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Screen * screen __asm("a0"),
                                                        register ULONG flags __asm("d0"),
                                                        register LONG x1 __asm("d1"),
                                                        register LONG y1 __asm("d2"),
                                                        register LONG x2 __asm("d3"),
                                                        register LONG y2 __asm("d4"))
{
    LXA_UNIMPLEMENTED("intuition", "ScreenPosition", "partial: SPOS_MAKEVISIBLE ignored, display not updated (Phase 256)");

    /* GCC m68k move.w fix: sign-extend d-register LONG coordinate params */
    x1 = (LONG)(WORD)x1;
    y1 = (LONG)(WORD)y1;
    x2 = (LONG)(WORD)x2;
    y2 = (LONG)(WORD)y2;

    DPRINTF (LOG_DEBUG, "_intuition: ScreenPosition() screen=0x%08lx flags=0x%08lx pos=(%ld,%ld)\n", 
             (ULONG)screen, flags, x1, y1);
             
    if (!screen) return;
    
    /* AmigaOS 3.1 reference: only the vertical position changes (see
     * MoveScreen()). */
    if (flags & SPOS_ABSOLUTE)
    {
        /* x1, y1 are new coordinates */
        screen->TopEdge = y1;
    }
    else /* SPOS_RELATIVE (default) */
    {
        /* x1, y1 are deltas */
        screen->TopEdge += y1;
    }

    
    /* TODO: SPOS_MAKEVISIBLE logic */
    /* TODO: Update display */
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
    DPRINTF (LOG_DEBUG, "_intuition: ScrollWindowRaster() win=0x%08lx dx=%d dy=%d rect=(%d,%d)-(%d,%d)\n",
             (ULONG)win, dx, dy, xMin, yMin, xMax, yMax);
    
    if (!win || !win->RPort)
    {
        DPRINTF(LOG_ERROR, "_intuition: ScrollWindowRaster() NULL window or RPort\n");
        return;
    }
    
    /* Call graphics.library ScrollRaster to perform the actual scrolling */
    if (GfxBase)
    {
        ScrollRaster(win->RPort, dx, dy, xMin, yMin, xMax, yMax);
    }
}

VOID _intuition_LendMenus ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Window * fromwindow __asm("a0"),
                                                        register struct Window * towindow __asm("a1"))
{
    DPRINTF (LOG_DEBUG, "_intuition: LendMenus() from=0x%08lx to=0x%08lx\n",
             (ULONG)fromwindow, (ULONG)towindow);

    struct LXAWindowState *state;

    if (!fromwindow)
        return;

    /* The lending is private window state: fromwindow->MenuStrip is not
     * touched (AmigaOS 3.1 reference). */
    state = _intuition_ensure_window_state((struct LXAIntuitionBase *)IntuitionBase, fromwindow);
    if (state)
        state->lend_menus_to = (towindow == fromwindow) ? NULL : towindow;
}

ULONG _intuition_DoGadgetMethodA ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Gadget * gad __asm("a0"),
                                                        register struct Window * win __asm("a1"),
                                                        register struct Requester * req __asm("a2"),
                                                        register Msg message __asm("a3"))
{
    /*
     * DoGadgetMethodA() dispatches an arbitrary method to a BOOPSI gadget,
     * providing it with a GadgetInfo from the window context.
     */
    DPRINTF (LOG_DEBUG, "_intuition: DoGadgetMethodA() gad=0x%08lx win=0x%08lx method=0x%08lx\n",
             (ULONG)gad, (ULONG)win, message ? message->MethodID : 0);
    
    if (!gad || !message)
        return 0;
    
    struct IClass *cl = OCLASS((Object *)gad);
    if (!cl)
        return 0;

    {
        struct GadgetInfo gi;
        ULONG result;

        memset(&gi, 0, sizeof(gi));
        if (win)
        {
            gi.gi_Screen = win->WScreen;
            gi.gi_Window = win;
            gi.gi_Requester = req;
            gi.gi_RastPort = win->RPort;
            gi.gi_Layer = win->WLayer;
            _intuition_gadget_domain(win, &gi.gi_Domain);
            gi.gi_DrInfo = _intuition_GetScreenDrawInfo(IntuitionBase, win->WScreen);
        }

        /* The GadgetInfo goes into the message: ops_GInfo for OM_NEW/OM_SET/
         * OM_NOTIFY/OM_UPDATE, the first parameter (gpX_GInfo) for every
         * other method.  (So an OM_GET message loses its opg_AttrID, as on
         * AmigaOS 3.1, where DoGadgetMethodA(OM_GET) fails.) */
        switch (message->MethodID)
        {
            case OM_NEW:
            case OM_SET:
            case OM_NOTIFY:
            case OM_UPDATE:
                ((struct opSet *)message)->ops_GInfo = &gi;
                break;
            default:
                ((struct gpRender *)message)->gpr_GInfo = &gi;
                break;
        }

        result = _intuition_dispatch_method(cl, (Object *)gad, message);

        if (win && gi.gi_DrInfo)
            _intuition_FreeScreenDrawInfo(IntuitionBase, win->WScreen, gi.gi_DrInfo);
        return result;
    }
}

VOID _intuition_SetWindowPointerA ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Window * win __asm("a0"),
                                                        register const struct TagItem * taglist __asm("a1"))
{
    struct TagItem *state;
    struct TagItem *tag;
    UWORD *pointer = NULL;
    BOOL busy = FALSE;

    DPRINTF (LOG_DEBUG, "_intuition: SetWindowPointerA() win=0x%08lx taglist=0x%08lx\n",
             (ULONG)win, (ULONG)taglist);

    if (!win)
        return;

    if (!taglist)
    {
        _intuition_ClearPointer(IntuitionBase, win);
        return;
    }

    state = (struct TagItem *)taglist;
    while ((tag = NextTagItem(&state)))
    {
        switch (tag->ti_Tag)
        {
            case WA_Pointer:
                pointer = (UWORD *)tag->ti_Data;
                break;
            case WA_BusyPointer:
                busy = tag->ti_Data ? TRUE : FALSE;
                break;
            case WA_PointerDelay:
                break;
        }
    }

    if (busy)
        _intuition_SetPointer(IntuitionBase, win, (UWORD *)_intuition_busy_pointer, 16, 16, -6, 0);
    else if (pointer)
        _intuition_SetPointer(IntuitionBase, win, pointer, win->PtrHeight, win->PtrWidth, win->XOffset, win->YOffset);
    else
        _intuition_ClearPointer(IntuitionBase, win);
}

BOOL _intuition_TimedDisplayAlert ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register ULONG alertNumber __asm("d0"),
                                                        register CONST_STRPTR string __asm("a0"),
                                                        register UWORD height __asm("d1"),
                                                        register ULONG time __asm("a1"))
{
    DPRINTF (LOG_DEBUG, "_intuition: TimedDisplayAlert() alert=0x%08lx height=%u time=%lu\n",
             alertNumber, (unsigned)height, time);
    (void)string;
    /* Headless lxa has no user who could click before the timeout expires:
     * like AmigaOS 3.1 without input, the alert times out and returns FALSE. */
    return FALSE;
}

VOID _intuition_HelpControl ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Window * win __asm("a0"),
                                                        register ULONG flags __asm("d0"))
{
    DPRINTF (LOG_DEBUG, "_intuition: HelpControl() win=0x%08lx flags=0x%08lx\n",
             (ULONG)win, flags);

    if (!win)
        return;

    if (flags & HC_GADGETHELP)
        win->MoreFlags |= LXA_WMF_GADGETHELP;
    else
        win->MoreFlags &= ~LXA_WMF_GADGETHELP;
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
    (ULONG)               sizeof(struct LXAIntuitionBase),        // LibBaseSize
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
    _intuition_SetIPrefs, // offset = -576
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
