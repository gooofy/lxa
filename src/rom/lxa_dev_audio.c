/*
 * lxa audio.device implementation
 *
 * Hosted compatibility implementation for channel allocation, timed playback,
 * wait-cycle/finish/pervol requests, and audio interrupt delivery.
 */

#include <exec/execbase.h>
#include <exec/resident.h>
#include <exec/initializers.h>
#include <exec/io.h>
#include <exec/memory.h>
#include <exec/errors.h>
#include <exec/interrupts.h>
#include <exec/lists.h>
#include <clib/exec_protos.h>
#include <inline/exec.h>

#include <devices/audio.h>
#include <devices/timer.h>
#include <hardware/intbits.h>

#include "util.h"

#define VERSION    37
#define REVISION   2
#define EXDEVNAME  "audio"
#define EXDEVVER   " 37.2 (2026/03/10)"

#define AUDIO_CHANNELS 4
#define AUDIO_MAX_MASK 0x0f
#define AUDIO_INT_VECTOR(ch) (INTB_AUD0 + (ch))

char __aligned _g_audio_ExDevName [] = EXDEVNAME ".device";
char __aligned _g_audio_ExDevID   [] = EXDEVNAME EXDEVVER;
char __aligned _g_audio_Copyright [] = "(C)opyright 2026 by G. Bartsch. Licensed under the MIT License.";

char __aligned _g_audio_VERSTRING [] = "\0$VER: " EXDEVNAME EXDEVVER;

extern struct ExecBase *SysBase;

struct AudioChannel
{
    struct List  ac_WriteList;
    WORD         ac_AllocKey;
    BYTE         ac_Precedence;
    UBYTE        ac_Channel;
    BOOL         ac_Stopped;
    BOOL         ac_Playing;
    UWORD        ac_Period;
    UWORD        ac_Volume;
    UWORD        ac_CyclesRemaining;
    ULONG        ac_CycleMicros;
    struct timeval ac_CycleDeadline;
};

struct AudioPrivate
{
    struct List         ap_PendingAllocations;
    struct List         ap_MiscRequests;
    WORD                ap_KeyGen;
    struct AudioChannel ap_Channels[AUDIO_CHANNELS];
};

struct AudioBase
{
    struct Device  ab_Device;
    BPTR           ab_SegList;
    struct AudioPrivate ab_Private;
};

static struct AudioPrivate *audio_private(struct AudioBase *audio)
{
    return &audio->ab_Private;
}

static struct List *audio_pending_allocations(struct AudioBase *audio)
{
    return &audio_private(audio)->ap_PendingAllocations;
}

static struct List *audio_misc_requests(struct AudioBase *audio)
{
    return &audio_private(audio)->ap_MiscRequests;
}

static struct AudioChannel *audio_channel_state(struct AudioBase *audio, UBYTE channel)
{
    return &audio_private(audio)->ap_Channels[channel];
}

static void audio_get_raw_time(struct timeval *tv)
{
    emucall1(EMU_CALL_GETSYSTIME, (ULONG)tv);
}

static void audio_normalize_timeval(struct timeval *tv)
{
    while (tv->tv_micro >= 1000000)
    {
        tv->tv_secs++;
        tv->tv_micro -= 1000000;
    }
}

static void audio_add_micros(struct timeval *tv, ULONG micros)
{
    tv->tv_secs += micros / 1000000UL;
    tv->tv_micro += micros % 1000000UL;
    audio_normalize_timeval(tv);
}

static LONG audio_cmp_time(const struct timeval *a, const struct timeval *b)
{
    if (a->tv_secs < b->tv_secs)
    {
        return -1;
    }
    if (a->tv_secs > b->tv_secs)
    {
        return 1;
    }
    if (a->tv_micro < b->tv_micro)
    {
        return -1;
    }
    if (a->tv_micro > b->tv_micro)
    {
        return 1;
    }
    return 0;
}

static void audio_u64_add_u32(ULONG *hi, ULONG *lo, ULONG value)
{
    ULONG old_lo = *lo;

    *lo += value;
    if (*lo < old_lo)
    {
        (*hi)++;
    }
}

static void audio_mul_u32_u32_to_u64(ULONG a, ULONG b, ULONG *hi, ULONG *lo)
{
    ULONG a_lo = a & 0xffff;
    ULONG a_hi = a >> 16;
    ULONG b_lo = b & 0xffff;
    ULONG b_hi = b >> 16;
    ULONG p0 = a_lo * b_lo;
    ULONG p1 = a_lo * b_hi;
    ULONG p2 = a_hi * b_lo;
    ULONG p3 = a_hi * b_hi;
    ULONG carry;

    *lo = p0;
    *hi = p3;

    carry = (p1 & 0xffff) << 16;
    audio_u64_add_u32(hi, lo, carry);
    *hi += p1 >> 16;

    carry = (p2 & 0xffff) << 16;
    audio_u64_add_u32(hi, lo, carry);
    *hi += p2 >> 16;
}

