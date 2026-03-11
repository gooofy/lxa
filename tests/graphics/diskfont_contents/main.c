#include <exec/types.h>
#include <exec/memory.h>
#include <clib/exec_protos.h>
#include <clib/dos_protos.h>
#include <clib/diskfont_protos.h>
#include <clib/graphics_protos.h>
#include <dos/doshunks.h>
#include <graphics/text.h>
#include <inline/exec.h>
#include <inline/dos.h>
#include <inline/diskfont.h>
#include <inline/graphics.h>
#include <libraries/diskfont.h>
#include <diskfont/diskfonttag.h>

extern struct ExecBase *SysBase;
extern struct DosLibrary *DOSBase;
extern struct GfxBase *GfxBase;

struct Library *DiskfontBase;

static LONG errors;

static void print(const char *s)
{
    BPTR out = Output();
    LONG len = 0;

    while (s[len])
        len++;

    Write(out, (CONST APTR)s, len);
}

static void print_num(LONG value)
{
    char buf[16];
    int i = 0;
    BOOL negative = FALSE;

    if (value < 0)
    {
        negative = TRUE;
        value = -value;
    }

    if (value == 0)
    {
        buf[i++] = '0';
    }
    else
    {
        while (value > 0 && i < (int)sizeof(buf) - 1)
        {
            buf[i++] = '0' + (value % 10);
            value /= 10;
        }
    }

    if (negative)
        buf[i++] = '-';

    while (i-- > 0)
        Write(Output(), &buf[i], 1);
}

static void pass(const char *msg)
{
    print("OK: ");
    print(msg);
    print("\n");
}

static void fail(const char *msg)
{
    print("FAIL: ");
    print(msg);
    print("\n");
    errors++;
}

static BOOL streq(const char *a, const char *b)
{
    while (*a && *b)
    {
        if (*a != *b)
            return FALSE;
        a++;
        b++;
    }

    return *a == *b;
}

static BOOL write_all(BPTR fh, const void *data, LONG len)
{
    return Write(fh, (CONST APTR)data, len) == len;
}

static void ensure_dir(const char *path)
{
    BPTR lock = Lock((CONST_STRPTR)path, SHARED_LOCK);

    if (lock)
    {
        UnLock(lock);
        return;
    }

    CreateDir((CONST_STRPTR)path);
}

static BOOL create_font_contents_file(const char *path)
{
    BPTR fh;
    struct FontContentsHeader header;
    struct FontContents entries[2];
    int i;

    fh = Open((CONST_STRPTR)path, MODE_NEWFILE);
    if (!fh)
        return FALSE;

    header.fch_FileID = FCH_ID;
    header.fch_NumEntries = 2;

    for (i = 0; i < 2; i++)
    {
        int j;

        for (j = 0; j < MAXFONTPATH; j++)
            entries[i].fc_FileName[j] = 0;

        entries[i].fc_YSize = (i == 0) ? 8 : 12;
        entries[i].fc_Style = (i == 0) ? 0 : FSF_BOLD;
        entries[i].fc_Flags = (i == 0) ? FPF_ROMFONT : FPF_DISKFONT;
    }

    entries[0].fc_FileName[0] = 't';
    entries[0].fc_FileName[1] = 'e';
    entries[0].fc_FileName[2] = 's';
    entries[0].fc_FileName[3] = 't';
    entries[0].fc_FileName[4] = '/';
    entries[0].fc_FileName[5] = 't';
    entries[0].fc_FileName[6] = 'e';
    entries[0].fc_FileName[7] = 's';
    entries[0].fc_FileName[8] = 't';
    entries[0].fc_FileName[9] = '8';

    entries[1].fc_FileName[0] = 't';
    entries[1].fc_FileName[1] = 'e';
    entries[1].fc_FileName[2] = 's';
    entries[1].fc_FileName[3] = 't';
    entries[1].fc_FileName[4] = '/';
    entries[1].fc_FileName[5] = 't';
    entries[1].fc_FileName[6] = 'e';
    entries[1].fc_FileName[7] = 's';
    entries[1].fc_FileName[8] = 't';
    entries[1].fc_FileName[9] = '1';
    entries[1].fc_FileName[10] = '2';

    if (!write_all(fh, &header, sizeof(header)) ||
        !write_all(fh, &entries[0], sizeof(entries)))
    {
        Close(fh);
        return FALSE;
    }

    Close(fh);
    return TRUE;
}

static BOOL create_invalid_contents_file(const char *path)
{
    BPTR fh;
    struct FontContentsHeader header;

    fh = Open((CONST_STRPTR)path, MODE_NEWFILE);
    if (!fh)
        return FALSE;

    header.fch_FileID = 0x1234;
    header.fch_NumEntries = 0;

    if (!write_all(fh, &header, sizeof(header)))
    {
        Close(fh);
        return FALSE;
    }

    Close(fh);
    return TRUE;
}

struct MinimalDiskFontPayload
{
    ULONG return_code;
    struct DiskFontHeader dfh;
    UBYTE char_data[12];
    ULONG char_loc[1];
};

struct ProportionalDiskFontPayload
{
    ULONG return_code;
    struct DiskFontHeader dfh;
    UBYTE char_data[12];
    ULONG char_loc[1];
    WORD char_space[1];
    WORD char_kern[1];
};

struct ColorDiskFontHeader
{
    struct Node dfh_DF;
    UWORD dfh_FileID;
    UWORD dfh_Revision;
    BPTR dfh_Segment;
    char dfh_Name[MAXFONTNAME];
    struct ColorTextFont ctf;
};

