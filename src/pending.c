#include "bkd.h"

#define V_QUIET		-2
#define V_COUNT		-1

extern char *pager;

typedef struct {
	u32	cset:1;
	u32	patch:1;
	u32	delta:1;
	u32	parkfile:1;
	u32	modified:1;
	u32	extra:1;
	u32	skip_topheader:1;
	u32	skip_allheader:1;
	u32	unique:1;	/* skip dup cset */
	u32	skip_trailer:1;
	u32	forward:1;	/* cset/delta ordering */
	int	verbose;	/* -1 => count only, -2 => quiet */
	char	*dspec;
	MDBM	*seen;		/* key list */
} options;

private int doit(options *opts, char *url);
private int doit_local(options *opts);
private int doit_remote(options *opts, char *url);
private int do_patch(options *opts);
private int do_delta(options *opts);
private int do_parkfile(options *opts);
private int do_modified(options *opts);
private int do_extra(options *opts);


int
pending_main(int ac, char **av)
{
	int	c;
	int	pending = 0, gotone = 0;
	pid_t	pid;
	options	opts;

	bzero(&opts, sizeof(opts));
	while ((c = getopt(ac, av, "qSTHhD:cdfpkmxau")) != -1) { 
		gotone = 1;
		switch (c) {
		    case 'q':	opts.verbose = V_QUIET ; break;
		    case 'S':	opts.verbose = V_COUNT; break;
		    case 'T':	opts.skip_trailer = 1; break;   /* undoc */
		    case 'H':	opts.skip_topheader = 1; break; /* undoc */
		    case 'h':	opts.skip_allheader = 1; break;
		    case 'D':	opts.dspec = optarg; break;
		    case 'c':	opts.cset = 1; break;
		    case 'f':	opts.forward = 1; break;
		    case 'p':	opts.patch = 1; break;
		    case 'd':	opts.delta = 1; break;
		    case 'k':	opts.parkfile = 1; break;
		    case 'm':	opts.modified = 1; break;
		    case 'x':	opts.extra = 1; break;
		    case 'u':	opts.unique = 1; break;
		    case 'a': 	opts.cset = opts.patch = opts.delta = \
				opts.parkfile =  opts.modified = \
				opts.extra = 1;
				break;
		    default:
			system("bk help -s pending");
			return (1);
		}

	}

	/*
	 * Backward compat:
	 * a) If no pending type is specified default to -d
	 * b) If no option is specified, default to -d -h -T
	 */
	if (!opts.cset && !opts.patch && !opts.delta &&
	    !opts.parkfile && !opts.modified && !opts.extra) {
		opts.delta = 1;

		unless (gotone) {
			/* for backward compat */
			opts.skip_allheader = 1;
			opts.skip_trailer = 1;
		}
	}

	if (proj_cd2root()) {
		fprintf(stderr, "pending: cannot find project root\n");
		exit(1);
	}

	if (opts.cset && opts.unique) opts.seen = mdbm_mem();
	pid = mkpager();
	if (av[optind]) {
		if (streq("-", av[optind])) {
			char buf[MAXPATH];

			while (fnext(buf, stdin)) {
				chomp(buf);
				if (!opts.skip_allheader &&
				    (opts.verbose > V_QUIET)) {
					printf("%s\n", buf);
					fflush(stdout); /* for error path */
				}
				pending |= doit(&opts, buf);
			}
		} else {
			while (av[optind]) {
				if (!opts.skip_allheader &&
				    (opts.verbose > V_QUIET)) {
					printf("%s\n", av[optind]);
					fflush(stdout); /* for error path */
				}
				pending |= doit(&opts, av[optind]);
				optind++;
			}
		}
	} else {
		pending |= doit_local(&opts);
	}
	if (opts.seen) mdbm_close(opts.seen);

	if (!pending && !opts.skip_trailer && (opts.verbose >= 0)) {
		printf("*** No pending item found.\n");
	}
	if (pid > 0)  { /* force pager exit */
		fclose(stdout);
		waitpid(pid, 0, 0);
	}
	if (pending < 0) return (2); /* 2 means error */
	return (pending ? 0 : 1);
}

