#include "bkd.h"

/*
 * Set or show the root pathname.
 *
 * Usage: root [/full/path/name]
 */
int
cmd_root(int ac, char **av)
{
	int	c, key = 0;

	while ((c = getopt(ac, av, "k")) != -1) {
		switch (c) {
		    case 'k': key = 1; break;
		}
	}

	if (av[optind] && !av[optind+1]) {
		if (chdir(av[optind])) {
			writen(1, "ERROR: Can not change to directory '");
			writen(1, av[1]);
			writen(1, "'\n");
			return (-1);
		}
		unless (exists("BitKeeper/etc")) {
			writen(1, "ERROR: directory '");
			writen(1, av[1]);
			writen(1, "' is not a project root\n");
			return (-1);
		}
		writen(1, "OK-root OK\n");
	} else if (!av[optind]) {
		char	buf[MAXPATH];

		if (key) {
			FILE	*p;
			
			p = popen("bk prs -hr+ -d:ROOTKEY: ChangeSet", "r");
			if (fnext(buf, p)) {
				writen(1, buf);
			} else {
				writen(1, "ERROR-no root key found\n");
				return (-1);
			}
			pclose(p);
		} else {
			unless (exists("BitKeeper/etc")) {
				writen(1, "ERROR-not at a repository root\n");
			} else if (getcwd(buf, sizeof(buf))) {
				writen(1, buf);
				writen(1, "\n");
			} else {
				writen(1, "ERROR-can't get CWD\n");
			}
		}
	} else {
		writen(1, "ERROR-usage: root [-k] OR [pathname]\n");
		return (-1);
	}
	return (0);
}
