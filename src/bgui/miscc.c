#include "include/classdefs.h"
#include <utility/hooks.h>
#include <intuition/classes.h>
#include <intuition/cghooks.h>
#include <proto/alib.h>
#include <proto/exec.h>
#include <proto/utility.h>
#include <proto/graphics.h>
#include <proto/intuition.h>
#include <libraries/locale.h>

#ifndef BGUI_COMPILERSPECIFIC_H
#include <bgui/bgui_compilerspecific.h>
#endif

BOOL DisplayAGuideInfo(struct NewAmigaGuide *nag, Tag tag, ...)
{
  AROS_SLOWSTACKTAGS_PRE_AS(tag, BOOL)
  struct Library * AmigaGuideBase;
  AmigaGuideBase = OpenLibrary("amigaguide.library",0);
 
  retval = FALSE;
  if (NULL != AmigaGuideBase)
  {
    AMIGAGUIDECONTEXT handle = OpenAmigaGuideA(nag, AROS_SLOWSTACKTAGS_ARG(tag));
    if (0 != handle)
    {
      retval = TRUE;
      CloseAmigaGuide(handle);
    }
    CloseLibrary(AmigaGuideBase);
  }
  AROS_SLOWSTACKTAGS_POST
}

ASM ULONG ScaleWeight(ULONG e __asm("d2"), ULONG f __asm("d3"), ULONG a __asm("d4"))
{
  ULONG r = UMult32(a,e);
  return UDivMod32((f >> 1) + r, f);
}

//void MyPutChProc_StrLenfA(BYTE c __asm("d0"), ULONG * putChData __asm("a3"))
REGFUNC2(void, MyPutChProc_StrLenfA,
        REGPARAM(D0, BYTE, c),
        REGPARAM(A3, ULONG *, putChData))
{
  (*putChData)++;
}
REGFUNC_END

ASM ULONG StrLenfA(UBYTE * FormatString __asm("a0"), RAWARG DataStream __asm("a1"))
{
  ULONG c = 0;
  RawDoFmt(FormatString, DataStream, ((APTR)MyPutChProc_StrLenfA), &c);
  return c;
}

//void MyPutChProc_SPrintfA(char c __asm("d0"), char ** PutChData __asm("a3"))
REGFUNC2(void, MyPutChProc_SPrintfA,
        REGPARAM(D0, char, c),
        REGPARAM(A3, char **, PutChData))
{
  **PutChData = c;
  (*PutChData)++;
}
REGFUNC_END

ASM VOID SPrintfA(UBYTE * buffer __asm("a3"), UBYTE * format __asm("a0"), RAWARG args __asm("a1"))
{
  RawDoFmt(format, args, ((APTR)MyPutChProc_SPrintfA), &buffer);
}


ASM REGFUNC3(VOID, LHook_Count,
        REGPARAM(A0, struct Hook *, hook),
        REGPARAM(A2, struct Locale *, loc),
        REGPARAM(A1, ULONG, chr))
{
  hook->h_Data++;
}
REGFUNC_END

ASM REGFUNC3(VOID, LHook_Format,
        REGPARAM(A0, struct Hook *, hook),
        REGPARAM(A2, struct Locale *, loc),
        REGPARAM(A1, ULONG, chr))
{
  char * cptr = (char *)hook->h_Data;
  *(cptr++) = (char)chr;
  hook->h_Data = cptr;
}
REGFUNC_END


ASM struct RastPort *BGUI_ObtainGIRPort(struct GadgetInfo * gi __asm("a0"))
{
  struct RastPort * rp;
  BYTE * userdata = NULL;
  
  if (NULL != gi)
  {
    if (NULL != gi->gi_Window)
      userdata = gi->gi_Window->UserData;

    if (NULL != gi->gi_Layer)
    {
      /* Does this make sense?? */
      LockLayerRom(gi->gi_Layer); // this function returns void!!
      UnlockLayerRom(gi->gi_Layer);
    }
  }
  rp = ObtainGIRPort(gi);
  if (NULL != rp)
    rp->RP_User = (APTR)userdata;
    
  return rp;
}

IPTR AsmDoMethod(Object * obj, STACKULONG MethodID, ...)
{
     AROS_SLOWSTACKMETHODS_PRE(MethodID)
     if (!obj)
       retval = 0L;
     else
       retval = CallHookPkt ((struct Hook *)OCLASS(obj)
                      , obj
                      , AROS_SLOWSTACKMETHODS_ARG(MethodID)
                );
     AROS_SLOWSTACKMETHODS_POST
}

IPTR AsmDoMethodA(Object * obj __asm("a2"), Msg message __asm("a1"))
{
  return DoMethodA(obj, message);
}

IPTR AsmDoSuperMethod( Class * cl, Object * obj, STACKULONG MethodID, ...)
{
   AROS_SLOWSTACKMETHODS_PRE(MethodID)
   if ((!obj) || (!cl))
       retval = 0L;
   else
       retval = CallHookPkt ((struct Hook *)cl->cl_Super
                       , obj
                       , AROS_SLOWSTACKMETHODS_ARG(MethodID)
                );
   AROS_SLOWSTACKMETHODS_POST
}

IPTR AsmDoSuperMethodA( Class * cl __asm("a0"), Object * obj __asm("a2"), Msg message __asm("a1"))
{
  return DoSuperMethodA(cl,obj,message);
}

IPTR AsmCoerceMethod( Class * cl, Object * obj, STACKULONG MethodID, ...)
{
    AROS_SLOWSTACKMETHODS_PRE(MethodID)
    if ((!obj) || (!cl))
      retval = 0L;
    else
      retval = CallHookPkt ((struct Hook *)cl
                      , obj
                      , AROS_SLOWSTACKMETHODS_ARG(MethodID)
               );
    AROS_SLOWSTACKMETHODS_POST
}

IPTR AsmCoerceMethodA( Class * cl __asm("a0"), Object * obj __asm("a2"), Msg message __asm("a1"))
{
  return CoerceMethodA(cl, obj, message);
}
