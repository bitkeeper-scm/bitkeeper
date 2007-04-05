#include "sccs.h"
#include "binpool.h"

private FILE	*server(void);

/*
 * Simple key sync.
 * Receive a list of keys on stdin and return a list of
 * keys not found locally.
 * Currently only -B (for binpool) is implemented.
 */
int
havekeys_main(int ac, char **av)
{
	char	*dfile;
	int	c;
	int	rc = 0, binpool = 0;
	FILE	*f = 0;
	char	buf[MAXLINE];

	while ((c = getopt(ac, av, "Bq")) != -1) {
		switch (c) {
		    case 'B': binpool = 1; break;
		    case 'q': break;	/* ignored for now */
		    default:
usage:			fprintf(stderr, "usage: bk %s [-q] [-B] -\n", av[0]);
			return (1);
		}
	}
	unless (av[optind] && streq(av[optind], "-")) goto usage;
	if (proj_cd2root()) {
		fprintf(stderr, "%s: must be run in a bk repository.\n",av[0]);
		return (1);
	}
	unless (binpool) {
		fprintf(stderr, "%s: only -B(binpool) supported.\n", av[0]);
		return (1);
	}
	/*
	 * What this should do is on the first missing key, popen another
	 * havekeys to our server (if we have one, else FILE * is just stdout)
	 * and then fputs the missing keys to that one which will filter through
	 * both our local binpool and our servers and send back the list of 
	 * keys that neither of us have.
	 *
	 * XXX - the optimization we want is to know if the server is local
	 * and do the work directly.
	 * XXX - need to make sure this is locked.
	 * XXX - what if the server has a server?  Should we keep going?
	 * If we do we need to remember all the places we looked so we
	 * don't loop and deadlock.  Fun!
	 *
	 * XXX - this is going to fail when we can't lock the bp server.
	 * In that case, annoying though it is, I believe we want to have
	 * cached all the keys and send them to stdout and get them locally.
	 * We want to make sure we lock and unlock as fast as possible.
	 */
	while (fnext(buf, stdin)) {
		chomp(buf);
		unless (dfile = bp_lookupkeys(0, buf)) {
			unless (f) f = server();
			// XXX - need to check error status.
			fprintf(f, "%s\n", buf);	/* not here */
		}
		free(dfile);
	}
	if (f && (f != stdout) && pclose(f)) {
		// XXX - should send all the keys we don't have.
		fprintf(stderr, "havekeys: server error\n");
		rc = 1;
	}
	fflush(stdout);
	return (rc);
}

/*
 * Figure out if we have a server and if we do go run a havekeys there.
 */
private FILE *
server(void)
{
	char	*repo;
	FILE	*f;
	char	buf[MAXPATH];

	if (bp_serverID(&repo)) return (stdout);	// OK?
	if (repo == 0) return (stdout);
	free(repo);
	sprintf(buf,
	    "bk -q@'%s' -Lr havekeys -B -", proj_configval(0,"binpool_server"));
	f = popen(buf, "w");
ttyprintf("f=%x buf=%s\n", f, buf);
	return (f ? f : stdout);
}
