/*
 * iffparse.library regression test
 */

#include <exec/types.h>
#include <exec/memory.h>
#include <dos/dos.h>
#include <devices/clipboard.h>
#include <libraries/iffparse.h>
#include <clib/exec_protos.h>
#include <clib/dos_protos.h>
#include <clib/iffparse_protos.h>

#include <stdio.h>
#include <string.h>

struct Library *IFFParseBase;

#define ID_TEST MAKE_ID('T','E','S','T')
#define ID_DATA MAKE_ID('D','A','T','A')
#define ID_NAME MAKE_ID('N','A','M','E')

static const char name_data[] = "Test IFF File";
static const char name_extra[] = "ABCDWXYZ";
static const char data_data[] = "Hello, IFF World!";
static const char data_extra[] = "123456";

static LONG g_failures = 0;
static LONG g_entry_hits = 0;
static LONG g_exit_hits = 0;

static void expect_true(BOOL condition, const char *message)
{
    if (condition)
        printf("OK: %s\n", message);
    else
    {
        printf("FAIL: %s\n", message);
        g_failures++;
    }
}

static BOOL bytes_equal(const char *a, const char *b, LONG len)
{
    LONG i;

    for (i = 0; i < len; i++)
    {
        if (a[i] != b[i])
            return FALSE;
    }

    return TRUE;
}

static ULONG entry_handler(register struct Hook *hook __asm("a0"),
                          register APTR object __asm("a2"),
                          register LONG *message __asm("a1"))
{
    (void)hook;

    if (object && message && *message == IFFCMD_ENTRY)
        (*(LONG *)object)++;

    return 0;
}

static ULONG exit_handler(register struct Hook *hook __asm("a0"),
                         register APTR object __asm("a2"),
                         register LONG *message __asm("a1"))
{
    (void)hook;

    if (object && message && *message == IFFCMD_EXIT)
        (*(LONG *)object)++;

    return 0;
}

static BOOL write_test_file(struct IFFHandle *iff)
{
    BPTR file;
    LONG error;

    file = Open((CONST_STRPTR)"T:test.iff", MODE_NEWFILE);
    if (!file)
    {
        expect_true(FALSE, "Create T:test.iff");
        return FALSE;
    }

    iff->iff_Stream = (ULONG)file;
    InitIFFasDOS(iff);
    error = OpenIFF(iff, IFFF_WRITE);
    expect_true(error == 0, "OpenIFF write");
    if (error != 0)
    {
        Close(file);
        return FALSE;
    }

    error = PushChunk(iff, ID_TEST, ID_FORM, IFFSIZE_UNKNOWN);
    expect_true(error == 0, "Push FORM TEST");
    if (error == 0)
    {
        error = PushChunk(iff, 0, ID_NAME, IFFSIZE_UNKNOWN);
        expect_true(error == 0, "Push NAME");
        if (error == 0)
        {
            expect_true(WriteChunkBytes(iff, (APTR)name_data, strlen(name_data)) == (LONG)strlen(name_data),
                        "WriteChunkBytes NAME");
            expect_true(WriteChunkRecords(iff, (APTR)name_extra, 4, 2) == 2,
                        "WriteChunkRecords NAME");
            expect_true(PopChunk(iff) == 0, "PopChunk NAME");
        }

        error = PushChunk(iff, 0, ID_DATA, IFFSIZE_UNKNOWN);
        expect_true(error == 0, "Push DATA");
        if (error == 0)
        {
            expect_true(WriteChunkBytes(iff, (APTR)data_data, strlen(data_data)) == (LONG)strlen(data_data),
                        "WriteChunkBytes DATA");
            expect_true(WriteChunkRecords(iff, (APTR)data_extra, 3, 2) == 2,
                        "WriteChunkRecords DATA");
            expect_true(PopChunk(iff) == 0, "PopChunk DATA");
        }

        expect_true(PopChunk(iff) == 0, "PopChunk FORM");
    }

    CloseIFF(iff);
    Close(file);
    return TRUE;
}

