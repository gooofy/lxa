/*
    Copyright (C) 2010, The AROS Development Team. All rights reserved.

    Desc: BCPL support

    Ported to lxa (Phase 239) from AROS arch/m68k-all/dos/bcpl.h; the
    structure offsets below replace AROS' generated <aros/m68k/asm.h>.
    This file is shared by bcpl.S and bcpl_support.c.  Licence: AROS
    Public License 1.1, see LICENSE in this directory.
*/
#ifndef LXA_BCPL_H
#define LXA_BCPL_H

#define BCPL_GlobVec_NegSize    0xb0
#define BCPL_GlobVec_PosSize    0x21c

/* Our BCPL stub private data */

#define GV_DEBUG_Result2        -0xac
#define GV_DOSBase              0x204   /* Not a function, but a value */

/* Structure offsets and LVOs used by the assembler code (NDK 3.x layout,
 * checked against offsetof() in bcpl_support.c) */
#define LVO_FindTask            -294
#define OFS_ThisTask            276     /* struct ExecBase */
#define OFS_pr_MsgPort          92      /* struct Process */
#define OFS_pr_SegList          128
#define OFS_pr_Result2          148
#define OFS_pr_CurrentDir       152
#define OFS_pr_ConsoleTask      164
#define OFS_pr_FileSystemTask   168
#define OFS_pr_ReturnAddr       176
#define OFS_dl_Root             34      /* struct DosLibrary */
#define OFS_dl_IntuitionBase    66
#define OFS_mp_SigTask          16      /* struct MsgPort */
#define OFS_ln_Name             10      /* struct Node */
#define OFS_dp_Type             8       /* struct DosPacket */
#define OFS_dp_Res1             12

#endif
