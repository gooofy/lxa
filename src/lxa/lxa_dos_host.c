/*
 * lxa_dos_host.c - Host-side AmigaDOS filesystem bridge.
 *
 * Contains: timer, audio, lock/record-lock management, DOS filesystem
 * operations, trackdisk, console emulation, and notify.
 *
 * Phase 125: extracted from lxa.c.
 */

#include "lxa_internal.h"
#include "lxa_memory.h"

/* Forward declaration (definition near end of file) */
static int _linux_path_to_amiga(const char *linux_path, char *amiga_buf, size_t bufsize);

/* Private static data owned by lxa_dos_host.c */
lock_entry_t        g_locks[MAX_LOCKS];
record_lock_entry_t g_record_locks[MAX_RECORD_LOCKS];
timer_request_t     g_timer_queue[MAX_TIMER_REQUESTS];
notify_entry_t      g_notify_requests[MAX_NOTIFY_REQUESTS];

#ifdef SDL2_FOUND
audio_channel_state_t g_audio_channels[AUDIO_HOST_CHANNELS];
SDL_AudioDeviceID     g_audio_device;
bool                  g_audio_initialized = false;
#endif

static uint64_t _timer_get_time_us(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000000ULL + (uint64_t)tv.tv_usec;
}

static void __attribute__((unused)) _audio_init(void)
{
#ifdef SDL2_FOUND
    SDL_AudioSpec want;

    if (g_audio_initialized)
    {
        return;
    }

    if (!(SDL_WasInit(SDL_INIT_AUDIO) & SDL_INIT_AUDIO))
    {
        if (SDL_InitSubSystem(SDL_INIT_AUDIO) < 0)
        {
            DPRINTF(LOG_WARNING, "lxa: SDL audio init failed: %s\n", SDL_GetError());
            return;
        }
    }

    memset(&want, 0, sizeof(want));
    want.freq = 22050;
    want.format = AUDIO_S16SYS;
    want.channels = 1;
    want.samples = 1024;

    g_audio_device = SDL_OpenAudioDevice(NULL, 0, &want, NULL, 0);
    if (g_audio_device == 0)
    {
        DPRINTF(LOG_WARNING, "lxa: SDL_OpenAudioDevice failed: %s\n", SDL_GetError());
        return;
    }

    SDL_PauseAudioDevice(g_audio_device, 0);
    g_audio_initialized = true;
#endif
}

void _audio_shutdown(void)
{
#ifdef SDL2_FOUND
    if (g_audio_initialized)
    {
        SDL_ClearQueuedAudio(g_audio_device);
        SDL_CloseAudioDevice(g_audio_device);
        g_audio_device = 0;
        g_audio_initialized = false;
    }
#endif

    return;
}

void _audio_queue_fragment(uint32_t packed,
                                  uint32_t data_ptr,
                                  uint32_t length,
                                  uint32_t period,
                                  uint32_t volume)
{
#ifdef SDL2_FOUND
    uint8_t channel = packed & 0xff;
    uint32_t cycles = packed >> 8;
    int16_t mixbuf[4096];
    uint32_t count;
    uint32_t i;

    if (channel >= AUDIO_HOST_CHANNELS)
    {
        return;
    }

    _audio_init();

    if (!g_audio_initialized)
    {
        return;
    }

    if (length > 4096)
    {
        length = 4096;
    }

    count = length;
    if (count == 0)
    {
        return;
    }

    if (volume > 64)
    {
        volume = 64;
    }

    for (i = 0; i < count; i++)
    {
        int8_t sample = (int8_t)m68k_read_memory_8(data_ptr + i);
        mixbuf[i] = (int16_t)((sample * (int32_t)volume * 256) / 64);
    }

    SDL_ClearQueuedAudio(g_audio_device);
    SDL_QueueAudio(g_audio_device, mixbuf, count * sizeof(int16_t));

    g_audio_channels[channel].active = true;
    g_audio_channels[channel].channel = channel;
    g_audio_channels[channel].volume = (uint16_t)volume;
    g_audio_channels[channel].period = (uint16_t)period;
    g_audio_channels[channel].length = count;
    g_audio_channels[channel].cycles = cycles;
#else
#endif

    (void)packed;
    (void)data_ptr;
    (void)length;
    (void)period;
    (void)volume;
}

/* Add a timer request to the queue.
 * The delay is specified as seconds + microseconds from NOW.
 */
bool _timer_add_request(uint32_t ioreq_ptr, uint32_t delay_secs, uint32_t delay_micros)
{
    /* Compute absolute wake time by adding delay to current host time */
    uint64_t now = _timer_get_time_us();
    uint64_t delay_us = (uint64_t)delay_secs * 1000000ULL + (uint64_t)delay_micros;
    uint64_t wake_time_us = now + delay_us;
    
    DPRINTF(LOG_DEBUG, "lxa: _timer_add_request: ioreq=0x%08x delay=%u.%06u now=%" PRIu64 " wake=%" PRIu64 "\n",
            ioreq_ptr, delay_secs, delay_micros, now, wake_time_us);
    
    for (int i = 0; i < MAX_TIMER_REQUESTS; i++) {
        if (!g_timer_queue[i].in_use) {
            g_timer_queue[i].in_use = true;
            g_timer_queue[i].expired = false;
            g_timer_queue[i].ioreq_ptr = ioreq_ptr;
            g_timer_queue[i].wake_time_us = wake_time_us;
            return true;
        }
    }
    
    DPRINTF(LOG_ERROR, "lxa: _timer_add_request: queue full!\n");
    return false;
}

/* Remove a timer request from the queue */
bool _timer_remove_request(uint32_t ioreq_ptr)
{
    DPRINTF(LOG_DEBUG, "lxa: _timer_remove_request: ioreq=0x%08x\n", ioreq_ptr);
    
    for (int i = 0; i < MAX_TIMER_REQUESTS; i++) {
        if (g_timer_queue[i].in_use && g_timer_queue[i].ioreq_ptr == ioreq_ptr) {
            g_timer_queue[i].in_use = false;
            return true;
        }
    }
    return false;
}

/* Check and mark expired timer requests - exported for lxa_api.c
 * This is called from the main emulation loop.
 * It only marks timers as expired - the actual ReplyMsg() is done from the
 * m68k side via the _timer_VBlankHook() function.
 */
int _timer_check_expired(void)
{
    uint64_t now = _timer_get_time_us();
    int expired_count = 0;
    
    for (int i = 0; i < MAX_TIMER_REQUESTS; i++) {
        if (g_timer_queue[i].in_use && !g_timer_queue[i].expired) {
            uint64_t wake_time = g_timer_queue[i].wake_time_us;
            DPRINTF(LOG_DEBUG, "lxa: _timer_check_expired: [%d] now=%" PRIu64 " wake=%" PRIu64 " diff=%lld\n", 
                    i, now, wake_time, (long long)(wake_time - now));
            
            if (wake_time <= now) {
                DPRINTF(LOG_DEBUG, "lxa: _timer_check_expired: ioreq=0x%08x marking as expired\n", 
                        g_timer_queue[i].ioreq_ptr);
            
                /* Mark as expired - it will be retrieved by EMU_CALL_TIMER_GET_EXPIRED */
                g_timer_queue[i].expired = true;
                expired_count++;
            }
        }
    }
    
    return expired_count;
}

/* Get the next expired timer request.
 * Returns the m68k IORequest pointer and marks the entry as no longer in use.
 * Returns 0 if no expired timers are available.
 */
uint32_t _timer_get_expired(void)
{
    for (int i = 0; i < MAX_TIMER_REQUESTS; i++) {
        if (g_timer_queue[i].in_use && g_timer_queue[i].expired) {
            uint32_t ioreq = g_timer_queue[i].ioreq_ptr;
            
            DPRINTF(LOG_DEBUG, "lxa: _timer_get_expired: returning ioreq=0x%08x\n", ioreq);
            
            /* Mark as no longer in use */
            g_timer_queue[i].in_use = false;
            g_timer_queue[i].expired = false;
            
            return ioreq;
        }
    }
    
    return 0;
}

/* Amiga DOS protection flag bits */
#define FIBF_DELETE     (1<<0)  /* File is deletable */
#define FIBF_EXECUTE    (1<<1)  /* File is executable */
#define FIBF_WRITE      (1<<2)  /* File is writeable */
#define FIBF_READ       (1<<3)  /* File is readable */
#define FIBF_ARCHIVE    (1<<4)  /* File has been archived */
#define FIBF_PURE       (1<<5)  /* Pure (re-entrant) */
#define FIBF_SCRIPT     (1<<6)  /* Script file */

/* Amiga DOS file types */
#define ST_ROOT         1
#define ST_USERDIR      2
#define ST_SOFTLINK     3
#define ST_LINKDIR      4
#define ST_FILE        -3
#define ST_LINKFILE    -4

/* Amiga lock modes */
#define ACCESS_READ    -2  /* SHARED_LOCK */
#define ACCESS_WRITE   -1  /* EXCLUSIVE_LOCK */

/* Amiga error codes */
#define ERROR_NO_FREE_STORE      103
#define ERROR_BAD_NUMBER         115
#define ERROR_REQUIRED_ARG_MISSING 116
#define ERROR_OBJECT_IN_USE      202
#define ERROR_OBJECT_EXISTS      203
#define ERROR_DIR_NOT_FOUND      204
#define ERROR_OBJECT_NOT_FOUND   205
#define ERROR_BAD_STREAM_NAME    206
#define ERROR_OBJECT_TOO_LARGE   207
#define ERROR_ACTION_NOT_KNOWN   209
#define ERROR_INVALID_LOCK       211
#define ERROR_OBJECT_WRONG_TYPE  212
#define ERROR_DISK_NOT_VALIDATED 213
#define ERROR_DISK_WRITE_PROTECTED 214
#define ERROR_RENAME_ACROSS_DEVICES 215
#define ERROR_DIRECTORY_NOT_EMPTY 216
#define ERROR_TOO_MANY_LEVELS    217
#define ERROR_SEEK_ERROR         219
#define ERROR_DISK_FULL          221
#define ERROR_DELETE_PROTECTED   222
#define ERROR_WRITE_PROTECTED    223
#define ERROR_READ_PROTECTED     224
#define ERROR_NOT_A_DOS_DISK     225
#define ERROR_NO_MORE_ENTRIES    232
#define ERROR_RECORD_NOT_LOCKED  240
#define ERROR_LOCK_COLLISION     241
#define ERROR_LOCK_TIMEOUT       242
#define ERROR_BUFFER_OVERFLOW    303

#define DVP_DVP_PORT     0
#define DVP_DVP_LOCK     4
#define DVP_DVP_FLAGS    8
#define DVP_DVP_DEVNODE  12

#define DVPF_UNLOCK      (1UL << 0)
#define DVPF_ASSIGN      (1UL << 1)

#define NRF_SEND_MESSAGE 1
#define NRF_SEND_SIGNAL  2
#define NRF_NOTIFY_INITIAL 16

#define AMIGA_EPOCH_OFFSET 252460800

/* Allocate a new lock entry */
static int _lock_alloc(void)
{
    for (int i = 1; i < MAX_LOCKS; i++) {
        if (!g_locks[i].in_use) {
            memset(&g_locks[i], 0, sizeof(lock_entry_t));
            g_locks[i].in_use = true;
            g_locks[i].refcount = 1;
            return i;
        }
    }
    return 0; /* No free slot */
}

/* Free a lock entry */
static void _lock_free(int lock_id)
{
    if (lock_id <= 0 || lock_id >= MAX_LOCKS) return;
    if (!g_locks[lock_id].in_use) return;
    
    if (g_locks[lock_id].dir) {
        closedir(g_locks[lock_id].dir);
        g_locks[lock_id].dir = NULL;
    }
    g_locks[lock_id].in_use = false;
}

/* Get lock entry */
static lock_entry_t *_lock_get(int lock_id)
{
    if (lock_id <= 0 || lock_id >= MAX_LOCKS) return NULL;
    if (!g_locks[lock_id].in_use) return NULL;
    return &g_locks[lock_id];
}

static uint64_t _record_lock_end(uint64_t offset, uint64_t length)
{
    if (length > UINT64_MAX - offset)
    {
        return UINT64_MAX;
    }

    return offset + length;
}

static bool _record_locks_overlap(uint64_t offset_a, uint64_t length_a,
                                  uint64_t offset_b, uint64_t length_b)
{
    uint64_t end_a;
    uint64_t end_b;

    if (length_a == 0 || length_b == 0)
    {
        return false;
    }

    end_a = _record_lock_end(offset_a, length_a);
    end_b = _record_lock_end(offset_b, length_b);

    return offset_a < end_b && offset_b < end_a;
}

static int _record_lock_alloc(void)
{
    for (int i = 0; i < MAX_RECORD_LOCKS; i++)
    {
        if (!g_record_locks[i].in_use)
        {
            memset(&g_record_locks[i], 0, sizeof(g_record_locks[i]));
            g_record_locks[i].in_use = true;
            return i;
        }
    }

    return -1;
}

static int _record_lock_find_conflict(dev_t dev, ino_t ino, uint32_t owner_fh68k,
                                      uint64_t offset, uint64_t length, bool exclusive)
{
    for (int i = 0; i < MAX_RECORD_LOCKS; i++)
    {
        record_lock_entry_t *entry = &g_record_locks[i];

        if (!entry->in_use)
        {
            continue;
        }

        if (entry->dev != dev || entry->ino != ino)
        {
            continue;
        }

        if (entry->owner_fh68k == owner_fh68k)
        {
            continue;
        }

        if (!_record_locks_overlap(entry->offset, entry->length, offset, length))
        {
            continue;
        }

        if (entry->exclusive || exclusive)
        {
            return i;
        }
    }

    return -1;
}

static bool _record_lock_add(dev_t dev, ino_t ino, uint32_t owner_fh68k,
                             uint64_t offset, uint64_t length, bool exclusive)
{
    int slot = _record_lock_alloc();

    if (slot < 0)
    {
        return false;
    }

    g_record_locks[slot].dev = dev;
    g_record_locks[slot].ino = ino;
    g_record_locks[slot].owner_fh68k = owner_fh68k;
    g_record_locks[slot].offset = offset;
    g_record_locks[slot].length = length;
    g_record_locks[slot].exclusive = exclusive;

    return true;
}

static bool _record_lock_remove(dev_t dev, ino_t ino, uint32_t owner_fh68k,
                                 uint64_t offset, uint64_t length)
{
    for (int i = 0; i < MAX_RECORD_LOCKS; i++)
    {
        record_lock_entry_t *entry = &g_record_locks[i];

        if (!entry->in_use)
        {
            continue;
        }

        if (entry->dev != dev || entry->ino != ino)
        {
            continue;
        }

        if (entry->owner_fh68k != owner_fh68k)
        {
            continue;
        }

        if (entry->offset != offset || entry->length != length)
        {
            continue;
        }

        entry->in_use = false;
        return true;
    }

    return false;
}

static bool _dos_lock_info_from_fh(uint32_t fh68k, dev_t *dev_out, ino_t *ino_out)
{
    int fd;
    int kind;
    struct stat st;

    if (fh68k == 0 || !dev_out || !ino_out)
    {
        return false;
    }

    kind = m68k_read_memory_32(fh68k + 32);
    fd = m68k_read_memory_32(fh68k + 36);

    if (kind != FILE_KIND_REGULAR)
    {
        return false;
    }

    if (fstat(fd, &st) != 0)
    {
        return false;
    }

    *dev_out = st.st_dev;
    *ino_out = st.st_ino;
    return true;
}