static void audio_divmod_u64_u32(ULONG num_hi,
                                 ULONG num_lo,
                                 ULONG denom,
                                 ULONG *quot,
                                 ULONG *rem)
{
    ULONG q = 0;
    ULONG r = 0;
    int bit;

    for (bit = 31; bit >= 0; bit--)
    {
        r = (r << 1) | ((num_hi >> bit) & 1);
        if (r >= denom)
        {
            r -= denom;
            q |= (1UL << bit);
        }
    }

    for (bit = 31; bit >= 0; bit--)
    {
        r = (r << 1) | ((num_lo >> bit) & 1);
        if (r >= denom)
        {
            r -= denom;
            q |= (1UL << bit);
        }
    }

    *quot = q;
    *rem = r;
}

static ULONG audio_compute_cycle_micros(struct IOAudio *io)
{
    ULONG product_hi;
    ULONG product_lo;
    ULONG micros;
    ULONG remainder;
    ULONG period;
    ULONG length;

    period = io->ioa_Period;
    length = io->ioa_Length;

    if (period < 124)
    {
        period = 124;
    }
    if (length < 2)
    {
        length = 2;
    }

    audio_mul_u32_u32_to_u64(length, period, &product_hi, &product_lo);
    audio_divmod_u64_u32(product_hi, product_lo, 3579545UL, &micros, &remainder);
    if (remainder != 0)
    {
        micros++;
    }
    if (micros == 0)
    {
        micros = 1;
    }

    return micros;
}

static WORD audio_next_key(struct AudioBase *audio)
{
    do
    {
        audio_private(audio)->ap_KeyGen++;
    }
    while (audio_private(audio)->ap_KeyGen == 0);

    return audio_private(audio)->ap_KeyGen;
}

static struct IOAudio *audio_list_head(struct List *list)
{
    return (struct IOAudio *)GETHEAD(list);
}

static BOOL audio_request_is_on_list(struct List *list, struct IOAudio *io)
{
    struct Node *node;

    for (node = GETHEAD(list); node; node = GETSUCC(node))
    {
        if (node == (struct Node *)io)
        {
            return TRUE;
        }
    }

    return FALSE;
}

static void audio_reply_request(struct IOAudio *io, BYTE error)
{
    io->ioa_Request.io_Error = error;
    ReplyMsg(&io->ioa_Request.io_Message);
}

static void audio_call_interrupt(APTR handler, APTR data)
{
    if (!handler)
    {
        return;
    }

    __asm volatile (
        "move.l  a5,-(sp)\n\t"
        "move.l  %0,a5\n\t"
        "move.l  %1,a1\n\t"
        "jsr     (a5)\n\t"
        "move.l  (sp)+,a5"
        :
        : "r" (handler), "r" (data)
        : "a0", "a1", "d0", "d1", "memory"
    );
}

static void audio_dispatch_irq(UBYTE channel)
{
    UBYTE vector_num = AUDIO_INT_VECTOR(channel);
    struct IntVector *iv;
    struct Interrupt *handler;
    struct Interrupt *server;
    APTR code;

    if (vector_num >= 16)
    {
        return;
    }

    iv = &SysBase->IntVects[vector_num];
    code = iv->iv_Code;
    handler = (struct Interrupt *)iv->iv_Node;

    if (handler && code && (ULONG)code != ~0UL)
    {
        audio_call_interrupt(code, handler->is_Data);
    }

    for (server = (struct Interrupt *)GETHEAD((struct List *)iv->iv_Data);
         server;
         server = (struct Interrupt *)GETSUCC(server))
    {
        audio_call_interrupt(server->is_Code, server->is_Data);
    }
}

static void audio_host_start_write(UBYTE channel, struct IOAudio *io)
{
    ULONG packed = ((ULONG)io->ioa_Cycles << 8) | (ULONG)channel;

    if (!io->ioa_Data || io->ioa_Length < 2)
    {
        return;
    }

    emucall5(EMU_CALL_AUDIO_PLAY,
             packed,
             (ULONG)io->ioa_Data,
             io->ioa_Length,
             io->ioa_Period,
             io->ioa_Volume);
}

static UBYTE audio_lowest_channel(UBYTE mask)
{
    UBYTE ch;

    for (ch = 0; ch < AUDIO_CHANNELS; ch++)
    {
        if (mask & (1U << ch))
        {
            return ch;
        }
    }

    return AUDIO_CHANNELS;
}

static BOOL audio_key_matches(struct AudioChannel *channel, WORD key)
{
    return channel->ac_AllocKey != 0 && channel->ac_AllocKey == key;
}

static void audio_set_request_mask(struct IOAudio *io, UBYTE mask)
{
    io->ioa_Request.io_Unit = (struct Unit *)(ULONG)mask;
}

static void audio_clear_request_mask(struct IOAudio *io)
{
    io->ioa_Request.io_Unit = NULL;
}

static void audio_start_channel_if_possible(struct AudioBase *audio,
                                            struct AudioChannel *channel);

