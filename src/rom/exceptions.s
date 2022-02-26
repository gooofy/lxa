
    /* SysFlags */
    .set SFF_QuantumOver, 6

    .set _LVOEnqueue    , -270
    .set _LVOSwitch     , -54

    /* ExecBase */
    .set ThisTask       , 276
    .set DispCount      , 284
    .set Quantum        , 288
    .set Elapsed        , 290
    .set SysFlags       , 292
    .set IDNestCnt      , 294
    .set TDNestCnt      , 295
    .set TaskReady      , 406

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

    DPUTS       __exec_Schedule_s2                  | "Schedule() called"

    move.l      4, a6                               | SysBase -> a6

    move.w      #0x2700, sr                         | disable interrupts
    /* FIXME: Sysflags */
    move.l      ThisTask(a6), a1                    | SysBase->ThisTask -> a1

    /* FIXME: check for exception flags */

    /* do we have another task that is ready? */
    lea.l       TaskReady(a6), a0                   | &SysBase->TaskReady -> a0
    cmp.l       8(a0), a0                           | list empty?
    beq.s       __exec_Schedule_exit                | yes -> exit

    move.l      (a0), a0                            | first task in ready list -> a0
    move.b      9(a0), d1                           | ln_Pri -> d1
    cmp.b       9(a1), d1                           | > ThisTask->ln_Pri ?
    bgt.s       __do_switch                         | yes -> __do_switch

    DPUTS       __exec_Schedule_s4                  | "Schedule() got another task that is ready"

    btst.b      #SFF_QuantumOver, SysFlags(a6)      | time slice expired? (bit #6 in SysBase->SysFlags)
    beq.s       __exec_Schedule_exit                | no -> exit

__do_switch:

    | DPUTS      __exec_Schedule_s1

    lea.l       TaskReady(a6), a0                   | &SysBase->TaskReady -> a0
    jsr         _LVOEnqueue(a6)                     | -> Enqueue()
    move.b      #3, 0xf(a1)                         | Ready -> ThisTask->tc_State
    move.w      #0x2000, sr                         | enable interrupts
    movem.l     (a7)+, d0-d1/a0-a1/a5-a6            | restore registers

    move.l      #_exec_Switch, -(sp)                | push Switch() pc on SP

    rts                                             | -> Switch()

__exec_Schedule_exit:
    DPUTS       __exec_Schedule_s3                  | "Schedule() exit"

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
    move.w   0xffff, IDNestCnt(a6)                  | -1 -> SysBase->IDNestCnt

    move.w   #0xC000, INTENA                        | enable interrupts

    move.l   (a7)+, 13*4(a5)                        | restore a5 in user stack
    move.w   (a7)+, -(a5)                           | push SR on user stack
    move.l   (a7)+, -(a5)                           | push return address on user stack

    move.l   ThisTask(a6), a3                       | SysBase->ThisTask -> a3
    move.w   d0, 16(a3)                             | IDNestCnt -> ThisTask->tc_IDNestCnt
    move.l   a5, 54(a3)                             | user stack -> ThisTask->tc_SPReg

    /* FIXME: call ThisTask->tc_Switch() if set */

    bra.s   __dispatch

    .globl _exec_Dispatch
_exec_Dispatch:
    /* FIXME lea.l   ... */
    move.w  #0xffff, IDNestCnt(a6)                  | -1 -> SysBase->IDNestCnt

    move.w   #0xC000, INTENA                        | enable interrupts

__dispatch:
    lea      TaskReady(a6), a0                      | &SysBase->TaskReady -> a0
1:
    move.w   #0x2700, sr                            | disable cpu interrupts
    move.l   (a0), a3                               | first task in ready list -> a3
    move.l   (a3), d0                               | ->ln_Succ -> d0
    bne.s    2f                                     | not empty -> launch this task

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

    /* FIXME: handle task launch / exception (check tc_Flags) */

    move.l   54(a3), a5                             | ThisTask->tc_SPReg -> a5
    lea      2+16*4(a5), a2                         | setup return address -> a2
    move.l   a2, usp                                | a2 -> USP
    move.l   (a5)+, -(sp)                           | push pc on SSP
    move.w   (a5)+, -(sp)                           | push sr on SSP
    movem.l  (a5), d0-d7/a0-a6                      | restore regs
    rte                                             | this should return to ThisTask now

    .global _handleIRQ3

_handleIRQ3:

    movem.l     d0/a6, -(sp)                        | save registers

    DPUTS       __exec_handleIRQ3_s1                | "_handleIRQ3() called"

    move.l      4, a6

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

    DPUTS       __exec_handleIRQ3_s2                | "_handleIRQ3() -> Schedule()"
    movem.l     (sp)+, d0/a6                        | restore registers
    bra         _exec_Schedule                      | jump into our scheduler

3:
    movem.l     (sp)+, d0/a6                        | restore registers
    rte

    .globl _exec_Supervisor
_exec_Supervisor:
    ori.w       #0x2000, sr                         | this will cause a priv viol exception if we're not in supervisor mode already

    /* we were in supervisor mode already -> create a fake exception stack frame */
    move.l      #1f, -(a7)                          | push return address on stack
    move.w      sr, -(a7)                           | push sr on stack
    jmp         (a5)                                | call user function to be executed in supervisor mode

1:  rts

    /* privilege violation (may be caused by Supervisor()) */
    .globl _handleVec08
_handleVec08:
    cmp.l       #_exec_Supervisor, 2(a7)            | was this caused by a Supervisor() call?
    bne.s       2f                                  | no -> regular exception handler
    move.l      #1f, 2(a7)                          | fix return address in exception stack frame
    jmp         (a5)                                | call user function to be executed in supervisor mode
1:
    rts

2:
    movem.l     d0/d1, -(a7)                        | save registers
    move.l      #5, d0                              | EMU_CALL_EXCEPTION
    move.l      #8, d1                              | vector number
    illegal                                         | emucall
    movem.l     (a7)+, d0/d1;                       | restore registers
    rte

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
__exec_handleIRQ3_s1:
    .asciz "_handleIRQ3() called\n"

    .align 4
__exec_handleIRQ3_s2:
    .asciz "_handleIRQ3() -> Schedule()\n"

