#include "system.h"
#include "sccs.h"
#include "bkd.h"

#define	GZ_TOBKD	1	/* gzip the stdin we send */
#define	GZ_FROMBKD	2	/* ungzip the stdout we receive */

private int	doit(char **av,
		    char *url, int quiet, u32 bytes, char *input, int gzip);
private	void	stream_stdin(remote *r, int gzip);

/*
 * Turn
 *	bk [-[sfiles]r] -@[URL] cmd [....]
 * into remote calls to
 *	bk [-[sfiles]r] cmd [...]
 * in the URL or parent repostory or repostories.
 *
 * -@@filename gets the list of urls from filename.
 * -q turns off per repo headers.
 *
 * XXX - on multiple parents do we stop on first error?
 */
int
remote_bk(int quiet, int ac, char **av)
{
	int	i, j, ret = 0, gzip = GZ_TOBKD|GZ_FROMBKD;
	u32	bytes = 0;
	char	*p;
	char	**l, **urls = 0, *input = 0;
	char	**data = 0;
	char	buf[63<<10];

	setmode(0, _O_BINARY);	/* need to be able to read binary data */
	/*
	 * parse the options between 'bk' and the command to remove -@
	 */
	for (i = 1; av[i]; i++) {
		if (av[i][0] != '-') break;

		/* find '@<opt>' argument, but don't look in -r<dir> */
		if ((p = strchrs(av[i], "Lr@")) && (*p == '@')) {
			if (streq(p, "@")) {
				unless (l = parent_allp()) {
					fprintf(stderr,
					    "failed: repository "
					    "has no default parent\n");
					ret = 1;
					goto out;
				}
				EACH_INDEX(l, j) urls = addLine(urls, l[j]);
				freeLines(l, 0);
				l = 0;
			} else if (strneq(p, "@@", 2)) {
				unless (l = file2Lines(urls, p+2)) {
					perror(p+2);
					ret = 1;
					goto out;
				}
				EACH_INDEX(l, j) urls = addLine(urls, l[j]);
				freeLines(l, 0);
				l = 0;
			} else {
				urls = addLine(urls, strdup(p+1));
			}
			/* turn -@ into -q since that is harmless */
			strcpy(p, "q");
		}

		/* look for -z for compression, but pass it on */
		if (streq(av[i], "-z0")) gzip = 0;
		if (streq(av[i], "-zi0")) gzip &= ~GZ_TOBKD;
		if (streq(av[i], "-zo0")) gzip &= ~GZ_FROMBKD;
	}
	assert(urls);
	if (p = getenv("_BK_REMOTEGZIP")) gzip = atoi(p);

	/*
	 * If we have multiple URLs or are talking to a http server
	 * then we need to buffer up stdin.
	 * sizeof(buf) is 63k above, it can't be 64K or data_append()
	 * will crash on a 16-bit limitation.
	 */
	if (streq(av[ac-1], "-") &&
	    ((nLines(urls) > 1) || strneq(urls[1], "http", 4))) {
		while ((i = fread(buf, 1, sizeof(buf), stdin)) > 0) {
			data = data_append(data, buf, i, 0);
		}
		input = data_pullup(&bytes, data);
	}

	EACH(urls) {
		ret |= doit(av, urls[i], quiet, bytes, input, gzip);

		/* see if our output is gone, they may have paged output */
		if (fflush(stdout)) {
			ttyprintf("Exiting early\n");
			break;
		}
	}
	if (input) free(input);
out:	freeLines(urls, free);
	return (ret);
}

