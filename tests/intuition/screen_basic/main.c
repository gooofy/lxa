/*
 * Test: intuition/screen_basic
 * Tests OpenScreen() and CloseScreen() functions
 */

#include <exec/types.h>
#include <exec/memory.h>
#include <graphics/gfx.h>
#include <graphics/rastport.h>
#include <intuition/intuition.h>
#include <intuition/screens.h>
#include <utility/tagitem.h>
#include <clib/exec_protos.h>
#include <clib/graphics_protos.h>
#include <clib/intuition_protos.h>
#include <clib/dos_protos.h>
#include <inline/exec.h>
#include <inline/graphics.h>
#include <inline/intuition.h>
#include <inline/dos.h>

extern struct DosLibrary *DOSBase;
extern struct ExecBase *SysBase;
extern struct GfxBase *GfxBase;
extern struct IntuitionBase *IntuitionBase;

static void print(const char *s)
{
    BPTR out = Output();
    LONG len = 0;
    const char *p = s;
    while (*p++) len++;
    Write(out, (CONST APTR)s, len);
}

static void print_num(LONG v)
{
    char buf[16];
    int i = 15;
    int neg = v < 0;
    buf[i] = 0;
    if (neg) v = -v;
    do { buf[--i] = '0' + (v % 10); v /= 10; } while (v);
    if (neg) buf[--i] = '-';
    print(&buf[i]);
}

static void print_dims(struct Screen *s)
{
    print("  screen ");
    print_num(s->Width);
    print("x");
    print_num(s->Height);
    print("x");
    print_num(s->BitMap.Depth);
    print("\n");
}

