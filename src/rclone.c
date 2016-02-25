/*
 * Copyright 2001-2013,2016 BitMover, Inc
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

#include "bkd.h"
#include "nested.h"
#include "progress.h"

private	struct {
	u32	debug:1;		/* -d debug mode */
	u32	quiet:1;		/* -q shut up */
	u32	verbose:1;		/* -v old style noise */
	u32	detach:1;		/* is detach command? */
	u32	transaction:1;		/* is _BK_TRANSACTION set */
	u32	sendenv_flags;		/* flags for sendEnv(); */
	char	*rev;
	char	*bam_url;		/* -B URL */
	u32	in, out;
	u32	jobs;			/* -j%d */
	u64	bpsz;
	char	**av;			/* ensemble commands */
	char	**aliases;		/* ensemble aliases list */
	char	*sfiotitle;		/* pass this down */
} opts;

private int rclone(char **av, remote *r, char **envVar);
private int rclone_part1(remote *r, char **envVar);
private int rclone_part2(char **av, remote *r, char **ev, char *bp);
private	int rclone_part3(char **av, remote *r, char **ev, char *bp);
private int send_part1_msg(remote *r, char **envVar);
private	int send_sfio_msg(remote *r, char **envVar);
private u32 send_sfio(remote *r, int gzip);
private	int rclone_ensemble(remote *r);

int
rclone_main(int ac, char **av)
{
	int	c, rc, isLocal;
	int	gzip = -1;
	char	*url;
	char    **envVar = 0;
	remote	*l, *r;
	char	buf[MAXLINE];
	longopt	lopts[] = {
		{ "sccs-compat", 300 },		/* old non-remapped repo */
		{ "hide-sccs-dirs", 301 },	/* move sfiles to .bk */
		{ "sfiotitle;", 302 },		/* pass title for sfio */
		{ 0, 0 }
	};

	bzero(&opts, sizeof(opts));
	if (streq(av[0], "_rclone_detach")) {
		opts.detach = 1;
		av[0] = "_rclone";
	}
	while ((c = getopt(ac, av, "B;dE:j;pqr;s;vw|z|", lopts)) != -1) {
		unless ((c == 'r') || (c == 's') || (c > 256)) {
			opts.av = bk_saveArg(opts.av, av, c);
		}
		switch (c) {
		    case 'B': opts.bam_url = optarg; break;
		    case 'd': opts.debug = 1; break;
		    case 'E':
			unless (strneq("BKU_", optarg, 4)) {
				fprintf(stderr,
				    "clone: vars must start with BKU_\n");
				return (1);
			}
			envVar = addLine(envVar, strdup(optarg)); break;
		    case 'j': opts.jobs = atoi(optarg); break;
		    case 'p': break; /* ignore no parent */
		    case 'q': opts.quiet = 1; break;
		    case 'r': opts.rev = optarg; break;
		    case 's':
			opts.aliases = addLine(opts.aliases, strdup(optarg));
			break;
		    case 'v': opts.verbose = 1; break;
		    case 'w': /* ignored */ break;
		    case 'z':
			if (optarg) gzip = atoi(optarg);
			if ((gzip < 0) || (gzip > 9)) gzip = Z_BEST_SPEED;
			break;
		    case 300:	/* --sccs-compat */
			opts.sendenv_flags |= SENDENV_FORCENOREMAP;
			break;
		    case 301:	/* --hide-sccs-dirs */
			opts.sendenv_flags |= SENDENV_FORCEREMAP;
			break;
		    case 302:
			opts.sfiotitle = optarg; break;
		    default: bk_badArg(c, av);
		}
	}

	if (getenv("_BK_TRANSACTION")) opts.transaction = 1;

	/*
	 * Validate arguments
	 */
	unless (av[optind] && av[optind + 1]) usage();
	l = remote_parse(av[optind], REMOTE_BKDURL);
	unless (l) usage();
	isLocal = (l->host == NULL);
	remote_free(l);
	unless (isLocal) usage();

	if (chdir(av[optind])) {
		perror(av[optind]);
		exit(1);
	}
	unless (exists(BKROOT)) {
		fprintf(stderr, "%s is not a BitKeeper root\n", av[optind]);
		exit(1);
	}
	if (!opts.transaction && proj_isComponent(0) && !opts.detach) {
		fprintf(stderr,
		    "clone: clone of a component is not allowed, use -s\n");
		exit(1);
	}
	if (opts.detach && !proj_isComponent(0)) {
		fprintf(stderr, "detach: can detach only a component\n");
		exit(1);
	}
	if (hasLocalWork(GONE)) {
		fprintf(stderr,
		    "clone: must commit local changes to %s\n", GONE);
		exit(1);
	}
	if (hasLocalWork(ALIASES)) {
		fprintf(stderr,
		    "clone: must commit local changes to %s\n", ALIASES);
		exit(1);
	}
	unless (opts.quiet) progress_startMulti();
	r = remote_parse(av[optind + 1], REMOTE_BKDURL);
	unless (r) usage();
	if (r->host && !r->path) {
		fprintf(stderr,
		    "clone: %s needs a path component.\n", av[optind + 1]);
		exit(1);
	}
	if (gzip == -1) gzip = bk_gzipLevel();
	r->gzip_in = gzip;

	/*
	 * If rcloning a master, they MUST provide a back pointer URL to
	 * a master (not necessarily this one).  If they point at a different
	 * server Dr Wayne says we'll magically fill in the missing parts.
	 */
	if (!opts.bam_url && (url = bp_serverURL(buf)) && streq(url, ".")) {
		fprintf(stderr,
		    "clone: when cloning a BAM server -B<url> is required.\n"
		    "<url> must name a BAM server reachable from %s\n",
		    av[optind+1]);
		return (1);
	}

	if (!opts.transaction && proj_isEnsemble(0) && !opts.detach) {
		cmdlog_lock(CMD_NESTED_RDLOCK);
		rc = rclone_ensemble(r);
	} else {
		cmdlog_lock(CMD_RDLOCK);
		rc = rclone(av, r, envVar);
	}
	freeLines(envVar, free);
	freeLines(opts.av, free);
	freeLines(opts.aliases, free);
	remote_free(r);
	return (rc);
}

