def _llvm_mingw_toolchain_impl(ctx):
    # Download and extract llvm-mingw release
    url = "https://github.com/mstorsjo/llvm-mingw/releases/download/20260616/llvm-mingw-20260616-msvcrt-ubuntu-22.04-x86_64.tar.xz"
    sha256 = "a1f7968b48ba8d949194d6dee6c76f3cd0f61cba91658599af2c2c834a55ab87"
    strip_prefix = "llvm-mingw-20260616-msvcrt-ubuntu-22.04-x86_64"

    ctx.download_and_extract(
        url = url,
        sha256 = sha256,
        stripPrefix = strip_prefix,
    )

    ctx.file("wrappers/fix_d.py", ctx.read(Label("//toolchains/llvm_mingw:fix_d.py")))

    # Generate wrappers
    ctx.file("wrappers/gcc", """#!/usr/bin/env bash
REAL_WRAPPER="$(realpath "${BASH_SOURCE[0]}")"
SCRIPT_DIR="$(cd "$(dirname "$REAL_WRAPPER")" && pwd -P)"
TOOL_DIR="$(cd "$SCRIPT_DIR/.." && pwd -P)"

MF_FILE=""
is_mf=0
OUT_FILE=""
is_out=0
for arg in "$@"; do
    if [ "$is_mf" -eq 1 ]; then
        MF_FILE="$arg"
        is_mf=0
    elif [ "$arg" = "-MF" ]; then
        is_mf=1
    fi
    if [ "$is_out" -eq 1 ]; then
        OUT_FILE="$arg"
        is_out=0
    elif [ "$arg" = "-o" ]; then
        is_out=1
    fi
done

"$TOOL_DIR/bin/i686-w64-mingw32-clang" "$@"
RC=$?

if [ $RC -eq 0 ]; then
    if [ -n "$MF_FILE" ] && [ -f "$MF_FILE" ]; then
        python3 "$TOOL_DIR/wrappers/fix_d.py" "$MF_FILE" "$TOOL_DIR"
    fi
    if [ -n "$OUT_FILE" ]; then
        if [ -f "${OUT_FILE}.exe" ] && [ ! -f "$OUT_FILE" ]; then
            cp -f "${OUT_FILE}.exe" "$OUT_FILE"
        fi
    fi
fi

exit $RC
""", executable = True)

    ctx.file("wrappers/g++", """#!/usr/bin/env bash
REAL_WRAPPER="$(realpath "${BASH_SOURCE[0]}")"
SCRIPT_DIR="$(cd "$(dirname "$REAL_WRAPPER")" && pwd -P)"
TOOL_DIR="$(cd "$SCRIPT_DIR/.." && pwd -P)"

MF_FILE=""
is_mf=0
OUT_FILE=""
is_out=0
for arg in "$@"; do
    if [ "$is_mf" -eq 1 ]; then
        MF_FILE="$arg"
        is_mf=0
    elif [ "$arg" = "-MF" ]; then
        is_mf=1
    fi
    if [ "$is_out" -eq 1 ]; then
        OUT_FILE="$arg"
        is_out=0
    elif [ "$arg" = "-o" ]; then
        is_out=1
    fi
done

"$TOOL_DIR/bin/i686-w64-mingw32-clang++" "$@"
RC=$?

if [ $RC -eq 0 ]; then
    if [ -n "$MF_FILE" ] && [ -f "$MF_FILE" ]; then
        python3 "$TOOL_DIR/wrappers/fix_d.py" "$MF_FILE" "$TOOL_DIR"
    fi
    if [ -n "$OUT_FILE" ]; then
        if [ -f "${OUT_FILE}.exe" ] && [ ! -f "$OUT_FILE" ]; then
            cp -f "${OUT_FILE}.exe" "$OUT_FILE"
        fi
    fi
fi

exit $RC
""", executable = True)

    ctx.file("wrappers/ld", """#!/usr/bin/env bash
REAL_WRAPPER="$(realpath "${BASH_SOURCE[0]}")"
SCRIPT_DIR="$(cd "$(dirname "$REAL_WRAPPER")" && pwd -P)"
TOOL_DIR="$(cd "$SCRIPT_DIR/.." && pwd -P)"

OUT_FILE=""
is_out=0
for arg in "$@"; do
    if [ "$is_out" -eq 1 ]; then
        OUT_FILE="$arg"
        is_out=0
    elif [ "$arg" = "-o" ]; then
        is_out=1
    fi
done

"$TOOL_DIR/bin/i686-w64-mingw32-clang" "$@"
RC=$?

if [ $RC -eq 0 ] && [ -n "$OUT_FILE" ]; then
    if [ -f "${OUT_FILE}.exe" ] && [ ! -f "$OUT_FILE" ]; then
        cp -f "${OUT_FILE}.exe" "$OUT_FILE"
    fi
fi

exit $RC
""", executable = True)

    ctx.file("wrappers/cpp", """#!/usr/bin/env bash
REAL_WRAPPER="$(realpath "${BASH_SOURCE[0]}")"
SCRIPT_DIR="$(cd "$(dirname "$REAL_WRAPPER")" && pwd -P)"
TOOL_DIR="$(cd "$SCRIPT_DIR/.." && pwd -P)"
exec "$TOOL_DIR/bin/i686-w64-mingw32-clang" -E "$@"
""", executable = True)

    ctx.file("wrappers/ar", """#!/usr/bin/env bash
REAL_WRAPPER="$(realpath "${BASH_SOURCE[0]}")"
SCRIPT_DIR="$(cd "$(dirname "$REAL_WRAPPER")" && pwd -P)"
TOOL_DIR="$(cd "$SCRIPT_DIR/.." && pwd -P)"
exec "$TOOL_DIR/bin/llvm-ar" "$@"
""", executable = True)

    ctx.file("wrappers/as", """#!/usr/bin/env bash
REAL_WRAPPER="$(realpath "${BASH_SOURCE[0]}")"
SCRIPT_DIR="$(cd "$(dirname "$REAL_WRAPPER")" && pwd -P)"
TOOL_DIR="$(cd "$SCRIPT_DIR/.." && pwd -P)"
exec "$TOOL_DIR/bin/llvm-as" "$@"
""", executable = True)

    ctx.file("wrappers/nm", """#!/usr/bin/env bash
REAL_WRAPPER="$(realpath "${BASH_SOURCE[0]}")"
SCRIPT_DIR="$(cd "$(dirname "$REAL_WRAPPER")" && pwd -P)"
TOOL_DIR="$(cd "$SCRIPT_DIR/.." && pwd -P)"
exec "$TOOL_DIR/bin/llvm-nm" "$@"
""", executable = True)

    ctx.file("wrappers/objdump", """#!/usr/bin/env bash
REAL_WRAPPER="$(realpath "${BASH_SOURCE[0]}")"
SCRIPT_DIR="$(cd "$(dirname "$REAL_WRAPPER")" && pwd -P)"
TOOL_DIR="$(cd "$SCRIPT_DIR/.." && pwd -P)"
exec "$TOOL_DIR/bin/llvm-objdump" "$@"
""", executable = True)

    ctx.file("wrappers/strip", """#!/usr/bin/env bash
REAL_WRAPPER="$(realpath "${BASH_SOURCE[0]}")"
SCRIPT_DIR="$(cd "$(dirname "$REAL_WRAPPER")" && pwd -P)"
TOOL_DIR="$(cd "$SCRIPT_DIR/.." && pwd -P)"
exec "$TOOL_DIR/bin/llvm-strip" "$@"
""", executable = True)

    ctx.file("wrappers/windres", """#!/usr/bin/env bash
REAL_WRAPPER="$(realpath "${BASH_SOURCE[0]}")"
SCRIPT_DIR="$(cd "$(dirname "$REAL_WRAPPER")" && pwd -P)"
TOOL_DIR="$(cd "$SCRIPT_DIR/.." && pwd -P)"
exec "$TOOL_DIR/bin/i686-w64-mingw32-windres" "$@"
""", executable = True)

    ctx.file("wrappers/gcov", """#!/usr/bin/env bash
exit 0
""", executable = True)

    # Template substitutions
    substitutions = {
        "%{TOOLCHAIN_REPO_NAME}": ctx.name,
    }

    ctx.template(
        "BUILD.bazel",
        Label("//toolchains/llvm_mingw:BUILD.toolchain.tpl"),
        substitutions = substitutions,
    )

    ctx.file("cc_toolchain_config.bzl", ctx.read(Label("//toolchains/llvm_mingw:cc_toolchain_config.bzl")))

llvm_mingw_toolchain_repo = repository_rule(
    implementation = _llvm_mingw_toolchain_impl,
    attrs = {
        "version": attr.string(default = "20260616-v3"),
    },
)
