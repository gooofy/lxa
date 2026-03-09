/*
 * boopsi_ic/main.c - BOOPSI Inter-Object Communication Tests
 *
 * Tests:
 *   1. propgclass creation with PGA_Top/PGA_Visible/PGA_Total tags
 *   2. strgclass creation with STRINGA_LongVal/STRINGA_MaxChars tags
 *   3. GetAttr() verification for propgclass attributes
 *   4. GetAttr() verification for strgclass attributes
 *   5. ICA_TARGET / ICA_MAP setup and SetGadgetAttrs
 *   6. icclass creation and disposal
 *   7. modelclass creation and disposal
 *   8. Talk2Boopsi-style prop/string linking
 */

#include <exec/types.h>
#include <exec/memory.h>
#include <utility/tagitem.h>
#include <intuition/intuition.h>
#include <intuition/classes.h>
#include <intuition/classusr.h>
#include <intuition/gadgetclass.h>
#include <intuition/icclass.h>

#include <clib/exec_protos.h>
#include <clib/intuition_protos.h>
#include <clib/alib_protos.h>

#include <stdio.h>

struct Library *IntuitionBase;

/* Attribute mapping tables */
struct TagItem prop2intmap[] =
{
    {PGA_Top, STRINGA_LongVal},
    {TAG_END,}
};

struct TagItem int2propmap[] =
{
    {STRINGA_LongVal, PGA_Top},
    {TAG_END,}
};

static int test_count = 0;
static int pass_count = 0;
static int fail_count = 0;

#define TEST_START(name) \
    do { test_count++; printf("--- Test %d: %s\n", test_count, name); } while(0)

#define TEST_PASS(msg) \
    do { pass_count++; printf("PASS: %s\n", msg); } while(0)

#define TEST_FAIL(msg) \
    do { fail_count++; printf("FAIL: %s\n", msg); } while(0)

#define TEST_CHECK(cond, pass_msg, fail_msg) \
    do { if (cond) { TEST_PASS(pass_msg); } else { TEST_FAIL(fail_msg); } } while(0)

static void test_propgclass_creation(void)
{
    Object *prop;
    ULONG val = 0;

    TEST_START("propgclass creation with PGA tags");

    prop = NewObject(NULL, (CONST_STRPTR)"propgclass",
        GA_Left,     10,
        GA_Top,      20,
        GA_Width,    15,
        GA_Height,   80,
        GA_ID,       1,
        PGA_Total,   100,
        PGA_Top,     25,
        PGA_Visible, 10,
        PGA_NewLook, TRUE,
        TAG_END);

    TEST_CHECK(prop != NULL, "propgclass object created", "propgclass creation failed");
    if (!prop) return;

    /* Verify PGA_Top */
    if (GetAttr(PGA_Top, prop, &val))
    {
        printf("  PGA_Top = %lu (expected 25)\n", val);
        TEST_CHECK(val == 25, "PGA_Top correct", "PGA_Top incorrect");
    }
    else
    {
        TEST_FAIL("GetAttr(PGA_Top) failed");
    }

    /* Verify PGA_Visible */
    val = 0;
    if (GetAttr(PGA_Visible, prop, &val))
    {
        printf("  PGA_Visible = %lu (expected 10)\n", val);
        TEST_CHECK(val == 10, "PGA_Visible correct", "PGA_Visible incorrect");
    }
    else
    {
        TEST_FAIL("GetAttr(PGA_Visible) failed");
    }

    /* Verify PGA_Total */
    val = 0;
    if (GetAttr(PGA_Total, prop, &val))
    {
        printf("  PGA_Total = %lu (expected 100)\n", val);
        TEST_CHECK(val == 100, "PGA_Total correct", "PGA_Total incorrect");
    }
    else
    {
        TEST_FAIL("GetAttr(PGA_Total) failed");
    }

    /* Verify GA_ID */
    val = 0;
    if (GetAttr(GA_ID, prop, &val))
    {
        printf("  GA_ID = %lu (expected 1)\n", val);
        TEST_CHECK(val == 1, "GA_ID correct", "GA_ID incorrect");
    }
    else
    {
        TEST_FAIL("GetAttr(GA_ID) failed");
    }

    DisposeObject(prop);
    TEST_PASS("propgclass disposed");
}

