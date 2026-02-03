/*
 * lxa datatypes.library implementation
 *
 * Basic framework for datatypes following AmigaOS 3.x specification.
 * Implements core datatypes.library functions for managing datatype objects.
 */

#include <exec/types.h>
#include <exec/memory.h>
#include <exec/lists.h>
#include <exec/execbase.h>
#include <exec/resident.h>
#include <exec/initializers.h>
#include <clib/exec_protos.h>
#include <inline/exec.h>
#include <inline/alib.h>

#include <dos/dos.h>
#include <dos/dosextens.h>
#include <clib/dos_protos.h>
#include <inline/dos.h>

#include <intuition/intuition.h>
#include <intuition/classes.h>
#include <intuition/classusr.h>
#include <intuition/gadgetclass.h>
#include <clib/intuition_protos.h>
#include <inline/intuition.h>

/* Declare BOOPSI alib functions manually to avoid conflicts */
static ULONG DoMethodA(Object *obj, Msg message);
static ULONG DoSuperMethodA(struct IClass *cl, Object *obj, Msg message);
#ifdef __GNUC__
static ULONG DoMethod(Object *obj, ULONG methodID, ...);
#endif

#include <datatypes/datatypes.h>
#include <datatypes/datatypesclass.h>
#include <clib/datatypes_protos.h>

#include <utility/tagitem.h>
#include <clib/utility_protos.h>
#include <inline/utility.h>

#include <libraries/iffparse.h>
#include <clib/iffparse_protos.h>
#include <inline/iffparse.h>

#include <graphics/gfx.h>

#include "util.h"

/* Standard library macros */
#ifndef offsetof
#define offsetof(type, member) ((ULONG)&(((type *)0)->member))
#endif

/* IFF Form IDs */
#ifndef ID_ILBM
#define ID_ILBM MAKE_ID('I','L','B','M')
#endif

/*
 * Minimal BOOPSI support functions (normally in amiga.lib)
 * These directly invoke the hook/dispatcher.
 */
static ULONG DoMethodA(Object *obj, Msg message)
{
    struct IClass *cl;
    ULONG (*dispatcher)(register struct IClass *cl __asm("a0"),
                       register Object *o __asm("a2"),
                       register Msg msg __asm("a1"));
    
    if (!obj || !message)
        return 0;
    
    /* Get the class from the object */
    cl = OCLASS(obj);
    if (!cl || !cl->cl_Dispatcher.h_Entry)
        return 0;
    
    /* Call the dispatcher directly */
    dispatcher = (ULONG (*)(register struct IClass *, register Object *, register Msg))cl->cl_Dispatcher.h_Entry;
    return dispatcher(cl, obj, message);
}

static ULONG DoSuperMethodA(struct IClass *cl, Object *obj, Msg message)
{
    ULONG (*dispatcher)(register struct IClass *cl __asm("a0"),
                       register Object *o __asm("a2"),
                       register Msg msg __asm("a1"));
    
    if (!cl || !obj || !message)
        return 0;
    
    /* Get the superclass */
    cl = cl->cl_Super;
    if (!cl || !cl->cl_Dispatcher.h_Entry)
        return 0;
    
    /* Call the superclass dispatcher directly */
    dispatcher = (ULONG (*)(register struct IClass *, register Object *, register Msg))cl->cl_Dispatcher.h_Entry;
    return dispatcher(cl, obj, message);
}

#ifdef __GNUC__
static ULONG DoMethod(Object *obj, ULONG methodID, ...)
{
    return DoMethodA(obj, (Msg)&methodID);
}
#endif

extern struct UtilityBase *UtilityBase;
extern struct IntuitionBase *IntuitionBase;
extern struct Library *IFFParseBase;

#define VERSION    40
#define REVISION   1
#define EXLIBNAME  "datatypes"
#define EXLIBVER   " 40.1 (2026/02/03)"

char __aligned _g_datatypes_ExLibName [] = EXLIBNAME ".library";
char __aligned _g_datatypes_ExLibID   [] = EXLIBNAME EXLIBVER;
char __aligned _g_datatypes_Copyright [] = "(C)opyright 2026 by G. Bartsch. Licensed under the MIT license.";

char __aligned _g_datatypes_VERSTRING [] = "\0$VER: " EXLIBNAME EXLIBVER;

extern struct ExecBase *SysBase;
extern struct DosLibrary *DOSBase;

/*
 * DataTypesBase structure
 */
struct DataTypesBase
{
    struct Library lib;
    BPTR           SegList;
    struct IClass *DatatypesClass;  /* Base datatypes BOOPSI class */
};

/*
 * Internal DataType structure with reference counting
 */
