/*
 * Simple TCP server.
 */
#include "bkd.h"

int
cmd_eof(int ac, char **av)
{
	out("OK-Goodbye\n");
	exit(0);
	return (0);	/* lint */
}

int
cmd_help(int ac, char **av)
{
	int	i;

	for (i = 0; cmds[i].name; i++) {
		out(cmds[i].name);
		out(" - ");
		out(cmds[i].description);
		out("\n");
	}
	return (0);
}

char *
vpath_translate(const char *path)
{
	char	*vhost;
	char	buf[MAXPATH];
	const char	*s;
	char	*t;

	unless (path && (vhost = getenv("BK_VHOST"))) return (strdup("."));
	s = path;
	t = buf;
	while (*s) {
		if (*s != '%') {
			*t++ = *s++;
		} else {
			char	*p;
			s++;
			switch (*s) {
			    case '%': *t++ = '%'; break;
			    case 'c': *t++ = vhost[0]; break;
			    case 'h':
				p = vhost;
				while (*p && *p != '.') *t++ = *p++;
				break;
			    case 'H':
				p = vhost;
				while (*p) *t++ = *p++;
				break;
			    default:
				fprintf(stderr,
					"Unknown escape %%%c in -V path\n",
					*s);
				exit(1);
			}
			s++;
		}
	}
	*t = 0;

	return (strdup(buf));
}

int
cmd_putenv(int ac, char **av)
{
	char	*p;
	char	*oldenv;
	char	*var;

	unless (av[1]) return (1);
	unless (p = strchr(av[1], '=')) return (1);

	var = strdup_tochar(av[1], '=');
	/*
	 * We also disallow anything not starting with one of
	 * _BK_, BK_, or BKD_.  Not sure we need the BKD_, but hey.
	 * Also disable BK_HOST, this screws up the locks.
	 */
	if (streq("BK_HOST", var)) return (1);
	unless (strneq("BK_", var, 3) ||
	    strneq("BKD_", var, 4) || strneq("_BK_", var, 4)) {
	    	return (1);
	}

	oldenv = getenv(var);
	unless (oldenv && streq(oldenv, p+1)) {
		if (streq(var, "_BK_USER")) {
			sccs_resetuser();
			putenv(&av[1][1]);	/* convert to BK_USER */
		} else {
			putenv(av[1]);
		}

		if (streq(var, "BK_VHOST")) {
			/*
			 * Lookup new vhost, do path translation and cd to new
			 * bkd root.  only do this once!
			 */
			char	*newpath = vpath_translate(Opts.vhost_dirpath);
			chdir(newpath);
			free(newpath);
		}
	}
	free(var);

	return (0);
}

int
cmd_abort(int ac, char **av)
{
	int	status, rc;

	out("@ABORT INFO@\n");
	status = system("bk abort -f 2>&1");
	rc = WEXITSTATUS(status); 
	fputc(BKD_NUL, stdout);
	fputc('\n', stdout);
	if (rc) printf("%c%d\n", BKD_RC, rc);
	fflush(stdout);
	out("@END@\n");
	return (rc);
}

int
cmd_check(int ac, char **av)
{
	int	status, rc;

	out("@CHECK INFO@\n");
	status = system("bk -r check -a 2>&1");
	rc = WEXITSTATUS(status); 
	fputc(BKD_NUL, stdout);
	fputc('\n', stdout);
	if (rc) printf("%c%d\n", BKD_RC, rc);
	fflush(stdout);
	out("@END@\n");
	return (rc);
}
