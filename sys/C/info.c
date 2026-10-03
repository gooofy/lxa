/*
 * INFO - show information about mounted volumes
 *
 * Template: DEVICE,VOLS/S,GOODONLY/S,BLOCKS/S,DEVICES/S
 *
 * Layout of AmigaOS 3.1 (verified on the reference, Phase 221; the values
 * are machine-dependent):
 *
 *   Mounted disks:
 *   Unit	  Size	  Used	  Free Full Errs   Status   Name
 *   RAM:       16K      16       0 100%   0  Read/Write Ram Disk
 *
 *   Volumes available:
 *   Ram Disk [Mounted]
 *
 * A device that is not mounted is not listed (no message, RC 0).
 */

#include <exec/types.h>
#include <exec/memory.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <clib/exec_protos.h>
#include <clib/dos_protos.h>
#include <inline/exec.h>
#include <inline/dos.h>

#include <string.h>

#pragma GCC diagnostic ignored "-Wpointer-sign"

extern struct DosLibrary *DOSBase;
extern struct ExecBase *SysBase;

#define TEMPLATE "DEVICE,VOLS/S,GOODONLY/S,BLOCKS/S,DEVICES/S"

enum { A_DEVICE, A_VOLS, A_GOODONLY, A_BLOCKS, A_DEVICES, A_COUNT };

static LONG args[A_COUNT];

/* the volume name of a lock (its root directory's name) */
static void volume_name(BPTR lock, char *buf, int len)
{
    char path[256];
    char *colon;

    buf[0] = '\0';
    if (!NameFromLock(lock, (STRPTR)path, sizeof(path)))
        return;
    colon = strchr(path, ':');
    if (colon)
        *colon = '\0';
    strncpy(buf, path, len - 1);
    buf[len - 1] = '\0';
}

static BOOL show_unit(const char *unit, BOOL volume_list, char *volname, int vlen)
{
    BPTR lock = Lock((STRPTR)unit, SHARED_LOCK);
    struct InfoData *id;
    char size[16], vol[64];

    if (!lock)
        return FALSE;
    id = AllocVec(sizeof(*id), MEMF_PUBLIC | MEMF_CLEAR);
    if (!id || !Info(lock, id)) {
        if (id)
            FreeVec(id);
        UnLock(lock);
        return FALSE;
    }
    volume_name(lock, vol, sizeof(vol));
    if (volname) {
        strncpy(volname, vol, vlen - 1);
        volname[vlen - 1] = '\0';
    }
    if (!volume_list) {
        ULONG nb = id->id_NumBlocks, bpb = id->id_BytesPerBlock;
        ULONG kb = (nb >> 10) * bpb + (((nb & 1023) * bpb) >> 10);
        LONG used = id->id_NumBlocksUsed, free = id->id_NumBlocks - id->id_NumBlocksUsed;
        LONG full = nb ? (LONG)((ULONG)id->id_NumBlocksUsed / ((nb + 99) / 100)) : 0;
        LONG a[1];
        if (args[A_BLOCKS])
            kb = id->id_NumBlocks;
        a[0] = kb;
        if (!args[A_BLOCKS] && kb >= 10000) {
            a[0] = kb / 1024;
            RawDoFmt((STRPTR)"%ldM", a, (void (*)())"\x16\xc0\x4e\x75", size);
        } else {
            RawDoFmt((STRPTR)(args[A_BLOCKS] ? "%ld" : "%ldK"), a, (void (*)())"\x16\xc0\x4e\x75", size);
        }
        Printf((STRPTR)"%-4s%10s%8ld%8ld %3ld%%%4ld  %-10s %s\n", (LONG)unit, (LONG)size, used, free, full,
               (LONG)id->id_NumSoftErrors,
               (LONG)(id->id_DiskState == ID_WRITE_PROTECTED ? "Read Only" :
                      id->id_DiskState == ID_VALIDATING ? "Validating" : "Read/Write"),
               (LONG)vol);
    }
    FreeVec(id);
    UnLock(lock);
    return TRUE;
}

int main(void)
{
    struct RDArgs *rda;
    static const char *units[] = { "RAM:", "SYS:", "DH0:", "DH1:", "DH2:", "DF0:", "DF1:", NULL };
    char vols[8][64];
    int nvols = 0, i;

    memset(args, 0, sizeof(args));
    rda = ReadArgs((STRPTR)TEMPLATE, args, NULL);
    if (!rda) {
        PrintFault(IoErr(), NULL);
        return RETURN_FAIL;
    }

    if (args[A_DEVICE]) {
        BPTR lock = Lock((STRPTR)args[A_DEVICE], SHARED_LOCK);
        if (lock) {
            UnLock(lock);
            PutStr((STRPTR)"\nMounted disks:\nUnit\t  Size\t  Used\t  Free Full Errs   Status   Name\n");
            if (show_unit((char *)args[A_DEVICE], FALSE, vols[0], sizeof(vols[0])) && vols[0][0])
                Printf((STRPTR)"\nVolumes available:\n%s [Mounted]\n", (LONG)vols[0]);
        }
        FreeArgs(rda);
        return 0;
    }

    if (!args[A_VOLS]) {
        PutStr((STRPTR)"\nMounted disks:\nUnit\t  Size\t  Used\t  Free Full Errs   Status   Name\n");
        for (i = 0; units[i]; i++)
            if (show_unit(units[i], FALSE, nvols < 8 ? vols[nvols] : NULL, 64) && nvols < 8)
                nvols++;
    } else {
        for (i = 0; units[i]; i++)
            if (nvols < 8 && show_unit(units[i], TRUE, vols[nvols], 64))
                nvols++;
    }
    if (!args[A_DEVICES]) {
        PutStr((STRPTR)"\nVolumes available:\n");
        for (i = 0; i < nvols; i++) {
            int j;
            BOOL dup = FALSE;
            for (j = 0; j < i; j++)
                if (!stricmp(vols[j], vols[i]))
                    dup = TRUE;
            if (!dup && vols[i][0])
                Printf((STRPTR)"%s [Mounted]\n", (LONG)vols[i]);
        }
    }
    FreeArgs(rda);
    return 0;
}
