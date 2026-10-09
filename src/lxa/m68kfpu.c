#include <math.h>
#include <stdio.h>
#include <stdarg.h>

extern void exit(int);

/* lxa: an FPU form this core does not implement must not kill the
 * emulator (Fred Fish mass run, Phase 232): report it and raise the F-line
 * exception, as a CPU without that support would. */
static void fatalerror(char *format, ...) {
      static int reported;
      va_list ap;
      if (reported++ < 5) {
            va_start(ap,format);
            vfprintf(stderr,format,ap);  // JFF: fixed. Was using fprintf and arguments were wrong
            va_end(ap);
      }
      m68ki_exception_1111();
}

#define FPCC_N			0x08000000
#define FPCC_Z			0x04000000
#define FPCC_I			0x02000000
#define FPCC_NAN		0x01000000

#define DOUBLE_INFINITY					(unsigned long long)(0x7ff0000000000000)
#define DOUBLE_EXPONENT					(unsigned long long)(0x7ff0000000000000)
#define DOUBLE_MANTISSA					(unsigned long long)(0x000fffffffffffff)

extern flag floatx80_is_nan( floatx80 a );

// masks for packed dwords, positive k-factor
static uint32 pkmask2[18] =
{
	0xffffffff, 0, 0xf0000000, 0xff000000, 0xfff00000, 0xffff0000,
	0xfffff000, 0xffffff00, 0xfffffff0, 0xffffffff,
	0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff,
	0xffffffff, 0xffffffff, 0xffffffff
};

static uint32 pkmask3[18] =
{
	0xffffffff, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0xf0000000, 0xff000000, 0xfff00000, 0xffff0000,
	0xfffff000, 0xffffff00, 0xfffffff0, 0xffffffff,
};

static inline double fx80_to_double(floatx80 fx)
{
	uint64 d;
	double *foo;

	foo = (double *)&d;

	d = floatx80_to_float64(fx);

	return *foo;
}

static inline floatx80 double_to_fx80(double in)
{
	uint64 *d;

	d = (uint64 *)&in;

	return float64_to_floatx80(*d);
}

static inline floatx80 load_extended_float80(uint32 ea)
{
	uint32 d1,d2;
	uint16 d3;
	floatx80 fp;

	d3 = m68ki_read_16(ea);
	d1 = m68ki_read_32(ea+4);
	d2 = m68ki_read_32(ea+8);

	fp.high = d3;
	fp.low = ((uint64)d1<<32) | (d2 & 0xffffffff);

	return fp;
}

static inline void store_extended_float80(uint32 ea, floatx80 fpr)
{
	m68ki_write_16(ea+0, fpr.high);
	m68ki_write_16(ea+2, 0);
	m68ki_write_32(ea+4, (fpr.low>>32)&0xffffffff);
	m68ki_write_32(ea+8, fpr.low&0xffffffff);
}

static inline floatx80 load_pack_float80(uint32 ea)
{
	uint32 dw1, dw2, dw3;
	floatx80 result;
	double tmp;
	char str[128], *ch;

	dw1 = m68ki_read_32(ea);
	dw2 = m68ki_read_32(ea+4);
	dw3 = m68ki_read_32(ea+8);

	ch = &str[0];
	if (dw1 & 0x80000000)	// mantissa sign
	{
		*ch++ = '-';
	}
	*ch++ = (char)((dw1 & 0xf) + '0');
	*ch++ = '.';
	*ch++ = (char)(((dw2 >> 28) & 0xf) + '0');
	*ch++ = (char)(((dw2 >> 24) & 0xf) + '0');
	*ch++ = (char)(((dw2 >> 20) & 0xf) + '0');
	*ch++ = (char)(((dw2 >> 16) & 0xf) + '0');
	*ch++ = (char)(((dw2 >> 12) & 0xf) + '0');
	*ch++ = (char)(((dw2 >> 8)  & 0xf) + '0');
	*ch++ = (char)(((dw2 >> 4)  & 0xf) + '0');
	*ch++ = (char)(((dw2 >> 0)  & 0xf) + '0');
	*ch++ = (char)(((dw3 >> 28) & 0xf) + '0');
	*ch++ = (char)(((dw3 >> 24) & 0xf) + '0');
	*ch++ = (char)(((dw3 >> 20) & 0xf) + '0');
	*ch++ = (char)(((dw3 >> 16) & 0xf) + '0');
	*ch++ = (char)(((dw3 >> 12) & 0xf) + '0');
	*ch++ = (char)(((dw3 >> 8)  & 0xf) + '0');
	*ch++ = (char)(((dw3 >> 4)  & 0xf) + '0');
	*ch++ = (char)(((dw3 >> 0)  & 0xf) + '0');
	*ch++ = 'E';
	if (dw1 & 0x40000000)	// exponent sign
	{
		*ch++ = '-';
	}
	*ch++ = (char)(((dw1 >> 24) & 0xf) + '0');
	*ch++ = (char)(((dw1 >> 20) & 0xf) + '0');
	*ch++ = (char)(((dw1 >> 16) & 0xf) + '0');
	*ch = '\0';

	sscanf(str, "%le", &tmp);

	result = double_to_fx80(tmp);

	return result;
}