struct InternalDataType
{
    struct DataType dt;
    ULONG           RefCount;       /* Reference counter for ObtainDataType/ReleaseDataType */
};

/*
 * Forward declarations of library functions
 */
struct DataType * _datatypes_ObtainDataTypeA(register struct DataTypesBase *DataTypesBase __asm("a6"),
                                            register ULONG type __asm("d0"),
                                            register APTR handle __asm("a0"),
                                            register const struct TagItem *attrs __asm("a1"));
void _datatypes_ReleaseDataType(register struct DataTypesBase *DataTypesBase __asm("a6"),
                                register struct DataType *dt __asm("a0"));
Object * _datatypes_NewDTObjectA(register struct DataTypesBase *DataTypesBase __asm("a6"),
                                 register CONST_STRPTR name __asm("d0"),
                                 register const struct TagItem *attrs __asm("a0"));
void _datatypes_DisposeDTObject(register struct DataTypesBase *DataTypesBase __asm("a6"),
                                register Object *o __asm("a0"));
ULONG _datatypes_SetDTAttrsA(register struct DataTypesBase *DataTypesBase __asm("a6"),
                             register Object *o __asm("a0"),
                             register struct Window *win __asm("a1"),
                             register struct Requester *req __asm("a2"),
                             register const struct TagItem *attrs __asm("a3"));
ULONG _datatypes_GetDTAttrsA(register struct DataTypesBase *DataTypesBase __asm("a6"),
                             register Object *o __asm("a0"),
                             register const struct TagItem *attrs __asm("a2"));
LONG _datatypes_AddDTObject(register struct DataTypesBase *DataTypesBase __asm("a6"),
                            register struct Window *win __asm("a0"),
                            register struct Requester *req __asm("a1"),
                            register Object *o __asm("a2"),
                            register LONG pos __asm("d0"));
void _datatypes_RefreshDTObjectA(register struct DataTypesBase *DataTypesBase __asm("a6"),
                                 register Object *o __asm("a0"),
                                 register struct Window *win __asm("a1"),
                                 register struct Requester *req __asm("a2"),
                                 register const struct TagItem *attrs __asm("a3"));
ULONG _datatypes_DoAsyncLayout(register struct DataTypesBase *DataTypesBase __asm("a6"),
                               register Object *o __asm("a0"),
                               register struct gpLayout *gpl __asm("a1"));
ULONG _datatypes_DoDTMethodA(register struct DataTypesBase *DataTypesBase __asm("a6"),
                             register Object *o __asm("a0"),
                             register struct Window *win __asm("a1"),
                             register struct Requester *req __asm("a2"),
                             register Msg msg __asm("a3"));
LONG _datatypes_RemoveDTObject(register struct DataTypesBase *DataTypesBase __asm("a6"),
                               register struct Window *win __asm("a0"),
                               register Object *o __asm("a1"));
ULONG * _datatypes_GetDTMethods(register struct DataTypesBase *DataTypesBase __asm("a6"),
                                register const Object *object __asm("a0"));
struct DTMethods * _datatypes_GetDTTriggerMethods(register struct DataTypesBase *DataTypesBase __asm("a6"),
                                                  register Object *object __asm("a0"));
ULONG _datatypes_PrintDTObjectA(register struct DataTypesBase *DataTypesBase __asm("a6"),
                                register Object *o __asm("a0"),
                                register struct Window *w __asm("a1"),
                                register struct Requester *r __asm("a2"),
                                register struct dtPrint *msg __asm("a3"));
STRPTR _datatypes_GetDTString(register struct DataTypesBase *DataTypesBase __asm("a6"),
                              register ULONG id __asm("d0"));

/* Forward declarations for BOOPSI class dispatcher */
static ULONG DT_Dispatcher(register struct IClass *cl __asm("a0"),
                          register Object *o __asm("a2"),
                          register Msg msg __asm("a1"));

/*
 * Library init/open/close/expunge
 */
struct DataTypesBase * __g_lxa_datatypes_InitLib ( register struct DataTypesBase *dtbase __asm("a6"),
                                                   register BPTR                seglist __asm("a0"),
                                                   register struct ExecBase    *sysb __asm("d0"))
{
    DPRINTF (LOG_DEBUG, "_datatypes: InitLib() called\n");
    dtbase->SegList = seglist;
    
    /* Create the base datatypesclass BOOPSI class */
    dtbase->DatatypesClass = MakeClass((UBYTE *)DATATYPESCLASS, (UBYTE *)GADGETCLASS, NULL, sizeof(struct DTSpecialInfo), 0);
    if (dtbase->DatatypesClass)
    {
        dtbase->DatatypesClass->cl_Dispatcher.h_Entry = (ULONG (*)())DT_Dispatcher;
        DPRINTF(LOG_DEBUG, "_datatypes: Created datatypesclass at 0x%08lx\n", (ULONG)dtbase->DatatypesClass);
    }
    else
    {
        LPRINTF(LOG_ERROR, "_datatypes: Failed to create datatypesclass\n");
    }
    
    return dtbase;
}

