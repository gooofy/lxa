/* SimpleImage.c - Adapted from RKM sample
 * Demonstrates the use of Intuition Images and DrawImage()
 */

#include <exec/types.h>
#include <intuition/intuition.h>
#include <intuition/intuitionbase.h>

#include <clib/exec_protos.h>
#include <clib/dos_protos.h>
#include <clib/intuition_protos.h>

#include <stdio.h>

struct IntuitionBase *IntuitionBase = NULL;

#define MYIMAGE_LEFT    (0)
#define MYIMAGE_TOP     (0)
#define MYIMAGE_WIDTH   (24)
#define MYIMAGE_HEIGHT  (10)
#define MYIMAGE_DEPTH   (1)

/* This is the image data.  It is a one bit-plane open rectangle which is 24
** pixels wide and 10 high.  The data is in CHIP memory for DMA access.
*/
UWORD __chip myImageData[] =
    {
    0xFFFF, 0xFF00,
    0xC000, 0x0300,
    0xC000, 0x0300,
    0xC000, 0x0300,
    0xC000, 0x0300,
    0xC000, 0x0300,
    0xC000, 0x0300,
    0xC000, 0x0300,
    0xC000, 0x0300,
    0xFFFF, 0xFF00,
    };

int main(int argc, char *argv[])
{
    struct Window *win;
    struct Image myImage;

    printf("SimpleImage: Demonstrating Intuition Image rendering\n\n");

    IntuitionBase = (struct IntuitionBase *)OpenLibrary("intuition.library", 37);
    if (IntuitionBase == NULL)
    {
        printf("SimpleImage: ERROR - Can't open intuition.library v37\n");
        return RETURN_FAIL;
    }
    printf("SimpleImage: Opened intuition.library v%d\n", IntuitionBase->LibNode.lib_Version);

    win = OpenWindowTags(NULL,
                        WA_Width,       200,
                        WA_Height,      100,
                        WA_Title,       (ULONG)"SimpleImage Demo",
                        WA_Flags,       WFLG_DRAGBAR | WFLG_DEPTHGADGET | WFLG_CLOSEGADGET,
                        WA_IDCMP,       IDCMP_CLOSEWINDOW,
                        WA_RMBTrap,     TRUE,
                        TAG_END);
    if (win == NULL)
    {
        printf("SimpleImage: ERROR - Can't open window\n");
        CloseLibrary((struct Library *)IntuitionBase);
        return RETURN_FAIL;
    }
    printf("SimpleImage: Opened window at %d,%d size %dx%d\n", 
           win->LeftEdge, win->TopEdge, win->Width, win->Height);

    /* Initialize the Image structure */
    myImage.LeftEdge    = MYIMAGE_LEFT;
    myImage.TopEdge     = MYIMAGE_TOP;
    myImage.Width       = MYIMAGE_WIDTH;
    myImage.Height      = MYIMAGE_HEIGHT;
    myImage.Depth       = MYIMAGE_DEPTH;
    myImage.ImageData   = myImageData;
    myImage.PlanePick   = 0x1;              /* use first bit-plane */
    myImage.PlaneOnOff  = 0x0;              /* clear all unused planes */
    myImage.NextImage   = NULL;

    printf("SimpleImage: Image structure initialized\n");
    printf("  Size: %dx%d, Depth: %d\n", myImage.Width, myImage.Height, myImage.Depth);
    printf("  PlanePick: 0x%02x, PlaneOnOff: 0x%02x\n", myImage.PlanePick, myImage.PlaneOnOff);

    /* Draw the 1 bit-plane image into the first bit-plane (color 1) */
    printf("\nSimpleImage: Drawing image at position (10, 20)...\n");
    DrawImage(win->RPort, &myImage, 10, 20);
    printf("SimpleImage: First DrawImage() complete\n");

    /* Draw the same image at a new location */
    printf("SimpleImage: Drawing image at position (100, 20)...\n");
    DrawImage(win->RPort, &myImage, 100, 20);
    printf("SimpleImage: Second DrawImage() complete\n");

    /* Draw a third copy to test more */
    printf("SimpleImage: Drawing image at position (50, 50)...\n");
    DrawImage(win->RPort, &myImage, 50, 50);
    printf("SimpleImage: Third DrawImage() complete\n");

    printf("\nSimpleImage: All images drawn successfully!\n");

    /* Clean up */
    printf("\nSimpleImage: Closing window...\n");
    CloseWindow(win);

    printf("SimpleImage: Closing library...\n");
    CloseLibrary((struct Library *)IntuitionBase);

    printf("\nSimpleImage: Demo complete.\n");
    return RETURN_OK;
}
