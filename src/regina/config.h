/*
 * config.h for building Regina with m68k-amigaos-gcc and libnix (lxa,
 * Phase 250).  Written by hand instead of running configure; see
 * README.lxa.
 */
#ifndef REGINA_LXA_CONFIG_H
#define REGINA_LXA_CONFIG_H

#define LXA_AMIGA 1
#ifndef _AMIGA
# define _AMIGA 1
#endif
#define __AMIGADATE__ "(01.11.2026)"

#define STDC_HEADERS 1
#define TIME_WITH_SYS_TIME 1

#define HAVE_ASSERT_H 1
#define HAVE_CTYPE_H 1
#define HAVE_ERRNO_H 1
#define HAVE_FCNTL_H 1
#define HAVE_LIMITS_H 1
#define HAVE_MATH_H 1
#define HAVE_MEMORY_H 1
#define HAVE_SETJMP_H 1
#define HAVE_SIGNAL_H 1
#define HAVE_STDARG_H 1
#define HAVE_STDIO_H 1
#define HAVE_STDLIB_H 1
#define HAVE_STRING_H 1
#define HAVE_SYS_STAT_H 1
#define HAVE_SYS_TIME_H 1
#define HAVE_SYS_TYPES_H 1
#define HAVE_TIME_H 1
#define HAVE_UNISTD_H 1

#define HAVE_DIV 1
#define HAVE_MEMCPY 1
#define HAVE_MEMMOVE 1
#define HAVE_STRERROR 1
#define HAVE_VSPRINTF 1
#define HAVE_GETTIMEOFDAY 1
#define HAVE_PUTENV 1


#define REGINA_VERSION_DATE "25 Jun 2022"
#define REGINA_VERSION_MAJOR "3"
#define REGINA_VERSION_MINOR "9"
#define REGINA_VERSION_RELEASE "6"
#define REGINA_VERSION_SUPP ""

#define NO_EXTERNAL_QUEUES 1
#define REGINA_BITS 32

#define REGINA_SHARE_DIRECTORY "SYS:Rexxc"

#endif
