/*
 * rexx_common.h - shared by RexxMast, the ARexx interpreter (SYS:System/
 * ARexx) and the Rexxc commands (Phase 250).
 *
 * The global ARexx state lives in the rexxsyslib.library base, as on
 * AmigaOS 3.1: the Library List (function hosts and libraries), the Clip
 * List, the task count and the halt/trace flags.  Everybody changes it
 * under LockRexxBase().
 *
 * Copyright (C) 2026 by G. Bartsch. Licensed under the MIT License.
 */
#ifndef LXA_REXX_COMMON_H
#define LXA_REXX_COMMON_H

#include <exec/types.h>
#include <exec/memory.h>
#include <exec/ports.h>
#include <exec/lists.h>
#include <rexx/storage.h>
#include <rexx/rxslib.h>
#include <proto/exec.h>
#include <proto/rexxsyslib.h>

/* the interpreter RexxMast starts for every RXCOMM/RXFUNC packet */
#define LXA_REXX_INTERPRETER "SYS:System/ARexx"

/* stack of an interpreter process (Regina recurses deeply) */
#define LXA_REXX_STACK 200000

#include "rexx_errtext.h"

/* ---- small string helpers (no C library needed) ---------------------- */

static inline LONG lxa_rexx_strlen(const char *s)
{
    LONG n = 0;
    while (s && s[n])
        n++;
    return n;
}

static inline char lxa_rexx_upc(char c)
{
    return (c >= 'a' && c <= 'z') ? (char)(c - 32) : c;
}

/* case-insensitive comparison of a NUL-terminated name with (s, len) */
static inline BOOL lxa_rexx_name_eq(const char *name, const char *s, LONG len)
{
    LONG i;
    if (!name)
        return FALSE;
    for (i = 0; i < len; i++)
        if (!name[i] || lxa_rexx_upc(name[i]) != lxa_rexx_upc(s[i]))
            return FALSE;
    return name[len] == 0;
}

/* a RexxMsg argument that may be an integer or a numeric argstring */
static inline LONG lxa_rexx_arg_long(STRPTR arg)
{
    LONG v = (LONG)arg, n = 0, sign = 1;
    const char *s = (const char *)arg;

    if (v >= -65536 && v < 1024)
        return v;               /* passed as a plain integer */
    while (*s == ' ')
        s++;
    if (*s == '-' || *s == '+')
        sign = *s++ == '-' ? -1 : 1;
    while (*s >= '0' && *s <= '9')
        n = n * 10 + (*s++ - '0');
    return sign * n;
}

/* ---- resource nodes ---------------------------------------------------- */

static inline struct RexxRsrc *lxa_rexx_find(struct List *list, const char *s, LONG len)
{
    struct Node *n;
    for (n = list->lh_Head; n->ln_Succ; n = n->ln_Succ)
        if (lxa_rexx_name_eq(n->ln_Name, s, len))
            return (struct RexxRsrc *)n;
    return NULL;
}

static inline void lxa_rexx_free_node(struct RexxRsrc *rr)
{
    if (rr->rr_Node.ln_Name)
        DeleteArgstring((UBYTE *)rr->rr_Node.ln_Name);
    if (rr->rr_Node.ln_Type == RRT_CLIP && rr->rr_Arg1)
        DeleteArgstring((UBYTE *)rr->rr_Arg1);
    FreeMem(rr, sizeof(struct RexxRsrc));
}

static inline struct RexxRsrc *lxa_rexx_new_node(UBYTE type, const char *s, LONG len, BYTE pri)
{
    struct RexxRsrc *rr = (struct RexxRsrc *)AllocMem(sizeof(struct RexxRsrc), MEMF_PUBLIC | MEMF_CLEAR);
    if (!rr)
        return NULL;
    rr->rr_Node.ln_Type = type;
    rr->rr_Node.ln_Pri = pri;
    rr->rr_Size = sizeof(struct RexxRsrc);
    rr->rr_Node.ln_Name = (char *)CreateArgstring((STRPTR)s, len);
    if (!rr->rr_Node.ln_Name) {
        FreeMem(rr, sizeof(struct RexxRsrc));
        return NULL;
    }
    return rr;
}

