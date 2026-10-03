/*
 * Probe (Phase 222e): gadtools.library CreateGadgetA/CreateContext/
 * FreeGadgets - the gadget list every kind creates (geometry, flags,
 * GadgetType, imagery, labels, SpecialInfo) with topaz 8.
 */
#include "gtprobe.h"

int main(void)
{
    return gtprobe_main("topaz.font", 8);
}
