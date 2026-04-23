/*
 * @(#) $Header$
 *
 * BGUI library
 * gfx.c
 *
 * (C) Copyright 1998 Manuel Lemos.
 * (C) Copyright 1996-1997 Ian J. Einman.
 * (C) Copyright 1993-1996 Jaba Development.
 * (C) Copyright 1993-1996 Jan van den Baard.
 * All Rights Reserved.
 *
 * $Log$
 * Revision 42.6  2004/06/16 20:16:48  verhaegs
 * Use METHODPROTO, METHOD_END and REGFUNCPROTOn where needed.
 *
 * Revision 42.5  2003/01/18 19:09:56  chodorowski
 * Instead of using the _AROS or __AROS preprocessor symbols, use __AROS__.
 *
 * Revision 42.4  2000/08/09 11:45:57  chodorowski
 * Removed a lot of #ifdefs that disabled the AROS_LIB* macros when not building on AROS. This is now handled in contrib/bgui/include/bgui_compilerspecific.h.
 *
 * Revision 42.3  2000/05/29 00:40:23  bergers
 * Update to compile with AROS now. Should also still compile with SASC etc since I only made changes that test the define __AROS__. The compilation is still very noisy but it does the trick for the main directory. Maybe members of the BGUI team should also have a look at the compiler warnings because some could also cause problems on other systems... (Comparison always TRUE due to datatype (or something like that)). And please compile it on an Amiga to see whether it still works... Thanks.
 *
 * Revision 42.2  2000/05/15 19:27:01  stegerg
 * another hundreds of REG() macro replacements in func headers/protos.
 *
 * Revision 42.1  2000/05/14 23:32:47  stegerg
 * changed over 200 function headers which all use register
 * parameters (oh boy ...), because the simple REG() macro
 * doesn't work with AROS. And there are still hundreds
 * of headers left to be fixed :(
 *
 * Many of these functions would also work with stack
 * params, but since i have fixed every single one
 * I encountered up to now, I guess will have to do
 * the same for the rest.
 *
 * Revision 42.0  2000/05/09 22:09:05  mlemos
 * Bumped to revision 42.0 before handing BGUI to AROS team
 *
 * Revision 41.11  2000/05/09 19:54:20  mlemos
 * Merged with the branch Manuel_Lemos_fixes.
 *
 * Revision 41.10.2.2  1999/07/04 05:16:05  mlemos
 * Added a debuging version of the function BRectFill.
 *
 * Revision 41.10.2.1  1998/07/05 19:26:58  mlemos
 * Added debugging code to trap invalid RectFill calls.
 *
 * Revision 41.10  1998/02/25 21:12:07  mlemos
 * Bumping to 41.10
 *
 * Revision 1.1  1998/02/25 17:08:24  mlemos
 * Ian sources
 *
 *
 */

#include "include/classdefs.h"

/*
 * Default drawinfo pens.
 */
makeproto UWORD DefDriPens[12] = { 0, 1, 1, 2, 1, 3, 1, 0, 2, 1, 2, 1 };

/*
 * Disabled pattern.
 */
STATIC UWORD DisPat[2] = { 0x2222, 0x8888 };

/*
 * Calculate the text width.
 */
makeproto ASM ULONG TextWidth(struct RastPort * rp __asm("a1"), UBYTE * text __asm("a0"))
{
   return TextWidthNum(rp, text, strlen(text));
}

makeproto ASM ULONG TextWidthNum(struct RastPort * rp __asm("a1"), UBYTE * text __asm("a0"), ULONG len __asm("d0"))
{
   struct TextExtent te;
   ULONG             extent;

   /*
    * Call TextExtent to find out the text width.
    */
   TextExtent(rp, text, len, &te);

   /*
    * Figure out extent width.
    */
   extent = te.te_Extent.MaxX - te.te_Extent.MinX + 1;

   /*
    * Return which ever is bigger.. extent or te.te_Width.
    */
   return( extent /*( extent > te.te_Width ) ? extent : te.te_Width */);
}

/*
 * Disable the given area.
 */