private int
doit(options *opts, char *url)
{
	int	pending = 0;

	if (url == NULL) {
		pending |= doit_local(opts);
	} else {
		//XXX TODO need to check for url == local repo
		pending |= doit_remote(opts, url);
	}
	return (pending);
}

private void
print_count(char *type, int count)
{
	printf("%-8s %5d\n", type, count);
}

private void
print_counthdr(void)
{
	printf("%-8s %5s\n", "TYPE", "COUNT");
	printf("==============\n");
}

private void
print_tophdr(options *opts)
{
	if (opts->verbose == V_COUNT) print_counthdr();
}

private int
doit_local(options *opts)
{
	int	pending = 0;

	unless (opts->skip_topheader || opts->skip_allheader) {
		print_tophdr(opts);
	}

	/*
	 * Note: No need to check for opts->cset, cset count
	 * is zero by definition becuase we will be diffing the cset key
	 * against ourself.
	 */ 
	if (opts->patch) pending |= do_patch(opts);
	if (opts->delta) pending |= do_delta(opts);
	if (opts->parkfile) pending |= do_parkfile(opts);
	if (opts->modified) pending |= do_modified(opts);
	if (opts->extra) pending |= do_extra(opts);
	return (pending);
}

private int
send_part1_msg(options *opts, remote *r)
{
	char	*cmd, buf[MAXPATH];
	int	rc;
	FILE 	*f;

	bktmp(buf, "pending");
	f = fopen(buf, "w");
	assert(f);
	sendEnv(f, NULL, r, 0);
	if (r->path) add_cd_command(f, r);
	fprintf(f, "pending_part1");
	fputs("\n", f);
	fclose(f);

	cmd = aprintf("bk _probekey  >> %s", buf);
	system(cmd);
	free(cmd);

	rc = send_file(r, buf, 0, 0);
	unlink(buf);
	return (rc);
}

private int
send_part2_msg(options *opts, remote *r, char *key_list, int rcsets)
{
	int	rc;
	char	msgfile[MAXPATH], buf[MAXLINE];
	FILE	*f;

	bktmp(msgfile, "pending_part2");
	f = fopen(msgfile, "wb");
	assert(f);
	sendEnv(f, NULL, r, 0);

	if (r->path && (r->type == ADDR_HTTP)) add_cd_command(f, r);
	fprintf(f, "pending_part2");
	if (opts->verbose == V_QUIET) fputs(" -q", f);
	if (opts->verbose == V_COUNT) fputs(" -S", f);
	if (opts->skip_allheader) fputs(" -h", f);
	if (opts->cset) fputs(" -c", f);
	if (opts->patch) fputs(" -p", f);
	if (opts->delta) fputs(" -d", f);
	if (opts->parkfile) fputs(" -k", f);
	if (opts->modified) fputs(" -m", f);
	if (opts->extra) fputs(" -x", f);
	if (opts->forward) fputs(" -f", f);
	if (opts->dspec) fprintf(f, " \"-D%s\"", opts->dspec);
	fputs("\n", f);
	fclose(f);

	if (opts->verbose >= 0 ) {
		if (rcsets > 0) {
			rc = send_file(r, msgfile, size(key_list) + 17, 0);
			f = fopen(key_list, "rt");
			assert(f);
			write_blk(r, "@KEY LIST@\n", 11);
			while (fnext(buf, f)) {
				write_blk(r, buf, strlen(buf));
				chomp(buf);
				/* mark the seen key, so we can skip them */
				if (opts->seen != NULL) {
					mdbm_store_str(opts->seen,
							buf, "", MDBM_INSERT);
				}
			}
			write_blk(r, "@END@\n", 6);
			fclose(f);
		} else {
			rc = send_file(r, msgfile, 14, 0);
			write_blk(r, "@NO KEY LIST@\n", 16);
		}
	} else {
		/* count or empty/notempty  */
		rc = send_file(r, msgfile, 0, 0);
	}
	unlink(msgfile);
	return (rc);
}

