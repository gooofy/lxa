/*
 * lxa gadtools.library implementation
 *
 * Provides the GadTools API for creating standard AmigaOS 2.0+ gadgets.
 * This is a stub implementation that provides basic functionality.
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

#include <graphics/gfx.h>
#include <graphics/rastport.h>
#include <clib/graphics_protos.h>
#include <inline/graphics.h>

#include <libraries/gadtools.h>
#include <intuition/intuition.h>
#include <intuition/gadgetclass.h>
#include <intuition/screens.h>
#include <clib/intuition_protos.h>
#include <inline/intuition.h>
#include <utility/tagitem.h>
#include <clib/utility_protos.h>
#include <inline/utility.h>

#include "util.h"
#include "lxa_images.h"
#include <intuition/imageclass.h>
#include <intuition/sghooks.h>
#include <intuition/classusr.h>

#define VERSION    40
#define REVISION   1
#define EXLIBNAME  "gadtools"
#define EXLIBVER   " 40.1 (2025/02/02)"

char __aligned _g_gadtools_ExLibName [] = EXLIBNAME ".library";
char __aligned _g_gadtools_ExLibID   [] = EXLIBNAME EXLIBVER;
char __aligned _g_gadtools_Copyright [] = "(C)opyright 2025 by G. Bartsch. Licensed under the MIT License.";

char __aligned _g_gadtools_VERSTRING [] = "\0$VER: " EXLIBNAME EXLIBVER;

extern struct ExecBase *SysBase;
extern struct GfxBase  *GfxBase;
extern struct UtilityBase *UtilityBase;
extern struct IntuitionBase *IntuitionBase;

/* GadToolsBase structure */
struct GadToolsBase {
    struct Library lib;
    BPTR           SegList;
};

/* VisualInfo structure - opaque handle for screen visual information */
struct VisualInfo {
    struct Screen *vi_Screen;
    struct DrawInfo *vi_DrawInfo;
};

/* GadgetType bit AmigaOS 3.1 GadTools sets on every gadget it creates
 * (including the context gadget, which has no class bits at all). */
#define LXA_GTYP_GADTOOLS 0x0100

enum gt_kind
{
    GT_KIND_UNKNOWN = 0,
    GT_KIND_BUTTON,
    GT_KIND_STRING,
    GT_KIND_INTEGER,
    GT_KIND_CHECKBOX,
    GT_KIND_SLIDER,
    GT_KIND_CYCLE,
    GT_KIND_MX,
    GT_KIND_SCROLLER,
    GT_KIND_LISTVIEW,
    GT_KIND_PALETTE,
    GT_KIND_TEXT,
    GT_KIND_NUMBER,
    GT_KIND_GENERIC
};

/*
 * A GadTools kind is a group of Intuition gadgets, exactly as AmigaOS 3.1
 * builds it (tests/probes/gadtools/gadgets_*.c): e.g. a slider is a prop
 * gadget followed by a frame gadget carrying the label, a listview is the
 * list area, a scroller (prop + two arrow buttons + frame) and the frame
 * with the label.  CreateGadget() returns the last gadget of the group.
 * All members share one GTGadgetData; `main` is the gadget whose state
 * GadTools keeps (prop, string, list area, ...).
 */
struct GTGadgetData
{
    ULONG kind;
    LONG value;
    LONG min;
    LONG max;
    APTR aux;
    struct IntuiText *level_text;   /* slider level / cycle text (private) */
    STRPTR level_buffer;
    STRPTR format;
    ULONG max_level_len;
    ULONG max_pixel_len;
    UWORD level_place;
    UBYTE justification;
    LONG (*disp_func)(struct Gadget *, LONG);
    LONG underline_pos;     /* label character to underline (-1 if none) */
    ULONG magic;            /* GT_DATA_MAGIC */
    struct VisualInfo *vi;
    struct TextFont *font;  /* ng_TextAttr, opened at creation */
    WORD ng_left, ng_top;   /* the NewGadget box (window coordinates) */
    WORD box_w, box_h;
    STRPTR label;           /* BUTTON_KIND label (drawn by the button) */
    UWORD label_pen;        /* dri pen index of that label */
    WORD label_x, label_y;  /* BUTTON_KIND label position in the box */
    BOOL label_in;
    WORD mx_spacing;
    WORD arrows;            /* GTSC_Arrows / listview arrow height */
    BOOL vertical;
    WORD lv_top;
    BOOL lv_showsel;
    BOOL lv_readonly;
    WORD lv_scroll_w;
    WORD level_x0;          /* slider level field, relative to the box */
    UWORD pal_depth;
    UWORD pal_offset;
    WORD pal_cols, pal_rows;
    BOOL border;            /* GTTX_Border / GTNM_Border */
    /* the group */
    struct Gadget *main;
    struct Gadget *pub;         /* returned by CreateGadget() */
    struct Gadget *label_gad;   /* carries the label IntuiText */
    struct Gadget *arrow_dec, *arrow_inc;
    struct Gadget *lv_prop;
    struct Gadget *lv_string;
    struct Gadget **items;      /* MX buttons */
    WORD nitems;
    WORD refs;
    ULONG drawn_pass;
};

#define GT_DATA_MAGIC 0x47544433UL   /* "GTD3" */

/* member roles */
#define GT_ROLE_CONTEXT    1
#define GT_ROLE_MAIN       2
#define GT_ROLE_FRAME      3    /* frame / label gadget (no class bits) */
#define GT_ROLE_ARROW_DEC  4
#define GT_ROLE_ARROW_INC  5
#define GT_ROLE_LVPROP     6
#define GT_ROLE_LVFRAME    7    /* listview scroller frame */
#define GT_ROLE_MXITEM     8
#define GT_ROLE_CONTAINER  9    /* MX container */

/* Every gadget GadTools allocates: an ExtGadget plus private fields. */
struct GTGad
{
    struct ExtGadget eg;
    ULONG magic;                /* GT_GAD_MAGIC */
    struct GTGadgetData *data;
    UWORD role;
    WORD index;                 /* MX button number */
    APTR render_img;            /* images we allocated */
    APTR select_img;
    BOOL boopsi_imgs;           /* render/select are BOOPSI objects */
    struct Gadget *ctx_last;    /* context gadget: last gadget appended */
    /* what GadTools allocated (the application may replace the fields,
     * e.g. GadgetText or SpecialInfo of a GENERIC_KIND gadget) */
    struct IntuiText *own_text;
    APTR own_special;
    UWORD own_special_type;     /* GTYP_STRGADGET / GTYP_PROPGADGET / 0 */
};

#define GT_GAD_MAGIC 0x47544741UL    /* "GTGA" */

BOOL _gadtools_IsCheckbox(register struct Gadget *gad __asm("a0"));
BOOL _gadtools_GetCheckboxState(register struct Gadget *gad __asm("a0"));
VOID _gadtools_SetCheckboxState(register struct Gadget *gad __asm("a0"),
                                register BOOL checked __asm("d0"));
BOOL _gadtools_IsCycle(register struct Gadget *gad __asm("a0"));
UWORD _gadtools_GetCycleState(register struct Gadget *gad __asm("a0"));
VOID _gadtools_SetCycleState(register struct Gadget *gad __asm("a0"),
                             register UWORD active __asm("d0"));
UWORD _gadtools_AdvanceCycleState(register struct Gadget *gad __asm("a0"));
STRPTR _gadtools_GetCycleLabel(register struct Gadget *gad __asm("a0"));

struct gt_format_state
{
    STRPTR cursor;
    ULONG remaining;
};

static struct GTGad *gt_gad(struct Gadget *gad)
{
    struct GTGad *g = (struct GTGad *)gad;

    if (!gad || !(gad->GadgetType & LXA_GTYP_GADTOOLS) || (gad->GadgetType & GTYP_SYSGADGET))
        return NULL;
    if ((gad->GadgetType & GTYP_GTYPEMASK) == GTYP_CUSTOMGADGET)
        return NULL;
    if (g->magic != GT_GAD_MAGIC)
        return NULL;
    return g;
}

/* The context gadget is a plain gadget of type GTYP_GADTOOLS (0x0100, no
 * gadget class bits), zero size, GFLG_GADGHNONE and SpecialInfo NULL -
 * exactly as AmigaOS 3.1 GadTools creates it. */
static BOOL gt_is_context_gadget(struct Gadget *gad)
{
    struct GTGad *g = gt_gad(gad);

    return g && g->role == GT_ROLE_CONTEXT;
}

static struct Gadget *gt_public_glist(struct Gadget *gad)
{
    if (!gt_is_context_gadget(gad))
        return gad;

    return gad->NextGadget;
}

/*
 * Library init/open/close/expunge
 */

struct GadToolsBase * __g_lxa_gadtools_InitLib ( register struct GadToolsBase *gtb    __asm("d0"),
                                                 register BPTR                seglist __asm("a0"),
                                                 register struct ExecBase    *sysb    __asm("a6"))
{
    DPRINTF (LOG_DEBUG, "_gadtools: InitLib() called\n");
    gtb->SegList = seglist;
    return gtb;
}

BPTR __g_lxa_gadtools_ExpungeLib ( register struct GadToolsBase *gtb __asm("a6") )
{
    return 0;
}

struct GadToolsBase * __g_lxa_gadtools_OpenLib ( register struct GadToolsBase *gtb __asm("a6") )
{
    DPRINTF (LOG_DEBUG, "_gadtools: OpenLib() called, gtb=0x%08lx\n", (ULONG)gtb);
    gtb->lib.lib_OpenCnt++;
    gtb->lib.lib_Flags &= ~LIBF_DELEXP;
    return gtb;
}

BPTR __g_lxa_gadtools_CloseLib ( register struct GadToolsBase *gtb __asm("a6") )
{
    DPRINTF (LOG_DEBUG, "_gadtools: CloseLib() called, gtb=0x%08lx\n", (ULONG)gtb);
    gtb->lib.lib_OpenCnt--;
    return 0;
}

ULONG __g_lxa_gadtools_ExtFuncLib ( void )
{
    PRIVATE_FUNCTION_ERROR("_gadtools", "ExtFuncLib");
    return 0;
}

/*
 * Gadget Functions
 */

/* Bevel box border insets for STRING_KIND/INTEGER_KIND.
 * Per v40 source: LEFTTRIM=4, TOPTRIM=2, BEVELXSIZE=2, BEVELYSIZE=1.
 * The bevel border is drawn outside the text editing area.
 */
#define GT_BEVEL_LEFT   4
#define GT_BEVEL_TOP    2
#define GT_INTERWIDTH   4  /* Horizontal space between gadget and text label */
#define GT_FONT_HEIGHT  8  /* topaz 8 height */
#define GT_FONT_BASELINE 6 /* topaz 8 baseline */
#define CHECKBOX_WIDTH  26 /* Fixed width for unscaled checkbox gadget */
#define CHECKBOX_HEIGHT 11 /* Fixed height for unscaled checkbox gadget */

/* Helper: compute length of a string (we cannot use strlen in ROM code) */
static WORD gt_strlen(CONST_STRPTR s)
{
    WORD len = 0;
    if (!s) return 0;
    while (*s++) len++;
    return len;
}

static struct GTGadgetData *gt_alloc_data(ULONG kind)
{
    struct GTGadgetData *data;

    data = (struct GTGadgetData *)AllocMem(sizeof(struct GTGadgetData), MEMF_CLEAR | MEMF_PUBLIC);
    if (data)
    {
        data->kind = kind;
        data->magic = GT_DATA_MAGIC;
        data->underline_pos = -1;
    }

    return data;
}

static struct IClass *g_gt_palette_class;

struct GTPaletteInst
{
    ULONG magic;
    struct GTGadgetData *data;
    APTR img;
};

static struct GTGadgetData *gt_get_data(struct Gadget *gad)
{
    struct GTGad *g = gt_gad(gad);

    if (g)
        return (g->data && g->data->magic == GT_DATA_MAGIC) ? g->data : NULL;
    /* the palette is a BOOPSI gadget of GadTools' private class */
    if (gad && g_gt_palette_class && (gad->GadgetType & LXA_GTYP_GADTOOLS) &&
        (gad->GadgetType & GTYP_GTYPEMASK) == GTYP_CUSTOMGADGET &&
        OCLASS(gad) == g_gt_palette_class)
    {
        struct GTPaletteInst *pi = (struct GTPaletteInst *)INST_DATA(g_gt_palette_class, gad);
        if (pi->magic == GT_DATA_MAGIC && pi->data && pi->data->magic == GT_DATA_MAGIC)
            return pi->data;
    }
    return NULL;
}

/* the gadget whose state GadTools keeps (prop, string, list area...) */
static struct Gadget *gt_main(struct Gadget *gad)
{
    struct GTGadgetData *data = gt_get_data(gad);
    return (data && data->main) ? data->main : gad;
}

static WORD gt_font_ysize(struct TextFont *font);

/* TRUE for the gadgets GadTools draws itself (Intuition asks before rendering) */
BOOL _gadtools_IsGadTools(register struct Gadget *gad __asm("a0"))
{
    struct GTGad *g = gt_gad(gad);

    if (!g)     /* the palette (a BOOPSI gadget) */
        return gt_get_data(gad) != NULL;
    return (g->role != GT_ROLE_CONTEXT && g->data) ? TRUE : FALSE;
}

/* RefreshGList() brackets a whole list with this, so that a GadTools group
 * (several Intuition gadgets) is drawn once per pass. */
static ULONG g_gt_pass;
static BOOL g_gt_in_pass;

VOID _gadtools_RefreshPass(register LONG begin __asm("d0"))
{
    if (begin)
    {
        g_gt_pass++;
        g_gt_in_pass = TRUE;
    }
    else
        g_gt_in_pass = FALSE;
}

static void gt_update_mx_flags(struct GTGadgetData *data)
{
    WORD i;

    for (i = 0; i < data->nitems; i++)
    {
        if (i == data->value)
            data->items[i]->Flags |= GFLG_SELECTED;
        else
            data->items[i]->Flags &= ~GFLG_SELECTED;
    }
}

static LONG gt_list_count(struct List *list)
{
    struct Node *n;
    LONG k = 0;

    if (!list || list == (struct List *)~0)
        return 0;
    for (n = list->lh_Head; n->ln_Succ; n = n->ln_Succ)
        k++;
    return k;
}

static void gt_scroller_values(LONG top, LONG total, LONG visible, UWORD *pot, UWORD *body);

static WORD gt_lv_visible(struct GTGadgetData *data)
{
    WORD fh = gt_font_ysize(data->font);
    return data->main ? (WORD)(data->main->Height / fh) : 0;
}

static void gt_lv_update_prop(struct GTGadgetData *data)
{
    struct PropInfo *pi;

    if (!data->lv_prop || !(pi = (struct PropInfo *)data->lv_prop->SpecialInfo))
        return;
    gt_scroller_values(data->lv_top, gt_list_count((struct List *)data->aux), gt_lv_visible(data),
                       &pi->VertPot, &pi->VertBody);
}

static void gt_lv_clamp_top(struct GTGadgetData *data)
{
    LONG max = gt_list_count((struct List *)data->aux) - gt_lv_visible(data);

    if (data->lv_top > max)
        data->lv_top = (WORD)max;
    if (data->lv_top < 0)
        data->lv_top = 0;
}

static void gt_scroller_update_prop(struct GTGadgetData *data)
{
    struct PropInfo *pi = data->main ? (struct PropInfo *)data->main->SpecialInfo : NULL;
    UWORD pot, body;

    if (!pi)
        return;
    gt_scroller_values(data->value, data->max, data->min, &pot, &body);
    if (pi->Flags & FREEVERT)
    {
        pi->VertPot = pot;
        pi->VertBody = body;
    }
    else
    {
        pi->HorizPot = pot;
        pi->HorizBody = body;
    }
}

/*
 * Click on an MX, listview, palette gadget or an arrow button (relative to
 * the gadget).  Updates the GadTools state; returns the IntuiMessage code,
 * or -1 when the click selects nothing.
 */
LONG _gadtools_HandleClick(register struct Gadget *gad __asm("a0"),
                           register LONG relx __asm("d0"),
                           register LONG rely __asm("d1"))
{
    struct GTGadgetData *data = gt_get_data(gad);
    struct GTGad *g = gt_gad(gad);
    WORD fh;

    if (!data)
        return -1;
    relx = (LONG)(WORD)relx;
    rely = (LONG)(WORD)rely;
    fh = gt_font_ysize(data->font);

    if (g && (g->role == GT_ROLE_ARROW_DEC || g->role == GT_ROLE_ARROW_INC))
    {
        WORD d = (g->role == GT_ROLE_ARROW_DEC) ? -1 : 1;
        if (data->kind == GT_KIND_LISTVIEW)
        {
            data->lv_top += d;
            gt_lv_clamp_top(data);
            gt_lv_update_prop(data);
            return data->lv_top;
        }
        if (data->kind == GT_KIND_SCROLLER)
        {
            LONG top = data->value + d;
            if (top > data->max - data->min)
                top = data->max - data->min;
            if (top < 0)
                top = 0;
            data->value = top;
            gt_scroller_update_prop(data);
            return top;
        }
        return -1;
    }
    if (g && g->role == GT_ROLE_MXITEM)
    {
        data->value = g->index;
        gt_update_mx_flags(data);
        return g->index;
    }
    if (gad != data->main)
        return -1;

    switch (data->kind)
    {
        case GT_KIND_LISTVIEW:
        {
            struct List *list = (struct List *)data->aux;
            struct Node *n;
            WORD i = (WORD)(rely / fh) + data->lv_top, k = 0;

            if (data->lv_readonly || !list || list == (struct List *)~0)
                return -1;
            for (n = list->lh_Head; n->ln_Succ; n = n->ln_Succ, k++)
                if (k == i)
                {
                    data->value = i;
                    return i;
                }
            return -1;
        }
        case GT_KIND_PALETTE:
        {
            WORD cols = data->pal_cols ? data->pal_cols : 1;
            WORD rows = data->pal_rows ? data->pal_rows : 1;
            WORD bw = (data->box_w - 5) / cols;
            WORD bh = (data->box_h - 5) / rows;
            WORD c = (bw > 0) ? (WORD)((relx - 3) / bw) : 0;
            WORD r = (bh > 0) ? (WORD)((rely - 3) / bh) : 0;
            WORD i;
            if (c < 0) c = 0;
            if (c >= cols) c = cols - 1;
            if (r < 0) r = 0;
            if (r >= rows) r = rows - 1;
            i = r * cols + c;
            if (i < 0 || i >= (1 << data->pal_depth))
                return -1;
            data->value = data->pal_offset + i;
            return data->value;
        }
        default:
            return -1;
    }
}

BOOL _gadtools_IsCheckbox(register struct Gadget *gad __asm("a0"))
{
    struct GTGadgetData *data = gt_get_data(gad);

    return (data && data->kind == GT_KIND_CHECKBOX) ? TRUE : FALSE;
}

BOOL _gadtools_GetCheckboxState(register struct Gadget *gad __asm("a0"))
{
    struct GTGadgetData *data = gt_get_data(gad);

    if (!data || data->kind != GT_KIND_CHECKBOX)
        return FALSE;

    return data->value ? TRUE : FALSE;
}

VOID _gadtools_SetCheckboxState(register struct Gadget *gad __asm("a0"),
                                register BOOL checked __asm("d0"))
{
    struct GTGadgetData *data = gt_get_data(gad);

    if (!data || data->kind != GT_KIND_CHECKBOX)
        return;

    data->value = checked ? TRUE : FALSE;
    if (checked)
        gad->Flags |= GFLG_SELECTED;
    else
        gad->Flags &= ~GFLG_SELECTED;
}

static UWORD gt_cycle_label_count(struct GTGadgetData *data)
{
    STRPTR *labels;
    UWORD count = 0;

    if (!data || data->kind != GT_KIND_CYCLE || !data->aux)
        return 0;

    labels = (STRPTR *)data->aux;
    while (labels[count])
        count++;

    return count;
}

static UWORD gt_cycle_max_label_len(struct GTGadgetData *data)
{
    STRPTR *labels;
    UWORD count;
    UWORD i;
    UWORD max_len = 0;

    if (!data || data->kind != GT_KIND_CYCLE || !data->aux)
        return 0;

    labels = (STRPTR *)data->aux;
    count = gt_cycle_label_count(data);
    for (i = 0; i < count; i++)
    {
        UWORD len = gt_strlen(labels[i]);

        if (len > max_len)
            max_len = len;
    }

    return max_len;
}

static VOID gt_update_cycle_label_display(struct GTGadgetData *data)
{
    STRPTR *labels;
    UWORD count;
    UWORD active;
    STRPTR label;
    UWORD i;
    WORD textWidth;

    if (!data || data->kind != GT_KIND_CYCLE || !data->level_buffer)
        return;

    if (!data->aux)
    {
        data->level_buffer[0] = '\0';
        return;
    }

    labels = (STRPTR *)data->aux;
    count = gt_cycle_label_count(data);
    if (count == 0)
    {
        data->level_buffer[0] = '\0';
        return;
    }

    if (data->value < 0)
        active = 0;
    else if ((ULONG)data->value >= count)
        active = count - 1;
    else
        active = (UWORD)data->value;

    label = labels[active];
    if (!label)
    {
        data->level_buffer[0] = '\0';
        return;
    }

    for (i = 0; label[i] != '\0' && i < data->max_level_len; i++)
        data->level_buffer[i] = label[i];
    data->level_buffer[i] = '\0';

    /* Re-centre the label text in the right sub-region [22, gadWidth-1].
     * Per spec §10: CYCLEGLYPHWIDTH=20, divider occupies x=20..21,
     * so the label area starts at x=22.
     * textWidth uses 8px/char (topaz 8 default).
     * If level_text is available, update its LeftEdge now. */
    if (data->level_text && data->max_pixel_len > 0)
    {
        WORD gadWidth = (WORD)data->max_pixel_len; /* stored as gadget width in cycle */
        WORD rightRegionWidth = gadWidth - 22;
        textWidth = (WORD)(gt_strlen(data->level_buffer) * 8);
        if (rightRegionWidth > 0)
        {
            WORD le = 22 + (rightRegionWidth - textWidth) / 2;
            if (le < 22) le = 22;
            data->level_text->LeftEdge = le;
        }
    }
}

