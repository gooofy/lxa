/*
 * include/aros/class-protos.h
 *
 */

/* listclass.c                                */

extern Class *InitListClass(void);

/* KillBGUI.c                                 */

/* No external prototypes defined. */

/* buttonclass.c                              */

extern Class *InitButtonClass(void);

/* labelclass.c                               */

extern Class *InitLabelClass(void);

/* aslreqclass.c                              */

extern Class *InitAslReqClass(void);

/* strformat.c                                */

extern ASM ULONG CompStrlenF(UBYTE * __asm("a0"), RAWARG args __asm("a1"));

extern ASM VOID DoSPrintF(UBYTE * __asm("a0"), UBYTE * __asm("a1"), RAWARG args __asm("a2"));
#ifdef __AROS__
#else
/* lxa: upstream declared `void sprintf(char *, char *, ...)` here, which
 * collides with bebbo's <stdio.h> standard `int sprintf(char *,
 * const char *, ...)`.  BGUI's strformat.c uses DoSPrintF/SPrintfA
 * internally and does not actually need a custom sprintf prototype,
 * so we drop the redeclaration. */
#endif
extern void tprintf(char *, ...);

/* misc.c                                     */

extern struct TextAttr Topaz80;

extern ASM LONG max(LONG __asm("d0"), LONG __asm("d1"));

extern ASM LONG min(LONG __asm("d0"), LONG __asm("d1"));

extern ASM LONG abs(LONG __asm("d0"));

extern ASM LONG range(LONG __asm("d0"), LONG __asm("d1"), LONG __asm("d2"));

extern ASM ULONG CountLabels(UBYTE ** __asm("a0"));

extern ASM BOOL PointInBox(struct IBox * __asm("a0"), WORD __asm("d0"), WORD __asm("d1"));

extern VOID Scale(struct RastPort *, UWORD *, UWORD *, UWORD, UWORD, struct TextAttr *);
extern WORD MapKey(UWORD, UWORD, APTR *);

extern ASM VOID ShowHelpReq( struct Window * __asm("a0"), UBYTE * __asm("a1"));

extern ASM UBYTE *DoBuffer(UBYTE * __asm("a0"), UBYTE ** __asm("a1"), ULONG * __asm("a2"), RAWARG args __asm("a3"));

extern VOID DoMultiSet(Tag, IPTR, ULONG, Object *, ...);

extern ASM VOID SetGadgetBounds(Object * __asm("a0"), struct IBox * __asm("a1"));

extern ASM VOID SetImageBounds(Object * __asm("a0"), struct IBox * __asm("a1"));

extern ASM VOID UnmapTags(struct TagItem * __asm("a0"), struct TagItem * __asm("a1"));

extern ASM Object *CreateVector(struct TagItem * __asm("a0"));

extern ASM struct TagItem *BGUI_NextTagItem(struct TagItem ** __asm("a0"));

/* baseclass.c                                */

extern ULONG CalcDimensions(Class *, Object *, struct bmDimensions *, UWORD, UWORD);
#ifndef __AROS__
extern SAVEDS ASM VOID BGUI_PostRender(Class * __asm("a0"), Object * __asm("a2"), struct gpRender * __asm("a1"));
#endif

extern Class *BaseClass;
extern Class *InitBaseClass(void);

/* ver.c                                      */

/* No external prototypes defined. */

/* request.c                                  */

#ifndef __AROS__
extern SAVEDS ASM ULONG BGUI_RequestA(struct Window * __asm("a0"), struct bguiRequest * __asm("a1"), ULONG * __asm("a2"));
#endif

/* cycleclass.c                               */

extern Class *InitCycleClass(void);

/* task.c                                     */

extern TL                     TaskList;
extern struct SignalSemaphore TaskLock;
extern void InitTaskList(void);
extern struct TextFont *BGUI_OpenFontDebug(struct TextAttr *,STRPTR,ULONG);
extern void BGUI_CloseFontDebug(struct TextFont *,STRPTR,ULONG);
extern void FreeTaskList(void);
extern TM *FindTaskMember(void);
extern TM *InternalFindTaskMember(struct Task *);
extern UWORD AddTaskMember(void);
extern BOOL FreeTaskMember( void);
extern BOOL GetWindowBounds(ULONG, struct IBox *);
extern VOID SetWindowBounds(ULONG, struct IBox *);

