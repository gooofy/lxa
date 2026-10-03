
    /* SysFlags */
    .set SFF_QuantumOver, 6

    .set _LVOEnqueue    , -270
    .set _LVOSwitch     , -54
    .set _LVOException  , -66

    /* ExecBase */
    .set ThisTask       , 276
    .set DispCount      , 284
    .set Quantum        , 288
    .set Elapsed        , 290
    .set SysFlags       , 292
    .set IDNestCnt      , 294
    .set TDNestCnt      , 295
    .set TaskReady      , 406

    /* Task structure offsets */
    .set tc_Flags       , 14
    .set tc_State       , 15
    .set tc_SPReg       , 54
    .set tc_Switch      , 66
    .set tc_Launch      , 70

    /* Task flag bits (for btst/bset/bclr on tc_Flags byte) */
    .set TB_EXCEPT      , 5
    .set TB_SWITCH      , 6
    .set TB_LAUNCH      , 7

    /* emucalls */
    .set EMU_CALL_TRACE   , 3
    .set EMU_CALL_WAIT    , 6
    .set EMU_CALL_MONITOR , 8

    /* custom chip registers */
    .set INTENA           , 0xdff09a

    .macro TRACE
    movem.l  d0/d1, -(a7)                           | save d0/d1
    move.l   #EMU_CALL_TRACE, d0                    | #EMU_CALL_TRACE -> d0
    move.l   #0xffffffff, d1                        | #TRUE -> d1
    illegal                                         | emucall
    movem.l (a7)+, d0/d1                            | restore d0/d1
    .endm

    .macro MONITOR
    move.l  d0, -(a7)                               | save d0
    move.l  #EMU_CALL_MONITOR, d0                   | #EMU_CALL_MONITOR -> d0
    illegal                                         | emucall
    move.l (a7)+, d0                                | restore d0
    .endm

    .macro DPUTS l
    movem.l d0-d2, -(a7)
    move.l  #4, d0
    move.l  #1, d1
    move.l  #\l, d2
    illegal
    movem.l (a7)+, d0-d2
    .endm

    /*
     * task scheduler - supervisor mode only!
     */

    .global _exec_Schedule
_exec_Schedule:

    movem.l     d0-d1/a0-a1/a5-a6, -(a7)            | save registers

    | DPUTS       __exec_Schedule_s2                  | "Schedule() called"

    move.l      4, a6                               | SysBase -> a6

    move.w      #0x2700, sr                         | disable interrupts
    move.l      ThisTask(a6), a1                    | SysBase->ThisTask -> a1
    
    /* If ThisTask is NULL (e.g., last task was removed), just exit */
    /* No current task to switch from, Dispatch will pick the next ready task */
    move.l      a1, d0                              | copy a1 to d0 for NULL test
    bne.s       1f                                  | ThisTask != NULL -> continue
    | DPUTS       __exec_Schedule_s5                  | "ThisTask is NULL"
    bra         __exec_Schedule_exit
1:

    /*
     * If ThisTask is NOT running (tc_State != TS_RUN), we're being called
     * from the dispatch idle loop (after VBlank). In this case, don't try
     * to switch - just exit and let the dispatch loop pick up any ready tasks.
     * Trying to Enqueue a non-running task would corrupt the list.
     * tc_State is at offset 15 (0xf) in the Task structure, TS_RUN = 2.
     */
    cmp.b       #2, 0xf(a1)                         | ThisTask->tc_State == TS_RUN?
    beq.s       2f                                  | yes -> continue
    | DPUTS       __exec_Schedule_s6                  | "ThisTask not running"
    bra         __exec_Schedule_exit
2:

    /* If TF_EXCEPT is set, force a reschedule so exceptions are processed */
    btst.b      #TB_EXCEPT, tc_Flags(a1)            | ThisTask->tc_Flags & TF_EXCEPT?
    bne.s       __do_switch                         | yes -> force switch

    /* do we have another task that is ready? */
    lea.l       TaskReady(a6), a0                   | &SysBase->TaskReady -> a0
    cmp.l       8(a0), a0                           | list empty?
    bne.s       3f                                  | not empty -> continue
    | DPUTS       __exec_Schedule_s7                  | "TaskReady empty"
    bra         __exec_Schedule_exit
