#ifndef HAVE_EMUCALLS_H
#define HAVE_EMUCALLS_H

#define EMU_CALL_LPUTC         1
#define EMU_CALL_STOP          2
#define EMU_CALL_TRACE         3
#define EMU_CALL_LPUTS         4
#define EMU_CALL_EXCEPTION     5
#define EMU_CALL_WAIT          6
#define EMU_CALL_LOADED        7
#define EMU_CALL_MONITOR       8
#define EMU_CALL_LOADFILE      9
#define EMU_CALL_SYMBOL       10
#define EMU_CALL_GETSYSTIME   11
#define EMU_CALL_GETARGS      12
#define EMU_CALL_EXIT        127

/*
 * DOS Library emucalls (1000-1999)
 */

/* Basic File I/O (Phase 2) */
#define EMU_CALL_DOS_OPEN      1000
#define EMU_CALL_DOS_READ      1001
#define EMU_CALL_DOS_SEEK      1002
#define EMU_CALL_DOS_CLOSE     1003
#define EMU_CALL_DOS_INPUT     1004
#define EMU_CALL_DOS_OUTPUT    1005
#define EMU_CALL_DOS_WRITE     1006

/* Locks & Examination API (Phase 3) */
#define EMU_CALL_DOS_LOCK      1007  /* Lock(name, mode) -> lock_id or 0 */
#define EMU_CALL_DOS_UNLOCK    1008  /* UnLock(lock_id) */
#define EMU_CALL_DOS_DUPLOCK   1009  /* DupLock(lock_id) -> lock_id */
#define EMU_CALL_DOS_EXAMINE   1010  /* Examine(lock_id, fib68k) -> success */
#define EMU_CALL_DOS_EXNEXT    1011  /* ExNext(lock_id, fib68k) -> success */
#define EMU_CALL_DOS_INFO      1012  /* Info(lock_id, infodata68k) -> success */
#define EMU_CALL_DOS_SAMELOCK  1013  /* SameLock(lock1, lock2) -> result */
#define EMU_CALL_DOS_PARENTDIR 1014  /* ParentDir(lock_id) -> lock_id */
#define EMU_CALL_DOS_CREATEDIR 1015  /* CreateDir(name) -> lock_id */
#define EMU_CALL_DOS_CURRENTDIR 1016 /* CurrentDir(lock_id) -> old_lock_id */
#define EMU_CALL_DOS_DELETE    1017  /* DeleteFile(name) -> success */
#define EMU_CALL_DOS_RENAME    1018  /* Rename(old, new) -> success */
#define EMU_CALL_DOS_SETPROTECTION 1019 /* SetProtection(name, mask) -> success */
#define EMU_CALL_DOS_SETCOMMENT 1020 /* SetComment(name, comment) -> success */
#define EMU_CALL_DOS_NAMEFROMLOCK 1021 /* NameFromLock(lock, buf, len) -> success */
#define EMU_CALL_DOS_FLUSH     1022  /* Flush(fh) -> success */

#endif

