/* %K% Copyright (c) 1999 Andrew Chang */
#include "system.h"
#include "sccs.h"



private char *usage = "pwd [-scr] [path]\n";

int
pwd_main(int ac, char **av)
{
	char	buf[MAXPATH], *p;
	int	c, shortname = 0, cygwin = 0, bk_rpath = 0;

	setmode(1, _O_BINARY);
	while ((c = getopt(ac, av, "scr")) != -1) {
		switch (c) {
			case 's': shortname = 1; break;
			case 'c': cygwin = 1; break; /* output cygwin path */
			case 'r': bk_rpath = 1; break; /* bk relative path */
			default: fprintf(stderr, usage); exit(1);
		}
	}
	
	/*
	 * If a pathname is supplied, chdir there first
	 * before we do a getcwd(). This is a efficient way
	 * to convert a relative path to a full path
	 */
	if (av[optind]) {
		if (chdir(av[optind]) != 0) return (1);
	}
	p = &buf[3]; /* reserve same space in front, we might need it below */
	if (getRealCwd(p, sizeof buf -3) == NULL){
		perror("getcwd");
	}

#ifdef WIN32 /* handle drive, shortname and cygwin path */
	if (*p == '/') {
		buf[1] = 'A' + _getdrive() - 1;
		buf[2] = ':';
		p = &buf[1];
	}
	if (shortname) {
		GetShortPathName(p, p, sizeof buf - (p - buf));
		nt2bmfname(p, p); /* needed for win98 */
	}
	if (cygwin) { 
		printf("/cygdrive/");
		p[1] = p[0];
		p++;
	}
#endif
	printf("%s\n", bk_rpath ? _relativeName(p, 1, 0, 1, 1, 0, 0): p);
	return (0);
}

/*
 * Return the full pathname to a config file.
 *   old = path to old name under home directory
 *   new = path to new name under ~/.bk
 *
 * If the new file hasn't been created yet, then the old
 * one is moved to the new location.
 */
char *
findDotFile(char *old, char *new, char *buf)
{
	char	*home;
	char	oldpath[MAXPATH];

	concat_path(buf, getBkDir(), new);
	unless (exists(buf)) {
		/* need to upgrade? */
		home = getHomeDir();
		concat_path(oldpath, home, old);
		if (exists(oldpath)) {
			rename(oldpath,  buf);
			if (strrchr(old, '/')) {
				*strrchr(oldpath, '/') = 0;
				/* oldpath now points to parent dir */
				if (emptyDir(oldpath)) rmdir(oldpath);
			}
		}
		free(home);
	}
	mkdirf(buf);
	return (buf);
}

int
dotbk_main(int ac, char **av)
{
	char	buf[MAXPATH];

	if (ac == 3) {
		puts(findDotFile(av[1], av[2], buf));
	} else if (ac == 1) {
		puts(getBkDir());
	} else {
		fprintf(stderr, "usage: bk dotbk [old new]\n");
		return (1);
	}
	return (0);
}
