/*
 * %K%
 * Copyright (c) 1999 Larry McVoy
 */
#include <stdio.h>
#include <unistd.h>

extern unsigned int program_size;
extern unsigned char program_data[];
/*
 * Must match list in bk.sh and port/unix_platform.tcl
 */
char	*dirs[] = {
	"/usr/libexec",
	"/usr/lib",
	"/usr",
	"/opt",
	"/usr/local",
	"/usr/local/bin",
	"/usr/bin",
	0
};

main()
{
	char	buf[1024];
	char	buf2[1024];
	FILE	*extract;
	int	i, bad, n = 0;

	while (1) {
		bad = 1;
		fprintf(stderr,
"
-------------------------------------------------------------------
		    Welcome to BitKeeper.

You are about to install BitKeeper on your system.  This program
contains a gzipped tar file which it will expand and place in a
directory of your choosing.  We will create a subdirectory there
called ``bitkeeper'' and create a few symlinks from /usr/bin
into that directory.  

The directory must be one of the following:

");
		for (i = 0; dirs[i]; ++i) {
			fprintf(stderr, "%s\n", dirs[i]);
		}
		fprintf(stderr, "\nDirectory: ");
		fgets(buf, sizeof(buf), stdin);
		chop(buf);
		for (i = 0; dirs[i]; ++i) {
			if (strcmp(buf, dirs[i]) == 0) {
				bad = 0;
				break;
			}
		}
		if (bad) continue;
		if (chdir(buf) != 0) {
			fprintf(stderr,
			    "Can not change directories to %s\n", buf);
			continue;
		}
		if (access("bitkeeper", W_OK) != 0) {
			fprintf(stderr,
				"You have no write permission on %s/bitkeeper,",
			    buf);
			fprintf(stderr,
				"\nperhaps you need to do this as root?\n");
			continue;
		}
		break;	/* yeah */
	}
	if (access("bitkeeper/bk", F_OK) == 0) {
		fprintf(stderr,
"
==========================================================================

    ERROR: existing installation found in this directory.
    Please move it or remove it and try again.

==========================================================================
");
		exit(1);
	}

	extract = popen("gunzip | tar xvf -", "w");
	while (n < program_size) {
		int	left;

		left = program_size - n > 4096 ? 4096 : program_size - n;
		n += fwrite(&program_data[n], 1, left, extract);
	}
	pclose(extract);
	fprintf(stderr, "\n");
	if (chdir("bitkeeper")) {
		fprintf(stderr,
		    "We were unable to create bitkeeper directory.\n");
		exit(1);
	}
	if (access("/usr/bin", W_OK) != 0) {
		fprintf(stderr, 
"You have no write permission on /usr/bin, so no links will be created there.
You need to add the bitkeeper directory to your path (not recommended), 
or symlink bk into some public bin directory.

If you want emacs vc mode to work, you need to create links for all of the
following:

	admin get delta unget rmdel prs bk

If you just want make to work (so it automatically checks out files),
the list is

	get bk
");
	} else {
		system("BK_BIN=.; export BK_BIN; ./bk links");
		fprintf(stderr, "\n");
		if (access("/usr/bin/bk", X_OK) != 0) {
			fprintf(stderr,
			    "We were unable to create links in /usr/bin.\n");
			exit(1);
		}
	}
	fprintf(stderr,
"
-------------------------------------------------------------------
		    Installation was successful.

For more information, you can go to http://www.bitkeeper.com .

To test the system try ``bk regression''.

For local command line information, try ``bk help''.

For graphical information, try ``bk helptool''.

			      Enjoy!
-------------------------------------------------------------------
");
	exit(0);
}

chop(char *s)
{
	while (*s++);
	s[-2] = 0;
}