makeproto ASM VOID BDisableBox(struct BaseInfo * bi __asm("a0"), struct IBox * area __asm("a1"))
{
   BSetAfPt(bi, DisPat, 1);
   BSetDrMd(bi, JAM1);
   BSetDPenA(bi, SHADOWPEN);
   BBoxFillA(bi, area);
   BClearAfPt(bi);
}

#ifdef DEBUG_BGUI
makeproto ASM VOID SRectFillDebug(struct RastPort * rp __asm("a0"), LONG l __asm("d0"), LONG t __asm("d1"), LONG r __asm("d2"), LONG b __asm("d3"),STRPTR file __asm("a1"),ULONG line __asm("d4"))
#else
ASM VOID SRectFill(struct RastPort * rp __asm("a0"), LONG l __asm("d0"), LONG t __asm("d1"), LONG r __asm("d2"), LONG b __asm("d3"))
#endif
{
   if ((r >= l) && (b >= t))
      RectFill(rp, l, t, r, b);
#ifdef DEBUG_BGUI
   else
	   D(bug("***Invalid RectFill (%lx,%ld,%ld,%ld,%ld) (%s,%lu)\n",rp,l,t,r,b,file ? file : (STRPTR)"Unknown file",line));
#endif
}

/*
 * Do a safe rect-fill.
 */
#ifdef DEBUG_BGUI
  makeproto ASM VOID BRectFillDebug(struct BaseInfo * bi __asm("a0"), LONG l __asm("d0"), LONG t __asm("d1"), LONG r __asm("d2"), LONG b __asm("d3"),STRPTR file __asm("a1"),ULONG line __asm("d4"))
  {
     SRectFillDebug(bi->bi_RPort, l, t, r, b,file,line);
  }
#else
  ASM VOID BRectFill(struct BaseInfo * bi __asm("a0"), LONG l __asm("d0"), LONG t __asm("d1"), LONG r __asm("d2"), LONG b __asm("d3"))
  {
     SRectFill(bi->bi_RPort, l, t, r, b);
  }
#endif


/*
 * Render a frame/separator title.
 */
makeproto VOID RenderTitle(Object *title, struct BaseInfo *bi, WORD l, WORD t, WORD w, BOOL highlight, BOOL center, UWORD place)
{
   struct IBox box;

   w -= 24;
   l += 12;
   
   if (w > 8)
   {
      DoMethod(title, TEXTM_DIMENSIONS, bi->bi_RPort, &box.Width, &box.Height);
	 
      /*
       * Figure out where to render.
       */
      if (box.Width > w) box.Width = w;

      switch (place)
      {
      case 1:
	 box.Left = l;
	 break;
      case 2:
	 box.Left = l + w - box.Width;
	 break;
      default:
	 box.Left = l + ((w - box.Width) >> 1);
	 break;
      };
      if (box.Left < l) box.Left = l;

      /*
       * Figure out y position.
       */
      if (center) box.Top = t - (box.Height >> 1);
      else        box.Top = t - bi->bi_RPort->TxBaseline;

      /*
       * Setup rastport.
       */
      BSetDPenA(bi, BACKGROUNDPEN);
      BSetDrMd(bi, JAM1);
      BClearAfPt(bi);

      /*
       * Clear text area.
       */
      BRectFill(bi, box.Left - 4, box.Top, box.Left + box.Width + 4, box.Top + box.Height);

      BSetDPenA(bi, highlight ? HIGHLIGHTTEXTPEN : TEXTPEN);
      DoMethod(title, TEXTM_RENDER, bi, &box);
   };
}

makeproto VOID ASM SetDashedLine(struct BaseInfo * bi __asm("a0"), UWORD offset __asm("d0"))
{
   /*
    * Render a SHINE/SHADOW pen, dotted box or,
    * when the two pens are equal a SHADOW/BACKGROUND
    * pen dotted box.
    */
   BSetDPenA(bi, SHINEPEN);
   BSetDPenB(bi, SHADOWPEN);
   BSetDrMd(bi, JAM2);
   BSetDrPt(bi, 0xF0F0F0F0 >> offset);

   if (bi->bi_RPort->FgPen == bi->bi_RPort->BgPen)
      BSetDPenA(bi, BACKGROUNDPEN);
}

