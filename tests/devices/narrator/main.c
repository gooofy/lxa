/*
 * Test for narrator.device opens, parameter defaults/validation, and mouth
 * sync generation for write/read cycles.
 */

#include <exec/types.h>
#include <exec/io.h>
#include <exec/ports.h>
#include <exec/errors.h>
#include <devices/newstyle.h>
#include <devices/narrator.h>
#include <clib/exec_protos.h>
#include <clib/dos_protos.h>
#include <inline/exec.h>
#include <inline/dos.h>

#ifndef NDF_READMOUTH
#define NDF_READMOUTH 0x01
#define NDF_READWORD  0x02
#define NDF_READSYL   0x04
#endif

extern struct ExecBase *SysBase;
extern struct DosLibrary *DOSBase;

static LONG test_pass = 0;
static LONG test_fail = 0;

static void print(const char *s)
{
    BPTR out = Output();
    LONG len = 0;
    const char *p = s;

    while (*p++)
    {
        len++;
    }

    Write(out, (CONST APTR)s, len);
}

static void print_num(LONG num)
{
    char buf[16];
    int i = 0;
    BOOL neg = FALSE;

    if (num < 0)
    {
        neg = TRUE;
        num = -num;
    }

    if (num == 0)
    {
        buf[i++] = '0';
    }
    else
    {
        char temp[16];
        int j = 0;

        while (num > 0)
        {
            temp[j++] = '0' + (num % 10);
            num /= 10;
        }

        if (neg)
        {
            buf[i++] = '-';
        }

        while (j > 0)
        {
            buf[i++] = temp[--j];
        }
    }

    buf[i] = '\0';
    print(buf);
}

static void test_ok(const char *name)
{
    print("  OK: ");
    print(name);
    print("\n");
    test_pass++;
}

static void test_fail_msg(const char *name)
{
    print("  FAIL: ");
    print(name);
    print("\n");
    test_fail++;
}

