/*
 * UUencode/decode for BitSCCS.
 * 
 * This version doesn't bother converting spaces to backquotes since we aren't
 * mailing SCCS files around (yet).
 *
 * %W% %@%
 */
#include <stdio.h>
#include "system.h"
#include "sccs.h"

int
uuencode_main(int ac, char **av)
{
	FILE	*f;

	if (av[1]) {
		unless (f = fopen(av[1], "r")) {
			perror(av[1]);
			exit(1);
		}
		uuencode(f, stdout);
		fclose(f);
	} else {
		uuencode(stdin, stdout);
	}
	return (0);
}

int
uudecode_main(int ac, char **av)
{
	FILE	*f;

	if (av[1]) {
		unless (f = fopen(av[1], "wb")) {
			perror(av[1]);
			exit(1);
		}
		uudecode(stdin, f);
		fclose(f);
	} else {
		uudecode(stdin, stdout);
	}
	return (0);
}