3:
    move.l      (a0), a0                            | first task in ready list -> a0
    move.b      9(a0), d1                           | ln_Pri -> d1
    cmp.b       9(a1), d1                           | > ThisTask->ln_Pri ?
    bgt.s       __do_switch                         | yes -> __do_switch

    | DPUTS       __exec_Schedule_s4                  | "Schedule() got another task that is ready"

    btst.b      #SFF_QuantumOver, SysFlags(a6)      | time slice expired? (bit #6 in SysBase->SysFlags)
    beq.s       __exec_Schedule_exit                | no -> exit

__do_switch:

    | DPUTS      __exec_Schedule_s1

    move.b      #3, 0xf(a1)                         | Ready -> ThisTask->tc_State (set BEFORE Enqueue)
    lea.l       TaskReady(a6), a0                   | &SysBase->TaskReady -> a0
    jsr         _LVOEnqueue(a6)                     | -> Enqueue() (a1 may be clobbered by C function)
    move.w      #0x2000, sr                         | enable interrupts
    movem.l     (a7)+, d0-d1/a0-a1/a5-a6            | restore registers

    move.l      #_exec_Switch, -(sp)                | push Switch() pc on SP

    rts                                             | -> Switch()

__exec_Schedule_exit:
    | DPUTS       __exec_Schedule_s3                  | "Schedule() exit"

    movem.l     (a7)+, d0-d1/a0-a1/a5-a6            | restore registers
    rte


    .globl _exec_Switch
_exec_Switch:

    /* freeze SysBase->ThisTask */

    move.w   #0x2000, sr                            | enable interrupts
    move.l   a5, -(a7)                              | save a5

    move.l   usp, a5                                | return address from user stack
    movem.l  d0-d7/a0-a6,-(a5)                      | push registers on user stack

    move.l   4, a6                                  | SysBase -> a6
    move.w   IDNestCnt(a6), d0                      | SysBase->IDNestCnt -> d0
    move.w   #0xffff, IDNestCnt(a6)                 | -1 -> SysBase->IDNestCnt

    move.w   #0xC000, INTENA                        | enable interrupts

    move.l   (a7)+, 13*4(a5)                        | restore a5 in user stack
    move.w   (a7)+, -(a5)                           | push SR on user stack
    move.l   (a7)+, -(a5)                           | push return address on user stack

    move.l   ThisTask(a6), a3                       | SysBase->ThisTask -> a3
    move.w   d0, 16(a3)                             | IDNestCnt -> ThisTask->tc_IDNestCnt
    move.l   a5, 54(a3)                             | user stack -> ThisTask->tc_SPReg

    /* Call ThisTask->tc_Switch() if TF_SWITCH is set */
    btst.b   #TB_SWITCH, tc_Flags(a3)               | ThisTask->tc_Flags & TF_SWITCH?
    beq.s    __dispatch                             | no -> skip to dispatch
    move.l   tc_Switch(a3), a5                      | ThisTask->tc_Switch -> a5
    jsr      (a5)                                   | call tc_Switch callback (a6=SysBase)

    .globl _exec_Dispatch
_exec_Dispatch:
    move.l  4, a6                                   | SysBase -> a6
    move.w  #0xffff, IDNestCnt(a6)                  | -1 -> SysBase->IDNestCnt

    move.w   #0xC000, INTENA                        | enable interrupts

__dispatch:
    lea      TaskReady(a6), a0                      | &SysBase->TaskReady -> a0
1:
    move.w   #0x2700, sr                            | disable cpu interrupts
    move.l   (a0), a3                               | first task in ready list -> a3
    move.l   (a3), d0                               | ->ln_Succ -> d0
    bne.s    2f                                     | not empty -> launch this task

    move.w   #0x2000, sr                            | enable cpu interrupts to allow VBlank
    move.l   #EMU_CALL_WAIT, d0                     | #EMU_CALL_WAIT -> d0
    illegal                                         | emucall

    bra.s    1b                                     | loop until we got a task that is ready

2:
    move.l   d0, (a0)                               | remove task from ready list
    move.l   d0, a1                                 |
    move.l   a0, 4(a1)                              | 4: ln_Pred

    addq.l   #1, DispCount(a6)                      | increment SysBase->DispCount
    move.l   a3, ThisTask(a6)                       | a3 -> SysBase->ThisTask
    move.l   Quantum(a6), Elapsed(a6)               | SysBase->Quantum -> SysBase->Elapsed
    bclr.b   #SFF_QuantumOver, SysFlags(a6)         | clear QuantumOver bit in SysBase->SysFlags
    move.b   #2, 15(a3)                             | 'Run' -> ThisTask->tc_State
    move.w   16(a3), IDNestCnt(a6)                  | tc_IDNestCnt/tc_TDNestCnt -> SysBase->IDNestCnt/TDNestCnt

    /* apply IDNestCnt */
    tst.b    IDNestCnt(a6)                          | SysBase->IDNestCnt (disable) set?
    bmi.s    3f                                     | no -> skip

    move.w   #0x4000, INTENA                        | disable INTENA interrupts