static inline void store_pack_float80(uint32 ea, int k, floatx80 fpr)
{
	uint32 dw1, dw2, dw3;
	char str[128], *ch;
	int i, j, exp;

	dw1 = dw2 = dw3 = 0;
	ch = &str[0];

	sprintf(str, "%.16e", fx80_to_double(fpr));

	if (*ch == '-')
	{
		ch++;
		dw1 = 0x80000000;
	}

	if (*ch == '+')
	{
		ch++;
	}

	dw1 |= (*ch++ - '0');

	if (*ch == '.')
	{
		ch++;
	}

	// handle negative k-factor here
	if ((k <= 0) && (k >= -13))
	{
		exp = 0;
		for (i = 0; i < 3; i++)
		{
			if (ch[18+i] >= '0' && ch[18+i] <= '9')
			{
				exp = (exp << 4) | (ch[18+i] - '0');
			}
		}

		if (ch[17] == '-')
		{
			exp = -exp;
		}

		k = -k;
		// last digit is (k + exponent - 1)
		k += (exp - 1);

		// round up the last significant mantissa digit
		if (ch[k+1] >= '5')
		{
			ch[k]++;
		}

		// zero out the rest of the mantissa digits
		for (j = (k+1); j < 16; j++)
		{
			ch[j] = '0';
		}

		// now zero out K to avoid tripping the positive K detection below
		k = 0;
	}

	// crack 8 digits of the mantissa
	for (i = 0; i < 8; i++)
	{
		dw2 <<= 4;
		if (*ch >= '0' && *ch <= '9')
		{
			dw2 |= *ch++ - '0';
		}
	}

	// next 8 digits of the mantissa
	for (i = 0; i < 8; i++)
	{
		dw3 <<= 4;
		if (*ch >= '0' && *ch <= '9')
		dw3 |= *ch++ - '0';
	}

	// handle masking if k is positive
	if (k >= 1)
	{
		if (k <= 17)
		{
			dw2 &= pkmask2[k];
			dw3 &= pkmask3[k];
		}
		else
		{
			dw2 &= pkmask2[17];
			dw3 &= pkmask3[17];
//			m68ki_cpu.fpcr |=  (need to set OPERR bit)
		}
	}

	// finally, crack the exponent
	if (*ch == 'e' || *ch == 'E')
	{
		ch++;
		if (*ch == '-')
		{
			ch++;
			dw1 |= 0x40000000;
		}

		if (*ch == '+')
		{
			ch++;
		}

		j = 0;
		for (i = 0; i < 3; i++)
		{
			if (*ch >= '0' && *ch <= '9')
			{
				j = (j << 4) | (*ch++ - '0');
			}
		}

		dw1 |= (j << 16);
	}

	m68ki_write_32(ea, dw1);
	m68ki_write_32(ea+4, dw2);
	m68ki_write_32(ea+8, dw3);
}

static inline void SET_CONDITION_CODES(floatx80 reg)
{
	REG_FPSR &= ~(FPCC_N|FPCC_Z|FPCC_I|FPCC_NAN);

	// sign flag
	if (reg.high & 0x8000)
	{
		REG_FPSR |= FPCC_N;
	}

	// zero flag
	if (((reg.high & 0x7fff) == 0) && ((reg.low<<1) == 0))
	{
		REG_FPSR |= FPCC_Z;
	}

	// infinity flag
	if (((reg.high & 0x7fff) == 0x7fff) && ((reg.low<<1) == 0))
	{
		REG_FPSR |= FPCC_I;
	}

	// NaN flag
	if (floatx80_is_nan(reg))
	{
		REG_FPSR |= FPCC_NAN;
	}
}

static inline int TEST_CONDITION(int condition)
{
	int n = (REG_FPSR & FPCC_N) != 0;
	int z = (REG_FPSR & FPCC_Z) != 0;
	int nan = (REG_FPSR & FPCC_NAN) != 0;
	int r = 0;
	switch (condition)
	{
		case 0x10:
		case 0x00:		return 0;					// False

		case 0x11:
		case 0x01:		return (z);					// Equal

		case 0x12:
		case 0x02:		return (!(nan || z || n));			// Greater Than

		case 0x13:
		case 0x03:		return (z || !(nan || n));			// Greater or Equal

		case 0x14:
		case 0x04:		return (n && !(nan || z));			// Less Than

		case 0x15:
		case 0x05:		return (z || (n && !nan));			// Less Than or Equal

		case 0x16:
		case 0x06:		return !nan && !z;

		case 0x17:
		case 0x07:		return !nan;

		case 0x18:
		case 0x08:		return nan;

		case 0x19:
		case 0x09:		return nan || z;

		case 0x1a:
		case 0x0a:		return (nan || !(n || z));			// Not Less Than or Equal

		case 0x1b:
		case 0x0b:		return (nan || z || !n);			// Not Less Than

		case 0x1c:
		case 0x0c:		return (nan || (n && !z));			// Not Greater or Equal Than

		case 0x1d:
		case 0x0d:		return (nan || z || n);				// Not Greater Than

		case 0x1e:
		case 0x0e:		return (!z);					// Not Equal

		case 0x1f:
		case 0x0f:		return 1;					// True

		default:		fatalerror("M68kFPU: test_condition: unhandled condition %02X\n", condition);
	}

	return r;
}

/* lxa: one effective-address calculation for every FPU operand (Phase 237).
 * The original core handled a few addressing modes per operand size, so
 * common compiler output such as fmove.d (xxx).L,fp0 (MoonTool, SManCP)
 * took an F-line exception.  All memory modes of the 68881/68882 are
 * accepted: (An), (An)+, -(An), (d16,An), (d8,An,Xn) and the full extension
 * forms, (xxx).W, (xxx).L, (d16,PC), (d8,PC,Xn) and #<data> (read where it
 * sits in the instruction stream; a byte immediate is the low byte of a
 * word). */
static int fpu_ea_ok;