bool _dos_change_mode(uint32_t type, uint32_t object, int32_t new_mode, uint32_t err68k)
{
    dev_t dev;
    ino_t ino;
    lock_entry_t *lock;
    uint32_t fh68k;
    uint32_t lock_id;
    uint32_t err = 0;

    DPRINTF(LOG_DEBUG,
            "lxa: _dos_change_mode(): type=%u object=0x%08x new_mode=%d err=0x%08x\n",
            type, object, new_mode, err68k);

    if (type != CHANGE_LOCK && type != CHANGE_FH)
    {
        err = ERROR_BAD_NUMBER;
        goto fail;
    }

    if (new_mode != SHARED_LOCK && new_mode != EXCLUSIVE_LOCK)
    {
        err = ERROR_BAD_NUMBER;
        goto fail;
    }

    if (type == CHANGE_FH)
    {
        fh68k = object << 2;
        if (!_dos_lock_info_from_fh(fh68k, &dev, &ino))
        {
            err = ERROR_OBJECT_WRONG_TYPE;
            goto fail;
        }
    }
    else
    {
        lock_id = object;
        lock = _lock_get(lock_id);
        if (!lock)
        {
            err = ERROR_INVALID_LOCK;
            goto fail;
        }

        {
            struct stat st;
            if (stat(lock->linux_path, &st) != 0)
            {
                err = errno2Amiga();
                goto fail;
            }
            dev = st.st_dev;
            ino = st.st_ino;
        }
    }

    if (new_mode == EXCLUSIVE_LOCK)
    {
        for (int i = 0; i < MAX_LOCKS; i++)
        {
            lock_entry_t *entry = &g_locks[i];
            struct stat st;

            if (!entry->in_use)
            {
                continue;
            }

            if (type == CHANGE_LOCK && i == (int)object)
            {
                continue;
            }

            if (stat(entry->linux_path, &st) != 0)
            {
                continue;
            }

            if (st.st_dev == dev && st.st_ino == ino)
            {
                err = ERROR_OBJECT_IN_USE;
                goto fail;
            }
        }
    }

    if (type == CHANGE_FH)
    {
        DPRINTF(LOG_DEBUG, "lxa: _dos_change_mode(): fh change accepted\n");
    }
    else
    {
        lock = _lock_get(object);
        if (!lock)
        {
            err = ERROR_INVALID_LOCK;
            goto fail;
        }

        lock->access_mode = new_mode;
        DPRINTF(LOG_DEBUG, "lxa: _dos_change_mode(): lock %u access now %d\n", object, new_mode);
    }

    if (err68k)
    {
        m68k_write_memory_32(err68k, 0);
    }

    return true;

fail:
    if (err68k)
    {
        m68k_write_memory_32(err68k, err);
    }

    DPRINTF(LOG_DEBUG, "lxa: _dos_change_mode(): fail err=%u\n", err);
    return false;
}

static void _record_lock_release_all(uint32_t owner_fh68k)
{
    for (int i = 0; i < MAX_RECORD_LOCKS; i++)
    {
        if (g_record_locks[i].in_use && g_record_locks[i].owner_fh68k == owner_fh68k)
        {
            g_record_locks[i].in_use = false;
        }
    }
}

/*
 * Check if a List is empty by examining its lh_Head pointer.
 * An empty List has lh_Head pointing to &lh_Tail (list_addr + 4).
 * A non-empty List has lh_Head pointing to the first Node, whose
 * ln_Succ is not NULL.
 */
bool is_list_empty(uint32_t list_addr)
{
    uint32_t lh_Head = m68k_read_memory_32(list_addr);      // lh_Head at offset 0
    uint32_t lh_Tail_addr = list_addr + 4;                  // &lh_Tail is at offset 4
    return lh_Head == lh_Tail_addr;
}

/*
 * Check if there are other tasks running besides the current one.
 * Returns true if there are tasks in TaskReady or TaskWait.
 */
bool other_tasks_running(void)
{
    uint32_t sysbase = m68k_read_memory_32(4);  // SysBase at address 4
    
    bool ready_empty = is_list_empty(sysbase + EXECBASE_TASKREADY);
    bool wait_empty = is_list_empty(sysbase + EXECBASE_TASKWAIT);
    
    DPRINTF(LOG_DEBUG, "*** other_tasks_running: sysbase=0x%08x, TaskReady empty=%d, TaskWait empty=%d\n",
            sysbase, ready_empty, wait_empty);
    
    if (!ready_empty)
        return true;
    if (!wait_empty)
        return true;
    
    return false;
}


char *_mgetstr (uint32_t address)
{
    if ((address >= RAM_START) && (address <= RAM_END))
    {
        uint32_t addr = address - RAM_START;
        return (char *) &g_ram[addr];
    }
    else if ((address >= ROM_START) && (address <= ROM_END))
    {
        uint32_t addr = address - ROM_START;
        return (char *) &g_rom[addr];
    }
    else
    {
        printf("ERROR: _mgetstr at invalid address 0x%08x\n", address);
        assert (false);
    }
    return NULL;
}

static void _dos_path2linux (const char *amiga_path, char *linux_path, int buf_len)
{
    // First try VFS resolution
    if (vfs_resolve_path (amiga_path, linux_path, buf_len))
    {
        DPRINTF (LOG_DEBUG, "lxa: _dos_path2linux: VFS resolved %s -> %s\n", amiga_path, linux_path);
        return;
    }
    
    // Fallback to legacy behavior for backward compatibility
    if (!strncasecmp (amiga_path, "NIL:", 4))
        snprintf (linux_path, buf_len, "/dev/null");
    else if (g_sysroot && !vfs_resolve_case_path (g_sysroot, amiga_path, linux_path, buf_len))
        snprintf (linux_path, buf_len, "%s/%s", g_sysroot, amiga_path);
    else if (!g_sysroot)
        snprintf (linux_path, buf_len, "%s", amiga_path);
    
    DPRINTF (LOG_DEBUG, "lxa: _dos_path2linux: legacy resolved %s -> %s\n", amiga_path, linux_path);
}

int errno2Amiga (void)
{
    switch (errno)
    {
        case 0:         return 0;
        case ENOENT:    return ERROR_OBJECT_NOT_FOUND;
        case ENOTDIR:   return ERROR_DIR_NOT_FOUND;
        case EEXIST:    return ERROR_OBJECT_EXISTS;
        case EACCES:    return ERROR_READ_PROTECTED;
        case EPERM:     return ERROR_READ_PROTECTED;
        case ENOMEM:    return ERROR_NO_FREE_STORE;
        case ENOSPC:    return ERROR_DISK_FULL;
        case EISDIR:    return ERROR_OBJECT_WRONG_TYPE;
        case ENOTEMPTY: return ERROR_DIRECTORY_NOT_EMPTY;
        case EXDEV:     return ERROR_RENAME_ACROSS_DEVICES;
        case ENAMETOOLONG: return ERROR_BAD_STREAM_NAME;
        case EFBIG:     return ERROR_OBJECT_TOO_LARGE;
        case EROFS:     return ERROR_DISK_WRITE_PROTECTED;
        case EMFILE:    /* fall through */
        case ENFILE:    return ERROR_NO_FREE_STORE;
        case EBUSY:     return ERROR_OBJECT_IN_USE;
        case ESPIPE:    return ERROR_SEEK_ERROR;
        case ELOOP:     return ERROR_TOO_MANY_LEVELS;
        default:
            DPRINTF (LOG_WARNING, "lxa: errno2Amiga: unmapped errno %d (%s)\n",
                     errno, strerror(errno));
            return ERROR_ACTION_NOT_KNOWN;
    }
}

void _dos_stdinout_fh (uint32_t fh68k, int is_input)
{
    DPRINTF (LOG_DEBUG, "lxa: _dos_stdinout_fh(): fh68k=0x%08x, is_input=%d\n", fh68k, is_input);
    m68k_write_memory_32 (fh68k+32, FILE_KIND_CONSOLE); // fh_Func3
    m68k_write_memory_32 (fh68k+36, is_input ? STDIN_FILENO : STDOUT_FILENO);     // fh_Args
}

int _dos_open (uint32_t path68k, uint32_t accessMode, uint32_t fh68k)
{
    char *amiga_path = _mgetstr(path68k);
    char lxpath[PATH_MAX];

    DPRINTF (LOG_DEBUG, "lxa: _dos_open(): amiga_path=%s\n", amiga_path);

    if (!strncasecmp (amiga_path, "CONSOLE:", 8) || (amiga_path[0]=='*'))
    {
        _dos_stdinout_fh (fh68k, 0);
    }
    else if (!strncasecmp (amiga_path, "CON:", 4) || !strncasecmp (amiga_path, "RAW:", 4))
    {
        /* CON:/RAW: window - mark as console, m68k side will handle the rest */
        /* We set FILE_KIND_CON to indicate this needs special handling */
        m68k_write_memory_32 (fh68k+32, FILE_KIND_CON);     // fh_Func3 = FILE_KIND_CON
        m68k_write_memory_32 (fh68k+36, 0);                 // fh_Args = 0 (will be set later)
        /* Store the raw mode flag: RAW: = 1, CON: = 0 */
        m68k_write_memory_8 (fh68k+44, !strncasecmp(amiga_path, "RAW:", 4) ? 1 : 0);  // fh_Buf (unused field)
        DPRINTF (LOG_DEBUG, "lxa: _dos_open(): CON:/RAW: window, path=%s\n", amiga_path);
    }
    else
    {
        // FIXME: amiga paths are case insensitive!
        _dos_path2linux (amiga_path, lxpath, PATH_MAX);
        DPRINTF (LOG_DEBUG, "lxa: _dos_open(): lxpath=%s\n", lxpath);

        int flags = 0;
        mode_t mode = 0644;

        switch (accessMode)
        {
            case MODE_NEWFILE:
                flags = O_CREAT | O_TRUNC | O_RDWR;
                break;
            case MODE_OLDFILE:
                flags = O_RDONLY;  // Read-only for existing files
                break;
            case MODE_READWRITE:
                flags = O_CREAT | O_RDWR;
                break;
            default:
                assert(FALSE);
        }

        int fd = open (lxpath, flags, mode);
        int err = errno;

        LPRINTF (LOG_DEBUG, "lxa: _dos_open(): open() result: fd=%d, err=%d\n", fd, err);

        if (fd >= 0)
        {
            m68k_write_memory_32 (fh68k+36, fd);                // fh_Args
            m68k_write_memory_32 (fh68k+32, FILE_KIND_REGULAR); // fh_Func3
            return 0;  // Success
        }
        else
        {
            int amiga_err = errno2Amiga();
            m68k_write_memory_32 (fh68k+40, amiga_err);  // fh_Arg2 = error code
            return amiga_err;
        }
    }

    return 0;
}

int _dos_read (uint32_t fh68k, uint32_t buf68k, uint32_t len68k)
{
    DPRINTF (LOG_DEBUG, "lxa: _dos_read(): fh=0x%08x, buf68k=0x%08x, len68k=%d\n", fh68k, buf68k, len68k);

    int fd = m68k_read_memory_32 (fh68k+36);
    int kind = m68k_read_memory_32 (fh68k+32);
    DPRINTF (LOG_DEBUG, "                  -> fd = %d\n", fd);

    void *buf = _mgetstr (buf68k);
    if (kind == FILE_KIND_CONSOLE && !lxa_host_console_input_empty())
    {
        uint8_t *dst = (uint8_t *)buf;
        uint32_t count = 0;

        while (count < len68k)
        {
            int ch = lxa_host_console_input_pop();

            if (ch < 0)
                break;

            dst[count++] = (uint8_t)ch;

            if (ch == '\r' || ch == '\n')
                break;
        }

        return (int)count;
    }

    ssize_t l = read (fd, buf, len68k);

    if (l<0)
        m68k_write_memory_32 (fh68k+40, errno2Amiga()); // fh_Arg2

    return l;
}

int _dos_seek (uint32_t fh68k, int32_t position, int32_t mode)
{
    DPRINTF (LOG_DEBUG, "lxa: _dos_seek(): fh=0x%08x, position=0x%08x, mode=%d\n", fh68k, position, mode);

    int fd   = m68k_read_memory_32 (fh68k+36);

    int whence = 0;
    switch (mode)
    {
        case OFFSET_BEGINNING: whence = SEEK_SET; break;
        case OFFSET_CURRENT  : whence = SEEK_CUR; break;
        case OFFSET_END      : whence = SEEK_END; break;
        default:
            assert(FALSE);
    }

    off_t o = lseek (fd, position, whence);

    if (o<0)
        m68k_write_memory_32 (fh68k+40, errno2Amiga()); // fh_Arg2

    return o;
}

int _dos_setfilesize(uint32_t fh68k, int32_t offset, int32_t mode)
{
    int fd = m68k_read_memory_32(fh68k + 36);
    off_t base;
    off_t new_size;

    DPRINTF(LOG_DEBUG, "lxa: _dos_setfilesize(): fh=0x%08x, offset=%d, mode=%d\n",
            fh68k, offset, mode);

    switch (mode)
    {
        case OFFSET_BEGINNING:
            base = 0;
            break;

        case OFFSET_CURRENT:
            base = lseek(fd, 0, SEEK_CUR);
            break;

        case OFFSET_END:
            base = lseek(fd, 0, SEEK_END);
            break;

        default:
            errno = EINVAL;
            m68k_write_memory_32(fh68k + 40, errno2Amiga());
            return -1;
    }

    if (base < 0)
    {
        m68k_write_memory_32(fh68k + 40, errno2Amiga());
        return -1;
    }

    new_size = base + offset;
    if (new_size < 0)
    {
        errno = EINVAL;
        m68k_write_memory_32(fh68k + 40, errno2Amiga());
        return -1;
    }

    if (ftruncate(fd, new_size) != 0)
    {
        m68k_write_memory_32(fh68k + 40, errno2Amiga());
        return -1;
    }

    return (int)new_size;
}

static int trackdisk_errno_to_td_error(void)
{
    switch (errno)
    {
        case 0:
            return 0;
        case ENOENT:
        case ENODEV:
            return TDERR_DISKCHANGED_HOST;
        case EROFS:
        case EACCES:
        case EPERM:
            return TDERR_WRITEPROT_HOST;
        case ENOMEM:
            return TDERR_NOMEM_HOST;
        case EINVAL:
            return TDERR_BADSECPREAMBLE_HOST;
        case EIO:
            return TDERR_NOTSPECIFIED_HOST;
        default:
            return TDERR_NOTSPECIFIED_HOST;
    }
}

static int trackdisk_unit_path(uint32_t unit, char *path, size_t path_size)
{
    char drive_name[8];
    const char *drive_path;
    struct stat st;
    DIR *dir;
    struct dirent *de;

    snprintf(drive_name, sizeof(drive_name), "DF%u", unit);
    drive_path = vfs_get_drive_path(drive_name);
    if (!drive_path)
        return TDERR_DISKCHANGED_HOST;

    if (stat(drive_path, &st) != 0)
        return trackdisk_errno_to_td_error();

    if (S_ISREG(st.st_mode))
    {
        strncpy(path, drive_path, path_size - 1);
        path[path_size - 1] = '\0';
        return 0;
    }

    if (!S_ISDIR(st.st_mode))
        return TDERR_DISKCHANGED_HOST;

    dir = opendir(drive_path);
    if (!dir)
        return trackdisk_errno_to_td_error();

    while ((de = readdir(dir)) != NULL)
    {
        size_t len = strlen(de->d_name);
        if (len >= 4 && strcasecmp(de->d_name + len - 4, ".adf") == 0)
        {
            snprintf(path, path_size, "%s/%s", drive_path, de->d_name);
            closedir(dir);
            return 0;
        }
    }

    closedir(dir);
    return TDERR_DISKCHANGED_HOST;
}

static int trackdisk_check_bounds(off_t file_size, uint32_t offset, uint32_t length)
{
    uint64_t end = (uint64_t)offset + (uint64_t)length;

    if (end > (uint64_t)file_size)
        return TDERR_TOOFEWSECS_HOST;

    return 0;
}

