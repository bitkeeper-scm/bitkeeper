/*
 * Stat the file and print out the time as YYYY-MM-DD-HH-MM-SS
 */
#include <stdio.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>

int
main(int ac, char **av)
{
	struct	stat st;
	struct	tm *t;

	if (ac != 2) {
		fprintf(stderr, "usage: %s file\n", av[0]);
		return (1);
	}
	if (stat(av[1], &st)) {
		perror(av[1]);
		return (2);
	}

	/*
	 * GNU's touch seems to set things in local time.
	 * We'll see if this is portable.
	 */
	t = localtime(&st.st_mtime);
	printf("%d-%02d-%02d-%02d:%02d:%02d\n",
	    t->tm_year + 1900,
	    t->tm_mon + 1, 
	    t->tm_mday,
	    t->tm_hour,
	    t->tm_min,
	    t->tm_sec);

	return (0);
}
