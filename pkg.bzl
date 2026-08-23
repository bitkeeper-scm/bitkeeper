"""Rule to assemble BitKeeper installation directory."""

def _bitkeeper_directory_impl(ctx):
    out = ctx.actions.declare_directory(ctx.label.name)
    
    script = ctx.actions.declare_file(ctx.label.name + "_builder.sh")
    
    is_windows = ctx.target_platform_has_constraint(ctx.attr._windows_constraint[platform_common.ConstraintValueInfo])
    
    # Generate builder script
    if is_windows:
        cmd = """#!/bin/bash
set -e
dest="$1"
mkdir -p "$dest/contrib" "$dest/lscripts" "$dest/www"

cp "{bk}" "$dest/"
for s in {scripts}; do
    cp "$s" "$dest/"
    chmod +x "$dest/$(basename "$s")"
done

for d in {data}; do
    cp "$d" "$dest/"
done

for l in {lscripts}; do
    cp "$l" "$dest/lscripts/"
done

for c in {contrib}; do
    cp "$c" "$dest/contrib/"
done

for w in {www}; do
    rpath="${{w#*src/www/}}"
    dir="$(dirname "$rpath")"
    mkdir -p "$dest/www/$dir"
    cp "$w" "$dest/www/$rpath"
done

tar -C "$dest" -xzf "{gui}"

mkdir -p "$dest/man"
tar -C "$dest/man" -xf "{man}"

# Windows-specific components
if [ -f "$dest/bk" ]; then
    cp -f "$dest/bk" "$dest/bk.exe"
    rm -f "$dest/bk"
fi
if [ -n "{bkg}" ] && [ -f "{bkg}" ]; then
    cp -f "{bkg}" "$dest/bkg.exe"
fi

mkdir -p "$dest/gnu"
for m in {msys}; do
    if [[ "$m" == *"msys_dist/"* ]]; then
        mpath="${{m#*msys_dist/}}"
        mdir="$(dirname "$mpath")"
        mkdir -p "$dest/gnu/$mdir"
        cp -f "$m" "$dest/gnu/$mpath"
    elif [[ "$m" == *"src/win32/msys/"* ]]; then
        mpath="${{m#*src/win32/msys/}}"
        mdir="$(dirname "$mpath")"
        mkdir -p "$dest/gnu/$mdir"
        cp -f "$m" "$dest/gnu/$mpath"
    fi
done

if [ -n "{svcmgr}" ] && [ -f "{svcmgr}" ]; then cp -f "{svcmgr}" "$dest/svcmgr.exe"; fi
if [ -n "{blat}" ] && [ -f "{blat}" ]; then cp -f "{blat}" "$dest/blat.exe"; fi
if [ -n "{winctl}" ] && [ -f "{winctl}" ]; then cp -f "{winctl}" "$dest/winctlw.exe"; fi

for icon in {icons}; do
    cp -f "$icon" "$dest/"
done
mkdir -p "$dest/Icons"
for dll in {dlls}; do
    case "$dll" in
        *.ico) cp -f "$dll" "$dest/Icons/" ;;
        *) cp -f "$dll" "$dest/" ;;
    esac
done
""".format(
            bk = ctx.file.bk.path,
            bkg = ctx.file.bkg.path if ctx.file.bkg else "",
            scripts = " ".join([f.path for f in ctx.files.scripts]),
            data = " ".join([f.path for f in ctx.files.data]),
            lscripts = " ".join([f.path for f in ctx.files.lscripts]),
            contrib = " ".join([f.path for f in ctx.files.contrib]),
            www = " ".join([f.path for f in ctx.files.www]),
            gui = ctx.file.gui.path,
            man = ctx.file.man.path,
            msys = " ".join([f.path for f in ctx.files.msys]),
            svcmgr = ctx.file.svcmgr.path if ctx.file.svcmgr else "",
            blat = ctx.file.blat.path if ctx.file.blat else "",
            winctl = ctx.file.winctl.path if ctx.file.winctl else "",
            icons = " ".join([f.path for f in ctx.files.icons]),
            dlls = " ".join([f.path for f in ctx.files.dlls]),
        )
    else:
        cmd = """#!/bin/bash
set -e
dest="$1"
mkdir -p "$dest/contrib" "$dest/lscripts" "$dest/www"

cp "{bk}" "$dest/"
for s in {scripts}; do
    cp "$s" "$dest/"
    chmod +x "$dest/$(basename "$s")"
done

for d in {data}; do
    cp "$d" "$dest/"
done

for l in {lscripts}; do
    cp "$l" "$dest/lscripts/"
done

for c in {contrib}; do
    cp "$c" "$dest/contrib/"
done

for w in {www}; do
    rpath="${{w#*src/www/}}"
    dir="$(dirname "$rpath")"
    mkdir -p "$dest/www/$dir"
    cp "$w" "$dest/www/$rpath"
done

tar -C "$dest" -xzf "{gui}"

mkdir -p "$dest/man"
tar -C "$dest/man" -xf "{man}"
""".format(
            bk = ctx.file.bk.path,
            scripts = " ".join([f.path for f in ctx.files.scripts]),
            data = " ".join([f.path for f in ctx.files.data]),
            lscripts = " ".join([f.path for f in ctx.files.lscripts]),
            contrib = " ".join([f.path for f in ctx.files.contrib]),
            www = " ".join([f.path for f in ctx.files.www]),
            gui = ctx.file.gui.path,
            man = ctx.file.man.path,
        )

    ctx.actions.write(
        output = script,
        content = cmd,
        is_executable = True,
    )

    all_inputs = (
        [ctx.file.bk, ctx.file.gui, ctx.file.man] +
        ctx.files.scripts +
        ctx.files.data +
        ctx.files.lscripts +
        ctx.files.contrib +
        ctx.files.www +
        ([ctx.file.bkg] if ctx.file.bkg else []) +
        ctx.files.msys +
        ([ctx.file.svcmgr] if ctx.file.svcmgr else []) +
        ([ctx.file.blat] if ctx.file.blat else []) +
        ([ctx.file.winctl] if ctx.file.winctl else []) +
        ctx.files.icons +
        ctx.files.dlls
    )

    ctx.actions.run(
        outputs = [out],
        inputs = all_inputs,
        executable = script,
        arguments = [out.path],
        progress_message = "Assembling BitKeeper directory %s" % out.short_path,
    )

    return [
        DefaultInfo(
            files = depset([out]),
            runfiles = ctx.runfiles(files = [out]),
        ),
    ]

bitkeeper_directory = rule(
    implementation = _bitkeeper_directory_impl,
    attrs = {
        "bk": attr.label(mandatory = True, allow_single_file = True),
        "scripts": attr.label_list(allow_files = True),
        "data": attr.label_list(allow_files = True),
        "lscripts": attr.label_list(allow_files = True),
        "contrib": attr.label_list(allow_files = True),
        "www": attr.label_list(allow_files = True),
        "gui": attr.label(mandatory = True, allow_single_file = True),
        "man": attr.label(mandatory = True, allow_single_file = True),
        "bkg": attr.label(allow_single_file = True),
        "msys": attr.label_list(allow_files = True),
        "svcmgr": attr.label(allow_single_file = True),
        "blat": attr.label(allow_single_file = True),
        "winctl": attr.label(allow_single_file = True),
        "icons": attr.label_list(allow_files = True),
        "dlls": attr.label_list(allow_files = True),
        "_windows_constraint": attr.label(
            default = Label("@platforms//os:windows"),
        ),
    },
)
