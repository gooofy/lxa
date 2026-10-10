/*
 * lxa commodities.library implementation
 *
 * Commodities Exchange: brokers and object networks that see the input
 * stream through an input.device handler (IND_ADDHANDLER, priority 53).
 *
 * Every input event is routed through the brokers in priority order.  An
 * active broker diverts it down its object list; filters divert it down
 * theirs when it matches their input expression, senders post a copy to
 * a port, signal objects signal a task, translators swallow it and put
 * their own events into the stream in its place, custom objects call an
 * application function synchronously.  At the end of a list the message
 * returns to the successor of the object it was diverted from (routing
 * stack, see DivertCxMsg()).  An event that survives the network leaves
 * the handler for the rest of the input chain.
 *
 * Behaviour verified against AmigaOS 3.1 with the probes in
 * tests/probes/commodities/ (ParseIX() grammar, MatchIX(), the error
 * bits, SetCxObjPri() results, broker lists, BrokerCommand(), routing,
 * translator output order, AddIEvents()).
 *
 * Written from the NDK autodocs and headers and black-box probes.
 */

#include <exec/types.h>
#include <exec/memory.h>
#include <exec/execbase.h>
#include <exec/resident.h>
#include <exec/initializers.h>
#include <exec/interrupts.h>
#include <exec/ports.h>
#include <clib/exec_protos.h>
#include <inline/exec.h>

#include <devices/inputevent.h>
#include <devices/keymap.h>
#include <clib/keymap_protos.h>
#include <inline/keymap.h>

#include <dos/dos.h>
#include <dos/dosextens.h>

#include <utility/tagitem.h>

#include <libraries/commodities.h>

#include "util.h"

#define VERSION    40
#define REVISION   2
#define EXLIBNAME  "commodities"
#define EXLIBVER   " 40.2 (11.2.93)\r\n"

char __aligned _g_commodities_ExLibName [] = EXLIBNAME ".library";
char __aligned _g_commodities_ExLibID   [] = EXLIBNAME EXLIBVER;
char __aligned _g_commodities_Copyright [] = "(C)opyright 2025 by G. Bartsch. Licensed under the MIT License.";

char __aligned _g_commodities_VERSTRING [] = "\0$VER: " EXLIBNAME EXLIBVER;

extern struct ExecBase *SysBase;
extern struct Library  *KeymapBase;

/* input.device (lxa_dev_input.c) */
extern BOOL _input_device_add_handler(struct Interrupt *handler);
extern void _input_device_inject(struct InputEvent *events);

#define CX_HANDLER_PRI   53
#define CX_STACK_DEPTH   32
#define CX_EV_GENS       4
#define CX_EV_EXTRA      48

/* object flags */
#define COF_ACTIVE       2          /* public value, returned by ActivateCxObj() */

/* broker flags reported to Exchange (bc_Flags) always carry this bit */
#define CX_BROKER_FLAG   1

/* message flags */
#define CMF_DISPOSED     0x01
#define CMF_ALLOCATED    0x02

struct CxObjPriv
{
    struct Node co_Node;            /* ln_Type = CX_* type, ln_Pri = priority */
    UBYTE       co_Flags;
    UBYTE       co_Pad;
    UWORD       co_Error;
    struct List co_List;            /* objects attached to this one */
    union
    {
        IX ix;                                          /* CX_FILTER */
        ULONG typemask;                                 /* CX_TYPEFILTER */
        struct { struct MsgPort *port; LONG id; } send; /* CX_SEND */
        struct { struct Task *task; LONG sig; } sig;    /* CX_SIGNAL */
        struct InputEvent *trans;                       /* CX_TRANSLATE */
        LONG debug_id;                                  /* CX_DEBUG */
        struct { APTR action; LONG id; } custom;        /* CX_CUSTOM */
    } u;
    /* CX_BROKER */
    char            co_Name[CBD_NAMELEN];
    char            co_Title[CBD_TITLELEN];
    char            co_Descr[CBD_DESCRLEN];
    struct Task    *co_Task;
    struct MsgPort *co_Port;
    WORD            co_BFlags;
    WORD            co_Unique;
};

struct CxMsgPriv
{
    struct Message     cm_Msg;
    UBYTE              cm_Type;
    UBYTE              cm_Flags;
    WORD               cm_Level;
    LONG               cm_ID;
    APTR               cm_Data;
    struct Node       *cm_Pos;      /* next destination (may be a list tail) */
    struct CxObjPriv  *cm_Stack[CX_STACK_DEPTH];
    struct InputEvent  cm_Event;    /* copy of the event for senders */
    UBYTE              cm_Extra[CX_EV_EXTRA];
};

/* an input event owned by the library (AddIEvents(), translations) */
struct CxEvBlock
{
    struct MinNode    eb_Node;
    struct InputEvent eb_Event;
    UBYTE             eb_Extra[CX_EV_EXTRA];
};

/* private structure filled by CopyBrokerList() (used by Exchange) */
struct BrokerCopy
{
    struct Node     bc_Node;
    char            bc_Name[CBD_NAMELEN];
    char            bc_Title[CBD_TITLELEN];
    char            bc_Descr[CBD_DESCRLEN];
    struct Task    *bc_Task;
    struct MsgPort *bc_Port;
    ULONG           bc_Dummy2;
    WORD            bc_Flags;
};

struct CommoditiesBase
{
    struct Library    lib;
    BPTR              SegList;
    struct List       cx_Brokers;
    struct MsgPort    cx_ReplyPort;     /* replied CxMsgs, reused */
    struct Interrupt  cx_Handler;
    UBYTE             cx_HandlerAdded;
    UBYTE             cx_Busy;
    UBYTE             cx_Gen;
    UBYTE             cx_Pad;
    struct MinList    cx_EvFree;
    struct MinList    cx_EvGen[CX_EV_GENS];
    struct MinList    cx_EvPending;     /* AddIEvents() events not yet routed */
    struct InputEvent *cx_OutHead;
    struct InputEvent *cx_OutTail;
    struct CxMsgPriv  cx_RouteMsg;      /* the message travelling the network */
};

#define OBJ(co)   ((struct CxObjPriv *)(co))
#define MSG(m)    ((struct CxMsgPriv *)(m))

/****************************************************************************
 * small helpers
 ****************************************************************************/

static void cx_strncpy(char *dst, CONST_STRPTR src, LONG size)
{
    LONG i = 0;

    if (src)
    {
        while (i < size - 1 && src[i])
        {
            dst[i] = src[i];
            i++;
        }
    }
    dst[i] = 0;
}

static BOOL cx_streq(const char *a, const char *b)
{
    while (*a && *a == *b)
    {
        a++;
        b++;
    }
    return *a == *b;
}

static void cx_init_list(struct List *l)
{
    l->lh_Head = (struct Node *)&l->lh_Tail;
    l->lh_Tail = NULL;
    l->lh_TailPred = (struct Node *)&l->lh_Head;
}

static BOOL cx_linked(struct CxObjPriv *co)
{
    return co->co_Node.ln_Succ != NULL && co->co_Node.ln_Pred != NULL;
}

static void cx_unlink(struct CxObjPriv *co)
{
    if (cx_linked(co))
    {
        Remove(&co->co_Node);
    }
    co->co_Node.ln_Succ = NULL;
    co->co_Node.ln_Pred = NULL;
}

/* ln_Pri of exec nodes is a BYTE */
static LONG cx_pri(struct CxObjPriv *co)
{
    return (LONG)(UBYTE)co->co_Node.ln_Pri;
}

