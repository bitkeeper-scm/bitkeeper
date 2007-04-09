#include "sccs.h"
#include "bkd.h"
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
	char	*p;
	FILE	*f;

	if (bp_serverID(&p)) return (stdout);	// OK?
	if (p == 0) return (stdout);
	free(p);
	p = aprintf(
	    "bk -q@'%s' -Lr havekeys -B -", proj_configval(0,"binpool_server"));
	f = popen(p, "w");
	free(p);
	return (f ? f : stdout);
}

/*
 * An options last part of the bkd connection for clone and pull that
 * examines the incoming data and requests binpool keys that are missing.
 * from the local server.
 * This extra pass is only called if BKD_BINPOOL=YES indicating that the
 * remote bkd has binpool data, and if we share the same binpool server then
 * we won't request data.
 */
int
bkd_binpool_part3(remote *r, char **envVar, int quiet, char *range)
{
	FILE	*f;
	int	fd, i, bytes, rc = 1;
	char	*cmd, *keys;
	zputbuf	*zout;
	char	cmd_file[MAXPATH];
	char	buf[MAXLINE];

	if ((r->type == ADDR_HTTP) && bkd_connect(r, 1, !quiet)) {
		return (-1);
	}
	bktmp(cmd_file, "binpoolmsg");
	f = fopen(cmd_file, "w");
	assert(f);
	sendEnv(f, envVar, r, 0);
	/*
	 * No need to do "cd" again if we have a non-http connection
	 * becuase we already did a "cd" in part 1
	 */
	if (r->path && (r->type == ADDR_HTTP)) add_cd_command(f, r);

	fprintf(f, "bk -zo0 sfio -oqB -\n");
	zout = zputs_init(0, f);
	unless (bp_sharedServer()) {
		keys = bktmp(0, 0);
		cmd = aprintf("bk changes -Bv -nd'" BINPOOL_DSPEC "' %s |"
		    "bk havekeys -B - > '%s'", range, keys);
		if (system(cmd)) goto done;
		sprintf(buf, "@STDIN=%u@\n", size(keys));
		zputs(zout, buf, strlen(buf));
		fd = open(keys, O_RDONLY);
		assert(fd >= 0);
		while ((i = read(fd, buf, sizeof(buf))) > 0) {
			zputs(zout, buf, i);
		}
		close(fd);
		unlink(keys);
		free(keys);
	}
	sprintf(buf, "@STDIN=0@\n");
	zputs(zout, buf, strlen(buf));
	zputs_done(zout);
	fclose(f);
	rc = send_file(r, cmd_file, 0);
	if (rc) goto done;

	if (r->type == ADDR_HTTP) skip_http_hdr(r);
	unless (r->rf) r->rf = fdopen(r->rfd, "r");

	/* XXX currently writes local, binpool_server might be needed. */
	f = 0;
	while (fnext(buf, r->rf) && strneq(buf, "@STDOUT=", 8)) {
		bytes = atoi(&buf[8]);
		if (bytes == 0) break;
		while (bytes > 0) {
			i = min(sizeof(buf), bytes);
			if  ((i = fread(buf, 1, i, r->rf)) <= 0) break;
			unless (f) {
				f = popen("bk sfio -iqB -", "w");
				assert(f);
			}
			fwrite(buf, 1, i, f);
			bytes -= i;
		}
	}
	if (f) pclose(f);
	// XXX error handling

	rc = 0;
done:	wait_eof(r, 0);
	disconnect(r, 2);
	unlink(cmd_file);
	return (rc);
}
