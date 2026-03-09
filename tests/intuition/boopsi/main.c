#include <exec/types.h>
#include <exec/memory.h>
#include <intuition/intuition.h>
#include <intuition/classes.h>
#include <intuition/classusr.h>
#include <utility/tagitem.h>
#include <clib/exec_protos.h>
#include <clib/intuition_protos.h>
#include <clib/alib_protos.h>
#include <stdio.h>

#define TESTA_Value      (TAG_USER + 0x1000)
#define TESTA_Updates    (TAG_USER + 0x1001)
#define TESTM_GetValue   (TAG_USER + 0x1100)

struct Library *IntuitionBase;

struct TestData {
    ULONG value;
    ULONG updates;
};

static int failures = 0;

#define CHECK(cond, msg) \
    do { \
        if (cond) { \
            printf("PASS: %s\n", msg); \
        } else { \
            printf("FAIL: %s\n", msg); \
            failures++; \
        } \
    } while (0)

static ULONG call_super(struct IClass *cl, Object *obj, Msg msg)
{
    typedef ULONG (*DispatchEntry)(register struct IClass *cl __asm("a0"),
                                   register Object *obj __asm("a2"),
                                   register Msg msg __asm("a1"));
    struct IClass *super;
    DispatchEntry entry;

    super = cl->cl_Super;
    if (!super || !super->cl_Dispatcher.h_Entry)
        return 0;

    entry = (DispatchEntry)super->cl_Dispatcher.h_Entry;
    return entry(super, obj, msg);
}

static void apply_value_tags(struct TagItem *tags, struct TestData *data)
{
    if (!tags || !data)
        return;

    while (tags->ti_Tag != TAG_END) {
        if (tags->ti_Tag == TESTA_Value)
            data->value = tags->ti_Data;
        tags++;
    }
}

static ULONG base_dispatch(register struct IClass *cl __asm("a0"),
                           register Object *obj __asm("a2"),
                           register Msg msg __asm("a1"))
{
    struct TestData *data;

    data = (struct TestData *)INST_DATA(cl, obj);

    switch (msg->MethodID) {
        case OM_NEW:
            if (!call_super(cl, obj, msg))
                return 0;
            data->value = 0;
            data->updates = 0;
            apply_value_tags(((struct opSet *)msg)->ops_AttrList, data);
            return (ULONG)obj;

        case OM_SET:
            apply_value_tags(((struct opSet *)msg)->ops_AttrList, data);
            return 1;

        case OM_DISPOSE:
            return call_super(cl, obj, msg);

        case OM_UPDATE:
        case OM_NOTIFY:
            data->updates++;
            apply_value_tags(((struct opUpdate *)msg)->opu_AttrList, data);
            return 1;

        case OM_GET:
        {
            struct opGet *opg;

            opg = (struct opGet *)msg;
            if (opg->opg_AttrID == TESTA_Value) {
                *opg->opg_Storage = data->value;
                return 1;
            }
            if (opg->opg_AttrID == TESTA_Updates) {
                *opg->opg_Storage = data->updates;
                return 1;
            }
            return 0;
        }

        case TESTM_GetValue:
            return data->value;
    }

    return call_super(cl, obj, msg);
}

static ULONG sub_dispatch(register struct IClass *cl __asm("a0"),
                          register Object *obj __asm("a2"),
                          register Msg msg __asm("a1"))
{
    switch (msg->MethodID) {
        case TESTM_GetValue:
            return CoerceMethodA(cl->cl_Super, obj, msg) + 100;
    }

    return call_super(cl, obj, msg);
}

static void test_makeclass_requires_superclass(void)
{
    Class *cl;

    printf("Test: MakeClass requires valid superclass\n");
    cl = MakeClass(NULL, NULL, NULL, sizeof(struct TestData), 0);
    CHECK(cl == NULL, "MakeClass rejects missing superclass");
}

