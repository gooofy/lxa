#ifndef HAVE_EXCEPTIONS_H
#define HAVE_EXCEPTIONS_H

void  __saveds exec_Schedule   ( register struct ExecBase * __libBase __asm("a6"));
void  __saveds exec_Switch     ( register struct ExecBase * __libBase __asm("a6"));
void  __saveds exec_Dispatch   ( register struct ExecBase * __libBase __asm("a6"));
ULONG __saveds exec_Supervisor ( register struct ExecBase * __libBase __asm("a6"), register VOID (*__fpt)() __asm("a5"));

void  __saveds handleVec08     (void);
void  __saveds handleIRQ3      (void);

#endif