/****************************************************************************
 * messages
 ****************************************************************************/

/* a message for a port: a replied one is reused, else a new one */
static struct CxMsgPriv *cx_alloc_msg(struct CommoditiesBase *cb)
{
    struct CxMsgPriv *m = (struct CxMsgPriv *)GetMsg(&cb->cx_ReplyPort);

    if (!m)
    {
        m = (struct CxMsgPriv *)AllocMem(sizeof(struct CxMsgPriv), MEMF_PUBLIC | MEMF_CLEAR);
        if (!m)
        {
            return NULL;
        }
    }
    m->cm_Msg.mn_Node.ln_Type = NT_MESSAGE;
    m->cm_Msg.mn_Node.ln_Succ = NULL;
    m->cm_Msg.mn_Node.ln_Pred = NULL;
    m->cm_Msg.mn_ReplyPort = &cb->cx_ReplyPort;
    m->cm_Msg.mn_Length = sizeof(struct CxMsgPriv);
    m->cm_Flags = CMF_ALLOCATED;
    m->cm_Level = 0;
    m->cm_Pos = NULL;
    m->cm_ID = 0;
    m->cm_Data = NULL;
    m->cm_Type = 0;
    return m;
}

/* copy an input event (and the data a NEWPOINTERPOS event points to) */
static void cx_copy_event(struct InputEvent *dst, UBYTE *extra, const struct InputEvent *src)
{
    *dst = *src;
    dst->ie_NextEvent = NULL;

    if (src->ie_Class == IECLASS_NEWPOINTERPOS && src->ie_EventAddress)
    {
        ULONG size = 0;

        switch (src->ie_SubClass)
        {
            case IESUBCLASS_PIXEL:     size = sizeof(struct IEPointerPixel);  break;
            case IESUBCLASS_TABLET:    size = sizeof(struct IEPointerTablet); break;
            case IESUBCLASS_NEWTABLET: size = sizeof(struct IENewTablet);     break;
            default: break;
        }
        if (size && size <= CX_EV_EXTRA)
        {
            CopyMem(src->ie_EventAddress, extra, size);
            dst->ie_EventAddress = extra;
        }
    }
}

static void cx_send_command(struct CommoditiesBase *cb, struct MsgPort *port, LONG id)
{
    struct CxMsgPriv *m = cx_alloc_msg(cb);

    if (!m)
    {
        return;
    }
    m->cm_Type = CXM_COMMAND;
    m->cm_ID = id;
    PutMsg(port, &m->cm_Msg);
}

/****************************************************************************
 * library-owned input events
 ****************************************************************************/

static struct CxEvBlock *cx_alloc_event(struct CommoditiesBase *cb)
{
    struct CxEvBlock *eb = (struct CxEvBlock *)RemHead((struct List *)&cb->cx_EvFree);

    if (!eb)
    {
        eb = (struct CxEvBlock *)AllocMem(sizeof(struct CxEvBlock), MEMF_PUBLIC | MEMF_CLEAR);
    }
    return eb;
}

static void cx_out_append(struct CommoditiesBase *cb, struct InputEvent *ie)
{
    ie->ie_NextEvent = NULL;
    if (cb->cx_OutTail)
    {
        cb->cx_OutTail->ie_NextEvent = ie;
    }
    else
    {
        cb->cx_OutHead = ie;
    }
    cb->cx_OutTail = ie;
}

/****************************************************************************
 * input expressions
 ****************************************************************************/

struct CxWord
{
    const char *name;
    UWORD       value;
    UWORD       extra;
};

static const struct CxWord g_cx_classes[] =
{
    { "rawkey",       IECLASS_RAWKEY,       0 },
    { "rawmouse",     IECLASS_RAWMOUSE,     0 },
    { "event",        IECLASS_EVENT,        0 },
    { "pointerpos",   IECLASS_POINTERPOS,   0 },
    { "timer",        IECLASS_TIMER,        0 },
    { "newprefs",     IECLASS_NEWPREFS,     0 },
    { "diskremoved",  IECLASS_DISKREMOVED,  0 },
    { "diskinserted", IECLASS_DISKINSERTED, 0 },
    { NULL, 0, 0 }
};

/* qualifiers; extra = IXSYM_* for the synonyms */
static const struct CxWord g_cx_quals[] =
{
    { "lshift",        IEQUALIFIER_LSHIFT,        0 },
    { "left_shift",    IEQUALIFIER_LSHIFT,        0 },
    { "rshift",        IEQUALIFIER_RSHIFT,        0 },
    { "right_shift",   IEQUALIFIER_RSHIFT,        0 },
    { "capslock",      IEQUALIFIER_CAPSLOCK,      0 },
    { "caps_lock",     IEQUALIFIER_CAPSLOCK,      0 },
    { "ctrl",          IEQUALIFIER_CONTROL,       0 },
    { "control",       IEQUALIFIER_CONTROL,       0 },
    { "lalt",          IEQUALIFIER_LALT,          0 },
    { "left_alt",      IEQUALIFIER_LALT,          0 },
    { "ralt",          IEQUALIFIER_RALT,          0 },
    { "right_alt",     IEQUALIFIER_RALT,          0 },
    { "lcommand",      IEQUALIFIER_LCOMMAND,      0 },
    { "lamiga",        IEQUALIFIER_LCOMMAND,      0 },
    { "left_amiga",    IEQUALIFIER_LCOMMAND,      0 },
    { "left_command",  IEQUALIFIER_LCOMMAND,      0 },
    { "rcommand",      IEQUALIFIER_RCOMMAND,      0 },
    { "ramiga",        IEQUALIFIER_RCOMMAND,      0 },
    { "right_amiga",   IEQUALIFIER_RCOMMAND,      0 },
    { "right_command", IEQUALIFIER_RCOMMAND,      0 },
    { "numericpad",    IEQUALIFIER_NUMERICPAD,    0 },
    { "numeric_pad",   IEQUALIFIER_NUMERICPAD,    0 },
    { "numpad",        IEQUALIFIER_NUMERICPAD,    0 },
    { "num_pad",       IEQUALIFIER_NUMERICPAD,    0 },
    { "repeat",        IEQUALIFIER_REPEAT,        0 },
    { "midbutton",     IEQUALIFIER_MIDBUTTON,     0 },
    { "mbutton",       IEQUALIFIER_MIDBUTTON,     0 },
    { "middlebutton",  IEQUALIFIER_MIDBUTTON,     0 },
    { "middle_button", IEQUALIFIER_MIDBUTTON,     0 },
    { "rbutton",       IEQUALIFIER_RBUTTON,       0 },
    { "rightbutton",   IEQUALIFIER_RBUTTON,       0 },
    { "right_button",  IEQUALIFIER_RBUTTON,       0 },
    { "lbutton",       IEQUALIFIER_LEFTBUTTON,    0 },
    { "leftbutton",    IEQUALIFIER_LEFTBUTTON,    0 },
    { "left_button",   IEQUALIFIER_LEFTBUTTON,    0 },
    { "relativemouse", IEQUALIFIER_RELATIVEMOUSE, 0 },
    { "shift",         IXSYM_SHIFTMASK,           IXSYM_SHIFT },
    { "caps",          IXSYM_CAPSMASK,            IXSYM_CAPS },
    { "alt",           IXSYM_ALTMASK,             IXSYM_ALT },
    { NULL, 0, 0 }
};