#ifndef __AROS__
extern SAVEDS ASM APTR BGUI_AllocPoolMemDebug(ULONG __asm("d0"), STRPTR __asm("a0"), ULONG __asm("d1"));
extern SAVEDS ASM APTR BGUI_AllocPoolMem(ULONG __asm("d0"));
extern SAVEDS ASM VOID BGUI_FreePoolMemDebug(APTR __asm("a0"), STRPTR __asm("a1"), ULONG __asm("d0"));
extern SAVEDS ASM VOID BGUI_FreePoolMem(APTR __asm("a0"));
#endif

extern ASM BOOL AddIDReport(struct Window * __asm("a0"), ULONG __asm("d0"), struct Task * __asm("a1"));

extern ASM ULONG GetIDReport(struct Window * __asm("a0"));

extern struct Window *GetFirstIDReportWindow(void);

extern ASM VOID RemoveIDReport(struct Window * __asm("a0"));

extern ASM VOID AddWindow(Object * __asm("a0"), struct Window * __asm("a1"));

extern ASM VOID RemWindow(Object * __asm("a0"));

extern ASM Object *WhichWindow(struct Screen * __asm("a0"));

extern struct TagItem *DefTagList(ULONG, struct TagItem *);

extern SAVEDS ASM ULONG BGUI_CountTagItems(struct TagItem * __asm("a0"));

extern SAVEDS ASM struct TagItem *BGUI_MergeTagItems(struct TagItem * __asm("a0"), struct TagItem * __asm("a1"));

extern SAVEDS ASM struct TagItem *BGUI_CleanTagItems(struct TagItem * __asm("a0"), LONG __asm("d0"));

#ifndef __AROS__
extern SAVEDS ASM struct TagItem *BGUI_GetDefaultTags(ULONG __asm("d0"));
extern SAVEDS ASM VOID BGUI_DefaultPrefs(VOID);
extern SAVEDS ASM VOID BGUI_LoadPrefs(UBYTE * __asm("a0"));
#endif

/* arexxclass.c                               */

extern Class *InitArexxClass(void);

/* stringclass.c                              */

extern Class *InitStringClass(void);

/* externalclass.c                            */

extern Class *InitExtClass( void);

/* classes.c                                  */

extern UBYTE *RootClass;
extern UBYTE *ImageClass;
extern UBYTE *GadgetClass;
extern UBYTE *PropGClass;
extern UBYTE *StrGClass;

extern Class *BGUI_MakeClass(Tag, ...);

#ifndef __AROS__
extern ASM Class *BGUI_MakeClassA(struct TagItem * __asm("a0"));
extern SAVEDS ASM BOOL BGUI_FreeClass(Class * __asm("a0"));
#endif

extern ULONG ASM BGUI_GetAttrChart(Class * __asm("a0"), Object * __asm("a2"), struct rmAttr * __asm("a1"));

extern ULONG ASM BGUI_SetAttrChart(Class * __asm("a0"), Object * __asm("a2"), struct rmAttr * __asm("a1"));

extern ULONG BGUI_PackStructureTag(UBYTE *, ULONG *, Tag tag, IPTR data);
extern ULONG BGUI_UnpackStructureTag(UBYTE *, ULONG *, Tag tag, IPTR *dataptr);

#ifndef __AROS__
extern SAVEDS ULONG ASM BGUI_PackStructureTags(APTR __asm("a0"), ULONG * __asm("a1"), struct TagItem * __asm("a2"));
extern SAVEDS ULONG ASM BGUI_UnpackStructureTags(APTR __asm("a0"), ULONG * __asm("a1"), struct TagItem * __asm("a2"));
#endif

extern ASM ULONG Get_Attr(Object * __asm("a0"), ULONG __asm("d0"), IPTR * __asm("a1"));

extern ASM ULONG Get_SuperAttr(Class * __asm("a2"), Object * __asm("a0"), ULONG __asm("d0"), IPTR * __asm("a1"));

