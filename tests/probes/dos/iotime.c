/*
 * Probe (Phase 237d): how much time dos.library file operations take on
 * the RAM: disk of AmigaOS 3.1.  Fred Fish DiskPerf (disks 48, 187) divides
 * by the time a batch of file operations took; on lxa they took no
 * emulated time at all and the program died with a divide by zero.
 * (Other volumes are charged as FS-UAE directory drives, measured once on
 * the reference's LXAREF: - see lxa_dispatch.c.)
 * Measured with ReadEClock(); printed as the power of two of the
 * microseconds per operation (bucket n: 2^n <= us < 2^(n+1)), so the
 * output does not depend on small timing differences.  Run with an
 * argument to also print the raw microseconds.
 */
#include <exec/types.h>
#include <exec/memory.h>
#include <dos/dos.h>
#include <devices/timer.h>
#include <clib/exec_protos.h>
#include <clib/dos_protos.h>
#include <clib/timer_protos.h>
#include <inline/exec.h>
#include <inline/dos.h>
#include <inline/timer.h>
#include "probe.h"

extern struct ExecBase *SysBase;
struct Device *TimerBase;
static ULONG efreq;
static int verbose;
static UBYTE *buf;

static ULONG now_us(void)
{
    struct EClockVal ev;
    efreq = ReadEClock(&ev);
    return (ULONG)(((unsigned long long)ev.ev_hi << 32 | ev.ev_lo) * 1000000ULL / efreq);
}

static void report(const char *name, ULONG us, LONG n)
{
    ULONG per = us / n;
    int b = 0;
    while ((per >> (b + 1)) != 0)
        b++;
    probe_s(name);
    probe_s(": 2^");
    probe_dec(per ? b : -1);
    probe_s(" us");
    if (verbose) {
        probe_s(" (");
        probe_dec(per);
        probe_s(")");
    }
    probe_ch('\n');
}

static char name[] = "T:iotime-0";

int main(int argc, char **argv)
{
    struct timerequest tr;
    BPTR fh, lock;
    ULONG t0;
    LONG i;

    verbose = argc > 1;
    if (OpenDevice((STRPTR)"timer.device", UNIT_ECLOCK, (struct IORequest *)&tr, 0))
        return 20;
    TimerBase = tr.tr_node.io_Device;
    buf = AllocMem(32768, MEMF_CLEAR);
    if (!buf)
        return 20;

    t0 = now_us();
    for (i = 0; i < 50; i++) {
        name[9] = (char)('A' + i % 26);
        fh = Open((STRPTR)name, MODE_NEWFILE);
        Close(fh);
    }
    report("create file", now_us() - t0, 50);

    t0 = now_us();
    for (i = 0; i < 200; i++) {
        fh = Open((STRPTR)name, MODE_OLDFILE);
        Close(fh);
    }
    report("open + close", now_us() - t0, 200);

    t0 = now_us();
    for (i = 0; i < 200; i++) {
        lock = Lock((STRPTR)name, SHARED_LOCK);
        UnLock(lock);
    }
    report("lock + unlock", now_us() - t0, 200);

    fh = Open((STRPTR)name, MODE_NEWFILE);
    t0 = now_us();
    for (i = 0; i < 200; i++)
        Write(fh, buf, 512);
    report("write 512", now_us() - t0, 200);
    t0 = now_us();
    for (i = 0; i < 20; i++)
        Write(fh, buf, 32768);
    report("write 32768", now_us() - t0, 20);
    Seek(fh, 0, OFFSET_BEGINNING);
    t0 = now_us();
    for (i = 0; i < 200; i++)
        Read(fh, buf, 512);
    report("read 512", now_us() - t0, 200);
    t0 = now_us();
    for (i = 0; i < 20; i++)
        Read(fh, buf, 32768);
    report("read 32768", now_us() - t0, 20);
    t0 = now_us();
    for (i = 0; i < 200; i++)
        Seek(fh, (i * 7919) % 400000, OFFSET_BEGINNING);
    report("seek", now_us() - t0, 200);
    Close(fh);

    fh = Open((STRPTR)name, MODE_OLDFILE);
    if (fh) {
        t0 = now_us();
        for (i = 0; i < 200; i++)
            Read(fh, buf, 1);
        report("read 1", now_us() - t0, 200);
        Close(fh);
    }

    t0 = now_us();
    for (i = 0; i < 26; i++) {
        name[9] = (char)('A' + i);
        DeleteFile((STRPTR)name);
    }
    report("delete file", now_us() - t0, 26);

    FreeMem(buf, 32768);
    CloseDevice((struct IORequest *)&tr);
    return 0;
}