static const struct CxWord g_cx_rawkeys[] =
{
    { "f1",  0x50, 0 }, { "f2",  0x51, 0 }, { "f3",  0x52, 0 }, { "f4",  0x53, 0 },
    { "f5",  0x54, 0 }, { "f6",  0x55, 0 }, { "f7",  0x56, 0 }, { "f8",  0x57, 0 },
    { "f9",  0x58, 0 }, { "f10", 0x59, 0 }, { "f11", 0x4b, 0 }, { "f12", 0x6f, 0 },
    { "esc",          0x45, 0 },
    { "escape",       0x45, 0 },
    { "backspace",    0x41, 0 },
    { "tab",          0x42, 0 },
    { "enter",        0x43, 0 },
    { "return",       0x44, 0 },
    { "del",          0x46, 0 },
    { "delete",       0x46, 0 },
    { "insert",       0x47, 0 },
    { "page_up",      0x48, 0 },
    { "page_down",    0x49, 0 },
    { "up",           0x4c, 0 },
    { "cursor_up",    0x4c, 0 },
    { "down",         0x4d, 0 },
    { "cursor_down",  0x4d, 0 },
    { "right",        0x4e, 0 },
    { "cursor_right", 0x4e, 0 },
    { "left",         0x4f, 0 },
    { "cursor_left",  0x4f, 0 },
    { "help",         0x5f, 0 },
    { "comma",        0x38, 0 },
    { "space",        0x40, 0 },
    { "spacebar",     0x40, 0 },
    { "break",        0x6e, 0 },
    { "pause",        0x6e, 0 },
    { "home",         0x70, 0 },
    { "end",          0x71, 0 },
    { NULL, 0, 0 }
};

static const struct CxWord g_cx_mousekeys[] =
{
    { "mouse_leftpress",   IECODE_LBUTTON, 0 },
    { "mouse_rightpress",  IECODE_RBUTTON, 0 },
    { "mouse_middlepress", IECODE_MBUTTON, 0 },
    { NULL, 0, 0 }
};

static const struct CxWord g_cx_upstroke[] =
{
    { "upstroke", IECODE_UP_PREFIX, 0 },
    { NULL, 0, 0 }
};

/* single characters with the numericpad qualifier: the keypad keys */
static const UBYTE g_cx_numpad[] =
{
    '0', 0x0f, '1', 0x1d, '2', 0x1e, '3', 0x1f, '4', 0x2d, '5', 0x2e,
    '6', 0x2f, '7', 0x3d, '8', 0x3e, '9', 0x3f, '.', 0x3c, '-', 0x4a,
    '(', 0x5a, ')', 0x5b, '/', 0x5c, '*', 0x5d, '+', 0x5e, 0, 0
};

static BOOL cx_is_sep(UBYTE c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == ',';
}

static UBYTE cx_lower(UBYTE c)
{
    return (c >= 'A' && c <= 'Z') ? (UBYTE)(c + 32) : c;
}

static const struct CxWord *cx_lookup(const struct CxWord *tab, const UBYTE *t, LONG len)
{
    for (; tab->name; tab++)
    {
        const UBYTE *n = (const UBYTE *)tab->name;
        LONG i;

        for (i = 0; i < len && n[i] && n[i] == cx_lower(t[i]); i++)
            ;
        if (i == len && !n[i])
        {
            return tab;
        }
    }
    return NULL;
}

static BOOL cx_invert_key(UBYTE c, struct InputEvent *ie, struct KeyMap *km);

/*
 * The input description grammar (AmigaOS 3.1):
 *   [class] { [-]qualifier | [-]synonym } [ [-]upstroke ] [ [-]key ]
 * Words are separated by blanks, tabs, newlines or commas and compared
 * without case.  A class only as the first word.  A key is a key name of
 * the class or (rawkey only) a single character, which must end the string
 * and contributes the qualifiers the keymap needs to type it (keypad keys
 * with the numericpad qualifier).  A '-' clears the bits from the mask; on
 * keys it is ignored.  Nothing may follow the key.
 */
static LONG cx_parse_ix(CONST_STRPTR desc, IX *ix)
{
    const UBYTE *s = (const UBYTE *)desc;
    BOOL first = TRUE, upstroke = FALSE;

    ix->ix_Class = IECLASS_RAWKEY;
    ix->ix_Code = 0;
    ix->ix_CodeMask = 0xffff;
    ix->ix_Qualifier = 0;
    ix->ix_QualMask = 0x73ff;
    ix->ix_QualSame = 0;

    if (!s || !*s)
    {
        ix->ix_Code = 0xffff;
        return -2;
    }

    for (;;)
    {
        const UBYTE *t;
        const struct CxWord *w;
        LONG len;
        BOOL neg = FALSE;

        while (*s && cx_is_sep(*s))
            s++;
        if (!*s)
            return 0;

        t = s;
        while (*s && !cx_is_sep(*s))
            s++;
        len = s - t;

        if (*t == '-')
        {
            neg = TRUE;
            t++;
            len--;
            if (len <= 0)
                return -1;
        }

        if (first && !neg && (w = cx_lookup(g_cx_classes, t, len)) != NULL)
        {
            ix->ix_Class = (UBYTE)w->value;
            first = FALSE;
            continue;
        }
        first = FALSE;

        if (!upstroke)
        {
            if ((w = cx_lookup(g_cx_quals, t, len)) != NULL)
            {
                if (neg)
                    ix->ix_QualMask &= ~w->value;
                else
                    ix->ix_Qualifier |= w->value;
                ix->ix_QualSame |= w->extra;
                continue;
            }
            if (cx_lookup(g_cx_upstroke, t, len))
            {
                upstroke = TRUE;
                if (neg)
                    ix->ix_CodeMask &= ~IECODE_UP_PREFIX;
                else
                    ix->ix_Code |= IECODE_UP_PREFIX;
                continue;
            }
        }

        /* the key */
        if (len == 1 && !t[1])
        {
            if (ix->ix_Class != IECLASS_RAWKEY)
                return -1;

            if (ix->ix_Qualifier & IEQUALIFIER_NUMERICPAD)
            {
                const UBYTE *p;

                for (p = g_cx_numpad; *p; p += 2)
                {
                    if (*p == *t)
                    {
                        ix->ix_Code = (ix->ix_Code & IECODE_UP_PREFIX) | p[1];
                        break;
                    }
                }
            }
            else
            {
                struct InputEvent ie;

                if (!cx_invert_key(*t, &ie, NULL))
                    return -1;
                ix->ix_Code = (ix->ix_Code & IECODE_UP_PREFIX) | ie.ie_Code;
                ix->ix_Qualifier |= ie.ie_Qualifier;
            }
            return 0;
        }

        if (ix->ix_Class == IECLASS_RAWKEY)
            w = cx_lookup(g_cx_rawkeys, t, len);
        else if (ix->ix_Class == IECLASS_RAWMOUSE)
            w = cx_lookup(g_cx_mousekeys, t, len);
        else
            w = NULL;
        if (!w)
            return -1;
        ix->ix_Code = (ix->ix_Code & IECODE_UP_PREFIX) | w->value;

        while (*s && cx_is_sep(*s))
            s++;
        return *s ? -1 : 0;
    }
}