extern IPTR  NewSuperObject(Class *, Object *, struct TagItem *);
extern ULONG DoSetMethodNG(Object *, Tag, ...);
extern ULONG DoSuperSetMethodNG(Class *, Object *, Tag, ...);
extern ULONG DoSetMethod(Object *, struct GadgetInfo *, Tag, ...);
extern ULONG DoSuperSetMethod(Class *, Object *, struct GadgetInfo *, Tag, ...);
extern ULONG DoUpdateMethod(Object *, struct GadgetInfo *, ULONG, Tag, ...);
extern ULONG DoNotifyMethod(Object *, struct GadgetInfo *, ULONG, Tag, ...);

extern ASM IPTR  DoRenderMethod(Object * __asm("a0"), struct GadgetInfo * __asm("a1"), ULONG __asm("d0"));

extern ASM IPTR  ForwardMsg(Object * __asm("a0"), Object * __asm("a1"), Msg __asm("a2"));

extern struct BaseInfo *AllocBaseInfoDebug(STRPTR, ULONG,Tag, ...);
extern struct BaseInfo *AllocBaseInfo(Tag, ...);

extern SAVEDS ASM struct BaseInfo *BGUI_AllocBaseInfoDebugA(struct TagItem * __asm("a0"),STRPTR __asm("a1"), ULONG __asm("d0"));

extern SAVEDS ASM struct BaseInfo *BGUI_AllocBaseInfoA(struct TagItem * __asm("a0"));

extern void FreeBaseInfoDebug(struct BaseInfo *, STRPTR, ULONG);
extern void FreeBaseInfo(struct BaseInfo *);

/* indicatorclass.c                           */

extern Class *InitIndicatorClass(void);

/* checkboxclass.c                            */

extern Class *InitCheckBoxClass(void);

/* radiobuttonclass.c                         */

extern Class *InitRadioButtonClass(void);

/* bgui_locale.c                              */

extern UWORD  NumCatCompStrings;
extern struct CatCompArrayType CatCompArray[];

/* commodityclass.c                           */

extern Class *InitCxClass(void);

/* fontreqclass.c                             */

extern Class *InitFontReqClass(void);

/* frameclass.c                               */

extern Class *InitFrameClass(void);

/* screenreqclass.c                           */

extern Class *InitScreenReqClass(void);

/* sliderclass.c                              */

extern Class *InitSliderClass(void);

/* spacingclass.c                             */

extern Class *InitSpacingClass(void);

/* systemiclass.c                             */

extern Class *InitSystemClass(void);

/* pageclass.c                                */

extern Class *InitPageClass(void);

/* MakeProto.c                                */

/* No external prototypes defined. */

/* windowclass.c                              */

extern Class *InitWindowClass(void);

/* dgm.c                                      */

extern ULONG myDoGadgetMethod(Object *, struct Window *, struct Requester *, STACKULONG MethodID, ...);

#ifndef __AROS__
extern SAVEDS ASM ULONG BGUI_DoGadgetMethodA( Object * __asm("a0"), struct Window * __asm("a1"), struct Requester * __asm("a2"), Msg __asm("a3"));
#endif

extern Class *InitDGMClass(void);

/* groupclass.c                               */

extern Class *InitGroupNodeClass(void);

extern ASM ULONG RelayoutGroup(Object * __asm("a0"));

extern Class *InitGroupClass(void);

/* rootclass.c                                */

extern Object *ListHeadObject(struct List *);
extern Object *ListTailObject(struct List *);
extern Class *InitRootClass(void);
extern Class *InitGadgetClass(void);
extern Class *InitImageClass(void);

/* libfunc.c                                  */

extern BOOL FreeClasses(void);
extern ULONG TrackNewObject(Object *,struct TagItem *);
extern BOOL TrackDisposedObject(Object *);
extern void MarkFreedClass(Class *);

#ifndef __AROS__
extern SAVEDS ASM Class *BGUI_GetClassPtr(ULONG __asm("d0"));
#endif

extern Object *BGUI_NewObject(ULONG, Tag, ...);