BPTR __g_lxa_datatypes_ExpungeLib ( register struct DataTypesBase *dtbase __asm("a6") )
{
    /* Free the BOOPSI class */
    if (dtbase->DatatypesClass)
    {
        FreeClass(dtbase->DatatypesClass);
        dtbase->DatatypesClass = NULL;
    }
    return 0;
}

struct DataTypesBase * __g_lxa_datatypes_OpenLib ( register struct DataTypesBase *dtbase __asm("a6") )
{
    DPRINTF (LOG_DEBUG, "_datatypes: OpenLib() called, dtbase=0x%08lx\n", (ULONG)dtbase);
    dtbase->lib.lib_OpenCnt++;
    dtbase->lib.lib_Flags &= ~LIBF_DELEXP;
    return dtbase;
}

BPTR __g_lxa_datatypes_CloseLib ( register struct DataTypesBase *dtbase __asm("a6") )
{
    DPRINTF (LOG_DEBUG, "_datatypes: CloseLib() called, dtbase=0x%08lx\n", (ULONG)dtbase);
    dtbase->lib.lib_OpenCnt--;
    return 0;
}

ULONG __g_lxa_datatypes_ExtFuncLib ( void )
{
    return 0;
}

/*
 * BOOPSI Class Dispatcher for datatypesclass
 */
static ULONG DT_Dispatcher(register struct IClass *cl __asm("a0"),
                          register Object *o __asm("a2"),
                          register Msg msg __asm("a1"))
{
    ULONG retval = 0;
    
    switch (msg->MethodID)
    {
        case OM_NEW:
        {
            struct opSet *ops = (struct opSet *)msg;
            DPRINTF(LOG_DEBUG, "_datatypes: OM_NEW\n");
            
            /* Call superclass first */
            retval = DoSuperMethodA(cl, o, msg);
            if (retval)
            {
                Object *obj = (Object *)retval;
                struct DTSpecialInfo *si = (struct DTSpecialInfo *)INST_DATA(cl, obj);
                
                /* Initialize DTSpecialInfo */
                if (si)
                {
                    InitSemaphore(&si->si_Lock);
                    si->si_Flags = 0;
                    si->si_TopVert = 0;
                    si->si_VisVert = 0;
                    si->si_TotVert = 0;
                    si->si_OTopVert = 0;
                    si->si_VertUnit = 1;
                    si->si_TopHoriz = 0;
                    si->si_VisHoriz = 0;
                    si->si_TotHoriz = 0;
                    si->si_OTopHoriz = 0;
                    si->si_HorizUnit = 1;
                }
                
                /* Set initial attributes */
                if (ops->ops_AttrList)
                {
                    /* Would call OM_SET here, but for basic implementation we skip it */
                }
            }
            break;
        }
        
        case OM_DISPOSE:
            DPRINTF(LOG_DEBUG, "_datatypes: OM_DISPOSE\n");
            /* Clean up and call superclass */
            retval = DoSuperMethodA(cl, o, msg);
            break;
            
        case OM_SET:
        case OM_UPDATE:
        {
            struct opSet *ops = (struct opSet *)msg;
            struct DTSpecialInfo *si = (struct DTSpecialInfo *)INST_DATA(cl, o);
            struct TagItem *tag, *tstate = ops->ops_AttrList;
            ULONG refresh = 0;
            
            DPRINTF(LOG_DEBUG, "_datatypes: OM_SET/OM_UPDATE\n");
            
            /* Process datatypes-specific attributes */
            while ((tag = NextTagItem(&tstate)))
            {
                switch (tag->ti_Tag)
                {
                    case DTA_TopVert:
                        if (si) si->si_TopVert = tag->ti_Data;
                        refresh = 1;
                        break;
                    case DTA_VisibleVert:
                        if (si) si->si_VisVert = tag->ti_Data;
                        break;
                    case DTA_TotalVert:
                        if (si) si->si_TotVert = tag->ti_Data;
                        break;
                    case DTA_VertUnit:
                        if (si) si->si_VertUnit = tag->ti_Data;
                        break;
                    case DTA_TopHoriz:
                        if (si) si->si_TopHoriz = tag->ti_Data;
                        refresh = 1;
                        break;
                    case DTA_VisibleHoriz:
                        if (si) si->si_VisHoriz = tag->ti_Data;
                        break;
                    case DTA_TotalHoriz:
                        if (si) si->si_TotHoriz = tag->ti_Data;
                        break;
                    case DTA_HorizUnit:
                        if (si) si->si_HorizUnit = tag->ti_Data;
                        break;
                }
            }
            
            /* Call superclass */
            retval = DoSuperMethodA(cl, o, msg);
            
            /* Return refresh flag if needed */
            if (refresh && msg->MethodID == OM_UPDATE)
                retval = refresh;
            break;
        }
        
        case OM_GET:
        {
            struct opGet *opg = (struct opGet *)msg;
            struct DTSpecialInfo *si = (struct DTSpecialInfo *)INST_DATA(cl, o);
            
            DPRINTF(LOG_DEBUG, "_datatypes: OM_GET tag=0x%08lx\n", opg->opg_AttrID);
            
            switch (opg->opg_AttrID)
            {
                case DTA_TopVert:
                    if (si) *opg->opg_Storage = si->si_TopVert;
                    retval = 1;
                    break;
                case DTA_VisibleVert:
                    if (si) *opg->opg_Storage = si->si_VisVert;
                    retval = 1;
                    break;
                case DTA_TotalVert:
                    if (si) *opg->opg_Storage = si->si_TotVert;
                    retval = 1;
                    break;
                case DTA_VertUnit:
                    if (si) *opg->opg_Storage = si->si_VertUnit;
                    retval = 1;
                    break;
                case DTA_TopHoriz:
                    if (si) *opg->opg_Storage = si->si_TopHoriz;
                    retval = 1;
                    break;
                case DTA_VisibleHoriz:
                    if (si) *opg->opg_Storage = si->si_VisHoriz;
                    retval = 1;
                    break;
                case DTA_TotalHoriz:
                    if (si) *opg->opg_Storage = si->si_TotHoriz;
                    retval = 1;
                    break;
                case DTA_HorizUnit:
                    if (si) *opg->opg_Storage = si->si_HorizUnit;
                    retval = 1;
                    break;
                default:
                    /* Try superclass */
                    retval = DoSuperMethodA(cl, o, msg);
                    break;
            }
            break;
        }
        
        case GM_RENDER:
        {
            DPRINTF(LOG_DEBUG, "_datatypes: GM_RENDER\n");
            /* For now, just call superclass */
            retval = DoSuperMethodA(cl, o, msg);
            break;
        }
        
        case GM_LAYOUT:
        case DTM_PROCLAYOUT:
        {
            DPRINTF(LOG_DEBUG, "_datatypes: GM_LAYOUT/DTM_PROCLAYOUT\n");
            /* For basic implementation, just return success */
            retval = 1;
            break;
        }
        
        case DTM_FRAMEBOX:
        {
            DPRINTF(LOG_DEBUG, "_datatypes: DTM_FRAMEBOX\n");
            /* Return NULL for now - no special frame requirements */
            retval = 0;
            break;
        }
        
        default:
            /* Pass unknown methods to superclass */
            retval = DoSuperMethodA(cl, o, msg);
            break;
    }
    
    return retval;
}