/*
 * Quickly render a bevelled box.
 */
makeproto VOID RenderBevelBox(struct BaseInfo *bi, WORD l, WORD t, WORD r, WORD b, UWORD state, BOOL recessed, BOOL thin)
{
   struct RastPort *rp = bi->bi_RPort;

   /*
    * Selected or normal?
    */
   if ((state == IDS_SELECTED) || (state == IDS_INACTIVESELECTED))
   {
      recessed = !recessed;
   };
   
   /*
    * Shiny side.
    */
   BSetDPenA(bi, recessed ? SHADOWPEN : SHINEPEN);

   HLine(rp, l, t, r);
   VLine(rp, l, t, b - 1);
   if (!thin) VLine(rp, l + 1, t + 1, b - 1);

   /*
    * Shadow side.
    */
   BSetDPenA(bi, recessed ? SHINEPEN : SHADOWPEN);

   HLine(rp, l, b, r);
   VLine(rp, r, t + 1, b);
   if (!thin) VLine(rp, r - 1, t + 1, b - 1);
}

makeproto ASM VOID BRectFillA(struct BaseInfo * bi __asm("a0"), struct Rectangle * rect __asm("a1"))
{
   BRectFill(bi, rect->MinX, rect->MinY, rect->MaxX, rect->MaxY);
}

makeproto ASM VOID BBoxFill(struct BaseInfo * bi __asm("a0"), LONG l __asm("d0"), LONG t __asm("d1"), LONG w __asm("d2"), LONG h __asm("d3"))
{
   SRectFill(bi->bi_RPort, l, t, l + w - 1, t + h - 1);
}

makeproto ASM VOID BBoxFillA(struct BaseInfo * bi __asm("a0"), struct IBox * box __asm("a1"))
{
   BBoxFill(bi, box->Left, box->Top, box->Width, box->Height);
}

/*
 * Background filling.
 */
#ifdef DEBUG_BGUI
makeproto ASM VOID RenderBackFillRasterDebug(struct RastPort * rp __asm("a0"), struct IBox * ib __asm("a1"), UWORD apen __asm("d0"), UWORD bpen __asm("d1"),STRPTR file __asm("a2"), ULONG line __asm("d2"))
#else
ASM VOID RenderBackFillRaster(struct RastPort * rp __asm("a0"), struct IBox * ib __asm("a1"), UWORD apen __asm("d0"), UWORD bpen __asm("d1"))
#endif
{
   static UWORD pat[] = { 0x5555, 0xAAAA };
   /*
    * Setup RastPort.
    */

   FSetABPenDrMd(rp, apen, bpen, JAM2);

   if (apen == bpen)
   {
      FSetDrMd(rp, JAM1);
      FClearAfPt(rp);
   }
   else
   {
      SetAfPt(rp, pat, 1);
   };

   /*
    * Render...
    */
#ifdef DEBUG_BGUI
   SRectFillDebug(rp, ib->Left, ib->Top, ib->Left + ib->Width - 1, ib->Top + ib->Height - 1,file,line);
#else
   SRectFill(rp, ib->Left, ib->Top, ib->Left + ib->Width - 1, ib->Top + ib->Height - 1);
#endif

   /*
    * Clear area pattern.
    */
   FClearAfPt(rp);
}


makeproto ASM VOID RenderBackFill(struct RastPort * rp __asm("a0"), struct IBox * ib __asm("a1"), UWORD * pens __asm("a2"), ULONG type __asm("d0"))
{
   int apen, bpen;

   if (!pens) pens = DefDriPens;

   /*
    * Which type?
    */
   switch (type)
   {
   case  SHINE_RASTER:
      apen = SHINEPEN;
      bpen = BACKGROUNDPEN;
      break;

   case  SHADOW_RASTER:
      apen = SHADOWPEN;
      bpen = BACKGROUNDPEN;
      break;

   case  SHINE_SHADOW_RASTER:
      apen = SHINEPEN;
      bpen = SHADOWPEN;
      break;

   case  FILL_RASTER:
      apen = FILLPEN;
      bpen = BACKGROUNDPEN;
      break;

   case  SHINE_FILL_RASTER:
      apen = SHINEPEN;
      bpen = FILLPEN;
      break;

   case  SHADOW_FILL_RASTER:
      apen = SHADOWPEN;
      bpen = FILLPEN;
      break;

   case  SHINE_BLOCK:
      apen = SHINEPEN;
      bpen = apen;
      break;

   case  SHADOW_BLOCK:
      apen = SHADOWPEN;
      bpen = apen;
      break;

   default:
      apen = BACKGROUNDPEN;
      bpen = apen;
      break;
   };
   RenderBackFillRaster(rp, ib, pens[apen], pens[bpen]);
}

