/*
 * app_close.h - shut down an app a launch test started (Phase 220)
 *
 * A launch test must not leave the app running: on the AmigaOS reference the
 * next program would see its window, and lxa only stops once every task has
 * exited.  close_app() closes the app the way a user would, by delivering an
 * IDCMP_CLOSEWINDOW message (the close gadget) to its window, then waits for
 * the process to exit.
 *
 * Needs: print(), SysBase, IntuitionBase, <intuition/intuitionbase.h>.
 */

#ifndef APP_CLOSE_H
#define APP_CLOSE_H

static BOOL app_task_alive(struct Task *t)
{
    struct Node *n;
    BOOL found = FALSE;

    Forbid();
    if (SysBase->ThisTask == t)
        found = TRUE;
    for (n = SysBase->TaskReady.lh_Head; !found && n->ln_Succ; n = n->ln_Succ)
        if ((struct Task *)n == t)
            found = TRUE;
    for (n = SysBase->TaskWait.lh_Head; !found && n->ln_Succ; n = n->ln_Succ)
        if ((struct Task *)n == t)
            found = TRUE;
    Permit();
    return found;
}

static struct Window *app_find_window(struct Task *t)
{
    struct Screen *scr;
    struct Window *win;
    struct Window *hit = NULL;
    ULONG lock = LockIBase(0);

    for (scr = IntuitionBase->FirstScreen; scr && !hit; scr = scr->NextScreen)
        for (win = scr->FirstWindow; win && !hit; win = win->NextWindow)
            if (win->UserPort && win->UserPort->mp_SigTask == t &&
                (win->IDCMPFlags & IDCMP_CLOSEWINDOW))
                hit = win;
    UnlockIBase(lock);
    return hit;
}

/* returns 1 when the app exited */
static int close_app(struct Process *proc)
{
    struct MsgPort *reply;
    struct IntuiMessage *im;
    struct Window *win;
    int ticks;
    BOOL replied = FALSE;

    win = app_find_window(&proc->pr_Task);
    {   /* DIAG */
        struct Screen *scr; struct Window *w; struct Gadget *g;
        Delay(100);
        for (scr = IntuitionBase->FirstScreen; scr; scr = scr->NextScreen)
            for (w = scr->FirstWindow; w; w = w->NextWindow) {
                struct Task *t = w->UserPort ? (struct Task *)w->UserPort->mp_SigTask : NULL;
                if (t != &proc->pr_Task) continue;
                print("DIAG win\n");
                for (g = w->FirstGadget; g; g = g->NextGadget) {
                    print_num("DIAG gad id=", g->GadgetID, "");
                    print_num(" type=", g->GadgetType, "");
                    print_num(" flags=", g->Flags, "");
                    print_num(" act=", g->Activation, " text=");
                    if (g->GadgetText && !(g->Flags & GFLG_LABELMASK) && g->GadgetText->IText) print((char*)g->GadgetText->IText);
                    print("\n");
                }
            }
    }
    if (!win)
    {
        print("FAIL: no app window with IDCMP_CLOSEWINDOW\n");
        return 0;
    }
    reply = CreateMsgPort();
    if (!reply)
        return 0;
    im = (struct IntuiMessage *)AllocMem(sizeof(struct IntuiMessage), MEMF_PUBLIC | MEMF_CLEAR);
    if (!im)
    {
        DeleteMsgPort(reply);
        return 0;
    }

    im->ExecMessage.mn_Node.ln_Type = NT_MESSAGE;
    im->ExecMessage.mn_ReplyPort = reply;
    im->ExecMessage.mn_Length = sizeof(struct IntuiMessage);
    im->Class = IDCMP_CLOSEWINDOW;
    im->IDCMPWindow = win;
    im->MouseX = win->MouseX;
    im->MouseY = win->MouseY;
    CurrentTime(&im->Seconds, &im->Micros);
    PutMsg(win->UserPort, (struct Message *)im);

    for (ticks = 0; ticks < 500; ticks += 5)
    {
        Delay(5);
        if (!replied && GetMsg(reply))
            replied = TRUE;
        if (replied && !app_task_alive(&proc->pr_Task))
            break;
    }
    if (!replied)
    {
        /* never free a message the app may still hold */
        print("FAIL: app did not reply to IDCMP_CLOSEWINDOW\n");
        return 0;
    }
    FreeMem(im, sizeof(struct IntuiMessage));
    DeleteMsgPort(reply);
    if (app_task_alive(&proc->pr_Task))
    {
        print("FAIL: app still running after IDCMP_CLOSEWINDOW\n");
        return 0;
    }
    print("OK: app quit after IDCMP_CLOSEWINDOW\n");
    return 1;
}

#endif