private int
doit(char **av, char *url, int quiet, u32 bytes, char *input, int gzip)
{
	remote	*r = remote_parse(url, REMOTE_BKDURL);
	FILE	*f;
	int	i, rc, fd, did_header = 0;
	char	*u = 0;
	char	*p, *tmpf;
	u8	*line;
	int	dostream;
	zgetbuf	*zin = 0;
	zputbuf	*zout = 0;
	char	buf[BSIZE];	/* must match bkd_misc.c:cmd_bk()/buf */

	unless (r) return (1<<2);
	r->remote_cmd = 1;
	if (bkd_connect(r)) return (1<<3);
	u = remote_unparse(r);
	tmpf = bktmp(0, "rcmd");
	f = fopen(tmpf, "w");
	assert(f);
	sendEnv(f, NULL, r, proj_root(0) ? 0 : SENDENV_NOREPO);
	add_cd_command(f, r);
	/* Force the command name to "bk" because full paths aren't allowed */
	fprintf(f, "bk ");
	for (i = 1; av[i]; i++) {
		fprintf(f, "%s ", p = shellquote(av[i]));
		free(p);
	}
	fprintf(f, "\n");
	dostream = (!input && streq(av[i-1], "-"));
	if ((gzip & GZ_TOBKD) && !dostream) {
		zout = zputs_init(zputs_hfwrite, f, -1);
	}
	if (input) {
		assert(bytes > 0);
		while (bytes > 0) {
			i = min(bytes, sizeof(buf));
			sprintf(buf, "@STDIN=%u@\n", i);
			if (zout) {
				zputs(zout, buf, strlen(buf));
				zputs(zout, input, i);
			} else {
				fputs(buf, f);
				fwrite(input, 1, i, f);
			}
			input += i;
			bytes -= i;
		}
	}
	unless (dostream) {
		strcpy(buf, "@STDIN=0@\n");
		if (zout) {
			zputs(zout, buf, strlen(buf));
		} else {
			fputs(buf, f);
		}
	}
	if (zout) zputs_done(zout);
	fclose(f);
	rc = send_file(r, tmpf, 5);
	unlink(tmpf);
	free(tmpf);
	if (rc) goto out;
	if (dostream) stream_stdin(r, gzip);
	writen(r->wfd, "quit\n", 5);
	unless (r->rf) r->rf = fdopen(r->rfd, "r");
	if (r->type == ADDR_HTTP) skip_http_hdr(r);
	line = (getline2(r, buf, sizeof(buf)) > 0) ? buf : 0;
	unless (line) {
		i = 1<<5;
		fprintf(stderr, "##### %s #####\n", u);
		fprintf(stderr, "Protocol error, aborting.\n");
		goto out;
	}
	if (strneq("ERROR-BAD CMD: bk", line, 17)) {
		fprintf(stderr,
		    "error: bkd does not support remote commands (%s)\n",
		    url);
		i = 63;
		goto out;
	}
	if (strneq("ERROR-", line, 6)) {
err:		fprintf(stderr, "##### %s #####\n", u);
		if (p = strchr(line+6, '\n')) *p = 0; /* terminate line */
		fprintf(stderr, "%s\n", &line[6]);
		i = 1<<5;
		goto out;
	}
	if (streq("@GZIP@", line)) {
		zin = zgets_initCustom(zgets_hfread, r->rf);
		line = zgets(zin);
		unless (line) {
			i = 1<<6;
			goto out;
		}
	}
	while (strneq(line, "@STDOUT=", 8) || strneq(line, "@STDERR=", 8)) {
		bytes = atoi(&line[8]);
		assert(bytes <= sizeof(buf));
		fd = strneq(line, "@STDOUT=", 8) ? 1 : 2;
		if (zin) {
			i = zread(zin, buf, bytes);
		} else {
			i = read_blk(r, buf, bytes);
		}
		if (i == bytes) {
			unless (quiet || did_header) {
				printf("##### %s #####\n", u);
				if (fflush(stdout)) {
					i = 1<<6;
					goto out;
				}
				did_header = 1;
			}
			writen(fd, buf, i);
			bytes -= i;
		} else {
			perror("read/recv in bk -@");
			break;
		}
		if (zin) {
			line = zgets(zin);
		} else {
			line = fnext(buf, r->rf) ? buf : 0;
		}
		unless (line) {
			i = 1<<6;
			goto out;
		}
	}
	if (strneq("ERROR-", line, 6)) goto err;
	unless (sscanf(line, "@EXIT=%d@", &i)) i = 100;
out:	if (zin) zgets_done(zin);
	disconnect(r, 1);
	wait_eof(r, 0);
	disconnect(r, 2);
	return (i);
}
private void
stream_stdin(remote *r, int gzip)
{
	int	i;
	FILE	*f;
	zputbuf	*zout = 0;
	char	line[64];
	char	buf[BSIZE];

	f = fdopen(dup(r->wfd), "w"); /* dup() so fclose leaves r->wfd open */
	if (gzip & GZ_TOBKD) zout = zputs_init(zputs_hfwrite, f, -1);
	while ((i = fread(buf, 1, sizeof(buf), stdin)) > 0) {
		sprintf(line, "@STDIN=%u@\n", i);
		if (zout) {
			zputs(zout, line, strlen(line));
			zputs(zout, buf, i);
		} else {
			writen(r->wfd, line, strlen(line));
			writen(r->wfd, buf, i);
		}
	}
	sprintf(line, "@STDIN=0@\n");
	if (zout) {
		zputs(zout, line, strlen(line));
	} else {
		writen(r->wfd, line, strlen(line));
	}
	if (zout) zputs_done(zout);
	fclose(f);
}
