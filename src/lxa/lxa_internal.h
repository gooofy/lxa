/*
 * lxa_internal.h - Internal shared declarations for lxa modules.
 *
 * This header is included by lxa.c, lxa_custom.c, lxa_memory.c,
 * lxa_dos_host.c, and lxa_dispatch.c.  It must NOT be included by
 * external consumers (use lxa_api.h for that).
 *
 * Phase 125: lxa.c decomposition.
 */

#ifndef LXA_INTERNAL_H
#define LXA_INTERNAL_H

#include <stdio.h>
#include <inttypes.h>
#include <stdbool.h>
#include <assert.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <regex.h>
#include <dirent.h>
#include <signal.h>
#include <math.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <time.h>
#include <stdint.h>
#include <linux/limits.h>
#include <sys/xattr.h>
#include <readline/readline.h>
#include <readline/history.h>

#define HAVE_XATTR 1

#ifdef SDL2_FOUND
#include <SDL.h>
#endif

#include "m68k.h"
#include "emucalls.h"
#include "util.h"
#include "vfs.h"
#include "config.h"
#include "display.h"
#include "lxa_copper.h"

/* =========================================================
 * Memory map #defines
 * ========================================================= */

#define RAM_START   0x000000
#define RAM_SIZE    10 * 1024 * 1024
#define RAM_END     RAM_START + RAM_SIZE - 1

#define DEFAULT_ROM_PATH "../rom/lxa.rom"

#define ROM_SIZE    512 * 1024
#define ROM_START   0xf80000
#define ROM_END     ROM_START + ROM_SIZE - 1

#define CUSTOM_START 0xdff000
#define CUSTOM_END   0xdfffff

/* Extended ROM area (A3000/A4000 extended Kickstart) - return 0 for reads */
#define EXTROM_START 0xf00000
#define EXTROM_END   0xf7ffff

/* Zorro-II expansion bus area - return 0 for reads */
#define ZORRO2_START 0x200000
#define ZORRO2_END   0x9fffff
#define ZORRO2_AUTOCONFIG_START 0xe80000
#define ZORRO2_AUTOCONFIG_END   0xefffff

/* CIA memory ranges */
#define CIA_START 0xbfd000
#define CIA_END   0xbfffff

/* Slow RAM / Ranger RAM area */
#define SLOWRAM_START    0xc00000
#define SLOWRAM_END      0xd7ffff
#define RANGER_START     0xe00000
#define RANGER_END       0xe7ffff

/* =========================================================
 * Custom chip register #defines
 * ========================================================= */

#define CUSTOM_REG_INTENA   0x09a
#define CUSTOM_REG_INTREQ   0x09c
#define CUSTOM_REG_DMACON   0x096
#define CUSTOM_REG_DMACONR  0x002
#define CUSTOM_REG_VPOSR    0x004
#define CUSTOM_REG_VHPOSR   0x006

#define CUSTOM_REG_BLTCON0  0x040
#define CUSTOM_REG_BLTCON1  0x042
#define CUSTOM_REG_BLTAFWM  0x044
#define CUSTOM_REG_BLTALWM  0x046
#define CUSTOM_REG_BLTCPTH  0x048
#define CUSTOM_REG_BLTCPTL  0x04a
#define CUSTOM_REG_BLTBPTH  0x04c
#define CUSTOM_REG_BLTBPTL  0x04e
#define CUSTOM_REG_BLTAPTH  0x050
#define CUSTOM_REG_BLTAPTL  0x052
#define CUSTOM_REG_BLTDPTH  0x054
#define CUSTOM_REG_BLTDPTL  0x056
#define CUSTOM_REG_BLTSIZE  0x058
#define CUSTOM_REG_BLTCON0L 0x05b
#define CUSTOM_REG_BLTSIZV  0x05c
#define CUSTOM_REG_BLTSIZH  0x05e
#define CUSTOM_REG_BLTCMOD  0x060
#define CUSTOM_REG_BLTBMOD  0x062
#define CUSTOM_REG_BLTAMOD  0x064
#define CUSTOM_REG_BLTDMOD  0x066
#define CUSTOM_REG_BLTCDAT  0x070
#define CUSTOM_REG_BLTBDAT  0x072
#define CUSTOM_REG_BLTADAT  0x074
#define CUSTOM_REG_DENISEID 0x07c
#define CUSTOM_REG_ADKCON   0x09e
#define CUSTOM_REG_ADKCONR  0x010
#define CUSTOM_REG_POTGO    0x034
#define CUSTOM_REG_POTINP   0x016
#define CUSTOM_REG_JOY0DAT  0x00a
#define CUSTOM_REG_JOY1DAT  0x00c
#define CUSTOM_REG_SERDATR  0x018
#define CUSTOM_REG_INTENAR  0x01c
#define CUSTOM_REG_INTREQR  0x01e
#define CUSTOM_REG_COLOR00  0x180
#define CUSTOM_REG_COP1LC   0x080
#define CUSTOM_REG_COP2LC   0x084
#define CUSTOM_REG_COPJMP1  0x088
#define CUSTOM_REG_COPJMP2  0x08a
#define CUSTOM_REG_COPCON   0x02e

