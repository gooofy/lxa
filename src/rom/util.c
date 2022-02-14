#include "util.h"

#include <stdint.h>
#include <stdarg.h>

#if 0
#include <ctype.h>
#include <stdlib.h>
#include <limits.h>
#include <math.h>
#include <float.h>
#include "stdio.h"
#endif

// a union to handle the
union _d_bits {
	double d;
	struct {
		unsigned sign :1;
		unsigned exp :11;
		unsigned frac0 :20;
		unsigned frac1 :32;
	} b;
	unsigned u;
};

uint32_t strlen(const char *string)
{
    const char *s = string;

    while (*s++) {
    }
    return ~(string - s);
}

ULONG emucall1 (ULONG func, ULONG param1)
{
    ULONG res;
    asm( "move.l    %1, d0\n\t"
         "move.l    %2, d1\n\t"
         "illegal\n\t"
         "move.l    d0, %0\n"
        : "=r" (res)
        : "r" (func), "r" (param1)
        : "cc", "d0", "d1"
        );
    return res;
}

ULONG emucall3 (ULONG func, ULONG param1, ULONG param2, ULONG param3)
{
    ULONG res;
    asm( "move.l    %1, d0\n\t"
         "move.l    %2, d1\n\t"
         "move.l    %3, d2\n\t"
         "move.l    %4, d3\n\t"
         "illegal\n\t"
         "move.l    d0, %0\n"
        : "=r" (res)
        : "r" (func), "r" (param1), "r" (param2), "r" (param3)
        : "cc", "d0", "d1", "d2", "d3"
        );
    return res;
}

void emu_stop (void)
{
    asm( "move.l    #2, d0 /* EMU_CALL_STOP */\n\t"
         "illegal"
        : /* no outputs */
        : /* no inputs  */
        : "cc", "d0"
        );
}

void lputc (int level, char c)
{
    asm( "move.l    #1, d0 /* EMU_CALL_LPUTC */\n\t"
         "move.l    %0, d1\n\t"
         "move.l    %1, d2\n\t"
         "illegal"
        : /* no outputs */
        : "r" (level), "r" (c)
        : "cc", "d0", "d1", "d2"
        );
}

void lputs (int lvl, const char *s)
{
    asm( "move.l    #4, d0 /* EMU_CALL_LPUTS */\n\t"
         "move.l    %0, d1\n\t"
         "move.l    %1, d2\n\t"
         "illegal"
        : /* no outputs */
        : "r" (lvl), "r" (s)
        : "cc", "d0", "d1", "d2"
        );
}

#define CHAR_BIT 8
#define MININTSIZE (sizeof(unsigned long long)*CHAR_BIT/3+1)
#define MINPOINTSIZE (sizeof(void *)*CHAR_BIT/4+1)
#define REQUIREDBUFFER (MININTSIZE>MINPOINTSIZE?MININTSIZE:MINPOINTSIZE)

/**
 * '#'
 * Used with o, exponent or X specifiers the value is preceeded with 0, 0x or 0X
 * respectively for values different than zero.
 * Used with a, A, e, E, f, F, g or G it forces the written output
 * to contain a decimal point even if no more digits follow.
 * By default, if no digits follow, no decimal point is written.
 */
#define ALTERNATEFLAG 1  /* '#' is set */

/**
 * '0'
 * Left-pads the number with zeroes (0) instead of spaces when padding is specified
 * (see width sub-specifier).
 */
#define ZEROPADFLAG   2  /* '0' is set */

/**
 * '-'
 * Left-justify within the given field width;
 * Right justification is the default (see width sub-specifier).
 */
#define LALIGNFLAG    4  /* '-' is set */

/**
 * ' '
 * If no sign is going to be written, a blank space is inserted before the value.
 */
#define BLANKFLAG     8  /* ' ' is set */

/**
 * '+'
 * Forces to preceed the result with a plus or minus sign (+ or -) even for positive numbers.
 * By default, only negative numbers are preceded with a - sign.
 */
#define SIGNFLAG      16 /* '+' is set */

static const char flagc[] = { '#', '0', '-', ' ', '+' };