/* Clip List: set (value != NULL) or remove (value == NULL) a clip.
 * Returns FALSE when a clip to remove does not exist or memory is short. */
static inline BOOL lxa_rexx_setclip(const char *name, LONG nlen, const char *value, LONG vlen)
{
    struct RxsLib *rl = RexxSysBase;
    struct RexxRsrc *rr, *nr = NULL;
    UBYTE *val = NULL;
    BOOL ok = TRUE;

    if (value) {
        nr = lxa_rexx_new_node(RRT_CLIP, name, nlen, 0);
        val = CreateArgstring((STRPTR)value, vlen);
        if (!nr || !val) {
            if (nr)
                lxa_rexx_free_node(nr);
            if (val)
                DeleteArgstring(val);
            return FALSE;
        }
        nr->rr_Arg1 = (LONG)val;
    }
    LockRexxBase(0);
    rr = lxa_rexx_find(&rl->rl_ClipList, name, nlen);
    if (rr) {
        Remove(&rr->rr_Node);
        rl->rl_NumClip--;
    } else if (!value) {
        ok = FALSE;
    }
    if (nr) {
        AddTail(&rl->rl_ClipList, &nr->rr_Node);
        rl->rl_NumClip++;
    }
    UnlockRexxBase(0);
    if (rr)
        lxa_rexx_free_node(rr);
    return ok;
}

/* Library List: add a function host (offset < 0 means none) or a function
 * library.  Returns FALSE when the name is already listed. */
static inline BOOL lxa_rexx_addlib(const char *name, LONG nlen, LONG pri, BOOL is_lib,
                                   LONG offset, LONG version)
{
    struct RxsLib *rl = RexxSysBase;
    struct RexxRsrc *nr;
    BOOL ok = FALSE;

    if (pri < -100)
        pri = -100;
    if (pri > 100)
        pri = 100;
    nr = lxa_rexx_new_node(is_lib ? RRT_LIB : RRT_HOST, name, nlen, (BYTE)pri);
    if (!nr)
        return FALSE;
    nr->rr_Arg1 = offset;
    nr->rr_Arg2 = version;
    LockRexxBase(0);
    if (!lxa_rexx_find(&rl->rl_LibList, name, nlen)) {
        Enqueue(&rl->rl_LibList, &nr->rr_Node);
        rl->rl_NumLib++;
        ok = TRUE;
    }
    UnlockRexxBase(0);
    if (!ok)
        lxa_rexx_free_node(nr);
    return ok;
}

static inline BOOL lxa_rexx_remlib(const char *name, LONG nlen)
{
    struct RxsLib *rl = RexxSysBase;
    struct RexxRsrc *rr;

    LockRexxBase(0);
    rr = lxa_rexx_find(&rl->rl_LibList, name, nlen);
    if (rr) {
        Remove(&rr->rr_Node);
        rl->rl_NumLib--;
    }
    UnlockRexxBase(0);
    if (!rr)
        return FALSE;
    lxa_rexx_free_node(rr);
    return TRUE;
}

/* send a packet to the REXX port and wait for the reply; FALSE when the
 * port does not exist (RexxMast is not running) */
static inline BOOL lxa_rexx_send(struct RexxMsg *rm, struct MsgPort *reply)
{
    struct MsgPort *rexx;

    Forbid();
    rexx = FindPort((STRPTR)"REXX");
    if (rexx)
        PutMsg(rexx, (struct Message *)rm);
    Permit();
    if (!rexx)
        return FALSE;
    WaitPort(reply);
    GetMsg(reply);
    return TRUE;
}

#endif
