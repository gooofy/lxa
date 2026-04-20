/*
 * lxa_profile.c - Per-ROM-function profiling infrastructure (Phase 126).
 *
 * Compiled into both the lxa executable and liblxa so both can write
 * profiling JSON.  The counters are updated in op_illg() whenever
 * PROFILE_BUILD is defined.
 */

#include "lxa_api.h"
#include "lxa_internal.h"

#include <inttypes.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* =========================================================
 * Counter storage (linked against exactly once)
 * ========================================================= */

uint64_t g_profile_calls[LXA_PROFILE_MAX_EMUCALL];
uint64_t g_profile_ns[LXA_PROFILE_MAX_EMUCALL];

/* =========================================================
 * Public API
 * ========================================================= */

void lxa_profile_reset(void)
{
    memset(g_profile_calls, 0, sizeof(g_profile_calls));
    memset(g_profile_ns,    0, sizeof(g_profile_ns));
}

int lxa_profile_get(lxa_profile_entry_t *entries, int max_count)
{
    if (!entries || max_count <= 0)
        return 0;

    int count = 0;
    for (int i = 0; i < LXA_PROFILE_MAX_EMUCALL && count < max_count; i++)
    {
        if (g_profile_calls[i] > 0)
        {
            entries[count].emucall_id = i;
            entries[count].call_count = g_profile_calls[i];
            entries[count].total_ns   = g_profile_ns[i];
            count++;
        }
    }
    return count;
}

