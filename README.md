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

### Getting Sources for Bitkeeper

### Building BitKeeper

BitKeeper is built using [Bazel](https://bazel.build/). Third-party dependencies (such as zlib, lz4, PCRE, libtomcrypt, and libtommath) are fetched and built automatically by Bazel.

#### Prerequisites

- **Bazel** (or [Bazelisk](https://github.com/bazelbuild/bazelisk))
- A C11 compiler (**GCC** or **Clang**)
- **GNU gperf**
- **Perl**
- **GNU groff** (for documentation generation)

On Debian/Ubuntu:
```bash
sudo apt-get install build-essential gperf groff perl
```

On Fedora/RHEL:
```bash
sudo dnf install gcc gcc-c++ gperf groff perl
```

#### Build Commands

Build the core `bk` binary:
```bash
bazel build //src:bk
```
The compiled binary will be located at `bazel-bin/src/bk`.

To build the full installation package (tarball):
```bash
bazel build //src:install_image
```
The resulting package will be at `bazel-bin/src/bitkeeper.tar.gz`.

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
