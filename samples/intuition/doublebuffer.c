/*
 * doublebuffer.c - Double-Buffered Screen Animation Sample
 *
 * This is an RKM-style sample demonstrating double-buffered screen
 * animation. It creates two bitmaps and alternates between them for
 * smooth animation without flicker.
 *
 * Key Functions Demonstrated:
 *   - AllocMem() / FreeMem() for BitMap structures
 *   - AllocRaster() / FreeRaster() for plane memory
 *   - InitBitMap() - Initialize BitMap structure
 *   - OpenScreen() with CUSTOMBITMAP flag
 *   - MakeScreen() - Build copper list for screen
 *   - RethinkDisplay() - Merge and display copper lists
 *   - SetRast() / RectFill() - Drawing primitives
 *
 * Based on RKM Intuition sample code.
 * Copyright (c) 1992 Commodore-Amiga, Inc.
 */

#include <exec/types.h>
#include <exec/memory.h>
#include <intuition/intuition.h>
#include <intuition/screens.h>

#include <clib/exec_protos.h>
#include <clib/graphics_protos.h>
#include <clib/intuition_protos.h>

#include <stdio.h>

/* Screen characteristics */
#define SCR_WIDTH  320
#define SCR_HEIGHT 200
#define SCR_DEPTH  2

struct Library *IntuitionBase = NULL;
struct Library *GfxBase = NULL;

/* Allocate bitplane memory */
static LONG setupPlanes(struct BitMap *bitMap, LONG depth, LONG width, LONG height)
{
    SHORT plane_num;
    
    for (plane_num = 0; plane_num < depth; plane_num++)
    {
        bitMap->Planes[plane_num] = (PLANEPTR)AllocRaster(width, height);
        if (bitMap->Planes[plane_num] != NULL)
        {
            BltClear(bitMap->Planes[plane_num], (width / 8) * height, 1);
        }
        else
        {
            /* Cleanup on failure */
            while (--plane_num >= 0)
            {
                if (bitMap->Planes[plane_num])
                    FreeRaster(bitMap->Planes[plane_num], width, height);
            }
            return 0;
        }
    }
    return 1;  /* Success */
}

/* Free bitplane memory */
static void freePlanes(struct BitMap *bitMap, LONG depth, LONG width, LONG height)
{
    SHORT plane_num;
    
    for (plane_num = 0; plane_num < depth; plane_num++)
    {
        if (bitMap->Planes[plane_num] != NULL)
            FreeRaster(bitMap->Planes[plane_num], width, height);
    }
}

/* Setup double-buffer bitmaps */
static struct BitMap **setupBitMaps(LONG depth, LONG width, LONG height)
{
    static struct BitMap *myBitMaps[2];
    
    myBitMaps[0] = (struct BitMap *)AllocMem(sizeof(struct BitMap), MEMF_CLEAR);
    if (myBitMaps[0] != NULL)
    {
        myBitMaps[1] = (struct BitMap *)AllocMem(sizeof(struct BitMap), MEMF_CLEAR);
        if (myBitMaps[1] != NULL)
        {
            InitBitMap(myBitMaps[0], depth, width, height);
            InitBitMap(myBitMaps[1], depth, width, height);
            
            if (setupPlanes(myBitMaps[0], depth, width, height))
            {
                if (setupPlanes(myBitMaps[1], depth, width, height))
                    return myBitMaps;
                
                freePlanes(myBitMaps[0], depth, width, height);
            }
            FreeMem(myBitMaps[1], sizeof(struct BitMap));
        }
        FreeMem(myBitMaps[0], sizeof(struct BitMap));
    }
    return NULL;
}

/* Free double-buffer bitmaps */
static void freeBitMaps(struct BitMap **myBitMaps, LONG depth, LONG width, LONG height)
{
    freePlanes(myBitMaps[0], depth, width, height);
    freePlanes(myBitMaps[1], depth, width, height);
    
    FreeMem(myBitMaps[0], sizeof(struct BitMap));
    FreeMem(myBitMaps[1], sizeof(struct BitMap));
}