static uint32 fpu_ea_addr(int mode, int reg, int size)
{
	uint32 ea;

	fpu_ea_ok = 1;
	switch (mode)
	{
		case 2:		// (An)
			return REG_A[reg];
		case 3:		// (An)+
			ea = REG_A[reg];
			REG_A[reg] += (size == 1 && reg == 7) ? 2 : size;
			return ea;
		case 4:		// -(An)
			REG_A[reg] -= (size == 1 && reg == 7) ? 2 : size;
			return REG_A[reg];
		case 5:		// (d16,An)
			return REG_A[reg] + MAKE_INT_16(m68ki_read_imm_16());
		case 6:		// (d8,An,Xn), full extension word forms
			return m68ki_get_ea_ix(REG_A[reg]);
		case 7:
			switch (reg)
			{
				case 0:		// (xxx).W
					return MAKE_INT_16(m68ki_read_imm_16());
				case 1:		// (xxx).L
					return m68ki_read_imm_32();
				case 2:		// (d16,PC)
					return m68ki_get_ea_pcdi();
				case 3:		// (d8,PC,Xn)
					return m68ki_get_ea_pcix();
				case 4:		// #<data>
					ea = REG_PC;
					if (size == 1)
					{
						ea++;
						size = 2;
					}
					REG_PC += size;
					return ea;
			}
			break;
	}
	fpu_ea_ok = 0;
	fatalerror("M68kFPU: unhandled addressing mode %d, reg %d at %08X\n", mode, reg, REG_PC);
	return 0;
}

static uint8 READ_EA_8(int ea)
{
	int mode = (ea >> 3) & 0x7;
	int reg = (ea & 0x7);
	uint32 addr;

	if (mode == 0)
		return REG_D[reg];
	addr = fpu_ea_addr(mode, reg, 1);
	return fpu_ea_ok ? m68ki_read_8(addr) : 0;
}

static uint16 READ_EA_16(int ea)
{
	int mode = (ea >> 3) & 0x7;
	int reg = (ea & 0x7);
	uint32 addr;

	if (mode == 0)
		return (uint16)(REG_D[reg]);
	addr = fpu_ea_addr(mode, reg, 2);
	return fpu_ea_ok ? m68ki_read_16(addr) : 0;
}

static uint32 READ_EA_32(int ea)
{
	int mode = (ea >> 3) & 0x7;
	int reg = (ea & 0x7);
	uint32 addr;

	if (mode == 0)
		return REG_D[reg];
	if (mode == 1)
		return REG_A[reg];
	addr = fpu_ea_addr(mode, reg, 4);
	return fpu_ea_ok ? m68ki_read_32(addr) : 0;
}

static uint64 READ_EA_64(int ea)
{
	uint32 addr = fpu_ea_addr((ea >> 3) & 7, ea & 7, 8);

	if (!fpu_ea_ok)
		return 0;
	return (uint64)m68ki_read_32(addr) << 32 | (uint64)m68ki_read_32(addr + 4);
}

static floatx80 READ_EA_FPE(int ea)
{
	floatx80 fpr = { 0, 0 };
	uint32 addr = fpu_ea_addr((ea >> 3) & 7, ea & 7, 12);

	if (fpu_ea_ok)
		fpr = load_extended_float80(addr);
	return fpr;
}

static floatx80 READ_EA_PACK(int ea)
{
	floatx80 fpr = { 0, 0 };
	uint32 addr = fpu_ea_addr((ea >> 3) & 7, ea & 7, 12);

	if (fpu_ea_ok)
		fpr = load_pack_float80(addr);
	return fpr;
}

static void WRITE_EA_8(int ea, uint8 data)
{
	int mode = (ea >> 3) & 0x7;
	int reg = (ea & 0x7);
	uint32 addr;

	if (mode == 0)
	{
		REG_D[reg] = (REG_D[reg] & 0xffffff00) | data;
		return;
	}
	addr = fpu_ea_addr(mode, reg, 1);
	if (fpu_ea_ok)
		m68ki_write_8(addr, data);
}

static void WRITE_EA_16(int ea, uint16 data)
{
	int mode = (ea >> 3) & 0x7;
	int reg = (ea & 0x7);
	uint32 addr;

	if (mode == 0)
	{
		REG_D[reg] = (REG_D[reg] & 0xffff0000) | data;
		return;
	}
	addr = fpu_ea_addr(mode, reg, 2);
	if (fpu_ea_ok)
		m68ki_write_16(addr, data);
}

static void WRITE_EA_32(int ea, uint32 data)
{
	int mode = (ea >> 3) & 0x7;
	int reg = (ea & 0x7);
	uint32 addr;

	if (mode == 0)
	{
		REG_D[reg] = data;
		return;
	}
	if (mode == 1)
	{
		REG_A[reg] = data;
		return;
	}
	addr = fpu_ea_addr(mode, reg, 4);
	if (fpu_ea_ok)
		m68ki_write_32(addr, data);
}

static void WRITE_EA_64(int ea, uint64 data)
{
	uint32 addr = fpu_ea_addr((ea >> 3) & 7, ea & 7, 8);

	if (!fpu_ea_ok)
		return;
	m68ki_write_32(addr, (uint32)(data >> 32));
	m68ki_write_32(addr + 4, (uint32)(data));
}

static void WRITE_EA_FPE(int ea, floatx80 fpr)
{
	uint32 addr = fpu_ea_addr((ea >> 3) & 7, ea & 7, 12);

	if (fpu_ea_ok)
		store_extended_float80(addr, fpr);
}

static void WRITE_EA_PACK(int ea, int k, floatx80 fpr)
{
	uint32 addr = fpu_ea_addr((ea >> 3) & 7, ea & 7, 12);

	if (fpu_ea_ok)
		store_pack_float80(addr, k, fpr);
}

/* lxa (Phase 237): the 68881/68882 operations the original core lacked -
 * transcendental functions, FINT/FINTRZ beyond 32 bits, FGETEXP/FGETMAN,
 * FMOD, FSCALE, FSGLMUL/FSGLDIV and the 68040 single/double-rounded
 * arithmetic forms.  Transcendentals go through the host's long double
 * (the 80-bit extended format on x86 hosts, so no precision is lost
 * there). */