/* =========================================================
 * DOS / AmigaDOS #defines
 * ========================================================= */

#define DEFAULT_AMIGA_SYSROOT "sys"

#define MODE_OLDFILE        1005
#define MODE_NEWFILE        1006
#define MODE_READWRITE      1004

#define SHARED_LOCK         -2
#define EXCLUSIVE_LOCK      -1

#define CHANGE_LOCK         0
#define CHANGE_FH           1

#define OFFSET_BEGINNING    -1
#define OFFSET_CURRENT       0
#define OFFSET_END           1

#define LINK_HARD            0
#define LINK_SOFT            1

#define FILE_KIND_REGULAR    42
#define FILE_KIND_CONSOLE    23

#define TD_SECTOR_HOST             512
#define TDERR_NOTSPECIFIED_HOST    20
#define TDERR_BADSECPREAMBLE_HOST  22
#define TDERR_TOOFEWSECS_HOST      26
#define TDERR_WRITEPROT_HOST       28
#define TDERR_DISKCHANGED_HOST     29
#define TDERR_SEEKERROR_HOST       30
#define TDERR_NOMEM_HOST           31
#define FILE_KIND_CON        99

/* ExecBase offsets for task list checking */
#define EXECBASE_THISTASK    276
#define EXECBASE_TASKREADY   406
#define EXECBASE_TASKWAIT    420

/* Interrupt flags */
#define INTENA_MASTER 0x4000
#define INTENA_VBLANK 0x0020

/* Lock/record lock limits */
#define MAX_LOCKS 256
#define MAX_RECORD_LOCKS 256
#define MAX_TIMER_REQUESTS 64
#define MAX_NOTIFY_REQUESTS 128

/* Amiga DOS protection flag bits */
#define FIBF_DELETE     (1<<0)
#define FIBF_EXECUTE    (1<<1)
#define FIBF_WRITE      (1<<2)
#define FIBF_READ       (1<<3)
#define FIBF_ARCHIVE    (1<<4)
#define FIBF_PURE       (1<<5)
#define FIBF_SCRIPT     (1<<6)

/* Amiga DOS file types */
#define ST_ROOT         1
#define ST_USERDIR      2
#define ST_SOFTLINK     3
#define ST_LINKDIR      4
#define ST_FILE        -3
#define ST_LINKFILE    -4

/* Amiga lock modes */
#define ACCESS_READ    -2
#define ACCESS_WRITE   -1

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

/* Timer interval */
#define TIMER_INTERVAL_US 20000

/* =========================================================
 * Struct typedefs (lock, record lock, timer, audio, notify)
 * ========================================================= */

typedef struct lock_entry_s {
    bool in_use;
    char linux_path[PATH_MAX];
    char amiga_path[PATH_MAX];
    DIR *dir;
    int refcount;
    bool is_dir;
    int32_t access_mode;
} lock_entry_t;

typedef struct record_lock_entry_s {
    bool in_use;
    dev_t dev;
    ino_t ino;
    uint32_t owner_fh68k;
    uint64_t offset;
    uint64_t length;
    bool exclusive;
} record_lock_entry_t;

typedef struct timer_request_s {
    bool in_use;
    bool expired;
    uint32_t ioreq_ptr;
    uint64_t wake_time_us;
} timer_request_t;

#ifdef SDL2_FOUND
#define AUDIO_HOST_CHANNELS 4
typedef struct audio_channel_state_s {
    bool active;
    uint8_t channel;
    uint16_t volume;
    uint16_t period;
    uint32_t length;
    uint32_t cycles;
} audio_channel_state_t;
#endif