static BOOL cx_match_ix(const struct InputEvent *ie, const IX *ix)
{
    UWORD q, same;

    if (ix->ix_Class != IECLASS_NULL && ie->ie_Class != ix->ix_Class)
        return FALSE;
    if ((ie->ie_Code ^ ix->ix_Code) & ix->ix_CodeMask)
        return FALSE;

    q = ie->ie_Qualifier;
    same = ix->ix_QualSame;
    if ((same & IXSYM_SHIFT) && (q & IXSYM_SHIFTMASK))
        q |= IXSYM_SHIFTMASK;
    if ((same & IXSYM_CAPS) && (q & IXSYM_CAPSMASK))
        q |= IXSYM_CAPSMASK;
    if ((same & IXSYM_ALT) && (q & IXSYM_ALTMASK))
        q |= IXSYM_ALTMASK;

    return ((q ^ ix->ix_Qualifier) & ix->ix_QualMask) == 0;
}

/* InvertKeyMap(): MapANSI() of one character; dead key prefixes go to
 * ie_Prev1Down / ie_Prev2Down */
static BOOL cx_invert_key(UBYTE c, struct InputEvent *ie, struct KeyMap *km)
{
    UBYTE buf[8];
    LONG n;

    if (!KeymapBase)
        return FALSE;

    n = MapANSI((STRPTR)&c, 1, (STRPTR)buf, 3, km);
    if (n < 1 || n > 3)
        return FALSE;

    ie->ie_Class = IECLASS_RAWKEY;
    ie->ie_SubClass = 0;
    ie->ie_TimeStamp.tv_secs = 0;
    ie->ie_TimeStamp.tv_micro = 0;
    ie->ie_Code = buf[(n - 1) * 2];
    ie->ie_Qualifier = buf[(n - 1) * 2 + 1];
    ie->ie_Prev1DownCode = 0;
    ie->ie_Prev1DownQual = 0;
    ie->ie_Prev2DownCode = 0;
    ie->ie_Prev2DownQual = 0;
    if (n >= 2)
    {
        ie->ie_Prev1DownCode = buf[(n - 2) * 2];
        ie->ie_Prev1DownQual = buf[(n - 2) * 2 + 1];
    }
    if (n == 3)
    {
        ie->ie_Prev2DownCode = buf[0];
        ie->ie_Prev2DownQual = buf[1];
    }
    return TRUE;
}

/****************************************************************************
 * routing
 ****************************************************************************/

/* custom objects: void action(CxMsg *msg, CxObj *co), arguments on the
 * stack (and in a0/a1); every register but d0/d1/a0/a1 survives */
extern void cx_call_custom(APTR action, struct CxMsgPriv *msg, struct CxObjPriv *co);
asm(
"        .text                                       \n"
"        .even                                       \n"
"        .globl  _cx_call_custom                     \n"
"_cx_call_custom:                                    \n"
"        movem.l d2-d7/a2-a6,-(sp)                   \n"
"        move.l  48(sp),a2                           | action \n"
"        move.l  52(sp),a0                           | msg \n"
"        move.l  56(sp),a1                           | co \n"
"        move.l  a1,-(sp)                            \n"
"        move.l  a0,-(sp)                            \n"
"        jsr     (a2)                                \n"
"        addq.l  #8,sp                               \n"
"        movem.l (sp)+,d2-d7/a2-a6                   \n"
"        rts                                         \n"
);

static void cx_divert(struct CxMsgPriv *m, struct CxObjPriv *head, struct CxObjPriv *ret)
{
    if (m->cm_Level >= CX_STACK_DEPTH)
    {
        LPRINTF(LOG_WARNING, "_commodities: routing stack overflow\n");
        return;
    }
    m->cm_Stack[m->cm_Level++] = ret;
    m->cm_Pos = head->co_List.lh_Head;
}

/* translations leave the network at once, in reverse list order */
static void cx_translate(struct CommoditiesBase *cb, struct InputEvent *list)
{
    struct InputEvent *head = NULL, *tail = NULL, *ie;

    for (ie = list; ie; ie = ie->ie_NextEvent)
    {
        struct CxEvBlock *eb = cx_alloc_event(cb);

        if (!eb)
            break;
        AddTail((struct List *)&cb->cx_EvGen[cb->cx_Gen], (struct Node *)eb);
        cx_copy_event(&eb->eb_Event, eb->eb_Extra, ie);
        eb->eb_Event.ie_NextEvent = head;
        head = &eb->eb_Event;
        if (!tail)
            tail = head;
    }
    if (head)
    {
        if (cb->cx_OutTail)
            cb->cx_OutTail->ie_NextEvent = head;
        else
            cb->cx_OutHead = head;
        cb->cx_OutTail = tail;
        tail->ie_NextEvent = NULL;
    }
}

static void cx_route(struct CommoditiesBase *cb, struct CxMsgPriv *m)
{
    while (!(m->cm_Flags & CMF_DISPOSED) && m->cm_Pos)
    {
        struct Node *n = m->cm_Pos;
        struct CxObjPriv *co;

        if (!n->ln_Succ)
        {
            /* end of a list: back to the successor of the object the
             * message was diverted from */
            struct CxObjPriv *ret;

            if (m->cm_Level <= 0)
            {
                m->cm_Pos = NULL;
                break;
            }
            ret = m->cm_Stack[--m->cm_Level];
            m->cm_Pos = ret->co_Node.ln_Succ ? ret->co_Node.ln_Succ : &ret->co_Node;
            continue;
        }

        co = (struct CxObjPriv *)n;
        m->cm_Pos = n->ln_Succ;

        if (!(co->co_Flags & COF_ACTIVE))
            continue;

        switch (co->co_Node.ln_Type)
        {
            case CX_BROKER:
                cx_divert(m, co, co);
                break;

            case CX_FILTER:
                if (m->cm_Type == CXM_IEVENT && m->cm_Data &&
                    !(co->co_Error & COERR_BADFILTER) &&
                    cx_match_ix((struct InputEvent *)m->cm_Data, &co->u.ix))
                {
                    cx_divert(m, co, co);
                }
                break;

            case CX_TYPEFILTER:
                if (m->cm_Type & co->u.typemask)
                    cx_divert(m, co, co);
                break;

            case CX_SEND:
                if (co->u.send.port)
                {
                    struct CxMsgPriv *c = cx_alloc_msg(cb);

                    if (c)
                    {
                        c->cm_Type = m->cm_Type;
                        c->cm_ID = co->u.send.id;
                        if (m->cm_Type == CXM_IEVENT && m->cm_Data)
                        {
                            cx_copy_event(&c->cm_Event, c->cm_Extra, (struct InputEvent *)m->cm_Data);
                            c->cm_Data = &c->cm_Event;
                        }
                        PutMsg(co->u.send.port, &c->cm_Msg);
                    }
                }
                break;

            case CX_SIGNAL:
                if (co->u.sig.task)
                    Signal(co->u.sig.task, 1UL << (co->u.sig.sig & 31));
                break;

            case CX_TRANSLATE:
                if (m->cm_Type == CXM_IEVENT)
                {
                    cx_translate(cb, co->u.trans);
                    m->cm_Flags |= CMF_DISPOSED;
                }
                break;

            case CX_DEBUG:
                DPRINTF(LOG_DEBUG, "_commodities: debug object id=%ld msg type=0x%02lx id=%ld\n",
                        co->u.debug_id, (ULONG)m->cm_Type, m->cm_ID);
                break;

            case CX_CUSTOM:
                if (co->u.custom.action)
                {
                    m->cm_ID = co->u.custom.id;
                    cx_call_custom(co->u.custom.action, m, co);
                }
                break;

            default:
                break;
        }
    }
}

