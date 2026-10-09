/*
 * lxa - library frame of the BOOPSI gadget class libraries
 * (colorwheel.gadget, gradientslider.gadget, tapedeck.gadget).
 *
 * (C) 2026 by G. Bartsch. Licensed under the MIT License.
 *
 * A class library opens the libraries its class needs, creates its public
 * class with MakeClass() and AddClass()es it at InitLib time, so any
 * program can NewObject(NULL, "<name>", ...) once the library is open.
 * Expunge removes and frees the class again - unless objects of it still
 * exist, in which case the library stays (delayed expunge).
 */
#include <exec/resident.h>
#include <exec/initializers.h>

#include "classlib.h"

struct ExecBase *SysBase;
struct Library  *IntuitionBase;
struct GfxBase  *GfxBase;
struct Library  *UtilityBase;
struct Library  *LayersBase;

static void classlib_CloseLibs(void)
{
    if (LayersBase)    CloseLibrary(LayersBase);
    if (UtilityBase)   CloseLibrary(UtilityBase);
    if (GfxBase)       CloseLibrary((struct Library *)GfxBase);
    if (IntuitionBase) CloseLibrary(IntuitionBase);
    LayersBase = UtilityBase = IntuitionBase = NULL;
    GfxBase = NULL;
}

static void classlib_FreeBase(struct ClassLibBase *base)
{
    ULONG neg = base->cb_Lib.lib_NegSize;
    FreeMem((UBYTE *)base - neg, neg + base->cb_Lib.lib_PosSize);
}

/* MakeClass() + AddClass(); FALSE if that failed */
static BOOL classlib_CreateClass(struct ClassLibBase *base)
{
    Class *cl = MakeClass((ClassID)classlib_LibName, (ClassID)classlib_SuperClass, NULL,
                          classlib_InstSize, 0);
    if (!cl)
        return FALSE;
    cl->cl_Dispatcher.h_Entry    = (ULONG (*)())classlib_Dispatcher;
    cl->cl_Dispatcher.h_SubEntry = NULL;
    cl->cl_UserData              = (ULONG)base;
    AddClass(cl);
    base->cb_Class = cl;
    return TRUE;
}

/* RemoveClass() + FreeClass(); FALSE (class stays public) while objects exist */
static BOOL classlib_DestroyClass(struct ClassLibBase *base)
{
    Class *cl = base->cb_Class;
    if (!cl)
        return TRUE;
    RemoveClass(cl);
    if (!FreeClass(cl))
    {
        AddClass(cl);
        return FALSE;
    }
    base->cb_Class = NULL;
    return TRUE;
}

struct Library *classlib_Init(register struct ClassLibBase *base __asm("d0"),
                              register BPTR seglist __asm("a0"),
                              register struct ExecBase *sysb __asm("a6"))
{
    SysBase = sysb;
    base->cb_SegList = seglist;
    base->cb_Class   = NULL;

    IntuitionBase = OpenLibrary((CONST_STRPTR)"intuition.library", 39);
    GfxBase       = (struct GfxBase *)OpenLibrary((CONST_STRPTR)"graphics.library", 39);
    UtilityBase   = OpenLibrary((CONST_STRPTR)"utility.library", 39);
    LayersBase    = OpenLibrary((CONST_STRPTR)"layers.library", 39);

    if (!IntuitionBase || !GfxBase || !UtilityBase || !LayersBase || !classlib_CreateClass(base))
    {
        classlib_CloseLibs();
        classlib_FreeBase(base);
        return NULL;
    }
    return &base->cb_Lib;
}

struct Library *classlib_Open(register struct ClassLibBase *base __asm("a6"))
{
    base->cb_Lib.lib_OpenCnt++;
    base->cb_Lib.lib_Flags &= ~LIBF_DELEXP;
    return &base->cb_Lib;
}

BPTR classlib_Expunge(register struct ClassLibBase *base __asm("a6"))
{
    BPTR seglist;

    if (base->cb_Lib.lib_OpenCnt || !classlib_DestroyClass(base))
    {
        base->cb_Lib.lib_Flags |= LIBF_DELEXP;
        return 0;
    }
    Remove(&base->cb_Lib.lib_Node);
    classlib_CloseLibs();
    seglist = base->cb_SegList;
    classlib_FreeBase(base);
    return seglist;
}

BPTR classlib_Close(register struct ClassLibBase *base __asm("a6"))
{
    if (base->cb_Lib.lib_OpenCnt)
        base->cb_Lib.lib_OpenCnt--;
    if (!base->cb_Lib.lib_OpenCnt && (base->cb_Lib.lib_Flags & LIBF_DELEXP))
        return classlib_Expunge(base);
    return 0;
}

ULONG classlib_ExtFunc(void)
{
    return 0;
}

/* RomTag */

struct ClassLibDataInit
{
    UWORD ln_Type_Init     ; UWORD ln_Type_Offset     ; UWORD ln_Type_Content     ;
    UBYTE ln_Name_Init     ; UBYTE ln_Name_Offset     ; ULONG ln_Name_Content     ;
    UWORD lib_Flags_Init   ; UWORD lib_Flags_Offset   ; UWORD lib_Flags_Content   ;
    UBYTE lib_IdString_Init; UBYTE lib_IdString_Offset; ULONG lib_IdString_Content;
    ULONG ENDMARK;
};

static struct ClassLibDataInit classlib_DataTab =
{
    INITBYTE(OFFSET(Node, ln_Type), NT_LIBRARY),
    0x80, (UBYTE)(ULONG)OFFSET(Node, ln_Name), (ULONG)&classlib_LibName[0],
    INITBYTE(OFFSET(Library, lib_Flags), LIBF_SUMUSED | LIBF_CHANGED),
    0x80, (UBYTE)(ULONG)OFFSET(Library, lib_IdString), (ULONG)&classlib_LibID[0],
    0
};

/* lib_Version/lib_Revision come from the class file: set them in Init */
static struct Library *classlib_InitVersion(register struct ClassLibBase *base __asm("d0"),
                                            register BPTR seglist __asm("a0"),
                                            register struct ExecBase *sysb __asm("a6"))
{
    base->cb_Lib.lib_Version  = classlib_Version;
    base->cb_Lib.lib_Revision = classlib_Revision;
    return classlib_Init(base, seglist, sysb);
}

static struct InitTable classlib_InitTab =
{
    sizeof(struct ClassLibBase),
    (APTR *)&classlib_FuncTab[0],
    (APTR *)&classlib_DataTab,
    (APTR)classlib_InitVersion
};

extern APTR classlib_EndResident;

struct Resident __aligned classlib_RomTag =
{
    RTC_MATCHWORD,
    &classlib_RomTag,
    &classlib_EndResident,
    RTF_AUTOINIT,
    40,
    NT_LIBRARY,
    0,
    (char *)classlib_LibName,
    (char *)classlib_LibID,
    &classlib_InitTab
};

APTR classlib_EndResident;