private int
rclone_ensemble(remote *r)
{
	nested	*n = 0;
	comp	*c;
	char	**vp;
	char	*name, *url;
	char	*dstpath;
	int	errs;
	int	i, j, status, which = 0, k = 0, rc = 0;
	u32	flags = NESTED_PRODUCTFIRST;

	url = remote_unparse(r);

	START_TRANSACTION();
	n = nested_init(0, opts.rev, 0, flags);
	assert(n);
	unless (opts.aliases) {
		unless (opts.aliases = clone_defaultAlias(n)) goto out;
	}
	if (nested_aliases(n, n->tip, &opts.aliases, proj_cwd(), n->pending)) {
		fprintf(stderr, "%s: unable to expand aliases\n", prog);
		rc = 1;
		goto out;
	}
	n->product->alias = 1;
	errs = 0;
	EACH_STRUCT(n->comps, c, i) {
		c->remotePresent = c->alias;	/* used in setFromEnv() */
		if (c->alias && !C_PRESENT(c)) {
			fprintf(stderr,
			    "%s: component %s not present.\n",
			    prog, c->path);
			++errs;
		}
		if (c->alias) k++;
	}
	if (errs) {
		fprintf(stderr, "%s: missing components\n", prog);
		rc = 1;
		goto out;
	}
	urlinfo_setFromEnv(n, url);
	EACH_STRUCT(n->comps, c, j) {
		unless (c->included && c->alias) continue;
		proj_cd2product();
		vp = addLine(0, strdup("bk"));
		if (c->product) {
			vp = addLine(vp, aprintf("--title=%s", PRODUCT));
		} else {
			vp = addLine(vp, aprintf("--title=%d/%d %s", which,
						 k, c->path));
		}
		vp = addLine(vp, strdup("_rclone"));
		if (c->product) {
			vp = addLine(vp, aprintf("--sfiotitle=%s", PRODUCT));
		} else {
			vp = addLine(vp,
			    aprintf("--sfiotitle=%d/%d %s",
			    which, k, c->path));
		}
		EACH(opts.av) vp = addLine(vp, strdup(opts.av[i]));
		vp = addLine(vp, aprintf("-r%s", C_DELTAKEY(c)));
		if (c->product) {
			EACH(opts.aliases) {
				vp = addLine(vp,
				    aprintf("-s%s", opts.aliases[i]));
		    	}
			name = PRODUCT;
			vp = addLine(vp, strdup("."));
			vp = addLine(vp, strdup(url));
		} else {
			name = c->path;
			vp = addLine(vp, strdup(c->path));
			dstpath = key2path(C_DELTAKEY(c), 0, 0, 0);
			vp = addLine(vp, aprintf("%s/%s", url,
				dirname(dstpath)));
			free(dstpath);
		}
		vp = addLine(vp, 0);
		if (opts.verbose) printf("#### %s ####\n", name);
		fflush(stdout);
		status = spawnvp(_P_WAIT, "bk", &vp[1]);
		rc = WIFEXITED(status) ? WEXITSTATUS(status) : 199;
		freeLines(vp, free);
		if (rc) break;
		++which;
	}
	unless (opts.quiet || opts.verbose) {
		title = aprintf("%d/%d %s", which, which, PRODUCT);
		progress_end(PROGRESS_BAR, rc ? "FAILED" : "OK", PROGRESS_MSG);
	}

	unless (rc) urlinfo_write(n);
	/*
	 * XXX - put code in here to finish the transaction.
	 */

out:	free(url);
	nested_free(n);
	STOP_TRANSACTION();
	return (rc);
}

