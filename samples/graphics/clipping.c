/* Clipping.c - Adapted from RKM sample
 * Demonstrates clipping regions and layers library:
 * - NewRegion(), DisposeRegion()
 * - OrRectRegion()
 * - InstallClipRegion()
 * Modified for automated testing (no user interaction)
 */

#define INTUI_V36_NAMES_ONLY

#include <exec/types.h>
#include <intuition/intuition.h>
#include <intuition/intuitionbase.h>
#include <graphics/displayinfo.h>
#include <graphics/regions.h>

#include <clib/exec_protos.h>
#include <clib/dos_protos.h>
#include <clib/intuition_protos.h>
#include <clib/graphics_protos.h>
#include <clib/layers_protos.h>

#include <stdio.h>

#define MY_WIN_WIDTH  (300)
#define MY_WIN_HEIGHT (100)

struct IntuitionBase *IntuitionBase = NULL;
struct GfxBase *GfxBase = NULL;
struct Library *LayersBase = NULL;

/* Remove a clipping region installed, disposing of it */
void unclipWindow(struct Window *win)
{
    struct Region *old_region;
    
    if ((old_region = InstallClipRegion(win->WLayer, NULL)) != NULL)
    {
        DisposeRegion(old_region);
    }
}

/* Clip a window to a specified rectangle */
struct Region *clipWindow(struct Window *win,
    LONG minX, LONG minY, LONG maxX, LONG maxY)
{
    struct Region *new_region;
    struct Rectangle my_rectangle;
    
    /* Set up the limits for the clip */
    my_rectangle.MinX = minX;
    my_rectangle.MinY = minY;
    my_rectangle.MaxX = maxX;
    my_rectangle.MaxY = maxY;
    
    /* Get a new region and OR in the limits */
    new_region = NewRegion();
    if (new_region != NULL)
    {
        if (OrRectRegion(new_region, &my_rectangle) == FALSE)
        {
            DisposeRegion(new_region);
            new_region = NULL;
        }
    }
    
    /* Install the new region, and return any existing region */
    return InstallClipRegion(win->WLayer, new_region);
}

/* Clip a window to its borders */
struct Region *clipWindowToBorders(struct Window *win)
{
    return clipWindow(win, 
        win->BorderLeft, 
        win->BorderTop,
        win->Width - win->BorderRight - 1, 
        win->Height - win->BorderBottom - 1);
}

/* Draw in window and verify clipping */
void draw_and_verify(struct Window *win, char *message, 
                     LONG expectedMinX, LONG expectedMinY, 
                     LONG expectedMaxX, LONG expectedMaxY)
{
    struct Layer *layer = win->WLayer;
    struct Region *cr;
    
    printf("  %s\n", message);
    
    /* Fill with color to show clipping area */
    SetRast(win->RPort, 3);
    
    /* Verify the clip region bounds */
    cr = layer->ClipRegion;
    if (cr)
    {
        printf("    ClipRegion: 0x%08lx\n", (ULONG)cr);
        printf("    Bounds: (%ld,%ld)-(%ld,%ld)\n", 
               (LONG)cr->bounds.MinX, (LONG)cr->bounds.MinY,
               (LONG)cr->bounds.MaxX, (LONG)cr->bounds.MaxY);
        
        /* Check if bounds match expected */
        if (cr->bounds.MinX == expectedMinX && cr->bounds.MinY == expectedMinY &&
            cr->bounds.MaxX == expectedMaxX && cr->bounds.MaxY == expectedMaxY)
        {
            printf("    Clip bounds: CORRECT\n");
        }
        else
        {
            printf("    Clip bounds: MISMATCH (expected (%ld,%ld)-(%ld,%ld))\n",
                   expectedMinX, expectedMinY, expectedMaxX, expectedMaxY);
        }
    }
    else
    {
        printf("    ClipRegion: NULL (no clipping)\n");
    }
    
    /* Clear and refresh */
    SetRast(win->RPort, 0);
    RefreshWindowFrame(win);
}

