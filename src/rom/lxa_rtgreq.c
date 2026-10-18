/*
 * lxa_rtgreq.c - the screen mode requester of lxa's RTG disk libraries
 * (p96RequestModeIDTagList, CModeRequestTagList; Phase 246).
 *
 * Both libraries show the same small GadTools window, laid out like the
 * one of Picasso96 2.0 on the `rtg` reference machine: a "Board" cycle
 * gadget, a "Mode" list of the board modes the caller's tags allow, Ok and
 * Cancel buttons and a Project menu (About, Quit).  The geometry and the
 * list texts were observed on the reference (tests/scenarios/
 * gallery-rtg-*req.yaml); the code is lxa's own.
 */
#include <exec/types.h>
#include <exec/memory.h>
#include <exec/lists.h>
#include <clib/exec_protos.h>
#include <inline/exec.h>
#include <graphics/gfx.h>
#include <graphics/text.h>
#include <graphics/gfxmacros.h>
#include <clib/graphics_protos.h>
#include <inline/graphics.h>
#include <intuition/intuition.h>
#include <intuition/screens.h>
#include <clib/intuition_protos.h>
#include <inline/intuition.h>
#include <libraries/gadtools.h>
#include <clib/gadtools_protos.h>
#include <inline/gadtools.h>
#include <utility/tagitem.h>

#include "util.h"
#include "lxa_rtglib.h"
#include "lxa_p96.h"

struct Library *GadToolsBase = NULL;

/* geometry of the reference window (topaz 8): offsets below the title bar */
#define RQ_WIDTH      355
#define RQ_GAD_LEFT   70
#define RQ_GAD_WIDTH  264
#define RQ_BOARD_TOP  9
#define RQ_LIST_TOP   25
#define RQ_LIST_LINES 11
#define RQ_BTN_TOP    125
#define RQ_BTN_WIDTH  86
#define RQ_BTN_LEFT   13
#define RQ_BTN_RIGHT  256
#define RQ_BOTTOM     144     /* window height - title bar */
#define RQ_IDCMP      0x0060077cUL
#define RQ_PANEL_LEFT   13
#define RQ_PANEL_TOP    5
#define RQ_PANEL_WIDTH  329
#define RQ_PANEL_HEIGHT 116

enum { GID_BOARD, GID_MODE, GID_OK, GID_CANCEL };

/* one entry of the "Mode" list */
typedef struct
{
    struct Node node;
    ULONG       id;
    char        text[48];
} rq_mode_t;

static char *rq_dec(char *p, ULONG v, int width, BOOL left)
{
    char tmp[12];
    int n = 0;

    do
    {
        tmp[n++] = (char)('0' + v % 10);
        v /= 10;
    } while (v && n < (int)sizeof(tmp));
    if (!left)
        while (width-- > n)
            *p++ = ' ';
    else
        width -= n;
    while (n)
        *p++ = tmp[--n];
    if (left)
        while (width-- > 0)
            *p++ = ' ';
    return p;
}

/* "%4ldx%-4ld %2ld bit <format name>" */
static void rq_mode_text(rq_mode_t *m, const rtgl_modereq_t *r)
{
    char *p = m->text, *end = m->text + sizeof(m->text) - 1;
    const char *s;
    ULONG fmt = rtgl_call(RTG_OP_MODE_ATTR, m->id, P96IDA_RGBFORMAT, 0, 0);

    p = rq_dec(p, rtgl_call(RTG_OP_MODE_ATTR, m->id, P96IDA_WIDTH, 0, 0), 4, FALSE);
    *p++ = 'x';
    p = rq_dec(p, rtgl_call(RTG_OP_MODE_ATTR, m->id, P96IDA_HEIGHT, 0, 0), 4, TRUE);
    *p++ = ' ';
    p = rq_dec(p, rtgl_call(RTG_OP_MODE_ATTR, m->id, P96IDA_DEPTH, 0, 0), 2, FALSE);
    for (s = " bit "; *s; s++)
        *p++ = *s;
    for (s = (const char *)r->fmt_name(fmt); *s && p < end; s++)
        *p++ = *s;
    *p = 0;
}

