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
	make install		# install (default /usr/libexec/bitkeeper)

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

<!---   Disable this section until it is actually true

We have an internal process that we'd like to see if we can get external
people to follow as it has served us well for almost 2 decades.

How we do it is we have a central file / build server called "work".

Everyone gets logins there (and for the open source world we'd make 
another one of these that faces the outside world and give people 
logins).  

There are some special locations in the file system:

/build	- not backed up, typically the fastest SSD we can find, we do
	builds and regressions here.

/home/bk/$QUEUE
	We have a repo that is also a queue for all the ongoing projects.

	We have a /home/bk/bugfix which is mostly idle, it's always supposed
	to be in "ready to ship" state (or very close).  So no feature 
	development goes here, this is the tree used to build commercial
	releases.

	We have /home/bk/dev which is where new features go.  Periodically
	we stabilize that and it becomes the next bugfix.  For example,
	our bugfix tree is the bk-7.x release.  Dev is nothing yet, but
	when we stabilize it, bugfix will get moved to bugfix-7.z, dev
	will get moved to bugfix (and tagged bk-8.0), and a new dev will
	be created.

	We how also have /home/bk/dev-oss which is this repository.  It's
	our shiny all Apache 2 licensed tree (well for the parts we own
	it is, the GNU parts are some GPL thingy and Tcl is whatever it
	is, etc.  But the code we wrote, all of it, is Apache 2.)

/home/bk/$USER/$FEATURE
	/home/bk is writable by everyone and when you join the effort
	you make a /home/bk/<you> directory.  Under that directory are
	a pile of clones of one of the queues, typically named like so

	lm/bugfix-doc-fixes
	rick/dev-fast-takepatch
	wscott/dev-oss-readme (where these docs where written)

OK, so what's the process to get stuff in?  Here's the old process:

	bk clone bk://work/dev-oss dev-oss-$FEATURE
	cd dev-oss-$FEATURE
	hack, hack, hack
	document
	write some tests
	run the entire test suite
	works for me, check it in
	bk clone -sHERE . bk://work/lm/dev-oss-$FEATURE
	send mail to dev@bitkeeper.com and ask for a review

Obviously this didn't scale, we needed some sort of formal review process
so we built one.  It's web based, you fill in a form with the path to the
repo on work, tell people what it is, pick some reviewers, mark yourself
as owner, etc.  We call it a "Request to integrate" or RTI (borrowed from
Larry's days at Sun, they had a very similar process).

The reviews can be done completely in the web, we have side by side diffs,
the ability to add comments, etc.  But you also can just clone the repo
and work on it directly and push your changes back.

We need to set up this review process on an outward facing machine and 
see if it takes.  It's certainly worked well for us.

-->