/* Test various clipping configurations */
void clip_test(struct Window *win)
{
    struct Region *old_region;
    
    printf("\nClipping: Test 1 - Window with no clipping...\n");
    draw_and_verify(win, "No user clip region installed", 0, 0, 0, 0);
    
    printf("\nClipping: Test 2 - Clip to window borders...\n");
    old_region = clipWindowToBorders(win);
    if (old_region != NULL)
        DisposeRegion(old_region);
    
    draw_and_verify(win, "Clipped to borders",
        win->BorderLeft, win->BorderTop,
        win->Width - win->BorderRight - 1, 
        win->Height - win->BorderBottom - 1);
    
    unclipWindow(win);
    printf("  Clip region removed\n");
    
    printf("\nClipping: Test 3 - Clip to custom rectangle (20,20)-(100,50)...\n");
    old_region = clipWindow(win, 20, 20, 100, 50);
    if (old_region != NULL)
        DisposeRegion(old_region);
    
    draw_and_verify(win, "Clipped to (20,20)-(100,50)", 20, 20, 100, 50);
    
    unclipWindow(win);
    printf("  Clip region removed\n");
    
    printf("\nClipping: Test 4 - Multiple rectangle regions...\n");
    {
        struct Region *region;
        struct Rectangle rect1, rect2;
        
        region = NewRegion();
        if (region)
        {
            /* First rectangle */
            rect1.MinX = 10; rect1.MinY = 10;
            rect1.MaxX = 50; rect1.MaxY = 40;
            OrRectRegion(region, &rect1);
            printf("  Added rect1: (10,10)-(50,40)\n");
            
            /* Second rectangle (OR'd in) */
            rect2.MinX = 60; rect2.MinY = 20;
            rect2.MaxX = 120; rect2.MaxY = 60;
            OrRectRegion(region, &rect2);
            printf("  Added rect2: (60,20)-(120,60)\n");
            
            /* Install */
            old_region = InstallClipRegion(win->WLayer, region);
            if (old_region != NULL)
                DisposeRegion(old_region);
            
            /* Check combined bounds */
            printf("  Combined bounds: (%d,%d)-(%d,%d)\n",
                   region->bounds.MinX, region->bounds.MinY,
                   region->bounds.MaxX, region->bounds.MaxY);
            
            /* Draw to show clipping */
            SetRast(win->RPort, 2);
            SetRast(win->RPort, 0);
            RefreshWindowFrame(win);
            
            /* Clean up */
            unclipWindow(win);
            printf("  Clip region removed\n");
        }
        else
        {
            printf("  ERROR: Failed to create region!\n");
        }
    }
}

int main(int argc, char **argv)
{
    struct Window *win;
    
    printf("Clipping: Demonstrating clipping regions\n");
    
    /* Open libraries */
    IntuitionBase = (struct IntuitionBase *)OpenLibrary("intuition.library", 37);
    if (!IntuitionBase)
    {
        printf("Clipping: ERROR - Can't open intuition.library v37\n");
        return 20;
    }
    printf("Clipping: Opened intuition.library v%d\n", IntuitionBase->LibNode.lib_Version);
    
    GfxBase = (struct GfxBase *)OpenLibrary("graphics.library", 37);
    if (!GfxBase)
    {
        printf("Clipping: ERROR - Can't open graphics.library v37\n");
        CloseLibrary((struct Library *)IntuitionBase);
        return 20;
    }
    printf("Clipping: Opened graphics.library v%d\n", GfxBase->LibNode.lib_Version);
    
    LayersBase = OpenLibrary("layers.library", 37);
    if (!LayersBase)
    {
        printf("Clipping: ERROR - Can't open layers.library v37\n");
        CloseLibrary((struct Library *)GfxBase);
        CloseLibrary((struct Library *)IntuitionBase);
        return 20;
    }
    printf("Clipping: Opened layers.library v%d\n", LayersBase->lib_Version);
    
    /* Open window */
    printf("\nClipping: Opening test window...\n");
    win = OpenWindowTags(NULL,
                        WA_Width,       MY_WIN_WIDTH,
                        WA_Height,      MY_WIN_HEIGHT,
                        WA_IDCMP,       IDCMP_CLOSEWINDOW,
                        WA_CloseGadget, TRUE,
                        WA_DragBar,     TRUE,
                        WA_Activate,    TRUE,
                        TAG_END);
    
    if (win)
    {
        printf("Clipping: Window opened at (%d,%d), size %dx%d\n",
               win->LeftEdge, win->TopEdge, win->Width, win->Height);
        printf("Clipping: Window borders: L=%d T=%d R=%d B=%d\n",
               win->BorderLeft, win->BorderTop,
               win->BorderRight, win->BorderBottom);
        printf("Clipping: Layer: 0x%08lx\n", (ULONG)win->WLayer);
        
        /* Run clipping tests */
        clip_test(win);
        
        /* Clean up */
        printf("\nClipping: Closing window...\n");
        CloseWindow(win);
    }
    else
    {
        printf("Clipping: ERROR - Can't open window!\n");
    }
    
    /* Close libraries */
    printf("Clipping: Closing libraries...\n");
    CloseLibrary(LayersBase);
    CloseLibrary((struct Library *)GfxBase);
    CloseLibrary((struct Library *)IntuitionBase);
    
    printf("\nClipping: Demo complete.\n");
    return 0;
}