#ifndef __AROS__
extern SAVEDS ASM Object *BGUI_NewObjectA(ULONG __asm("d0"), struct TagItem * __asm("a0"));
extern SAVEDS ASM struct BitMap *BGUI_AllocBitMap(ULONG __asm("d0"), ULONG __asm("d1"), ULONG __asm("d2"), ULONG __asm("d3"), struct BitMap * __asm("a0"));
extern SAVEDS ASM VOID BGUI_FreeBitMap(struct BitMap * __asm("a0"));
extern SAVEDS ASM struct RastPort *BGUI_CreateRPortBitMap(struct RastPort * __asm("a0"), ULONG __asm("d0"), ULONG __asm("d1"), ULONG __asm("d2"));
extern SAVEDS ASM VOID BGUI_FreeRPortBitMap(struct RastPort * __asm("a0"));
extern SAVEDS ASM BOOL BGUI_Help(struct Window * __asm("a0"), UBYTE * __asm("a1"), UBYTE * __asm("a2"), ULONG __asm("d0"));
extern SAVEDS ASM APTR BGUI_LockWindow( struct Window * __asm("a0"));
extern SAVEDS ASM VOID BGUI_UnlockWindow( APTR __asm("a0"));
extern SAVEDS ASM STRPTR BGUI_GetLocaleStr(struct bguiLocale * __asm("a0"), ULONG __asm("d0"));
extern SAVEDS ASM CONST_STRPTR BGUI_GetCatalogStr(struct bguiLocale * __asm("a0"), ULONG __asm("d0"), CONST_STRPTR __asm("a1"));
#endif

extern SAVEDS ASM IPTR BGUI_CallHookPkt(struct Hook * __asm("a0"),APTR __asm("a2"),APTR __asm("a1"));

/* filereqclass.c                             */

extern Class *InitFileReqClass(void);

/* separatorclass.c                           */

extern Class *InitSepClass(void);

/* areaclass.c                                */

extern Class *InitAreaClass(void);

/* Tap.c                                      */

/* No external prototypes defined. */

/* vectorclass.c                              */

extern Class *InitVectorClass(void);

/* mxclass.c                                  */

extern Class *InitMxClass(void);

/* viewclass.c                                */

extern Class *InitViewClass(void);

/* propclass.c                                */

extern Class *InitPropClass(void);

/* infoclass.c                                */

extern Class *InitInfoClass( void);

/* textclass.c                                */

extern Class *InitTextClass(void);
#ifndef __AROS__
extern SAVEDS ASM VOID BGUI_InfoTextSize(struct RastPort * __asm("a0"), UBYTE * __asm("a1"), UWORD * __asm("a2"), UWORD * __asm("a3"));
extern SAVEDS ASM VOID BGUI_InfoText( struct RastPort * __asm("a0"), UBYTE * __asm("a1"), struct IBox * __asm("a2"), struct DrawInfo * __asm("a3"));
#endif

extern UWORD ASM TotalHeight( struct RastPort * __asm("a0"), UBYTE * __asm("a1"));

extern UWORD ASM TotalWidth(struct RastPort * __asm("a0"), UBYTE * __asm("a1"));

extern void RenderInfoText(struct RastPort *, UBYTE *, UWORD *, struct IBox *, UWORD);
extern void RenderText(struct BaseInfo *, UBYTE *, struct IBox *);

/* lib.c                                      */

extern BOOL OS30;
extern VOID InitLocale(void);
#ifdef __AROS__
#else
/* lxa: lib.c defines LibInit with 3 args (D0 dummy, A0 seg, A6 sysbase).
 * Phase 42.13 of lib.c notes the D0 param "may not leave out, even if
 * unused, otherwise it crashes on machines where all arguments are passed
 * on stack."  AROS's MakeProto generated a 2-arg signature here.
 *
 * lxa: For bebbo we need explicit __asm("dN")/("aN") clauses on each
 * argument so GCC actually passes them in registers (the empty REG()
 * fallback degrades to plain stack-passed C).  Use REGPARAM consistently
 * so prototype and definition agree.
 */
extern SAVEDS ASM struct Library *LibInit(REGPARAM(D0, ULONG, dummy),
                                          REGPARAM(A0, BPTR, segment),
                                          REGPARAM(A6, struct ExecBase *, syslib));
extern SAVEDS ASM struct Library *LibOpen(REGPARAM(A6, struct Library *, lib),
                                          REGPARAM(D0, ULONG, version));
extern SAVEDS ASM BPTR LibClose(REGPARAM(A6, struct Library *, lib));
extern SAVEDS ASM BPTR LibExpunge(REGPARAM(A6, struct Library *, lib));
extern SAVEDS LONG LibVoid(void);
#endif

/* gfx.c                                      */

extern UWORD DefDriPens[12];

extern ASM ULONG TextWidth(struct RastPort * __asm("a1"), UBYTE * __asm("a0"));

