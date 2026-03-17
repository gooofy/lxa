/*
 * lxa narrator.device implementation
 *
 * Hosted compatibility implementation with parameter validation, buffered
 * sync/mouth events, and translator-friendly phoneme stream handling.
 */

#include <exec/types.h>
#include <exec/execbase.h>
#include <exec/resident.h>
#include <exec/initializers.h>
#include <exec/io.h>
#include <exec/errors.h>
#include <exec/lists.h>
#include <exec/memory.h>
#include <clib/exec_protos.h>
#include <inline/exec.h>

#include <devices/newstyle.h>
#include <devices/narrator.h>

#include "util.h"

#define VERSION   40
#define REVISION  1
#define EXDEVNAME "narrator"
#define EXDEVVER  " 40.1 (2026/03/17)"

#define NARRATOR_UNIT_COUNT 1

#ifndef NDF_READMOUTH
#define NDF_READMOUTH 0x01
#define NDF_READWORD  0x02
#define NDF_READSYL   0x04
#endif

char __aligned _g_narrator_ExDevName [] = EXDEVNAME ".device";
char __aligned _g_narrator_ExDevID   [] = EXDEVNAME EXDEVVER;
char __aligned _g_narrator_Copyright [] = "(C)opyright 2026 by G. Bartsch. Licensed under the MIT License.";

char __aligned _g_narrator_VERSTRING [] = "\0$VER: " EXDEVNAME EXDEVVER;

extern struct ExecBase *SysBase;

struct NarratorEvent
{
    UBYTE ne_Width;
    UBYTE ne_Height;
    UBYTE ne_Sync;
};

struct NarratorUnit
{
    struct Unit nu_Unit;
    struct NarratorUnit *nu_Next;
    struct NarratorEvent *nu_Events;
    ULONG nu_EventCount;
    ULONG nu_EventIndex;
};

struct NarratorBase
{
    struct Device nb_Device;
    BPTR nb_SegList;
    struct NarratorUnit *nb_Units;
};

static const UWORD narrator_supported_commands[] =
{
    CMD_FLUSH,
    CMD_READ,
    CMD_RESET,
    CMD_WRITE,
    NSCMD_DEVICEQUERY,
    0
};

static void narrator_reply_request(struct IORequest *ioreq)
{
    if (!(ioreq->io_Flags & IOF_QUICK))
    {
        ReplyMsg(&ioreq->io_Message);
    }
}

static BOOL narrator_request_is_valid(const struct IORequest *ioreq)
{
    return ioreq && ioreq->io_Message.mn_Length >= sizeof(struct narrator_rb);
}

static BOOL narrator_request_has_mouth(const struct IORequest *ioreq)
{
    return ioreq && ioreq->io_Message.mn_Length >= sizeof(struct mouth_rb);
}

static struct NarratorUnit *narrator_get_unit(struct IORequest *ioreq)
{
    if (!ioreq)
    {
        return NULL;
    }

    return (struct NarratorUnit *)ioreq->io_Unit;
}

static void narrator_set_default_request(struct narrator_rb *io)
{
    UBYTE flags;

    if (!io)
    {
        return;
    }

    flags = io->flags & NDF_NEWIORB;

    io->rate = DEFRATE;
    io->pitch = DEFPITCH;
    io->mode = DEFMODE;
    io->sex = DEFSEX;
    io->ch_masks = NULL;
    io->nm_masks = 0;
    io->volume = DEFVOL;
    io->sampfreq = DEFFREQ;
    io->mouths = 0;
    io->chanmask = 0;
    io->numchan = 0;
    io->flags = flags;
    io->F0enthusiasm = DEFF0ENTHUS;
    io->F0perturb = DEFF0PERT;
    io->F1adj = 0;
    io->F2adj = 0;
    io->F3adj = 0;
    io->A1adj = 0;
    io->A2adj = 0;
    io->A3adj = 0;
    io->articulate = DEFARTIC;
    io->centralize = DEFCENTRAL;
    io->centphon = NULL;
    io->AVbias = 0;
    io->AFbias = 0;
    io->priority = DEFPRIORITY;
    io->pad1 = 0;
}