void lxa_profile_emucall_name(int emucall_id, char *buf, int buf_size)
{
    /* Generated from emucalls.h — keep in sync when new EMU_CALL_* are added. */
    static const struct { int id; const char *name; } s_names[] = {
        { 1,    "EMU_CALL_LPUTC" },
        { 2,    "EMU_CALL_STOP" },
        { 3,    "EMU_CALL_TRACE" },
        { 4,    "EMU_CALL_LPUTS" },
        { 5,    "EMU_CALL_EXCEPTION" },
        { 6,    "EMU_CALL_WAIT" },
        { 7,    "EMU_CALL_LOADED" },
        { 8,    "EMU_CALL_MONITOR" },
        { 9,    "EMU_CALL_LOADFILE" },
        { 10,   "EMU_CALL_SYMBOL" },
        { 11,   "EMU_CALL_GETSYSTIME" },
        { 12,   "EMU_CALL_GETARGS" },
        { 13,   "EMU_CALL_DELAY" },
        { 127,  "EMU_CALL_EXIT" },
        /* DOS */
        { 1000, "EMU_CALL_DOS_OPEN" },
        { 1001, "EMU_CALL_DOS_READ" },
        { 1002, "EMU_CALL_DOS_SEEK" },
        { 1003, "EMU_CALL_DOS_CLOSE" },
        { 1004, "EMU_CALL_DOS_INPUT" },
        { 1005, "EMU_CALL_DOS_OUTPUT" },
        { 1006, "EMU_CALL_DOS_WRITE" },
        { 1007, "EMU_CALL_DOS_LOCK" },
        { 1008, "EMU_CALL_DOS_UNLOCK" },
        { 1009, "EMU_CALL_DOS_DUPLOCK" },
        { 1010, "EMU_CALL_DOS_EXAMINE" },
        { 1011, "EMU_CALL_DOS_EXNEXT" },
        { 1012, "EMU_CALL_DOS_INFO" },
        { 1013, "EMU_CALL_DOS_SAMELOCK" },
        { 1014, "EMU_CALL_DOS_PARENTDIR" },
        { 1015, "EMU_CALL_DOS_CREATEDIR" },
        { 1016, "EMU_CALL_DOS_CURRENTDIR" },
        { 1017, "EMU_CALL_DOS_DELETE" },
        { 1018, "EMU_CALL_DOS_RENAME" },
        { 1019, "EMU_CALL_DOS_SETPROTECTION" },
        { 1020, "EMU_CALL_DOS_SETCOMMENT" },
        { 1021, "EMU_CALL_DOS_NAMEFROMLOCK" },
        { 1022, "EMU_CALL_DOS_FLUSH" },
        { 1030, "EMU_CALL_DOS_ASSIGN_ADD" },
        { 1031, "EMU_CALL_DOS_ASSIGN_REMOVE" },
        { 1032, "EMU_CALL_DOS_ASSIGN_LIST" },
        { 1033, "EMU_CALL_DOS_ASSIGN_REMOVE_PATH" },
        { 1034, "EMU_CALL_DOS_GETDEVPROC" },
        { 1035, "EMU_CALL_DOS_NOTIFY_START" },
        { 1036, "EMU_CALL_DOS_NOTIFY_END" },
        { 1037, "EMU_CALL_DOS_NOTIFY_POLL" },
        { 1040, "EMU_CALL_DOS_DUPLOCKFROMFH" },
        { 1041, "EMU_CALL_DOS_EXAMINEFH" },
        { 1042, "EMU_CALL_DOS_NAMEFROMFH" },
        { 1043, "EMU_CALL_DOS_PARENTOFFH" },
        { 1044, "EMU_CALL_DOS_OPENFROMLOCK" },
        { 1045, "EMU_CALL_DOS_WAITFORCHAR" },
        { 1046, "EMU_CALL_DOS_SETFILESIZE" },
        { 1047, "EMU_CALL_DOS_SETFILEDATE" },
        { 1048, "EMU_CALL_DOS_READLINK" },
        { 1049, "EMU_CALL_DOS_MAKELINK" },
        { 1050, "EMU_CALL_TRACKDISK_READ" },
        { 1051, "EMU_CALL_TRACKDISK_WRITE" },
        { 1052, "EMU_CALL_TRACKDISK_FORMAT" },
        { 1053, "EMU_CALL_TRACKDISK_SEEK" },
        { 1054, "EMU_CALL_DOS_LOCKRECORD" },
        { 1055, "EMU_CALL_DOS_UNLOCKRECORD" },
        { 1056, "EMU_CALL_DOS_CHANGEMODE" },
        { 1057, "EMU_CALL_DOS_SETOWNER" },
        /* Graphics */
        { 2000, "EMU_CALL_GFX_INIT" },
        { 2001, "EMU_CALL_GFX_SHUTDOWN" },
        { 2002, "EMU_CALL_GFX_OPEN_DISPLAY" },
        { 2003, "EMU_CALL_GFX_CLOSE_DISPLAY" },
        { 2004, "EMU_CALL_GFX_REFRESH" },
        { 2010, "EMU_CALL_GFX_SET_COLOR" },
        { 2011, "EMU_CALL_GFX_SET_PALETTE4" },
        { 2012, "EMU_CALL_GFX_SET_PALETTE32" },
        { 2020, "EMU_CALL_GFX_WRITE_PIXEL" },
        { 2021, "EMU_CALL_GFX_READ_PIXEL" },
        { 2022, "EMU_CALL_GFX_RECT_FILL" },
        { 2023, "EMU_CALL_GFX_DRAW_LINE" },
        { 2030, "EMU_CALL_GFX_UPDATE_PLANAR" },
        { 2031, "EMU_CALL_GFX_UPDATE_CHUNKY" },
        { 2040, "EMU_CALL_GFX_GET_SIZE" },
        { 2041, "EMU_CALL_GFX_AVAILABLE" },
        { 2042, "EMU_CALL_GFX_POLL_EVENTS" },
        { 2050, "EMU_CALL_GFX_BLT_BITMAP" },
        /* Intuition */
        { 3000, "EMU_CALL_INT_OPEN_SCREEN" },
        { 3001, "EMU_CALL_INT_CLOSE_SCREEN" },
        { 3002, "EMU_CALL_INT_REFRESH_SCREEN" },
        { 3003, "EMU_CALL_INT_SET_SCREEN_BITMAP" },
        { 3010, "EMU_CALL_INT_POLL_INPUT" },
        { 3011, "EMU_CALL_INT_GET_MOUSE_POS" },
        { 3012, "EMU_CALL_INT_GET_MOUSE_BTN" },
        { 3013, "EMU_CALL_INT_GET_KEY" },
        { 3014, "EMU_CALL_INT_GET_EVENT_WIN" },
        { 3020, "EMU_CALL_INT_OPEN_WINDOW" },
        { 3021, "EMU_CALL_INT_CLOSE_WINDOW" },
        { 3022, "EMU_CALL_INT_MOVE_WINDOW" },
        { 3023, "EMU_CALL_INT_SIZE_WINDOW" },
        { 3024, "EMU_CALL_INT_WINDOW_TOFRONT" },
        { 3025, "EMU_CALL_INT_WINDOW_TOBACK" },
        { 3026, "EMU_CALL_INT_SET_TITLE" },
        { 3027, "EMU_CALL_INT_REFRESH_WINDOW" },
        { 3028, "EMU_CALL_INT_GET_ROOTLESS" },
        { 3029, "EMU_CALL_INT_ATTACH_WINDOW" },
        /* Timer */
        { 3500, "EMU_CALL_TIMER_ADD" },
        { 3501, "EMU_CALL_TIMER_REMOVE" },
        { 3502, "EMU_CALL_TIMER_CHECK" },
        { 3503, "EMU_CALL_TIMER_GET_EXPIRED" },
        /* Audio */
        { 3600, "EMU_CALL_AUDIO_PLAY" },
        /* Console */
        { 4000, "EMU_CALL_CON_READ" },
        { 4001, "EMU_CALL_CON_INPUT_READY" },
        /* Test infrastructure */
        { 4100, "EMU_CALL_TEST_INJECT_KEY" },
        { 4101, "EMU_CALL_TEST_INJECT_STRING" },
        { 4102, "EMU_CALL_TEST_INJECT_MOUSE" },
        { 4110, "EMU_CALL_TEST_CAPTURE_SCREEN" },
        { 4111, "EMU_CALL_TEST_CAPTURE_WINDOW" },
        { 4112, "EMU_CALL_TEST_COMPARE_SCREEN" },
        { 4120, "EMU_CALL_TEST_SET_HEADLESS" },
        { 4121, "EMU_CALL_TEST_GET_HEADLESS" },
        { 4122, "EMU_CALL_TEST_WAIT_IDLE" },
        /* IEEE DP math */
        { 5000, "EMU_CALL_IEEEDP_FIX" },
        { 5001, "EMU_CALL_IEEEDP_FLT" },
        { 5002, "EMU_CALL_IEEEDP_CMP" },
        { 5003, "EMU_CALL_IEEEDP_TST" },
        { 5004, "EMU_CALL_IEEEDP_ABS" },
        { 5005, "EMU_CALL_IEEEDP_NEG" },
        { 5006, "EMU_CALL_IEEEDP_ADD" },
        { 5007, "EMU_CALL_IEEEDP_SUB" },
        { 5008, "EMU_CALL_IEEEDP_MUL" },
        { 5009, "EMU_CALL_IEEEDP_DIV" },
        { 5010, "EMU_CALL_IEEEDP_FLOOR" },
        { 5011, "EMU_CALL_IEEEDP_CEIL" },
        { 5020, "EMU_CALL_IEEEDP_ATAN" },
        { 5021, "EMU_CALL_IEEEDP_SIN" },
        { 5022, "EMU_CALL_IEEEDP_COS" },
        { 5023, "EMU_CALL_IEEEDP_TAN" },
        { 5024, "EMU_CALL_IEEEDP_SINCOS" },
        { 5025, "EMU_CALL_IEEEDP_SINH" },
        { 5026, "EMU_CALL_IEEEDP_COSH" },
        { 5027, "EMU_CALL_IEEEDP_TANH" },
        { 5028, "EMU_CALL_IEEEDP_EXP" },
        { 5029, "EMU_CALL_IEEEDP_LOG" },
        { 5030, "EMU_CALL_IEEEDP_POW" },
        { 5031, "EMU_CALL_IEEEDP_SQRT" },
        { 5032, "EMU_CALL_IEEEDP_TIEEE" },
        { 5033, "EMU_CALL_IEEEDP_FIEEE" },
        { 5034, "EMU_CALL_IEEEDP_ASIN" },
        { 5035, "EMU_CALL_IEEEDP_ACOS" },
        { 5036, "EMU_CALL_IEEEDP_LOG10" },
        /* IEEE SP math */
        { 5100, "EMU_CALL_IEEESP_FIX" },
        { 5101, "EMU_CALL_IEEESP_FLT" },
        { 5102, "EMU_CALL_IEEESP_CMP" },
        { 5103, "EMU_CALL_IEEESP_TST" },
        { 5104, "EMU_CALL_IEEESP_ABS" },
        { 5105, "EMU_CALL_IEEESP_NEG" },
        { 5106, "EMU_CALL_IEEESP_ADD" },
        { 5107, "EMU_CALL_IEEESP_SUB" },
        { 5108, "EMU_CALL_IEEESP_MUL" },
        { 5109, "EMU_CALL_IEEESP_DIV" },
        { 5110, "EMU_CALL_IEEESP_FLOOR" },
        { 5111, "EMU_CALL_IEEESP_CEIL" },
        /* FFP math */
        { 5120, "EMU_CALL_FFP_ATAN" },
        { 5121, "EMU_CALL_FFP_SIN" },
        { 5122, "EMU_CALL_FFP_COS" },
        { 5123, "EMU_CALL_FFP_TAN" },
        { 5124, "EMU_CALL_FFP_SINCOS" },
        { 5125, "EMU_CALL_FFP_SINH" },
        { 5126, "EMU_CALL_FFP_COSH" },
        { 5127, "EMU_CALL_FFP_TANH" },
        { 5128, "EMU_CALL_FFP_EXP" },
        { 5129, "EMU_CALL_FFP_LOG" },
        { 5130, "EMU_CALL_FFP_POW" },
        { 5131, "EMU_CALL_FFP_SQRT" },
        { 5132, "EMU_CALL_FFP_TIEEE" },
        { 5133, "EMU_CALL_FFP_FIEEE" },
        { 5134, "EMU_CALL_FFP_ASIN" },
        { 5135, "EMU_CALL_FFP_ACOS" },
        { 5136, "EMU_CALL_FFP_LOG10" },
        { 5137, "EMU_CALL_FFP_FLOOR" },
        { 5138, "EMU_CALL_FFP_CEIL" },
    };

    for (int i = 0; i < (int)(sizeof(s_names)/sizeof(s_names[0])); i++)
    {
        if (s_names[i].id == emucall_id)
        {
            snprintf(buf, (size_t)buf_size, "%s", s_names[i].name);
            return;
        }
    }
    snprintf(buf, (size_t)buf_size, "EMU_CALL_UNKNOWN_%d", emucall_id);
}

