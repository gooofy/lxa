/*
 * REQUESTFILE - ask for a file name with the ASL file requester
 *
 * Template: DRAWER,FILE/K,PATTERN/K,TITLE/K,POSITIVE/K,NEGATIVE/K,
 *           ACCEPTPATTERN/K,REJECTPATTERN/K,SAVEMODE/S,MULTISELECT/S,
 *           DRAWERSONLY/S,NOICONS/S,PUBSCREEN/K,INITIALVOLUMES/S
 *
 * Prints the chosen name in quotes ("SYS:S/Startup-Sequence"; with
 * MULTISELECT every selected name, separated by spaces), RC 0; RC 5 when
 * the requester is cancelled, RC 20 if it cannot be opened.
 */

#include <exec/types.h>
#include <exec/memory.h>
#include <dos/dos.h>
#include <libraries/asl.h>
#include <workbench/startup.h>
#include <utility/tagitem.h>
#include <clib/exec_protos.h>
#include <clib/dos_protos.h>
#include <clib/asl_protos.h>
#include <inline/exec.h>
#include <inline/dos.h>
#include <inline/asl.h>

#include <string.h>

extern struct DosLibrary *DOSBase;
extern struct ExecBase *SysBase;
struct Library *AslBase;

#define TEMPLATE "DRAWER,FILE/K,PATTERN/K,TITLE/K,POSITIVE/K,NEGATIVE/K,ACCEPTPATTERN/K," \
                 "REJECTPATTERN/K,SAVEMODE/S,MULTISELECT/S,DRAWERSONLY/S,NOICONS/S,PUBSCREEN/K," \
                 "INITIALVOLUMES/S"

enum { A_DRAWER, A_FILE, A_PATTERN, A_TITLE, A_POSITIVE, A_NEGATIVE, A_ACCEPT, A_REJECT,
       A_SAVEMODE, A_MULTI, A_DRAWERSONLY, A_NOICONS, A_PUBSCREEN, A_VOLUMES, A_COUNT };

static void print_name(const char *drawer, const char *file, BOOL first)
{
    char path[512];

    strncpy(path, drawer ? drawer : "", sizeof(path) - 1);
    path[sizeof(path) - 1] = '\0';
    if (file && *file)
        AddPart((STRPTR)path, (STRPTR)file, sizeof(path));
    Printf((STRPTR)(first ? "\"%s\"" : " \"%s\""), (LONG)path);
}

int main(void)
{
    LONG args[A_COUNT];
    struct RDArgs *rda;
    struct FileRequester *fr;
    struct TagItem tags[16];
    int t = 0;
    LONG rc = 0;
    static char accept[256], reject[256];

    memset(args, 0, sizeof(args));
    rda = ReadArgs((STRPTR)TEMPLATE, args, NULL);
    if (!rda) {
        PrintFault(IoErr(), NULL);
        return RETURN_FAIL;
    }
    AslBase = OpenLibrary((STRPTR)"asl.library", 37);
    if (!AslBase) {
        PrintFault(IoErr(), (STRPTR)"asl.library");
        FreeArgs(rda);
        return RETURN_FAIL;
    }

#define TAG(a, b) do { tags[t].ti_Tag = (a); tags[t].ti_Data = (ULONG)(b); t++; } while (0)
    if (args[A_DRAWER])      TAG(ASLFR_InitialDrawer, args[A_DRAWER]);
    if (args[A_FILE])        TAG(ASLFR_InitialFile, args[A_FILE]);
    if (args[A_PATTERN]) {
        TAG(ASLFR_InitialPattern, args[A_PATTERN]);
        TAG(ASLFR_DoPatterns, TRUE);
    }
    if (args[A_TITLE])       TAG(ASLFR_TitleText, args[A_TITLE]);
    if (args[A_POSITIVE])    TAG(ASLFR_PositiveText, args[A_POSITIVE]);
    if (args[A_NEGATIVE])    TAG(ASLFR_NegativeText, args[A_NEGATIVE]);
    if (args[A_ACCEPT] && ParsePatternNoCase((STRPTR)args[A_ACCEPT], (STRPTR)accept, sizeof(accept)) >= 0)
        TAG(ASLFR_AcceptPattern, accept);
    if (args[A_REJECT] && ParsePatternNoCase((STRPTR)args[A_REJECT], (STRPTR)reject, sizeof(reject)) >= 0)
        TAG(ASLFR_RejectPattern, reject);
    if (args[A_SAVEMODE])    TAG(ASLFR_DoSaveMode, TRUE);
    if (args[A_MULTI])       TAG(ASLFR_DoMultiSelect, TRUE);
    if (args[A_DRAWERSONLY]) TAG(ASLFR_DrawersOnly, TRUE);
    if (args[A_NOICONS])     TAG(ASLFR_RejectIcons, TRUE);
    if (args[A_PUBSCREEN])   TAG(ASLFR_PubScreenName, args[A_PUBSCREEN]);
    TAG(TAG_DONE, 0);
#undef TAG

    fr = (struct FileRequester *)AllocAslRequest(ASL_FileRequest, tags);
    if (!fr) {
        PrintFault(ERROR_NO_FREE_STORE, NULL);
        rc = RETURN_FAIL;
    } else {
        if (AslRequest(fr, NULL)) {
            if (args[A_MULTI] && fr->fr_NumArgs > 0) {
                LONG i;
                for (i = 0; i < fr->fr_NumArgs; i++)
                    print_name((char *)fr->fr_Drawer, (char *)fr->fr_ArgList[i].wa_Name, i == 0);
            } else {
                print_name((char *)fr->fr_Drawer, (char *)fr->fr_File, TRUE);
            }
            PutStr((STRPTR)"\n");
        } else {
            rc = RETURN_WARN;
        }
        FreeAslRequest(fr);
    }
    CloseLibrary(AslBase);
    FreeArgs(rda);
    return rc;
}