static BOOL open_read_file(struct IFFHandle *iff, BPTR *file_out)
{
    LONG error;
    BPTR file;

    file = Open((CONST_STRPTR)"T:test.iff", MODE_OLDFILE);
    expect_true(file != 0, "Open T:test.iff for reading");
    if (!file)
        return FALSE;

    iff->iff_Stream = (ULONG)file;
    InitIFFasDOS(iff);
    error = OpenIFF(iff, IFFF_READ);
    expect_true(error == 0, "OpenIFF read");
    if (error != 0)
    {
        Close(file);
        return FALSE;
    }

    *file_out = file;
    return TRUE;
}

static void close_read_file(struct IFFHandle *iff, BPTR file)
{
    CloseIFF(iff);
    Close(file);
}

static void verify_basic_read(struct IFFHandle *iff)
{
    BPTR file;
    LONG error;
    LONG name_seen = 0;
    LONG data_seen = 0;

    if (!open_read_file(iff, &file))
        return;

    expect_true(CurrentChunk(NULL) == NULL, "CurrentChunk NULL");
    expect_true(CurrentChunk(iff) != NULL && CurrentChunk(iff)->cn_ID == ID_FORM,
                "CurrentChunk after OpenIFF is FORM");
    expect_true(FindPropContext(iff) != NULL, "FindPropContext inside FORM");

    while ((error = ParseIFF(iff, IFFPARSE_RAWSTEP)) == 0 || error == IFFERR_EOC)
    {
        struct ContextNode *cn;

        if (error == IFFERR_EOC)
            continue;

        cn = CurrentChunk(iff);
        if (!cn)
        {
            expect_true(FALSE, "CurrentChunk available during RAWSTEP");
            continue;
        }

        if (cn->cn_ID == ID_NAME)
        {
            char first[9];
            char rest[32];
            struct ContextNode *parent = ParentChunk(cn);

            memset(first, 0, sizeof(first));
            memset(rest, 0, sizeof(rest));
            expect_true(parent != NULL && parent->cn_ID == ID_FORM, "ParentChunk NAME -> FORM");
            expect_true(ReadChunkRecords(iff, first, 4, 2) == 2, "ReadChunkRecords NAME");
            expect_true(ReadChunkBytes(iff, rest, sizeof(rest) - 1) == 14, "ReadChunkBytes NAME remainder");
            expect_true(bytes_equal(first, name_data, 8), "NAME first records content");
            expect_true(strcmp(rest, " FileABCDWXYZ") == 0, "NAME remainder content");
            name_seen++;
        }
        else if (cn->cn_ID == ID_DATA)
        {
            char first[7];
            char rest[32];
            struct ContextNode *parent = ParentChunk(cn);

            memset(first, 0, sizeof(first));
            memset(rest, 0, sizeof(rest));
            expect_true(parent != NULL && parent->cn_ID == ID_FORM, "ParentChunk DATA -> FORM");
            expect_true(ReadChunkRecords(iff, first, 3, 2) == 2, "ReadChunkRecords DATA");
            expect_true(ReadChunkBytes(iff, rest, sizeof(rest) - 1) == 18, "ReadChunkBytes DATA remainder");
            expect_true(bytes_equal(first, data_data, 6), "DATA first records content");
            expect_true(strcmp(rest, " IFF World!123456") == 0, "DATA remainder content");
            data_seen++;
        }
    }

    expect_true(error == IFFERR_EOF, "ParseIFF RAWSTEP reaches EOF");
    expect_true(name_seen == 1, "RAWSTEP saw NAME once");
    expect_true(data_seen == 1, "RAWSTEP saw DATA once");

    close_read_file(iff, file);
}