static void narrator_clear_events(struct NarratorUnit *unit)
{
    if (!unit)
    {
        return;
    }

    if (unit->nu_Events)
    {
        FreeMem(unit->nu_Events, sizeof(*unit->nu_Events) * unit->nu_EventCount);
        unit->nu_Events = NULL;
    }

    unit->nu_EventCount = 0;
    unit->nu_EventIndex = 0;
}

static BOOL narrator_is_word_char(UBYTE value)
{
    if (value >= 'A' && value <= 'Z')
    {
        return TRUE;
    }
    if (value >= 'a' && value <= 'z')
    {
        return TRUE;
    }
    if (value >= '0' && value <= '9')
    {
        return TRUE;
    }
    if (value == '\'' || value == '-')
    {
        return TRUE;
    }

    return FALSE;
}

static BOOL narrator_is_vowel(UBYTE value)
{
    switch (value)
    {
        case 'A': case 'E': case 'I': case 'O': case 'U': case 'Y':
        case 'a': case 'e': case 'i': case 'o': case 'u': case 'y':
            return TRUE;
    }

    return FALSE;
}

static ULONG narrator_effective_length(const UBYTE *text, ULONG length)
{
    ULONG count = 0;

    while (count < length && text[count] != 0 && text[count] != '#')
    {
        count++;
    }

    return count;
}

static void narrator_set_mouth_shape(UBYTE value, UBYTE *width, UBYTE *height)
{
    if (narrator_is_vowel(value))
    {
        *width = 9;
        *height = 7;
    }
    else if (value == 'W' || value == 'w' || value == 'O' || value == 'o')
    {
        *width = 7;
        *height = 8;
    }
    else
    {
        *width = 5;
        *height = 4;
    }
}

static LONG narrator_build_events(struct NarratorUnit *unit,
                                  const UBYTE *text,
                                  ULONG length,
                                  BOOL want_mouth,
                                  BOOL want_word,
                                  BOOL want_syllable)
{
    struct NarratorEvent *events;
    ULONG index;
    BOOL in_word = FALSE;
    BOOL syllable_ready = TRUE;

    narrator_clear_events(unit);

    if (!want_mouth && !want_word && !want_syllable)
    {
        return 0;
    }

    events = (struct NarratorEvent *)AllocMem(sizeof(*events) * length, MEMF_PUBLIC | MEMF_CLEAR);
    if (!events)
    {
        return ND_NoMem;
    }

    unit->nu_Events = events;
    unit->nu_EventCount = 0;
    unit->nu_EventIndex = 0;

    for (index = 0; index < length; index++)
    {
        UBYTE value = text[index];
        UBYTE sync = 0;
        UBYTE width = 0;
        UBYTE height = 0;

        if (!narrator_is_word_char(value))
        {
            in_word = FALSE;
            syllable_ready = TRUE;
            continue;
        }

        if (!in_word)
        {
            in_word = TRUE;
            syllable_ready = TRUE;
            if (want_word)
            {
                sync |= NDF_READWORD;
            }
        }

        if (want_syllable && syllable_ready)
        {
            sync |= NDF_READSYL;
            syllable_ready = FALSE;
        }
        else if (narrator_is_vowel(value))
        {
            syllable_ready = FALSE;
        }

        if (want_mouth)
        {
            sync |= NDF_READMOUTH;
            narrator_set_mouth_shape(value, &width, &height);
        }

        if (sync != 0)
        {
            events[unit->nu_EventCount].ne_Width = width;
            events[unit->nu_EventCount].ne_Height = height;
            events[unit->nu_EventCount].ne_Sync = sync;
            unit->nu_EventCount++;
        }
    }

    return 0;
}