BOOL _gadtools_IsCycle(register struct Gadget *gad __asm("a0"))
{
    struct GTGadgetData *data = gt_get_data(gad);

    return (data && data->kind == GT_KIND_CYCLE) ? TRUE : FALSE;
}

UWORD _gadtools_GetCycleState(register struct Gadget *gad __asm("a0"))
{
    struct GTGadgetData *data = gt_get_data(gad);
    UWORD count;

    if (!data || data->kind != GT_KIND_CYCLE)
        return 0;

    count = gt_cycle_label_count(data);
    if (count == 0)
        return 0;

    if (data->value < 0)
        return 0;

    if ((ULONG)data->value >= count)
        return count - 1;

    return (UWORD)data->value;
}

VOID _gadtools_SetCycleState(register struct Gadget *gad __asm("a0"),
                             register UWORD active __asm("d0"))
{
    struct GTGadgetData *data = gt_get_data(gad);
    UWORD count;

    if (!data || data->kind != GT_KIND_CYCLE)
        return;

    count = gt_cycle_label_count(data);
    if (count == 0)
    {
        data->value = 0;
        return;
    }

    if (active >= count)
        active = count - 1;

    data->value = (LONG)active;
    gt_update_cycle_label_display(data);
}

UWORD _gadtools_AdvanceCycleState(register struct Gadget *gad __asm("a0"))
{
    struct GTGadgetData *data = gt_get_data(gad);
    UWORD count;
    UWORD active;

    if (!data || data->kind != GT_KIND_CYCLE)
        return 0;

    count = gt_cycle_label_count(data);
    if (count == 0)
    {
        data->value = 0;
        return 0;
    }

    active = _gadtools_GetCycleState(gad);
    active++;
    if (active >= count)
        active = 0;

    data->value = (LONG)active;
    gt_update_cycle_label_display(data);
    return active;
}

STRPTR _gadtools_GetCycleLabel(register struct Gadget *gad __asm("a0"))
{
    struct GTGadgetData *data = gt_get_data(gad);
    STRPTR *labels;
    UWORD active;
    UWORD count;

    if (!data || data->kind != GT_KIND_CYCLE || !data->aux)
        return NULL;

    labels = (STRPTR *)data->aux;
    count = gt_cycle_label_count(data);
    if (count == 0)
        return NULL;

    active = _gadtools_GetCycleState(gad);
    return labels[active];
}

static WORD gt_format_long(STRPTR buf, ULONG maxchars, LONG value)
{
    ULONG magnitude;
    WORD i = 0;
    WORD start;
    WORD end;

    if (!buf || maxchars == 0)
        return 0;

    if (maxchars == 1)
    {
        buf[0] = '\0';
        return 0;
    }

    if (value < 0)
    {
        buf[i++] = '-';
        magnitude = (ULONG)(-(value + 1)) + 1;
    }
    else
    {
        magnitude = (ULONG)value;
    }

    if (magnitude == 0)
    {
        if (i < (WORD)maxchars - 1)
            buf[i++] = '0';
    }
    else
    {
        start = i;
        while (magnitude > 0 && i < (WORD)maxchars - 1)
        {
            buf[i++] = '0' + (UBYTE)(magnitude % 10);
            magnitude /= 10;
        }

        end = i - 1;
        while (start < end)
        {
            UBYTE tmp = buf[start];
            buf[start] = buf[end];
            buf[end] = tmp;
            start++;
            end--;
        }
    }

    buf[i] = '\0';
    return i;
}

static void gt_format_char_hook(register UBYTE ch __asm("d0"),
                                register struct gt_format_state *state __asm("a3"))
{
    if (!state || !state->cursor || state->remaining == 0)
        return;

    *(state->cursor)++ = ch;
    state->remaining--;
}

static STRPTR gt_strdup(CONST_STRPTR text)
{
    STRPTR copy;
    WORD len;
    WORD i;

    if (!text)
        return NULL;

    len = gt_strlen(text);
    copy = (STRPTR)AllocMem(len + 1, MEMF_PUBLIC);
    if (!copy)
        return NULL;

    for (i = 0; i < len; i++)
        copy[i] = text[i];
    copy[len] = '\0';

    return copy;
}

static WORD gt_format_slider_level(STRPTR buf, ULONG maxchars,
                                   CONST_STRPTR format, LONG value)
{
    struct gt_format_state state;

    if (!buf || maxchars == 0)
        return 0;

    if (!format)
        format = (CONST_STRPTR)"%ld";

    state.cursor = buf;
    state.remaining = maxchars - 1;

    RawDoFmt(format, &value, (VOID (*)())gt_format_char_hook, &state);
    *state.cursor = '\0';

    return (WORD)(state.cursor - buf);
}


static WORD gt_text_width(struct TextFont *font, CONST_STRPTR s, WORD len);
static WORD gt_font_ysize(struct TextFont *font);

/* place the slider level text against the NewGadget box (AmigaOS 3.1:
 * 8 pixels beside the box, vertically centred), relative to the prop gadget */
static void gt_position_slider_level_text(struct GTGadgetData *data,
                                          struct IntuiText *level_text,
                                          WORD unused_w,
                                          WORD unused_h)
{
    ULONG place;
    WORD text_width;
    WORD text_left;
    WORD fh, W, H;

    if (!data || !level_text)
        return;

    place = data->level_place;
    if (!place)
        place = PLACETEXT_LEFT;
    W = data->box_w;
    H = data->box_h;
    fh = gt_font_ysize(data->font);

    text_width = gt_text_width(data->font, level_text->IText, gt_strlen(level_text->IText));
    if (text_width > (WORD)data->max_pixel_len)
        text_width = (WORD)data->max_pixel_len;

    if (data->justification == GTJ_RIGHT)
        text_left = (WORD)data->max_pixel_len - text_width;
    else if (data->justification == GTJ_CENTER)
        text_left = ((WORD)data->max_pixel_len - text_width) / 2;
    else
        text_left = 0;

    if (text_left < 0)
        text_left = 0;

    if (place & PLACETEXT_LEFT)
    {
        level_text->LeftEdge = -((WORD)data->max_pixel_len) - 8 + text_left;
        level_text->TopEdge  = (H - fh + 1) >> 1;
    }
    else if (place & PLACETEXT_RIGHT)
    {
        level_text->LeftEdge = W + 7 + text_left;
        level_text->TopEdge  = (H - fh + 1) >> 1;
    }
    else if (place & PLACETEXT_ABOVE)
    {
        level_text->LeftEdge = (W - (WORD)data->max_pixel_len) / 2 + text_left;
        level_text->TopEdge  = -fh - 4;
    }
    else
    {
        level_text->LeftEdge = (W - (WORD)data->max_pixel_len) / 2 + text_left;
        level_text->TopEdge  = H + 3;
    }
    data->level_x0 = level_text->LeftEdge - text_left;
}

VOID _gadtools_UpdateSliderLevelDisplay(register struct Gadget *gad __asm("a0"),
                                        register LONG level __asm("d0"))
{
    struct GTGadgetData *data;
    LONG display_level;

    level = (LONG)(WORD)level; /* sign-extend: GCC m68k move.w workaround */

    if (!gad)
        return;

    data = gt_get_data(gad);
    if (!data || data->kind != GT_KIND_SLIDER)
        return;
    data->value = level;
    if (!data->level_text || !data->level_buffer)
        return;

    display_level = data->disp_func ? data->disp_func(gad, level) : level;
    gt_format_slider_level(data->level_buffer, data->max_level_len + 1,
                           data->format, display_level);
    gt_position_slider_level_text(data, data->level_text, gad->Width, gad->Height);
}

static struct TagItem *gt_find_tagitem(ULONG tag_value, struct TagItem *taglist)
{
    struct TagItem *tag;

    tag = taglist;
    while (tag)
    {
        switch (tag->ti_Tag)
        {
            case TAG_DONE:
                return NULL;

            case TAG_IGNORE:
                tag++;
                break;

            case TAG_SKIP:
                tag += tag->ti_Data + 1;
                break;

            case TAG_MORE:
                tag = (struct TagItem *)tag->ti_Data;
                break;

            default:
                if (tag->ti_Tag == tag_value)
                    return tag;
                tag++;
                break;
        }
    }

    return NULL;
}

/* Width of a label in the gadget font, accounting for GT_Underscore (the
 * underscore prefix character is not displayed). */
static WORD gt_text_width(struct TextFont *font, CONST_STRPTR s, WORD len)
{
    struct RastPort rp;

    if (!s || len <= 0)
        return 0;
    InitRastPort(&rp);
    if (font)
        SetFont(&rp, font);
    return TextLength(&rp, (STRPTR)s, len);
}

/* Width of a gadget label as AmigaOS 3.1 GadTools places it (gallery
 * goldens, helvetica 13): the larger of TextLength() and the ink extent
 * (TextExtent() MaxX + 1), which differ for kerned proportional fonts. */
static WORD gt_label_text_width(struct TextFont *font, CONST_STRPTR s, WORD len)
{
    struct RastPort rp;
    struct TextExtent te;
    WORD w;

    if (!s || len <= 0)
        return 0;
    InitRastPort(&rp);
    if (font)
        SetFont(&rp, font);
    w = TextLength(&rp, (STRPTR)s, len);
    TextExtent(&rp, (STRPTR)s, len, &te);
    if (te.te_Extent.MaxX + 1 > w)
        w = (WORD)(te.te_Extent.MaxX + 1);
    return w;
}

static WORD gt_font_ysize(struct TextFont *font)
{
    return font ? font->tf_YSize : GT_FONT_HEIGHT;
}

/* Helper: create a stripped copy of a label string.
 * Removes all occurrences of the underscore prefix character 'us'.
 * Returns allocated string or NULL. Also returns the character index
 * (in the stripped string) of the first underlined character via *ul_pos.
 * *ul_pos is set to -1 if no underscore is found.
 */
static STRPTR gt_strip_underscore(CONST_STRPTR label, UBYTE us, WORD *ul_pos)
{
    STRPTR stripped;
    WORD src, dst, slen;

    *ul_pos = -1;
    if (!label) return NULL;

    slen = gt_strlen(label);
    stripped = (STRPTR)AllocMem(slen + 1, MEMF_PUBLIC);
    if (!stripped) return NULL;

    dst = 0;
    for (src = 0; label[src]; src++)
    {
        if (us && label[src] == us)
        {
            /* Mark next character as underlined (first occurrence only) */
            if (*ul_pos < 0 && label[src + 1])
                *ul_pos = dst;
            /* Skip the underscore prefix itself */
            continue;
        }
        stripped[dst++] = label[src];
    }
    stripped[dst] = '\0';
    return stripped;
}

/*
 * Label placement of AmigaOS 3.1 GadTools (tests/probes/gadtools): 8 pixels
 * left of the box, 7 right of it, fontheight+4 above, 3 below, centred
 * otherwise ((size - textsize + 1) >> 1).
 */
struct GTULText
{
    struct IntuiText it;
    struct TextAttr ta;
};

static ULONG gt_label_place(ULONG flags, ULONG defaultPlace)
{
    ULONG place = flags & (PLACETEXT_LEFT | PLACETEXT_RIGHT | PLACETEXT_ABOVE |
                           PLACETEXT_BELOW | PLACETEXT_IN);
    return place ? place : defaultPlace;
}

static void gt_label_pos(ULONG place, WORD w, WORD h, WORD tw, WORD fh, WORD *x, WORD *y)
{
    if (place & PLACETEXT_LEFT)
    {
        *x = -tw - 8;
        *y = (h - fh + 1) >> 1;
    }
    else if (place & PLACETEXT_RIGHT)
    {
        *x = w + 7;
        *y = (h - fh + 1) >> 1;
    }
    else if (place & PLACETEXT_ABOVE)
    {
        *x = (w - tw + 1) >> 1;
        *y = -fh - 4;
    }
    else if (place & PLACETEXT_BELOW)
    {
        *x = (w - tw + 1) >> 1;
        *y = h + 3;
    }
    else
    {
        *x = (w - tw + 1) >> 1;
        *y = (h - fh + 1) >> 1;
    }
}

/* ExtGadget bounds: the box widened by the label (AmigaOS 3.1) */
static void gt_set_bounds(struct Gadget *gad, ULONG more, WORD x, WORD y, WORD w, WORD h,
                          ULONG place, WORD tw, WORD fh, BOOL has_label)
{
    struct ExtGadget *eg = (struct ExtGadget *)gad;

    if (has_label)
    {
        if (place & PLACETEXT_LEFT)
        {
            x -= tw + 8;
            w += tw + 8;
        }
        else if (place & PLACETEXT_RIGHT)
            w += tw + 8;
        else if (place & PLACETEXT_ABOVE)
        {
            y -= fh + 4;
            h += fh + 4;
        }
        else if (place & PLACETEXT_BELOW)
            h += fh + 4;
        if ((place & (PLACETEXT_LEFT | PLACETEXT_RIGHT)) && h < fh + 1)
            h = fh + 1;
    }
    gad->Flags |= GFLG_EXTENDED;
    eg->MoreFlags = more;
    eg->BoundsLeftEdge = x;
    eg->BoundsTopEdge = y;
    eg->BoundsWidth = w;
    eg->BoundsHeight = h;
}

/*
 * Create the IntuiText label of a gadget, placed against the box
 * (0,0,gadWidth,gadHeight).  The underlined character index (GT_Underscore)
 * is returned in *ul, the text width in *twp.
 */
static struct IntuiText * gt_create_label(CONST_STRPTR text, ULONG flags,
                                           ULONG defaultPlace,
                                           WORD gadWidth, WORD gadHeight,
                                           UBYTE us, struct TextFont *font,
                                           struct TextAttr *ta, WORD *ul)
{
    struct IntuiText *it;
    WORD textWidth, fh;
    STRPTR displayText;
    WORD ul_pos;

    if (ul)
        *ul = -1;
    if (!text) return NULL;

    it = (struct IntuiText *)AllocMem(sizeof(struct IntuiText), MEMF_CLEAR | MEMF_PUBLIC);
    if (!it) return NULL;

    displayText = gt_strip_underscore(text, us, &ul_pos);
    if (!displayText)
    {
        FreeMem(it, sizeof(struct IntuiText));
        return NULL;
    }

    /* TEXTPEN (1) normally, HIGHLIGHTTEXTPEN (2) for NG_HIGHLABEL */
    it->FrontPen  = (flags & NG_HIGHLABEL) ? 2 : 1;
    it->BackPen   = 0;
    it->DrawMode  = JAM1;
    it->ITextFont = ta;
    it->IText     = displayText;
    it->NextText  = NULL;

    textWidth = gt_label_text_width(font, displayText, gt_strlen(displayText));
    fh = gt_font_ysize(font);
    gt_label_pos(gt_label_place(flags, defaultPlace), gadWidth, gadHeight, textWidth, fh,
                 &it->LeftEdge, &it->TopEdge);

    /* GT_Underscore: AmigaOS 3.1 chains a second IntuiText with the
     * underlined character in an underlined copy of the font */
    if (ul_pos >= 0)
    {
        struct GTULText *u = (struct GTULText *)AllocMem(sizeof(struct GTULText), MEMF_CLEAR | MEMF_PUBLIC);
        STRPTR c = (STRPTR)AllocMem(2, MEMF_CLEAR | MEMF_PUBLIC);
        if (u && c)
        {
            c[0] = displayText[ul_pos];
            u->it = *it;
            u->it.IText = c;
            u->it.LeftEdge = it->LeftEdge + gt_text_width(font, displayText, ul_pos);
            if (ta)
            {
                u->ta = *ta;
                u->ta.ta_Style |= FSF_UNDERLINED;
                u->it.ITextFont = &u->ta;
            }
            it->NextText = &u->it;
        }
        else
        {
            if (u)
                FreeMem(u, sizeof(struct GTULText));
            if (c)
                FreeMem(c, 2);
        }
    }

    if (ul)
        *ul = ul_pos;
    return it;
}

static WORD gt_label_width(CONST_STRPTR text, UBYTE us, struct TextFont *font)
{
    WORD ul;
    STRPTR s;
    WORD w;

    if (!text)
        return 0;
    s = gt_strip_underscore(text, us, &ul);
    if (!s)
        return 0;
    w = gt_text_width(font, s, gt_strlen(s));
    FreeMem(s, gt_strlen(s) + 1);
    return w;
}

/* Helper: free an IntuiText label allocated by gt_create_label. */

/* scroller/listview prop values (AmigaOS 3.1 GadTools: one line overlap) */
static void gt_scroller_values(LONG top, LONG total, LONG visible, UWORD *pot, UWORD *body)
{
    if (total > visible && total > 1 && visible > 0)
    {
        *body = (UWORD)(((ULONG)(visible - 1) * 0xFFFFUL) / (ULONG)(total - 1));
        *pot = (UWORD)(((ULONG)top * 0xFFFFUL + (ULONG)(total - visible) / 2) / (ULONG)(total - visible));
    }
    else
    {
        *body = 0xFFFF;
        *pot = 0;
    }
}

/*
 * Gadget group construction.  Geometry, flags and the list layout follow
 * AmigaOS 3.1 GadTools as observed by tests/probes/gadtools/gadgets_*.c.
 */
struct gt_build
{
    struct GTGadgetData *data;
    struct NewGadget *ng;
    struct Gadget *first, *last;
    BOOL failed;
};

static void gt_free_member(struct Gadget *gad);

static struct Gadget *gt_member(struct gt_build *b, UWORD role, UWORD type, UWORD flags, UWORD act,
                                WORD l, WORD t, WORD w, WORD h)
{
    struct GTGad *g;
    struct Gadget *gad;

    if (b->failed)
        return NULL;
    g = (struct GTGad *)AllocMem(sizeof(struct GTGad), MEMF_CLEAR | MEMF_PUBLIC);
    if (!g)
    {
        b->failed = TRUE;
        return NULL;
    }
    gad = (struct Gadget *)&g->eg;
    g->magic = GT_GAD_MAGIC;
    g->data = b->data;
    g->role = role;
    b->data->refs++;
    gad->GadgetType = LXA_GTYP_GADTOOLS | type;
    gad->Flags = flags;
    gad->Activation = act;
    gad->LeftEdge = l;
    gad->TopEdge = t;
    gad->Width = w;
    gad->Height = h;
    gad->GadgetID = b->ng->ng_GadgetID;
    gad->UserData = b->ng->ng_UserData;
    if (b->last)
        b->last->NextGadget = gad;
    else
        b->first = gad;
    b->last = gad;
    return gad;
}

static APTR gt_new_frame(WORD l, WORD t, WORD w, WORD h, ULONG type, BOOL recessed)
{
    struct TagItem tags[7];

    tags[0].ti_Tag = IA_Left;      tags[0].ti_Data = (ULONG)(LONG)l;
    tags[1].ti_Tag = IA_Top;       tags[1].ti_Data = (ULONG)(LONG)t;
    tags[2].ti_Tag = IA_Width;     tags[2].ti_Data = (ULONG)(LONG)w;
    tags[3].ti_Tag = IA_Height;    tags[3].ti_Data = (ULONG)(LONG)h;
    tags[4].ti_Tag = IA_FrameType; tags[4].ti_Data = type;
    tags[5].ti_Tag = IA_Recessed;  tags[5].ti_Data = recessed;
    tags[6].ti_Tag = TAG_DONE;     tags[6].ti_Data = 0;
    return NewObjectA(NULL, (UBYTE *)FRAMEICLASS, tags);
}

/* GadgetRender (and SelectRender) as BOOPSI frame images */
static void gt_images(struct gt_build *b, struct Gadget *gad, WORD l, WORD t, WORD w, WORD h,
                      ULONG type, BOOL recessed, BOOL select)
{
    struct GTGad *g = (struct GTGad *)gad;

    if (!gad)
        return;
    g->boopsi_imgs = TRUE;
    g->render_img = gt_new_frame(l, t, w, h, type, recessed);
    gad->GadgetRender = g->render_img;
    if (select)
    {
        g->select_img = gt_new_frame(l, t, w, h, type, recessed);
        gad->SelectRender = g->select_img;
    }
    if (!g->render_img || (select && !g->select_img))
        b->failed = TRUE;
}

/* the (empty) AUTOKNOB image Intuition fills in */
static void gt_knob_image(struct gt_build *b, struct Gadget *gad)
{
    struct GTGad *g = (struct GTGad *)gad;

    if (!gad)
        return;
    g->render_img = AllocMem(sizeof(struct Image), MEMF_CLEAR | MEMF_PUBLIC);
    gad->GadgetRender = g->render_img;
    if (!g->render_img)
        b->failed = TRUE;
}

static struct PropInfo *gt_new_prop(struct gt_build *b, struct Gadget *gad, BOOL vert)
{
    struct PropInfo *pi;

    if (!gad)
        return NULL;
    pi = (struct PropInfo *)AllocMem(sizeof(struct PropInfo), MEMF_CLEAR | MEMF_PUBLIC);
    if (!pi)
    {
        b->failed = TRUE;
        return NULL;
    }
    pi->Flags = AUTOKNOB | PROPBORDERLESS | (vert ? FREEVERT : FREEHORIZ);
    gad->SpecialInfo = (APTR)pi;
    return pi;
}

