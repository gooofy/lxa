#ifndef HAVE_MEM_MAP
#define HAVE_MEM_MAP

#include <exec/execbase.h>

#include "dos/dos.h"
#include "dos/dosextens.h"

#define NUM_EXEC_FUNCS 130

// memory map
#define EXCEPTION_VECTORS_START 0
#define EXCEPTION_VECTORS_SIZE  1024
#define EXCEPTION_VECTORS_END   EXCEPTION_VECTORS_START + EXCEPTION_VECTORS_SIZE - 1

// exec

#define EXEC_VECTORS_START      EXCEPTION_VECTORS_END + 1
#define EXEC_VECTORS_SIZE       NUM_EXEC_FUNCS * 6
#define EXEC_VECTORS_END        EXEC_VECTORS_START + EXEC_VECTORS_SIZE -1

#define EXEC_BASE_START         EXEC_VECTORS_END + 1
#define EXEC_BASE_SIZE          sizeof (struct ExecBase)
#define EXEC_BASE_END           EXEC_BASE_START + EXEC_BASE_SIZE -1

#define EXEC_MH_START           EXEC_BASE_END + 1
#define EXEC_MH_SIZE            sizeof (struct MemHeader)
#define EXEC_MH_END             EXEC_MH_START + EXEC_MH_SIZE -1

// dos

#define NUM_DOS_FUNCS           31
#define DOS_VECTORS_START       EXEC_MH_END + 1
#define DOS_VECTORS_SIZE        NUM_DOS_FUNCS * 6
#define DOS_VECTORS_END         DOS_VECTORS_START + DOS_VECTORS_SIZE -1

#define DOS_BASE_START          DOS_VECTORS_END + 1
#define DOS_BASE_SIZE           sizeof (struct DosLibrary)
#define DOS_BASE_END            DOS_BASE_START + DOS_BASE_SIZE -1

#define RAM_START               DOS_BASE_END + 1
#define RAM_SIZE                10 * 1024 * 1024
#define RAM_END                 RAM_START + RAM_SIZE - 1

#endif
