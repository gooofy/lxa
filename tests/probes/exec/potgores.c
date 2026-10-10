/*
 * Probe (Phase 237d): potgo.resource.  Fred Fish JoyDemo (AnalogJoystick,
 * disk 247) opens it and quits without it.  Prints what AllocPotBits()
 * grants for several requests and that FreePotBits() gives bits back.
 */
#include <exec/types.h>
#include <resources/potgo.h>
#include <clib/exec_protos.h>
#include <clib/potgo_protos.h>
#include <inline/exec.h>
#include <inline/potgo.h>
#include "probe.h"

extern struct ExecBase *SysBase;
struct Library *PotgoBase;

int main(void)
{
    UWORD a, b, c, d;

    PotgoBase = (struct Library *)OpenResource((STRPTR)"potgo.resource");
    P_BOOL("OpenResource(potgo.resource)", PotgoBase != NULL);
    if (!PotgoBase)
        return 0;
    a = AllocPotBits(0xf000);
    P_HEX("AllocPotBits($f000)", a);
    b = AllocPotBits(0xf000);
    P_HEX("AllocPotBits($f000) again", b);
    c = AllocPotBits(0x0f01);
    P_HEX("AllocPotBits($0f01)", c);
    d = AllocPotBits(0x00fe);
    P_HEX("AllocPotBits($00fe)", d);
    FreePotBits(a);
    FreePotBits(c);
    FreePotBits(d);
    a = AllocPotBits(0xff01);
    P_HEX("AllocPotBits($ff01) after FreePotBits", a);
    WritePotgo(0x0000, a);
    FreePotBits(a);
    return 0;
}