static void audio_abort_misc_for_mask(struct AudioBase *audio, UBYTE mask, BYTE error)
{
    struct IOAudio *io;
    struct IOAudio *next;

    for (io = (struct IOAudio *)GETHEAD(audio_misc_requests(audio));
         io;
         io = next)
    {
        next = (struct IOAudio *)GETSUCC(io);
        if (((UBYTE)(ULONG)io->ioa_Request.io_Unit & AUDIO_MAX_MASK) & mask)
        {
            Remove((struct Node *)io);
            audio_reply_request(io, error);
        }
    }
}

static void audio_complete_current_write(struct AudioBase *audio,
                                         struct AudioChannel *channel,
                                         BYTE error)
{
    struct IOAudio *io = audio_list_head(&channel->ac_WriteList);

    if (!io)
    {
        channel->ac_Playing = FALSE;
        return;
    }

    Remove((struct Node *)io);
    channel->ac_Playing = FALSE;
    audio_reply_request(io, error);

    if (GETHEAD(&channel->ac_WriteList))
    {
        audio_start_channel_if_possible(audio, channel);
    }
}

static void audio_abort_channel(struct AudioBase *audio,
                                struct AudioChannel *channel,
                                BYTE error)
{
    struct IOAudio *io;
    struct IOAudio *next;
    UBYTE mask = (1U << channel->ac_Channel);

    for (io = (struct IOAudio *)GETHEAD(&channel->ac_WriteList);
         io;
         io = next)
    {
        next = (struct IOAudio *)GETSUCC(io);
        Remove((struct Node *)io);
        audio_reply_request(io, error);
    }

    audio_abort_misc_for_mask(audio, mask, error);
    channel->ac_Playing = FALSE;
    channel->ac_Stopped = FALSE;
    channel->ac_CyclesRemaining = 0;
    channel->ac_CycleMicros = 0;
    channel->ac_CycleDeadline.tv_secs = 0;
    channel->ac_CycleDeadline.tv_micro = 0;
}

static void audio_release_mask(struct AudioBase *audio, UBYTE mask)
{
    UBYTE ch;

    for (ch = 0; ch < AUDIO_CHANNELS; ch++)
    {
        if (mask & (1U << ch))
        {
    audio_abort_channel(audio, audio_channel_state(audio, ch), IOERR_ABORTED);
    audio_channel_state(audio, ch)->ac_AllocKey = 0;
    audio_channel_state(audio, ch)->ac_Precedence = ADALLOC_MINPREC;
        }
    }
}

static void audio_try_pending_allocations(struct AudioBase *audio);

static void audio_free_mask(struct AudioBase *audio, UBYTE mask)
{
    audio_release_mask(audio, mask);
    audio_try_pending_allocations(audio);
}

static void audio_steal_mask(struct AudioBase *audio, UBYTE mask)
{
    audio_release_mask(audio, mask);
}

static void audio_start_channel_if_possible(struct AudioBase *audio,
                                            struct AudioChannel *channel)
{
    struct IOAudio *io;

    if (channel->ac_Stopped || channel->ac_Playing)
    {
        return;
    }

    io = audio_list_head(&channel->ac_WriteList);
    if (!io)
    {
        return;
    }

    if ((io->ioa_Request.io_Flags & ADIOF_PERVOL) || channel->ac_Period == 0)
    {
        channel->ac_Period = io->ioa_Period;
        channel->ac_Volume = io->ioa_Volume;
    }

    channel->ac_CycleMicros = audio_compute_cycle_micros(io);
    channel->ac_CyclesRemaining = io->ioa_Cycles;
    audio_get_raw_time(&channel->ac_CycleDeadline);
    audio_add_micros(&channel->ac_CycleDeadline, channel->ac_CycleMicros);
    channel->ac_Playing = TRUE;

    if ((io->ioa_Request.io_Flags & ADIOF_WRITEMESSAGE) &&
        io->ioa_WriteMsg.mn_ReplyPort)
    {
        ReplyMsg(&io->ioa_WriteMsg);
    }

    audio_host_start_write(channel->ac_Channel, io);
}

static void audio_finish_immediately(struct AudioBase *audio,
                                     struct AudioChannel *channel)
{
    if (!audio_list_head(&channel->ac_WriteList))
    {
        return;
    }

    audio_complete_current_write(audio, channel, 0);
}

static UBYTE audio_validate_mask(struct AudioBase *audio, UBYTE requested_mask, WORD key)
{
    UBYTE matched = 0;
    UBYTE ch;

    for (ch = 0; ch < AUDIO_CHANNELS; ch++)
    {
        if ((requested_mask & (1U << ch)) && audio_key_matches(audio_channel_state(audio, ch), key))
        {
            matched |= (1U << ch);
        }
    }

    return matched;
}