static int _profile_cmp_by_ns(const void *a, const void *b)
{
    const lxa_profile_entry_t *ea = (const lxa_profile_entry_t *)a;
    const lxa_profile_entry_t *eb = (const lxa_profile_entry_t *)b;
    if (eb->total_ns > ea->total_ns) return  1;
    if (eb->total_ns < ea->total_ns) return -1;
    return 0;
}

bool lxa_profile_write_json(const char *path)
{
    if (!path) return false;

    lxa_profile_entry_t entries[LXA_PROFILE_MAX_EMUCALL];
    int count = lxa_profile_get(entries, LXA_PROFILE_MAX_EMUCALL);
    if (count == 0)
    {
        FILE *f = fopen(path, "w");
        if (!f) return false;
        fprintf(f, "[]\n");
        fclose(f);
        return true;
    }

    qsort(entries, (size_t)count, sizeof(entries[0]), _profile_cmp_by_ns);

    FILE *f = fopen(path, "w");
    if (!f) return false;

    fprintf(f, "[\n");
    char name[64];
    for (int i = 0; i < count; i++)
    {
        lxa_profile_emucall_name(entries[i].emucall_id, name, (int)sizeof(name));
        fprintf(f, "  {\"id\": %d, \"name\": \"%s\", \"calls\": %" PRIu64 ", \"total_ns\": %" PRIu64 "}%s\n",
                entries[i].emucall_id,
                name,
                entries[i].call_count,
                entries[i].total_ns,
                (i < count - 1) ? "," : "");
    }
    fprintf(f, "]\n");
    fclose(f);
    return true;
}
