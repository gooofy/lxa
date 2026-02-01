/*
 * lxa translator.library implementation
 *
 * Converts text to phonemes for the Amiga speech synthesizer.
 * This is a stub implementation that just copies input to output.
 */

#include <exec/types.h>
#include <exec/memory.h>
#include <exec/execbase.h>
#include <exec/resident.h>
#include <exec/initializers.h>
#include <clib/exec_protos.h>
#include <inline/exec.h>

#include "util.h"

#define VERSION    44
#define REVISION   1
#define EXLIBNAME  "translator"
#define EXLIBVER   " 44.1 (2025/02/01)"

char __aligned _g_translator_ExLibName [] = EXLIBNAME ".library";
char __aligned _g_translator_ExLibID   [] = EXLIBNAME EXLIBVER;
char __aligned _g_translator_Copyright [] = "(C)opyright 2025 by G. Bartsch. Licensed under the MIT license.";

char __aligned _g_translator_VERSTRING [] = "\0$VER: " EXLIBNAME EXLIBVER;

extern struct ExecBase *SysBase;

/* TranslatorBase structure */
struct TranslatorBase {
    struct Library lib;
    UWORD          Pad;
    BPTR           SegList;
};

/****************************************************************************/
/* Library management functions                                              */
/****************************************************************************/

struct TranslatorBase * __g_lxa_translator_InitLib ( register struct TranslatorBase *trb     __asm("a6"),
                                                      register BPTR                  seglist __asm("a0"),
                                                      register struct ExecBase       *sysb   __asm("d0"))
{
    DPRINTF (LOG_DEBUG, "_translator: InitLib() called\n");
    trb->SegList = seglist;
    return trb;
}

struct TranslatorBase * __g_lxa_translator_OpenLib ( register struct TranslatorBase *TranslatorBase __asm("a6"))
{
    DPRINTF (LOG_DEBUG, "_translator: OpenLib() called\n");
    TranslatorBase->lib.lib_OpenCnt++;
    TranslatorBase->lib.lib_Flags &= ~LIBF_DELEXP;
    return TranslatorBase;
}

BPTR __g_lxa_translator_CloseLib ( register struct TranslatorBase *trb __asm("a6"))
{
    DPRINTF (LOG_DEBUG, "_translator: CloseLib() called\n");
    trb->lib.lib_OpenCnt--;
    return (BPTR)0;
}

BPTR __g_lxa_translator_ExpungeLib ( register struct TranslatorBase *trb __asm("a6"))
{
    return (BPTR)0;
}

ULONG __g_lxa_translator_ExtFuncLib(void)
{
    return 0;
}

/****************************************************************************/
/* Main functions                                                            */
/****************************************************************************/

/*
 * Translate()
 * 
 * Converts English text to phonemes for the narrator.device speech synthesizer.
 * 
 * Returns:
 *   0 = Success
 *   >0 = Character position where translation stopped due to buffer overflow
 *   <0 = Translation error
 *
 * This stub implementation simply copies the input text to output,
 * as we don't have a real phoneme translator.
 */
LONG _translator_Translate ( register struct TranslatorBase *TranslatorBase __asm("a6"),
                             register CONST_STRPTR           inputString    __asm("a0"),
                             register LONG                   inputLength    __asm("d0"),
                             register STRPTR                 outputBuffer   __asm("a1"),
                             register LONG                   bufferSize     __asm("d1"))
{
    DPRINTF (LOG_DEBUG, "_translator: Translate() called inputLen=%ld bufSize=%ld\n",
             inputLength, bufferSize);
    
    if (!inputString || !outputBuffer || bufferSize <= 0)
        return -1; /* Error */
    
    /* Simple stub: just copy input to output */
    LONG copyLen = inputLength;
    if (copyLen >= bufferSize)
        copyLen = bufferSize - 1;
    
    LONG i;
    for (i = 0; i < copyLen; i++)
    {
        outputBuffer[i] = inputString[i];
    }
    outputBuffer[copyLen] = '\0';
    
    /* Return 0 for success, or position where we stopped if buffer too small */
    if (inputLength >= bufferSize)
        return copyLen; /* Position where we stopped */
    
    return 0; /* Success */
}

