#include <exec/types.h>
#include <intuition/intuition.h>
#include <intuition/screens.h>
#include <libraries/gadtools.h>
#include <clib/exec_protos.h>
#include <clib/intuition_protos.h>
#include <clib/gadtools_protos.h>
#include <stdio.h>

struct Library *IntuitionBase = NULL;
struct Library *GadToolsBase = NULL;

static int fail(const char *message)
{
    printf("FAIL: %s\n", message);
    return 1;
}

int main(void)
{
    struct Screen *screen = NULL;
    APTR vi = NULL;
    struct Menu *menus = NULL;
    struct Menu *project;
    struct Menu *edit;
    struct MenuItem *open_item;
    struct MenuItem *save_item;
    struct MenuItem *bar_item;
    struct MenuItem *print_item;
    struct MenuItem *draft_item;
    struct IntuiText *itext;
    ULONG secondary_error = 0;
    struct TagItem create_tags[] = {
        { GTMN_SecondaryError, (ULONG)&secondary_error },
        { TAG_DONE, 0 }
    };
    struct TagItem layout_item_tags[] = {
        { GTMN_TextAttr, 0 },
        { GTMN_FrontPen, 3 },
        { TAG_DONE, 0 }
    };
    struct TagItem layout_menu_tags[] = {
        { GTMN_TextAttr, 0 },
        { GTMN_FrontPen, 3 },
        { TAG_DONE, 0 }
    };
    struct NewMenu valid_menu[] = {
        { NM_TITLE, "Project", 0, 0, 0, (APTR)0x11111111 },
        { NM_ITEM, "Open...", "O", 0, 0, (APTR)0x22222222 },
        { NM_IGNORE | NM_ITEM, "Ignored", 0, 0, 0, (APTR)0x99999999 },
        { NM_ITEM, "Save", "S", CHECKIT | CHECKED | MENUTOGGLE, 0, (APTR)0x33333333 },
        { NM_ITEM, NM_BARLABEL, 0, 0, 0, 0 },
        { NM_ITEM, "Print", 0, ITEMENABLED, 0, (APTR)0x44444444 },
        { NM_SUB, "Draft", 0, 0, 0, (APTR)0x55555555 },
        { NM_TITLE, "Edit", 0, 0, 0, (APTR)0x66666666 },
        { NM_ITEM, "Cut", "X", 0, 0, (APTR)0x77777777 },
        { NM_END, NULL, 0, 0, 0, 0 }
    };
    struct NewMenu invalid_menu[] = {
        { NM_ITEM, "Broken", 0, 0, 0, 0 },
        { NM_END, NULL, 0, 0, 0, 0 }
    };

    printf("Testing GadTools menu APIs...\n");

    IntuitionBase = OpenLibrary((CONST_STRPTR)"intuition.library", 37);
    GadToolsBase = OpenLibrary((CONST_STRPTR)"gadtools.library", 37);
    if (!IntuitionBase || !GadToolsBase)
        return fail("cannot open required libraries");

    screen = LockPubScreen(NULL);
    if (!screen)
        return fail("cannot lock public screen");

    vi = GetVisualInfoA(screen, NULL);
    if (!vi)
        return fail("GetVisualInfoA returned NULL");

    layout_item_tags[0].ti_Data = (ULONG)screen->Font;
    layout_menu_tags[0].ti_Data = (ULONG)screen->Font;

    menus = CreateMenusA(valid_menu, create_tags);
    if (!menus)
        return fail("CreateMenusA rejected a valid menu array");
    if (secondary_error != 0)
        return fail("CreateMenusA reported an unexpected secondary error");

    project = menus;
    edit = menus->NextMenu;
    if (!project || !edit || edit->NextMenu != NULL)
        return fail("CreateMenusA did not build the expected top-level menu chain");
    if (GTMENU_USERDATA(project) != (APTR)0x11111111)
        return fail("menu user data was not stored after the Menu struct");
    if (GTMENU_USERDATA(edit) != (APTR)0x66666666)
        return fail("second menu user data was not stored");

    open_item = project->FirstItem;
    save_item = open_item ? open_item->NextItem : NULL;
    bar_item = save_item ? save_item->NextItem : NULL;
    print_item = bar_item ? bar_item->NextItem : NULL;
    draft_item = print_item ? print_item->SubItem : NULL;
    if (!open_item || !save_item || !bar_item || !print_item || !draft_item)
        return fail("CreateMenusA did not build the expected item/sub-item chain");
    if (GTMENUITEM_USERDATA(open_item) != (APTR)0x22222222)
        return fail("item user data was not stored after the MenuItem struct");
    if (GTMENUITEM_USERDATA(draft_item) != (APTR)0x55555555)
        return fail("sub-item user data was not stored after the MenuItem struct");
    if (!(open_item->Flags & COMMSEQ) || open_item->Command != 'O')
        return fail("command key metadata was not copied to the menu item");
    if (!(save_item->Flags & CHECKIT) || !(save_item->Flags & CHECKED) || !(save_item->Flags & MENUTOGGLE))
        return fail("checkmark/toggle flags were not copied to the menu item");
    if (print_item->Flags & ITEMENABLED)
        return fail("NM_ITEMDISABLED item should not remain enabled");
    if (bar_item->Flags & ITEMENABLED)
        return fail("separator item should be disabled");
    printf("OK: CreateMenusA stored menu hierarchy, flags, and user data\n");

    if (!LayoutMenuItemsA(project->FirstItem, vi, layout_item_tags))
        return fail("LayoutMenuItemsA failed for a valid menu item chain");
    if (open_item->Width <= 0 || save_item->TopEdge <= open_item->TopEdge)
        return fail("LayoutMenuItemsA did not position menu items vertically");
    if (draft_item->LeftEdge != open_item->Width)
        return fail("LayoutMenuItemsA did not place sub-items to the right of their parent item");
    itext = (struct IntuiText *)open_item->ItemFill;
    if (!itext || itext->FrontPen != 3 || itext->ITextFont != screen->Font)
        return fail("LayoutMenuItemsA did not apply text font/pen tags");
    printf("OK: LayoutMenuItemsA sized and annotated menu items\n");

    if (!LayoutMenusA(menus, vi, layout_menu_tags))
        return fail("LayoutMenusA failed for a valid menu strip");
    if (project->Width <= 0 || edit->LeftEdge <= project->LeftEdge)
        return fail("LayoutMenusA did not assign menu bar positions");
    if (project->Height != screen->BarHeight + 1)
        return fail("LayoutMenusA did not use the screen bar height");
    printf("OK: LayoutMenusA positioned the menu strip\n");

    FreeMenus(menus);
    menus = NULL;
    printf("OK: FreeMenus released the menu strip\n");

    secondary_error = 0;
    if (CreateMenusA(invalid_menu, create_tags) != NULL)
        return fail("CreateMenusA should reject an item before the first title");
    if (secondary_error != GTMENU_INVALID)
        return fail("CreateMenusA did not report GTMENU_INVALID for an invalid menu array");
    printf("OK: invalid menu arrays report GTMENU_INVALID\n");

    FreeVisualInfo(vi);
    UnlockPubScreen(NULL, screen);
    CloseLibrary(GadToolsBase);
    CloseLibrary(IntuitionBase);

    printf("PASS: menu api test complete\n");
    return 0;
}
