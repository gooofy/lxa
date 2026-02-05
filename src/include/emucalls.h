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
#define EMU_CALL_DELAY        13   /* Delay with interrupt processing: d1=milliseconds */
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

/* Phase 7: Assignment System */
#define EMU_CALL_DOS_ASSIGN_ADD    1030  /* AssignAdd(name, path, type) -> success */
#define EMU_CALL_DOS_ASSIGN_REMOVE 1031  /* AssignRemove(name) -> success */
#define EMU_CALL_DOS_ASSIGN_LIST   1032  /* AssignList(buf, buflen) -> count */

/* Phase 10: File Handle Utilities */
#define EMU_CALL_DOS_DUPLOCKFROMFH 1040  /* DupLockFromFH(fh) -> lock_id */
#define EMU_CALL_DOS_EXAMINEFH     1041  /* ExamineFH(fh, fib68k) -> success */
#define EMU_CALL_DOS_NAMEFROMFH    1042  /* NameFromFH(fh, buf, len) -> success */
#define EMU_CALL_DOS_PARENTOFFH    1043  /* ParentOfFH(fh) -> lock_id */
#define EMU_CALL_DOS_OPENFROMLOCK  1044  /* OpenFromLock(lock) -> fh */
#define EMU_CALL_DOS_WAITFORCHAR   1045  /* WaitForChar(fh, timeout) -> success */

/*
 * Graphics Library emucalls (2000-2999)
 *
 * Phase 13: Graphics Foundation
 * These emucalls bridge the m68k graphics.library to host SDL2 display.
 */

/* Display Management */
#define EMU_CALL_GFX_INIT          2000  /* Initialize display subsystem */
#define EMU_CALL_GFX_SHUTDOWN      2001  /* Shutdown display subsystem */
#define EMU_CALL_GFX_OPEN_DISPLAY  2002  /* Open display: (width, height, depth) -> handle */
#define EMU_CALL_GFX_CLOSE_DISPLAY 2003  /* Close display: (handle) */
#define EMU_CALL_GFX_REFRESH       2004  /* Refresh display: (handle) */

/* Palette Management */
#define EMU_CALL_GFX_SET_COLOR     2010  /* Set color: (handle, index, rgb) */
#define EMU_CALL_GFX_SET_PALETTE4  2011  /* Set palette RGB4: (handle, start, count, colors_ptr) */
#define EMU_CALL_GFX_SET_PALETTE32 2012  /* Set palette RGB32: (handle, start, count, colors_ptr) */

/* Pixel/Rect Operations */
#define EMU_CALL_GFX_WRITE_PIXEL   2020  /* Write pixel: (handle, x, y, pen) */
#define EMU_CALL_GFX_READ_PIXEL    2021  /* Read pixel: (handle, x, y) -> pen */
#define EMU_CALL_GFX_RECT_FILL     2022  /* Fill rect: (handle, x1, y1, x2, y2, pen) */
#define EMU_CALL_GFX_DRAW_LINE     2023  /* Draw line: (handle, x1, y1, x2, y2, pen) */

/* Bitmap Updates */
#define EMU_CALL_GFX_UPDATE_PLANAR 2030  /* Update from planar: (handle, x, y, w, h, planes_ptr, bpr, depth) */
#define EMU_CALL_GFX_UPDATE_CHUNKY 2031  /* Update from chunky: (handle, x, y, w, h, pixels_ptr, pitch) */

/* Query Functions */
#define EMU_CALL_GFX_GET_SIZE      2040  /* Get display size: (handle) -> packed w/h/d */
#define EMU_CALL_GFX_AVAILABLE     2041  /* Check if SDL2 available: () -> bool */
#define EMU_CALL_GFX_POLL_EVENTS   2042  /* Poll events: () -> quit_requested */

/*
 * Intuition Library emucalls (3000-3999)
 *
 * Phase 13.5: Screen Management
 * These emucalls bridge the m68k intuition.library to host display.
 */

/* Screen Management */
#define EMU_CALL_INT_OPEN_SCREEN   3000  /* OpenScreen: (width, height, depth, title_ptr) -> display_handle */
#define EMU_CALL_INT_CLOSE_SCREEN  3001  /* CloseScreen: (display_handle) -> success */
#define EMU_CALL_INT_REFRESH_SCREEN 3002 /* RefreshScreen: (display_handle, planes_ptr, bpr, depth) */
#define EMU_CALL_INT_SET_SCREEN_BITMAP 3003 /* SetScreenBitmap: (display_handle, planes_ptr, bpr_depth) */