typedef struct notify_entry_s {
    bool in_use;
    uint32_t notify68k;
    char linux_path[PATH_MAX];
    bool pending;
    bool existed;
    time_t mtime;
    off_t size;
    ino_t ino;
    dev_t dev;
} notify_entry_t;

/* =========================================================
 * Exported globals (defined in lxa.c)
 * ========================================================= */

extern uint8_t  g_ram[];
extern uint8_t  g_rom[];
extern bool     g_verbose;
extern bool     g_running;
extern char    *g_loadfile;
extern char     g_args[];
extern int      g_args_len;
extern int      g_rv;
extern uint16_t g_color_regs[32];
extern uint16_t g_intena;
extern uint16_t g_intreq;
extern uint16_t g_dmacon;
extern volatile sig_atomic_t g_pending_irq;

/* Globals defined in lxa.c needed by other modules */
extern bool     g_trace;
extern display_event_t g_last_event;
extern void   (*g_console_output_hook)(const char *data, int len);
extern void   (*g_text_hook)(const char *str, int len, int x, int y, void *userdata);
extern void    *g_text_hook_userdata;
extern char    *g_sysroot;

/* Globals defined in lxa_dos_host.c */
extern lock_entry_t        g_locks[];
extern record_lock_entry_t g_record_locks[];
extern timer_request_t     g_timer_queue[];
extern notify_entry_t      g_notify_requests[];
#ifdef SDL2_FOUND
extern audio_channel_state_t g_audio_channels[];
extern SDL_AudioDeviceID     g_audio_device;
extern bool                  g_audio_initialized;
#endif

/* Pending-breakpoint type (used by lxa.c debugger + lxa_dispatch.c op_illg) */
typedef struct pending_bp_s pending_bp_t;
struct pending_bp_s {
    pending_bp_t *next;
    char          name[256];
};
extern pending_bp_t *_g_pending_bps;

/* Console input queue helpers (defined in lxa.c, used by lxa_dos_host.c) */
bool lxa_host_console_input_empty(void);
int  lxa_host_console_input_pop(void);

/* =========================================================
 * Forward declarations of functions called across modules
 * ========================================================= */

/* From lxa_custom.c */
void _handle_custom_write(uint16_t reg, uint16_t value);
void _handle_custom_write_ext(uint16_t reg, uint16_t value);

/* From lxa.c (debugger) */
void _debug(uint32_t pc);
void hexdump(int lvl, uint32_t offset, uint32_t len);
void _update_debug_active(void);
char *_mgetstr(uint32_t address);
/* Debugger internals called by op_illg in lxa_dispatch.c */
void     _debug_add_bp(uint32_t addr);
uint32_t _debug_parse_addr(const char *buf);
bool     _symtab_add(char *name, uint32_t offset);
extern   pending_bp_t *_g_pending_bps;

/* From lxa_dos_host.c */
int errno2Amiga(void);
bool is_list_empty(uint32_t list_addr);
bool other_tasks_running(void);

int _timer_check_expired(void);
/* Timer internals called by op_illg */
bool     _timer_add_request(uint32_t ioreq_ptr, uint32_t delay_secs, uint32_t delay_micros);
bool     _timer_remove_request(uint32_t ioreq_ptr);
uint32_t _timer_get_expired(void);

void _dos_stdinout_fh(uint32_t fh68k, int is_input);
int _dos_open(uint32_t path68k, uint32_t accessMode, uint32_t fh68k);
int _dos_read(uint32_t fh68k, uint32_t buf68k, uint32_t len68k);
int _dos_seek(uint32_t fh68k, int32_t position, int32_t mode);
int _dos_setfilesize(uint32_t fh68k, int32_t offset, int32_t mode);
int _dos_write(uint32_t fh68k, uint32_t buf68k, uint32_t len68k);
int _dos_flush(uint32_t fh68k);
int _dos_close(uint32_t fh68k);

uint32_t _dos_lock(uint32_t name68k, int32_t mode);
void _dos_unlock(uint32_t lock_id);
uint32_t _dos_duplock(uint32_t lock_id);
uint32_t _dos_lockrecord(uint32_t fh68k, uint32_t offset, uint32_t length,
                          uint32_t timeout, uint32_t mode);