extern ASM ULONG TextWidthNum(struct RastPort * __asm("a1"), UBYTE * __asm("a0"), ULONG __asm("d0"));

extern ASM VOID BDisableBox(struct BaseInfo * __asm("a0"), struct IBox * __asm("a1"));

extern VOID RenderTitle(Object *, struct BaseInfo *, WORD, WORD, WORD, BOOL, BOOL, UWORD);

extern VOID ASM SetDashedLine(struct BaseInfo * __asm("a0"), UWORD __asm("d0"));

extern VOID RenderBevelBox(struct BaseInfo *, WORD, WORD, WORD, WORD, UWORD, BOOL, BOOL);

extern ASM VOID SRectFillDebug(struct RastPort * __asm("a0"), LONG __asm("d0"), LONG __asm("d1"), LONG __asm("d2"), LONG __asm("d3"),STRPTR __asm("a1"),ULONG __asm("d4"));
        
extern ASM VOID BRectFillDebug(struct BaseInfo * __asm("a0"), LONG __asm("d0"), LONG __asm("d1"), LONG __asm("d2"), LONG __asm("d3"),STRPTR __asm("a1"),ULONG __asm("d4"));

extern ASM VOID BRectFillA(struct BaseInfo * __asm("a0"), struct Rectangle * __asm("a1"));

extern ASM VOID BBoxFill(struct BaseInfo * __asm("a0"), LONG __asm("d0"), LONG __asm("d1"), LONG __asm("d2"), LONG __asm("d3"));

extern ASM VOID BBoxFillA(struct BaseInfo * __asm("a0"), struct IBox * __asm("a1"));

extern ASM VOID RenderBackFill(struct RastPort * __asm("a0"), struct IBox * __asm("a1"), UWORD * __asm("a2"), ULONG __asm("d0"));

extern ASM VOID RenderBackFillRasterDebug(struct RastPort * __asm("a0"), struct IBox * __asm("a1"), UWORD __asm("d0"), UWORD __asm("d1"),STRPTR __asm("a2"), ULONG __asm("d2"));
        
extern ASM VOID DottedBox(struct BaseInfo * __asm("a0"), struct IBox * __asm("a1"));

extern ASM ULONG GadgetState(struct BaseInfo * __asm("a0"), Object * __asm("a1"), BOOL __asm("d0"));

#ifndef __AROS__
extern SAVEDS ASM VOID BGUI_FillRectPattern(struct RastPort * __asm("a1"), struct bguiPattern * __asm("a0"), ULONG __asm("d0"), ULONG __asm("d1"), ULONG __asm("d2"), ULONG __asm("d3"));
#endif

extern VOID ASM HLine(struct RastPort * __asm("a1"), UWORD __asm("d0"), UWORD __asm("d1"), UWORD __asm("d2"));

extern VOID ASM VLine(struct RastPort * __asm("a1"), UWORD __asm("d0"), UWORD __asm("d1"), UWORD __asm("d2"));

extern ASM ULONG FGetAPen(struct RastPort * __asm("a1"));

extern ASM ULONG FGetBPen(struct RastPort * __asm("a1"));
        
extern ASM ULONG FGetDrMd(struct RastPort * __asm("a1"));

extern ASM ULONG FGetDepth(struct RastPort * __asm("a1"));

extern ASM VOID FSetAPen(struct RastPort * __asm("a1"), ULONG __asm("d0"));

extern ASM VOID FSetBPen(struct RastPort * __asm("a1"), ULONG __asm("d0"));

extern ASM VOID FSetDrMd(struct RastPort * __asm("a1"), ULONG __asm("d0"));

extern ASM VOID FSetABPenDrMd(struct RastPort * __asm("a1"), ULONG __asm("d0"), ULONG __asm("d1"), ULONG __asm("d2"));

extern ASM VOID FSetFont(struct RastPort * __asm("a1"), struct TextFont * __asm("a0"));

extern ASM VOID FSetFontStyle(struct RastPort * __asm("a1"), ULONG __asm("d0"));

extern ASM VOID FClearAfPt(struct RastPort * __asm("a1"));

extern ASM VOID BSetDPenA(struct BaseInfo * __asm("a0"), LONG __asm("d0"));

extern ASM VOID BSetPenA(struct BaseInfo * __asm("a0"), ULONG __asm("d0"));