int _trackdisk_read(uint32_t unit, uint32_t data_ptr, uint32_t length, uint32_t offset, uint32_t is_ext)
{
    char path[PATH_MAX];
    int fd;
    struct stat st;
    uint32_t i;
    int error;
    (void)is_ext;

    error = trackdisk_unit_path(unit, path, sizeof(path));
    if (error != 0)
        return error;

    fd = open(path, O_RDONLY);
    if (fd < 0)
        return trackdisk_errno_to_td_error();

    if (fstat(fd, &st) != 0)
    {
        error = trackdisk_errno_to_td_error();
        close(fd);
        return error;
    }

    error = trackdisk_check_bounds(st.st_size, offset, length);
    if (error != 0)
    {
        close(fd);
        return error;
    }

    if (lseek(fd, (off_t)offset, SEEK_SET) < 0)
    {
        error = trackdisk_errno_to_td_error();
        close(fd);
        return error;
    }

    for (i = 0; i < length; i++)
    {
        unsigned char byte;
        ssize_t n = read(fd, &byte, 1);
        if (n != 1)
        {
            close(fd);
            return (n < 0) ? trackdisk_errno_to_td_error() : TDERR_TOOFEWSECS_HOST;
        }
        m68k_write_memory_8(data_ptr + i, byte);
    }

    close(fd);
    return 0;
}

int _trackdisk_write(uint32_t unit, uint32_t data_ptr, uint32_t length, uint32_t offset, uint32_t is_ext)
{
    char path[PATH_MAX];
    int fd;
    struct stat st;
    uint32_t i;
    int error;
    (void)is_ext;

    error = trackdisk_unit_path(unit, path, sizeof(path));
    if (error != 0)
        return error;

    fd = open(path, O_RDWR);
    if (fd < 0)
        return trackdisk_errno_to_td_error();

    if (fstat(fd, &st) != 0)
    {
        error = trackdisk_errno_to_td_error();
        close(fd);
        return error;
    }

    error = trackdisk_check_bounds(st.st_size, offset, length);
    if (error != 0)
    {
        close(fd);
        return error;
    }

    if (lseek(fd, (off_t)offset, SEEK_SET) < 0)
    {
        error = trackdisk_errno_to_td_error();
        close(fd);
        return error;
    }

    for (i = 0; i < length; i++)
    {
        unsigned char byte = (unsigned char)m68k_read_memory_8(data_ptr + i);
        if (write(fd, &byte, 1) != 1)
        {
            error = trackdisk_errno_to_td_error();
            close(fd);
            return error;
        }
    }

    close(fd);
    return 0;
}

int _trackdisk_format(uint32_t unit, uint32_t data_ptr, uint32_t length, uint32_t offset, uint32_t is_ext)
{
    return _trackdisk_write(unit, data_ptr, length, offset, is_ext);
}

int _trackdisk_seek(uint32_t unit, uint32_t offset, uint32_t is_ext)
{
    char path[PATH_MAX];
    int fd;
    struct stat st;
    int error;
    (void)is_ext;

    error = trackdisk_unit_path(unit, path, sizeof(path));
    if (error != 0)
        return error;

    fd = open(path, O_RDONLY);
    if (fd < 0)
        return trackdisk_errno_to_td_error();

    if (fstat(fd, &st) != 0)
    {
        error = trackdisk_errno_to_td_error();
        close(fd);
        return error;
    }

    if ((uint64_t)offset > (uint64_t)st.st_size)
    {
        close(fd);
        return TDERR_SEEKERROR_HOST;
    }

    if (lseek(fd, (off_t)offset, SEEK_SET) < 0)
    {
        error = trackdisk_errno_to_td_error();
        close(fd);
        return error;
    }

    close(fd);
    return 0;
}

/*
 * Phase 16: Console Device Enhancement
 *
 * This implements Amiga console CSI (Control Sequence Introducer) escape sequence
 * translation to ANSI terminal sequences. The Amiga uses 0x9B as CSI, while
 * standard ANSI terminals use ESC[ (0x1B 0x5B).
 *
 * Supported sequences:
 * - Cursor positioning: H (absolute), A/B/C/D (relative), E/F (line start)
 * - Screen/line control: J (erase display), K (erase line), L/M (insert/delete line)
 * - Character control: @ (insert), P (delete)
 * - Scrolling: S (scroll up), T (scroll down)
 * - Text attributes: m (SGR - colors, bold, underline, etc.)
 * - Cursor visibility: p
 * - Window queries: q (reports window size), n (cursor position)
 */


/* Console state for window size queries (used in DPRINTF when debug enabled) */
static int g_console_rows __attribute__((unused)) = 24;
static int g_console_cols __attribute__((unused)) = 80;

/* Parse CSI parameters from buffer, returns number of parameters found */
static int _csi_parse_params(const char *buf, int len, int *params, int max_params)
{
    int num_params = 0;
    int current = 0;
    bool has_digits = false;

    for (int i = 0; i < len && num_params < max_params; i++) {
        char c = buf[i];
        if (c >= '0' && c <= '9') {
            current = current * 10 + (c - '0');
            has_digits = true;
        } else if (c == ';') {
            params[num_params++] = has_digits ? current : 1;
            current = 0;
            has_digits = false;
        } else if (c == ' ' || c == '?' || c == '>') {
            /* Skip intermediate characters */
            continue;
        }
    }
    /* Don't forget the last parameter */
    if (has_digits || num_params > 0) {
        params[num_params++] = has_digits ? current : 1;
    }
    return num_params;
}

/* Check if buffer contains a specific prefix (for sequences like "0 " in "0 p") */
static bool _csi_has_prefix(const char *buf, int len, const char *prefix)
{
    int plen = strlen(prefix);
    if (len < plen)
        return false;
    return strncmp(buf, prefix, plen) == 0;
}

/* Process a complete Amiga CSI sequence and emit ANSI equivalent */
static void _csi_process(const char *buf, int len, char final_byte)
{
    int params[CSI_MAX_PARAMS] = {0};
    int num_params = _csi_parse_params(buf, len, params, CSI_MAX_PARAMS);

    /* Default parameter is 1 for most movement commands */
    int p1 = (num_params >= 1 && params[0] > 0) ? params[0] : 1;
    int p2 = (num_params >= 2 && params[1] > 0) ? params[1] : 1;

    switch (final_byte) {
        /*
         * Cursor Movement
         */
        case 'H':  /* CUP - Cursor Position: CSI row;col H */
        case 'f':  /* HVP - Horizontal/Vertical Position (same as H) */
            if (num_params == 0) {
                /* Home position */
                fputs(CSI "H", stdout);
            } else {
                fprintf(stdout, CSI "%d;%dH", p1, p2);
            }
            break;

        case 'A':  /* CUU - Cursor Up */
            fprintf(stdout, CSI "%dA", p1);
            break;

        case 'B':  /* CUD - Cursor Down */
            fprintf(stdout, CSI "%dB", p1);
            break;

        case 'C':  /* CUF - Cursor Forward (Right) */
            fprintf(stdout, CSI "%dC", p1);
            break;

        case 'D':  /* CUB - Cursor Backward (Left) */
            fprintf(stdout, CSI "%dD", p1);
            break;

        case 'E':  /* CNL - Cursor Next Line */
            fprintf(stdout, CSI "%dE", p1);
            break;

        case 'F':  /* CPL - Cursor Preceding Line */
            fprintf(stdout, CSI "%dF", p1);
            break;

        case 'G':  /* CHA - Cursor Horizontal Absolute (column) */
            fprintf(stdout, CSI "%dG", p1);
            break;

        case 'I':  /* CHT - Cursor Horizontal Tab */
            for (int i = 0; i < p1; i++)
                fputc('\t', stdout);
            break;

        case 'Z':  /* CBT - Cursor Backward Tab */
            /* ANSI supports this directly */
            fprintf(stdout, CSI "%dZ", p1);
            break;

        /*
         * Erase Functions
         */
        case 'J':  /* ED - Erase in Display */
            /* 0 = cursor to end, 1 = start to cursor, 2 = entire display */
            if (num_params == 0 || params[0] == 0) {
                fputs(CSI "J", stdout);  /* Erase to end of display */
            } else {
                fprintf(stdout, CSI "%dJ", params[0]);
            }
            break;

        case 'K':  /* EL - Erase in Line */
            /* 0 = cursor to end, 1 = start to cursor, 2 = entire line */
            if (num_params == 0 || params[0] == 0) {
                fputs(CSI "K", stdout);  /* Erase to end of line */
            } else {
                fprintf(stdout, CSI "%dK", params[0]);
            }
            break;

        /*
         * Insert/Delete
         */
        case 'L':  /* IL - Insert Line(s) */
            fprintf(stdout, CSI "%dL", p1);
            break;

        case 'M':  /* DL - Delete Line(s) */
            fprintf(stdout, CSI "%dM", p1);
            break;

        case '@':  /* ICH - Insert Character(s) */
            fprintf(stdout, CSI "%d@", p1);
            break;

        case 'P':  /* DCH - Delete Character(s) */
            fprintf(stdout, CSI "%dP", p1);
            break;

        /*
         * Scrolling
         */
        case 'S':  /* SU - Scroll Up */
            fprintf(stdout, CSI "%dS", p1);
            break;

        case 'T':  /* SD - Scroll Down */
            fprintf(stdout, CSI "%dT", p1);
            break;

        /*
         * Text Attributes (SGR - Select Graphic Rendition)
         */
        case 'm':
            if (num_params == 0) {
                /* Reset all attributes */
                fputs(CSI "0m", stdout);
            } else {
                /* Build ANSI SGR sequence from Amiga parameters */
                fputs(CSI, stdout);
                for (int i = 0; i < num_params; i++) {
                    int attr = params[i];
                    if (i > 0) fputc(';', stdout);

                    switch (attr) {
                        /* Reset */
                        case 0:  fprintf(stdout, "0"); break;

                        /* Styles */
                        case 1:  fprintf(stdout, "1"); break;   /* Bold */
                        case 2:  fprintf(stdout, "2"); break;   /* Faint/Dim */
                        case 3:  fprintf(stdout, "3"); break;   /* Italic */
                        case 4:  fprintf(stdout, "4"); break;   /* Underline */
                        case 7:  fprintf(stdout, "7"); break;   /* Inverse/Reverse */
                        case 8:  fprintf(stdout, "8"); break;   /* Concealed/Hidden */

                        /* Style off (V36+) */
                        case 22: fprintf(stdout, "22"); break;  /* Normal intensity */
                        case 23: fprintf(stdout, "23"); break;  /* Italic off */
                        case 24: fprintf(stdout, "24"); break;  /* Underline off */
                        case 27: fprintf(stdout, "27"); break;  /* Inverse off */
                        case 28: fprintf(stdout, "28"); break;  /* Concealed off */

                        /* Foreground colors (30-37, 39=default) */
                        case 30: case 31: case 32: case 33:
                        case 34: case 35: case 36: case 37:
                        case 39:
                            fprintf(stdout, "%d", attr);
                            break;

                        /* Background colors (40-47, 49=default) */
                        case 40: case 41: case 42: case 43:
                        case 44: case 45: case 46: case 47:
                        case 49:
                            fprintf(stdout, "%d", attr);
                            break;

                        default:
                            /* Pass through unknown attributes */
                            fprintf(stdout, "%d", attr);
                            break;
                    }
                }
                fputs("m", stdout);
            }
            break;

        /*
         * Cursor Visibility
         */
        case 'p':
            if (_csi_has_prefix(buf, len, "0 ")) {
                /* Cursor invisible */
                fputs(CSI "?25l", stdout);
            } else if (_csi_has_prefix(buf, len, " ") || _csi_has_prefix(buf, len, "1 ")) {
                /* Cursor visible */
                fputs(CSI "?25h", stdout);
            }
            break;

        /*
         * Window Status/Queries
         */
        case 'q':
            if (_csi_has_prefix(buf, len, "0 ")) {
                /* Window Status Request - Amiga expects response:
                 * CSI 1;1;<bottom>;<right> r
                 * We can't inject input, so just log for now */
                DPRINTF(LOG_DEBUG, "lxa: Console window status request (rows=%d, cols=%d)\n",
                        g_console_rows, g_console_cols);
            }
            break;

        case 'n':
            if (num_params >= 1 && params[0] == 6) {
                /* Device Status Report (cursor position query)
                 * Amiga expects response: CSI row;col R
                 * We can't inject input, so just log for now */
                DPRINTF(LOG_DEBUG, "lxa: Console cursor position query\n");
            }
            break;

        /*
         * Tab Control
         */
        case 'W':
            if (_csi_has_prefix(buf, len, "0")) {
                /* Set tab at current position - not directly supported in ANSI */
                DPRINTF(LOG_DEBUG, "lxa: Set tab stop (ignored)\n");
            } else if (_csi_has_prefix(buf, len, "2")) {
                /* Clear tab at current position */
                fputs(CSI "0g", stdout);
            } else if (_csi_has_prefix(buf, len, "5")) {
                /* Clear all tabs */
                fputs(CSI "3g", stdout);
            }
            break;

        /*
         * Mode Control
         */
        case 'h':  /* SM - Set Mode */
            if (_csi_has_prefix(buf, len, "20")) {
                /* Set LF mode (LF acts as CR+LF) - not directly translatable */
                DPRINTF(LOG_DEBUG, "lxa: Set LF mode (ignored)\n");
            } else if (_csi_has_prefix(buf, len, "?7")) {
                /* Enable autowrap */
                fputs(CSI "?7h", stdout);
            }
            break;

        case 'l':  /* RM - Reset Mode */
            if (_csi_has_prefix(buf, len, "20")) {
                /* Reset LF mode */
                DPRINTF(LOG_DEBUG, "lxa: Reset LF mode (ignored)\n");
            } else if (_csi_has_prefix(buf, len, "?7")) {
                /* Disable autowrap */
                fputs(CSI "?7l", stdout);
            }
            break;

        /*
         * Raw Events (Amiga-specific, not translatable)
         */
        case '{':  /* Set Raw Events */
            DPRINTF(LOG_DEBUG, "lxa: Set raw events %d (ignored)\n", p1);
            break;

        case '}':  /* Reset Raw Events */
            DPRINTF(LOG_DEBUG, "lxa: Reset raw events %d (ignored)\n", p1);
            break;

        /*
         * Page/Window Setup (Amiga-specific)
         */
        case 't':  /* Set page length */
            DPRINTF(LOG_DEBUG, "lxa: Set page length %d (ignored)\n", p1);
            break;

        case 'u':  /* Set line length */
            DPRINTF(LOG_DEBUG, "lxa: Set line length %d (ignored)\n", p1);
            break;

        case 'x':  /* Set left offset */
            DPRINTF(LOG_DEBUG, "lxa: Set left offset %d (ignored)\n", p1);
            break;

        case 'y':  /* Set top offset */
            DPRINTF(LOG_DEBUG, "lxa: Set top offset %d (ignored)\n", p1);
            break;

        default:
            DPRINTF(LOG_DEBUG, "lxa: Unhandled CSI sequence: %.*s%c\n", len, buf, final_byte);
            break;
    }
}

/* Process special C0/C1 control characters */
static void _console_control_char(uint8_t c)
{
    switch (c) {
        case 0x07:  /* BEL - Bell */
            fputc('\a', stdout);
            break;
        case 0x08:  /* BS - Backspace */
            fputc('\b', stdout);
            break;
        case 0x09:  /* HT - Horizontal Tab */
            fputc('\t', stdout);
            break;
        case 0x0A:  /* LF - Line Feed */
            fputc('\n', stdout);
            break;
        case 0x0B:  /* VT - Vertical Tab (move up one line on Amiga) */
            fputs(CSI "A", stdout);
            break;
        case 0x0C:  /* FF - Form Feed (clear screen on Amiga) */
            fputs(CSI "2J" CSI "H", stdout);
            break;
        case 0x0D:  /* CR - Carriage Return */
            fputc('\r', stdout);
            break;
        case 0x84:  /* IND - Index (move down one line) */
            fputs(CSI "D", stdout);
            break;
        case 0x85:  /* NEL - Next Line (CR+LF) */
            fputs("\r\n", stdout);
            break;
        case 0x8D:  /* RI - Reverse Index (move up one line) */
            fputs(CSI "M", stdout);
            break;
        default:
            /* Pass through printable characters */
            if (c >= 0x20 && c < 0x7F) {
                fputc(c, stdout);
            } else if (c >= 0xA0) {
                /* High ASCII / Latin-1 */
                fputc(c, stdout);
            }
            break;
    }
}