/****************************************************************************/
/* ROMTag and library initialization                                         */
/****************************************************************************/

struct MyDataInit
{
    UWORD ln_Type_Init     ; UWORD ln_Type_Offset     ; UWORD ln_Type_Content     ;
    UBYTE ln_Name_Init     ; UBYTE ln_Name_Offset     ; ULONG ln_Name_Content     ;
    UWORD lib_Flags_Init   ; UWORD lib_Flags_Offset   ; UWORD lib_Flags_Content   ;
    UWORD lib_Version_Init ; UWORD lib_Version_Offset ; UWORD lib_Version_Content ;
    UWORD lib_Revision_Init; UWORD lib_Revision_Offset; UWORD lib_Revision_Content;
    UBYTE lib_IdString_Init; UBYTE lib_IdString_Offset; ULONG lib_IdString_Content;
    ULONG ENDMARK;
};

extern APTR              __g_lxa_translator_FuncTab [];
extern struct MyDataInit __g_lxa_translator_DataTab;
extern struct InitTable  __g_lxa_translator_InitTab;
extern APTR              __g_lxa_translator_EndResident;

static struct Resident __aligned ROMTag =
{
    RTC_MATCHWORD,                        // UWORD rt_MatchWord
    &ROMTag,                              // struct Resident *rt_MatchTag
    &__g_lxa_translator_EndResident,      // APTR  rt_EndSkip
    RTF_AUTOINIT,                         // UBYTE rt_Flags
    VERSION,                              // UBYTE rt_Version
    NT_LIBRARY,                           // UBYTE rt_Type
    0,                                    // BYTE  rt_Pri
    &_g_translator_ExLibName[0],          // char  *rt_Name
    &_g_translator_ExLibID[0],            // char  *rt_IdString
    &__g_lxa_translator_InitTab           // APTR  rt_Init
};

APTR __g_lxa_translator_EndResident;
struct Resident *__lxa_translator_ROMTag = &ROMTag;

struct InitTable __g_lxa_translator_InitTab =
{
    (ULONG)               sizeof(struct TranslatorBase),
    (APTR              *) &__g_lxa_translator_FuncTab[0],
    (APTR)                &__g_lxa_translator_DataTab,
    (APTR)                __g_lxa_translator_InitLib
};

/* Function table - offsets from translator_pragmas.h 
 * Translate is at 0x1e = 30 = LVO -30
 */
APTR __g_lxa_translator_FuncTab [] =
{
    __g_lxa_translator_OpenLib,           // -6   (0x06) pos 0
    __g_lxa_translator_CloseLib,          // -12  (0x0c) pos 1
    __g_lxa_translator_ExpungeLib,        // -18  (0x12) pos 2
    __g_lxa_translator_ExtFuncLib,        // -24  (0x18) pos 3
    _translator_Translate,                // -30  (0x1e) pos 4
    (APTR) ((LONG)-1)
};

struct MyDataInit __g_lxa_translator_DataTab =
{
    /* ln_Type      */ INITBYTE(OFFSET(Node,         ln_Type),      NT_LIBRARY),
    /* ln_Name      */ 0x80, (UBYTE) (ULONG) OFFSET(Node,    ln_Name), (ULONG) &_g_translator_ExLibName[0],
    /* lib_Flags    */ INITBYTE(OFFSET(Library,      lib_Flags),    LIBF_SUMUSED|LIBF_CHANGED),
    /* lib_Version  */ INITWORD(OFFSET(Library,      lib_Version),  VERSION),
    /* lib_Revision */ INITWORD(OFFSET(Library,      lib_Revision), REVISION),
    /* lib_IdString */ 0x80, (UBYTE) (ULONG) OFFSET(Library, lib_IdString), (ULONG) &_g_translator_ExLibID[0],
    (ULONG) 0
};
