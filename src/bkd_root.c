#include "bkd.h"

/*
 * Set or show the root pathname.
 *
 * Usage: root [/full/path/name]
 */
int
cmd_root(int ac, char **av, int in, int out, int err)
{
	int	c, key = 0;

	while ((c = getopt(ac, av, "k")) != -1) {
		switch (c) {
		    case 'k': key = 1; break;
		}
	}

	if (av[optind] && !av[optind+1]) {
		if (chdir(av[optind])) {
			writen(err, "ERROR: Can not change to directory '");
			writen(err, av[1]);
			writen(err, "'\n");
			return (-1);
		}
	} else if (!av[optind]) {
		char	buf[MAXPATH];

		if (key) {
			FILE	*p;
			
			p = popen("bk prs -hr+ -d:ROOTKEY: ChangeSet", "r");
			if (fnext(buf, p)) {
				writen(out, buf);
			} else {
				writen(out, "ERROR: no root key found\n");
				return (-1);
			}
			pclose(p);
		} else {
			if (getcwd(buf, sizeof(buf))) {
				writen(out, buf);
				writen(out, "\n");
			} else {
				writen(out, "ERROR: can't get CWD\n");
			}
		}
	} else {
		writen(err, "usage: root [-k] OR [pathname]\n");
		return (-1);
	}
	return (0);
}
