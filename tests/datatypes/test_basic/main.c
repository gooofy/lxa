/*
 * Test basic datatypes.library functionality
 */

#include <exec/types.h>
#include <exec/memory.h>
#include <exec/libraries.h>
#include <clib/exec_protos.h>
#include <inline/exec.h>

#include <dos/dos.h>
#include <clib/dos_protos.h>
#include <inline/dos.h>

#include <datatypes/datatypes.h>
#include <datatypes/datatypesclass.h>
#include <clib/datatypes_protos.h>
#include <inline/datatypes.h>

#include <utility/tagitem.h>

extern struct Library *SysBase;
extern struct Library *DOSBase;

struct Library *DataTypesBase = NULL;

int main(void)
{
    Object *obj = NULL;
    struct DataType *dt = NULL;
    ULONG topvert = 0, totalvert = 0;
    
    Printf((STRPTR)"datatypes.library basic test\n");
    
    /* Open datatypes.library */
    DataTypesBase = OpenLibrary((STRPTR)"datatypes.library", 0);
    if (!DataTypesBase)
    {
        Printf((STRPTR)"ERROR: Could not open datatypes.library\n");
        return 20;
    }
    Printf((STRPTR)"PASS: Opened datatypes.library version %ld.%ld\n",
           DataTypesBase->lib_Version, DataTypesBase->lib_Revision);
    
    /* Test ObtainDataType */
    Printf((STRPTR)"\nTesting ObtainDataType...\n");
    dt = ObtainDataTypeA(DTST_RAM, NULL, NULL);
    if (!dt)
    {
        Printf((STRPTR)"ERROR: ObtainDataTypeA failed\n");
        CloseLibrary(DataTypesBase);
        return 20;
    }
    Printf((STRPTR)"PASS: ObtainDataTypeA succeeded, dt=0x%08lx\n", (ULONG)dt);
    
    /* Check DataType structure */
    if (!dt->dtn_Header)
    {
        Printf((STRPTR)"ERROR: DataType has no header\n");
        ReleaseDataType(dt);
        CloseLibrary(DataTypesBase);
        return 20;
    }
    Printf((STRPTR)"PASS: DataType has header\n");
    Printf((STRPTR)"  GroupID: 0x%08lx\n", dt->dtn_Header->dth_GroupID);
    PutStr((STRPTR)"  Name: ");
    PutStr(dt->dtn_Header->dth_Name ? dt->dtn_Header->dth_Name : (STRPTR)"(null)");
    PutStr((STRPTR)"\n");
    PutStr((STRPTR)"  BaseName: ");
    PutStr(dt->dtn_Header->dth_BaseName ? dt->dtn_Header->dth_BaseName : (STRPTR)"(null)");
    PutStr((STRPTR)"\n");
    
    /* Test ReleaseDataType */
    Printf((STRPTR)"\nTesting ReleaseDataType...\n");
    ReleaseDataType(dt);
    Printf((STRPTR)"PASS: ReleaseDataType succeeded\n");
    dt = NULL;
    
    /* Test NewDTObject */
    Printf((STRPTR)"\nTesting NewDTObjectA...\n");
    {
        struct TagItem newtags[] = {
            { DTA_SourceType, DTST_RAM },
            { DTA_GroupID, GID_PICTURE },
            { TAG_END, 0 }
        };
        obj = NewDTObjectA(NULL, newtags);
    }
    if (!obj)
    {
        Printf((STRPTR)"ERROR: NewDTObjectA failed\n");
        CloseLibrary(DataTypesBase);
        return 20;
    }
    Printf((STRPTR)"PASS: NewDTObjectA succeeded, obj=0x%08lx\n", (ULONG)obj);
    
    /* Test GetDTAttrs */
    Printf((STRPTR)"\nTesting GetDTAttrsA...\n");
    {
        struct TagItem gettags[] = {
            { DTA_TopVert, (ULONG)&topvert },
            { DTA_TotalVert, (ULONG)&totalvert },
            { TAG_END, 0 }
        };
        if (GetDTAttrsA(obj, gettags) < 1)
        {
            Printf((STRPTR)"ERROR: GetDTAttrsA failed\n");
            DisposeDTObject(obj);
            CloseLibrary(DataTypesBase);
            return 20;
        }
    }
    Printf((STRPTR)"PASS: GetDTAttrsA succeeded\n");
    Printf((STRPTR)"  TopVert: %ld\n", topvert);
    Printf((STRPTR)"  TotalVert: %ld\n", totalvert);
    
    /* Test SetDTAttrs */
    Printf((STRPTR)"\nTesting SetDTAttrsA...\n");
    {
        struct TagItem settags[] = {
            { DTA_TopVert, 10 },
            { DTA_TotalVert, 100 },
            { TAG_END, 0 }
        };
        SetDTAttrsA(obj, NULL, NULL, settags);
    }
    Printf((STRPTR)"PASS: SetDTAttrsA succeeded\n");
    
    /* Verify the set worked */
    topvert = 0;
    totalvert = 0;
    {
        struct TagItem gettags[] = {
            { DTA_TopVert, (ULONG)&topvert },
            { DTA_TotalVert, (ULONG)&totalvert },
            { TAG_END, 0 }
        };
        GetDTAttrsA(obj, gettags);
    }
    if (topvert != 10 || totalvert != 100)
    {
        Printf((STRPTR)"ERROR: Set attributes not reflected in Get\n");
        Printf((STRPTR)"  Expected TopVert=10, got %ld\n", topvert);
        Printf((STRPTR)"  Expected TotalVert=100, got %ld\n", totalvert);
        DisposeDTObject(obj);
        CloseLibrary(DataTypesBase);
        return 20;
    }
    Printf((STRPTR)"PASS: Set attributes verified\n");
    Printf((STRPTR)"  TopVert: %ld (expected 10)\n", topvert);
    Printf((STRPTR)"  TotalVert: %ld (expected 100)\n", totalvert);
    
    /* Test GetDTString */
    Printf((STRPTR)"\nTesting GetDTString...\n");
    {
        STRPTR errstr = GetDTString(DTERROR_UNKNOWN_DATATYPE);
        if (!errstr)
        {
            Printf((STRPTR)"ERROR: GetDTString returned NULL\n");
            DisposeDTObject(obj);
            CloseLibrary(DataTypesBase);
            return 20;
        }
        Printf((STRPTR)"PASS: GetDTString succeeded\n");
        PutStr((STRPTR)"  DTERROR_UNKNOWN_DATATYPE: ");
        PutStr(errstr);
        PutStr((STRPTR)"\n");
    }
    
    /* Test DisposeDTObject */
    Printf((STRPTR)"\nTesting DisposeDTObject...\n");
    DisposeDTObject(obj);
    Printf((STRPTR)"PASS: DisposeDTObject succeeded\n");
    obj = NULL;
    
    /* Close library */
    CloseLibrary(DataTypesBase);
    Printf((STRPTR)"\nAll tests PASSED!\n");
    
    return 0;
}