/*
 * ObtainDataTypeA - Obtain a DataType descriptor
 */
struct DataType * _datatypes_ObtainDataTypeA(register struct DataTypesBase *DataTypesBase __asm("a6"),
                                            register ULONG type __asm("d0"),
                                            register APTR handle __asm("a0"),
                                            register const struct TagItem *attrs __asm("a1"))
{
    struct InternalDataType *idt;
    struct DataType *dt;
    
    DPRINTF(LOG_DEBUG, "_datatypes: ObtainDataTypeA(type=%ld, handle=0x%08lx)\n", type, (ULONG)handle);
    
    /* For basic implementation, create a minimal DataType structure */
    idt = (struct InternalDataType *)AllocVec(sizeof(struct InternalDataType), MEMF_PUBLIC | MEMF_CLEAR);
    if (!idt)
    {
        LPRINTF(LOG_ERROR, "_datatypes: Failed to allocate InternalDataType\n");
        return NULL;
    }
    
    dt = &idt->dt;
    idt->RefCount = 1;
    
    /* Allocate and initialize DataTypeHeader */
    dt->dtn_Header = (struct DataTypeHeader *)AllocVec(sizeof(struct DataTypeHeader), MEMF_PUBLIC | MEMF_CLEAR);
    if (!dt->dtn_Header)
    {
        LPRINTF(LOG_ERROR, "_datatypes: Failed to allocate DataTypeHeader\n");
        FreeVec(idt);
        return NULL;
    }
    
    /* Initialize basic fields based on type */
    switch (type)
    {
        case DTST_FILE:
            /* File-based datatype - would analyze file here */
            dt->dtn_Header->dth_GroupID = GID_PICTURE;  /* Default to picture for now */
            dt->dtn_Header->dth_ID = ID_ILBM;           /* Default to ILBM */
            dt->dtn_Header->dth_Name = (STRPTR)"Picture";
            dt->dtn_Header->dth_BaseName = (STRPTR)"picture";
            break;
        case DTST_RAM:
        case DTST_MEMORY:
            /* Memory-based datatype */
            dt->dtn_Header->dth_GroupID = GID_PICTURE;
            dt->dtn_Header->dth_ID = 0;
            dt->dtn_Header->dth_Name = (STRPTR)"Picture";
            dt->dtn_Header->dth_BaseName = (STRPTR)"picture";
            break;
        default:
            LPRINTF(LOG_ERROR, "_datatypes: Unknown type %ld\n", type);
            FreeVec(dt->dtn_Header);
            FreeVec(idt);
            return NULL;
    }
    
    /* Initialize tool list */
    NewList(&dt->dtn_ToolList);
    
    DPRINTF(LOG_DEBUG, "_datatypes: ObtainDataTypeA() returning dt=0x%08lx\n", (ULONG)dt);
    return dt;
}

