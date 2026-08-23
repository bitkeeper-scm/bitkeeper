# BitKeeper Distributed Source Control System

**Welcome to BitKeeper!**

BitKeeper is the original distributed source control system.  Well,
sort of.  Larry wrote a semi-distributed source-control system back at
Sun (productized as Teamware) and Rick had a research system of sorts,
but for better or worse, BitKeeper was the first widely used distributed
source-control system.

The BitKeeper history needs to be written up but the short version
is that it happened because Larry wanted to help Linux not turn into
a bunch of splintered factions like 386BSD, FreeBSD, NetBSD, OpenBSD,
DragonFlyBSD, etc.  He saw that the problem was one of tooling. He built
a team and built BitKeeper so the kernel guys would have a reasonable
tool and no need to split up the team (the problem was that Linus refused
to use any source management system: "they all suck!" which wasn't bad
for him but really sucked for the downstream people who had to merge
everything by hand each time Linus released).

It took a couple years. Then the PowerPC people led by Cort Dougan took
a chance on early BK. A couple years later Linus moved to it and most of
the developers followed.  They stayed in it for three more years before
moving to Git because BitKeeper wasn't open source.

## License

BitKeeper is now distributed under the
[Apache 2.0](http://www.apache.org/licenses/LICENSE-2.0)
license. It is free to use and free to modify.
There are some open source components and they have their own licenses.

## Getting Started

Information on using BitKeeper can be found of the
[www.bitkeeper.org](https://www.bitkeeper.org) website or using the
built in [manpages](https://www.bitkeeper.org/man/). Try running
`bk helptool` for a GUI help browser.

### System Requirements

The BitKeeper source tree is highly portable and compiles on most platforms.
This includes:

* Linux (x86, PPC, Arm & MIPS)
* FreeBSD
* Windows
* Solaris

and used to include IRIX, AIX, HP-UX, etc.  Any Posix-like system is a
pretty easy port.

**Runtime Dependency Note:** `diff`, `diff3`, and `patch` on the user's `PATH` are assumed to be the GNU versions (e.g. GNU diffutils and GNU patch) for BitKeeper to work correctly. Commands such as `bk diff`, `bk merge`, and `bk patch` rely on GNU-specific behavior and options.

### Getting Sources for Bitkeeper

### Building BitKeeper

BitKeeper is built using [Bazel](https://bazel.build/). Third-party dependencies and build tools (such as zlib, lz4, PCRE, gperf, libtomcrypt, and libtommath) are fetched and built automatically by Bazel.

#### Prerequisites

- **Bazel** (or [Bazelisk](https://github.com/bazelbuild/bazelisk))
- A C11 compiler (**GCC** or **Clang**)
- **Perl**
- **GNU groff** (for documentation generation)

On Debian/Ubuntu:
```bash
sudo apt-get install build-essential groff perl
```

On Fedora/RHEL:
```bash
sudo dnf install gcc gcc-c++ groff perl
```

On macOS (via [Homebrew](https://brew.sh/)):
```bash
brew install bazelisk groff gpatch diffutils
```
macOS ships Clang, Perl, and BSD `soelim`/`patch`/`diff3`, but not GNU
`groff`, which is required to generate man pages and built-in help
text. Bazel genrule sandboxes use a minimal `PATH`
(`/bin:/usr/bin:/usr/local/bin`) that does not include Homebrew's Apple
Silicon prefix, so the `//man` build rules explicitly add
`/opt/homebrew/bin` (and `/usr/local/bin` for Intel Macs) to `PATH`
when invoking `groff`.

Running the regression tests (`bazel test //src/t/...`) also requires
GNU `patch` and GNU `diff3` (installed above as `gpatch` and via
`diffutils`, since Homebrew keeps them out of `PATH` to avoid
clobbering the system tools): `bk patch` and `bk merge` invoke the
system `patch`/`diff3` by bare name, and Apple's bundled versions
behave differently (fuzzy-match behavior in `patch`, and `-E`
conflict-marker behavior in `diff3`), causing spurious test failures.
`src/t/test_runner.sh` symlinks the Homebrew GNU versions into each
test's isolated bin directory when found, so no global `PATH` changes
are required.

#### Build Commands

Build the core `bk` binary:
```bash
bazel build //src:bk
```
The compiled binary will be located at `bazel-bin/src/bk`.

Build the portable BitKeeper application directory:
```bash
bazel build //:bitkeeper
```
This produces a fully self-contained, runnable BitKeeper installation in `bazel-bin/bitkeeper` (e.g. `./bazel-bin/bitkeeper/bk version`).

#### Windows Builds

For instructions on cross-compiling BitKeeper for Windows from Linux, see [README-windows.md](README-windows.md).

## Packaging and Installation

### 1. Creating the Self-Extracting Installer

To build the standalone self-extracting installer executable with Bazel (use `-c opt` for optimized release builds):
```bash
bazel build -c opt //:image
```
The resulting installer binary will be at `bazel-bin/image` (or `bazel-bin/installer`).

### 2. Installing on the Current Machine

#### Option A: Using the Self-Extracting Installer
Run the generated installer binary:
```bash
# Run interactively or install to default location:
./bazel-bin/image

# Or install directly to a specified directory (e.g., /opt/bitkeeper):
sudo ./bazel-bin/image /opt/bitkeeper
```

#### Option B: Using the Portable App Directory Directly
You can copy the assembled portable directory `bazel-bin/bitkeeper` directly to your destination and use `bk links` to set up symlinks in your `PATH`:

**System-wide Installation (requires root/sudo):**
```bash
sudo cp -r bazel-bin/bitkeeper /opt/bitkeeper
sudo /opt/bitkeeper/bk links /usr/local/bin
```

**User-local Installation (no root required):**
```bash
mkdir -p ~/bin
cp -r bazel-bin/bitkeeper ~/bitkeeper
~/bitkeeper/bk links ~/bin
# Ensure ~/bin is in your PATH (e.g. export PATH="$HOME/bin:$PATH")
```

*(Note: GNU Make (`make -C src p`) is the legacy build system and is retained primarily for reference during ongoing build modernization.)*

## Testing BitKeeper

BitKeeper includes an extensive regression test suite. Tests can be executed through Bazel:

Run all tests:
```bash
bazel test //src/t:...
```

Run a specific test (dots in filenames are replaced with underscores, e.g. `t.basic` -> `t_basic`):
```bash
bazel test //src/t:t_basic
```

Run a test with failure output displayed in the terminal:
```bash
bazel test --test_output=errors //src/t:t_basic
```

Run a test with verbose output (`-v`) or shell tracing (`-x`):
```bash
bazel test //src/t:t_basic --test_arg=-v --test_output=all
bazel test //src/t:t_basic --test_arg=-x --test_output=all
```

For more details on test execution, debugging flags, and test suite structure, see [src/t/README.md](src/t/README.md).

## Contributing to BitKeeper

See our [community](https://www.bitkeeper.org/community.html) page for
information on how to contact us with questions or contribute
improvements.