uint32_t _dos_unlockrecord(uint32_t fh68k, uint32_t offset, uint32_t length);
int _dos_examine(uint32_t lock_id, uint32_t fib68k);
int _dos_exnext(uint32_t lock_id, uint32_t fib68k);
int _dos_info(uint32_t lock_id, uint32_t infodata68k);
int _dos_samelock(uint32_t lock1_id, uint32_t lock2_id);
uint32_t _dos_parentdir(uint32_t lock_id);
uint32_t _dos_createdir(uint32_t name68k);
int _dos_deletefile(uint32_t name68k);
int _dos_rename(uint32_t old68k, uint32_t new68k);
int _dos_namefromlock(uint32_t lock_id, uint32_t buf68k, uint32_t buflen);
int _dos_setprotection(uint32_t name68k, uint32_t protect);
int _dos_setcomment(uint32_t name68k, uint32_t comment68k);
int _dos_setowner(uint32_t name68k, uint32_t owner68k, uint32_t err68k);
int _dos_setfiledate(uint32_t name68k, uint32_t date68k, uint32_t err68k);
int _dos_readlink(uint32_t path68k, uint32_t buffer68k, uint32_t size, uint32_t err68k);
int _dos_makelink(uint32_t name68k, uint32_t dest_param, int32_t soft, uint32_t err68k);
int _dos_assign_add(uint32_t name68k, uint32_t path68k, uint32_t type);
int _dos_assign_remove(uint32_t name68k);
int _dos_assign_list(uint32_t buf68k, uint32_t buflen);
int _dos_assign_remove_path(uint32_t name68k, uint32_t path68k);
int _dos_getdevproc(uint32_t name68k, uint32_t dp68k, uint32_t err68k);
int _dos_notify_start(uint32_t notify68k, uint32_t fullname68k, uint32_t flags);
void _dos_notify_end(uint32_t notify68k);
uint32_t _dos_notify_poll(void);
uint32_t _dos_duplockfromfh(uint32_t fh68k);
int _dos_examinefh(uint32_t fh68k, uint32_t fib68k);
int _dos_namefromfh(uint32_t fh68k, uint32_t buf68k, uint32_t len);
uint32_t _dos_parentoffh(uint32_t fh68k);
int _dos_openfromlock(uint32_t lock_id, uint32_t fh68k);
int _dos_waitforchar(uint32_t fh68k, uint32_t timeout_us);
bool _dos_change_mode(uint32_t type, uint32_t object, int32_t new_mode, uint32_t err68k);

int _trackdisk_read(uint32_t unit, uint32_t data_ptr, uint32_t length, uint32_t offset, uint32_t is_ext);
int _trackdisk_write(uint32_t unit, uint32_t data_ptr, uint32_t length, uint32_t offset, uint32_t is_ext);
int _trackdisk_format(uint32_t unit, uint32_t data_ptr, uint32_t length, uint32_t offset, uint32_t is_ext);
int _trackdisk_seek(uint32_t unit, uint32_t offset, uint32_t is_ext);

void _audio_queue_fragment(uint32_t packed, uint32_t data_ptr, uint32_t length,
                            uint32_t period, uint32_t volume);

bool lxa_host_console_enqueue_char(char ch);
bool lxa_host_console_enqueue_rawkey(int rawkey, int qualifier, bool down);

/* From lxa_dispatch.c */
int op_illg(int level);

/* Phase 126: Profiling counters (defined in lxa_api.c, updated in lxa_dispatch.c) */
#define LXA_PROFILE_MAX_EMUCALL 6000
extern uint64_t g_profile_calls[LXA_PROFILE_MAX_EMUCALL];
extern uint64_t g_profile_ns[LXA_PROFILE_MAX_EMUCALL];

/* Debugger jitter tolerance when matching symbol names to PC */
#define MAX_JITTER 1024

/* Number of m68k registers (used by debugger) */
#define NUM_M68K_REGS 27

/* ANSI/VT100 CSI escape sequence prefix (used by console and debugger) */
#define CSI "\e["
#define CSI_BUF_LEN  64
#define CSI_MAX_PARAMS 8

/* From lxa_dos_host.c - audio shutdown (called from lxa.c main cleanup) */
void _audio_shutdown(void);

#endif /* LXA_INTERNAL_H */