int _dos_write(uint32_t fh68k, uint32_t buf68k, uint32_t len68k)
{
    DPRINTF(LOG_DEBUG, "lxa: _dos_write(): fh68k=0x%08x, buf68k=0x%08x, len68k=%d\n", fh68k, buf68k, len68k);

    int fd   = m68k_read_memory_32(fh68k + 36);
    int kind = m68k_read_memory_32(fh68k + 32);
    DPRINTF(LOG_DEBUG, "                  -> fd=%d, kind=%d\n", fd, kind);

    char *buf = _mgetstr(buf68k);
    ssize_t l = 0;

    switch (kind) {
        case FILE_KIND_REGULAR:
        {
            l = write(fd, buf, len68k);

            if (l < 0)
                m68k_write_memory_32(fh68k + 40, errno2Amiga()); // fh_Arg2

            break;
        }
        case FILE_KIND_CONSOLE:
        {
            static bool     bCSI = FALSE;
            static bool     bESC = FALSE;
            static char     csiBuf[CSI_BUF_LEN];
            static uint16_t csiBufLen = 0;

            l = len68k;

            if (g_console_output_hook && l > 0)
                g_console_output_hook(buf, (int)l);

            for (int i = 0; i < l; i++) {
                uint8_t uc = (uint8_t)buf[i];

                if (!bCSI && !bESC) {
                    if (uc == 0x9B) {
                        /* Amiga CSI (single byte) */
                        bCSI = TRUE;
                        csiBufLen = 0;
                        continue;
                    } else if (uc == 0x1B) {
                        /* ESC - might be start of ESC[ sequence */
                        bESC = TRUE;
                        continue;
                    } else {
                        /* Regular character or control code */
                        _console_control_char(uc);
                    }
                } else if (bESC) {
                    if (uc == '[') {
                        /* ESC[ is equivalent to 0x9B CSI */
                        bESC = FALSE;
                        bCSI = TRUE;
                        csiBufLen = 0;
                    } else if (uc == 'c') {
                        /* ESC c - Reset to Initial State */
                        fputs(CSI "!p" CSI "?3;4l" CSI "4l" CSI "0m", stdout);
                        bESC = FALSE;
                    } else {
                        /* Unknown ESC sequence, output both */
                        fputc(0x1B, stdout);
                        fputc(uc, stdout);
                        bESC = FALSE;
                    }
                } else if (bCSI) {
                    /*
                     * CSI sequence parsing:
                     * 0x30–0x3F (ASCII 0–9:;<=>?)                  parameter bytes
                     * 0x20–0x2F (ASCII space and !"#$%&'()*+,-./)  intermediate bytes
                     * 0x40–0x7E (ASCII @A–Z[\]^_`a–z{|}~)          final byte
                     */
                    if (uc >= 0x40 && uc <= 0x7E) {
                        /* Final byte - process the sequence */
                        csiBuf[csiBufLen] = '\0';
                        _csi_process(csiBuf, csiBufLen, uc);
                        bCSI = FALSE;
                        csiBufLen = 0;
                    } else if (uc >= 0x20 && uc <= 0x3F) {
                        /* Parameter or intermediate byte */
                        if (csiBufLen < CSI_BUF_LEN - 1)
                            csiBuf[csiBufLen++] = uc;
                    } else {
                        /* Invalid byte in CSI sequence - abort */
                        bCSI = FALSE;
                        csiBufLen = 0;
                        _console_control_char(uc);
                    }
                }
            }
            /* Flush stdout after each write to console to ensure output is visible immediately */
            fflush(stdout);
            break;
        }
        default:
            assert(FALSE);
    }

    return l;
}

int _dos_flush (uint32_t fh68k)
{
    DPRINTF (LOG_DEBUG, "lxa: _dos_flush(): fh=0x%08x\n", fh68k);

    int fd   = m68k_read_memory_32 (fh68k+36);
    int kind = m68k_read_memory_32 (fh68k+32);

    if (kind == FILE_KIND_REGULAR)
    {
        if (fsync(fd) == 0)
            return 1;
        else
        {
            m68k_write_memory_32 (fh68k+40, errno2Amiga());
            return 0;
        }
    }
    else if (kind == FILE_KIND_CONSOLE)
    {
        /* For console, just flush stdout */
        fflush(stdout);
        return 1;
    }

    return 0;
}

int _dos_close (uint32_t fh68k)
{
    DPRINTF (LOG_DEBUG, "lxa: _dos_close(): fh=0x%08x\n", fh68k);

    int fd = m68k_read_memory_32 (fh68k+36);

    _record_lock_release_all(fh68k);

    close(fd);

    DPRINTF (LOG_DEBUG, "lxa: _dos_close(): fh=0x%08x done\n", fh68k);
    return 1;
}

/*
 * Phase 3: Lock and Examine API host-side implementation
 */

/* Convert Linux stat mode to Amiga protection bits */
static uint32_t _unix_mode_to_amiga(mode_t mode)
{
    uint32_t prot = 0;
    
    /* Note: Amiga protection bits are inverted - bit SET means permission denied */
    /* We set the bit to 1 if permission is NOT granted, 0 if granted */
    
    /* Owner/basic permissions (bits 0-3) */
    if (!(mode & S_IRUSR)) prot |= FIBF_READ;
    if (!(mode & S_IWUSR)) prot |= FIBF_WRITE;
    if (!(mode & S_IXUSR)) prot |= FIBF_EXECUTE;
    /* Note: delete permission is tied to write permission */
    if (!(mode & S_IWUSR)) prot |= FIBF_DELETE;
    
    /* Group permissions (bits 8-11) */
    if (!(mode & S_IRGRP)) prot |= (1 << 11);  /* FIBB_GRP_READ */
    if (!(mode & S_IWGRP)) prot |= (1 << 10);  /* FIBB_GRP_WRITE */
    if (!(mode & S_IXGRP)) prot |= (1 << 9);   /* FIBB_GRP_EXECUTE */
    if (!(mode & S_IWGRP)) prot |= (1 << 8);   /* FIBB_GRP_DELETE */
    
    /* Other permissions (bits 12-15) */
    if (!(mode & S_IROTH)) prot |= (1 << 15);  /* FIBB_OTR_READ */
    if (!(mode & S_IWOTH)) prot |= (1 << 14);  /* FIBB_OTR_WRITE */
    if (!(mode & S_IXOTH)) prot |= (1 << 13);  /* FIBB_OTR_EXECUTE */
    if (!(mode & S_IWOTH)) prot |= (1 << 12);  /* FIBB_OTR_DELETE */
    
    /* Amiga also has archive and script flags - leave them as 0 (not set) */
    
    return prot;
}

/* Convert Unix time_t to Amiga DateStamp */
static void _unix_timespec_to_datestamp(time_t unix_time, long unix_nsec, uint32_t ds68k)
{
    time_t amiga_time = unix_time - AMIGA_EPOCH_OFFSET;
    if (amiga_time < 0) amiga_time = 0;
    
    /* DateStamp format: days, minutes, ticks (50ths of a second) */
    uint32_t days = amiga_time / (24 * 60 * 60);
    uint32_t remaining = amiga_time % (24 * 60 * 60);
    uint32_t minutes = remaining / 60;
    uint32_t ticks = (remaining % 60) * 50 + (uint32_t)(unix_nsec / 20000000L);

    m68k_write_memory_32(ds68k + 0, days);
    m68k_write_memory_32(ds68k + 4, minutes);
    m68k_write_memory_32(ds68k + 8, ticks);
}

static time_t _datestamp68k_to_unix_time(uint32_t ds68k)
{
    uint32_t days = m68k_read_memory_32(ds68k + 0);
    uint32_t minutes = m68k_read_memory_32(ds68k + 4);
    uint32_t ticks = m68k_read_memory_32(ds68k + 8);
    uint64_t amiga_time = (uint64_t)days * 24ULL * 60ULL * 60ULL +
                          (uint64_t)minutes * 60ULL +
                          (uint64_t)ticks / 50ULL;

    return (time_t)(amiga_time + AMIGA_EPOCH_OFFSET);
}

static bool _resolve_amiga_path_host(const char *amiga_path, char *linux_path, size_t maxlen)
{
    if (vfs_resolve_path(amiga_path, linux_path, maxlen))
        return true;

    _dos_path2linux(amiga_path, linux_path, (int)maxlen);
    return linux_path[0] != '\0';
}

/* Lock a file or directory */
uint32_t _dos_lock(uint32_t name68k, int32_t mode)
{
    char *amiga_path = _mgetstr(name68k);
    char linux_path[PATH_MAX];
    
    DPRINTF(LOG_DEBUG, "lxa: _dos_lock: amiga_path='%s'\n", amiga_path);
    
    /* Resolve the Amiga path to Linux path */
    if (!vfs_resolve_path(amiga_path, linux_path, sizeof(linux_path))) {
        /* Try legacy fallback */
        _dos_path2linux(amiga_path, linux_path, sizeof(linux_path));
    }
    
    DPRINTF(LOG_DEBUG, "lxa: _dos_lock: linux_path='%s'\n", linux_path);
    
    /* Check if the path exists */
    struct stat st;
    if (stat(linux_path, &st) != 0) {
        DPRINTF(LOG_DEBUG, "lxa: _dos_lock: stat failed: %s\n", strerror(errno));
        return 0; /* ERROR_OBJECT_NOT_FOUND will be set by caller */
    }
    
    /* Allocate a lock entry */
    int lock_id = _lock_alloc();
    if (lock_id == 0) {
        DPRINTF(LOG_DEBUG, "lxa: _dos_lock: no free lock slots\n");
        return 0;
    }
    
    lock_entry_t *lock = &g_locks[lock_id];
    strncpy(lock->linux_path, linux_path, sizeof(lock->linux_path) - 1);
    strncpy(lock->amiga_path, amiga_path, sizeof(lock->amiga_path) - 1);
    lock->is_dir = S_ISDIR(st.st_mode);
    lock->dir = NULL;
    lock->access_mode = mode;
    
    DPRINTF(LOG_DEBUG, "lxa: _dos_lock: allocated lock_id=%d\n", lock_id);
    
    return lock_id;
}

/* Unlock a file or directory */
void _dos_unlock(uint32_t lock_id)
{
    DPRINTF(LOG_DEBUG, "lxa: _dos_unlock(): lock_id=%d\n", lock_id);
    
    lock_entry_t *lock = _lock_get(lock_id);
    if (!lock) return;
    
    lock->refcount--;
    if (lock->refcount <= 0) {
        _lock_free(lock_id);
    }
}

/* Duplicate a lock */
uint32_t _dos_duplock(uint32_t lock_id)
{
    DPRINTF(LOG_DEBUG, "lxa: _dos_duplock(): lock_id=%d\n", lock_id);
    
    if (lock_id == 0) return 0;
    
    lock_entry_t *lock = _lock_get(lock_id);
    if (!lock) return 0;
    
    /* Create a new lock with the same path */
    int new_lock_id = _lock_alloc();
    if (new_lock_id == 0) return 0;
    
    lock_entry_t *new_lock = &g_locks[new_lock_id];
    strncpy(new_lock->linux_path, lock->linux_path, sizeof(new_lock->linux_path) - 1);
    strncpy(new_lock->amiga_path, lock->amiga_path, sizeof(new_lock->amiga_path) - 1);
    new_lock->is_dir = lock->is_dir;
    new_lock->dir = NULL;
    new_lock->access_mode = lock->access_mode;
    
    DPRINTF(LOG_DEBUG, "lxa: _dos_duplock(): new_lock_id=%d\n", new_lock_id);
    return new_lock_id;
}

uint32_t _dos_lockrecord(uint32_t fh68k, uint32_t offset, uint32_t length,
                                uint32_t mode, uint32_t timeout)
{
    int kind;
    int fd;
    struct stat st;
    bool exclusive;
    bool immediate;
    uint64_t deadline_us = 0;

    DPRINTF(LOG_DEBUG,
            "lxa: _dos_lockrecord(): fh=0x%08x offset=%u length=%u mode=%u timeout=%u\n",
            fh68k, offset, length, mode, timeout);

    if (fh68k == 0)
    {
        return 0;
    }

    kind = m68k_read_memory_32(fh68k + 32);
    fd = m68k_read_memory_32(fh68k + 36);

    if (mode > 3)
    {
        m68k_write_memory_32(fh68k + 40, ERROR_BAD_NUMBER);
        return 0;
    }

    if (kind != FILE_KIND_REGULAR)
    {
        m68k_write_memory_32(fh68k + 40, ERROR_OBJECT_WRONG_TYPE);
        return 0;
    }

    if (fstat(fd, &st) != 0)
    {
        m68k_write_memory_32(fh68k + 40, errno2Amiga());
        return 0;
    }

    exclusive = (mode == 0 || mode == 1);
    immediate = (mode == 1 || mode == 3);

    if (!immediate)
    {
        deadline_us = _timer_get_time_us() + ((uint64_t)timeout * 20000ULL);
    }

    for (;;)
    {
        if (_record_lock_find_conflict(st.st_dev, st.st_ino, fh68k,
                                       offset, length, exclusive) < 0)
        {
            if (!_record_lock_add(st.st_dev, st.st_ino, fh68k,
                                  offset, length, exclusive))
            {
                m68k_write_memory_32(fh68k + 40, ERROR_NO_FREE_STORE);
                return 0;
            }

            m68k_write_memory_32(fh68k + 40, 0);
            return 1;
        }

        if (immediate)
        {
            m68k_write_memory_32(fh68k + 40, ERROR_LOCK_COLLISION);
            return 0;
        }

        if (timeout == 0 || _timer_get_time_us() >= deadline_us)
        {
            m68k_write_memory_32(fh68k + 40, ERROR_LOCK_TIMEOUT);
            return 0;
        }

        usleep(1000);
    }
}

uint32_t _dos_unlockrecord(uint32_t fh68k, uint32_t offset, uint32_t length)
{
    int kind;
    int fd;
    struct stat st;

    DPRINTF(LOG_DEBUG,
            "lxa: _dos_unlockrecord(): fh=0x%08x offset=%u length=%u\n",
            fh68k, offset, length);

    if (fh68k == 0)
    {
        return 0;
    }

    kind = m68k_read_memory_32(fh68k + 32);
    fd = m68k_read_memory_32(fh68k + 36);

    if (kind != FILE_KIND_REGULAR)
    {
        m68k_write_memory_32(fh68k + 40, ERROR_OBJECT_WRONG_TYPE);
        return 0;
    }

    if (fstat(fd, &st) != 0)
    {
        m68k_write_memory_32(fh68k + 40, errno2Amiga());
        return 0;
    }

    if (!_record_lock_remove(st.st_dev, st.st_ino, fh68k, offset, length))
    {
        m68k_write_memory_32(fh68k + 40, ERROR_RECORD_NOT_LOCKED);
        return 0;
    }

    m68k_write_memory_32(fh68k + 40, 0);
    return 1;
}