struct ColorDiskFontPayload
{
    ULONG return_code;
    struct ColorDiskFontHeader dfh;
    UBYTE plane0[12];
    UBYTE plane1[12];
    ULONG char_loc[1];
};

static BOOL create_bitmap_font_file(const char *path, const char *font_name)
{
    BPTR fh;
    struct MinimalDiskFontPayload payload;
    ULONG hunk_header = HUNK_HEADER;
    ULONG zero = 0;
    ULONG one = 1;
    ULONG hunk_code = HUNK_CODE;
    ULONG hunk_reloc32 = HUNK_RELOC32;
    ULONG hunk_end = HUNK_END;
    ULONG hunk_size_longs;
    ULONG padded_size;
    ULONG payload_size = sizeof(payload);
    ULONG reloc_count = 2;
    ULONG reloc_offsets[2];
    UBYTE pad[4];
    int i;

    fh = Open((CONST_STRPTR)path, MODE_NEWFILE);
    if (!fh)
        return FALSE;

    hunk_size_longs = (payload_size + 3) / 4;
    padded_size = hunk_size_longs * 4;

    for (i = 0; i < (int)sizeof(payload); i++)
        ((UBYTE *)&payload)[i] = 0;

    payload.return_code = 0x70004E75;
    payload.dfh.dfh_FileID = DFH_ID;
    payload.dfh.dfh_Revision = 1;

    for (i = 0; i < MAXFONTNAME - 1 && font_name[i]; i++)
        payload.dfh.dfh_Name[i] = font_name[i];

    payload.dfh.dfh_TF.tf_YSize = 12;
    payload.dfh.dfh_TF.tf_Style = FS_NORMAL;
    payload.dfh.dfh_TF.tf_Flags = FPF_DESIGNED;
    payload.dfh.dfh_TF.tf_XSize = 8;
    payload.dfh.dfh_TF.tf_Baseline = 9;
    payload.dfh.dfh_TF.tf_BoldSmear = 1;
    payload.dfh.dfh_TF.tf_LoChar = 'A';
    payload.dfh.dfh_TF.tf_HiChar = 'A';
    payload.dfh.dfh_TF.tf_Modulo = 1;
    payload.dfh.dfh_TF.tf_CharData = (APTR)((UBYTE *)payload.char_data - (UBYTE *)&payload);
    payload.dfh.dfh_TF.tf_CharLoc = (APTR)((UBYTE *)payload.char_loc - (UBYTE *)&payload);

    for (i = 0; i < 12; i++)
        payload.char_data[i] = (i == 0 || i == 11) ? 0xFF : 0x81;

    payload.char_loc[0] = 8;

    reloc_offsets[0] = (ULONG)((UBYTE *)&payload.dfh.dfh_TF.tf_CharData - (UBYTE *)&payload);
    reloc_offsets[1] = (ULONG)((UBYTE *)&payload.dfh.dfh_TF.tf_CharLoc - (UBYTE *)&payload);

    pad[0] = 0;
    pad[1] = 0;
    pad[2] = 0;
    pad[3] = 0;

    if (!write_all(fh, &hunk_header, 4) ||
        !write_all(fh, &zero, 4) ||
        !write_all(fh, &one, 4) ||
        !write_all(fh, &zero, 4) ||
        !write_all(fh, &zero, 4) ||
        !write_all(fh, &hunk_size_longs, 4) ||
        !write_all(fh, &hunk_code, 4) ||
        !write_all(fh, &hunk_size_longs, 4) ||
        !write_all(fh, &payload, payload_size) ||
        (padded_size > payload_size && !write_all(fh, pad, padded_size - payload_size)) ||
        !write_all(fh, &hunk_reloc32, 4) ||
        !write_all(fh, &reloc_count, 4) ||
        !write_all(fh, &zero, 4) ||
        !write_all(fh, &reloc_offsets[0], 4) ||
        !write_all(fh, &reloc_offsets[1], 4) ||
        !write_all(fh, &zero, 4) ||
        !write_all(fh, &hunk_end, 4))
    {
        Close(fh);
        return FALSE;
    }

    Close(fh);
    return TRUE;
}

static BOOL create_tagged_font_contents_file(const char *path)
{
    BPTR fh;
    struct FontContentsHeader header;
    struct TFontContents entry;
    int i;

    fh = Open((CONST_STRPTR)path, MODE_NEWFILE);
    if (!fh)
        return FALSE;

    header.fch_FileID = TFCH_ID;
    header.fch_NumEntries = 1;

    for (i = 0; i < MAXFONTPATH - 2; i++)
        entry.tfc_FileName[i] = 0;

    entry.tfc_TagCount = 0;
    entry.tfc_YSize = 12;
    entry.tfc_Style = FS_NORMAL;
    entry.tfc_Flags = FPF_DISKFONT | FPF_PROPORTIONAL;

    entry.tfc_FileName[0] = 'p';
    entry.tfc_FileName[1] = 'r';
    entry.tfc_FileName[2] = 'o';
    entry.tfc_FileName[3] = 'p';
    entry.tfc_FileName[4] = '/';
    entry.tfc_FileName[5] = 'p';
    entry.tfc_FileName[6] = 'r';
    entry.tfc_FileName[7] = 'o';
    entry.tfc_FileName[8] = 'p';
    entry.tfc_FileName[9] = '1';
    entry.tfc_FileName[10] = '2';

    if (!write_all(fh, &header, sizeof(header)) ||
        !write_all(fh, &entry, sizeof(entry)))
    {
        Close(fh);
        return FALSE;
    }

    Close(fh);
    return TRUE;
}