static void verify_property_collection_handlers(struct IFFHandle *iff)
{
    BPTR file;
    LONG error;
    struct Hook entry_hook;
    struct Hook exit_hook;
    LONG prop_checked = 0;
    LONG collection_checked = 0;

    if (!open_read_file(iff, &file))
        return;

    g_entry_hits = 0;
    g_exit_hits = 0;

    entry_hook.h_Entry = (ULONG (*)())entry_handler;
    entry_hook.h_SubEntry = NULL;
    entry_hook.h_Data = NULL;
    exit_hook.h_Entry = (ULONG (*)())exit_handler;
    exit_hook.h_SubEntry = NULL;
    exit_hook.h_Data = NULL;

    expect_true(PropChunk(iff, ID_TEST, ID_NAME) == 0, "PropChunk NAME declaration");
    expect_true(CollectionChunk(iff, ID_TEST, ID_DATA) == 0, "CollectionChunk DATA declaration");
    expect_true(EntryHandler(iff, ID_TEST, ID_DATA, IFFSLI_ROOT, &entry_hook, &g_entry_hits) == 0,
                "EntryHandler DATA declaration");
    expect_true(ExitHandler(iff, ID_TEST, ID_DATA, IFFSLI_ROOT, &exit_hook, &g_exit_hits) == 0,
                "ExitHandler DATA declaration");

    while ((error = ParseIFF(iff, IFFPARSE_STEP)) == 0 || error == IFFERR_EOC)
    {
        struct ContextNode *cn;

        if (error == IFFERR_EOC)
            continue;

        cn = CurrentChunk(iff);
        if (!cn)
            continue;

        if (cn->cn_ID == ID_NAME)
        {
            struct StoredProperty *prop = FindProp(iff, ID_TEST, ID_NAME);

            expect_true(prop != NULL, "FindProp NAME while in scope");
            if (prop)
            {
                expect_true(prop->sp_Size == 22, "StoredProperty NAME size");
                expect_true(bytes_equal((const char *)prop->sp_Data, name_data, strlen(name_data)),
                            "StoredProperty NAME prefix");
                expect_true(bytes_equal((const char *)prop->sp_Data + strlen(name_data),
                                        name_extra,
                                        strlen(name_extra)),
                            "StoredProperty NAME suffix");
            }
            prop_checked = 1;
        }
        else if (cn->cn_ID == ID_DATA)
        {
            struct StoredProperty *prop = FindProp(iff, ID_TEST, ID_NAME);
            struct CollectionItem *collection = FindCollection(iff, ID_TEST, ID_DATA);

            expect_true(prop != NULL, "FindProp NAME still visible in DATA scope");
            expect_true(collection != NULL && collection->ci_Next != NULL,
                        "FindCollection DATA while in scope");
            if (collection && collection->ci_Next)
            {
                expect_true(collection->ci_Next->ci_Size == 24, "CollectionItem DATA size");
                expect_true(bytes_equal((const char *)collection->ci_Next->ci_Data,
                                        data_data,
                                        strlen(data_data)),
                            "CollectionItem DATA prefix");
                expect_true(bytes_equal((const char *)collection->ci_Next->ci_Data + strlen(data_data),
                                        data_extra,
                                        strlen(data_extra)),
                            "CollectionItem DATA suffix");
            }
            collection_checked = 1;
        }
    }

    expect_true(error == IFFERR_EOF, "ParseIFF STEP reaches EOF");
    expect_true(prop_checked == 1, "Property chunk observed");
    expect_true(collection_checked == 1, "Collection chunk observed");
    expect_true(g_entry_hits == 1, "Entry handler invoked once");
    expect_true(g_exit_hits == 1, "Exit handler invoked once");

    close_read_file(iff, file);
}

static void verify_stop_conditions(struct IFFHandle *iff)
{
    BPTR file;
    LONG error;

    if (!open_read_file(iff, &file))
        return;

    expect_true(StopChunk(iff, ID_TEST, ID_DATA) == 0, "StopChunk DATA declaration");
    error = ParseIFF(iff, IFFPARSE_SCAN);
    expect_true(error == 0, "ParseIFF SCAN stops on DATA entry");
    expect_true(CurrentChunk(iff) != NULL && CurrentChunk(iff)->cn_ID == ID_DATA,
                "CurrentChunk is DATA after StopChunk");
    close_read_file(iff, file);

    if (!open_read_file(iff, &file))
        return;

    expect_true(StopOnExit(iff, ID_TEST, ID_DATA) == 0, "StopOnExit DATA declaration");
    error = ParseIFF(iff, IFFPARSE_SCAN);
    expect_true(error == 0 || error == IFFERR_EOC, "ParseIFF SCAN stops on DATA exit");
    expect_true(CurrentChunk(iff) != NULL && CurrentChunk(iff)->cn_ID == ID_DATA,
                "CurrentChunk remains DATA at StopOnExit");
    do {
        error = ParseIFF(iff, IFFPARSE_SCAN);
    } while (error == 0 || error == IFFERR_EOC);
    expect_true(error == IFFERR_EOF || error == IFFERR_READ,
                "ParseIFF continues after StopOnExit without crashing");
    close_read_file(iff, file);
}

