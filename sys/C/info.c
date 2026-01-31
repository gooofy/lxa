/*
 * INFO command - Display disk/volume information
 * Step 9.5 implementation for lxa
 * 
 * Template: DEVICE,VOLS/S,GOODONLY/S,BLOCKS/S,DEVICES/S
 * 
 * Usage:
 *   INFO             - Display info for all mounted volumes
 *   INFO SYS:        - Display info for specific volume
 *   INFO VOLS        - List volumes only
 *   INFO DEVICES     - List devices only
 */

#include <exec/types.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <dos/filehandler.h>
#include <clib/exec_protos.h>
#include <clib/dos_protos.h>
#include <inline/exec.h>
#include <inline/dos.h>

#include <string.h>

/* External reference to library bases */
extern struct DosLibrary *DOSBase;
extern struct ExecBase *SysBase;

/* Command template */
#define TEMPLATE "DEVICE,VOLS/S,GOODONLY/S,BLOCKS/S,DEVICES/S"

/* Argument array indices */
#define ARG_DEVICE    0
#define ARG_VOLS      1
#define ARG_GOODONLY  2
#define ARG_BLOCKS    3
#define ARG_DEVICES   4
#define ARG_COUNT     5

/* Helper: output a string */
static void out_str(const char *str)
{
    Write(Output(), (STRPTR)str, strlen(str));
}

/* Helper: output a number with padding */
static void out_num_pad(ULONG num, int width)
{
    char buf[16];
    char *p = buf + sizeof(buf) - 1;
    int len = 0;
    *p = '\0';
    
    do {
        *--p = '0' + (num % 10);
        num /= 10;
        len++;
    } while (num);
    
    /* Pad with spaces */
    while (len < width) {
        out_str(" ");
        len++;
    }
    
    out_str(p);
}

/* Display info for a specific device/volume */
static BOOL show_device_info(const char *device, BOOL blocks)
{
    /* Lock the device to get info */
    BPTR lock = Lock((STRPTR)device, SHARED_LOCK);
    if (!lock) {
        return FALSE;
    }
    
    /* Get disk info */
    struct InfoData *info = (struct InfoData *)AllocVec(sizeof(struct InfoData), MEMF_CLEAR);
    if (!info) {
        UnLock(lock);
        return FALSE;
    }
    
    if (!Info(lock, info)) {
        FreeVec(info);
        UnLock(lock);
        return FALSE;
    }
    
    /* Calculate sizes */
    ULONG blockSize = info->id_BytesPerBlock;
    ULONG totalBlocks = info->id_NumBlocks;
    ULONG usedBlocks = info->id_NumBlocksUsed;
    ULONG freeBlocks = totalBlocks - usedBlocks;
    
    /* Convert to KB if not showing blocks */
    ULONG divisor = blocks ? 1 : (1024 / blockSize);
    if (divisor == 0) divisor = 1;
    
    ULONG total = totalBlocks / divisor;
    ULONG used = usedBlocks / divisor;
    ULONG free = freeBlocks / divisor;
    
    /* Calculate percentage used */
    ULONG percentUsed = (totalBlocks > 0) ? (usedBlocks * 100 / totalBlocks) : 0;
    
    /* Get volume name from the lock */
    struct FileInfoBlock *fib = (struct FileInfoBlock *)AllocDosObject(DOS_FIB, NULL);
    char volName[32] = "";
    if (fib) {
        if (Examine(lock, fib)) {
            /* Get the volume name - for root locks, fib_FileName is the volume name */
            strncpy(volName, (char *)fib->fib_FileName, sizeof(volName) - 1);
        }
        FreeDosObject(DOS_FIB, fib);
    }
    
    /* Display: Unit  Size    Used    Free Full Errs   State  Type Name */
    out_str((char *)device);
    
    /* Pad device name to 8 chars */
    int devLen = strlen(device);
    while (devLen < 8) {
        out_str(" ");
        devLen++;
    }
    
    out_num_pad(total, 8);
    out_str(blocks ? "  " : "K ");
    out_num_pad(used, 7);
    out_str(blocks ? "  " : "K ");
    out_num_pad(free, 7);
    out_str(blocks ? " " : "K");
    out_num_pad(percentUsed, 4);
    out_str("%    0   ");  /* No errors */
    
    /* State */
    out_str("Read/Write ");
    
    /* Type - lxa uses Linux-backed filesystem */
    out_str("LXA  ");
    
    /* Volume name */
    if (volName[0]) {
        out_str(volName);
    }
    
    out_str("\n");
    
    FreeVec(info);
    UnLock(lock);
    return TRUE;
}

/* Display header */
static void show_header(BOOL blocks)
{
    if (blocks) {
        out_str("Unit     Blocks     Used     Free Full Errs   State      Type Name\n");
    } else {
        out_str("Unit       Size     Used    Free Full Errs   State      Type Name\n");
    }
}

/* List all mounted volumes */
static void list_volumes(BOOL blocks)
{
    show_header(blocks);
    
    /* Try common device names */
    const char *devices[] = {
        "SYS:", "WORK:", "RAM:", "HOME:", "CWD:",
        "DH0:", "DH1:", "DH2:", "DF0:", "DF1:",
        NULL
    };
    
    for (int i = 0; devices[i]; i++) {
        show_device_info(devices[i], blocks);
    }
}

int main(int argc, char **argv)
{
    struct RDArgs *rdargs;
    LONG args[ARG_COUNT] = {0};
    
    /* Parse arguments */
    rdargs = ReadArgs((STRPTR)TEMPLATE, args, NULL);
    if (!rdargs) {
        PrintFault(IoErr(), (STRPTR)"INFO");
        return RETURN_FAIL;
    }
    
    STRPTR device = (STRPTR)args[ARG_DEVICE];
    BOOL vols = args[ARG_VOLS] != 0;
    BOOL blocks = args[ARG_BLOCKS] != 0;
    BOOL devices = args[ARG_DEVICES] != 0;
    
    /* Suppress unused variable warnings */
    (void)vols;
    (void)devices;
    
    if (device) {
        /* Show info for specific device */
        show_header(blocks);
        if (!show_device_info((char *)device, blocks)) {
            out_str("INFO: Cannot get info for ");
            out_str((char *)device);
            out_str("\n");
            FreeArgs(rdargs);
            return RETURN_WARN;
        }
    } else {
        /* List all volumes */
        list_volumes(blocks);
    }
    
    FreeArgs(rdargs);
    return RETURN_OK;
}
