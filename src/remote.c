/*
 * Copyright 2006-2013,2016 BitMover, Inc
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "system.h"
#include "sccs.h"
#include "bkd.h"

private	void	rgzip(FILE *fin, FILE *fout, int opts);

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
 * With multiple parents we don't stop on errors, but instead
 * or the exit status' together.
 *
 * returns
 *   if contact to bkd succeeds
 *      returns exit status from remote command
 *   else
 *      1 argument parsing failure
 *      4 can't parse url
 *      8 bkd_connect failed
 *     16 bkd returns chdir failure
 *     32 protocol failure
 *     33 a 'havekeys' special -- means remote repo and local repo are same
 *     63 bk-remote not supported
 *     64 unexpected eof
 */
int
remote_bk(int quiet, int ac, char **av)
{
	int	i, ret = 0, gzip = REMOTE_GZ_SND|REMOTE_GZ_RCV;
	FILE	*fin = 0;
	int	opts = 0;
	char	*p;
	char	**urls = 0;
	char	buf[8<<10];

	setmode(0, _O_BINARY);	/* need to be able to read binary data */
	putenv("BKD_NESTED_LOCK="); /* don't inherit nested lock */

	/*
	 * parse the options between 'bk' and the command to remove -@
	 */
	for (i = 1; av[i]; i++) {
		if (av[i][0] != '-') break;
		if (av[i][1] == '-') {
			if (av[i][2]) continue; /* skip long options */
			break;			/* stop at '--' */
		}

		/* find '@<opt>' argument, but don't look in -r<dir> */
		if ((p = strchrs(av[i], "Lsr@")) && (*p == '@')) {
			if (bk_urlArg(&urls, p+1)) {
				ret = 1;
				goto out;
			}
			/* turn -@ into -q since that is harmless */
			strcpy(p, "q");
		}

		/* look for -z for compression, but pass it on */
		if (streq(av[i], "-z0")) gzip = 0;
		if (streq(av[i], "-zi0")) gzip &= ~REMOTE_GZ_SND;
		if (streq(av[i], "-zo0")) gzip &= ~REMOTE_GZ_RCV;
	}
	assert(urls);
	if (p = getenv("_BK_REMOTEGZIP")) gzip = atoi(p);

	opts = (quiet ? SILENT : 0) | gzip | REMOTE_BKDERRS;
	/*
	 * If we have multiple URLs then we need to buffer up stdin.
	 */
	if (streq(av[ac-1], "-")) {
		if (nLines(urls) > 1) {
			/* buffer it */
			fin = fmem();
			while ((i = fread(buf, 1, sizeof(buf), stdin)) > 0) {
				fwrite(buf, 1, i, fin);
			}
		} else {
			/* stream it */
			fin = stdin;
			opts |= REMOTE_STREAM;
		}
	}

	EACH(urls) {
		if (fin && (fin != stdin)) rewind(fin);

		ret |= remote_cmd(av, urls[i], fin, stdout, stderr, 0, opts);

		/* see if our output is gone, they may have paged output */
		if (fflush(stdout)) {
			break;
		}
	}
out:	if (fin && (fin != stdin)) fclose(fin);
	freeLines(urls, free);
	return (ret);
}

