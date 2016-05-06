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

BitKeeper requires the following prerequisites to build:

* GNU make
* GNU gperf
* GNU bison
* some lex
* GNU groff
* X libraries for Tk

If you are building on a Debian based Linux then the following
packages are required:

  sudo apt-get install make gperf groff bison flex libxft2-dev

Build using the following sequence (we build on 12 core systems;
hence the -j12 sprinkled here and there):

	cd src
	make -j12 p		# 'p'roduction build
	make image		# create install image (at src/utils)
	./utils/bk-*.bin	# run installer created above

(make *must* be GNU make)

Building on Windows requires msys and is more involved. See the thread
on the
[forum](https://users.bitkeeper.org/t/howto-building-bitkeeper-on-windows/78)
about Windows builds.

## Testing BitKeeper

An extensive regression suite is found in `src/t` and can be run using
the doit script in that directory.  The test harness can be run in
parallel using multiple cores like so:

	cd src
	make p
	cd t
	./doit -j12

Look [here](https://users.bitkeeper.org/t/howto-running-regressions-on-a-clean-linux-machine/74)
for help with getting regressions to pass cleanly.

## Contributing to BitKeeper

See our [community](https://www.bitkeeper.org/community.html) page for
information on how to contact us with questions or contribute
improvements.