/*
 * Draw a dotted box.
 */
makeproto ASM VOID DottedBox(struct BaseInfo * bi __asm("a0"), struct IBox * ibx __asm("a1"))
{
   int x1 = ibx->Left;
   int x2 = ibx->Left + ibx->Width - 1;
   int y1 = ibx->Top;
   int y2 = ibx->Top + ibx->Height - 1;
   
   ULONG secs, micros, hundredths;

   struct RastPort *rp = bi->bi_RPort;
   
   /*
    * We clear any thick framing which may be
    * there or not.
    */
   BSetDPenA(bi, BACKGROUNDPEN);
   Move(rp, x1 + 1, y1 + 1);
   Draw(rp, x1 + 1, y2 - 1);
   Draw(rp, x2 - 1, y2 - 1);
   Draw(rp, x2 - 1, y1 + 1);
   Draw(rp, x1 + 2, y1 + 1);

   CurrentTime(&secs, &micros);
   hundredths = ((secs & 0xFFFFFF) * 100) + (micros / 10000);
   SetDashedLine(bi, (hundredths / 5) % 8);

   /*
    * Draw the box.
    */
   Move(rp, x1, y1);
   Draw(rp, x2, y1);
   Draw(rp, x2, y2);
   Draw(rp, x1, y2);
   Draw(rp, x1, y1 + 1);
}

/*
 * Find out rendering state.
 */
makeproto ASM ULONG GadgetState(struct BaseInfo * bi __asm("a0"), Object * obj __asm("a1"), BOOL norec __asm("d0"))
{
   BOOL active = !(GADGET(obj)->Activation & BORDERMASK) || (bi->bi_IWindow->Flags & WFLG_WINDOWACTIVE);
   BOOL normal = !(GADGET(obj)->Flags & GFLG_SELECTED) || norec;
   
   return active ? (ULONG)(normal ? IDS_NORMAL         : IDS_SELECTED)
		 : (ULONG)(normal ? IDS_INACTIVENORMAL : IDS_INACTIVESELECTED);
}

#ifdef __AROS__
makearosproto
AROS_LH6(VOID, BGUI_FillRectPattern,
    AROS_LHA(struct RastPort *, r, A1),
    AROS_LHA(struct bguiPattern *, bp, A0),
    AROS_LHA(ULONG, x1, D0),
    AROS_LHA(ULONG, y1, D1),
    AROS_LHA(ULONG, x2, D2),
    AROS_LHA(ULONG, y2, D3),
    struct Library *, BGUIBase, 22, BGUI)
#else
makeproto SAVEDS ASM VOID BGUI_FillRectPattern(struct RastPort * r __asm("a1"), struct bguiPattern * bp __asm("a0"),
   ULONG x1 __asm("d0"), ULONG y1 __asm("d1"), ULONG x2 __asm("d2"), ULONG y2 __asm("d3"))
