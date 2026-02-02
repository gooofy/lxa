/*
 * lxa keymap.library implementation
 *
 * Provides keyboard mapping functionality for converting raw key events
 * to ANSI characters and vice versa.
 */

#include <exec/types.h>
#include <exec/memory.h>
#include <exec/execbase.h>
#include <exec/resident.h>
#include <exec/initializers.h>
#include <exec/lists.h>
#include <clib/exec_protos.h>
#include <inline/exec.h>

#include <devices/keymap.h>
#include <devices/inputevent.h>

#include "util.h"

/* IPTR is a pointer-sized integer type */
#ifndef IPTR
#define IPTR ULONG
#endif

#define VERSION    40
#define REVISION   1
#define EXLIBNAME  "keymap"
#define EXLIBVER   " 40.1 (2025/02/02)"

char __aligned _g_keymap_ExLibName [] = EXLIBNAME ".library";
char __aligned _g_keymap_ExLibID   [] = EXLIBNAME EXLIBVER;
char __aligned _g_keymap_Copyright [] = "(C)opyright 2025 by G. Bartsch. Licensed under the MIT license.";

char __aligned _g_keymap_VERSTRING [] = "\0$VER: " EXLIBNAME EXLIBVER;

extern struct ExecBase *SysBase;

/* KeymapBase structure */
struct KeymapBase {
    struct Library        lib;
    UWORD                 Pad;
    BPTR                  SegList;
    struct KeyMap        *DefaultKeymap;
    struct KeyMapResource KeymapResource;
};

/****************************************************************************/
/* Default US keymap                                                         */
/****************************************************************************/

/* Simplified key types for low keymap (0x00-0x3F) */
#define N  KC_NOQUAL
#define S  KCF_SHIFT
#define A  KCF_ALT
#define C  KCF_CONTROL
#define V  KC_VANILLA
#define ST KCF_STRING
#define NOP KCF_NOP

static const UBYTE lokeymaptypes[] =
{
    V,    V,    V,    V,    V,    V,    V,    V,    /* 00-07 */
    V,    V,    V,    V,    S,    V,    NOP,  N,    /* 08-0F */
    V,    V,    V,    V,    V,    V,    V,    V,    /* 10-17: q-i */
    V,    V,    V,    V,    NOP,  N,    N,    N,    /* 18-1F: o-p */
    V,    V,    V,    V,    V,    V,    V,    V,    /* 20-27: a-k */
    V,    S,    S,    NOP,  NOP,  N,    N,    N,    /* 28-2F: l */
    S|A,  V,    V,    V,    V,    V,    V,    V,    /* 30-37: z-m */
    S,    S,    S,    NOP,  N,    N,    N,    N,    /* 38-3F */
};