static void test_strgclass_creation(void)
{
    Object *str;
    ULONG val = 0;

    TEST_START("strgclass creation with STRINGA tags");

    str = NewObject(NULL, (CONST_STRPTR)"strgclass",
        GA_Left,           50,
        GA_Top,            50,
        GA_Width,          100,
        GA_Height,         15,
        GA_ID,             2,
        STRINGA_LongVal,   42,
        STRINGA_MaxChars,  10,
        TAG_END);

    TEST_CHECK(str != NULL, "strgclass object created", "strgclass creation failed");
    if (!str) return;

    /* Verify STRINGA_LongVal */
    if (GetAttr(STRINGA_LongVal, str, &val))
    {
        printf("  STRINGA_LongVal = %lu (expected 42)\n", val);
        TEST_CHECK(val == 42, "STRINGA_LongVal correct", "STRINGA_LongVal incorrect");
    }
    else
    {
        TEST_FAIL("GetAttr(STRINGA_LongVal) failed");
    }

    /* Verify STRINGA_TextVal */
    val = 0;
    if (GetAttr(STRINGA_TextVal, str, &val))
    {
        printf("  STRINGA_TextVal = '%s' (expected '42')\n", val ? (char *)val : "(null)");
        TEST_PASS("GetAttr(STRINGA_TextVal) returned");
    }
    else
    {
        TEST_FAIL("GetAttr(STRINGA_TextVal) failed");
    }

    /* Verify GA_ID */
    val = 0;
    if (GetAttr(GA_ID, str, &val))
    {
        printf("  GA_ID = %lu (expected 2)\n", val);
        TEST_CHECK(val == 2, "GA_ID correct", "GA_ID incorrect");
    }
    else
    {
        TEST_FAIL("GetAttr(GA_ID) failed");
    }

    DisposeObject(str);
    TEST_PASS("strgclass disposed");
}

static void test_icclass(void)
{
    Object *ic;

    TEST_START("icclass creation and disposal");

    ic = NewObject(NULL, (CONST_STRPTR)"icclass",
        ICA_TARGET, NULL,
        TAG_END);

    TEST_CHECK(ic != NULL, "icclass object created", "icclass creation failed");
    if (!ic) return;

    DisposeObject(ic);
    TEST_PASS("icclass disposed");
}

static void test_modelclass(void)
{
    Object *model;

    TEST_START("modelclass creation and disposal");

    model = NewObject(NULL, (CONST_STRPTR)"modelclass",
        TAG_END);

    TEST_CHECK(model != NULL, "modelclass object created", "modelclass creation failed");
    if (!model) return;

    DisposeObject(model);
    TEST_PASS("modelclass disposed");
}

static void test_ica_target_setup(void)
{
    struct Window *w;
    struct Gadget *prop, *integer;

    TEST_START("ICA_TARGET/ICA_MAP prop-string linking");

    /* Open a test window */
    w = OpenWindowTags(NULL,
        WA_Width,    300,
        WA_Height,   150,
        WA_Flags,    WFLG_CLOSEGADGET | WFLG_DRAGBAR,
        WA_IDCMP,    IDCMP_CLOSEWINDOW,
        WA_Title,    (ULONG)"BOOPSI IC Test",
        TAG_END);

    TEST_CHECK(w != NULL, "Window opened", "Window open failed");
    if (!w) return;

    /* Create prop gadget with mapping to string */
    prop = (struct Gadget *)NewObject(NULL, (CONST_STRPTR)"propgclass",
        GA_ID,       1,
        GA_Top,      w->BorderTop + 5,
        GA_Left,     w->BorderLeft + 5,
        GA_Width,    15,
        GA_Height,   80,
        ICA_MAP,     prop2intmap,
        PGA_Total,   100,
        PGA_Top,     25,
        PGA_Visible, 10,
        PGA_NewLook, TRUE,
        TAG_END);

    TEST_CHECK(prop != NULL, "propgclass with ICA_MAP created", "propgclass creation failed");
    if (!prop)
    {
        CloseWindow(w);
        return;
    }

    /* Create string gadget targeting prop */
    integer = (struct Gadget *)NewObject(NULL, (CONST_STRPTR)"strgclass",
        GA_ID,             2,
        GA_Top,            w->BorderTop + 5,
        GA_Left,           w->BorderLeft + 30,
        GA_Width,          60,
        GA_Height,         18,
        ICA_MAP,           int2propmap,
        ICA_TARGET,        prop,
        GA_Previous,       prop,
        STRINGA_LongVal,   25,
        STRINGA_MaxChars,  5,
        TAG_END);

    TEST_CHECK(integer != NULL, "strgclass with ICA_TARGET created",
               "strgclass creation failed");
    if (!integer)
    {
        DisposeObject(prop);
        CloseWindow(w);
        return;
    }

    /* Connect prop -> integer via SetGadgetAttrs */
    SetGadgetAttrs(prop, w, NULL,
        ICA_TARGET, (ULONG)integer,
        TAG_END);
    TEST_PASS("SetGadgetAttrs(ICA_TARGET) called successfully");

    /* Add gadgets to window */
    AddGList(w, prop, -1, -1, NULL);
    RefreshGList(prop, w, NULL, -1);
    TEST_PASS("Gadgets added to window and refreshed");

    /* Verify prop attributes are still intact */
    {
        ULONG val = 0;
        if (GetAttr(PGA_Top, (Object *)prop, &val))
        {
            printf("  PGA_Top after AddGList = %lu (expected 25)\n", val);
            TEST_CHECK(val == 25, "PGA_Top preserved after AddGList",
                       "PGA_Top changed unexpectedly");
        }
    }

    /* Clean up */
    RemoveGList(w, prop, -1);
    TEST_PASS("Gadgets removed from window");

    DisposeObject(integer);
    TEST_PASS("strgclass disposed");
    DisposeObject(prop);
    TEST_PASS("propgclass disposed");

    CloseWindow(w);
    TEST_PASS("Window closed");
}

