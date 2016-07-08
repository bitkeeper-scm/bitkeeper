This repository contains the code for the BitKeeper project and is
covered by the Apache 2.0 license.

The following subdirectories are from 3rd party sources are are
not considered part of BitKeeper and are covered by their own licenses:

Linked to 'bk' binary:

	./src/contrib/cat.c			[BSD]
	./src/contrib/test.c		[Public Domain]
	./src/gui/tcltk/pcre (*)	[BSD]
	./src/libc/lz4 (*)		    [BSDv2]
	./src/libc/mdbm			    [BSD]
	./src/libc/stdio		    [BSD] stdio from NetBSD
	./src/libc/zlib	(*)		    [custom]
	./src/libc/string		    [BSD] strcasestr.c strsep.[c3]
	./src/libc/utils/crc32c.c	[Public Domain w copywrite ]
	./src/tomcrypt (*)		    [Public Domain]
	./src/tommath (*)		    [Public Domain]

The items marked (*) above will not be included if your system already
has copies installed or when not building from the official
repository.

Included with standalone with package:

	./src/gui/tcltk/bwidget		[looks BSD]
	./src/gui/tcltk/tcl		    [looks BSD]
	./src/gui/tcltk/tk		    [looks BSD]
	./src/gui/tcltk/tkcon		[looks BSD]
	./src/gui/tcltk/tktable		[looks BSD]
	./src/gui/tcltk/tktreectrl	[looks BSD]
	./src/win32/blat			[Public Domain]
	./src/win32/msys		    [GPLv2]
	./src/win32/svcmgr			[Public Domain]
	./src/win32/winctl	        [GPLv2]