static long double fx80_to_ld(floatx80 a)
{
	int exp = a.high & 0x7fff;
	long double v;

	if (exp == 0x7fff)
	{
		if ((a.low << 1) == 0)
			return (a.high & 0x8000) ? -HUGE_VALL : HUGE_VALL;
		return NAN;
	}
	v = ldexpl((long double)a.low, (exp ? exp : 1) - 0x3fff - 63);
	return (a.high & 0x8000) ? -v : v;
}

static floatx80 ld_to_fx80(long double v)
{
	floatx80 r;
	int e, sign = signbit(v) ? 0x8000 : 0;
	long double m;

	if (isnan(v))
	{
		r.high = 0x7fff;
		r.low = U64(0xffffffffffffffff);
		return r;
	}
	if (v < 0)
		v = -v;
	if (isinf(v))
	{
		r.high = 0x7fff | sign;
		r.low = 0;
		return r;
	}
	if (v == 0)
	{
		r.high = sign;
		r.low = 0;
		return r;
	}
	m = frexpl(v, &e);		/* v = m * 2^e, 0.5 <= m < 1 */
	e = e - 1 + 0x3fff;
	if (e <= 0)			/* denormal */
	{
		r.low = (uint64)ldexpl(m, 64 + e - 1);
		r.high = sign;
		return r;
	}
	r.low = (uint64)ldexpl(m, 64);
	r.high = sign | e;
	return r;
}

/* round to an integer in the given softfloat rounding mode */
static floatx80 fx80_round(floatx80 a, int mode)
{
	int8 saved = float_rounding_mode;
	floatx80 r;

	float_rounding_mode = mode;
	r = floatx80_round_to_int(a);
	float_rounding_mode = saved;
	return r;
}

static int fpgen_extra(int opmode, int dst, floatx80 source)
{
	long double s, d, r;
	floatx80 res;

	switch (opmode)
	{
		case 0x01:	/* FINT */
			res = fx80_round(source, float_rounding_mode);
			break;
		case 0x03:	/* FINTRZ */
			res = fx80_round(source, float_round_to_zero);
			break;
		case 0x1e:	/* FGETEXP */
			if (((source.high & 0x7fff) == 0 && source.low == 0) || (source.high & 0x7fff) == 0x7fff)
				res = (source.high & 0x7fff) == 0x7fff ? ld_to_fx80(NAN) : source;
			else
			{
				int e;
				frexpl(fx80_to_ld(source), &e);
				res = int32_to_floatx80(e - 1);
			}
			break;
		case 0x1f:	/* FGETMAN */
			if (((source.high & 0x7fff) == 0 && source.low == 0) || (source.high & 0x7fff) == 0x7fff)
				res = (source.high & 0x7fff) == 0x7fff ? ld_to_fx80(NAN) : source;
			else
			{
				int e;
				r = frexpl(fx80_to_ld(source), &e) * 2;
				res = ld_to_fx80(r);
			}
			break;
		case 0x02: case 0x06: case 0x08: case 0x09: case 0x0a: case 0x0c: case 0x0d:
		case 0x0e: case 0x0f: case 0x10: case 0x11: case 0x12: case 0x14: case 0x15:
		case 0x16: case 0x19: case 0x1c: case 0x1d:
			s = fx80_to_ld(source);
			switch (opmode)
			{
				case 0x02: r = sinhl(s); break;
				case 0x06: r = log1pl(s); break;
				case 0x08: r = expm1l(s); break;
				case 0x09: r = tanhl(s); break;
				case 0x0a: r = atanl(s); break;
				case 0x0c: r = asinl(s); break;
				case 0x0d: r = atanhl(s); break;
				case 0x0e: r = sinl(s); break;
				case 0x0f: r = tanl(s); break;
				case 0x10: r = expl(s); break;
				case 0x11: r = exp2l(s); break;
				case 0x12: r = powl(10.0L, s); break;
				case 0x14: r = logl(s); break;
				case 0x15: r = log10l(s); break;
				case 0x16: r = log2l(s); break;
				case 0x19: r = coshl(s); break;
				case 0x1c: r = acosl(s); break;
				default:   r = cosl(s); break;	/* 0x1d */
			}
			res = ld_to_fx80(r);
			USE_CYCLES(100);
			break;
		case 0x30: case 0x31: case 0x32: case 0x33:
		case 0x34: case 0x35: case 0x36: case 0x37:	/* FSINCOS: FPc = cos, FPs = sin */
			s = fx80_to_ld(source);
			REG_FP[opmode & 7] = ld_to_fx80(cosl(s));
			res = ld_to_fx80(sinl(s));
			USE_CYCLES(100);
			break;
		case 0x21:	/* FMOD: remainder of the truncated quotient */
			d = fx80_to_ld(REG_FP[dst]);
			s = fx80_to_ld(source);
			res = ld_to_fx80(fmodl(d, s));
			break;
		case 0x26:	/* FSCALE */
			d = fx80_to_ld(REG_FP[dst]);
			res = ld_to_fx80(ldexpl(d, (int)fx80_to_ld(fx80_round(source, float_round_to_zero))));
			break;
		case 0x24:	/* FSGLDIV */
		case 0x64:	/* FDDIV */
			res = floatx80_div(REG_FP[dst], source);
			break;
		case 0x27:	/* FSGLMUL */
		case 0x67:	/* FDMUL */
			res = floatx80_mul(REG_FP[dst], source);
			break;
		case 0x40: case 0x44:	/* FSMOVE, FDMOVE */
			res = source;
			break;
		case 0x41: case 0x45:	/* FSSQRT, FDSQRT */
			res = floatx80_sqrt(source);
			break;
		case 0x58: case 0x5c:	/* FSABS, FDABS */
			res = source;
			res.high &= 0x7fff;
			break;
		case 0x5a: case 0x5e:	/* FSNEG, FDNEG */
			res = source;
			res.high ^= 0x8000;
			break;
		case 0x62: case 0x66:	/* FSADD, FDADD */
			res = floatx80_add(REG_FP[dst], source);
			break;
		case 0x68: case 0x6c:	/* FSSUB, FDSUB */
			res = floatx80_sub(REG_FP[dst], source);
			break;
		default:
			return 0;
	}
	REG_FP[dst] = res;
	SET_CONDITION_CODES(REG_FP[dst]);
	USE_CYCLES(10);
	return 1;
}

