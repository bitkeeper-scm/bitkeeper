# Building BitKeeper for Windows

BitKeeper for Windows (32-bit `x86_32`) can be cross-compiled on Linux using Bazel and a Linux-native LLVM-MinGW toolchain.

## Overview

The Windows distribution of BitKeeper includes:
- **Core executables**: `bk.exe` (main CLI multi-call binary) and `bkg.exe` (GUI launcher).
- **Windows helper utilities**: `svcmgr.exe`, `blat.exe`, `winctlw.exe`.
- **In-tree GUI subsystem**: `bkgui.exe`, `tclsh.exe`, and Tcl/Tk scripts and packages (BWidget, Tkcon).
- **MSYS runtime**: bundled under `gnu/` to provide POSIX utility commands on Windows.
- **Shell extension & DLLs**: Windows Shell extension and SCC integration DLLs.

All toolchain dependencies (LLVM-MinGW) and third-party libraries (zlib, PCRE2, libtommath, libtomcrypt) are fetched and managed automatically by Bazel.

---

## Building the Windows Release

### Prerequisites

- Linux host with **Bazel** (or [Bazelisk](https://github.com/bazelbuild/bazelisk))
- Standard host build tools (`build-essential`, `perl`, `groff`)

### Build Commands

#### 1. Build the Windows Core Binary (`bk.exe`)
```bash
bazel build --platforms=//platforms:windows_x86_32 //src:bk
```
The output binary will be located at `bazel-bin/src/bk.exe`.

#### 2. Build the Complete Windows Release Package (`bitkeeper.tar.gz`)
To build the complete Windows release archive containing all executables, MSYS runtime, DLLs, and GUI components:
```bash
bazel build --platforms=//platforms:windows_x86_32 //src:install_image
```
The resulting tarball will be located at:
```
bazel-bin/src/bitkeeper.tar.gz
```

---

## Technical Details

- **Target Platform**: `//platforms:windows_x86_32` (`i686-w64-mingw32`).
- **Toolchain**: Downloaded via `toolchains/llvm_mingw/extension.bzl` (LLVM MinGW release).
- **Tcl/Tk Compilation**: Cross-compiled from `src/gui/tcltk/` using `src/gui/build_tcltk.sh` with static PCRE2 and bundled zlib/tommath.
