/*
 * Test: exec/resources
 *
 * Phase 31: Extended CPU & Hardware Support
 *
 * Tests:
 * 1. OpenResource() functionality
 * 2. CIA resource availability (ciaa.resource, ciab.resource)
 * 3. blitter.resource absence, cia.resource vectors
 * 4. AddResource()/RemResource() lifecycle
 */

#include <exec/types.h>
#include <exec/execbase.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <clib/exec_protos.h>
#include <clib/cia_protos.h>
#include <clib/dos_protos.h>
#include <inline/exec.h>
#include <inline/dos.h>

#if defined(AddICRVector)
#undef AddICRVector
#endif
#if defined(RemICRVector)
#undef RemICRVector
#endif
#if defined(AbleICR)
#undef AbleICR
#endif
#if defined(SetICR)
#undef SetICR
#endif

extern struct DosLibrary *DOSBase;
extern struct ExecBase *SysBase;

static void print(const char *s)
{
    BPTR out = Output();
    LONG len = 0;
    const char *p = s;
    while (*p++) len++;
    Write(out, (CONST APTR)s, len);
}

/* CIA interrupt handler: A1 = is_Data */
static ULONG cia_handler(register volatile ULONG *data __asm("a1"))
{
    (*data)++;
    return 0;
}

int main(void)
{
    int errors = 0;
    struct Node customResource;

    print("=== exec/resources Test ===\n\n");

    /* ========== Test 1: OpenResource() for existing resource ========== */
    print("--- Test 1: OpenResource() for ciaa.resource ---\n");
    
    APTR ciaA = OpenResource((CONST_STRPTR)"ciaa.resource");
    if (ciaA == NULL) {
        print("FAIL: OpenResource(\"ciaa.resource\") returned NULL\n");
        errors++;
    } else {
        print("OK: ciaa.resource opened successfully\n");
        
        /* Verify it's actually a resource node */
        struct Node *node = (struct Node *)ciaA;
        if (node->ln_Type != NT_RESOURCE) {
            print("FAIL: ciaa.resource has wrong node type\n");
            errors++;
        } else {
            print("OK: ciaa.resource has correct node type (NT_RESOURCE)\n");
        }
    }

    /* ========== Test 2: OpenResource() for ciab.resource ========== */
    print("\n--- Test 2: OpenResource() for ciab.resource ---\n");
    
    APTR ciaB = OpenResource((CONST_STRPTR)"ciab.resource");
    if (ciaB == NULL) {
        print("FAIL: OpenResource(\"ciab.resource\") returned NULL\n");
        errors++;
    } else {
        print("OK: ciab.resource opened successfully\n");
        
        /* Verify it's actually a resource node */
        struct Node *node = (struct Node *)ciaB;
        if (node->ln_Type != NT_RESOURCE) {
            print("FAIL: ciab.resource has wrong node type\n");
            errors++;
        } else {
            print("OK: ciab.resource has correct node type (NT_RESOURCE)\n");
        }
    }

    /* ========== Test 3: OpenResource() for non-existent resource ========== */
    print("\n--- Test 3: OpenResource() for non-existent resource ---\n");
    
    APTR fake = OpenResource((CONST_STRPTR)"fake.resource");
    if (fake != NULL) {
        print("FAIL: OpenResource(\"fake.resource\") should return NULL\n");
        errors++;
    } else {
        print("OK: OpenResource(\"fake.resource\") correctly returned NULL\n");
    }

    /* ========== Test 4: Verify resources are in ResourceList ========== */
    print("\n--- Test 4: Verify resources in ResourceList ---\n");
    
    int resourceCount = 0;
    int foundCiaA = 0;
    int foundCiaB = 0;
    
    Forbid();
    for (struct Node *node = SysBase->ResourceList.lh_Head; 
         node->ln_Succ != NULL; 
         node = node->ln_Succ) {
        resourceCount++;
        
        if (node == ciaA) foundCiaA = 1;
        if (node == ciaB) foundCiaB = 1;
    }
    Permit();
    
    if (resourceCount == 0) {
        print("FAIL: ResourceList is empty\n");
        errors++;
    } else {
        print("OK: ResourceList contains resources\n");
    }
    
    if (!foundCiaA) {
        print("FAIL: ciaa.resource not found in ResourceList\n");
        errors++;
    } else {
        print("OK: ciaa.resource found in ResourceList\n");
    }
    
    if (!foundCiaB) {
        print("FAIL: ciab.resource not found in ResourceList\n");
        errors++;
    } else {
        print("OK: ciab.resource found in ResourceList\n");
    }

    /* ========== Test 5: blitter.resource does not exist ========== */
    /* AmigaOS 3.1 has no blitter.resource (reference-verified, Phase 220). */
    print("\n--- Test 5: OpenResource() for blitter.resource ---\n");

    if (OpenResource((CONST_STRPTR)"blitter.resource") != NULL) {
        print("FAIL: OpenResource(\"blitter.resource\") should return NULL\n");
        errors++;
    } else {
        print("OK: blitter.resource does not exist\n");
    }

    /* ========== Test 5b: cia.resource callable vectors ========== */
    /* Which CIA bits are free depends on the machine (keyboard, timers):
     * claim the first free one of FLG/ALRM/TB/TA and report nothing that
     * would reveal which. */
    print("\n--- Test 5b: cia.resource callable vectors ---\n");

    if (ciaA == NULL) {
        print("FAIL: ciaa.resource unavailable for vector tests\n");
        errors++;
    } else {
        static const WORD candidates[] = { 4, 2, 1, 0 };
        struct Interrupt irq;
        volatile ULONG hits = 0;
        WORD bit = -1;
        WORD m;
        int i;

        irq.is_Node.ln_Type = NT_INTERRUPT;
        irq.is_Node.ln_Pri = 0;
        irq.is_Node.ln_Name = (char *)"test-cia";
        irq.is_Data = (APTR)&hits;
        irq.is_Code = (VOID (*)())cia_handler;

        Disable();
        for (i = 0; i < 4 && bit < 0; i++) {
            if (AddICRVector((struct Library *)ciaA, candidates[i], &irq) == NULL)
                bit = candidates[i];
        }
        if (bit >= 0)
            SetICR((struct Library *)ciaA, 1 << bit);   /* clear a stale request */
        Enable();

        if (bit < 0) {
            print("FAIL: AddICRVector() found no free CIA bit\n");
            errors++;
        } else {
            print("OK: AddICRVector() claims a free CIA bit\n");

            if (AbleICR((struct Library *)ciaA, 0) & (1 << bit))
                print("OK: AddICRVector() enables claimed bit\n");
            else {
                print("FAIL: AddICRVector() should enable claimed bit\n");
                errors++;
            }

            if (AddICRVector((struct Library *)ciaA, bit, &irq) == &irq)
                print("OK: AddICRVector() reports current owner\n");
            else {
                print("FAIL: AddICRVector() should report current owner\n");
                errors++;
            }

            m = SetICR((struct Library *)ciaA, 0x80 | (1 << bit));
            if (m & (1 << bit)) {
                print("FAIL: SetICR() reported the bit pending before it was caused\n");
                errors++;
            } else {
                print("OK: SetICR() reports previous request mask\n");
            }

            for (i = 0; i < 50 && hits == 0; i++)
                Delay(1);
            if (hits)
                print("OK: SetICR() caused the interrupt handler to run\n");
            else {
                print("FAIL: SetICR() did not run the interrupt handler\n");
                errors++;
            }

            if (SetICR((struct Library *)ciaA, 0) & (1 << bit)) {
                print("FAIL: handled CIA request is still pending\n");
                errors++;
            } else {
                print("OK: handled CIA request is no longer pending\n");
            }

            RemICRVector((struct Library *)ciaA, bit, &irq);
            if (AbleICR((struct Library *)ciaA, 0) & (1 << bit)) {
                print("FAIL: RemICRVector() should disable removed bit\n");
                errors++;
            } else {
                print("OK: RemICRVector() disables removed bit\n");
            }
        }
    }

    /* ========== Test 6: AddResource()/OpenResource()/RemResource() ========== */
    print("\n--- Test 6: AddResource/RemResource lifecycle ---\n");

    customResource.ln_Name = (char *)"test.resource";
    customResource.ln_Pri = 7;
    customResource.ln_Succ = NULL;
    customResource.ln_Pred = NULL;
    customResource.ln_Type = 0;

    AddResource(&customResource);

    /* AmigaOS 3.1 AddResource() leaves ln_Type alone (reference-verified) */
    if (customResource.ln_Type != 0) {
        print("FAIL: AddResource() changed ln_Type\n");
        errors++;
    } else {
        print("OK: AddResource() leaves ln_Type unchanged\n");
    }

    if (OpenResource((CONST_STRPTR)"test.resource") != &customResource) {
        print("FAIL: OpenResource() did not return custom resource\n");
        errors++;
    } else {
        print("OK: OpenResource() returned custom resource\n");
    }

    RemResource(&customResource);

    if (OpenResource((CONST_STRPTR)"test.resource") != NULL) {
        print("FAIL: RemResource() left resource visible\n");
        errors++;
    } else {
        print("OK: RemResource() removed resource from list\n");
    }

    /* ========== Final result ========== */
    print("\n=== Test Results ===\n");
    if (errors == 0) {
        print("PASS: All tests passed\n");
        return 0;
    } else {
        print("FAIL: Some tests failed\n");
        return 20;
    }
}
