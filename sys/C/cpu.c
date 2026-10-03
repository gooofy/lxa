/*
 * CPU - show and set the processor's cache modes
 *
 * Template: CACHE/S,BURST/S,NOCACHE/S,NOBURST/S,DATACACHE/S,DATABURST/S,
 *           NODATACACHE/S,NODATABURST/S,INSTCACHE/S,INSTBURST/S,
 *           NOINSTCACHE/S,NOINSTBURST/S,COPYBACK/S,NOCOPYBACK/S,
 *           EXTERNALCACHE/S,NOEXTERNALCACHE/S,FASTROM/S,NOFASTROM/S,
 *           TRAP/S,NOTRAP/S,NOMMUTEST/S,CHECK/K
 *
 * Output of AmigaOS 3.1 (verified on the reference, Phase 221):
 *   System: 68040 68882 (INST: Cache Burst) (DATA: Cache CopyBack)
 * The processor and FPU come from SysBase->AttnFlags, the cache modes
 * from CacheControl(); the switches change them through CacheControl().
 * CHECK 68010|68020|68030|68040|68881|68882|FPU|MMU returns RC 5 when
 * the part is missing.  FASTROM, TRAP and EXTERNALCACHE need an MMU or
 * hardware lxa does not emulate and are accepted without effect.
 */

#include <exec/types.h>
#include <exec/execbase.h>
#include <dos/dos.h>
#include <clib/exec_protos.h>
#include <clib/dos_protos.h>
#include <inline/exec.h>
#include <inline/dos.h>

#include <string.h>

extern struct DosLibrary *DOSBase;
extern struct ExecBase *SysBase;

#define TEMPLATE "CACHE/S,BURST/S,NOCACHE/S,NOBURST/S,DATACACHE/S,DATABURST/S,NODATACACHE/S," \
                 "NODATABURST/S,INSTCACHE/S,INSTBURST/S,NOINSTCACHE/S,NOINSTBURST/S,COPYBACK/S," \
                 "NOCOPYBACK/S,EXTERNALCACHE/S,NOEXTERNALCACHE/S,FASTROM/S,NOFASTROM/S,TRAP/S," \
                 "NOTRAP/S,NOMMUTEST/S,CHECK/K"

enum { A_CACHE, A_BURST, A_NOCACHE, A_NOBURST, A_DATACACHE, A_DATABURST, A_NODATACACHE,
       A_NODATABURST, A_INSTCACHE, A_INSTBURST, A_NOINSTCACHE, A_NOINSTBURST, A_COPYBACK,
       A_NOCOPYBACK, A_EXTERNALCACHE, A_NOEXTERNALCACHE, A_FASTROM, A_NOFASTROM, A_TRAP,
       A_NOTRAP, A_NOMMUTEST, A_CHECK, A_COUNT };

#ifndef CACRF_EnableI
#define CACRF_EnableI      (1L << 0)
#define CACRF_IBE          (1L << 4)
#define CACRF_EnableD      (1L << 8)
#define CACRF_DBE          (1L << 12)
#define CACRF_CopyBack     (1L << 31)
#endif

static const char *cpu_name(UWORD attn)
{
    if (attn & AFF_68040)
        return "68040";
    if (attn & AFF_68030)
        return "68030";
    if (attn & AFF_68020)
        return "68020";
    if (attn & AFF_68010)
        return "68010";
    return "68000";
}

static const char *fpu_name(UWORD attn)
{
    if (attn & AFF_FPU40)
        return (attn & AFF_68882) ? " 68882" : " 68040FPU";
    if (attn & AFF_68882)
        return " 68882";
    if (attn & AFF_68881)
        return " 68881";
    return "";
}

int main(void)
{
    LONG args[A_COUNT];
    struct RDArgs *rda;
    UWORD attn = SysBase->AttnFlags;
    ULONG set = 0, clear = 0, cacr;
    LONG rc = 0;

    memset(args, 0, sizeof(args));
    rda = ReadArgs((STRPTR)TEMPLATE, args, NULL);
    if (!rda) {
        PrintFault(IoErr(), NULL);
        return RETURN_FAIL;
    }

    if (args[A_CACHE])        set |= CACRF_EnableI | CACRF_EnableD;
    if (args[A_BURST])        set |= CACRF_IBE | CACRF_DBE;
    if (args[A_NOCACHE])      clear |= CACRF_EnableI | CACRF_EnableD;
    if (args[A_NOBURST])      clear |= CACRF_IBE | CACRF_DBE;
    if (args[A_DATACACHE])    set |= CACRF_EnableD;
    if (args[A_DATABURST])    set |= CACRF_DBE;
    if (args[A_NODATACACHE])  clear |= CACRF_EnableD;
    if (args[A_NODATABURST])  clear |= CACRF_DBE;
    if (args[A_INSTCACHE])    set |= CACRF_EnableI;
    if (args[A_INSTBURST])    set |= CACRF_IBE;
    if (args[A_NOINSTCACHE])  clear |= CACRF_EnableI;
    if (args[A_NOINSTBURST])  clear |= CACRF_IBE;
    if (args[A_COPYBACK])     set |= CACRF_CopyBack;
    if (args[A_NOCOPYBACK])   clear |= CACRF_CopyBack;
    if (set | clear)
        CacheControl(set & ~clear, set | clear);

    if (args[A_CHECK]) {
        const char *c = (char *)args[A_CHECK];
        BOOL have = TRUE;
        if (!stricmp(c, "68010"))
            have = (attn & AFF_68010) != 0;
        else if (!stricmp(c, "68020"))
            have = (attn & AFF_68020) != 0;
        else if (!stricmp(c, "68030"))
            have = (attn & AFF_68030) != 0;
        else if (!stricmp(c, "68040"))
            have = (attn & AFF_68040) != 0;
        else if (!stricmp(c, "68881"))
            have = (attn & AFF_68881) != 0;
        else if (!stricmp(c, "68882"))
            have = (attn & AFF_68882) != 0;
        else if (!stricmp(c, "FPU"))
            have = (attn & (AFF_68881 | AFF_68882 | AFF_FPU40)) != 0;
        else if (!stricmp(c, "MMU"))
            have = (attn & (AFF_68030 | AFF_68040)) != 0;
        if (!have)
            rc = RETURN_WARN;
    }

    cacr = CacheControl(0, 0);
    Printf((STRPTR)"System: %s%s (INST: %s %s) (DATA: %s %s)\n",
           (LONG)cpu_name(attn), (LONG)fpu_name(attn),
           (LONG)((cacr & CACRF_EnableI) ? "Cache" : "NoCache"),
           (LONG)((cacr & CACRF_IBE) ? "Burst" : "NoBurst"),
           (LONG)((cacr & CACRF_EnableD) ? "Cache" : "NoCache"),
           (LONG)((cacr & CACRF_CopyBack) ? "CopyBack" : (cacr & CACRF_DBE) ? "Burst" : "NoBurst"));
    FreeArgs(rda);
    return rc;
}