static void fpgen_rm_reg(uint16 w2)
{
	int ea = REG_IR & 0x3f;
	int rm = (w2 >> 14) & 0x1;
	int src = (w2 >> 10) & 0x7;
	int dst = (w2 >>  7) & 0x7;
	int opmode = w2 & 0x7f;
	floatx80 source;

	// fmovecr #$f, fp0	f200 5c0f

	if (rm)
	{
		switch (src)
		{
			case 0:		// Long-Word Integer
			{
				sint32 d = READ_EA_32(ea);
				source = int32_to_floatx80(d);
				break;
			}
			case 1:		// Single-precision Real
			{
				uint32 d = READ_EA_32(ea);
				source = float32_to_floatx80(d);
				break;
			}
			case 2:		// Extended-precision Real
			{
			source = READ_EA_FPE(ea);
			  	break;
			}
			case 3:		// Packed-decimal Real
			{
				source = READ_EA_PACK(ea);
				break;
			}
			case 4:		// Word Integer
			{
				sint16 d = READ_EA_16(ea);
				source = int32_to_floatx80((sint32)d);
				break;
			}
			case 5:		// Double-precision Real
			{
				uint64 d = READ_EA_64(ea);

				source = float64_to_floatx80(d);
				break;
			}
			case 6:		// Byte Integer
			{
				sint8 d = READ_EA_8(ea);
				source = int32_to_floatx80((sint32)d);
				break;
			}
			case 7:		// FMOVECR load from constant ROM
			{
				switch (w2 & 0x7f)
				{
					case 0x0:	// Pi
						source.high = 0x4000;
						source.low = U64(0xc90fdaa22168c235);
						break;

					case 0xb:	// log10(2)
						source.high = 0x3ffd;
						source.low = U64(0x9a209a84fbcff798);
						break;

					case 0xc:	// e
						source.high = 0x4000;
						source.low = U64(0xadf85458a2bb4a9b);
						break;

					case 0xd:	// log2(e)
						source.high = 0x3fff;
						source.low = U64(0xb8aa3b295c17f0bc);
						break;

					case 0xe:	// log10(e)
						source.high = 0x3ffd;
						source.low = U64(0xde5bd8a937287195);
						break;

					case 0xf:	// 0.0
						source = int32_to_floatx80((sint32)0);
						break;

					case 0x30:	// ln(2)
						source.high = 0x3ffe;
						source.low = U64(0xb17217f7d1cf79ac);
						break;

					case 0x31:	// ln(10)
						source.high = 0x4000;
						source.low = U64(0x935d8dddaaa8ac17);
						break;

					case 0x32:	// 1 (or 100?  manuals are unclear, but 1 would make more sense)
						source = int32_to_floatx80((sint32)1);
						break;

					case 0x33:	// 10^1
						source = int32_to_floatx80((sint32)10);
						break;

					case 0x34:	// 10^2
						source = int32_to_floatx80((sint32)10*10);
						break;

					default:	/* lxa: 10^4 .. 10^4096; the other offsets read 0 */
						if ((w2 & 0x7f) >= 0x35 && (w2 & 0x7f) <= 0x3f)
							source = ld_to_fx80(powl(10.0L, (long double)(1 << ((w2 & 0x7f) - 0x33))));
						else
							source = int32_to_floatx80((sint32)0);
						break;
				}

				// handle it right here, the usual opmode bits aren't valid in the FMOVECR case
				REG_FP[dst] = source;
	     		SET_CONDITION_CODES(REG_FP[dst]); // JFF when destination is a register, we HAVE to update FPCR
				USE_CYCLES(4);
				return;
			}
			default:	fatalerror("fmove_rm_reg: invalid source specifier %x at %08X\n", src, REG_PC-4);
		}
	}
	else
	{
		source = REG_FP[src];
	}



	if (fpgen_extra(opmode, dst, source))
		return;

	switch (opmode)
	{
		case 0x00:		// FMOVE
		{
			REG_FP[dst] = source;
		    SET_CONDITION_CODES(REG_FP[dst]);  // JFF needs update condition codes
			USE_CYCLES(4);
			break;
		}
		case 0x01:		// Fsint
		{
			sint32 temp;
			temp = floatx80_to_int32(source);
			REG_FP[dst] = int32_to_floatx80(temp);
	  		SET_CONDITION_CODES(REG_FP[dst]);  // JFF needs update condition codes
			break;
		}
		case 0x03:		// FsintRZ
		{
			sint32 temp;
			temp = floatx80_to_int32_round_to_zero(source);
			REG_FP[dst] = int32_to_floatx80(temp);
			SET_CONDITION_CODES(REG_FP[dst]);  // JFF needs update condition codes
			break;
		}
		case 0x04:		// FSQRT
		{
			REG_FP[dst] = floatx80_sqrt(source);
			SET_CONDITION_CODES(REG_FP[dst]);
			USE_CYCLES(109);
			break;
		}
		case 0x18:		// FABS
		{
			REG_FP[dst] = source;
			REG_FP[dst].high &= 0x7fff;
			SET_CONDITION_CODES(REG_FP[dst]);
			USE_CYCLES(3);
			break;
		}
		case 0x1a:		// FNEG
		{
			REG_FP[dst] = source;
			REG_FP[dst].high ^= 0x8000;
			SET_CONDITION_CODES(REG_FP[dst]);
			USE_CYCLES(3);
			break;
		}
		case 0x1e:		// FGETEXP
		{
			sint16 temp;
			temp = source.high;	// get the exponent
			temp -= 0x3fff;	// take off the bias
			REG_FP[dst] = double_to_fx80((double)temp);
			SET_CONDITION_CODES(REG_FP[dst]);
			USE_CYCLES(6);
			break;
		}
  	    case 0x60:		// FSDIVS (JFF) (source has already been converted to floatx80)
		case 0x20:		// FDIV
		{
			REG_FP[dst] = floatx80_div(REG_FP[dst], source);
		    SET_CONDITION_CODES(REG_FP[dst]); // JFF
			USE_CYCLES(43);
			break;
		}
		case 0x22:		// FADD
		{
			REG_FP[dst] = floatx80_add(REG_FP[dst], source);
			SET_CONDITION_CODES(REG_FP[dst]);
			USE_CYCLES(9);
			break;
		}
   		case 0x63:		// FSMULS (JFF) (source has already been converted to floatx80)
		case 0x23:		// FMUL
		{
			REG_FP[dst] = floatx80_mul(REG_FP[dst], source);
			SET_CONDITION_CODES(REG_FP[dst]);
			USE_CYCLES(11);
			break;
		}
		case 0x25:		// FREM
		{
			REG_FP[dst] = floatx80_rem(REG_FP[dst], source);
			SET_CONDITION_CODES(REG_FP[dst]);
			USE_CYCLES(43);	// guess
			break;
		}
		case 0x28:		// FSUB
		{
			REG_FP[dst] = floatx80_sub(REG_FP[dst], source);
			SET_CONDITION_CODES(REG_FP[dst]);
			USE_CYCLES(9);
			break;
		}
		case 0x38:		// FCMP
		{
			floatx80 res;
			res = floatx80_sub(REG_FP[dst], source);
			SET_CONDITION_CODES(res);
			USE_CYCLES(7);
			break;
		}
		case 0x3a:		// FTST
		{
			floatx80 res;
			res = source;
			SET_CONDITION_CODES(res);
			USE_CYCLES(7);
			break;
		}

		default:	fatalerror("fpgen_rm_reg: unimplemented opmode %02X at %08X\n", opmode, REG_PC-4);
	}
}