/* attach the ng_GadgetText label to `gad`, placed against the box (bx,by,bw,bh),
 * and set the ExtGadget bounds of `gad` (more flags `more`) */
static void gt_label(struct gt_build *b, struct Gadget *gad, ULONG defplace, WORD bx, WORD by,
                     WORD bw, WORD bh, UBYTE us, ULONG more)
{
    struct NewGadget *ng = b->ng;
    struct GTGadgetData *data = b->data;
    ULONG place = gt_label_place(ng->ng_Flags, defplace);
    WORD ul = -1, tw = 0;

    if (!gad)
        return;
    if (ng->ng_GadgetText)
    {
        struct IntuiText *it = gt_create_label(ng->ng_GadgetText, ng->ng_Flags, defplace, bw, bh, us,
                                               data->font, (struct TextAttr *)ng->ng_TextAttr, &ul);
        if (!it)
        {
            b->failed = TRUE;
            return;
        }
        it->LeftEdge += bx - gad->LeftEdge;
        it->TopEdge += by - gad->TopEdge;
        gad->GadgetText = it;
        data->label_gad = gad;
        data->underline_pos = ul;
        tw = gt_label_text_width(data->font, it->IText, gt_strlen(it->IText));
    }
    gt_set_bounds(gad, more, bx, by, bw, bh, place, tw, gt_font_ysize(data->font),
                  ng->ng_GadgetText != NULL);
}

static struct Gadget *gt_palette_new(struct gt_build *b, WORD l, WORD t, WORD w, WORD h);

/* CreateGadgetA - Create a GadTools gadget
 * kind: gadget kind (BUTTON_KIND, STRING_KIND, etc.)
 * gad: previous gadget in list (or context gadget)
 * ng: NewGadget structure
 * taglist: additional tags
 */
struct Gadget * _gadtools_CreateGadgetA ( register struct GadToolsBase *GadToolsBase __asm("a6"),
                                          register ULONG kind __asm("d0"),
                                          register struct Gadget *gad __asm("a0"),
                                          register struct NewGadget *ng __asm("a1"),
                                          register struct TagItem *taglist __asm("a2") )
{
    struct gt_build b;
    struct GTGadgetData *data;
    struct Gadget *mg = NULL, *pub = NULL, *x;
    UBYTE us;
    struct TextFont *font = NULL;
    WORD fh, L, T, W, H;
    UWORD dis;

    DPRINTF (LOG_DEBUG, "_gadtools: CreateGadgetA() kind=%ld, prevgad=0x%08lx, ng=0x%08lx\n",
             kind, (ULONG)gad, (ULONG)ng);

    /* a failed CreateGadget() earlier in the chain propagates (autodoc) */
    if (!ng || !gad)
        return NULL;

    /* An MX gadget without labels cannot be created (AmigaOS 3.1). */
    if (kind == MX_KIND && !GetTagData(GTMX_Labels, 0, taglist))
        return NULL;

    us = (UBYTE)GetTagData(GT_Underscore, 0, taglist);

    data = gt_alloc_data(GT_KIND_UNKNOWN);
    if (!data)
        return NULL;
    data->vi = (struct VisualInfo *)ng->ng_VisualInfo;
    if (ng->ng_TextAttr)
        font = OpenFont(ng->ng_TextAttr);
    data->font = font;
    fh = gt_font_ysize(font);
    L = ng->ng_LeftEdge;
    T = ng->ng_TopEdge;
    W = ng->ng_Width;
    H = ng->ng_Height;
    data->ng_left = L;
    data->ng_top = T;
    data->box_w = W;
    data->box_h = H;
    dis = GetTagData(GA_Disabled, FALSE, taglist) ? GFLG_DISABLED : 0;

    b.data = data;
    b.ng = ng;
    b.first = b.last = NULL;
    b.failed = FALSE;

    switch (kind) {
        case BUTTON_KIND:
        {
            WORD ul_pos, tw;
            ULONG place = gt_label_place(ng->ng_Flags, PLACETEXT_IN);

            data->kind = GT_KIND_BUTTON;
            mg = gt_member(&b, GT_ROLE_MAIN, GTYP_BOOLGADGET,
                             GFLG_EXTENDED | GFLG_GADGIMAGE | GFLG_GADGHIMAGE | dis,
                             GACT_RELVERIFY | (GetTagData(GA_Immediate, FALSE, taglist) ? GACT_IMMEDIATE : 0),
                             L, T, W, H);
            gt_images(&b, mg, 0, 0, W, H, FRAME_BUTTON, FALSE, TRUE);
            /* the label is part of the button imagery (no GadgetText) */
            data->label = gt_strip_underscore(ng->ng_GadgetText, us, &ul_pos);
            data->label_pen = (ng->ng_Flags & NG_HIGHLABEL) ? HIGHLIGHTTEXTPEN : TEXTPEN;
            data->underline_pos = ul_pos;
            tw = data->label ? gt_label_text_width(font, data->label, gt_strlen(data->label)) : 0;
            gt_label_pos(place, W, H, tw, fh, &data->label_x, &data->label_y);
            data->label_in = (place & (PLACETEXT_LEFT | PLACETEXT_RIGHT | PLACETEXT_ABOVE | PLACETEXT_BELOW)) == 0;
            if (mg)
                gt_set_bounds(mg, GMORE_BOUNDS | GMORE_GADGETHELP, L, T, W, H, place, tw, fh,
                              data->label != NULL);
            break;
        }

        case STRING_KIND:
        case INTEGER_KIND:
        {
            struct StringInfo *si = NULL;
            struct StringExtend *se;
            ULONG maxchars;
            WORD dy = (H - fh + 1) >> 1;
            UWORD act = GACT_RELVERIFY | GACT_STRINGEXTEND;
            ULONG just = GetTagData(STRINGA_Justification, GACT_STRINGLEFT, taglist);

            data->kind = (kind == INTEGER_KIND) ? GT_KIND_INTEGER : GT_KIND_STRING;
            if (kind == INTEGER_KIND)
                act |= GACT_LONGINT;
            act |= just & (GACT_STRINGCENTER | GACT_STRINGRIGHT);
            mg = gt_member(&b, GT_ROLE_MAIN, GTYP_STRGADGET,
                             GFLG_EXTENDED | GFLG_GADGIMAGE | dis |
                             (GetTagData(GA_TabCycle, TRUE, taglist) ? GFLG_TABCYCLE : 0),
                             act, L + 6, T + dy, W - 12, fh);
            /* the ridge frame around the string area */
            gt_images(&b, mg, -6, -dy, W, H, FRAME_RIDGE, FALSE, FALSE);
            gt_label(&b, mg, PLACETEXT_LEFT, L, T, W, H, us, GMORE_BOUNDS | GMORE_GADGETHELP);

            if (kind == INTEGER_KIND)
                maxchars = GetTagData(GTIN_MaxChars, 10, taglist) + 1;
            else
                maxchars = GetTagData(GTST_MaxChars, 64, taglist) + 1;
            if (mg && !b.failed)
                si = (struct StringInfo *)AllocMem(sizeof(struct StringInfo), MEMF_CLEAR | MEMF_PUBLIC);
            if (!si)
            {
                b.failed = TRUE;
                break;
            }
            mg->SpecialInfo = (APTR)si;
            /* one spare byte: 3.1 copies up to MaxChars characters */
            si->Buffer = (STRPTR)AllocMem(maxchars + 1, MEMF_CLEAR | MEMF_PUBLIC);
            si->UndoBuffer = (STRPTR)AllocMem(maxchars + 1, MEMF_CLEAR | MEMF_PUBLIC);
            se = (struct StringExtend *)AllocMem(sizeof(struct StringExtend), MEMF_CLEAR | MEMF_PUBLIC);
            si->Extension = se;
            si->MaxChars = (WORD)maxchars;
            if (!si->Buffer || !si->UndoBuffer || !se)
            {
                b.failed = TRUE;
                break;
            }
            se->Font = font;
            se->Pens[0] = 1;
            se->Pens[1] = 0;
            se->ActivePens[0] = 1;
            se->ActivePens[1] = 0;

            if (kind == INTEGER_KIND)
            {
                LONG num = (LONG)GetTagData(GTIN_Number, 0, taglist);
                data->value = num;
                si->LongInt = num;
                gt_format_long(si->Buffer, maxchars, num);
            }
            else
            {
                STRPTR initstr = (STRPTR)GetTagData(GTST_String, 0, taglist);
                WORD len = 0;
                if (initstr)
                    while (initstr[len] != '\0' && len < (WORD)maxchars)
                    {
                        si->Buffer[len] = initstr[len];
                        len++;
                    }
                si->Buffer[len] = '\0';
            }
            break;
        }

        case CHECKBOX_KIND:
        {
            BOOL scaled = (BOOL)GetTagData(GTCB_Scaled, FALSE, taglist);
            WORD cw = scaled ? W : CHECKBOX_WIDTH, ch = scaled ? H : CHECKBOX_HEIGHT, ct = T;

            data->kind = GT_KIND_CHECKBOX;
            /* with a left/right label the box is centred on taller fonts */
            if (!scaled && ng->ng_GadgetText &&
                !(ng->ng_Flags & (PLACETEXT_ABOVE | PLACETEXT_BELOW | PLACETEXT_IN)) && fh > 7)
                ct += (fh - 7) / 2;
            data->value = GetTagData(GTCB_Checked, FALSE, taglist) ? TRUE : FALSE;
            data->ng_top = ct;
            data->box_w = cw;
            data->box_h = ch;
            mg = gt_member(&b, GT_ROLE_MAIN, GTYP_BOOLGADGET,
                             GFLG_EXTENDED | GFLG_GADGIMAGE | GFLG_GADGHIMAGE | dis |
                             (data->value ? GFLG_SELECTED : 0),
                             GACT_RELVERIFY | GACT_TOGGLESELECT, L, ct, cw, ch);
            gt_images(&b, mg, 0, 0, cw, ch, FRAME_BUTTON, FALSE, TRUE);
            gt_label(&b, mg, PLACETEXT_LEFT, L, ct, cw, ch, us, GMORE_BOUNDS | GMORE_GADGETHELP);
            break;
        }

        case CYCLE_KIND:
        {
            struct IntuiText *cycle_text;
            UWORD max_label_len;

            data->kind = GT_KIND_CYCLE;
            data->aux = (APTR)GetTagData(GTCY_Labels, 0, taglist);
            data->value = (LONG)GetTagData(GTCY_Active, 0, taglist);
            data->max_pixel_len = (ULONG)W;
            mg = gt_member(&b, GT_ROLE_MAIN, GTYP_BOOLGADGET,
                             GFLG_EXTENDED | GFLG_GADGIMAGE | GFLG_GADGHIMAGE | dis,
                             GACT_RELVERIFY, L, T, W, H);
            gt_images(&b, mg, 0, 0, W, H, FRAME_BUTTON, FALSE, TRUE);
            gt_label(&b, mg, PLACETEXT_LEFT, L, T, W, H, us, GMORE_BOUNDS | GMORE_GADGETHELP);

            max_label_len = gt_cycle_max_label_len(data);
            if (max_label_len == 0)
                max_label_len = 1;
            data->max_level_len = max_label_len;
            data->level_buffer = (STRPTR)AllocMem(max_label_len + 1, MEMF_CLEAR | MEMF_PUBLIC);
            cycle_text = (struct IntuiText *)AllocMem(sizeof(struct IntuiText), MEMF_CLEAR | MEMF_PUBLIC);
            data->level_text = cycle_text;
            if (!data->level_buffer || !cycle_text || !mg)
            {
                b.failed = TRUE;
                break;
            }
            cycle_text->FrontPen = 1;
            cycle_text->DrawMode = JAM1;
            cycle_text->ITextFont = (struct TextAttr *)ng->ng_TextAttr;
            cycle_text->IText = data->level_buffer;
            cycle_text->TopEdge = (H - fh + 1) / 2;
            data->main = mg;
            _gadtools_SetCycleState(mg, (UWORD)data->value);
            break;
        }

        case MX_KIND:
        {
            STRPTR *labels = (STRPTR *)GetTagData(GTMX_Labels, 0, taglist);
            BOOL scaled = (BOOL)GetTagData(GTMX_Scaled, FALSE, taglist);
            ULONG place = gt_label_place(ng->ng_Flags, PLACETEXT_LEFT);
            ULONG tplace = GetTagData(GTMX_TitlePlace, 0, taglist);
            WORD n = 0, i, maxw = 0, iw = 17, ih = 9, top, cl, cw, chh;
            UWORD siw = 17, sih = 9;

            lxa_sysi_dims(MXIMAGE, SYSISIZE_MEDRES, &siw, &sih);
            iw = siw;
            ih = sih;
            if (scaled)
            {
                iw = W;
                ih = H;
            }
            data->kind = GT_KIND_MX;
            data->aux = (APTR)labels;
            data->value = (LONG)GetTagData(GTMX_Active, 0, taglist);
            data->mx_spacing = (WORD)GetTagData(GTMX_Spacing, 1, taglist);
            while (labels[n])
            {
                WORD tw = gt_label_width(labels[n], us, font);
                if (tw > maxw)
                    maxw = tw;
                n++;
            }
            data->max = n;
            data->nitems = n;
            data->box_w = iw;
            data->box_h = ih;
            data->items = n ? (struct Gadget **)AllocMem(n * sizeof(struct Gadget *), MEMF_CLEAR | MEMF_PUBLIC)
                            : NULL;
            if (n && !data->items)
            {
                b.failed = TRUE;
                break;
            }
            top = T + ((!scaled && fh > 7) ? (fh - 7) / 2 : 0);
            data->ng_top = top;
            for (i = 0; i < n; i++)
            {
                struct GTGad *g;
                struct Gadget *it = gt_member(&b, GT_ROLE_MXITEM, GTYP_BOOLGADGET,
                                              GFLG_GADGIMAGE | GFLG_GADGHIMAGE | dis |
                                              (i == data->value ? GFLG_SELECTED : 0),
                                              GACT_IMMEDIATE, L, top + i * (fh + data->mx_spacing), iw, ih);
                if (!it)
                    break;
                g = (struct GTGad *)it;
                g->index = i;
                it->GadgetID = i;
                data->items[i] = it;
                gt_images(&b, it, 0, 0, iw, ih, FRAME_BUTTON, FALSE, TRUE);
                it->GadgetText = gt_create_label(labels[i], ng->ng_Flags & ~NG_HIGHLABEL, PLACETEXT_LEFT,
                                                 iw, ih, us, font, (struct TextAttr *)ng->ng_TextAttr, NULL);
                if (!it->GadgetText)
                    b.failed = TRUE;
            }
            /* the container: items and their labels */
            cl = L;
            cw = iw;
            if (place & (PLACETEXT_LEFT | PLACETEXT_RIGHT))
            {
                cw += maxw + 8;
                if (place & PLACETEXT_LEFT)
                    cl -= maxw + 8;
            }
            chh = (n > 0 ? (n - 1) * (fh + data->mx_spacing) : 0) + ((ih > fh + 2) ? ih : fh + 2);
            pub = gt_member(&b, GT_ROLE_CONTAINER, 0, GFLG_EXTENDED | GFLG_GADGHNONE, 0, cl, T, cw, chh);
            if (pub && tplace && ng->ng_GadgetText)
            {
                struct NewGadget tng = *ng;
                tng.ng_Flags = (ng->ng_Flags & NG_HIGHLABEL) | tplace;
                b.ng = &tng;
                gt_label(&b, pub, tplace, cl, T, cw, chh, us, GMORE_BOUNDS | GMORE_GADGETHELP);
                b.ng = ng;
            }
            else if (pub)
                gt_set_bounds(pub, GMORE_BOUNDS | GMORE_GADGETHELP, cl, T, cw, chh, 0, 0, fh, FALSE);
            mg = data->items ? data->items[0] : pub;
            break;
        }

        case SLIDER_KIND:
        case SCROLLER_KIND:
        {
            struct PropInfo *pi;
            BOOL vert = (GetTagData(PGA_Freedom, LORIENT_HORIZ, taglist) == LORIENT_VERT);
            WORD a = 0, fw = W, fhh = H;

            data->vertical = vert;
            if (kind == SCROLLER_KIND)
            {
                a = (WORD)GetTagData(GTSC_Arrows, 0, taglist);
                data->arrows = a;
                if (vert)
                    fhh = H - 2 * a;
                else
                    fw = W - 2 * a;
            }
            mg = gt_member(&b, GT_ROLE_MAIN, GTYP_PROPGADGET, GFLG_GADGHNONE | GFLG_GADGIMAGE | dis,
                             GACT_RELVERIFY | GACT_IMMEDIATE | GACT_FOLLOWMOUSE, L + 4, T + 2, fw - 8, fhh - 4);
            gt_knob_image(&b, mg);
            pi = gt_new_prop(&b, mg, vert);
            if (!pi)
                break;

            if (kind == SLIDER_KIND)
            {
                LONG sl_min = (LONG)GetTagData(GTSL_Min, 0, taglist);
                LONG sl_max = (LONG)GetTagData(GTSL_Max, 15, taglist);
                LONG sl_level = (LONG)GetTagData(GTSL_Level, 0, taglist);
                UWORD max_level_len;
                UWORD pot = 0, body = 0xFFFF;

                data->kind = GT_KIND_SLIDER;
                if (sl_level < sl_min) sl_level = sl_min;
                if (sl_level > sl_max) sl_level = sl_max;
                data->min = sl_min;
                data->max = sl_max;
                data->value = sl_level;
                if (sl_max > sl_min)
                {
                    pot = (UWORD)(((ULONG)(vert ? sl_max - sl_level : sl_level - sl_min) * 0xFFFFUL) /
                                  (ULONG)(sl_max - sl_min));
                    body = (UWORD)(0xFFFFUL / (ULONG)(sl_max - sl_min + 1));
                }
                if (vert)
                {
                    pi->VertPot = pot;
                    pi->VertBody = body;
                }
                else
                {
                    pi->HorizPot = pot;
                    pi->HorizBody = body;
                }
                /* min/max for Intuition's level computation */
                pi->CWidth = (UWORD)sl_min;
                pi->CHeight = (UWORD)sl_max;

                data->format = gt_strdup((STRPTR)GetTagData(GTSL_LevelFormat, (ULONG)"%ld", taglist));
                max_level_len = (UWORD)GetTagData(GTSL_MaxLevelLen, 2, taglist);
                if (max_level_len == 0)
                    max_level_len = 2;
                data->max_level_len = max_level_len;
                data->level_place = (UWORD)GetTagData(GTSL_LevelPlace, PLACETEXT_LEFT, taglist);
                data->justification = (UBYTE)GetTagData(GTSL_Justification, GTJ_LEFT, taglist);
                data->max_pixel_len = GetTagData(GTSL_MaxPixelLen,
                                                 (ULONG)(max_level_len * gt_text_width(font, (CONST_STRPTR)"0", 1)),
                                                 taglist);
                data->disp_func = (LONG (*)(struct Gadget *, LONG))GetTagData(GTSL_DispFunc, 0, taglist);
                data->level_buffer = (STRPTR)AllocMem(max_level_len + 1, MEMF_CLEAR | MEMF_PUBLIC);
                if (!data->format || !data->level_buffer)
                {
                    b.failed = TRUE;
                    break;
                }
                /* AmigaOS 3.1 shows the level only when GTSL_LevelFormat is given */
                if (gt_find_tagitem(GTSL_LevelFormat, taglist))
                {
                    struct IntuiText *lt = (struct IntuiText *)AllocMem(sizeof(struct IntuiText),
                                                                        MEMF_CLEAR | MEMF_PUBLIC);
                    if (!lt)
                    {
                        b.failed = TRUE;
                        break;
                    }
                    lt->FrontPen = 1;
                    lt->DrawMode = JAM1;
                    lt->ITextFont = (struct TextAttr *)ng->ng_TextAttr;
                    lt->IText = data->level_buffer;
                    data->level_text = lt;
                }
            }
            else
            {
                LONG sc_top = (LONG)GetTagData(GTSC_Top, 0, taglist);
                LONG sc_total = (LONG)GetTagData(GTSC_Total, 0, taglist);
                LONG sc_visible = (LONG)GetTagData(GTSC_Visible, 2, taglist);

                data->kind = GT_KIND_SCROLLER;
                if (sc_total > sc_visible)
                {
                    if (sc_top > sc_total - sc_visible)
                        sc_top = sc_total - sc_visible;
                }
                else
                    sc_top = 0;
                if (sc_top < 0)
                    sc_top = 0;
                data->value = sc_top;
                data->min = sc_visible;   /* Visible */
                data->max = sc_total;     /* Total */
                data->main = mg;
                gt_scroller_update_prop(data);
                if (a > 0)
                {
                    if (vert)
                    {
                        data->arrow_dec = gt_member(&b, GT_ROLE_ARROW_DEC, GTYP_BOOLGADGET,
                                                    GFLG_EXTENDED | GFLG_GADGIMAGE | GFLG_GADGHIMAGE,
                                                    GACT_RELVERIFY | GACT_IMMEDIATE, L, T + H - 2 * a, W, a);
                        gt_images(&b, data->arrow_dec, 0, 0, W, a, FRAME_BUTTON, FALSE, TRUE);
                        data->arrow_inc = gt_member(&b, GT_ROLE_ARROW_INC, GTYP_BOOLGADGET,
                                                    GFLG_EXTENDED | GFLG_GADGIMAGE | GFLG_GADGHIMAGE,
                                                    GACT_RELVERIFY | GACT_IMMEDIATE, L, T + H - a, W, a);
                        gt_images(&b, data->arrow_inc, 0, 0, W, a, FRAME_BUTTON, FALSE, TRUE);
                    }
                    else
                    {
                        data->arrow_dec = gt_member(&b, GT_ROLE_ARROW_DEC, GTYP_BOOLGADGET,
                                                    GFLG_EXTENDED | GFLG_GADGIMAGE | GFLG_GADGHIMAGE,
                                                    GACT_RELVERIFY | GACT_IMMEDIATE, L + W - 2 * a, T, a, H);
                        gt_images(&b, data->arrow_dec, 0, 0, a, H, FRAME_BUTTON, FALSE, TRUE);
                        data->arrow_inc = gt_member(&b, GT_ROLE_ARROW_INC, GTYP_BOOLGADGET,
                                                    GFLG_EXTENDED | GFLG_GADGIMAGE | GFLG_GADGHIMAGE,
                                                    GACT_RELVERIFY | GACT_IMMEDIATE, L + W - a, T, a, H);
                        gt_images(&b, data->arrow_inc, 0, 0, a, H, FRAME_BUTTON, FALSE, TRUE);
                    }
                    if (data->arrow_dec)
                        gt_set_bounds(data->arrow_dec, GMORE_BOUNDS, data->arrow_dec->LeftEdge,
                                      data->arrow_dec->TopEdge, data->arrow_dec->Width,
                                      data->arrow_dec->Height, 0, 0, fh, FALSE);
                    if (data->arrow_inc)
                        gt_set_bounds(data->arrow_inc, GMORE_BOUNDS, data->arrow_inc->LeftEdge,
                                      data->arrow_inc->TopEdge, data->arrow_inc->Width,
                                      data->arrow_inc->Height, 0, 0, fh, FALSE);
                }
            }
            /* the frame gadget with the label */
            pub = gt_member(&b, GT_ROLE_FRAME, 0, GFLG_EXTENDED | GFLG_GADGHNONE | GFLG_GADGIMAGE, 0,
                            L, T, W, H);
            gt_images(&b, pub, 0, 0, fw, fhh, FRAME_BUTTON, FALSE, FALSE);
            gt_label(&b, pub, PLACETEXT_LEFT, L, T, W, H, us, GMORE_BOUNDS | GMORE_GADGETHELP);
            /* the slider level beside the box widens the bounds */
            if (pub && data->level_text && (data->level_place & PLACETEXT_RIGHT))
                ((struct ExtGadget *)pub)->BoundsWidth += (WORD)data->max_pixel_len;
            break;
        }

        case LISTVIEW_KIND:
        {
            struct Gadget *sg = (struct Gadget *)GetTagData(GTLV_ShowSelected, (ULONG)~0, taglist);
            struct GTGadgetData *sdata = NULL;
            struct PropInfo *pi;
            WORD sw = (WORD)GetTagData(GTLV_ScrollWidth, 16, taglist);
            WORD avail = H, frameH, ah;
            struct Gadget *sframe;

            data->kind = GT_KIND_LISTVIEW;
            data->aux = (APTR)GetTagData(GTLV_Labels, 0, taglist);
            data->lv_top = (WORD)GetTagData(GTLV_Top, 0, taglist);
            data->value = (LONG)GetTagData(GTLV_Selected, (ULONG)~0, taglist);
            data->lv_showsel = (sg != (struct Gadget *)~0);
            data->lv_readonly = GetTagData(GTLV_ReadOnly, FALSE, taglist) ? TRUE : FALSE;
            if (data->lv_readonly)
                data->lv_showsel = FALSE;
            if (sg == (struct Gadget *)~0)
                sg = NULL;
            if (sg && (sdata = gt_get_data(sg)) && sdata->kind == GT_KIND_STRING)
                avail = H - sdata->box_h - 2;
            else
                sg = NULL;
            frameH = ((avail - 4) / fh) * fh + 4;
            ah = (frameH / 4 < fh) ? frameH / 4 : fh;
            data->arrows = ah;
            data->lv_scroll_w = sw;
            data->box_h = frameH;

            mg = gt_member(&b, GT_ROLE_MAIN, GTYP_BOOLGADGET, GFLG_GADGHNONE | GFLG_GADGIMAGE | dis,
                             GACT_RELVERIFY | GACT_IMMEDIATE | GACT_FOLLOWMOUSE,
                             L + 2, T + 2, W - sw - 4, frameH - 4);
            data->main = mg;
            data->lv_prop = gt_member(&b, GT_ROLE_LVPROP, GTYP_PROPGADGET, GFLG_GADGHNONE | GFLG_GADGIMAGE | dis,
                                      GACT_RELVERIFY | GACT_IMMEDIATE | GACT_FOLLOWMOUSE,
                                      L + W - sw + 4, T + 2, sw - 8, frameH - 2 * ah - 4);
            gt_knob_image(&b, data->lv_prop);
            pi = gt_new_prop(&b, data->lv_prop, TRUE);
            if (pi && mg)
            {
                gt_lv_clamp_top(data);
                gt_lv_update_prop(data);
            }
            data->arrow_dec = gt_member(&b, GT_ROLE_ARROW_DEC, GTYP_BOOLGADGET,
                                        GFLG_EXTENDED | GFLG_GADGIMAGE | GFLG_GADGHIMAGE | dis,
                                        GACT_RELVERIFY | GACT_IMMEDIATE, L + W - sw, T + frameH - 2 * ah, sw, ah);
            gt_images(&b, data->arrow_dec, 0, 0, sw, ah, FRAME_BUTTON, FALSE, TRUE);
            data->arrow_inc = gt_member(&b, GT_ROLE_ARROW_INC, GTYP_BOOLGADGET,
                                        GFLG_EXTENDED | GFLG_GADGIMAGE | GFLG_GADGHIMAGE | dis,
                                        GACT_RELVERIFY | GACT_IMMEDIATE, L + W - sw, T + frameH - ah, sw, ah);
            gt_images(&b, data->arrow_inc, 0, 0, sw, ah, FRAME_BUTTON, FALSE, TRUE);
            sframe = gt_member(&b, GT_ROLE_LVFRAME, 0, GFLG_EXTENDED | GFLG_GADGHNONE | GFLG_GADGIMAGE, 0,
                               L + W - sw, T, sw, frameH);
            gt_images(&b, sframe, 0, 0, sw, frameH - 2 * ah, FRAME_BUTTON, FALSE, FALSE);
            for (x = data->arrow_dec; x && x != sframe->NextGadget; x = x->NextGadget)
                gt_set_bounds(x, GMORE_BOUNDS, x->LeftEdge, x->TopEdge, x->Width, x->Height, 0, 0, fh, FALSE);
            pub = gt_member(&b, GT_ROLE_FRAME, 0, GFLG_EXTENDED | GFLG_GADGHNONE | GFLG_GADGIMAGE, 0,
                            L, T, W, frameH);
            gt_images(&b, pub, 0, 0, W - sw, frameH, FRAME_BUTTON, data->lv_readonly, FALSE);
            gt_label(&b, pub, PLACETEXT_ABOVE, L, T, W, frameH, us, GMORE_BOUNDS | GMORE_GADGETHELP);
            if (sg && pub && !b.failed)
            {
                /* the selection is shown in the string gadget below the list */
                struct GTGad *sgg = (struct GTGad *)sg;
                struct Image *simg = (struct Image *)sg->GadgetRender;
                WORD dy = simg ? -simg->TopEdge : 0;

                sg->LeftEdge = L + 6;
                sg->TopEdge = T + frameH + 3;
                sg->Width = W - 12;
                sdata->ng_left = L;
                sdata->ng_top = sg->TopEdge - dy;
                sdata->box_w = W;
                ((struct ExtGadget *)sg)->MoreFlags &= ~GMORE_GADGETHELP;
                ((struct ExtGadget *)pub)->BoundsHeight += sdata->box_h;
                data->lv_string = sg;
                (void)sgg;
            }
            break;
        }

        case PALETTE_KIND:
        {
            WORD n, cols, rows, bestc = 1, bestm = -1;
            ULONG defplace = GetTagData(GTPA_IndicatorWidth, 0, taglist) ? PLACETEXT_LEFT : PLACETEXT_ABOVE;

            data->kind = GT_KIND_PALETTE;
            data->pal_depth = (UWORD)GetTagData(GTPA_Depth, 1, taglist);
            data->pal_offset = (UWORD)GetTagData(GTPA_ColorOffset, 0, taglist);
            data->value = (LONG)GetTagData(GTPA_Color, 1, taglist);
            n = 1 << data->pal_depth;
            /* colour boxes in the grid whose boxes are the most square */
            for (cols = 1; cols <= n; cols <<= 1)
            {
                WORD bw = (W - 5) / cols, bh = (H - 5) / (n / cols);
                WORD m = bw < bh ? bw : bh;
                if (m > bestm)
                {
                    bestm = m;
                    bestc = cols;
                }
            }
            cols = bestc;
            rows = n / cols;
            data->pal_cols = cols;
            data->pal_rows = rows;
            data->box_w = ((W - 5) / cols) * cols + 5;
            data->box_h = ((H - 5) / rows) * rows + 5;
            mg = gt_palette_new(&b, L, T, data->box_w, data->box_h);
            if (mg)
            {
                mg->Flags |= dis;
                gt_label(&b, mg, defplace, L, T, data->box_w, data->box_h, us,
                         GMORE_BOUNDS | GMORE_GADGETHELP);
            }
            break;
        }

        case TEXT_KIND:
        case NUMBER_KIND:
            mg = gt_member(&b, GT_ROLE_MAIN, 0, GFLG_EXTENDED | GFLG_GADGHNONE | GFLG_GADGIMAGE, 0,
                             L, T, W, H);
            if (kind == TEXT_KIND)
            {
                data->kind = GT_KIND_TEXT;
                data->border = GetTagData(GTTX_Border, FALSE, taglist) ? TRUE : FALSE;
                data->level_buffer = gt_strdup((STRPTR)GetTagData(GTTX_Text, (ULONG)"", taglist));
                if (!data->level_buffer)
                    b.failed = TRUE;
            }
            else
            {
                data->kind = GT_KIND_NUMBER;
                data->border = GetTagData(GTNM_Border, FALSE, taglist) ? TRUE : FALSE;
                data->value = (LONG)GetTagData(GTNM_Number, 0, taglist);
                data->format = gt_strdup((STRPTR)GetTagData(GTNM_Format, (ULONG)"%ld", taglist));
                data->level_buffer = (STRPTR)AllocMem(32, MEMF_CLEAR | MEMF_PUBLIC);
                if (!data->level_buffer || !data->format)
                    b.failed = TRUE;
                else
                    gt_format_slider_level(data->level_buffer, 32, data->format, data->value);
            }
            if (data->border)
                gt_images(&b, mg, 0, 0, W, H, FRAME_BUTTON, TRUE, FALSE);
            gt_label(&b, mg, PLACETEXT_LEFT, L, T, W, H, us, GMORE_BOUNDS | GMORE_GADGETHELP);
            break;

        default:    /* GENERIC_KIND: a plain gadget the application fills in */
            data->kind = GT_KIND_GENERIC;
            mg = gt_member(&b, GT_ROLE_MAIN, 0, GFLG_EXTENDED, 0, L, T, W, H);
            if (mg)
            {
                if (ng->ng_GadgetText)
                {
                    struct IntuiText *it = (struct IntuiText *)AllocMem(sizeof(struct IntuiText),
                                                                        MEMF_CLEAR | MEMF_PUBLIC);
                    if (it)
                    {
                        it->FrontPen = 1;
                        it->DrawMode = JAM1;
                        it->ITextFont = (struct TextAttr *)ng->ng_TextAttr;
                        it->IText = gt_strdup(ng->ng_GadgetText);
                        mg->GadgetText = it;
                        data->label_gad = mg;
                    }
                }
                gt_set_bounds(mg, GMORE_BOUNDS | GMORE_GADGETHELP, L, T, W, H, 0, 0, fh, FALSE);
            }
            break;
    }

    if (!pub)
        pub = mg;
    if (b.failed || !pub)
    {
        struct Gadget *next;
        if (!b.first && kind == PALETTE_KIND && mg)
            b.first = mg;
        for (x = b.first; x; x = next)
        {
            next = x->NextGadget;
            gt_free_member(x);
        }
        if (!b.first)
        {
            if (font)
                CloseFont(font);
            FreeMem(data, sizeof(struct GTGadgetData));
        }
        return NULL;
    }
    data->main = mg;
    data->pub = pub;
    for (x = b.first; x; x = x->NextGadget)
    {
        struct GTGad *gg = gt_gad(x);
        if (gg)
        {
            gg->own_text = x->GadgetText;
            gg->own_special = x->SpecialInfo;
            gg->own_special_type = x->SpecialInfo ? (x->GadgetType & GTYP_GTYPEMASK) : 0;
        }
        if (x == b.last)
            break;
    }
    if (data->kind == GT_KIND_SLIDER)
        _gadtools_UpdateSliderLevelDisplay(mg, data->value);
    if (!b.first)
        b.first = b.last = mg;    /* the palette object */

    /* Link to previous gadget */
    if (gt_is_context_gadget(gad))
    {
        struct GTGad *ctx = (struct GTGad *)gad;
        struct Gadget *tail = ctx->ctx_last ? ctx->ctx_last : gad;

        tail->NextGadget = b.first;
        ctx->ctx_last = b.last;
    }
    else
        gad->NextGadget = b.first;

    DPRINTF (LOG_DEBUG, "_gadtools: CreateGadgetA() -> 0x%08lx\n", (ULONG)pub);
    return pub;
}

