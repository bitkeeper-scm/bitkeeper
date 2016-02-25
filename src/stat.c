/*
 * Copyright 2005-2009,2015-2016 BitMover, Inc
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "system.h"
#include "sccs.h"

private	void	print_sb(struct stat *sb, char *fn);
private int	do_stat(int which, int ac, char **av);

#define	USE_STAT	0
#define	USE_LSTAT	1

int
lstat_main(int ac, char **av)
{
	return (do_stat(USE_LSTAT, ac, av));
}

int
stat_main(int ac, char **av)
{
	return (do_stat(USE_STAT, ac, av));
}

private int
do_stat(int which, int ac, char **av)
{
	int	i, error = 0, rval = 0;
	char	buf[MAXPATH];
	struct	stat sb;

	if (av[1]) {
		for (i = 1; i < ac; i++) {
			if (which == USE_STAT) {
				error = stat(av[i], &sb);
			} else {
				error = lstat(av[i], &sb);
			}
			unless (error) print_sb(&sb, av[i]);
			rval |= error;
		}
	} else {
		while (fnext(buf, stdin)) {
			unless (chomp(buf)) {
				fprintf(stderr, "Bad filename '%s'\n", buf);
				rval = 1;
				continue;
			}
			if (which == USE_STAT) {
				error = stat(buf, &sb);
			} else {
				error = lstat(buf, &sb);
			}
			unless (error) print_sb(&sb, buf);
			rval |= error;
		}
	}
	return (rval);
}

private void
print_sb(struct stat *sb, char *fn)
{
	char	fmtbuf[128];
	u64	inode = sb->st_ino;
#ifdef WIN32
	HANDLE				h;
	BY_HANDLE_FILE_INFORMATION	info;

	h = CreateFile(fn, 0, FILE_SHARE_READ, 0,
	    OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, 0);
	if (h != INVALID_HANDLE_VALUE) {
		if (GetFileInformationByHandle(h, &info)) {
			inode = ((u64)info.nFileIndexHigh << 32) |
				info.nFileIndexLow;
		}
		CloseHandle(h);
	}
#endif

	bzero(fmtbuf, sizeof(fmtbuf));
	/* build the format string according to sizes */
#ifdef __FreeBSD__
/* FreeBSD 2.2.8 didn't have %llu, so we need to use %qu */
#define	szfmt(x)	switch (sizeof(x)) { \
			case 2:						\
			case 4: strcat(fmtbuf, "%u|"); break;		\
			case 8: strcat(fmtbuf, "%qu|"); break;		\
			default: fprintf(stderr, "weird size?\n");	\
				 return;				\
		}
#else
#define	szfmt(x)	switch (sizeof(x)) { \
			case 2:						\
			case 4: strcat(fmtbuf, "%u|"); break;		\
			case 8: strcat(fmtbuf, "%llu|"); break;		\
			default: fprintf(stderr, "weird size?\n");	\
				 return;				\
		}
#endif
	szfmt(sb->st_dev); szfmt(inode);
	strcat(fmtbuf, "%o|"); /* st_mode goes in octal */
	szfmt(linkcount(fn, sb)); szfmt(sb->st_uid); szfmt(sb->st_gid);
	szfmt(sb->st_rdev); szfmt(sb->st_size); szfmt(sb->st_atime);
	szfmt(sb->st_mtime); szfmt(sb->st_ctime);
	strcat(fmtbuf, "%s\n");
	/* dev|ino|mode|...|filename */
	printf(fmtbuf,
	    sb->st_dev,		/* 0 */
	    inode,		/* 1 */
	    sb->st_mode,	/* 2 */
	    linkcount(fn, sb),	/* 3 */
	    sb->st_uid,		/* 4 */
	    sb->st_gid,		/* 5 */
	    sb->st_rdev,	/* 6 */
	    sb->st_size,	/* 7 */
	    sb->st_atime,	/* 8 */
	    sb->st_mtime,	/* 9 */
	    sb->st_ctime,	/* 10 */
	    fn);		/* 11 */
}

/*
 * Usage: bk _access <file> [r|w|x]
 * defaults to F_OK.
 */
int
access_main(int ac, char **av)
{
	int	mode = F_OK;

	unless (av[1]) {
usage:		fprintf(stderr, "usage: bk _access <file> [r|w|x]\n");
		exit(1);
	}
	if (av[2]) {
		switch (av[2][0]) {
		    case 'r': mode = R_OK; break;
		    case 'w': mode = W_OK; break;
		    case 'x': mode = X_OK; break;
		    default: goto usage;
		}
	}
	if (access(av[1], mode) == 0) {
		printf("%s OK\n", av[1]);
		return (0);
	} else {
		printf("%s FAILED\n", av[1]);
		perror("access");
		return (1);
	}
}

/*
 * debug routine to access the internal getdir()
 */
int
getdir_main(int ac, char **av)
{
	char	**dir;
	int	c, i;

	while ((c = getopt(ac, av, "", 0)) != -1) {
		switch(c) {
		    default: bk_badArg(c, av);
		}
	}
	if (!av[optind] || av[optind+1]) usage();

	unless (dir = getdir(av[optind])) {
		fprintf(stderr, "%s: %s %s\n",
		    prog, av[optind], strerror(errno));
		return (1);
	}
	EACH(dir) {
		printf("%s|%s\n", dir[i], dir[i] + strlen(dir[i]) + 1);
	}
	return (0);
}