/*
 * XXX - does not appear to do csets-out
 */
private int
rclone(char **av, remote *r, char **envVar)
{
	int	rc;
	char	*bp_keys = 0;
	char	revs[MAXKEY];

	if (opts.rev) {
		sccs	*s;
		ser_t	d;

		s = sccs_csetInit(SILENT);
		d = sccs_findrev(s, opts.rev);
		sccs_free(s);

		unless (d) {
			fprintf(stderr, "clone: rev %s doesn't exist\n",
			    opts.rev);
			rc = 1;
			goto done;
		}
	}
	sprintf(revs, "..%s", opts.rev ? opts.rev : "+");
	safe_putenv("BK_CSETS=%s", revs);
	if (rc = trigger(av[0], "pre"))  goto done;
	if (rc = rclone_part1(r, envVar))  goto done;
	if (bp_hasBAM()) bp_keys = bktmp(0);
	rc = rclone_part2(av, r, envVar, bp_keys);
	if (bp_keys) {
		unless (rc) {
			rc = rclone_part3(av, r, envVar, bp_keys);
		}
		unlink(bp_keys);
	}
	if (rc) {
		putenv("BK_STATUS=FAILED");
	} else {
		putenv("BK_STATUS=OK");
	}
	unless (opts.quiet) {
		progress_end(PROGRESS_BAR, rc ? "FAILED" : "OK", PROGRESS_MSG);
	}
	trigger(av[0], "post");

done:	putenv("BK_CSETS=");
	return (rc);
}

private int
rclone_part1(remote *r, char **envVar)
{
	int	rc;
	char	*p;
	char	buf[MAXPATH];

	if (bkd_connect(r, 0)) return (-1);
	if (send_part1_msg(r, envVar)) return (-1);
	if (r->type == ADDR_HTTP) skip_http_hdr(r);
	if (getline2(r, buf, sizeof(buf)) <= 0) return (-1);
	if (streq(buf, "@SERVER INFO@"))  {
		if (getServerInfo(r, 0)) return (-1);
		if (opts.jobs) {
			// $ bk changes -rbk-5.4 -nd:TIME_T:
			u32	bk5_4 = 1321047729;

			p = getenv("BKD_TIME_T");
			assert(p);
			unless (strtoul(p, 0, 10) > bk5_4) {
				p = remote_unparse(r);
				fprintf(stderr,
				    "%s does not understand -j\n", p);
				exit(1);
			}
		}
	} else {
		drainErrorMsg(r, buf, sizeof(buf));
		exit(1);
	}
	if (getline2(r, buf, sizeof(buf)) <= 0) return (-1);
	if (streq(buf, "@TRIGGER INFO@")) {
		if (getTriggerInfoBlock(r, opts.quiet)) return (-1);
	}
	if (strneq(buf, "ERROR-BAM server URL \"", 22)) {
		if (p = strchr(buf + 22, '"')) *p = 0;
		p = remote_unparse(r);
		fprintf(stderr,
		    "%s: unable to contact BAM server '%s'\n",
		    p, buf + 22);
		free(p);
		return (-1);
	}
	if (get_ok(r, buf, 1)) return (-1);
	if (bp_hasBAM() && !bkd_hasFeature(FEAT_BAMv2)) {
		fprintf(stderr,
		    "clone: please upgrade the remote bkd to a "
		    "BAMv2 aware version (4.1.1 or later).\n");
		return (-1);
	}
	if (r->type == ADDR_HTTP) disconnect(r);
	if (rc = bp_updateServer(getenv("BK_CSETS"), 0, !opts.verbose)) {
		fprintf(stderr, "Unable to update BAM server %s (%s)\n",
		    bp_serverURL(buf),
		    (rc == 2) ? "can't get lock" : "unknown reason");
		return (-1);
	}
	return (0);
}