private int
pending_part1(options *opts, remote *r, char *key_list)
{
	char	buf[MAXPATH], s_cset[] = CHANGESET;
	int	flags, fd, rc, rcsets = 0, rtags = 0;
	sccs	*s;

	/*
	 * Even when opts->cset is off, we still
	 * do the keysync. I've considered streamlining this case,
	 * but keysync is quiet efficient, I am not sure it really
	 * buy us that much. Another reason to skip optimizing this
	 * case is that almost 90% percent of the time, user want to
	 * know about new cset in the remote repository. Why optimize
	 * a rare case ?
	 */
	if (bkd_connect(r, 0, 1)) return (-1);
	send_part1_msg(opts, r);
	if (r->rfd < 0) return (-1);

	if (r->type == ADDR_HTTP) skip_http_hdr(r);
	if (getline2(r, buf, sizeof(buf)) <= 0) return (-1);
	if ((rc = remote_lock_fail(buf, 1))) {
		return (rc); /* -2 means locked */
	} else if (streq(buf, "@SERVER INFO@")) {
		getServerInfoBlock(r);
		getline2(r, buf, sizeof(buf));
	} else {
		drainErrorMsg(r, buf, sizeof(buf));
		return (-1);
	}
	if (get_ok(r, buf, 1)) return (-1);

	/*
	 * What we want is: "remote => bk _prunekey => keylist"
	 */
	bktmp(key_list, "pending_keylist");
	fd = open(key_list, O_CREAT|O_WRONLY, 0644);
	s = sccs_init(s_cset, 0, 0);
	flags = PK_REVPREFIX|PK_RKEY;
	rc = prunekey(s, r, opts->seen, fd, flags, 0, NULL, &rcsets, &rtags);
	if (rc < 0) {
		switch (rc) {
		    case -2:
			printf(
"You are trying to sync to an unrelated package. The root keys for the\n\
ChangeSet file do not match.  Please check the pathnames and try again.\n");
			close(fd);
			sccs_free(s);
			return (-1); /* needed to force bkd unlock */
		    case -3:
			printf("You are syncing to an empty directory\n");
			sccs_free(s);
			return (-1); /* empty dir */
			break;
		}
		close(fd);
		disconnect(r, 2);
		sccs_free(s);
		return (-1);
	}
	close(fd);
	sccs_free(s);
	if (r->type == ADDR_HTTP) disconnect(r, 2);
	return (rcsets);
}

private int
pending_part2(options *opts, remote *r, char *key_list, int rcsets)
{
	int 	pending = (rcsets > 0);
	int	rc_lock;
	char	buf[MAXLINE];

	if ((r->type == ADDR_HTTP) && bkd_connect(r, 0, 0)) {
		return (-1);
	}

	send_part2_msg(opts, r, key_list, rcsets);
	if (r->type == ADDR_HTTP) skip_http_hdr(r);

	getline2(r, buf, sizeof(buf));
	if (rc_lock = remote_lock_fail(buf, 0)) {
		fprintf(stderr, "locking error %d\n", rc_lock);
		goto done;
	} else if (streq(buf, "@SERVER INFO@")) {
		getServerInfoBlock(r);
	}

	getline2(r, buf, sizeof(buf));
	unless (streq("@PENDING INFO@", buf)) {
		fprintf(stderr, "protocol  error %d\n", rc_lock);
		goto done;
	}
	while (getline2(r, buf, sizeof(buf)) > 0) {
		if (buf[0] == BKD_RC) {
			pending |= atoi(&buf[1]);
			getline2(r, buf, sizeof(buf));
		}
		if (streq("@END@", buf)) break;
		if (write(1, &buf[1], strlen(buf) - 1) < 0) {
			break;
		}
		write(1, "\n", 1);
	}

done:	unlink(key_list);
	disconnect(r, 1);
	wait_eof(r, 0);
	return (pending);
}