static BOOL create_color_font_contents_file(const char *path)
{
    BPTR fh;
    struct FontContentsHeader header;
    struct FontContents entry;
    int i;

    fh = Open((CONST_STRPTR)path, MODE_NEWFILE);
    if (!fh)
        return FALSE;

    header.fch_FileID = FCH_ID;
    header.fch_NumEntries = 1;

    for (i = 0; i < MAXFONTPATH; i++)
        entry.fc_FileName[i] = 0;

    entry.fc_YSize = 12;
    entry.fc_Style = FSF_COLORFONT;
    entry.fc_Flags = FPF_DISKFONT;

    entry.fc_FileName[0] = 'c';
    entry.fc_FileName[1] = 'o';
    entry.fc_FileName[2] = 'l';
    entry.fc_FileName[3] = 'o';
    entry.fc_FileName[4] = 'r';
    entry.fc_FileName[5] = '/';
    entry.fc_FileName[6] = 'c';
    entry.fc_FileName[7] = 'o';
    entry.fc_FileName[8] = 'l';
    entry.fc_FileName[9] = 'o';
    entry.fc_FileName[10] = 'r';
    entry.fc_FileName[11] = '1';
    entry.fc_FileName[12] = '2';

    if (!write_all(fh, &header, sizeof(header)) ||
        !write_all(fh, &entry, sizeof(entry)))
    {
        Close(fh);
        return FALSE;
    }

    Close(fh);
    return TRUE;
}

static BOOL create_proportional_font_file(const char *path, const char *font_name)
{
    BPTR fh;
    struct ProportionalDiskFontPayload payload;
    ULONG hunk_header = HUNK_HEADER;
    ULONG zero = 0;
    ULONG one = 1;
    ULONG hunk_code = HUNK_CODE;
    ULONG hunk_reloc32 = HUNK_RELOC32;
    ULONG hunk_end = HUNK_END;
    ULONG hunk_size_longs;
    ULONG padded_size;
    ULONG payload_size = sizeof(payload);
    ULONG reloc_count = 4;
    ULONG reloc_offsets[4];
    UBYTE pad[4];
    int i;

    fh = Open((CONST_STRPTR)path, MODE_NEWFILE);
    if (!fh)
        return FALSE;

    hunk_size_longs = (payload_size + 3) / 4;
    padded_size = hunk_size_longs * 4;

    for (i = 0; i < (int)sizeof(payload); i++)
        ((UBYTE *)&payload)[i] = 0;

    payload.return_code = 0x70004E75;
    payload.dfh.dfh_FileID = DFH_ID;
    payload.dfh.dfh_Revision = 1;

    for (i = 0; i < MAXFONTNAME - 1 && font_name[i]; i++)
        payload.dfh.dfh_Name[i] = font_name[i];

    payload.dfh.dfh_TF.tf_YSize = 12;
    payload.dfh.dfh_TF.tf_Style = FS_NORMAL;
    payload.dfh.dfh_TF.tf_Flags = FPF_DESIGNED | FPF_PROPORTIONAL;
    payload.dfh.dfh_TF.tf_XSize = 8;
    payload.dfh.dfh_TF.tf_Baseline = 9;
    payload.dfh.dfh_TF.tf_BoldSmear = 1;
    payload.dfh.dfh_TF.tf_LoChar = 'A';
    payload.dfh.dfh_TF.tf_HiChar = 'A';
    payload.dfh.dfh_TF.tf_Modulo = 1;
    payload.dfh.dfh_TF.tf_CharData = (APTR)((UBYTE *)payload.char_data - (UBYTE *)&payload);
    payload.dfh.dfh_TF.tf_CharLoc = (APTR)((UBYTE *)payload.char_loc - (UBYTE *)&payload);
    payload.dfh.dfh_TF.tf_CharSpace = (APTR)((UBYTE *)payload.char_space - (UBYTE *)&payload);
    payload.dfh.dfh_TF.tf_CharKern = (APTR)((UBYTE *)payload.char_kern - (UBYTE *)&payload);

    for (i = 0; i < 12; i++)
        payload.char_data[i] = (i == 0 || i == 11) ? 0x7C : 0x44;

    payload.char_loc[0] = 5;
    payload.char_space[0] = 6;
    payload.char_kern[0] = -1;

    reloc_offsets[0] = (ULONG)((UBYTE *)&payload.dfh.dfh_TF.tf_CharData - (UBYTE *)&payload);
    reloc_offsets[1] = (ULONG)((UBYTE *)&payload.dfh.dfh_TF.tf_CharLoc - (UBYTE *)&payload);
    reloc_offsets[2] = (ULONG)((UBYTE *)&payload.dfh.dfh_TF.tf_CharSpace - (UBYTE *)&payload);
    reloc_offsets[3] = (ULONG)((UBYTE *)&payload.dfh.dfh_TF.tf_CharKern - (UBYTE *)&payload);

    pad[0] = 0;
    pad[1] = 0;
    pad[2] = 0;
    pad[3] = 0;

    if (!write_all(fh, &hunk_header, 4) ||
        !write_all(fh, &zero, 4) ||
        !write_all(fh, &one, 4) ||
        !write_all(fh, &zero, 4) ||
        !write_all(fh, &zero, 4) ||
        !write_all(fh, &hunk_size_longs, 4) ||
        !write_all(fh, &hunk_code, 4) ||
        !write_all(fh, &hunk_size_longs, 4) ||
        !write_all(fh, &payload, payload_size) ||
        (padded_size > payload_size && !write_all(fh, pad, padded_size - payload_size)) ||
        !write_all(fh, &hunk_reloc32, 4) ||
        !write_all(fh, &reloc_count, 4) ||
        !write_all(fh, &zero, 4) ||
        !write_all(fh, &reloc_offsets[0], 4) ||
        !write_all(fh, &reloc_offsets[1], 4) ||
        !write_all(fh, &reloc_offsets[2], 4) ||
        !write_all(fh, &reloc_offsets[3], 4) ||
        !write_all(fh, &zero, 4) ||
        !write_all(fh, &hunk_end, 4))
    {
        Close(fh);
        return FALSE;
    }

    Close(fh);
    return TRUE;
}

