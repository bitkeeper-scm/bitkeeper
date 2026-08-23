#!/usr/bin/env bash
# Wine toolchain wrapper for running MinGW win32 tools
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TOOL_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
TOOL_NAME="$(basename "$0")"

export WINEDEBUG="-all"
export WINEPREFIX="${WINEPREFIX:-/tmp/wine-bazel-$USER}"
mkdir -p "$WINEPREFIX"

EXE="$TOOL_DIR/bin/${TOOL_NAME}.exe"
if [ ! -f "$EXE" ]; then
    echo "Error: Wine tool $EXE not found" >&2
    exit 1
fi

exec wine "$EXE" "$@"