private  int
send_part1_msg(remote *r, char **envVar)
{
	char	buf[MAXPATH];
	FILE	*f;
	int	rc;

	bktmp(buf);
	f = fopen(buf, "w");
	assert(f);
	sendEnv(f, envVar, r, opts.sendenv_flags);
	fprintf(f, "rclone_part1");
	fprintf(f, " -z%d", r->gzip);
	if (opts.rev) fprintf(f, " '-r%s'", opts.rev);
	if (opts.verbose) fprintf(f, " -v");
	if (opts.transaction) {
		if (proj_isProduct(0)) {
			fprintf(f, " -P");
		} else {
			fprintf(f, " -T");
		}
	}
	if (opts.bam_url) fprintf(f, " '-B%s'", opts.bam_url);
	if (r->path) fprintf(f, " '%s'", r->path);
	fputs("\n", f);
	fclose(f);

	rc = send_file(r, buf, 0);
	unlink(buf);
	return (rc);
}

private int
rclone_part2(char **av, remote *r, char **envVar, char *bp_keys)
{
	FILE	*zin;
	FILE	*f;
	char	*line, *p;
	int	rc = 0, n, i;
	u32	bytes;
	char	buf[MAXPATH];

	if ((r->type == ADDR_HTTP) && bkd_connect(r, 0)) {
		rc = 1;
		goto done;
	}

	send_sfio_msg(r, envVar);
	unless (opts.quiet || opts.verbose) progress_nlneeded();

	if (r->type == ADDR_HTTP) skip_http_hdr(r);
	getline2(r, buf, sizeof(buf));
	if (streq(buf, "@SERVER INFO@")) {
		if (getServerInfo(r, 0)) {
			rc = 1;
			goto done;
		}
	}

	/*
	 * Get remote progress status
	 * Get any remote BAM keys we need to send.
	 */
	getline2(r, buf, sizeof(buf));
	unless (streq(buf, "@SFIO INFO@")) {
		fprintf(stderr, "rclone: protocol error, no SFIO INFO\n");
		rc = 1;
		goto done;
	}

	/* Spit out anything we see until a null. */
	while ((n = read_blk(r, buf, 1)) > 0) {
		if (buf[0] == BKD_NUL) break;
		unless (opts.quiet) writen(2, buf, n);
	}
	getline2(r, buf, sizeof(buf));

	/* optional bad exit status */
	if (buf[0] == BKD_RC) {
		if (rc = atoi(&buf[1])) {
			rc = 1;
			goto done;
		}
		getline2(r, buf, sizeof(buf));
	}

	/* optional BAM keys */
	if (streq(buf, "@BAM@")) {
		assert(rc == 0);
		unless (bp_keys) {
			fprintf(stderr, "rclone: unexpected BAM keys\n");
			rc = 1;
			goto done;
		}
		unless (r->rf) r->rf = fdopen(r->rfd, "r");
		zin = fopen_zip(r->rf, "rh");
		f = fopen(bp_keys, "w");
		while ((line = fgetline(zin)) &&
		    strneq(line, "@STDIN=", 7)) {
			bytes = atoi(line+7);
			unless (bytes) break;
			while (bytes > 0) {
				i = min(bytes, sizeof(buf));
				i = fread(buf, 1, i, zin);
				fwrite(buf, 1, i, f);
				bytes -= i;
			}
		}
		if (fclose(zin)) {
			rc = 1;
			goto done;
		}
		getline2(r, buf, sizeof(buf));
		unless (strneq(buf, "@DATASIZE=", 10)) {
			fprintf(stderr, "rclone: bad input '%s'\n", buf);
			rc = 1;
			goto done;
		}
		p = strchr(buf, '=');
		opts.bpsz = scansize(p+1);
		fclose(f);
		if (r->type == ADDR_HTTP) disconnect(r);
		return (0);
	} else if (bp_keys) {
		fprintf(stderr,
		    "rclone failed: @BAM@ section expected, got %s\n", buf);
		rc = 1;
		goto done;
	} else {
		unless (streq(buf, "@END@")) {
			fprintf(stderr, "rclone: unexpected data '%s'\n", buf);
			rc = 1;
			goto done;
		}
	}
done:	wait_eof(r, opts.debug); /* wait for remote to disconnect */
	disconnect(r);
	return (rc);
}

