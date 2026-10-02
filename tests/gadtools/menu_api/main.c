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

/* Item geometry; separators only report their box (lxa still represents
 * the bar as text, AmigaOS 3.1 as an Image - roadmap Phase 222). */
static void print_items(const char *what, struct MenuItem *item, struct MenuItem *bar)
{
    int n = 0;

    for (; item; item = item->NextItem, n++)
    {
        printf("%s item %d: L%d T%d W%d H%d", what, n, item->LeftEdge, item->TopEdge, item->Width, item->Height);
        if (item != bar && (item->Flags & ITEMTEXT))
        {
            struct IntuiText *it = (struct IntuiText *)item->ItemFill;
            printf(" flags 0x%04x text L%d T%d pen %d", item->Flags, it->LeftEdge, it->TopEdge, it->FrontPen);
            if (it->NextText)
                printf(" indicator L%d", it->NextText->LeftEdge);
        }
        printf("\n");
        if (item->SubItem)
            print_items("  sub", item->SubItem, NULL);
    }
}

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
        { NM_TITLE, "Broken", 0, 0, 0, 0 },
        { NM_SUB, "Orphan", 0, 0, 0, 0 },
        { NM_ITEM, "Item", 0, 0, 0, 0 },
        { NM_END, NULL, 0, 0, 0, 0 }
    };
    struct NewMenu empty_menu[] = {
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

    if (open_item->Width != 0 || open_item->Height != 0 || project->Width != 0)
        return fail("CreateMenusA should leave the geometry to the layout functions");
    itext = (struct IntuiText *)print_item->ItemFill;
    if (!itext || !itext->NextText || !itext->NextText->IText || itext->NextText->IText[0] != (UBYTE)0xbb)
        return fail("CreateMenusA did not add the sub-menu indicator to the parent label");
    printf("OK: CreateMenusA left geometry to layout and added the sub-menu indicator\n");

    if (!LayoutMenuItemsA(project->FirstItem, vi, layout_item_tags))
        return fail("LayoutMenuItemsA failed for a valid menu item chain");
    print_items("LayoutMenuItemsA", project->FirstItem, bar_item);
    if (open_item->Height != screen->Font->ta_YSize + 1 || save_item->TopEdge != open_item->Height)
        return fail("LayoutMenuItemsA did not stack items at font height + 1");
    if (open_item->Width != save_item->Width || open_item->Width != print_item->Width)
        return fail("LayoutMenuItemsA did not give all items the same width");
    if (draft_item->LeftEdge != print_item->Width - print_item->Width / 4 || draft_item->TopEdge != -1)
        return fail("LayoutMenuItemsA did not overlap the sub-menu with its parent item");
    itext = (struct IntuiText *)open_item->ItemFill;
    if (!itext || itext->FrontPen != 3 || itext->ITextFont != screen->Font)
        return fail("LayoutMenuItemsA did not apply text font/pen tags");
    printf("OK: LayoutMenuItemsA sized and annotated menu items\n");

    if (!LayoutMenusA(menus, vi, layout_menu_tags))
        return fail("LayoutMenusA failed for a valid menu strip");
    printf("menu Project L%d T%d W%d H%d, Edit L%d T%d W%d H%d\n",
        project->LeftEdge, project->TopEdge, project->Width, project->Height,
        edit->LeftEdge, edit->TopEdge, edit->Width, edit->Height);
    print_items("LayoutMenusA Project", project->FirstItem, bar_item);
    print_items("LayoutMenusA Edit", edit->FirstItem, NULL);
    if (project->Width <= 0 || edit->LeftEdge <= project->LeftEdge + project->Width)
        return fail("LayoutMenusA did not assign menu bar positions");
    if (project->Height != 0)
        return fail("LayoutMenusA should leave Menu.Height to Intuition");
    printf("OK: LayoutMenusA positioned the menu strip\n");

    FreeMenus(menus);
    menus = NULL;
    printf("OK: FreeMenus released the menu strip\n");

    /* AmigaOS 3.1 rejects a sub-item without a parent item and an empty
     * array.  (An item before the first title is not rejected there - the
     * result is undefined, so it is not tested.) */
    secondary_error = 0;
    if (CreateMenusA(invalid_menu, create_tags) != NULL)
        return fail("CreateMenusA should reject a sub-item without a parent item");
    if (secondary_error != GTMENU_INVALID)
        return fail("CreateMenusA did not report GTMENU_INVALID for an invalid menu array");
    secondary_error = 0;
    if (CreateMenusA(empty_menu, create_tags) != NULL || secondary_error != GTMENU_INVALID)
        return fail("CreateMenusA should reject an empty menu array with GTMENU_INVALID");
    printf("OK: invalid menu arrays report GTMENU_INVALID\n");

    FreeVisualInfo(vi);
    UnlockPubScreen(NULL, screen);
    CloseLibrary(GadToolsBase);
    CloseLibrary(IntuitionBase);

    printf("PASS: menu api test complete\n");
    return 0;
}
