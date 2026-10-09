/*
 * Probe (Phase 237): mouse/joystick buttons as programs read them from the
 * hardware with no button pressed: CIA-A PRA bits 6/7 (left button /
 * fire, active low) and POTINP bit 10 (right button, active low).
 * GFA-BASIC's MOUSEK (Fish A-Gene, FHSpread) polls $BFE001.
 */
#include <exec/types.h>
#include "probe.h"

static volatile UBYTE *volatile ciaa_pra;
static volatile UWORD *volatile potinp;

int main(void)
{
    ciaa_pra = (volatile UBYTE *)0xbfe001;
    potinp = (volatile UWORD *)0xdff016;

    P_SECTION("no button pressed");
    P_HEX("CIA-A PRA & 0xc0", *ciaa_pra & 0xc0);
    P_HEX("POTINP & 0x0400", *potinp & 0x0400);
    return 0;
}
