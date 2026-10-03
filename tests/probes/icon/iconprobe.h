/*
 * iconprobe.h - shared helpers for the icon.library probes (Phase 222f):
 * print every DiskObject field (pointers only as NULL/non-NULL) and dump
 * written .info files with pointer fields masked.
 */
#ifndef LXA_ICONPROBE_H
#define LXA_ICONPROBE_H

#include <exec/types.h>
#include <exec/memory.h>
#include <intuition/intuition.h>
#include <workbench/workbench.h>
#include <clib/exec_protos.h>
#include <clib/icon_protos.h>
#include <inline/exec.h>
#include <inline/icon.h>
#include "probe.h"

extern struct ExecBase *SysBase;
extern struct Library *IconBase;

static void p_kv(const char *k, LONG v)
{
    probe_s(" ");
    probe_s(k);
    probe_s("=");
    probe_dec(v);
}

static void p_kh(const char *k, ULONG v, int digits)
{
    probe_s(" ");
    probe_s(k);
    probe_s("=");
    probe_hex(v, digits);
}

static void p_kp(const char *k, const void *p)
{
    probe_s(" ");
    probe_s(k);
    probe_s(p ? "=ptr" : "=NULL");
}

static void p_image(const char *label, struct Image *im)
{
    probe_s("  ");
    probe_s(label);
    probe_s(":");
    if (!im) {
        probe_s(" NULL\n");
        return;
    }
    p_kv("left", im->LeftEdge);
    p_kv("top", im->TopEdge);
    p_kv("w", im->Width);
    p_kv("h", im->Height);
    p_kv("depth", im->Depth);
    p_kh("pick", im->PlanePick, 2);
    p_kh("onoff", im->PlaneOnOff, 2);
    p_kp("data", im->ImageData);
    p_kp("next", im->NextImage);
    if (im->ImageData && im->Width > 0 && im->Height > 0 && im->Depth > 0) {
        LONG bytes = ((im->Width + 15) / 16) * 2 * im->Height * im->Depth;
        p_kh("hash", probe_hash(im->ImageData, bytes), 8);
    }
    probe_ch('\n');
}

static void p_strfield(const char *label, const char *s)
{
    probe_s("  ");
    probe_s(label);
    if (s) {
        probe_s(" = \"");
        probe_s(s);
        probe_s("\"\n");
    } else
        probe_s(" = NULL\n");
}

static void p_diskobj(const char *label, struct DiskObject *d)
{
    struct Gadget *g;
    probe_s(label);
    if (!d) {
        probe_s(": NULL");
        p_kv("IoErr", IoErr());
        probe_ch('\n');
        return;
    }
    probe_s(":");
    p_kh("magic", d->do_Magic, 4);
    p_kv("version", d->do_Version);
    p_kv("type", d->do_Type);
    p_kv("stack", d->do_StackSize);
    p_kh("curx", (ULONG)d->do_CurrentX, 8);
    p_kh("cury", (ULONG)d->do_CurrentY, 8);
    probe_ch('\n');
    g = &d->do_Gadget;
    probe_s("  gadget:");
    p_kp("next", g->NextGadget);
    p_kv("left", g->LeftEdge);
    p_kv("top", g->TopEdge);
    p_kv("w", g->Width);
    p_kv("h", g->Height);
    p_kh("flags", g->Flags, 4);
    p_kh("act", g->Activation, 4);
    p_kh("gtype", g->GadgetType, 4);
    p_kp("text", g->GadgetText);
    p_kh("mutex", (ULONG)g->MutualExclude, 8);
    p_kp("special", g->SpecialInfo);
    p_kv("id", g->GadgetID);
    p_kh("userdata", (ULONG)g->UserData, 8);
    probe_ch('\n');
    p_image("render", (struct Image *)g->GadgetRender);
    p_image("select", (struct Image *)g->SelectRender);
    p_strfield("defaulttool", (const char *)d->do_DefaultTool);
    if (d->do_ToolTypes) {
        STRPTR *tt = d->do_ToolTypes;
        LONG n = 0;
        while (tt[n]) {
            probe_s("  tooltype[");
            probe_dec(n);
            probe_s("] = \"");
            probe_s((const char *)tt[n]);
            probe_s("\"\n");
            n++;
        }
        P_LONG("  tooltypes count", n);
    } else
        probe_s("  tooltypes = NULL\n");
    p_strfield("toolwindow", (const char *)d->do_ToolWindow);
    if (d->do_DrawerData) {
        struct DrawerData *dd = d->do_DrawerData;
        struct NewWindow *nw = &dd->dd_NewWindow;
        probe_s("  drawer:");
        p_kv("left", nw->LeftEdge);
        p_kv("top", nw->TopEdge);
        p_kv("w", nw->Width);
        p_kv("h", nw->Height);
        p_kv("dpen", nw->DetailPen);
        p_kv("bpen", nw->BlockPen);
        p_kh("idcmp", nw->IDCMPFlags, 8);
        p_kh("wflags", nw->Flags, 8);
        p_kp("fgad", nw->FirstGadget);
        p_kp("check", nw->CheckMark);
        p_kp("title", nw->Title);
        p_kp("scr", nw->Screen);
        p_kp("bm", nw->BitMap);
        p_kv("minw", nw->MinWidth);
        p_kv("minh", nw->MinHeight);
        p_kv("maxw", nw->MaxWidth);
        p_kv("maxh", nw->MaxHeight);
        p_kv("wtype", nw->Type);
        p_kv("cx", dd->dd_CurrentX);
        p_kv("cy", dd->dd_CurrentY);
        probe_ch('\n');
        probe_s("  drawer2:");
        p_kh("ddflags", dd->dd_Flags, 8);
        p_kh("viewmodes", dd->dd_ViewModes, 4);
        probe_ch('\n');
    } else
        probe_s("  drawer = NULL\n");
}

