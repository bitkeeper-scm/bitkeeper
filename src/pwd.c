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
		proj_free(bk_proj);
		bk_proj = proj_init(0);
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
