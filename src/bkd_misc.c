/*
 * Simple TCP server.
 */
#include "bkd.h"
extern	char *vRootPrefix;

private	int	cd2vroot(void);

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


private int
cd2vroot()
{
	char	*vhost, *vroot, *p;

	/*
	 * Get first token in BK_VHOST
	 */
	unless (vhost = getenv("BK_VHOST")) {
		out("ERROR-BK_VHOST missing\n");
err:		drain();
		return (1);
	}
	unless (p = strchr(vhost, '.')) {
		out("ERROR-BK_VHOST should have more then one token\n");
		goto err;
	}
	*p = 0;
	assert(vRootPrefix);
	vroot = aprintf("%s/%c/%s/bk", vRootPrefix, vhost[0], vhost);
	*p = '.';
	mkdirp(vroot);
	assert(IsFullPath(vroot));
	if (chdir(vroot)) {
		out("ERROR-cannot cd to virtual root\n");
		goto err;
	}
	free(vroot);
	return (0);
}

int
cmd_putenv(int ac, char **av)
{
	char	*p;
	int	len;

	unless (av[1])  return (1);

	p = strchr(av[1], '=');
	unless (p) return (1);
	len = p - av[1];
	/*
	 * For security, we dis-allow setting PATH and IFS
	 * We also disallow anything not starting with one of
	 * _BK_, BK_, or BKD_.  Not sure we need the BKD_, but hey.
	 */
	if ((len == 3) && strneq(av[1], "IFS", 3)) return (1);
	if ((len == 4) && strneq(av[1], "PATH", 4)) return (1);
	unless (strneq("BK_", av[1], 3) ||
	    strneq("BKD_", av[1], 4) || strneq("_BK_", av[1], 4)) {
	    	return (1);
	}
	putenv(strdup(av[1])); /* XXX - memory is not released until we exit */
	if (strneq("BK_USER=", av[1], 8)) sccs_resetuser();

	/*
	 * Special processing for virtual host/root
	 */
	if (vRootPrefix && (len == 8) && strneq(av[1], "BK_VHOST", 8)) {
		if (cd2vroot()) return (1);
	}
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
