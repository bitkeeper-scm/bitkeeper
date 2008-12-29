#include "sccs.h"
#include "bkd.h"
#include "bam.h"

private FILE	*server(int recurse);

/*
 * Simple key sync.
 * Receive a list of keys on stdin and return a list of
 * keys not found locally.
 * Currently only -B (for BAM) is implemented.
 * With -B, if the key is a 4 tuple, ignore the first one, that's the size,
 * but send it back so we can calculate the total amount to be sent.
 *
 * When calling this for a remote BAM server, the command line should
 * look like this:
 *     bk -q@URL -Lr -Bstdin havekeys -B [-l] -
 *  (read lock, compress both ways, buffer input)
 */
int
havekeys_main(int ac, char **av)
{
	char	*dfile, *key;
	int	c;
	int	rc = 0, BAM = 0, recurse = 1;
	FILE	*f = 0;
	char	buf[MAXLINE];

	while ((c = getopt(ac, av, "Blq")) != -1) {
		switch (c) {
		    case 'B': BAM = 1; break;
		    case 'l': recurse = 0; break;
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
	unless (BAM) {
		fprintf(stderr, "%s: only -B(BAM) supported.\n", av[0]);
		return (1);
	}
	/*
	 * What this should do is on the first missing key, popen another
	 * havekeys to our server (if we have one, else FILE * is just stdout)
	 * and then fputs the missing keys to that one which will filter through
	 * both our local BAM pool and our servers and send back the list of 
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
 * XXX - this doesn't handle loops other than just failing miserably
 * if the server is locked.
 */
private FILE *
server(int recurse)
{
	char	*p;
	FILE	*f;

	unless (recurse) return (stdout);
	unless (bp_serverID(1)) return (stdout);
	p = aprintf("bk -q@'%s' -Lr -Bstdin havekeys -B -l -",
	    bp_serverURL());
	f = popen(p, "w");
	free(p);
	return (f ? f : stdout);
}

/*
 * An optional last part of the bkd connection for clone and pull that
 * examines the incoming data and requests BAM keys that are missing.
 * from the local server.
 * This extra pass is only called if BKD_BAM=YES indicating that the
 * remote bkd has BAM data, and if we share the same BAM server then
 * we won't request data.
 */
int
bkd_BAM_part3(remote *r, char **envVar, int quiet, char *range)
{
	FILE	*f;
	int	i, bytes, rc = 1, err;
	u64	sfio;
	char	*p;
	char	cmd_file[MAXPATH];
	char	buf[BSIZE];	/* must match remote.c:doit()/buf */

	if ((r->type == ADDR_HTTP) && bkd_connect(r, 1, !quiet)) {
		return (-1);
	}
	bktmp(cmd_file, "BAMmsg");
	f = fopen(cmd_file, "w");
	assert(f);
	sendEnv(f, envVar, r, 0);
	/*
	 * No need to do "cd" again if we have a non-http connection
	 * because we already did a "cd" in part 1
	 */
	if (r->path && (r->type == ADDR_HTTP)) add_cd_command(f, r);

	if (bp_fetchData() == 2) {
		/* we do want to recurse here, havekeys did */
		fprintf(f, "bk -zo0 -Bstdin sfio -oqB -\n");
	} else {
		fprintf(f, "bk -zo0 -Bstdin sfio -oqBl -\n");
	}
	if (rc = bp_sendkeys(f, range, &sfio)) goto done;
	fprintf(f, "rdunlock\n");
	fprintf(f, "quit\n");
	fclose(f);
	if ((sfio > 0) && !quiet) {
		p = remote_unparse(r);
		fprintf(stderr, "Fetching BAM files from %s...\n", p);
		free(p);
	}
	rc = send_file(r, cmd_file, 0);
	if (rc) goto done;

	if (r->type == ADDR_HTTP) skip_http_hdr(r);
	unless (r->rf) r->rf = fdopen(r->rfd, "r");

	f = 0;
	buf[0] = 0;
	while (fnext(buf, r->rf) &&
	    (strneq(buf, "@STDERR=", 8) || strneq(buf, "@STDOUT=", 8))) {
		err = (buf[4] == 'E'); /* to stderr? */
		bytes = atoi(&buf[8]);
		if (bytes == 0) break;
		while (bytes > 0) {
			i = min(sizeof(buf), bytes);
			if  ((i = fread(buf, 1, i, r->rf)) <= 0) break;
			if (err) {
				fwrite(buf, 1, i, stderr);
			} else {
				unless (f) {
					p = aprintf("bk sfio -ir%sBb%s -",
					    quiet ? "q":"", psize(sfio));
					f = popen(p, "w");
					free(p);
					assert(f);
				}
				fwrite(buf, 1, i, f);
			}
			bytes -= i;
		}
	}
	if (f && pclose(f)) {
		rc = 1;
		goto done;
	}
	unless (streq(buf, "@EXIT=0@\n")) {
		fprintf(stderr, "bad exit in part3: %s\n", buf);
		rc = 1;
		goto done;
	}
	// XXX error handling

	rc = 0;
done:	disconnect(r, 1);
	wait_eof(r, 0);
	disconnect(r, 2);
	unlink(cmd_file);
	return (rc);
}

int
bp_sendkeys(FILE *fout, char *range, u64 *bytep)
{
	FILE	*f;
	int	debug, len, space, where, ret;
	int	rc = 0;
	char	*cmd, *p;
	zputbuf	*zout;
	char	hdr[64];
	char	line[MAXLINE];
	char	buf[BSIZE];	/* must match remote.c:doit() buf */

	debug = ((p = getenv("_BK_BAM_DEBUG")) && *p);
	*bytep = 0;
	zout = zputs_init(zputs_hfwrite, fout, -1);
	if (ret = bp_fetchData()) {
		/*
		 * If we have a server then we want to recurse one level up
		 * to the server because we don't need the data here, we just
		 * need to be able to get it from somewhere.  So that's the
		 * R1 option to havekeys.
		 */
		cmd = aprintf("bk changes -Bv -nd'$if(:BAMHASH:)"
		    "{|:BAMSIZE:|:BAMHASH: :KEY: :MD5KEY|1.0:}' %s |"
		    "bk havekeys -B%s -", range, ret == 2 ? "" : "l");
		f = popen(cmd, "r");
		assert(f);
		space = sizeof(buf);
		where = 0;
		while (fnext(line, f)) {
			if (debug) ttyprintf("%s", line);
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
