def _mingw32_toolchain_impl(ctx):
    # Extract MingW tools from local directory or archives
    local_dir = ctx.attr.local_path
    if local_dir == "":
        local_dir = "/home/wscott/Documents/bk/bk-win32"
    
    # We unpack archives into the repository directory
    tools_dir = local_dir + "/build/win32tools"
    
    archives = [
        "binutils-2.13.90-20030111-1.tar.gz",
        "gcc-core-3.4.1-20040711-1.tar.gz",
        "gcc-g++-3.4.1-20040711-1.tar.gz",
        "mingw-runtime-3.2.tar.gz",
        "w32api-2.5.tar.gz",
    ]
    
    for archive in archives:
        tar_path = tools_dir + "/" + archive
        ctx.extract(tar_path)
    
    # Extract MSYS buildenv for runtime packaging if present
    buildenv_tar = local_dir + "/build/buildenv.tgz"
    res = ctx.execute(["test", "-f", buildenv_tar])
    if res.return_code == 0:
        ctx.extract(buildenv_tar)

    # Extract msys runtime tgz and tcltk tgz if present
    msys_tar = local_dir + "/build/obj/msys-A2V66Uk1aS5ZXQUstIyYiA.tgz"
    res = ctx.execute(["test", "-f", msys_tar])
    if res.return_code == 0:
        ctx.extract(msys_tar, output = "msys_dist")

    tcltk_tar = local_dir + "/build/obj/tcltk-Txr0ikAKS_XUA2Zu0CRosQ.tgz"
    res = ctx.execute(["test", "-f", tcltk_tar])
    if res.return_code == 0:
        ctx.extract(tcltk_tar, output = "tcltk_dist")

    # Generate wrapper scripts in wrappers/ that call wine
    tools = [
        "gcc", "g++", "cpp", "ar", "as", "ld", "nm", "objdump", "strip", "gcov", "windres", "ranlib"
    ]
    
    for tool_name in tools:
        ctx.file("wrappers/" + tool_name, """#!/usr/bin/env bash
REAL_WRAPPER="$(realpath "${BASH_SOURCE[0]}")"
SCRIPT_DIR="$(cd "$(dirname "$REAL_WRAPPER")" && pwd -P)"
TOOL_DIR="$(cd "$SCRIPT_DIR/.." && pwd -P)"

export WINEDEBUG="-all"
export WINEPREFIX="${WINEPREFIX:-/tmp/wine-prefix-bk}"
export WINEDLLPATH="/usr/lib/i386-linux-gnu/wine:/usr/lib/x86_64-linux-gnu/wine"
export PATH="$TOOL_DIR/bin:$TOOL_DIR/mingw32/bin:/bin:/usr/bin:/usr/local/bin:$PATH"

# Ensure Wine server is running or launched with WINEDLLPATH
if ! /usr/lib/x86_64-linux-gnu/wine/wineserver -k0 2>/dev/null; then
    WINEDLLPATH="/usr/lib/i386-linux-gnu/wine:/usr/lib/x86_64-linux-gnu/wine" /usr/lib/x86_64-linux-gnu/wine/wineserver -p 2>/dev/null || true
fi

# Filter out flags unsupported by GCC 3.4.1 or flags with formatting issues
args=()
skip_next=0
skip_next_mf=0
mf_file=""
for arg in "$@"; do
    if [ "$skip_next" -eq 1 ]; then
        skip_next=0
        # Convert -iquote path to -I if the path exists
        if [ -d "$arg" ]; then
            args+=("-I$arg")
        fi
        continue
    fi
    if [ "$skip_next_mf" -eq 1 ]; then
        skip_next_mf=0
        mf_file="$arg"
        args+=("$arg")
        continue
    fi
    case "$arg" in
        -no-canonical-prefixes|-fno-canonical-system-headers|-Wno-deprecated-non-prototype|-Wno-unused-but-set-parameter|-Wno-free-nonheap-object)
            # Skip flags unsupported by GCC 3.4.1
            ;;
        -iquote)
            skip_next=1
            ;;
        -MF)
            skip_next_mf=1
            args+=("$arg")
            ;;
        -std=c11|/std:c11|-std=c99|/std:c99)
            args+=("-std=gnu99")
            ;;
        rcsD)
            args+=("rcs")
            ;;
        -I*|-iquote*)
            # Check -Ipath
            ipath="${arg#-I}"
            if [ -d "$ipath" ]; then
                args+=("$arg")
            fi
            ;;
        *)
            args+=("$arg")
            ;;
    esac
done

# Ensure current working directory is physical path
cd "$(pwd -P)"

if [ \"""" + tool_name + """\" = "ar" ]; then
    /usr/bin/ar "${args[@]}"
    rc=$?
else
    wine "$TOOL_DIR/bin/""" + tool_name + """.exe" "${args[@]}"
    rc=$?
    # If linking an executable with output -o <file>, MinGW gcc automatically appends .exe
    # Create symlink or copy if Bazel expects the extensionless target name
    if [ $rc -eq 0 ] && [ \"""" + tool_name + """\" = "gcc" -o \"""" + tool_name + """\" = "g++" ]; then
        for ((i=0; i<${#args[@]}; i++)); do
            if [ "${args[i]}" = "-o" ] && [ $((i+1)) -lt ${#args[@]} ]; then
                out_target="${args[i+1]}"
                if [ ! -f "$out_target" ] && [ -f "${out_target}.exe" ]; then
                    cp -f "${out_target}.exe" "$out_target" 2>/dev/null || ln -sf "${out_target}.exe" "$out_target"
                fi
                break
            fi
        done
    fi
fi

if [ $rc -eq 0 ] && [ -n "$mf_file" ] && [ -f "$mf_file" ]; then
    # Fix Wine drive letter (Z:) and make toolchain include paths relative to execroot
    python3 "$TOOL_DIR/wrappers/fix_d.py" "$mf_file" "$TOOL_DIR"
fi

exit $rc
""", executable = True)

    ctx.file("wrappers/fix_d.py", ctx.read(Label("//toolchains/mingw32_wine:fix_d.py")))

    ctx.file("cc_toolchain_config.bzl", ctx.read(Label("//toolchains/mingw32_wine:cc_toolchain_config.bzl")))

    ctx.template(
        "BUILD.bazel",
        Label("//toolchains/mingw32_wine:BUILD.toolchain.tpl"),
        substitutions = {
            "%{TOOLCHAIN_REPO_NAME}": ctx.attr.name,
        },
    )

mingw32_toolchain_repo = repository_rule(
    implementation = _mingw32_toolchain_impl,
    attrs = {
        "local_path": attr.string(default = "/home/wscott/Documents/bk/bk-win32"),
        "_fix_d": attr.label(default = "//toolchains/mingw32_wine:fix_d.py"),
    },
)