/* Run the double-buffer animation */
static void runDBuff(struct Screen *screen, struct BitMap **myBitMaps)
{
    WORD ktr, xpos, ypos;
    WORD toggleFrame;
    
    printf("DoubleBuffer: Starting animation loop (20 frames)\n");
    
    toggleFrame = 0;
    SetAPen(&(screen->RastPort), 1);
    
    /* Run 20 frames for the test (original does 400) */
    for (ktr = 1; ktr <= 20; ktr++)
    {
        /* Calculate position - simple animation */
        xpos = ktr * 10;
        if ((ktr % 10) >= 5)
            ypos = 50 - ((ktr % 5) * 10);
        else
            ypos = (ktr % 5) * 10;
        
        /* Switch to the current draw buffer */
        screen->RastPort.BitMap = myBitMaps[toggleFrame];
        screen->ViewPort.RasInfo->BitMap = myBitMaps[toggleFrame];
        
        /* Clear and draw */
        SetRast(&(screen->RastPort), 0);
        RectFill(&(screen->RastPort), xpos, ypos, xpos + 50, ypos + 50);
        
        /* Update display */
        MakeScreen(screen);
        RethinkDisplay();
        
        /* Toggle frame */
        toggleFrame ^= 1;
    }
    
    printf("DoubleBuffer: Animation complete\n");
}

void main(void)
{
    struct BitMap **myBitMaps;
    struct Screen *screen;
    struct NewScreen myNewScreen;
    
    printf("DoubleBuffer: Starting double-buffered screen demo\n");
    
    IntuitionBase = OpenLibrary("intuition.library", 33L);
    if (IntuitionBase != NULL)
    {
        printf("DoubleBuffer: Opened intuition.library v%d\n",
               IntuitionBase->lib_Version);
        
        GfxBase = OpenLibrary("graphics.library", 33L);
        if (GfxBase != NULL)
        {
            printf("DoubleBuffer: Opened graphics.library v%d\n",
                   GfxBase->lib_Version);
            
            myBitMaps = setupBitMaps(SCR_DEPTH, SCR_WIDTH, SCR_HEIGHT);
            if (myBitMaps != NULL)
            {
                printf("DoubleBuffer: Created two %dx%dx%d bitmaps\n",
                       SCR_WIDTH, SCR_HEIGHT, SCR_DEPTH);
                
                /* Setup NewScreen structure */
                myNewScreen.LeftEdge = 0;
                myNewScreen.TopEdge = 0;
                myNewScreen.Width = SCR_WIDTH;
                myNewScreen.Height = SCR_HEIGHT;
                myNewScreen.Depth = SCR_DEPTH;
                myNewScreen.DetailPen = 0;
                myNewScreen.BlockPen = 1;
                myNewScreen.ViewModes = 0;  /* LORES */
                myNewScreen.Type = CUSTOMSCREEN | CUSTOMBITMAP | SCREENQUIET;
                myNewScreen.Font = NULL;
                myNewScreen.DefaultTitle = NULL;
                myNewScreen.Gadgets = NULL;
                myNewScreen.CustomBitMap = myBitMaps[0];
                
                screen = OpenScreen(&myNewScreen);
                if (screen != NULL)
                {
                    printf("DoubleBuffer: Opened screen %dx%d depth=%d\n",
                           screen->Width, screen->Height, screen->BitMap.Depth);
                    
                    /* Mark rastport as double-buffered */
                    screen->RastPort.Flags = DBUFFER;
                    
                    /* Run the animation */
                    runDBuff(screen, myBitMaps);
                    
                    CloseScreen(screen);
                    printf("DoubleBuffer: Screen closed\n");
                }
                else
                {
                    printf("DoubleBuffer: Failed to open screen\n");
                }
                
                freeBitMaps(myBitMaps, SCR_DEPTH, SCR_WIDTH, SCR_HEIGHT);
                printf("DoubleBuffer: Bitmaps freed\n");
            }
            else
            {
                printf("DoubleBuffer: Failed to allocate bitmaps\n");
            }
            
            CloseLibrary(GfxBase);
        }
        else
        {
            printf("DoubleBuffer: Failed to open graphics.library\n");
        }
        
        CloseLibrary(IntuitionBase);
    }
    else
    {
        printf("DoubleBuffer: Failed to open intuition.library v33\n");
    }
    
    printf("DoubleBuffer: Done\n");
}