/* Input Handling (Phase 14) */
#define EMU_CALL_INT_POLL_INPUT    3010  /* Poll for input: () -> event_type (0=none, 1=mouse, 2=key, 3=close) */
#define EMU_CALL_INT_GET_MOUSE_POS 3011  /* Get mouse position: () -> (x << 16) | y */
#define EMU_CALL_INT_GET_MOUSE_BTN 3012  /* Get mouse button state: () -> button_code (IECODE_LBUTTON, etc) */
#define EMU_CALL_INT_GET_KEY       3013  /* Get key event: () -> rawkey | (qualifier << 16) */
#define EMU_CALL_INT_GET_EVENT_WIN 3014  /* Get window for last event: () -> window_id (display_handle) */

/* Rootless Windowing (Phase 15) */
#define EMU_CALL_INT_OPEN_WINDOW   3020  /* Open window: (screen_handle, x, y, w, h, title_ptr) -> window_handle */
#define EMU_CALL_INT_CLOSE_WINDOW  3021  /* Close window: (window_handle) -> success */
#define EMU_CALL_INT_MOVE_WINDOW   3022  /* Move window: (window_handle, x, y) -> success */
#define EMU_CALL_INT_SIZE_WINDOW   3023  /* Resize window: (window_handle, w, h) -> success */
#define EMU_CALL_INT_WINDOW_TOFRONT 3024 /* Bring window to front: (window_handle) -> success */
#define EMU_CALL_INT_WINDOW_TOBACK  3025 /* Send window to back: (window_handle) -> success */
#define EMU_CALL_INT_SET_TITLE     3026  /* Set window title: (window_handle, title_ptr) -> success */
#define EMU_CALL_INT_REFRESH_WINDOW 3027 /* Refresh window: (window_handle, planes_ptr, bpr, depth) */
#define EMU_CALL_INT_GET_ROOTLESS  3028  /* Get rootless mode: () -> bool */

/*
 * Timer Device emucalls (3500-3599)
 *
 * Phase 45: Async I/O & Timer Completion
 * These emucalls manage the host-side timer queue for TR_ADDREQUEST.
 */
#define EMU_CALL_TIMER_ADD         3500  /* Add timer request: d1=ioreq_ptr, d2=delay_secs, d3=delay_micros -> success */
#define EMU_CALL_TIMER_REMOVE      3501  /* Remove timer request: d1=ioreq_ptr -> success */
#define EMU_CALL_TIMER_CHECK       3502  /* Check expired timers: () -> count */
#define EMU_CALL_TIMER_GET_EXPIRED 3503  /* Get next expired timer: () -> ioreq_ptr or 0 */

/*
 * Console Device emucalls (4000-4099)
 */
#define EMU_CALL_CON_READ          4000  /* Read from console: (buf, len) -> bytes_read (-1 if no data yet) */
#define EMU_CALL_CON_INPUT_READY   4001  /* Check if input is ready: () -> bool */

/*
 * Test Infrastructure emucalls (4100-4199)
 *
 * Phase 21: UI Testing Infrastructure
 * These emucalls allow automated testing by injecting input events
 * and capturing screen output programmatically.
 */

/* Input Injection */
#define EMU_CALL_TEST_INJECT_KEY     4100  /* Inject key: d1=rawkey, d2=qualifier, d3=down -> success */
#define EMU_CALL_TEST_INJECT_STRING  4101  /* Inject string as keys: a0=string_ptr -> success */
#define EMU_CALL_TEST_INJECT_MOUSE   4102  /* Inject mouse: d1=(x<<16)|y, d2=buttons, d3=event_type -> success */

/* Screen Capture */
#define EMU_CALL_TEST_CAPTURE_SCREEN 4110  /* Capture screen: a0=filename_ptr, d1=display_handle -> success */
#define EMU_CALL_TEST_CAPTURE_WINDOW 4111  /* Capture window: a0=filename_ptr, d1=window_handle -> success */
#define EMU_CALL_TEST_COMPARE_SCREEN 4112  /* Compare screen to PNG: a0=filename_ptr, d1=display_handle -> similarity (0-100) */

/* Test Control */
#define EMU_CALL_TEST_SET_HEADLESS   4120  /* Set headless mode: d1=enable -> previous_state */
#define EMU_CALL_TEST_GET_HEADLESS   4121  /* Get headless mode: () -> is_headless */
#define EMU_CALL_TEST_WAIT_IDLE      4122  /* Wait for event queue empty: d1=timeout_ms -> success */