static unsigned __ulldivus(unsigned long long * llp, unsigned short n) {
	struct LL {
		unsigned long hi;
		union {
			unsigned long lo;
			struct {
				unsigned short exponent;
				unsigned short y;
			} s;
		} u;
	}* hl = (struct LL *) llp;

	unsigned r;
	unsigned long h = hl->hi;
	if (h) {
		unsigned l = hl->u.s.exponent;
		unsigned k = hl->u.s.y;
		unsigned c = h % n;
		h = h / n;
		l = l + (c << 16);
		c = l % n;
		l = l / n;
		k = k + (c << 16);
		r = k % n;
		k = k / n;
		hl->u.lo = (l << 16) + k;
		hl->hi = h + (l >> 16);
		return r;
	}

	r = hl->u.lo % n;
	hl->u.lo /= n;
	return r;
}

const unsigned char __ctype[]=
{ 0x00,
  0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x28,0x28,0x28,0x28,0x28,0x20,0x20,
  0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,
  0x88,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,
  0x04,0x04,0x04,0x04,0x04,0x04,0x04,0x04,0x04,0x04,0x10,0x10,0x10,0x10,0x10,0x10,
  0x10,0x41,0x41,0x41,0x41,0x41,0x41,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,
  0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x10,0x10,0x10,0x10,0x10,
  0x10,0x42,0x42,0x42,0x42,0x42,0x42,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,
  0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x10,0x10,0x10,0x10,0x20,
  0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,
  0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,
  0x08,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,
  0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,
  0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,
  0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x10,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x02,
  0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,
  0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x10,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,
  0x00,0x00,0x00
};

const unsigned char * const _ctype_=__ctype;

int isdigit(int c)
{
    return _ctype_[1+c]&4;
}

static void vlprintf(int lvl, const char *format, va_list args)
{
    //lputs("1\n");

	while (*format)
    {
        //lputs("2\n");
		if (*format == '%') {
			static const char lowertabel[] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f' };
			static const char uppertabel[] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F' };
			short width = 0;
			unsigned short preci = 0x7fff;
			short flags = 0; /* Specifications */
			char type, subtype = 'i';
			char buffer1[2]; /* Signs and that like */
			char buffer[REQUIREDBUFFER]; /* The body */
			char *buffer2 = buffer; /* So we can set this to any other strings */
			uint32_t size1 = 0, size2 = 0; /* How many chars in buffer? */
			const char *ptr = format + 1; /* pointer to format string */
			unsigned short i, pad; /* Some temporary variables */

			do /* read flags */
				for (i = 0; i < sizeof(flagc); i++)
					if (flagc[i] == *ptr) {
						flags |= 1 << i;
						ptr++;
						break;
					} while (i < sizeof(flagc));

			if (*ptr == '*') /* read width from arguments */
			{
				signed int a;
				ptr++;
				a = va_arg(args, signed int);
				if (a < 0) {
					flags |= LALIGNFLAG;
					width = -a;
				} else
					width = a;
			} else {
				while (isdigit(*ptr))
					width = width * 10 + (*ptr++ - '0');
			}

			if (*ptr == 'h' || *ptr == 'l' || *ptr == 'L' || *ptr == 'j'
					|| *ptr == 'z' || *ptr == 't') {
				subtype = *ptr++;
				if (*ptr == 'h' || *ptr == 'l')
					++ptr, ++subtype;
			} else
				subtype = 0;

			type = *ptr++;

			switch (type) {
			case 'd':
			case 'i':
			case 'o':
			case 'p':
			case 'u':
			case 'x':
			case 'X': {
				unsigned long long v;
				const char *tabel;
				int base;

				if (type == 'p') {
					subtype = 'l'; /* This is written as %#lx */
					type = 'x';
					flags |= ALTERNATEFLAG;
				}

				if (type == 'd' || type == 'i') /* These are signed */
				{
					signed long long v2;
					if (subtype == 'l')
						v2 = va_arg(args, signed long);
					else if (subtype == 'm' || subtype == 'j')
						v2 = va_arg(args, signed long long);
					else
						v2 = va_arg(args, signed int);
					if (v2 < 0) {
						buffer1[size1++] = '-';
						v = -v2;
					} else {
						if (flags & SIGNFLAG)
							buffer1[size1++] = '+';
						else if (flags & BLANKFLAG)
							buffer1[size1++] = ' ';
						v = v2;
					}
				} else /* These are unsigned */
				{
					if (subtype == 'l')
						v = va_arg(args, unsigned long);
					else if (subtype == 'm' || subtype == 'j')
						v = va_arg(args, unsigned long long);
					else
						v = va_arg(args, unsigned int);
					if (flags & ALTERNATEFLAG) {
						if (type == 'o') {
							if (!preci || v)
								buffer1[size1++] = '0';
						} else if ((type == 'x' || type == 'X') && v) {
							buffer1[size1++] = '0';
							buffer1[size1++] = type;
						}
					}
				}

				buffer2 = &buffer[sizeof(buffer)]; /* Calculate body string */
				base = type == 'x' || type == 'X' ? 16 : (type == 'o' ? 8 : 10);
				tabel = type != 'X' ? lowertabel : uppertabel;
				do {
					*--buffer2 = tabel[__ulldivus(&v, base)];
					size2++;
				} while (v);
				if (preci == 0x7fff) /* default */
					preci = 0;
				else
					flags &= ~ZEROPADFLAG;
				break;
			}
			case 'c':
				if (subtype == 'l')
					*buffer2 = va_arg(args, long);
				else
					*buffer2 = va_arg(args, int);
				size2 = 1;
				preci = 0;
				break;
			case 's':
				buffer2 = va_arg(args, char *);
				size2 = strlen(buffer2);
				size2 = size2 <= preci ? size2 : preci;
				preci = 0;
				break;

			case '%':
				buffer2 = "%";
				size2 = 1;
				preci = 0;
				break;
			default:
				if (!type)
					ptr--; /* We've gone too far - step one back */
				buffer2 = (char *) format;
				size2 = ptr - format;
				width = preci = 0;
				break;
			}

			pad = size1 + (size2 >= preci ? size2 : preci); /* Calculate the number of characters */
			pad = pad >= width ? 0 : width - pad; /* and the number of resulting pad bytes */

			if (flags & ZEROPADFLAG) /* print sign and that like */
				for (i = 0; i < size1; i++)
					lputc(lvl, buffer1[i]);

			if (!(flags & LALIGNFLAG)) /* Pad left */
				for (i = 0; i < pad; i++)
					lputc(lvl, flags&ZEROPADFLAG?'0':' ');

			if (!(flags & ZEROPADFLAG)) /* print sign if not zero padded */
				for (i = 0; i < size1; i++)
					lputc(lvl, buffer1[i]);

			for (i = size2; i < preci; i++) /* extend to precision */
				lputc(lvl, '0');

			for (i = 0; i < size2; i++) /* print body */
				lputc(lvl, buffer2[i]);

			if (flags & LALIGNFLAG) /* Pad right */
				for (i = 0; i < pad; i++)
					lputc(lvl, ' ');

			format = ptr;
		} else
			lputc(lvl, *format++);
	}
}