/* route one input event; TRUE if it survives the network */
static BOOL cx_route_event(struct CommoditiesBase *cb, struct InputEvent *ie)
{
    struct CxMsgPriv *m = &cb->cx_RouteMsg;

    m->cm_Type = CXM_IEVENT;
    m->cm_Flags = 0;
    m->cm_Level = 0;
    m->cm_ID = 0;
    m->cm_Data = ie;
    m->cm_Pos = cb->cx_Brokers.lh_Head;
    cx_route(cb, m);
    return !(m->cm_Flags & CMF_DISPOSED);
}

/*
 * The input handler (input.device, priority 53).  a0 = events, a1 = base.
 */
static struct InputEvent *cx_input_handler(register struct InputEvent *events __asm("a0"),
                                           register struct CommoditiesBase *cb __asm("a1"))
{
    struct InputEvent *ie, *next;
    struct CxEvBlock *eb;
    UBYTE gen;

    if (!cb || cb->cx_Busy)
        return events;

    Disable();
    cb->cx_Busy = TRUE;

    /* events this handler returned a few calls ago are no longer used */
    gen = (UBYTE)((cb->cx_Gen + 1) % CX_EV_GENS);
    cb->cx_Gen = gen;
    while ((eb = (struct CxEvBlock *)RemHead((struct List *)&cb->cx_EvGen[gen])) != NULL)
        AddTail((struct List *)&cb->cx_EvFree, (struct Node *)eb);

    cb->cx_OutHead = NULL;
    cb->cx_OutTail = NULL;

    /* AddIEvents() events first */
    while ((eb = (struct CxEvBlock *)RemHead((struct List *)&cb->cx_EvPending)) != NULL)
    {
        AddTail((struct List *)&cb->cx_EvGen[gen], (struct Node *)eb);
        if (cx_route_event(cb, &eb->eb_Event))
            cx_out_append(cb, &eb->eb_Event);
    }

    for (ie = events; ie; ie = next)
    {
        next = ie->ie_NextEvent;
        ie->ie_NextEvent = NULL;

        /* the marker event AddIEvents() writes to get here */
        if (ie->ie_Class == IECLASS_NULL && ie->ie_EventAddress == (APTR)cb)
            continue;

        if (cx_route_event(cb, ie))
            cx_out_append(cb, ie);
    }

    events = cb->cx_OutHead;
    cb->cx_Busy = FALSE;
    Enable();
    return events;
}

static void cx_install_handler(struct CommoditiesBase *cb)
{
    if (cb->cx_HandlerAdded)
        return;

    cb->cx_Handler.is_Node.ln_Type = NT_INTERRUPT;
    cb->cx_Handler.is_Node.ln_Pri = CX_HANDLER_PRI;
    cb->cx_Handler.is_Node.ln_Name = _g_commodities_ExLibName;
    cb->cx_Handler.is_Data = cb;
    cb->cx_Handler.is_Code = (VOID (*)())cx_input_handler;
    /* the handler list is walked from the VBlank interrupt */
    Disable();
    if (_input_device_add_handler(&cb->cx_Handler))
        cb->cx_HandlerAdded = TRUE;
    Enable();
}

/****************************************************************************
 * objects
 ****************************************************************************/

static struct CxObjPriv *cx_new_obj(UBYTE type)
{
    struct CxObjPriv *co = (struct CxObjPriv *)AllocMem(sizeof(struct CxObjPriv), MEMF_PUBLIC | MEMF_CLEAR);

    if (!co)
        return NULL;
    co->co_Node.ln_Type = type;
    co->co_Flags = COF_ACTIVE;
    cx_init_list(&co->co_List);
    return co;
}

static void cx_delete_obj(struct CxObjPriv *co, BOOL all)
{
    struct Node *n, *succ;

    Disable();
    cx_unlink(co);
    for (n = co->co_List.lh_Head; (succ = n->ln_Succ) != NULL; n = succ)
    {
        n->ln_Succ = NULL;
        n->ln_Pred = NULL;
        if (all)
        {
            Enable();
            cx_delete_obj((struct CxObjPriv *)n, TRUE);
            Disable();
        }
    }
    cx_init_list(&co->co_List);
    Enable();

    FreeMem(co, sizeof(struct CxObjPriv));
}

static CxObj *cx_broker(struct CommoditiesBase *cb, struct NewBroker *nb, LONG *error)
{
    struct CxObjPriv *co, *old;
    char name[CBD_NAMELEN];
    struct Node *n;

    if (error)
        *error = CBERR_OK;
    if (!nb)
    {
        if (error)
            *error = CBERR_SYSERR;
        return NULL;
    }

    cx_install_handler(cb);
    cx_strncpy(name, nb->nb_Name, CBD_NAMELEN);

    Forbid();
    if (nb->nb_Unique & NBU_UNIQUE)
    {
        for (n = cb->cx_Brokers.lh_Head; n->ln_Succ; n = n->ln_Succ)
        {
            old = (struct CxObjPriv *)n;
            if (cx_streq(old->co_Name, name))
            {
                if ((nb->nb_Unique & NBU_NOTIFY) && old->co_Port)
                    cx_send_command(cb, old->co_Port, CXCMD_UNIQUE);
                Permit();
                if (error)
                    *error = CBERR_DUP;
                return NULL;
            }
        }
    }

    co = cx_new_obj(CX_BROKER);
    if (!co)
    {
        Permit();
        if (error)
            *error = CBERR_SYSERR;
        return NULL;
    }
    co->co_Flags = 0;   /* brokers start inactive */
    CopyMem(name, co->co_Name, CBD_NAMELEN);
    cx_strncpy(co->co_Title, nb->nb_Title, CBD_TITLELEN);
    cx_strncpy(co->co_Descr, nb->nb_Descr, CBD_DESCRLEN);
    co->co_Node.ln_Name = co->co_Name;
    co->co_Node.ln_Pri = nb->nb_Pri;
    co->co_Task = FindTask(NULL);
    co->co_Port = nb->nb_Port;
    co->co_BFlags = nb->nb_Flags;
    co->co_Unique = nb->nb_Unique;

    Disable();
    Enqueue(&cb->cx_Brokers, &co->co_Node);
    Enable();
    Permit();

    DPRINTF(LOG_DEBUG, "_commodities: CxBroker(\"%s\") pri=%ld -> 0x%08lx\n",
            co->co_Name, (LONG)nb->nb_Pri, (ULONG)co);
    return (CxObj *)co;
}

/****************************************************************************
 * Library init/open/close/expunge
 ****************************************************************************/

struct CommoditiesBase * __g_lxa_commodities_InitLib ( register struct CommoditiesBase *cxb __asm("d0"),
                                                       register BPTR                   seglist __asm("a0"),
                                                       register struct ExecBase        *sysb __asm("a6"))
{
    WORD i;

    DPRINTF (LOG_DEBUG, "_commodities: InitLib() called\n");
    cxb->SegList = seglist;

    cx_init_list(&cxb->cx_Brokers);
    cxb->cx_ReplyPort.mp_Node.ln_Type = NT_MSGPORT;
    cxb->cx_ReplyPort.mp_Node.ln_Name = _g_commodities_ExLibName;
    cxb->cx_ReplyPort.mp_Flags = PA_IGNORE;
    cxb->cx_ReplyPort.mp_SigBit = 0;
    cxb->cx_ReplyPort.mp_SigTask = NULL;
    cx_init_list(&cxb->cx_ReplyPort.mp_MsgList);
    cx_init_list((struct List *)&cxb->cx_EvFree);
    cx_init_list((struct List *)&cxb->cx_EvPending);
    for (i = 0; i < CX_EV_GENS; i++)
        cx_init_list((struct List *)&cxb->cx_EvGen[i]);
    cxb->cx_HandlerAdded = FALSE;
    cxb->cx_Busy = FALSE;
    cxb->cx_Gen = 0;
    cxb->cx_RouteMsg.cm_Msg.mn_Node.ln_Type = NT_MESSAGE;
    cxb->cx_RouteMsg.cm_Msg.mn_Length = sizeof(struct CxMsgPriv);
    return cxb;
}