3:
    move     #0x2000, sr                            | enable cpu interrupts

    /* Call ThisTask->tc_Launch() if TF_LAUNCH is set */
    btst.b   #TB_LAUNCH, tc_Flags(a3)               | ThisTask->tc_Flags & TF_LAUNCH?
    beq.s    4f                                     | no -> skip
    move.l   tc_Launch(a3), a5                      | ThisTask->tc_Launch -> a5
    jsr      (a5)                                   | call tc_Launch callback (a6=SysBase)
4:
    /* Handle TF_EXCEPT: if exception flag is set, call Exception() */
    btst.b   #TB_EXCEPT, tc_Flags(a3)               | ThisTask->tc_Flags & TF_EXCEPT?
    beq.s    5f                                     | no -> skip
    jsr      _LVOException(a6)                      | call Exception() (LVO -66)
5:

    move.l   54(a3), a5                             | ThisTask->tc_SPReg -> a5
    lea      2+16*4(a5), a2                         | setup return address -> a2
    move.l   a2, usp                                | a2 -> USP
    move.l   (a5)+, -(sp)                           | push pc on SSP
    move.w   (a5)+, -(sp)                           | push sr on SSP
    movem.l  (a5), d0-d7/a0-a6                      | restore regs
    rte                                             | this should return to ThisTask now

    .global _handleIRQ3

_handleIRQ3:

    movem.l     d0-d2/a0-a2/a6, -(sp)           | save registers (more for C call)

    | DPUTS       __exec_handleIRQ3_s1                | "_handleIRQ3() called"

    move.l      4, a6

    /* Process input events via Intuition hook */
    /* This ensures IDCMP messages are delivered even when app doesn't use WaitTOF */
    jsr         __intuition_VBlankInputHook

    /* Process expired timer requests via Timer hook */
    /* This calls ReplyMsg() for expired timers, properly waking up waiting tasks */
    jsr         __timer_VBlankHook

    /* Process audio playback timing and end-of-sample interrupts */
    jsr         __audio_VBlankHook

    /* Process pending console reads via Console hook (Phase 45: async I/O) */
    /* This completes async CMD_READ requests when input becomes available */
    jsr         __console_VBlankHook

    /* Process DOS notify requests after timer/input updates */
    jsr         __dos_NotifyVBlankHook

    /* Wake tasks blocked in WaitTOF()/WaitBOVP() */
    jsr         __graphics_VBlankHook

    /* Run the INTB_VERTB interrupt server chain (AddIntServer) */
    jsr         __exec_VBlankServers

    move.l      4, a6                               | restore a6 (C call may have changed it)

    /* count down current task's time slice */
    move.w      Elapsed(a6), d0                     | SysBase->Elapsed -> d0
    beq.s       1f                                  | Elapsed==0 ? -> goto 1:
    subq.w      #1, d0                              | Elapsed--
    move.w      d0, Elapsed(a6)
    bra.s       2f                                  | continue

1:
    bset        #SFF_QuantumOver, SysFlags(a6)      | set QuantumOver bit SysFlags(a6) for scheduler

2:
    tst.b       TDNestCnt(a6)                       | multitasking disabled?
    bge.s       3f                                  | yes -> skip Schedule()

    | DPUTS       __exec_handleIRQ3_s2                | "_handleIRQ3() -> Schedule()"
    movem.l     (sp)+, d0-d2/a0-a2/a6               | restore registers
    bra         _exec_Schedule                      | jump into our scheduler

3:
    movem.l     (sp)+, d0-d2/a0-a2/a6               | restore registers
    rte

    .globl _exec_Supervisor
_exec_Supervisor:
    ori.w       #0x2000, sr                         | this will cause a priv viol exception if we're not in supervisor mode already

    /* we were in supervisor mode already -> create a fake exception stack frame */
    move.l      #1f, -(a7)                          | push return address on stack
    move.w      sr, -(a7)                           | push sr on stack
    jmp         (a5)                                | call user function to be executed in supervisor mode