static void fmove_reg_mem(uint16 w2)
{
	int ea = REG_IR & 0x3f;
	int src = (w2 >>  7) & 0x7;
	int dst = (w2 >> 10) & 0x7;
	int k = (w2 & 0x7f);

	switch (dst)
	{
		case 0:		// Long-Word Integer
		{
			sint32 d = (sint32)floatx80_to_int32(REG_FP[src]);
			WRITE_EA_32(ea, d);
			break;
		}
		case 1:		// Single-precision Real
		{
			uint32 d = floatx80_to_float32(REG_FP[src]);
			WRITE_EA_32(ea, d);
			break;
		}
		case 2:		// Extended-precision Real
		{
			WRITE_EA_FPE(ea, REG_FP[src]);
			break;
		}
		case 3:		// Packed-decimal Real with Static K-factor
		{
			// sign-extend k
			k = (k & 0x40) ? (k | 0xffffff80) : (k & 0x7f);
			WRITE_EA_PACK(ea, k, REG_FP[src]);
			break;
		}
		case 4:		// Word Integer
		{
			WRITE_EA_16(ea, (sint16)floatx80_to_int32(REG_FP[src]));
			break;
		}
		case 5:		// Double-precision Real
		{
			uint64 d;

			d = floatx80_to_float64(REG_FP[src]);

			WRITE_EA_64(ea, d);
			break;
		}
		case 6:		// Byte Integer
		{
			WRITE_EA_8(ea, (sint8)floatx80_to_int32(REG_FP[src]));
			break;
		}
		case 7:		// Packed-decimal Real with Dynamic K-factor
		{
			WRITE_EA_PACK(ea, REG_D[k>>4], REG_FP[src]);
			break;
		}
	}

	USE_CYCLES(12);
}

/* FMOVE/FMOVEM of the control registers (lxa, Phase 237): any subset of
 * FPCR/FPSR/FPIAR, transferred in that order to consecutive longwords;
 * a single register may also live in Dn (or An for FPIAR). */
static void fpcr_set(uint32 v)
{
	/* FPCR rounding mode (bits 5-4): RN, RZ, RM, RP -> softfloat modes */
	static const int8 rmode[4] = { float_round_nearest_even, float_round_to_zero,
	                               float_round_down, float_round_up };
	REG_FPCR = v & 0x0000fff0;
	float_rounding_mode = rmode[(REG_FPCR >> 4) & 3];
}

static void fmove_fpcr(uint16 w2)
{
	int ea = REG_IR & 0x3f;
	int mode = (ea >> 3) & 0x7;
	int dir = (w2 >> 13) & 0x1;
	int list = (w2 >> 10) & 0x7;
	int count = ((list >> 2) & 1) + ((list >> 1) & 1) + (list & 1);
	uint32 addr = 0;
	int i;

	if (count == 0)
		list = 1, count = 1;	/* no register: the 68881 moves FPIAR */

	if (count == 1 || mode == 0 || mode == 1)
	{
		if (dir)
		{
			if (list & 4) WRITE_EA_32(ea, REG_FPCR);
			if (list & 2) WRITE_EA_32(ea, REG_FPSR);
			if (list & 1) WRITE_EA_32(ea, REG_FPIAR);
		}
		else
		{
			if (list & 4) fpcr_set(READ_EA_32(ea));
			if (list & 2) REG_FPSR = READ_EA_32(ea);
			if (list & 1) REG_FPIAR = READ_EA_32(ea);
		}
		USE_CYCLES(10);
		return;
	}

	addr = fpu_ea_addr(mode, ea & 7, 4 * count);
	if (!fpu_ea_ok)
		return;
	for (i = 2; i >= 0; i--)
	{
		if (!(list & (1 << i)))
			continue;
		if (dir)
			m68ki_write_32(addr, i == 2 ? REG_FPCR : i == 1 ? REG_FPSR : REG_FPIAR);
		else
		{
			uint32 v = m68ki_read_32(addr);
			if (i == 2) fpcr_set(v);
			else if (i == 1) REG_FPSR = v;
			else REG_FPIAR = v;
		}
		addr += 4;
	}
	USE_CYCLES(10 * count);
}