/* Key mappings for low keymap - simplified US layout */
static const IPTR lokeymap[] =
{
    /* 00 */ '`'|('~'<<8),                                   /* grave/tilde */
    /* 01 */ '1'|('!'<<8),                                   /* 1/! */
    /* 02 */ '2'|('@'<<8),                                   /* 2/@ */
    /* 03 */ '3'|('#'<<8),                                   /* 3/# */
    /* 04 */ '4'|('$'<<8),                                   /* 4/$ */
    /* 05 */ '5'|('%'<<8),                                   /* 5/% */
    /* 06 */ '6'|('^'<<8),                                   /* 6/^ */
    /* 07 */ '7'|('&'<<8),                                   /* 7/& */
    /* 08 */ '8'|('*'<<8),                                   /* 8 asterisk */
    /* 09 */ '9'|('('<<8),                                   /* 9/( */
    /* 0A */ '0'|(')'<<8),                                   /* 0/) */
    /* 0B */ '-'|('_'<<8),                                   /* -/_ */
    /* 0C */ '='|('+'<<8),                                   /* =/+ */
    /* 0D */ '\\'|('|'<<8),                                  /* \/| */
    /* 0E */ 0,                                              /* NOP */
    /* 0F */ '0',                                            /* numeric 0 */
    /* 10 */ 'q'|('Q'<<8)|('q'<<16)|('Q'<<24),              /* q/Q */
    /* 11 */ 'w'|('W'<<8)|('w'<<16)|('W'<<24),              /* w/W */
    /* 12 */ 'e'|('E'<<8)|('e'<<16)|('E'<<24),              /* e/E */
    /* 13 */ 'r'|('R'<<8)|('r'<<16)|('R'<<24),              /* r/R */
    /* 14 */ 't'|('T'<<8)|('t'<<16)|('T'<<24),              /* t/T */
    /* 15 */ 'y'|('Y'<<8)|('y'<<16)|('Y'<<24),              /* y/Y */
    /* 16 */ 'u'|('U'<<8)|('u'<<16)|('U'<<24),              /* u/U */
    /* 17 */ 'i'|('I'<<8)|('i'<<16)|('I'<<24),              /* i/I */
    /* 18 */ 'o'|('O'<<8)|('o'<<16)|('O'<<24),              /* o/O */
    /* 19 */ 'p'|('P'<<8)|('p'<<16)|('P'<<24),              /* p/P */
    /* 1A */ '['|(('{'<<8)),                                 /* [/{ */
    /* 1B */ ']'|(('}'<<8)),                                 /* ]/} */
    /* 1C */ 0,                                              /* NOP */
    /* 1D */ '1',                                            /* numeric 1 */
    /* 1E */ '2',                                            /* numeric 2 */
    /* 1F */ '3',                                            /* numeric 3 */
    /* 20 */ 'a'|('A'<<8)|('a'<<16)|('A'<<24),              /* a/A */
    /* 21 */ 's'|('S'<<8)|('s'<<16)|('S'<<24),              /* s/S */
    /* 22 */ 'd'|('D'<<8)|('d'<<16)|('D'<<24),              /* d/D */
    /* 23 */ 'f'|('F'<<8)|('f'<<16)|('F'<<24),              /* f/F */
    /* 24 */ 'g'|('G'<<8)|('g'<<16)|('G'<<24),              /* g/G */
    /* 25 */ 'h'|('H'<<8)|('h'<<16)|('H'<<24),              /* h/H */
    /* 26 */ 'j'|('J'<<8)|('j'<<16)|('J'<<24),              /* j/J */
    /* 27 */ 'k'|('K'<<8)|('k'<<16)|('K'<<24),              /* k/K */
    /* 28 */ 'l'|('L'<<8)|('l'<<16)|('L'<<24),              /* l/L */
    /* 29 */ ';'|(':'<<8),                                   /* ;/: */
    /* 2A */ '\''|('"'<<8),                                  /* '/" */
    /* 2B */ 0,                                              /* NOP */
    /* 2C */ 0,                                              /* NOP */
    /* 2D */ '4',                                            /* numeric 4 */
    /* 2E */ '5',                                            /* numeric 5 */
    /* 2F */ '6',                                            /* numeric 6 */
    /* 30 */ 0,                                              /* NOP (int backslash) */
    /* 31 */ 'z'|('Z'<<8)|('z'<<16)|('Z'<<24),              /* z/Z */
    /* 32 */ 'x'|('X'<<8)|('x'<<16)|('X'<<24),              /* x/X */
    /* 33 */ 'c'|('C'<<8)|('c'<<16)|('C'<<24),              /* c/C */
    /* 34 */ 'v'|('V'<<8)|('v'<<16)|('V'<<24),              /* v/V */
    /* 35 */ 'b'|('B'<<8)|('b'<<16)|('B'<<24),              /* b/B */
    /* 36 */ 'n'|('N'<<8)|('n'<<16)|('N'<<24),              /* n/N */
    /* 37 */ 'm'|('M'<<8)|('m'<<16)|('M'<<24),              /* m/M */
    /* 38 */ ','|('<'<<8),                                   /* ,/< */
    /* 39 */ '.'|('>'<<8),                                   /* ./> */
    /* 3A */ '/'|('?'<<8),                                   /* ?/? */
    /* 3B */ 0,                                              /* NOP */
    /* 3C */ '.',                                            /* numeric . */
    /* 3D */ '7',                                            /* numeric 7 */
    /* 3E */ '8',                                            /* numeric 8 */
    /* 3F */ '9',                                            /* numeric 9 */
};