int
remote_cmd(char **av, char *url, FILE *in, FILE *out, FILE *err,
    hash *bkdEnv, int opts)
{
	remote	*r = remote_parse(url, REMOTE_BKDURL);
	FILE	*f, *wf;
	int	i, rc, did_header = 0;
	int	stream = (in && (opts & REMOTE_STREAM));
	int	bytes;
	char	*u = 0;
	char	*p, *tmpf;
	u8	*line;
	FILE	*zin = 0;
	char	buf[BSIZE];	/* must match bkd_misc.c:cmd_bk()/buf */

	unless (r) return (1<<2);
	r->remote_cmd = 1;
	if (r->type == ADDR_HTTP) stream = 0;
	if (bkd_connect(r, (opts & REMOTE_BKDERRS) ? 0: SILENT)) return (1<<3);
	u = remote_unparse(r);
	tmpf = bktmp(0);
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
	unless (stream) rgzip(in, f, opts);
	fclose(f);
	rc = send_file(r, tmpf, stream ? 1 : 0);
	unlink(tmpf);
	free(tmpf);
	if (rc) {
		i = 1<<5;
		goto out;
	}
	if (stream) {
		/*
		 * write directly from 'in' to the socket.
		 *
		 * XXX Evenutally this needs to be a loop using
		 *     select() so that this code can start processing
		 *     the data coming from the bkd interleaved with
		 *     sending data.  For now we needs to always add
		 *     -Bstdin to remote commands that read and write
		 *     more than 1k of data.
		 */

		/* dup() so fclose leaves r->wfd open */
		f = fdopen(dup(r->wfd), "w");

		rgzip(in, f, opts);
		fclose(f);
		send_file_extra_done(r);
	}

	unless (r->rf) r->rf = fdopen(r->rfd, "r");
	if (r->type == ADDR_HTTP) skip_http_hdr(r);
	line = (getline2(r, buf, sizeof(buf)) > 0) ? buf : 0;
	unless (line) {
		/* no data at all? connect failure */
		i = 1<<3;
		if (err) fprintf(err, "##### %s #####\n", u);
		if (err) fprintf(err, "Connection fail, aborting.\n");
		goto out;
	}
	if (streq("@SERVER INFO@", buf)) {
		if (getServerInfo(r, bkdEnv)) {
			i = 1<<5;
			goto out;
		}
		line = (getline2(r, buf, sizeof(buf)) > 0) ? buf : 0;
	}

	unless (line) {
		i = 1<<5;
		if (err) fprintf(err, "##### %s #####\n", u);
		if (err) fprintf(err, "Protocol error, aborting.\n");
		goto out;
	}
	if (strneq(buf, "ERROR-cannot use key", 20 ) ||
	    strneq(buf, "ERROR-cannot cd to ", 19)) {
		i = 1<<4;
		if (err) fprintf(err, "##### %s #####\n", u);
		if (err) fprintf(err, "Repository doesn\'t exist.\n");
		goto out;
	}
	if (strneq("ERROR-BAD CMD: bk", line, 17)) {
		if (err) fprintf(err,
		    "error: bkd does not support remote commands (%s)\n",
		    url);
		i = 63;
		goto out;
	}
	if (strneq("ERROR-", line, 6)) {
err:		if (err) fprintf(err, "##### %s #####\n", u);
		if (p = strchr(line+6, '\n')) *p = 0; /* terminate line */
		if (err) fprintf(err, "%s\n", &line[6]);
		/*
		 * N.B.
		 * This is the path taken when remote commands are not
		 * enabled.  clone -@ (clone.c) and clonemod (bk.sh)
		 * look for this return value in response to a remote
		 * level request.
		 */
		i = 1<<5;
		goto out;
	}
	if (streq("@GZIP@", line)) {
		zin = fopen_zip(r->rf, "rh");
		line = fgetline(zin);
		unless (line) {
			i = 1<<6;
			goto out;
		}
	}
	while (strneq(line, "@STDOUT=", 8) || strneq(line, "@STDERR=", 8)) {
		wf = out;
		if (strneq(line, "@STDERR=", 8)) wf = err;
		bytes = atoi(&line[8]);
		assert(bytes <= sizeof(buf));
		if (zin) {
			i = fread(buf, 1, bytes, zin);
		} else {
			i = read_blk(r, buf, bytes);
		}
		if (i == bytes) {
			unless ((opts & SILENT) || did_header) {
				if (wf) fprintf(wf, "##### %s #####\n", u);
				if (wf && fflush(wf)) {
					i = 1<<6;
					goto out;
				}
				did_header = 1;
			}
			if (wf) fwrite(buf, 1, i, wf);
		} else {
			perror("read/recv in bk -@");
			i = 1<<5;
			goto out;
		}
		if (zin) {
			line = fgetline(zin);
		} else {
			line = fnext(buf, r->rf) ? buf : 0;
		}
		unless (line) {
			i = 1<<6;
			goto out;
		}
	}
	if (strneq("ERROR-", line, 6)) goto err;
	unless (sscanf(line, "@EXIT=%d@", &i)) i = 1<<5;
out:	if (zin) fclose(zin);
	wait_eof(r, 0);
	disconnect(r);
	return (i);
}

private void
rgzip(FILE *fin, FILE *fout, int opts)
{
	int	i;
	char	buf[BSIZE];

	if (opts & REMOTE_GZ_SND) {
		fout = fopen_zip(fout, "wh", -1);
	}
	if (fin) {
		while ((i = fread(buf, 1, sizeof(buf), fin)) > 0) {
			fprintf(fout, "@STDIN=%u@\n", i);
			fwrite(buf, 1, i, fout);
		}
	}
	fprintf(fout, "@STDIN=0@\n");
	if (opts & REMOTE_GZ_SND) fclose(fout);
}