BPTR __g_lxa_commodities_ExpungeLib ( register struct CommoditiesBase *cxb __asm("a6") )
{
    return 0;
}

struct CommoditiesBase * __g_lxa_commodities_OpenLib ( register struct CommoditiesBase *cxb __asm("a6") )
{
    DPRINTF (LOG_DEBUG, "_commodities: OpenLib() called, cxb=0x%08lx\n", (ULONG)cxb);
    cxb->lib.lib_OpenCnt++;
    cxb->lib.lib_Flags &= ~LIBF_DELEXP;
    cx_install_handler(cxb);
    return cxb;
}

BPTR __g_lxa_commodities_CloseLib ( register struct CommoditiesBase *cxb __asm("a6") )
{
    DPRINTF (LOG_DEBUG, "_commodities: CloseLib() called, cxb=0x%08lx\n", (ULONG)cxb);
    cxb->lib.lib_OpenCnt--;
    return 0;
}

ULONG __g_lxa_commodities_ExtFuncLib ( void )
{
    PRIVATE_FUNCTION_ERROR("_commodities", "ExtFuncLib");
    return 0;
}

/****************************************************************************
 * Commodities Functions (V36+)
 ****************************************************************************/

CxObj * _commodities_CreateCxObj ( register struct CommoditiesBase *CxBase __asm("a6"),
                                   register ULONG type __asm("d0"),
                                   register LONG  arg1 __asm("a0"),
                                   register LONG  arg2 __asm("a1") )
{
    struct CxObjPriv *co;

    DPRINTF (LOG_DEBUG, "_commodities: CreateCxObj() type=%ld arg1=0x%08lx arg2=0x%08lx\n",
             type, arg1, arg2);

    if (type == CX_BROKER)
        return cx_broker(CxBase, (struct NewBroker *)arg1, (LONG *)arg2);
    if (type > CX_ZERO)
        return NULL;

    co = cx_new_obj((UBYTE)type);
    if (!co)
        return NULL;

    switch (type)
    {
        case CX_FILTER:
            if (cx_parse_ix((CONST_STRPTR)arg1, &co->u.ix) != 0)
                co->co_Error |= COERR_BADFILTER;
            break;
        case CX_TYPEFILTER:
            co->u.typemask = (ULONG)arg1;
            break;
        case CX_SEND:
            co->u.send.port = (struct MsgPort *)arg1;
            co->u.send.id = arg2;
            break;
        case CX_SIGNAL:
            co->u.sig.task = (struct Task *)arg1;
            co->u.sig.sig = arg2;
            break;
        case CX_TRANSLATE:
            co->u.trans = (struct InputEvent *)arg1;
            break;
        case CX_DEBUG:
            co->u.debug_id = arg1;
            break;
        case CX_CUSTOM:
            co->u.custom.action = (APTR)arg1;
            co->u.custom.id = arg2;
            break;
        default:
            break;
    }
    return (CxObj *)co;
}

CxObj * _commodities_CxBroker ( register struct CommoditiesBase *CxBase __asm("a6"),
                                register struct NewBroker *nb __asm("a0"),
                                register LONG *error __asm("d0") )
{
    return cx_broker(CxBase, nb, error);
}

LONG _commodities_ActivateCxObj ( register struct CommoditiesBase *CxBase __asm("a6"),
                                  register CxObj *co __asm("a0"),
                                  register LONG flag __asm("d0") )
{
    LONG old;

    if (!co)
        return 0;

    Disable();
    old = OBJ(co)->co_Flags & COF_ACTIVE;
    if (flag)
        OBJ(co)->co_Flags |= COF_ACTIVE;
    else
        OBJ(co)->co_Flags &= ~COF_ACTIVE;
    Enable();
    return old;
}

void _commodities_DeleteCxObj ( register struct CommoditiesBase *CxBase __asm("a6"),
                                register CxObj *co __asm("a0") )
{
    if (co)
        cx_delete_obj(OBJ(co), FALSE);
}

void _commodities_DeleteCxObjAll ( register struct CommoditiesBase *CxBase __asm("a6"),
                                   register CxObj *co __asm("a0") )
{
    if (co)
        cx_delete_obj(OBJ(co), TRUE);
}

ULONG _commodities_CxObjType ( register struct CommoditiesBase *CxBase __asm("a6"),
                               register CxObj *co __asm("a0") )
{
    return co ? (ULONG)OBJ(co)->co_Node.ln_Type : CX_INVALID;
}

LONG _commodities_CxObjError ( register struct CommoditiesBase *CxBase __asm("a6"),
                               register CxObj *co __asm("a0") )
{
    return co ? (LONG)OBJ(co)->co_Error : COERR_ISNULL;
}

void _commodities_ClearCxObjError ( register struct CommoditiesBase *CxBase __asm("a6"),
                                    register CxObj *co __asm("a0") )
{
    if (co)
        OBJ(co)->co_Error = 0;
}

/* returns the old priority as an unsigned byte (AmigaOS 3.1); the object
 * keeps its place in its list (autodoc BUGS) */
LONG _commodities_SetCxObjPri ( register struct CommoditiesBase *CxBase __asm("a6"),
                                register CxObj *co __asm("a0"),
                                register LONG pri __asm("d0") )
{
    LONG old;

    if (!co)
        return 0;
    old = cx_pri(OBJ(co));
    OBJ(co)->co_Node.ln_Pri = (BYTE)pri;
    return old;
}

/* common part of Attach/Enqueue/Insert: FALSE if nothing is to be done */
static BOOL cx_attach_check(CxObj *headObj, CxObj *co)
{
    if (!headObj)
    {
        if (co)
            cx_delete_obj(OBJ(co), TRUE);
        return FALSE;
    }
    if (!co)
    {
        OBJ(headObj)->co_Error |= COERR_NULLATTACH;
        return FALSE;
    }
    return TRUE;
}

void _commodities_AttachCxObj ( register struct CommoditiesBase *CxBase __asm("a6"),
                                register CxObj *headObj __asm("a0"),
                                register CxObj *co __asm("a1") )
{
    if (!cx_attach_check(headObj, co))
        return;
    Disable();
    cx_unlink(OBJ(co));
    AddTail(&OBJ(headObj)->co_List, &OBJ(co)->co_Node);
    Enable();
}

void _commodities_EnqueueCxObj ( register struct CommoditiesBase *CxBase __asm("a6"),
                                 register CxObj *headObj __asm("a0"),
                                 register CxObj *co __asm("a1") )
{
    if (!cx_attach_check(headObj, co))
        return;
    Disable();
    cx_unlink(OBJ(co));
    Enqueue(&OBJ(headObj)->co_List, &OBJ(co)->co_Node);
    Enable();
}

void _commodities_InsertCxObj ( register struct CommoditiesBase *CxBase __asm("a6"),
                                register CxObj *headObj __asm("a0"),
                                register CxObj *co __asm("a1"),
                                register CxObj *pred __asm("a2") )
{
    if (!cx_attach_check(headObj, co))
        return;
    Disable();
    cx_unlink(OBJ(co));
    if (pred && !cx_linked(OBJ(pred)))
        pred = NULL;
    Insert(&OBJ(headObj)->co_List, &OBJ(co)->co_Node, pred ? &OBJ(pred)->co_Node : NULL);
    Enable();
}