/* FMOVEM.X (lxa, Phase 237): static or dynamic (Dn) register lists, every
 * memory addressing mode.  Register mask: for -(An) bit i is FPi and FP7 is
 * stored first (at the highest address); for (An)+ and the control modes
 * bit 7 is FP0 and FP0 is transferred first (at the lowest address).  Either
 * way FP0 ends up at the lowest address, like MOVEM. */
static void fmovem(uint16 w2)
{
	int i;
	int ea = REG_IR & 0x3f;
	int imode = (ea >> 3) & 0x7;
	int reg = ea & 0x7;
	int dir = (w2 >> 13) & 0x1;
	int mode = (w2 >> 11) & 0x3;
	int reglist = (mode & 1) ? (REG_D[(w2 >> 4) & 7] & 0xff) : (w2 & 0xff);
	uint32 addr;

	if (imode == 4)			// -(An): registers to memory only
	{
		if (!dir)
		{
			fatalerror("M68kFPU: FMOVEM -(An) to registers at %08X\n", REG_PC-4);
			return;
		}
		for (i = 7; i >= 0; i--)
		{
			if (reglist & (1 << i))
			{
				REG_A[reg] -= 12;
				store_extended_float80(REG_A[reg], REG_FP[i]);
				USE_CYCLES(2);
			}
		}
		return;
	}

	if (imode == 3)			// (An)+: memory to registers only
	{
		addr = REG_A[reg];
	}
	else
	{
		addr = fpu_ea_addr(imode, reg, 0);
		if (!fpu_ea_ok)
			return;
	}
	for (i = 0; i < 8; i++)
	{
		if (reglist & (0x80 >> i))
		{
			if (dir)
				store_extended_float80(addr, REG_FP[i]);
			else
				REG_FP[i] = load_extended_float80(addr);
			addr += 12;
			USE_CYCLES(2);
		}
	}
	if (imode == 3)
		REG_A[reg] = addr;
}

/* Condition-code group (lxa, Phase 237): FScc <ea>, FDBcc Dn,<label> and
 * FTRAPcc (#<data>). */
static void fscc()
{
	int condition = OPER_I_16() & 0x3f;
	int mode = (REG_IR >> 3) & 0x7;
	int reg = REG_IR & 0x7;
	int cc = TEST_CONDITION(condition);

	if (mode == 1)			// FDBcc Dn,<disp16>
	{
		uint32 base = REG_PC;
		sint32 offset = MAKE_INT_16(OPER_I_16());
		if (!cc)
		{
			uint16 cnt = (uint16)(REG_D[reg] - 1);
			REG_D[reg] = (REG_D[reg] & 0xffff0000) | cnt;
			if (cnt != 0xffff)
			{
				m68ki_trace_t0();
				m68ki_jump(base + offset);
			}
		}
		USE_CYCLES(7);
		return;
	}
	if (mode == 7 && reg >= 2 && reg <= 4)	// FTRAPcc
	{
		if (reg == 2) OPER_I_16();
		else if (reg == 3) OPER_I_32();
		if (cc)
			m68ki_exception_trap(EXCEPTION_TRAPV);
		USE_CYCLES(7);
		return;
	}
	WRITE_EA_8(REG_IR & 0x3f, cc ? 0xff : 0x00);
	USE_CYCLES(7);
}

static void fbcc16(void)
{
	sint32 offset;
	int condition = REG_IR & 0x3f;

	offset = (sint16)(OPER_I_16());

	// TODO: condition and jump!!!
	if (TEST_CONDITION(condition))
	{
		m68ki_trace_t0();			   /* auto-disable (see m68kcpu.h) */
		m68ki_branch_16(offset-2);
	}

	USE_CYCLES(7);
	}

static void fbcc32(void)
{
	sint32 offset;
	int condition = REG_IR & 0x3f;

	offset = OPER_I_32();

	// TODO: condition and jump!!!
	if (TEST_CONDITION(condition))
	{
		m68ki_trace_t0();			   /* auto-disable (see m68kcpu.h) */
		m68ki_branch_32(offset-4);
	}

	USE_CYCLES(7);
}


void m68040_fpu_op0()
{
	m68ki_cpu.fpu_just_reset = 0;

	switch ((REG_IR >> 6) & 0x3)
	{
		case 0:
		{
			uint16 w2 = OPER_I_16();
			switch ((w2 >> 13) & 0x7)
			{
				case 0x0:	// FPU ALU FP, FP
				case 0x2:	// FPU ALU ea, FP
				{
					fpgen_rm_reg(w2);
					break;
				}

				case 0x3:	// FMOVE FP, ea
				{
					fmove_reg_mem(w2);
					break;
				}

				case 0x4:	// FMOVEM ea, FPCR
				case 0x5:	// FMOVEM FPCR, ea
				{
					fmove_fpcr(w2);
					break;
				}

				case 0x6:	// FMOVEM ea, list
				case 0x7:	// FMOVEM list, ea
				{
					fmovem(w2);
					break;
				}

				default:	fatalerror("M68kFPU: unimplemented subop %d at %08X\n", (w2 >> 13) & 0x7, REG_PC-4);
			}
			break;
		}

	    case 1:           // FScc (JFF)
		{
		  fscc();
		  break;
		}
		case 2:		// FBcc disp16
		{
			fbcc16();
			break;
		}
		case 3:		// FBcc disp32
		{
			fbcc32();
			break;
		}

      default:	fatalerror("M68kFPU: unimplemented main op %d at %08X\n", (m68ki_cpu.ir >> 6) & 0x3,  REG_PC-4);
	}
}