/* Capsable keys (bitfield) - letters are capsable */
static const UBYTE locapsable[] =
{
    0x00, 0x00,                                              /* 00-0F */
    0xFF, 0x03,                                              /* 10-1F: q-s */
    0xFF, 0x03,                                              /* 20-2F: a-s */
    0xFE, 0x00,                                              /* 30-3F: z-m */
};

/* Repeatable keys (bitfield) - most keys repeatable */
static const UBYTE lorepeatable[] =
{
    0xFF, 0xFF, 0xFF, 0xFF,                                  /* all keys repeatable */
    0xFF, 0xFF, 0xFF, 0xFF,
};

/* Simplified key types for high keymap (0x40-0x77) */
static const UBYTE hikeymaptypes[] =
{
    N,    N,    N,    N,    N,    N,    N,    N,    /* 40-47: space, backspace, tab, enter, ret, esc, del, ins */
    N,    N,    N,    S,    N,    N,    N,    N,    /* 48-4F */
    ST,   ST,   ST,   ST,   ST,   ST,   ST,   ST,   /* 50-57: F1-F8 */
    ST,   ST,   N,    N,    N,    N,    N,    N,    /* 58-5F: F9-F10 */
    NOP,  NOP,  NOP,  NOP,  NOP,  NOP,  NOP,  NOP,  /* 60-67 */
    NOP,  NOP,  NOP,  NOP,  NOP,  NOP,  NOP,  NOP,  /* 68-6F */
    NOP,  NOP,  NOP,  NOP,  NOP,  NOP,  NOP,  NOP,  /* 70-77 */
};

/* High keymap key mappings */
static const IPTR hikeymap[] =
{
    /* 40 */ ' '|(' '<<8)|(' '<<16)|(' '<<24),              /* space */
    /* 41 */ '\b'|('\b'<<8)|('\b'<<16)|('\b'<<24),          /* backspace */
    /* 42 */ '\t'|('\t'<<8)|('\t'<<16)|('\t'<<24),          /* tab */
    /* 43 */ '\r'|('\r'<<8)|('\r'<<16)|('\r'<<24),          /* enter */
    /* 44 */ '\r'|('\r'<<8)|('\r'<<16)|('\r'<<24),          /* return */
    /* 45 */ 0x1B|(0x1B<<8)|(0x1B<<16)|(0x1B<<24),          /* escape */
    /* 46 */ 0x7F|(0x7F<<8)|(0x7F<<16)|(0x7F<<24),          /* del */
    /* 47 */ 0,                                              /* insert (NOP) */
    /* 48 */ 0,                                              /* page up */
    /* 49 */ 0,                                              /* page down */
    /* 4A */ '-',                                            /* numeric - */
    /* 4B */ 0,                                              /* F11 */
    /* 4C */ 0,                                              /* cursor up */
    /* 4D */ 0,                                              /* cursor down */
    /* 4E */ 0,                                              /* cursor right */
    /* 4F */ 0,                                              /* cursor left */
    /* 50 */ (IPTR)"\x9B\x30\x7E",                          /* F1: CSI 0 ~ */
    /* 51 */ (IPTR)"\x9B\x31\x7E",                          /* F2: CSI 1 ~ */
    /* 52 */ (IPTR)"\x9B\x32\x7E",                          /* F3: CSI 2 ~ */
    /* 53 */ (IPTR)"\x9B\x33\x7E",                          /* F4: CSI 3 ~ */
    /* 54 */ (IPTR)"\x9B\x34\x7E",                          /* F5: CSI 4 ~ */
    /* 55 */ (IPTR)"\x9B\x35\x7E",                          /* F6: CSI 5 ~ */
    /* 56 */ (IPTR)"\x9B\x36\x7E",                          /* F7: CSI 6 ~ */
    /* 57 */ (IPTR)"\x9B\x37\x7E",                          /* F8: CSI 7 ~ */
    /* 58 */ (IPTR)"\x9B\x38\x7E",                          /* F9: CSI 8 ~ */
    /* 59 */ (IPTR)"\x9B\x39\x7E",                          /* F10: CSI 9 ~ */
    /* 5A */ 0,                                              /* numeric ( */
    /* 5B */ 0,                                              /* numeric ) */
    /* 5C */ 0,                                              /* numeric / */
    /* 5D */ 0,                                              /* numeric * */
    /* 5E */ '+',                                            /* numeric + */
    /* 5F */ 0,                                              /* help */
    /* 60-77 */ 0, 0, 0, 0, 0, 0, 0, 0,                      /* NOP */
                0, 0, 0, 0, 0, 0, 0, 0,
                0, 0, 0, 0, 0, 0, 0, 0,
};