static void test_private_class_and_dispatch(void)
{
    Class *base_class;
    Class *sub_class;
    Object *obj;
    ULONG value;
    ULONG method;

    printf("Test: private classes and DoMethodA/CoerceMethodA\n");

    base_class = MakeClass(NULL, ROOTCLASS, NULL, sizeof(struct TestData), 0);
    CHECK(base_class != NULL, "MakeClass creates private rootclass subclass");
    if (!base_class)
        return;

    base_class->cl_Dispatcher.h_Entry = (ULONG (*)())base_dispatch;

    sub_class = MakeClass(NULL, NULL, base_class, 0, 0);
    CHECK(sub_class != NULL, "MakeClass creates private subclass from pointer");
    if (!sub_class) {
        FreeClass(base_class);
        return;
    }

    sub_class->cl_Dispatcher.h_Entry = (ULONG (*)())sub_dispatch;

    obj = (Object *)NewObject(sub_class, NULL, TESTA_Value, 42, TAG_END);
    CHECK(obj != NULL, "NewObject creates subclass instance");
    if (!obj) {
        FreeClass(sub_class);
        FreeClass(base_class);
        return;
    }

    value = 0;
    CHECK(GetAttr(TESTA_Value, obj, &value) && value == 42,
          "GetAttr reads object instance data");

    CHECK(SetAttrs(obj, TESTA_Value, 77, TAG_END) != 0,
          "SetAttrs updates object instance data");
    value = 0;
    CHECK(GetAttr(TESTA_Value, obj, &value) && value == 77,
          "SetAttrs changed TESTA_Value");

    method = TESTM_GetValue;
    CHECK(DoMethodA(obj, (Msg)&method) == 177,
          "DoMethodA dispatches subclass method");
    CHECK(CoerceMethodA(base_class, obj, (Msg)&method) == 77,
          "CoerceMethodA dispatches exact superclass method");

    DisposeObject(obj);
    CHECK(FreeClass(sub_class) == TRUE, "FreeClass frees private subclass");
    CHECK(FreeClass(base_class) == TRUE, "FreeClass frees private base class");
}

static void test_public_class_lifecycle(void)
{
    Class *public_class;
    Class *duplicate_class;
    Object *obj;
    ULONG value;

    printf("Test: public class AddClass/RemoveClass/FreeClass lifecycle\n");

    public_class = MakeClass((CONST_STRPTR)"lxa.test.public", ROOTCLASS, NULL,
                             sizeof(struct TestData), 0);
    CHECK(public_class != NULL, "MakeClass creates public class");
    if (!public_class)
        return;

    public_class->cl_Dispatcher.h_Entry = (ULONG (*)())base_dispatch;
    AddClass(public_class);
    CHECK((public_class->cl_Flags & CLF_INLIST) != 0, "AddClass marks class public");

    duplicate_class = MakeClass((CONST_STRPTR)"lxa.test.public", ROOTCLASS, NULL,
                                sizeof(struct TestData), 0);
    CHECK(duplicate_class == NULL, "MakeClass rejects duplicate public class ID");

    obj = (Object *)NewObject(NULL, (CONST_STRPTR)"lxa.test.public", TESTA_Value, 19, TAG_END);
    CHECK(obj != NULL, "NewObject finds public class by name");
    if (!obj) {
        FreeClass(public_class);
        return;
    }

    value = 0;
    CHECK(GetAttr(TESTA_Value, obj, &value) && value == 19,
          "public class object stores tags");

    CHECK(FreeClass(public_class) == FALSE,
          "FreeClass reports busy class when objects still exist");
    CHECK((public_class->cl_Flags & CLF_INLIST) == 0,
          "FreeClass removes busy public class from class list");
    CHECK(NewObject(NULL, (CONST_STRPTR)"lxa.test.public", TAG_END) == NULL,
          "removed public class is no longer discoverable");

    DisposeObject(obj);
    CHECK(FreeClass(public_class) == TRUE,
          "FreeClass succeeds after outstanding objects are disposed");
}