static void perform_fsave(uint32 addr, int inc)
{
	if (inc)
	{
		// 68881 IDLE, version 0x1f
		m68ki_write_32(addr, 0x1f180000);
		m68ki_write_32(addr+4, 0);
		m68ki_write_32(addr+8, 0);
		m68ki_write_32(addr+12, 0);
		m68ki_write_32(addr+16, 0);
		m68ki_write_32(addr+20, 0);
		m68ki_write_32(addr+24, 0x70000000);
	}
	else
	{
		m68ki_write_32(addr, 0x70000000);
		m68ki_write_32(addr-4, 0);
		m68ki_write_32(addr-8, 0);
		m68ki_write_32(addr-12, 0);
		m68ki_write_32(addr-16, 0);
		m68ki_write_32(addr-20, 0);
		m68ki_write_32(addr-24, 0x1f180000);
	}
}

// FRESTORE on a NULL frame reboots the FPU - all registers to NaN, the 3 status regs to 0
static void do_frestore_null()
{
	int i;

	REG_FPCR = 0;
	REG_FPSR = 0;
	REG_FPIAR = 0;
	for (i = 0; i < 8; i++)
	{
		REG_FP[i].high = 0x7fff;
		REG_FP[i].low = U64(0xffffffffffffffff);
	}

	// Mac IIci at 408458e6 wants an FSAVE of a just-restored NULL frame to also be NULL
	// The PRM says it's possible to generate a NULL frame, but not how/when/why.  (need the 68881/68882 manual!)
	m68ki_cpu.fpu_just_reset = 1;
}

void m68040_fpu_op1()
{
	int ea = REG_IR & 0x3f;
	int mode = (ea >> 3) & 0x7;
	int reg = (ea & 0x7);
	uint32 addr, temp;

	switch ((REG_IR >> 6) & 0x3)
	{
		case 0:		// FSAVE <ea>
		{
			switch (mode)
			{
				case 3:	// (An)+
		    			addr = EA_AY_PI_32();

					if (m68ki_cpu.fpu_just_reset)
					{
						m68ki_write_32(addr, 0);
					}
					else
					{
						// we normally generate an IDLE frame
						REG_A[reg] += 6*4;
						perform_fsave(addr, 1);
					}
					break;

				case 4: // -(An)
		    			addr = EA_AY_PD_32();

					if (m68ki_cpu.fpu_just_reset)
					{
						m68ki_write_32(addr, 0);
					}
					else
					{
						// we normally generate an IDLE frame
						REG_A[reg] -= 6*4;
						perform_fsave(addr, 0);
					}
					break;

				default:
					fatalerror("M68kFPU: FSAVE unhandled mode %d reg %d at %x\n", mode, reg, REG_PC);
			}
			break;
		}
		break;

		case 1:		// FRESTORE <ea>
		{
			switch (mode)
			{
				case 2: // (An)
					addr = REG_A[reg];
					temp = m68ki_read_32(addr);

					// check for NULL frame
					if (temp & 0xff000000)
					{
						// we don't handle non-NULL frames and there's no pre/post inc/dec to do here
						m68ki_cpu.fpu_just_reset = 0;
					}
					else
					{
						do_frestore_null();
					}
					break;

			case 3:	// (An)+
	    			addr = EA_AY_PI_32();
				temp = m68ki_read_32(addr);

				// check for NULL frame
				if (temp & 0xff000000)
				{
					m68ki_cpu.fpu_just_reset = 0;

					// how about an IDLE frame?
					if ((temp & 0x00ff0000) == 0x00180000)
					{
						REG_A[reg] += 6*4;
					} // check UNIMP
					else if ((temp & 0x00ff0000) == 0x00380000)
					{
						REG_A[reg] += 14*4;
					} // check BUSY
					else if ((temp & 0x00ff0000) == 0x00b40000)
					{
						REG_A[reg] += 45*4;
					}
				}
				else
				{
					do_frestore_null();
				}
				break;

			case 4:	// -(An) - unusual but handle it
				addr = REG_A[reg];
				temp = m68ki_read_32(addr);

				// check for NULL frame
				if (temp & 0xff000000)
				{
					m68ki_cpu.fpu_just_reset = 0;

					// how about an IDLE frame?
					if ((temp & 0x00ff0000) == 0x00180000)
					{
						// Don't modify An for predecrement mode
					} // check UNIMP
					else if ((temp & 0x00ff0000) == 0x00380000)
					{
						// Don't modify An for predecrement mode
					} // check BUSY
					else if ((temp & 0x00ff0000) == 0x00b40000)
					{
						// Don't modify An for predecrement mode
					}
				}
				else
				{
					do_frestore_null();
				}
				break;

			case 5:	// (d16,An)
				addr = EA_AY_DI_32();
				temp = m68ki_read_32(addr);

				// check for NULL frame
				if (temp & 0xff000000)
				{
					m68ki_cpu.fpu_just_reset = 0;
					// No register modification for displacement addressing
				}
				else
				{
					do_frestore_null();
				}
				break;

				default:
					fatalerror("M68kFPU: FRESTORE unhandled mode %d reg %d at %x\n", mode, reg, REG_PC);
			}
			break;
		}
		break;

		default:	fatalerror("m68040_fpu_op1: unimplemented op %d at %08X\n", (REG_IR >> 6) & 0x3, REG_PC-2);
	}
}