static void verify_clipboard_roundtrip(void)
{
    struct ClipboardHandle *clip;
    struct IFFHandle *iff;
    LONG error;

    clip = OpenClipboard(PRIMARY_CLIP);
    expect_true(clip != NULL, "OpenClipboard PRIMARY_CLIP");
    if (!clip)
        return;

    iff = AllocIFF();
    expect_true(iff != NULL, "AllocIFF for clipboard");
    if (!iff)
    {
        CloseClipboard(clip);
        return;
    }

    iff->iff_Stream = (ULONG)clip;
    InitIFFasClip(iff);
    error = OpenIFF(iff, IFFF_WRITE);
    expect_true(error == 0, "OpenIFF clipboard write");
    if (error == 0)
    {
        expect_true(PushChunk(iff, ID_TEST, ID_FORM, IFFSIZE_UNKNOWN) == 0, "Clipboard Push FORM");
        expect_true(PushChunk(iff, 0, ID_DATA, IFFSIZE_UNKNOWN) == 0, "Clipboard Push DATA");
        expect_true(WriteChunkBytes(iff, (APTR)data_data, strlen(data_data)) == (LONG)strlen(data_data),
                    "Clipboard WriteChunkBytes DATA");
        expect_true(WriteChunkRecords(iff, (APTR)data_extra, 3, 2) == 2,
                    "Clipboard WriteChunkRecords DATA");
        expect_true(PopChunk(iff) == 0, "Clipboard Pop DATA");
        expect_true(PopChunk(iff) == 0, "Clipboard Pop FORM");
        CloseIFF(iff);
    }

    iff->iff_Stream = (ULONG)clip;
    InitIFFasClip(iff);
    error = OpenIFF(iff, IFFF_READ);
    expect_true(error == 0, "OpenIFF clipboard read");
    if (error == 0)
    {
        char buffer[32];

        expect_true(StopChunk(iff, ID_TEST, ID_DATA) == 0, "Clipboard StopChunk DATA");
        error = ParseIFF(iff, IFFPARSE_SCAN);
        expect_true(error == 0, "ParseIFF clipboard stops on DATA entry");
        memset(buffer, 0, sizeof(buffer));
        expect_true(ReadChunkBytes(iff, buffer, sizeof(buffer) - 1) == 24, "ReadChunkBytes clipboard DATA");
        expect_true(bytes_equal(buffer, data_data, strlen(data_data)), "Clipboard DATA prefix");
        expect_true(bytes_equal(buffer + strlen(data_data), data_extra, strlen(data_extra)),
                    "Clipboard DATA suffix");
        CloseIFF(iff);
    }

    FreeIFF(iff);
    CloseClipboard(clip);
}

int main(void)
{
    struct IFFHandle *iff;

    printf("IFFParse library regression test\n");
    printf("===============================\n\n");

    IFFParseBase = OpenLibrary((CONST_STRPTR)"iffparse.library", 0);
    expect_true(IFFParseBase != NULL, "OpenLibrary iffparse.library");
    if (!IFFParseBase)
        return 20;

    expect_true(GoodID(ID_FORM) != FALSE, "GoodID FORM");
    expect_true(GoodID(0) == FALSE, "GoodID rejects zero");
    expect_true(GoodType(ID_TEST) != FALSE, "GoodType TEST");
    expect_true(GoodType(MAKE_ID(' ','A','B','C')) == FALSE, "GoodType rejects leading space");

    iff = AllocIFF();
    expect_true(iff != NULL, "AllocIFF");
    if (!iff)
    {
        CloseLibrary(IFFParseBase);
        return 20;
    }

    expect_true(write_test_file(iff), "Create IFF test file");
    verify_basic_read(iff);
    verify_property_collection_handlers(iff);
    verify_stop_conditions(iff);
    verify_clipboard_roundtrip();

    FreeIFF(iff);
    CloseLibrary(IFFParseBase);

    if (g_failures == 0)
        printf("PASS: iffparse regression test complete\n");
    else
        printf("FAIL: iffparse regression test complete (%ld failures)\n", g_failures);

    return g_failures ? 20 : 0;
}