1:  rts

    /*
     * CPU exceptions and TRAP #0-#14 (AmigaOS convention, RKRM Exec "Traps"):
     * the exception number (longword) is pushed on the supervisor stack on
     * top of the exception frame and control passes to ThisTask->tc_TrapCode
     * (exec's default when there is no task or no handler).  A handler
     * removes the number (addq.l #4,sp) and returns with RTE.  All registers
     * are as they were at the fault.
     */
    .set tc_TrapCode, 50
    .set TaskTrapCode, 304

    .globl _handleVec02
_handleVec02:
    move.l      #2, -(a7)                           | exception number on top of the frame
    bra         _dispatchTrap

    .globl _handleVec03
_handleVec03:
    move.l      #3, -(a7)                           | exception number on top of the frame
    bra         _dispatchTrap

    .globl _handleVec04
_handleVec04:
    move.l      #4, -(a7)                           | exception number on top of the frame
    bra         _dispatchTrap

    .globl _handleVec05
_handleVec05:
    move.l      #5, -(a7)                           | exception number on top of the frame
    bra         _dispatchTrap

    .globl _handleVec06
_handleVec06:
    move.l      #6, -(a7)                           | exception number on top of the frame
    bra         _dispatchTrap

    .globl _handleVec07
_handleVec07:
    move.l      #7, -(a7)                           | exception number on top of the frame
    bra         _dispatchTrap

    .globl _handleVec08
_handleVec08:
    cmp.l       #_exec_Supervisor, 2(a7)            | was this caused by a Supervisor() call?
    bne.s       2f                                  | no -> regular exception
    move.l      #1f, 2(a7)                          | fix return address in exception stack frame
    jmp         (a5)                                | call user function to be executed in supervisor mode
1:
    rts
2:
    move.l      #8, -(a7)
    bra         _dispatchTrap

    .globl _handleVec09
_handleVec09:
    move.l      #9, -(a7)                           | exception number on top of the frame
    bra         _dispatchTrap

    .globl _handleVec10
_handleVec10:
    move.l      #10, -(a7)                           | exception number on top of the frame
    bra         _dispatchTrap

    .globl _handleVec11
_handleVec11:
    move.l      #11, -(a7)                           | exception number on top of the frame
    bra         _dispatchTrap

    .globl _handleTrap0
_handleTrap0:
    move.l      #32, -(a7)
    bra         _dispatchTrap

    .globl _handleTrap1
_handleTrap1:
    move.l      #33, -(a7)
    bra         _dispatchTrap

    .globl _handleTrap2
_handleTrap2:
    move.l      #34, -(a7)
    bra         _dispatchTrap

    .globl _handleTrap3
_handleTrap3:
    move.l      #35, -(a7)
    bra         _dispatchTrap

    .globl _handleTrap4
_handleTrap4:
    move.l      #36, -(a7)
    bra         _dispatchTrap

    .globl _handleTrap5
_handleTrap5:
    move.l      #37, -(a7)
    bra         _dispatchTrap

    .globl _handleTrap6
_handleTrap6:
    move.l      #38, -(a7)
    bra         _dispatchTrap

    .globl _handleTrap7
_handleTrap7:
    move.l      #39, -(a7)
    bra         _dispatchTrap

    .globl _handleTrap8
_handleTrap8:
    move.l      #40, -(a7)
    bra         _dispatchTrap

    .globl _handleTrap9
_handleTrap9:
    move.l      #41, -(a7)
    bra         _dispatchTrap

    .globl _handleTrap10
_handleTrap10:
    move.l      #42, -(a7)
    bra         _dispatchTrap

    .globl _handleTrap11
_handleTrap11:
    move.l      #43, -(a7)
    bra         _dispatchTrap

    .globl _handleTrap12
_handleTrap12:
    move.l      #44, -(a7)
    bra         _dispatchTrap

    .globl _handleTrap13
_handleTrap13:
    move.l      #45, -(a7)
    bra         _dispatchTrap

    .globl _handleTrap14
_handleTrap14:
    move.l      #46, -(a7)
    bra         _dispatchTrap

_dispatchTrap:
    subq.l      #4, a7                              | slot for the handler address
    move.l      a0, -(a7)
    move.l      4, a0                               | SysBase
    move.l      ThisTask(a0), a0
    cmp.w       #0, a0
    beq.s       1f
    move.l      tc_TrapCode(a0), a0
    cmp.w       #0, a0
    bne.s       2f
