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

#define VERSION    39
#define REVISION   1
#define EXLIBNAME  "gadtools"
#define EXLIBVER   " 39.1 (2025/02/02)"

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

/* GadgetContext - internal context for gadget list creation */
struct GadgetContext {
    ULONG magic;
    struct Gadget *gc_Last;
};

#define GT_CONTEXT_MAGIC 0x47544358UL

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
    GT_KIND_NUMBER
};

struct GTGadgetData
{
    ULONG kind;
    LONG value;
    LONG min;
    LONG max;
    APTR aux;
    struct IntuiText *level_text;
    STRPTR level_buffer;
    STRPTR format;
    ULONG max_level_len;
    ULONG max_pixel_len;
    UWORD level_place;
    UBYTE justification;
    LONG (*disp_func)(struct Gadget *, LONG);
};

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

static struct GadgetContext *gt_get_context(struct Gadget *gad)
{
    struct GadgetContext *context;

    if (!gad || gad->GadgetType != GTYP_CUSTOMGADGET)
        return NULL;

    context = (struct GadgetContext *)gad->SpecialInfo;
    if (!context || context->magic != GT_CONTEXT_MAGIC)
        return NULL;

    return context;
}

static BOOL gt_is_context_gadget(struct Gadget *gad)
{
    return gt_get_context(gad) != NULL;
}

static struct Gadget *gt_public_glist(struct Gadget *gad)
{
    struct GadgetContext *context = gt_get_context(gad);

    if (!context)
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
        data->kind = kind;

    return data;
}

static struct GTGadgetData *gt_get_data(struct Gadget *gad)
{
    return (struct GTGadgetData *)gad->SelectRender;
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

static struct IntuiText *gt_label_chain_tail(struct IntuiText *it)
{
    if (!it)
        return NULL;

    while (it->NextText)
        it = it->NextText;

    return it;
}

static void gt_position_slider_level_text(struct GTGadgetData *data,
                                          struct IntuiText *level_text,
                                          WORD gadWidth,
                                          WORD gadHeight)
{
    ULONG place;
    WORD text_width;
    WORD text_left;

    if (!data || !level_text)
        return;

    place = data->level_place;
    if (!place)
        place = PLACETEXT_LEFT;

    text_width = gt_strlen(level_text->IText) * 8;
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
        level_text->LeftEdge = -((WORD)data->max_pixel_len) - GT_INTERWIDTH + text_left;
        level_text->TopEdge  = (gadHeight - GT_FONT_HEIGHT) / 2;
    }
    else if (place & PLACETEXT_RIGHT)
    {
        level_text->LeftEdge = gadWidth + GT_INTERWIDTH + text_left;
        level_text->TopEdge  = (gadHeight - GT_FONT_HEIGHT) / 2;
    }
    else if (place & PLACETEXT_ABOVE)
    {
        level_text->LeftEdge = (gadWidth - (WORD)data->max_pixel_len) / 2 + text_left;
        level_text->TopEdge  = -GT_FONT_HEIGHT - 2;
    }
    else
    {
        level_text->LeftEdge = (gadWidth - (WORD)data->max_pixel_len) / 2 + text_left;
        level_text->TopEdge  = gadHeight + 3;
    }
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
    if (!data || data->kind != GT_KIND_SLIDER || !data->level_text || !data->level_buffer)
        return;