private  int
send_sfio_msg(remote *r, char **envVar)
{
	char	buf[MAXPATH];
	FILE	*f;
	int	i, rc;
	u32	m = 0, n, extra = 0;
	u32	flags = opts.sendenv_flags |
		    (clone_sfioCompat(0) ? 0 : SENDENV_SENDFMT);

	bktmp(buf);
	f = fopen(buf, "w");
	assert(f);
	sendEnv(f, envVar, r, flags);
	fprintf(f, "rclone_part2");
	if (proj_isProduct(0)) fprintf(f, " -P");
	if (opts.rev) fprintf(f, " '-r%s'", opts.rev); 
	unless (opts.quiet) fprintf(f, " -v");
	if (opts.bam_url) fprintf(f, " '-B%s'", opts.bam_url);
	if (opts.detach) fprintf(f, " -D");
	if (opts.jobs) fprintf(f, " -j%d", opts.jobs);
	EACH(opts.aliases) fprintf(f, " '-s%s'", opts.aliases[i]);
	if (r->path) fprintf(f, " '%s'", r->path);
	fputs("\n", f);
	fclose(f);

	/*
	 * Httpd wants the message length in the header
	 * We have to compute the file size before we sent
	 * 7 is the size of "@SFIO@"
	 * 6 is the size of "@END@" string
	 */
	if (r->type == ADDR_HTTP) {
		m = send_sfio(0, r->gzip);
		assert(m > 0);
		extra = m + 7 + 6;
	} else {
		/* if not http, just pass on that we are sending extra */
		extra = 1;
	}
	rc = send_file(r, buf, extra);
	unlink(buf);

	writen(r->wfd, "@SFIO@\n", 7);
	n = send_sfio(r, r->gzip);
	if ((r->type == ADDR_HTTP) && (m != n)) {
		fprintf(stderr,
		    "Error: sfio file changed size from %d to %d\n", m, n);
		disconnect(r);
		return (-1);
	}
	writen(r->wfd, "@END@\n", 6);
	send_file_extra_done(r);
	return (rc);
}

private int
send_BAM_msg(remote *r, char *bp_keys, char **envVar, u64 bpsz)
{
	FILE	*f, *fnull;
	int	i, rc;
	u32	extra = 1, m = 0, n;
	char	msgfile[MAXPATH];

	bktmp(msgfile);
	f = fopen(msgfile, "w");
	assert(f);
	sendEnv(f, envVar, r, 0);

	/*
	 * No need to do "cd" again if we have a non-http connection
	 * becuase we already did a "cd" in pull part 1
	 */
	if (r->type == ADDR_HTTP) add_cd_command(f, r);
	fprintf(f, "rclone_part3");
	fprintf(f, " -z%d", r->gzip);
	if (opts.rev) fprintf(f, " '-r%s'", opts.rev);
	if (opts.debug) fprintf(f, " -d");
	if (opts.detach) fprintf(f, " -D");
	unless (opts.quiet) fprintf(f, " -v");
	EACH(opts.aliases) fprintf(f, " '-s%s'", opts.aliases[i]);
	if (opts.bam_url) fprintf(f, " '-B%s'", opts.bam_url);
	fputs("\n", f);

	if (size(bp_keys) == 0) {
		fprintf(f, "@NOBAM@\n");
		extra = 0;
	} else {
		/*
		 * Httpd wants the message length in the header
		 * We have to compute the patch size before we sent
		 * 6 is the size of "@BAM@\n"
		 * 6 is the size of "@END@\n" string
		 */
		if (r->type == ADDR_HTTP) {
			fnull = fopen(DEVNULL_WR, "w");
			assert(fnull);
			m = send_BAM_sfio(fnull, bp_keys, bpsz, r->gzip,
			    opts.quiet, opts.verbose);
			fclose(fnull);
			assert(m > 0);
			extra = m + 6 + 6;
		} else {
			extra = 1;
		}
	}
	fclose(f);

	rc = send_file(r, msgfile, extra);

	if (extra > 0) {
		writen(r->wfd, "@BAM@\n", 6);
		f = fdopen(dup(r->wfd), "wb");
		n = send_BAM_sfio(f, bp_keys, bpsz, r->gzip,
		    opts.quiet, opts.verbose);
		if ((r->type == ADDR_HTTP) && (m != n)) {
			fprintf(stderr,
			    "Error: patch has changed size from %d to %d\n",
			    m, n);
			disconnect(r);
			return (-1);
		}
		fputs("@END@\n", f);
		fclose(f);
		send_file_extra_done(r);
	}
	if (unlink(msgfile)) perror(msgfile);
	if (rc == -1) {
		disconnect(r);
		return (-1);
	}

	if (opts.debug) {
		if (r->type == ADDR_HTTP) {
			getMsg("http_delay", 0, 0, stderr);
		}
	}
	return (0);
}