static BOOL audio_choose_allocation(struct AudioBase *audio,
                                    struct IOAudio *io,
                                    UBYTE *selected_mask,
                                    UBYTE *stolen_mask)
{
    BYTE precedence = io->ioa_Request.io_Message.mn_Node.ln_Pri;
    UBYTE best_mask = 0;
    UBYTE best_stolen = 0;
    ULONG best_cost = 0xffffffffUL;
    UBYTE index;

    *selected_mask = 0;
    *stolen_mask = 0;

    for (index = 0; index < io->ioa_Length && index < 16; index++)
    {
        UBYTE mask = io->ioa_Data[index] & AUDIO_MAX_MASK;
        UBYTE ch;
        BOOL ok = TRUE;
        UBYTE steal = 0;
        ULONG cost = 0;

        if (mask == 0)
        {
            *selected_mask = 0;
            *stolen_mask = 0;
            return TRUE;
        }

        for (ch = 0; ch < AUDIO_CHANNELS; ch++)
        {
            struct AudioChannel *channel = audio_channel_state(audio, ch);

            if (!(mask & (1U << ch)))
            {
                continue;
            }

            if (channel->ac_AllocKey == 0)
            {
                continue;
            }

            if (channel->ac_Precedence >= precedence)
            {
                ok = FALSE;
                break;
            }

            steal |= (1U << ch);
            cost += (ULONG)((LONG)channel->ac_Precedence - ADALLOC_MINPREC);
        }

        if (!ok)
        {
            continue;
        }

        if (steal == 0)
        {
            *selected_mask = mask;
            *stolen_mask = 0;
            return TRUE;
        }

        if (cost < best_cost)
        {
            best_cost = cost;
            best_mask = mask;
            best_stolen = steal;
        }
    }

    if (best_mask != 0)
    {
        *selected_mask = best_mask;
        *stolen_mask = best_stolen;
        return TRUE;
    }

    return FALSE;
}

static void audio_commit_allocation(struct AudioBase *audio,
                                    struct IOAudio *io,
                                    UBYTE mask)
{
    WORD key = io->ioa_AllocKey;
    BYTE precedence = io->ioa_Request.io_Message.mn_Node.ln_Pri;
    UBYTE ch;

    if (key == 0)
    {
        key = audio_next_key(audio);
        io->ioa_AllocKey = key;
    }

    for (ch = 0; ch < AUDIO_CHANNELS; ch++)
    {
        if (mask & (1U << ch))
        {
            struct AudioChannel *channel = audio_channel_state(audio, ch);

            channel->ac_AllocKey = key;
            channel->ac_Precedence = precedence;
            channel->ac_Stopped = FALSE;
        }
    }

    audio_set_request_mask(io, mask);
    io->ioa_Request.io_Error = 0;
}

static BOOL audio_allocate_now(struct AudioBase *audio,
                               struct IOAudio *io,
                               BOOL allow_queue)
{
    UBYTE selected_mask;
    UBYTE stolen_mask;

    io->ioa_Request.io_Error = 0;
    audio_clear_request_mask(io);

    if (!audio_choose_allocation(audio, io, &selected_mask, &stolen_mask))
    {
        if ((io->ioa_Request.io_Flags & ADIOF_NOWAIT) || !allow_queue)
        {
            io->ioa_Request.io_Error = ADIOERR_ALLOCFAILED;
            return TRUE;
        }

        AddTail(audio_pending_allocations(audio), (struct Node *)io);
        io->ioa_Request.io_Flags &= ~IOF_QUICK;
        return FALSE;
    }

    if (stolen_mask != 0)
    {
        audio_steal_mask(audio, stolen_mask);
    }

    audio_commit_allocation(audio, io, selected_mask);
    return TRUE;
}

static void audio_try_pending_allocations(struct AudioBase *audio)
{
    struct IOAudio *io;
    struct IOAudio *next;

    for (io = (struct IOAudio *)GETHEAD(audio_pending_allocations(audio));
         io;
         io = next)
    {
        next = (struct IOAudio *)GETSUCC(io);
        Remove((struct Node *)io);
        if (!audio_allocate_now(audio, io, FALSE))
        {
            AddTail(audio_pending_allocations(audio), (struct Node *)io);
            continue;
        }

        if (!(io->ioa_Request.io_Flags & IOF_QUICK))
        {
            ReplyMsg(&io->ioa_Request.io_Message);
        }
    }
}

static BOOL audio_handle_waitcycle(struct AudioBase *audio, struct IOAudio *io)
{
    UBYTE requested = (UBYTE)(ULONG)io->ioa_Request.io_Unit & AUDIO_MAX_MASK;
    UBYTE channel_num = audio_lowest_channel(requested);
    struct AudioChannel *channel;

    audio_clear_request_mask(io);
    io->ioa_Request.io_Error = 0;

    if (channel_num >= AUDIO_CHANNELS)
    {
        io->ioa_Request.io_Error = ADIOERR_NOALLOCATION;
        return TRUE;
    }

    channel = audio_channel_state(audio, channel_num);
    if (!audio_key_matches(channel, io->ioa_AllocKey))
    {
        io->ioa_Request.io_Error = ADIOERR_NOALLOCATION;
        return TRUE;
    }

    audio_set_request_mask(io, (1U << channel_num));
    if (!channel->ac_Playing || !audio_list_head(&channel->ac_WriteList) || channel->ac_Stopped)
    {
        return TRUE;
    }

    AddTail(audio_misc_requests(audio), (struct Node *)io);
    io->ioa_Request.io_Flags &= ~IOF_QUICK;
    return FALSE;
}