/*
 * ReleaseDataType - Release a DataType descriptor
 */
void _datatypes_ReleaseDataType(register struct DataTypesBase *DataTypesBase __asm("a6"),
                                register struct DataType *dt __asm("a0"))
{
    struct InternalDataType *idt;
    
    DPRINTF(LOG_DEBUG, "_datatypes: ReleaseDataType(dt=0x%08lx)\n", (ULONG)dt);
    
    if (!dt)
        return;
    
    /* Get internal structure */
    idt = (struct InternalDataType *)((UBYTE *)dt - offsetof(struct InternalDataType, dt));
    
    /* Decrement reference count */
    if (idt->RefCount > 0)
    {
        idt->RefCount--;
        DPRINTF(LOG_DEBUG, "_datatypes: RefCount now %ld\n", idt->RefCount);
    }
    
    /* Free if reference count reaches zero */
    if (idt->RefCount == 0)
    {
        DPRINTF(LOG_DEBUG, "_datatypes: Freeing DataType\n");
        
        if (dt->dtn_Header)
            FreeVec(dt->dtn_Header);
        if (dt->dtn_AttrList)
            FreeVec(dt->dtn_AttrList);
            
        FreeVec(idt);
    }
}

/*
 * NewDTObjectA - Create a new datatype object
 */
Object * _datatypes_NewDTObjectA(register struct DataTypesBase *DataTypesBase __asm("a6"),
                                 register CONST_STRPTR name __asm("d0"),
                                 register const struct TagItem *attrs __asm("a0"))
{
    Object *obj;
    struct DataType *dt = NULL;
    ULONG sourceType = DTST_FILE;
    APTR handle = NULL;
    struct TagItem *tag;
    struct TagItem *tstate = (struct TagItem *)attrs;
    
    DPRINTF(LOG_DEBUG, "_datatypes: NewDTObjectA(name=%s)\n", STRORNULL(name));
    
    /* Parse tags to get source type and handle */
    while ((tag = NextTagItem(&tstate)))
    {
        switch (tag->ti_Tag)
        {
            case DTA_SourceType:
                sourceType = tag->ti_Data;
                break;
            case DTA_Handle:
                handle = (APTR)tag->ti_Data;
                break;
        }
    }
    
    /* Obtain datatype descriptor */
    dt = _datatypes_ObtainDataTypeA(DataTypesBase, sourceType, handle, attrs);
    if (!dt)
    {
        LPRINTF(LOG_ERROR, "_datatypes: Failed to obtain DataType\n");
        return NULL;
    }
    
    /* Create BOOPSI object using datatypesclass */
    obj = NewObject(DataTypesBase->DatatypesClass, NULL,
                   DTA_DataType, (ULONG)dt,
                   DTA_Name, (ULONG)name,
                   TAG_MORE, (ULONG)attrs,
                   TAG_END);
    
    if (!obj)
    {
        LPRINTF(LOG_ERROR, "_datatypes: Failed to create BOOPSI object\n");
        _datatypes_ReleaseDataType(DataTypesBase, dt);
        return NULL;
    }
    
    DPRINTF(LOG_DEBUG, "_datatypes: NewDTObjectA() returning obj=0x%08lx\n", (ULONG)obj);
    return obj;
}

/*
 * DisposeDTObject - Dispose of a datatype object
 */
