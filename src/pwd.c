/* %K% Copyright (c) 1999 Andrew Chang */
#ifdef WIN32
#include <windows.h>
#endif
#include "system.h"
#include "sccs.h"



private char *usage = "pwd [-scf] path\n";

pwd_main(int ac, char **av)
{
	char buf[1024], realname[MAXPATH], *p;
	int c, shortname = 0, cygwin = 0, forwardSlash = 0;
	extern	void platformSpecificInit(char *);

	setmode(1, _O_BINARY);
	while ((c = getopt(ac, av, "scf")) != -1) {
		switch (c) {
			case 's': shortname = 1; break;
			case 'c': cygwin = 1; break; /* output cygwin style path */
			case 'f':  forwardSlash = 1; break;
			default: fprintf(stderr, usage); exit(1);
		}
	}
	
	/*
	 * If a pathname is supplied, chdir there first
	 * before we do a getcwd(). This is a efficient way
	 * to convert a relative path to a full path
	 */
	if (av[optind]) {
		if (chdir(av[optind]) != 0) exit(1);
	}
	p = &buf[1];
	if (getcwd(p, sizeof buf -1) == NULL){
		perror("getcwd");
	}
#ifdef WIN32
	if (shortname) GetShortPathName(p, p, sizeof(buf));
	if (forwardSlash) nt2bmfname(buf, buf);
#endif
	if (cygwin) { 
		buf[2] = buf[1];
		buf[0] = buf[1] = '/'; 
		p = buf;
	}
	getRealName(p, NULL, realname);
	printf("%s\n", realname);
	return (0);
}