static BOOL create_color_font_file(const char *path, const char *font_name)
{
    BPTR fh;
    struct ColorDiskFontPayload payload;
    ULONG hunk_header = HUNK_HEADER;
    ULONG zero = 0;
    ULONG one = 1;
    ULONG hunk_code = HUNK_CODE;
    ULONG hunk_reloc32 = HUNK_RELOC32;
    ULONG hunk_end = HUNK_END;
    ULONG hunk_size_longs;
    ULONG padded_size;
    ULONG payload_size = sizeof(payload);
    ULONG reloc_count = 4;
    ULONG reloc_offsets[4];
    UBYTE pad[4];
    int i;

    fh = Open((CONST_STRPTR)path, MODE_NEWFILE);
    if (!fh)
        return FALSE;

    hunk_size_longs = (payload_size + 3) / 4;
    padded_size = hunk_size_longs * 4;

    for (i = 0; i < (int)sizeof(payload); i++)
        ((UBYTE *)&payload)[i] = 0;

    payload.return_code = 0x70004E75;
    payload.dfh.dfh_FileID = DFH_ID;
    payload.dfh.dfh_Revision = 1;

    for (i = 0; i < MAXFONTNAME - 1 && font_name[i]; i++)
        payload.dfh.dfh_Name[i] = font_name[i];

    payload.dfh.ctf.ctf_TF.tf_YSize = 12;
    payload.dfh.ctf.ctf_TF.tf_Style = FSF_COLORFONT;
    payload.dfh.ctf.ctf_TF.tf_Flags = FPF_DESIGNED;
    payload.dfh.ctf.ctf_TF.tf_XSize = 6;
    payload.dfh.ctf.ctf_TF.tf_Baseline = 9;
    payload.dfh.ctf.ctf_TF.tf_BoldSmear = 1;
    payload.dfh.ctf.ctf_TF.tf_LoChar = 'A';
    payload.dfh.ctf.ctf_TF.tf_HiChar = 'A';
    payload.dfh.ctf.ctf_TF.tf_Modulo = 1;
    payload.dfh.ctf.ctf_TF.tf_CharData = (APTR)((UBYTE *)payload.plane0 - (UBYTE *)&payload);
    payload.dfh.ctf.ctf_TF.tf_CharLoc = (APTR)((UBYTE *)payload.char_loc - (UBYTE *)&payload);
    payload.dfh.ctf.ctf_Depth = 2;
    payload.dfh.ctf.ctf_FgColor = 0xFF;
    payload.dfh.ctf.ctf_Low = 0;
    payload.dfh.ctf.ctf_High = 3;
    payload.dfh.ctf.ctf_PlanePick = 0x03;
    payload.dfh.ctf.ctf_PlaneOnOff = 0x00;
    payload.dfh.ctf.ctf_CharData[0] = (APTR)((UBYTE *)payload.plane0 - (UBYTE *)&payload);
    payload.dfh.ctf.ctf_CharData[1] = (APTR)((UBYTE *)payload.plane1 - (UBYTE *)&payload);

    for (i = 0; i < 12; i++)
    {
        payload.plane0[i] = 0xF8;
        payload.plane1[i] = 0xF8;
    }

    payload.char_loc[0] = 5;

    reloc_offsets[0] = (ULONG)((UBYTE *)&payload.dfh.ctf.ctf_TF.tf_CharData - (UBYTE *)&payload);
    reloc_offsets[1] = (ULONG)((UBYTE *)&payload.dfh.ctf.ctf_TF.tf_CharLoc - (UBYTE *)&payload);
    reloc_offsets[2] = (ULONG)((UBYTE *)&payload.dfh.ctf.ctf_CharData[0] - (UBYTE *)&payload);
    reloc_offsets[3] = (ULONG)((UBYTE *)&payload.dfh.ctf.ctf_CharData[1] - (UBYTE *)&payload);

    pad[0] = 0;
    pad[1] = 0;
    pad[2] = 0;
    pad[3] = 0;

    if (!write_all(fh, &hunk_header, 4) ||
        !write_all(fh, &zero, 4) ||
        !write_all(fh, &one, 4) ||
        !write_all(fh, &zero, 4) ||
        !write_all(fh, &zero, 4) ||
        !write_all(fh, &hunk_size_longs, 4) ||
        !write_all(fh, &hunk_code, 4) ||
        !write_all(fh, &hunk_size_longs, 4) ||
        !write_all(fh, &payload, payload_size) ||
        (padded_size > payload_size && !write_all(fh, pad, padded_size - payload_size)) ||
        !write_all(fh, &hunk_reloc32, 4) ||
        !write_all(fh, &reloc_count, 4) ||
        !write_all(fh, &zero, 4) ||
        !write_all(fh, &reloc_offsets[0], 4) ||
        !write_all(fh, &reloc_offsets[1], 4) ||
        !write_all(fh, &reloc_offsets[2], 4) ||
        !write_all(fh, &reloc_offsets[3], 4) ||
        !write_all(fh, &zero, 4) ||
        !write_all(fh, &hunk_end, 4))
    {
        Close(fh);
        return FALSE;
    }

    Close(fh);
    return TRUE;
}