/* FileInfoBlock offsets (from dos/dos.h) */
#define FIB_fib_DiskKey      0   /* LONG */
#define FIB_fib_DirEntryType 4   /* LONG */
#define FIB_fib_FileName     8   /* char[108] */
#define FIB_fib_Protection   116 /* LONG */
#define FIB_fib_EntryType    120 /* LONG (obsolete) */
#define FIB_fib_Size         124 /* LONG */
#define FIB_fib_NumBlocks    128 /* LONG */
#define FIB_fib_Date         132 /* struct DateStamp (12 bytes) */
#define FIB_fib_Comment      144 /* char[80] */
#define FIB_fib_OwnerUID     224 /* UWORD */
#define FIB_fib_OwnerGID     226 /* UWORD */
#define FIB_fib_Reserved     228 /* char[32] */
#define FIB_SIZE             260

static uint32_t _default_owner_info_from_stat(const struct stat *st)
{
    return (((uint32_t)st->st_uid & 0xffffU) << 16) | ((uint32_t)st->st_gid & 0xffffU);
}

static void _write_owner_info(uint32_t fib68k, uint32_t owner_info)
{
    m68k_write_memory_16(fib68k + FIB_fib_OwnerUID, (owner_info >> 16) & 0xffffU);
    m68k_write_memory_16(fib68k + FIB_fib_OwnerGID, owner_info & 0xffffU);
}

static uint32_t _read_owner_info(const char *linux_path, const struct stat *st)
{
    char owner_buf[32];
    char *end = NULL;
    unsigned long owner_info;

    memset(owner_buf, 0, sizeof(owner_buf));

#ifdef HAVE_XATTR
    {
        ssize_t len = getxattr(linux_path, "user.amiga.owner", owner_buf, sizeof(owner_buf) - 1);

        if (len > 0)
        {
            owner_buf[len] = '\0';
            owner_info = strtoul(owner_buf, &end, 16);
            if (end != owner_buf)
                return (uint32_t)owner_info;
        }
    }
#endif

    {
        char sidecar_path[PATH_MAX + 8];
        FILE *f;

        snprintf(sidecar_path, sizeof(sidecar_path), "%s.owner", linux_path);
        f = fopen(sidecar_path, "r");
        if (f)
        {
            if (fgets(owner_buf, sizeof(owner_buf), f))
            {
                owner_info = strtoul(owner_buf, &end, 16);
                fclose(f);
                if (end != owner_buf)
                    return (uint32_t)owner_info;
            }
            else
            {
                fclose(f);
            }
        }
    }

    return _default_owner_info_from_stat(st);
}

/* Read file comment from xattr or sidecar file */
static void _read_file_comment(const char *linux_path, uint32_t fib68k)
{
    char comment[80];
    memset(comment, 0, sizeof(comment));
    
    #ifdef HAVE_XATTR
    /* Try to read from extended attribute first */
    ssize_t len = getxattr(linux_path, "user.amiga.comment", comment, sizeof(comment) - 1);
    if (len > 0) {
        comment[len] = '\0';
        for (int i = 0; i < (int)len && i < 79; i++) {
            m68k_write_memory_8(fib68k + FIB_fib_Comment + i, comment[i]);
        }
        m68k_write_memory_8(fib68k + FIB_fib_Comment + ((len < 79) ? len : 79), 0);
        return;
    }
    #endif
    
    /* Fallback: try sidecar file */
    char sidecar_path[PATH_MAX + 10];
    snprintf(sidecar_path, sizeof(sidecar_path), "%s.comment", linux_path);
    
    FILE *f = fopen(sidecar_path, "r");
    if (f) {
        if (fgets(comment, sizeof(comment), f)) {
            /* Remove trailing newline if present */
            int clen = strlen(comment);
            if (clen > 0 && comment[clen - 1] == '\n') {
                comment[clen - 1] = '\0';
                clen--;
            }
            for (int i = 0; i < clen && i < 79; i++) {
                m68k_write_memory_8(fib68k + FIB_fib_Comment + i, comment[i]);
            }
            m68k_write_memory_8(fib68k + FIB_fib_Comment + ((clen < 79) ? clen : 79), 0);
        }
        fclose(f);
        return;
    }
    
    /* No comment found - already cleared to 0 by FIB clear */
    m68k_write_memory_8(fib68k + FIB_fib_Comment, 0);
}

/* Examine a lock (fill FileInfoBlock) */
int _dos_examine(uint32_t lock_id, uint32_t fib68k)
{
    DPRINTF(LOG_DEBUG, "lxa: _dos_examine(): lock_id=%d, fib68k=0x%08x\n", lock_id, fib68k);
    
    lock_entry_t *lock = _lock_get(lock_id);
    if (!lock) {
        DPRINTF(LOG_DEBUG, "lxa: _dos_examine(): invalid lock_id\n");
        return 0;
    }
    
    struct stat st;
    if (stat(lock->linux_path, &st) != 0) {
        DPRINTF(LOG_DEBUG, "lxa: _dos_examine(): stat failed: %s\n", strerror(errno));
        return 0;
    }
    
    /* Clear the FIB first */
    for (int i = 0; i < FIB_SIZE; i++) {
        m68k_write_memory_8(fib68k + i, 0);
    }
    
    /* Get the filename from the path */
    const char *filename = strrchr(lock->linux_path, '/');
    filename = filename ? filename + 1 : lock->linux_path;
    
    /* Fill the FileInfoBlock */
    m68k_write_memory_32(fib68k + FIB_fib_DiskKey, (uint32_t)st.st_ino);
    
    int32_t type = S_ISDIR(st.st_mode) ? ST_USERDIR : ST_FILE;
    m68k_write_memory_32(fib68k + FIB_fib_DirEntryType, type);
    m68k_write_memory_32(fib68k + FIB_fib_EntryType, type);
    
    /* Write filename (null-terminated C string, NOT BSTR) */
    int namelen = strlen(filename);
    if (namelen > 107) namelen = 107;  /* Leave room for null terminator */
    for (int i = 0; i < namelen; i++) {
        m68k_write_memory_8(fib68k + FIB_fib_FileName + i, filename[i]);
    }
    m68k_write_memory_8(fib68k + FIB_fib_FileName + namelen, 0);  /* Null terminator */
    
    m68k_write_memory_32(fib68k + FIB_fib_Protection, _unix_mode_to_amiga(st.st_mode));
    m68k_write_memory_32(fib68k + FIB_fib_Size, st.st_size);
    m68k_write_memory_32(fib68k + FIB_fib_NumBlocks, (st.st_size + 511) / 512);
    
    _unix_timespec_to_datestamp(st.st_mtime, st.st_mtim.tv_nsec, fib68k + FIB_fib_Date);
    
    /* Read file comment from xattr or sidecar */
    _read_file_comment(lock->linux_path, fib68k);
    
    _write_owner_info(fib68k, _read_owner_info(lock->linux_path, &st));
    
    /* If this is a directory, open it for ExNext iteration */
    if (S_ISDIR(st.st_mode) && !lock->dir) {
        lock->dir = opendir(lock->linux_path);
        DPRINTF(LOG_DEBUG, "lxa: _dos_examine(): opened dir handle for ExNext\n");
    }
    
    DPRINTF(LOG_DEBUG, "lxa: _dos_examine(): success, type=%d, size=%ld\n", type, st.st_size);
    return 1;
}

/* Examine next entry in directory */
int _dos_exnext(uint32_t lock_id, uint32_t fib68k)
{
    DPRINTF(LOG_DEBUG, "lxa: _dos_exnext(): lock_id=%d, fib68k=0x%08x\n", lock_id, fib68k);
    
    lock_entry_t *lock = _lock_get(lock_id);
    if (!lock) {
        DPRINTF(LOG_DEBUG, "lxa: _dos_exnext(): invalid lock_id\n");
        return 0;
    }
    
    if (!lock->is_dir) {
        DPRINTF(LOG_DEBUG, "lxa: _dos_exnext(): not a directory\n");
        return 0;
    }
    
    /* Open directory if not already open */
    if (!lock->dir) {
        lock->dir = opendir(lock->linux_path);
        if (!lock->dir) {
            DPRINTF(LOG_DEBUG, "lxa: _dos_exnext(): opendir failed: %s\n", strerror(errno));
            return 0;
        }
    }
    
    /* Read next entry, skipping . and .. */
    struct dirent *de;
    while ((de = readdir(lock->dir)) != NULL) {
        if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0)
            continue;
        break;
    }
    
    if (!de) {
        DPRINTF(LOG_DEBUG, "lxa: _dos_exnext(): no more entries\n");
        closedir(lock->dir);
        lock->dir = NULL;
        return 0; /* ERROR_NO_MORE_ENTRIES */
    }
    
    /* Build full path to get stat info */
    char fullpath[PATH_MAX];
    int n = snprintf(fullpath, sizeof(fullpath), "%s/%s", lock->linux_path, de->d_name);
    if (n >= (int)sizeof(fullpath)) {
        /* Path too long, skip this entry */
        DPRINTF(LOG_DEBUG, "lxa: _dos_exnext(): path too long, skipping\n");
        return _dos_exnext(lock_id, fib68k);
    }
    
    struct stat st;
    if (stat(fullpath, &st) != 0) {
        DPRINTF(LOG_DEBUG, "lxa: _dos_exnext(): stat failed for %s: %s\n", fullpath, strerror(errno));
        /* Skip this entry and try next */
        return _dos_exnext(lock_id, fib68k);
    }
    
    /* Clear the FIB first */
    for (int i = 0; i < FIB_SIZE; i++) {
        m68k_write_memory_8(fib68k + i, 0);
    }
    
    /* Fill the FileInfoBlock */
    m68k_write_memory_32(fib68k + FIB_fib_DiskKey, (uint32_t)st.st_ino);
    
    int32_t type = S_ISDIR(st.st_mode) ? ST_USERDIR : ST_FILE;
    m68k_write_memory_32(fib68k + FIB_fib_DirEntryType, type);
    m68k_write_memory_32(fib68k + FIB_fib_EntryType, type);
    
    /* Write filename (null-terminated C string, NOT BSTR) */
    int namelen = strlen(de->d_name);
    if (namelen > 107) namelen = 107;  /* Leave room for null terminator */
    for (int i = 0; i < namelen; i++) {
        m68k_write_memory_8(fib68k + FIB_fib_FileName + i, de->d_name[i]);
    }
    m68k_write_memory_8(fib68k + FIB_fib_FileName + namelen, 0);  /* Null terminator */
    
    m68k_write_memory_32(fib68k + FIB_fib_Protection, _unix_mode_to_amiga(st.st_mode));
    m68k_write_memory_32(fib68k + FIB_fib_Size, st.st_size);
    m68k_write_memory_32(fib68k + FIB_fib_NumBlocks, (st.st_size + 511) / 512);
    
    _unix_timespec_to_datestamp(st.st_mtime, st.st_mtim.tv_nsec, fib68k + FIB_fib_Date);
    
    /* Read file comment from xattr or sidecar */
    _read_file_comment(fullpath, fib68k);
    
    _write_owner_info(fib68k, _read_owner_info(fullpath, &st));
    
    DPRINTF(LOG_DEBUG, "lxa: _dos_exnext(): success, name=%s, type=%d\n", de->d_name, type);
    return 1;
}

/* InfoData offsets (from dos/dos.h) */
#define ID_id_NumSoftErrors   0   /* LONG */
#define ID_id_UnitNumber      4   /* LONG */
#define ID_id_DiskState       8   /* LONG */
#define ID_id_NumBlocks       12  /* LONG */
#define ID_id_NumBlocksUsed   16  /* LONG */
#define ID_id_BytesPerBlock   20  /* LONG */
#define ID_id_DiskType        24  /* LONG */
#define ID_id_VolumeNode      28  /* BPTR */
#define ID_id_InUse           32  /* LONG */
#define ID_SIZE               36

#define ID_VALIDATED        82   /* ID_VALIDATED */
#define ID_DOS_DISK    0x444F5300  /* 'DOS\0' */

/* Get volume info */
int _dos_info(uint32_t lock_id, uint32_t infodata68k)
{
    DPRINTF(LOG_DEBUG, "lxa: _dos_info(): lock_id=%d, infodata68k=0x%08x\n", lock_id, infodata68k);
    
    lock_entry_t *lock = _lock_get(lock_id);
    if (!lock) {
        DPRINTF(LOG_DEBUG, "lxa: _dos_info(): invalid lock_id\n");
        return 0;
    }
    
    struct statvfs vfs;
    if (statvfs(lock->linux_path, &vfs) != 0) {
        DPRINTF(LOG_DEBUG, "lxa: _dos_info(): statvfs failed: %s\n", strerror(errno));
        return 0;
    }
    
    /* Fill InfoData structure */
    m68k_write_memory_32(infodata68k + ID_id_NumSoftErrors, 0);
    m68k_write_memory_32(infodata68k + ID_id_UnitNumber, 0);
    m68k_write_memory_32(infodata68k + ID_id_DiskState, ID_VALIDATED);
    m68k_write_memory_32(infodata68k + ID_id_NumBlocks, vfs.f_blocks);
    m68k_write_memory_32(infodata68k + ID_id_NumBlocksUsed, vfs.f_blocks - vfs.f_bfree);
    m68k_write_memory_32(infodata68k + ID_id_BytesPerBlock, vfs.f_bsize);
    m68k_write_memory_32(infodata68k + ID_id_DiskType, ID_DOS_DISK);
    m68k_write_memory_32(infodata68k + ID_id_VolumeNode, 0);
    m68k_write_memory_32(infodata68k + ID_id_InUse, 0);
    
    DPRINTF(LOG_DEBUG, "lxa: _dos_info(): success, blocks=%lu, used=%lu\n", 
            vfs.f_blocks, vfs.f_blocks - vfs.f_bfree);
    return 1;
}

/* Check if two locks refer to the same object */
int _dos_samelock(uint32_t lock1_id, uint32_t lock2_id)
{
    DPRINTF(LOG_DEBUG, "lxa: _dos_samelock(): lock1=%d, lock2=%d\n", lock1_id, lock2_id);
    
    if (lock1_id == lock2_id) return 0; /* LOCK_SAME */
    
    lock_entry_t *lock1 = _lock_get(lock1_id);
    lock_entry_t *lock2 = _lock_get(lock2_id);
    
    if (!lock1 || !lock2) return -1; /* LOCK_DIFFERENT */
    
    struct stat st1, st2;
    if (stat(lock1->linux_path, &st1) != 0) return -1;
    if (stat(lock2->linux_path, &st2) != 0) return -1;
    
    if (st1.st_dev == st2.st_dev && st1.st_ino == st2.st_ino) {
        return 0; /* LOCK_SAME */
    }
    
    if (st1.st_dev == st2.st_dev) {
        return 1; /* LOCK_SAME_VOLUME */
    }
    
    return -1; /* LOCK_DIFFERENT */
}

/* Get parent directory of a lock */
uint32_t _dos_parentdir(uint32_t lock_id)
{
    DPRINTF(LOG_DEBUG, "lxa: _dos_parentdir(): lock_id=%d\n", lock_id);
    
    lock_entry_t *lock = _lock_get(lock_id);
    if (!lock) return 0;
    
    /* Find parent path (linux) */
    char parent_path[PATH_MAX];
    strncpy(parent_path, lock->linux_path, sizeof(parent_path) - 1);
    
    char *last_slash = strrchr(parent_path, '/');
    if (!last_slash || last_slash == parent_path) {
        /* Already at root */
        return 0;
    }
    *last_slash = '\0';
    
    /* Find parent path (amiga) */
    char parent_amiga[PATH_MAX];
    strncpy(parent_amiga, lock->amiga_path, sizeof(parent_amiga) - 1);
    
    /* Amiga paths use / or : as separators */
    char *last_sep = strrchr(parent_amiga, '/');
    char *colon = strchr(parent_amiga, ':');
    if (last_sep) {
        *last_sep = '\0';
    } else if (colon && colon[1] != '\0') {
        /* Path is like "SYS:dir" - parent is "SYS:" */
        colon[1] = '\0';
    }
    
    /* Create lock for parent */
    int new_lock_id = _lock_alloc();
    if (new_lock_id == 0) return 0;
    
    lock_entry_t *new_lock = &g_locks[new_lock_id];
    strncpy(new_lock->linux_path, parent_path, sizeof(new_lock->linux_path) - 1);
    strncpy(new_lock->amiga_path, parent_amiga, sizeof(new_lock->amiga_path) - 1);
    new_lock->is_dir = true;
    new_lock->dir = NULL;
    new_lock->access_mode = SHARED_LOCK;
    
    DPRINTF(LOG_DEBUG, "lxa: _dos_parentdir(): parent=%s, amiga=%s, new_lock_id=%d\n", parent_path, parent_amiga, new_lock_id);
    return new_lock_id;
}