private int
doit_remote(options *opts, char *url)
{
	char    key_list[MAXPATH] = "";
	int     rcsets = 0, pending = -1;
	remote  *r;

	loadNetLib();
	has_proj("pending");
	r = remote_parse(url, 1);
	unless (r) {
		fprintf(stderr, "invalid url: %s\n", url);
		return (-1);
	}

	rcsets = pending_part1(opts, r, key_list);
	if (rcsets >= 0) {
		unless(opts->skip_allheader) print_tophdr(opts);
		if (opts->verbose == -1) print_count("cset", rcsets);
		fflush(stdout); /* important */
		pending = pending_part2(opts, r, key_list, rcsets);
	}
	remote_free(r);
	if (key_list[0]) unlink(key_list);
	return (pending);
}

/*
 * XXX TODO what we really want is to extract the new cset in the patch
 */
private int
do_patch(options *opts)
{
	int	first = 1, i = 0;
	char	**list = NULL;
#define PENDING "PENDING"

	if (!isdir(PENDING) || emptyDir(PENDING)) goto done;
	list = getdir(PENDING);
	EACH(list) {
		if (opts->verbose >= 0) {
			if (first) {
				first = 0;
				unless (opts->skip_allheader) {
					printf("  %s\n", "patch");
				}
			}
			printf("    %s\n", list[i]);
		}
	}
	assert(i > 0);
	i--;
	if (i && (opts->verbose >= 0)) fputs("\n", stdout);
done:	if (opts->verbose == V_COUNT) print_count("patch", i);
	if (list) freeLines(list, free);
	return (i > 0); /* return ture if non-empty */
}

private int
do_delta(options *opts)
{
	int	pending = 0;
	char	*tmp, *cmd = 0;
	char	*dspec =
		":DPN:@:I:, :Dy:-:Dm:-:Dd: :T::TZ:, :P:$if(:HT:){@:HT:}\\n"
		"$each(:C:){  (:C:)\n}$each(:SYMBOL:){  TAG: (:SYMBOL:)\\n"
		"}\n";
	char	buf[MAXLINE];
	FILE	*f, *p = 0;

	tmp = bktmp(0, "pending_sfiles");
	sysio(0, tmp, 0, "bk", "sfiles", "-pCA", SYS);

	if (opts->verbose == V_QUIET) {
		pending = (size(tmp) > 0) ? 1 : 0;
	} else if (opts->verbose == V_COUNT) {
		int	count = 0;

		f = fopen(tmp, "rt");
		assert(f);
		while (fnext(buf, f)) count++;
		fclose(f);
		print_count("delta", count);
		pending =  (count > 0);
	} else {
		pending = (size(tmp) > 0) ? 1 : 0;
		unless (pending) goto done;
		unless (opts->skip_allheader) printf("  %s\n", "delta");
		if (opts->dspec) dspec = opts->dspec;
		cmd = aprintf("bk prs -Yh %s -d'%s' - < %s",
			opts->forward ? "-f" : "", dspec, tmp);
		p = popen(cmd, "r");
		while (fnext(buf, p)) {
			fputs("    ", stdout); /* indent */
			fputs(buf, stdout);
		}
		pclose(p);
	}

done:	unlink(tmp);
	free(tmp);
	if (cmd) free(cmd);
	return (pending);
}

private int
do_cmd(char *type, char *cmd, options *opts)
{
	int	first = 1, count = 0;
	FILE	*f;
	char	buf[MAXPATH];

	f = popen(cmd, "r");
	assert(f);
	while (fnext(buf, f)) {
		chomp(buf);
		if (opts->verbose >= 0) {
			if (first) {
				first = 0;
				unless (opts->skip_allheader) {
					printf("  %s\n", type);
				}
			}
			printf("    %s\n", buf);
		}
		count++;
	}
	pclose(f);
	if (count && (opts->verbose >= 0)) fputs("\n", stdout);
	if (opts->verbose == V_COUNT) print_count(type, count);
	return (count > 0);
}

private int
do_parkfile(options *opts)
{
	return (do_cmd("parkfile", "bk park -l", opts));
}

private int
do_modified(options *opts)
{
	return (do_cmd("modified", "bk sfiles -gc", opts));
}

private int
do_extra(options *opts)
{
	return (do_cmd("extra", "bk sfiles -x", opts));
}