static void test_modelclass_members(void)
{
    Class *member_class;
    Object *model;
    Object *member1;
    Object *member2;
    struct opMember member_msg;
    struct TagItem update_tags[2];
    struct opUpdate update_msg;
    ULONG value;

    printf("Test: modelclass member add/remove and update broadcast\n");

    member_class = MakeClass(NULL, ROOTCLASS, NULL, sizeof(struct TestData), 0);
    CHECK(member_class != NULL, "MakeClass creates model observer class");
    if (!member_class)
        return;
    member_class->cl_Dispatcher.h_Entry = (ULONG (*)())base_dispatch;

    model = (Object *)NewObject(NULL, MODELCLASS, TAG_END);
    CHECK(model != NULL, "NewObject creates modelclass object");
    if (!model) {
        FreeClass(member_class);
        return;
    }

    member1 = (Object *)NewObject(member_class, NULL, TAG_END);
    member2 = (Object *)NewObject(member_class, NULL, TAG_END);
    CHECK(member1 != NULL && member2 != NULL, "observer members created");
    if (!member1 || !member2) {
        if (member1)
            DisposeObject(member1);
        if (member2)
            DisposeObject(member2);
        DisposeObject(model);
        FreeClass(member_class);
        return;
    }

    member_msg.MethodID = OM_ADDMEMBER;
    member_msg.opam_Object = member1;
    CHECK(DoMethodA(model, (Msg)&member_msg) == 0, "modelclass accepts first member");
    member_msg.opam_Object = member2;
    CHECK(DoMethodA(model, (Msg)&member_msg) == 0, "modelclass accepts second member");

    update_tags[0].ti_Tag = TESTA_Value;
    update_tags[0].ti_Data = 55;
    update_tags[1].ti_Tag = TAG_END;
    update_tags[1].ti_Data = 0;

    update_msg.MethodID = OM_UPDATE;
    update_msg.opu_AttrList = update_tags;
    update_msg.opu_GInfo = NULL;
    update_msg.opu_Flags = 0;
    CHECK(DoMethodA(model, (Msg)&update_msg) == 0, "modelclass broadcasts OM_UPDATE");

    value = 0;
    CHECK(GetAttr(TESTA_Value, member1, &value) && value == 55,
          "first model member receives update value");
    value = 0;
    CHECK(GetAttr(TESTA_Updates, member1, &value) && value == 1,
          "first model member tracks update count");
    value = 0;
    CHECK(GetAttr(TESTA_Value, member2, &value) && value == 55,
          "second model member receives update value");

    member_msg.MethodID = OM_REMMEMBER;
    member_msg.opam_Object = member2;
    CHECK(DoMethodA(model, (Msg)&member_msg) == 0, "modelclass removes second member");

    update_tags[0].ti_Data = 66;
    CHECK(DoMethodA(model, (Msg)&update_msg) == 0, "modelclass broadcasts after member removal");

    value = 0;
    CHECK(GetAttr(TESTA_Updates, member1, &value) && value == 2,
          "remaining member still receives later updates");
    value = 0;
    CHECK(GetAttr(TESTA_Updates, member2, &value) && value == 1,
          "removed member stops receiving updates");

    member_msg.opam_Object = member1;
    DoMethodA(model, (Msg)&member_msg);

    DisposeObject(member1);
    DisposeObject(member2);
    DisposeObject(model);
    CHECK(FreeClass(member_class) == TRUE, "FreeClass frees observer class");
}

int main(void)
{
    IntuitionBase = OpenLibrary((CONST_STRPTR)"intuition.library", 39);
    if (!IntuitionBase) {
        printf("FAIL: Failed to open intuition.library\n");
        return 1;
    }

    printf("BOOPSI class lifecycle tests\n");
    printf("===========================\n");

    test_makeclass_requires_superclass();
    test_private_class_and_dispatch();
    test_public_class_lifecycle();
    test_modelclass_members();

    CloseLibrary(IntuitionBase);

    if (failures == 0) {
        printf("All tests passed!\n");
        return 0;
    }

    printf("FAIL: %d checks failed\n", failures);
    return 1;
}
