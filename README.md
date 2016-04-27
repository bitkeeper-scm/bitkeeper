# BitKeeper Distributed Source Control System

**Welcome to BitKeeper!**

BitKeeper is the original distributed source control system.  Well,
sort of.  Larry wrote a semi distributed source control system back at
Sun (productized as Teamware) and Rick had a research system of sorts,
but for better or worse, BitKeeper was the first widely used distributed
source control system.

The BitKeeper history really needs to be written up but the short version
is that it happened because Larry wanted to help Linux not turn into a 
bunch of splintered factions like 386BSD, FreeBSD, NetBSD, OpenBSD, 
DragonFlyBSD, etc.  He saw that the problem was one of tooling, built
a team, and built BitKeeper so the kernel guys would have a reasonable
tool and no need to split up (the problem was that Linus refused to use
any source management system: "they all suck!" which wasn't bad for him
but really sucked for the downstream people who had to merge everything
by hand).

It took a couple years, then the PowerPC people led by Cort Dougan took
a chance on early BK, a couple years later Linus moved to it and most of
the developers followed.  They stayed in it for three more years before
moving to Git because BitKeeper wasn't open source.

## Documentation

## Getting Started

Information on using BitKeeper can be found of the
[bitkeeper.com](http://www.bitkeeper.com) website or using the built
in manpages. Try running `bk helptool` for a GUI help browser.

### System Requirements

The Bitkeeper source tree is highly portable and compiles on most platforms.
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

If you are building on a Debian based Linux then the following
packages are required:
  sudo apt-get install make gperf groff bison flex libxft2-dev

Build using the following sequence (we build on 12 core systems,
hence the -j12 sprinkled here and there):

	cd src
	make -j12 p		# 'p'roduction build
	make image		# create install image (at src/utils)
	make install		# install (default /usr/local/bitkeeper)

(make must be GNU make)

## Testing BitKeeper

An extensive regression suite is found in `src/t` and can be run using
the doit script in that directory.  The test harness can be run in parallel,
we use it with rsh (fast/lightweight) and multiple cores like so:

	cd src
	./build p
	cd t
	./doit -r -j12

## Contributing to BitKeeper

Questions, comments and code patches are welcome at support@bitkeeper.com.