static LONG narrator_validate_write_request(const struct narrator_rb *io)
{
    if (io->rate < MINRATE || io->rate > MAXRATE)
    {
        return ND_RateErr;
    }
    if (io->pitch < MINPITCH || io->pitch > MAXPITCH)
    {
        return ND_PitchErr;
    }
    if (io->sex != MALE && io->sex != FEMALE)
    {
        return ND_SexErr;
    }
    if (io->mode != NATURALF0 && io->mode != ROBOTICF0 && io->mode != MANUALF0)
    {
        return ND_ModeErr;
    }
    if (io->sampfreq < MINFREQ || io->sampfreq > MAXFREQ)
    {
        return ND_FreqErr;
    }
    if (io->volume < MINVOL || io->volume > MAXVOL)
    {
        return ND_VolErr;
    }
    if (io->centralize < MINCENT || io->centralize > MAXCENT)
    {
        return ND_DCentErr;
    }
    if (io->centralize != 0 && (!io->centphon || io->centphon[0] == 0))
    {
        return ND_CentPhonErr;
    }
    if ((io->flags & ~(NDF_NEWIORB | NDF_WORDSYNC | NDF_SYLSYNC)) != 0)
    {
        return ND_Unimpl;
    }

    return 0;
}

static BPTR narrator_expunge_if_possible(struct NarratorBase *narratorbase)
{
    struct NarratorUnit *unit;
    struct NarratorUnit *next;

    if (FindName(&SysBase->DeviceList,
                 (CONST_STRPTR)narratorbase->nb_Device.dd_Library.lib_Node.ln_Name) ==
        &narratorbase->nb_Device.dd_Library.lib_Node)
    {
        Remove(&narratorbase->nb_Device.dd_Library.lib_Node);
    }

    if (narratorbase->nb_Device.dd_Library.lib_OpenCnt != 0)
    {
        narratorbase->nb_Device.dd_Library.lib_Flags |= LIBF_DELEXP;
        return 0;
    }

    narratorbase->nb_Device.dd_Library.lib_Flags &= ~LIBF_DELEXP;

    for (unit = narratorbase->nb_Units; unit; unit = next)
    {
        next = unit->nu_Next;
        narrator_clear_events(unit);
        FreeMem(unit, sizeof(*unit));
    }

    narratorbase->nb_Units = NULL;
    return narratorbase->nb_SegList;
}

static struct Library * __g_lxa_narrator_InitDev(register struct Library *dev __asm("d0"),
                                                  register BPTR seglist __asm("a0"),
                                                  register struct ExecBase *sysb __asm("a6"))
{
    struct NarratorBase *narratorbase = (struct NarratorBase *)dev;

    (void)sysb;

    narratorbase->nb_SegList = seglist;
    narratorbase->nb_Units = NULL;
    return dev;
}

static void __g_lxa_narrator_Open(register struct Library *dev __asm("a6"),
                                  register struct IORequest *ioreq __asm("a1"),
                                  register ULONG unit __asm("d0"),
                                  register ULONG flags __asm("d1"))
{
    struct NarratorBase *narratorbase = (struct NarratorBase *)dev;
    struct narrator_rb *io = (struct narrator_rb *)ioreq;
    struct NarratorUnit *narratorunit;

    (void)flags;

    ioreq->io_Error = 0;
    ioreq->io_Device = NULL;
    ioreq->io_Unit = NULL;

    if ((dev->lib_Flags & LIBF_DELEXP) != 0 || unit >= NARRATOR_UNIT_COUNT || !narrator_request_is_valid(ioreq))
    {
        ioreq->io_Error = IOERR_OPENFAIL;
        return;
    }

    narratorunit = (struct NarratorUnit *)AllocMem(sizeof(*narratorunit), MEMF_PUBLIC | MEMF_CLEAR);
    if (!narratorunit)
    {
        ioreq->io_Error = ND_NoMem;
        return;
    }

    narratorunit->nu_Unit.unit_OpenCnt = 1;
    narratorunit->nu_Next = narratorbase->nb_Units;
    narratorbase->nb_Units = narratorunit;

    ioreq->io_Device = (struct Device *)narratorbase;
    ioreq->io_Unit = (struct Unit *)narratorunit;
    narrator_set_default_request(io);
    dev->lib_OpenCnt++;
    dev->lib_Flags &= ~LIBF_DELEXP;
}

