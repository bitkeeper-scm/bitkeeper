/*
 * what - look for SCCS what strings.
 *
 * Copyright (c) 1997 by Larry McVoy; All rights reserved.
 *
 */
#include "system.h"
#include "sccs.h"

private	int	print_id(char *file);

int
what_main(int ac, char **av)
{
	char	*name, *gfile;

	for (name = sfileFirst("what", &av[1], 0); name; name = sfileNext()) {
		gfile = sccs2name(name);
		print_id(gfile);
		free(gfile);
	}
	sfileDone();
	return (0);
}

private int
print_id(char *file)
{
	FILE	*f;
	char	*p, *cmd;
	int	printed;
	char	buf[MAXLINE];

	cmd = aprintf("bk cat '%s'", file);
	unless (f = popen(cmd, "r")) {
		free(cmd);
		return (1);
	}
	free(cmd);
	while (fnext(buf, f)) {
		unless (p = strstr(buf, "@(#)")) continue;
		printed = 0;
		p += 4;
		for (;;) {
			if ((*p == '\r') ||
			    (*p == '\n') || (*p == '"') || (*p == 0)) break;
			unless (printed) {
				printf("%s:", file);
				printed = 1;
			}
			putchar(*p);
			p++;
		}
		if (printed) putchar('\n');
	}
	pclose(f);
	return (0);
}
