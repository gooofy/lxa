/*
 * lxa_override.h - library override mode (Phase 235, like WINEDLLOVERRIDES)
 *
 * DIAGNOSTIC ONLY.  LXA_OVERRIDE=asl,iffparse makes OpenLibrary() load the
 * user's own Workbench 3.1 disk-library binaries (LXA_OVERRIDE_DIR, default
 * the reference system's Libs/ in ~/.cache/lxa/refsys/SYS-aga/Libs) instead
 * of lxa's implementation, to bisect whether a divergence lives in lxa's
 * library or below it.  Only hardware-independent libraries that AmigaOS
 * 3.1 itself loads from disk may be overridden.  The binaries are the
 * user's; they are never committed or distributed, and lxa's own
 * implementations stay mandatory (AGENTS.md §1).
 */
#ifndef LXA_OVERRIDE_H
#define LXA_OVERRIDE_H

#include <stdbool.h>
#include <stddef.h>

void lxa_override_init(void);
/* name: "asl.library" (case-insensitive) -> true and the host path */
bool lxa_override_lookup(const char *name, char *path, size_t maxlen);
int  lxa_override_count(void);

#endif