/* --- raw .info dump ---------------------------------------------------- */

static UBYTE info_buf[4096];

static ULONG be32(const UBYTE *p)
{
    return ((ULONG)p[0] << 24) | ((ULONG)p[1] << 16) | ((ULONG)p[2] << 8) | p[3];
}

static UWORD be16(const UBYTE *p)
{
    return (UWORD)((p[0] << 8) | p[1]);
}

/* replace a pointer field by 00000000 or 00000001 */
static void mask_ptr(UBYTE *p)
{
    ULONG v = be32(p);
    p[0] = p[1] = p[2] = 0;
    p[3] = v ? 1 : 0;
}

static void dump_hex(const char *label, const UBYTE *p, LONG len)
{
    LONG off = 0;
    while (off < len) {
        LONG n = len - off > 32 ? 32 : len - off;
        probe_s("  ");
        probe_s(label);
        probe_s("+");
        probe_dec(off);
        P_BYTES("", p + off, n);
        off += n;
    }
}

static LONG walk_image(const UBYTE *b, LONG pos, LONG size, int num)
{
    UBYTE im[20];
    LONG bytes;
    if (pos + 20 > size) {
        probe_s("  image truncated\n");
        return size;
    }
    CopyMem((APTR)(b + pos), im, 20);
    mask_ptr(im + 10);              /* ImageData */
    mask_ptr(im + 16);              /* NextImage */
    probe_s("  image");
    probe_dec(num);
    P_BYTES(" struct", im, 20);
    pos += 20;
    bytes = (((WORD)be16(im + 4) + 15) / 16) * 2 * (WORD)be16(im + 6) * (WORD)be16(im + 8);
    if (bytes < 0 || pos + bytes > size) {
        probe_s("  image data truncated\n");
        return size;
    }
    probe_s("  image");
    probe_dec(num);
    p_kv("databytes", bytes);
    p_kh("hash", probe_hash(b + pos, bytes), 8);
    probe_ch('\n');
    return pos + bytes;
}

/* read <name>.info and print it, pointer fields masked */
static void dump_info(const char *name)
{
    char path[128];
    BPTR fh;
    LONG size, pos;
    UBYTE hdr[78];
    int i = 0;
    while (name[i] && i < 100) {
        path[i] = name[i];
        i++;
    }
    path[i] = 0;
    {
        const char *s = ".info";
        while (*s)
            path[i++] = *s++;
        path[i] = 0;
    }
    fh = Open((STRPTR)path, MODE_OLDFILE);
    if (!fh) {
        probe_s("  file: missing\n");
        return;
    }
    size = Read(fh, info_buf, sizeof(info_buf));
    Close(fh);
    P_LONG("  file size", size);
    if (size < 78) {
        dump_hex("raw", info_buf, size);
        return;
    }
    CopyMem(info_buf, hdr, 78);
    mask_ptr(hdr + 4);              /* NextGadget */
    mask_ptr(hdr + 22);             /* GadgetRender */
    mask_ptr(hdr + 26);             /* SelectRender */
    mask_ptr(hdr + 30);             /* GadgetText */
    mask_ptr(hdr + 38);             /* SpecialInfo */
    mask_ptr(hdr + 50);             /* DefaultTool */
    mask_ptr(hdr + 54);             /* ToolTypes */
    mask_ptr(hdr + 66);             /* DrawerData */
    mask_ptr(hdr + 70);             /* ToolWindow */
    dump_hex("hdr", hdr, 78);
    pos = 78;
    if (be32(info_buf + 66)) {
        UBYTE dd[56];
        if (pos + 56 > size) {
            probe_s("  drawerdata truncated\n");
            return;
        }
        CopyMem(info_buf + pos, dd, 56);
        mask_ptr(dd + 18);          /* FirstGadget */
        mask_ptr(dd + 22);          /* CheckMark */
        mask_ptr(dd + 26);          /* Title */
        mask_ptr(dd + 30);          /* Screen */
        mask_ptr(dd + 34);          /* BitMap */
        dump_hex("drawer", dd, 56);
        pos += 56;
    }
    if (be32(info_buf + 22))
        pos = walk_image(info_buf, pos, size, 1);
    if (be32(info_buf + 26))
        pos = walk_image(info_buf, pos, size, 2);
    if (pos < size)
        dump_hex("tail", info_buf + pos, size - pos);
}

#endif