void _datatypes_DisposeDTObject(register struct DataTypesBase *DataTypesBase __asm("a6"),
                                register Object *o __asm("a0"))
{
    struct DataType *dt = NULL;
    
    DPRINTF(LOG_DEBUG, "_datatypes: DisposeDTObject(o=0x%08lx)\n", (ULONG)o);
    
    if (!o)
        return;
    
    /* Get the DataType from the object */
    GetAttr(DTA_DataType, o, (ULONG *)&dt);
    
    /* Dispose of the BOOPSI object */
    DisposeObject(o);
    
    /* Release the DataType descriptor */
    if (dt)
        _datatypes_ReleaseDataType(DataTypesBase, dt);
}

/*
 * SetDTAttrsA - Set datatype object attributes
 */
ULONG _datatypes_SetDTAttrsA(register struct DataTypesBase *DataTypesBase __asm("a6"),
                             register Object *o __asm("a0"),
                             register struct Window *win __asm("a1"),
                             register struct Requester *req __asm("a2"),
                             register const struct TagItem *attrs __asm("a3"))
{
    struct opSet ops;
    struct opUpdate opu;
    
    DPRINTF(LOG_DEBUG, "_datatypes: SetDTAttrsA(o=0x%08lx)\n", (ULONG)o);
    
    if (!o || !attrs)
        return 0;
    
    /* Use OM_UPDATE if window is provided, otherwise OM_SET */
    if (win)
    {
        opu.MethodID = OM_UPDATE;
        opu.opu_AttrList = (struct TagItem *)attrs;
        opu.opu_GInfo = NULL;
        opu.opu_Flags = 0;
        
        return DoMethodA(o, (Msg)&opu);
    }
    else
    {
        ops.MethodID = OM_SET;
        ops.ops_AttrList = (struct TagItem *)attrs;
        ops.ops_GInfo = NULL;
        
        return DoMethodA(o, (Msg)&ops);
    }
}

/*
 * GetDTAttrsA - Get datatype object attributes
 */
ULONG _datatypes_GetDTAttrsA(register struct DataTypesBase *DataTypesBase __asm("a6"),
                             register Object *o __asm("a0"),
                             register const struct TagItem *attrs __asm("a2"))
{
    struct TagItem *tag;
    struct TagItem *tstate = (struct TagItem *)attrs;
    ULONG count = 0;
    
    DPRINTF(LOG_DEBUG, "_datatypes: GetDTAttrsA(o=0x%08lx)\n", (ULONG)o);
    
    if (!o || !attrs)
        return 0;
    
    /* Get each attribute */
    while ((tag = NextTagItem(&tstate)))
    {
        if (GetAttr(tag->ti_Tag, o, (ULONG *)tag->ti_Data))
            count++;
    }
    
    return count;
}

/*
 * AddDTObject - Add datatype object to window
 */
LONG _datatypes_AddDTObject(register struct DataTypesBase *DataTypesBase __asm("a6"),
                            register struct Window *win __asm("a0"),
                            register struct Requester *req __asm("a1"),
                            register Object *o __asm("a2"),
                            register LONG pos __asm("d0"))
{
    struct Gadget *gad;
    
    DPRINTF(LOG_DEBUG, "_datatypes: AddDTObject(win=0x%08lx, o=0x%08lx, pos=%ld)\n",
            (ULONG)win, (ULONG)o, pos);
    
    if (!win || !o)
        return -1;
    
    /* Get the gadget from the object */
    GetAttr(GA_ID, o, (ULONG *)&gad);
    if (!gad)
        gad = (struct Gadget *)o;  /* Object IS the gadget */
    
    /* Add gadget to window */
    return AddGadget(win, gad, pos);
}

/*
 * RefreshDTObjectA - Refresh a datatype object
 */
void _datatypes_RefreshDTObjectA(register struct DataTypesBase *DataTypesBase __asm("a6"),
                                 register Object *o __asm("a0"),
                                 register struct Window *win __asm("a1"),
                                 register struct Requester *req __asm("a2"),
                                 register const struct TagItem *attrs __asm("a3"))
{
    struct gpRender gpr;
    
    DPRINTF(LOG_DEBUG, "_datatypes: RefreshDTObjectA(o=0x%08lx, win=0x%08lx)\n",
            (ULONG)o, (ULONG)win);
    
    if (!o)
        return;
    
    /* Send GM_RENDER to the object */
    gpr.MethodID = GM_RENDER;
    gpr.gpr_GInfo = NULL;
    gpr.gpr_RPort = win ? win->RPort : NULL;
    gpr.gpr_Redraw = GREDRAW_REDRAW;
    
    DoMethodA(o, (Msg)&gpr);
}

/*
 * DoAsyncLayout - Perform asynchronous layout
 */
