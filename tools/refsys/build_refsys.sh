#!/usr/bin/env bash
# build_refsys.sh - build the RDD reference system (Phase 210)
#
# Builds a FRESH AmigaOS 3.1 hard-disk system from the user's original
# Workbench 3.1 ADFs, matching roadmap "Canonical Reference Configuration":
#
#   Workbench 3.1 (40.42) = Workbench + Extras + Storage + Locale + Fonts
#   + 68040.library from the Install disk
#   + HD Startup-Sequence with the LXAREF: hook (boot handshake, lxaprobe)
#   + [profile rtg] Picasso96 + uaegfx monitor (needs a Picasso96 that
#     supports FS-UAE's built-in uaegfx board, see roadmap Phase 210)
#
# The build is idempotent: it assembles into a staging directory, computes a
# content checksum and only replaces <out>/SYS when the content changed.
#
#   tools/refsys/build_refsys.sh [--profile aga|rtg] [--out DIR] [--force]
#
# Environment overrides: LXA_REF_OS_DIR (default ~/media/sys/amiga/os/3.1),
# LXA_REF_P96_DIR (Picasso96 2.x install tree for the rtg profile; default:
# the Picasso96 2.0 contribution on the user's AmigaOS 3.9 media) or
# LXA_REF_P96_ADF (a Picasso96 install ADF).
set -euo pipefail

PROFILE=aga
OUT=${LXA_REFSYS_DIR:-$HOME/.cache/lxa/refsys}
FORCE=0
while [ $# -gt 0 ]; do
    case $1 in
        --profile) PROFILE=$2; shift 2 ;;
        --out) OUT=$2; shift 2 ;;
        --force) FORCE=1; shift ;;
        -h|--help) sed -n '2,21p' "$0"; exit 0 ;;
        *) echo "unknown option $1" >&2; exit 2 ;;
    esac
done

HERE=$(cd "$(dirname "$0")" && pwd)
OS_DIR=${LXA_REF_OS_DIR:-$HOME/media/sys/amiga/os/3.1}
P96_DIR=${LXA_REF_P96_DIR:-/mnt/dagobert-home/media/sys/amiga/os/AmigaOS39/Contribution/Picasso96Install}
P96_ADF=${LXA_REF_P96_ADF:-}

die() { echo "build_refsys: $*" >&2; exit 1; }
command -v xdftool >/dev/null || die "xdftool (amitools) not found"

adf() {
    local f
    f=$(ls "$OS_DIR"/"Workbench v3.1 rev 40.42"*"Disk $1 of 6"*.adf 2>/dev/null | head -1)
    [ -n "$f" ] || die "Workbench 3.1 disk $1 not found in $OS_DIR"
    echo "$f"
}

mkdir -p "$OUT"
STAGE=$(mktemp -d "$OUT/stage.XXXXXX")
trap 'rm -rf "$STAGE"' EXIT
WORK=$STAGE/work
SYS=$STAGE/SYS
mkdir -p "$WORK" "$SYS"

# --- 1. unpack the six Workbench 3.1 disks ---------------------------------
for n in 1 2 3 4 5 6; do
    mkdir -p "$WORK/d$n"
    xdftool "$(adf $n)" unpack "$WORK/d$n" >/dev/null
done
vol() { find "$WORK/d$1" -mindepth 1 -maxdepth 1 -type d | head -1; }

# --- 2. merge into one system partition --------------------------------------
# Workbench disk is the base; Extras adds Tools/Prefs/System/...; Storage,
# Locale and Fonts become SYS:Storage, SYS:Locale, SYS:Fonts.
cp -a "$(vol 2)"/. "$SYS"/
cp -a "$(vol 3)"/. "$SYS"/
mkdir -p "$SYS/Storage" "$SYS/Locale" "$SYS/Fonts"
cp -a "$(vol 4)"/. "$SYS/Storage"/
cp -a "$(vol 5)"/. "$SYS/Locale"/
cp -a "$(vol 6)"/. "$SYS/Fonts"/
# Storage carries its own disk icon / bootblock files; not needed on the HD
rm -f "$SYS/Storage/Disk.info" "$SYS/Locale/Disk.info" "$SYS/Fonts/Disk.info"
# the 68040 CPU library from the Install disk
lib040=$(find "$WORK/d1" -iname 68040.library | head -1)
[ -n "$lib040" ] || die "68040.library not found on the Install disk"
cp "$lib040" "$SYS/Libs/68040.library"
mkdir -p "$SYS/T"

# --- 3. HD Startup-Sequence with the LXAREF: hook ----------------------------
cp "$HERE/Startup-Sequence" "$SYS/S/Startup-Sequence"

# --- 4. profile rtg: Picasso96 ------------------------------------------------
if [ "$PROFILE" = rtg ]; then
    if [ -n "$P96_ADF" ]; then
        mkdir -p "$WORK/p96"
        xdftool "$P96_ADF" unpack "$WORK/p96" >/dev/null
        python3 "$HERE/install_p96.py" "$WORK/p96" "$SYS"
    elif [ -d "$P96_DIR/Libs" ]; then
        python3 "$HERE/install_p96.py" "$P96_DIR" "$SYS"
    else
        die "profile rtg needs Picasso96 2.x (LXA_REF_P96_DIR or LXA_REF_P96_ADF)"
    fi
elif [ "$PROFILE" != aga ]; then
    die "unknown profile $PROFILE (aga|rtg)"
fi

# lxaprobe guest agent (Phase 211), if built
AGENT=$HERE/agent/lxaprobe
[ -f "$AGENT" ] && cp "$AGENT" "$SYS/C/lxaprobe"

echo "$PROFILE" > "$SYS/S/lxaref-profile"

# --- 5. checksum + atomic install --------------------------------------------
sum=$(cd "$SYS" && find . -type f -print0 | LC_ALL=C sort -z | xargs -0 sha256sum | sha256sum | cut -c1-16)
dest=$OUT/SYS-$PROFILE
if [ $FORCE = 0 ] && [ -f "$dest/.refsys-checksum" ] && [ "$(cat "$dest/.refsys-checksum")" = "$sum" ]; then
    echo "build_refsys: $dest up to date ($sum)"
    exit 0
fi
echo "$sum" > "$SYS/.refsys-checksum"
rm -rf "$dest.old"
[ -d "$dest" ] && mv "$dest" "$dest.old"
mv "$SYS" "$dest"
rm -rf "$dest.old"
echo "build_refsys: built $dest (profile $PROFILE, checksum $sum)"
