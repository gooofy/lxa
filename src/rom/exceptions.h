#ifndef HAVE_EXCEPTIONS_H
#define HAVE_EXCEPTIONS_H

void __saveds exec_Schedule ( register struct ExecBase * __libBase __asm("a6"));
void __saveds exec_Switch   ( register struct ExecBase * __libBase __asm("a6"));
void __saveds exec_Dispatch ( register struct ExecBase * __libBase __asm("a6"));

void handleIRQ3 (void);

#endif