extern ASM VOID BSetDPenB(struct BaseInfo * __asm("a0"), LONG __asm("d0"));

extern ASM VOID BSetPenB(struct BaseInfo * __asm("a0"), ULONG __asm("d0"));

extern ASM VOID BSetDrMd(struct BaseInfo * __asm("a0"), ULONG __asm("d0"));

extern ASM VOID BSetFont(struct BaseInfo * __asm("a0"), struct TextFont * __asm("a1"));

extern ASM VOID BSetFontStyle(struct BaseInfo * __asm("a0"), ULONG __asm("d0"));

extern ASM VOID BSetAfPt(struct BaseInfo * __asm("a0"), UWORD * __asm("a1"), ULONG __asm("d0"));

extern ASM VOID BClearAfPt(struct BaseInfo * __asm("a0"));

extern ASM VOID BSetDrPt(struct BaseInfo * __asm("a0"), ULONG __asm("d0"));

extern ASM VOID BDrawImageState(struct BaseInfo * __asm("a0"), Object * __asm("a1"), ULONG __asm("d0"), ULONG __asm("d1"), ULONG __asm("d2"));

/* progressclass.c                            */

extern Class *InitProgressClass(void);

/* blitter.c                                  */

extern VOID ASM EraseBMO(BMO * __asm("a0"));

extern VOID ASM LayerBMO(BMO * __asm("a0"));

extern VOID ASM DrawBMO(BMO * __asm("a0"));

extern ASM BMO *CreateBMO(Object * __asm("a0"), struct GadgetInfo * __asm("a1"));

extern ASM VOID DeleteBMO(BMO * __asm("a0"));

extern ASM VOID MoveBMO(BMO * __asm("a0"), WORD __asm("d0"), WORD __asm("d1"));

/* miscasm.asm                                */

struct NewAmigaGuide;
extern BOOL DisplayAGuideInfo(struct NewAmigaGuide *, Tag, ...);

extern ASM ULONG ScaleWeight(ULONG e __asm("d2"), ULONG f __asm("d3"), ULONG a __asm("d4"));

extern /*__stdargs*/ ASM ULONG StrLenfA(UBYTE * __asm("a0"), RAWARG args __asm("a1"));

extern /*__stdargs*/ ASM VOID SPrintfA(UBYTE * __asm("a3"), UBYTE * __asm("a0"), RAWARG args __asm("a1"));

//extern ASM VOID LHook_Count(struct Hook * hook __asm("a0"), ULONG chr __asm("a1"), struct Locale * loc __asm("a2"));
extern REGFUNCPROTO3(VOID, LHook_Count,
	REGPARAM(A0, struct Hook *, hook),
	REGPARAM(A2, struct Locale *, loc),
	REGPARAM(A1, ULONG, chr));

//extern ASM VOID LHook_Format(struct Hook * hook __asm("a0"), ULONG chr __asm("a1"), struct Locale * loc __asm("a2"));
extern REGFUNCPROTO3(VOID, LHook_Format,
	REGPARAM(A0, struct Hook *, hook),
	REGPARAM(A2, struct Locale *, loc),
	REGPARAM(A1, ULONG, chr));

extern ASM struct RastPort *BGUI_ObtainGIRPort( struct GadgetInfo * __asm("a0"));

extern IPTR AsmDoMethod( Object *, STACKULONG MethodID, ... );

extern IPTR AsmDoSuperMethod( Class *, Object *, STACKULONG MethodID, ... );
extern IPTR AsmCoerceMethod( Class *, Object *, STACKULONG MethodID, ... );

extern ASM IPTR AsmDoMethodA( Object * __asm("a2"), Msg __asm("a1"));

extern ASM IPTR AsmDoSuperMethodA( Class * __asm("a0"), Object * __asm("a2"), Msg __asm("a1"));

extern ASM IPTR AsmCoerceMethodA( Class * __asm("a0"), Object * __asm("a2"), Msg __asm("a1"));

/* libtag.asm                                 */

/* No external prototypes defined. */

/* stkext.asm                                 */

extern ASM APTR EnsureStack(VOID);

extern ASM VOID RevertStack( APTR __asm("a0"));

extern VOID InitInputStack(VOID);
extern VOID FreeInputStack(VOID);

#define makeproto
#define makearosproto