/*
 * The palette: a BOOPSI gadget of a private gadgetclass subclass, like the
 * one AmigaOS 3.1 GadTools creates (GadgetType GTYP_GADTOOLS|CUSTOMGADGET).
 */
BOOL _gadtools_RenderGadget(register struct Window *win __asm("a0"),
                            register struct Gadget *gad __asm("a1"),
                            register struct RastPort *rp __asm("a2"),
                            register LONG gl __asm("d0"),
                            register LONG gt __asm("d1"));

static ULONG gt_palette_dispatch(register struct IClass *cl __asm("a0"),
                                 register Object *obj __asm("a2"),
                                 register Msg msg __asm("a1"))
{
    struct Gadget *gad = (struct Gadget *)obj;

    switch (msg->MethodID)
    {
        case GM_RENDER:
        {
            struct gpRender *gpr = (struct gpRender *)msg;
            struct Window *win = gpr->gpr_GInfo ? gpr->gpr_GInfo->gi_Window : NULL;
            struct Requester *req = gpr->gpr_GInfo ? gpr->gpr_GInfo->gi_Requester : NULL;
            LONG l = gad->LeftEdge, t = gad->TopEdge;

            if (req)
            {
                l += req->LeftEdge + (win ? win->BorderLeft : 0);
                t += req->TopEdge + (win ? win->BorderTop : 0);
            }
            _gadtools_RenderGadget(win, gad, gpr->gpr_RPort, l, t);
            return 0;
        }
        case GM_HITTEST:
            return GMR_GADGETHIT;
        case GM_GOACTIVE:
        case GM_HANDLEINPUT:
            return GMR_NOREUSE;
        case GM_GOINACTIVE:
            return 0;
        default:
            return CallHookPkt(&cl->cl_Super->cl_Dispatcher, obj, msg);
    }
}

static struct Gadget *gt_palette_new(struct gt_build *b, WORD l, WORD t, WORD w, WORD h)
{
    struct TagItem tags[5];
    struct Gadget *gad;
    struct GTPaletteInst *inst;
    APTR img;

    if (!g_gt_palette_class)
    {
        struct IClass *cl = MakeClass(NULL, (UBYTE *)GADGETCLASS, NULL, sizeof(struct GTPaletteInst), 0);
        if (!cl)
        {
            b->failed = TRUE;
            return NULL;
        }
        cl->cl_Dispatcher.h_Entry = (ULONG (*)())gt_palette_dispatch;
        g_gt_palette_class = cl;
    }
    tags[0].ti_Tag = GA_Left;   tags[0].ti_Data = (ULONG)(LONG)l;
    tags[1].ti_Tag = GA_Top;    tags[1].ti_Data = (ULONG)(LONG)t;
    tags[2].ti_Tag = GA_Width;  tags[2].ti_Data = (ULONG)(LONG)w;
    tags[3].ti_Tag = GA_Height; tags[3].ti_Data = (ULONG)(LONG)h;
    tags[4].ti_Tag = TAG_DONE;  tags[4].ti_Data = 0;
    gad = (struct Gadget *)NewObjectA(g_gt_palette_class, NULL, tags);
    if (!gad)
    {
        b->failed = TRUE;
        return NULL;
    }
    inst = (struct GTPaletteInst *)INST_DATA(g_gt_palette_class, gad);
    inst->magic = GT_DATA_MAGIC;
    inst->data = b->data;
    b->data->refs++;
    gad->LeftEdge = l;
    gad->TopEdge = t;
    gad->Width = w;
    gad->Height = h;
    gad->GadgetType = LXA_GTYP_GADTOOLS | GTYP_CUSTOMGADGET;
    gad->Flags = GFLG_EXTENDED | GFLG_GADGHNONE | GFLG_GADGIMAGE;
    gad->Activation = GACT_RELVERIFY | GACT_IMMEDIATE;
    gad->GadgetID = b->ng->ng_GadgetID;
    gad->UserData = b->ng->ng_UserData;
    /* a custom gadget's MutualExclude is its dispatcher hook */
    gad->MutualExclude = (ULONG)&g_gt_palette_class->cl_Dispatcher;
    img = gt_new_frame(0, 0, w, h, FRAME_BUTTON, FALSE);
    inst->img = img;
    gad->GadgetRender = img;
    if (!img)
        b->failed = TRUE;
    return gad;
}

/*
 * GadTools imagery (Phase 223).
 *
 * Every GadTools kind is drawn the way AmigaOS 3.1 draws it (reference:
 * tests/scenarios/gallery-gt-*.yaml): frameiclass-style frames, sysiclass
 * images for checkboxes and mutual-exclude buttons, the cycle glyph, solid
 * prop knobs, and the 3.1 ghosting pattern over disabled gadgets.
 * Intuition calls _gadtools_RenderGadget() for every GadTools gadget.
 */

static const UWORD *gt_pens(struct GTGadgetData *data)
{
    if (data && data->vi && data->vi->vi_DrawInfo && data->vi->vi_DrawInfo->dri_Pens)
        return data->vi->vi_DrawInfo->dri_Pens;
    return lxa_default_pens();
}

static WORD gt_font_height(struct RastPort *rp)
{
    return rp->Font ? rp->Font->tf_YSize : GT_FONT_HEIGHT;
}

/* JAM1 text with an optional underlined character (GT_Underscore) */
static void gt_draw_text(struct RastPort *rp, WORD x, WORD y, CONST_STRPTR s, WORD len,
                         UWORD pen, WORD ul)
{
    if (!s || len <= 0)
        return;
    SetAPen(rp, pen);
    SetDrMd(rp, JAM1);
    Move(rp, x, y + rp->TxBaseline);
    Text(rp, (STRPTR)s, len);
    if (ul >= 0 && ul < len)
    {
        /* AmigaOS 3.1 draws the character again in the underlined style */
        ULONG old = AskSoftStyle(rp) & rp->AlgoStyle;
        SetSoftStyle(rp, FSF_UNDERLINED, FSF_UNDERLINED);
        Move(rp, x + TextLength(rp, (STRPTR)s, ul), y + rp->TxBaseline);
        Text(rp, (STRPTR)s + ul, 1);
        SetSoftStyle(rp, old, FSF_UNDERLINED);
    }
}

/* arrow glyphs of the GadTools scroller/listview arrow buttons, observed on
 * AmigaOS 3.1 for the default sizes: rows of (first, last) pixel pairs
 * relative to the button, terminated by -1 */
static const BYTE g_gt_arrow_left_16x11[] = {
    2, 10, 11, -2, 3, 8, 10, -2, 4, 6, 8, -2, 5, 4, 6, -2, 6, 5, 7, -2,
    7, 7, 9, -2, 8, 9, 11, -2, -1 };

/* draw a glyph table; dir 0 = as stored, 1 = mirrored horizontally,
 * 2 = mirrored vertically (row r -> h-1-r) */
static void gt_draw_glyph(struct RastPort *rp, const BYTE *g, WORD x, WORD y, WORD w, WORD h,
                          UWORD dir)
{
    while (*g >= 0)
    {
        WORD row = *g++;
        if (dir == 2)
            row = h - 1 - row;
        while (*g != -2)
        {
            WORD a = *g++, b = *g++;
            if (dir == 1)
            {
                WORD t = w - 1 - b;
                b = w - 1 - a;
                a = t;
            }
            RectFill(rp, x + a, y + row, x + b, y + row);
        }
        g++;
    }
}

/* generic scaled chevron for sizes without an observed table */
static void gt_draw_chevron(struct RastPort *rp, WORD x, WORD y, WORD w, WORD h, UWORD which)
{
    WORD cx = x + w / 2, cy = y + h / 2, i, n;

    if (which == LEFTIMAGE || which == RIGHTIMAGE)
    {
        n = (h - 4) / 2;
        for (i = -n; i <= n; i++)
        {
            WORD dx = (i < 0 ? -i : i) * 2;
            WORD px = (which == LEFTIMAGE) ? cx - 3 + dx : cx + 3 - dx - 2;
            RectFill(rp, px, cy + i, px + 2, cy + i);
        }
    }
    else
    {
        n = (w - 6) / 4;
        for (i = 0; i <= n && i < h - 4; i++)
        {
            WORD py = (which == UPIMAGE) ? y + 2 + i : y + h - 3 - i;
            RectFill(rp, cx - 1 - i, py, cx - i, py);
            RectFill(rp, cx + i, py, cx + 1 + i, py);
        }
    }
}

