#include <stdio.h>
#ifdef WIN32
#include <stdlib.h>
#endif

void undos(char *s);

int
main(int ac, char **av)
{
	FILE	*f;
	char	buf[1024];

	if (ac != 2) {
		printf("usage: %s filename\n", av[0]);
		exit(1);
	}
	f = fopen(av[1], "r");
	if (!f) {
		perror(av[1]);
		exit(1);
	}
	while (fgets(buf, sizeof(buf), f)) {
		undos(buf);
		puts(buf);
	}

	return (0);
}

/* kill the newline and the \r */
void
undos(register char *s)
{
	if (!s[0]) return;
	while (s[1]) s++;
	if (s[-1] == '\r') {
		s[-1] = 0;
	}
	if (s[0] == '\n')
	    s[0] = 0;
}
