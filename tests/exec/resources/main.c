/*
 * Test: exec/resources
 *
 * Phase 31: Extended CPU & Hardware Support
 *
 * Tests:
 * 1. OpenResource() functionality
 * 2. CIA resource availability (ciaa.resource, ciab.resource)
 * 3. blitter.resource availability
 * 4. AddResource()/RemResource() lifecycle
 */

#include <exec/types.h>
#include <exec/execbase.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <clib/exec_protos.h>
#include <clib/dos_protos.h>
#include <inline/exec.h>
#include <inline/dos.h>

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

    /* ========== Test 5: OpenResource() for blitter.resource ========== */
    print("\n--- Test 5: OpenResource() for blitter.resource ---\n");

    if (OpenResource((CONST_STRPTR)"blitter.resource") == NULL) {
        print("FAIL: OpenResource(\"blitter.resource\") returned NULL\n");
        errors++;
    } else {
        print("OK: blitter.resource opened successfully\n");
    }

    /* ========== Test 6: AddResource()/OpenResource()/RemResource() ========== */
    print("\n--- Test 6: AddResource/RemResource lifecycle ---\n");

    customResource.ln_Name = (char *)"test.resource";
    customResource.ln_Pri = 7;
    customResource.ln_Succ = NULL;
    customResource.ln_Pred = NULL;
    customResource.ln_Type = 0;

    AddResource(&customResource);

    if (customResource.ln_Type != NT_RESOURCE) {
        print("FAIL: AddResource() did not set NT_RESOURCE type\n");
        errors++;
    } else {
        print("OK: AddResource() set NT_RESOURCE type\n");
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