/* Create a directory */
uint32_t _dos_createdir(uint32_t name68k)
{
    char *amiga_path = _mgetstr(name68k);
    char linux_path[PATH_MAX];
    
    DPRINTF(LOG_DEBUG, "lxa: _dos_createdir(): amiga_path=%s\n", amiga_path);
    
    if (!vfs_resolve_path(amiga_path, linux_path, sizeof(linux_path))) {
        _dos_path2linux(amiga_path, linux_path, sizeof(linux_path));
    }
    
    if (mkdir(linux_path, 0755) != 0) {
        DPRINTF(LOG_DEBUG, "lxa: _dos_createdir(): mkdir failed: %s\n", strerror(errno));
        return 0;
    }
    
    /* Return a lock on the new directory */
    int lock_id = _lock_alloc();
    if (lock_id == 0) return 0;
    
    lock_entry_t *lock = &g_locks[lock_id];
    strncpy(lock->linux_path, linux_path, sizeof(lock->linux_path) - 1);
    strncpy(lock->amiga_path, amiga_path, sizeof(lock->amiga_path) - 1);
    lock->is_dir = true;
    lock->dir = NULL;
    lock->access_mode = EXCLUSIVE_LOCK;
    
    DPRINTF(LOG_DEBUG, "lxa: _dos_createdir(): success, lock_id=%d\n", lock_id);
    return lock_id;
}

/* Delete a file or directory */
int _dos_deletefile(uint32_t name68k)
{
    char *amiga_path = _mgetstr(name68k);
    char linux_path[PATH_MAX];
    
    DPRINTF(LOG_DEBUG, "lxa: _dos_deletefile(): amiga_path=%s\n", amiga_path);
    
    if (!vfs_resolve_path(amiga_path, linux_path, sizeof(linux_path))) {
        _dos_path2linux(amiga_path, linux_path, sizeof(linux_path));
    }
    
    struct stat st;
    if (lstat(linux_path, &st) != 0) {
        DPRINTF(LOG_DEBUG, "lxa: _dos_deletefile(): stat failed: %s\n", strerror(errno));
        return 0;
    }
    
    int result;
    if (S_ISDIR(st.st_mode)) {
        result = rmdir(linux_path);
    } else {
        result = unlink(linux_path);
    }
    
    if (result != 0) {
        DPRINTF(LOG_DEBUG, "lxa: _dos_deletefile(): delete failed: %s\n", strerror(errno));
        return 0;
    }

    if (!S_ISDIR(st.st_mode)) {
        char sidecar[PATH_MAX + 32];
        if (snprintf(sidecar, sizeof(sidecar), "%s.amiga.readlink", linux_path) < (int)sizeof(sidecar)) {
            unlink(sidecar);
        }
    }

    DPRINTF(LOG_DEBUG, "lxa: _dos_deletefile(): success\n");
    return 1;
}

/* Rename a file or directory */
int _dos_rename(uint32_t old68k, uint32_t new68k)
{
    char *old_amiga = _mgetstr(old68k);
    char *new_amiga = _mgetstr(new68k);
    char old_linux[PATH_MAX], new_linux[PATH_MAX];
    
    DPRINTF(LOG_DEBUG, "lxa: _dos_rename(): old=%s, new=%s\n", old_amiga, new_amiga);
    
    if (!vfs_resolve_path(old_amiga, old_linux, sizeof(old_linux))) {
        _dos_path2linux(old_amiga, old_linux, sizeof(old_linux));
    }
    if (!vfs_resolve_path(new_amiga, new_linux, sizeof(new_linux))) {
        _dos_path2linux(new_amiga, new_linux, sizeof(new_linux));
    }
    
    if (rename(old_linux, new_linux) != 0) {
        DPRINTF(LOG_DEBUG, "lxa: _dos_rename(): rename failed: %s\n", strerror(errno));
        return 0;
    }
    
    DPRINTF(LOG_DEBUG, "lxa: _dos_rename(): success\n");
    return 1;
}

/* Get path name from lock */
int _dos_namefromlock(uint32_t lock_id, uint32_t buf68k, uint32_t buflen)
{
    DPRINTF(LOG_DEBUG, "lxa: _dos_namefromlock(): lock_id=%d\n", lock_id);
    
    lock_entry_t *lock = _lock_get(lock_id);
    if (!lock) return 0;
    
    /* Return the Amiga path stored in the lock */
    const char *path = lock->amiga_path;
    size_t len = strlen(path);
    if (len >= buflen) len = buflen - 1;
    
    DPRINTF(LOG_DEBUG, "lxa: _dos_namefromlock(): returning '%s'\n", path);
    
    for (size_t i = 0; i < len; i++) {
        m68k_write_memory_8(buf68k + i, path[i]);
    }
    m68k_write_memory_8(buf68k + len, '\0');
    
    return 1;
}

/*
 * Phase 4: Metadata operations
 */

/* Amiga protection bits from dos/dos.h (inverted logic - bit SET = protection denied) 
 * 
 * Layout:
 *   Bits 0-3:  Basic owner RWED (Read, Write, Execute, Delete)
 *   Bits 4-7:  Special flags (Archive, Pure, Script, Hold)
 *   Bits 8-11: Group RWED
 *   Bits 12-15: Other RWED
 */
#define FIBB_DELETE       0   /* Owner: delete */
#define FIBB_EXECUTE      1   /* Owner: execute */
#define FIBB_WRITE        2   /* Owner: write */
#define FIBB_READ         3   /* Owner: read */
#define FIBB_ARCHIVE      4   /* Archived bit */
#define FIBB_PURE         5   /* Pure bit (reentrant executable) */
#define FIBB_SCRIPT       6   /* Script bit */
#define FIBB_HOLD         7   /* Hold bit */
#define FIBB_GRP_DELETE   8   /* Group: delete */
#define FIBB_GRP_EXECUTE  9   /* Group: execute */
#define FIBB_GRP_WRITE    10  /* Group: write */
#define FIBB_GRP_READ     11  /* Group: read */
#define FIBB_OTR_DELETE   12  /* Other: delete */
#define FIBB_OTR_EXECUTE  13  /* Other: execute */
#define FIBB_OTR_WRITE    14  /* Other: write */
#define FIBB_OTR_READ     15  /* Other: read */

/* Set file protection bits (Amiga -> Linux mapping) */
int _dos_setprotection(uint32_t name68k, uint32_t protect)
{
    char *amiga_path = _mgetstr(name68k);
    char linux_path[PATH_MAX];
    
    DPRINTF(LOG_DEBUG, "lxa: _dos_setprotection(): amiga_path=%s, protect=0x%08x\n", amiga_path, protect);
    
    if (!vfs_resolve_path(amiga_path, linux_path, sizeof(linux_path))) {
        _dos_path2linux(amiga_path, linux_path, sizeof(linux_path));
    }
    
    /* Amiga protection bits are inverted (bit SET = permission denied)
     * Map to Unix permissions:
     * - Basic owner bits (0-3) map to user rwx
     * - Group bits (8-11) map to group rwx  
     * - Other bits (12-15) map to other rwx
     * - Delete bit controls write permission
     */
    mode_t mode = 0;
    
    /* Owner permissions (inverted Amiga bits - bit clear means permission granted) */
    if (!(protect & (1 << FIBB_READ)))    mode |= S_IRUSR;
    if (!(protect & (1 << FIBB_WRITE)))   mode |= S_IWUSR;
    if (!(protect & (1 << FIBB_EXECUTE))) mode |= S_IXUSR;
    
    /* Group permissions */
    if (!(protect & (1 << FIBB_GRP_READ)))    mode |= S_IRGRP;
    if (!(protect & (1 << FIBB_GRP_WRITE)))   mode |= S_IWGRP;
    if (!(protect & (1 << FIBB_GRP_EXECUTE))) mode |= S_IXGRP;
    
    /* Other permissions */
    if (!(protect & (1 << FIBB_OTR_READ)))    mode |= S_IROTH;
    if (!(protect & (1 << FIBB_OTR_WRITE)))   mode |= S_IWOTH;
    if (!(protect & (1 << FIBB_OTR_EXECUTE))) mode |= S_IXOTH;
    
    /* If no permissions set, default to rw-r--r-- */
    if (mode == 0) mode = 0644;
    
    if (chmod(linux_path, mode) != 0) {
        DPRINTF(LOG_DEBUG, "lxa: _dos_setprotection(): chmod failed: %s\n", strerror(errno));
        return 0;
    }
    
    DPRINTF(LOG_DEBUG, "lxa: _dos_setprotection(): success, mode=0%03o\n", mode);
    return 1;
}

/* Set file comment (stored as xattr or sidecar file) */
int _dos_setcomment(uint32_t name68k, uint32_t comment68k)
{
    char *amiga_path = _mgetstr(name68k);
    char *comment = _mgetstr(comment68k);
    char linux_path[PATH_MAX];
    
    DPRINTF(LOG_DEBUG, "lxa: _dos_setcomment(): amiga_path=%s, comment=%s\n", amiga_path, comment);
    
    if (!vfs_resolve_path(amiga_path, linux_path, sizeof(linux_path))) {
        _dos_path2linux(amiga_path, linux_path, sizeof(linux_path));
    }
    
    /* Check if file exists first */
    struct stat st;
    if (stat(linux_path, &st) != 0) {
        DPRINTF(LOG_DEBUG, "lxa: _dos_setcomment(): file not found: %s\n", linux_path);
        return 0;
    }
    
    /* Try to use extended attributes first (Linux native) */
    #ifdef HAVE_XATTR
    if (comment && strlen(comment) > 0) {
        if (setxattr(linux_path, "user.amiga.comment", comment, strlen(comment), 0) == 0) {
            DPRINTF(LOG_DEBUG, "lxa: _dos_setcomment(): set xattr success\n");
            return 1;
        }
    } else {
        /* Empty comment = remove xattr */
        removexattr(linux_path, "user.amiga.comment");
        return 1;
    }
    #endif
    
    /* Fallback: use sidecar file (.comment extension) */
    char sidecar_path[PATH_MAX + 10];
    snprintf(sidecar_path, sizeof(sidecar_path), "%s.comment", linux_path);
    
    if (comment && strlen(comment) > 0) {
        FILE *f = fopen(sidecar_path, "w");
        if (!f) {
            DPRINTF(LOG_DEBUG, "lxa: _dos_setcomment(): failed to create sidecar: %s\n", strerror(errno));
            return 0;
        }
        fprintf(f, "%s", comment);
        fclose(f);
        DPRINTF(LOG_DEBUG, "lxa: _dos_setcomment(): sidecar created\n");
    } else {
        /* Empty comment = delete sidecar */
        unlink(sidecar_path);
    }
    
    return 1;
}

int _dos_setowner(uint32_t name68k, uint32_t owner68k, uint32_t err68k)
{
    char *amiga_path = _mgetstr(name68k);
    char linux_path[PATH_MAX];
    struct stat st;
    char owner_buf[32];

    DPRINTF(LOG_DEBUG, "lxa: _dos_setowner(): amiga_path=%s, owner=0x%08x\n",
            amiga_path ? amiga_path : "NULL", owner68k);

    if (!amiga_path)
    {
        m68k_write_memory_32(err68k, ERROR_REQUIRED_ARG_MISSING);
        return 0;
    }

    if (!_resolve_amiga_path_host(amiga_path, linux_path, sizeof(linux_path)))
    {
        m68k_write_memory_32(err68k, ERROR_OBJECT_NOT_FOUND);
        return 0;
    }

    if (stat(linux_path, &st) != 0)
    {
        m68k_write_memory_32(err68k, errno2Amiga());
        return 0;
    }

#ifdef HAVE_XATTR
    snprintf(owner_buf, sizeof(owner_buf), "%08x", owner68k);
    if (setxattr(linux_path, "user.amiga.owner", owner_buf, strlen(owner_buf), 0) == 0)
    {
        m68k_write_memory_32(err68k, 0);
        return 1;
    }
#endif

    {
        char sidecar_path[PATH_MAX + 8];
        FILE *f;

        snprintf(sidecar_path, sizeof(sidecar_path), "%s.owner", linux_path);
        f = fopen(sidecar_path, "w");
        if (!f)
        {
            m68k_write_memory_32(err68k, errno2Amiga());
            return 0;
        }

        snprintf(owner_buf, sizeof(owner_buf), "%08x", owner68k);
        fprintf(f, "%s", owner_buf);
        fclose(f);
    }

    m68k_write_memory_32(err68k, 0);
    return 1;
}

int _dos_setfiledate(uint32_t name68k, uint32_t date68k, uint32_t err68k)
{
    char *amiga_path = _mgetstr(name68k);
    char linux_path[PATH_MAX];
    struct stat st;
    struct timespec times[2];
    time_t unix_time;

    DPRINTF(LOG_DEBUG, "lxa: _dos_setfiledate(): amiga_path=%s, date=0x%08x\n",
            amiga_path ? amiga_path : "NULL", date68k);

    if (!amiga_path || !date68k)
    {
        m68k_write_memory_32(err68k, ERROR_REQUIRED_ARG_MISSING);
        return 0;
    }

    if (!_resolve_amiga_path_host(amiga_path, linux_path, sizeof(linux_path)))
    {
        m68k_write_memory_32(err68k, ERROR_OBJECT_NOT_FOUND);
        return 0;
    }

    if (stat(linux_path, &st) != 0)
    {
        m68k_write_memory_32(err68k, errno2Amiga());
        return 0;
    }

    unix_time = _datestamp68k_to_unix_time(date68k);
    times[0].tv_sec = st.st_atime;
    times[0].tv_nsec = 0;
    times[1].tv_sec = unix_time;
    times[1].tv_nsec = (long)((m68k_read_memory_32(date68k + 8) % 50U) * 20000000U);

    if (utimensat(AT_FDCWD, linux_path, times, 0) != 0)
    {
        m68k_write_memory_32(err68k, errno2Amiga());
        return 0;
    }

    m68k_write_memory_32(err68k, 0);
    return 1;
}

