#ifndef HAVE_MEM_MAP
#define HAVE_MEM_MAP

#include <exec/execbase.h>

#include "dos/dos.h"
#include "dos/dosextens.h"

#include <utility/utility.h>

#define NUM_EXEC_FUNCS 130

// memory map
#define EXCEPTION_VECTORS_START      0
#define EXCEPTION_VECTORS_SIZE       1024
#define EXCEPTION_VECTORS_END        EXCEPTION_VECTORS_START + EXCEPTION_VECTORS_SIZE - 1

// exec

#define EXEC_VECTORS_START           EXCEPTION_VECTORS_END + 1
#define EXEC_VECTORS_SIZE            NUM_EXEC_FUNCS * 6
#define EXEC_VECTORS_END             EXEC_VECTORS_START + EXEC_VECTORS_SIZE -1

#define EXEC_BASE_START              EXEC_VECTORS_END + 1
#define EXEC_BASE_SIZE               sizeof (struct ExecBase)
#define EXEC_BASE_END                EXEC_BASE_START + EXEC_BASE_SIZE -1

#define EXEC_MH_START                EXEC_BASE_END + 1
#define EXEC_MH_SIZE                 sizeof (struct MemHeader)
#define EXEC_MH_END                  EXEC_MH_START + EXEC_MH_SIZE -1

// utility

#define NUM_UTILITY_FUNCS            (45+4)
#define UTILITY_VECTORS_START        EXEC_MH_END + 1
#define UTILITY_VECTORS_SIZE         NUM_UTILITY_FUNCS * 6
#define UTILITY_VECTORS_END          UTILITY_VECTORS_START + UTILITY_VECTORS_SIZE -1

#define UTILITY_BASE_START           UTILITY_VECTORS_END + 1
#define UTILITY_BASE_SIZE            sizeof (struct UtilityBase)
#define UTILITY_BASE_END             UTILITY_BASE_START + UTILITY_BASE_SIZE -1

// dos

#define NUM_DOS_FUNCS                (162+4)
#define DOS_VECTORS_START            UTILITY_BASE_END + 1
#define DOS_VECTORS_SIZE             NUM_DOS_FUNCS * 6
#define DOS_VECTORS_END              DOS_VECTORS_START + DOS_VECTORS_SIZE -1

#define DOS_BASE_START               DOS_VECTORS_END + 1
#define DOS_BASE_SIZE                sizeof (struct DosLibrary)
#define DOS_BASE_END                 DOS_BASE_START + DOS_BASE_SIZE -1

// mathffp

#define NUM_MATHFFP_FUNCS            (12+4)
#define MATHFFP_VECTORS_START        DOS_BASE_END + 1
#define MATHFFP_VECTORS_SIZE         NUM_MATHFFP_FUNCS * 6
#define MATHFFP_VECTORS_END          MATHFFP_VECTORS_START + MATHFFP_VECTORS_SIZE -1

#define MATHFFP_BASE_START           MATHFFP_VECTORS_END + 1
#define MATHFFP_BASE_SIZE            sizeof (struct Library)
#define MATHFFP_BASE_END             MATHFFP_BASE_START + MATHFFP_BASE_SIZE -1

// mathtrans

#define NUM_MATHTRANS_FUNCS          (17+4)
#define MATHTRANS_VECTORS_START      MATHFFP_BASE_END + 1
#define MATHTRANS_VECTORS_SIZE       NUM_MATHTRANS_FUNCS * 6
#define MATHTRANS_VECTORS_END        MATHTRANS_VECTORS_START + MATHTRANS_VECTORS_SIZE -1

#define MATHTRANS_BASE_START         MATHTRANS_VECTORS_END + 1
#define MATHTRANS_BASE_SIZE          sizeof (struct Library)
#define MATHTRANS_BASE_END           MATHTRANS_BASE_START + MATHTRANS_BASE_SIZE -1

// input.device

#define NUM_DEVICE_INPUT_FUNCS       (0+6)
#define DEVICE_INPUT_VECTORS_START   MATHTRANS_BASE_END + 1
#define DEVICE_INPUT_VECTORS_SIZE    NUM_DEVICE_INPUT_FUNCS * 6
#define DEVICE_INPUT_VECTORS_END     DEVICE_INPUT_VECTORS_START + DEVICE_INPUT_VECTORS_SIZE -1

#define DEVICE_INPUT_BASE_START      DEVICE_INPUT_VECTORS_END + 1
#define DEVICE_INPUT_BASE_SIZE       sizeof (struct Library)
#define DEVICE_INPUT_BASE_END        DEVICE_INPUT_BASE_START + DEVICE_INPUT_BASE_SIZE -1

// RAM

#define RAM_START                    ALIGN (DEVICE_INPUT_BASE_END + 1, 4)
#define RAM_SIZE                     10 * 1024 * 1024
#define RAM_END                      RAM_SIZE - 1

extern struct Resident *__lxa_dos_ROMTag;
extern struct Resident *__lxa_utility_ROMTag;
extern struct Resident *__lxa_mathffp_ROMTag;
extern struct Resident *__lxa_mathtrans_ROMTag;
extern struct Resident *__lxa_input_ROMTag;

#endif