static BOOL ptr_in_range(const void *ptr, const void *start, ULONG size)
{
    const UBYTE *p = (const UBYTE *)ptr;
    const UBYTE *s = (const UBYTE *)start;
    return p >= s && p < (s + size);
}

static void test_avail_fonts(void)
{
    LONG needed;
    LONG shortage;
    struct AvailFontsHeader *buffer;
    struct AvailFonts *entries;
    ULONG buffer_size;
    BOOL saw_test8 = FALSE;
    BOOL saw_test12 = FALSE;
    BOOL saw_prop12 = FALSE;
    BOOL saw_color12 = FALSE;
    UWORD i;

    needed = AvailFonts(NULL, 0, AFF_DISK);
    if (needed > (LONG)sizeof(struct AvailFontsHeader))
        pass("AvailFonts reports full buffer size for disk entries");
    else
        fail("AvailFonts reports full buffer size for disk entries");

    buffer_size = (ULONG)needed;
    buffer = (struct AvailFontsHeader *)AllocMem(buffer_size - 1, MEMF_PUBLIC | MEMF_CLEAR);
    if (!buffer)
    {
        fail("AllocMem for AvailFonts shortage buffer");
        return;
    }

    shortage = AvailFonts(buffer, buffer_size - 1, AFF_DISK);
    if (shortage == 1)
        pass("AvailFonts returns additional bytes needed for short buffer");
    else
        fail("AvailFonts returns additional bytes needed for short buffer");

    FreeMem(buffer, buffer_size - 1);

    buffer = (struct AvailFontsHeader *)AllocMem(buffer_size, MEMF_PUBLIC | MEMF_CLEAR);
    if (!buffer)
    {
        fail("AllocMem for AvailFonts buffer");
        return;
    }

    if (AvailFonts(buffer, buffer_size, AFF_DISK) == 0)
        pass("AvailFonts fills exact-size disk buffer");
    else
        fail("AvailFonts fills exact-size disk buffer");

    if (buffer->afh_NumEntries == 4)
        pass("AvailFonts reports bitmap, proportional, and color disk entries");
    else
        fail("AvailFonts reports bitmap, proportional, and color disk entries");

    entries = (struct AvailFonts *)(buffer + 1);
    for (i = 0; i < buffer->afh_NumEntries; i++)
    {
        if (!ptr_in_range(entries[i].af_Attr.ta_Name, buffer, buffer_size))
            continue;

        if (streq((const char *)entries[i].af_Attr.ta_Name, "test.font") && entries[i].af_Attr.ta_YSize == 8)
            saw_test8 = TRUE;
        else if (streq((const char *)entries[i].af_Attr.ta_Name, "test.font") && entries[i].af_Attr.ta_YSize == 12)
            saw_test12 = TRUE;
        else if (streq((const char *)entries[i].af_Attr.ta_Name, "prop.font") && entries[i].af_Attr.ta_YSize == 12)
            saw_prop12 = TRUE;
        else if (streq((const char *)entries[i].af_Attr.ta_Name, "color.font") && entries[i].af_Attr.ta_YSize == 12)
            saw_color12 = TRUE;
    }

    if (saw_test8 && saw_test12 && saw_prop12 && saw_color12)
    {
        pass("AvailFonts packs disk font names inside caller buffer");
    }
    else
    {
        fail("AvailFonts packs disk font names inside caller buffer");
    }

    saw_test8 = FALSE;
    saw_test12 = FALSE;
    saw_prop12 = FALSE;
    saw_color12 = FALSE;

    for (i = 0; i < buffer->afh_NumEntries; i++)
    {
        if (entries[i].af_Type != AFF_DISK)
            continue;

        if (entries[i].af_Attr.ta_YSize == 8 && !(entries[i].af_Attr.ta_Flags & FPF_PROPORTIONAL))
            saw_test8 = TRUE;
        else if (entries[i].af_Attr.ta_YSize == 12 && !(entries[i].af_Attr.ta_Flags & FPF_PROPORTIONAL))
        {
            if (entries[i].af_Attr.ta_Style & FSF_COLORFONT)
                saw_color12 = TRUE;
            else
                saw_test12 = TRUE;
        }
        else if (entries[i].af_Attr.ta_YSize == 12 && (entries[i].af_Attr.ta_Flags & FPF_PROPORTIONAL))
            saw_prop12 = TRUE;
    }

    if (saw_test8 && saw_test12 && saw_prop12 && saw_color12)
    {
        pass("AvailFonts preserves disk entry metadata including proportional and color flags");
    }
    else
    {
        fail("AvailFonts preserves disk entry metadata including proportional and color flags");
    }

    FreeMem(buffer, buffer_size);
}

static void test_tagged_avail_fonts(void)
{
    LONG needed;
    struct AvailFontsHeader *buffer;
    struct TAvailFonts *entries;
    ULONG buffer_size;
    BOOL saw_prop = FALSE;
    BOOL saw_color = FALSE;
    UWORD i;

    needed = AvailFonts(NULL, 0, AFF_DISK | AFF_TAGGED);
    buffer_size = (ULONG)needed;
    buffer = (struct AvailFontsHeader *)AllocMem(buffer_size, MEMF_PUBLIC | MEMF_CLEAR);
    if (!buffer)
    {
        fail("AllocMem for tagged AvailFonts buffer");
        return;
    }

    if (AvailFonts(buffer, buffer_size, AFF_DISK | AFF_TAGGED) == 0)
        pass("AvailFonts fills tagged disk buffer");
    else
        fail("AvailFonts fills tagged disk buffer");

    entries = (struct TAvailFonts *)(buffer + 1);
    if (buffer->afh_NumEntries == 4)
    {
        for (i = 0; i < buffer->afh_NumEntries; i++)
        {
            if (!ptr_in_range(entries[i].taf_Attr.tta_Name, buffer, buffer_size) ||
                entries[i].taf_Attr.tta_Tags != NULL)
                continue;

            if (streq((const char *)entries[i].taf_Attr.tta_Name, "prop.font") &&
                (entries[i].taf_Attr.tta_Flags & FPF_PROPORTIONAL))
            {
                saw_prop = TRUE;
            }

            if (streq((const char *)entries[i].taf_Attr.tta_Name, "color.font") &&
                (entries[i].taf_Attr.tta_Style & FSF_COLORFONT))
            {
                saw_color = TRUE;
            }
        }
    }

    if (saw_prop && saw_color)
    {
        pass("AvailFonts packs tagged entries with inline names, NULL tags, and proportional/color flags");
    }
    else
    {
        fail("AvailFonts packs tagged entries with inline names, NULL tags, and proportional/color flags");
    }

    FreeMem(buffer, buffer_size);
}

