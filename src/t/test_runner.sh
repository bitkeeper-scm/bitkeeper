#!/bin/bash
set -e

# Bazel test runner for BitKeeper t.* tests.

if [ -z "$TEST_SRCDIR" ]; then
    echo "TEST_SRCDIR is not set" >&2
    exit 1
fi

TEST_NAME="$1"
if [ -z "$TEST_NAME" ]; then
    echo "Usage: test_runner.sh <t.testname>" >&2
    exit 1
fi

# Find the built bk binary and support tools/files in the runfiles tree
BK_BIN_DIR="$(cd "$TEST_SRCDIR/_main/src" && pwd)"
MAN_DIR="$(cd "$TEST_SRCDIR/_main/man" && pwd)"
DOC_DIR="$(cd "$TEST_SRCDIR/_main/doc" && pwd)"
ROOT_DIR="$(cd "$TEST_SRCDIR/_main" && pwd)"
SRC_DIR="$(cd "$TEST_SRCDIR/_main/src" && pwd)"
T_DIR="$(cd "$TEST_SRCDIR/_main/src/t" && pwd)"

# Setup isolated execution directory
WORK_DIR="${TEST_TMPDIR:-$(mktemp -d)}/bk_test_work"
mkdir -p "$WORK_DIR"
cd "$WORK_DIR"

# Stage runtime environment
BIN_DIR="$WORK_DIR/bin"
mkdir -p "$BIN_DIR"
cp -f "$BK_BIN_DIR/bk" "$BIN_DIR/bk"
chmod +x "$BIN_DIR/bk"

# Stage data and doc files alongside bk
cp -f "$SRC_DIR/bkmsg.txt" "$BIN_DIR/" 2>/dev/null || true
cp -f "$SRC_DIR/version" "$BIN_DIR/" 2>/dev/null || true
cp -f "$MAN_DIR/bkhelp.txt" "$BIN_DIR/" 2>/dev/null || true
cp -f "$DOC_DIR/bk_refcard.pdf" "$BIN_DIR/" 2>/dev/null || true
cp -f "$ROOT_DIR/RELEASE-NOTES.md" "$BIN_DIR/" 2>/dev/null || true

# Copy dspecs
for f in "$SRC_DIR"/dspec-*; do
    if [ -f "$f" ]; then
        cp -f "$f" "$BIN_DIR/"
    fi
done

# Copy wrapper scripts
for s in uuwrap unuuwrap gzip_uuwrap ungzip_uuwrap b64wrap unb64wrap gzip_b64wrap ungzip_b64wrap bk.script import; do
    if [ -f "$SRC_DIR/$s" ]; then
        cp -f "$SRC_DIR/$s" "$BIN_DIR/"
        chmod +x "$BIN_DIR/$s" 2>/dev/null || true
    fi
done

# Copy lscripts
if [ -d "$SRC_DIR/lscripts" ]; then
    mkdir -p "$BIN_DIR/lscripts"
    cp -rf "$SRC_DIR/lscripts"/* "$BIN_DIR/lscripts/" 2>/dev/null || true
fi

# Copy contrib
if [ -d "$SRC_DIR/contrib" ]; then
    mkdir -p "$BIN_DIR/contrib"
    cp -rf "$SRC_DIR/contrib"/* "$BIN_DIR/contrib/" 2>/dev/null || true
fi

# Copy www
if [ -d "$SRC_DIR/www" ]; then
    mkdir -p "$BIN_DIR/www"
    cp -rf "$SRC_DIR/www"/* "$BIN_DIR/www/" 2>/dev/null || true
fi

# Unpack GUI package (tclsh, bkgui, lib, images) into $BIN_DIR/gui
GUI_TAR="$TEST_SRCDIR/_main/src/gui/gui.tar.gz"
if [ -f "$GUI_TAR" ]; then
    tar -C "$BIN_DIR" -xzf "$GUI_TAR" 2>/dev/null || true
fi

# Copy t helper scripts / ref files to $BIN_DIR/t so tests can find them via `bk bin`/t
mkdir -p "$BIN_DIR/t"
cp -rf "$T_DIR"/* "$BIN_DIR/t/" 2>/dev/null || true

# Copy source file needed by t.fast-import
cp -f "$SRC_DIR/slib.c" "$BIN_DIR/slib.c" 2>/dev/null || true

# Copy generated cmd.c needed by t.a.simple-interface.*
if [ -f "$SRC_DIR/cmd.c" ]; then
    cp -f "$SRC_DIR/cmd.c" "$BIN_DIR/cmd.c" 2>/dev/null || true
fi

# Copy mtst if built into $BIN_DIR/libc/mtst
if [ -f "$TEST_SRCDIR/_main/src/libc/mtst" ]; then
    mkdir -p "$BIN_DIR/libc"
    cp -f "$TEST_SRCDIR/_main/src/libc/mtst" "$BIN_DIR/libc/mtst"
    chmod +x "$BIN_DIR/libc/mtst"
fi

export PATH="$BIN_DIR:$PATH"
export BK_ROOT="${BK_ROOT:-$ROOT_DIR}"
export TST_DIR="$WORK_DIR/tst"
mkdir -p "$TST_DIR"

# Stage doit script
sed -e 's,@TEST_SH@,/bin/bash,g' "$T_DIR/doit.sh" > "$WORK_DIR/doit"
chmod +x "$WORK_DIR/doit"

# Copy test file and setup into working test directory
TEST_SRC_DIR="$WORK_DIR/t_src"
mkdir -p "$TEST_SRC_DIR"
cp -f "$T_DIR/setup" "$TEST_SRC_DIR/"
cp -f "$T_DIR/$TEST_NAME" "$TEST_SRC_DIR/"
if [ -f "$T_DIR/simple-interface.setup" ]; then
    cp -f "$T_DIR/simple-interface.setup" "$TEST_SRC_DIR/"
fi

cd "$TEST_SRC_DIR"
# Run doit pointing to our temp TST_DIR, forwarding any extra options (-v, -x, etc.)
shift || true
"$WORK_DIR/doit" -t "$TST_DIR" "$@" ${BK_TEST_FLAGS} "$TEST_NAME"
