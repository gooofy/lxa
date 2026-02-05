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

/*
 * Minimum usable screen/window height threshold.
 * When applications pass suspiciously small height values (< 50 pixels),
 * we expand them to the display mode's default height. This handles older
 * apps that relied on ViewModes-based height expansion (e.g., GFA Basic).
 */
#define MIN_USABLE_HEIGHT 50

/* LACE flag for interlaced display modes */
#define LACE_FLAG 0x0004

extern struct GfxBase *GfxBase;
extern struct Library *LayersBase;
extern struct UtilityBase *UtilityBase;

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

struct LXAIntuitionBase;

/* Forward declarations */
VOID _intuition_RefreshGList ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Gadget * gadgets __asm("a0"),
                                                        register struct Window * window __asm("a1"),
                                                        register struct Requester * requester __asm("a2"),
                                                        register WORD numGad __asm("d0"));

/* Forward declaration for internal string gadget key handling */
static BOOL _handle_string_gadget_key(struct Gadget *gad, struct Window *window, 
                                       UWORD rawkey, UWORD qualifier);

UWORD _intuition_RemoveGList ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Window * remPtr __asm("a0"),
                                                        register struct Gadget * gadget __asm("a1"),
                                                        register WORD numGad __asm("d0"));

struct LXAClassNode {
    struct Node node;
    struct IClass *class_ptr;
};

/* Extended IntuitionBase to hold private data */
struct LXAIntuitionBase {
    struct IntuitionBase ib;
    struct List ClassList;      /* List of public classes */
    struct IClass *RootClass;   /* Pointer to rootclass */
    struct IClass *GadgetClass; /* Pointer to gadgetclass */
    struct IClass *ButtonGClass;/* Pointer to buttongclass */
    struct IClass *PropGClass;  /* Pointer to propgclass */
    struct IClass *StrGClass;   /* Pointer to strgclass */
};

static struct IClass *_intuition_find_class(struct LXAIntuitionBase *base, CONST_STRPTR classID);
static ULONG _intuition_dispatch_method(struct IClass *cl, Object *obj, Msg msg);