static void test_open_disk_font_path(void)
{
    struct TextAttr ta;
    struct TextFont *font;
    struct TextExtent extent;

    ta.ta_Name = (STRPTR)"T:diskfont_test/test.font";
    ta.ta_YSize = 12;
    ta.ta_Style = FS_NORMAL;
    ta.ta_Flags = 0;

    font = OpenDiskFont(&ta);
    if (!font)
    {
        fail("OpenDiskFont loads bitmap font via explicit .font path");
        return;
    }

    if (font->tf_YSize == 12 && streq((const char *)font->tf_Message.mn_Node.ln_Name, "T:diskfont_test/test.font"))
        pass("OpenDiskFont preserves requested path and font size");
    else
        fail("OpenDiskFont preserves requested path and font size");

    FontExtent(font, &extent);
    if (extent.te_Height == 12 && extent.te_Extent.MaxX == 7)
        pass("OpenDiskFont relocates bitmap CharLoc data");
    else
        fail("OpenDiskFont relocates bitmap CharLoc data");

    CloseFont(font);
    pass("CloseFont accepts bitmap disk font loaded from path");
}

static void test_open_proportional_disk_font_path(void)
{
    struct TextAttr ta;
    struct TextFont *font;
    struct TextExtent extent;
    struct RastPort rp;
    WORD len;

    ta.ta_Name = (STRPTR)"T:diskfont_test/prop.font";
    ta.ta_YSize = 12;
    ta.ta_Style = FS_NORMAL;
    ta.ta_Flags = 0;

    font = OpenDiskFont(&ta);
    if (!font)
    {
        fail("OpenDiskFont loads proportional bitmap font via explicit .font path");
        return;
    }

    if ((font->tf_Flags & FPF_PROPORTIONAL) &&
        font->tf_CharSpace != NULL &&
        font->tf_CharKern != NULL)
    {
        pass("OpenDiskFont preserves proportional spacing tables");
    }
    else
    {
        fail("OpenDiskFont preserves proportional spacing tables");
    }

    if (((WORD *)font->tf_CharKern)[0] == -1 &&
        ((WORD *)font->tf_CharSpace)[0] == 6)
    {
        pass("OpenDiskFont relocates CharSpace and CharKern data");
    }
    else
    {
        fail("OpenDiskFont relocates CharSpace and CharKern data");
    }

    InitRastPort(&rp);
    rp.Font = font;

    len = TextLength(&rp, (CONST_STRPTR)"A", 1);
    if (len == 5)
        pass("TextLength uses proportional diskfont CharKern and CharSpace");
    else
    {
        print("FAIL detail TextLength(A)=");
        print_num(len);
        print("\n");
        fail("TextLength uses proportional diskfont CharKern and CharSpace");
    }

    TextExtent(&rp, (CONST_STRPTR)"A", 1, &extent);
    if (extent.te_Width == 5 &&
        extent.te_Extent.MinX == -1 &&
        extent.te_Extent.MaxX == 4)
    {
        pass("TextExtent uses proportional diskfont CharKern and glyph width");
    }
    else
    {
        print("FAIL detail TextExtent width/min/max=");
        print_num(extent.te_Width);
        print(",");
        print_num(extent.te_Extent.MinX);
        print(",");
        print_num(extent.te_Extent.MaxX);
        print("\n");
        fail("TextExtent uses proportional diskfont CharKern and glyph width");
    }

    FontExtent(font, &extent);
    if (extent.te_Width == 5 &&
        extent.te_Extent.MinX == -1 &&
        extent.te_Extent.MaxX == 3)
    {
        pass("FontExtent uses proportional diskfont spacing tables");
    }
    else
    {
        fail("FontExtent uses proportional diskfont spacing tables");
    }

    CloseFont(font);
    pass("CloseFont accepts proportional bitmap disk font loaded from path");
}