static BPTR __g_lxa_narrator_Close(register struct Library *dev __asm("a6"),
                                   register struct IORequest *ioreq __asm("a1"))
{
    struct NarratorBase *narratorbase = (struct NarratorBase *)dev;
    struct NarratorUnit *narratorunit = narrator_get_unit(ioreq);
    struct NarratorUnit **link;

    if (narratorunit)
    {
        for (link = &narratorbase->nb_Units; *link; link = &(*link)->nu_Next)
        {
            if (*link == narratorunit)
            {
                *link = narratorunit->nu_Next;
                break;
            }
        }

        narrator_clear_events(narratorunit);
        FreeMem(narratorunit, sizeof(*narratorunit));
    }

    ioreq->io_Device = NULL;
    ioreq->io_Unit = NULL;

    if (narratorbase->nb_Device.dd_Library.lib_OpenCnt > 0)
    {
        narratorbase->nb_Device.dd_Library.lib_OpenCnt--;
    }

    if (narratorbase->nb_Device.dd_Library.lib_OpenCnt == 0 &&
        (narratorbase->nb_Device.dd_Library.lib_Flags & LIBF_DELEXP))
    {
        return narrator_expunge_if_possible(narratorbase);
    }

    return 0;
}

static BPTR __g_lxa_narrator_Expunge(register struct Library *dev __asm("a6"))
{
    return narrator_expunge_if_possible((struct NarratorBase *)dev);
}

static BPTR __g_lxa_narrator_BeginIO(register struct Library *dev __asm("a6"),
                                     register struct IORequest *ioreq __asm("a1"))
{
    struct NarratorUnit *narratorunit = narrator_get_unit(ioreq);
    struct narrator_rb *voice = (struct narrator_rb *)ioreq;
    struct IOStdReq *iostd = (struct IOStdReq *)ioreq;
    LONG result = 0;

    (void)dev;

    ioreq->io_Message.mn_Node.ln_Type = NT_MESSAGE;
    ioreq->io_Error = 0;
    iostd->io_Actual = 0;

    if (!narratorunit)
    {
        ioreq->io_Error = IOERR_OPENFAIL;
        narrator_reply_request(ioreq);
        return 0;
    }

    switch (ioreq->io_Command)
    {
        case NSCMD_DEVICEQUERY:
        {
            struct NSDeviceQueryResult *query = (struct NSDeviceQueryResult *)iostd->io_Data;

            if (!query)
            {
                result = IOERR_BADADDRESS;
                break;
            }
            if (iostd->io_Length < sizeof(*query))
            {
                result = IOERR_BADLENGTH;
                break;
            }

            query->nsdqr_DevQueryFormat = 0;
            query->nsdqr_SizeAvailable = sizeof(*query);
            query->nsdqr_DeviceType = NSDEVTYPE_UNKNOWN;
            query->nsdqr_DeviceSubType = 0;
            query->nsdqr_SupportedCommands = (APTR)narrator_supported_commands;
            iostd->io_Actual = sizeof(*query);
            break;
        }

        case CMD_FLUSH:
            narrator_clear_events(narratorunit);
            break;

        case CMD_RESET:
            narrator_clear_events(narratorunit);
            narrator_set_default_request(voice);
            break;

        case CMD_WRITE:
        {
            const UBYTE *text = (const UBYTE *)iostd->io_Data;
            ULONG length;

            if (!text && iostd->io_Length != 0)
            {
                result = IOERR_BADADDRESS;
                break;
            }

            result = narrator_validate_write_request(voice);
            if (result != 0)
            {
                break;
            }

            length = narrator_effective_length(text, iostd->io_Length);
            iostd->io_Actual = length;
            result = narrator_build_events(narratorunit,
                                           text,
                                           length,
                                           voice->mouths != 0,
                                           (voice->flags & NDF_WORDSYNC) != 0,
                                           (voice->flags & NDF_SYLSYNC) != 0);
            break;
        }

        case CMD_READ:
        {
            struct mouth_rb *mouth = (struct mouth_rb *)ioreq;

            if (!narrator_request_has_mouth(ioreq))
            {
                result = IOERR_BADLENGTH;
                break;
            }

            if (narratorunit->nu_EventIndex >= narratorunit->nu_EventCount)
            {
                result = ND_NoWrite;
                break;
            }

            mouth->width = narratorunit->nu_Events[narratorunit->nu_EventIndex].ne_Width;
            mouth->height = narratorunit->nu_Events[narratorunit->nu_EventIndex].ne_Height;
            mouth->shape = 0;
            mouth->sync = narratorunit->nu_Events[narratorunit->nu_EventIndex].ne_Sync;
            narratorunit->nu_EventIndex++;
            iostd->io_Actual = 1;
            break;
        }

        default:
            result = ND_Unimpl;
            break;
    }

    ioreq->io_Error = result;
    narrator_reply_request(ioreq);
    return 0;
}

