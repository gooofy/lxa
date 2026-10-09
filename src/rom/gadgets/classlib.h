/*
 * lxa - shared code of the BOOPSI gadget class libraries
 * (colorwheel.gadget, gradientslider.gadget, tapedeck.gadget).
 *
 * (C) 2026 by G. Bartsch. Licensed under the MIT License.
 *
 * classlib.c is lxa's own library frame (RomTag, Open/Close/Expunge,
 * MakeClass()/AddClass() of the public class); each class file provides
 * the strings, the function table and the class dispatcher below.
 */
#ifndef LXA_GADGETS_CLASSLIB_H
#define LXA_GADGETS_CLASSLIB_H

#include <exec/types.h>
#include <exec/memory.h>
#include <exec/execbase.h>
#include <exec/libraries.h>
#include <intuition/intuition.h>
#include <intuition/classes.h>
#include <intuition/classusr.h>
#include <intuition/gadgetclass.h>
#include <intuition/imageclass.h>
#include <intuition/cghooks.h>
#include <utility/tagitem.h>

#include <clib/exec_protos.h>
#include <clib/intuition_protos.h>
#include <clib/graphics_protos.h>
#include <clib/utility_protos.h>
#include <clib/layers_protos.h>
#include <inline/exec.h>
#include <inline/intuition.h>
#include <inline/graphics.h>
#include <inline/utility.h>
#include <inline/layers.h>

#include "../util.h"

/* the library base of a class library */
struct ClassLibBase
{
    struct Library  cb_Lib;
    BPTR            cb_SegList;
    Class          *cb_Class;
};

extern struct ExecBase *SysBase;
extern struct Library  *IntuitionBase;
extern struct GfxBase  *GfxBase;
extern struct Library  *UtilityBase;
extern struct Library  *LayersBase;

/* provided by each class library */
extern const char  classlib_LibName[];      /* "colorwheel.gadget" (= class ID) */
extern const char  classlib_LibID[];        /* "colorwheel 40.x (date)\r\n" */
extern const UWORD classlib_Version;
extern const UWORD classlib_Revision;
extern APTR        classlib_FuncTab[];      /* Open, Close, Expunge, Reserved, ..., -1 */
extern const char  classlib_SuperClass[];   /* "gadgetclass" */
extern const UWORD classlib_InstSize;
ULONG classlib_Dispatcher(register Class *cl __asm("a0"), register Object *o __asm("a2"),
                          register Msg msg __asm("a1"));

/* the four standard library vectors (classlib.c) */
struct Library *classlib_Open(register struct ClassLibBase *base __asm("a6"));
BPTR classlib_Close(register struct ClassLibBase *base __asm("a6"));
BPTR classlib_Expunge(register struct ClassLibBase *base __asm("a6"));
ULONG classlib_ExtFunc(void);

/* BOOPSI helpers (amiga.lib equivalents) */
typedef ULONG (*classlib_DispFn)(register Class *cl __asm("a0"), register Object *o __asm("a2"),
                                 register Msg msg __asm("a1"));

static inline ULONG CL_CoerceMethodA(Class *cl, Object *o, Msg msg)
{
    return ((classlib_DispFn)cl->cl_Dispatcher.h_Entry)(cl, o, msg);
}

static inline ULONG CL_DoSuperMethodA(Class *cl, Object *o, Msg msg)
{
    return CL_CoerceMethodA(cl->cl_Super, o, msg);
}

static inline ULONG CL_DoMethodA(Object *o, Msg msg)
{
    return CL_CoerceMethodA(OCLASS(o), o, msg);
}

#endif