static void test_open_color_disk_font_path(void)
{
    struct TextAttr ta;
    struct TextFont *font;
    struct RastPort rp;
    struct BitMap bm;
    PLANEPTR plane0;
    PLANEPTR plane1;
    ULONG pen;

    ta.ta_Name = (STRPTR)"T:diskfont_test/color.font";
    ta.ta_YSize = 12;
    ta.ta_Style = FS_NORMAL;
    ta.ta_Flags = 0;

    font = OpenDiskFont(&ta);
    if (!font)
    {
        fail("OpenDiskFont loads multi-plane color font via explicit .font path");
        return;
    }

    if ((font->tf_Style & FSF_COLORFONT) && font->tf_Modulo == 1)
        pass("OpenDiskFont preserves color font style and modulo");
    else
        fail("OpenDiskFont preserves color font style and modulo");

    plane0 = AllocRaster(16, 16);
    plane1 = AllocRaster(16, 16);
    if (!plane0 || !plane1)
    {
        fail("AllocRaster for multi-plane diskfont render bitmap");
        if (plane0)
            FreeRaster(plane0, 16, 16);
        if (plane1)
            FreeRaster(plane1, 16, 16);
        CloseFont(font);
        return;
    }

    InitBitMap(&bm, 2, 16, 16);
    bm.Planes[0] = plane0;
    bm.Planes[1] = plane1;

    InitRastPort(&rp);
    rp.BitMap = &bm;
    rp.FgPen = 1;
    rp.BgPen = 0;
    rp.DrawMode = JAM1;

    SetFont(&rp, font);
    Move(&rp, 0, font->tf_Baseline);
    Text(&rp, (CONST_STRPTR)"A", 1);

    pen = ReadPixel(&rp, 2, 5);
    if (pen == 3)
        pass("Text renders multi-plane diskfont glyph colors from all planes");
    else
        fail("Text renders multi-plane diskfont glyph colors from all planes");

    pen = ReadPixel(&rp, 6, 5);
    if (pen == 0)
        pass("Text confines multi-plane diskfont rendering to glyph bounds");
    else
        fail("Text confines multi-plane diskfont rendering to glyph bounds");

    if (rp.cp_x == 6)
        pass("Text advances cursor by color font nominal width");
    else
        fail("Text advances cursor by color font nominal width");

    FreeRaster(plane0, 16, 16);
    FreeRaster(plane1, 16, 16);
    CloseFont(font);
    pass("CloseFont accepts multi-plane color disk font loaded from path");
}

static void test_new_font_contents(BPTR fonts_lock)
{
    struct FontContentsHeader *contents;
    struct FontContents *entries;

    contents = NewFontContents(fonts_lock, (CONST_STRPTR)"test.font");
    if (!contents)
    {
        fail("NewFontContents returns data for valid .font file");
        return;
    }

    if (contents->fch_FileID == FCH_ID)
        pass("NewFontContents preserves file ID");
    else
        fail("NewFontContents preserves file ID");

    if (contents->fch_NumEntries == 2)
        pass("NewFontContents preserves entry count");
    else
        fail("NewFontContents preserves entry count");

    entries = (struct FontContents *)(contents + 1);

    if (streq((const char *)entries[0].fc_FileName, "test/test8") &&
        entries[0].fc_YSize == 8 &&
        entries[0].fc_Style == 0 &&
        entries[0].fc_Flags == FPF_ROMFONT)
    {
        pass("NewFontContents copies first entry");
    }
    else
    {
        fail("NewFontContents copies first entry");
    }

    if (streq((const char *)entries[1].fc_FileName, "test/test12") &&
        entries[1].fc_YSize == 12 &&
        entries[1].fc_Style == FSF_BOLD &&
        entries[1].fc_Flags == FPF_DISKFONT)
    {
        pass("NewFontContents copies second entry");
    }
    else
    {
        fail("NewFontContents copies second entry");
    }

    DisposeFontContents(contents);
    pass("DisposeFontContents accepts NewFontContents result");
}

static void test_new_tagged_font_contents(BPTR fonts_lock)
{
    struct FontContentsHeader *contents;
    struct TFontContents *entries;

    contents = NewFontContents(fonts_lock, (CONST_STRPTR)"prop.font");
    if (!contents)
    {
        fail("NewFontContents returns tagged data for proportional .font file");
        return;
    }

    if (contents->fch_FileID == TFCH_ID && contents->fch_NumEntries == 1)
        pass("NewFontContents preserves tagged proportional file metadata");
    else
        fail("NewFontContents preserves tagged proportional file metadata");

    entries = (struct TFontContents *)(contents + 1);
    if (streq((const char *)entries[0].tfc_FileName, "prop/prop12") &&
        entries[0].tfc_TagCount == 0 &&
        entries[0].tfc_YSize == 12 &&
        entries[0].tfc_Flags == (FPF_DISKFONT | FPF_PROPORTIONAL))
    {
        pass("NewFontContents copies tagged proportional entry");
    }
    else
    {
        fail("NewFontContents copies tagged proportional entry");
    }

    DisposeFontContents(contents);
    pass("DisposeFontContents accepts tagged proportional NewFontContents result");
}

static void test_invalid_inputs(BPTR fonts_lock)
{
    if (NewFontContents(fonts_lock, (CONST_STRPTR)"test") == NULL)
        pass("NewFontContents rejects missing .font suffix");
    else
        fail("NewFontContents rejects missing .font suffix");

    if (NewFontContents(fonts_lock, (CONST_STRPTR)"broken.font") == NULL)
        pass("NewFontContents rejects invalid file IDs");
    else
        fail("NewFontContents rejects invalid file IDs");

    DisposeFontContents(NULL);
    pass("DisposeFontContents accepts NULL");
}

