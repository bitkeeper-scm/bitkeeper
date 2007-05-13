#include "sccs.h"
#include "bkd.h"
#include "binpool.h"

private FILE	*server(int recurse);

/*
 * Simple key sync.
 * Receive a list of keys on stdin and return a list of
 * keys not found locally.
 * Currently only -B (for binpool) is implemented.
 * With -B, if the key is a 4 tuple, ignore the first one, that's the size,
 * but send it back so we can calculate the total amount to be sent.
 */
int
havekeys_main(int ac, char **av)
{
	char	*dfile, *key;
	int	c;
	int	rc = 0, binpool = 0, recurse = 0;
	FILE	*f = 0;
	char	buf[MAXLINE];

	while ((c = getopt(ac, av, "BR;q")) != -1) {
		switch (c) {
		    case 'B': binpool = 1; break;
		    case 'R': recurse = atoi(optarg); break;
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

		if (buf[0] == '|') {
			key = strchr(buf+1, '|') + 1;
		} else {
			key = buf;
		}
		unless (dfile = bp_lookupkeys(0, key)) {
			unless (f ) f = server(recurse);
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
server(int recurse)
{
	char	*p;
	FILE	*f;

	unless (recurse > 0) return (stdout);
	if (bp_serverID(&p)) return (stdout);	// OK?
	if (p == 0) return (stdout);
	free(p);
	p = aprintf("bk -q@'%s' -Lr -Bstdin havekeys -BR%d -",
	    proj_configval(0,"binpool_server"), recurse - 1);
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
	int	i, bytes, rc = 1;
	u64	sfio;
	char	*p;
	char	cmd_file[MAXPATH];
	char	buf[BSIZE];	/* must match remote.c:doit()/buf */

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

	/* we do want to recurse one level here, this is the proxy case */
	fprintf(f, "bk -zo0 -Bstdin sfio -oqBR1 -\n");
	fflush(f);
	if (rc = bp_sendkeys(fileno(f), range, &sfio)) goto done;
	fprintf(f, "rdunlock\n");
	fprintf(f, "quit\n");
	fclose(f);
	if ((sfio > 0) && !quiet) {
		p = remote_unparse(r);
		fprintf(stderr, "Fetching binpool files from %s...\n", p);
		free(p);
	}
	rc = send_file(r, cmd_file, 0);
	if (rc) goto done;

	if (r->type == ADDR_HTTP) skip_http_hdr(r);
	unless (r->rf) r->rf = fdopen(r->rfd, "r");

	f = 0;
	buf[0] = 0;
	while (fnext(buf, r->rf) && strneq(buf, "@STDOUT=", 8)) {
		bytes = atoi(&buf[8]);
		if (bytes == 0) break;
		while (bytes > 0) {
			i = min(sizeof(buf), bytes);
			if  ((i = fread(buf, 1, i, r->rf)) <= 0) break;
			unless (f) {
				p = aprintf("bk sfio -ir%sBb%s -",
				    quiet ? "q":"", psize(sfio));
				f = popen(p, "w");
				free(p);
				assert(f);
			}
			fwrite(buf, 1, i, f);
			bytes -= i;
		}
	}
	if (f) pclose(f);
	unless (streq(buf, "@EXIT=0@\n")) {
		fprintf(stderr, "bad exit in part3: %s\n", buf);
		rc = 1;
		goto done;
	}
	// XXX error handling

	rc = 0;
done:	wait_eof(r, 0);
	disconnect(r, 2);
	unlink(cmd_file);
	return (rc);
}

int
bp_sendkeys(int fdout, char *range, u64 *bytep)
{
	FILE	*f;
	int	len, space, where, rc = 0;
	char	*cmd, *p;
	zputbuf	*zout;
	char	hdr[64];
	char	line[MAXLINE];
	char	buf[BSIZE];	/* must match remote.c:doit() buf */

	*bytep = 0;
	zout = zputs_init(zputs_hwrite, int2p(fdout));
	unless (bp_sharedServer()) {
		cmd = aprintf("bk changes -Bv -nd'"
		    "$if(:BPHASH:){|:SIZE:|:BPHASH: :KEY: :MD5KEY|1.0:}' %s |"
		    /* The Wayne meister says one level of recursion.
		     * It's obvious.  Obvious to Leonardo...
		     */
		    "bk havekeys -BR1 -", range);
		f = popen(cmd, "r");
		assert(f);
		space = sizeof(buf);
		where = 0;
		while (fnext(line, f)) {
			p = strchr(line+1, '|'); /* skip size */
			*p++ = 0;
			*bytep += atoi(line+1);
			len = strlen(p);
			if (len > space) {
				sprintf(hdr, "@STDIN=%u@\n", where);
				zputs(zout, hdr, strlen(hdr));
				zputs(zout, buf, where);
				space = sizeof(buf);
				where = 0;
			}
			strcpy(&buf[where], p);
			where += len;
			space -= len;
		}
		if (pclose(f)) rc = 1;
		if (where) {
			sprintf(hdr, "@STDIN=%u@\n", where);
			zputs(zout, hdr, strlen(hdr));
			zputs(zout, buf, where);
		}
	}
	sprintf(hdr, "@STDIN=0@\n");
	zputs(zout, hdr, strlen(hdr));
	if (zputs_done(zout)) rc = 1;
	return (rc);
}