static void rq_free_list(struct List *l)
{
    struct Node *n;

    while ((n = RemHead(l)) != NULL)
        FreeVec(n);
}

/* the board modes the caller accepts, in board order; the first one is
 * selected (*sel = 0), whatever mode the caller passes (reference) */
static LONG rq_build_list(const rtgl_modereq_t *r, struct List *l, LONG *sel)
{
    ULONG id;
    LONG n = 0;

    l->lh_Head = (struct Node *)&l->lh_Tail;
    l->lh_Tail = NULL;
    l->lh_TailPred = (struct Node *)&l->lh_Head;
    *sel = 0;
    for (id = rtgl_call(RTG_OP_NEXT_ID, INVALID_ID, 0, 0, 0); id != INVALID_ID;
         id = rtgl_call(RTG_OP_NEXT_ID, id, 0, 0, 0))
    {
        rq_mode_t *m;

        if (!r->match(r->ud, id))
            continue;
        m = (rq_mode_t *)AllocVec(sizeof(rq_mode_t), MEMF_PUBLIC | MEMF_CLEAR);
        if (!m)
            break;
        m->id = id;
        rq_mode_text(m, r);
        m->node.ln_Name = m->text;
        AddTail(l, &m->node);
        n++;
    }
    return n;
}

static ULONG rq_id_at(struct List *l, LONG index)
{
    struct Node *n;

    for (n = l->lh_Head; n->ln_Succ; n = n->ln_Succ)
        if (index-- == 0)
            return ((rq_mode_t *)n)->id;
    return INVALID_ID;
}

/* the window background (reference): a FILLPEN/BACKGROUNDPEN dither and a
 * recessed panel around the board and mode gadgets */
static void rq_backdrop(struct Window *win, APTR vi, WORD bt)
{
    static const UWORD dither[2] = { 0x5555, 0xaaaa };
    struct RastPort *rp = win->RPort;
    struct DrawInfo *dri = GetScreenDrawInfo(win->WScreen);
    UBYTE fill = dri ? (UBYTE)dri->dri_Pens[FILLPEN] : 3;
    UBYTE back = dri ? (UBYTE)dri->dri_Pens[BACKGROUNDPEN] : 0;
    WORD x0 = win->BorderLeft, y0 = win->BorderTop;
    WORD x1 = (WORD)(win->Width - win->BorderRight - 1);
    WORD y1 = (WORD)(win->Height - win->BorderBottom - 1);

    if (dri)
        FreeScreenDrawInfo(win->WScreen, dri);
    SetDrMd(rp, JAM2);
    SetAPen(rp, fill);
    SetBPen(rp, back);
    SetAfPt(rp, (UWORD *)dither, 1);
    RectFill(rp, x0, y0, x1, y1);
    SetAfPt(rp, NULL, 0);
    SetAPen(rp, back);
    RectFill(rp, RQ_PANEL_LEFT + 2, bt + RQ_PANEL_TOP + 1,
             RQ_PANEL_LEFT + RQ_PANEL_WIDTH - 3, bt + RQ_PANEL_TOP + RQ_PANEL_HEIGHT - 2);
    {
        struct TagItem t[] = { { GT_VisualInfo, (ULONG)vi }, { GTBB_Recessed, TRUE }, { TAG_DONE, 0 } };
        DrawBevelBoxA(rp, RQ_PANEL_LEFT, bt + RQ_PANEL_TOP, RQ_PANEL_WIDTH, RQ_PANEL_HEIGHT, t);
    }
}

static void rq_about(const rtgl_modereq_t *r)
{
    struct EasyStruct es;

    es.es_StructSize = sizeof(es);
    es.es_Flags = 0;
    es.es_Title = (UBYTE *)r->about_title;
    es.es_TextFormat = (UBYTE *)r->about_text;
    es.es_GadgetFormat = (UBYTE *)"Ok";
    EasyRequestArgs(NULL, &es, NULL, NULL);
}

