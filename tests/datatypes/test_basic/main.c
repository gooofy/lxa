/*
 * Test basic datatypes.library functionality.
 *
 * Expected values come from AmigaOS 3.1 (datatypes.library 40.6 with the
 * Workbench 3.1 DEVS:DataTypes descriptors), Phase 220:
 *   - ObtainDataTypeA() classifies IFF files by FORM type, plain files as the
 *     built-in "binary" type and directories as "directory";
 *   - DTST_RAM cannot be examined (NULL, ERROR_NOT_IMPLEMENTED);
 *   - NewDTObjectA() fails with DTERROR_COULDNT_OPEN / DTERROR_UNKNOWN_DATATYPE;
 *   - a DTST_RAM picture object has no DTA_DataType and keeps scroll attributes.
 * Creating objects from real picture files needs the picture/ilbm classes
 * (roadmap Phase 253) and is not covered here.
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

static LONG errors = 0;

static void check(BOOL ok, const char *msg)
{
    Printf((STRPTR)"%s: %s\n", (STRPTR)(ok ? "PASS" : "FAIL"), (STRPTR)msg);
    if (!ok)
        errors++;
}

static void id_string(ULONG id, char *buf)
{
    buf[0] = (char)(id >> 24);
    buf[1] = (char)(id >> 16);
    buf[2] = (char)(id >> 8);
    buf[3] = (char)id;
    buf[4] = '\0';
}

static void show_header(struct DataType *dt)
{
    struct DataTypeHeader *h = dt->dtn_Header;
    char group[5], id[5];

    id_string(h->dth_GroupID, group);
    id_string(h->dth_ID, id);
    Printf((STRPTR)"  Name=%s BaseName=%s Pattern=%s\n",
           h->dth_Name ? h->dth_Name : (STRPTR)"(null)",
           h->dth_BaseName ? h->dth_BaseName : (STRPTR)"(null)",
           h->dth_Pattern ? h->dth_Pattern : (STRPTR)"(null)");
    Printf((STRPTR)"  GroupID=%s ID=%s Flags=0x%04lx Priority=%ld MaskLen=%ld\n",
           (STRPTR)group, (STRPTR)id, (ULONG)h->dth_Flags, (LONG)h->dth_Priority, (LONG)h->dth_MaskLen);
}

static void write_file(const char *name, const UBYTE *data, LONG len)
{
    BPTR fh = Open((STRPTR)name, MODE_NEWFILE);
    if (fh)
    {
        Write(fh, (APTR)data, len);
        Close(fh);
    }
}

static void obtain_file(const char *name)
{
    BPTR lock = Lock((STRPTR)name, ACCESS_READ);
    struct DataType *dt;

    Printf((STRPTR)"\nObtainDataTypeA(DTST_FILE, %s)\n", (STRPTR)name);
    if (!lock)
    {
        check(FALSE, "Lock");
        return;
    }
    dt = ObtainDataTypeA(DTST_FILE, (APTR)lock, NULL);
    check(dt != NULL, "ObtainDataTypeA succeeded");
    if (dt)
    {
        check(dt->dtn_Header != NULL, "DataType has header");
        if (dt->dtn_Header)
            show_header(dt);
        ReleaseDataType(dt);
    }
    UnLock(lock);
}

static const UBYTE ilbm_data[] = {
    'F','O','R','M', 0,0,0,48, 'I','L','B','M',
    'B','M','H','D', 0,0,0,20, 0,16,0,2, 0,0,0,0, 1,0,0,0, 0,0, 10,11, 0,16,0,2,
    'B','O','D','Y', 0,0,0,4, 0xff,0xff,0,0
};
static const UBYTE svx_data[] = {
    'F','O','R','M', 0,0,0,40, '8','S','V','X',
    'V','H','D','R', 0,0,0,20, 0,0,0,4, 0,0,0,0, 0,0,0,0, 0x1f,0x40, 1, 0, 0,1,0,0,
    'B','O','D','Y', 0,0,0,4, 1,2,3,4
};
static const UBYTE ftxt_data[] = {
    'F','O','R','M', 0,0,0,16, 'F','T','X','T',
    'C','H','R','S', 0,0,0,4, 'a','b','c','d'
};
static const UBYTE bin_data[] = {
    0x00,0x01,0x02,0x03,0xff,0xfe,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
};

int main(void)
{
    Object *obj;
    struct DataType *dt;
    LONG n;

    Printf((STRPTR)"datatypes.library basic test\n");

    DataTypesBase = OpenLibrary((STRPTR)"datatypes.library", 39);
    if (!DataTypesBase)
    {
        Printf((STRPTR)"ERROR: Could not open datatypes.library V39\n");
        return 20;
    }
    Printf((STRPTR)"PASS: Opened datatypes.library\n");

    write_file("T:dt_test.ilbm", ilbm_data, sizeof(ilbm_data));
    write_file("T:dt_test.8svx", svx_data, sizeof(svx_data));
    write_file("T:dt_test.ftxt", ftxt_data, sizeof(ftxt_data));
    write_file("T:dt_test.bin", bin_data, sizeof(bin_data));

    /* DTST_RAM cannot be examined */
    Printf((STRPTR)"\nObtainDataTypeA(DTST_RAM)\n");
    SetIoErr(0);
    dt = ObtainDataTypeA(DTST_RAM, NULL, NULL);
    check(dt == NULL, "ObtainDataTypeA(DTST_RAM) returns NULL");
    check(IoErr() == ERROR_NOT_IMPLEMENTED, "IoErr() is ERROR_NOT_IMPLEMENTED");
    if (dt)
        ReleaseDataType(dt);

    obtain_file("T:dt_test.ilbm");
    obtain_file("T:dt_test.8svx");
    obtain_file("T:dt_test.ftxt");
    obtain_file("T:dt_test.bin");
    obtain_file("T:");

    /* NewDTObjectA error paths */
    Printf((STRPTR)"\nNewDTObjectA error paths\n");
    SetIoErr(0);
    obj = NewDTObjectA((APTR)"T:dt_test.bin", NULL);
    check(obj == NULL, "NewDTObjectA(binary file) fails");
    check(IoErr() == DTERROR_UNKNOWN_DATATYPE, "IoErr() is DTERROR_UNKNOWN_DATATYPE");
    if (obj)
        DisposeDTObject(obj);
    SetIoErr(0);
    obj = NewDTObjectA((APTR)"T:dt_test_missing", NULL);
    check(obj == NULL, "NewDTObjectA(missing file) fails");
    check(IoErr() == DTERROR_COULDNT_OPEN, "IoErr() is DTERROR_COULDNT_OPEN");
    if (obj)
        DisposeDTObject(obj);

    /* DTST_RAM picture object */
    Printf((STRPTR)"\nNewDTObjectA(DTST_RAM, GID_PICTURE)\n");
    {
        struct TagItem newtags[] = {
            { DTA_SourceType, DTST_RAM },
            { DTA_GroupID, GID_PICTURE },
            { TAG_END, 0 }
        };
        obj = NewDTObjectA(NULL, newtags);
    }
    check(obj != NULL, "NewDTObjectA succeeded");
    if (obj)
    {
        ULONG topvert = 0xdead, totalvert = 0xdead;
        struct DataType *odt = (struct DataType *)1;

        {
            struct TagItem gettags[] = {
                { DTA_TopVert, (ULONG)&topvert },
                { DTA_TotalVert, (ULONG)&totalvert },
                { DTA_DataType, (ULONG)&odt },
                { TAG_END, 0 }
            };
            n = GetDTAttrsA(obj, gettags);
        }
        Printf((STRPTR)"  GetDTAttrsA returned %ld\n", n);
        Printf((STRPTR)"  TopVert: %ld TotalVert: %ld\n", topvert, totalvert);
        check(odt == NULL, "RAM object has no DTA_DataType");

        {
            struct TagItem settags[] = {
                { DTA_TopVert, 10 },
                { DTA_TotalVert, 100 },
                { TAG_END, 0 }
            };
            n = SetDTAttrsA(obj, NULL, NULL, settags);
        }
        Printf((STRPTR)"  SetDTAttrsA returned %ld\n", n);

        topvert = totalvert = 0;
        {
            struct TagItem gettags[] = {
                { DTA_TopVert, (ULONG)&topvert },
                { DTA_TotalVert, (ULONG)&totalvert },
                { TAG_END, 0 }
            };
            GetDTAttrsA(obj, gettags);
        }
        Printf((STRPTR)"  TopVert: %ld TotalVert: %ld\n", topvert, totalvert);
        check(topvert == 10 && totalvert == 100, "Set attributes verified");

        DisposeDTObject(obj);
        Printf((STRPTR)"PASS: DisposeDTObject succeeded\n");
    }

    /* Error strings */
    Printf((STRPTR)"\nGetDTString\n");
    for (n = DTERROR_UNKNOWN_DATATYPE; n <= DTERROR_INVALID_DATA; n++)
    {
        STRPTR s = GetDTString(n);
        Printf((STRPTR)"  %ld: %s\n", n, s ? s : (STRPTR)"(NULL)");
    }

    DeleteFile((STRPTR)"T:dt_test.ilbm");
    DeleteFile((STRPTR)"T:dt_test.8svx");
    DeleteFile((STRPTR)"T:dt_test.ftxt");
    DeleteFile((STRPTR)"T:dt_test.bin");

    CloseLibrary(DataTypesBase);

    if (errors)
    {
        Printf((STRPTR)"\nFAIL: %ld datatypes tests failed\n", errors);
        return 20;
    }
    Printf((STRPTR)"\nAll tests PASSED!\n");
    return 0;
}
