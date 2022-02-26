#ifndef HAVE_EXCEPTIONS_H
#define HAVE_EXCEPTIONS_H

void  __saveds exec_Schedule   ( register struct ExecBase * __libBase __asm("a6"));
void  __saveds exec_Switch     ( register struct ExecBase * __libBase __asm("a6"));
void  __saveds exec_Dispatch   ( register struct ExecBase * __libBase __asm("a6"));
ULONG __saveds exec_Supervisor ( register struct ExecBase * __libBase __asm("a6"), register VOID (*__fpt)() __asm("a5"));

void  __saveds handleVec02     ( void );
void  __saveds handleVec03     ( void );
void  __saveds handleVec04     ( void );
void  __saveds handleVec05     ( void );
void  __saveds handleVec06     ( void );
void  __saveds handleVec07     ( void );
void  __saveds handleVec09     ( void );
void  __saveds handleVec10     ( void );
void  __saveds handleVec11     ( void );

void  __saveds handleVec08     ( void );
void  __saveds handleIRQ3      ( void );

#endif