static void test_dogadgetmethod_and_setgadgetattrs(void)
{
    struct Window *w;
    struct Gadget *prop;
    struct opGet opg;
    ULONG value = 0;

    TEST_START("SetGadgetAttrs and DoGadgetMethodA dispatch through gadget class");

    w = OpenWindowTags(NULL,
        WA_Width,  220,
        WA_Height, 120,
        WA_Flags,  WFLG_CLOSEGADGET | WFLG_DRAGBAR,
        WA_Title,  (ULONG)"BOOPSI Gadget Method Test",
        TAG_END);

    TEST_CHECK(w != NULL, "Window opened", "Window open failed");
    if (!w) return;

    prop = (struct Gadget *)NewObject(NULL, (CONST_STRPTR)"propgclass",
        GA_ID,       7,
        GA_Left,     12,
        GA_Top,      12,
        GA_Width,    18,
        GA_Height,   70,
        PGA_Total,   100,
        PGA_Top,     10,
        PGA_Visible, 20,
        PGA_Freedom, FREEVERT,
        TAG_END);

    TEST_CHECK(prop != NULL, "propgclass object created", "propgclass creation failed");
    if (!prop)
    {
        CloseWindow(w);
        return;
    }

    AddGList(w, prop, (UWORD)~0, 1, NULL);
    RefreshGList(prop, w, NULL, 1);
    TEST_PASS("propgclass added to window");

    if (!SetGadgetAttrs(prop, w, NULL,
            GA_Disabled, TRUE,
            PGA_Top, 33,
            TAG_END))
    {
        TEST_FAIL("SetGadgetAttrs did not report a change");
    }
    else if (!(prop->Flags & GFLG_DISABLED))
    {
        TEST_FAIL("SetGadgetAttrs did not update GA_Disabled");
    }
    else if (!GetAttr(PGA_Top, (Object *)prop, &value) || value != 33)
    {
        TEST_FAIL("SetGadgetAttrs did not update PGA_Top");
    }
    else
    {
        TEST_PASS("SetGadgetAttrs updated gadget and class attributes");
    }

    opg.MethodID = OM_GET;
    opg.opg_AttrID = PGA_Top;
    opg.opg_Storage = &value;
    value = 0;
    if (!DoGadgetMethodA(prop, w, NULL, (Msg)&opg))
    {
        TEST_FAIL("DoGadgetMethodA(OM_GET) failed");
    }
    else if (value != 33)
    {
        TEST_FAIL("DoGadgetMethodA(OM_GET) returned wrong PGA_Top");
    }
    else
    {
        TEST_PASS("DoGadgetMethodA dispatched OM_GET successfully");
    }

    RemoveGList(w, prop, -1);
    DisposeObject(prop);
    TEST_PASS("propgclass disposed");
    CloseWindow(w);
    TEST_PASS("Window closed");
}

int main(void)
{
    printf("BOOPSI IC Test Suite\n");
    printf("====================\n\n");

    IntuitionBase = OpenLibrary((CONST_STRPTR)"intuition.library", 39);
    if (!IntuitionBase)
    {
        printf("FAIL: Failed to open intuition.library v39\n");
        return 1;
    }
    printf("Opened intuition.library v%d\n\n", (int)IntuitionBase->lib_Version);

    test_propgclass_creation();
    printf("\n");
    test_strgclass_creation();
    printf("\n");
    test_icclass();
    printf("\n");
    test_modelclass();
    printf("\n");
    test_ica_target_setup();
    printf("\n");
    test_dogadgetmethod_and_setgadgetattrs();
    printf("\n");

    printf("====================\n");
    printf("Results: %d tests, %d passed, %d failed\n", test_count, pass_count, fail_count);
    if (fail_count == 0)
        printf("All Tests Completed Successfully!\n");
    else
        printf("SOME TESTS FAILED!\n");

    CloseLibrary(IntuitionBase);
    return fail_count > 0 ? 1 : 0;
}