#endif
{
   AROS_LIBFUNC_INIT

   int i, j;

   int x0 = bp->bp_Left;
   int y0 = bp->bp_Top;
   int w  = bp->bp_Width;
   int h  = bp->bp_Height;
   
   int col_min, col_max;
   int row_min, row_max;

   int from_x, to_x1, to_x2, to_w;
   int from_y, to_y1, to_y2, to_h;

   struct BitMap *bm = bp->bp_BitMap;
   
   if (bm && w && h)
   {
      col_min = x1 / w;
      row_min = y1 / h;
      col_max = x2++ / w + 1;
      row_max = y2++ / h + 1;
   
      for (i = col_min; i <= col_max; i++)
      {
	 to_x1 = i * w;
	 to_x2 = to_x1 + w;
      
	 if (to_x1 < x1)
	 {
	    from_x = x0 + x1 - to_x1;
	    to_x1  = x1;
	 }
	 else
	 {
	    from_x = x0;
	 };
	 if (to_x2 > x2)
	 {
	    to_x2 = x2;
	 };
	 if ((to_w = to_x2 - to_x1) > 0)
	 {
	    for (j = row_min; j <= row_max; j++)
	    {
	       to_y1 = j * h;
	       to_y2 = to_y1 + h;      
   
	       if (to_y1 < y1)
	       {
		  from_y = y0 + y1 - to_y1;
		  to_y1  = y1;
	       }
	       else
	       {
		  from_y = y0;
	       }
	       if (to_y2 > y2)
	       {
		  to_y2 = y2;
	       };
	       if ((to_h = to_y2 - to_y1) > 0)
	       {
		  BltBitMapRastPort(bm, from_x, from_y, r, to_x1, to_y1, to_w, to_h, 0xC0);
	       };
	    };
	 };
      };
   };

   AROS_LIBFUNC_EXIT
}

makeproto VOID ASM HLine(struct RastPort * rp __asm("a1"), UWORD l __asm("d0"), UWORD t __asm("d1"), UWORD r __asm("d2"))
{
   Move(rp, l, t);
   Draw(rp, r, t);
}

makeproto VOID ASM VLine(struct RastPort * rp __asm("a1"), UWORD l __asm("d0"), UWORD t __asm("d1"), UWORD b __asm("d2"))
{
   Move(rp, l, t);
   Draw(rp, l, b);
}

makeproto ASM ULONG FGetAPen(struct RastPort * rp __asm("a1"))
{
   #ifdef ENHANCED
   return GetAPen(rp);
   #else
   if (OS30) return GetAPen(rp);
   return rp->FgPen;
   #endif
}

makeproto ASM ULONG FGetBPen(struct RastPort * rp __asm("a1"))
{
   #ifdef ENHANCED
   return GetBPen(rp);
   #else
   if (OS30) return GetBPen(rp);
   return rp->BgPen;
   #endif
}

makeproto ASM ULONG FGetDrMd(struct RastPort * rp __asm("a1"))
{
   #ifdef ENHANCED
   return GetDrMd(rp);
   #else
   if (OS30) return GetDrMd(rp);
   return rp->DrawMode;
   #endif
}

makeproto ASM ULONG FGetDepth(struct RastPort * rp __asm("a1"))
{
   #ifdef ENHANCED
   return (ULONG)GetBitMapAttr(rp->BitMap, BMA_DEPTH);
   #else
   if (OS30) return GetBitMapAttr(rp->BitMap, BMA_DEPTH);
   return rp->BitMap->Depth;
   #endif
}

makeproto ASM VOID FSetAPen(struct RastPort * rp __asm("a1"), ULONG pen __asm("d0"))
{
   #ifdef ENHANCED
   SetRPAttrs(rp, RPTAG_APen, pen, TAG_END);
   #else
   if (OS30) SetRPAttrs(rp, RPTAG_APen, pen, TAG_END);
   else if (rp->FgPen != pen) SetAPen(rp, pen);
   #endif
}

makeproto ASM VOID FSetBPen(struct RastPort * rp __asm("a1"), ULONG pen __asm("d0"))
{
   #ifdef ENHANCED
   SetRPAttrs(rp, RPTAG_BPen, pen, TAG_END);
   #else
   if (OS30) SetRPAttrs(rp, RPTAG_BPen, pen, TAG_END);
   else if (rp->BgPen != pen) SetBPen(rp, pen);
   #endif
}

makeproto ASM VOID FSetDrMd(struct RastPort * rp __asm("a1"), ULONG drmd __asm("d0"))
{
   #ifdef ENHANCED
   SetRPAttrs(rp, RPTAG_DrMd, drmd, TAG_END);
   #else
   if (OS30) SetRPAttrs(rp, RPTAG_DrMd, drmd, TAG_END);
   else if (rp->DrawMode != drmd) SetDrMd(rp, drmd);
   #endif
}

