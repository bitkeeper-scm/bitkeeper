#include <stdio.h>
#ifdef WIN32
#include <stdlib.h>
#endif

void undos(char *s);
extern void platformSpecificInit(char *);

int
undos_main(int ac, char **av)
{
	FILE	*f;
	char	buf[1024];

	platformSpecificInit(NULL);
	if (ac != 2) {
		printf("usage: %s filename\n", av[0]);
		exit(1);
	}
	f = fopen(av[1], "rb");
	if (!f) {
		perror(av[1]);
		exit(1);
	}
	buf[0] = 0;
	while (fgets(buf, sizeof(buf), f)) {
		undos(buf);
		fputs(buf, stdout);
	}
	if (!strchr(buf, '\n')) fputc('\n', stdout);
	return (0);
}

/* kill the newline and the \r */
void
undos(register char *s)
{	
	static char last = '\0';
	if (!s[0]) return;
	/*
	 * This code is strange because we need to
	 * handle lines longer than the 1K buffer size 
	 */
	if ((last == '\r') && (s[0] == '\n')) return;
	while (s[1]) s++;
	last = s[0];
	if (last == '\r') *s-- = 0;
	if ((s[-1] == '\r') && (s[0] == '\n')) {
		s[-1] = '\n';
		s[0] = '\0';
		return;
	}
}
