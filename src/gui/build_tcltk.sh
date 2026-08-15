#!/bin/bash
set -e

# Build in-tree Tcl/Tk and extensions for BitKeeper
# Outputs: $1 is destination directory or tarball path

OUT_TAR="$1"
shift
PCRE_LIB=""
PCRE_HDR=""
PCRE_HDRS=()
TOMMATH_FILES=()

for arg in "$@"; do
    case "$arg" in
        *tommath*|*bn_*|*bncore*)
            if [ -f "$arg" ]; then
                TOMMATH_FILES+=("$arg")
            fi
            ;;
        *.a)
            if [ -f "$arg" ]; then
                PCRE_LIB="$(cd "$(dirname "$arg")" && pwd)/$(basename "$arg")"
            fi
            ;;
        *.h)
            if [ -f "$arg" ]; then
                abs_h="$(cd "$(dirname "$arg")" && pwd)/$(basename "$arg")"
                PCRE_HDRS+=("$abs_h")
                if [ "$(basename "$arg")" = "pcre2.h" ] || [ "$(basename "$arg")" = "pcre.h" ]; then
                    PCRE_HDR="$abs_h"
                fi
            fi
            ;;
    esac
done

if [ -z "$OUT_TAR" ]; then
    echo "Usage: $0 <output_tar.tar.gz> [pcre_files...] [tommath_files...]" >&2
    exit 1
fi
OUT_TAR="$(cd "$(dirname "$OUT_TAR")" && pwd)/$(basename "$OUT_TAR")"

TOP="$PWD"
SRCDIR="$TOP/src/gui/tcltk"
WORK="$(mktemp -d)"

# Copy source tree (dereferencing symlinks so sandbox links don't modify repo)
cp -rL "$SRCDIR"/* "$WORK/"
mkdir -p "$WORK/tommath"
for f in "${TOMMATH_FILES[@]}"; do
    cp -f "$f" "$WORK/tommath/"
done
echo '#include "tclTomMathInt.h"' > "$WORK/tommath/tommath.h"

cd "$WORK"

mkdir -p bin lib include share

# 1. Setup PCRE from Bazel @pcre2
if [ -z "$PCRE_LIB" ] || [ -z "$PCRE_HDR" ]; then
    echo "Error: PCRE library and header must be provided to $0" >&2
    exit 1
fi
mkdir -p "$WORK/pcre_dist/include" "$WORK/pcre_dist/lib"
for h in "${PCRE_HDRS[@]}"; do
    cp -f "$h" "$WORK/pcre_dist/include/"
done
cp -f "$PCRE_HDR" "$WORK/pcre_dist/include/pcre2.h"
cp -f "$PCRE_HDR" "$WORK/pcre_dist/include/pcre.h"
cp -f "$PCRE_LIB" "$WORK/pcre_dist/lib/libpcre2.a"
cp -f "$PCRE_LIB" "$WORK/pcre_dist/lib/libpcre.a"
PCRE_PREFIX="$WORK/pcre_dist"
PCRE_A="$WORK/pcre_dist/lib/libpcre.a"

# 2. Generate L version files
(cd tcl && ../Lversion-L.sh > library/Lver.tcl && ../Lversion-C.sh > generic/Lver.h)

# 3. Build Tcl
(
    cd tcl/unix
    ./configure \
        --enable-pcre=default \
        --with-pcre="$PCRE_PREFIX" \
        --enable-64bit \
        --disable-shared \
        --with-tommath="$WORK/tommath" \
        CFLAGS="-g -O2 -Wno-incompatible-pointer-types -Wno-int-conversion"
    make -j"$(nproc 2>/dev/null || echo 2)" Q= prefix= exec_prefix= INSTALL_ROOT="$WORK" XLIBS="$PCRE_A" install-binaries install-libraries
)

if [ -f "$WORK/bin/tclsh8.6" ]; then
    mv -f "$WORK/bin/tclsh8.6" "$WORK/bin/tclsh"
fi
chmod +x "$WORK/bin/tclsh" 2>/dev/null || true

# 4. Try building Tk (if X11/Aqua/Win is available)
set +e
(
    cd tk/unix
    ./configure \
        --with-tcl=../../tcl/unix \
        --enable-64bit \
        --disable-xss \
        --enable-xft \
        --disable-shared \
        CFLAGS="-g -O2 -Wno-incompatible-pointer-types -Wno-int-conversion" && \
    make -j"$(nproc 2>/dev/null || echo 2)" prefix= exec_prefix= INSTALL_ROOT="$WORK" XLIBS="$PCRE_A" BK_TCL_LIB="$WORK/tcl/unix/libtcl8.6.a" install-binaries install-libraries
)
if [ -f "$WORK/bin/wish8.6" ]; then
    mv -f "$WORK/bin/wish8.6" "$WORK/bin/bkgui"
elif [ -f "$WORK/bin/wish" ]; then
    mv -f "$WORK/bin/wish" "$WORK/bin/bkgui"
fi
chmod +x "$WORK/bin/bkgui" 2>/dev/null || true
set -e

# 5. Copy BWidget
BWIDGET="BWidget1.8"
mkdir -p "$WORK/lib/$BWIDGET/lang" "$WORK/lib/$BWIDGET/images"
cp -f bwidget/*.tcl "$WORK/lib/$BWIDGET/" 2>/dev/null || true
cp -f bwidget/lang/*.rc "$WORK/lib/$BWIDGET/lang/" 2>/dev/null || true
cp -f bwidget/images/*.gif "$WORK/lib/$BWIDGET/images/" 2>/dev/null || true
cp -f bwidget/images/*.xbm "$WORK/lib/$BWIDGET/images/" 2>/dev/null || true

# 6. Copy Tkcon
mkdir -p "$WORK/lib/Tkcon"
cp -f tkcon/tkcon.tcl "$WORK/lib/Tkcon/" 2>/dev/null || true
cp -f tkcon/pkgIndex.tcl "$WORK/lib/Tkcon/" 2>/dev/null || true

# Clean up unwanted artifacts
rm -rf "$WORK/include" "$WORK/share" "$WORK/man" "$WORK/lib/tcl8.6/tcltest"*

# 7. Package bin and lib
cd "$WORK"
tar -czf "$OUT_TAR" bin lib

rm -rf "$WORK"