ULONG _datatypes_DoAsyncLayout(register struct DataTypesBase *DataTypesBase __asm("a6"),
                               register Object *o __asm("a0"),
                               register struct gpLayout *gpl __asm("a1"))
{
    DPRINTF(LOG_DEBUG, "_datatypes: DoAsyncLayout(o=0x%08lx)\n", (ULONG)o);
    
    if (!o)
        return 0;
    
    /* For basic implementation, just send DTM_PROCLAYOUT */
    return DoMethod(o, DTM_PROCLAYOUT, gpl);
}

/*
 * DoDTMethodA - Perform a datatype method
 */
ULONG _datatypes_DoDTMethodA(register struct DataTypesBase *DataTypesBase __asm("a6"),
                             register Object *o __asm("a0"),
                             register struct Window *win __asm("a1"),
                             register struct Requester *req __asm("a2"),
                             register Msg msg __asm("a3"))
{
    DPRINTF(LOG_DEBUG, "_datatypes: DoDTMethodA(o=0x%08lx, msg->MethodID=0x%08lx)\n",
            (ULONG)o, msg ? msg->MethodID : 0);
    
    if (!o || !msg)
        return 0;
    
    return DoMethodA(o, msg);
}

/*
 * RemoveDTObject - Remove datatype object from window
 */
LONG _datatypes_RemoveDTObject(register struct DataTypesBase *DataTypesBase __asm("a6"),
                               register struct Window *win __asm("a0"),
                               register Object *o __asm("a1"))
{
    struct Gadget *gad;
    
    DPRINTF(LOG_DEBUG, "_datatypes: RemoveDTObject(win=0x%08lx, o=0x%08lx)\n",
            (ULONG)win, (ULONG)o);
    
    if (!win || !o)
        return -1;
    
    /* Get the gadget from the object */
    GetAttr(GA_ID, o, (ULONG *)&gad);
    if (!gad)
        gad = (struct Gadget *)o;  /* Object IS the gadget */
    
    /* Remove gadget from window */
    return RemoveGadget(win, gad);
}

/*
 * GetDTMethods - Get supported methods
 */
ULONG * _datatypes_GetDTMethods(register struct DataTypesBase *DataTypesBase __asm("a6"),
                                register const Object *object __asm("a0"))
{
    ULONG *methods;
    
    DPRINTF(LOG_DEBUG, "_datatypes: GetDTMethods(object=0x%08lx)\n", (ULONG)object);
    
    if (!object)
        return NULL;
    
    /* Get DTA_Methods attribute */
    GetAttr(DTA_Methods, (Object *)object, (ULONG *)&methods);
    
    return methods;
}

/*
 * GetDTTriggerMethods - Get supported trigger methods
 */
struct DTMethods * _datatypes_GetDTTriggerMethods(register struct DataTypesBase *DataTypesBase __asm("a6"),
                                                  register Object *object __asm("a0"))
{
    struct DTMethods *methods;
    
    DPRINTF(LOG_DEBUG, "_datatypes: GetDTTriggerMethods(object=0x%08lx)\n", (ULONG)object);
    
    if (!object)
        return NULL;
    
    /* Get DTA_TriggerMethods attribute */
    GetAttr(DTA_TriggerMethods, object, (ULONG *)&methods);
    
    return methods;
}

/*
 * PrintDTObjectA - Print a datatype object
 */
ULONG _datatypes_PrintDTObjectA(register struct DataTypesBase *DataTypesBase __asm("a6"),
                                register Object *o __asm("a0"),
                                register struct Window *w __asm("a1"),
                                register struct Requester *r __asm("a2"),
                                register struct dtPrint *msg __asm("a3"))
{
    DPRINTF(LOG_DEBUG, "_datatypes: PrintDTObjectA(o=0x%08lx)\n", (ULONG)o);
    
    /* For basic implementation, return error */
    return 0;
}

/*
 * GetDTString - Get localized string
 */
STRPTR _datatypes_GetDTString(register struct DataTypesBase *DataTypesBase __asm("a6"),
                              register ULONG id __asm("d0"))
{
    DPRINTF(LOG_DEBUG, "_datatypes: GetDTString(id=%ld)\n", id);
    
    /* Return basic error strings */
    switch (id)
    {
        case DTERROR_UNKNOWN_DATATYPE:
            return (STRPTR)"Unknown data type";
        case DTERROR_COULDNT_SAVE:
            return (STRPTR)"Couldn't save data";
        case DTERROR_COULDNT_OPEN:
            return (STRPTR)"Couldn't open file";
        case DTERROR_COULDNT_SEND_MESSAGE:
            return (STRPTR)"Couldn't send message";
        case DTERROR_COULDNT_OPEN_CLIPBOARD:
            return (STRPTR)"Couldn't open clipboard";
        case DTERROR_UNKNOWN_COMPRESSION:
            return (STRPTR)"Unknown compression";
        case DTERROR_NOT_ENOUGH_DATA:
            return (STRPTR)"Not enough data";
        case DTERROR_INVALID_DATA:
            return (STRPTR)"Invalid data";
        case DTERROR_NOT_AVAILABLE:
            return (STRPTR)"Not available";
        default:
            return (STRPTR)"Unknown error";
    }
}

