#include "system.h"
#include "sccs.h"

/* Look for files containing binary data that BitKeeper cannot handle.
 * This consists of (a) NULs
 */
int
isascii_main(int ac, char **av)
{
	if (ac == 2 && streq("--help", av[1])) {
		system("bk help isascii");
		return (1);
	}

	if (ac != 2) {
		system("bk help -s isascii");
		return (1);
	}
	return (!ascii(av[1]));
}


private int
printable(char c)
{
	if (isprint(c)) return (1);
	if (isspace(c)) return (1);
	return (0);
}



/*
 * Win32 note: This may not work on WIN98, it seems to have problem
 * sending a mmaped buffer down a pipe.
 * We will fix it when we  port this code to win32
 */
void
printFilteredText(MMAP *m)
{
	u8	*p, *end;
	int	cnt = 0;
#define	MIN_LEN	8

	for (p = (u8*)m->where, end = (u8*)m->end; p < end; ) {
		if (printable(*p)) {
			cnt++;
			p++;
		} else {
			if (cnt >= MIN_LEN) {
				write(1, &p[-cnt], cnt);
				unless(p[-1] == '\n') write(1, "\n", 1);
			}
			cnt = 0;
			p++;
		}
	}
	if (cnt >= MIN_LEN) {
		write(1, &p[-cnt], cnt);
	}
}




int
strings_main(int ac, char **av)
{
	MMAP    *m;

	if (ac != 2) {
		fprintf(stderr, "usage: bk string file\n");
		return (1);
	}

	m = mopen(av[1], "b");
	if (!m) return (2);
	printFilteredText(m);
	mclose(m);
	return (0);
}