static BOOL audio_handle_pervol(struct AudioBase *audio, struct IOAudio *io)
{
    UBYTE requested = (UBYTE)(ULONG)io->ioa_Request.io_Unit & AUDIO_MAX_MASK;
    UBYTE matched = audio_validate_mask(audio, requested, io->ioa_AllocKey);
    UBYTE ch;

    audio_set_request_mask(io, matched);
    io->ioa_Request.io_Error = (matched != 0) ? 0 : ADIOERR_NOALLOCATION;

    if (!matched)
    {
        return TRUE;
    }

    if (io->ioa_Request.io_Flags & ADIOF_SYNCCYCLE)
    {
        for (ch = 0; ch < AUDIO_CHANNELS; ch++)
        {
            if ((matched & (1U << ch)) && audio_channel_state(audio, ch)->ac_Playing && !audio_channel_state(audio, ch)->ac_Stopped)
            {
                AddTail(audio_misc_requests(audio), (struct Node *)io);
                io->ioa_Request.io_Flags &= ~IOF_QUICK;
                return FALSE;
            }
        }
    }

    for (ch = 0; ch < AUDIO_CHANNELS; ch++)
    {
        if (matched & (1U << ch))
        {
            audio_channel_state(audio, ch)->ac_Period = io->ioa_Period;
            audio_channel_state(audio, ch)->ac_Volume = io->ioa_Volume;
        }
    }

    return TRUE;
}

static BOOL audio_handle_finish(struct AudioBase *audio, struct IOAudio *io)
{
    UBYTE requested = (UBYTE)(ULONG)io->ioa_Request.io_Unit & AUDIO_MAX_MASK;
    UBYTE matched = audio_validate_mask(audio, requested, io->ioa_AllocKey);
    UBYTE ch;

    audio_set_request_mask(io, matched);
    io->ioa_Request.io_Error = (matched != 0) ? 0 : ADIOERR_NOALLOCATION;

    if (!matched)
    {
        return TRUE;
    }

    if (io->ioa_Request.io_Flags & ADIOF_SYNCCYCLE)
    {
        for (ch = 0; ch < AUDIO_CHANNELS; ch++)
        {
            if ((matched & (1U << ch)) && audio_channel_state(audio, ch)->ac_Playing && !audio_channel_state(audio, ch)->ac_Stopped)
            {
                AddTail(audio_misc_requests(audio), (struct Node *)io);
                io->ioa_Request.io_Flags &= ~IOF_QUICK;
                return FALSE;
            }
        }
    }

    for (ch = 0; ch < AUDIO_CHANNELS; ch++)
    {
        if (matched & (1U << ch))
        {
            audio_finish_immediately(audio, audio_channel_state(audio, ch));
        }
    }

    return TRUE;
}

static BOOL audio_handle_write(struct AudioBase *audio, struct IOAudio *io)
{
    UBYTE requested = (UBYTE)(ULONG)io->ioa_Request.io_Unit & AUDIO_MAX_MASK;
    UBYTE channel_num = audio_lowest_channel(requested);
    struct AudioChannel *channel;

    audio_clear_request_mask(io);
    io->ioa_Request.io_Error = 0;

    if (channel_num >= AUDIO_CHANNELS || !io->ioa_Data || io->ioa_Length < 2 || (io->ioa_Length & 1))
    {
        io->ioa_Request.io_Error = (channel_num >= AUDIO_CHANNELS) ? ADIOERR_NOALLOCATION : IOERR_BADLENGTH;
        return TRUE;
    }

    channel = audio_channel_state(audio, channel_num);
    if (!audio_key_matches(channel, io->ioa_AllocKey))
    {
        io->ioa_Request.io_Error = ADIOERR_NOALLOCATION;
        return TRUE;
    }

    audio_set_request_mask(io, (1U << channel_num));
    AddTail(&channel->ac_WriteList, (struct Node *)io);
    io->ioa_Request.io_Flags &= ~IOF_QUICK;
    audio_start_channel_if_possible(audio, channel);
    return FALSE;
}

static BOOL audio_handle_read(struct AudioBase *audio, struct IOAudio *io)
{
    UBYTE requested = (UBYTE)(ULONG)io->ioa_Request.io_Unit & AUDIO_MAX_MASK;
    UBYTE channel_num = audio_lowest_channel(requested);
    struct AudioChannel *channel;

    audio_clear_request_mask(io);
    io->ioa_Request.io_Error = 0;
    io->ioa_Data = NULL;

    if (channel_num >= AUDIO_CHANNELS)
    {
        io->ioa_Request.io_Error = ADIOERR_NOALLOCATION;
        return TRUE;
    }

    channel = audio_channel_state(audio, channel_num);
    if (!audio_key_matches(channel, io->ioa_AllocKey))
    {
        io->ioa_Request.io_Error = ADIOERR_NOALLOCATION;
        return TRUE;
    }

    audio_set_request_mask(io, (1U << channel_num));
    if (!channel->ac_Stopped)
    {
        io->ioa_Data = (UBYTE *)audio_list_head(&channel->ac_WriteList);
    }

    return TRUE;
}