private int
rclone_part3(char **av, remote *r, char **envVar, char *bp_keys)
{
	int	n, rc = 0;
	char	buf[4096];

	if ((r->type == ADDR_HTTP) && bkd_connect(r, 0)) {
		rc = 1;
		goto done;
	}

	if (rc = send_BAM_msg(r, bp_keys, envVar, opts.bpsz)) goto done;
	if (r->type == ADDR_HTTP) skip_http_hdr(r);
	getline2(r, buf, sizeof(buf));
	if (remote_lock_fail(buf, opts.verbose)) {
		rc = 1;
		goto done;
	} else if (streq(buf, "@SERVER INFO@")) {
		if (getServerInfo(r, 0)) {
			rc = 1;
			goto done;
		}
	}
	/* Spit out anything we see until a null. */
	unless (opts.quiet) progress_nldone();
	while (read_blk(r, buf, 1) > 0) {
		if (buf[0] == BKD_NUL) {
			/* now back in protocol - look for end or RC */
			n = getline2(r, buf, sizeof(buf));
			if ((n > 0) && streq(buf, "@END@")) break;
			unless (opts.quiet) progress_nlneeded();
			fprintf(stderr,
			    "rclone: bkd failed to apply BAM data %s\n", buf);
			rc = 1;
			if ((n > 0) && (buf[0] == BKD_RC)) {
				rc = strtol(&buf[1], 0, 10);
			}
			goto done;
		}
		fputc(buf[0], stderr);
	}
	unless (opts.quiet) progress_nlneeded();
done:
	wait_eof(r, opts.debug); /* wait for remote to disconnect */
	disconnect(r);
	return (rc);
}

/*
 * XXX - for the size pass for http this could be oh so much faster.
 * We need a -s option to sfio that just adds up the sizes and spits it out.
 */
private u32
send_sfio(remote *r, int gzip)
{
	int	status;
	char	*b, *tmpf;
	FILE	*fh;
	char	*sfiocmd;
	FILE	*fout;
	char	title[MAXPATH];
	char	*marg = (bkd_hasFeature(FEAT_mSFIO) ? "-m2" : "");
	char	*compat = clone_sfioCompat(0) ? "-C" : "";
	char	buf[200] = { "" };

	tmpf = bktmp(0);
	status = systemf("bk _sfiles_clone %s > '%s'", marg, tmpf);
	unless (WIFEXITED(status) && WEXITSTATUS(status) == 0) return (0);

	if (opts.quiet) {
		sprintf(buf, "q");
	} else unless (opts.verbose) {
		sprintf(buf, "N%u", repo_nfiles(0,0));
	}
	if (opts.sfiotitle) {
		sprintf(title, " --title='%s'", opts.sfiotitle);
	} else {
		title[0] = 0;
	}

	if (r && r->path) {
		b = basenm(r->path);
		sfiocmd = aprintf(
		    "bk%s sfio %s -P'%s/' -o%s %s < '%s'",
		    title, compat, b, buf, marg, tmpf);
		fout = fdopen(dup(r->wfd), "wb");
	} else {
		fout = fopen(DEVNULL_WR, "w");
		sfiocmd = aprintf("bk%s sfio %s -o%s %s < '%s'",
		    title, compat, buf, marg, tmpf);
	}
	assert(fout);
	fh = popen(sfiocmd, "r");
	free(sfiocmd);
	opts.in = opts.out = 0;
	gzipAll2fh(fileno(fh), fout, gzip, &opts.in, &opts.out, 0);
	status = pclose(fh);
	unlink(tmpf);
	free(tmpf);
	fclose(fout);
	unless (WIFEXITED(status) && WEXITSTATUS(status) == 0) return (0);
	return (opts.out);

}