void _commodities_RemoveCxObj ( register struct CommoditiesBase *CxBase __asm("a6"),
                                register CxObj *co __asm("a0") )
{
    if (!co)
        return;
    Disable();
    cx_unlink(OBJ(co));
    Enable();
}

/* Private function slot */
ULONG _commodities_Private1 ( register struct CommoditiesBase *CxBase __asm("a6") )
{
    PRIVATE_FUNCTION_ERROR("_commodities", "Private1");
    return 0;
}

void _commodities_SetTranslate ( register struct CommoditiesBase *CxBase __asm("a6"),
                                 register CxObj *translator __asm("a0"),
                                 register struct InputEvent *events __asm("a1") )
{
    if (!translator)
        return;
    if (OBJ(translator)->co_Node.ln_Type != CX_TRANSLATE)
    {
        OBJ(translator)->co_Error |= COERR_BADTYPE;
        return;
    }
    OBJ(translator)->u.trans = events;
}

void _commodities_SetFilter ( register struct CommoditiesBase *CxBase __asm("a6"),
                              register CxObj *filter __asm("a0"),
                              register STRPTR text __asm("a1") )
{
    IX ix;
    LONG r;

    if (!filter)
        return;
    if (OBJ(filter)->co_Node.ln_Type != CX_FILTER)
    {
        OBJ(filter)->co_Error |= COERR_BADTYPE;
        return;
    }
    r = cx_parse_ix(text, &ix);
    Disable();
    OBJ(filter)->u.ix = ix;
    if (r == 0)
        OBJ(filter)->co_Error &= ~COERR_BADFILTER;
    else
        OBJ(filter)->co_Error |= COERR_BADFILTER;
    Enable();
}

void _commodities_SetFilterIX ( register struct CommoditiesBase *CxBase __asm("a6"),
                                register CxObj *filter __asm("a0"),
                                register IX *ix __asm("a1") )
{
    if (!filter)
        return;
    if (OBJ(filter)->co_Node.ln_Type != CX_FILTER)
    {
        OBJ(filter)->co_Error |= COERR_BADTYPE;
        return;
    }
    if (!ix || ix->ix_Version != IX_VERSION)
    {
        OBJ(filter)->co_Error |= COERR_BADFILTER;
        return;
    }
    Disable();
    OBJ(filter)->u.ix = *ix;
    OBJ(filter)->co_Error &= ~COERR_BADFILTER;
    Enable();
}

LONG _commodities_ParseIX ( register struct CommoditiesBase *CxBase __asm("a6"),
                            register STRPTR description __asm("a0"),
                            register IX *ix __asm("a1") )
{
    UBYTE version;
    LONG r;

    if (!ix)
        return -1;
    /* ParseIX() leaves ix_Version alone (AmigaOS 3.1) */
    version = ix->ix_Version;
    r = cx_parse_ix(description, ix);
    ix->ix_Version = version;
    return r;
}

ULONG _commodities_CxMsgType ( register struct CommoditiesBase *CxBase __asm("a6"),
                               register CxMsg *cxm __asm("a0") )
{
    return cxm ? (ULONG)MSG(cxm)->cm_Type : 0;
}

APTR _commodities_CxMsgData ( register struct CommoditiesBase *CxBase __asm("a6"),
                              register CxMsg *cxm __asm("a0") )
{
    return cxm ? MSG(cxm)->cm_Data : NULL;
}

LONG _commodities_CxMsgID ( register struct CommoditiesBase *CxBase __asm("a6"),
                            register CxMsg *cxm __asm("a0") )
{
    return cxm ? MSG(cxm)->cm_ID : 0;
}

void _commodities_DivertCxMsg ( register struct CommoditiesBase *CxBase __asm("a6"),
                                register CxMsg *cxm __asm("a0"),
                                register CxObj *headObj __asm("a1"),
                                register CxObj *returnObj __asm("a2") )
{
    if (!cxm || !headObj)
        return;
    cx_divert(MSG(cxm), OBJ(headObj), returnObj ? OBJ(returnObj) : OBJ(headObj));
}

void _commodities_RouteCxMsg ( register struct CommoditiesBase *CxBase __asm("a6"),
                               register CxMsg *cxm __asm("a0"),
                               register CxObj *co __asm("a1") )
{
    if (!cxm || !co)
        return;
    MSG(cxm)->cm_Pos = &OBJ(co)->co_Node;
}

void _commodities_DisposeCxMsg ( register struct CommoditiesBase *CxBase __asm("a6"),
                                 register CxMsg *cxm __asm("a0") )
{
    struct CxMsgPriv *m = MSG(cxm);

    if (!m)
        return;
    if (m == &CxBase->cx_RouteMsg)
    {
        m->cm_Flags |= CMF_DISPOSED;
        return;
    }
    /* a message from a sender: back to the pool */
    if (m->cm_Flags & CMF_ALLOCATED)
    {
        m->cm_Flags |= CMF_DISPOSED;
        Disable();
        AddTail(&CxBase->cx_ReplyPort.mp_MsgList, &m->cm_Msg.mn_Node);
        Enable();
    }
}

BOOL _commodities_InvertKeyMap ( register struct CommoditiesBase *CxBase __asm("a6"),
                                 register ULONG ansiCode __asm("d0"),
                                 register struct InputEvent *event __asm("a0"),
                                 register struct KeyMap *km __asm("a1") )
{
    if (!event)
        return FALSE;
    return cx_invert_key((UBYTE)ansiCode, event, km);
}

void _commodities_AddIEvents ( register struct CommoditiesBase *CxBase __asm("a6"),
                               register struct InputEvent *events __asm("a0") )
{
    struct MinList list;
    struct InputEvent *ie;
    struct CxEvBlock *eb;
    struct InputEvent marker;

    if (!events)
        return;

    cx_init_list((struct List *)&list);
    for (ie = events; ie; ie = ie->ie_NextEvent)
    {
        Disable();
        eb = cx_alloc_event(CxBase);
        Enable();
        if (!eb)
            break;
        cx_copy_event(&eb->eb_Event, eb->eb_Extra, ie);
        AddTail((struct List *)&list, (struct Node *)eb);
    }

    Disable();
    while ((eb = (struct CxEvBlock *)RemHead((struct List *)&list)) != NULL)
        AddTail((struct List *)&CxBase->cx_EvPending, (struct Node *)eb);
    Enable();

    /* run them through the input chain now (from inside the handler they
     * wait for the next input event) */
    if (!CxBase->cx_Busy)
    {
        marker.ie_NextEvent = NULL;
        marker.ie_Class = IECLASS_NULL;
        marker.ie_SubClass = 0;
        marker.ie_Code = 0;
        marker.ie_Qualifier = 0;
        marker.ie_EventAddress = (APTR)CxBase;
        _input_device_inject(&marker);
    }
}

/*
 * Private functions used by Exchange
 */