static BOOL audio_handle_null_cmd(struct AudioBase *audio, struct IOAudio *io)
{
    UBYTE requested = (UBYTE)(ULONG)io->ioa_Request.io_Unit & AUDIO_MAX_MASK;
    UBYTE matched = audio_validate_mask(audio, requested, io->ioa_AllocKey);

    audio_set_request_mask(io, matched);
    io->ioa_Request.io_Error = (matched != 0) ? 0 : ADIOERR_NOALLOCATION;
    return TRUE;
}

static BOOL audio_handle_flush(struct AudioBase *audio, struct IOAudio *io)
{
    UBYTE requested = (UBYTE)(ULONG)io->ioa_Request.io_Unit & AUDIO_MAX_MASK;
    UBYTE matched = audio_validate_mask(audio, requested, io->ioa_AllocKey);
    UBYTE ch;

    audio_set_request_mask(io, matched);
    io->ioa_Request.io_Error = (matched != 0) ? 0 : ADIOERR_NOALLOCATION;

    for (ch = 0; ch < AUDIO_CHANNELS; ch++)
    {
        if (matched & (1U << ch))
        {
            audio_abort_channel(audio, audio_channel_state(audio, ch), IOERR_ABORTED);
        }
    }

    return TRUE;
}

static BOOL audio_handle_reset(struct AudioBase *audio, struct IOAudio *io)
{
    UBYTE requested = (UBYTE)(ULONG)io->ioa_Request.io_Unit & AUDIO_MAX_MASK;
    UBYTE matched = audio_validate_mask(audio, requested, io->ioa_AllocKey);
    UBYTE ch;

    audio_set_request_mask(io, matched);
    io->ioa_Request.io_Error = (matched != 0) ? 0 : ADIOERR_NOALLOCATION;

    for (ch = 0; ch < AUDIO_CHANNELS; ch++)
    {
        if (matched & (1U << ch))
        {
            audio_abort_channel(audio, audio_channel_state(audio, ch), IOERR_ABORTED);
        }
    }

    return TRUE;
}

static BOOL audio_handle_setprec(struct AudioBase *audio, struct IOAudio *io)
{
    UBYTE requested = (UBYTE)(ULONG)io->ioa_Request.io_Unit & AUDIO_MAX_MASK;
    UBYTE matched = audio_validate_mask(audio, requested, io->ioa_AllocKey);
    UBYTE ch;

    audio_set_request_mask(io, matched);
    io->ioa_Request.io_Error = (matched != 0) ? 0 : ADIOERR_NOALLOCATION;

    for (ch = 0; ch < AUDIO_CHANNELS; ch++)
    {
        if (matched & (1U << ch))
        {
            audio_channel_state(audio, ch)->ac_Precedence = io->ioa_Request.io_Message.mn_Node.ln_Pri;
        }
    }

    audio_try_pending_allocations(audio);
    return TRUE;
}

static BOOL audio_handle_free(struct AudioBase *audio, struct IOAudio *io)
{
    UBYTE requested = (UBYTE)(ULONG)io->ioa_Request.io_Unit & AUDIO_MAX_MASK;
    UBYTE matched = audio_validate_mask(audio, requested, io->ioa_AllocKey);

    audio_set_request_mask(io, matched);
    io->ioa_Request.io_Error = (matched != 0) ? 0 : ADIOERR_NOALLOCATION;

    if (matched)
    {
        audio_free_mask(audio, matched);
    }

    return TRUE;
}

static BOOL audio_process_command(struct AudioBase *audio, struct IOAudio *io)
{
    switch (io->ioa_Request.io_Command)
    {
        case CMD_CLEAR:
        case CMD_UPDATE:
            return audio_handle_null_cmd(audio, io);

        case CMD_FLUSH:
            return audio_handle_flush(audio, io);

        case CMD_RESET:
            return audio_handle_reset(audio, io);

        case CMD_READ:
            return audio_handle_read(audio, io);

        case CMD_WRITE:
            return audio_handle_write(audio, io);

        case ADCMD_ALLOCATE:
            return audio_allocate_now(audio, io, TRUE);

        case ADCMD_FREE:
            return audio_handle_free(audio, io);

        case ADCMD_SETPREC:
            return audio_handle_setprec(audio, io);

        case ADCMD_PERVOL:
            return audio_handle_pervol(audio, io);

        case ADCMD_FINISH:
            return audio_handle_finish(audio, io);

        case ADCMD_WAITCYCLE:
            return audio_handle_waitcycle(audio, io);

        case CMD_STOP:
        case CMD_START:
            return audio_handle_null_cmd(audio, io);

        default:
            io->ioa_Request.io_Error = IOERR_NOCMD;
            return TRUE;
    }
}