/* up/down chevron as AmigaOS 3.1 draws it for any button size: two lines
 * from the apex (w/2-1 | w/2, row 2) to (4 | w-5, row h-3), two pixels wide,
 * x rounded half down (left line) / half up (right line) */
static void gt_draw_vchevron(struct RastPort *rp, WORD x, WORD y, WORD w, WORD h, BOOL down)
{
    WORD x0 = w / 2 - 1, x1 = 4, dy = h - 5, dx, i;

    if (dy < 1 || x0 <= x1)
        return;
    dx = x0 - x1;
    for (i = 0; i <= dy; i++)
    {
        WORD off = (WORD)((2 * i * dx + dy - 1) / (2 * dy));
        WORD row = down ? (h - 3 - i) : (2 + i);
        WORD lx = x0 - off;
        /* the right line rounds half up */
        WORD rx = w / 2 + (WORD)((2 * i * dx + dy) / (2 * dy));
        RectFill(rp, x + lx, y + row, x + lx + 1, y + row);
        RectFill(rp, x + rx - 1, y + row, x + rx, y + row);
    }
}

static void gt_draw_arrow_button(struct RastPort *rp, WORD x, WORD y, WORD w, WORD h,
                                 UWORD which, BOOL selected, const UWORD *pens)
{
    lxa_draw_frame(rp, FRAME_BUTTON, FALSE, x, y, w, h,
                   selected ? IDS_SELECTED : IDS_NORMAL, FALSE, pens);
    SetAPen(rp, pens[TEXTPEN]);
    if ((which == LEFTIMAGE || which == RIGHTIMAGE) && w == 16 && h == 11)
        gt_draw_glyph(rp, g_gt_arrow_left_16x11, x, y, w, h, which == RIGHTIMAGE ? 1 : 0);
    else if (which == UPIMAGE || which == DOWNIMAGE)
        gt_draw_vchevron(rp, x, y, w, h, which == DOWNIMAGE);
    else
        gt_draw_chevron(rp, x, y, w, h, which);
}

/* knob of a GadTools slider/scroller: solid, no border, the size rounded up */
static void gt_draw_prop(struct RastPort *rp, struct PropInfo *pi, WORD x, WORD y, WORD w, WORD h,
                         const UWORD *pens)
{
    WORD kw = w, kh = h, kx = x, ky = y;

    SetAPen(rp, pens[BACKGROUNDPEN]);
    RectFill(rp, x, y, x + w - 1, y + h - 1);
    if (!pi)
        return;
    if (pi->Flags & FREEHORIZ)
    {
        kw = (WORD)(((ULONG)w * pi->HorizBody + 0xFFFE) / 0xFFFF);
        if (kw < 6) kw = 6;
        if (kw > w) kw = w;
        kx = x + (WORD)(((ULONG)(w - kw) * pi->HorizPot) / 0xFFFF);
    }
    if (pi->Flags & FREEVERT)
    {
        kh = (WORD)(((ULONG)h * pi->VertBody + 0xFFFE) / 0xFFFF);
        if (kh < 4) kh = 4;
        if (kh > h) kh = h;
        ky = y + (WORD)(((ULONG)(h - kh) * pi->VertPot) / 0xFFFF);
    }
    SetAPen(rp, pens[SHADOWPEN]);
    RectFill(rp, kx, ky, kx + kw - 1, ky + kh - 1);
}

static void gt_draw_cycle_glyph(struct RastPort *rp, WORD x, WORD y, WORD gh)
{
    /* 3.1 cycle glyph (9 wide, gh high, gh >= 9) */
    RectFill(rp, x + 1, y, x + 7, y);                   /* top */
    RectFill(rp, x + 1, y + gh - 1, x + 7, y + gh - 1); /* bottom */
    RectFill(rp, x, y + 1, x + 1, y + gh - 2);          /* left, double */
    RectFill(rp, x + 7, y + 1, x + 8, y + 2);           /* right, upper */
    RectFill(rp, x + 5, y + 3, x + 10, y + 3);          /* arrow */
    RectFill(rp, x + 6, y + 4, x + 9, y + 4);
    RectFill(rp, x + 7, y + 5, x + 8, y + 5);
    RectFill(rp, x + 7, y + gh - 2, x + 8, y + gh - 2); /* right, lower */
}

static void gt_draw_label_chain(struct RastPort *rp, struct Gadget *gad, WORD gl, WORD gt,
                                struct GTGadgetData *data, const UWORD *pens)
{
    struct IntuiText *it;

    if (!gad)
        return;
    /* only the label itself: the underlined-character text that follows it
     * is drawn as the underline */
    for (it = gad->GadgetText; it; it = (gad->GadgetText == it && gad == data->label_gad) ? NULL : it->NextText)
    {
        if (!it->IText)
            continue;
        gt_draw_text(rp, gl + it->LeftEdge, gt + it->TopEdge, it->IText, gt_strlen(it->IText),
                     pens[it->FrontPen == 2 ? HIGHLIGHTTEXTPEN : TEXTPEN],
                     (gad == data->label_gad && it == gad->GadgetText) ? data->underline_pos : -1);
    }
}

/*
 * Draw a whole GadTools group.  `gad` is any member, (gl, gt) its position
 * in the RastPort; every other member is placed relative to it.
 */
BOOL _gadtools_RenderGadget(register struct Window *win __asm("a0"),
                            register struct Gadget *gad __asm("a1"),
                            register struct RastPort *rp __asm("a2"),
                            register LONG gl __asm("d0"),
                            register LONG gt __asm("d1"))
{
    struct GTGadgetData *data = gt_get_data(gad);
    struct Gadget *mg;
    const UWORD *pens;
    struct TextFont *oldfont;
    UBYTE oldpen, olddm;
    WORD ox, oy, mgl, mgt, L, T, W, H, fh;
    BOOL disabled;

    if (!data || !rp || !data->main)
        return FALSE;
    if (g_gt_in_pass)
    {
        if (data->drawn_pass == g_gt_pass)
            return TRUE;
        data->drawn_pass = g_gt_pass;
    }

    mg = data->main;
    pens = gt_pens(data);
    oldfont = rp->Font;
    oldpen = rp->FgPen;
    olddm = rp->DrawMode;
    if (data->font)
        SetFont(rp, data->font);
    fh = gt_font_height(rp);
    /* gadget coordinates -> RastPort coordinates */
    ox = (WORD)gl - gad->LeftEdge;
    oy = (WORD)gt - gad->TopEdge;
    mgl = ox + mg->LeftEdge;
    mgt = oy + mg->TopEdge;
    /* the NewGadget box */
    L = ox + data->ng_left;
    T = oy + data->ng_top;
    W = data->box_w;
    H = data->box_h;
    disabled = (mg->Flags & GFLG_DISABLED) != 0;

    switch (data->kind)
    {
        case GT_KIND_BUTTON:
        {
            BOOL sel = (mg->Flags & GFLG_SELECTED) != 0;
            lxa_draw_frame(rp, FRAME_BUTTON, FALSE, L, T, W, H,
                           sel ? IDS_SELECTED : IDS_NORMAL, FALSE, pens);
            if (data->label)
            {
                WORD len = gt_strlen(data->label);
                if (data->label_in)
                {
                    WORD tl = TextLength(rp, data->label, len);
                    gt_draw_text(rp, L + ((W - tl + 1) >> 1), T + ((H - fh + 1) >> 1), data->label, len,
                                 sel ? pens[FILLTEXTPEN] : pens[data->label_pen], data->underline_pos);
                }
                else
                    gt_draw_text(rp, L + data->label_x, T + data->label_y, data->label, len,
                                 pens[data->label_pen], data->underline_pos);
            }
            if (disabled)
                lxa_ghost_rect(rp, L, T, L + W - 1, T + H - 1, pens);
            break;
        }

        case GT_KIND_CHECKBOX:
        {
            WORD w = mg->Width, h = mg->Height;
            ULONG st = data->value ? IDS_SELECTED : IDS_NORMAL;
            UWORD iw = 26, ih = 11;

            lxa_sysi_dims(CHECKIMAGE, SYSISIZE_MEDRES, &iw, &ih);
            if (w == iw && h == ih)
                lxa_sysi_draw(rp, CHECKIMAGE, SYSISIZE_MEDRES, mgl, mgt, st, pens);
            else
            {
                lxa_draw_frame(rp, FRAME_BUTTON, FALSE, mgl, mgt, w, h, IDS_NORMAL, FALSE, pens);
                if (data->value)
                {
                    SetAPen(rp, pens[TEXTPEN]);
                    RectFill(rp, mgl + w / 3, mgt + h / 2, mgl + w / 2, mgt + h - 3);
                    RectFill(rp, mgl + w / 2, mgt + 2, mgl + 2 * w / 3, mgt + h / 2);
                }
            }
            gt_draw_label_chain(rp, mg, mgl, mgt, data, pens);
            if (disabled)
                lxa_ghost_rect(rp, mgl, mgt, mgl + w - 1, mgt + h - 1, pens);
            break;
        }

        case GT_KIND_MX:
        {
            WORD i;
            UWORD iw = 17, ih = 9;

            lxa_sysi_dims(MXIMAGE, SYSISIZE_MEDRES, &iw, &ih);
            for (i = 0; i < data->nitems; i++)
            {
                struct Gadget *it = data->items[i];
                WORD x = ox + it->LeftEdge, y = oy + it->TopEdge;
                ULONG st = (i == data->value) ? IDS_SELECTED : IDS_NORMAL;

                if (it->Width == (WORD)iw && it->Height == (WORD)ih)
                    lxa_sysi_draw(rp, MXIMAGE, SYSISIZE_MEDRES, x, y, st, pens);
                else
                {
                    UWORD sx = x + (it->Width - (WORD)iw) / 2, sy = y + (it->Height - (WORD)ih) / 2;
                    lxa_sysi_draw(rp, MXIMAGE, SYSISIZE_MEDRES, sx, sy, st, pens);
                }
                gt_draw_label_chain(rp, it, x, y, data, pens);
                if (it->Flags & GFLG_DISABLED)
                    lxa_ghost_rect(rp, x, y, x + it->Width - 1, y + it->Height - 1, pens);
            }
            if (data->label_gad)
                gt_draw_label_chain(rp, data->label_gad, ox + data->label_gad->LeftEdge,
                                    oy + data->label_gad->TopEdge, data, pens);
            break;
        }

        case GT_KIND_CYCLE:
        {
            BOOL sel = (mg->Flags & GFLG_SELECTED) != 0;
            WORD gh = H - 5;
            STRPTR s = data->level_buffer;

            if (gh < 9)
                gh = 9;
            lxa_draw_frame(rp, FRAME_BUTTON, FALSE, L, T, W, H,
                           sel ? IDS_SELECTED : IDS_NORMAL, FALSE, pens);
            SetAPen(rp, sel ? pens[FILLTEXTPEN] : pens[TEXTPEN]);
            gt_draw_cycle_glyph(rp, L + 6, T + (H - gh) / 2, gh);
            SetAPen(rp, pens[SHADOWPEN]);
            RectFill(rp, L + 20, T + 2, L + 20, T + H - 3);
            SetAPen(rp, pens[SHINEPEN]);
            RectFill(rp, L + 21, T + 2, L + 21, T + H - 3);
            if (s)
            {
                WORD len = gt_strlen(s);
                WORD tl = TextLength(rp, s, len);
                gt_draw_text(rp, L + 20 + ((W - 20 - tl + 1) >> 1), T + ((H - fh + 1) >> 1), s, len,
                             sel ? pens[FILLTEXTPEN] : pens[TEXTPEN], -1);
            }
            gt_draw_label_chain(rp, mg, mgl, mgt, data, pens);
            if (disabled)
                lxa_ghost_rect(rp, L, T, L + W - 1, T + H - 1, pens);
            break;
        }

        case GT_KIND_STRING:
        case GT_KIND_INTEGER:
        {
            struct StringInfo *si = (struct StringInfo *)mg->SpecialInfo;
            WORD gw = mg->Width, gh2 = mg->Height;

            lxa_draw_frame(rp, FRAME_RIDGE, FALSE, L, T, W, H, IDS_NORMAL, TRUE, pens);
            SetAPen(rp, pens[BACKGROUNDPEN]);
            RectFill(rp, mgl, mgt, mgl + gw - 1, mgt + gh2 - 1);
            if (si && si->Buffer)
            {
                WORD len = gt_strlen(si->Buffer);
                WORD disp = si->DispPos;

                /* rendering takes the buffer as it is (AmigaOS 3.1) */
                si->NumChars = len;
                if (si->BufferPos > len)
                    si->BufferPos = len;
                WORD fit;

                if (disp < 0 || disp > len)
                    disp = 0;
                fit = lxa_text_fit(rp, si->Buffer + disp, len - disp, gw);
                SetAPen(rp, pens[TEXTPEN]);
                SetBPen(rp, pens[BACKGROUNDPEN]);
                SetDrMd(rp, JAM2);
                Move(rp, mgl, mgt + rp->TxBaseline);
                if (fit > 0)
                    Text(rp, si->Buffer + disp, fit);
                if (mg->Flags & GFLG_SELECTED)
                {
                    WORD pos = si->BufferPos - disp;
                    WORD cx = mgl + ((pos > 0) ? TextLength(rp, si->Buffer + disp, pos) : 0);
                    WORD cw = (si->BufferPos < len) ? TextLength(rp, si->Buffer + si->BufferPos, 1)
                                                    : TextLength(rp, (STRPTR)" ", 1);
                    SetDrMd(rp, COMPLEMENT);
                    RectFill(rp, cx, mgt, cx + cw - 1, mgt + gh2 - 1);
                    SetDrMd(rp, JAM1);
                }
            }
            gt_draw_label_chain(rp, mg, mgl, mgt, data, pens);
            if (disabled)
                lxa_ghost_rect(rp, mgl, mgt, mgl + gw - 1, mgt + gh2 - 1, pens);
            break;
        }

        case GT_KIND_SLIDER:
        {
            lxa_draw_frame(rp, FRAME_BUTTON, FALSE, L, T, W, H, IDS_NORMAL, TRUE, pens);
            gt_draw_prop(rp, (struct PropInfo *)mg->SpecialInfo, mgl, mgt, mg->Width, mg->Height, pens);
            if (data->level_text)
            {
                struct IntuiText *lt = data->level_text;
                /* clear the level text field before redrawing it */
                SetAPen(rp, pens[BACKGROUNDPEN]);
                RectFill(rp, L + data->level_x0, T + lt->TopEdge,
                         L + data->level_x0 + (WORD)data->max_pixel_len - 1, T + lt->TopEdge + fh - 1);
                gt_draw_text(rp, L + lt->LeftEdge, T + lt->TopEdge, lt->IText, gt_strlen(lt->IText),
                             pens[TEXTPEN], -1);
            }
            if (data->label_gad)
                gt_draw_label_chain(rp, data->label_gad, ox + data->label_gad->LeftEdge,
                                    oy + data->label_gad->TopEdge, data, pens);
            if (disabled)
                lxa_ghost_rect(rp, mgl, mgt, mgl + mg->Width - 1, mgt + mg->Height - 1, pens);
            break;
        }

        case GT_KIND_SCROLLER:
        {
            WORD fw = W, fhh = H;

            if (data->arrow_dec && data->arrow_inc)
            {
                struct Gadget *d = data->arrow_dec, *u = data->arrow_inc;
                BOOL vert = data->vertical;
                gt_draw_arrow_button(rp, ox + d->LeftEdge, oy + d->TopEdge, d->Width, d->Height,
                                     vert ? UPIMAGE : LEFTIMAGE, (d->Flags & GFLG_SELECTED) != 0, pens);
                gt_draw_arrow_button(rp, ox + u->LeftEdge, oy + u->TopEdge, u->Width, u->Height,
                                     vert ? DOWNIMAGE : RIGHTIMAGE, (u->Flags & GFLG_SELECTED) != 0, pens);
                if (vert)
                    fhh = H - 2 * data->arrows;
                else
                    fw = W - 2 * data->arrows;
            }
            lxa_draw_frame(rp, FRAME_BUTTON, FALSE, L, T, fw, fhh, IDS_NORMAL, TRUE, pens);
            gt_draw_prop(rp, (struct PropInfo *)mg->SpecialInfo, mgl, mgt, mg->Width, mg->Height, pens);
            if (data->label_gad)
                gt_draw_label_chain(rp, data->label_gad, ox + data->label_gad->LeftEdge,
                                    oy + data->label_gad->TopEdge, data, pens);
            if (disabled)
                lxa_ghost_rect(rp, L, T, L + W - 1, T + H - 1, pens);
            break;
        }

        case GT_KIND_LISTVIEW:
        {
            struct List *list = (struct List *)data->aux;
            WORD sw = data->lv_scroll_w, lw = W - sw;
            WORD lines, i, y;
            struct Node *n;

            /* read-only lists sit in a recessed frame (AmigaOS 3.1) */
            lxa_draw_frame(rp, FRAME_BUTTON, data->lv_readonly, L, T, lw, H, IDS_NORMAL, TRUE, pens);
            lines = mg->Height / fh;
            SetAPen(rp, pens[BACKGROUNDPEN]);
            RectFill(rp, L + 2, T + 2, L + lw - 3, T + H - 3);
            n = (list && list != (struct List *)~0) ? list->lh_Head : NULL;
            for (i = 0; n && n->ln_Succ && i < data->lv_top; i++)
                n = n->ln_Succ;
            for (i = 0, y = mgt; n && n->ln_Succ && i < lines; i++, y += fh, n = n->ln_Succ)
            {
                BOOL sel = data->lv_showsel && (data->lv_top + i == data->value);
                if (sel)
                {
                    SetAPen(rp, pens[FILLPEN]);
                    RectFill(rp, L + 2, y, L + lw - 3, y + fh - 1);
                }
                if (n->ln_Name)
                {
                    WORD len = gt_strlen((STRPTR)n->ln_Name);
                    WORD fit = lxa_text_fit(rp, (STRPTR)n->ln_Name, len, lw - 4 - 4);
                    gt_draw_text(rp, L + 4, y, (STRPTR)n->ln_Name, fit,
                                 sel ? pens[FILLTEXTPEN] : pens[TEXTPEN], -1);
                }
            }
            /* scroller with arrows */
            if (data->lv_prop && data->arrow_dec && data->arrow_inc)
            {
                struct Gadget *p = data->lv_prop, *d = data->arrow_dec, *u = data->arrow_inc;
                lxa_draw_frame(rp, FRAME_BUTTON, FALSE, L + lw, T, sw, H - 2 * data->arrows,
                               IDS_NORMAL, TRUE, pens);
                gt_draw_prop(rp, (struct PropInfo *)p->SpecialInfo, ox + p->LeftEdge, oy + p->TopEdge,
                             p->Width, p->Height, pens);
                gt_draw_arrow_button(rp, ox + d->LeftEdge, oy + d->TopEdge, d->Width, d->Height, UPIMAGE,
                                     (d->Flags & GFLG_SELECTED) != 0, pens);
                gt_draw_arrow_button(rp, ox + u->LeftEdge, oy + u->TopEdge, u->Width, u->Height, DOWNIMAGE,
                                     (u->Flags & GFLG_SELECTED) != 0, pens);
            }
            if (data->label_gad)
                gt_draw_label_chain(rp, data->label_gad, ox + data->label_gad->LeftEdge,
                                    oy + data->label_gad->TopEdge, data, pens);
            if (disabled)
                lxa_ghost_rect(rp, L, T, L + W - 1, T + H - 1, pens);
            break;
        }

        case GT_KIND_PALETTE:
        {
            WORD ncol = 1 << data->pal_depth;
            WORD cols = data->pal_cols ? data->pal_cols : ncol, rows = data->pal_rows ? data->pal_rows : 1;
            WORD i, bw, bh;

            /* AmigaOS 3.1: colour boxes on a (W - 5) / n pitch, 3 pixels apart */
            bw = (W - 5) / cols;
            bh = (H - 5) / rows;
            lxa_draw_frame(rp, FRAME_BUTTON, FALSE, L, T, W, H, IDS_NORMAL, FALSE, pens);
            for (i = 0; i < ncol; i++)
            {
                WORD x = L + 4 + (i % cols) * bw, y = T + 2 + (i / cols) * bh;
                SetAPen(rp, data->pal_offset + i);
                RectFill(rp, x, y, x + bw - 4, y + bh - ((i / cols) < rows - 1 ? 2 : 0));
            }
            gt_draw_label_chain(rp, mg, mgl, mgt, data, pens);
            if (disabled)
                lxa_ghost_rect(rp, L, T, L + W - 1, T + H - 1, pens);
            break;
        }

        case GT_KIND_TEXT:
        case GT_KIND_NUMBER:
        {
            STRPTR s = data->level_buffer;

            if (data->border)
                lxa_draw_frame(rp, FRAME_BUTTON, TRUE, L, T, W, H, IDS_NORMAL, FALSE, pens);
            if (s)
            {
                WORD len = gt_strlen(s);
                WORD inner = data->border ? W - 8 : W;
                WORD fit = lxa_text_fit(rp, s, len, inner);
                gt_draw_text(rp, L + (data->border ? 4 : 0), T + (H - fh + 1) / 2, s, fit,
                             pens[TEXTPEN], -1);
            }
            gt_draw_label_chain(rp, mg, mgl, mgt, data, pens);
            break;
        }

        default:
            gt_draw_label_chain(rp, mg, mgl, mgt, data, pens);
            break;
    }

    SetFont(rp, oldfont);
    SetAPen(rp, oldpen);
    SetDrMd(rp, olddm);
    return TRUE;
}

