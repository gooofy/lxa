/*
 * rexx_packet.h - a one-shot packet to the REXX port for the Rexxc
 * commands RXC, TCO and TCC (Phase 250).
 *
 * Copyright (C) 2026 by G. Bartsch. Licensed under the MIT License.
 */
#ifndef LXA_REXX_PACKET_H
#define LXA_REXX_PACKET_H

/* returns the packet's rc, or -1 when ARexx is not active */
static LONG rexx_packet(LONG action)
{
    struct MsgPort *reply;
    struct RexxMsg *rm;
    LONG rc = -1;

    RexxSysBase = (struct RxsLib *)OpenLibrary((STRPTR)"rexxsyslib.library", 36);
    if (!RexxSysBase)
        return -1;
    reply = CreateMsgPort();
    rm = reply ? CreateRexxMsg(reply, NULL, NULL) : NULL;
    if (rm) {
        rm->rm_Action = action;
        if (lxa_rexx_send(rm, reply))
            rc = rm->rm_Result1;
        DeleteRexxMsg(rm);
    }
    if (reply)
        DeleteMsgPort(reply);
    CloseLibrary((struct Library *)RexxSysBase);
    return rc;
}

#endif