1:  move.l      4, a0
    move.l      TaskTrapCode(a0), a0
    cmp.w       #0, a0
    bne.s       2f
    lea         __exec_DefaultTrapCode, a0
2:  move.l      a0, 4(a7)                           | handler -> slot
    move.l      (a7)+, a0
    rts                                             | "return" into the handler: stack = [number][frame]

    /*
     * exec's default trap handler (SysBase->TaskTrapCode): a CPU exception in
     * a task is a "Software Failure": the host logs it, and a task that
     * faulted in user mode is held (it continues in _exec_TaskHeld, which
     * never returns) while all other tasks keep running; this includes a
     * fault in supervisor mode (a dead-end alert on AmigaOS).  Without a
     * current task the fault is logged and execution continues after RTE.
     * Stack on entry: [number.l] [SR.w] [PC.l] ...
     */
    .globl __exec_DefaultTrapCode
__exec_DefaultTrapCode:
    movem.l     d0/d1, -(a7)
    move.l      8(a7), d1                           | exception number
    move.l      #5, d0                              | EMU_CALL_EXCEPTION
    illegal                                         | emucall (host log + exception record)
    movem.l     (a7)+, d0/d1
    move.l      a0, -(a7)
    move.l      4, a0
    tst.l       ThisTask(a0)                        | no task (boot): just return
    move.l      (a7)+, a0
    beq.s       1f
    btst        #5, 4(a7)                           | supervisor mode ...
    beq.s       2f
    btst        #0, 4(a7)                           | ... with an interrupt level set: an
    bne.s       1f                                  | interrupt handler faulted - a dead
    btst        #1, 4(a7)                           | end (Guru) on AmigaOS; the host
    bne.s       1f                                  | stops the run
    btst        #2, 4(a7)
    bne.s       1f
2:
    move.l      a0, -(a7)                           | the task's SP may be garbage: hold it
    move.l      4, a0                               | on the top of its own allocated stack
    move.l      ThisTask(a0), a0
    move.l      62(a0), a0                          | tc_SPUpper
    cmp.w       #0, a0
    beq.s       4f
    lea         -64(a0), a0
    move.l      a0, usp
4:  move.l      (a7)+, a0
    move.l      #__exec_TaskHeld, 6(a7)              | resume the task in _exec_TaskHeld
    andi.w      #0x1fff, 4(a7)                      | ... in user mode, without trace bits
                                                    | (a supervisor-mode fault is a dead end
                                                    | on AmigaOS; lxa holds the task instead)
1:  addq.l      #4, a7                              | pop the exception number
    rte

	.globl _exec_SetSR
_exec_SetSR:
        move.l  a5, a0                              | save a5
        lea     _super_SetSR(PC), a5                | writing to SR requires superuser privileges
        jmp     -30(a6)                             | Supervisor()

_super_SetSR:
        move.l  a0, a5
        move    (sp), a0                            | preserve oldSR
        and     d1, d0                              | d0: bits to set
        not     d1                                  | d1: inverted bit mask
        and     d1, (sp)                            | clear bits about to be set
        or      d0, (sp)                            | set them
        clr.l   d0
        move.w  a0, d0                              | result: oldSR -> d0
        rte                                         | done

    .data

    .align 4
__exec_Schedule_s1:
    .asciz  "\n\n_exec_Schedule: Switching tasks!\n\n"

    .align 4
__exec_Schedule_s2:
    .asciz  "_exec_Schedule: Schedule() called\n"

    .align 4
__exec_Schedule_s3:
    .asciz  "_exec_Schedule: Schedule() exit\n"

    .align 4
__exec_Schedule_s4:
    .asciz  "_exec_Schedule: Schedule() got another task that is ready\n"

    .align 4
__exec_Schedule_s5:
    .asciz  "_exec_Schedule: ThisTask is NULL\n"

    .align 4
__exec_Schedule_s6:
    .asciz  "_exec_Schedule: ThisTask not running\n"

    .align 4
__exec_Schedule_s7:
    .asciz  "_exec_Schedule: TaskReady empty\n"

    .align 4
__exec_handleIRQ3_s1:
    .asciz "_handleIRQ3() called\n"

    .align 4
__exec_handleIRQ3_s2:
    .asciz "_handleIRQ3() -> Schedule()\n"