int main(void)
{
    struct MsgPort *voice_port;
    struct MsgPort *mouth_port;
    struct narrator_rb *voice;
    struct mouth_rb *mouth;
    struct IOStdReq *small_req;
    struct NSDeviceQueryResult query;
    UBYTE phonemes[] = "DHIHS IHZ AHMIY5GAH SPIY5KIHNX.#";
    LONG error;

    print("Testing narrator.device\n\n");

    voice_port = CreateMsgPort();
    mouth_port = CreateMsgPort();
    if (!voice_port || !mouth_port)
    {
        print("FAIL: Cannot create message ports\n");
        return 20;
    }

    voice = (struct narrator_rb *)CreateIORequest(voice_port, sizeof(struct narrator_rb));
    mouth = (struct mouth_rb *)CreateIORequest(mouth_port, sizeof(struct mouth_rb));
    small_req = (struct IOStdReq *)CreateIORequest(voice_port, sizeof(struct IOStdReq));
    if (!voice || !mouth || !small_req)
    {
        print("FAIL: Cannot create IO requests\n");
        return 20;
    }

    error = OpenDevice((STRPTR)"narrator.device", 1, (struct IORequest *)voice, 0);
    if (error != 0)
        test_ok("OpenDevice rejects invalid unit");
    else
    {
        test_fail_msg("OpenDevice rejects invalid unit");
        CloseDevice((struct IORequest *)voice);
    }

    error = OpenDevice((STRPTR)"narrator.device", 0, (struct IORequest *)small_req, 0);
    if (error != 0)
        test_ok("OpenDevice rejects undersized IORequest");
    else
    {
        test_fail_msg("OpenDevice rejects undersized IORequest");
        CloseDevice((struct IORequest *)small_req);
    }

    voice->flags = NDF_NEWIORB;
    error = OpenDevice((STRPTR)"narrator.device", 0, (struct IORequest *)voice, 0);
    if (error == 0)
        test_ok("OpenDevice succeeds");
    else
    {
        test_fail_msg("OpenDevice succeeds");
        print("    error=");
        print_num(error);
        print("\n");
        return 20;
    }

    if (voice->message.io_Device != NULL && voice->message.io_Unit != NULL)
        test_ok("OpenDevice stores device and unit pointers");
    else
        test_fail_msg("OpenDevice stores device and unit pointers");

    if (voice->rate == DEFRATE && voice->pitch == DEFPITCH && voice->volume == DEFVOL && voice->sampfreq == DEFFREQ)
        test_ok("OpenDevice loads default narrator parameters");
    else
        test_fail_msg("OpenDevice loads default narrator parameters");

    voice->message.io_Command = NSCMD_DEVICEQUERY;
    voice->message.io_Flags = IOF_QUICK;
    voice->message.io_Data = &query;
    voice->message.io_Length = sizeof(query);
    DoIO((struct IORequest *)voice);
    if (voice->message.io_Error == 0 &&
        voice->message.io_Actual == sizeof(query) &&
        query.nsdqr_SupportedCommands != NULL)
        test_ok("NSCMD_DEVICEQUERY reports narrator capabilities");
    else
        test_fail_msg("NSCMD_DEVICEQUERY reports narrator capabilities");

    voice->rate = MINRATE - 1;
    voice->message.io_Command = CMD_WRITE;
    voice->message.io_Flags = IOF_QUICK;
    voice->message.io_Data = phonemes;
    voice->message.io_Length = sizeof(phonemes) - 1;
    DoIO((struct IORequest *)voice);
    if (voice->message.io_Error == ND_RateErr)
        test_ok("CMD_WRITE rejects invalid speaking rate");
    else
        test_fail_msg("CMD_WRITE rejects invalid speaking rate");

    voice->rate = DEFRATE;
    voice->centralize = 1;
    voice->centphon = NULL;
    DoIO((struct IORequest *)voice);
    if (voice->message.io_Error == ND_CentPhonErr)
        test_ok("CMD_WRITE rejects missing central phoneme");
    else
        test_fail_msg("CMD_WRITE rejects missing central phoneme");

    voice->centralize = 0;
    voice->flags = NDF_NEWIORB | NDF_WORDSYNC | NDF_SYLSYNC;
    voice->mouths = 1;
    voice->message.io_Command = CMD_WRITE;
    voice->message.io_Flags = IOF_QUICK;
    voice->message.io_Data = phonemes;
    voice->message.io_Length = sizeof(phonemes) - 1;
    DoIO((struct IORequest *)voice);
    if (voice->message.io_Error == 0 && voice->message.io_Actual > 0)
        test_ok("CMD_WRITE accepts phoneme stream and reports consumed bytes");
    else
        test_fail_msg("CMD_WRITE accepts phoneme stream and reports consumed bytes");

    mouth->voice.message.io_Device = voice->message.io_Device;
    mouth->voice.message.io_Unit = voice->message.io_Unit;
    mouth->voice.message.io_Message.mn_ReplyPort = mouth_port;
    mouth->voice.message.io_Command = CMD_READ;
    mouth->voice.message.io_Flags = IOF_QUICK;
    DoIO((struct IORequest *)mouth);
    if (mouth->voice.message.io_Error == 0 &&
        (mouth->sync & NDF_READMOUTH) != 0 &&
        (mouth->sync & NDF_READWORD) != 0 &&
        (mouth->sync & NDF_READSYL) != 0 &&
        mouth->width != 0 && mouth->height != 0)
        test_ok("CMD_READ returns combined mouth, word, and syllable sync event");
    else
        test_fail_msg("CMD_READ returns combined mouth, word, and syllable sync event");

    mouth->voice.message.io_Command = CMD_READ;
    mouth->voice.message.io_Flags = IOF_QUICK;
    DoIO((struct IORequest *)mouth);
    if (mouth->voice.message.io_Error == 0 && (mouth->sync & NDF_READMOUTH) != 0)
        test_ok("CMD_READ returns subsequent mouth events while speech data remains");
    else
        test_fail_msg("CMD_READ returns subsequent mouth events while speech data remains");

    voice->message.io_Command = CMD_FLUSH;
    voice->message.io_Flags = IOF_QUICK;
    DoIO((struct IORequest *)voice);
    if (voice->message.io_Error == 0)
        test_ok("CMD_FLUSH succeeds");
    else
        test_fail_msg("CMD_FLUSH succeeds");

    mouth->voice.message.io_Command = CMD_READ;
    mouth->voice.message.io_Flags = IOF_QUICK;
    DoIO((struct IORequest *)mouth);
    if (mouth->voice.message.io_Error == ND_NoWrite)
        test_ok("CMD_FLUSH clears pending mouth events");
    else
        test_fail_msg("CMD_FLUSH clears pending mouth events");

    voice->flags = NDF_NEWIORB | NDF_WORDSYNC;
    voice->mouths = 1;
    voice->rate = 200;
    voice->pitch = 140;
    voice->message.io_Command = CMD_RESET;
    voice->message.io_Flags = IOF_QUICK;
    DoIO((struct IORequest *)voice);
    if (voice->message.io_Error == 0 &&
        voice->rate == DEFRATE &&
        voice->pitch == DEFPITCH &&
        voice->mouths == 0 &&
        (voice->flags & (NDF_NEWIORB | NDF_WORDSYNC)) == NDF_NEWIORB)
        test_ok("CMD_RESET restores defaults and preserves NEWIORB flag only");
    else
        test_fail_msg("CMD_RESET restores defaults and preserves NEWIORB flag only");

    CloseDevice((struct IORequest *)voice);
    test_ok("CloseDevice handle");

    DeleteIORequest((struct IORequest *)small_req);
    DeleteIORequest((struct IORequest *)mouth);
    DeleteIORequest((struct IORequest *)voice);
    DeleteMsgPort(mouth_port);
    DeleteMsgPort(voice_port);

    print("\n");
    if (test_fail == 0)
    {
        print("PASS: All ");
        print_num(test_pass);
        print(" tests passed!\n");
        return 0;
    }

    print("FAIL: ");
    print_num(test_fail);
    print(" of ");
    print_num(test_pass + test_fail);
    print(" tests failed\n");
    return 20;
}