void lprintf(int lvl, const char *format, ...)
{
    va_list args;

    //lputs("0\n");
    //lputs(format);

    va_start(args, format);
    vlprintf(lvl, format, args);
    va_end(args);
}

__stdargs void __assert_func (const char *file_name, int line_number, const char *e)
{
    lprintf (LOG_ERROR, "*** assertion failed: %s:%d %s\n", file_name, line_number, e);
    emu_stop();
}

void *memset(void *dst, int c, ULONG n)
{
    if (n)
    {
        char *d = dst;

        do
            *d++ = c;
        while (--n != 0);
    }
    return dst;
}

int strcmp(const char* s1, const char* s2)
{
    while(*s1 && (*s1 == *s2))
    {
        s1++;
        s2++;
    }
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}

#define HEXDUMP_COLS 16

BOOL isprintable (char c)
{
    return (c>=32) & (c<127);
}

void hexdump (int lvl, void *mem, unsigned int len)
{
    unsigned int i, j;

    for(i = 0; i < len + ((len % HEXDUMP_COLS) ? (HEXDUMP_COLS - len % HEXDUMP_COLS) : 0); i++)
    {
        /* print offset */
        if(i % HEXDUMP_COLS == 0)
        {
            LPRINTF(lvl, "0x%06x: ", i);
        }

        /* print hex data */
        if(i < len)
        {
            LPRINTF(lvl, "%02x ", 0xFF & ((char*)mem)[i]);
        }
        else /* end of block, just aligning for ASCII dump */
        {
            LPRINTF(lvl, "   ");
        }

        /* print ASCII dump */
        if(i % HEXDUMP_COLS == (HEXDUMP_COLS - 1))
        {
            for(j = i - (HEXDUMP_COLS - 1); j <= i; j++)
            {
                if(j >= len) /* end of block, not really printing */
                {
                    LPRINTF(lvl, " ");
                }
                else if (isprintable((((char*)mem)[j]))) /* printable char */
                {
                    LPRINTF(lvl, "%c", 0xFF & ((char*)mem)[j]);
                }
                else /* other char */
                {
                    LPRINTF(lvl, ".");
                }
            }
            LPRINTF(lvl, "\n");
        }
    }
}