static void test_diskfont_ctrl(void)
{
    struct TagItem tags[6];

    if (GetDiskFontCtrl(DFCTRL_XDPI) == 72 &&
        GetDiskFontCtrl(DFCTRL_YDPI) == 72 &&
        GetDiskFontCtrl(DFCTRL_XDOTP) == 100 &&
        GetDiskFontCtrl(DFCTRL_YDOTP) == 100 &&
        GetDiskFontCtrl(DFCTRL_CACHE) == FALSE &&
        GetDiskFontCtrl(DFCTRL_SORTMODE) == DFCTRL_SORT_OFF)
    {
        pass("GetDiskFontCtrl returns documented default settings");
    }
    else
    {
        fail("GetDiskFontCtrl returns documented default settings");
    }

    tags[0].ti_Tag = DFCTRL_XDPI;
    tags[0].ti_Data = 90;
    tags[1].ti_Tag = DFCTRL_YDPI;
    tags[1].ti_Data = 91;
    tags[2].ti_Tag = DFCTRL_XDOTP;
    tags[2].ti_Data = 110;
    tags[3].ti_Tag = DFCTRL_YDOTP;
    tags[3].ti_Data = 111;
    tags[4].ti_Tag = DFCTRL_CACHE;
    tags[4].ti_Data = TRUE;
    tags[5].ti_Tag = TAG_DONE;
    SetDiskFontCtrlA(tags);

    if (GetDiskFontCtrl(DFCTRL_XDPI) == 90 &&
        GetDiskFontCtrl(DFCTRL_YDPI) == 91 &&
        GetDiskFontCtrl(DFCTRL_XDOTP) == 110 &&
        GetDiskFontCtrl(DFCTRL_YDOTP) == 111 &&
        GetDiskFontCtrl(DFCTRL_CACHE) == TRUE)
    {
        pass("SetDiskFontCtrlA updates diskfont control settings");
    }
    else
    {
        fail("SetDiskFontCtrlA updates diskfont control settings");
    }

    tags[0].ti_Tag = DFCTRL_SORTMODE;
    tags[0].ti_Data = DFCTRL_SORT_ASC;
    tags[1].ti_Tag = DFCTRL_CHARSET;
    tags[1].ti_Data = 106;
    tags[2].ti_Tag = DFCTRL_CACHEFLUSH;
    tags[2].ti_Data = TRUE;
    tags[3].ti_Tag = TAG_DONE;
    SetDiskFontCtrlA(tags);

    if (GetDiskFontCtrl(DFCTRL_SORTMODE) == DFCTRL_SORT_ASC &&
        GetDiskFontCtrl(DFCTRL_CHARSET) == 106 &&
        GetDiskFontCtrl(DFCTRL_CACHE) == TRUE)
    {
        pass("SetDiskFontCtrlA accepts sort, charset, and cache flush tags");
    }
    else
    {
        fail("SetDiskFontCtrlA accepts sort, charset, and cache flush tags");
    }
}

int main(void)
{
    BPTR old_dir;
    BPTR test_dir_lock;
    BPTR fonts_lock;

    print("Diskfont contents test\n");
    print("======================\n");

    DiskfontBase = OpenLibrary((CONST_STRPTR)"diskfont.library", 0);
    if (!DiskfontBase)
    {
        print("FAIL: Cannot open diskfont.library\n");
        return 20;
    }

    ensure_dir("T:diskfont_test");

    test_dir_lock = Lock((CONST_STRPTR)"T:diskfont_test", SHARED_LOCK);
    if (!test_dir_lock)
    {
        print("FAIL: Cannot lock T:diskfont_test\n");
        CloseLibrary(DiskfontBase);
        return 20;
    }

    old_dir = CurrentDir(test_dir_lock);

    ensure_dir("test");
    ensure_dir("prop");
    ensure_dir("color");

    if (!create_font_contents_file("test.font") ||
        !create_tagged_font_contents_file("prop.font") ||
        !create_color_font_contents_file("color.font") ||
        !create_invalid_contents_file("broken.font") ||
        !create_bitmap_font_file("test/test12", "test.font") ||
        !create_proportional_font_file("prop/prop12", "prop.font") ||
        !create_color_font_file("color/color12", "color.font"))
    {
        print("FAIL: Cannot create test font files\n");
        CurrentDir(old_dir);
        CloseLibrary(DiskfontBase);
        return 20;
    }

    fonts_lock = Lock((CONST_STRPTR)"T:diskfont_test", SHARED_LOCK);
    if (!fonts_lock)
    {
        print("FAIL: Cannot lock current test directory\n");
        CurrentDir(old_dir);
        CloseLibrary(DiskfontBase);
        return 20;
    }

    test_new_font_contents(fonts_lock);
    test_new_tagged_font_contents(fonts_lock);
    test_invalid_inputs(fonts_lock);
    test_diskfont_ctrl();

    if (AssignPath((STRPTR)"FONTS", (STRPTR)"T:diskfont_test"))
    {
        pass("AssignPath updates FONTS: for AvailFonts regression");
        test_avail_fonts();
        test_tagged_avail_fonts();
    }
    else
    {
        fail("AssignPath updates FONTS: for AvailFonts regression");
    }

    test_open_disk_font_path();
    test_open_proportional_disk_font_path();
    test_open_color_disk_font_path();

    UnLock(fonts_lock);
    CurrentDir(old_dir);
    DeleteFile((CONST_STRPTR)"T:diskfont_test/color/color12");
    DeleteFile((CONST_STRPTR)"T:diskfont_test/color");
    DeleteFile((CONST_STRPTR)"T:diskfont_test/prop/prop12");
    DeleteFile((CONST_STRPTR)"T:diskfont_test/prop");
    DeleteFile((CONST_STRPTR)"T:diskfont_test/test/test12");
    DeleteFile((CONST_STRPTR)"T:diskfont_test/test");
    DeleteFile((CONST_STRPTR)"T:diskfont_test/color.font");
    DeleteFile((CONST_STRPTR)"T:diskfont_test/prop.font");
    DeleteFile((CONST_STRPTR)"T:diskfont_test/test.font");
    DeleteFile((CONST_STRPTR)"T:diskfont_test/broken.font");
    CloseLibrary(DiskfontBase);

    if (errors == 0)
    {
        print("PASS: diskfont contents helpers passed\n");
        return 0;
    }

    print("FAIL: ");
    print_num(errors);
    print(" checks failed\n");
    return 5;
}