static ULONG __g_lxa_narrator_AbortIO(register struct Library *dev __asm("a6"),
                                      register struct IORequest *ioreq __asm("a1"))
{
    (void)dev;
    (void)ioreq;

    return (ULONG)-1;
}

struct MyDataInit
{
    UWORD ln_Type_Init     ; UWORD ln_Type_Offset     ; UWORD ln_Type_Content     ;
    UBYTE ln_Name_Init     ; UBYTE ln_Name_Offset     ; ULONG ln_Name_Content     ;
    UWORD lib_Flags_Init   ; UWORD lib_Flags_Offset   ; UWORD lib_Flags_Content   ;
    UWORD lib_Version_Init ; UWORD lib_Version_Offset ; UWORD lib_Version_Content ;
    UWORD lib_Revision_Init; UWORD lib_Revision_Offset; UWORD lib_Revision_Content;
    UBYTE lib_IdString_Init; UBYTE lib_IdString_Offset; ULONG lib_IdString_Content;
    ULONG ENDMARK;
};

extern APTR              __g_lxa_narrator_FuncTab [];
extern struct MyDataInit __g_lxa_narrator_DataTab;
extern struct InitTable  __g_lxa_narrator_InitTab;
extern APTR              __g_lxa_narrator_EndResident;

static struct Resident __aligned ROMTag =
{
    RTC_MATCHWORD,
    &ROMTag,
    &__g_lxa_narrator_EndResident,
    RTF_AUTOINIT,
    VERSION,
    NT_DEVICE,
    0,
    &_g_narrator_ExDevName[0],
    &_g_narrator_ExDevID[0],
    &__g_lxa_narrator_InitTab
};

APTR __g_lxa_narrator_EndResident;
struct Resident *__lxa_narrator_ROMTag = &ROMTag;

struct InitTable __g_lxa_narrator_InitTab =
{
    (ULONG)sizeof(struct NarratorBase),
    (APTR *)&__g_lxa_narrator_FuncTab[0],
    (APTR)&__g_lxa_narrator_DataTab,
    (APTR)__g_lxa_narrator_InitDev
};

APTR __g_lxa_narrator_FuncTab [] =
{
    __g_lxa_narrator_Open,
    __g_lxa_narrator_Close,
    __g_lxa_narrator_Expunge,
    NULL,
    __g_lxa_narrator_BeginIO,
    __g_lxa_narrator_AbortIO,
    (APTR)((LONG)-1)
};

struct MyDataInit __g_lxa_narrator_DataTab =
{
    /* ln_Type      */ INITBYTE(OFFSET(Node,         ln_Type),      NT_DEVICE),
    /* ln_Name      */ 0x80, (UBYTE)(ULONG)OFFSET(Node,    ln_Name), (ULONG)&_g_narrator_ExDevName[0],
    /* lib_Flags    */ INITBYTE(OFFSET(Library,      lib_Flags),    LIBF_SUMUSED|LIBF_CHANGED),
    /* lib_Version  */ INITWORD(OFFSET(Library,      lib_Version),  VERSION),
    /* lib_Revision */ INITWORD(OFFSET(Library,      lib_Revision), REVISION),
    /* lib_IdString */ 0x80, (UBYTE)(ULONG)OFFSET(Library, lib_IdString), (ULONG)&_g_narrator_ExDevID[0],
    (ULONG)0
};