/* High keymap capsable/repeatable */
static const UBYTE hicapsable[] =
{
    0x00, 0x00, 0x00, 0x00,                                  /* no capsable keys */
    0x00, 0x00, 0x00, 0x00,
};

static const UBYTE hirepeatable[] =
{
    0xFF, 0xFF, 0xFF, 0xFF,                                  /* all keys repeatable */
    0xFF, 0xFF, 0xFF, 0xFF,
};

/* Default keymap structure */
static struct KeyMap default_keymap =
{
    (UBYTE *)lokeymaptypes,
    (IPTR *)lokeymap,
    (UBYTE *)locapsable,
    (UBYTE *)lorepeatable,
    (UBYTE *)hikeymaptypes,
    (IPTR *)hikeymap,
    (UBYTE *)hicapsable,
    (UBYTE *)hirepeatable,
};

/****************************************************************************/
/* Helper functions                                                          */
/****************************************************************************/

#define GetBitProperty(ubytearray, idx) \
    ( (ubytearray)[(idx) / 8] & ( 1 << ((idx) & 0x07) ))

#define GetMapChar(key_mapping, idx) \
    ( ((key_mapping) >> ((3 - (idx)) * 8)) & 0x000000FF )

/* Qualifier conversion table from KCF_xxx to index */
static const BYTE keymaptype_table[8][8] =
{
    /* KC_NOQUAL (0) */
    { 0, -1, -1, -1, -1, -1, -1, -1 },
    /* KCF_SHIFT (1) */
    { 0,  1, -1, -1, -1, -1, -1, -1 },
    /* KCF_ALT (2) */
    { 0, -1,  2, -1, -1, -1, -1, -1 },
    /* KCF_SHIFT|KCF_ALT (3) */
    { 0,  1,  2,  3, -1, -1, -1, -1 },
    /* KCF_CONTROL (4) */
    { 0, -2, -1, -1, -2, -1, -1, -1 },    /* -2 means use CTRL handling */
    /* KCF_SHIFT|KCF_CONTROL (5) */
    { 0,  1, -1, -1, -2, -1, -1, -1 },
    /* KCF_ALT|KCF_CONTROL (6) */
    { 0, -1,  2, -1, -2, -1, -1, -1 },
    /* KCF_SHIFT|KCF_ALT|KCF_CONTROL (7) */
    { 0,  1,  2,  3, -2, -1, -1, -1 }
};

/* String table indices */
static const BYTE keymapstr_table[8][8] =
{
    { 0, -1, -1, -1, -1, -1, -1, -1 },     /* KC_NOQUAL */
    { 0,  1, -1, -1, -1, -1, -1, -1 },     /* KCF_SHIFT */
    { 0, -1,  1, -1, -1, -1, -1, -1 },     /* KCF_ALT */
    { 0,  1,  2,  3, -1, -1, -1, -1 },     /* KCF_SHIFT|KCF_ALT */
    { 0, -1, -1, -1,  1, -1, -1, -1 },     /* KCF_CONTROL */
    { 0,  1, -1, -1,  2, -1, -1, -1 },     /* KCF_SHIFT|KCF_CONTROL */
    { 0, -1,  1, -1,  2, -1, -1, -1 },     /* KCF_ALT|KCF_CONTROL */
    { 0,  1,  2,  3,  4,  5,  6,  7 }      /* ALL */
};

struct BufInfo
{
    UBYTE *Buffer;
    LONG   BufLength;
    LONG   CharsWritten;
};

struct KeyInfo
{
    UBYTE Key_MapType;
    IPTR  Key_Mapping;
    UBYTE KCFQual;
};