/*
 * IEEE Double Precision Math emucalls (5000-5099)
 *
 * Phase 61: mathieeedoubbas.library
 * These emucalls bridge the m68k IEEE double math to host native double.
 * Double values are passed/returned as two 32-bit values: d1=hi, d2=lo
 * For two-operand functions: d1:d2=left, d3:d4=right
 */
#define EMU_CALL_IEEEDP_FIX          5000  /* IEEEDPFix: d1:d2=double -> d0=LONG */
#define EMU_CALL_IEEEDP_FLT          5001  /* IEEEDPFlt: d1=LONG -> d0:d1=double */
#define EMU_CALL_IEEEDP_CMP          5002  /* IEEEDPCmp: d1:d2=left, d3:d4=right -> d0=-1,0,1 */
#define EMU_CALL_IEEEDP_TST          5003  /* IEEEDPTst: d1:d2=double -> d0=-1,0,1 */
#define EMU_CALL_IEEEDP_ABS          5004  /* IEEEDPAbs: d1:d2=double -> d0:d1=double */
#define EMU_CALL_IEEEDP_NEG          5005  /* IEEEDPNeg: d1:d2=double -> d0:d1=double */
#define EMU_CALL_IEEEDP_ADD          5006  /* IEEEDPAdd: d1:d2=left, d3:d4=right -> d0:d1=double */
#define EMU_CALL_IEEEDP_SUB          5007  /* IEEEDPSub: d1:d2=left, d3:d4=right -> d0:d1=double */
#define EMU_CALL_IEEEDP_MUL          5008  /* IEEEDPMul: d1:d2=left, d3:d4=right -> d0:d1=double */
#define EMU_CALL_IEEEDP_DIV          5009  /* IEEEDPDiv: d1:d2=dividend, d3:d4=divisor -> d0:d1=double */
#define EMU_CALL_IEEEDP_FLOOR        5010  /* IEEEDPFloor: d1:d2=double -> d0:d1=double */
#define EMU_CALL_IEEEDP_CEIL         5011  /* IEEEDPCeil: d1:d2=double -> d0:d1=double */

/*
 * IEEE Double Precision Transcendental Math emucalls (5020-5039)
 *
 * Phase 61: mathieeedoubtrans.library
 * Transcendental functions (sin, cos, exp, log, etc.)
 */
#define EMU_CALL_IEEEDP_ATAN         5020  /* IEEEDPAtan: d1:d2=double -> d0:d1=double */
#define EMU_CALL_IEEEDP_SIN          5021  /* IEEEDPSin: d1:d2=double -> d0:d1=double */
#define EMU_CALL_IEEEDP_COS          5022  /* IEEEDPCos: d1:d2=double -> d0:d1=double */
#define EMU_CALL_IEEEDP_TAN          5023  /* IEEEDPTan: d1:d2=double -> d0:d1=double */
#define EMU_CALL_IEEEDP_SINCOS       5024  /* IEEEDPSincos: d1:d2=double, a0=cosptr -> d0:d1=sin */
#define EMU_CALL_IEEEDP_SINH         5025  /* IEEEDPSinh: d1:d2=double -> d0:d1=double */
#define EMU_CALL_IEEEDP_COSH         5026  /* IEEEDPCosh: d1:d2=double -> d0:d1=double */
#define EMU_CALL_IEEEDP_TANH         5027  /* IEEEDPTanh: d1:d2=double -> d0:d1=double */
#define EMU_CALL_IEEEDP_EXP          5028  /* IEEEDPExp: d1:d2=double -> d0:d1=double */
#define EMU_CALL_IEEEDP_LOG          5029  /* IEEEDPLog: d1:d2=double -> d0:d1=double */
#define EMU_CALL_IEEEDP_POW          5030  /* IEEEDPPow: d1:d2=base, d3:d4=exp -> d0:d1=double */
#define EMU_CALL_IEEEDP_SQRT         5031  /* IEEEDPSqrt: d1:d2=double -> d0:d1=double */
#define EMU_CALL_IEEEDP_TIEEE        5032  /* IEEEDPTieee: d1:d2=double -> d0=single */
#define EMU_CALL_IEEEDP_FIEEE        5033  /* IEEEDPFieee: d1=single -> d0:d1=double */
#define EMU_CALL_IEEEDP_ASIN         5034  /* IEEEDPAsin: d1:d2=double -> d0:d1=double */
#define EMU_CALL_IEEEDP_ACOS         5035  /* IEEEDPAcos: d1:d2=double -> d0:d1=double */
#define EMU_CALL_IEEEDP_LOG10        5036  /* IEEEDPLog10: d1:d2=double -> d0:d1=double */

#endif