/* CopyBrokerList(list) - a BrokerCopy node for each broker; returns the count */
LONG _commodities_CopyBrokerList ( register struct CommoditiesBase *CxBase __asm("a6"),
                                   register struct List *list __asm("a0") )
{
    struct Node *n;
    LONG count = 0;

    if (!list)
        return 0;

    Forbid();
    for (n = CxBase->cx_Brokers.lh_Head; n->ln_Succ; n = n->ln_Succ)
    {
        struct CxObjPriv *co = (struct CxObjPriv *)n;
        struct BrokerCopy *bc = (struct BrokerCopy *)AllocMem(sizeof(struct BrokerCopy),
                                                              MEMF_PUBLIC | MEMF_CLEAR);
        if (!bc)
            break;
        CopyMem(co->co_Name, bc->bc_Name, CBD_NAMELEN);
        CopyMem(co->co_Title, bc->bc_Title, CBD_TITLELEN);
        CopyMem(co->co_Descr, bc->bc_Descr, CBD_DESCRLEN);
        bc->bc_Node.ln_Name = bc->bc_Name;
        bc->bc_Node.ln_Pri = co->co_Node.ln_Pri;
        bc->bc_Task = co->co_Task;
        bc->bc_Port = co->co_Port;
        bc->bc_Flags = (WORD)(co->co_BFlags | (co->co_Flags & COF_ACTIVE) | CX_BROKER_FLAG);
        AddTail(list, &bc->bc_Node);
        count++;
    }
    Permit();
    return count;
}

/* FreeBrokerList(list) - frees the nodes CopyBrokerList() made */
void _commodities_FreeBrokerList ( register struct CommoditiesBase *CxBase __asm("a6"),
                                   register struct List *list __asm("a0") )
{
    struct Node *n;

    if (!list)
        return;
    while ((n = RemHead(list)) != NULL)
        FreeMem(n, sizeof(struct BrokerCopy));
}

/* BrokerCommand(name, id) - send a CXM_COMMAND to the (first) broker of
 * that name: 0 sent, -1 no such broker, -2 the broker has no port */
LONG _commodities_BrokerCommand ( register struct CommoditiesBase *CxBase __asm("a6"),
                                  register STRPTR name __asm("a0"),
                                  register LONG id __asm("d0") )
{
    struct Node *n;
    LONG r = -1;

    if (!name)
        return -1;

    Forbid();
    for (n = CxBase->cx_Brokers.lh_Head; n->ln_Succ; n = n->ln_Succ)
    {
        struct CxObjPriv *co = (struct CxObjPriv *)n;

        if (cx_streq(co->co_Name, (const char *)name))
        {
            if (co->co_Port)
            {
                cx_send_command(CxBase, co->co_Port, id);
                r = 0;
            }
            else
            {
                r = -2;
            }
            break;
        }
    }
    Permit();
    return r;
}

BOOL _commodities_MatchIX ( register struct CommoditiesBase *CxBase __asm("a6"),
                            register struct InputEvent *event __asm("a0"),
                            register IX *ix __asm("a1") )
{
    if (!event || !ix)
        return FALSE;
    return cx_match_ix(event, ix);
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

extern APTR              __g_lxa_commodities_FuncTab [];
extern struct MyDataInit __g_lxa_commodities_DataTab;
extern struct InitTable  __g_lxa_commodities_InitTab;
extern APTR              __g_lxa_commodities_EndResident;

static struct Resident __aligned ROMTag =
{
    RTC_MATCHWORD,                         // UWORD rt_MatchWord
    &ROMTag,                               // struct Resident *rt_MatchTag
    &__g_lxa_commodities_EndResident,      // APTR  rt_EndSkip
    RTF_AUTOINIT,                          // UBYTE rt_Flags
    VERSION,                               // UBYTE rt_Version
    NT_LIBRARY,                            // UBYTE rt_Type
    0,                                     // BYTE  rt_Pri
    &_g_commodities_ExLibName[0],          // char  *rt_Name
    &_g_commodities_ExLibID[0],            // char  *rt_IdString
    &__g_lxa_commodities_InitTab           // APTR  rt_Init
};

APTR __g_lxa_commodities_EndResident;
struct Resident *__lxa_commodities_ROMTag = &ROMTag;

struct InitTable __g_lxa_commodities_InitTab =
{
    (ULONG)               sizeof(struct CommoditiesBase),
    (APTR              *) &__g_lxa_commodities_FuncTab[0],
    (APTR)                &__g_lxa_commodities_DataTab,
    (APTR)                __g_lxa_commodities_InitLib
};

/* Function table - from commodities_lib.fd; the private slots -186,
 * -192 and -198 are CopyBrokerList(), FreeBrokerList() and
 * BrokerCommand() (used by Exchange). */
APTR __g_lxa_commodities_FuncTab [] =
{
    __g_lxa_commodities_OpenLib,           // -6   Standard
    __g_lxa_commodities_CloseLib,          // -12  Standard
    __g_lxa_commodities_ExpungeLib,        // -18  Standard
    __g_lxa_commodities_ExtFuncLib,        // -24  Standard (reserved)
    _commodities_CreateCxObj,              // -30  CreateCxObj
    _commodities_CxBroker,                 // -36  CxBroker
    _commodities_ActivateCxObj,            // -42  ActivateCxObj
    _commodities_DeleteCxObj,              // -48  DeleteCxObj
    _commodities_DeleteCxObjAll,           // -54  DeleteCxObjAll
    _commodities_CxObjType,                // -60  CxObjType
    _commodities_CxObjError,               // -66  CxObjError
    _commodities_ClearCxObjError,          // -72  ClearCxObjError
    _commodities_SetCxObjPri,              // -78  SetCxObjPri
    _commodities_AttachCxObj,              // -84  AttachCxObj
    _commodities_EnqueueCxObj,             // -90  EnqueueCxObj
    _commodities_InsertCxObj,              // -96  InsertCxObj
    _commodities_RemoveCxObj,              // -102 RemoveCxObj
    _commodities_Private1,                 // -108 Private1
    _commodities_SetTranslate,             // -114 SetTranslate
    _commodities_SetFilter,                // -120 SetFilter
    _commodities_SetFilterIX,              // -126 SetFilterIX
    _commodities_ParseIX,                  // -132 ParseIX
    _commodities_CxMsgType,                // -138 CxMsgType
    _commodities_CxMsgData,                // -144 CxMsgData
    _commodities_CxMsgID,                  // -150 CxMsgID
    _commodities_DivertCxMsg,              // -156 DivertCxMsg
    _commodities_RouteCxMsg,               // -162 RouteCxMsg
    _commodities_DisposeCxMsg,             // -168 DisposeCxMsg
    _commodities_InvertKeyMap,             // -174 InvertKeyMap
    _commodities_AddIEvents,               // -180 AddIEvents
    _commodities_CopyBrokerList,           // -186 CopyBrokerList (private)
    _commodities_FreeBrokerList,           // -192 FreeBrokerList (private)
    _commodities_BrokerCommand,            // -198 BrokerCommand (private)
    _commodities_MatchIX,                  // -204 MatchIX
    (APTR) ((LONG)-1)
};

struct MyDataInit __g_lxa_commodities_DataTab =
{
    /* ln_Type      */ INITBYTE(OFFSET(Node,         ln_Type),      NT_LIBRARY),
    /* ln_Name      */ 0x80, (UBYTE) (ULONG) OFFSET(Node,    ln_Name), (ULONG) &_g_commodities_ExLibName[0],
    /* lib_Flags    */ INITBYTE(OFFSET(Library,      lib_Flags),    LIBF_SUMUSED|LIBF_CHANGED),
    /* lib_Version  */ INITWORD(OFFSET(Library,      lib_Version),  VERSION),
    /* lib_Revision */ INITWORD(OFFSET(Library,      lib_Revision), REVISION),
    /* lib_IdString */ 0x80, (UBYTE) (ULONG) OFFSET(Library, lib_IdString), (ULONG) &_g_commodities_ExLibID[0],
    (ULONG) 0
};
