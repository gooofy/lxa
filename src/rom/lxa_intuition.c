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
#include <intuition/preferences.h>
#include <intuition/classes.h>
#include <intuition/classusr.h>
#include <intuition/imageclass.h>
#include <intuition/gadgetclass.h>
#include <intuition/icclass.h>
#include <intuition/cghooks.h>

#include <graphics/gfx.h>
#include <graphics/rastport.h>
#include <graphics/view.h>
#include <graphics/clip.h>
#include <graphics/layers.h>
#include <clib/graphics_protos.h>
#include <inline/graphics.h>
#include <clib/layers_protos.h>
#include <inline/layers.h>
#include <clib/utility_protos.h>
#include <inline/utility.h>

#include "util.h"

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

/*
 * Minimum usable screen/window height threshold.
 * When applications pass suspiciously small height values (< 50 pixels),
 * we expand them to the display mode's default height. This handles older
 * apps that relied on ViewModes-based height expansion (e.g., GFA Basic).
 */
#define MIN_USABLE_HEIGHT 50

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
};

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
        DeleteMsgPort(reply_port);
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
        window->WindowPort = CreateMsgPort();
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

struct LXAPubScreenNode {
    struct PubScreenNode pub;
};

static struct IClass *_intuition_find_class(struct LXAIntuitionBase *base, CONST_STRPTR classID);
static ULONG _intuition_dispatch_method(struct IClass *cl, Object *obj, Msg msg);
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

