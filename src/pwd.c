/* %K% Copyright (c) 1999 Andrew Chang */
#ifdef WIN32
#include <windows.h>
#endif
#include "system.h"
#include "sccs.h"



private char *usage = "pwd [-sc] [path]\n";

pwd_main(int ac, char **av)
{
	char	buf[1024], *p;
	int	c, shortname = 0, cygwin = 0;

	setmode(1, _O_BINARY);
	while ((c = getopt(ac, av, "sc")) != -1) {
		switch (c) {
			case 's': shortname = 1; break;
			case 'c': cygwin = 1; break; /* output cygwin path */
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
#ifdef WIN32
	if (*p == '/') {
		buf[1] = 'A' + _getdrive() - 1;
		buf[2] = ':';
		p = &buf[1];
	}
	if (shortname) GetShortPathName(p, p, sizeof buf - (p - buf));
	if (cygwin) { 
		p[1] = p[0];
		p[-1] = p[0] = '/'; 
		p--;
	}
#endif
	printf("%s\n", p);
	return (0);
}