    data->value = level;
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

/* Helper: compute display width of a label, accounting for GT_Underscore.
 * The underscore prefix character marks the next character for underlining
 * and is NOT displayed. For width calculation, subtract 8px per underscore.
 * 'us' is the underscore character (usually '_'), or 0 for none.
 */
static WORD gt_label_width(CONST_STRPTR label, UBYTE us)
{
    WORD len;
    if (!label) return 0;
    len = gt_strlen(label);
    if (us)
    {
        CONST_STRPTR s = label;
        while (*s)
        {
            if (*s == us)
                len--;
            s++;
        }
    }
    return len * 8;  /* topaz 8: 8px per character */
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
        if (label[src] == us)
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

/* Helper: create an IntuiText label from ng_GadgetText with positioning
 * based on ng_Flags and the gadget dimensions.
 *
 * Returns allocated IntuiText or NULL.
 * LeftEdge/TopEdge are set relative to the gadget's (LeftEdge, TopEdge).
 *
 * 'us' is the GT_Underscore character (e.g. '_'), or 0 if not set.
 * When 'us' is set, the underscore prefix is stripped from the displayed text
 * and an underline is drawn under the shortcut character.
 *
 * Per v40 source (textsupp.c):
 * - PLACETEXT_LEFT:  text right-aligned to left of gadget
 * - PLACETEXT_RIGHT: text left-aligned to right of gadget
 * - PLACETEXT_ABOVE: text centered above gadget
 * - PLACETEXT_BELOW: text centered below gadget
 * - PLACETEXT_IN:    text centered inside gadget
 * - Default placement is kind-specific (BUTTON=IN, STRING=LEFT, SLIDER=LEFT)
 */
static struct IntuiText * gt_create_label(CONST_STRPTR text, ULONG flags,
                                           ULONG defaultPlace,
                                           WORD gadWidth, WORD gadHeight,
                                           UBYTE us)
{
    struct IntuiText *it;
    ULONG place;
    WORD textWidth, textHeight;
    STRPTR displayText;
    WORD ul_pos;

    if (!text) return NULL;

    it = (struct IntuiText *)AllocMem(sizeof(struct IntuiText), MEMF_CLEAR | MEMF_PUBLIC);
    if (!it) return NULL;

    /* If GT_Underscore is set, create a stripped copy of the label.
     * Otherwise, create a plain copy of the label.
     * We always allocate a copy so FreeGadgets can always free it. */
    if (us)
    {
        displayText = gt_strip_underscore(text, us, &ul_pos);
        if (!displayText)
        {
            FreeMem(it, sizeof(struct IntuiText));
            return NULL;
        }
    }
    else
    {
        WORD slen = gt_strlen(text);
        WORD i;
        displayText = (STRPTR)AllocMem(slen + 1, MEMF_PUBLIC);
        if (!displayText)
        {
            FreeMem(it, sizeof(struct IntuiText));
            return NULL;
        }
        for (i = 0; i < slen; i++)
            displayText[i] = text[i];
        displayText[slen] = '\0';
        ul_pos = -1;
    }

    /* Use TEXTPEN (1) normally, HIGHLIGHTTEXTPEN (2) for NG_HIGHLABEL */
    it->FrontPen  = (flags & NG_HIGHLABEL) ? 2 : 1;
    it->BackPen   = 0;
    it->DrawMode  = JAM1;
    it->ITextFont = NULL;    /* Use default font */
    it->IText     = displayText;
    it->NextText  = NULL;

    /* Determine placement */
    place = flags & (PLACETEXT_LEFT | PLACETEXT_RIGHT | PLACETEXT_ABOVE |
                     PLACETEXT_BELOW | PLACETEXT_IN);
    if (!place)
        place = defaultPlace;

    textWidth  = gt_label_width(text, us);
    textHeight = GT_FONT_HEIGHT;

    /*
     * IntuiText.TopEdge is the top edge of the character cell, NOT the
     * baseline.  The renderer (_render_gadget in lxa_intuition.c) adds
     * tf_Baseline before calling Move()/Text().  Therefore we must NOT
     * add GT_FONT_BASELINE here — doing so would double-offset the text.
     */
    if (place & PLACETEXT_LEFT)
    {
        it->LeftEdge = -textWidth - GT_INTERWIDTH;
        it->TopEdge  = (gadHeight - textHeight) / 2;
    }
    else if (place & PLACETEXT_RIGHT)
    {
        it->LeftEdge = gadWidth + GT_INTERWIDTH;
        it->TopEdge  = (gadHeight - textHeight) / 2;
    }
    else if (place & PLACETEXT_ABOVE)
    {
        it->LeftEdge = (gadWidth - textWidth) / 2;
        it->TopEdge  = -textHeight - 2;
    }
    else if (place & PLACETEXT_BELOW)
    {
        it->LeftEdge = (gadWidth - textWidth) / 2;
        it->TopEdge  = gadHeight + 3;
    }
    else /* PLACETEXT_IN */
    {
        it->LeftEdge = (gadWidth - textWidth) / 2;
        it->TopEdge  = (gadHeight - textHeight) / 2;
    }

    /* If we have an underline position, create a second IntuiText linked via
     * NextText to draw an underline under the shortcut character.
     * The underline is a horizontal line 1 pixel below the baseline. */
    if (ul_pos >= 0)
    {
        struct IntuiText *ulText;
        STRPTR ul_char;

        ulText = (struct IntuiText *)AllocMem(sizeof(struct IntuiText), MEMF_CLEAR | MEMF_PUBLIC);
        ul_char = gt_strdup((CONST_STRPTR)"_");
        if (ulText && ul_char)
        {
            ulText->FrontPen  = it->FrontPen;
            ulText->BackPen   = it->BackPen;
            ulText->DrawMode  = JAM1;
            ulText->ITextFont = NULL;
            ulText->IText     = ul_char;
            ulText->NextText  = NULL;
            /* Position relative to the main text: offset by ul_pos characters */
            ulText->LeftEdge  = it->LeftEdge + ul_pos * 8;
            ulText->TopEdge   = it->TopEdge;
            it->NextText = ulText;
        }
        else
        {
            if (ul_char)
                FreeMem(ul_char, 2);
            if (ulText)
                FreeMem(ulText, sizeof(struct IntuiText));
        }
    }

    return it;
}

/* Helper: create a bevel box Border for a gadget.
 *
 * For raised (button): shine on top-left, shadow on bottom-right.
 * For recessed (string): shadow on top-left, shine on bottom-right.
 *
 * We create two linked Border structs: one for top-left edges, one for bottom-right.
 * Each Border has a set of XY coordinate pairs for its line segments.
 *
 * Memory layout: Border1 + XY1[6] + Border2 + XY2[6]
 */
static struct Border * gt_create_bevel(WORD width, WORD height, BOOL recessed)
{
    struct Border *b1, *b2;
    WORD *xy1, *xy2;
    APTR mem;
    ULONG size;

    /* We need: 2 Border structs + 2 sets of 6 WORD coords (3 points each, 2 WORDs per point) */
    size = 2 * sizeof(struct Border) + 2 * (6 * sizeof(WORD));
    mem = AllocMem(size, MEMF_CLEAR | MEMF_PUBLIC);
    if (!mem) return NULL;

    b1 = (struct Border *)mem;
    xy1 = (WORD *)((UBYTE *)mem + sizeof(struct Border));
    b2 = (struct Border *)((UBYTE *)xy1 + 6 * sizeof(WORD));
    xy2 = (WORD *)((UBYTE *)b2 + sizeof(struct Border));

    /* Top-left edge: 3 points forming an L shape (bottom-left -> top-left -> top-right) */
    xy1[0] = 0;           xy1[1] = height - 1;  /* bottom-left */
    xy1[2] = 0;           xy1[3] = 0;            /* top-left */
    xy1[4] = width - 1;   xy1[5] = 0;            /* top-right */

    b1->LeftEdge = 0;
    b1->TopEdge  = 0;
    b1->FrontPen = recessed ? 1 : 2;  /* shadow(1) for recessed, shine(2) for raised */
    b1->BackPen  = 0;
    b1->DrawMode = JAM1;
    b1->Count    = 3;
    b1->XY       = xy1;
    b1->NextBorder = b2;

    /* Bottom-right edge: 3 points forming an L shape (top-right -> bottom-right -> bottom-left) */
    xy2[0] = width - 1;   xy2[1] = 0;            /* top-right */
    xy2[2] = width - 1;   xy2[3] = height - 1;   /* bottom-right */
    xy2[4] = 0;            xy2[5] = height - 1;   /* bottom-left */

    b2->LeftEdge = 0;
    b2->TopEdge  = 0;
    b2->FrontPen = recessed ? 2 : 1;  /* shine(2) for recessed, shadow(1) for raised */
    b2->BackPen  = 0;
    b2->DrawMode = JAM1;
    b2->Count    = 3;
    b2->XY       = xy2;
    b2->NextBorder = NULL;

    return b1;
}

/* Helper: create a double-bevel (ridge/groove) border for STRING_KIND gadgets.
 *
 * On a real Amiga, GadTools string gadgets use a FRAME_RIDGE style border:
 *   Outer bevel (recessed): SHADOWPEN top-left, SHINEPEN bottom-right
 *   Inner bevel (raised):   SHINEPEN top-left, SHADOWPEN bottom-right
 * This creates a groove effect: dark-light-content-dark-light
 *
 * Memory layout: 4 Border structs + 4 sets of 6 WORD coords (3 points each)
 */
static struct Border * gt_create_ridge_bevel(WORD width, WORD height)
{
    struct Border *b1, *b2, *b3, *b4;
    WORD *xy1, *xy2, *xy3, *xy4;
    APTR mem;
    ULONG size;
    ULONG border_plus_xy = sizeof(struct Border) + 6 * sizeof(WORD);

    /* We need: 4 Border structs + 4 sets of 6 WORD coords */
    size = 4 * border_plus_xy;
    mem = AllocMem(size, MEMF_CLEAR | MEMF_PUBLIC);
    if (!mem) return NULL;

    b1  = (struct Border *)mem;
    xy1 = (WORD *)((UBYTE *)b1 + sizeof(struct Border));
    b2  = (struct Border *)((UBYTE *)b1 + border_plus_xy);
    xy2 = (WORD *)((UBYTE *)b2 + sizeof(struct Border));
    b3  = (struct Border *)((UBYTE *)b2 + border_plus_xy);
    xy3 = (WORD *)((UBYTE *)b3 + sizeof(struct Border));
    b4  = (struct Border *)((UBYTE *)b3 + border_plus_xy);
    xy4 = (WORD *)((UBYTE *)b4 + sizeof(struct Border));

    /* Outer top-left L (recessed = SHADOWPEN) */
    xy1[0] = 0;           xy1[1] = height - 1;
    xy1[2] = 0;           xy1[3] = 0;
    xy1[4] = width - 1;   xy1[5] = 0;

    b1->FrontPen   = 1;  /* SHADOWPEN */
    b1->DrawMode   = JAM1;
    b1->Count      = 3;
    b1->XY         = xy1;
    b1->NextBorder = b2;

    /* Outer bottom-right L (recessed = SHINEPEN) */
    xy2[0] = width - 1;   xy2[1] = 0;
    xy2[2] = width - 1;   xy2[3] = height - 1;
    xy2[4] = 0;           xy2[5] = height - 1;

    b2->FrontPen   = 2;  /* SHINEPEN */
    b2->DrawMode   = JAM1;
    b2->Count      = 3;
    b2->XY         = xy2;
    b2->NextBorder = b3;

    /* Inner top-left L (raised = SHINEPEN), inset by 1 pixel */
    xy3[0] = 1;               xy3[1] = height - 2;
    xy3[2] = 1;               xy3[3] = 1;
    xy3[4] = width - 2;       xy3[5] = 1;

    b3->FrontPen   = 2;  /* SHINEPEN */
    b3->DrawMode   = JAM1;
    b3->Count      = 3;
    b3->XY         = xy3;
    b3->NextBorder = b4;

    /* Inner bottom-right L (raised = SHADOWPEN), inset by 1 pixel */
    xy4[0] = width - 2;       xy4[1] = 1;
    xy4[2] = width - 2;       xy4[3] = height - 2;
    xy4[4] = 1;               xy4[5] = height - 2;

    b4->FrontPen   = 1;  /* SHADOWPEN */
    b4->DrawMode   = JAM1;
    b4->Count      = 3;
    b4->XY         = xy4;
    b4->NextBorder = NULL;

    return b1;
}

/* Helper: free bevel box Border chain.
 * count=2 for single bevel (gt_create_bevel), count=4 for ridge bevel. */
static void gt_free_bevel_n(struct Border *b, ULONG count)
{
    if (b)
    {
        ULONG border_plus_xy = sizeof(struct Border) + 6 * sizeof(WORD);
        FreeMem(b, count * border_plus_xy);
    }
}

/* Helper: free single bevel box Border allocated by gt_create_bevel */
static void gt_free_bevel(struct Border *b)
{
    gt_free_bevel_n(b, 2);
}

/* Helper: free ridge bevel box Border allocated by gt_create_ridge_bevel */
static void gt_free_ridge_bevel(struct Border *b)
{
    gt_free_bevel_n(b, 4);
}

/* Helper: free an IntuiText label allocated by gt_create_label.
 * This frees the allocated IText copy, any NextText underline IntuiText,
 * and the IntuiText struct itself. */
static void gt_free_label(struct IntuiText *it)
{
    while (it)
    {
        struct IntuiText *next = it->NextText;

        if (it->IText)
            FreeMem(it->IText, gt_strlen(it->IText) + 1);

        FreeMem(it, sizeof(struct IntuiText));
        it = next;
    }
}

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
    struct Gadget *newgad;
    struct GTGadgetData *data;
    UBYTE us;  /* GT_Underscore character, or 0 if not set */

    DPRINTF (LOG_DEBUG, "_gadtools: CreateGadgetA() kind=%ld, prevgad=0x%08lx, ng=0x%08lx\n",
             kind, (ULONG)gad, (ULONG)ng);

    if (!ng)
        return NULL;

    /* Parse GT_Underscore tag — this is common to all gadget kinds */
    us = (UBYTE)GetTagData(GT_Underscore, 0, taglist);

    /* Allocate gadget structure */
    newgad = AllocMem(sizeof(struct Gadget), MEMF_CLEAR | MEMF_PUBLIC);
    if (!newgad)
        return NULL;

    data = gt_alloc_data(GT_KIND_UNKNOWN);
    if (!data)
    {
        FreeMem(newgad, sizeof(struct Gadget));
        return NULL;
    }
    newgad->SelectRender = (APTR)data;

    /* Fill in basic gadget fields from NewGadget */
    newgad->LeftEdge = ng->ng_LeftEdge;
    newgad->TopEdge = ng->ng_TopEdge;
    newgad->Width = ng->ng_Width;
    newgad->Height = ng->ng_Height;
    newgad->GadgetID = ng->ng_GadgetID;
    newgad->UserData = ng->ng_UserData;

    /* Set activation based on gadget kind */
    switch (kind) {
        case BUTTON_KIND:
        {
            data->kind = GT_KIND_BUTTON;
            newgad->GadgetType = GTYP_BOOLGADGET;
            newgad->Activation = GACT_RELVERIFY;
            newgad->Flags = GFLG_GADGHCOMP;

            /* Create raised bevel box border for the button */
            newgad->GadgetRender = (APTR)gt_create_bevel(ng->ng_Width, ng->ng_Height, FALSE);

            /* Create text label — default placement is PLACETEXT_IN (centered inside button) */
            newgad->GadgetText = gt_create_label(ng->ng_GadgetText, ng->ng_Flags,
                                                  PLACETEXT_IN,
                                                  ng->ng_Width, ng->ng_Height, us);

            DPRINTF (LOG_DEBUG, "_gadtools: CreateGadgetA() BUTTON: bevel=0x%08lx text=0x%08lx\n",
                     (ULONG)newgad->GadgetRender, (ULONG)newgad->GadgetText);
            break;
        }
        case STRING_KIND:
        case INTEGER_KIND:
        {
            struct StringInfo *si;
            STRPTR buf;
            ULONG maxchars;
            STRPTR initstr;
            WORD len;

            data->kind = (kind == INTEGER_KIND) ? GT_KIND_INTEGER : GT_KIND_STRING;
            newgad->GadgetType = GTYP_STRGADGET;
            newgad->Activation = GACT_RELVERIFY;
            newgad->Flags = GFLG_GADGHCOMP;

            /* Per v40 source: shrink the gadget hitbox inward to leave room for
             * the bevel border. The bevel is drawn outside the text area using
             * a Border with negative LeftEdge/TopEdge offsets. */
            newgad->LeftEdge  = ng->ng_LeftEdge + GT_BEVEL_LEFT;
            newgad->TopEdge   = ng->ng_TopEdge + GT_BEVEL_TOP;
            newgad->Width     = ng->ng_Width - 2 * GT_BEVEL_LEFT;
            newgad->Height    = ng->ng_Height - 2 * GT_BEVEL_TOP;

            /* Create recessed ridge (groove) bevel box border around the string gadget.
             * On a real Amiga, STRING_KIND uses a double-bevel (FRAME_RIDGE style):
             *   Outer bevel: recessed (shadow top-left, shine bottom-right)
             *   Inner bevel: raised (shine top-left, shadow bottom-right)
             * The border is positioned at negative offsets so it draws around
             * the (smaller) text editing area. */
            {
                struct Border *bevel = gt_create_ridge_bevel(ng->ng_Width, ng->ng_Height);
                if (bevel)
                {
                    /* Set negative offsets on all 4 Border structs */
                    struct Border *b = bevel;
                    while (b)
                    {
                        b->LeftEdge = -GT_BEVEL_LEFT;
                        b->TopEdge  = -GT_BEVEL_TOP;
                        b = b->NextBorder;
                    }
                }
                newgad->GadgetRender = (APTR)bevel;
            }

            /* Create text label — default placement is PLACETEXT_LEFT */
            newgad->GadgetText = gt_create_label(ng->ng_GadgetText, ng->ng_Flags,
                                                  PLACETEXT_LEFT,
                                                  ng->ng_Width, ng->ng_Height, us);

            /* Determine max chars from tags (default 128 per GadTools convention) */
            if (kind == INTEGER_KIND)
                maxchars = GetTagData(GTIN_MaxChars, 10, taglist);
            else
                maxchars = GetTagData(GTST_MaxChars, 128, taglist);

            /* Allocate StringInfo structure */
            si = (struct StringInfo *)AllocMem(sizeof(struct StringInfo), MEMF_CLEAR | MEMF_PUBLIC);
            if (!si)
            {
                FreeMem(data, sizeof(struct GTGadgetData));
                gt_free_label(newgad->GadgetText);
                gt_free_ridge_bevel((struct Border *)newgad->GadgetRender);
                FreeMem(newgad, sizeof(struct Gadget));
                return NULL;
            }

            /* Allocate buffer (+1 for NUL already included in MaxChars convention) */
            buf = (STRPTR)AllocMem(maxchars, MEMF_CLEAR | MEMF_PUBLIC);
            if (!buf)
            {
                FreeMem(si, sizeof(struct StringInfo));
                FreeMem(data, sizeof(struct GTGadgetData));
                gt_free_label(newgad->GadgetText);
                gt_free_ridge_bevel((struct Border *)newgad->GadgetRender);
                FreeMem(newgad, sizeof(struct Gadget));
                return NULL;
            }

            si->Buffer = buf;
            si->MaxChars = (WORD)maxchars;

            /* Copy initial string into buffer */
            if (kind == INTEGER_KIND)
            {
                LONG num = (LONG)GetTagData(GTIN_Number, 0, taglist);
                data->value = num;
                si->LongInt = num;
                si->NumChars = gt_format_long(buf, maxchars, num);
            }
            else
            {
                initstr = (STRPTR)GetTagData(GTST_String, 0, taglist);
                if (initstr)
                {
                    len = 0;
                    while (initstr[len] != '\0' && len < maxchars - 1)
                    {
                        buf[len] = initstr[len];
                        len++;
                    }
                    buf[len] = '\0';
                    si->NumChars = len;
                }
                else
                {
                    buf[0] = '\0';
                    si->NumChars = 0;
                }
            }

            si->BufferPos = si->NumChars;
            newgad->SpecialInfo = (APTR)si;

            DPRINTF (LOG_DEBUG, "_gadtools: CreateGadgetA() STRING/INTEGER: si=0x%08lx, buf='%s', maxchars=%ld, bevel=0x%08lx text=0x%08lx\n",
                     (ULONG)si, STRORNULL(buf), maxchars, (ULONG)newgad->GadgetRender, (ULONG)newgad->GadgetText);
            break;
        }
         case CHECKBOX_KIND:
        {
            BOOL cb_scaled = (BOOL)GetTagData(GTCB_Scaled, FALSE, taglist);
            WORD cb_w, cb_h;

            data->kind = GT_KIND_CHECKBOX;
            newgad->GadgetType = GTYP_BOOLGADGET;
            newgad->Activation = GACT_RELVERIFY | GACT_TOGGLESELECT;
            newgad->Flags = GFLG_GADGHCOMP;

            if (GetTagData(GTCB_Checked, FALSE, taglist))
                data->value = TRUE;

             if (cb_scaled)
            {
                /* Scaled mode: use caller-supplied dimensions as-is */
                cb_w = ng->ng_Width;
                cb_h = ng->ng_Height;
            }
            else
            {
                /* Unscaled mode: force standard 26×11 size */
                cb_w = CHECKBOX_WIDTH;
                cb_h = CHECKBOX_HEIGHT;
                newgad->Width  = cb_w;
                newgad->Height = cb_h;

                /* Vertically centre the checkbox box against left/right labels
                 * for fonts taller than 8 pixels (spec §8).
                 * Condition: PLACETEXT_LEFT, PLACETEXT_RIGHT, or no placement
                 * flag, AND a label text is present. */
                if (ng->ng_GadgetText)
                {
                    ULONG place_mask = ng->ng_Flags &
                        (PLACETEXT_LEFT | PLACETEXT_RIGHT | PLACETEXT_ABOVE |
                         PLACETEXT_BELOW | PLACETEXT_IN);
                    /* place_mask == 0 defaults to PLACETEXT_LEFT */
                    if (place_mask == 0 ||
                        place_mask == PLACETEXT_LEFT ||
                        place_mask == PLACETEXT_RIGHT)
                    {
                        WORD ta_ysize = (ng->ng_TextAttr != NULL)
                                            ? (WORD)ng->ng_TextAttr->ta_YSize
                                            : (WORD)GT_FONT_HEIGHT;
                        WORD adj = (ta_ysize - 7) / 2;
                        if (adj > 0)
                            newgad->TopEdge += adj;
                    }
                }
            }

            /* Checkbox artwork is drawn entirely by _render_gadget() in lxa_intuition.c
             * (VIB_THICK3D bevel + checkmark polygon, spec §11.4/§11.5).
             * No Border/Image needed here; GadgetRender stays NULL. */

            /* Create text label — default placement is PLACETEXT_LEFT */
            newgad->GadgetText = gt_create_label(ng->ng_GadgetText, ng->ng_Flags,
                                                  PLACETEXT_LEFT,
                                                  cb_w, cb_h, us);
            break;
        }
        case SLIDER_KIND:
        {
            struct PropInfo *pi;
            struct IntuiText *label_tail;
            struct IntuiText *level_text;
            LONG sl_min, sl_max, sl_level;
            LONG display_level;
            UWORD horizPot, horizBody;
            UWORD max_level_len;
            UWORD level_place;
            ULONG max_pixel_len;
            UBYTE justification;

            data->kind = GT_KIND_SLIDER;
            newgad->GadgetType = GTYP_PROPGADGET;
            newgad->Activation = GACT_RELVERIFY | GACT_IMMEDIATE;
            newgad->Flags = GFLG_GADGHCOMP;

            /* Read SLIDER tags */
            sl_min   = (LONG)GetTagData(GTSL_Min,   0,  taglist);
            sl_max   = (LONG)GetTagData(GTSL_Max,   15, taglist);
            sl_level = (LONG)GetTagData(GTSL_Level, 0,  taglist);
            data->min = sl_min;
            data->max = sl_max;

            /* Clamp level to range */
            if (sl_level < sl_min) sl_level = sl_min;
            if (sl_level > sl_max) sl_level = sl_max;
            data->value = sl_level;
            data->format = gt_strdup((STRPTR)GetTagData(GTSL_LevelFormat, (ULONG)"%ld", taglist));
            if (!data->format)
            {
                FreeMem(data, sizeof(struct GTGadgetData));
                FreeMem(newgad, sizeof(struct Gadget));
                return NULL;
            }

            max_level_len = (UWORD)GetTagData(GTSL_MaxLevelLen, 2, taglist);
            if (max_level_len == 0)
                max_level_len = 2;
            level_place = (UWORD)GetTagData(GTSL_LevelPlace, PLACETEXT_LEFT, taglist);
            justification = (UBYTE)GetTagData(GTSL_Justification, GTJ_LEFT, taglist);
            max_pixel_len = GetTagData(GTSL_MaxPixelLen, (ULONG)(max_level_len * 8), taglist);

            data->max_level_len = max_level_len;
            data->level_place = level_place;
            data->justification = justification;
            data->max_pixel_len = max_pixel_len;
            data->disp_func = (LONG (*)(struct Gadget *, LONG))GetTagData(GTSL_DispFunc, 0, taglist);
            data->level_buffer = (STRPTR)AllocMem(max_level_len + 1, MEMF_CLEAR | MEMF_PUBLIC);
            if (!data->level_buffer)
            {
                FreeMem(data->format, gt_strlen(data->format) + 1);
                FreeMem(data, sizeof(struct GTGadgetData));
                FreeMem(newgad, sizeof(struct Gadget));
                return NULL;
            }

            /* Compute HorizPot from level:
             * Pot = ((level - min) * MAXPOT) / (max - min) */
            if (sl_max > sl_min)
                horizPot = (UWORD)(((sl_level - sl_min) * (LONG)0xFFFF) / (sl_max - sl_min));
            else
                horizPot = 0;

            /* Compute HorizBody: one position out of the total range.
             * Body = MAXBODY / (max - min + 1), but at least 1 */
            if (sl_max > sl_min)
                horizBody = (UWORD)((LONG)0xFFFF / (sl_max - sl_min + 1));
            else
                horizBody = 0xFFFF;

            /* Allocate PropInfo */
            pi = (struct PropInfo *)AllocMem(sizeof(struct PropInfo), MEMF_CLEAR | MEMF_PUBLIC);
            if (!pi)
            {
                FreeMem(data->level_buffer, max_level_len + 1);
                FreeMem(data->format, gt_strlen(data->format) + 1);
                FreeMem(data, sizeof(struct GTGadgetData));
                FreeMem(newgad, sizeof(struct Gadget));
                return NULL;
            }

            pi->Flags     = AUTOKNOB | FREEHORIZ | PROPNEWLOOK;
            pi->HorizPot  = horizPot;
            pi->VertPot   = 0;
            pi->HorizBody = horizBody;
            pi->VertBody  = 0xFFFF;  /* No vertical movement */
            /* Store min/max in unused PropInfo fields for GT_SetGadgetAttrs */
            pi->CWidth    = (UWORD)sl_min;
            pi->CHeight   = (UWORD)sl_max;

            newgad->SpecialInfo = (APTR)pi;

            /* Create recessed bevel box border around the slider container
             * (real GadTools uses recessed for slider trough) */
            newgad->GadgetRender = (APTR)gt_create_bevel(ng->ng_Width, ng->ng_Height, TRUE);

            /* Create text label — default placement is PLACETEXT_LEFT */
            newgad->GadgetText = gt_create_label(ng->ng_GadgetText, ng->ng_Flags,
                                                  PLACETEXT_LEFT,
                                                  ng->ng_Width, ng->ng_Height, us);
            if (!newgad->GadgetText)
            {
                FreeMem(pi, sizeof(struct PropInfo));
                gt_free_bevel((struct Border *)newgad->GadgetRender);
                FreeMem(data->level_buffer, max_level_len + 1);
                FreeMem(data->format, gt_strlen(data->format) + 1);
                FreeMem(data, sizeof(struct GTGadgetData));
                FreeMem(newgad, sizeof(struct Gadget));
                return NULL;
            }

            display_level = data->disp_func ? data->disp_func(newgad, sl_level) : sl_level;
            gt_format_slider_level(data->level_buffer, max_level_len + 1,
                                   data->format, display_level);
            level_text = gt_create_label(data->level_buffer, 0, level_place,
                                         ng->ng_Width, ng->ng_Height, 0);
            if (!level_text)
            {
                gt_free_label(newgad->GadgetText);
                FreeMem(pi, sizeof(struct PropInfo));
                gt_free_bevel((struct Border *)newgad->GadgetRender);
                FreeMem(data->level_buffer, max_level_len + 1);
                FreeMem(data->format, gt_strlen(data->format) + 1);
                FreeMem(data, sizeof(struct GTGadgetData));
                FreeMem(newgad, sizeof(struct Gadget));
                return NULL;
            }

            if (level_text->IText)
                FreeMem(level_text->IText, gt_strlen(level_text->IText) + 1);
            level_text->IText = data->level_buffer;

            data->level_text = level_text;
            gt_position_slider_level_text(data, level_text, ng->ng_Width, ng->ng_Height);
            label_tail = gt_label_chain_tail(newgad->GadgetText);
            if (label_tail)
                label_tail->NextText = level_text;
            else
                newgad->GadgetText = level_text;

            DPRINTF (LOG_DEBUG, "_gadtools: CreateGadgetA() SLIDER: pi=0x%08lx min=%ld max=%ld level=%ld pot=%u body=%u bevel=0x%08lx text=0x%08lx\n",
                     (ULONG)pi, sl_min, sl_max, sl_level, horizPot, horizBody,
                     (ULONG)newgad->GadgetRender, (ULONG)newgad->GadgetText);
            break;
        }
        case CYCLE_KIND:
        {
            struct IntuiText *label_tail;
            struct IntuiText *cycle_text;
            UWORD max_label_len;

            data->kind = GT_KIND_CYCLE;
            data->aux = (APTR)GetTagData(GTCY_Labels, 0, taglist);
            data->value = (LONG)GetTagData(GTCY_Active, 0, taglist);
            /* Store gadget width in max_pixel_len so gt_update_cycle_label_display
             * can re-centre the label in the right sub-region [22, gadWidth-1]. */
            data->max_pixel_len = (ULONG)ng->ng_Width;
            newgad->GadgetType = GTYP_BOOLGADGET;
            newgad->Activation = GACT_RELVERIFY;
            newgad->Flags = GFLG_GADGHCOMP;

            max_label_len = gt_cycle_max_label_len(data);
            if (max_label_len == 0)
                max_label_len = 1;
            data->max_level_len = max_label_len;
            data->level_buffer = (STRPTR)AllocMem(max_label_len + 1, MEMF_CLEAR | MEMF_PUBLIC);
            if (!data->level_buffer)
            {
                FreeMem(data, sizeof(struct GTGadgetData));
                FreeMem(newgad, sizeof(struct Gadget));
                return NULL;
            }

            newgad->GadgetRender = (APTR)gt_create_bevel(ng->ng_Width, ng->ng_Height, TRUE);
            newgad->GadgetText = gt_create_label(ng->ng_GadgetText, ng->ng_Flags,
                                                  PLACETEXT_LEFT,
                                                  ng->ng_Width, ng->ng_Height, us);
            /* Create cycle value text; LeftEdge will be set by gt_update_cycle_label_display
             * to centre in the right sub-region [22, gadWidth-1] per spec §10. */
            cycle_text = gt_create_label(data->level_buffer, 0, PLACETEXT_IN,
                                         ng->ng_Width, ng->ng_Height, 0);
            if (!newgad->GadgetRender || !newgad->GadgetText || !cycle_text)
            {
                if (cycle_text)
                    gt_free_label(cycle_text);
                gt_free_label(newgad->GadgetText);
                if (newgad->GadgetRender)
                    gt_free_bevel((struct Border *)newgad->GadgetRender);
                FreeMem(data->level_buffer, max_label_len + 1);
                FreeMem(data, sizeof(struct GTGadgetData));
                FreeMem(newgad, sizeof(struct Gadget));
                return NULL;
            }

            if (cycle_text->IText)
                FreeMem(cycle_text->IText, gt_strlen(cycle_text->IText) + 1);
            cycle_text->IText = data->level_buffer;
            cycle_text->TopEdge = (ng->ng_Height - GT_FONT_HEIGHT) / 2;
            data->level_text = cycle_text;

            /* Set initial active state and compute initial LeftEdge */
            _gadtools_SetCycleState(newgad, (UWORD)data->value);

            label_tail = gt_label_chain_tail(newgad->GadgetText);
            if (label_tail)
                label_tail->NextText = cycle_text;
            else
                newgad->GadgetText = cycle_text;
            break;
        }
        case MX_KIND:
        case LISTVIEW_KIND:
        case PALETTE_KIND:
            data->kind = (kind == MX_KIND) ? GT_KIND_MX :
                         (kind == LISTVIEW_KIND) ? GT_KIND_LISTVIEW : GT_KIND_PALETTE;
            newgad->GadgetType = GTYP_PROPGADGET;
            newgad->Activation = GACT_RELVERIFY;
            newgad->Flags = GFLG_GADGHCOMP;

            /* Create text label — default placement is PLACETEXT_LEFT */
            newgad->GadgetText = gt_create_label(ng->ng_GadgetText, ng->ng_Flags,
                                                  PLACETEXT_LEFT,
                                                  ng->ng_Width, ng->ng_Height, us);
            break;
        case SCROLLER_KIND:
        {
            struct PropInfo *pi;
            LONG sc_top, sc_total, sc_visible;
            UWORD freedom;
            UWORD pot, body;

            data->kind = GT_KIND_SCROLLER;
            newgad->GadgetType = GTYP_PROPGADGET;
            newgad->Activation = GACT_RELVERIFY | GACT_IMMEDIATE;
            newgad->Flags = GFLG_GADGHCOMP;

            /* Read SCROLLER tags */
            sc_top     = (LONG)GetTagData(GTSC_Top,     0,  taglist);
            sc_total   = (LONG)GetTagData(GTSC_Total,   0,  taglist);
            sc_visible = (LONG)GetTagData(GTSC_Visible, 2,  taglist);
            freedom    = (UWORD)GetTagData(PGA_Freedom, LORIENT_HORIZ, taglist);

            /* Store scroller parameters:
             * value = Top, min = 0 (unused), max = Total */
            data->value = sc_top;
            data->min   = sc_visible;  /* store Visible in min for GT_SetGadgetAttrs */
            data->max   = sc_total;

            /* Clamp top to valid range */
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

            /* Compute Pot and Body from Top/Total/Visible:
             *   Body = (Visible * MAXBODY) / Total
             *   Pot  = (Top * MAXPOT) / (Total - Visible)
             */
            if (sc_total > 0 && sc_visible > 0)
                body = (UWORD)(((LONG)sc_visible * 0xFFFF) / sc_total);
            else
                body = 0xFFFF;

            if (sc_total > sc_visible && sc_total > 0)
                pot = (UWORD)(((LONG)sc_top * 0xFFFF) / (sc_total - sc_visible));
            else
                pot = 0;

            /* Allocate PropInfo */
            pi = (struct PropInfo *)AllocMem(sizeof(struct PropInfo), MEMF_CLEAR | MEMF_PUBLIC);
            if (!pi)
            {
                FreeMem(data, sizeof(struct GTGadgetData));
                FreeMem(newgad, sizeof(struct Gadget));
                return NULL;
            }

            if (freedom == LORIENT_VERT)
            {
                pi->Flags    = AUTOKNOB | FREEVERT | PROPNEWLOOK;
                pi->HorizPot = 0;
                pi->VertPot  = pot;
                pi->HorizBody = 0xFFFF;
                pi->VertBody  = body;
            }
            else
            {
                pi->Flags    = AUTOKNOB | FREEHORIZ | PROPNEWLOOK;
                pi->HorizPot = pot;
                pi->VertPot  = 0;
                pi->HorizBody = body;
                pi->VertBody  = 0xFFFF;
            }

            newgad->SpecialInfo = (APTR)pi;

            /* Create recessed bevel box border around the scroller container */
            newgad->GadgetRender = (APTR)gt_create_bevel(ng->ng_Width, ng->ng_Height, TRUE);

            /* Create text label — default placement is PLACETEXT_LEFT */
            newgad->GadgetText = gt_create_label(ng->ng_GadgetText, ng->ng_Flags,
                                                  PLACETEXT_LEFT,
                                                  ng->ng_Width, ng->ng_Height, us);

            DPRINTF(LOG_DEBUG, "_gadtools: CreateGadgetA() SCROLLER: pi=0x%08lx top=%ld total=%ld visible=%ld freedom=%d pot=%u body=%u\n",
                    (ULONG)pi, sc_top, sc_total, sc_visible, freedom, pot, body);
            break;
        }
        case TEXT_KIND:
        case NUMBER_KIND:
            data->kind = (kind == TEXT_KIND) ? GT_KIND_TEXT : GT_KIND_NUMBER;
            newgad->GadgetType = GTYP_BOOLGADGET;
            newgad->Flags = GFLG_GADGHNONE;

            /* Create text label — default placement is PLACETEXT_LEFT */
            newgad->GadgetText = gt_create_label(ng->ng_GadgetText, ng->ng_Flags,
                                                  PLACETEXT_LEFT,
                                                  ng->ng_Width, ng->ng_Height, us);
            break;
        default:
            newgad->GadgetType = GTYP_BOOLGADGET;
            newgad->Flags = GFLG_GADGHCOMP;
            break;
    }

    /* Link to previous gadget */
    if (gad) {
        if (gt_is_context_gadget(gad))
        {
            struct GadgetContext *context = gt_get_context(gad);
            struct Gadget *tail = context ? context->gc_Last : gad;

            tail->NextGadget = newgad;
            if (context)
                context->gc_Last = newgad;
        }
        else
        {
            gad->NextGadget = newgad;
        }
    }

    DPRINTF (LOG_DEBUG, "_gadtools: CreateGadgetA() -> 0x%08lx\n", (ULONG)newgad);
    return newgad;
}

/* FreeGadgets - Free a list of gadgets created by CreateGadgetA */
void _gadtools_FreeGadgets ( register struct GadToolsBase *GadToolsBase __asm("a6"),
                             register struct Gadget *gad __asm("a0") )
{
    struct Gadget *next;

    DPRINTF (LOG_DEBUG, "_gadtools: FreeGadgets() gad=0x%08lx\n", (ULONG)gad);

    if (gt_is_context_gadget(gad))
    {
        struct GadgetContext *context = gt_get_context(gad);
        struct Gadget *first = gad->NextGadget;

        if (context)
        {
            context->magic = 0;
            FreeMem(context, sizeof(*context));
        }

        FreeMem(gad, sizeof(struct Gadget));
        gad = first;
    }

    while (gad) {
        next = gad->NextGadget;

        /* Free StringInfo and buffer for string gadgets */
        if ((gad->GadgetType & GTYP_GTYPEMASK) == GTYP_STRGADGET && gad->SpecialInfo)
        {
            struct StringInfo *si = (struct StringInfo *)gad->SpecialInfo;
            if (si->Buffer)
                FreeMem(si->Buffer, si->MaxChars);
            FreeMem(si, sizeof(struct StringInfo));
        }

        /* Free PropInfo for proportional gadgets (SLIDER_KIND) */
        if ((gad->GadgetType & GTYP_GTYPEMASK) == GTYP_PROPGADGET && gad->SpecialInfo)
        {
            FreeMem(gad->SpecialInfo, sizeof(struct PropInfo));
        }

        if (gad->SelectRender)
        {
            struct GTGadgetData *data = (struct GTGadgetData *)gad->SelectRender;

            if (data->format)
                FreeMem(data->format, gt_strlen(data->format) + 1);
            if (data->level_buffer)
                FreeMem(data->level_buffer, data->max_level_len + 1);
        }

        if (gad->SelectRender)
            FreeMem(gad->SelectRender, sizeof(struct GTGadgetData));

        /* Free GadgetText (IntuiText) if we allocated one.
         * gt_create_label always allocates a copy of IText, and may
         * also create a NextText IntuiText for underscore underlines. */
        gt_free_label(gad->GadgetText);

        /* Free GadgetRender (bevel Border) if we allocated one.
         * We only free if it's NOT GFLG_GADGIMAGE (our borders are Border, not Image).
         * STRING_KIND uses a ridge bevel (4 borders), others use single bevel (2 borders). */
        if (gad->GadgetRender && !(gad->Flags & GFLG_GADGIMAGE))
        {
            if ((gad->GadgetType & GTYP_GTYPEMASK) == GTYP_STRGADGET)
                gt_free_ridge_bevel((struct Border *)gad->GadgetRender);
            else
                gt_free_bevel((struct Border *)gad->GadgetRender);
        }

        FreeMem(gad, sizeof(struct Gadget));
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

                /* Recompute HorizPot from new level */
                if (sl_max > sl_min)
                    pi->HorizPot = (UWORD)(((level - sl_min) * (LONG)0xFFFF) / (sl_max - sl_min));
                else
                    pi->HorizPot = 0;

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

                /* Recompute Pot and Body */
                if (sc_total > 0 && sc_visible > 0)
                    body = (UWORD)(((LONG)sc_visible * 0xFFFF) / sc_total);
                else
                    body = 0xFFFF;

                if (sc_total > sc_visible && sc_total > 0)
                    pot = (UWORD)(((LONG)sc_top * 0xFFFF) / (sc_total - sc_visible));
                else
                    pot = 0;

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

    if (needs_refresh && win)
        RefreshGList(gad, win, req, 1);
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

static WORD gt_menu_label_width(CONST_STRPTR label)
{
    if (!label || label == NM_BARLABEL)
        return 0;

    return gt_strlen(label) * 8;
}

static BOOL gt_is_separator_item(const struct MenuItem *item)
{
    struct IntuiText *it;

    if (!item || !(item->Flags & ITEMTEXT) || !item->ItemFill)
        return FALSE;

    it = (struct IntuiText *)item->ItemFill;
    if (!it->IText)
        return FALSE;

    return it->IText[0] == '-';
}

static WORD gt_measure_menu_item_width(struct MenuItem *item)
{
    struct IntuiText *it;
    WORD width = 16;

    if (!item)
        return 0;

    if (gt_is_separator_item(item))
        return 24;

    if ((item->Flags & ITEMTEXT) && item->ItemFill)
    {
        it = (struct IntuiText *)item->ItemFill;
        width += gt_menu_label_width(it->IText);
    }
    else if (item->ItemFill)
    {
        width += item->Width;
    }

    if (item->Flags & CHECKIT)
        width += 12;

    if (item->Flags & COMMSEQ)
        width += 28;

    if (item->SubItem)
        width += 12;

    if (width < 40)
        width = 40;

    return width;
}

static BOOL gt_layout_menu_item_chain(struct MenuItem *firstitem, WORD left_edge,
                                      struct TextAttr *textattr, UBYTE front_pen,
                                      WORD *out_width, WORD *out_height)
{
    struct MenuItem *item;
    struct IntuiText *it;
    WORD max_width = 0;
    WORD y = 0;

    if (!firstitem)
    {
        if (out_width)
            *out_width = 0;
        if (out_height)
            *out_height = 0;
        return TRUE;
    }

    for (item = firstitem; item; item = item->NextItem)
    {
        WORD needed = gt_measure_menu_item_width(item);
        if (needed > max_width)
            max_width = needed;
    }

    if (max_width == 0)
        max_width = 40;

    for (item = firstitem; item; item = item->NextItem)
    {
        item->LeftEdge = left_edge;
        item->TopEdge = y;
        item->Width = max_width;

        if (item->Height <= 0)
            item->Height = gt_is_separator_item(item) ? 6 : 10;

        if ((item->Flags & ITEMTEXT) && item->ItemFill)
        {
            it = (struct IntuiText *)item->ItemFill;
            it->FrontPen = front_pen;
            it->ITextFont = textattr;
            if (gt_is_separator_item(item))
            {
                it->LeftEdge = 2;
                it->TopEdge = 0;
            }
            else
            {
                it->LeftEdge = (item->Flags & CHECKIT) ? 14 : 2;
                it->TopEdge = 1;
            }
        }

        if (item->SubItem)
        {
            WORD dummy_width;
            WORD dummy_height;

            if (!gt_layout_menu_item_chain(item->SubItem, max_width, textattr,
                                           front_pen, &dummy_width, &dummy_height))
                return FALSE;
        }

        y += item->Height;
    }

    if (out_width)
        *out_width = max_width;
    if (out_height)
        *out_height = y;

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
    WORD menuLeft = 0;
    BOOL menu_image;

    DPRINTF (LOG_DEBUG, "_gadtools: CreateMenusA() newmenu=0x%08lx\n", (ULONG)newmenu);

    if (!newmenu)
        return NULL;

    gt_set_menu_error(taglist, 0);

    if (!gt_validate_newmenu_array(newmenu))
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
                struct Menu *menu = AllocMem(sizeof(struct Menu) + sizeof(APTR), MEMF_CLEAR | MEMF_PUBLIC);
                if (!menu) {
                    /* Out of memory - free what we have and return NULL */
                    gt_set_menu_error(taglist, GTMENU_NOMEM);
                    if (firstMenu)
                        _gadtools_FreeMenus(GadToolsBase, firstMenu);
                    return NULL;
                }

                menu->LeftEdge = menuLeft;
                menu->TopEdge = 0;
                menu->Width = 80;  /* Will be adjusted by LayoutMenusA */
                menu->Height = 10;
                menu->Flags = MENUENABLED;
                menu->MenuName = nm->nm_Label;
                menu->FirstItem = NULL;
                menu->NextMenu = NULL;

                /* Apply flags from NewMenu */
                if (nm->nm_Flags & NM_MENUDISABLED)
                    menu->Flags &= ~MENUENABLED;

                /* Estimate width for next menu position */
                if (nm->nm_Label) {
                    int len = 0;
                    CONST_STRPTR s = nm->nm_Label;
                    while (*s++) len++;
                    menu->Width = (len + 2) * 8;  /* Rough estimate */
                }
                menuLeft += menu->Width;

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
                    if (firstMenu)
                        _gadtools_FreeMenus(GadToolsBase, firstMenu);
                    return NULL;
                }

                item = AllocMem(sizeof(struct MenuItem) + sizeof(APTR), MEMF_CLEAR | MEMF_PUBLIC);
                if (!item) {
                    gt_set_menu_error(taglist, GTMENU_NOMEM);
                    if (firstMenu)
                        _gadtools_FreeMenus(GadToolsBase, firstMenu);
                    return NULL;
                }

                item->LeftEdge = 0;
                item->TopEdge = lastItem ? (lastItem->TopEdge + lastItem->Height) : 0;
                item->Width = 150;  /* Will be adjusted by LayoutMenuItemsA */
                item->Height = 10;
                item->Flags = ITEMTEXT | ITEMENABLED | HIGHCOMP;
                item->MutualExclude = nm->nm_MutualExclude;
                item->NextItem = NULL;
                item->SubItem = NULL;
                item->Command = 0;

                /* Handle bar label (separator) */
                if (nm->nm_Label == NM_BARLABEL) {
                    /* Create a simple separator - we'll use a minimal IntuiText */
                    itext = AllocMem(sizeof(struct IntuiText), MEMF_CLEAR | MEMF_PUBLIC);
                    if (itext) {
                        itext->FrontPen = 1;
                        itext->BackPen = 0;
                        itext->DrawMode = JAM1;
                        itext->LeftEdge = 0;
                        itext->TopEdge = 0;
                        itext->ITextFont = NULL;
                        itext->IText = (STRPTR)"----------------";
                        itext->NextText = NULL;
                    }
                    item->ItemFill = itext;
                    item->Flags &= ~ITEMENABLED;  /* Separators are not selectable */
                    item->Height = 6;  /* Shorter height for separators */
                } else if (menu_image) {
                    item->ItemFill = (APTR)nm->nm_Label;
                    item->Flags &= ~ITEMTEXT;
                } else {
                    /* Create IntuiText for the label */
                    itext = AllocMem(sizeof(struct IntuiText), MEMF_CLEAR | MEMF_PUBLIC);
                    if (itext) {
                        itext->FrontPen = 0;  /* Will use screen colors */
                        itext->BackPen = 1;
                        itext->DrawMode = JAM1;
                        itext->LeftEdge = 2;
                        itext->TopEdge = 1;
                        itext->ITextFont = NULL;
                        itext->IText = (STRPTR)nm->nm_Label;
                        itext->NextText = NULL;
                    }
                    item->ItemFill = itext;
                }

                /* Handle command key */
                if (nm->nm_CommKey && nm->nm_CommKey[0]) {
                    item->Flags |= COMMSEQ;
                    item->Command = nm->nm_CommKey[0];
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
                    if (firstMenu)
                        _gadtools_FreeMenus(GadToolsBase, firstMenu);
                    return NULL;
                }

                subitem = AllocMem(sizeof(struct MenuItem) + sizeof(APTR), MEMF_CLEAR | MEMF_PUBLIC);
                if (!subitem) {
                    gt_set_menu_error(taglist, GTMENU_NOMEM);
                    if (firstMenu)
                        _gadtools_FreeMenus(GadToolsBase, firstMenu);
                    return NULL;
                }

                subitem->LeftEdge = currentItem->Width;
                subitem->TopEdge = lastSubItem ? (lastSubItem->TopEdge + lastSubItem->Height) : 0;
                subitem->Width = 120;
                subitem->Height = 10;
                subitem->Flags = ITEMTEXT | ITEMENABLED | HIGHCOMP;
                subitem->MutualExclude = nm->nm_MutualExclude;
                subitem->NextItem = NULL;
                subitem->SubItem = NULL;
                subitem->Command = 0;

                /* Handle bar label (separator) */
                if (nm->nm_Label == NM_BARLABEL) {
                    itext = AllocMem(sizeof(struct IntuiText), MEMF_CLEAR | MEMF_PUBLIC);
                    if (itext) {
                        itext->FrontPen = 1;
                        itext->BackPen = 0;
                        itext->DrawMode = JAM1;
                        itext->IText = (STRPTR)"------------";
                    }
                    subitem->ItemFill = itext;
                    subitem->Flags &= ~ITEMENABLED;
                    subitem->Height = 6;
                } else if (menu_image) {
                    subitem->ItemFill = (APTR)nm->nm_Label;
                    subitem->Flags &= ~ITEMTEXT;
                } else {
                    itext = AllocMem(sizeof(struct IntuiText), MEMF_CLEAR | MEMF_PUBLIC);
                    if (itext) {
                        itext->FrontPen = 0;
                        itext->BackPen = 1;
                        itext->DrawMode = JAM1;
                        itext->LeftEdge = 2;
                        itext->TopEdge = 1;
                        itext->IText = (STRPTR)nm->nm_Label;
                    }
                    subitem->ItemFill = itext;
                }

                /* Handle command key */
                if (nm->nm_CommKey && nm->nm_CommKey[0]) {
                    subitem->Flags |= COMMSEQ;
                    subitem->Command = nm->nm_CommKey[0];
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

        /* Free the IntuiText if we created one */
        if ((item->Flags & ITEMTEXT) && item->ItemFill) {
            FreeMem(item->ItemFill, sizeof(struct IntuiText));
        }

        FreeMem(item, sizeof(struct MenuItem) + sizeof(APTR));
        item = next;
    }
}

/* FreeMenus - Free menus created by CreateMenusA */
void _gadtools_FreeMenus ( register struct GadToolsBase *GadToolsBase __asm("a6"),
                           register struct Menu *menu __asm("a0") )
{
    DPRINTF (LOG_DEBUG, "_gadtools: FreeMenus() menu=0x%08lx\n", (ULONG)menu);

    while (menu) {
        struct Menu *nextMenu = menu->NextMenu;

        /* Free all items in this menu */
        if (menu->FirstItem) {
            FreeMenuItems(menu->FirstItem);
        }

        /* Free the menu itself */
        FreeMem(menu, sizeof(struct Menu) + sizeof(APTR));
        menu = nextMenu;
    }
}

/* LayoutMenuItemsA - Layout menu items */
BOOL _gadtools_LayoutMenuItemsA ( register struct GadToolsBase *GadToolsBase __asm("a6"),
                                  register struct MenuItem *firstitem __asm("a0"),
                                  register APTR vi __asm("a1"),
                                  register struct TagItem *taglist __asm("a2") )
{
    struct TextAttr *textattr;
    struct VisualInfo *visual_info;
    UBYTE front_pen;
    WORD width;
    WORD height;

    DPRINTF (LOG_DEBUG, "_gadtools: LayoutMenuItemsA()\n");

    if (!firstitem)
        return FALSE;

    textattr = (struct TextAttr *)GetTagData(GTMN_TextAttr, 0, taglist);
    if (!textattr && vi)
    {
        visual_info = (struct VisualInfo *)vi;
        if (visual_info->vi_Screen)
            textattr = visual_info->vi_Screen->Font;
    }

    front_pen = (UBYTE)GetTagData(GTMN_FrontPen, 0, taglist);

    return gt_layout_menu_item_chain(firstitem, 0, textattr, front_pen,
                                     &width, &height);
}

/* LayoutMenusA - Layout entire menu structure */
BOOL _gadtools_LayoutMenusA ( register struct GadToolsBase *GadToolsBase __asm("a6"),
                              register struct Menu *firstmenu __asm("a0"),
                              register APTR vi __asm("a1"),
                              register struct TagItem *taglist __asm("a2") )
{
    struct Menu *menu;
    struct VisualInfo *visual_info;
    struct Screen *screen = NULL;
    struct TextAttr *textattr;
    WORD left = 0;
    WORD width;
    WORD height;

    DPRINTF (LOG_DEBUG, "_gadtools: LayoutMenusA()\n");

    if (!firstmenu)
        return FALSE;

    visual_info = (struct VisualInfo *)vi;
    if (visual_info)
        screen = visual_info->vi_Screen;

    textattr = (struct TextAttr *)GetTagData(GTMN_TextAttr, 0, taglist);
    if (!textattr && screen)
        textattr = screen->Font;

    for (menu = firstmenu; menu; menu = menu->NextMenu)
    {
        menu->LeftEdge = left;
        menu->TopEdge = 0;
        menu->Height = screen ? (screen->BarHeight + 1) : 10;
        menu->Width = gt_menu_label_width(menu->MenuName) + 16;
        if (menu->Width < 24)
            menu->Width = 24;

        if (menu->FirstItem)
        {
            if (!gt_layout_menu_item_chain(menu->FirstItem, 0, textattr,
                                           (UBYTE)GetTagData(GTMN_FrontPen, 0, taglist),
                                           &width, &height))
                return FALSE;
        }

        left += menu->Width;
    }

    return TRUE;
}

/*
 * Event Handling Functions
 */

/* GT_GetIMsg - Get an IntuiMessage, filtering GadTools messages */
struct IntuiMessage * _gadtools_GT_GetIMsg ( register struct GadToolsBase *GadToolsBase __asm("a6"),
                                             register struct MsgPort *iport __asm("a0") )
{
    DPRINTF (LOG_DEBUG, "_gadtools: GT_GetIMsg() iport=0x%08lx\n", (ULONG)iport);
    /* Just call GetMsg - no filtering needed for stub */
    return (struct IntuiMessage *)GetMsg(iport);
}

/* GT_ReplyIMsg - Reply to an IntuiMessage from GT_GetIMsg */
void _gadtools_GT_ReplyIMsg ( register struct GadToolsBase *GadToolsBase __asm("a6"),
                              register struct IntuiMessage *imsg __asm("a1") )
{
    DPRINTF (LOG_DEBUG, "_gadtools: GT_ReplyIMsg() imsg=0x%08lx\n", (ULONG)imsg);
    if (imsg) {
        ReplyMsg((struct Message *)imsg);
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
    DPRINTF (LOG_DEBUG, "_gadtools: GT_FilterIMsg() imsg=0x%08lx\n", (ULONG)imsg);
    return imsg;  /* No filtering in stub */
}

/* GT_PostFilterIMsg - Post-filter an IntuiMessage */
struct IntuiMessage * _gadtools_GT_PostFilterIMsg ( register struct GadToolsBase *GadToolsBase __asm("a6"),
                                                    register struct IntuiMessage *imsg __asm("a1") )
{
    DPRINTF (LOG_DEBUG, "_gadtools: GT_PostFilterIMsg() imsg=0x%08lx\n", (ULONG)imsg);
    return imsg;  /* No filtering in stub */
}

/* CreateContext - Create a gadget list context */
struct Gadget * _gadtools_CreateContext ( register struct GadToolsBase *GadToolsBase __asm("a6"),
                                          register struct Gadget **glistptr __asm("a0") )
{
    struct Gadget *context;
    struct GadgetContext *context_data;

    DPRINTF (LOG_DEBUG, "_gadtools: CreateContext() glistptr=0x%08lx\n", (ULONG)glistptr);

    if (!glistptr)
        return NULL;

    /* Allocate a dummy context gadget */
    context = AllocMem(sizeof(struct Gadget), MEMF_CLEAR | MEMF_PUBLIC);
    if (!context)
        return NULL;

    context_data = AllocMem(sizeof(struct GadgetContext), MEMF_CLEAR | MEMF_PUBLIC);
    if (!context_data)
    {
        FreeMem(context, sizeof(struct Gadget));
        return NULL;
    }

    context_data->magic = GT_CONTEXT_MAGIC;
    context_data->gc_Last = context;

    /* The context gadget is a placeholder, not a real gadget */
    context->GadgetType = GTYP_CUSTOMGADGET;
    context->Flags = GFLG_GADGHNONE;
    context->SpecialInfo = context_data;

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
    UBYTE shiPen = 2;    /* Default shine pen */
    UBYTE shaPen = 1;    /* Default shadow pen */
    WORD x0, y0, x1, y1;
    UBYTE topPen, botPen;
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
                    recessed = (BOOL)tag->ti_Data;
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
    
    /* Get pen colors from VisualInfo if available */
    if (vi && vi->vi_DrawInfo && vi->vi_DrawInfo->dri_Pens) {
        shiPen = vi->vi_DrawInfo->dri_Pens[SHINEPEN];
        shaPen = vi->vi_DrawInfo->dri_Pens[SHADOWPEN];
    }
    
    /* Save current pen */
    savedAPen = rport->FgPen;
    
    /* Calculate corners */
    x0 = left;
    y0 = top;
    x1 = left + width - 1;
    y1 = top + height - 1;
    
    /* Determine which pen goes where based on recessed state */
    topPen = recessed ? shaPen : shiPen;
    botPen = recessed ? shiPen : shaPen;
    
    switch (frameType) {
        case BBFT_RIDGE:
            /* Double border: outer raised, inner recessed */
            /* Outer frame - raised */
            SetAPen(rport, shiPen);
            Move(rport, x0, y1);
            Draw(rport, x0, y0);
            Draw(rport, x1, y0);
            SetAPen(rport, shaPen);
            Draw(rport, x1, y1);
            Draw(rport, x0, y1);
            
            /* Inner frame - recessed (inset by 1 pixel) */
            if (width > 2 && height > 2) {
                SetAPen(rport, shaPen);
                Move(rport, x0 + 1, y1 - 1);
                Draw(rport, x0 + 1, y0 + 1);
                Draw(rport, x1 - 1, y0 + 1);
                SetAPen(rport, shiPen);
                Draw(rport, x1 - 1, y1 - 1);
                Draw(rport, x0 + 1, y1 - 1);
            }
            break;
            
        case BBFT_ICONDROPBOX:
            /* Similar to ridge but with different appearance */
            /* Fall through to default for now */
        case BBFT_DISPLAY:
            /* Display box - usually recessed */
            topPen = shaPen;
            botPen = shiPen;
            /* Fall through */
        case BBFT_BUTTON:
        default:
            /* Standard single bevel box */
            /* Top and left edges (light or dark based on recessed) */
            SetAPen(rport, topPen);
            Move(rport, x0, y1 - 1);  /* Start at bottom-left, one pixel up */
            Draw(rport, x0, y0);       /* Draw up to top-left */
            Draw(rport, x1 - 1, y0);   /* Draw across to top-right (minus corner) */
            
            /* Bottom and right edges */
            SetAPen(rport, botPen);
            Move(rport, x1, y0);       /* Start at top-right */
            Draw(rport, x1, y1);       /* Draw down to bottom-right */
            Draw(rport, x0, y1);       /* Draw across to bottom-left */
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
    vi->vi_DrawInfo = NULL;  /* Would get from GetScreenDrawInfo() */

    DPRINTF (LOG_DEBUG, "_gadtools: GetVisualInfoA() -> 0x%08lx\n", (ULONG)vi);
    return vi;
}

/* FreeVisualInfo - Free visual info */
void _gadtools_FreeVisualInfo ( register struct GadToolsBase *GadToolsBase __asm("a6"),
                                register APTR vi __asm("a0") )
{
    DPRINTF (LOG_DEBUG, "_gadtools: FreeVisualInfo() vi=0x%08lx\n", (ULONG)vi);
    if (vi) {
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