/* Rootclass dispatcher */
static ULONG rootclass_dispatch(
    register struct IClass *cl __asm("a0"),
    register Object *obj __asm("a2"),
    register Msg msg __asm("a1"))
{
    switch (msg->MethodID) {
        case OM_NEW:
            return (ULONG)obj;
            
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

/* Forward declarations for gadget class dispatchers */
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
char __aligned _g_intuition_Copyright [] = "(C)opyright 2022 by G. Bartsch. Licensed under the MIT license.";

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
            if (super && super->cl_Dispatcher.h_Entry) {
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
            
            /* Process tags from opSet */
            struct opSet *ops = (struct opSet *)msg;
            struct TagItem *tags = ops->ops_AttrList;
            struct TagItem *tag;
            
            while ((tag = NextTagItem(&tags))) {
                switch (tag->ti_Tag) {
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
                }
            }
            
            return (ULONG)obj;
        }
            
        case OM_DISPOSE:
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
            
        case OM_SET:
        case OM_UPDATE: {
            struct opSet *ops = (struct opSet *)msg;
            struct TagItem *tags = ops->ops_AttrList;
            struct TagItem *tag;
            ULONG changed = 0;
            
            while ((tag = NextTagItem(&tags))) {
                switch (tag->ti_Tag) {
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
            
        case OM_GET: {
            struct opGet *opg = (struct opGet *)msg;
            switch (opg->opg_AttrID) {
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
                default:
                    return 0;
            }
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
            
            /* Set proportional gadget-specific defaults */
            gadget->GadgetType = GTYP_PROPGADGET;
            gadget->Flags = GFLG_GADGHCOMP;
            gadget->Activation = GACT_RELVERIFY | GACT_IMMEDIATE;
            
            /* TODO: Allocate and initialize PropInfo structure */
            /* For now, just set basics */
            
            return (ULONG)obj;
        }
        
        case GM_RENDER: {
            struct gpRender *gpr = (struct gpRender *)msg;
            struct RastPort *rp = gpr->gpr_RPort;
            struct DrawInfo *dri = gpr->gpr_GInfo ? gpr->gpr_GInfo->gi_DrInfo : NULL;
            struct PropInfo *pi = (struct PropInfo *)gadget->SpecialInfo;
            
            if (!rp) return 0;
            
            /* Draw container (Recessed) */
            _draw_bevel_box(rp, gadget->LeftEdge, gadget->TopEdge, gadget->Width, gadget->Height, IDS_SELECTED, dri);
            
            /* Draw Knob if we have PropInfo */
            if (pi) {
                WORD knobW, knobH, knobX, knobY;
                
                /* Calculate Knob Dimensions */
                /* Autoknob check */
                if (gadget->Flags & GFLG_GADGIMAGE) {
                    /* Image-based knob - not handled here for now */
                } else {
                    /* Autoknob */
                    WORD containerW = gadget->Width - 4; // Borders
                    WORD containerH = gadget->Height - 4;
                    
                    if (pi->Flags & AUTOKNOB) {
                        /* Calculate knob size based on Body */
                         /* Body: 0xFFFF = full size */
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
                         /* Pot: 0xFFFF = max position */
                         WORD maxMoveX = containerW - knobW;
                         WORD maxMoveY = containerH - knobH;
                         
                         knobX = gadget->LeftEdge + 2 + ((maxMoveX * (ULONG)pi->HorizPot) / 0xFFFF);
                         knobY = gadget->TopEdge + 2 + ((maxMoveY * (ULONG)pi->VertPot) / 0xFFFF);
                         
                         /* Draw Knob (Raised) */
                         _draw_bevel_box(rp, knobX, knobY, knobW, knobH, 0, dri);
                    }
                }
            }
            return 0;
        }
        
        case GM_HANDLEINPUT: {
            struct gpInput *gpi = (struct gpInput *)msg;
            struct InputEvent *ie = gpi->gpi_IEvent;
            struct PropInfo *pi = (struct PropInfo *)gadget->SpecialInfo;
            
            if (!ie || !pi)
                return GMR_MEACTIVE;
            
            DPRINTF(LOG_DEBUG, "_intuition: propgclass GM_HANDLEINPUT ie_Class=%d ie_Code=0x%x mouse=%d,%d\n",
                    ie->ie_Class, ie->ie_Code, gpi->gpi_Mouse.X, gpi->gpi_Mouse.Y);
            
            /* Handle mouse button release */
            if (ie->ie_Class == IECLASS_RAWMOUSE) {
                if (ie->ie_Code == (IECODE_LBUTTON | IECODE_UP_PREFIX)) {
                    /* Mouse released - end interaction */
                    if (gpi->gpi_Termination)
                        *gpi->gpi_Termination = gadget->GadgetID;
                    return GMR_NOREUSE | GMR_VERIFY;
                }
            }
            
            /* Handle mouse movement while dragging */
            if (ie->ie_Class == IECLASS_RAWMOUSE || ie->ie_Class == 0) {
                /* Calculate the container dimensions */
                WORD containerW = gadget->Width - 4;
                WORD containerH = gadget->Height - 4;
                WORD knobW, knobH;
                WORD maxMoveX, maxMoveY;
                WORD mouseX = gpi->gpi_Mouse.X - gadget->LeftEdge - 2;
                WORD mouseY = gpi->gpi_Mouse.Y - gadget->TopEdge - 2;
                
                /* Calculate knob size */
                if (pi->Flags & AUTOKNOB) {
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
                } else {
                    knobW = containerW;
                    knobH = containerH;
                }
                
                /* Calculate max movement range */
                maxMoveX = containerW - knobW;
                maxMoveY = containerH - knobH;
                
                /* Update pot values based on mouse position */
                if ((pi->Flags & FREEHORIZ) && maxMoveX > 0) {
                    /* Clamp mouse position */
                    if (mouseX < 0) mouseX = 0;
                    if (mouseX > maxMoveX) mouseX = maxMoveX;
                    
                    pi->HorizPot = (mouseX * 0xFFFF) / maxMoveX;
                }
                
                if ((pi->Flags & FREEVERT) && maxMoveY > 0) {
                    /* Clamp mouse position */
                    if (mouseY < 0) mouseY = 0;
                    if (mouseY > maxMoveY) mouseY = maxMoveY;
                    
                    pi->VertPot = (mouseY * 0xFFFF) / maxMoveY;
                }
                
                DPRINTF(LOG_DEBUG, "_intuition: propgclass updated HorizPot=%d VertPot=%d\n",
                        pi->HorizPot, pi->VertPot);
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

/* StrGClass dispatcher - string gadget class */
static ULONG strgclass_dispatch(
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
            
            /* Set string gadget-specific defaults */
            gadget->GadgetType = GTYP_STRGADGET;
            gadget->Flags = GFLG_GADGHCOMP;
            gadget->Activation = GACT_RELVERIFY;
            
            /* TODO: Allocate and initialize StringInfo structure */
            /* For now, just set basics */
            
            return (ULONG)obj;
        }
        
        case GM_RENDER: {
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
            if (si && si->Buffer) {
                LONG len = strlen((char *)si->Buffer);
                
                SetAPen(rp, txtColor);
                SetBPen(rp, bgColor);
                SetDrMd(rp, JAM2);
                
                Move(rp, gadget->LeftEdge + 4, textY);
                if (len > 0) {
                    Text(rp, si->Buffer, len);
                }
                
                /* Draw cursor if gadget is selected (active) */
                if (gadget->Flags & GFLG_SELECTED) {
                    WORD cursorX;
                    WORD cursorPos = si->BufferPos;
                    
                    /* Calculate cursor X position */
                    if (cursorPos > 0 && rp->Font) {
                        /* Position is after cursorPos characters */
                        cursorX = gadget->LeftEdge + 4 + TextLength(rp, si->Buffer, cursorPos);
                    } else {
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
        
        case GM_HANDLEINPUT: {
            struct gpInput *gpi = (struct gpInput *)msg;
            struct InputEvent *ie = gpi->gpi_IEvent;
            struct StringInfo *si = (struct StringInfo *)gadget->SpecialInfo;
            
            if (!ie || !si || !si->Buffer)
                return GMR_MEACTIVE;
            
            DPRINTF(LOG_DEBUG, "_intuition: strgclass GM_HANDLEINPUT ie_Class=%d ie_Code=0x%x\n",
                    ie->ie_Class, ie->ie_Code);
            
            /* Handle mouse button release */
            if (ie->ie_Class == IECLASS_RAWMOUSE) {
                if (ie->ie_Code == (IECODE_LBUTTON | IECODE_UP_PREFIX)) {
                    /* Click - don't deactivate, stay active for editing */
                    return GMR_MEACTIVE;
                }
            }
            
            /* Handle keyboard input */
            if (ie->ie_Class == IECLASS_RAWKEY) {
                UWORD code = ie->ie_Code;
                BOOL isUpKey = (code & IECODE_UP_PREFIX) != 0;
                
                if (isUpKey) {
                    /* Key release - ignore */
                    return GMR_MEACTIVE;
                }
                
                code &= ~IECODE_UP_PREFIX;
                
                /* Check for special keys */
                switch (code) {
                    case 0x44: /* Return key */
                        if (gpi->gpi_Termination)
                            *gpi->gpi_Termination = gadget->GadgetID;
                        gadget->Flags &= ~GFLG_SELECTED;
                        return GMR_NOREUSE | GMR_VERIFY;
                        
                    case 0x45: /* Escape key */
                        gadget->Flags &= ~GFLG_SELECTED;
                        return GMR_NOREUSE;
                        
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
                        }
                        return GMR_MEACTIVE;
                        
                    case 0x46: /* Delete */
                        if (si->BufferPos < si->NumChars) {
                            /* Delete character at cursor */
                            WORD i;
                            for (i = si->BufferPos; i < si->NumChars - 1; i++) {
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
                        /* Check for regular character input via qualifier */
                        if (ie->ie_Qualifier & (IEQUALIFIER_LSHIFT | IEQUALIFIER_RSHIFT |
                                                 IEQUALIFIER_CAPSLOCK | IEQUALIFIER_CONTROL)) {
                            /* Handled below with mapping */
                        }
                        break;
                }
                
                /* Try to convert the raw key to ASCII */
                /* For now, use simple mapping for common keys */
                {
                    UBYTE ch = 0;
                    
                    /* Simple ASCII mapping for unshifted keys */
                    if (code >= 0x00 && code <= 0x09) {
                        /* Numbers 1-0 on main keyboard */
                        static const UBYTE numRow[] = "1234567890";
                        static const UBYTE numRowShift[] = "!@#$%^&*()";
                        if (ie->ie_Qualifier & (IEQUALIFIER_LSHIFT | IEQUALIFIER_RSHIFT))
                            ch = numRowShift[code];
                        else
                            ch = numRow[code];
                    } else if (code >= 0x10 && code <= 0x19) {
                        /* QWERTYUIOP */
                        static const UBYTE qRow[] = "qwertyuiop";
                        ch = qRow[code - 0x10];
                        if (ie->ie_Qualifier & (IEQUALIFIER_LSHIFT | IEQUALIFIER_RSHIFT | IEQUALIFIER_CAPSLOCK))
                            ch = ch - 'a' + 'A';
                    } else if (code >= 0x20 && code <= 0x28) {
                        /* ASDFGHJKL */
                        static const UBYTE aRow[] = "asdfghjkl";
                        ch = aRow[code - 0x20];
                        if (ie->ie_Qualifier & (IEQUALIFIER_LSHIFT | IEQUALIFIER_RSHIFT | IEQUALIFIER_CAPSLOCK))
                            ch = ch - 'a' + 'A';
                    } else if (code >= 0x31 && code <= 0x39) {
                        /* ZXCVBNM,. */
                        static const UBYTE zRow[] = "zxcvbnm,.";
                        ch = zRow[code - 0x31];
                        if (ie->ie_Qualifier & (IEQUALIFIER_LSHIFT | IEQUALIFIER_RSHIFT | IEQUALIFIER_CAPSLOCK))
                            ch = ch - 'a' + 'A';
                    } else if (code == 0x40) {
                        /* Space */
                        ch = ' ';
                    } else if (code == 0x0A) {
                        /* Minus/Underscore */
                        ch = (ie->ie_Qualifier & (IEQUALIFIER_LSHIFT | IEQUALIFIER_RSHIFT)) ? '_' : '-';
                    } else if (code == 0x0B) {
                        /* Equals/Plus */
                        ch = (ie->ie_Qualifier & (IEQUALIFIER_LSHIFT | IEQUALIFIER_RSHIFT)) ? '+' : '=';
                    }
                    
                    /* Insert character if we got one */
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
                    }
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
        gadgetclass->cl_InstSize = sizeof(struct Gadget);
        
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
        propgclass->cl_InstSize = 0; /* TODO: Add PropInfo size */
        
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
        strgclass->cl_InstSize = 0; /* TODO: Add StringInfo size */
        
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
    // FIXME IntuitionBase->dl_lib.lib_OpenCnt++;
    // FIXME IntuitionBase->dl_lib.lib_Flags &= ~LIBF_DELEXP;
    return IntuitionBase;
}

BPTR __g_lxa_intuition_CloseLib ( register struct IntuitionBase  *intuitionb __asm("a6"))
{
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
    DPRINTF (LOG_ERROR, "_intuition: ClearDMRequest() unimplemented STUB called.\n");
    assert(FALSE);
    return FALSE;
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
    /* No-op - we use the system default pointer */
}

BOOL _intuition_CloseScreen ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Screen * screen __asm("a0"))
{
    ULONG display_handle;
    UBYTE i;

    DPRINTF (LOG_DEBUG, "_intuition: CloseScreen() screen=0x%08lx\n", (ULONG)screen);

    if (!screen)
    {
        LPRINTF (LOG_ERROR, "_intuition: CloseScreen() called with NULL screen\n");
        return FALSE;
    }

    /* TODO: Check if any windows are still open on this screen */

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

    /* Free the Screen structure */
    FreeMem(screen, sizeof(struct Screen));

    DPRINTF (LOG_DEBUG, "_intuition: CloseScreen() done\n");

    return TRUE;
}

VOID _intuition_CloseWindow ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Window * window __asm("a0"))
{
    DPRINTF (LOG_DEBUG, "_intuition: CloseWindow() window=0x%08lx\n", (ULONG)window);

    if (!window)
    {
        LPRINTF (LOG_ERROR, "_intuition: CloseWindow() called with NULL window\n");
        return;
    }

    /* Phase 15: Close the rootless host window if one exists */
    if (window->UserData)
    {
        DPRINTF (LOG_DEBUG, "_intuition: CloseWindow() closing rootless window_handle=0x%08lx\n",
                 (ULONG)window->UserData);
        emucall1(EMU_CALL_INT_CLOSE_WINDOW, (ULONG)window->UserData);
        window->UserData = NULL;
    }

    /* TODO: Reply to any pending IDCMP messages */

    /* Free the IDCMP message port if we created it */
    if (window->UserPort)
    {
        /* Remove and delete the port */
        /* Note: caller should have already drained the messages */
        DeleteMsgPort(window->UserPort);
        window->UserPort = NULL;
    }

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

    /* Free the Window structure */
    FreeMem(window, sizeof(struct Window));

    DPRINTF (LOG_DEBUG, "_intuition: CloseWindow() done\n");
}

LONG _intuition_CloseWorkBench ( register struct IntuitionBase * IntuitionBase __asm("a6"))
{
    DPRINTF (LOG_ERROR, "_intuition: CloseWorkBench() unimplemented STUB called.\n");
    assert(FALSE);
    return 0;
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
    
    /* Default double-click time is 0.5 seconds (500000 microseconds) */
    /* Use IntuitionBase preferences if available */
    doubleClickTime = 500000;  /* Default: 0.5 seconds */
    
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
    
    if (!rp || !image)
        return;
    
    /* Walk the Image chain and render each image */
    while (image)
    {
        WORD x = leftOffset + image->LeftEdge;
        WORD y = topOffset + image->TopEdge;
        
        DPRINTF (LOG_DEBUG, "_intuition: DrawImage() rendering image: w=%d h=%d depth=%d leftEdge=%d topEdge=%d -> x=%d y=%d\n",
                 (int)image->Width, (int)image->Height, (int)image->Depth,
                 (int)image->LeftEdge, (int)image->TopEdge, (int)x, (int)y);
        
        if (image->ImageData && image->Width > 0 && image->Height > 0)
        {
            /* Render image data using BltTemplate or pixel-by-pixel */
            /* For now, use a simple blit approach */
            
            UWORD wordsPerRow = (image->Width + 15) / 16;
            UBYTE planePick = image->PlanePick;
            UBYTE planeOnOff = image->PlaneOnOff;
            UWORD *data = image->ImageData;
            
            for (WORD py = 0; py < image->Height; py++)
            {
                for (WORD px = 0; px < image->Width; px++)
                {
                    UWORD wordIndex = px / 16;
                    UWORD bitIndex = 15 - (px % 16);
                    
                    /* Build the color from all planes */
                    UBYTE color = 0;
                    UWORD *planeData = data;
                    
                    for (WORD plane = 0; plane < image->Depth && plane < 8; plane++)
                    {
                        if (planePick & (1 << plane))
                        {
                            /* This plane has data */
                            UWORD word = planeData[py * wordsPerRow + wordIndex];
                            if (word & (1 << bitIndex))
                                color |= (1 << plane);
                            planeData += wordsPerRow * image->Height;
                        }
                        else
                        {
                            /* Use PlaneOnOff for this plane */
                            if (planeOnOff & (1 << plane))
                                color |= (1 << plane);
                        }
                    }
                    
                    /* Only draw non-zero pixels (or all if depth is 0) */
                    if (color != 0 || image->Depth == 0)
                    {
                        SetAPen(rp, color);
                        WritePixel(rp, x + px, y + py);
                    }
                }
            }
        }
        
        image = image->NextImage;
    }
}

VOID _intuition_EndRequest ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Requester * requester __asm("a0"),
                                                        register struct Window * window __asm("a1"))
{
    struct Requester *curr, *prev = NULL;

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
    
    /* Restore background */
    /* Typically this involves refreshing the window area covered by the requester.
     * Since we don't have BackFill layers logic fully hooked up to refresh logic yet,
     * we might just trigger a refresh event or clear the area?
     * "Proper" way: Restore bits if saved, or damage layer.
     */
     
    /* For now, just invalidate the area if we have layers */
    if (window->WLayer && LayersBase)
    {
        /* TODO: DamageLayerRegion or similar? */
        /* Simplest: Clear to window background and let refresh handle it */
        /* SetAPen(window->RPort, window->BackFill ? 0 : 0);
           RectFill(window->RPort, requester->LeftEdge, ...);
           RefeshWindowFrame...
        */
    }
}

struct Preferences * _intuition_GetDefPrefs ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Preferences * preferences __asm("a0"),
                                                        register WORD size __asm("d0"))
{
    DPRINTF (LOG_ERROR, "_intuition: GetDefPrefs() unimplemented STUB called.\n");
    assert(FALSE);
    return NULL;
}

struct Preferences * _intuition_GetPrefs ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Preferences * preferences __asm("a0"),
                                                        register WORD size __asm("d0"))
{
    /*
     * GetPrefs() copies the current Intuition preferences into the user's buffer.
     * We provide reasonable defaults for a standard PAL/NTSC Workbench setup.
     */
    DPRINTF (LOG_DEBUG, "_intuition: GetPrefs() preferences=0x%08lx size=%d\n", 
             (ULONG)preferences, (int)size);
    
    if (!preferences || size <= 0) {
        return NULL;
    }
    
    /* Clear the buffer first */
    char *p = (char *)preferences;
    for (int i = 0; i < size && i < (int)sizeof(struct Preferences); i++) {
        p[i] = 0;
    }
    
    /* Fill in sensible defaults - only fill what fits in the provided buffer */
    struct Preferences defaults;
    char *d = (char *)&defaults;
    for (int i = 0; i < (int)sizeof(defaults); i++) d[i] = 0;
    
    defaults.FontHeight = 8;          /* TOPAZ_EIGHTY - standard 80-col font */
    defaults.PrinterPort = 0;         /* PARALLEL_PRINTER */
    defaults.BaudRate = 5;            /* BAUD_9600 */
    
    /* Key repeat: ~500ms delay, ~100ms repeat */
    defaults.KeyRptDelay.tv_secs = 0;
    defaults.KeyRptDelay.tv_micro = 500000;
    defaults.KeyRptSpeed.tv_secs = 0;
    defaults.KeyRptSpeed.tv_micro = 100000;
    
    /* Double-click: ~500ms */
    defaults.DoubleClick.tv_secs = 0;
    defaults.DoubleClick.tv_micro = 500000;
    
    /* Pointer offsets */
    defaults.XOffset = -1;
    defaults.YOffset = -1;
    defaults.PointerTicks = 1;
    
    /* Workbench colors (standard 4-color palette) */
    defaults.color0 = 0x0AAA;         /* Grey background */
    defaults.color1 = 0x0000;         /* Black */
    defaults.color2 = 0x0FFF;         /* White */
    defaults.color3 = 0x068B;         /* Blue */
    
    /* Pointer colors */
    defaults.color17 = 0x0E44;        /* Orange-red */
    defaults.color18 = 0x0000;        /* Black */
    defaults.color19 = 0x0EEC;        /* Yellow */
    
    /* View offsets (PAL defaults) */
    defaults.ViewXOffset = 0;
    defaults.ViewYOffset = 0;
    defaults.ViewInitX = 0x0081;      /* Standard HIRES offset */
    defaults.ViewInitY = 0x002C;      /* Standard PAL offset */
    
    defaults.EnableCLI = TRUE | (1 << 14);  /* SCREEN_DRAG enabled */
    
    /* Printer defaults */
    defaults.PrinterType = 0x07;      /* EPSON */
    defaults.PrintPitch = 0;          /* PICA */
    defaults.PrintQuality = 0;        /* DRAFT */
    defaults.PrintSpacing = 0;        /* SIX_LPI */
    defaults.PrintLeftMargin = 5;
    defaults.PrintRightMargin = 75;
    defaults.PrintImage = 0;          /* IMAGE_POSITIVE */
    defaults.PrintAspect = 0;         /* ASPECT_HORIZ */
    defaults.PrintShade = 1;          /* SHADE_GREYSCALE */
    defaults.PrintThreshold = 7;
    
    /* Paper defaults */
    defaults.PaperSize = 0;           /* US_LETTER */
    defaults.PaperLength = 66;        /* 66 lines per page */
    defaults.PaperType = 0;           /* FANFOLD */
    
    /* Serial defaults: 8N1 */
    defaults.SerRWBits = 0;           /* 8 read, 8 write bits */
    defaults.SerStopBuf = 0x01;       /* 1 stop bit, 1024 buf */
    defaults.SerParShk = 0x02;        /* No parity, no handshake */
    
    defaults.LaceWB = 0;              /* No interlace */
    
    /* Copy to user buffer */
    int copy_size = size < (int)sizeof(struct Preferences) ? size : (int)sizeof(struct Preferences);
    for (int i = 0; i < copy_size; i++) {
        p[i] = d[i];
    }
    
    return preferences;
}

VOID _intuition_InitRequester ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Requester * requester __asm("a0"))
{
    DPRINTF (LOG_DEBUG, "_intuition: InitRequester() req=0x%08lx\n", (ULONG)requester);
    
    if (!requester) return;
    
    /* Clear relevant fields */
    /* Note: Application usually allocates this, we just init defaults if needed.
     * RKRM says: "Initializes a requester for use."
     * It typically clears flags and sets up pointers.
     */
    requester->OlderRequest = NULL;
    requester->Flags &= ~REQACTIVE;
    /* We don't clear other fields as they are set by the app */
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

    /* If flags are being cleared and we had IDCMP active, remove the port */
    if (flags == 0 && window->UserPort)
    {
        /* Drain any pending messages first */
        struct Message *msg;
        while ((msg = GetMsg(window->UserPort)) != NULL)
        {
            ReplyMsg(msg);
        }
        
        DeleteMsgPort(window->UserPort);
        window->UserPort = NULL;
    }
    /* If we're setting flags and didn't have a port, create one */
    else if (flags != 0 && !window->UserPort)
    {
        window->UserPort = CreateMsgPort();
        if (!window->UserPort)
        {
            LPRINTF (LOG_ERROR, "_intuition: ModifyIDCMP() failed to create port\n");
            return FALSE;
        }
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
static struct Gadget * _find_gadget_at_pos(struct Window *window, WORD relX, WORD relY)
{
    struct Gadget *gad;
    WORD gx0, gy0, gx1, gy1;
    
    if (!window)
        return NULL;
    
    /* Walk through gadget list */
    for (gad = window->FirstGadget; gad; gad = gad->NextGadget)
    {
        /* Skip disabled gadgets */
        if (gad->Flags & GFLG_DISABLED)
            continue;
        
        /* Calculate gadget bounds (handling relative positioning) */
        gx0 = gad->LeftEdge;
        gy0 = gad->TopEdge;
        gx1 = gx0 + gad->Width;
        gy1 = gy0 + gad->Height;
        
        /* Handle GFLG_REL* flags for relative positioning */
        if (gad->Flags & GFLG_RELRIGHT)
            gx0 += window->Width - 1;
        if (gad->Flags & GFLG_RELBOTTOM)
            gy0 += window->Height - 1;
        if (gad->Flags & GFLG_RELWIDTH)
            gx1 = gx0 + gad->Width + window->Width;
        if (gad->Flags & GFLG_RELHEIGHT)
            gy1 = gy0 + gad->Height + window->Height;
        
        /* Update end coordinates if relative flags changed start */
        if (gad->Flags & GFLG_RELRIGHT)
            gx1 = gx0 + gad->Width;
        if (gad->Flags & GFLG_RELBOTTOM)
            gy1 = gy0 + gad->Height;
        
        /* Check if point is inside gadget bounds */
        if (relX >= gx0 && relX < gx1 && relY >= gy0 && relY < gy1)
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
    WORD gx0, gy0, gx1, gy1;
    
    if (!window || !gad)
        return FALSE;
    
    /* Calculate gadget bounds */
    gx0 = gad->LeftEdge;
    gy0 = gad->TopEdge;
    gx1 = gx0 + gad->Width;
    gy1 = gy0 + gad->Height;
    
    /* Handle GFLG_REL* flags */
    if (gad->Flags & GFLG_RELRIGHT)
        gx0 += window->Width - 1;
    if (gad->Flags & GFLG_RELBOTTOM)
        gy0 += window->Height - 1;
    if (gad->Flags & GFLG_RELWIDTH)
        gx1 = gx0 + gad->Width + window->Width;
    if (gad->Flags & GFLG_RELHEIGHT)
        gy1 = gy0 + gad->Height + window->Height;
    
    if (gad->Flags & GFLG_RELRIGHT)
        gx1 = gx0 + gad->Width;
    if (gad->Flags & GFLG_RELBOTTOM)
        gy1 = gy0 + gad->Height;
    
    return (relX >= gx0 && relX < gx1 && relY >= gy0 && relY < gy1);
}

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
            /* TODO: Implement WindowToBack/WindowToFront toggle */
            DPRINTF(LOG_DEBUG, "_intuition: Depth gadget clicked - TODO: window depth change\n");
            /* For now, post IDCMP_CHANGEWINDOW as a notification */
            _post_idcmp_message(window, IDCMP_CHANGEWINDOW, 0, 0, window,
                               window->MouseX, window->MouseY);
            break;
        
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

/* State for tracking active gadget during mouse press
 * Note: These MUST NOT have initializers - uninitialized statics go into .bss (RAM),
 * while initialized statics go into .data which is in ROM (read-only). */
static struct Gadget *g_active_gadget;
static struct Window *g_active_window;

/* State for menu bar handling
 * Note: These MUST NOT have initializers because initialized static data goes
 * into .data section which is placed in ROM (read-only). Uninitialized statics
 * go into .bss which is in RAM. We initialize them in _enter_menu_mode(). */
static BOOL g_menu_mode;                    /* TRUE when right mouse button held */
static struct Window *g_menu_window;        /* Window whose menu is being shown */
static struct Menu *g_active_menu;          /* Currently highlighted menu */
static struct MenuItem *g_active_item;      /* Currently highlighted item */
static UWORD g_menu_selection;              /* Encoded menu selection */

/* Menu bar saved screen area (for restoration) - TODO: implement proper saving
static PLANEPTR g_menu_save_buffer = NULL;
static WORD g_menu_save_x = 0;
static WORD g_menu_save_y = 0;
static WORD g_menu_save_width = 0;
static WORD g_menu_save_height = 0;
*/

/*
 * Encode menu selection into FULLMENUNUM format
 * menuNum: 0-31, itemNum: 0-63, subNum: 0-31
 */
static UWORD _encode_menu_selection(WORD menuNum, WORD itemNum, WORD subNum)
{
    if (menuNum == NOMENU || itemNum == NOITEM)
        return MENUNULL;
    
    UWORD code = (menuNum & 0x1F);              /* Bits 0-4: menu number */
    code |= ((itemNum & 0x3F) << 5);            /* Bits 5-10: item number */
    if (subNum != NOSUB)
        code |= ((subNum & 0x1F) << 11);        /* Bits 11-15: sub-item number */
    
    return code;
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
    
    if (!menu || !menu->FirstItem)
        return NULL;
    
    for (item = menu->FirstItem; item; item = item->NextItem)
    {
        /* Skip disabled items */
        if (!(item->Flags & ITEMENABLED))
            continue;
        
        if (x >= item->LeftEdge && x < item->LeftEdge + item->Width &&
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
 * Render the drop-down menu for the active menu
 */
static void _render_menu_items(struct Window *window)
{
    struct Screen *screen;
    struct RastPort *rp;
    struct Menu *menu;
    struct MenuItem *item;
    struct IntuiText *it;
    WORD barHeight;
    WORD menuX, menuY, menuWidth, menuHeight;
    WORD itemY;
    
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
    
    /* Calculate menu width/height from items */
    menuWidth = 0;
    menuHeight = 0;
    for (item = menu->FirstItem; item; item = item->NextItem)
    {
        WORD w = item->LeftEdge + item->Width;
        WORD h = item->TopEdge + item->Height;
        if (w > menuWidth) menuWidth = w;
        if (h > menuHeight) menuHeight = h;
    }
    menuWidth += 8;  /* Add some padding */
    menuHeight += 4;
    
    /* Draw menu box background */
    SetAPen(rp, 1);  /* Background color */
    RectFill(rp, menuX, menuY, menuX + menuWidth - 1, menuY + menuHeight - 1);
    
    /* Draw menu box border (3D effect) */
    SetAPen(rp, 2);  /* White/shine */
    Move(rp, menuX, menuY + menuHeight - 2);
    Draw(rp, menuX, menuY);
    Draw(rp, menuX + menuWidth - 2, menuY);
    
    SetAPen(rp, 0);  /* Black/shadow */
    Move(rp, menuX + menuWidth - 1, menuY);
    Draw(rp, menuX + menuWidth - 1, menuY + menuHeight - 1);
    Draw(rp, menuX, menuY + menuHeight - 1);
    
    /* Render menu items */
    SetDrMd(rp, JAM2);
    
    for (item = menu->FirstItem; item; item = item->NextItem)
    {
        WORD itemX = menuX + item->LeftEdge + 4;
        itemY = menuY + item->TopEdge + 2;
        
        /* Highlight selected item */
        if (item == g_active_item && (item->Flags & ITEMENABLED))
        {
            SetAPen(rp, 0);  /* Highlighted background */
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
        
        /* Draw item text (if IntuiText) */
        it = (struct IntuiText *)item->ItemFill;
        if (it && it->IText)
        {
            /* Disabled items shown in gray (pen 1 on pen 1 = gray effect) */
            if (!(item->Flags & ITEMENABLED))
            {
                SetAPen(rp, 2);  /* Gray text for disabled */
                SetBPen(rp, 1);
            }
            
            Move(rp, itemX + it->LeftEdge, itemY + it->TopEdge + rp->TxBaseline);
            Text(rp, (STRPTR)it->IText, strlen((const char *)it->IText));
            
            /* Draw checkmark if checked */
            if (item->Flags & CHECKED)
            {
                Move(rp, menuX + 3, itemY + rp->TxBaseline);
                Text(rp, (STRPTR)"\x9E", 1);  /* Amiga checkmark character */
            }
            
            /* Draw command key if present */
            if (item->Flags & COMMSEQ)
            {
                char cmdStr[4];
                cmdStr[0] = 'A';  /* Amiga key symbol (simplified) */
                cmdStr[1] = '-';
                cmdStr[2] = item->Command;
                cmdStr[3] = '\0';
                Move(rp, menuX + menuWidth - 40, itemY + rp->TxBaseline);
                Text(rp, (STRPTR)cmdStr, 3);
            }
        }
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
    g_menu_selection = MENUNULL;
    
    DPRINTF(LOG_DEBUG, "_intuition: Entering menu mode for window 0x%08lx\n", (ULONG)window);
    
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
    
    if (!g_menu_mode)
        return;
    
    DPRINTF(LOG_DEBUG, "_intuition: Exiting menu mode, active_menu=0x%08lx active_item=0x%08lx\n",
            (ULONG)g_active_menu, (ULONG)g_active_item);
    
    /* Calculate menu selection code if we have a valid selection */
    if (g_active_menu && g_active_item && (g_active_item->Flags & ITEMENABLED))
    {
        WORD menuNum = _get_menu_index(g_menu_window, g_active_menu);
        WORD itemNum = _get_item_index(g_active_menu, g_active_item);
        menuCode = _encode_menu_selection(menuNum, itemNum, NOSUB);
        
        DPRINTF(LOG_DEBUG, "_intuition: Menu selection: menu=%d item=%d code=0x%04x\n",
                (int)menuNum, (int)itemNum, menuCode);
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
    g_menu_selection = MENUNULL;
    
    /* Redraw screen to clear menu bar */
    /* For now, we'll just let the next refresh handle it */
    /* A proper implementation would restore saved screen area */
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
    
    DPRINTF(LOG_DEBUG, "_intuition: _handle_string_gadget_key code=0x%02x qual=0x%04x\n",
            code, qualifier);
    
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
            
        default:
            /* Try to convert raw key to ASCII character */
            {
                UBYTE ch = 0;
                
                /* Simple ASCII mapping for common keys */
                if (code >= 0x00 && code <= 0x09) {
                    /* Numbers 1-0 on main keyboard */
                    static const UBYTE numRow[] = "1234567890";
                    static const UBYTE numRowShift[] = "!@#$%^&*()";
                    if (qualifier & (IEQUALIFIER_LSHIFT | IEQUALIFIER_RSHIFT))
                        ch = numRowShift[code];
                    else
                        ch = numRow[code];
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
                } else if (code >= 0x31 && code <= 0x39) {
                    /* ZXCVBNM,. */
                    static const UBYTE zRow[] = "zxcvbnm,.";
                    ch = zRow[code - 0x31];
                    if (qualifier & (IEQUALIFIER_LSHIFT | IEQUALIFIER_RSHIFT | IEQUALIFIER_CAPSLOCK))
                        ch = ch - 'a' + 'A';
                } else if (code == 0x40) {
                    /* Space */
                    ch = ' ';
                } else if (code == 0x0A) {
                    /* Minus/Underscore */
                    ch = (qualifier & (IEQUALIFIER_LSHIFT | IEQUALIFIER_RSHIFT)) ? '_' : '-';
                } else if (code == 0x0B) {
                    /* Equals/Plus */
                    ch = (qualifier & (IEQUALIFIER_LSHIFT | IEQUALIFIER_RSHIFT)) ? '+' : '=';
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
                } else if (code == 0x30) {
                    /* Grave/tilde */
                    ch = (qualifier & (IEQUALIFIER_LSHIFT | IEQUALIFIER_RSHIFT)) ? '~' : '`';
                } else if (code == 0x2B) {
                    /* Backslash/pipe */
                    ch = (qualifier & (IEQUALIFIER_LSHIFT | IEQUALIFIER_RSHIFT)) ? '|' : '\\';
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
        _intuition_RefreshGList(IntuitionBase, gad, window, NULL, 1);
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
    struct IntuiMessage *imsg;
    
    if (!window || !window->UserPort) {
        return FALSE;
    }
    
    /* Check if window is interested in this message class */
    if (!(window->IDCMPFlags & class)) {
        return FALSE;
    }
    
    /* Allocate IntuiMessage */
    imsg = (struct IntuiMessage *)AllocMem(sizeof(struct IntuiMessage), MEMF_PUBLIC | MEMF_CLEAR);
    if (!imsg)
    {
        LPRINTF(LOG_ERROR, "_intuition: _post_idcmp_message() out of memory\n");
        return FALSE;
    }
    
    /* Fill in the message */
    imsg->ExecMessage.mn_Node.ln_Type = NT_MESSAGE;
    imsg->ExecMessage.mn_Length = sizeof(struct IntuiMessage);
    imsg->ExecMessage.mn_ReplyPort = NULL;  /* No reply expected for IDCMP */
    
    imsg->Class = class;
    imsg->Code = code;
    imsg->Qualifier = qualifier;
    imsg->IAddress = iaddress;
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
    
    LPRINTF(LOG_INFO, "_intuition: Posted IDCMP 0x%08lx to window 0x%08lx\n",
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
    
    if (!screen)
        return;
    
    /* Prevent re-entry - if already processing events, skip */
    if (g_processing_events)
        return;
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
                
                if (window)
                {
                    /* Convert to window-relative coordinates */
                    WORD relX = mouseX - window->LeftEdge;
                    WORD relY = mouseY - window->TopEdge;
                    
                    /* Check for gadget hit on mouse down (SELECTDOWN) */
                    if (code == SELECTDOWN)
                    {
                        struct Gadget *gad = _find_gadget_at_pos(window, relX, relY);
                        if (gad)
                        {
                            DPRINTF(LOG_DEBUG, "_intuition: SELECTDOWN on gadget type=0x%04x\n",
                                    gad->GadgetType);
                            
                            /* Set as active gadget for tracking */
                            g_active_gadget = gad;
                            g_active_window = window;
                            
                            /* Set gadget as selected */
                            gad->Flags |= GFLG_SELECTED;
                            
                            /* Post IDCMP_GADGETDOWN if it's a GADGIMMEDIATE gadget */
                            if (gad->Activation & GACT_IMMEDIATE)
                            {
                                _post_idcmp_message(window, IDCMP_GADGETDOWN, 0,
                                                   qualifier, gad, relX, relY);
                            }
                            
                            /* TODO: Render selected state (GADGHCOMP/GADGHBOX) */
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
                                /* Don't clear g_active_gadget or g_active_window */
                            }
                            else
                            {
                                /* Non-string gadgets: clear selected state */
                                gad->Flags &= ~GFLG_SELECTED;
                                
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
                                
                                /* TODO: Render normal state */
                                
                                /* Clear active gadget */
                                g_active_gadget = NULL;
                                g_active_window = NULL;
                            }
                        }
                    }
                    /* Right mouse button press - enter menu mode */
                    else if (code == MENUDOWN)
                    {
                        /* Find window with menu at this position (check title bar) */
                        struct Window *menuWin = NULL;
                        
                        /* Check if we're in the screen's title bar area (where menus are displayed) */
                        if (mouseY < screen->BarHeight + 1)
                        {
                            /* Find the active window with a menu strip */
                            menuWin = screen->FirstWindow;
                            while (menuWin)
                            {
                                if (menuWin->MenuStrip && (menuWin->Flags & WFLG_MENUSTATE) == 0)
                                    break;
                                menuWin = menuWin->NextWindow;
                            }
                        }
                        
                        if (menuWin && menuWin->MenuStrip)
                        {
                            _enter_menu_mode(menuWin, screen, mouseX, mouseY);
                        }
                    }
                    /* Right mouse button release - exit menu mode and select item */
                    else if (code == MENUUP)
                    {
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
                
                /* Handle menu tracking when in menu mode */
                if (g_menu_mode && g_menu_window)
                {
                    BOOL needRedraw = FALSE;
                    struct Menu *oldMenu = g_active_menu;
                    
                    /* Check if mouse is in menu bar area */
                    if (mouseY < screen->BarHeight + 1)
                    {
                        struct Menu *newMenu = _find_menu_at_x(g_menu_window, mouseX);
                        if (newMenu != g_active_menu)
                        {
                            g_active_menu = newMenu;
                            g_active_item = NULL;  /* Clear item when switching menus */
                            needRedraw = TRUE;
                        }
                    }
                    /* Check if mouse is in drop-down menu area */
                    else if (g_active_menu && g_active_menu->FirstItem)
                    {
                        /* Calculate position relative to menu drop-down */
                        WORD menuTop = screen->BarHeight + 1;
                        WORD menuLeft = screen->BarHBorder + g_active_menu->LeftEdge + g_active_menu->BeatX;
                        WORD itemX = mouseX - menuLeft;
                        WORD itemY = mouseY - menuTop;
                        
                        struct MenuItem *newItem = _find_item_at_pos(g_active_menu, itemX, itemY);
                        if (newItem != g_active_item)
                        {
                            g_active_item = newItem;
                            needRedraw = TRUE;
                        }
                    }
                    else
                    {
                        /* Mouse outside menu areas */
                        if (g_active_item)
                        {
                            g_active_item = NULL;
                            needRedraw = TRUE;
                        }
                    }
                    
                    /* Redraw if selection changed */
                    if (needRedraw)
                    {
                        if (oldMenu != g_active_menu)
                        {
                            _render_menu_bar(g_menu_window);
                        }
                        if (g_active_menu)
                        {
                            _render_menu_items(g_menu_window);
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
                    /* No active string gadget - post RAWKEY to window as normal */
                    WORD relX = mouseX - window->LeftEdge;
                    WORD relY = mouseY - window->TopEdge;
                    _post_idcmp_message(window, IDCMP_RAWKEY, rawkey, 
                                       qualifier, NULL, relX, relY);
                }
                break;
            }
            
            case 4:  /* Close window request (from host window manager) */
            {
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
        return;
    }
    
    for (screen = IntuitionBase->FirstScreen; screen; screen = screen->NextScreen)
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
        /* Just free the message - we don't track them centrally */
        FreeMem(imsg, sizeof(struct IntuiMessage));
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
    
    if (rootless_mode && window->UserData)
    {
        /* Update host window - emucall expects absolute coordinates */
        emucall3(EMU_CALL_INT_MOVE_WINDOW, (ULONG)window->UserData, (ULONG)new_x, (ULONG)new_y);
    }

    /* Send IDCMP_CHANGEWINDOW notification */
    if (window->IDCMPFlags & IDCMP_CHANGEWINDOW)
    {
        struct IntuiMessage *msg;
        msg = (struct IntuiMessage *)AllocMem(sizeof(struct IntuiMessage), MEMF_PUBLIC | MEMF_CLEAR);
        if (msg)
        {
            msg->Class = IDCMP_CHANGEWINDOW;
            msg->Code = CWCODE_MOVESIZE;
            msg->Qualifier = 0;
            msg->IAddress = window;
            msg->IDCMPWindow = window;
            msg->MouseX = window->MouseX;
            msg->MouseY = window->MouseY;
            
            if (window->UserPort)
            {
                PutMsg(window->UserPort, (struct Message *)msg);
            }
            else
            {
                FreeMem(msg, sizeof(struct IntuiMessage));
            }
        }
    }
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
    /*
     * OffMenu() disables a menu, menu item, or sub-item.
     * The menuNumber encodes: menu (bits 0-4), item (bits 5-10), subitem (bits 11-15).
     * For now we just log it - menu rendering isn't implemented yet.
     */
    DPRINTF (LOG_DEBUG, "_intuition: OffMenu() window=0x%08lx menuNumber=0x%04x (no-op)\n",
             (ULONG)window, (unsigned)menuNumber);
    /* No-op - menu enable/disable is visual only */
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
    /*
     * OnMenu() enables a menu, menu item, or sub-item.
     * The menuNumber encodes: menu (bits 0-4), item (bits 5-10), subitem (bits 11-15).
     * For now we just log it - menu rendering isn't implemented yet.
     */
    DPRINTF (LOG_DEBUG, "_intuition: OnMenu() window=0x%08lx menuNumber=0x%04x (no-op)\n",
             (ULONG)window, (unsigned)menuNumber);
    /* No-op - menu enable/disable is visual only */
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

    /* Set bar heights (simplified) */
    screen->BarHeight = 10;
    screen->BarVBorder = 1;
    screen->BarHBorder = 5;
    screen->WBorTop = 11;
    screen->WBorLeft = 4;
    screen->WBorRight = 4;
    screen->WBorBottom = 2;

    /* Clear the screen to color 0 */
    SetRast(&screen->RastPort, 0);

    /* Initialize Layer_Info for this screen */
    InitLayers(&screen->LayerInfo);
    screen->LayerInfo.top_layer = NULL;

    /* Link screen into IntuitionBase screen list (at front) */
    screen->NextScreen = IntuitionBase->FirstScreen;
    IntuitionBase->FirstScreen = screen;
    
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
    WORD rightX = window->Width - window->BorderRight - gadWidth;
    
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
    struct Gadget *gad;
    UBYTE detPen, blkPen, shiPen, shaPen;
    
    LPRINTF(LOG_INFO, "_intuition: _render_window_frame() window=0x%08lx RPort=0x%08lx\n", 
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
                /* Draw overlapping rectangles (depth icon) */
                WORD cx = (gx0 + gx1) / 2;
                WORD cy = (gy0 + gy1) / 2;
                /* Back rectangle */
                SetAPen(rp, shaPen);
                Move(rp, cx - 3, cy + 2);
                Draw(rp, cx - 3, cy - 2);
                Draw(rp, cx + 1, cy - 2);
                Draw(rp, cx + 1, cy + 2);
                Draw(rp, cx - 3, cy + 2);
                /* Front rectangle */
                SetAPen(rp, shiPen);
                Move(rp, cx - 1, cy + 3);
                Draw(rp, cx - 1, cy);
                Draw(rp, cx + 3, cy);
                Draw(rp, cx + 3, cy + 3);
                Draw(rp, cx - 1, cy + 3);
                break;
            }
            
            case GTYP_WDRAGGING:
                /* Drag gadget has no special imagery - just the frame */
                break;
        }
    }
}

struct Window * _intuition_OpenWindow ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register const struct NewWindow * newWindow __asm("a0"))
{
    struct Window *window;
    struct Screen *screen;
    WORD width, height;
    ULONG rootless_mode;

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
            if (screen->Flags & WBENCHSCREEN)
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
                if (screen->Flags & WBENCHSCREEN)
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

    /* Apply defaults if dimensions are zero */
    if (width == 0)
        width = screen->Width - newWindow->LeftEdge;
    if (height == 0)
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

    if (rootless_mode)
    {
        /* Phase 15: Create a separate host window for this Amiga window */
        ULONG window_handle;
        ULONG screen_handle = (ULONG)screen->ExtData;

        window_handle = emucall4(EMU_CALL_INT_OPEN_WINDOW,
                                 screen_handle,
                                 ((ULONG)(WORD)newWindow->LeftEdge << 16) | ((ULONG)(WORD)newWindow->TopEdge & 0xFFFF),
                                 ((ULONG)width << 16) | ((ULONG)height & 0xFFFF),
                                 (ULONG)newWindow->Title);

        if (window_handle == 0)
        {
            LPRINTF (LOG_WARNING, "_intuition: OpenWindow() rootless window creation failed, continuing without\n");
        }

        /* Store the host window handle in UserData */
        window->UserData = (APTR)window_handle;

        DPRINTF (LOG_DEBUG, "_intuition: OpenWindow() rootless window_handle=0x%08lx\n", window_handle);
    }
    else
    {
        window->UserData = NULL;
    }

    /* Create IDCMP message port if IDCMP flags are set */
    if (newWindow->IDCMPFlags)
    {
        window->UserPort = CreateMsgPort();
        if (!window->UserPort)
        {
            LPRINTF (LOG_ERROR, "_intuition: OpenWindow() failed to create IDCMP port\n");
            if (window->UserData)
            {
                emucall1(EMU_CALL_INT_CLOSE_WINDOW, (ULONG)window->UserData);
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
        window->Flags |= WFLG_WINDOWACTIVE;
        /* TODO: Deactivate previous active window */
    }

    /* Create system gadgets based on window flags */
    _create_window_sys_gadgets(window);
    
    /* Render the window frame and gadgets (in non-rootless mode) */
    if (!rootless_mode)
    {
        _render_window_frame(window);
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
        if (wbscreen->Flags & WBENCHSCREEN)
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
    DPRINTF (LOG_DEBUG, "_intuition: RefreshGadgets() stub - no-op for now\n");
    /* TODO: Implement gadget refresh when gadget system is implemented */
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
    
    left = req->LeftEdge;
    top = req->TopEdge;
    width = req->Width;
    height = req->Height;
    
    if (req->Flags & POINTREL)
    {
        left += window->BorderLeft; /* Or current mouse pos? POINTREL usually relative to window top-left or mouse? 
                                     * RKRM: "LeftEdge, TopEdge are relative to the window's top-left."
                                     * Wait, POINTREL usually means relative to Mouse? No, that's specialized.
                                     * Usually standard requesters are window-relative.
                                     */
         /* Actually, if not POINTREL, it's relative to screen? No, Requester is always in Window. 
          * flags: POINTREL = "Relative to the pointer position".
          */
    }
    
    /* Draw background */
    SetAPen(rp, req->BackFill);
    RectFill(rp, left, top, left + width - 1, top + height - 1);
    
    /* Draw Border if present */
    /* TODO: Render req->ReqBorder */
    
    /* Draw Text if present */
    /* TODO: Render req->ReqText via PrintIText (which calls IntuiTextLength/Move/Text) */
    
    /* Draw Gadgets */
    /* TODO: RefreshGList(req->ReqGadget, window, req, -1) */
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
    DPRINTF (LOG_ERROR, "_intuition: SetDMRequest() unimplemented STUB called.\n");
    assert(FALSE);
    return FALSE;
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
        /* TODO: actually redraw the screen title */
    }
}

VOID _intuition_ShowTitle ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Screen * screen __asm("a0"),
                                                        register BOOL showIt __asm("d0"))
{
    /*
     * ShowTitle() controls whether the screen title bar is shown in front of
     * or behind backdrop windows. Since we don't have true screen layering,
     * this is a no-op.
     */
    DPRINTF (LOG_DEBUG, "_intuition: ShowTitle() screen=0x%08lx showIt=%d\n", screen, showIt);
}

VOID _intuition_SizeWindow ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Window * window __asm("a0"),
                                                        register WORD dx __asm("d0"),
                                                        register WORD dy __asm("d1"))
{
    LONG new_w, new_h;
    BOOL rootless_mode;

    DPRINTF(LOG_DEBUG, "_intuition: SizeWindow() window=0x%08lx dx=%d dy=%d\n", (ULONG)window, dx, dy);

    if (!window) return;

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

    /* Update Layer if present */
    if (window->WLayer && LayersBase)
    {
        _call_SizeLayer(LayersBase, window->WLayer, dx, dy);
    }

    /* Check for rootless mode */
    rootless_mode = emucall0(EMU_CALL_INT_GET_ROOTLESS);
    
    if (rootless_mode && window->UserData)
    {
        /* Update host window - emucall expects absolute dimensions */
        emucall3(EMU_CALL_INT_SIZE_WINDOW, (ULONG)window->UserData, (ULONG)new_w, (ULONG)new_h);
    }

    /* Send IDCMP_NEWSIZE */
    if (window->IDCMPFlags & IDCMP_NEWSIZE)
    {
        struct IntuiMessage *msg;
        msg = (struct IntuiMessage *)AllocMem(sizeof(struct IntuiMessage), MEMF_PUBLIC | MEMF_CLEAR);
        if (msg)
        {
            msg->Class = IDCMP_NEWSIZE;
            msg->Code = 0;
            msg->Qualifier = 0;
            msg->IAddress = window;
            msg->IDCMPWindow = window;
            msg->MouseX = window->MouseX;
            msg->MouseY = window->MouseY; 
            
            if (window->UserPort)
            {
                PutMsg(window->UserPort, (struct Message *)msg);
            }
            else
            {
                FreeMem(msg, sizeof(struct IntuiMessage));
            }
        }
    }

    /* Send IDCMP_CHANGEWINDOW */
    if (window->IDCMPFlags & IDCMP_CHANGEWINDOW)
    {
        struct IntuiMessage *msg;
        msg = (struct IntuiMessage *)AllocMem(sizeof(struct IntuiMessage), MEMF_PUBLIC | MEMF_CLEAR);
        if (msg)
        {
            msg->Class = IDCMP_CHANGEWINDOW;
            msg->Code = CWCODE_MOVESIZE;
            msg->Qualifier = 0;
            msg->IAddress = window;
            msg->IDCMPWindow = window;
            msg->MouseX = window->MouseX;
            msg->MouseY = window->MouseY;
            
            if (window->UserPort)
            {
                PutMsg(window->UserPort, (struct Message *)msg);
            }
            else
            {
                FreeMem(msg, sizeof(struct IntuiMessage));
            }
        }
    }
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

    DPRINTF (LOG_DEBUG, "_intuition: WindowToBack() window=0x%08lx\n", (ULONG)window);
    
    if (!window) return;

    /* Update internal structures */
    if (window->WLayer && LayersBase)
    {
        _call_BehindLayer(LayersBase, window->WLayer);
    }

    /* Check for rootless mode */
    rootless_mode = emucall0(EMU_CALL_INT_GET_ROOTLESS);
    
    if (rootless_mode && window->UserData)
    {
        emucall1(EMU_CALL_INT_WINDOW_TOBACK, (ULONG)window->UserData);
    }

    /* Send IDCMP_CHANGEWINDOW notification */
    if (window->IDCMPFlags & IDCMP_CHANGEWINDOW)
    {
        struct IntuiMessage *msg;
        msg = (struct IntuiMessage *)AllocMem(sizeof(struct IntuiMessage), MEMF_PUBLIC | MEMF_CLEAR);
        if (msg)
        {
            msg->Class = IDCMP_CHANGEWINDOW;
            msg->Code = CWCODE_DEPTH;
            msg->Qualifier = 0;
            msg->IAddress = window;
            msg->IDCMPWindow = window;
            msg->MouseX = window->MouseX;
            msg->MouseY = window->MouseY;
            
            if (window->UserPort)
            {
                PutMsg(window->UserPort, (struct Message *)msg);
            }
            else
            {
                FreeMem(msg, sizeof(struct IntuiMessage));
            }
        }
    }
}

VOID _intuition_WindowToFront ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Window * window __asm("a0"))
{
    BOOL rootless_mode;

    DPRINTF (LOG_DEBUG, "_intuition: WindowToFront() window=0x%08lx\n", (ULONG)window);
    
    if (!window) return;

    /* Update internal structures */
    if (window->WLayer && LayersBase)
    {
        _call_UpfrontLayer(LayersBase, window->WLayer);
    }

    /* Check for rootless mode */
    rootless_mode = emucall0(EMU_CALL_INT_GET_ROOTLESS);
    
    if (rootless_mode && window->UserData)
    {
        emucall1(EMU_CALL_INT_WINDOW_TOFRONT, (ULONG)window->UserData);
    }

    /* Send IDCMP_CHANGEWINDOW notification */
    if (window->IDCMPFlags & IDCMP_CHANGEWINDOW)
    {
        struct IntuiMessage *msg;
        msg = (struct IntuiMessage *)AllocMem(sizeof(struct IntuiMessage), MEMF_PUBLIC | MEMF_CLEAR);
        if (msg)
        {
            msg->Class = IDCMP_CHANGEWINDOW;
            msg->Code = CWCODE_DEPTH;
            msg->Qualifier = 0;
            msg->IAddress = window;
            msg->IDCMPWindow = window;
            msg->MouseX = window->MouseX;
            msg->MouseY = window->MouseY;
            
            if (window->UserPort)
            {
                PutMsg(window->UserPort, (struct Message *)msg);
            }
            else
            {
                FreeMem(msg, sizeof(struct IntuiMessage));
            }
        }
    }
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
    DPRINTF (LOG_ERROR, "_intuition: SetPrefs() unimplemented STUB called.\n");
    assert(FALSE);
    return NULL;
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
    DPRINTF (LOG_DEBUG, "_intuition: WBenchToBack() stub called - no-op.\n");
    return TRUE;
}

BOOL _intuition_WBenchToFront ( register struct IntuitionBase * IntuitionBase __asm("a6"))
{
    struct Screen *wbscreen;
    
    DPRINTF (LOG_DEBUG, "_intuition: WBenchToFront() called.\n");
    
    /* First, open the Workbench screen if it doesn't exist.
     * According to AmigaOS behavior, WBenchToFront should open the
     * Workbench if not already open (similar to OpenWorkBench but
     * also brings it to front).
     */
    wbscreen = IntuitionBase->FirstScreen;
    while (wbscreen)
    {
        if (wbscreen->Flags & WBENCHSCREEN)
            break;
        wbscreen = wbscreen->NextScreen;
    }
    
    if (!wbscreen)
    {
        /* Workbench not open - open it */
        DPRINTF (LOG_DEBUG, "_intuition: WBenchToFront() - Workbench not open, opening it.\n");
        if (!_intuition_OpenWorkBench(IntuitionBase))
        {
            DPRINTF (LOG_ERROR, "_intuition: WBenchToFront() - failed to open Workbench.\n");
            return FALSE;
        }
    }
    
    /* TODO: Bring Workbench to front (screen depth ordering) */
    /* For now, since we typically only have one screen, this is a no-op */
    
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
    /* AutoRequest() displays a modal requester with Yes/No buttons.
     * For now, we just log the request and return TRUE (positive response).
     * A full implementation would create a window and handle user input. */

    DPRINTF (LOG_DEBUG, "_intuition: AutoRequest() window=0x%08lx pFlag=0x%08lx nFlag=0x%08lx %dx%d\n",
             (ULONG)window, pFlag, nFlag, (int)width, (int)height);

    /* Print the body text chain to debug output */
    const struct IntuiText *it = body;
    while (it)
    {
        if (it->IText)
        {
            DPRINTF (LOG_DEBUG, "_intuition: AutoRequest body: %s\n", (char *)it->IText);
        }
        it = it->NextText;
    }

    /* Print button texts if available */
    if (posText && posText->IText)
    {
        DPRINTF (LOG_DEBUG, "_intuition: AutoRequest positive: %s\n", (char *)posText->IText);
    }
    if (negText && negText->IText)
    {
        DPRINTF (LOG_DEBUG, "_intuition: AutoRequest negative: %s\n", (char *)negText->IText);
    }

    /* Return TRUE (positive response) by default */
    return TRUE;
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
    DPRINTF(LOG_DEBUG, "_intuition: BeginRefresh() window=0x%08lx\n", (ULONG)window);

    if (!window)
        return;

    struct Layer *layer = window->WLayer;
    if (!layer)
    {
        DPRINTF(LOG_DEBUG, "_intuition: BeginRefresh() window has no layer\n");
        return;
    }

    /* Call the layers.library BeginUpdate which sets up clipping
     * to only allow drawing in damaged regions */
    BeginUpdate(layer);

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
    struct NewWindow nw;
    struct Window *reqWindow;
    /* struct Gadget *posGad = NULL, *negGad = NULL; */
    /* LONG textW, textH; */
    LONG winW = width, winH = height;
    
    DPRINTF (LOG_DEBUG, "_intuition: BuildSysRequest() body=%s\n", 
             (bodyText && bodyText->IText) ? (char *)bodyText->IText : "NULL");

    /* Default dimensions if not provided */
    if (winW == 0) winW = 320;
    if (winH == 0) winH = 100;
    
    /* TODO: Calculate actual size from text */

    memset(&nw, 0, sizeof(nw));
    nw.LeftEdge = (window) ? window->LeftEdge + 20 : 0;
    nw.TopEdge = (window) ? window->TopEdge + 20 : 0;
    nw.Width = winW;
    nw.Height = winH;
    nw.DetailPen = 0;
    nw.BlockPen = 1;
    nw.Title = (UBYTE *)"System Request";
    nw.Flags = WFLG_ACTIVATE | WFLG_RMBTRAP | WFLG_NOCAREREFRESH | WFLG_SIMPLE_REFRESH | WFLG_BORDERLESS;
    nw.IDCMPFlags = IDCMP_GADGETUP | IDCMP_MOUSEBUTTONS | IDCMP_VANILLAKEY;
    
    if (window && window->WScreen) {
        nw.Type = CUSTOMSCREEN;
        nw.Screen = window->WScreen;
    } else {
        nw.Type = WBENCHSCREEN;
    }
    
    /* Open Window */
    reqWindow = _intuition_OpenWindow(IntuitionBase, &nw);
    if (!reqWindow) return NULL;
    
    /* Create Gadgets manually for now (simplified) 
     * In a real implementation, we'd use NewObject or alloc Gadget structs.
     * Here we'll just alloc memory for them.
     */
     
    /* TODO: Create Gadgets */
    /* Draw Text */
    /* TODO: Draw IntuiText */
    
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
    DPRINTF(LOG_DEBUG, "_intuition: EndRefresh() window=0x%08lx complete=%ld\n",
            (ULONG)window, complete);

    if (!window)
        return;

    struct Layer *layer = window->WLayer;
    if (!layer)
    {
        DPRINTF(LOG_DEBUG, "_intuition: EndRefresh() window has no layer\n");
        return;
    }

    /* Call the layers.library EndUpdate to restore normal clipping.
     * If complete is TRUE (non-zero), clear the damage list. */
    EndUpdate(layer, complete ? TRUE : FALSE);

    /* Clear the LAYERREFRESH flag if we're done */
    if (complete && layer)
    {
        layer->Flags &= ~LAYERREFRESH;
    }
}

VOID _intuition_FreeSysRequest ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Window * window __asm("a0"))
{
    DPRINTF (LOG_DEBUG, "_intuition: FreeSysRequest() window=0x%08lx\n", (ULONG)window);
    
    if (window)
    {
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
    DPRINTF (LOG_ERROR, "_intuition: private0() unimplemented STUB called.\n");
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

/* Helper to render a single gadget */
static void _render_gadget(struct Window *window, struct Requester *req, struct Gadget *gad)
{
    struct RastPort *rp;
    LONG left, top, width, height;
    
    if (!window || !gad || !window->RPort) return;
    
    rp = window->RPort;
    
    /* Calculate base position */
    left = gad->LeftEdge;
    top = gad->TopEdge;
    width = gad->Width;
    height = gad->Height;
    
    if (req)
    {
        left += req->LeftEdge;
        top += req->TopEdge;
    }
    
    /* Handle relative positioning flags */
    if (gad->Flags & GFLG_RELRIGHT)
    {
        LONG containerW = (req) ? req->Width : window->Width;
        left += containerW;
    }
    if (gad->Flags & GFLG_RELBOTTOM)
    {
        LONG containerH = (req) ? req->Height : window->Height;
        top += containerH;
    }
    if (gad->Flags & GFLG_RELWIDTH)
    {
        LONG containerW = (req) ? req->Width : window->Width;
        width += containerW;
    }
    if (gad->Flags & GFLG_RELHEIGHT)
    {
        LONG containerH = (req) ? req->Height : window->Height;
        height += containerH;
    }
    
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
     
    /* Draw Image */
    if (gad->Flags & GFLG_GADGIMAGE)
    {
        if (gad->GadgetRender)
        {
            /* If selected and GFLG_SELECTED, use SelectRender if available?
             * Or if GFLG_GADGHIMAGE is set, use SelectRender.
             */
             struct Image *img = (struct Image *)gad->GadgetRender;
             if ((gad->Flags & GFLG_SELECTED) && (gad->Flags & GFLG_GADGHIMAGE) && gad->SelectRender)
             {
                 img = (struct Image *)gad->SelectRender;
             }
             
             _intuition_DrawImage((struct IntuitionBase *)NULL, rp, img, left, top);
        }
    }
    else if (gad->Flags & GFLG_GADGHBOX)
    {
        /* Draw Border - use border pen */
        /* _intuition_DrawBorder(...) */
        /* For now simple rect */
        SetAPen(rp, window->BlockPen);
        Move(rp, left, top);
        Draw(rp, left + width - 1, top);
        Draw(rp, left + width - 1, top + height - 1);
        Draw(rp, left, top + height - 1);
        Draw(rp, left, top);
    }
    else if (gad->Flags & GFLG_GADGHCOMP)
    {
        /* Complement mode - usually inverts on selection, but initially draws normally? 
         * Or just a box?
         */
    }
    
    /* Draw Text */
    if (gad->GadgetText)
    {
        struct IntuiText *it = gad->GadgetText;
        /* Calculate text position */
        LONG tx = left + it->LeftEdge;
        LONG ty = top + it->TopEdge;
        /* _intuition_PrintIText(..., rp, it, tx, ty); */
        /* Stub for PrintIText inside module? */
        /* We can use the text routines directly if needed */
        if (it->IText)
        {
            SetAPen(rp, it->FrontPen);
            SetBPen(rp, it->BackPen);
            Move(rp, tx, ty);
            Text(rp, (STRPTR)it->IText, strlen((char *)it->IText));
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
    DPRINTF (LOG_DEBUG, "_intuition: AddGList() window=0x%08lx gadget=0x%08lx pos=%d numGad=%d req=0x%08lx\n",
             (ULONG)window, (ULONG)gadget, position, numGad, (ULONG)requester);

    if (!window || !gadget)
        return 0;

    /* Count the gadgets to add (or use numGad if specified) */
    WORD count = 0;
    struct Gadget *gad = gadget;
    struct Gadget *last = gadget;
    
    while (gad && (numGad == -1 || count < numGad)) {
        count++;
        last = gad;
        gad = gad->NextGadget;
    }
    
    /* Insert at the specified position in window's gadget list */
    if (position == 0 || position == (UWORD)~0 || !window->FirstGadget) {
        /* Insert at beginning or first position */
        last->NextGadget = window->FirstGadget;
        window->FirstGadget = gadget;
    } else {
        /* Find the insertion point */
        struct Gadget *prev = window->FirstGadget;
        UWORD pos = 1;
        
        while (prev->NextGadget && pos < position) {
            prev = prev->NextGadget;
            pos++;
        }
        
        /* Insert after prev */
        last->NextGadget = prev->NextGadget;
        prev->NextGadget = gadget;
    }
    
    return (UWORD)count;
}

UWORD _intuition_RemoveGList ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Window * remPtr __asm("a0"),
                                                        register struct Gadget * gadget __asm("a1"),
                                                        register WORD numGad __asm("d0"))
{
    DPRINTF (LOG_DEBUG, "_intuition: RemoveGList() window=0x%08lx gadget=0x%08lx numGad=%d\n",
             (ULONG)remPtr, (ULONG)gadget, numGad);

    if (!remPtr || !gadget)
        return 0;

    /* Find the gadget in the window's list */
    struct Gadget *prev = NULL;
    struct Gadget *curr = remPtr->FirstGadget;
    
    while (curr && curr != gadget) {
        prev = curr;
        curr = curr->NextGadget;
    }
    
    if (!curr)
        return 0;  /* Not found */

    /* Count gadgets to remove */
    UWORD count = 0;
    struct Gadget *last = gadget;
    
    while (last && (numGad == -1 || count < (UWORD)numGad)) {
        count++;
        if (!last->NextGadget || (numGad != -1 && count >= (UWORD)numGad))
            break;
        last = last->NextGadget;
    }
    
    /* Remove the gadgets from the list */
    if (prev)
        prev->NextGadget = last->NextGadget;
    else
        remPtr->FirstGadget = last->NextGadget;
    
    return count;
}

VOID _intuition_ActivateWindow ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Window * window __asm("a0"))
{
    DPRINTF (LOG_DEBUG, "_intuition: ActivateWindow() window=0x%08lx\n", (ULONG)window);

    if (!window)
        return;

    /* Set this window as the active window */
    IntuitionBase->ActiveWindow = window;
    
    /* Set the WFLG_WINDOWACTIVE flag */
    window->Flags |= WFLG_WINDOWACTIVE;
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
    DPRINTF (LOG_DEBUG, "_intuition: ActivateGadget() gad=0x%08lx\n", (ULONG)gadget);
    
    if (!gadget || !window) return FALSE;
    
    /* Typically this is for string gadgets or proprietary gadgets that accept input.
     * We should set the active gadget field in the window.
     * Note: Does not handle full activation logic (QWERTY, cursor, etc.) yet, just state.
     */
     
    /* Deactivate old if any (TODO: Send IDCMP_GADGETUP/DOWN if needed?) */
    
    /* Set new */
    /* window->ActiveGadget = gadget; ? No standard field exposed like this in simple Window struct?
     * Actually `Window` has `ActiveGadget`?
     * Let's check struct definition again via our logic.
     * No, standard struct Window does not have ActiveGadget directly.
     * It's usually tracked internally by Intuition.
     */
     
    /* For now, just return TRUE to pretend success. */
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
    DPRINTF (LOG_ERROR, "_intuition: NewModifyProp() unimplemented STUB called.\n");
    assert(FALSE);
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
    DPRINTF (LOG_ERROR, "_intuition: MoveWindowInFrontOf() unimplemented STUB called.\n");
    assert(FALSE);
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
    DPRINTF (LOG_ERROR, "_intuition: SetEditHook() unimplemented STUB called.\n");
    assert(FALSE);
    return NULL;
}

LONG _intuition_SetMouseQueue ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Window * window __asm("a0"),
                                                        register UWORD queueLength __asm("d0"))
{
    DPRINTF (LOG_ERROR, "_intuition: SetMouseQueue() unimplemented STUB called.\n");
    assert(FALSE);
    return 0;
}

VOID _intuition_ZipWindow ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Window * window __asm("a0"))
{
    WORD target_w, target_h;
    WORD screen_w, screen_h;
    
    DPRINTF (LOG_DEBUG, "_intuition: ZipWindow() window=0x%08lx\n", (ULONG)window);
    
    if (!window) return;
    
    /* Determine screen dimensions for clamping/max */
    if (window->WScreen)
    {
        screen_w = window->WScreen->Width;
        screen_h = window->WScreen->Height;
    }
    else
    {
        screen_w = 640; /* Fallback */
        screen_h = 256;
    }

    /* Simple heuristic:
     * If currently small (at min dimensions), toggle to max.
     * Otherwise, toggle to min.
     */
    if (window->Width <= window->MinWidth && window->Height <= window->MinHeight)
    {
        /* Toggle to Max */
        target_w = (window->MaxWidth != (UWORD)-1) ? window->MaxWidth : screen_w;
        target_h = (window->MaxHeight != (UWORD)-1) ? window->MaxHeight : screen_h;
        
        /* Clamp to screen */
        if (target_w > screen_w) target_w = screen_w;
        if (target_h > screen_h) target_h = screen_h;
    }
    else
    {
        /* Toggle to Min */
        target_w = window->MinWidth;
        target_h = window->MinHeight;
    }

    /* Use ChangeWindowBox to effect the change (keeping Top/Left for now) 
     * TODO: Adjust Top/Left to keep centered or anchored? 
     * AmigaOS usually keeps Top/Left unless it would push window offscreen.
     */
    _intuition_ChangeWindowBox(IntuitionBase, window, window->LeftEdge, window->TopEdge, target_w, target_h);
}

struct Screen * _intuition_LockPubScreen ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register CONST_STRPTR name __asm("a0"))
{
    struct Screen *screen;
    
    DPRINTF (LOG_DEBUG, "_intuition: LockPubScreen() name='%s', FirstScreen=0x%08lx\n", 
             name ? (char *)name : "(null/default)", (ULONG)IntuitionBase->FirstScreen);
    
    /* If name is NULL or "Workbench", return the default public screen */
    if (!name || strcmp((const char *)name, "Workbench") == 0)
    {
        screen = IntuitionBase->FirstScreen;
        if (screen)
        {
            /* Increment a visitor count (we don't actually track this, but apps expect it to work) */
            DPRINTF (LOG_DEBUG, "_intuition: LockPubScreen() returning FirstScreen=0x%08lx\n", (ULONG)screen);
            return screen;
        }
        
        /* No screen exists - auto-open Workbench screen */
        DPRINTF (LOG_INFO, "_intuition: LockPubScreen() no screen, auto-opening Workbench\n");
        if (_intuition_OpenWorkBench(IntuitionBase))
        {
            screen = IntuitionBase->FirstScreen;
            if (screen)
            {
                DPRINTF (LOG_DEBUG, "_intuition: LockPubScreen() returning new Workbench screen=0x%08lx\n", (ULONG)screen);
                return screen;
            }
        }
        DPRINTF (LOG_ERROR, "_intuition: LockPubScreen() failed to auto-open Workbench\n");
        return NULL;
    }
    
    /* Named public screens not supported yet - return NULL
     * (app should fall back to default screen) */
    DPRINTF (LOG_DEBUG, "_intuition: LockPubScreen() named screen '%s' not found\n", (char *)name);
    return NULL;
}

VOID _intuition_UnlockPubScreen ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register CONST_STRPTR name __asm("a0"),
                                                        register struct Screen * screen __asm("a1"))
{
    DPRINTF (LOG_DEBUG, "_intuition: UnlockPubScreen() name='%s', screen=0x%08lx\n",
             name ? (char *)name : "(null)", (ULONG)screen);
    /* In a full implementation, we'd decrement a visitor count */
    /* For now, this is a no-op since we don't track screen locks */
}

struct List * _intuition_LockPubScreenList ( register struct IntuitionBase * IntuitionBase __asm("a6"))
{
    /* Lock the list of public screens for reading.
     * Returns a pointer to the list head.
     * In our simplified implementation, we don't have a separate pub screen list,
     * so we return NULL. Applications should handle this gracefully.
     */
    DPRINTF (LOG_DEBUG, "_intuition: LockPubScreenList() - returning NULL (no pub screen list)\n");
    
    /* In a full implementation, this would:
     * 1. ObtainSemaphore on the pub screen list semaphore
     * 2. Return pointer to IntuitionBase's internal pub screen list
     * For now, return NULL since we don't track public screens separately.
     */
    return NULL;
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
    /*
     * PubScreenStatus() changes the status of a public screen.
     * Status flags include PSNF_PRIVATE (make private).
     * Returns the old status, or 0 if screen has visitor windows (can't make private).
     * We don't track public screens properly yet, so just return success.
     */
    DPRINTF (LOG_DEBUG, "_intuition: PubScreenStatus() screen=0x%08lx statusFlags=0x%04x\n",
             (ULONG)screen, statusFlags);
    return 1;  /* Success - old status was "public" */
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
    DPRINTF (LOG_ERROR, "_intuition: private1() unimplemented STUB called.\n");
    assert(FALSE);
}

VOID _intuition_GetDefaultPubScreen ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register STRPTR nameBuffer __asm("a0"))
{
    /* Get the name of the default public screen.
     * Copies the name into nameBuffer (which must be at least MAXPUBSCREENNAME+1 bytes).
     */
    DPRINTF (LOG_DEBUG, "_intuition: GetDefaultPubScreen(nameBuffer=%p)\n", nameBuffer);
    
    if (!nameBuffer)
        return;
    
    /* In our simplified implementation, Workbench is always the default */
    strcpy((char *)nameBuffer, "Workbench");
}

LONG _intuition_EasyRequestArgs ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Window * window __asm("a0"),
                                                        register const struct EasyStruct * easyStruct __asm("a1"),
                                                        register ULONG * idcmpPtr __asm("a2"),
                                                        register const APTR args __asm("a3"))
{
    /* EasyRequestArgs() displays a requester with formatted text.
     * For now, we log the request and return 1 (first/rightmost gadget).
     * A full implementation would create a window and handle user input.
     *
     * Return values:
     *  0 = rightmost gadget (usually negative/cancel)
     *  1 = leftmost gadget (usually positive/ok)
     *  n = nth gadget from the right
     * -1 = IDCMP message received (if idcmpPtr != NULL)
     */

    DPRINTF (LOG_DEBUG, "_intuition: EasyRequestArgs() window=0x%08lx easyStruct=0x%08lx\n",
             (ULONG)window, (ULONG)easyStruct);

    if (easyStruct)
    {
        if (easyStruct->es_Title)
        {
            DPRINTF (LOG_DEBUG, "_intuition: EasyRequest title: %s\n", (char *)easyStruct->es_Title);
        }
        if (easyStruct->es_TextFormat)
        {
            DPRINTF (LOG_DEBUG, "_intuition: EasyRequest text: %s\n", (char *)easyStruct->es_TextFormat);
        }
        if (easyStruct->es_GadgetFormat)
        {
            DPRINTF (LOG_DEBUG, "_intuition: EasyRequest gadgets: %s\n", (char *)easyStruct->es_GadgetFormat);
        }
    }

    /* Return 1 (positive/first gadget response) by default */
    return 1;
}

struct Window * _intuition_BuildEasyRequestArgs ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Window * window __asm("a0"),
                                                        register const struct EasyStruct * easyStruct __asm("a1"),
                                                        register ULONG idcmp __asm("d0"),
                                                        register const APTR args __asm("a3"))
{
    DPRINTF (LOG_ERROR, "_intuition: BuildEasyRequestArgs() unimplemented STUB called.\n");
    assert(FALSE);
    return NULL;
}

LONG _intuition_SysReqHandler ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register struct Window * window __asm("a0"),
                                                        register ULONG * idcmpFlags __asm("a1"),
                                                        register LONG waitInput __asm("d0"))
{
    struct IntuiMessage *msg;
    LONG result = -2; /* No result yet */
    
    DPRINTF (LOG_DEBUG, "_intuition: SysReqHandler() window=0x%08lx wait=%ld\n", (ULONG)window, waitInput);
    
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
                return -1; /* Cancel */
            }
            /* Handle keyboard shortcuts if implemented */
        }
        
        if (!waitInput) break;
    }
    
    return result;
}

struct Window * _intuition_OpenWindowTagList ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register const struct NewWindow * newWindow __asm("a0"),
                                                        register const struct TagItem * tagList __asm("a1"))
{
    struct NewWindow nw;
    struct TagItem *tstate;
    struct TagItem *tag;
    
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
        /* Initialize with defaults */
        memset(&nw, 0, sizeof(nw));
        nw.Width = 200;
        nw.Height = 100;
        nw.DetailPen = 0;
        nw.BlockPen = 1;
        nw.Flags = WFLG_DRAGBAR | WFLG_DEPTHGADGET | WFLG_CLOSEGADGET;
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
                /* Tags we recognize but don't fully implement yet */
                case WA_PubScreenName:
                case WA_PubScreen:
                case WA_PubScreenFallBack:
                case WA_InnerWidth:
                case WA_InnerHeight:
                case WA_Zoom:
                case WA_MouseQueue:
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
    
    /* Call our existing OpenWindow with the assembled NewWindow */
    return _intuition_OpenWindow(IntuitionBase, &nw);
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
                    ns.Type = (UWORD)tag->ti_Data;
                    break;
                case SA_Behind:
                    if (tag->ti_Data)
                        ns.Type |= SCREENBEHIND;
                    break;
                case SA_Quiet:
                    if (tag->ti_Data)
                        ns.Type |= SCREENQUIET;
                    break;
                case SA_ShowTitle:
                    if (tag->ti_Data)
                        ns.Type |= SHOWTITLE;
                    break;
                case SA_AutoScroll:
                    if (tag->ti_Data)
                        ns.Type |= AUTOSCROLL;
                    break;
                case SA_BitMap:
                    ns.CustomBitMap = (struct BitMap *)tag->ti_Data;
                    if (tag->ti_Data)
                        ns.Type |= CUSTOMBITMAP;
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
    
    DPRINTF (LOG_DEBUG, "_intuition: DrawImageState() image=0x%08lx state=0x%08lx\n", (ULONG)image, state);
    
    if (!image || !rp) return;
    
    if (_is_object(base, image)) {
        /* Dispatch IM_DRAW */
        struct impDraw imp;
        imp.MethodID = IM_DRAW;
        imp.imp_RPort = rp;
        imp.imp_Offset.X = leftOffset;
        imp.imp_Offset.Y = topOffset;
        imp.imp_State = state;
        imp.imp_DrInfo = (struct DrawInfo *)drawInfo;
        
        /* Dimensions usually come from object itself, but impDraw doesn't take them.
         * The object knows its size.
         */
         
        _intuition_dispatch_method(_OBJECT(image)->o_Class, (Object *)image, (Msg)&imp);
    } else {
        /* Standard Image */
        _intuition_DrawImage(IntuitionBase, rp, image, leftOffset, topOffset);
    }
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
    
    if (_is_object(base, image)) {
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
     * SetGadgetAttrsA() sets attributes on a BOOPSI gadget.
     * For now, return 0 (no attrs changed).
     * TODO: Implement BOOPSI gadget system
     */
    DPRINTF (LOG_DEBUG, "_intuition: SetGadgetAttrsA() gadget=0x%08lx window=0x%08lx (stub)\n",
             (ULONG)gadget, (ULONG)window);
    return 0;
}

APTR _intuition_NextObject ( register struct IntuitionBase * IntuitionBase __asm("a6"),
                                                        register APTR objectPtrPtr __asm("a0"))
{
    DPRINTF (LOG_DEBUG, "_intuition: NextObject() objectPtrPtr=0x%08lx (stub)\n", (ULONG)objectPtrPtr);
    return NULL;
}

VOID _intuition_private2 ( register struct IntuitionBase * IntuitionBase __asm("a6"))
{
    DPRINTF (LOG_ERROR, "_intuition: private2() unimplemented STUB called.\n");
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

    /* Determine superclass */
    if (!superClass && superClassID)
        superClass = _intuition_find_class(base, superClassID);
    if (!superClass)
        superClass = base->RootClass;
    
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

    if (!classPtr)
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

    if (classPtr->cl_SubclassCount != 0)
        return FALSE;

    if (classPtr->cl_ObjectCount != 0)
        return FALSE;

    if (classPtr->cl_Flags & CLF_INLIST)
        _intuition_RemoveClass(IntuitionBase, classPtr);

    if (classPtr->cl_Super && classPtr->cl_Super->cl_SubclassCount > 0)
        classPtr->cl_Super->cl_SubclassCount--;

    FreeMem(classPtr, sizeof(struct IClass) + (classPtr->cl_ID ? strlen((char *)classPtr->cl_ID) + 1 : 0));
    return TRUE;
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

    if (flags & SDEPTH_TOFRONT)
    {
        _intuition_ScreenToFront(IntuitionBase, screen);
    }
    else if (flags & SDEPTH_TOBACK)
    {
        _intuition_ScreenToBack(IntuitionBase, screen);
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
    return 0;
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
    return FALSE;
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