static BOOL WriteToBuffer(struct BufInfo *bufinfo, const UBYTE *string, LONG numchars)
{
    if (bufinfo->CharsWritten + numchars > bufinfo->BufLength)
        return FALSE;

    /* Copy characters to buffer */
    for (LONG i = 0; i < numchars; i++)
    {
        bufinfo->Buffer[bufinfo->CharsWritten + i] = string[i];
    }
    
    bufinfo->Buffer += numchars;
    bufinfo->CharsWritten += numchars;

    return TRUE;
}

static BOOL GetKeyInfo(struct KeyInfo *ki, UWORD code, UWORD qual, struct KeyMap *km)
{
    BOOL valid = TRUE;

    if (code & IECODE_UP_PREFIX)
    {
        valid = FALSE;
    }
    else if (code >= 128)
    {
        valid = FALSE;
    }
    else
    {
        BYTE capsable;
        BYTE repeatable;

        ki->KCFQual = KC_NOQUAL;

        code &= ~IECODE_UP_PREFIX;

        /* Convert IEQUALIFIER_xxx to KCF_xxx */
        if (qual & (IEQUALIFIER_LSHIFT|IEQUALIFIER_RSHIFT))
            ki->KCFQual |= KCF_SHIFT;

        if (qual & (IEQUALIFIER_LALT|IEQUALIFIER_RALT))
            ki->KCFQual |= KCF_ALT;

        if (qual & IEQUALIFIER_CONTROL)
            ki->KCFQual |= KCF_CONTROL;

        /* Get the key info */
        if (code <= 0x3F)
        {
            /* Low keymap */
            ki->Key_MapType = km->km_LoKeyMapTypes[code];
            ki->Key_Mapping = km->km_LoKeyMap[code];
            capsable    = GetBitProperty(km->km_LoCapsable,   code);
            repeatable  = GetBitProperty(km->km_LoRepeatable, code);
        }
        else
        {
            code -= 0x40;
            if (code < 0x38)
            {
                /* High keymap */
                ki->Key_MapType = km->km_HiKeyMapTypes[code];
                ki->Key_Mapping = km->km_HiKeyMap[code];
                capsable    = GetBitProperty(km->km_HiCapsable,   code);
                repeatable  = GetBitProperty(km->km_HiRepeatable, code);
            }
            else
            {
                valid = FALSE;
            }
        }

        if (valid)
        {
            if ((qual & IEQUALIFIER_REPEAT) && (!repeatable))
            {
                valid = FALSE;
            }
            else
            {
                if ((qual & IEQUALIFIER_CAPSLOCK) && capsable)
                    ki->KCFQual |= KCF_SHIFT;
            }
        }
    }

    return valid;
}

/****************************************************************************/
/* Library management functions                                              */
/****************************************************************************/

struct KeymapBase * __g_lxa_keymap_InitLib ( register struct KeymapBase *kmb    __asm("a6"),
                                              register BPTR               seglist __asm("a0"),
                                              register struct ExecBase   *sysb    __asm("d0"))
{
    DPRINTF (LOG_DEBUG, "_keymap: InitLib() called\n");
    kmb->SegList = seglist;
    kmb->DefaultKeymap = &default_keymap;
    
    /* Initialize KeyMapResource */
    kmb->KeymapResource.kr_Node.ln_Type = NT_RESOURCE;
    kmb->KeymapResource.kr_Node.ln_Name = "keymap.resource";
    
    /* Initialize the list manually */
    kmb->KeymapResource.kr_List.lh_Head = (struct Node *)&kmb->KeymapResource.kr_List.lh_Tail;
    kmb->KeymapResource.kr_List.lh_Tail = NULL;
    kmb->KeymapResource.kr_List.lh_TailPred = (struct Node *)&kmb->KeymapResource.kr_List.lh_Head;
    
    return kmb;
}

struct KeymapBase * __g_lxa_keymap_OpenLib ( register struct KeymapBase *KeymapBase __asm("a6"))
{
    DPRINTF (LOG_DEBUG, "_keymap: OpenLib() called\n");
    KeymapBase->lib.lib_OpenCnt++;
    KeymapBase->lib.lib_Flags &= ~LIBF_DELEXP;
    return KeymapBase;
}

BPTR __g_lxa_keymap_CloseLib ( register struct KeymapBase *kmb __asm("a6"))
{
    DPRINTF (LOG_DEBUG, "_keymap: CloseLib() called\n");
    kmb->lib.lib_OpenCnt--;
    return (BPTR)0;
}

