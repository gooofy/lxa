/*
 * Probe (Phase 238): intuition.library NewObject/DisposeObject calling
 * convention.  NewObject() sends OM_NEW with the *true class* in place of
 * the object; rootclass allocates the instance and returns it, the
 * subclasses initialise their data after the superclass returned the
 * object.  OM_DISPOSE travels up the chain and rootclass frees.
 * (MUI's own OM_NEW walks cl_Super from that true class - Scout.)
 */
#include <exec/memory.h>
#include <exec/execbase.h>
#include <intuition/classes.h>
#include <intuition/classusr.h>
#include <intuition/imageclass.h>
#include <intuition/gadgetclass.h>
#include <clib/exec_protos.h>
#include <clib/intuition_protos.h>
#include <inline/exec.h>
#include <inline/intuition.h>
#include "probe.h"

extern struct ExecBase *SysBase;
struct Library *IntuitionBase;

typedef ULONG (*DispFn)(register Class *cl __asm("a0"), register Object *o __asm("a2"),
                        register Msg m __asm("a1"));

struct Seen
{
    LONG new_calls;
    LONG new_obj_is_true_class;     /* obj passed with OM_NEW == true class */
    LONG new_obj_is_own_class;      /* obj passed with OM_NEW == this class */
    LONG super_ret_ok;              /* superclass returned an object */
    LONG dispose_calls;
    LONG other_calls;
};

static struct Seen s1, s2;
static Class *c1, *c2;
static Class *true_class;

static ULONG super_call(Class *cl, Object *o, Msg m)
{
    return ((DispFn)cl->cl_Super->cl_Dispatcher.h_Entry)(cl->cl_Super, o, m);
}

static ULONG dispatch(register Class *cl __asm("a0"), register Object *o __asm("a2"),
                      register Msg m __asm("a1"))
{
    struct Seen *s = (cl == c1) ? &s1 : &s2;
    ULONG r;

    switch (m->MethodID) {
    case OM_NEW:
        s->new_calls++;
        if ((Class *)o == true_class)
            s->new_obj_is_true_class++;
        if ((Class *)o == cl)
            s->new_obj_is_own_class++;
        r = super_call(cl, o, m);
        if (r) {
            s->super_ret_ok++;
            /* instance data of this class: write a marker */
            *(ULONG *)INST_DATA(cl, (Object *)r) = (cl == c1) ? 0x11111111UL : 0x22222222UL;
        }
        return r;
    case OM_DISPOSE:
        s->dispose_calls++;
        return super_call(cl, o, m);
    default:
        s->other_calls++;
        return super_call(cl, o, m);
    }
}

static void clear(void)
{
    {
        UBYTE *p = (UBYTE *)&s1;
        int i;
        for (i = 0; i < (int)sizeof(s1); i++) p[i] = 0;
        p = (UBYTE *)&s2;
        for (i = 0; i < (int)sizeof(s2); i++) p[i] = 0;
    }
}

static void seen(const char *label, struct Seen *s)
{
    probe_s(label);
    probe_s(": new=");
    probe_dec(s->new_calls);
    probe_s(" obj=trueclass ");
    probe_dec(s->new_obj_is_true_class);
    probe_s(" obj=ownclass ");
    probe_dec(s->new_obj_is_own_class);
    probe_s(" superok ");
    probe_dec(s->super_ret_ok);
    probe_s(" dispose=");
    probe_dec(s->dispose_calls);
    probe_ch('\n');
}

int main(void)
{
    Object *o;
    struct _Object *hdr;

    IntuitionBase = OpenLibrary((CONST_STRPTR)"intuition.library", 37);
    if (!IntuitionBase)
        return 20;

    P_SECTION("private classes: c1 < rootclass, c2 < c1");
    c1 = MakeClass(NULL, (ClassID)ROOTCLASS, NULL, 8, 0);
    c2 = c1 ? MakeClass(NULL, NULL, c1, 12, 0) : NULL;
    P_NULL("c1", c1);
    P_NULL("c2", c2);
    if (!c1 || !c2)
        return 20;
    c1->cl_Dispatcher.h_Entry = (ULONG (*)())dispatch;
    c2->cl_Dispatcher.h_Entry = (ULONG (*)())dispatch;
    P_LONG("c1 InstOffset", c1->cl_InstOffset);
    P_LONG("c1 InstSize", c1->cl_InstSize);
    P_LONG("c2 InstOffset", c2->cl_InstOffset);
    P_LONG("c2 InstSize", c2->cl_InstSize);

    P_SECTION("NewObject(c2)");
    clear();
    true_class = c2;
    o = NewObject(c2, NULL, TAG_DONE);
    P_NULL("object", o);
    seen("c2", &s2);
    seen("c1", &s1);
    if (o) {
        hdr = _OBJECT(o);
        P_LONG("OCLASS == c2", OCLASS(o) == c2);
        P_HEX("c1 data", *(ULONG *)INST_DATA(c1, o));
        P_HEX("c2 data", *(ULONG *)INST_DATA(c2, o));
        P_LONG("c2 ObjectCount", c2->cl_ObjectCount);
        P_LONG("c1 ObjectCount", c1->cl_ObjectCount);
        P_NULL("o_Node.mln_Succ", hdr->o_Node.mln_Succ);
    }

    P_SECTION("NewObject(c1)");
    clear();
    true_class = c1;
    {
        Object *o1 = NewObject(c1, NULL, TAG_DONE);
        P_NULL("object", o1);
        seen("c1", &s1);
        seen("c2", &s2);
        P_LONG("c1 ObjectCount", c1->cl_ObjectCount);
        clear();
        DisposeObject(o1);
        seen("dispose c1", &s1);
        P_LONG("c1 ObjectCount after", c1->cl_ObjectCount);
    }

    P_SECTION("DisposeObject(c2 object)");
    clear();
    DisposeObject(o);
    seen("c2", &s2);
    seen("c1", &s1);
    P_LONG("c2 ObjectCount", c2->cl_ObjectCount);
    P_LONG("FreeClass(c2)", FreeClass(c2));
    P_LONG("FreeClass(c1)", FreeClass(c1));

    P_SECTION("public classes: OM_NEW returns the object");
    {
        Object *img = NewObject(NULL, (ClassID)"frameiclass", IA_Width, 20, IA_Height, 10, TAG_DONE);
        Object *gad = NewObject(NULL, (ClassID)"buttongclass", GA_Width, 30, GA_Height, 12, TAG_DONE);
        Object *mod = NewObject(NULL, (ClassID)"modelclass", TAG_DONE);
        P_NULL("frameiclass", img);
        if (img) {
            P_LONG("frameiclass Width", ((struct Image *)img)->Width);
            P_LONG("frameiclass OCLASS id ok", OCLASS(img)->cl_ID && OCLASS(img)->cl_ID[0] == 'f');
        }
        P_NULL("buttongclass", gad);
        if (gad)
            P_LONG("buttongclass Width", ((struct Gadget *)gad)->Width);
        P_NULL("modelclass", mod);
        DisposeObject(img);
        DisposeObject(gad);
        DisposeObject(mod);
    }

    CloseLibrary(IntuitionBase);
    return 0;
}