makeproto ASM VOID FSetABPenDrMd(struct RastPort * rp __asm("a1"), ULONG apen __asm("d0"), ULONG bpen __asm("d1"), ULONG mode __asm("d2"))
{
   #ifdef ENHANCED
   SetABPenDrMd(rp, apen, bpen, mode);
   #else
   if (OS30) SetABPenDrMd(rp, apen, bpen, mode);
   else
   {
      SetAPen(rp, apen);
      SetBPen(rp, bpen);
      SetDrMd(rp, mode);
   };
   #endif
}

makeproto ASM VOID FSetFont(struct RastPort * rp __asm("a1"), struct TextFont * tf __asm("a0"))
{
   #ifdef ENHANCED
   SetRPAttrs(rp, RPTAG_Font, tf, TAG_END);
   #else
   if (OS30) SetRPAttrs(rp, RPTAG_Font, tf, TAG_END);
   else
   {
      if (rp->Font != tf) SetFont(rp, tf);
   }
   #endif
}

makeproto ASM VOID FSetFontStyle(struct RastPort * rp __asm("a1"), ULONG style __asm("d0"))
{
   SetSoftStyle(rp, style, AskSoftStyle(rp));
}

makeproto ASM VOID FClearAfPt(struct RastPort * rp __asm("a1"))
{
   SetAfPt(rp, NULL, 0);
}

makeproto ASM VOID BSetDPenA(struct BaseInfo * bi __asm("a0"), LONG pen __asm("d0"))
{
   FSetAPen(bi->bi_RPort, bi->bi_Pens[pen]);
}

makeproto ASM VOID BSetPenA(struct BaseInfo * bi __asm("a0"), ULONG pen __asm("d0"))
{
   FSetAPen(bi->bi_RPort, pen);
}

makeproto ASM VOID BSetDPenB(struct BaseInfo * bi __asm("a0"), LONG pen __asm("d0"))
{
   FSetBPen(bi->bi_RPort, bi->bi_Pens[pen]);
}

makeproto ASM VOID BSetPenB(struct BaseInfo * bi __asm("a0"), ULONG pen __asm("d0"))
{
   FSetBPen(bi->bi_RPort, pen);
}

makeproto ASM VOID BSetDrMd(struct BaseInfo * bi __asm("a0"), ULONG drmd __asm("d0"))
{
   FSetDrMd(bi->bi_RPort, drmd);
}

makeproto ASM VOID BSetFont(struct BaseInfo * bi __asm("a0"), struct TextFont * tf __asm("a1"))
{
   FSetFont(bi->bi_RPort, tf);
}

makeproto ASM VOID BSetFontStyle(struct BaseInfo * bi __asm("a0"), ULONG style __asm("d0"))
{
   SetSoftStyle(bi->bi_RPort, style, AskSoftStyle(bi->bi_RPort));
}

makeproto ASM VOID BSetAfPt(struct BaseInfo * bi __asm("a0"), UWORD * pat __asm("a1"), ULONG size __asm("d0"))
{
   SetAfPt(bi->bi_RPort, pat, size);
}

makeproto ASM VOID BClearAfPt(struct BaseInfo * bi __asm("a0"))
{
   SetAfPt(bi->bi_RPort, NULL, 0);
}

makeproto ASM VOID BSetDrPt(struct BaseInfo * bi __asm("a0"), ULONG pat __asm("d0"))
{
   SetDrPt(bi->bi_RPort, pat & 0xFFFF);
}

makeproto ASM VOID BDrawImageState(struct BaseInfo * bi __asm("a0"), Object * image __asm("a1"),
   ULONG x __asm("d0"), ULONG y __asm("d1"), ULONG state __asm("d2"))
{
   //tprintf("%08lx %08lx %ld %ld %04lx %08lx\n", /4*
   DrawImageState(bi->bi_RPort, IMAGE(image), x, y, state, bi->bi_DrInfo);
}