static void gt_free_itext(struct IntuiText *it)
{
    BOOL first = TRUE;

    while (it)
    {
        struct IntuiText *next = it->NextText;
        if (it->IText)
            FreeMem(it->IText, gt_strlen(it->IText) + 1);
        /* the underlined-character text after the label (GT_Underscore) */
        FreeMem(it, first ? sizeof(struct IntuiText) : sizeof(struct GTULText));
        first = FALSE;
        it = next;
    }
}

static void gt_release_data(struct GTGadgetData *data)
{
    if (!data || --data->refs > 0)
        return;
    if (data->format)
        FreeMem(data->format, gt_strlen(data->format) + 1);
    if (data->level_buffer)
    {
        if (data->kind == GT_KIND_TEXT)
            FreeMem(data->level_buffer, gt_strlen(data->level_buffer) + 1);
        else if (data->kind == GT_KIND_NUMBER)
            FreeMem(data->level_buffer, 32);
        else
            FreeMem(data->level_buffer, data->max_level_len + 1);
    }
    if (data->level_text)
        FreeMem(data->level_text, sizeof(struct IntuiText));
    if (data->label)
        FreeMem(data->label, gt_strlen(data->label) + 1);
    if (data->items)
        FreeMem(data->items, data->nitems * sizeof(struct Gadget *));
    if (data->font)
        CloseFont(data->font);
    data->magic = 0;
    FreeMem(data, sizeof(struct GTGadgetData));
}

/* free one gadget GadTools allocated, with everything it owns */
static void gt_free_member(struct Gadget *gad)
{
    struct GTGad *g = gt_gad(gad);
    struct GTGadgetData *data;

    if (!g)
    {
        /* the palette object */
        data = gt_get_data(gad);
        if (data && g_gt_palette_class)
        {
            struct GTPaletteInst *inst = (struct GTPaletteInst *)INST_DATA(g_gt_palette_class, gad);
            gt_free_itext(gad->GadgetText);
            gad->GadgetText = NULL;
            if (inst->img)
                DisposeObject(inst->img);
            inst->magic = 0;
            gt_release_data(data);
            DisposeObject(gad);
        }
        return;
    }
    data = g->data;

    if (g->own_special_type == GTYP_STRGADGET && g->own_special)
    {
        struct StringInfo *si = (struct StringInfo *)g->own_special;
        if (si->Buffer)
            FreeMem(si->Buffer, si->MaxChars + 1);
        if (si->UndoBuffer)
            FreeMem(si->UndoBuffer, si->MaxChars + 1);
        if (si->Extension)
            FreeMem(si->Extension, sizeof(struct StringExtend));
        FreeMem(si, sizeof(struct StringInfo));
    }
    if (g->own_special_type == GTYP_PROPGADGET && g->own_special)
        FreeMem(g->own_special, sizeof(struct PropInfo));
    if (g->boopsi_imgs)
    {
        if (g->render_img)
            DisposeObject(g->render_img);
        if (g->select_img)
            DisposeObject(g->select_img);
    }
    else if (g->render_img)
        FreeMem(g->render_img, sizeof(struct Image));
    gt_free_itext(g->own_text);
    g->magic = 0;
    if (data)
        gt_release_data(data);
    FreeMem(g, sizeof(struct GTGad));
}

/* FreeGadgets - Free a list of gadgets created by CreateGadgetA */
void _gadtools_FreeGadgets ( register struct GadToolsBase *GadToolsBase __asm("a6"),
                             register struct Gadget *gad __asm("a0") )
{
    struct Gadget *next;

    DPRINTF (LOG_DEBUG, "_gadtools: FreeGadgets() gad=0x%08lx\n", (ULONG)gad);

    while (gad)
    {
        next = gad->NextGadget;
        if (gt_get_data(gad) || gt_is_context_gadget(gad))
            gt_free_member(gad);
        gad = next;
    }
}

/* GT_SetGadgetAttrsA - Set gadget attributes */
void _gadtools_GT_SetGadgetAttrsA ( register struct GadToolsBase *GadToolsBase __asm("a6"),
                                    register struct Gadget *gad __asm("a0"),
                                    register struct Window *win __asm("a1"),
                                    register struct Requester *req __asm("a2"),
                                    register struct TagItem *taglist __asm("a3") )
{
    struct GTGadgetData *data;
    struct TagItem *tag;
    BOOL needs_refresh = FALSE;

    DPRINTF (LOG_DEBUG, "_gadtools: GT_SetGadgetAttrsA() gad=0x%08lx\n", (ULONG)gad);

    if (!gad || !taglist)
        return;

    data = gt_get_data(gad);
    gad = gt_main(gad);

    if (data && data->kind == GT_KIND_CHECKBOX)
    {
        tag = gt_find_tagitem(GTCB_Checked, taglist);
        if (tag)
        {
            data->value = (tag->ti_Data != 0) ? TRUE : FALSE;
            needs_refresh = TRUE;
        }
    }

    if (data && data->kind == GT_KIND_CYCLE)
    {
        tag = gt_find_tagitem(GTCY_Active, taglist);
        if (tag)
        {
            _gadtools_SetCycleState(gad, (UWORD)tag->ti_Data);
            needs_refresh = TRUE;
        }
    }

    if (data && data->kind == GT_KIND_STRING)
    {
        tag = gt_find_tagitem(GTST_String, taglist);
        if (tag && gad->SpecialInfo)
        {
            struct StringInfo *si = (struct StringInfo *)gad->SpecialInfo;
            STRPTR src = (STRPTR)tag->ti_Data;
            WORD len = 0;

            if (!src)
                src = (STRPTR)"";

            while (src[len] != '\0' && len < si->MaxChars - 1)
            {
                si->Buffer[len] = src[len];
                len++;
            }
            si->Buffer[len] = '\0';
            si->NumChars = len;
            si->BufferPos = len;
            needs_refresh = TRUE;
        }
    }

    if (data && data->kind == GT_KIND_INTEGER)
    {
        tag = gt_find_tagitem(GTIN_Number, taglist);
        if (tag && gad->SpecialInfo)
        {
            struct StringInfo *si = (struct StringInfo *)gad->SpecialInfo;
            LONG value = (LONG)tag->ti_Data;

            data->value = value;
            si->LongInt = value;
            si->NumChars = gt_format_long(si->Buffer, si->MaxChars, value);
            si->BufferPos = si->NumChars;
            needs_refresh = TRUE;
        }
    }

    /* Handle prop gadgets (SLIDER_KIND) */
    if (data && data->kind == GT_KIND_SLIDER && (gad->GadgetType & GTYP_GTYPEMASK) == GTYP_PROPGADGET)
    {
        struct PropInfo *pi = (struct PropInfo *)gad->SpecialInfo;
        if (pi)
        {
            LONG level = (LONG)GetTagData(GTSL_Level, -1, taglist);
            if (level != -1)
            {
                /* min/max stored in CWidth/CHeight */
                LONG sl_min = (LONG)(WORD)pi->CWidth;
                LONG sl_max = (LONG)(WORD)pi->CHeight;

                if (level < sl_min) level = sl_min;
                if (level > sl_max) level = sl_max;
                data->min = sl_min;
                data->max = sl_max;
                data->value = level;

                /* Recompute the pot from the new level */
                {
                    UWORD pot = 0;
                    if (sl_max > sl_min)
                        pot = (UWORD)(((ULONG)((pi->Flags & FREEVERT) ? sl_max - level : level - sl_min) *
                                       0xFFFFUL) / (ULONG)(sl_max - sl_min));
                    if (pi->Flags & FREEVERT)
                        pi->VertPot = pot;
                    else
                        pi->HorizPot = pot;
                }

                _gadtools_UpdateSliderLevelDisplay(gad, level);

                DPRINTF (LOG_DEBUG, "_gadtools: GT_SetGadgetAttrsA() SLIDER level=%ld -> pot=%u\n",
                         level, pi->HorizPot);
                needs_refresh = TRUE;
            }
        }
    }

    /* Handle SCROLLER_KIND:  accepts GTSC_Top, GTSC_Total, GTSC_Visible */
    if (data && data->kind == GT_KIND_SCROLLER && (gad->GadgetType & GTYP_GTYPEMASK) == GTYP_PROPGADGET)
    {
        struct PropInfo *pi = (struct PropInfo *)gad->SpecialInfo;
        if (pi)
        {
            LONG sc_top     = data->value;
            LONG sc_total   = data->max;
            LONG sc_visible = data->min;
            BOOL changed = FALSE;

            tag = gt_find_tagitem(GTSC_Total, taglist);
            if (tag) { sc_total = (LONG)tag->ti_Data; data->max = sc_total; changed = TRUE; }

            tag = gt_find_tagitem(GTSC_Visible, taglist);
            if (tag) { sc_visible = (LONG)tag->ti_Data; data->min = sc_visible; changed = TRUE; }

            tag = gt_find_tagitem(GTSC_Top, taglist);
            if (tag) { sc_top = (LONG)tag->ti_Data; changed = TRUE; }

            if (changed)
            {
                UWORD pot, body;

                /* Clamp top */
                if (sc_total > sc_visible)
                {
                    if (sc_top > sc_total - sc_visible)
                        sc_top = sc_total - sc_visible;
                }
                else
                {
                    sc_top = 0;
                }
                if (sc_top < 0) sc_top = 0;
                data->value = sc_top;

                gt_scroller_values(sc_top, sc_total, sc_visible, &pot, &body);

                if (pi->Flags & FREEVERT)
                {
                    pi->VertPot  = pot;
                    pi->VertBody = body;
                }
                else
                {
                    pi->HorizPot  = pot;
                    pi->HorizBody = body;
                }

                DPRINTF(LOG_DEBUG, "_gadtools: GT_SetGadgetAttrsA() SCROLLER top=%ld total=%ld visible=%ld pot=%u body=%u\n",
                        sc_top, sc_total, sc_visible, pot, body);
                needs_refresh = TRUE;
            }
        }
    }

    tag = gt_find_tagitem(GA_Disabled, taglist);
    if (tag)
    {
        struct Gadget *m;
        WORD k;
        for (k = -4; k < (data ? data->nitems : 0); k++)
        {
            m = (k == -4) ? gad : (k == -3) ? (data ? data->arrow_dec : NULL)
              : (k == -2) ? (data ? data->arrow_inc : NULL) : (k == -1) ? (data ? data->lv_prop : NULL)
              : data->items[k];
            if (!m)
                continue;
            if (tag->ti_Data)
                m->Flags |= GFLG_DISABLED;
            else
                m->Flags &= ~GFLG_DISABLED;
        }
        needs_refresh = TRUE;
    }

    if (data && data->magic == GT_DATA_MAGIC)
    {
        switch (data->kind)
        {
            case GT_KIND_MX:
                tag = gt_find_tagitem(GTMX_Active, taglist);
                if (tag && (LONG)tag->ti_Data >= 0 && (LONG)tag->ti_Data < data->max)
                {
                    data->value = (LONG)tag->ti_Data;
                    gt_update_mx_flags(data);
                    needs_refresh = TRUE;
                }
                break;
            case GT_KIND_LISTVIEW:
                tag = gt_find_tagitem(GTLV_Labels, taglist);
                if (tag)
                {
                    data->aux = (APTR)tag->ti_Data;
                    needs_refresh = TRUE;
                }
                tag = gt_find_tagitem(GTLV_Top, taglist);
                if (tag)
                {
                    data->lv_top = (WORD)tag->ti_Data;
                    needs_refresh = TRUE;
                }
                tag = gt_find_tagitem(GTLV_Selected, taglist);
                if (tag)
                {
                    data->value = (LONG)tag->ti_Data;
                    needs_refresh = TRUE;
                }
                tag = gt_find_tagitem(GTLV_MakeVisible, taglist);
                if (tag)
                {
                    data->lv_top = (WORD)tag->ti_Data;
                    needs_refresh = TRUE;
                }
                if (needs_refresh)
                {
                    gt_lv_clamp_top(data);
                    gt_lv_update_prop(data);
                }
                break;
            case GT_KIND_PALETTE:
                tag = gt_find_tagitem(GTPA_Color, taglist);
                if (tag)
                {
                    data->value = (LONG)tag->ti_Data;
                    needs_refresh = TRUE;
                }
                break;
            case GT_KIND_TEXT:
                tag = gt_find_tagitem(GTTX_Text, taglist);
                if (tag)
                {
                    STRPTR copy = gt_strdup(tag->ti_Data ? (STRPTR)tag->ti_Data : (STRPTR)"");
                    if (copy)
                    {
                        if (data->level_buffer)
                            FreeMem(data->level_buffer, gt_strlen(data->level_buffer) + 1);
                        data->level_buffer = copy;
                        needs_refresh = TRUE;
                    }
                }
                break;
            case GT_KIND_NUMBER:
                tag = gt_find_tagitem(GTNM_Number, taglist);
                if (tag && data->level_buffer)
                {
                    data->value = (LONG)tag->ti_Data;
                    gt_format_slider_level(data->level_buffer, 32,
                                           data->format ? data->format : (STRPTR)"%ld", data->value);
                    needs_refresh = TRUE;
                }
                break;
            case GT_KIND_CHECKBOX:
                if (data->value)
                    gad->Flags |= GFLG_SELECTED;
                else
                    gad->Flags &= ~GFLG_SELECTED;
                break;
            default:
                break;
        }
    }

    if (needs_refresh && win)
        RefreshGList(data && data->pub ? data->pub : gad, win, req, 1);
}

/*
 * Menu Functions
 */

/* Forward declaration */
static void FreeMenuItems(struct MenuItem *item);
void _gadtools_FreeMenus ( register struct GadToolsBase *GadToolsBase __asm("a6"),
                           register struct Menu *menu __asm("a0") );

static void gt_set_menu_error(struct TagItem *taglist, ULONG code)
{
    ULONG *secondary_error;

    secondary_error = (ULONG *)GetTagData(GTMN_SecondaryError, 0, taglist);
    if (secondary_error)
        *secondary_error = code;
}

static BOOL gt_validate_newmenu_array(const struct NewMenu *newmenu)
{
    BOOL have_title = FALSE;
    BOOL have_item = FALSE;
    UBYTE type;

    if (!newmenu)
        return FALSE;

    while (newmenu->nm_Type != NM_END)
    {
        if (newmenu->nm_Type & NM_IGNORE)
        {
            newmenu++;
            continue;
        }

        type = newmenu->nm_Type & ~(MENU_IMAGE | NM_IGNORE);

        switch (type)
        {
            case NM_TITLE:
                have_title = TRUE;
                have_item = FALSE;
                break;

            case NM_ITEM:
                if (!have_title)
                    return FALSE;
                have_item = TRUE;
                break;

            case NM_SUB:
                if (!have_item)
                    return FALSE;
                break;

            default:
                return FALSE;
        }

        newmenu++;
    }

    return have_title;
}

#define GT_SUBMENU_INDICATOR "\xbb"

/* Menu and MenuItem allocations: the GTMENU(ITEM)_USERDATA field, then a
 * tag telling FreeMenus() a menu from an item list (CreateMenus() of a
 * NewMenu array without titles returns the item list itself). */
#define GT_MENU_MAGIC 0x47544D55UL   /* "GTMU" */
#define GT_ITEM_MAGIC 0x47544D49UL   /* "GTMI" */
#define GT_MENU_SIZE (sizeof(struct Menu) + sizeof(APTR) + sizeof(ULONG))
#define GT_ITEM_SIZE (sizeof(struct MenuItem) + sizeof(APTR) + sizeof(ULONG))
#define GT_MENU_MAGIC_OF(m) (((ULONG *)((struct Menu *)(m) + 1))[1])
#define GT_ITEM_MAGIC_OF(i) (((ULONG *)((struct MenuItem *)(i) + 1))[1])

static struct MenuItem *gt_alloc_menuitem(void)
{
    struct MenuItem *item = AllocMem(GT_ITEM_SIZE, MEMF_CLEAR | MEMF_PUBLIC);
    if (item)
        GT_ITEM_MAGIC_OF(item) = GT_ITEM_MAGIC;
    return item;
}

static struct IntuiText *gt_alloc_menu_itext(STRPTR label)
{
    struct IntuiText *itext = AllocMem(sizeof(struct IntuiText), MEMF_CLEAR | MEMF_PUBLIC);

    if (itext)
    {
        itext->DrawMode = JAM1;
        itext->TopEdge = 1;
        itext->IText = label;
    }
    return itext;
}

/* NM_BARLABEL: an image item (AmigaOS 3.1: Flags HIGHNONE, no ITEMTEXT,
 * not ITEMENABLED) whose Image is followed by GT_BAR_MAGIC */
#define GT_BAR_MAGIC 0x47544241UL   /* "GTBA" */

static struct Image *gt_alloc_bar_image(void)
{
    struct Image *im = AllocMem(sizeof(struct Image) + sizeof(ULONG), MEMF_CLEAR | MEMF_PUBLIC);
    if (im)
    {
        /* AmigaOS 3.1: 2,2 and two lines high before the layout */
        im->LeftEdge = 2;
        im->TopEdge = 2;
        im->Height = 2;
        *(ULONG *)(im + 1) = GT_BAR_MAGIC;
    }
    return im;
}

static BOOL gt_is_separator_item(const struct MenuItem *item)
{
    if (!item || (item->Flags & ITEMTEXT) || !item->ItemFill)
        return FALSE;
    return *(ULONG *)((struct Image *)item->ItemFill + 1) == GT_BAR_MAGIC;
}

/*
 * Menu layout (LayoutMenuItemsA / LayoutMenusA).
 *
 * The geometry rules were measured on AmigaOS 3.1 (Phase 220 probes,
 * tests/gadtools/menu_api):
 *  - item height = font YSize + 1, separator height 6; items stack from 0,
 *    sub-items from -1;
 *  - label at LeftEdge 2, or checkmark width + 2 for CHECKIT items, TopEdge 1;
 *  - width = max(label left + label width) + max(right part) + 3, the
 *    right part being 6 + Amiga-key width + command char width (COMMSEQ) or
 *    6 + width of the sub-menu indicator; the indicator sits at width - 2 -
 *    its own width;
 *  - sub-items start at parent width - parent width / 4;
 *  - old-look checkmark/Amiga-key widths are CHECKWIDTH/COMMWIDTH (hires) or
 *    LOWCHECKWIDTH/LOWCOMMWIDTH, new-look ones come from the DrawInfo images
 *    (15/23 for topaz 8);
 *  - menu titles: width = text width + 8, first title at 2, 8 pixels apart,
 *    Menu.TopEdge/Height stay 0; top-level items are at least title width
 *    + 1 wide.
 */
struct gt_menu_layout
{
    struct RastPort rp;
    struct TextFont *font;
    struct TextAttr *textattr;
    UBYTE front_pen;
    BOOL newlook;
    WORD check_width;
    WORD comm_width;
    WORD item_height;
};

static __attribute__((noinline)) WORD gt_menu_text_width(struct gt_menu_layout *ml, CONST_STRPTR text)
{
    if (!text || text == (CONST_STRPTR)NM_BARLABEL)
        return 0;
    return (WORD)TextLength(&ml->rp, (STRPTR)text, gt_strlen(text));
}

static void gt_menu_layout_init(struct gt_menu_layout *ml, struct VisualInfo *vi,
                                struct TagItem *taglist, BOOL *ok)
{
    struct Screen *screen = vi ? vi->vi_Screen : NULL;
    struct DrawInfo *dri = vi ? vi->vi_DrawInfo : NULL;
    BOOL newlook = (BOOL)GetTagData(GTMN_NewLookMenus, FALSE, taglist);
    BOOL hires = screen ? ((screen->ViewPort.Modes & HIRES) != 0) : TRUE;
    struct Image *check = (struct Image *)GetTagData(GTMN_Checkmark, 0, taglist);
    struct Image *amigakey_img = (struct Image *)GetTagData(GTMN_AmigaKey, 0, taglist);
    UBYTE default_pen = 0;

    InitRastPort(&ml->rp);
    ml->textattr = (struct TextAttr *)GetTagData(GTMN_TextAttr, 0, taglist);
    if (!ml->textattr && screen)
        ml->textattr = screen->Font;
    ml->font = ml->textattr ? OpenFont(ml->textattr) : NULL;
    if (ml->font)
        SetFont(&ml->rp, ml->font);
    else if (GfxBase->DefaultFont)
        SetFont(&ml->rp, GfxBase->DefaultFont);
    ml->item_height = ml->rp.TxHeight + 1;

    if (dri && dri->dri_NumPens > BARDETAILPEN)
        default_pen = (UBYTE)dri->dri_Pens[newlook ? BARDETAILPEN : DETAILPEN];
    else
        default_pen = newlook ? 1 : 0;
    ml->front_pen = (UBYTE)GetTagData(GTMN_FrontPen, default_pen, taglist);
    ml->newlook = newlook;

    if (newlook)
    {
        if (!check && dri)
            check = dri->dri_CheckMark;
        if (!amigakey_img && dri)
            amigakey_img = dri->dri_AmigaKey;
        ml->check_width = check ? check->Width : 15;
        ml->comm_width = amigakey_img ? amigakey_img->Width : 23;
    }
    else
    {
        ml->check_width = check ? check->Width : (hires ? CHECKWIDTH : LOWCHECKWIDTH);
        ml->comm_width = amigakey_img ? amigakey_img->Width : (hires ? COMMWIDTH : LOWCOMMWIDTH);
    }

