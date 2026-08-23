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

# Find the built BitKeeper install directory and test files in the runfiles tree
BK_BIN_DIR="$(cd "$TEST_SRCDIR/_main/bitkeeper" && pwd)"
ROOT_DIR="$(cd "$TEST_SRCDIR/_main" && pwd)"
SRC_DIR="$(cd "$TEST_SRCDIR/_main/src" && pwd)"
T_DIR="$(cd "$TEST_SRCDIR/_main/src/t" && pwd)"

# Setup isolated execution directory
WORK_DIR="${TEST_TMPDIR:-$(mktemp -d)}/bk_test_work"
mkdir -p "$WORK_DIR"
cd "$WORK_DIR"

# Stage runtime environment from the assembled bitkeeper install directory
BIN_DIR="$WORK_DIR/bin"
mkdir -p "$BIN_DIR"
cp -r -L "$BK_BIN_DIR"/* "$BIN_DIR/"
chmod +x "$BIN_DIR/bk"


# On macOS, the system-provided "patch" and "diff3" (from Apple/BSD) behave
# differently than the GNU versions BitKeeper expects (e.g. fuzzy-match
# behavior in `patch`, and `-E` conflict-marker behavior in `diff3`). "bk
# patch" and "bk merge" invoke these tools by bare name via $PATH, so
# symlink the Homebrew-provided GNU versions into $BIN_DIR (which is
# prepended to PATH below) when available, without disturbing the user's
# real PATH or requiring "gnubin" directories to be added globally.
for gnubin_dir in \
    /opt/homebrew/opt/gpatch/libexec/gnubin \
    /usr/local/opt/gpatch/libexec/gnubin
do
    if [ -x "$gnubin_dir/patch" ]; then
        ln -sf "$gnubin_dir/patch" "$BIN_DIR/patch"
        break
    fi
done
for diffutils_bin in \
    /opt/homebrew/opt/diffutils/bin \
    /usr/local/opt/diffutils/bin
do
    if [ -x "$diffutils_bin/diff3" ]; then
        # GNU diff3 shells out to "diff" as a subsidiary program, so it
        # must find the GNU diff (not the BSD one) on PATH too.
        ln -sf "$diffutils_bin/diff3" "$BIN_DIR/diff3"
        ln -sf "$diffutils_bin/diff" "$BIN_DIR/diff"
        break
    fi
done

# Copy t helper scripts / ref files to $BIN_DIR/t so tests can find them via `bk bin`/t
mkdir -p "$BIN_DIR/t"
cp -rf "$T_DIR"/* "$BIN_DIR/t/" 2>/dev/null || true

# Copy source file needed by t.fast-import
cp -f "$SRC_DIR/slib.c" "$BIN_DIR/slib.c" 2>/dev/null || true

# Copy flags.l needed by t.no-dup-flags
cp -f "$SRC_DIR/flags.l" "$BIN_DIR/flags.l" 2>/dev/null || true

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


ln -sf "$BIN_DIR/bk" "$WORK_DIR/bk"
export RUNBK_LEVEL=1
export DO_REMOTE="${DO_REMOTE:-NO}"
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
