#ifndef HAVE_EXCEPTIONS_H
#define HAVE_EXCEPTIONS_H

void  exec_Schedule   ( register struct ExecBase * __libBase __asm("a6"));
void  exec_Switch     ( register struct ExecBase * __libBase __asm("a6"));
void  exec_Dispatch   ( register struct ExecBase * __libBase __asm("a6"));
ULONG exec_Supervisor ( register struct ExecBase * __libBase __asm("a6"), register VOID (*__fpt)() __asm("a5"));
ULONG exec_SetSR      ( register struct ExecBase * SysBase __asm("a6"), register ULONG newSR __asm("d0"), register ULONG mask __asm("d1"));

void  handleVec02     ( void );
void  handleVec03     ( void );
void  handleVec04     ( void );
void  handleVec05     ( void );
void  handleVec06     ( void );
void  handleVec07     ( void );
void  handleVec09     ( void );
void  handleVec10     ( void );
void  handleVec11     ( void );

void  handleVec08     ( void );
void  handleIRQ3      ( void );

#endif