int _dos_readlink(uint32_t path68k, uint32_t buffer68k, uint32_t size, uint32_t err68k)
{
    char *amiga_path = _mgetstr(path68k);
    char linux_path[PATH_MAX];
    char target[PATH_MAX];
    char sidecar[PATH_MAX + 32];
    ssize_t len;

    DPRINTF(LOG_DEBUG, "lxa: _dos_readlink(): amiga_path=%s, size=%u\n",
            amiga_path ? amiga_path : "NULL", size);

    if (!amiga_path || !buffer68k || size == 0)
    {
        m68k_write_memory_32(err68k, ERROR_REQUIRED_ARG_MISSING);
        return -1;
    }

    if (!_resolve_amiga_path_host(amiga_path, linux_path, sizeof(linux_path)))
    {
        const char *slash = strrchr(amiga_path, '/');
        if (slash)
        {
            char parent_amiga[PATH_MAX];
            char parent_linux[PATH_MAX];
            size_t parent_len = (size_t)(slash - amiga_path);
            if (parent_len >= sizeof(parent_amiga))
                parent_len = sizeof(parent_amiga) - 1;
            memcpy(parent_amiga, amiga_path, parent_len);
            parent_amiga[parent_len] = '\0';

            if (!_resolve_amiga_path_host(parent_amiga, parent_linux, sizeof(parent_linux)))
            {
                m68k_write_memory_32(err68k, ERROR_OBJECT_NOT_FOUND);
                return -1;
            }

            if (snprintf(linux_path, sizeof(linux_path), "%s/%s", parent_linux, slash + 1) >= (int)sizeof(linux_path))
            {
                m68k_write_memory_32(err68k, ERROR_BAD_STREAM_NAME);
                return -1;
            }
        }
        else
        {
            m68k_write_memory_32(err68k, ERROR_OBJECT_NOT_FOUND);
            return -1;
        }
    }

    len = readlink(linux_path, target, sizeof(target) - 1);
    if (len < 0)
    {
        m68k_write_memory_32(err68k, errno2Amiga());
        return -1;
    }

    target[len] = '\0';

    if (snprintf(sidecar, sizeof(sidecar), "%s.amiga.readlink", linux_path) < (int)sizeof(sidecar)) {
        FILE *f = fopen(sidecar, "r");
        if (f) {
            len = fread(target, 1, sizeof(target) - 1, f);
            fclose(f);
            while (len > 0 && (target[len - 1] == '\n' || target[len - 1] == '\r'))
                len--;
            target[len] = '\0';
        }
    }

    if ((size_t)len >= size)
    {
        m68k_write_memory_32(err68k, ERROR_BUFFER_OVERFLOW);
        return -2;
    }

    for (ssize_t i = 0; i <= len; i++)
        m68k_write_memory_8(buffer68k + (uint32_t)i, (uint8_t)target[i]);

    m68k_write_memory_32(err68k, 0);
    return (int)len;
}

int _dos_makelink(uint32_t name68k, uint32_t dest_param, int32_t soft, uint32_t err68k)
{
    char *name = _mgetstr(name68k);
    char name_linux[PATH_MAX];
    int result;

    DPRINTF(LOG_DEBUG, "lxa: _dos_makelink(): name=%s, dest=0x%08x, soft=%d\n",
            name ? name : "NULL", dest_param, soft);

    if (!name)
    {
        m68k_write_memory_32(err68k, ERROR_REQUIRED_ARG_MISSING);
        return 0;
    }

    if (!_resolve_amiga_path_host(name, name_linux, sizeof(name_linux)))
    {
        const char *slash = strrchr(name, '/');
        if (slash)
        {
            char parent_amiga[PATH_MAX];
            char parent_linux[PATH_MAX];
            size_t parent_len = (size_t)(slash - name);
            if (parent_len >= sizeof(parent_amiga))
                parent_len = sizeof(parent_amiga) - 1;
            memcpy(parent_amiga, name, parent_len);
            parent_amiga[parent_len] = '\0';

            if (!_resolve_amiga_path_host(parent_amiga, parent_linux, sizeof(parent_linux)))
            {
                m68k_write_memory_32(err68k, ERROR_OBJECT_NOT_FOUND);
                return 0;
            }

            if (snprintf(name_linux, sizeof(name_linux), "%s/%s", parent_linux, slash + 1) >= (int)sizeof(name_linux))
            {
                m68k_write_memory_32(err68k, ERROR_BAD_STREAM_NAME);
                return 0;
            }
        }
        else
        {
            m68k_write_memory_32(err68k, ERROR_OBJECT_NOT_FOUND);
            return 0;
        }
    }

    if (soft == LINK_SOFT)
    {
        char *dest_name = _mgetstr(dest_param);
        char dest_linux[PATH_MAX];
        char sidecar[PATH_MAX + 32];

        if (!dest_name)
        {
            m68k_write_memory_32(err68k, ERROR_REQUIRED_ARG_MISSING);
            return 0;
        }

        if (!_resolve_amiga_path_host(dest_name, dest_linux, sizeof(dest_linux)))
        {
            strncpy(dest_linux, dest_name, sizeof(dest_linux) - 1);
            dest_linux[sizeof(dest_linux) - 1] = '\0';
        }

        result = symlink(dest_linux, name_linux);
        if (result == 0)
        {
            if (snprintf(sidecar, sizeof(sidecar), "%s.amiga.readlink", name_linux) < (int)sizeof(sidecar)) {
                FILE *f = fopen(sidecar, "w");
                if (f) {
                    fwrite(dest_name, 1, strlen(dest_name), f);
                    fclose(f);
                }
            }
        }
    }
    else
    {
        uint32_t lock_id = dest_param;
        lock_entry_t *lock = _lock_get(lock_id);

        if (!lock)
        {
            m68k_write_memory_32(err68k, ERROR_INVALID_LOCK);
            return 0;
        }

        result = link(lock->linux_path, name_linux);
    }

    if (result != 0)
    {
        m68k_write_memory_32(err68k, errno2Amiga());
        return 0;
    }

    m68k_write_memory_32(err68k, 0);
    return 1;
}

/*
 * Phase 7: Assignment System
 *
 * These functions handle assign operations. Assigns are managed by the VFS
 * layer, and these functions bridge the m68k emucalls to VFS.
 */

/* Add or replace an assign */
int _dos_assign_add(uint32_t name68k, uint32_t path68k, uint32_t type)
{
    char *name = _mgetstr(name68k);
    char *path = _mgetstr(path68k);
    char linux_path[PATH_MAX];
    
    DPRINTF(LOG_DEBUG, "lxa: _dos_assign_add(): name=%s, path=%s, type=%d\n", 
            name ? name : "NULL", path ? path : "NULL", type);
    
    if (!name || strlen(name) == 0) {
        DPRINTF(LOG_DEBUG, "lxa: _dos_assign_add(): invalid name\n");
        return 0;
    }
    
    /* Determine if we received a Linux path or an Amiga path.
     * Linux paths start with '/' while Amiga paths contain ':'
     * Note: NameFromLock currently returns Linux paths, while
     * AssignLate/AssignPath pass Amiga paths directly.
     */
    if (path && strlen(path) > 0) {
        if (path[0] == '/') {
            /* Already a Linux path - use it directly */
            strncpy(linux_path, path, sizeof(linux_path));
            linux_path[sizeof(linux_path) - 1] = '\0';
            DPRINTF(LOG_DEBUG, "lxa: _dos_assign_add(): using Linux path directly: %s\n", linux_path);
        } else {
            /* Amiga path - resolve to Linux */
            if (!vfs_resolve_path(path, linux_path, sizeof(linux_path))) {
                _dos_path2linux(path, linux_path, sizeof(linux_path));
            }
            DPRINTF(LOG_DEBUG, "lxa: _dos_assign_add(): resolved Amiga path to: %s\n", linux_path);
        }
        
        /* Verify the path exists */
        struct stat st;
        if (stat(linux_path, &st) != 0) {
            DPRINTF(LOG_DEBUG, "lxa: _dos_assign_add(): path not found: %s (errno=%d)\n", linux_path, errno);
            return 0;
        }
        
        /* For directory assigns, ensure it's actually a directory */
        if (!S_ISDIR(st.st_mode)) {
            DPRINTF(LOG_DEBUG, "lxa: _dos_assign_add(): not a directory: %s\n", linux_path);
            return 0;
        }
    } else {
        DPRINTF(LOG_DEBUG, "lxa: _dos_assign_add(): empty path\n");
        linux_path[0] = '\0';
    }
    
    if (type == 3) {
        if (vfs_assign_add_path(name, linux_path)) {
            DPRINTF(LOG_DEBUG, "lxa: _dos_assign_add(): add-path success\n");
            return 1;
        }

        DPRINTF(LOG_DEBUG, "lxa: _dos_assign_add(): add-path failed\n");
        return 0;
    }

    /* type: 0 = ASSIGN_LOCK, 1 = ASSIGN_LATE, 2 = ASSIGN_PATH */
    assign_type_t atype = (type <= 2) ? (assign_type_t)type : ASSIGN_LOCK;

    DPRINTF(LOG_DEBUG, "lxa: _dos_assign_add(): calling vfs_assign_add(name=%s, linux_path=%s, atype=%d)\n",
            name, linux_path, atype);

    if (vfs_assign_add(name, linux_path, atype)) {
        DPRINTF(LOG_DEBUG, "lxa: _dos_assign_add(): success\n");
        return 1;
    }
    
    DPRINTF(LOG_DEBUG, "lxa: _dos_assign_add(): vfs_assign_add failed\n");
    return 0;
}

/* Remove an assign */
int _dos_assign_remove(uint32_t name68k)
{
    char *name = _mgetstr(name68k);
    
    DPRINTF(LOG_DEBUG, "lxa: _dos_assign_remove(): name=%s\n", name ? name : "NULL");
    
    if (!name || strlen(name) == 0) {
        return 0;
    }
    
    if (vfs_assign_remove(name)) {
        DPRINTF(LOG_DEBUG, "lxa: _dos_assign_remove(): success\n");
        return 1;
    }
    
    return 0;
}

/* List assigns - returns count and writes packed strings to buffer */
int _dos_assign_list(uint32_t buf68k, uint32_t buflen)
{
    DPRINTF(LOG_DEBUG, "lxa: _dos_assign_list(): buf=0x%08x, buflen=%d\n", buf68k, buflen);
    
    /* Get list of assigns from VFS */
    const char *names[64];
    const char *paths[64];
    char amiga_paths[64][PATH_MAX];
    int count = vfs_assign_list(names, paths, 64);
    
    DPRINTF(LOG_DEBUG, "lxa: _dos_assign_list(): got %d assigns\n", count);
    
    if (buf68k == 0 || buflen == 0) {
        /* Just return the count */
        return count;
    }
    
    /* Write assigns to buffer as: name\0path\0name\0path\0...\0\0 */
    uint32_t offset = 0;
    int written = 0;
    
    for (int i = 0; i < count && offset < buflen - 2; i++) {
        size_t name_len = strlen(names[i]);
        const char *path = paths[i];
        size_t path_len;

        if (path && path[0] != '\0') {
            if (_linux_path_to_amiga(path, amiga_paths[i], sizeof(amiga_paths[i]))) {
                path = amiga_paths[i];
            }
        }

        path_len = path ? strlen(path) : 0;
        
        if (offset + name_len + 1 + path_len + 1 >= buflen - 1) {
            break; /* No more space */
        }
        
        /* Write name */
        for (size_t j = 0; j < name_len; j++) {
            m68k_write_memory_8(buf68k + offset++, names[i][j]);
        }
        m68k_write_memory_8(buf68k + offset++, '\0');
        
        /* Write path */
        if (path) {
            for (size_t j = 0; j < path_len; j++) {
                m68k_write_memory_8(buf68k + offset++, path[j]);
            }
        }
        m68k_write_memory_8(buf68k + offset++, '\0');
        
        written++;
    }
    
    /* Terminate with extra NUL */
    m68k_write_memory_8(buf68k + offset, '\0');
    
    return written;
}

static uint32_t _dos_make_lock_for_path(const char *linux_path, const char *amiga_path)
{
    struct stat st;
    int lock_id;
    lock_entry_t *lock;

    if (!linux_path || stat(linux_path, &st) != 0) {
        return 0;
    }

    lock_id = _lock_alloc();
    if (lock_id == 0) {
        return 0;
    }

    lock = &g_locks[lock_id];
    strncpy(lock->linux_path, linux_path, sizeof(lock->linux_path) - 1);
    lock->linux_path[sizeof(lock->linux_path) - 1] = '\0';

    if (amiga_path && amiga_path[0] != '\0') {
        strncpy(lock->amiga_path, amiga_path, sizeof(lock->amiga_path) - 1);
        lock->amiga_path[sizeof(lock->amiga_path) - 1] = '\0';
    } else {
        if (!_linux_path_to_amiga(linux_path, lock->amiga_path, sizeof(lock->amiga_path))) {
            strncpy(lock->amiga_path, linux_path, sizeof(lock->amiga_path) - 1);
            lock->amiga_path[sizeof(lock->amiga_path) - 1] = '\0';
        }
    }

    lock->is_dir = S_ISDIR(st.st_mode);
    lock->dir = NULL;
    return (uint32_t)lock_id;
}

int _dos_assign_remove_path(uint32_t name68k, uint32_t path68k)
{
    char *name = _mgetstr(name68k);
    char *path = _mgetstr(path68k);
    char linux_path[PATH_MAX];

    if (!name || !path || name[0] == '\0' || path[0] == '\0') {
        return 0;
    }

    if (path[0] == '/') {
        strncpy(linux_path, path, sizeof(linux_path) - 1);
        linux_path[sizeof(linux_path) - 1] = '\0';
    } else {
        _dos_path2linux(path, linux_path, sizeof(linux_path));
    }

    return vfs_assign_remove_path(name, linux_path) ? 1 : 0;
}

int _dos_getdevproc(uint32_t name68k, uint32_t dp68k, uint32_t err68k)
{
    char *name = _mgetstr(name68k);
    char volume[64];
    const char *colon;
    uint32_t index = 0;
    uint32_t lock_id = 0;
    char amiga_root[PATH_MAX];
    char linux_path[PATH_MAX];
    uint32_t flags = 0;

    if (err68k) {
        m68k_write_memory_32(err68k, ERROR_OBJECT_NOT_FOUND);
    }

    if (!name || !dp68k) {
        return 0;
    }

    colon = strchr(name, ':');
    if (!colon) {
        strncpy(volume, name, sizeof(volume) - 1);
        volume[sizeof(volume) - 1] = '\0';
    } else {
        size_t len = (size_t)(colon - name);
        if (len >= sizeof(volume)) {
            len = sizeof(volume) - 1;
        }
        memcpy(volume, name, len);
        volume[len] = '\0';
    }

    if (m68k_read_memory_32(dp68k + DVP_DVP_DEVNODE) != 0) {
        index = m68k_read_memory_32(dp68k + DVP_DVP_DEVNODE);
    }

    if (vfs_assign_exists(volume)) {
        const char *paths[64];
        int count = vfs_assign_get_paths(volume, paths, 64);

        if ((int)index >= count) {
            if (err68k) {
                m68k_write_memory_32(err68k, ERROR_NO_MORE_ENTRIES);
            }
            return 0;
        }

        if (_linux_path_to_amiga(paths[index], amiga_root, sizeof(amiga_root)) == 0) {
            snprintf(amiga_root, sizeof(amiga_root), "%s:", volume);
        }

        lock_id = _dos_make_lock_for_path(paths[index], amiga_root);
        flags |= DVPF_ASSIGN;
        flags |= DVPF_UNLOCK;
        m68k_write_memory_32(dp68k + DVP_DVP_DEVNODE, index + 1);
    } else {
        if (volume[0] == '\0') {
            if (err68k) {
                m68k_write_memory_32(err68k, ERROR_OBJECT_NOT_FOUND);
            }
            return 0;
        }

        snprintf(amiga_root, sizeof(amiga_root), "%s:", volume);
        _dos_path2linux(amiga_root, linux_path, sizeof(linux_path));
        lock_id = _dos_make_lock_for_path(linux_path, amiga_root);
        m68k_write_memory_32(dp68k + DVP_DVP_DEVNODE, 0);
    }

    if (!lock_id) {
        if (err68k) {
            m68k_write_memory_32(err68k, ERROR_OBJECT_NOT_FOUND);
        }
        return 0;
    }

    m68k_write_memory_32(dp68k + DVP_DVP_PORT, 0);
    m68k_write_memory_32(dp68k + DVP_DVP_LOCK, lock_id);
    m68k_write_memory_32(dp68k + DVP_DVP_FLAGS, flags);

    if (err68k) {
        m68k_write_memory_32(err68k, 0);
    }

    return (int)dp68k;
}