int main(void)
{
    struct NewScreen ns;
    struct Screen *screen;
    struct Screen *tagged_screen = NULL;
    struct Screen *default_screen = NULL;
    struct Screen *locked_screen = NULL;
    struct Window *window = NULL;
    struct List *pub_list;
    struct PubScreenNode *pub_node;
    char default_name[MAXPUBSCREENNAME + 1];
    int errors = 0;

    print("Testing OpenScreen()/CloseScreen()...\n");

    /* Set up NewScreen structure */
    ns.LeftEdge = 0;
    ns.TopEdge = 0;
    ns.Width = 320;
    ns.Height = 200;
    ns.Depth = 2;  /* 4 colors */
    ns.DetailPen = 0;
    ns.BlockPen = 1;
    ns.ViewModes = 0;
    ns.Type = CUSTOMSCREEN;
    ns.Font = NULL;
    ns.DefaultTitle = (UBYTE *)"Test Screen";
    ns.Gadgets = NULL;
    ns.CustomBitMap = NULL;

    /* Open the screen */
    screen = OpenScreen(&ns);

    if (screen == NULL) {
        print("FAIL: OpenScreen() returned NULL\n");
        return 20;
    }
    print("OK: OpenScreen() succeeded\n");

    /* Verify screen dimensions */
    if (screen->Width != 320) {
        print("FAIL: screen->Width != 320\n");
        errors++;
    } else {
        print("OK: screen->Width = 320\n");
    }

    if (screen->Height != 200) {
        print("FAIL: screen->Height != 200\n");
        errors++;
    } else {
        print("OK: screen->Height = 200\n");
    }

    /* Verify BitMap initialization */
    if (screen->BitMap.Depth != 2) {
        print("FAIL: BitMap.Depth != 2\n");
        errors++;
    } else {
        print("OK: BitMap.Depth = 2\n");
    }

    if (screen->BitMap.BytesPerRow != 40) {  /* 320/8 = 40 */
        print("FAIL: BitMap.BytesPerRow != 40\n");
        errors++;
    } else {
        print("OK: BitMap.BytesPerRow = 40\n");
    }

    if (screen->BitMap.Rows != 200) {
        print("FAIL: BitMap.Rows != 200\n");
        errors++;
    } else {
        print("OK: BitMap.Rows = 200\n");
    }

    /* Verify bitplanes are allocated */
    if (screen->BitMap.Planes[0] == NULL) {
        print("FAIL: BitMap.Planes[0] is NULL\n");
        errors++;
    } else {
        print("OK: BitMap.Planes[0] allocated\n");
    }

    if (screen->BitMap.Planes[1] == NULL) {
        print("FAIL: BitMap.Planes[1] is NULL\n");
        errors++;
    } else {
        print("OK: BitMap.Planes[1] allocated\n");
    }

    /* Verify RastPort renders into the screen bitmap.  On AmigaOS 3.x the
     * RastPort points to the real (separately allocated) BitMap and
     * Screen.BitMap is a copy of it, so compare the planes, not the pointer. */
    if (!screen->RastPort.BitMap || screen->RastPort.BitMap->Planes[0] != screen->BitMap.Planes[0]) {
        print("FAIL: RastPort.BitMap not connected\n");
        errors++;
    } else {
        print("OK: RastPort.BitMap connected\n");
    }

    /* Verify ViewPort dimensions */
    if (screen->ViewPort.DWidth != 320) {
        print("FAIL: ViewPort.DWidth != 320\n");
        errors++;
    } else {
        print("OK: ViewPort.DWidth = 320\n");
    }

    if (screen->ViewPort.DHeight != 200) {
        print("FAIL: ViewPort.DHeight != 200\n");
        errors++;
    } else {
        print("OK: ViewPort.DHeight = 200\n");
    }

    /* Test drawing to the screen */
    SetAPen(&screen->RastPort, 1);
    RectFill(&screen->RastPort, 10, 10, 50, 50);
    
    /* Verify pixel was set */
    if (ReadPixel(&screen->RastPort, 30, 30) != 1) {
        print("FAIL: Drawing to screen failed\n");
        errors++;
    } else {
        print("OK: Drawing to screen works\n");
    }

    /* CloseScreen() must refuse to close screens with open windows */
    {
        struct NewWindow nw = {0};
        nw.LeftEdge = 10;
        nw.TopEdge = 10;
        nw.Width = 120;
        nw.Height = 60;
        nw.DetailPen = (UBYTE)-1;
        nw.BlockPen = (UBYTE)-1;
        nw.Flags = WFLG_BORDERLESS;
        nw.Title = (UBYTE *)"CloseScreen guard";
        nw.Screen = screen;
        nw.Type = CUSTOMSCREEN;

        window = OpenWindow(&nw);
        if (!window) {
            print("FAIL: OpenWindow() for CloseScreen test returned NULL\n");
            errors++;
        } else if (CloseScreen(screen) != FALSE) {
            print("FAIL: CloseScreen() should fail while a window is open\n");
            errors++;
        } else {
            print("OK: CloseScreen() refuses screens with open windows\n");
        }
    }

    if (window) {
        CloseWindow(window);
        window = NULL;
    }

    /* Public screen helpers.  Workbench is open and is the default public
     * screen; a CUSTOMSCREEN without SA_PubName is not public. */
    default_name[0] = '\0';
    GetDefaultPubScreen(default_name);
    print("GetDefaultPubScreen() = ");
    print(default_name);
    print("\n");

    locked_screen = LockPubScreen(NULL);
    if (!locked_screen || locked_screen == screen) {
        print("FAIL: LockPubScreen(NULL) did not return the default public screen\n");
        errors++;
    } else {
        print("OK: LockPubScreen(NULL) returns the default public screen\n");
    }
    if (locked_screen) {
        UnlockPubScreen(NULL, locked_screen);
        locked_screen = NULL;
    }

    {
        static UBYTE psname[] = "ScreenBasicPub";
        struct TagItem ptags[] = {
            { SA_Width, 320 },
            { SA_Height, 200 },
            { SA_Depth, 2 },
            { SA_Title, (ULONG)"Public screen" },
            { SA_PubName, (ULONG)psname },
            { TAG_DONE, 0 }
        };
        struct Screen *ps = OpenScreenTagList(NULL, ptags);
        if (!ps) {
            print("FAIL: OpenScreenTagList(SA_PubName) returned NULL\n");
            errors++;
        } else {
            if (LockPubScreen(psname) != NULL) {
                print("FAIL: SA_PubName screen is public before PubScreenStatus()\n");
                errors++;
            } else {
                print("OK: SA_PubName screen starts private\n");
            }
            if (PubScreenStatus(ps, 0) != PSNF_PRIVATE) {
                print("FAIL: PubScreenStatus(0) did not report prior private state\n");
                errors++;
            } else {
                print("OK: PubScreenStatus(0) reports prior private state\n");
            }
            locked_screen = LockPubScreen(psname);
            if (locked_screen != ps) {
                print("FAIL: LockPubScreen() did not return the public screen\n");
                errors++;
            } else if (CloseScreen(ps) != FALSE) {
                print("FAIL: CloseScreen() should fail while the screen is locked\n");
                errors++;
                ps = NULL;
            } else {
                print("OK: LockPubScreen() prevents CloseScreen()\n");
            }

            pub_list = LockPubScreenList();
            if (!pub_list) {
                print("FAIL: LockPubScreenList() returned NULL\n");
                errors++;
            } else {
                int found = 0;
                for (pub_node = (struct PubScreenNode *)pub_list->lh_Head;
                     pub_node->psn_Node.ln_Succ;
                     pub_node = (struct PubScreenNode *)pub_node->psn_Node.ln_Succ) {
                    if (pub_node->psn_Screen == ps)
                        found = 1;
                }
                UnlockPubScreenList();
                if (!found) {
                    print("FAIL: LockPubScreenList() did not expose the public screen node\n");
                    errors++;
                } else {
                    print("OK: LockPubScreenList() exposes the public screen node\n");
                }
            }

            if (ps && PubScreenStatus(ps, PSNF_PRIVATE) != 0) {
                print("FAIL: PubScreenStatus() privatized a locked screen\n");
                errors++;
            } else {
                print("OK: PubScreenStatus() refuses to privatize a locked screen\n");
            }

            if (locked_screen) {
                UnlockPubScreen(NULL, locked_screen);
                locked_screen = NULL;
            }

            if (ps) {
                ULONG r = PubScreenStatus(ps, PSNF_PRIVATE);
                if (!(r & 1)) {
                    print("FAIL: PubScreenStatus() could not privatize an unlocked screen\n");
                    errors++;
                } else if (LockPubScreen(psname) != NULL) {
                    print("FAIL: LockPubScreen() should fail for a private screen\n");
                    errors++;
                } else {
                    print("OK: PubScreenStatus() can privatize the screen\n");
                }
                if (!CloseScreen(ps)) {
                    print("FAIL: CloseScreen(public screen) failed\n");
                    errors++;
                }
            }
        }
    }

    ShowTitle(screen, FALSE);
    if (screen->Flags & SHOWTITLE) {
        print("FAIL: ShowTitle(FALSE) did not clear SHOWTITLE\n");
        errors++;
    } else {
        print("OK: ShowTitle(FALSE) clears SHOWTITLE\n");
    }

    ShowTitle(screen, TRUE);
    if (!(screen->Flags & SHOWTITLE)) {
        print("FAIL: ShowTitle(TRUE) did not set SHOWTITLE\n");
        errors++;
    } else {
        print("OK: ShowTitle(TRUE) sets SHOWTITLE\n");
    }

    /* OpenScreenTagList() must honor tag overrides and allow clearing flags */
    {
        static UBYTE tag_title[] = "Tagged Screen";
        struct NewScreen base = {0};
        struct TagItem tags[] = {
            { SA_Width, 640 },
            { SA_Height, 256 },
            { SA_Depth, 3 },
            { SA_Title, (ULONG)tag_title },
            { SA_Behind, FALSE },
            { SA_Quiet, FALSE },
            { SA_ShowTitle, FALSE },
            { SA_AutoScroll, FALSE },
            { TAG_DONE, 0 }
        };

        base.Width = 160;
        base.Height = 100;
        base.Depth = 1;
        base.DetailPen = 0;
        base.BlockPen = 1;
        base.Type = CUSTOMSCREEN | SCREENBEHIND | SCREENQUIET | SHOWTITLE | AUTOSCROLL;
        base.DefaultTitle = (UBYTE *)"Base Screen";

        tagged_screen = OpenScreenTagList(&base, tags);
        if (!tagged_screen) {
            print("FAIL: OpenScreenTagList() returned NULL\n");
            errors++;
        } else {
            if (tagged_screen->Width != 640 || tagged_screen->Height != 256 || tagged_screen->BitMap.Depth != 3) {
                print("FAIL: OpenScreenTagList() did not apply size/depth tags\n");
                errors++;
            } else if (tagged_screen->Title != tag_title) {
                print("FAIL: OpenScreenTagList() did not apply title tag\n");
                errors++;
            } else if ((tagged_screen->Flags & (SCREENBEHIND | SCREENQUIET | SHOWTITLE | AUTOSCROLL)) != 0) {
                print("FAIL: OpenScreenTagList() did not clear boolean tags\n");
                errors++;
            } else {
                print("OK: OpenScreenTagList() applies and clears screen tags\n");
            }
        }
    }

    if (tagged_screen) {
        if (!CloseScreen(tagged_screen)) {
            print("FAIL: CloseScreen(tagged_screen) failed\n");
            errors++;
        }
        tagged_screen = NULL;
    }

    /* OpenScreenTagList() also supports a NULL NewScreen with tag-only defaults */
    {
        struct TagItem tags[] = {
            { SA_Width, 320 },
            { SA_Height, 40 },
            { SA_Depth, 2 },
            { TAG_DONE, 0 }
        };

        default_screen = OpenScreenTagList(NULL, tags);
        if (!default_screen) {
            print("FAIL: OpenScreenTagList(NULL, tags) returned NULL\n");
            errors++;
        } else if (print_dims(default_screen), 0) {
            errors++;
        } else {
            print("OK: OpenScreenTagList(NULL, tags) uses defaults and height expansion\n");
        }
    }

    if (default_screen) {
        if (!CloseScreen(default_screen)) {
            print("FAIL: CloseScreen(default_screen) failed\n");
            errors++;
        }
        default_screen = NULL;
    }

    /* STDSCREENHEIGHT (-1) selects the standard height (other negative
     * values: Tests/Intuition/InvalidArgs) */
    {
        struct TagItem tags[] = {
            { SA_Width, 640 },
            { SA_Height, (ULONG)STDSCREENHEIGHT },
            { SA_Depth, 2 },
            { TAG_DONE, 0 }
        };

        default_screen = OpenScreenTagList(NULL, tags);
        if (!default_screen) {
            print("FAIL: OpenScreenTagList(NULL, STDSCREENHEIGHT) returned NULL\n");
            errors++;
        } else if (print_dims(default_screen), 0) {
            errors++;
        } else {
            print("OK: OpenScreenTagList() sanitizes negative height sentinels\n");
        }
    }

    if (default_screen) {
        if (!CloseScreen(default_screen)) {
            print("FAIL: CloseScreen(default_screen) failed after STDSCREENHEIGHT test\n");
            errors++;
        }
        default_screen = NULL;
    }

    /* SA_Pens and GetScreenDrawInfo: verify per-screen pen storage */
    {
        static UWORD custom_pens[] = { 3, 2, 1, 0, 3, 2, 1, 0, (UWORD)~0 };
        struct TagItem pen_tags[] = {
            { SA_Width, 320 },
            { SA_Height, 200 },
            { SA_Depth, 2 },
            { SA_Pens, (ULONG)custom_pens },
            { SA_Title, (ULONG)"Pen Screen" },
            { TAG_DONE, 0 }
        };
        struct Screen *pen_screen = OpenScreenTagList(NULL, pen_tags);

        if (!pen_screen) {
            print("FAIL: OpenScreenTagList(SA_Pens) returned NULL\n");
            errors++;
        } else {
            struct DrawInfo *dri = GetScreenDrawInfo(pen_screen);
            if (!dri) {
                print("FAIL: GetScreenDrawInfo() returned NULL for pen screen\n");
                errors++;
            } else {
                if (dri->dri_Version < 1) {
                    print("FAIL: DrawInfo dri_Version < 1\n");
                    errors++;
                } else {
                    print("OK: DrawInfo dri_Version >= 1\n");
                }

                if (dri->dri_NumPens < 9) {
                    print("FAIL: DrawInfo dri_NumPens < 9 for SA_Pens with 8 entries\n");
                    errors++;
                } else {
                    print("OK: DrawInfo dri_NumPens >= 9\n");
                }

                /* Verify custom pens were stored:
                 * pens[] = {3,2,1,0,3,2,1,0,~0} maps to:
                 * DETAILPEN(0)=3, BLOCKPEN(1)=2, TEXTPEN(2)=1,
                 * SHINEPEN(3)=0, SHADOWPEN(4)=3, FILLPEN(5)=2,
                 * FILLTEXTPEN(6)=1, BACKGROUNDPEN(7)=0 */
                if (dri->dri_Pens[DETAILPEN] != 3) {
                    print("FAIL: DETAILPEN should be 3\n");
                    errors++;
                } else if (dri->dri_Pens[BLOCKPEN] != 2) {
                    print("FAIL: BLOCKPEN should be 2\n");
                    errors++;
                } else if (dri->dri_Pens[TEXTPEN] != 1) {
                    print("FAIL: TEXTPEN should be 1\n");
                    errors++;
                } else if (dri->dri_Pens[SHINEPEN] != 0) {
                    print("FAIL: SHINEPEN should be 0\n");
                    errors++;
                } else if (dri->dri_Pens[SHADOWPEN] != 3) {
                    print("FAIL: SHADOWPEN should be 3\n");
                    errors++;
                } else {
                    print("OK: SA_Pens custom pen values stored correctly\n");
                }

                if (dri->dri_Depth != 2) {
                    print("FAIL: DrawInfo dri_Depth != 2\n");
                    errors++;
                } else {
                    print("OK: DrawInfo dri_Depth = 2\n");
                }

                if (!(dri->dri_Flags & DRIF_NEWLOOK)) {
                    print("FAIL: DrawInfo DRIF_NEWLOOK not set with SA_Pens\n");
                    errors++;
                } else {
                    print("OK: DrawInfo DRIF_NEWLOOK set\n");
                }

                FreeScreenDrawInfo(pen_screen, dri);
            }
            CloseScreen(pen_screen);
        }
    }

    /* GetScreenDrawInfo on a default screen (no SA_Pens) */
    {
        struct TagItem def_tags[] = {
            { SA_Width, 320 },
            { SA_Height, 200 },
            { SA_Depth, 2 },
            { SA_Title, (ULONG)"Default Pen Screen" },
            { TAG_DONE, 0 }
        };
        struct Screen *def_screen = OpenScreenTagList(NULL, def_tags);

        if (!def_screen) {
            print("FAIL: OpenScreenTagList(no SA_Pens) returned NULL\n");
            errors++;
        } else {
            struct DrawInfo *dri = GetScreenDrawInfo(def_screen);
            if (!dri) {
                print("FAIL: GetScreenDrawInfo() returned NULL for default screen\n");
                errors++;
            } else {
                if (dri->dri_NumPens < 1) {
                    print("FAIL: Default DrawInfo dri_NumPens < 1\n");
                    errors++;
                } else {
                    print("OK: Default DrawInfo has pens\n");
                }

                if (dri->dri_Font == NULL) {
                    print("FAIL: Default DrawInfo dri_Font is NULL\n");
                    errors++;
                } else {
                    print("OK: Default DrawInfo dri_Font set\n");
                }

                /* Resolution should be non-zero */
                if (dri->dri_Resolution.X == 0 || dri->dri_Resolution.Y == 0) {
                    print("FAIL: Default DrawInfo resolution has zero component\n");
                    errors++;
                } else {
                    print("OK: Default DrawInfo resolution non-zero\n");
                }

                FreeScreenDrawInfo(def_screen, dri);
            }
            CloseScreen(def_screen);
        }
    }

    /* WA_PubScreenName: open a window on a named public screen */
    {
        static UBYTE pub_name[] = "WA_PubScreenNameTest";
        struct TagItem scr_tags[] = {
            { SA_Width, 320 },
            { SA_Height, 200 },
            { SA_Depth, 2 },
            { SA_Title, (ULONG)"PubScreen for WA test" },
            { SA_PubName, (ULONG)pub_name },
            { TAG_DONE, 0 }
        };
        struct Screen *pub_scr = OpenScreenTagList(NULL, scr_tags);

        if (!pub_scr) {
            print("FAIL: OpenScreenTagList(SA_PubName) returned NULL\n");
            errors++;
        } else {
            struct TagItem pw_tags[] = {
                { WA_Width, 100 },
                { WA_Height, 50 },
                { WA_PubScreenName, (ULONG)pub_name },
                { WA_Flags, WFLG_BORDERLESS },
                { TAG_DONE, 0 }
            };
            struct Window *priv_win = OpenWindowTagList(NULL, pw_tags);
            if (priv_win) {
                print("FAIL: WA_PubScreenName opened a window on a private screen\n");
                errors++;
                CloseWindow(priv_win);
            } else {
                print("OK: WA_PubScreenName fails while the screen is private\n");
            }
            PubScreenStatus(pub_scr, 0);

            /* Open a window on it using WA_PubScreenName */
            struct TagItem win_tags[] = {
                { WA_Left, 10 },
                { WA_Top, 20 },
                { WA_Width, 100 },
                { WA_Height, 50 },
                { WA_PubScreenName, (ULONG)pub_name },
                { WA_Flags, WFLG_BORDERLESS },
                { TAG_DONE, 0 }
            };
            struct Window *pub_win = OpenWindowTagList(NULL, win_tags);

            if (!pub_win) {
                print("FAIL: OpenWindowTagList(WA_PubScreenName) returned NULL\n");
                errors++;
            } else if (pub_win->WScreen != pub_scr) {
                print("FAIL: WA_PubScreenName did not open on the named screen\n");
                errors++;
            } else {
                print("OK: WA_PubScreenName opens window on correct screen\n");
            }

            if (pub_win)
                CloseWindow(pub_win);
            CloseScreen(pub_scr);
        }
    }

    /* WA_PubScreen: open a window on a screen pointer */
    {
        struct TagItem scr_tags[] = {
            { SA_Width, 320 },
            { SA_Height, 200 },
            { SA_Depth, 2 },
            { SA_Title, (ULONG)"PubScreen for WA_PubScreen test" },
            { TAG_DONE, 0 }
        };
        struct Screen *ptr_scr = OpenScreenTagList(NULL, scr_tags);

        if (!ptr_scr) {
            print("FAIL: OpenScreenTagList for WA_PubScreen test returned NULL\n");
            errors++;
        } else {
            struct TagItem win_tags[] = {
                { WA_Left, 5 },
                { WA_Top, 15 },
                { WA_Width, 80 },
                { WA_Height, 40 },
                { WA_PubScreen, (ULONG)ptr_scr },
                { WA_Flags, WFLG_BORDERLESS },
                { TAG_DONE, 0 }
            };
            struct Window *ptr_win = OpenWindowTagList(NULL, win_tags);

            if (!ptr_win) {
                print("FAIL: OpenWindowTagList(WA_PubScreen) returned NULL\n");
                errors++;
            } else if (ptr_win->WScreen != ptr_scr) {
                print("FAIL: WA_PubScreen did not open on the specified screen\n");
                errors++;
            } else {
                print("OK: WA_PubScreen opens window on correct screen\n");
            }

            if (ptr_win)
                CloseWindow(ptr_win);
            CloseScreen(ptr_scr);
        }
    }

    /* Close the screen */
    if (!CloseScreen(screen)) {
        print("FAIL: CloseScreen() failed after windows were closed\n");
        errors++;
    } else {
        print("OK: CloseScreen() completed\n");
    }

    /* Final result */
    if (errors == 0) {
        print("PASS: screen_basic all tests passed\n");
        return 0;
    } else {
        print("FAIL: screen_basic had errors\n");
        return 20;
    }
}