static void audio_process_misc_for_channel(struct AudioBase *audio,
                                           struct AudioChannel *channel,
                                           BOOL *finish_now)
{
    struct IOAudio *io;
    struct IOAudio *next;
    UBYTE mask = (1U << channel->ac_Channel);

    for (io = (struct IOAudio *)GETHEAD(audio_misc_requests(audio));
         io;
         io = next)
    {
        next = (struct IOAudio *)GETSUCC(io);

        if (!(((UBYTE)(ULONG)io->ioa_Request.io_Unit & AUDIO_MAX_MASK) & mask))
        {
            continue;
        }

        switch (io->ioa_Request.io_Command)
        {
            case ADCMD_WAITCYCLE:
                Remove((struct Node *)io);
                audio_reply_request(io, 0);
                break;

            case ADCMD_PERVOL:
                channel->ac_Period = io->ioa_Period;
                channel->ac_Volume = io->ioa_Volume;
                Remove((struct Node *)io);
                audio_reply_request(io, 0);
                break;

            case ADCMD_FINISH:
                Remove((struct Node *)io);
                audio_reply_request(io, 0);
                *finish_now = TRUE;
                break;

            default:
                break;
        }
    }
}

static void audio_tick_channel(struct AudioBase *audio,
                               struct AudioChannel *channel,
                               const struct timeval *now)
{
    struct IOAudio *current;

    if (!channel->ac_Playing || channel->ac_Stopped)
    {
        return;
    }

    current = audio_list_head(&channel->ac_WriteList);
    while (current && audio_cmp_time(&channel->ac_CycleDeadline, now) <= 0)
    {
        BOOL finish_now = FALSE;

        audio_dispatch_irq(channel->ac_Channel);
        audio_process_misc_for_channel(audio, channel, &finish_now);

        if (finish_now)
        {
            audio_complete_current_write(audio, channel, 0);
            current = audio_list_head(&channel->ac_WriteList);
            if (!current || !channel->ac_Playing)
            {
                return;
            }
            continue;
        }

        if (channel->ac_CyclesRemaining == 1)
        {
            audio_complete_current_write(audio, channel, 0);
            current = audio_list_head(&channel->ac_WriteList);
            if (!current || !channel->ac_Playing)
            {
                return;
            }
            continue;
        }

        if (channel->ac_CyclesRemaining > 1)
        {
            channel->ac_CyclesRemaining--;
        }

        audio_add_micros(&channel->ac_CycleDeadline, channel->ac_CycleMicros);
        current = audio_list_head(&channel->ac_WriteList);
    }
}

VOID _audio_VBlankHook(void)
{
    struct Node *node;
    struct AudioBase *audio = NULL;
    struct timeval now;
    int ch;

    for (node = GETHEAD(&SysBase->DeviceList); node; node = GETSUCC(node))
    {
        if (node->ln_Name && strcmp((const char *)node->ln_Name, _g_audio_ExDevName) == 0)
        {
            audio = (struct AudioBase *)node;
            break;
        }
    }

    if (!audio)
    {
        return;
    }

    audio_get_raw_time(&now);
    audio_normalize_timeval(&now);

    for (ch = 0; ch < AUDIO_CHANNELS; ch++)
    {
        audio_tick_channel(audio, audio_channel_state(audio, ch), &now);
    }
}

static struct Library * __g_lxa_audio_InitDev(register struct Library *dev __asm("d0"),
                                              register BPTR seglist __asm("a0"),
                                              register struct ExecBase *sysb __asm("a6"))
{
    struct AudioBase *audio = (struct AudioBase *)dev;
    UBYTE ch;

    audio->ab_SegList = seglist;
    audio_private(audio)->ap_KeyGen = 0x5a00;
    NEWLIST(audio_pending_allocations(audio));
    NEWLIST(audio_misc_requests(audio));

    for (ch = 0; ch < AUDIO_CHANNELS; ch++)
    {
        struct AudioChannel *channel = audio_channel_state(audio, ch);

        NEWLIST(&channel->ac_WriteList);
        channel->ac_AllocKey = 0;
        channel->ac_Precedence = ADALLOC_MINPREC;
        channel->ac_Channel = ch;
        channel->ac_Stopped = FALSE;
        channel->ac_Playing = FALSE;
        channel->ac_Period = 0;
        channel->ac_Volume = 0;
        channel->ac_CyclesRemaining = 0;
        channel->ac_CycleMicros = 0;
        channel->ac_CycleDeadline.tv_secs = 0;
        channel->ac_CycleDeadline.tv_micro = 0;
    }

    return dev;
}

static void __g_lxa_audio_Open(register struct Library *dev __asm("a6"),
                               register struct IORequest *ioreq __asm("a1"),
                               register ULONG unit __asm("d0"),
                               register ULONG flags __asm("d1"))
{
    struct AudioBase *audio = (struct AudioBase *)dev;
    struct IOAudio *io = (struct IOAudio *)ioreq;

    (void)audio;
    (void)unit;
    (void)flags;

    dev->lib_OpenCnt++;

    io->ioa_Request.io_Device = (struct Device *)dev;
    io->ioa_Request.io_Error = 0;
    io->ioa_Request.io_Unit = NULL;

    if (io->ioa_Length != 0)
    {
        io->ioa_Request.io_Flags |= ADIOF_NOWAIT;
        if (!audio_allocate_now((struct AudioBase *)dev, io, FALSE) || io->ioa_Request.io_Error != 0)
        {
            dev->lib_OpenCnt--;
            io->ioa_Request.io_Device = NULL;
            io->ioa_Request.io_Unit = NULL;
            if (io->ioa_Request.io_Error == 0)
            {
                io->ioa_Request.io_Error = ADIOERR_ALLOCFAILED;
            }
        }
        io->ioa_Request.io_Flags &= ~ADIOF_NOWAIT;
    }
}

