#include <time.h>
#include <stdio.h>

#define	DAY	(60*60*24)
#define	WEEK	(DAY*7)
#define	MONTH	(WEEK*4)
#define	YEAR	(DAY*365)

main()
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
}