/*
 * Data initialization structure for library
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

extern APTR              __g_lxa_datatypes_FuncTab [];
extern struct MyDataInit __g_lxa_datatypes_DataTab;
extern struct InitTable  __g_lxa_datatypes_InitTab;
extern APTR              __g_lxa_datatypes_EndResident;

static struct Resident __aligned ROMTag =
{
    RTC_MATCHWORD,                       // UWORD rt_MatchWord
    &ROMTag,                             // struct Resident *rt_MatchTag
    &__g_lxa_datatypes_EndResident,      // APTR  rt_EndSkip
    RTF_AUTOINIT,                        // UBYTE rt_Flags
    VERSION,                             // UBYTE rt_Version
    NT_LIBRARY,                          // UBYTE rt_Type
    0,                                   // BYTE  rt_Pri
    &_g_datatypes_ExLibName[0],          // char  *rt_Name
    &_g_datatypes_ExLibID[0],            // char  *rt_IdString
    &__g_lxa_datatypes_InitTab           // APTR  rt_Init
};

APTR __g_lxa_datatypes_EndResident;
struct Resident *__lxa_datatypes_ROMTag = &ROMTag;

struct InitTable __g_lxa_datatypes_InitTab =
{
    (ULONG)               sizeof(struct DataTypesBase),
    (APTR              *) &__g_lxa_datatypes_FuncTab[0],
    (APTR)                &__g_lxa_datatypes_DataTab,
    (APTR)                __g_lxa_datatypes_InitLib
};

/*
 * Function table
 */
APTR __g_lxa_datatypes_FuncTab [] =
{
    __g_lxa_datatypes_OpenLib,           // -6   Standard
    __g_lxa_datatypes_CloseLib,          // -12  Standard
    __g_lxa_datatypes_ExpungeLib,        // -18  Standard
    __g_lxa_datatypes_ExtFuncLib,        // -24  Standard (reserved)
    _datatypes_ObtainDataTypeA,          // -30  Stub (private) - datatypesPrivate1
    _datatypes_ObtainDataTypeA,          // -36  ObtainDataTypeA
    _datatypes_ReleaseDataType,          // -42  ReleaseDataType
    _datatypes_NewDTObjectA,             // -48  NewDTObjectA
    _datatypes_DisposeDTObject,          // -54  DisposeDTObject
    _datatypes_SetDTAttrsA,              // -60  SetDTAttrsA
    _datatypes_GetDTAttrsA,              // -66  GetDTAttrsA
    _datatypes_AddDTObject,              // -72  AddDTObject
    _datatypes_RefreshDTObjectA,         // -78  RefreshDTObjectA
    _datatypes_DoAsyncLayout,            // -84  DoAsyncLayout
    _datatypes_DoDTMethodA,              // -90  DoDTMethodA
    _datatypes_RemoveDTObject,           // -96  RemoveDTObject
    _datatypes_GetDTMethods,             // -102 GetDTMethods
    _datatypes_GetDTTriggerMethods,      // -108 GetDTTriggerMethods
    _datatypes_PrintDTObjectA,           // -114 PrintDTObjectA
    _datatypes_ObtainDataTypeA,          // -120 Stub (private) - datatypesPrivate2
    _datatypes_ObtainDataTypeA,          // -126 Stub (private) - datatypesPrivate3
    _datatypes_ObtainDataTypeA,          // -132 Stub (private) - datatypesPrivate4
    _datatypes_GetDTString,              // -138 GetDTString
    (APTR) -1
};

/*
 * Data table
 */
struct MyDataInit __g_lxa_datatypes_DataTab =
{
    /* ln_Type      */ INITBYTE(OFFSET(Node,         ln_Type),      NT_LIBRARY),
    /* ln_Name      */ 0x80, (UBYTE) (ULONG) OFFSET(Node,    ln_Name), (ULONG) &_g_datatypes_ExLibName[0],
    /* lib_Flags    */ INITBYTE(OFFSET(Library,      lib_Flags),    LIBF_SUMUSED|LIBF_CHANGED),
    /* lib_Version  */ INITWORD(OFFSET(Library,      lib_Version),  VERSION),
    /* lib_Revision */ INITWORD(OFFSET(Library,      lib_Revision), REVISION),
    /* lib_IdString */ 0x80, (UBYTE) (ULONG) OFFSET(Library, lib_IdString), (ULONG) &_g_datatypes_ExLibID[0],
    (ULONG) 0
};