static VOID _intuition_remove_window_state(struct LXAIntuitionBase *base, const struct Window *window)
{
    struct LXAWindowState *state;

    state = _intuition_find_window_state(base, window);
    if (!state)
        return;

    Remove(&state->node);
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

static VOID _intuition_init_preferences(struct Preferences *prefs)
{
    if (!prefs)
        return;

    memset(prefs, 0, sizeof(*prefs));

    prefs->FontHeight = 8;
    prefs->PrinterPort = 0;
    prefs->BaudRate = 5;

    prefs->KeyRptDelay.tv_secs = 0;
    prefs->KeyRptDelay.tv_micro = 500000;
    prefs->KeyRptSpeed.tv_secs = 0;
    prefs->KeyRptSpeed.tv_micro = 100000;

    prefs->DoubleClick.tv_secs = 0;
    prefs->DoubleClick.tv_micro = 500000;

    prefs->XOffset = -1;
    prefs->YOffset = -1;
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

static struct PubScreenNode *_intuition_find_pubscreen_by_screen(struct LXAIntuitionBase *base,
                                                                  const struct Screen *screen)
{
    struct Node *node;

    if (!base || !screen)
        return NULL;

    for (node = base->PubScreenList.lh_Head; node && node->ln_Succ; node = node->ln_Succ)
    {
        struct PubScreenNode *pub = (struct PubScreenNode *)node;
        if (pub->psn_Screen == screen)
            return pub;
    }

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
        pub = _intuition_find_pubscreen_by_screen(base, base->DefaultPubScreen);
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

    AddTail(&base->PubScreenList, &entry->pub.psn_Node);

    if (!base->DefaultPubScreen || ((base->DefaultPubScreen->Flags & SCREENTYPE) == WBENCHSCREEN))
        base->DefaultPubScreen = screen;
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
            return (ULONG)obj;

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
            return 0;
            
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
#define REVISION   1
#define EXLIBNAME  "intuition"
#define EXLIBVER   " 40.1 (2022/03/21)"

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

/* Forward declarations for gadget rendering helpers */
static void _complement_gadget_area(struct Window *window, struct Requester *req, struct Gadget *gad);
static void _render_gadget(struct Window *window, struct Requester *req, struct Gadget *gad);
static void _render_window_user_gadgets(struct Window *window);
static void _render_requester(struct Window *window, struct Requester *req);
static void _calculate_requester_box(struct Window *window, struct Requester *req,
                                     LONG *left, LONG *top,
                                     LONG *width, LONG *height);
static void _rerender_requester_stack(struct Window *window);
static struct Window * _find_window_at_pos(struct Screen *screen, WORD x, WORD y);
static struct Gadget * _find_gadget_at_pos(struct Window *window, WORD relX, WORD relY);
static BOOL _point_in_gadget(struct Window *window, struct Gadget *gad, WORD relX, WORD relY);
static struct Menu * _find_menu_at_x(struct Window *window, WORD screenX);
static struct MenuItem * _find_item_at_pos(struct Menu *menu, WORD x, WORD y);
static struct MenuItem * _find_item_in_chain_at_pos(struct MenuItem *firstItem, WORD x, WORD y);
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
static void _render_menu_items(struct Window *window);
static void _enter_menu_mode(struct Window *window, struct Screen *screen, WORD mouseX, WORD mouseY);
static void _exit_menu_mode(struct Window *window, WORD mouseX, WORD mouseY);
static void _handle_sys_gadget_verify(struct Window *window, struct Gadget *gadget);

VOID _intuition_SizeWindow(register struct IntuitionBase *IntuitionBase __asm("a6"),
                           register struct Window *window __asm("a0"),
                           register WORD dx __asm("d0"),
                           register WORD dy __asm("d1"));

static struct Gadget *g_active_gadget;
static struct Window *g_active_window;
static WORD g_prop_click_offset;
static BOOL g_menu_mode;
static struct Window *g_menu_window;
static struct Menu *g_active_menu;
static struct MenuItem *g_active_item;
static struct MenuItem *g_active_subitem;
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
                ULONG result = entry(super, obj, msg);
                if (!result)
                    return 0;
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
                ULONG result = entry(super, obj, msg);
                if (!result)
                    return 0;
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
                    _intuition_dispatch_method(mobj->o_Class, opm->opam_Object, (Msg)&at);
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
                    _intuition_dispatch_method(mobj->o_Class, opm->opam_Object, (Msg)&rm);
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
                    return entry(super, obj, msg);
                }
            }
            return 0;
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
                ULONG result = entry(super, obj, msg);
                if (!result)
                    return 0;
            }
            
            /* Initialize gadget structure */
            gadget->GadgetType = GTYP_CUSTOMGADGET;
            gadget->Flags = GFLG_GADGHNONE;
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
            struct opSet *ops = (struct opSet *)msg;
            struct TagItem *tags = ops->ops_AttrList;
            struct TagItem *tag;
            
            while ((tag = NextTagItem(&tags)))
            {
                switch (tag->ti_Tag)
                {
                    case GA_Left:
                        gadget->LeftEdge = (WORD)tag->ti_Data;
                        break;
                    case GA_Top:
                        gadget->TopEdge = (WORD)tag->ti_Data;
                        break;
                    case GA_Width:
                        gadget->Width = (WORD)tag->ti_Data;
                        break;
                    case GA_Height:
                        gadget->Height = (WORD)tag->ti_Data;
                        break;
                    case GA_ID:
                        gadget->GadgetID = (UWORD)tag->ti_Data;
                        break;
                    case GA_UserData:
                        gadget->UserData = (APTR)tag->ti_Data;
                        break;
                    case GA_Disabled:
                        if (tag->ti_Data)
                            gadget->Flags |= GFLG_DISABLED;
                        else
                            gadget->Flags &= ~GFLG_DISABLED;
                        break;
                    case GA_Immediate:
                        if (tag->ti_Data)
                            gadget->Activation |= GACT_IMMEDIATE;
                        break;
                    case GA_RelVerify:
                        if (tag->ti_Data)
                            gadget->Activation |= GACT_RELVERIFY;
                        break;
                    case GA_Selected:
                        if (tag->ti_Data)
                            gadget->Flags |= GFLG_SELECTED;
                        else
                            gadget->Flags &= ~GFLG_SELECTED;
                        break;
                    case ICA_TARGET:
                        ic->ic_Target = (Object *)tag->ti_Data;
                        break;
                    case ICA_MAP:
                        ic->ic_Mapping = (struct TagItem *)tag->ti_Data;
                        break;
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
            
            while ((tag = NextTagItem(&tags)))
            {
                switch (tag->ti_Tag)
                {
                    case GA_Disabled:
                        if (tag->ti_Data)
                            gadget->Flags |= GFLG_DISABLED;
                        else
                            gadget->Flags &= ~GFLG_DISABLED;
                        changed = 1;
                        break;
                    case GA_Selected:
                        if (tag->ti_Data)
                            gadget->Flags |= GFLG_SELECTED;
                        else
                            gadget->Flags &= ~GFLG_SELECTED;
                        changed = 1;
                        break;
                }
            }
            return changed;
        }
            
        case OM_GET:
        {
            struct opGet *opg = (struct opGet *)msg;
            switch (opg->opg_AttrID)
            {
                case GA_ID:
                    *(opg->opg_Storage) = gadget->GadgetID;
                    return 1;
                case GA_UserData:
                    *(opg->opg_Storage) = (ULONG)gadget->UserData;
                    return 1;
                case GA_Disabled:
                    *(opg->opg_Storage) = (gadget->Flags & GFLG_DISABLED) ? TRUE : FALSE;
                    return 1;
                case GA_Selected:
                    *(opg->opg_Storage) = (gadget->Flags & GFLG_SELECTED) ? TRUE : FALSE;
                    return 1;
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
                ULONG result = entry(super, obj, msg);
                if (!result)
                    return 0;
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
            
            /* Determine state for visual feedback */
            if (gadget->Flags & GFLG_SELECTED)
                state |= IDS_SELECTED;
                
            /* Draw the button frame */
            _draw_bevel_box(rp, gadget->LeftEdge, gadget->TopEdge, gadget->Width, gadget->Height, state, dri);
            
            /* Draw Label (GadgetText) */
            if (gadget->GadgetText) {
                struct IntuiText *it = gadget->GadgetText;
                
                SetAPen(rp, dri ? dri->dri_Pens[TEXTPEN] : 1);
                SetBPen(rp, dri ? dri->dri_Pens[BACKGROUNDPEN] : 0);
                
                /* Simple centering calculation */
                /* For now, just draw at offsets */
                Move(rp, gadget->LeftEdge + it->LeftEdge + 4, gadget->TopEdge + it->TopEdge + gadget->Height/2 + 2); // Approximate centering offset
                
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
                ULONG result = entry(super, obj, msg);
                if (!result)
                    return 0;
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
                case PGA_Top:
                    *(opg->opg_Storage) = data->top;
                    return 1;
                case PGA_Visible:
                    *(opg->opg_Storage) = data->visible;
                    return 1;
                case PGA_Total:
                    *(opg->opg_Storage) = data->total;
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
            
            /* Draw container (Recessed) */
            _draw_bevel_box(rp, gadget->LeftEdge, gadget->TopEdge, gadget->Width, gadget->Height, IDS_SELECTED, dri);
            
            /* Draw Knob if we have PropInfo */
            if (pi)
            {
                WORD knobW, knobH, knobX, knobY;
                
                /* Autoknob */
                WORD containerW = gadget->Width - 4;
                WORD containerH = gadget->Height - 4;
                
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
                     
                    knobX = gadget->LeftEdge + 2 + ((maxMoveX * (ULONG)pi->HorizPot) / 0xFFFF);
                    knobY = gadget->TopEdge + 2 + ((maxMoveY * (ULONG)pi->VertPot) / 0xFFFF);
                     
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
                ULONG result = entry(super, obj, msg);
                if (!result)
                    return 0;
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
            
            /* Draw container (Recessed) */
            _draw_bevel_box(rp, gadget->LeftEdge, gadget->TopEdge, gadget->Width, gadget->Height, IDS_SELECTED, dri);
            
            /* Clear background inside */
            SetAPen(rp, bgColor);
            RectFill(rp, gadget->LeftEdge + 2, gadget->TopEdge + 2, 
                         gadget->LeftEdge + gadget->Width - 3, gadget->TopEdge + gadget->Height - 3);
            
            /* Calculate text Y position (vertically centered) */
            textY = gadget->TopEdge + (gadget->Height / 2) + 3;
            
            /* Draw Text */
            if (si && si->Buffer)
            {
                LONG len = strlen((char *)si->Buffer);
                
                SetAPen(rp, txtColor);
                SetBPen(rp, bgColor);
                SetDrMd(rp, JAM2);
                
                Move(rp, gadget->LeftEdge + 4, textY);
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
                        cursorX = gadget->LeftEdge + 4 + TextLength(rp, si->Buffer, cursorPos);
                    }
                    else
                    {
                        cursorX = gadget->LeftEdge + 4;
                    }
                    
                    /* Draw cursor as a vertical bar (XOR mode) */
                    SetAPen(rp, txtColor);
                    SetDrMd(rp, COMPLEMENT);
                    RectFill(rp, cursorX, gadget->TopEdge + 3, 
                                 cursorX + 1, gadget->TopEdge + gadget->Height - 4);
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

    /* Initialize ClassList (NewList inline) */
    NewList(&base->ClassList);
    NewList(&base->PubScreenList);
    NewList(&base->WindowStateList);
    base->DefaultPubScreen = NULL;
    _intuition_init_preferences(&base->DefaultPrefs);
    base->ActivePrefs = base->DefaultPrefs;
    base->EditHook = NULL;

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
    IntuitionBase->LibNode.lib_OpenCnt++;
    IntuitionBase->LibNode.lib_Flags &= ~LIBF_DELEXP;
    return IntuitionBase;
}

BPTR __g_lxa_intuition_CloseLib ( register struct IntuitionBase  *intuitionb __asm("a6"))
{
    intuitionb->LibNode.lib_OpenCnt--;
    return (BPTR)0;
}

BPTR __g_lxa_intuition_ExpungeLib ( register struct IntuitionBase  *intuitionb      __asm("a6"))
{
    return (BPTR)0;
}

ULONG __g_lxa_intuition_ExtFuncLib(void)
{
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
    IntuitionBase->Seconds = tv.tv_secs;
    IntuitionBase->Micros = tv.tv_micro;
}

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

                if (_gadtools_IsCheckbox(gad) || _gadtools_IsCycle(gad))
                    _render_gadget(window, NULL, gad);

                if ((gad->GadgetType & GTYP_SYSGADGET) &&
                    (gad->GadgetType & GTYP_SYSTYPEMASK) == GTYP_SIZING)
                {
                    DPRINTF(LOG_DEBUG, "_intuition: Starting window resize: window=0x%08lx size=(%d,%d)\n",
                            (ULONG)window, window->Width, window->Height);
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
                {
                    struct PropInfo *pi = (struct PropInfo *)gad->SpecialInfo;
                    if (pi && (pi->Flags & AUTOKNOB))
                    {
                        WORD gadAbsL = gad->LeftEdge + window->LeftEdge;
                        WORD containerL = gadAbsL + 1;
                        WORD containerW = gad->Width - 2;
                        WORD knobW;
                        WORD maxMoveX;
                        WORD knobX;

                        if (pi->Flags & FREEHORIZ)
                            knobW = (WORD)(((ULONG)containerW * (ULONG)pi->HorizBody) / 0xFFFF);
                        else
                            knobW = containerW;
                        if (knobW < 6) knobW = 6;
                        if (knobW > containerW) knobW = containerW;

                        maxMoveX = containerW - knobW;
                        if (maxMoveX < 0) maxMoveX = 0;
                        knobX = containerL + (WORD)(((ULONG)maxMoveX * (ULONG)pi->HorizPot) / 0xFFFF);

                        if (mouseX >= knobX && mouseX < knobX + knobW)
                        {
                            g_prop_click_offset = mouseX - knobX;
                        }
                        else
                        {
                            g_prop_click_offset = knobW / 2;
                            {
                                WORD newKnobL = mouseX - g_prop_click_offset;
                                if (newKnobL < containerL) newKnobL = containerL;
                                if (newKnobL > containerL + maxMoveX)
                                    newKnobL = containerL + maxMoveX;
                                if (maxMoveX > 0)
                                    pi->HorizPot = (UWORD)(((ULONG)(newKnobL - containerL) * 0xFFFF) / (ULONG)maxMoveX);
                                else
                                    pi->HorizPot = 0;

                                {
                                    WORD sl_min = (WORD)pi->CWidth;
                                    WORD sl_max = (WORD)pi->CHeight;
                                    LONG level;

                                    if (sl_max > sl_min)
                                        level = sl_min + ((LONG)pi->HorizPot * (sl_max - sl_min)) / 0xFFFF;
                                    else
                                        level = sl_min;

                                    _gadtools_UpdateSliderLevelDisplay(gad, level);
                                }

                                _render_gadget(window, NULL, gad);
                            }
                        }

                        DPRINTF(LOG_DEBUG, "_intuition: Prop SELECTDOWN: knobX=%d knobW=%d maxMove=%d offset=%d pot=%u\n",
                                knobX, knobW, maxMoveX, g_prop_click_offset, pi->HorizPot);
                    }
                }

                if (gad->Activation & GACT_IMMEDIATE)
                {
                    _post_idcmp_message(window, IDCMP_GADGETDOWN, 0,
                                       qualifier, gad, relX, relY);
                }

                if ((gad->Flags & GFLG_GADGHIGHBITS) == GFLG_GADGHCOMP &&
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
                        WORD sl_min = (WORD)pi->CWidth;
                        WORD sl_max = (WORD)pi->CHeight;
                        LONG range = sl_max - sl_min;
                        LONG level;

                        if (range > 0)
                            level = sl_min + ((LONG)pi->HorizPot * range) / 0xFFFF;
                        else
                            level = sl_min;
                        _gadtools_UpdateSliderLevelDisplay(gad, level);
                        level_code = (UWORD)level;

                        DPRINTF(LOG_DEBUG, "_intuition: Prop SELECTUP: pot=%u min=%d max=%d level=%ld\n",
                                pi->HorizPot, sl_min, sl_max, level);
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
                        if (gad->GadgetType & GTYP_SYSGADGET)
                        {
                            _handle_sys_gadget_verify(activeWin, gad);
                        }
                        else if (gad->Activation & GACT_RELVERIFY)
                        {
                            _post_idcmp_message(activeWin, IDCMP_GADGETUP, 0,
                                               qualifier, gad, activeRelX, activeRelY);
                        }
                    }

                    if (_gadtools_IsCheckbox(gad) || _gadtools_IsCycle(gad))
                    {
                        _render_gadget(activeWin, NULL, gad);
                    }
                    else if ((gad->Flags & GFLG_GADGHIGHBITS) == GFLG_GADGHCOMP)
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

            DPRINTF(LOG_DEBUG, "_intuition: MENUDOWN at (%d,%d), window=0x%08lx MenuStrip=0x%08lx\n",
                    mouseX, mouseY, (ULONG)window, window ? (ULONG)window->MenuStrip : 0);

            if (window && window->MenuStrip && !(window->Flags & WFLG_RMBTRAP))
            {
                menuWin = window;
            }
            else if (screen)
            {
                menuWin = screen->FirstWindow;
                while (menuWin)
                {
                    if (menuWin->MenuStrip && !(menuWin->Flags & WFLG_RMBTRAP))
                        break;
                    menuWin = menuWin->NextWindow;
                }
            }

            if (menuWin && menuWin->MenuStrip && screen)
            {
                DPRINTF(LOG_DEBUG, "_intuition: Entering menu mode for menuWin=0x%08lx\n", (ULONG)menuWin);
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

        _post_idcmp_message(window, IDCMP_MOUSEBUTTONS, code,
                           qualifier, NULL, relX, relY);
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
    {
        BOOL needRedraw = FALSE;
        struct Menu *oldMenu = g_active_menu;
        struct MenuItem *oldItem = g_active_item;

        if (mouseY < screen->BarHeight + 1)
        {
            struct Menu *newMenu = _find_menu_at_x(g_menu_window, mouseX);
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
                    newSubItem = _find_item_in_chain_at_pos(g_active_item->SubItem,
                                                            mouseX - submenuX,
                                                            mouseY - submenuY);
                    if (newSubItem != g_active_subitem)
                    {
                        g_active_subitem = newSubItem;
                        needRedraw = TRUE;
                    }
                }
            }

            if (!handledSubmenu)
            {
                WORD menuTop = screen->BarHeight + 1;
                WORD menuLeft = screen->BarHBorder + g_active_menu->LeftEdge;
                WORD itemX = mouseX - menuLeft;
                WORD itemY = mouseY - menuTop;
                struct MenuItem *newItem = _find_item_at_pos(g_active_menu, itemX, itemY);

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
            if (_menu_hover_redraw_can_repaint_in_place(g_menu_window,
                                                        oldMenu,
                                                        oldItem))
            {
                _render_menu_items(g_menu_window);
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
        struct PropInfo *pi = (struct PropInfo *)gad->SpecialInfo;

        if (pi && (pi->Flags & AUTOKNOB) && (pi->Flags & FREEHORIZ))
        {
            WORD gadAbsL = gad->LeftEdge + activeWin->LeftEdge;
            WORD containerL = gadAbsL + 1;
            WORD containerW = gad->Width - 2;
            WORD knobW;
            WORD maxMoveX;
            WORD newKnobL;
            UWORD old_pot = pi->HorizPot;

            knobW = (WORD)(((ULONG)containerW * (ULONG)pi->HorizBody) / 0xFFFF);
            if (knobW < 6) knobW = 6;
            if (knobW > containerW) knobW = containerW;

            maxMoveX = containerW - knobW;
            if (maxMoveX < 0) maxMoveX = 0;

            newKnobL = mouseX - g_prop_click_offset;
            if (newKnobL < containerL) newKnobL = containerL;
            if (newKnobL > containerL + maxMoveX)
                newKnobL = containerL + maxMoveX;

            if (maxMoveX > 0)
                pi->HorizPot = (UWORD)(((ULONG)(newKnobL - containerL) * 0xFFFF) / (ULONG)maxMoveX);
            else
                pi->HorizPot = 0;

            if (pi->HorizPot != old_pot)
            {
                WORD sl_min;
                WORD sl_max;
                LONG range;
                LONG level;
                WORD relX;
                WORD relY;

                _render_gadget(activeWin, NULL, gad);

                sl_min = (WORD)pi->CWidth;
                sl_max = (WORD)pi->CHeight;
                range = sl_max - sl_min;
                if (range > 0)
                    level = sl_min + ((LONG)pi->HorizPot * range) / 0xFFFF;
                else
                    level = sl_min;

                _gadtools_UpdateSliderLevelDisplay(gad, level);

                relX = mouseX - activeWin->LeftEdge;
                relY = mouseY - activeWin->TopEdge;
                _post_idcmp_message(activeWin, IDCMP_MOUSEMOVE, (UWORD)level,
                                   qualifier, gad, relX, relY);
            }
        }
    }

    if (window)
    {
        WORD relX = mouseX - window->LeftEdge;
        WORD relY = mouseY - window->TopEdge;

        window->MouseX = relX;
        window->MouseY = relY;

        _post_idcmp_message(window, IDCMP_MOUSEMOVE, 0,
                           qualifier, NULL, relX, relY);
    }
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

        if (window->IDCMPFlags & IDCMP_VANILLAKEY)
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

VOID _intuition_Intuition ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct InputEvent * iEvent __asm("a0"))
{
    DPRINTF (LOG_DEBUG, "_intuition: Intuition() called iEvent=0x%08lx\n", (ULONG)iEvent);

    while (IntuitionBase && iEvent)
    {
        struct InputEvent current_event = *iEvent;
        struct Screen *screen;
        struct Window *window;
        struct InputEvent *next_event = iEvent->ie_NextEvent;
        WORD mouseX = IntuitionBase->MouseX;
        WORD mouseY = IntuitionBase->MouseY;

        current_event.ie_NextEvent = NULL;

        if (current_event.ie_Class == IECLASS_RAWMOUSE ||
            current_event.ie_Class == IECLASS_POINTERPOS)
        {
            mouseX = current_event.ie_X;
            mouseY = current_event.ie_Y;
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
                _intuition_handle_mouse_button_event(IntuitionBase, screen, window,
                                                     mouseX, mouseY,
                                                     current_event.ie_Code,
                                                     current_event.ie_Qualifier,
                                                     TRUE);
                break;

            case IECLASS_POINTERPOS:
                _intuition_handle_pointerpos_event(IntuitionBase, screen, window,
                                                   mouseX, mouseY,
                                                   current_event.ie_Qualifier,
                                                   TRUE);
                break;

            case IECLASS_RAWKEY:
                _intuition_handle_rawkey_event(IntuitionBase, window,
                                               mouseX, mouseY,
                                               current_event.ie_Code,
                                               current_event.ie_Qualifier,
                                               TRUE);
                break;

            case IECLASS_EVENT:
                if (current_event.ie_Code == IECODE_NEWACTIVE)
                {
                    _intuition_handle_event_class_event(IntuitionBase, window,
                                                        mouseX, mouseY,
                                                        current_event.ie_Code,
                                                        TRUE);
                }
                break;

            default:
                break;
        }

        iEvent = next_event;
    }
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
    
    /* Free the RasInfo if allocated */
    if (screen->ViewPort.RasInfo)
    {
        FreeMem(screen->ViewPort.RasInfo, sizeof(struct RasInfo));
    }

    if (screen->ViewPort.ColorMap)
    {
        FreeColorMap(screen->ViewPort.ColorMap);
    }

    _intuition_unregister_pubscreen(IntuitionBase, screen);

    /* Free the Screen structure */
    FreeMem(screen, sizeof(struct Screen));

    DPRINTF (LOG_DEBUG, "_intuition: CloseScreen() done\n");

    return TRUE;
}

VOID _intuition_CloseWindow ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Window * window __asm("a0"))
{
    struct LXAWindowState *state;

    DPRINTF (LOG_DEBUG, "_intuition: CloseWindow() window=0x%08lx\n", (ULONG)window);

    if (!window)
    {
        LPRINTF (LOG_ERROR, "_intuition: CloseWindow() called with NULL window\n");
        return;
    }

    state = _intuition_find_window_state((struct LXAIntuitionBase *)IntuitionBase, window);

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

    /* Free system gadgets we created */
    {
        struct Gadget *gad = window->FirstGadget;
        struct Gadget *next;
        while (gad)
        {
            next = gad->NextGadget;
            /* Only free gadgets we allocated (system gadgets) */
            if (gad->GadgetType & GTYP_SYSGADGET)
            {
                FreeMem(gad, sizeof(struct Gadget));
            }
            gad = next;
        }
        window->FirstGadget = NULL;
    }

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
     * DisplayAlert() shows a hardware/software alert (Guru Meditation).
     * We log the alert and return TRUE (user pressed left mouse = continue).
     * The string format is: y_position, string_chars, continuation_byte...
     */
    DPRINTF (LOG_ERROR, "_intuition: DisplayAlert() alertNumber=0x%08lx string=0x%08lx height=%u\n",
             alertNumber, (ULONG)string, (unsigned)height);
    
    /* Parse the alert string if possible */
    if (string) {
        /* Skip y position byte, print the text */
        const char *p = (const char *)string;
        if (*p) {
            p++;  /* Skip y position */
            DPRINTF (LOG_ERROR, "_intuition: DisplayAlert() message: %s\n", p);
        }
    }
    
    if (alertNumber & DEADEND_ALERT)
        return FALSE;

    /* Return TRUE = left mouse button (continue), FALSE = right mouse (reboot) */
    return TRUE;
}

VOID _intuition_DisplayBeep ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Screen * screen __asm("a0"))
{
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
    requester->ReqLayer = NULL;
    requester->RWindow = NULL;
    
    if (window->RPort)
    {
        _calculate_requester_box(window, requester, &left, &top, &width, &height);
        SetAPen(window->RPort, 0);
        RectFill(window->RPort, left, top, left + width - 1, top + height - 1);
        _rerender_requester_stack(window);
    }
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
        LONG container_width = req ? req->Width : window->Width;
        calc_left += container_width;
    }

    if (gad->Flags & GFLG_RELBOTTOM)
    {
        LONG container_height = req ? req->Height : window->Height;
        calc_top += container_height;
    }

    if (gad->Flags & GFLG_RELWIDTH)
    {
        LONG container_width = req ? req->Width : window->Width;
        calc_width += container_width;
    }

    if (gad->Flags & GFLG_RELHEIGHT)
    {
        LONG container_height = req ? req->Height : window->Height;
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

static struct Gadget * _find_gadget_at_pos(struct Window *window, WORD relX, WORD relY)
{
    struct Gadget *gad;
    LONG gx0, gy0, width, height;
    
    if (!window)
        return NULL;
    
    /* Walk through gadget list */
    for (gad = window->FirstGadget; gad; gad = gad->NextGadget)
    {
        /* Skip disabled gadgets */
        if (gad->Flags & GFLG_DISABLED)
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
            /* Zoom gadget - TODO: implement ZipWindow behavior */
            DPRINTF(LOG_DEBUG, "_intuition: Zoom gadget clicked - TODO: ZipWindow\n");
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

/* Menu drop-down save-behind buffer.
 * When a menu drop-down is rendered, the screen area beneath it is saved here
 * so it can be restored when the menu closes or switches to a different menu.
 * NOTE: No initializers — .bss must be in RAM, not ROM .data section. */
static UBYTE *g_menu_save_buffer;           /* Saved planar data (all planes concatenated) */
static ULONG  g_menu_save_size;             /* Size of allocated buffer in bytes */
static WORD   g_menu_save_x;               /* Left edge of saved area */
static WORD   g_menu_save_y;               /* Top edge of saved area */
static WORD   g_menu_save_w;               /* Width of saved area in pixels */
static WORD   g_menu_save_h;               /* Height of saved area in pixels */

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
            /* Compute NumChars from buffer contents */
            WORD len = 0;
            while (si->Buffer[len] != '\0' && len < si->MaxChars)
                len++;
            si->NumChars = len;

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

    if (!item || !(item->Flags & ITEMTEXT) || !item->ItemFill)
        return FALSE;

    it = (struct IntuiText *)item->ItemFill;
    if (!it || !it->IText)
        return FALSE;

    return it->IText[0] == '-';
}

static void _draw_submenu_indicator(struct RastPort *rp, WORD right, WORD center_y)
{
    WORD x;
    WORD half_height;

    if (!rp)
        return;

    half_height = 3;

    for (x = 0; x <= 3; x++)
    {
        Move(rp, right - x, center_y - half_height + x);
        Draw(rp, right - x, center_y + half_height - x);
    }
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
    struct MenuItem *item;
    WORD chain_left;
    
    if (!menu || !menu->FirstItem)
        return NULL;

    chain_left = menu->FirstItem->LeftEdge;
    
    for (item = menu->FirstItem; item; item = item->NextItem)
    {
        /* Skip disabled items */
        if (!(item->Flags & ITEMENABLED))
            continue;
        
        if (x >= item->LeftEdge - chain_left &&
            x < item->LeftEdge - chain_left + item->Width &&
            y >= item->TopEdge && y < item->TopEdge + item->Height)
        {
            return item;
        }
    }
    
    return NULL;
}

static struct MenuItem * _find_item_in_chain_at_pos(struct MenuItem *firstItem, WORD x, WORD y)
{
    struct MenuItem *item;
    WORD chain_left;

    if (!firstItem)
        return NULL;

    chain_left = firstItem->LeftEdge;

    for (item = firstItem; item; item = item->NextItem)
    {
        if (!(item->Flags & ITEMENABLED))
            continue;

        if (x >= item->LeftEdge - chain_left &&
            x < item->LeftEdge - chain_left + item->Width &&
            y >= item->TopEdge && y < item->TopEdge + item->Height)
        {
            return item;
        }
    }

    return NULL;
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

static void _get_menu_item_chain_box(struct MenuItem *firstItem,
                                     WORD baseX, WORD baseY,
                                     WORD *boxX, WORD *boxY,
                                     WORD *boxWidth, WORD *boxHeight)
{
    struct MenuItem *item;
    WORD chain_left;
    WORD max_width;
    WORD max_height;

    if (boxX)
        *boxX = baseX;
    if (boxY)
        *boxY = baseY;
    if (boxWidth)
        *boxWidth = 0;
    if (boxHeight)
        *boxHeight = 0;

    if (!firstItem)
        return;

    chain_left = firstItem->LeftEdge;
    max_width = 0;
    max_height = 0;

    for (item = firstItem; item; item = item->NextItem)
    {
        WORD width = (item->LeftEdge - chain_left) + item->Width;
        WORD height = item->TopEdge + item->Height;

        if (width > max_width)
            max_width = width;
        if (height > max_height)
            max_height = height;
    }

    if (boxWidth)
        *boxWidth = max_width + 8;
    if (boxHeight)
        *boxHeight = max_height + 4;
}

static BOOL _get_menu_submenu_box(struct Window *window,
                                  struct Menu *menu,
                                  struct MenuItem *item,
                                  WORD *boxX, WORD *boxY,
                                  WORD *boxWidth, WORD *boxHeight)
{
    struct Screen *screen;
    WORD menuX;
    WORD menuY;

    if (boxX)
        *boxX = 0;
    if (boxY)
        *boxY = 0;
    if (boxWidth)
        *boxWidth = 0;
    if (boxHeight)
        *boxHeight = 0;

    if (!window || !window->WScreen || !menu || !item || !item->SubItem)
        return FALSE;

    screen = window->WScreen;
    menuX = screen->BarHBorder + menu->LeftEdge;
    menuY = screen->BarHeight + 1;

    _get_menu_item_chain_box(item->SubItem,
                             menuX + item->LeftEdge + item->Width,
                             menuY + item->TopEdge,
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
 * Save a rectangular area of the screen's BitMap into g_menu_save_buffer.
 * This is used to save the area under a menu drop-down before drawing it,
 * so it can be restored when the menu closes.
 * Saves raw planar data (all bitplanes concatenated).
 */
static void _save_menu_dropdown_area(struct Screen *screen, WORD x, WORD y, WORD w, WORD h)
{
    struct BitMap *bm;
    WORD bpr;           /* bytes per row in screen bitmap */
    WORD depth;
    WORD startByte, endByte, saveBytes;
    ULONG totalSize;
    WORD plane, row;
    
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
    
    /* Free any previous save buffer */
    if (g_menu_save_buffer && g_menu_save_size > 0)
    {
        FreeMem(g_menu_save_buffer, g_menu_save_size);
        g_menu_save_buffer = NULL;
        g_menu_save_size = 0;
    }
    
    bpr = bm->BytesPerRow;
    depth = bm->Depth;
    
    /* Calculate byte range we need to save for each row */
    startByte = x / 8;
    endByte = (x + w + 7) / 8;
    saveBytes = endByte - startByte;
    
    /* Total size: saveBytes * h * depth */
    totalSize = (ULONG)saveBytes * (ULONG)h * (ULONG)depth;
    if (totalSize == 0 || totalSize > 65536)  /* Sanity limit */
        return;
    
    g_menu_save_buffer = (UBYTE *)AllocMem(totalSize, MEMF_PUBLIC);
    if (!g_menu_save_buffer)
        return;
    
    g_menu_save_size = totalSize;
    g_menu_save_x = x;
    g_menu_save_y = y;
    g_menu_save_w = w;
    g_menu_save_h = h;
    
    /* Copy planar data: for each plane, for each row, copy the relevant bytes */
    {
        UBYTE *dst = g_menu_save_buffer;
        for (plane = 0; plane < depth; plane++)
        {
            UBYTE *planeBase = (UBYTE *)bm->Planes[plane];
            if (!planeBase) continue;
            for (row = 0; row < h; row++)
            {
                UBYTE *src = planeBase + (ULONG)(y + row) * (ULONG)bpr + startByte;
                WORD i;
                for (i = 0; i < saveBytes; i++)
                    *dst++ = src[i];
            }
        }
    }
    
    DPRINTF(LOG_DEBUG, "_intuition: _save_menu_dropdown_area() saved %ldx%ld at (%d,%d), %ld bytes\n",
            (LONG)w, (LONG)h, (int)x, (int)y, (LONG)totalSize);
}

/*
 * Restore the previously saved menu drop-down area to the screen's BitMap.
 * Called when the menu closes or switches to a different menu.
 */
static void _restore_menu_dropdown_area(struct Screen *screen)
{
    struct BitMap *bm;
    WORD bpr;
    WORD depth;
    WORD startByte, endByte, saveBytes;
    WORD plane, row;
    
    if (!screen || !g_menu_save_buffer || g_menu_save_size == 0)
        return;
    
    bm = &screen->BitMap;
    if (!bm->Planes[0])
        return;
    
    bpr = bm->BytesPerRow;
    depth = bm->Depth;
    
    startByte = g_menu_save_x / 8;
    endByte = (g_menu_save_x + g_menu_save_w + 7) / 8;
    saveBytes = endByte - startByte;
    
    /* Restore planar data */
    {
        UBYTE *src = g_menu_save_buffer;
        for (plane = 0; plane < depth; plane++)
        {
            UBYTE *planeBase = (UBYTE *)bm->Planes[plane];
            if (!planeBase) continue;
            for (row = 0; row < g_menu_save_h; row++)
            {
                UBYTE *dst = planeBase + (ULONG)(g_menu_save_y + row) * (ULONG)bpr + startByte;
                WORD i;
                for (i = 0; i < saveBytes; i++)
                    dst[i] = *src++;
            }
        }
    }
    
    DPRINTF(LOG_DEBUG, "_intuition: _restore_menu_dropdown_area() restored %ldx%ld at (%d,%d)\n",
            (LONG)g_menu_save_w, (LONG)g_menu_save_h, (int)g_menu_save_x, (int)g_menu_save_y);
    
    /* Free the save buffer */
    FreeMem(g_menu_save_buffer, g_menu_save_size);
    g_menu_save_buffer = NULL;
    g_menu_save_size = 0;
    g_menu_save_w = 0;
    g_menu_save_h = 0;
}

/*
 * Render the screen title bar (shown when not in menu mode)
 * This restores the title bar after menu mode ends
 */
static void _render_screen_title_bar(struct Screen *screen)
{
    struct RastPort *rp;
    WORD barHeight;
    
    if (!screen)
        return;
    
    rp = &screen->RastPort;
    
    /* Validate RastPort has a valid BitMap */
    if (!rp->BitMap || !rp->BitMap->Planes[0])
        return;
    
    barHeight = screen->BarHeight + 1;
    
    /* Fill the title bar area with the screen title bar color (pen 1) */
    SetAPen(rp, 1);
    RectFill(rp, 0, 0, screen->Width - 1, barHeight - 1);
    
    /* Draw bottom border line */
    SetAPen(rp, 0);
    Move(rp, 0, barHeight - 1);
    Draw(rp, screen->Width - 1, barHeight - 1);
    
    /* Draw screen title if present */
    if (screen->Title)
    {
        SetAPen(rp, 0);    /* Text in black */
        SetBPen(rp, 1);    /* Background */
        SetDrMd(rp, JAM2);
        
        /* Draw title centered or left-aligned, after any screen gadgets */
        /* Standard position is a few pixels from the left */
        Move(rp, screen->BarHBorder, screen->BarVBorder + rp->TxBaseline);
        Text(rp, (STRPTR)screen->Title, strlen((const char *)screen->Title));
    }
    
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
    struct Menu *menu;
    WORD barHeight, barHBorder, barVBorder;
    WORD x, y;
    
    if (!window || !window->WScreen)
        return;
    
    screen = window->WScreen;
    rp = &screen->RastPort;
    
    /* Validate RastPort has a valid BitMap */
    if (!rp->BitMap || !rp->BitMap->Planes[0])
    {
        DPRINTF(LOG_ERROR, "_intuition: _render_menu_bar() invalid RastPort BitMap\n");
        return;
    }
    
    DPRINTF(LOG_DEBUG, "_intuition: _render_menu_bar() screen=%08lx rp=%08lx bm=%08lx planes[0]=%08lx\n",
            (ULONG)screen, (ULONG)rp, (ULONG)rp->BitMap, (ULONG)rp->BitMap->Planes[0]);
    DPRINTF(LOG_DEBUG, "_intuition: _render_menu_bar() width=%d height=%d barHeight=%d\n",
            screen->Width, screen->Height, screen->BarHeight + 1);
    
    barHeight = screen->BarHeight + 1;  /* BarHeight is one less than actual */
    barHBorder = screen->BarHBorder;
    barVBorder = screen->BarVBorder;
    
    /* Fill menu bar background with pen 1 (standard Amiga look) */
    SetAPen(rp, 1);
    RectFill(rp, 0, 0, screen->Width - 1, barHeight - 1);
    
    /* Draw bottom border line */
    SetAPen(rp, 0);
    Move(rp, 0, barHeight - 1);
    Draw(rp, screen->Width - 1, barHeight - 1);
    
    /* Render menu titles */
    if (window->MenuStrip)
    {
        SetAPen(rp, 0);    /* Text in black */
        SetBPen(rp, 1);    /* Background */
        SetDrMd(rp, JAM2);
        
        for (menu = window->MenuStrip; menu; menu = menu->NextMenu)
        {
            if (!menu->MenuName)
                continue;
            
            x = barHBorder + menu->LeftEdge;
            y = barVBorder;
            
            /* Highlight active menu */
            if (menu == g_active_menu)
            {
                /* Draw highlighted background */
                SetAPen(rp, 0);
                RectFill(rp, x - 2, 0, x + menu->Width + 1, barHeight - 2);
                SetAPen(rp, 1);
                SetBPen(rp, 0);
            }
            else
            {
                SetAPen(rp, 0);
                SetBPen(rp, 1);
            }
            
            /* Draw menu title text */
            Move(rp, x, y + rp->TxBaseline);
            Text(rp, (STRPTR)menu->MenuName, strlen((const char *)menu->MenuName));
        }
    }
}

/*
 * Save the screen area that would be covered by the drop-down for the given menu.
 * Must be called BEFORE _render_menu_items() when opening a new drop-down.
 */
static void _save_dropdown_for_menu(struct Window *window, struct Menu *menu)
{
    struct Screen *screen;
    struct MenuItem *item;
    WORD barHeight, menuX, menuY, menuWidth, menuHeight;
    WORD submenuX;
    WORD submenuY;
    WORD submenuWidth;
    WORD submenuHeight;
    WORD saveX;
    WORD saveY;
    WORD saveRight;
    WORD saveBottom;
    
    if (!window || !window->WScreen || !menu || !menu->FirstItem)
        return;
    
    screen = window->WScreen;
    barHeight = screen->BarHeight + 1;
    
    menuX = screen->BarHBorder + menu->LeftEdge;
    menuY = barHeight;
    
    /* Calculate menu dimensions from items (same logic as _render_menu_items) */
    menuWidth = 0;
    menuHeight = 0;
    for (item = menu->FirstItem; item; item = item->NextItem)
    {
        WORD w = item->LeftEdge + item->Width;
        WORD h = item->TopEdge + item->Height;
        if (w > menuWidth) menuWidth = w;
        if (h > menuHeight) menuHeight = h;
    }
    menuWidth += 8;    /* padding, matching _render_menu_items */
    menuHeight += 4;

    saveX = menuX;
    saveY = menuY;
    saveRight = menuX + menuWidth;
    saveBottom = menuY + menuHeight;

    if (_get_active_submenu_box(window, &submenuX, &submenuY, &submenuWidth, &submenuHeight))
    {
        if (submenuX < saveX)
            saveX = submenuX;
        if (submenuY < saveY)
            saveY = submenuY;
        if (submenuX + submenuWidth > saveRight)
            saveRight = submenuX + submenuWidth;
        if (submenuY + submenuHeight > saveBottom)
            saveBottom = submenuY + submenuHeight;
    }

    _save_menu_dropdown_area(screen, saveX, saveY, saveRight - saveX, saveBottom - saveY);
}

static void _render_menu_item_chain(struct RastPort *rp,
                                    struct MenuItem *firstItem,
                                    struct MenuItem *highlightedItem,
                                    WORD menuX, WORD menuY)
{
    struct MenuItem *item;
    struct IntuiText *it;
    WORD chain_left;
    WORD menuWidth;
    WORD menuHeight;
    WORD itemY;

    if (!rp || !firstItem)
        return;

    _get_menu_item_chain_box(firstItem, menuX, menuY, NULL, NULL, &menuWidth, &menuHeight);
    chain_left = firstItem->LeftEdge;

    SetAPen(rp, 1);
    RectFill(rp, menuX, menuY, menuX + menuWidth - 1, menuY + menuHeight - 1);

    SetAPen(rp, 2);
    Move(rp, menuX, menuY + menuHeight - 2);
    Draw(rp, menuX, menuY);
    Draw(rp, menuX + menuWidth - 2, menuY);

    SetAPen(rp, 0);
    Move(rp, menuX + menuWidth - 1, menuY);
    Draw(rp, menuX + menuWidth - 1, menuY + menuHeight - 1);
    Draw(rp, menuX, menuY + menuHeight - 1);

    for (item = firstItem; item; item = item->NextItem)
    {
        WORD item_box_x = menuX + item->LeftEdge - chain_left;
        WORD itemX = item_box_x + 4;

        itemY = menuY + item->TopEdge + 2;

        if (item == highlightedItem && (item->Flags & ITEMENABLED))
        {
            SetAPen(rp, 0);
            RectFill(rp, menuX + 2, itemY - 1,
                     menuX + menuWidth - 3, itemY + item->Height - 2);
            SetAPen(rp, 1);
            SetBPen(rp, 0);
        }
        else
        {
            SetAPen(rp, 0);
            SetBPen(rp, 1);
        }

        it = (struct IntuiText *)item->ItemFill;

        if (_is_separator_menu_item(item))
        {
            WORD separator_y = itemY + (item->Height / 2) - 1;
            WORD separator_left = item_box_x + 6;
            WORD separator_right = item_box_x + item->Width - 7;

            if (separator_right >= separator_left)
            {
                SetAPen(rp, 0);
                Move(rp, separator_left, separator_y + 1);
                Draw(rp, separator_right, separator_y + 1);

                SetAPen(rp, 2);
                Move(rp, separator_left, separator_y);
                Draw(rp, separator_right, separator_y);
            }

            continue;
        }

        if (it && it->IText)
        {
            if (!(item->Flags & ITEMENABLED))
            {
                SetAPen(rp, 2);
                SetBPen(rp, 1);
            }

            Move(rp, itemX + it->LeftEdge, itemY + it->TopEdge + rp->TxBaseline);
            Text(rp, (STRPTR)it->IText, strlen((const char *)it->IText));

            if (item->Flags & CHECKED)
            {
                Move(rp, menuX + 3, itemY + rp->TxBaseline);
                Text(rp, (STRPTR)"\x9E", 1);
            }

            if (item->Flags & COMMSEQ)
            {
                char cmdStr[4];
                cmdStr[0] = 'A';
                cmdStr[1] = '-';
                cmdStr[2] = item->Command;
                cmdStr[3] = '\0';
                Move(rp, menuX + menuWidth - 40, itemY + rp->TxBaseline);
                Text(rp, (STRPTR)cmdStr, 3);
            }

            if (item->SubItem)
            {
                WORD indicator_right = item_box_x + item->Width - 7;
                WORD indicator_center_y = itemY + (item->Height / 2) - 1;

                _draw_submenu_indicator(rp, indicator_right, indicator_center_y);
            }
        }
    }
}

/*
 * Render the drop-down menu for the active menu
 */
static void _render_menu_items(struct Window *window)
{
    struct Screen *screen;
    struct RastPort *rp;
    struct Menu *menu;
    WORD barHeight;
    WORD menuX, menuY;
    WORD submenuX;
    WORD submenuY;
    WORD submenuWidth;
    WORD submenuHeight;
    
    if (!window || !window->WScreen || !g_active_menu)
        return;
    
    screen = window->WScreen;
    rp = &screen->RastPort;
    
    /* Validate RastPort has a valid BitMap */
    if (!rp->BitMap || !rp->BitMap->Planes[0])
    {
        DPRINTF(LOG_ERROR, "_intuition: _render_menu_items() invalid RastPort BitMap\n");
        return;
    }
    
    menu = g_active_menu;
    
    barHeight = screen->BarHeight + 1;
    
    /* Calculate menu drop-down position and size */
    menuX = screen->BarHBorder + menu->LeftEdge;
    menuY = barHeight;

    SetDrMd(rp, JAM2);

    _render_menu_item_chain(rp, menu->FirstItem, g_active_item, menuX, menuY);

    if (_get_active_submenu_box(window, &submenuX, &submenuY, &submenuWidth, &submenuHeight))
    {
        (void)submenuWidth;
        (void)submenuHeight;
        _render_menu_item_chain(rp, g_active_item->SubItem, g_active_subitem, submenuX, submenuY);
    }
}

/*
 * Enter menu mode - called on MENUDOWN
 */
static void _enter_menu_mode(struct Window *window, struct Screen *screen, WORD mouseX, WORD mouseY)
{
    if (!window || !window->MenuStrip || !screen)
        return;
    
    /* Check if window has WFLG_RMBTRAP - if so, don't enter menu mode */
    if (window->Flags & WFLG_RMBTRAP)
        return;
    
    g_menu_mode = TRUE;
    g_menu_window = window;
    g_active_menu = NULL;
    g_active_item = NULL;
    g_active_subitem = NULL;
    g_menu_selection = MENUNULL;
    
    /* Render the menu bar */
    _render_menu_bar(window);
    
    /* Check if mouse is already over a menu */
    if (mouseY < screen->BarHeight + 1)
    {
        struct Menu *menu = _find_menu_at_x(window, mouseX);
        if (menu && (menu->Flags & MENUENABLED))
        {
            g_active_menu = menu;
            _render_menu_bar(window);
            _save_dropdown_for_menu(window, menu);
            _render_menu_items(window);
        }
    }
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
    
    DPRINTF(LOG_DEBUG, "_intuition: Exiting menu mode, active_menu=0x%08lx active_item=0x%08lx\n",
            (ULONG)g_active_menu, (ULONG)g_active_item);
    
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
            menuCode = _encode_menu_selection(menuNum, itemNum, subNum);
            selected_item->NextSelect = MENUNULL;

            DPRINTF(LOG_DEBUG, "_intuition: Menu selection: menu=%d item=%d sub=%d code=0x%04x\n",
                    (int)menuNum, (int)itemNum, (int)subNum, menuCode);
        }
    }
    
    /* Post IDCMP_MENUPICK if we have a valid selection */
    if (g_menu_window && menuCode != MENUNULL)
    {
        WORD relX = mouseX - g_menu_window->LeftEdge;
        WORD relY = mouseY - g_menu_window->TopEdge;
        _post_idcmp_message(g_menu_window, IDCMP_MENUPICK, menuCode,
                           IEQUALIFIER_RBUTTON, NULL, relX, relY);
    }
    
    /* Clear menu mode state */
    g_menu_mode = FALSE;
    g_menu_window = NULL;
    g_active_menu = NULL;
    g_active_item = NULL;
    g_active_subitem = NULL;
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
}

/*
 * Handle keyboard input for an active string gadget
 * Returns TRUE if the gadget handled the input and remains active
 * Returns FALSE if the gadget deactivated (Return/Escape pressed or focus lost)
 * 
 * NOTE: This function may be called from interrupt context (via VBlank hook)
 * so it uses the global IntuitionBase instead of calling OpenLibrary().
 */
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
    
    /* Check for special keys */
    switch (code) {
        case 0x44: /* Return key */
            gad->Flags &= ~GFLG_SELECTED;
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
 * Internal function to post an IDCMP message to a window
 * Returns TRUE if message was posted, FALSE if window not interested
 */
static BOOL _post_idcmp_message(struct Window *window, ULONG class, UWORD code, 
                                 UWORD qualifier, APTR iaddress, WORD mouseX, WORD mouseY)
{
    struct LXAWindowState *state;
    struct LXAIntuiMessage *rawkey_msg;
    struct IntuiMessage *imsg;
    ULONG msg_size;
    
    if (!window || !window->UserPort || !window->WindowPort) {
        return FALSE;
    }
    
    /* Check if window is interested in this message class */
    if (!(window->IDCMPFlags & class)) {
        return FALSE;
    }

    state = _intuition_ensure_window_state((struct LXAIntuitionBase *)IntuitionBase, window);
    if (class == IDCMP_MOUSEMOVE && state)
    {
        if (state->pending_mousemoves >= state->mouse_queue)
            return FALSE;
    }
    
    _reap_window_idcmp_replies(window);

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
    imsg->ExecMessage.mn_ReplyPort = window->WindowPort;
    
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
    
    /* Post the message to the window's UserPort */
    PutMsg(window->UserPort, (struct Message *)imsg);

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
static volatile BOOL g_processing_events;

VOID _intuition_ProcessInputEvents(struct Screen *screen)
{
    ULONG event_type;
    ULONG mouse_pos;
    ULONG button_code;
    ULONG key_data;
    WORD mouseX, mouseY;
    struct Window *window;
    struct InputEvent input_event;
    
    if (!screen)
        return;
    
    /* Prevent re-entry - if already processing events, skip */
    if (g_processing_events)
    {
        DPRINTF(LOG_DEBUG, "_intuition: ProcessInputEvents SKIPPED (re-entry)\n");
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
            break;  /* No more events */
        
        DPRINTF(LOG_DEBUG, "_intuition: ProcessInputEvents: got event type=%ld screen=0x%08lx\n",
                event_type, (ULONG)screen);
        
        /* Get mouse position for all events */
        mouse_pos = emucall0(EMU_CALL_INT_GET_MOUSE_POS);
        mouseX = (WORD)(mouse_pos >> 16);
        mouseY = (WORD)(mouse_pos & 0xFFFF);
        
        /* Update IntuitionBase with current mouse position and timestamp */
        if (IntuitionBase)
        {
            struct timeval tv;
            emucall1(EMU_CALL_GETSYSTIME, (ULONG)&tv);
            IntuitionBase->MouseX = mouseX;
            IntuitionBase->MouseY = mouseY;
            IntuitionBase->Seconds = tv.tv_secs;
            IntuitionBase->Micros = tv.tv_micro;
        }
        
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

                input_event.ie_Class = IECLASS_RAWMOUSE;
                input_event.ie_Code = code;
                input_event.ie_Qualifier = qualifier;
                _input_device_dispatch_event(&input_event);
                
                DPRINTF(LOG_DEBUG, "_intuition: MouseButton code=0x%02x qual=0x%04x at (%d,%d)\n",
                        code, qualifier, mouseX, mouseY);
                
                if (window)
                {
                    /* Convert to window-relative coordinates */
                    WORD relX = mouseX - window->LeftEdge;
                    WORD relY = mouseY - window->TopEdge;
                    
                    DPRINTF(LOG_DEBUG, "_intuition: MouseButton: window=0x%08lx at (%d,%d) size=(%d,%d) rel=(%d,%d)\n",
                            (ULONG)window, window->LeftEdge, window->TopEdge, window->Width, window->Height, relX, relY);
                    
                    /* Check for gadget hit on mouse down (SELECTDOWN) */
                    if (code == SELECTDOWN)
                    {
                        struct Gadget *gad = _find_gadget_at_pos(window, relX, relY);
                        DPRINTF(LOG_DEBUG, "_intuition: SELECTDOWN relX=%d relY=%d gad=0x%08lx firstGad=0x%08lx type=0x%04x\n",
                                relX, relY, (ULONG)gad, (ULONG)window->FirstGadget,
                                gad ? gad->GadgetType : 0xFFFF);
                        if (gad)
                        {
                            DPRINTF(LOG_DEBUG, "_intuition: SELECTDOWN on gadget type=0x%04x\n",
                                    gad->GadgetType);
                            
                            /* Set as active gadget for tracking */
                            g_active_gadget = gad;
                            g_active_window = window;
                            
                            /* Set gadget as selected */
                            gad->Flags |= GFLG_SELECTED;

                            if (_gadtools_IsCheckbox(gad) || _gadtools_IsCycle(gad))
                                _render_gadget(window, NULL, gad);
                            
                            /* For sizing system gadget, start resize drag immediately */
                            if ((gad->GadgetType & GTYP_SYSGADGET) &&
                                (gad->GadgetType & GTYP_SYSTYPEMASK) == GTYP_SIZING)
                            {
                                DPRINTF(LOG_DEBUG, "_intuition: Starting window resize: window=0x%08lx size=(%d,%d)\n",
                                        (ULONG)window, window->Width, window->Height);
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
                                    /* Position cursor at end of text */
                                    si->BufferPos = si->NumChars;
                                }

                                _render_gadget(window, NULL, gad);
                            }
                            
                            /* For prop gadgets, compute drag offset for smooth knob tracking */
                            if ((gad->GadgetType & GTYP_GTYPEMASK) == GTYP_PROPGADGET)
                            {
                                struct PropInfo *pi = (struct PropInfo *)gad->SpecialInfo;
                                if (pi && (pi->Flags & AUTOKNOB))
                                {
                                    /* Compute the knob position to determine click offset.
                                     * Container is inset 1px from the gadget border. */
                                    WORD gadAbsL = gad->LeftEdge + window->LeftEdge;
                                    WORD containerL = gadAbsL + 1;
                                    WORD containerW = gad->Width - 2;
                                    WORD knobW;
                                    WORD maxMoveX;
                                    WORD knobX;

                                    if (pi->Flags & FREEHORIZ)
                                        knobW = (WORD)(((ULONG)containerW * (ULONG)pi->HorizBody) / 0xFFFF);
                                    else
                                        knobW = containerW;
                                    if (knobW < 6) knobW = 6;
                                    if (knobW > containerW) knobW = containerW;

                                    maxMoveX = containerW - knobW;
                                    if (maxMoveX < 0) maxMoveX = 0;
                                    knobX = containerL + (WORD)(((ULONG)maxMoveX * (ULONG)pi->HorizPot) / 0xFFFF);

                                    /* Store offset of click from knob left edge.
                                     * If click is outside knob, center the knob on click. */
                                    if (mouseX >= knobX && mouseX < knobX + knobW)
                                    {
                                        g_prop_click_offset = mouseX - knobX;
                                    }
                                    else
                                    {
                                        /* Click outside knob: jump knob center to click position */
                                        g_prop_click_offset = knobW / 2;
                                        WORD newKnobL = mouseX - g_prop_click_offset;
                                        if (newKnobL < containerL) newKnobL = containerL;
                                        if (newKnobL > containerL + maxMoveX)
                                            newKnobL = containerL + maxMoveX;
                                        if (maxMoveX > 0)
                                            pi->HorizPot = (UWORD)(((ULONG)(newKnobL - containerL) * 0xFFFF) / (ULONG)maxMoveX);
                                        else
                                            pi->HorizPot = 0;

                                        {
                                            WORD sl_min = (WORD)pi->CWidth;
                                            WORD sl_max = (WORD)pi->CHeight;
                                            LONG level;

                                            if (sl_max > sl_min)
                                                level = sl_min + ((LONG)pi->HorizPot * (sl_max - sl_min)) / 0xFFFF;
                                            else
                                                level = sl_min;

                                            _gadtools_UpdateSliderLevelDisplay(gad, level);
                                        }

                                        /* Redraw the gadget with new knob position */
                                        _render_gadget(window, NULL, gad);
                                    }

                                    DPRINTF(LOG_DEBUG, "_intuition: Prop SELECTDOWN: knobX=%d knobW=%d maxMove=%d offset=%d pot=%u\n",
                                            knobX, knobW, maxMoveX, g_prop_click_offset, pi->HorizPot);
                                }
                            }
                            
                            /* Post IDCMP_GADGETDOWN if it's a GADGIMMEDIATE gadget */
                            if (gad->Activation & GACT_IMMEDIATE)
                            {
                                _post_idcmp_message(window, IDCMP_GADGETDOWN, 0,
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
                                    /* Recover min/max stored in CWidth/CHeight by CreateGadgetA */
                                    WORD sl_min = (WORD)pi->CWidth;
                                    WORD sl_max = (WORD)pi->CHeight;
                                    LONG range = sl_max - sl_min;
                                    LONG level;

                                    if (range > 0)
                                        level = sl_min + ((LONG)pi->HorizPot * range) / 0xFFFF;
                                    else
                                        level = sl_min;
                                    _gadtools_UpdateSliderLevelDisplay(gad, level);
                                    level_code = (UWORD)level;

                                    DPRINTF(LOG_DEBUG, "_intuition: Prop SELECTUP: pot=%u min=%d max=%d level=%ld\n",
                                            pi->HorizPot, sl_min, sl_max, level);
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
                                    /* Handle system gadget verification */
                                    if (gad->GadgetType & GTYP_SYSGADGET)
                                    {
                                        _handle_sys_gadget_verify(activeWin, gad);
                                    }
                                    /* Post IDCMP_GADGETUP for RELVERIFY gadgets */
                                    else if (gad->Activation & GACT_RELVERIFY)
                                    {
                                        _post_idcmp_message(activeWin, IDCMP_GADGETUP, 0,
                                                           qualifier, gad, activeRelX, activeRelY);
                                    }
                                }
                                
                                /* Render normal state - undo GADGHCOMP highlight */
                                if (_gadtools_IsCheckbox(gad) || _gadtools_IsCycle(gad))
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
                        DPRINTF(LOG_DEBUG, "_intuition: MENUDOWN at (%d,%d), window=0x%08lx MenuStrip=0x%08lx\n",
                                mouseX, mouseY, (ULONG)window, window ? (ULONG)window->MenuStrip : 0);
                        /* 
                         * On real AmigaOS, right-clicking ANYWHERE on the screen activates the
                         * menu bar. The window that receives the menu is the currently active
                         * window with a menu strip, or the window under the mouse if it has a menu.
                         */
                        struct Window *menuWin = NULL;
                        
                        /* First, check the window under the mouse */
                        if (window && window->MenuStrip && !(window->Flags & WFLG_RMBTRAP))
                        {
                            menuWin = window;
                        }
                        else
                        {
                            /* Otherwise, find the first window with a menu strip on this screen */
                            menuWin = screen->FirstWindow;
                            while (menuWin)
                            {
                                if (menuWin->MenuStrip && !(menuWin->Flags & WFLG_RMBTRAP))
                                    break;
                                menuWin = menuWin->NextWindow;
                            }
                        }
                        
                        if (menuWin && menuWin->MenuStrip)
                        {
                            DPRINTF(LOG_DEBUG, "_intuition: Entering menu mode for menuWin=0x%08lx\n", (ULONG)menuWin);
                            _enter_menu_mode(menuWin, screen, mouseX, mouseY);
                        }
                    }
                    /* Right mouse button release - exit menu mode and select item */
                    else if (code == MENUUP)
                    {
                        DPRINTF(LOG_DEBUG, "_intuition: MENUUP at (%d,%d), g_menu_mode=%d, g_active_menu=0x%08lx, g_active_item=0x%08lx\n",
                                mouseX, mouseY, g_menu_mode, (ULONG)g_active_menu, (ULONG)g_active_item);
                        if (g_menu_mode && g_menu_window)
                        {
                            _exit_menu_mode(g_menu_window, mouseX, mouseY);
                        }
                    }
                    
                    /* Post IDCMP_MOUSEBUTTONS for general notification */
                    _post_idcmp_message(window, IDCMP_MOUSEBUTTONS, code, 
                                       qualifier, NULL, relX, relY);
                }
                break;
            }
            
            case 2:  /* Mouse move */
            {
                key_data = emucall0(EMU_CALL_INT_GET_KEY);  /* Get qualifier */
                UWORD qualifier = (UWORD)(key_data >> 16);

                input_event.ie_Class = IECLASS_POINTERPOS;
                input_event.ie_Qualifier = qualifier;
                _input_device_dispatch_event(&input_event);
                
                /* Handle menu tracking when in menu mode */
                if (g_menu_mode && g_menu_window)
                {
                    BOOL needRedraw = FALSE;
                    struct Menu *oldMenu = g_active_menu;
                    struct MenuItem *oldItem = g_active_item;
                    
                    /* Check if mouse is in menu bar area */
                    if (mouseY < screen->BarHeight + 1)
                    {
                        struct Menu *newMenu = _find_menu_at_x(g_menu_window, mouseX);
                        if (newMenu != g_active_menu)
                        {
                            g_active_menu = newMenu;
                            g_active_item = NULL;  /* Clear item when switching menus */
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
                                newSubItem = _find_item_in_chain_at_pos(g_active_item->SubItem,
                                                                        mouseX - submenuX,
                                                                        mouseY - submenuY);
                                if (newSubItem != g_active_subitem)
                                {
                                    g_active_subitem = newSubItem;
                                    needRedraw = TRUE;
                                }
                            }
                        }

                        if (!handledSubmenu)
                        {
                            /* Check if mouse is in the main drop-down menu area.
                             * This keeps lower menu items responsive after visiting
                             * a submenu instead of pinning the parent item highlight. */
                            WORD menuTop = screen->BarHeight + 1;
                            WORD menuLeft = screen->BarHBorder + g_active_menu->LeftEdge;
                            WORD itemX = mouseX - menuLeft;
                            WORD itemY = mouseY - menuTop;
                            struct MenuItem *newItem = _find_item_at_pos(g_active_menu, itemX, itemY);

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
                        /* Mouse outside menu areas */
                        if (g_active_item || g_active_subitem)
                        {
                            g_active_item = NULL;
                            g_active_subitem = NULL;
                            needRedraw = TRUE;
                        }
                    }
                    
                    /* Redraw if selection changed */
                    if (needRedraw)
                    {
                        if (_menu_hover_redraw_can_repaint_in_place(g_menu_window,
                                                                    oldMenu,
                                                                    oldItem))
                        {
                            _render_menu_items(g_menu_window);
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
                    struct PropInfo *pi = (struct PropInfo *)gad->SpecialInfo;

                    if (pi && (pi->Flags & AUTOKNOB) && (pi->Flags & FREEHORIZ))
                    {
                        WORD gadAbsL = gad->LeftEdge + activeWin->LeftEdge;
                        WORD containerL = gadAbsL + 1;
                        WORD containerW = gad->Width - 2;
                        WORD knobW;
                        WORD maxMoveX;
                        WORD newKnobL;
                        UWORD old_pot = pi->HorizPot;

                        knobW = (WORD)(((ULONG)containerW * (ULONG)pi->HorizBody) / 0xFFFF);
                        if (knobW < 6) knobW = 6;
                        if (knobW > containerW) knobW = containerW;

                        maxMoveX = containerW - knobW;
                        if (maxMoveX < 0) maxMoveX = 0;

                        /* Calculate new knob position based on mouse position and drag offset */
                        newKnobL = mouseX - g_prop_click_offset;
                        if (newKnobL < containerL) newKnobL = containerL;
                        if (newKnobL > containerL + maxMoveX)
                            newKnobL = containerL + maxMoveX;

                        if (maxMoveX > 0)
                            pi->HorizPot = (UWORD)(((ULONG)(newKnobL - containerL) * 0xFFFF) / (ULONG)maxMoveX);
                        else
                            pi->HorizPot = 0;

                        /* Only re-render and post message if pot actually changed */
                        if (pi->HorizPot != old_pot)
                        {
                            /* Redraw the gadget with updated knob position */
                            _render_gadget(activeWin, NULL, gad);

                            /* Compute current level for the MOUSEMOVE message Code field.
                             * GadTools convention: MOUSEMOVE for sliders carries the
                             * gadget pointer in IAddress and the level in Code. */
                            WORD sl_min = (WORD)pi->CWidth;
                            WORD sl_max = (WORD)pi->CHeight;
                            LONG range = sl_max - sl_min;
                            LONG level;
                            if (range > 0)
                                level = sl_min + ((LONG)pi->HorizPot * range) / 0xFFFF;
                            else
                                level = sl_min;

                            _gadtools_UpdateSliderLevelDisplay(gad, level);

                            WORD relX = mouseX - activeWin->LeftEdge;
                            WORD relY = mouseY - activeWin->TopEdge;
                            _post_idcmp_message(activeWin, IDCMP_MOUSEMOVE, (UWORD)level,
                                               qualifier, gad, relX, relY);
                        }
                    }
                }
                
                if (window)
                {
                    /* Update window mouse position */
                    WORD relX = mouseX - window->LeftEdge;
                    WORD relY = mouseY - window->TopEdge;
                    window->MouseX = relX;
                    window->MouseY = relY;
                    
                    /* Post IDCMP_MOUSEMOVE if requested */
                    _post_idcmp_message(window, IDCMP_MOUSEMOVE, 0, 
                                       qualifier, NULL, relX, relY);
                }
                break;
            }
            
            case 3:  /* Key */
            {
                key_data = emucall0(EMU_CALL_INT_GET_KEY);
                UWORD rawkey = (UWORD)(key_data & 0xFFFF);
                UWORD qualifier = (UWORD)(key_data >> 16);

                input_event.ie_Class = IECLASS_RAWKEY;
                input_event.ie_Code = rawkey;
                input_event.ie_Qualifier = qualifier;
                _input_device_dispatch_event(&input_event);
                _keyboard_device_record_event(rawkey, qualifier);
                
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
                else if (window)
                {
                    struct LXAWindowState *state = _intuition_ensure_window_state(
                        (struct LXAIntuitionBase *)IntuitionBase, window);
                    WORD relX = mouseX - window->LeftEdge;
                    WORD relY = mouseY - window->TopEdge;
                    BOOL posted = FALSE;

                    /* If window wants VANILLAKEY, try to convert rawkey to ASCII */
                    if (window->IDCMPFlags & IDCMP_VANILLAKEY)
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
                /* System quit requested - post close to all windows */
                for (window = screen->FirstWindow; window; window = window->NextWindow)
                {
                    _post_idcmp_message(window, IDCMP_CLOSEWINDOW, 0, 0, window, 0, 0);
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
    struct Screen *screen;
    
    /* Use global IntuitionBase - no OpenLibrary calls from interrupt context! */
    if (!IntuitionBase)
    {
        LPRINTF(LOG_WARNING, "_intuition: VBlankInputHook: IntuitionBase is NULL!\n");
        return;
    }
    
    screen = IntuitionBase->FirstScreen;
    if (!screen)
    {
        /* No screen yet - this is normal during startup */
    }
    
    for (; screen; screen = screen->NextScreen)
    {
        _intuition_ProcessInputEvents(screen);
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
    DPRINTF (LOG_DEBUG, "_intuition: MoveScreen() screen=0x%08lx dx=%d dy=%d\n", (ULONG)screen, dx, dy);
    
    if (!screen) return;
    
    screen->LeftEdge += dx;
    screen->TopEdge += dy;
    
    /* TODO: Update display hardware/host window if applicable */
}

VOID _intuition_MoveWindow ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Window * window __asm("a0"),
                                                        register WORD dx __asm("d0"),
                                                        register WORD dy __asm("d1"))
{
    LONG new_x, new_y;
    BOOL rootless_mode;

    DPRINTF(LOG_DEBUG, "_intuition: MoveWindow() window=0x%08lx dx=%d dy=%d\n", (ULONG)window, dx, dy);

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

struct Screen * _intuition_OpenScreen ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register const struct NewScreen * newScreen __asm("a0"))
{
    struct Screen *screen;
    ULONG display_handle;
    UWORD width, height;
    UBYTE depth;
    UBYTE i;

    LPRINTF (LOG_INFO, "_intuition: OpenScreen() newScreen=0x%08lx\n", (ULONG)newScreen);

    if (!newScreen)
    {
        LPRINTF (LOG_ERROR, "_intuition: OpenScreen() called with NULL newScreen\n");
        return NULL;
    }

    /* Get screen dimensions */
    width = newScreen->Width;
    height = newScreen->Height;
    depth = (UBYTE)newScreen->Depth;

    /* Use defaults if width/height are 0 (means use display defaults) */
    if (width == 0 || width == (UWORD)-1)
        width = 640;
    if (height == 0 || height == (UWORD)-1)
        height = 256;
    if (depth == 0)
        depth = 2;
    
    /*
     * Handle ViewModes-based height expansion.
     * 
     * On real Amigas, when ViewModes specifies a display mode but the height
     * is suspiciously small (less than MIN_USABLE_HEIGHT), the system would use
     * the display mode's standard height instead.
     * 
     * Standard heights (PAL):
     * - Non-interlaced: 256 lines
     * - Interlaced (LACE): 512 lines
     * 
     * Many older applications relied on this behavior (e.g., GFA Basic).
     */
    if (height < MIN_USABLE_HEIGHT)
    {
        UWORD viewModes = newScreen->ViewModes;
        UWORD defaultHeight;
        
        /* Determine standard height based on ViewModes */
        if (viewModes & LACE_FLAG)
        {
            /* Interlaced mode: 512 lines (PAL) */
            defaultHeight = 512;
        }
        else
        {
            /* Non-interlaced: 256 lines (PAL) */
            defaultHeight = 256;
        }
        
        LPRINTF(LOG_INFO, "_intuition: OpenScreen() height %d < %d, expanding to %d based on ViewModes 0x%04x\n",
                (int)height, MIN_USABLE_HEIGHT, (int)defaultHeight, (unsigned)viewModes);
        height = defaultHeight;
    }

    DPRINTF (LOG_DEBUG, "_intuition: OpenScreen() %dx%dx%d\n",
             (int)width, (int)height, (int)depth);

    /* Allocate Screen structure */
    screen = (struct Screen *)AllocMem(sizeof(struct Screen), MEMF_PUBLIC | MEMF_CLEAR);
    if (!screen)
    {
        LPRINTF (LOG_ERROR, "_intuition: OpenScreen() out of memory for Screen\n");
        return NULL;
    }

    /* Open host display via emucall */
    /* Pack width/height into d1, depth into d2, title into d3 */
    display_handle = emucall3(EMU_CALL_INT_OPEN_SCREEN,
                              ((ULONG)width << 16) | (ULONG)height,
                              (ULONG)depth,
                              (ULONG)newScreen->DefaultTitle);

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
    screen->Flags = newScreen->Type;
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
    ULONG num_colors = 1UL << depth;
    screen->ViewPort.ColorMap = GetColorMap(num_colors);
    if (screen->ViewPort.ColorMap)
    {
        /* Initialize default Workbench colors for first 4 entries */
        /* These are the standard Amiga 2.0+ colors */
        SetRGB4CM(screen->ViewPort.ColorMap, 0, 0xA, 0xA, 0xA);  /* Gray background */
        SetRGB4CM(screen->ViewPort.ColorMap, 1, 0x0, 0x0, 0x0);  /* Black */
        SetRGB4CM(screen->ViewPort.ColorMap, 2, 0xF, 0xF, 0xF);  /* White */
        SetRGB4CM(screen->ViewPort.ColorMap, 3, 0x0, 0x5, 0xA);  /* Blue */
        
        /* Propagate initial palette to host display.
         * We do this directly via emucall because the screen isn't linked
         * into IntuitionBase->FirstScreen yet at this point.
         */
        emucall3(EMU_CALL_GFX_SET_COLOR, display_handle, 0, 0x00AAAAAA);  /* Gray */
        emucall3(EMU_CALL_GFX_SET_COLOR, display_handle, 1, 0x00000000);  /* Black */
        emucall3(EMU_CALL_GFX_SET_COLOR, display_handle, 2, 0x00FFFFFF);  /* White */
        emucall3(EMU_CALL_GFX_SET_COLOR, display_handle, 3, 0x000055AA);  /* Blue */
    }

    /* Set bar heights (simplified) */
    screen->BarHeight = 10;
    screen->BarVBorder = 1;
    screen->BarHBorder = 5;
    screen->WBorTop = 11;
    screen->WBorLeft = 4;
    screen->WBorRight = 4;
    screen->WBorBottom = 2;

    /* Set screen font (TextAttr pointer).
     * Per RKRM, screen->Font describes the default font for this screen.
     * If the caller supplied a font via NewScreen.Font (or SA_Font tag),
     * use that; otherwise, allocate a TextAttr describing the default
     * system font (Topaz 8).
     */
    if (newScreen->Font)
    {
        screen->Font = newScreen->Font;
    }
    else
    {
        /* Allocate a persistent TextAttr for the default font */
        struct TextAttr *ta;
        ta = (struct TextAttr *)AllocMem(sizeof(struct TextAttr), MEMF_PUBLIC | MEMF_CLEAR);
        if (ta)
        {
            ta->ta_Name = (STRPTR)"topaz.font";
            ta->ta_YSize = 8;
            ta->ta_Style = 0;
            ta->ta_Flags = 0;
        }
        screen->Font = ta;
    }

    /* Clear the screen to color 0 */
    SetRast(&screen->RastPort, 0);

    /* Initialize Layer_Info for this screen */
    InitLayers(&screen->LayerInfo);
    screen->LayerInfo.top_layer = NULL;

    /* Link screen into IntuitionBase screen list (at front) */
    screen->NextScreen = IntuitionBase->FirstScreen;
    IntuitionBase->FirstScreen = screen;
    _intuition_register_pubscreen(IntuitionBase, screen);
    
    /* Debug: print offset of FirstScreen */
    LPRINTF(LOG_INFO, "[ROM] OpenScreen: offsetof(FirstScreen)=%d, sizeof(IntuitionBase)=%d, sizeof(Library)=%d, sizeof(View)=%d\n",
            (int)((char*)&IntuitionBase->FirstScreen - (char*)IntuitionBase),
            (int)sizeof(struct IntuitionBase), (int)sizeof(struct Library), (int)sizeof(struct View));
    LPRINTF(LOG_INFO, "[ROM] OpenScreen: IntuitionBase=0x%08lx, FirstScreen set to 0x%08lx\n",
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

/*
 * System Gadget constants
 * These define the standard sizes for Amiga window gadgets
 */
#define SYS_GADGET_WIDTH   18  /* Width of close/depth gadgets */
#define SYS_GADGET_HEIGHT  10  /* Height of system gadgets (based on title bar) */

/*
 * Create a system gadget for a window
 * Returns the allocated gadget or NULL on failure
 */
static struct Gadget * _create_sys_gadget(struct Window *window, UWORD type,
                                           WORD left, WORD top, WORD width, WORD height)
{
    struct Gadget *gad;
    
    gad = (struct Gadget *)AllocMem(sizeof(struct Gadget), MEMF_PUBLIC | MEMF_CLEAR);
    if (!gad)
        return NULL;
    
    gad->LeftEdge = left;
    gad->TopEdge = top;
    gad->Width = width;
    gad->Height = height;
    gad->Flags = GFLG_GADGHCOMP;  /* Complement on select */
    gad->Activation = GACT_RELVERIFY | GACT_TOPBORDER;
    gad->GadgetType = GTYP_SYSGADGET | type | GTYP_BOOLGADGET;
    gad->GadgetRender = NULL;
    gad->SelectRender = NULL;
    gad->GadgetText = NULL;
    gad->SpecialInfo = NULL;
    gad->GadgetID = type;
    gad->UserData = (APTR)window;  /* Back-link to window */
    
    return gad;
}

/*
 * Create system gadgets for a window based on its flags
 * Links them into the window's gadget list
 */
static void _create_window_sys_gadgets(struct Window *window)
{
    struct Gadget *gad;
    struct Gadget **lastPtr = &window->FirstGadget;
    WORD gadWidth = SYS_GADGET_WIDTH;
    WORD gadHeight = window->BorderTop > 0 ? window->BorderTop - 1 : SYS_GADGET_HEIGHT;
    /* Title bar system gadgets (close, depth, zoom) are positioned relative to the
     * window edges, not the border area. The depth gadget goes at the far right. */
    WORD rightX = window->Width - gadWidth;
    
    /* Skip to end of existing gadget list */
    while (*lastPtr)
        lastPtr = &(*lastPtr)->NextGadget;
    
    /* Create Close gadget (top-left) if requested */
    if (window->Flags & WFLG_CLOSEGADGET)
    {
        gad = _create_sys_gadget(window, GTYP_CLOSE, 
                                  window->BorderLeft, 0, 
                                  gadWidth, gadHeight);
        if (gad)
        {
            *lastPtr = gad;
            lastPtr = &gad->NextGadget;
            DPRINTF(LOG_DEBUG, "_intuition: Created CLOSE gadget at (%d,%d) %dx%d\n",
                    (int)gad->LeftEdge, (int)gad->TopEdge, (int)gad->Width, (int)gad->Height);
        }
    }
    
    /* Create Depth gadget (top-right) if requested */
    if (window->Flags & WFLG_DEPTHGADGET)
    {
        gad = _create_sys_gadget(window, GTYP_WDEPTH,
                                  rightX, 0,
                                  gadWidth, gadHeight);
        if (gad)
        {
            *lastPtr = gad;
            lastPtr = &gad->NextGadget;
            DPRINTF(LOG_DEBUG, "_intuition: Created DEPTH gadget at (%d,%d) %dx%d\n",
                    (int)gad->LeftEdge, (int)gad->TopEdge, (int)gad->Width, (int)gad->Height);
            rightX -= gadWidth;  /* Next gadget goes to the left */
        }
    }
    
    /* Create Sizing gadget (bottom-right) if requested.
     * Per RKRM/NDK, the sizing gadget uses GFLG_RELRIGHT | GFLG_RELBOTTOM
     * so it tracks the window's bottom-right corner as the window is resized.
     * LeftEdge and TopEdge are negative offsets from the corner.
     * Activation uses GACT_RIGHTBORDER | GACT_BOTTOMBORDER (not TOPBORDER). */
    if (window->Flags & WFLG_SIZEGADGET)
    {
        gad = (struct Gadget *)AllocMem(sizeof(struct Gadget), MEMF_PUBLIC | MEMF_CLEAR);
        if (gad)
        {
            gad->LeftEdge = -(gadWidth - 1);     /* Offset from right edge */
            gad->TopEdge = -(SYS_GADGET_HEIGHT - 1);  /* Offset from bottom edge */
            gad->Width = gadWidth;
            gad->Height = SYS_GADGET_HEIGHT;
            gad->Flags = GFLG_GADGHCOMP | GFLG_RELRIGHT | GFLG_RELBOTTOM;
            gad->Activation = GACT_RELVERIFY | GACT_RIGHTBORDER | GACT_BOTTOMBORDER;
            gad->GadgetType = GTYP_SYSGADGET | GTYP_SIZING | GTYP_BOOLGADGET;
            gad->GadgetRender = NULL;
            gad->SelectRender = NULL;
            gad->GadgetText = NULL;
            gad->SpecialInfo = NULL;
            gad->GadgetID = GTYP_SIZING;
            gad->UserData = (APTR)window;

            *lastPtr = gad;
            lastPtr = &gad->NextGadget;
            DPRINTF(LOG_DEBUG, "_intuition: Created SIZING gadget at (%d,%d) %dx%d (relative)\n",
                    (int)gad->LeftEdge, (int)gad->TopEdge, (int)gad->Width, (int)gad->Height);
        }
    }
    
    /* Drag bar is handled via WFLG_DRAGBAR flag, not as a separate gadget */
    /* It uses the entire title bar area not covered by other gadgets */
}

/*
 * Render system gadgets for a window
 * Draws the window frame, title bar, and gadget imagery
 */
static void _render_window_frame(struct Window *window)
{
    struct RastPort *rp;
    WORD x0, y0, x1, y1;
    WORD titleBarBottom;
    WORD right_border_left;
    WORD right_border_top;
    WORD right_border_bottom;
    WORD bottom_border_top;
    WORD bottom_border_right;
    struct Gadget *gad;
    UBYTE detPen, blkPen, shiPen, shaPen;
    
    DPRINTF(LOG_DEBUG, "_intuition: _render_window_frame() window=0x%08lx RPort=0x%08lx\n", 
            (ULONG)window, window ? (ULONG)window->RPort : 0);
    
    if (!window || !window->RPort)
    {
        LPRINTF(LOG_WARNING, "_intuition: _render_window_frame() aborting - null window or RPort\n");
        return;
    }
    
    rp = window->RPort;
    
    /* Get pens - use window pens or defaults */
    detPen = window->DetailPen;
    blkPen = window->BlockPen;
    shiPen = 2;   /* Standard shine pen (white) */
    shaPen = 1;   /* Standard shadow pen (black) */
    
    DPRINTF (LOG_DEBUG, "_intuition: _render_window_frame() detPen=%d blkPen=%d shiPen=%d shaPen=%d\n",
             (int)detPen, (int)blkPen, (int)shiPen, (int)shaPen);
    
    /* Window interior bounds */
    x0 = 0;
    y0 = 0;
    x1 = window->Width - 1;
    y1 = window->Height - 1;
    titleBarBottom = window->BorderTop - 1;
    
    /* Skip if borderless */
    if (window->Flags & WFLG_BORDERLESS)
        return;
    
    /* Draw outer border (3D effect) */
    /* Top-left bright edge */
    SetAPen(rp, shiPen);
    Move(rp, x0, y1 - 1);
    Draw(rp, x0, y0);
    Draw(rp, x1 - 1, y0);
    
    /* Bottom-right dark edge */
    SetAPen(rp, shaPen);
    Move(rp, x1, y0 + 1);
    Draw(rp, x1, y1);
    Draw(rp, x0 + 1, y1);
    
    /* Fill title bar background if we have one */
    if (window->BorderTop > 1)
    {
        /* Fill title bar with block pen */
        SetAPen(rp, (window->Flags & WFLG_WINDOWACTIVE) ? blkPen : detPen);
        RectFill(rp, x0 + 1, y0 + 1, x1 - 1, titleBarBottom);
        
        /* Draw title bar bottom line */
        SetAPen(rp, shaPen);
        Move(rp, x0, titleBarBottom);
        Draw(rp, x1, titleBarBottom);
        
        /* Draw title text if present */
        if (window->Title)
        {
            WORD textX = window->BorderLeft;
            WORD textY = 1;  /* One pixel from top */
            
            /* Adjust for close gadget */
            if (window->Flags & WFLG_CLOSEGADGET)
                textX += SYS_GADGET_WIDTH + 2;
            
            /* Draw title */
            SetAPen(rp, (window->Flags & WFLG_WINDOWACTIVE) ? detPen : blkPen);
            SetBPen(rp, (window->Flags & WFLG_WINDOWACTIVE) ? blkPen : detPen);
            Move(rp, textX, textY + rp->TxBaseline);
            Text(rp, (STRPTR)window->Title, strlen((char *)window->Title));
        }
    }

    if (window->BorderRight > 1)
    {
        right_border_left = window->Width - window->BorderRight;
        right_border_top = window->BorderTop;
        right_border_bottom = window->Height - window->BorderBottom - 1;

        if (right_border_left <= x1 - 1 &&
            right_border_top <= right_border_bottom)
        {
            SetAPen(rp, 0);
            RectFill(rp,
                     right_border_left,
                     right_border_top,
                     x1 - 1,
                     right_border_bottom);
        }
    }

    if (window->BorderBottom > 1)
    {
        bottom_border_top = window->Height - window->BorderBottom;
        bottom_border_right = window->Width - 2;

        if (x0 + 1 <= bottom_border_right &&
            bottom_border_top <= y1 - 1)
        {
            SetAPen(rp, 0);
            RectFill(rp,
                     x0 + 1,
                     bottom_border_top,
                     bottom_border_right,
                     y1 - 1);
        }
    }
    
    /* Render system gadgets */
    for (gad = window->FirstGadget; gad; gad = gad->NextGadget)
    {
        UWORD sysType;
        WORD gx0, gy0, gx1, gy1;
        
        /* Skip non-system gadgets */
        if (!(gad->GadgetType & GTYP_SYSGADGET))
            continue;
        
        sysType = gad->GadgetType & GTYP_SYSTYPEMASK;
        gx0 = gad->LeftEdge;
        gy0 = gad->TopEdge;
        
        /* Handle GFLG_RELRIGHT / GFLG_RELBOTTOM for system gadgets
         * (e.g. the sizing gadget uses relative positioning) */
        if (gad->Flags & GFLG_RELRIGHT)
            gx0 += window->Width - 1;
        if (gad->Flags & GFLG_RELBOTTOM)
            gy0 += window->Height - 1;
        
        gx1 = gx0 + gad->Width - 1;
        gy1 = gy0 + gad->Height - 1;
        
        /* Draw gadget frame */
        SetAPen(rp, shiPen);
        Move(rp, gx0, gy1 - 1);
        Draw(rp, gx0, gy0);
        Draw(rp, gx1 - 1, gy0);
        SetAPen(rp, shaPen);
        Move(rp, gx1, gy0);
        Draw(rp, gx1, gy1);
        Draw(rp, gx0, gy1);
        
        /* Draw gadget-specific imagery */
        switch (sysType)
        {
            case GTYP_CLOSE:
            {
                /* Draw a small filled square in center (close icon) */
                WORD cx = (gx0 + gx1) / 2;
                WORD cy = (gy0 + gy1) / 2;
                SetAPen(rp, shaPen);
                RectFill(rp, cx - 2, cy - 2, cx + 2, cy + 2);
                SetAPen(rp, shiPen);
                RectFill(rp, cx - 1, cy - 1, cx + 1, cy + 1);
                break;
            }
            
            case GTYP_WDEPTH:
            {
                /* Draw two overlapping window-outline rectangles (depth icon).
                 * Matches AROS/AmigaOS classic look:
                 * - Back rect (top-left) drawn in shadow pen, filled with bg pen
                 * - Front rect (bottom-right) drawn in shadow pen, filled with shine pen
                 * This gives the visual of two overlapping window frames. */
                WORD il = gx0 + 1;     /* inner left (inside 3D frame) */
                WORD it = gy0 + 1;     /* inner top */
                WORD ir = gx1 - 1;     /* inner right */
                WORD ib = gy1 - 1;     /* inner bottom */
                WORD iw = ir - il + 1;
                WORD ih = ib - it + 1;
                WORD hs = iw / 6;      /* horizontal spacing */
                WORD vs = ih / 6;      /* vertical spacing */
                WORD dl, dt, dr, db, dw, dh;

                /* Fill gadget interior with background */
                SetAPen(rp, 0);  /* BACKGROUNDPEN */
                RectFill(rp, il, it, ir, ib);

                /* Apply spacing */
                dl = il + hs;
                dt = it + vs;
                dw = iw - hs * 2;
                dh = ih - vs * 2;
                dr = dl + dw - 1;
                db = dt + dh - 1;

                /* Back rectangle (top-left): outline in shadow pen */
                SetAPen(rp, shaPen);
                RectFill(rp, dl, dt, dr - dw / 3, dt);             /* top edge */
                RectFill(rp, dl, dt, dl, db - dh / 3);             /* left edge */
                RectFill(rp, dr - dw / 3, dt, dr - dw / 3, db - dh / 3); /* right edge */
                RectFill(rp, dl, db - dh / 3, dr - dw / 3, db - dh / 3); /* bottom edge */

                /* Front rectangle (bottom-right): outline in shadow pen */
                SetAPen(rp, shaPen);
                RectFill(rp, dl + dw / 3, dt + dh / 3, dr, dt + dh / 3);  /* top edge */
                RectFill(rp, dl + dw / 3, dt + dh / 3, dl + dw / 3, db);  /* left edge */
                RectFill(rp, dr, dt + dh / 3, dr, db);                     /* right edge */
                RectFill(rp, dl + dw / 3, db, dr, db);                     /* bottom edge */

                /* Fill front rectangle interior with shine pen */
                SetAPen(rp, shiPen);
                if (dl + dw / 3 + 1 <= dr - 1 && dt + dh / 3 + 1 <= db - 1)
                {
                    RectFill(rp, dl + dw / 3 + 1, dt + dh / 3 + 1,
                             dr - 1, db - 1);
                }
                break;
            }
            
            case GTYP_WDRAGGING:
                /* Drag gadget has no special imagery - just the frame */
                break;
            
            case GTYP_SIZING:
            {
                /* Draw sizing icon: small nested L-shapes in bottom-right corner.
                 * Classic Amiga style: two right-angle marks suggesting resize.
                 * Use 3D lines: shine on top-left, shadow on bottom-right. */
                WORD cx = (gx0 + gx1) / 2;
                WORD cy = (gy0 + gy1) / 2;
                
                /* Draw a small 3D box in the center (like a window thumbnail) */
                SetAPen(rp, shiPen);
                Move(rp, cx - 2, cy + 2);
                Draw(rp, cx - 2, cy - 2);
                Draw(rp, cx + 2, cy - 2);
                SetAPen(rp, shaPen);
                Draw(rp, cx + 2, cy + 2);
                Draw(rp, cx - 2, cy + 2);
                
                /* Draw a diagonal indicator */
                SetAPen(rp, shaPen);
                Move(rp, gx1 - 3, gy1 - 1);
                Draw(rp, gx1 - 1, gy1 - 3);
                break;
            }
        }
    }
    
    /* Render user gadgets after the frame/system gadgets. */
    _render_window_user_gadgets(window);
}

static void _render_window_user_gadgets(struct Window *window)
{
    struct Gadget *gad;

    if (!window)
        return;

    for (gad = window->FirstGadget; gad; gad = gad->NextGadget)
    {
        if (!(gad->GadgetType & GTYP_SYSGADGET))
            _render_gadget(window, NULL, gad);
    }
}

struct Window * _intuition_OpenWindow ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register const struct NewWindow * newWindow __asm("a0"))
{
    struct Window *window;
    struct Screen *screen;
    WORD width, height;
    ULONG rootless_mode;
    ULONG host_window_handle = 0;

    LPRINTF (LOG_INFO, "_intuition: OpenWindow() newWindow=0x%08lx\n", (ULONG)newWindow);

    if (!newWindow)
    {
        LPRINTF (LOG_ERROR, "_intuition: OpenWindow() called with NULL newWindow\n");
        return NULL;
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
        LPRINTF (LOG_INFO, "  Title string: '%s'\n", (char *)newWindow->Title);
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
            LPRINTF (LOG_INFO, "_intuition: OpenWindow() opening Workbench screen (called from OpenWindow)\n");
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
    width = newWindow->Width;
    height = newWindow->Height;

    /* Apply defaults if dimensions are zero or ~0 (sentinel from OpenWindowTagList with NULL newWindow) */
    if (width == 0 || width == (WORD)~0)
        width = screen->Width - newWindow->LeftEdge;
    if (height == 0 || height == (WORD)~0)
        height = screen->Height - newWindow->TopEdge;
    
    /*
     * Expand windows that appear to be sized for an unreasonably small screen.
     * 
     * When a screen's height was expanded (see OpenScreen height expansion logic),
     * windows that were sized to fill that small screen should also be expanded.
     * This handles cases where apps like GFA Basic used very small NewScreen heights
     * that got expanded, but the window dimensions still reference the old values.
     *
     * Heuristic: If the window is positioned at (0,y) and its height is very small
     * while the screen height is normal, expand the window to fill the screen vertically.
     */
    if (newWindow->LeftEdge == 0 && 
        width == screen->Width && height < MIN_USABLE_HEIGHT &&
        screen->Height >= MIN_USABLE_HEIGHT)
    {
        WORD newHeight = screen->Height - newWindow->TopEdge;
        LPRINTF(LOG_INFO, "_intuition: OpenWindow() window height %d < %d, expanding to %d\n",
                (int)height, MIN_USABLE_HEIGHT, (int)newHeight);
        height = newHeight;
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

    /* Initialize window fields */
    window->LeftEdge = newWindow->LeftEdge;
    window->TopEdge = newWindow->TopEdge;
    window->Width = width;
    window->Height = height;
    window->MinWidth = newWindow->MinWidth ? newWindow->MinWidth : width;
    window->MinHeight = newWindow->MinHeight ? newWindow->MinHeight : height;
    window->MaxWidth = newWindow->MaxWidth ? newWindow->MaxWidth : (UWORD)-1;
    window->MaxHeight = newWindow->MaxHeight ? newWindow->MaxHeight : (UWORD)-1;
    window->Flags = newWindow->Flags;
    window->IDCMPFlags = newWindow->IDCMPFlags;
    window->Title = newWindow->Title;
    window->WScreen = screen;
    
    /* Handle DetailPen and BlockPen: 0xFF (~0) means use screen defaults (per RKRM) */
    window->DetailPen = (newWindow->DetailPen == 0xFF) ? screen->DetailPen : newWindow->DetailPen;
    window->BlockPen = (newWindow->BlockPen == 0xFF) ? screen->BlockPen : newWindow->BlockPen;
    
    window->FirstGadget = newWindow->FirstGadget;
    window->CheckMark = newWindow->CheckMark;

    /* Set up border dimensions based on flags */
    if (newWindow->Flags & WFLG_BORDERLESS)
    {
        window->BorderLeft = 0;
        window->BorderTop = 0;
        window->BorderRight = 0;
        window->BorderBottom = 0;
    }
    else
    {
        /* Use the Screen's WBorXXX fields per NDK/AROS pattern */
        window->BorderLeft = screen->WBorLeft;
        window->BorderRight = screen->WBorRight;
        window->BorderBottom = screen->WBorBottom;
        
        /* Top border depends on whether we have a title bar */
        if ((newWindow->Flags & (WFLG_DRAGBAR | WFLG_CLOSEGADGET | WFLG_DEPTHGADGET)) || newWindow->Title)
        {
            /* Title bar: WBorTop + font height + 1 (per AROS pattern)
             * For now, use WBorTop which already includes space for title (11 pixels)
             * TODO: Use actual font height when fonts are supported:
             *   window->BorderTop = screen->WBorTop + GfxBase->DefaultFont->tf_YSize + 1;
             */
            window->BorderTop = screen->WBorTop;
        }
        else
        {
            /* No title bar, just use basic border */
            window->BorderTop = screen->WBorBottom;
        }
        
        /* Enlarge borders for sizing gadget per RKRM/NDK.
         * When WFLG_SIZEGADGET is set, the bottom and/or right border
         * must be large enough to contain the sizing gadget.
         * WFLG_SIZEBBOTTOM and WFLG_SIZEBRIGHT control which borders
         * are enlarged; if neither is set, both are enlarged by default. */
        if (newWindow->Flags & WFLG_SIZEGADGET)
        {
            BOOL sizeBBottom = (newWindow->Flags & WFLG_SIZEBBOTTOM) || 
                               !(newWindow->Flags & WFLG_SIZEBRIGHT);
            BOOL sizeBRight = (newWindow->Flags & WFLG_SIZEBRIGHT) ||
                              !(newWindow->Flags & WFLG_SIZEBBOTTOM);
            
            if (sizeBBottom && window->BorderBottom < SYS_GADGET_HEIGHT)
                window->BorderBottom = SYS_GADGET_HEIGHT;
            if (sizeBRight && window->BorderRight < SYS_GADGET_WIDTH)
                window->BorderRight = SYS_GADGET_WIDTH;
        }
    }

    if (window->Flags & WFLG_GIMMEZEROZERO)
    {
        window->GZZWidth = window->Width - window->BorderLeft - window->BorderRight;
        window->GZZHeight = window->Height - window->BorderTop - window->BorderBottom;
    }

    /* Create a Layer for this window so graphics operations use proper coordinate offsets */
    {
        struct Layer *layer;
        LONG x0 = newWindow->LeftEdge;
        LONG y0 = newWindow->TopEdge;
        LONG x1 = newWindow->LeftEdge + width - 1;
        LONG y1 = newWindow->TopEdge + height - 1;
        
        layer = CreateUpfrontLayer(&screen->LayerInfo, &screen->BitMap,
                                   x0, y0, x1, y1, LAYERSIMPLE, NULL);
        if (layer)
        {
            window->WLayer = layer;
            window->RPort = layer->rp;
            DPRINTF(LOG_DEBUG, "_intuition: OpenWindow() created layer=0x%08lx rp=0x%08lx bounds=[%ld,%ld]-[%ld,%ld]\n",
                    (ULONG)layer, (ULONG)layer->rp, x0, y0, x1, y1);
        }
        else
        {
            /* Fallback to screen's RastPort if layer creation fails */
            DPRINTF(LOG_WARNING, "_intuition: OpenWindow() layer creation failed, using screen RastPort\n");
            window->RPort = &screen->RastPort;
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

        window_handle = emucall4(EMU_CALL_INT_OPEN_WINDOW,
                                 screen_handle,
                                 ((ULONG)(WORD)newWindow->LeftEdge << 16) | ((ULONG)(WORD)newWindow->TopEdge & 0xFFFF),
                                 ((ULONG)width << 16) | ((ULONG)height & 0xFFFF),
                                 (ULONG)newWindow->Title);

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
            _post_idcmp_message(prevActive, IDCMP_INACTIVEWINDOW, 0, 0,
                                prevActive, 0, 0);
        }
        IntuitionBase->ActiveWindow = window;
        window->Flags |= WFLG_WINDOWACTIVE;

        if (window->IDCMPFlags & IDCMP_ACTIVEWINDOW)
        {
            _post_idcmp_message(window, IDCMP_ACTIVEWINDOW, 0, 0,
                                window, 0, 0);
        }
    }

    /* Create system gadgets based on window flags */
    _create_window_sys_gadgets(window);
    
    window->FirstGadget = _intuition_public_gadget_list(window->FirstGadget);

    /* Initialize string gadget NumChars for user gadgets (per RKRM, Intuition does this) */
    {
        struct Gadget *gad = window->FirstGadget;
        while (gad)
        {
            _init_string_gadget_info(gad);
            gad = gad->NextGadget;
        }
    }
    
    /* Render initial visuals.
     * Rootless windows still need user gadgets drawn into the backing bitmap,
     * even though the host window manager supplies the outer frame. */
    if (!rootless_mode)
    {
        _render_window_frame(window);
    }
    else
    {
        _render_window_user_gadgets(window);
    }

    DPRINTF (LOG_DEBUG, "_intuition: OpenWindow() -> 0x%08lx\n", (ULONG)window);

    return window;
}

ULONG _intuition_OpenWorkBench ( register struct IntuitionBase * IntuitionBase __asm("a6"))
{
    struct Screen *wbscreen;
    struct NewScreen ns;
    
    LPRINTF (LOG_INFO, "_intuition: OpenWorkBench() called, FirstScreen=0x%08lx\n", 
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
    
    /* Create a new Workbench screen */
    memset(&ns, 0, sizeof(ns));
    ns.LeftEdge = 0;
    ns.TopEdge = 0;
    ns.Width = 640;
    ns.Height = 256;
    ns.Depth = 2;
    ns.DetailPen = 0;
    ns.BlockPen = 1;
    ns.Type = WBENCHSCREEN;
    ns.DefaultTitle = (UBYTE *)"Workbench Screen";
    
    wbscreen = _intuition_OpenScreen(IntuitionBase, &ns);
    
    if (wbscreen)
    {
        LPRINTF (LOG_INFO, "_intuition: OpenWorkBench() - opened at 0x%08lx, Width=%d Height=%d\n", 
                 (ULONG)wbscreen, (int)wbscreen->Width, (int)wbscreen->Height);
        LPRINTF (LOG_INFO, "_intuition: OpenWorkBench() FirstScreen=0x%08lx, FirstScreen->Width=%d Height=%d\n",
                 (ULONG)IntuitionBase->FirstScreen,
                 IntuitionBase->FirstScreen ? (int)IntuitionBase->FirstScreen->Width : -1,
                 IntuitionBase->FirstScreen ? (int)IntuitionBase->FirstScreen->Height : -1);
        /* Dump raw bytes at screen structure offsets 8-16 to verify layout */
        UBYTE *p = (UBYTE *)wbscreen;
        LPRINTF (LOG_INFO, "_intuition: Screen raw bytes at offset 8-15: %02x %02x %02x %02x %02x %02x %02x %02x\n",
                 p[8], p[9], p[10], p[11], p[12], p[13], p[14], p[15]);
        /* Also dump IntuitionBase offsets 56-64 to check FirstScreen pointer location */
        UBYTE *ib = (UBYTE *)IntuitionBase;
        LPRINTF (LOG_INFO, "_intuition: IntuitionBase raw at offset 56-63: %02x %02x %02x %02x %02x %02x %02x %02x\n",
                 ib[56], ib[57], ib[58], ib[59], ib[60], ib[61], ib[62], ib[63]);
        
        /* Print IntuitionBase address (what a6 should be after this call) */
        LPRINTF (LOG_INFO, "_intuition: OpenWorkBench() returning, IntuitionBase=0x%08lx\n", (ULONG)IntuitionBase);
        
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
            
            /* Move to position and render text */
            Move(rp, x, y + 6);  /* +6 for baseline adjustment with 8-pixel font */
            
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
    requester->ReqLayer = window->WLayer;
    
    /* Render */
    _render_requester(window, requester);
    
    return TRUE;
}

VOID _intuition_ScreenToBack ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Screen * screen __asm("a0"))
{
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
    struct Screen *curr, *prev = NULL;

    DPRINTF (LOG_DEBUG, "_intuition: ScreenToFront() screen=0x%08lx\n", (ULONG)screen);
    
    if (!screen || !IntuitionBase->FirstScreen) return;
    
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

    /* Store the menu pointer for the window
     * NOTE: Actual menu rendering/interaction not yet implemented
     */
    window->MenuStrip = menu;

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
     * For now we just log the request.
     */
    DPRINTF (LOG_DEBUG, "_intuition: SetWindowTitles() window=0x%08lx\n", (ULONG)window);

    if (!window)
        return;

    /* Update window title if requested */
    if (windowTitle != (CONST_STRPTR)-1) {
        window->Title = (UBYTE *)windowTitle;
        /* TODO: actually redraw the title bar */
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
    BOOL rootless_mode;

    DPRINTF(LOG_DEBUG, "_intuition: SizeWindow() window=0x%08lx dx=%d dy=%d\n", (ULONG)window, dx, dy);

    if (!window) return;

    old_width = window->Width;
    old_height = window->Height;

    new_w = window->Width + dx;
    new_h = window->Height + dy;

    /* Enforce limits */
    if (new_w < window->MinWidth) new_w = window->MinWidth;
    if (new_w > window->MaxWidth) new_w = window->MaxWidth;
    if (new_h < window->MinHeight) new_h = window->MinHeight;
    if (new_h > window->MaxHeight) new_h = window->MaxHeight;

    /* Recalculate deltas in case limits were hit */
    dx = new_w - window->Width;
    dy = new_h - window->Height;

    if (dx == 0 && dy == 0) return;

    /* Update Window structure */
    window->Width = new_w;
    window->Height = new_h;

    if (window->Flags & WFLG_GIMMEZEROZERO)
    {
        window->GZZWidth = window->Width - window->BorderLeft - window->BorderRight;
        window->GZZHeight = window->Height - window->BorderTop - window->BorderBottom;
    }

    /* Update Layer if present */
    if (window->WLayer && LayersBase)
    {
        _call_SizeLayer(LayersBase, window->WLayer, dx, dy);
    }

    /* Check for rootless mode */
    rootless_mode = emucall0(EMU_CALL_INT_GET_ROOTLESS);
    
    if (rootless_mode)
    {
        ULONG window_handle = _intuition_get_host_window_handle((struct LXAIntuitionBase *)IntuitionBase, window);

        if (window_handle)
        {
            /* Update host window - emucall expects absolute dimensions */
            emucall3(EMU_CALL_INT_SIZE_WINDOW, window_handle, (ULONG)new_w, (ULONG)new_h);
        }
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

    _post_idcmp_message(window, IDCMP_CHANGEWINDOW, CWCODE_DEPTH, 0,
                        window, window->MouseX, window->MouseY);
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

    _post_idcmp_message(window, IDCMP_CHANGEWINDOW, CWCODE_DEPTH, 0,
                        window, window->MouseX, window->MouseY);
}

BOOL _intuition_WindowLimits ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Window * window __asm("a0"),
                                                        register LONG widthMin __asm("d0"),
                                                        register LONG heightMin __asm("d1"),
                                                        register ULONG widthMax __asm("d2"),
                                                        register ULONG heightMax __asm("d3"))
{
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
        return FALSE;

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

    if (posText && posText->IText)
        strcpy(gad_buf, (char *)posText->IText);
    gad_buf[pos_len] = '|';
    if (negText && negText->IText)
        strcpy(gad_buf + pos_len + 1, (char *)negText->IText);
    else
        strcpy(gad_buf + pos_len + 1, "Cancel");

    easy.es_StructSize = sizeof(easy);
    easy.es_Flags = 0;
    easy.es_Title = (UBYTE *)"System Request";
    easy.es_TextFormat = (UBYTE *)body_buf;
    easy.es_GadgetFormat = (UBYTE *)gad_buf;

    (void)width;
    (void)height;
    reqWindow = _intuition_BuildEasyRequestArgs(IntuitionBase, window, &easy, flags, NULL);

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
            window->FirstGadget = NULL;

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
    DPRINTF (LOG_DEBUG, "_intuition: RethinkDisplay() called\n");
    
    /*
     * RethinkDisplay() is the Intuition-compatible way to merge
     * all screen Copper lists and load the View. In lxa this is
     * equivalent to waiting for VBlank which refreshes the SDL
     * display with the current bitmap contents.
     *
     * Per RKRM: "This call also does a WaitTOF()"
     */
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
    assert(FALSE);
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
     * Since we're single-threaded, we just return a dummy lock value.
     */
    DPRINTF (LOG_DEBUG, "_intuition: LockIBase() dontknow=0x%08lx\n", dontknow);
    return 1;  /* Return non-zero "lock" value */
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
            /* Use FirstScreen if available, otherwise let app open its own */
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
    
    rp = window->RPort;
    
    _calculate_gadget_box(window, req, gad, &left, &top, &width, &height);
    
    DPRINTF(LOG_DEBUG, "_intuition: _complement_gadget_area() gad=0x%08lx at (%ld,%ld) %ldx%ld flags=0x%04x\n",
            (ULONG)gad, left, top, width, height, (unsigned)gad->Flags);
    
    /* Complement the gadget select box area using XOR draw mode */
    SetDrMd(rp, COMPLEMENT);
    SetAPen(rp, 0xFF);  /* All planes */
    RectFill(rp, left, top, left + width - 1, top + height - 1);
    SetDrMd(rp, JAM2);  /* Restore normal draw mode */
}

/* Helper to render a single gadget */
static void _render_gadget(struct Window *window, struct Requester *req, struct Gadget *gad)
{
    struct RastPort *rp;
    LONG left, top, width, height;
    
    if (!window || !gad || !window->RPort) return;
    
    DPRINTF(LOG_DEBUG, "_intuition: _render_gadget() gad=0x%08lx type=0x%04x flags=0x%04x GadgetRender=0x%08lx\n",
            (ULONG)gad, (unsigned)gad->GadgetType, (unsigned)gad->Flags, (ULONG)gad->GadgetRender);
    
    rp = window->RPort;
    
    _calculate_gadget_box(window, req, gad, &left, &top, &width, &height);
    
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
    if (gad->Flags & GFLG_GADGIMAGE)
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
    
    /* Draw Text — walk the IntuiText chain (NextText) so that
     * GT_Underscore underline IntuiTexts are rendered too. */
    if (gad->GadgetText)
    {
        struct IntuiText *it = gad->GadgetText;
        while (it)
        {
            if (it->IText)
            {
                LONG tx = left + it->LeftEdge;
                LONG ty = top + it->TopEdge;
                SetAPen(rp, it->FrontPen);
                SetBPen(rp, it->BackPen);
                SetDrMd(rp, it->DrawMode);
                if (it->IText[0] == '_' && it->IText[1] == '\0')
                {
                    Move(rp, tx, ty + 1);
                    Draw(rp, tx + 7, ty + 1);
                }
                else
                {
                    Move(rp, tx, ty);
                    Text(rp, (STRPTR)it->IText, strlen((char *)it->IText));
                }
            }
            it = it->NextText;
        }
    }
    
    /* Render proportional gadget knob (SLIDER_KIND and other prop gadgets) */
    if ((gad->GadgetType & GTYP_GTYPEMASK) == GTYP_PROPGADGET)
    {
        struct PropInfo *pi = (struct PropInfo *)gad->SpecialInfo;
        if (pi && (pi->Flags & AUTOKNOB))
        {
            /* Clear the interior of the prop container */
            WORD containerL = left + 1;
            WORD containerT = top + 1;
            WORD containerW = width - 2;
            WORD containerH = height - 2;
            WORD knobW, knobH, knobX, knobY;

            SetAPen(rp, 0);  /* Background pen */
            RectFill(rp, containerL, containerT,
                     containerL + containerW - 1, containerT + containerH - 1);

            /* Calculate knob size from Body */
            if (pi->Flags & FREEHORIZ)
                knobW = (WORD)(((ULONG)containerW * (ULONG)pi->HorizBody) / 0xFFFF);
            else
                knobW = containerW;

            if (pi->Flags & FREEVERT)
                knobH = (WORD)(((ULONG)containerH * (ULONG)pi->VertBody) / 0xFFFF);
            else
                knobH = containerH;

            /* Minimum knob size */
            if (knobW < 6) knobW = 6;
            if (knobH < 4) knobH = 4;
            if (knobW > containerW) knobW = containerW;
            if (knobH > containerH) knobH = containerH;

            /* Calculate knob position from Pot */
            {
                WORD maxMoveX = containerW - knobW;
                WORD maxMoveY = containerH - knobH;
                if (maxMoveX < 0) maxMoveX = 0;
                if (maxMoveY < 0) maxMoveY = 0;

                knobX = containerL + (WORD)(((ULONG)maxMoveX * (ULONG)pi->HorizPot) / 0xFFFF);
                knobY = containerT + (WORD)(((ULONG)maxMoveY * (ULONG)pi->VertPot) / 0xFFFF);
            }

            /* Draw knob as raised bevel box */
            /* Knob interior */
            SetAPen(rp, 0);  /* Grey background for knob interior */
            RectFill(rp, knobX, knobY, knobX + knobW - 1, knobY + knobH - 1);

            /* Knob top/left edge = shine (pen 2) */
            SetAPen(rp, 2);
            Move(rp, knobX, knobY + knobH - 2);
            Draw(rp, knobX, knobY);
            Draw(rp, knobX + knobW - 2, knobY);

            /* Knob bottom/right edge = shadow (pen 1) */
            SetAPen(rp, 1);
            Move(rp, knobX + knobW - 1, knobY);
            Draw(rp, knobX + knobW - 1, knobY + knobH - 1);
            Draw(rp, knobX, knobY + knobH - 1);

            DPRINTF(LOG_DEBUG, "_render_gadget: prop knob at (%d,%d) size %dx%d pot=%u body=%u\n",
                    knobX, knobY, knobW, knobH, pi->HorizPot, pi->HorizBody);
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

    if (_gadtools_IsCycle(gad))
    {
        WORD inner_top = top + 2;
        WORD inner_right = left + width - 3;
        WORD inner_bottom = top + height - 3;
        WORD arrow_left = inner_right - 11;
        WORD arrow_center_x = inner_right - 6;
        WORD arrow_center_y = top + (height / 2);

        if (inner_top <= inner_bottom)
        {
            if (arrow_left < left + width - 2)
            {
                SetAPen(rp, 0);
                RectFill(rp, arrow_left + 1, inner_top, inner_right, inner_bottom);

                SetAPen(rp, 1);
                Move(rp, arrow_left, inner_top);
                Draw(rp, arrow_left, inner_bottom);

                Move(rp, arrow_center_x - 3, arrow_center_y - 1);
                Draw(rp, arrow_center_x, arrow_center_y + 2);
                Draw(rp, arrow_center_x + 3, arrow_center_y - 1);
            }
        }
    }

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
            LONG len = (LONG)si->NumChars;
            WORD textY;
            WORD textX;
            
            DPRINTF(LOG_DEBUG, "_render_gadget: strgad clear interior left=%ld top=%ld w=%ld h=%ld\n",
                    left, top, width, height);
            
            /* Clear gadget interior with background pen */
            SetAPen(rp, 0);
            RectFill(rp, left, top, left + width - 1, top + height - 1);
            
            DPRINTF(LOG_DEBUG, "_render_gadget: strgad RectFill done\n");
            
            /* Calculate text Y position (vertically centered).
             * Text() uses baseline: y - tf_Baseline for actual rendering.
             * Default topaz font: tf_Baseline = 6, tf_YSize = 8.
             * For vertical centering: textY = top + (height / 2) + 3
             * gives good baseline alignment.
             */
            textY = top + (height / 2) + 3;
            textX = left + 2;  /* Small left margin */
            
            /* Handle GACT_STRINGCENTER: center text horizontally */
            if (gad->Activation & GACT_STRINGCENTER)
            {
                WORD textWidth = len * 8;  /* topaz 8: 8px per character */
                textX = left + (width - textWidth) / 2;
                if (textX < left + 2)
                    textX = left + 2;
            }
            
            /* Handle GACT_STRINGRIGHT: right-align text */
            if (gad->Activation & GACT_STRINGRIGHT)
            {
                WORD textWidth = len * 8;
                textX = left + width - textWidth - 2;
                if (textX < left + 2)
                    textX = left + 2;
            }
            
            /* Draw buffer text */
            SetAPen(rp, 1);  /* Text pen */
            SetBPen(rp, 0);  /* Background pen */
            SetDrMd(rp, JAM2);
            
            DPRINTF(LOG_DEBUG, "_render_gadget: strgad Text textX=%d textY=%d len=%ld\n",
                    textX, textY, len);
            
            Move(rp, textX, textY);
            if (len > 0)
            {
                Text(rp, si->Buffer, len);
            }
            
            DPRINTF(LOG_DEBUG, "_render_gadget: strgad Text done\n");
            
            /* Draw cursor if gadget is active (selected) */
            if (gad->Flags & GFLG_SELECTED)
            {
                WORD cursorX;
                WORD cursorPos = si->BufferPos;
                
                if (cursorPos > 0)
                {
                    DPRINTF(LOG_DEBUG, "_render_gadget: strgad TextLength pos=%d\n", cursorPos);
                    cursorX = textX + TextLength(rp, si->Buffer, cursorPos);
                    DPRINTF(LOG_DEBUG, "_render_gadget: strgad TextLength done cursorX=%d\n", cursorX);
                }
                else
                {
                    cursorX = textX;
                }
                
                /* Draw cursor as vertical bar in COMPLEMENT mode */
                SetAPen(rp, 1);
                SetDrMd(rp, COMPLEMENT);
                RectFill(rp, cursorX, top + 1, cursorX + 1, top + height - 2);
                SetDrMd(rp, JAM2);
                DPRINTF(LOG_DEBUG, "_render_gadget: strgad cursor done\n");
            }
            DPRINTF(LOG_DEBUG, "_render_gadget: strgad rendering complete\n");
        }
    }
    
}

VOID _intuition_RefreshGList ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Gadget * gadgets __asm("a0"),
                                                        register struct Window * window __asm("a1"),
                                                        register struct Requester * requester __asm("a2"),
                                                        register WORD numGad __asm("d0"))
{
    struct Gadget *gad = gadgets;
    WORD count = 0;
    
    DPRINTF (LOG_DEBUG, "_intuition: RefreshGList() gadgets=0x%08lx win=0x%08lx req=0x%08lx num=%d\n",
             (ULONG)gadgets, (ULONG)window, (ULONG)requester, numGad);
             
    if (!window || !gadgets) return;
    
    while (gad && (numGad == -1 || count < numGad))
    {
        /* Don't render if disabled (unless we want to render disabled state - which we should) 
         * But if GFLG_DISABLED is toggled, we probably render ghosted.
         * For now, just render.
         */
         
        /* Only render if it belongs to the window/requester context?
         * Usually caller ensures valid list.
         */
         
        _render_gadget(window, requester, gad);
        
        gad = gad->NextGadget;
        count++;
    }
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
            init = init->NextGadget;
            if (init_remaining > 0)
                init_remaining--;
        }
    }

    if (!requester || requester->ReqLayer)
    {
        _intuition_RefreshGList(IntuitionBase, gadget, window, requester, numGad);
    }

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

    /* Nothing to do if already active */
    if (prevActive == window)
        return;

    /* Deactivate the previously active window */
    if (prevActive)
    {
        prevActive->Flags &= ~WFLG_WINDOWACTIVE;
        _post_idcmp_message(prevActive, IDCMP_INACTIVEWINDOW, 0, 0,
                            prevActive, 0, 0);
        /* Re-render frame to show inactive title bar colors */
        _render_window_frame(prevActive);
    }

    /* Activate the new window */
    IntuitionBase->ActiveWindow = window;
    window->Flags |= WFLG_WINDOWACTIVE;
    _post_idcmp_message(window, IDCMP_ACTIVEWINDOW, 0, 0,
                        window, 0, 0);
    /* Re-render frame to show active title bar colors */
    _render_window_frame(window);
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
    if (gadget_type != GTYP_STRGADGET && gadget_type != GTYP_CUSTOMGADGET)
        return FALSE;

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
    DPRINTF (LOG_DEBUG, "_intuition: QueryOverscan() displayID=0x%08lx oScanType=%d\n", 
             displayID, oScanType);
    
    if (!rect)
        return FALSE;
    
    /* Return standard PAL hires dimensions for all modes
     * oScanType: 1=TEXT (visible), 2=STANDARD (past edges), 3=MAX, 4=VIDEO
     */
    switch (oScanType) {
        case 1:  /* OSCAN_TEXT - entirely visible */
            rect->MinX = 0;
            rect->MinY = 0;
            rect->MaxX = 639;
            rect->MaxY = 255;
            break;
        case 2:  /* OSCAN_STANDARD - just past edges */
            rect->MinX = 0;
            rect->MinY = 0;
            rect->MaxX = 703;
            rect->MaxY = 283;
            break;
        case 3:  /* OSCAN_MAX - as much as possible */
        case 4:  /* OSCAN_VIDEO - even more */
            rect->MinX = 0;
            rect->MinY = 0;
            rect->MaxX = 719;
            rect->MaxY = 283;
            break;
        default:
            rect->MinX = 0;
            rect->MinY = 0;
            rect->MaxX = 639;
            rect->MaxY = 255;
            break;
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

    _post_idcmp_message(window, IDCMP_CHANGEWINDOW, CWCODE_DEPTH, 0,
                        window, window->MouseX, window->MouseY);
}

VOID _intuition_ChangeWindowBox ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Window * window __asm("a0"),
                                                        register LONG left __asm("d0"),
                                                        register LONG top __asm("d1"),
                                                        register LONG width __asm("d2"),
                                                        register LONG height __asm("d3"))
{
    DPRINTF(LOG_DEBUG, "_intuition: ChangeWindowBox() window=0x%08lx pos=%ld,%ld size=%ldx%ld\n", 
            (ULONG)window, left, top, width, height);

    if (!window) return;

    _intuition_MoveWindow(IntuitionBase, window, left - window->LeftEdge, top - window->TopEdge);
    _intuition_SizeWindow(IntuitionBase, window, width - window->Width, height - window->Height);
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
         * No WA_Zoom data — use simple heuristic:
         * If currently at or below min size, expand to max.
         * Otherwise, shrink to min.
         */
        WORD target_w, target_h;
        WORD screen_w = 640, screen_h = 256;

        if (window->WScreen)
        {
            screen_w = window->WScreen->Width;
            screen_h = window->WScreen->Height;
        }

        if (window->Width <= window->MinWidth && window->Height <= window->MinHeight)
        {
            target_w = (window->MaxWidth != (UWORD)-1) ? window->MaxWidth : screen_w;
            target_h = (window->MaxHeight != (UWORD)-1) ? window->MaxHeight : screen_h;
            if (target_w > screen_w) target_w = screen_w;
            if (target_h > screen_h) target_h = screen_h;
        }
        else
        {
            target_w = window->MinWidth;
            target_h = window->MinHeight;
        }

        _intuition_ChangeWindowBox(IntuitionBase, window,
                                    window->LeftEdge, window->TopEdge,
                                    target_w, target_h);
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
    struct LXAIntuitionBase *base = (struct LXAIntuitionBase *)IntuitionBase;

    DPRINTF (LOG_DEBUG, "_intuition: LockPubScreenList() -> 0x%08lx\n", (ULONG)&base->PubScreenList);
    return &base->PubScreenList;
}

VOID _intuition_UnlockPubScreenList ( register struct IntuitionBase * IntuitionBase __asm("a6"))
{
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

    pub = _intuition_find_pubscreen_by_screen(base, screen);
    if (!pub)
        return 0;

    oldFlags = pub->psn_Flags;

    if ((statusFlags & PSNF_PRIVATE) && pub->psn_VisitorCount > 0)
        return 0;

    pub->psn_Flags = statusFlags & PSNF_PRIVATE;

    if ((pub->psn_Flags & PSNF_PRIVATE) && base->DefaultPubScreen == screen)
        base->DefaultPubScreen = NULL;
    else if (!(pub->psn_Flags & PSNF_PRIVATE) && !base->DefaultPubScreen)
        base->DefaultPubScreen = screen;

    return oldFlags;
}

struct RastPort	* _intuition_ObtainGIRPort ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct GadgetInfo * gInfo __asm("a0"))
{
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

VOID _intuition_private1 ( register struct IntuitionBase * IntuitionBase __asm("a6"))
{
    PRIVATE_FUNCTION_ERROR("_intuition", "private1");
    assert(FALSE);
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

    /* ---- Step 4: Calculate layout ---- */
    /* Font is fixed 8x8 topaz */
    WORD char_w = 8;
    WORD char_h = 8;
    WORD font_baseline = 6;

    /* Measure body text: split on \n, find max width and line count */
    WORD body_lines = 1;
    WORD max_body_width = 0;
    {
        WORD cur_width = 0;
        char *p = body_buf;
        while (*p)
        {
            if (*p == '\n')
            {
                if (cur_width > max_body_width) max_body_width = cur_width;
                cur_width = 0;
                body_lines++;
            }
            else
            {
                cur_width++;
            }
            p++;
        }
        if (cur_width > max_body_width) max_body_width = cur_width;
    }
    WORD body_pixel_w = max_body_width * char_w;
    WORD body_pixel_h = body_lines * (char_h + 2); /* 2px line spacing */

    /* Measure gadget labels and calculate button sizes */
    WORD gad_padding_x = 16;  /* horizontal padding inside button */
    WORD gad_padding_y = 4;   /* vertical padding inside button */
    WORD gad_spacing = 8;     /* spacing between buttons */
    WORD gad_height = char_h + gad_padding_y * 2; /* button height */
    WORD total_gad_width = 0;
    WORD gad_widths[10];

    for (i = 0; i < num_gadgets && i < 10; i++)
    {
        WORD label_len = 0;
        char *lp = gad_labels[i];
        while (*lp) { label_len++; lp++; }
        gad_widths[i] = label_len * char_w + gad_padding_x;
        if (gad_widths[i] < 60) gad_widths[i] = 60; /* minimum button width */
        total_gad_width += gad_widths[i];
    }
    total_gad_width += (num_gadgets - 1) * gad_spacing;

    /* Window dimensions */
    WORD border_left = 4;
    WORD border_right = 4;
    WORD border_top = 11; /* title bar */
    WORD border_bottom = 2;
    WORD text_margin = 16;  /* margin around text area */
    WORD gad_margin = 12;   /* margin around gadget row */

    WORD content_w = body_pixel_w + text_margin * 2;
    if (total_gad_width + gad_margin * 2 > content_w)
        content_w = total_gad_width + gad_margin * 2;

    WORD win_w = content_w + border_left + border_right;
    WORD win_h = border_top + text_margin + body_pixel_h + text_margin
                 + gad_height + gad_margin + border_bottom;

    /* Minimum window size */
    if (win_w < 150) win_w = 150;
    if (win_h < 60) win_h = 60;

    /* ---- Step 5: Determine screen and center window ---- */
    struct Screen *scr = NULL;
    if (window && window->WScreen)
        scr = window->WScreen;
    /* If no screen, we'll use the Workbench screen (default) */

    WORD scr_w = scr ? scr->Width : 640;
    WORD scr_h = scr ? scr->Height : 256;
    WORD win_x = (scr_w - win_w) / 2;
    WORD win_y = (scr_h - win_h) / 2;
    if (win_x < 0) win_x = 0;
    if (win_y < 0) win_y = 0;

    /* ---- Step 6: Resolve title ---- */
    const char *title = (const char *)easyStruct->es_Title;
    if (!title)
    {
        if (window && window->Title)
            title = (const char *)window->Title;
        else
            title = "System Request";
    }

    /* ---- Step 7: Allocate gadgets, borders, and IntuiText ---- */
    /* Each gadget needs: struct Gadget + 2 Borders (raised bevel = top+bottom lines) + IntuiText for label */
    LONG gadget_alloc = num_gadgets * sizeof(struct Gadget);
    LONG border_alloc = num_gadgets * 2 * sizeof(struct Border); /* 2 borders per gadget (shine + shadow) */
    LONG itext_alloc = num_gadgets * sizeof(struct IntuiText);
    /* Border XY data: each border needs 10 SHORTs (5 points x 2 coords) */
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

    /* ---- Step 8: Set up gadgets ---- */
    /* Gadget IDs: rightmost (last) = 0, others = 1,2,3... from left */
    /* Center gadgets in the gadget row */
    WORD gad_row_y = border_top + text_margin + body_pixel_h + text_margin;
    WORD gad_start_x = border_left + (content_w - total_gad_width) / 2;
    WORD gad_x = gad_start_x;

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

        /* Gadget ID: last gadget = 0, others count from 1 left-to-right */
        if (i == num_gadgets - 1)
            g->GadgetID = 0;
        else
            g->GadgetID = i + 1;

        g->LeftEdge = gad_x;
        g->TopEdge = gad_row_y;
        g->Width = bw;
        g->Height = bh;
        g->Flags = GFLG_GADGHCOMP;   /* complement highlight */
        g->Activation = GACT_RELVERIFY;
        g->GadgetType = GTYP_BOOLGADGET;
        g->GadgetRender = (APTR)b_shine;
        g->GadgetText = it;
        g->NextGadget = (i < num_gadgets - 1) ? &gadgets[i + 1] : NULL;

        /* Shine border (top-left): pen 2 */
        /* Points: bottom-left -> top-left -> top-right */
        xy_shine[0] = 0;        xy_shine[1] = bh - 1;
        xy_shine[2] = 0;        xy_shine[3] = 0;
        xy_shine[4] = bw - 1;   xy_shine[5] = 0;
        xy_shine[6] = bw - 1;   xy_shine[7] = 1;
        xy_shine[8] = 1;        xy_shine[9] = bh - 2;

        b_shine->LeftEdge = 0;
        b_shine->TopEdge = 0;
        b_shine->FrontPen = 2; /* SHINEPEN */
        b_shine->DrawMode = JAM1;
        b_shine->Count = 5;
        b_shine->XY = xy_shine;
        b_shine->NextBorder = b_shadow;

        /* Shadow border (bottom-right): pen 1 */
        /* Points: top-right -> bottom-right -> bottom-left */
        xy_shadow[0] = bw - 1;  xy_shadow[1] = 0;
        xy_shadow[2] = bw - 1;  xy_shadow[3] = bh - 1;
        xy_shadow[4] = 0;       xy_shadow[5] = bh - 1;
        xy_shadow[6] = 0;       xy_shadow[7] = bh - 2;
        xy_shadow[8] = bw - 2;  xy_shadow[9] = 1;

        b_shadow->LeftEdge = 0;
        b_shadow->TopEdge = 0;
        b_shadow->FrontPen = 1; /* SHADOWPEN */
        b_shadow->DrawMode = JAM1;
        b_shadow->Count = 5;
        b_shadow->XY = xy_shadow;
        b_shadow->NextBorder = NULL;

        /* IntuiText for label: centered in gadget */
        WORD label_len = 0;
        {
            char *lp = gad_labels[i];
            while (*lp) { label_len++; lp++; }
        }
        WORD text_x = (bw - label_len * char_w) / 2;
        WORD text_y = (bh - char_h) / 2;

        it->FrontPen = 1;
        it->BackPen = 0;
        it->DrawMode = JAM1;
        it->LeftEdge = text_x;
        it->TopEdge = text_y;
        it->ITextFont = NULL; /* use default screen font */
        it->IText = (UBYTE *)gad_labels[i];
        it->NextText = NULL;

        gad_x += bw + gad_spacing;
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
    nw.Flags = WFLG_DRAGBAR | WFLG_DEPTHGADGET | WFLG_ACTIVATE | WFLG_RMBTRAP;
    nw.IDCMPFlags = IDCMP_GADGETUP | idcmp;
    nw.FirstGadget = gadgets;

    if (scr)
    {
        nw.Type = CUSTOMSCREEN;
        nw.Screen = scr;
    }
    else
    {
        nw.Type = WBENCHSCREEN;
    }

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

    /* ---- Step 11: Render body text ---- */
    {
        struct RastPort *rp = reqWindow->RPort;
        WORD text_x = border_left + text_margin;
        WORD text_y = border_top + text_margin;
        char *p = body_buf;
        WORD line = 0;

        SetAPen(rp, 1); /* TEXTPEN */
        SetBPen(rp, 0); /* BACKGROUNDPEN */
        SetDrMd(rp, JAM1);

        while (*p)
        {
            /* Find end of line */
            char *line_start = p;
            WORD line_len = 0;
            while (*p && *p != '\n')
            {
                line_len++;
                p++;
            }

            if (line_len > 0)
            {
                WORD y = text_y + line * (char_h + 2) + font_baseline;
                Move(rp, text_x, y);
                Text(rp, (STRPTR)line_start, line_len);
            }

            if (*p == '\n') p++;
            line++;
        }
    }

    /* ---- Step 12: Render gadgets (borders + text) ---- */
    _intuition_RefreshGList(IntuitionBase, gadgets, reqWindow, NULL, -1);

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
            
            if (idcmpFlags) *idcmpFlags = class;
            
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

            return -1;
        }
        
        if (!waitInput) break;
    }
    
    return result;
}

struct Window * _intuition_OpenWindowTagList ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register const struct NewWindow * newWindow __asm("a0"),
                                                        register const struct TagItem * tagList __asm("a1"))
{
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
                    break;
                case WA_MouseQueue:
                    mouse_queue = (UWORD)tag->ti_Data;
                    break;
                /* Tags we recognize but don't fully implement yet */
                case WA_PubScreenName:
                case WA_PubScreen:
                case WA_PubScreenFallBack:
                case WA_BackFill:
                case WA_RptQueue:
                case WA_AutoAdjust:
                case WA_ScreenTitle:
                case WA_Checkmark:
                case WA_SuperBitMap:
                case WA_MenuHelp:
                case WA_NewLookMenus:
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
            if (nw.Flags & WFLG_BORDERLESS)
            {
                border_left = 0;
                border_right = 0;
                border_top = 0;
                border_bottom = 0;
            }
            else
            {
                border_left = screen->WBorLeft;
                border_right = screen->WBorRight;
                border_bottom = screen->WBorBottom;

                if ((nw.Flags & (WFLG_DRAGBAR | WFLG_CLOSEGADGET | WFLG_DEPTHGADGET)) || nw.Title)
                    border_top = screen->WBorTop;
                else
                    border_top = screen->WBorBottom;

                if (nw.Flags & WFLG_SIZEGADGET)
                {
                    BOOL sizeBBottom = (nw.Flags & WFLG_SIZEBBOTTOM) ||
                                       !(nw.Flags & WFLG_SIZEBRIGHT);
                    BOOL sizeBRight = (nw.Flags & WFLG_SIZEBRIGHT) ||
                                      !(nw.Flags & WFLG_SIZEBBOTTOM);

                    if (sizeBBottom && border_bottom < SYS_GADGET_HEIGHT)
                        border_bottom = SYS_GADGET_HEIGHT;
                    if (sizeBRight && border_right < SYS_GADGET_WIDTH)
                        border_right = SYS_GADGET_WIDTH;
                }
            }

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

    /* Call our existing OpenWindow with the assembled NewWindow */
    struct Window *win = _intuition_OpenWindow(IntuitionBase, &nw);

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
    struct NewScreen ns;
    struct TagItem *tstate;
    struct TagItem *tag;
    
    DPRINTF(LOG_DEBUG, "_intuition: OpenScreenTagList() called, newScreen=0x%08lx, tagList=0x%08lx\n",
            (ULONG)newScreen, (ULONG)tagList);
    
    /* Start with defaults from NewScreen if provided, else use sensible defaults */
    if (newScreen)
    {
        /* Copy the NewScreen structure */
        ns = *newScreen;
    }
    else
    {
        /* Initialize with defaults */
        memset(&ns, 0, sizeof(ns));
        ns.Width = 640;
        ns.Height = 256;
        ns.Depth = 2;
        ns.Type = CUSTOMSCREEN;
        ns.DetailPen = 0;
        ns.BlockPen = 1;
    }
    
    /* Process tags to override NewScreen fields */
    if (tagList)
    {
        tstate = (struct TagItem *)tagList;
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
                /* Tags we recognize but don't fully implement yet */
                case SA_DisplayID:
                case SA_DClip:
                case SA_Overscan:
                case SA_Colors:
                case SA_Colors32:
                case SA_Pens:
                case SA_PubName:
                case SA_PubSig:
                case SA_PubTask:
                case SA_SysFont:
                case SA_ErrorCode:
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
    
    /* Call our existing OpenScreen with the assembled NewScreen */
    return _intuition_OpenScreen(IntuitionBase, &ns);
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
                                  (state == IDS_SELECTED) ? 0x30 : 0xC0);
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
        /* Standard Image - Erase bounding box */
        /* Use background pen 0 */
        SetAPen(rp, 0);
        RectFill(rp, leftOffset + image->LeftEdge, 
                     topOffset + image->TopEdge,
                     leftOffset + image->LeftEdge + image->Width - 1,
                     topOffset + image->TopEdge + image->Height - 1);
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

static ULONG _intuition_dispatch_method(struct IClass *cl, Object *obj, Msg msg)
{
    if (!cl || !cl->cl_Dispatcher.h_Entry)
        return 0;

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
    ULONG size;
    UBYTE *object_memory;
    Object *public_obj;
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
        size = SIZEOF_INSTANCE(use_class);
        if (size < sizeof(struct _Object))
            size = sizeof(struct _Object);

        object_memory = AllocMem(size, MEMF_PUBLIC | MEMF_CLEAR);
        if (!object_memory)
            return NULL;

        public_obj = (Object *)(object_memory + sizeof(struct _Object));
        _OBJECT(public_obj)->o_Class = use_class;
        use_class->cl_ObjectCount++;

        op.MethodID = OM_NEW;
        op.ops_AttrList = (struct TagItem *)tagList;
        op.ops_GInfo = NULL;
        _intuition_dispatch_method(use_class, public_obj, (Msg)&op);

        return (APTR)public_obj;
    }

    /* Handle sysiclass - system imagery class */
    if (classID && strcmp((const char*)classID, SYSICLASS) == 0) {
        /* Create a minimal Image structure for system imagery */
        struct Image *img = AllocMem(sizeof(struct Image), MEMF_CLEAR | MEMF_PUBLIC);
        if (!img)
            return NULL;
        
        /* Initialize with minimal data - a 1x1 transparent image */
        img->LeftEdge = 0;
        img->TopEdge = 0;
        img->Width = 8;     /* Minimal size */
        img->Height = 8;
        img->Depth = 1;
        img->ImageData = NULL;  /* No actual image data - will render as empty */
        img->PlanePick = 0;     /* Don't pick any planes - essentially invisible */
        img->PlaneOnOff = 0;
        img->NextImage = NULL;
        
        DPRINTF (LOG_DEBUG, "_intuition: NewObjectA() sysiclass -> Image at 0x%08lx\n", (ULONG)img);
        return (APTR)img;
    }
    
    /* Handle imageclass - generic image class */
    if (classID && strcmp((const char*)classID, IMAGECLASS) == 0) {
        struct Image *img = AllocMem(sizeof(struct Image), MEMF_CLEAR | MEMF_PUBLIC);
        if (!img)
            return NULL;
        
        img->LeftEdge = 0;
        img->TopEdge = 0;
        img->Width = 1;
        img->Height = 1;
        img->Depth = 1;
        img->ImageData = NULL;
        img->PlanePick = 0;
        img->PlaneOnOff = 0;
        img->NextImage = NULL;
        
        DPRINTF (LOG_DEBUG, "_intuition: NewObjectA() imageclass -> Image at 0x%08lx\n", (ULONG)img);
        return (APTR)img;
    }
    
    /* Unknown class - return NULL */
    DPRINTF (LOG_DEBUG, "_intuition: NewObjectA() unknown class, returning NULL\n");
    return NULL;
}

VOID _intuition_DisposeObject ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register APTR object __asm("a0"))
{
    /*
     * DisposeObject() disposes of a BOOPSI object.
     * We free the memory allocated by NewObjectA for sysiclass/imageclass.
     */
    DPRINTF (LOG_DEBUG, "_intuition: DisposeObject() object=0x%08lx\n", (ULONG)object);
    
    if (!object)
        return;

    {
        struct _Object *obj_data = _OBJECT(object);
        struct IClass *cl = obj_data->o_Class;

        if (cl) {
            ULONG size = SIZEOF_INSTANCE(cl);
            struct { ULONG MethodID; } dispose_msg;
            if (size < sizeof(struct _Object))
                size = sizeof(struct _Object);
            dispose_msg.MethodID = OM_DISPOSE;
            _intuition_dispatch_method(cl, (Object *)object, (Msg)&dispose_msg);
            if (cl->cl_ObjectCount > 0)
                cl->cl_ObjectCount--;
            FreeMem(obj_data, size);
            return;
        }
    }

    /* Assume it's an Image structure from our sysiclass/imageclass stub */
    FreeMem(object, sizeof(struct Image));
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
        gi.gi_Domain.Left = window->BorderLeft;
        gi.gi_Domain.Top = window->BorderTop;
        gi.gi_Domain.Width = window->Width - window->BorderLeft - window->BorderRight;
        gi.gi_Domain.Height = window->Height - window->BorderTop - window->BorderBottom;
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
    
    /* Re-render the gadget if attrs changed and we have a window */
    if (result && window)
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
    assert(FALSE);
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
        superClass = _intuition_find_class(base, superClassID);

    if (!superClass)
        return NULL;

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
    struct DrawInfo *drawInfo;
    UWORD *pens;
    
    DPRINTF (LOG_DEBUG, "_intuition: GetScreenDrawInfo() screen=0x%08lx\n", (ULONG)screen);

    if (!screen)
        return NULL;

    /*
     * GetScreenDrawInfo returns information about the screen's drawing pens
     * and font. We allocate a DrawInfo structure and populate it with
     * the screen's attributes.
     */
    
    /* Allocate DrawInfo and pens array in RAM (not static - ROM is read-only!) */
    drawInfo = AllocMem(sizeof(struct DrawInfo), MEMF_CLEAR | MEMF_PUBLIC);
    if (!drawInfo)
        return NULL;
    
    pens = AllocMem((NUMDRIPENS + 2) * sizeof(UWORD), MEMF_PUBLIC);  /* +1 for terminator, +1 for safety */
    if (!pens) {
        FreeMem(drawInfo, sizeof(struct DrawInfo));
        return NULL;
    }
    
    /* Initialize default pens (0-12, plus terminator at 13) */
    pens[0] = 0;    /* DETAILPEN */
    pens[1] = 1;    /* BLOCKPEN */
    pens[2] = 1;    /* TEXTPEN */
    pens[3] = 2;    /* SHINEPEN */
    pens[4] = 1;    /* SHADOWPEN */
    pens[5] = 3;    /* FILLPEN */
    pens[6] = 1;    /* FILLTEXTPEN */
    pens[7] = 0;    /* BACKGROUNDPEN */
    pens[8] = 2;    /* HIGHLIGHTTEXTPEN */
    pens[9] = 1;    /* BARDETAILPEN - V39 */
    pens[10] = 2;   /* BARBLOCKPEN - V39 */
    pens[11] = 1;   /* BARTRIMPEN - V39 */
    pens[12] = 1;   /* BARCONTOURPEN - V39 */
    pens[13] = (UWORD)~0;  /* Terminator */
    
    /* Initialize DrawInfo */
    drawInfo->dri_Version = 2;           /* V39 compatible */
    drawInfo->dri_NumPens = NUMDRIPENS;
    drawInfo->dri_Pens = pens;
    drawInfo->dri_Font = screen->RastPort.Font;
    drawInfo->dri_Depth = 2;             /* Default 4 colors */
    drawInfo->dri_Resolution.X = 44;
    drawInfo->dri_Resolution.Y = 44;
    drawInfo->dri_Flags = DRIF_NEWLOOK;
    drawInfo->dri_CheckMark = NULL;
    drawInfo->dri_AmigaKey = NULL;
    
    /* Set depth from screen bitmap if available */
    if (screen->RastPort.BitMap) {
        drawInfo->dri_Depth = screen->RastPort.BitMap->Depth;
    }
    
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
    assert(FALSE);
}

VOID _intuition_private4 ( register struct IntuitionBase * IntuitionBase __asm("a6"))
{
    PRIVATE_FUNCTION_ERROR("_intuition", "private4");
    assert(FALSE);
}

VOID _intuition_private5 ( register struct IntuitionBase * IntuitionBase __asm("a6"))
{
    PRIVATE_FUNCTION_ERROR("_intuition", "private5");
    assert(FALSE);
}

VOID _intuition_private6 ( register struct IntuitionBase * IntuitionBase __asm("a6"))
{
    PRIVATE_FUNCTION_ERROR("_intuition", "private6");
    assert(FALSE);
}

VOID _intuition_private7 ( register struct IntuitionBase * IntuitionBase __asm("a6"))
{
    PRIVATE_FUNCTION_ERROR("_intuition", "private7");
    assert(FALSE);
}

VOID _intuition_private8 ( register struct IntuitionBase * IntuitionBase __asm("a6"))
{
    PRIVATE_FUNCTION_ERROR("_intuition", "private8");
    assert(FALSE);
}

VOID _intuition_private9 ( register struct IntuitionBase * IntuitionBase __asm("a6"))
{
    PRIVATE_FUNCTION_ERROR("_intuition", "private9");
    assert(FALSE);
}

VOID _intuition_private10 ( register struct IntuitionBase * IntuitionBase __asm("a6"))
{
    PRIVATE_FUNCTION_ERROR("_intuition", "private10");
    assert(FALSE);
}

struct ScreenBuffer * _intuition_AllocScreenBuffer ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Screen * sc __asm("a0"),
                                                        register struct BitMap * bm __asm("a1"),
                                                        register ULONG flags __asm("d0"))
{
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
    DPRINTF (LOG_DEBUG, "_intuition: ScreenPosition() screen=0x%08lx flags=0x%08lx pos=(%ld,%ld)\n", 
             (ULONG)screen, flags, x1, y1);
             
    if (!screen) return;
    
    if (flags & SPOS_ABSOLUTE)
    {
        /* x1, y1 are new coordinates */
        screen->LeftEdge = x1;
        screen->TopEdge = y1;
    }
    else /* SPOS_RELATIVE (default) */
    {
        /* x1, y1 are deltas */
        screen->LeftEdge += x1;
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

    if (!fromwindow)
        return;

    fromwindow->MenuStrip = towindow ? towindow->MenuStrip : NULL;
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
    
    return _intuition_dispatch_method(cl, (Object *)gad, message);
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
    return _intuition_DisplayAlert(IntuitionBase, alertNumber, string, height);
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