BPTR __g_lxa_keymap_ExpungeLib ( register struct KeymapBase *kmb __asm("a6"))
{
    return (BPTR)0;
}

ULONG __g_lxa_keymap_ExtFuncLib(void)
{
    return 0;
}

/****************************************************************************/
/* Main functions                                                            */
/****************************************************************************/

VOID _keymap_SetKeyMapDefault ( register struct KeymapBase *KeymapBase __asm("a6"),
                                register struct KeyMap     *keyMap     __asm("a0"))
{
    DPRINTF (LOG_DEBUG, "_keymap: SetKeyMapDefault() called keyMap=0x%08lx\n", keyMap);
    
    if (keyMap)
        KeymapBase->DefaultKeymap = keyMap;
}

struct KeyMap * _keymap_AskKeyMapDefault ( register struct KeymapBase *KeymapBase __asm("a6"))
{
    DPRINTF (LOG_DEBUG, "_keymap: AskKeyMapDefault() called, returning 0x%08lx\n",
             KeymapBase->DefaultKeymap);
    return KeymapBase->DefaultKeymap;
}

WORD _keymap_MapRawKey ( register struct KeymapBase  *KeymapBase __asm("a6"),
                         register struct InputEvent  *event      __asm("a0"),
                         register STRPTR              buffer     __asm("a1"),
                         register LONG                length     __asm("d1"),
                         register struct KeyMap      *keyMap     __asm("a2"))
{
    struct BufInfo bufinfo;
    struct KeyInfo ki;
    UWORD code, qual;

    DPRINTF (LOG_DEBUG, "_keymap: MapRawKey() called class=%d code=0x%04x qual=0x%04x\n",
             event ? event->ie_Class : 0,
             event ? event->ie_Code : 0,
             event ? event->ie_Qualifier : 0);

    bufinfo.Buffer       = (UBYTE *)buffer;
    bufinfo.BufLength    = length;
    bufinfo.CharsWritten = 0L;

    if (!keyMap)
        keyMap = KeymapBase->DefaultKeymap;

    /* Don't handle non-rawkey events */
    if (!event || event->ie_Class != IECLASS_RAWKEY)
        goto done;

    code = event->ie_Code;
    qual = event->ie_Qualifier;

    /* Only codes under 0x78 are valid keyboard codes */
    if ((code >= 0x78) || (!GetKeyInfo(&ki, code, qual, keyMap)))
        goto done;

    /* Handle different key types */
    switch (ki.Key_MapType & (KC_NOQUAL|KCF_STRING|KCF_DEAD|KCF_NOP))
    {
        case KC_NOQUAL:
        {
            BYTE idx;
            UBYTE c;

            idx = keymaptype_table[ki.Key_MapType & KC_VANILLA][ki.KCFQual];

            if (idx != -1)
            {
                if (idx == -2)
                {
                    /* CTRL handling - clear bits 5 and 6 */
                    idx = 3;
                    c = GetMapChar(ki.Key_Mapping, idx);
                    c &= ~((1 << 5)|(1 << 6));
                }
                else
                {
                    c = GetMapChar(ki.Key_Mapping, idx);
                }

                if (c != 0)
                {
                    if (!WriteToBuffer(&bufinfo, &c, 1))
                        goto overflow;
                }
            }
            break;
        }

        case KCF_STRING:
        {
            BYTE idx;

            idx = keymapstr_table[ki.Key_MapType & KC_VANILLA][ki.KCFQual];

            if (idx != -1)
            {
                const UBYTE *str_descrs = (const UBYTE *)ki.Key_Mapping;
                UBYTE len, offset;

                /* Each string descriptor uses two bytes */
                idx *= 2;

                /* Get string info from descriptor table */
                len    = str_descrs[idx];
                offset = str_descrs[idx + 1];

                /* Write string to buffer */
                if (!WriteToBuffer(&bufinfo, &(str_descrs[offset]), len))
                    goto overflow;
            }
            break;
        }

        case KCF_DEAD:
            /* Dead key support not implemented yet */
            DPRINTF (LOG_DEBUG, "_keymap: Dead key pressed (not fully implemented)\n");
            break;

        case KCF_NOP:
            /* Do nothing */
            break;

        default:
            DPRINTF (LOG_WARNING, "_keymap: Invalid keymap type for code 0x%04x\n", event->ie_Code);
            break;
    }

done:
    DPRINTF (LOG_DEBUG, "_keymap: MapRawKey() returning %ld chars\n", bufinfo.CharsWritten);
    return (WORD)bufinfo.CharsWritten;
    
overflow:
    DPRINTF (LOG_WARNING, "_keymap: MapRawKey() buffer overflow\n");
    return -1;
}