static BPTR __g_lxa_audio_Close(register struct Library *dev __asm("a6"),
                                register struct IORequest *ioreq __asm("a1"))
{
    struct AudioBase *audio = (struct AudioBase *)dev;
    struct IOAudio *io = (struct IOAudio *)ioreq;
    UBYTE mask = audio_validate_mask(audio, AUDIO_MAX_MASK, io->ioa_AllocKey);

    if (mask)
    {
        audio_free_mask(audio, mask);
    }

    dev->lib_OpenCnt--;
    io->ioa_Request.io_Unit = NULL;
    io->ioa_Request.io_Device = NULL;
    return 0;
}

static BPTR __g_lxa_audio_Expunge(register struct Library *dev __asm("a6"))
{
    (void)dev;
    return 0;
}

static BPTR __g_lxa_audio_BeginIO(register struct Library *dev __asm("a6"),
                                  register struct IORequest *ioreq __asm("a1"))
{
    struct AudioBase *audio = (struct AudioBase *)dev;
    struct IOAudio *io = (struct IOAudio *)ioreq;

    io->ioa_Request.io_Message.mn_Node.ln_Type = NT_MESSAGE;
    io->ioa_Request.io_Error = 0;

    if (audio_process_command(audio, io))
    {
        if (!(io->ioa_Request.io_Flags & IOF_QUICK))
        {
            ReplyMsg(&io->ioa_Request.io_Message);
        }
    }

    return 0;
}

static ULONG __g_lxa_audio_AbortIO(register struct Library *dev __asm("a6"),
                                   register struct IORequest *ioreq __asm("a1"))
{
    struct AudioBase *audio = (struct AudioBase *)dev;
    struct IOAudio *io = (struct IOAudio *)ioreq;
    UBYTE ch;

    if (audio_request_is_on_list(audio_pending_allocations(audio), io))
    {
        Remove((struct Node *)io);
        audio_reply_request(io, IOERR_ABORTED);
        return 0;
    }

    if (audio_request_is_on_list(audio_misc_requests(audio), io))
    {
        Remove((struct Node *)io);
        audio_reply_request(io, IOERR_ABORTED);
        return 0;
    }

    for (ch = 0; ch < AUDIO_CHANNELS; ch++)
    {
        struct AudioChannel *channel = audio_channel_state(audio, ch);
        struct IOAudio *head = audio_list_head(&channel->ac_WriteList);

        if (!audio_request_is_on_list(&channel->ac_WriteList, io))
        {
            continue;
        }

        Remove((struct Node *)io);
        audio_reply_request(io, IOERR_ABORTED);

        if (head == io)
        {
            channel->ac_Playing = FALSE;
            audio_start_channel_if_possible(audio, channel);
        }

        return 0;
    }

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

extern APTR              __g_lxa_audio_FuncTab [];
extern struct MyDataInit __g_lxa_audio_DataTab;
extern struct InitTable  __g_lxa_audio_InitTab;
extern APTR              __g_lxa_audio_EndResident;

static struct Resident __aligned ROMTag =
{
    RTC_MATCHWORD,
    &ROMTag,
    &__g_lxa_audio_EndResident,
    RTF_AUTOINIT,
    VERSION,
    NT_DEVICE,
    0,
    &_g_audio_ExDevName[0],
    &_g_audio_ExDevID[0],
    &__g_lxa_audio_InitTab
};

APTR __g_lxa_audio_EndResident;
struct Resident *__lxa_audio_ROMTag = &ROMTag;

struct InitTable __g_lxa_audio_InitTab =
{
    (ULONG)sizeof(struct AudioBase),
    (APTR *)&__g_lxa_audio_FuncTab[0],
    (APTR)&__g_lxa_audio_DataTab,
    (APTR)__g_lxa_audio_InitDev
};

APTR __g_lxa_audio_FuncTab [] =
{
    __g_lxa_audio_Open,
    __g_lxa_audio_Close,
    __g_lxa_audio_Expunge,
    0,
    __g_lxa_audio_BeginIO,
    __g_lxa_audio_AbortIO,
    (APTR)((LONG)-1)
};

struct MyDataInit __g_lxa_audio_DataTab =
{
    INITBYTE(OFFSET(Node, ln_Type), NT_DEVICE),
    0x80, (UBYTE)(ULONG)OFFSET(Node, ln_Name), (ULONG)&_g_audio_ExDevName[0],
    INITBYTE(OFFSET(Library, lib_Flags), LIBF_SUMUSED|LIBF_CHANGED),
    INITWORD(OFFSET(Library, lib_Version), VERSION),
    INITWORD(OFFSET(Library, lib_Revision), REVISION),
    0x80, (UBYTE)(ULONG)OFFSET(Library, lib_IdString), (ULONG)&_g_audio_ExDevID[0],
    (ULONG)0
};