static void _notify_snapshot(notify_entry_t *entry)
{
    struct stat st;

    if (stat(entry->linux_path, &st) == 0) {
        entry->existed = true;
        entry->mtime = st.st_mtime;
        entry->size = st.st_size;
        entry->ino = st.st_ino;
        entry->dev = st.st_dev;
    } else {
        entry->existed = false;
        entry->mtime = 0;
        entry->size = 0;
        entry->ino = 0;
        entry->dev = 0;
    }
}

static bool _notify_changed(notify_entry_t *entry)
{
    struct stat st;
    bool exists_now = (stat(entry->linux_path, &st) == 0);

    if (exists_now != entry->existed) {
        _notify_snapshot(entry);
        return true;
    }

    if (!exists_now) {
        return false;
    }

    if (st.st_mtime != entry->mtime ||
        st.st_size != entry->size ||
        st.st_ino != entry->ino ||
        st.st_dev != entry->dev) {
        _notify_snapshot(entry);
        return true;
    }

    return false;
}

int _dos_notify_start(uint32_t notify68k, uint32_t fullname68k, uint32_t flags)
{
    char *full_name = _mgetstr(fullname68k);
    char linux_path[PATH_MAX];

    if (!notify68k || !full_name || full_name[0] == '\0') {
        return 0;
    }

    _dos_path2linux(full_name, linux_path, sizeof(linux_path));

    for (int i = 0; i < MAX_NOTIFY_REQUESTS; i++) {
        if (g_notify_requests[i].in_use && g_notify_requests[i].notify68k == notify68k) {
            g_notify_requests[i].pending = (flags & NRF_NOTIFY_INITIAL) != 0;
            strncpy(g_notify_requests[i].linux_path, linux_path, sizeof(g_notify_requests[i].linux_path) - 1);
            g_notify_requests[i].linux_path[sizeof(g_notify_requests[i].linux_path) - 1] = '\0';
            _notify_snapshot(&g_notify_requests[i]);
            return 1;
        }
    }

    for (int i = 0; i < MAX_NOTIFY_REQUESTS; i++) {
        if (!g_notify_requests[i].in_use) {
            g_notify_requests[i].in_use = true;
            g_notify_requests[i].notify68k = notify68k;
            g_notify_requests[i].pending = (flags & NRF_NOTIFY_INITIAL) != 0;
            strncpy(g_notify_requests[i].linux_path, linux_path, sizeof(g_notify_requests[i].linux_path) - 1);
            g_notify_requests[i].linux_path[sizeof(g_notify_requests[i].linux_path) - 1] = '\0';
            _notify_snapshot(&g_notify_requests[i]);
            return 1;
        }
    }

    return 0;
}

void _dos_notify_end(uint32_t notify68k)
{
    for (int i = 0; i < MAX_NOTIFY_REQUESTS; i++) {
        if (g_notify_requests[i].in_use && g_notify_requests[i].notify68k == notify68k) {
            memset(&g_notify_requests[i], 0, sizeof(g_notify_requests[i]));
            break;
        }
    }
}

uint32_t _dos_notify_poll(void)
{
    for (int i = 0; i < MAX_NOTIFY_REQUESTS; i++) {
        if (!g_notify_requests[i].in_use) {
            continue;
        }

        if (!g_notify_requests[i].pending && !_notify_changed(&g_notify_requests[i])) {
            continue;
        }

        g_notify_requests[i].pending = false;
        return g_notify_requests[i].notify68k;
    }

    return 0;
}

/*
 * Phase 10: File Handle Utilities
 */

/* Get Linux path from fd using /proc/self/fd symlink */
static int _get_path_from_fd(int fd, char *buf, size_t bufsize)
{
    char proc_path[64];
    snprintf(proc_path, sizeof(proc_path), "/proc/self/fd/%d", fd);
    
    ssize_t len = readlink(proc_path, buf, bufsize - 1);
    if (len < 0) {
        DPRINTF(LOG_DEBUG, "lxa: _get_path_from_fd(): readlink failed for fd=%d: %s\n", fd, strerror(errno));
        return 0;
    }
    buf[len] = '\0';
    return 1;
}

/* Convert Linux path back to Amiga path (best effort) */
static int _linux_path_to_amiga(const char *linux_path, char *amiga_buf, size_t bufsize)
{
    if (vfs_path_to_amiga(linux_path, amiga_buf, bufsize)) {
        return 1;
    }
    
    /* No matching assign found - use the Linux path as-is */
    strncpy(amiga_buf, linux_path, bufsize - 1);
    amiga_buf[bufsize - 1] = '\0';
    return 1;
}

/* DupLockFromFH - Get a lock from an open file handle */
uint32_t _dos_duplockfromfh(uint32_t fh68k)
{
    DPRINTF(LOG_DEBUG, "lxa: _dos_duplockfromfh(): fh=0x%08x\n", fh68k);
    
    int fd = m68k_read_memory_32(fh68k + 36);   /* fh_Args */
    int kind = m68k_read_memory_32(fh68k + 32); /* fh_Func3 */
    
    DPRINTF(LOG_DEBUG, "lxa: _dos_duplockfromfh(): fd=%d, kind=%d\n", fd, kind);
    
    if (kind == FILE_KIND_CONSOLE) {
        /* Console doesn't have a meaningful lock */
        return 0;
    }
    
    /* Get the path from the fd */
    char linux_path[PATH_MAX];
    if (!_get_path_from_fd(fd, linux_path, sizeof(linux_path))) {
        return 0;
    }
    
    DPRINTF(LOG_DEBUG, "lxa: _dos_duplockfromfh(): linux_path='%s'\n", linux_path);
    
    /* Get the directory part of the path */
    char *last_slash = strrchr(linux_path, '/');
    if (last_slash && last_slash != linux_path) {
        *last_slash = '\0';  /* Truncate to directory */
    }
    
    /* Check if the path exists */
    struct stat st;
    if (stat(linux_path, &st) != 0) {
        DPRINTF(LOG_DEBUG, "lxa: _dos_duplockfromfh(): stat failed: %s\n", strerror(errno));
        return 0;
    }
    
    /* Allocate a lock entry */
    int lock_id = _lock_alloc();
    if (lock_id == 0) {
        return 0;
    }
    
    lock_entry_t *lock = &g_locks[lock_id];
    strncpy(lock->linux_path, linux_path, sizeof(lock->linux_path) - 1);
    
    /* Try to build an Amiga path */
    char amiga_path[PATH_MAX];
    _linux_path_to_amiga(linux_path, amiga_path, sizeof(amiga_path));
    strncpy(lock->amiga_path, amiga_path, sizeof(lock->amiga_path) - 1);

    lock->is_dir = S_ISDIR(st.st_mode);
    lock->dir = NULL;
    lock->access_mode = SHARED_LOCK;
    
    DPRINTF(LOG_DEBUG, "lxa: _dos_duplockfromfh(): returning lock_id=%d\n", lock_id);
    return lock_id;
}

/* ExamineFH - Examine an open file handle */
int _dos_examinefh(uint32_t fh68k, uint32_t fib68k)
{
    DPRINTF(LOG_DEBUG, "lxa: _dos_examinefh(): fh=0x%08x, fib=0x%08x\n", fh68k, fib68k);
    
    int fd = m68k_read_memory_32(fh68k + 36);   /* fh_Args */
    
    DPRINTF(LOG_DEBUG, "lxa: _dos_examinefh(): fd=%d\n", fd);
    
    /* Get file info via fstat */
    struct stat st;
    if (fstat(fd, &st) != 0) {
        DPRINTF(LOG_DEBUG, "lxa: _dos_examinefh(): fstat failed: %s\n", strerror(errno));
        return 0;
    }
    
    /* Clear the FIB first */
    for (int i = 0; i < FIB_SIZE; i++) {
        m68k_write_memory_8(fib68k + i, 0);
    }
    
    /* Get the filename from the fd */
    char linux_path[PATH_MAX];
    const char *filename = "";
    if (_get_path_from_fd(fd, linux_path, sizeof(linux_path))) {
        const char *last_slash = strrchr(linux_path, '/');
        filename = last_slash ? last_slash + 1 : linux_path;
    }
    
    /* Fill the FileInfoBlock */
    m68k_write_memory_32(fib68k + FIB_fib_DiskKey, (uint32_t)st.st_ino);
    
    int32_t type = S_ISDIR(st.st_mode) ? ST_USERDIR : ST_FILE;
    m68k_write_memory_32(fib68k + FIB_fib_DirEntryType, type);
    m68k_write_memory_32(fib68k + FIB_fib_EntryType, type);
    
    /* Write filename */
    int namelen = strlen(filename);
    if (namelen > 107) namelen = 107;
    for (int i = 0; i < namelen; i++) {
        m68k_write_memory_8(fib68k + FIB_fib_FileName + i, filename[i]);
    }
    m68k_write_memory_8(fib68k + FIB_fib_FileName + namelen, 0);
    
    m68k_write_memory_32(fib68k + FIB_fib_Protection, _unix_mode_to_amiga(st.st_mode));
    m68k_write_memory_32(fib68k + FIB_fib_Size, st.st_size);
    m68k_write_memory_32(fib68k + FIB_fib_NumBlocks, (st.st_size + 511) / 512);
    
    _unix_timespec_to_datestamp(st.st_mtime, st.st_mtim.tv_nsec, fib68k + FIB_fib_Date);
    
    /* Read file comment from xattr or sidecar */
    if (linux_path[0]) {
        _read_file_comment(linux_path, fib68k);
    }
    
    _write_owner_info(fib68k, _read_owner_info(linux_path, &st));
    
    DPRINTF(LOG_DEBUG, "lxa: _dos_examinefh(): success, type=%d, size=%ld\n", type, (long)st.st_size);
    return 1;
}

/* NameFromFH - Get the full path name from a file handle */
int _dos_namefromfh(uint32_t fh68k, uint32_t buf68k, uint32_t len)
{
    DPRINTF(LOG_DEBUG, "lxa: _dos_namefromfh(): fh=0x%08x, buf=0x%08x, len=%d\n", fh68k, buf68k, len);
    
    int fd = m68k_read_memory_32(fh68k + 36);   /* fh_Args */
    int kind = m68k_read_memory_32(fh68k + 32); /* fh_Func3 */
    
    DPRINTF(LOG_DEBUG, "lxa: _dos_namefromfh(): fd=%d, kind=%d\n", fd, kind);
    
    if (kind == FILE_KIND_CONSOLE) {
        /* Console doesn't have a meaningful path */
        const char *console = "CONSOLE:";
        int clen = strlen(console);
        if ((uint32_t)clen >= len) {
            return 0;
        }
        for (int i = 0; i <= clen; i++) {
            m68k_write_memory_8(buf68k + i, console[i]);
        }
        return 1;
    }
    
    /* Get the Linux path from the fd */
    char linux_path[PATH_MAX];
    if (!_get_path_from_fd(fd, linux_path, sizeof(linux_path))) {
        return 0;
    }
    
    /* Convert to Amiga path */
    char amiga_path[PATH_MAX];
    if (!_linux_path_to_amiga(linux_path, amiga_path, sizeof(amiga_path))) {
        return 0;
    }
    
    /* Write to m68k buffer */
    size_t path_len = strlen(amiga_path);
    if (path_len >= len) {
        return 0;  /* Buffer too small */
    }
    
    for (size_t i = 0; i <= path_len; i++) {
        m68k_write_memory_8(buf68k + i, amiga_path[i]);
    }
    
    DPRINTF(LOG_DEBUG, "lxa: _dos_namefromfh(): returning '%s'\n", amiga_path);
    return 1;
}

/* ParentOfFH - Get a lock on the parent directory of an open file */
uint32_t _dos_parentoffh(uint32_t fh68k)
{
    DPRINTF(LOG_DEBUG, "lxa: _dos_parentoffh(): fh=0x%08x\n", fh68k);
    
    int fd = m68k_read_memory_32(fh68k + 36);   /* fh_Args */
    int kind = m68k_read_memory_32(fh68k + 32); /* fh_Func3 */
    
    if (kind == FILE_KIND_CONSOLE) {
        return 0;
    }
    
    /* Get the Linux path from the fd */
    char linux_path[PATH_MAX];
    if (!_get_path_from_fd(fd, linux_path, sizeof(linux_path))) {
        return 0;
    }
    
    /* Get the parent directory */
    char *last_slash = strrchr(linux_path, '/');
    if (!last_slash) {
        return 0;
    }
    
    if (last_slash == linux_path) {
        /* Parent is root */
        linux_path[1] = '\0';
    } else {
        *last_slash = '\0';
    }
    
    /* Check if parent exists */
    struct stat st;
    if (stat(linux_path, &st) != 0) {
        return 0;
    }
    
    /* Allocate a lock for the parent */
    int lock_id = _lock_alloc();
    if (lock_id == 0) {
        return 0;
    }
    
    lock_entry_t *lock = &g_locks[lock_id];
    strncpy(lock->linux_path, linux_path, sizeof(lock->linux_path) - 1);
    
    char amiga_path[PATH_MAX];
    _linux_path_to_amiga(linux_path, amiga_path, sizeof(amiga_path));
    strncpy(lock->amiga_path, amiga_path, sizeof(lock->amiga_path) - 1);

    lock->is_dir = TRUE;
    lock->dir = NULL;
    lock->access_mode = SHARED_LOCK;
    
    DPRINTF(LOG_DEBUG, "lxa: _dos_parentoffh(): returning lock_id=%d\n", lock_id);
    return lock_id;
}

/* OpenFromLock - Convert a lock to a file handle (lock is consumed) */
int _dos_openfromlock(uint32_t lock_id, uint32_t fh68k)
{
    DPRINTF(LOG_DEBUG, "lxa: _dos_openfromlock(): lock_id=%d, fh=0x%08x\n", lock_id, fh68k);
    
    lock_entry_t *lock = _lock_get(lock_id);
    if (!lock) {
        DPRINTF(LOG_DEBUG, "lxa: _dos_openfromlock(): invalid lock_id\n");
        return 0;
    }
    
    /* Can only open files, not directories */
    if (lock->is_dir) {
        DPRINTF(LOG_DEBUG, "lxa: _dos_openfromlock(): cannot open directory\n");
        return 0;
    }
    
    /* Open the file for reading */
    int fd = open(lock->linux_path, O_RDONLY);
    if (fd < 0) {
        DPRINTF(LOG_DEBUG, "lxa: _dos_openfromlock(): open failed: %s\n", strerror(errno));
        m68k_write_memory_32(fh68k + 40, errno2Amiga());  /* fh_Arg2 */
        return 0;
    }
    
    /* Set up the file handle */
    m68k_write_memory_32(fh68k + 36, fd);                /* fh_Args */
    m68k_write_memory_32(fh68k + 32, FILE_KIND_REGULAR); /* fh_Func3 */
    
    /* Free the lock - it's consumed */
    _lock_free(lock_id);
    
    DPRINTF(LOG_DEBUG, "lxa: _dos_openfromlock(): success, fd=%d\n", fd);
    return 1;
}

/* WaitForChar - Check if input is available on a file handle */
int _dos_waitforchar(uint32_t fh68k, uint32_t timeout_us)
{
    DPRINTF(LOG_DEBUG, "lxa: _dos_waitforchar(): fh=0x%08x, timeout=%u us\n", fh68k, timeout_us);
    
    int fd = m68k_read_memory_32(fh68k + 36);   /* fh_Args */
    int kind = m68k_read_memory_32(fh68k + 32); /* fh_Func3 */

    if (kind == FILE_KIND_CONSOLE && !lxa_host_console_input_empty())
        return 1;
    
    /* Use select() to check for available input */
    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(fd, &readfds);
    
    struct timeval tv;
    tv.tv_sec = timeout_us / 1000000;
    tv.tv_usec = timeout_us % 1000000;
    
    int result = select(fd + 1, &readfds, NULL, NULL, &tv);
    
    DPRINTF(LOG_DEBUG, "lxa: _dos_waitforchar(): select returned %d\n", result);
    
    return (result > 0) ? 1 : 0;
}