LONG _keymap_MapANSI ( register struct KeymapBase *KeymapBase __asm("a6"),
                       register STRPTR             string     __asm("a0"),
                       register LONG               count      __asm("d0"),
                       register STRPTR             buffer     __asm("a1"),
                       register LONG               length     __asm("d1"),
                       register struct KeyMap     *keyMap     __asm("a2"))
{
    DPRINTF (LOG_DEBUG, "_keymap: MapANSI() unimplemented STUB called\n");
    /* Converting ANSI to rawkey is rarely used, stub for now */
    return 0;
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

extern APTR              __g_lxa_keymap_FuncTab [];
extern struct MyDataInit __g_lxa_keymap_DataTab;
extern struct InitTable  __g_lxa_keymap_InitTab;
extern APTR              __g_lxa_keymap_EndResident;

static struct Resident __aligned ROMTag =
{
    RTC_MATCHWORD,                      // UWORD rt_MatchWord
    &ROMTag,                            // struct Resident *rt_MatchTag
    &__g_lxa_keymap_EndResident,        // APTR  rt_EndSkip
    RTF_AUTOINIT,                       // UBYTE rt_Flags
    VERSION,                            // UBYTE rt_Version
    NT_LIBRARY,                         // UBYTE rt_Type
    0,                                  // BYTE  rt_Pri
    &_g_keymap_ExLibName[0],            // char  *rt_Name
    &_g_keymap_ExLibID[0],              // char  *rt_IdString
    &__g_lxa_keymap_InitTab             // APTR  rt_Init
};

APTR __g_lxa_keymap_EndResident;
struct Resident *__lxa_keymap_ROMTag = &ROMTag;

struct InitTable __g_lxa_keymap_InitTab =
{
    (ULONG)               sizeof(struct KeymapBase),
    (APTR              *) &__g_lxa_keymap_FuncTab[0],
    (APTR)                &__g_lxa_keymap_DataTab,
    (APTR)                __g_lxa_keymap_InitLib
};

/* Function table */
APTR __g_lxa_keymap_FuncTab [] =
{
    __g_lxa_keymap_OpenLib,             // -6   (0x06) pos 0
    __g_lxa_keymap_CloseLib,            // -12  (0x0c) pos 1
    __g_lxa_keymap_ExpungeLib,          // -18  (0x12) pos 2
    __g_lxa_keymap_ExtFuncLib,          // -24  (0x18) pos 3
    _keymap_SetKeyMapDefault,           // -30  (0x1e) pos 4
    _keymap_AskKeyMapDefault,           // -36  (0x24) pos 5
    _keymap_MapRawKey,                  // -42  (0x2a) pos 6
    _keymap_MapANSI,                    // -48  (0x30) pos 7
    (APTR) ((LONG)-1)
};

struct MyDataInit __g_lxa_keymap_DataTab =
{
    /* ln_Type      */ INITBYTE(OFFSET(Node,         ln_Type),      NT_LIBRARY),
    /* ln_Name      */ 0x80, (UBYTE) (ULONG) OFFSET(Node,    ln_Name), (ULONG) &_g_keymap_ExLibName[0],
    /* lib_Flags    */ INITBYTE(OFFSET(Library,      lib_Flags),    LIBF_SUMUSED|LIBF_CHANGED),
    /* lib_Version  */ INITWORD(OFFSET(Library,      lib_Version),  VERSION),
    /* lib_Revision */ INITWORD(OFFSET(Library,      lib_Revision), REVISION),
    /* lib_IdString */ 0x80, (UBYTE) (ULONG) OFFSET(Library, lib_IdString), (ULONG) &_g_keymap_ExLibID[0],
    (ULONG) 0
};