    if (ok)
        *ok = TRUE;
}

static void gt_menu_layout_done(struct gt_menu_layout *ml)
{
    if (ml->font)
        CloseFont(ml->font);
    ml->font = NULL;
}

static BOOL gt_layout_menu_item_chain(struct gt_menu_layout *ml, struct MenuItem *firstitem,
                                      WORD left_edge, WORD top_edge, WORD min_width)
{
    struct MenuItem *item;
    WORD max_label = 0, max_right = 0;
    WORD width;
    WORD y = top_edge;

    for (item = firstitem; item; item = item->NextItem)
    {
        WORD left = 2, text = 0, right = 0;

        if (gt_is_separator_item(item))
            continue;
        if (item->Flags & CHECKIT)
            left = ml->check_width + 2;
        if ((item->Flags & ITEMTEXT) && item->ItemFill)
            text = gt_menu_text_width(ml, ((struct IntuiText *)item->ItemFill)->IText);
        else if (item->ItemFill)
            text = ((struct Image *)item->ItemFill)->Width;
        if ((item->Flags & ITEMTEXT) && item->ItemFill && !item->SubItem &&
            ((struct IntuiText *)item->ItemFill)->NextText)
            /* NM_COMMANDSTRING: the command text right-aligned */
            right = 6 + gt_menu_text_width(ml, ((struct IntuiText *)item->ItemFill)->NextText->IText);
        if (item->Flags & COMMSEQ)
        {
            char cmd[2];
            cmd[0] = (char)item->Command;
            cmd[1] = 0;
            right = 6 + ml->comm_width + gt_menu_text_width(ml, (CONST_STRPTR)cmd);
        }
        if (item->SubItem)
        {
            WORD sub = 6 + gt_menu_text_width(ml, (CONST_STRPTR)GT_SUBMENU_INDICATOR);
            if (sub > right)
                right = sub;
        }
        if (left + text > max_label)
            max_label = left + text;
        if (right > max_right)
            max_right = right;
    }
    if (max_label == 0)
        max_label = 2;

    width = max_label + max_right + 3;
    if (width < min_width)
        width = min_width;

    for (item = firstitem; item; item = item->NextItem)
    {
        item->LeftEdge = left_edge;
        item->TopEdge = y;
        item->Width = width;
        item->Height = gt_is_separator_item(item) ? 6 : ml->item_height;
        if (gt_is_separator_item(item))
        {
            struct Image *im = (struct Image *)item->ItemFill;
            im->LeftEdge = 2;
            im->TopEdge = 2;
            im->Width = width - 4;
            im->Height = 2;
            im->PlaneOnOff = ml->newlook ? ml->front_pen : 0;
        }
        else if (!(item->Flags & ITEMTEXT) && item->ItemFill)
        {
            /* image items (AmigaOS 3.1): image at 2,1, item one line taller */
            struct Image *im = (struct Image *)item->ItemFill;
            im->LeftEdge = (item->Flags & CHECKIT) ? ml->check_width + 2 : 2;
            item->Height = im->TopEdge + im->Height;
        }

        if ((item->Flags & ITEMTEXT) && item->ItemFill)
        {
            struct IntuiText *it = (struct IntuiText *)item->ItemFill;

            it->FrontPen = ml->front_pen;
            it->BackPen = 0;
            it->ITextFont = ml->textattr;
            it->LeftEdge = gt_is_separator_item(item) ? 2 : ((item->Flags & CHECKIT) ? ml->check_width + 2 : 2);
            it->TopEdge = gt_is_separator_item(item) ? 0 : 1;
            if (it->NextText)
            {
                struct IntuiText *ind = it->NextText;

                ind->FrontPen = ml->front_pen;
                ind->ITextFont = ml->textattr;
                ind->TopEdge = 1;
                ind->LeftEdge = width - 2 - gt_menu_text_width(ml, ind->IText);
            }
        }

        if (item->SubItem)
        {
            if (!gt_layout_menu_item_chain(ml, item->SubItem, width - width / 4, -1, 0))
                return FALSE;
        }

        y += item->Height;
    }

    return TRUE;
}

/* CreateMenusA - Create menus from NewMenu array
 *
 * Parses the NewMenu array and creates the corresponding Menu and MenuItem
 * structures. The menu hierarchy is:
 *   Menu (title bar entry) -> MenuItem (dropdown item) -> SubItem (submenu item)
 */
struct Menu * _gadtools_CreateMenusA ( register struct GadToolsBase *GadToolsBase __asm("a6"),
                                       register struct NewMenu *newmenu __asm("a0"),
                                       register struct TagItem *taglist __asm("a1") )
{
    struct Menu *firstMenu = NULL;
    struct Menu *currentMenu = NULL;
    struct Menu *lastMenu = NULL;
    struct MenuItem *currentItem = NULL;
    struct MenuItem *lastItem = NULL;
    struct MenuItem *lastSubItem = NULL;
    struct IntuiText *itext;
    struct NewMenu *nm;
    BOOL menu_image;
    UBYTE front_pen = (UBYTE)GetTagData(GTMN_FrontPen, 0, taglist);
    struct Menu fake;     /* holds an item list without titles */
    BOOL items_only = FALSE;

    DPRINTF (LOG_DEBUG, "_gadtools: CreateMenusA() newmenu=0x%08lx\n", (ULONG)newmenu);

    if (!newmenu)
        return NULL;

    gt_set_menu_error(taglist, 0);

    {
        struct NewMenu *f = newmenu;
        while (f->nm_Type != NM_END && (f->nm_Type & NM_IGNORE))
            f++;
        if ((f->nm_Type & ~MENU_IMAGE) == NM_ITEM)
        {
            /* items without a title: the item list for LayoutMenuItems() */
            items_only = TRUE;
            for (f = newmenu; f->nm_Type != NM_END; f++)
                if (!(f->nm_Type & NM_IGNORE) && f->nm_Type == NM_TITLE)
                    items_only = FALSE;
        }
    }
    if (items_only)
    {
        memset(&fake, 0, sizeof(fake));
        firstMenu = currentMenu = lastMenu = &fake;
    }
    else if (!gt_validate_newmenu_array(newmenu))
    {
        gt_set_menu_error(taglist, GTMENU_INVALID);
        return NULL;
    }

    /* Second pass: create the menu structures */
    for (nm = newmenu; nm->nm_Type != NM_END; nm++) {
        UBYTE type;

        if (nm->nm_Type & NM_IGNORE)
            continue;

        menu_image = ((nm->nm_Type & MENU_IMAGE) != 0);
        type = nm->nm_Type & ~(MENU_IMAGE | NM_IGNORE);

        DPRINTF (LOG_DEBUG, "_gadtools: CreateMenusA: type=%d label='%s'\n",
                 type, nm->nm_Label ? (nm->nm_Label == (STRPTR)-1 ? "(bar)" : (char*)nm->nm_Label) : "(null)");

        switch (type) {
            case NM_TITLE: {
                /* Create a new Menu structure */
                struct Menu *menu = AllocMem(GT_MENU_SIZE, MEMF_CLEAR | MEMF_PUBLIC);
                if (!menu) {
                    /* Out of memory - free what we have and return NULL */
                    gt_set_menu_error(taglist, GTMENU_NOMEM);
                    if (items_only)
                        FreeMenuItems(fake.FirstItem);
                    else if (firstMenu)
                        _gadtools_FreeMenus(GadToolsBase, firstMenu);
                    return NULL;
                }

                /* Geometry stays 0 until LayoutMenusA() (AmigaOS 3.1). */
                GT_MENU_MAGIC_OF(menu) = GT_MENU_MAGIC;
                menu->Flags = MENUENABLED;
                menu->MenuName = nm->nm_Label;
                menu->FirstItem = NULL;
                menu->NextMenu = NULL;

                /* Apply flags from NewMenu */
                if (nm->nm_Flags & NM_MENUDISABLED)
                    menu->Flags &= ~MENUENABLED;

                /* Link to menu chain */
                if (!firstMenu) {
                    firstMenu = menu;
                } else if (lastMenu) {
                    lastMenu->NextMenu = menu;
                }
                GTMENU_USERDATA(menu) = nm->nm_UserData;
                lastMenu = menu;
                currentMenu = menu;
                currentItem = NULL;
                lastItem = NULL;
                lastSubItem = NULL;
                break;
            }

            case NM_ITEM: {
                /* Create a MenuItem for the current menu */
                struct MenuItem *item;

                if (!currentMenu) {
                    DPRINTF (LOG_ERROR, "_gadtools: CreateMenusA: NM_ITEM without NM_TITLE!\n");
                    gt_set_menu_error(taglist, GTMENU_INVALID);
                    if (items_only)
                        FreeMenuItems(fake.FirstItem);
                    else if (firstMenu)
                        _gadtools_FreeMenus(GadToolsBase, firstMenu);
                    return NULL;
                }

                item = gt_alloc_menuitem();
                if (!item) {
                    gt_set_menu_error(taglist, GTMENU_NOMEM);
                    if (items_only)
                        FreeMenuItems(fake.FirstItem);
                    else if (firstMenu)
                        _gadtools_FreeMenus(GadToolsBase, firstMenu);
                    return NULL;
                }

                /* Geometry stays 0 until LayoutMenuItemsA() (AmigaOS 3.1). */
                item->Flags = ITEMTEXT | ITEMENABLED | HIGHCOMP;
                item->MutualExclude = nm->nm_MutualExclude;
                item->NextItem = NULL;
                item->SubItem = NULL;
                item->Command = 0;

                /* Handle bar label (separator) */
                if (nm->nm_Label == NM_BARLABEL) {
                    item->ItemFill = gt_alloc_bar_image();
                    item->Flags = HIGHNONE;   /* image item, not selectable */
                } else if (menu_image) {
                    item->ItemFill = (APTR)nm->nm_Label;
                    item->Flags &= ~ITEMTEXT;
                    if (item->ItemFill)
                        ((struct Image *)item->ItemFill)->TopEdge = 1;
                } else {
                    /* Create IntuiText for the label */
                    itext = gt_alloc_menu_itext((STRPTR)nm->nm_Label);
                    if (itext)
                        itext->FrontPen = front_pen;
                    item->ItemFill = itext;
                }

                /* Handle command key */
                if (nm->nm_CommKey && nm->nm_CommKey[0]) {
                    if ((nm->nm_Flags & NM_COMMANDSTRING) && (item->Flags & ITEMTEXT) && item->ItemFill) {
                        /* the whole string, drawn right-aligned (V39) */
                        struct IntuiText *ct = gt_alloc_menu_itext((STRPTR)nm->nm_CommKey);
                        if (ct)
                            ct->FrontPen = front_pen;
                        ((struct IntuiText *)item->ItemFill)->NextText = ct;
                    } else {
                        item->Flags |= COMMSEQ;
                        item->Command = nm->nm_CommKey[0];
                    }
                }

                /* Handle checkmark */
                if (nm->nm_Flags & CHECKIT) {
                    item->Flags |= CHECKIT;
                    if (nm->nm_Flags & CHECKED)
                        item->Flags |= CHECKED;
                    if (nm->nm_Flags & MENUTOGGLE)
                        item->Flags |= MENUTOGGLE;
                }

                /* Handle disabled items */
                if (nm->nm_Flags & NM_ITEMDISABLED)
                    item->Flags &= ~ITEMENABLED;

                /* Store user data */
                /* Note: On real AmigaOS, UserData is stored in an extended structure */

                /* Link to menu */
                if (!currentMenu->FirstItem) {
                    currentMenu->FirstItem = item;
                } else if (lastItem) {
                    lastItem->NextItem = item;
                }
                GTMENUITEM_USERDATA(item) = nm->nm_UserData;
                lastItem = item;
                currentItem = item;
                lastSubItem = NULL;
                break;
            }

            case NM_SUB: {
                /* Create a sub-menu item for the current item */
                struct MenuItem *subitem;

                if (!currentItem) {
                    DPRINTF (LOG_ERROR, "_gadtools: CreateMenusA: NM_SUB without NM_ITEM!\n");
                    gt_set_menu_error(taglist, GTMENU_INVALID);
                    if (items_only)
                        FreeMenuItems(fake.FirstItem);
                    else if (firstMenu)
                        _gadtools_FreeMenus(GadToolsBase, firstMenu);
                    return NULL;
                }

                subitem = gt_alloc_menuitem();
                if (!subitem) {
                    gt_set_menu_error(taglist, GTMENU_NOMEM);
                    if (items_only)
                        FreeMenuItems(fake.FirstItem);
                    else if (firstMenu)
                        _gadtools_FreeMenus(GadToolsBase, firstMenu);
                    return NULL;
                }

                subitem->Flags = ITEMTEXT | ITEMENABLED | HIGHCOMP;
                subitem->MutualExclude = nm->nm_MutualExclude;
                subitem->NextItem = NULL;
                subitem->SubItem = NULL;
                subitem->Command = 0;

                /* Handle bar label (separator) */
                if (nm->nm_Label == NM_BARLABEL) {
                    subitem->ItemFill = gt_alloc_bar_image();
                    subitem->Flags = HIGHNONE;
                } else if (menu_image) {
                    subitem->ItemFill = (APTR)nm->nm_Label;
                    subitem->Flags &= ~ITEMTEXT;
                    if (subitem->ItemFill)
                        ((struct Image *)subitem->ItemFill)->TopEdge = 1;
                } else {
                    itext = gt_alloc_menu_itext((STRPTR)nm->nm_Label);
                    if (itext)
                        itext->FrontPen = front_pen;
                    subitem->ItemFill = itext;
                }

                /* Handle command key */
                if (nm->nm_CommKey && nm->nm_CommKey[0]) {
                    if ((nm->nm_Flags & NM_COMMANDSTRING) && (subitem->Flags & ITEMTEXT) && subitem->ItemFill) {
                        /* the whole string, drawn right-aligned (V39) */
                        struct IntuiText *ct = gt_alloc_menu_itext((STRPTR)nm->nm_CommKey);
                        if (ct)
                            ct->FrontPen = front_pen;
                        ((struct IntuiText *)subitem->ItemFill)->NextText = ct;
                    } else {
                        subitem->Flags |= COMMSEQ;
                        subitem->Command = nm->nm_CommKey[0];
                    }
                }

                /* Handle checkmark and flags */
                if (nm->nm_Flags & CHECKIT) {
                    subitem->Flags |= CHECKIT;
                    if (nm->nm_Flags & CHECKED)
                        subitem->Flags |= CHECKED;
                    if (nm->nm_Flags & MENUTOGGLE)
                        subitem->Flags |= MENUTOGGLE;
                }

                if (nm->nm_Flags & NM_ITEMDISABLED)
                    subitem->Flags &= ~ITEMENABLED;

                /* Link to parent item */
                if (!currentItem->SubItem) {
                    /* The parent's label gets the sub-menu indicator as
                     * NextText (AmigaOS 3.1: "\xbb", placed by layout). */
                    if ((currentItem->Flags & ITEMTEXT) && currentItem->ItemFill &&
                        !gt_is_separator_item(currentItem))
                        ((struct IntuiText *)currentItem->ItemFill)->NextText =
                            gt_alloc_menu_itext((STRPTR)GT_SUBMENU_INDICATOR);
                    currentItem->SubItem = subitem;
                } else if (lastSubItem) {
                    lastSubItem->NextItem = subitem;
                }
                GTMENUITEM_USERDATA(subitem) = nm->nm_UserData;
                lastSubItem = subitem;
                break;
            }
        }
    }

    if (items_only)
        return (struct Menu *)fake.FirstItem;
    DPRINTF (LOG_DEBUG, "_gadtools: CreateMenusA() -> 0x%08lx\n", (ULONG)firstMenu);
    return firstMenu;
}

/* Helper: Free a chain of menu items recursively */
static void FreeMenuItems(struct MenuItem *item)
{
    while (item) {
        struct MenuItem *next = item->NextItem;

        /* Free sub-items first */
        if (item->SubItem) {
            FreeMenuItems(item->SubItem);
        }

        if (gt_is_separator_item(item))
            FreeMem(item->ItemFill, sizeof(struct Image) + sizeof(ULONG));

        /* Free the IntuiText (and sub-menu indicator) if we created one */
        if ((item->Flags & ITEMTEXT) && item->ItemFill) {
            struct IntuiText *it = (struct IntuiText *)item->ItemFill;
            if (it->NextText)
                FreeMem(it->NextText, sizeof(struct IntuiText));
            FreeMem(it, sizeof(struct IntuiText));
        }

        FreeMem(item, GT_ITEM_SIZE);
        item = next;
    }
}

/* FreeMenus - Free menus created by CreateMenusA */
void _gadtools_FreeMenus ( register struct GadToolsBase *GadToolsBase __asm("a6"),
                           register struct Menu *menu __asm("a0") )
{
    DPRINTF (LOG_DEBUG, "_gadtools: FreeMenus() menu=0x%08lx\n", (ULONG)menu);

    if (menu && GT_MENU_MAGIC_OF(menu) != GT_MENU_MAGIC && GT_ITEM_MAGIC_OF(menu) == GT_ITEM_MAGIC)
    {
        FreeMenuItems((struct MenuItem *)menu);
        return;
    }

    while (menu) {
        struct Menu *nextMenu = menu->NextMenu;

        /* Free all items in this menu */
        if (menu->FirstItem) {
            FreeMenuItems(menu->FirstItem);
        }

        /* Free the menu itself */
        FreeMem(menu, GT_MENU_SIZE);
        menu = nextMenu;
    }
}

/* LayoutMenuItemsA - Layout menu items */
BOOL _gadtools_LayoutMenuItemsA ( register struct GadToolsBase *GadToolsBase __asm("a6"),
                                  register struct MenuItem *firstitem __asm("a0"),
                                  register APTR vi __asm("a1"),
                                  register struct TagItem *taglist __asm("a2") )
{
    struct gt_menu_layout ml;
    BOOL ok;

    DPRINTF (LOG_DEBUG, "_gadtools: LayoutMenuItemsA()\n");

    if (!firstitem)
        return FALSE;

    gt_menu_layout_init(&ml, (struct VisualInfo *)vi, taglist, &ok);
    ok = gt_layout_menu_item_chain(&ml, firstitem, 0, 0, 0);
    gt_menu_layout_done(&ml);
    return ok;
}

/* LayoutMenusA - Layout entire menu structure */
BOOL _gadtools_LayoutMenusA ( register struct GadToolsBase *GadToolsBase __asm("a6"),
                              register struct Menu *firstmenu __asm("a0"),
                              register APTR vi __asm("a1"),
                              register struct TagItem *taglist __asm("a2") )
{
    struct Menu *menu;
    struct gt_menu_layout ml;
    WORD left = 2;
    BOOL ok = TRUE;

    DPRINTF (LOG_DEBUG, "_gadtools: LayoutMenusA()\n");

    if (!firstmenu)
        return FALSE;

    gt_menu_layout_init(&ml, (struct VisualInfo *)vi, taglist, &ok);

    for (menu = firstmenu; menu && ok; menu = menu->NextMenu)
    {
        menu->LeftEdge = left;
        menu->Width = gt_menu_text_width(&ml, menu->MenuName) + 8;

        if (menu->FirstItem)
            ok = gt_layout_menu_item_chain(&ml, menu->FirstItem, 0, 0, menu->Width + 1);

        /* sub-menus that would leave the screen move left (AmigaOS 3.1
         * reference: devpac-edit golden) */
        if (ok && vi && ((struct VisualInfo *)vi)->vi_Screen)
        {
            struct Screen *scr = ((struct VisualInfo *)vi)->vi_Screen;
            WORD limit = scr->Width - menu->LeftEdge - 2 * scr->MenuHBorder - 1;
            struct MenuItem *it, *sub;

            for (it = menu->FirstItem; it; it = it->NextItem)
            {
                WORD maxleft;
                if (!it->SubItem)
                    continue;
                maxleft = limit - it->SubItem->Width;
                if (it->SubItem->LeftEdge > maxleft)
                    for (sub = it->SubItem; sub; sub = sub->NextItem)
                        sub->LeftEdge = maxleft;
            }
        }

        left += menu->Width + 8;
    }

    gt_menu_layout_done(&ml);
    return ok;
}

/*
 * Event Handling Functions
 */

/* GadTools hands the application a private copy of every IntuiMessage
 * (AmigaOS 3.1: GT_GetIMsg()/GT_FilterIMsg() never return the original
 * pointer; GT_PostFilterIMsg() maps the copy back to the original). */
#define GT_IMSG_MAGIC 0x47544D53UL

struct GTIMsgCopy {
    struct ExtIntuiMessage  copy;
    struct IntuiMessage    *original;
    ULONG                   magic;
};

static struct GTIMsgCopy *gt_imsg_copy(struct IntuiMessage *imsg)
{
    struct GTIMsgCopy *c = (struct GTIMsgCopy *)imsg;

    if (c && c->magic == GT_IMSG_MAGIC)
        return c;
    return NULL;
}

/*
 * Messages of the auxiliary gadgets of a GadTools group are translated to
 * the gadget CreateGadget() returned (AmigaOS 3.1 hands the application
 * only that one): listview scrolling is consumed, scroller arrows report
 * the new Top, palettes and listviews their selection.
 * Returns FALSE when GadTools consumed the message.
 */