ULONG rtgl_mode_request(const rtgl_modereq_t *r)
{
    static const char *const board_labels[] = { "uaegfx.card", NULL };
    struct NewMenu nm[] = {
        { NM_TITLE, (STRPTR)"Project", NULL, 0, 0, NULL },
        { NM_ITEM,  (STRPTR)"About",   (STRPTR)"?", 0, 0, NULL },
        { NM_ITEM,  NM_BARLABEL,       NULL, 0, 0, NULL },
        { NM_ITEM,  (STRPTR)"Quit",    (STRPTR)"Q", 0, 0, NULL },
        { NM_END,   NULL,              NULL, 0, 0, NULL }
    };
    struct List modes;
    struct Screen *scr = r->screen, *locked = NULL;
    struct Gadget *glist = NULL, *g, *lv = NULL;
    struct Menu *menu = NULL;
    struct Window *win = NULL;
    struct NewGadget ng;
    APTR vi = NULL;
    LONG count, sel = 0;
    WORD bt;
    ULONG result = INVALID_ID;
    BOOL done = FALSE;

    if (!GadToolsBase)
        GadToolsBase = OpenLibrary((CONST_STRPTR)"gadtools.library", 39);
    if (!GadToolsBase)
        return INVALID_ID;
    count = rq_build_list(r, &modes, &sel);
    if (!scr)
        scr = locked = LockPubScreen((CONST_STRPTR)r->pubname);
    if (!scr)
        goto out;
    vi = GetVisualInfoA(scr, NULL);
    if (!vi)
        goto out;
    bt = (WORD)(scr->WBorTop + scr->Font->ta_YSize + 1);

    g = CreateContext(&glist);
    ng.ng_TextAttr = scr->Font;
    ng.ng_VisualInfo = vi;
    ng.ng_UserData = NULL;

    ng.ng_LeftEdge = RQ_GAD_LEFT;
    ng.ng_TopEdge = bt + RQ_BOARD_TOP;
    ng.ng_Width = RQ_GAD_WIDTH;
    ng.ng_Height = 13;
    ng.ng_GadgetText = (STRPTR)"_Board";
    ng.ng_GadgetID = GID_BOARD;
    ng.ng_Flags = PLACETEXT_LEFT;
    {
        struct TagItem t[] = {
            { GTCY_Labels, (ULONG)board_labels }, { GT_Underscore, '_' }, { TAG_DONE, 0 } };
        g = CreateGadgetA(CYCLE_KIND, g, &ng, t);
    }

    ng.ng_TopEdge = bt + RQ_LIST_TOP;
    ng.ng_Height = (WORD)(RQ_LIST_LINES * scr->Font->ta_YSize + 4);
    ng.ng_GadgetText = (STRPTR)"_Mode";
    ng.ng_GadgetID = GID_MODE;
    {
        struct TagItem t[] = {
            { GTLV_Labels, (ULONG)&modes }, { GTLV_ShowSelected, 0 },
            { GTLV_Selected, (ULONG)(count ? sel : ~0) }, { GTLV_MakeVisible, (ULONG)sel },
            { GT_Underscore, '_' }, { TAG_DONE, 0 } };
        g = lv = CreateGadgetA(LISTVIEW_KIND, g, &ng, t);
    }

    ng.ng_LeftEdge = RQ_BTN_LEFT;
    ng.ng_TopEdge = bt + RQ_BTN_TOP;
    ng.ng_Width = RQ_BTN_WIDTH;
    ng.ng_Height = 13;
    ng.ng_GadgetText = (STRPTR)(r->ok_text ? r->ok_text : (CONST_STRPTR)"Ok");
    ng.ng_GadgetID = GID_OK;
    ng.ng_Flags = PLACETEXT_IN;
    g = CreateGadgetA(BUTTON_KIND, g, &ng, NULL);

    ng.ng_LeftEdge = RQ_BTN_RIGHT;
    ng.ng_GadgetText = (STRPTR)(r->cancel_text ? r->cancel_text : (CONST_STRPTR)"Cancel");
    ng.ng_GadgetID = GID_CANCEL;
    g = CreateGadgetA(BUTTON_KIND, g, &ng, NULL);
    if (!g)
        goto out;

    menu = CreateMenusA(nm, NULL);
    if (menu)
    {
        struct TagItem t[] = { { GTMN_NewLookMenus, (ULONG)r->newlook_layout }, { TAG_DONE, 0 } };
        if (!LayoutMenusA(menu, vi, t))
        {
            FreeMenus(menu);
            menu = NULL;
        }
    }

    {
        WORD h = (WORD)(bt + RQ_BOTTOM);
        struct TagItem t[] = {
            { WA_Left, (ULONG)((scr->Width - RQ_WIDTH) / 2) },
            { WA_Top, (ULONG)((scr->Height - h + 1) / 2) },
            { WA_Width, RQ_WIDTH }, { WA_Height, (ULONG)h },
            { WA_Title, (ULONG)(r->title ? r->title : (CONST_STRPTR)"Picasso96 - Best Mode") },
            { locked ? WA_PubScreen : WA_CustomScreen, (ULONG)scr },
            { WA_IDCMP, RQ_IDCMP },
            { WA_DragBar, TRUE }, { WA_DepthGadget, TRUE }, { WA_CloseGadget, TRUE },
            { WA_Activate, TRUE }, { WA_NewLookMenus, TRUE },
            { TAG_DONE, 0 } };
        win = OpenWindowTagList(NULL, t);
    }
    if (!win)
        goto out;
    rq_backdrop(win, vi, bt);
    AddGList(win, glist, (UWORD)~0, -1, NULL);
    RefreshGList(glist, win, NULL, -1);
    GT_RefreshWindow(win, NULL);
    if (menu)
        SetMenuStrip(win, menu);

    while (!done)
    {
        struct IntuiMessage *im;

        WaitPort(win->UserPort);
        while (!done && (im = GT_GetIMsg(win->UserPort)) != NULL)
        {
            ULONG cls = im->Class;
            UWORD code = im->Code;
            struct Gadget *gad = (struct Gadget *)im->IAddress;

            GT_ReplyIMsg(im);
            switch (cls)
            {
                case IDCMP_REFRESHWINDOW:
                    GT_BeginRefresh(win);
                    rq_backdrop(win, vi, bt);
                    RefreshGList(glist, win, NULL, -1);
                    GT_EndRefresh(win, TRUE);
                    break;
                case IDCMP_CLOSEWINDOW:
                    done = TRUE;
                    break;
                case IDCMP_GADGETUP:
                    switch (gad->GadgetID)
                    {
                        case GID_MODE:
                            sel = code;
                            break;
                        case GID_OK:
                            if (count)
                                result = rq_id_at(&modes, sel);
                            done = TRUE;
                            break;
                        case GID_CANCEL:
                            done = TRUE;
                            break;
                        default:
                            break;
                    }
                    break;
                case IDCMP_MENUPICK:
                    while (code != MENUNULL && !done)
                    {
                        struct MenuItem *item = ItemAddress(menu, code);

                        if (ITEMNUM(code) == 0)
                            rq_about(r);
                        else if (ITEMNUM(code) == 2)
                            done = TRUE;
                        if (!item)
                            break;
                        code = item->NextSelect;
                    }
                    break;
                case IDCMP_VANILLAKEY:
                    if (code == 27)
                        done = TRUE;
                    else if ((code == 'm' || code == 'M') && count)
                    {
                        /* the "_Mode" shortcut: next entry, previous with shift */
                        if (code == 'm')
                            sel = sel + 1 < count ? sel + 1 : 0;
                        else
                            sel = sel > 0 ? sel - 1 : count - 1;
                        {
                            struct TagItem t[] = {
                                { GTLV_Selected, (ULONG)sel }, { GTLV_MakeVisible, (ULONG)sel },
                                { TAG_DONE, 0 } };
                            GT_SetGadgetAttrsA(lv, win, NULL, t);
                        }
                    }
                    break;
                default:
                    break;
            }
        }
    }

out:
    if (win)
    {
        ClearMenuStrip(win);
        RemoveGList(win, glist, -1);
        CloseWindow(win);
    }
    if (menu)
        FreeMenus(menu);
    FreeGadgets(glist);
    if (vi)
        FreeVisualInfo(vi);
    if (locked)
        UnlockPubScreen(NULL, locked);
    rq_free_list(&modes);
    return result;
}
