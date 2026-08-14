#!/bin/bash
set -e

# Generate BitKeeper GUI tool scripts
# Usage: gen_guis.sh <platform_tcl> <bk_binary_or_empty> <output_dir>

PLATFORM_TCL="$(cd "$(dirname "$1")" && pwd)/$(basename "$1")"
INSTALLTOOL_TCL=""
if [ -n "$2" ] && [ -f "$2" ]; then
    INSTALLTOOL_TCL="$(cd "$(dirname "$2")" && pwd)/$(basename "$2")"
fi
BK_BIN="$3"
OUT_DIR="$4"

if [ -z "$PLATFORM_TCL" ] || [ -z "$OUT_DIR" ]; then
    echo "Usage: $0 <platform_tcl> <installtool_tcl> <bk_bin> <out_dir>" >&2
    exit 1
fi

mkdir -p "$OUT_DIR"
OUT_DIR="$(cd "$OUT_DIR" && pwd)"
GUIDIR="$(cd "$(dirname "$0")" && pwd)"
cd "$GUIDIR"

COMMON="$PLATFORM_TCL config.tcl bktheme.tcl tooltip.tcl common.tcl"
LCOMMON="common.l search.l"

# 1. bugform
(
    echo 'wm withdraw .'
    cat $COMMON search.tcl appState.tcl buglib.tcl bugform.tcl
) > "$OUT_DIR/bugform"

# 2. supportform
(
    echo 'wm withdraw .'
    cat $COMMON search.tcl appState.tcl supportlib.tcl supportform.tcl
) > "$OUT_DIR/supportform"

# 3. citool
(
    echo 'wm withdraw .'
    echo 'set ::tool_name citool'
    cat $COMMON appState.tcl ciedit.tcl
    echo
    echo 'L {'
    echo
    for f in $LCOMMON listbox.l citool.l; do
        echo "#line 1 \"$f\""
        cat "$f"
    done
    echo '}'
) > "$OUT_DIR/citool"

# 4. csettool
(
    echo 'wm withdraw .'
    echo 'set ::tool_name csettool'
    cat $COMMON search.tcl appState.tcl difflib.tcl csettool.tcl
) > "$OUT_DIR/csettool"

# 5. difftool
(
    echo 'wm withdraw .'
    echo 'set ::tool_name difftool'
    cat $COMMON search.tcl appState.tcl difflib.tcl difftool.tcl
) > "$OUT_DIR/difftool"

# 6. fmtool
(
    echo 'wm withdraw .'
    echo 'set ::tool_name fmtool'
    cat $COMMON appState.tcl difflib.tcl fmlib.tcl fmtool.tcl
) > "$OUT_DIR/fmtool"

# 7. fm3tool
(
    echo 'wm withdraw .'
    echo 'set ::tool_name fm3tool'
    cat $COMMON appState.tcl tooltip.tcl difflib.tcl fm3tool.tcl
) > "$OUT_DIR/fm3tool"

# 8. helptool
(
    echo 'wm withdraw .'
    echo 'set ::tool_name helptool'
    cat $COMMON appState.tcl helptool.tcl
) > "$OUT_DIR/helptool"

# 9. msgtool
(
    echo 'wm withdraw .'
    echo 'set ::tool_name msgtool'
    cat $COMMON msgtool.tcl
) > "$OUT_DIR/msgtool"

# 10. renametool
(
    echo 'wm withdraw .'
    echo 'set ::tool_name renametool'
    cat $COMMON appState.tcl renametool.tcl
) > "$OUT_DIR/renametool"

# 11. revtool
(
    echo 'wm withdraw .'
    echo 'set ::tool_name revtool'
    cat $COMMON search.tcl appState.tcl revtool.tcl
) > "$OUT_DIR/revtool"

# 12. setuptool
(
    echo 'wm withdraw .'
    echo 'set ::tool_name setuptool'
    cat $COMMON tkwizard.tcl setuptool.tcl
) > "$OUT_DIR/setuptool"

# 13. outputtool
(
    echo 'wm withdraw .'
    cat $COMMON
    echo
    echo 'L {'
    echo
    echo '#line 1 "outputtool.l"'
    cat outputtool.l
    echo '}'
) > "$OUT_DIR/outputtool"

# 14. installtool
LOGO_B64=""
if [ -f "$GUIDIR/imgsrc/bklogo.gif" ]; then
    if [ -n "$BK_BIN" ] && [ -x "$BK_BIN" ]; then
        LOGO_B64="$("$BK_BIN" base64 < "$GUIDIR/imgsrc/bklogo.gif")"
    elif command -v base64 >/dev/null 2>&1; then
        LOGO_B64="$(base64 -w 0 < "$GUIDIR/imgsrc/bklogo.gif" 2>/dev/null || base64 < "$GUIDIR/imgsrc/bklogo.gif" | tr -d '\n')"
    fi
fi

(
    echo 'wm withdraw .'
    echo 'set ::tool_name installtool'
    cat "$PLATFORM_TCL" "$INSTALLTOOL_TCL" tkwizard.tcl bktheme.tcl common.tcl
    if [ -n "$LOGO_B64" ]; then
        echo "image create photo bklogo -data {$LOGO_B64}"
    fi
    echo 'main'
) > "$OUT_DIR/installtool"

chmod -R +r "$OUT_DIR"