static BOOL gt_translate_imsg(struct IntuiMessage *m)
{
    struct Gadget *g;
    struct GTGadgetData *data;
    UWORD role;

    if (!(m->Class & (IDCMP_GADGETUP | IDCMP_GADGETDOWN | IDCMP_MOUSEMOVE)) || !m->IAddress)
        return TRUE;
    g = (struct Gadget *)m->IAddress;
    if (m->Class == IDCMP_MOUSEMOVE && m->IDCMPWindow && (APTR)g == (APTR)m->IDCMPWindow)
        return TRUE;
    data = gt_get_data(g);
    if (!data || !data->pub)
        return TRUE;
    role = gt_gad(g) ? gt_gad(g)->role : GT_ROLE_MAIN;

    if (data->kind == GT_KIND_LISTVIEW && role != GT_ROLE_MAIN)
    {
        if (role == GT_ROLE_LVPROP && data->lv_prop && data->lv_prop->SpecialInfo)
        {
            struct PropInfo *pi = (struct PropInfo *)data->lv_prop->SpecialInfo;
            LONG hidden = gt_list_count((struct List *)data->aux) - gt_lv_visible(data);
            if (hidden > 0)
                data->lv_top = (WORD)(((ULONG)pi->VertPot * (ULONG)hidden + 0x7FFF) / 0xFFFF);
            gt_lv_clamp_top(data);
            if (m->IDCMPWindow)
                RefreshGList(data->pub, m->IDCMPWindow, NULL, 1);
        }
        return FALSE;   /* scrolling a listview is GadTools' business */
    }
    if (data->kind == GT_KIND_SCROLLER)
    {
        if (role == GT_ROLE_MAIN && data->main->SpecialInfo)
        {
            struct PropInfo *pi = (struct PropInfo *)data->main->SpecialInfo;
            UWORD pot = (pi->Flags & FREEVERT) ? pi->VertPot : pi->HorizPot;
            LONG hidden = data->max - data->min;
            data->value = hidden > 0 ? (LONG)(((ULONG)pot * (ULONG)hidden + 0x7FFF) / 0xFFFF) : 0;
        }
        m->Code = (UWORD)data->value;
    }
    else if (data->kind == GT_KIND_PALETTE ||
             (data->kind == GT_KIND_LISTVIEW && m->Class == IDCMP_GADGETUP))
        m->Code = (UWORD)data->value;
    m->IAddress = (APTR)data->pub;
    return TRUE;
}

static struct IntuiMessage *gt_filter_imsg(struct IntuiMessage *imsg)
{
    struct GTIMsgCopy *c;

    if (!imsg)
        return NULL;

    c = AllocMem(sizeof(struct GTIMsgCopy), MEMF_PUBLIC | MEMF_CLEAR);
    if (!c)
        return imsg;  /* out of memory: hand out the original */

    CopyMem(imsg, &c->copy, sizeof(struct IntuiMessage));
    c->original = imsg;
    c->magic = GT_IMSG_MAGIC;
    if (!gt_translate_imsg(&c->copy.eim_IntuiMessage))
    {
        c->magic = 0;
        FreeMem(c, sizeof(struct GTIMsgCopy));
        return NULL;
    }
    return &c->copy.eim_IntuiMessage;
}

static struct IntuiMessage *gt_postfilter_imsg(struct IntuiMessage *imsg)
{
    struct GTIMsgCopy *c = gt_imsg_copy(imsg);
    struct IntuiMessage *orig;

    if (!c)
        return imsg;

    orig = c->original;
    c->magic = 0;
    FreeMem(c, sizeof(struct GTIMsgCopy));
    return orig;
}

/* GT_GetIMsg - Get an IntuiMessage, filtering GadTools messages */
struct IntuiMessage * _gadtools_GT_GetIMsg ( register struct GadToolsBase *GadToolsBase __asm("a6"),
                                             register struct MsgPort *iport __asm("a0") )
{
    struct IntuiMessage *imsg;

    DPRINTF (LOG_DEBUG, "_gadtools: GT_GetIMsg() iport=0x%08lx\n", (ULONG)iport);

    if (!iport)
        return NULL;

    while ((imsg = (struct IntuiMessage *)GetMsg(iport)) != NULL)
    {
        struct IntuiMessage *filtered = gt_filter_imsg(imsg);

        if (filtered)
            return filtered;
        ReplyMsg((struct Message *)imsg);
    }

    return NULL;
}

/* GT_ReplyIMsg - Reply to an IntuiMessage from GT_GetIMsg */
void _gadtools_GT_ReplyIMsg ( register struct GadToolsBase *GadToolsBase __asm("a6"),
                              register struct IntuiMessage *imsg __asm("a1") )
{
    DPRINTF (LOG_DEBUG, "_gadtools: GT_ReplyIMsg() imsg=0x%08lx\n", (ULONG)imsg);
    if (imsg) {
        ReplyMsg((struct Message *)gt_postfilter_imsg(imsg));
    }
}

/* GT_RefreshWindow - Refresh all GadTools gadgets in a window */
void _gadtools_GT_RefreshWindow ( register struct GadToolsBase *GadToolsBase __asm("a6"),
                                  register struct Window *win __asm("a0"),
                                  register struct Requester *req __asm("a1") )
{
    struct Gadget *gadgets;

    DPRINTF (LOG_DEBUG, "_gadtools: GT_RefreshWindow() win=0x%08lx\n", (ULONG)win);

    if (!win)
        return;

    gadgets = req ? req->ReqGadget : win->FirstGadget;
    gadgets = gt_public_glist(gadgets);
    if (!gadgets)
        return;

    RefreshGList(gadgets, win, req, -1);
}

/* GT_BeginRefresh - Begin a refresh operation */
void _gadtools_GT_BeginRefresh ( register struct GadToolsBase *GadToolsBase __asm("a6"),
                                 register struct Window *win __asm("a0") )
{
    DPRINTF (LOG_DEBUG, "_gadtools: GT_BeginRefresh() win=0x%08lx\n", (ULONG)win);

    if (!win)
        return;

    BeginRefresh(win);
}

/* GT_EndRefresh - End a refresh operation */
void _gadtools_GT_EndRefresh ( register struct GadToolsBase *GadToolsBase __asm("a6"),
                               register struct Window *win __asm("a0"),
                               register BOOL complete __asm("d0") )
{
    DPRINTF (LOG_DEBUG, "_gadtools: GT_EndRefresh() win=0x%08lx, complete=%d\n", (ULONG)win, complete);

    if (!win)
        return;

    EndRefresh(win, complete);
}

/* GT_FilterIMsg - Filter an IntuiMessage */
struct IntuiMessage * _gadtools_GT_FilterIMsg ( register struct GadToolsBase *GadToolsBase __asm("a6"),
                                                register struct IntuiMessage *imsg __asm("a1") )
{
    LXA_UNIMPLEMENTED("gadtools", "GT_FilterIMsg", "partial: hands out a private copy but never consumes GadTools-internal gadget events (Phase 256)");

    DPRINTF (LOG_DEBUG, "_gadtools: GT_FilterIMsg() imsg=0x%08lx\n", (ULONG)imsg);
    return gt_filter_imsg(imsg);
}

/* GT_PostFilterIMsg - Post-filter an IntuiMessage */
struct IntuiMessage * _gadtools_GT_PostFilterIMsg ( register struct GadToolsBase *GadToolsBase __asm("a6"),
                                                    register struct IntuiMessage *imsg __asm("a1") )
{
    DPRINTF (LOG_DEBUG, "_gadtools: GT_PostFilterIMsg() imsg=0x%08lx\n", (ULONG)imsg);
    return gt_postfilter_imsg(imsg);
}

/* CreateContext - Create a gadget list context */
struct Gadget * _gadtools_CreateContext ( register struct GadToolsBase *GadToolsBase __asm("a6"),
                                          register struct Gadget **glistptr __asm("a0") )
{
    struct GTGad *block;
    struct Gadget *context;

    DPRINTF (LOG_DEBUG, "_gadtools: CreateContext() glistptr=0x%08lx\n", (ULONG)glistptr);

    if (!glistptr)
        return NULL;

    block = AllocMem(sizeof(struct GTGad), MEMF_CLEAR | MEMF_PUBLIC);
    if (!block)
        return NULL;

    context = (struct Gadget *)&block->eg;
    block->magic = GT_GAD_MAGIC;
    block->role = GT_ROLE_CONTEXT;

    /* Invisible, zero-size placeholder that stays in the window's gadget
     * list (it is the list head the application passes to WA_Gadgets). */
    context->GadgetType = LXA_GTYP_GADTOOLS;
    context->Flags = GFLG_GADGHNONE;

    *glistptr = context;

    DPRINTF (LOG_DEBUG, "_gadtools: CreateContext() -> 0x%08lx\n", (ULONG)context);
    return context;
}

/*
 * Rendering Functions
 */

/* DrawBevelBoxA - Draw a 3D beveled box
 *
 * Supported tags:
 *   GTBB_Recessed  - Draw recessed (sunken) instead of raised box
 *   GT_VisualInfo  - VisualInfo for pen colors
 *   GTBB_FrameType - Frame type (BBFT_BUTTON, BBFT_RIDGE, etc.)
 */
void _gadtools_DrawBevelBoxA ( register struct GadToolsBase *GadToolsBase __asm("a6"),
                               register struct RastPort *rport __asm("a0"),
                               register WORD left __asm("d0"),
                               register WORD top __asm("d1"),
                               register WORD width __asm("d2"),
                               register WORD height __asm("d3"),
                               register struct TagItem *taglist __asm("a1") )
{
    struct VisualInfo *vi = NULL;
    BOOL recessed = FALSE;
    ULONG frameType = BBFT_BUTTON;
    const UWORD *pens = NULL;
    UBYTE savedAPen;
    struct TagItem *tag;

    DPRINTF (LOG_DEBUG, "_gadtools: DrawBevelBoxA() at %d,%d size %dx%d\n",
             left, top, width, height);

    if (!rport || width <= 0 || height <= 0)
        return;

    /* Parse tags manually without NextTagItem to avoid UtilityBase dependency */
    if (taglist) {
        for (tag = taglist; tag->ti_Tag != TAG_DONE; tag++) {
            if (tag->ti_Tag == TAG_SKIP) {
                tag += tag->ti_Data;
                continue;
            }
            if (tag->ti_Tag == TAG_IGNORE)
                continue;
            if (tag->ti_Tag == TAG_MORE) {
                tag = (struct TagItem *)tag->ti_Data;
                if (!tag) break;
                tag--;  /* Will be incremented by loop */
                continue;
            }

            switch (tag->ti_Tag) {
                case GTBB_Recessed:
                    /* AmigaOS 3.1 reference: the presence of the tag makes
                     * the box recessed, whatever its value */
                    recessed = TRUE;
                    break;
                case GT_VisualInfo:
                    vi = (struct VisualInfo *)tag->ti_Data;
                    break;
                case GTBB_FrameType:
                    frameType = tag->ti_Data;
                    break;
            }
        }
    }

    if (vi && vi->vi_DrawInfo && vi->vi_DrawInfo->dri_Pens)
        pens = vi->vi_DrawInfo->dri_Pens;

    savedAPen = rport->FgPen;

    /* BBFT_* match the frameiclass FRAME_* types (BUTTON 1, RIDGE 2,
     * ICONDROPBOX 3); the box is drawn exactly like the frameiclass frame */
    switch (frameType) {
        case BBFT_RIDGE:
            lxa_draw_frame(rport, FRAME_RIDGE, recessed, left, top, width, height,
                           IDS_NORMAL, TRUE, pens);
            break;
        case BBFT_ICONDROPBOX:
            lxa_draw_frame(rport, FRAME_ICONDROPBOX, recessed, left, top, width, height,
                           IDS_NORMAL, TRUE, pens);
            break;
        default:
            lxa_draw_frame(rport, FRAME_BUTTON, recessed, left, top, width, height,
                           IDS_NORMAL, TRUE, pens);
            break;
    }

    /* Restore pen */
    SetAPen(rport, savedAPen);
}

/*
 * VisualInfo Functions
 */

/* GetVisualInfoA - Get visual info for a screen */
APTR _gadtools_GetVisualInfoA ( register struct GadToolsBase *GadToolsBase __asm("a6"),
                                register struct Screen *screen __asm("a0"),
                                register struct TagItem *taglist __asm("a1") )
{
    struct VisualInfo *vi;

    DPRINTF (LOG_DEBUG, "_gadtools: GetVisualInfoA() screen=0x%08lx\n", (ULONG)screen);

    vi = AllocMem(sizeof(struct VisualInfo), MEMF_CLEAR | MEMF_PUBLIC);
    if (!vi)
        return NULL;

    vi->vi_Screen = screen;
    /* the screen's pens - preferences pens on the Workbench (Phase 236) */
    vi->vi_DrawInfo = screen ? GetScreenDrawInfo(screen) : NULL;

    DPRINTF (LOG_DEBUG, "_gadtools: GetVisualInfoA() -> 0x%08lx\n", (ULONG)vi);
    return vi;
}

/* FreeVisualInfo - Free visual info */
void _gadtools_FreeVisualInfo ( register struct GadToolsBase *GadToolsBase __asm("a6"),
                                register APTR vi __asm("a0") )
{
    DPRINTF (LOG_DEBUG, "_gadtools: FreeVisualInfo() vi=0x%08lx\n", (ULONG)vi);
    if (vi) {
        struct VisualInfo *v = (struct VisualInfo *)vi;
        if (v->vi_DrawInfo && v->vi_Screen)
            FreeScreenDrawInfo(v->vi_Screen, v->vi_DrawInfo);
        FreeMem(vi, sizeof(struct VisualInfo));
    }
}

/*
 * Private/Reserved Functions
 */

static void _gadtools_Private1 ( void ) { PRIVATE_FUNCTION_ERROR("_gadtools", "Private1"); }
static void _gadtools_Private2 ( void ) { PRIVATE_FUNCTION_ERROR("_gadtools", "Private2"); }
static void _gadtools_Private3 ( void ) { PRIVATE_FUNCTION_ERROR("_gadtools", "Private3"); }
static void _gadtools_Private4 ( void ) { PRIVATE_FUNCTION_ERROR("_gadtools", "Private4"); }
static void _gadtools_Private5 ( void ) { PRIVATE_FUNCTION_ERROR("_gadtools", "Private5"); }
static void _gadtools_Private6 ( void ) { PRIVATE_FUNCTION_ERROR("_gadtools", "Private6"); }

/* GT_GetGadgetAttrsA - Get gadget attributes (V39+) */
LONG _gadtools_GT_GetGadgetAttrsA ( register struct GadToolsBase *GadToolsBase __asm("a6"),
                                    register struct Gadget *gad __asm("a0"),
                                    register struct Window *win __asm("a1"),
                                    register struct Requester *req __asm("a2"),
                                    register struct TagItem *taglist __asm("a3") )
{
    struct GTGadgetData *data;
    LONG count = 0;
    struct TagItem *tag;

    DPRINTF (LOG_DEBUG, "_gadtools: GT_GetGadgetAttrsA() gad=0x%08lx\n", (ULONG)gad);

    if (!gad || !taglist)
        return 0;

    data = gt_get_data(gad);
    gad = gt_main(gad);

    for (tag = taglist; tag; )
    {
        switch (tag->ti_Tag)
        {
            case TAG_DONE:
                return count;

            case TAG_IGNORE:
                tag++;
                continue;

            case TAG_SKIP:
                tag += tag->ti_Data + 1;
                continue;

            case TAG_MORE:
                tag = (struct TagItem *)tag->ti_Data;
                continue;
        }

        if (!tag->ti_Data)
        {
            tag++;
            continue;
        }

        switch (tag->ti_Tag)
        {
            case GTCB_Checked:
                if (data && data->kind == GT_KIND_CHECKBOX)
                {
                    *(ULONG *)tag->ti_Data = data->value ? TRUE : FALSE;
                    count++;
                }
                break;

            case GTCY_Active:
                if (data)
                {
                    *(ULONG *)tag->ti_Data = (ULONG)data->value;
                    count++;
                }
                break;

            case GTSL_Min:
                if (data)
                {
                    *(ULONG *)tag->ti_Data = (ULONG)data->min;
                    count++;
                }
                break;

            case GTSL_Max:
                if (data)
                {
                    *(ULONG *)tag->ti_Data = (ULONG)data->max;
                    count++;
                }
                break;

            case GTSL_Level:
                if (data)
                {
                    *(ULONG *)tag->ti_Data = (ULONG)data->value;
                    count++;
                }
                break;

            case GTIN_Number:
                if (gad->SpecialInfo)
                {
                    struct StringInfo *si = (struct StringInfo *)gad->SpecialInfo;
                    *(ULONG *)tag->ti_Data = (ULONG)si->LongInt;
                    count++;
                }
                break;

            case GTST_String:
                if (gad->SpecialInfo)
                {
                    struct StringInfo *si = (struct StringInfo *)gad->SpecialInfo;
                    *(STRPTR *)tag->ti_Data = si->Buffer;
                    count++;
                }
                break;
        }

        tag++;
    }

    return count;
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

extern APTR              __g_lxa_gadtools_FuncTab [];
extern struct MyDataInit __g_lxa_gadtools_DataTab;
extern struct InitTable  __g_lxa_gadtools_InitTab;
extern APTR              __g_lxa_gadtools_EndResident;

static struct Resident __aligned ROMTag =
{
    RTC_MATCHWORD,                      // UWORD rt_MatchWord
    &ROMTag,                            // struct Resident *rt_MatchTag
    &__g_lxa_gadtools_EndResident,      // APTR  rt_EndSkip
    RTF_AUTOINIT,                       // UBYTE rt_Flags
    VERSION,                            // UBYTE rt_Version
    NT_LIBRARY,                         // UBYTE rt_Type
    0,                                  // BYTE  rt_Pri
    &_g_gadtools_ExLibName[0],          // char  *rt_Name
    &_g_gadtools_ExLibID[0],            // char  *rt_IdString
    &__g_lxa_gadtools_InitTab           // APTR  rt_Init
};

APTR __g_lxa_gadtools_EndResident;
struct Resident *__lxa_gadtools_ROMTag = &ROMTag;

struct InitTable __g_lxa_gadtools_InitTab =
{
    (ULONG)               sizeof(struct GadToolsBase),
    (APTR              *) &__g_lxa_gadtools_FuncTab[0],
    (APTR)                &__g_lxa_gadtools_DataTab,
    (APTR)                __g_lxa_gadtools_InitLib
};

/* Function table - from gadtools_lib.fd
 * Bias starts at -30 (first function after standard lib funcs)
 */
APTR __g_lxa_gadtools_FuncTab [] =
{
    __g_lxa_gadtools_OpenLib,           // -6   Standard
    __g_lxa_gadtools_CloseLib,          // -12  Standard
    __g_lxa_gadtools_ExpungeLib,        // -18  Standard
    __g_lxa_gadtools_ExtFuncLib,        // -24  Standard (reserved)
    _gadtools_CreateGadgetA,            // -30  CreateGadgetA
    _gadtools_FreeGadgets,              // -36  FreeGadgets
    _gadtools_GT_SetGadgetAttrsA,       // -42  GT_SetGadgetAttrsA
    _gadtools_CreateMenusA,             // -48  CreateMenusA
    _gadtools_FreeMenus,                // -54  FreeMenus
    _gadtools_LayoutMenuItemsA,         // -60  LayoutMenuItemsA
    _gadtools_LayoutMenusA,             // -66  LayoutMenusA
    _gadtools_GT_GetIMsg,               // -72  GT_GetIMsg
    _gadtools_GT_ReplyIMsg,             // -78  GT_ReplyIMsg
    _gadtools_GT_RefreshWindow,         // -84  GT_RefreshWindow
    _gadtools_GT_BeginRefresh,          // -90  GT_BeginRefresh
    _gadtools_GT_EndRefresh,            // -96  GT_EndRefresh
    _gadtools_GT_FilterIMsg,            // -102 GT_FilterIMsg
    _gadtools_GT_PostFilterIMsg,        // -108 GT_PostFilterIMsg
    _gadtools_CreateContext,            // -114 CreateContext
    _gadtools_DrawBevelBoxA,            // -120 DrawBevelBoxA
    _gadtools_GetVisualInfoA,           // -126 GetVisualInfoA
    _gadtools_FreeVisualInfo,           // -132 FreeVisualInfo
    _gadtools_Private1,                 // -138 SetDesignFontA (V47) - stub
    _gadtools_Private2,                 // -144 ScaleGadgetRectA (V47) - stub
    _gadtools_Private3,                 // -150 gadtoolsPrivate1
    _gadtools_Private4,                 // -156 gadtoolsPrivate2
    _gadtools_Private5,                 // -162 gadtoolsPrivate3
    _gadtools_Private6,                 // -168 gadtoolsPrivate4
    _gadtools_GT_GetGadgetAttrsA,       // -174 GT_GetGadgetAttrsA (V39)
    (APTR) ((LONG)-1)
};

struct MyDataInit __g_lxa_gadtools_DataTab =
{
    /* ln_Type      */ INITBYTE(OFFSET(Node,         ln_Type),      NT_LIBRARY),
    /* ln_Name      */ 0x80, (UBYTE) (ULONG) OFFSET(Node,    ln_Name), (ULONG) &_g_gadtools_ExLibName[0],
    /* lib_Flags    */ INITBYTE(OFFSET(Library,      lib_Flags),    LIBF_SUMUSED|LIBF_CHANGED),
    /* lib_Version  */ INITWORD(OFFSET(Library,      lib_Version),  VERSION),
    /* lib_Revision */ INITWORD(OFFSET(Library,      lib_Revision), REVISION),
    /* lib_IdString */ 0x80, (UBYTE) (ULONG) OFFSET(Library, lib_IdString), (ULONG) &_g_gadtools_ExLibID[0],
    (ULONG) 0
};
