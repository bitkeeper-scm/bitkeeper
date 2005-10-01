#include <time.h>
#include <stdio.h>
#include "system.h"
#include "sccs.h"

int
testdates_main(int argc, char **argv)
{
	time_t	m, y, start, t;

#ifdef	START
	start = START;
#else
	start = time(0);
#endif
	y = m = start;
	for (t = start + 1; t < 2114380800; ) {
		testdate(t);
		t += 11;
		if (t > (m + 2628000)) {
			printf("MONTH=%s\n", testdate(t));
			fflush(stdout);
			m += 2628000;
		}
		if (t > (y + 31536000)) {
			printf("YEAR=%s\n", testdate(t));
			fflush(stdout);
			y += 31536000;
		}
	}
	return (0);
}
