/*
 * Copyright (c) 2000, Andrew Chang & Larry McVoy
 */
#include "bkd.h"
#include "logging.h"
#include "ensemble.h"

private struct {
	int	list;			/* -l: long listing */
	u32	automerge:1;		/* -i: turn off automerge */
	u32	dont:1;			/* -n: do not actually do it */
	u32	quiet:1;		/* -q: shut up */
	u32	nospin:1;		/* -Q: no spin for the GUI */
	u32	fullPatch:1;		/* -F force fullpatch */
	u32	noresolve:1;		/* -R: don't run resolve at all */
	u32	textOnly:1;		/* -T: pass -T to resolve */
	u32	autoOnly:1;		/* -s: pass -s to resolve */
	u32	debug:1;		/* -d: debug */
	u32	update_only:1;		/* -u: pull iff no local csets */
	u32	gotsome:1;		/* we got some csets */
	u32	collapsedups:1;		/* -D: pass to takepatch (collapse dups) */
	u32	product:1;		/* is this a product pull? */
	u32	pass1:1;		/* set if we are driver pull */
	u32	port:1;			/* is port command? */
	u32	transaction:1;		/* is $_BK_TRANSACTION set? */
	u32	local:1;		/* set if we find local work */
	int	delay;			/* -w<delay> */
	char	*rev;			/* -r<rev> - no revs after this */
	u32	in, out;		/* stats */
	char	**av_pull;		/* saved av for ensemble pull */
	char	**av_clone;		/* saved av for ensemble clone */
} opts;

private int	pull(char **av, remote *r, char **envVar);
private int	pull_ensemble(repos *rps, remote *r);
private void	resolve_comments(remote *r);
private	int	resolve(void);
private	int	takepatch(remote *r);

private void
usage(char *prog)
{
	sys("bk", "help", "-s", prog, SYS);
}

int
pull_main(int ac, char **av)
{
	int	c, i, j = 1;
	int	try = -1; /* retry forever */
	int	rc = 0;
	int	gzip = 6;
	remote	*r;
	char	*p, *prog;
	char	**envVar = 0, **urls = 0;

	bzero(&opts, sizeof(opts));
	prog = basenm(av[0]);
	if (streq(prog, "port")) opts.port = 1;
	opts.automerge = 1;
	while ((c = getopt(ac, av, "c:DdE:GFilnqr;RstTuw|z|")) != -1) {
		unless (c == 'r') {
			if (optarg) {
				opts.av_pull = addLine(opts.av_pull,
				    aprintf("-%c%s", c, optarg));
			} else {
				opts.av_pull = addLine(opts.av_pull,
				    aprintf("-%c", c));
			}
		}
		if ((c == 'd') || (c == 'E') || (c == 'q') ||
		    (c == 'w') || (c == 'z')) {
			if (optarg) {
				opts.av_clone = addLine(opts.av_clone,
				    aprintf("-%c%s", c, optarg));
			} else {
				opts.av_clone = addLine(opts.av_clone,
				    aprintf("-%c", c));
			}
		}
		switch (c) {
		    case 'D': opts.collapsedups = 1;
		    case 'G': opts.nospin = 1; break;
		    case 'i': opts.automerge = 0; break;	/* doc 2.0 */
		    case 'l': opts.list++; break;		/* doc 2.0 */
		    case 'n': opts.dont = 1; break;		/* doc 2.0 */
		    case 'q': opts.quiet = 1; break;		/* doc 2.0 */
		    case 'r': opts.rev = optarg; break;
		    case 'R': opts.noresolve = 1; break;	/* doc 2.0 */
		    case 's': opts.autoOnly = 1; break;
		    case 'T': /* -T is preferred, remove -t in 5.0 */
		    case 't': opts.textOnly = 1; break;		/* doc 2.0 */
		    case 'd': opts.debug = 1; break;		/* undoc 2.0 */
		    case 'F': opts.fullPatch = 1; break;	/* undoc 2.0 */
		    case 'E': 					/* doc 2.0 */
			unless (strneq("BKU_", optarg, 4)) {
				fprintf(stderr,
				    "pull: vars must start with BKU_\n");
				return (1);
			}
			envVar = addLine(envVar, strdup(optarg)); break;
		    case 'c': try = atoi(optarg); break;	/* doc 2.0 */
		    case 'u': opts.update_only = 1; break;
		    case 'w': opts.delay = atoi(optarg); break;	/* undoc 2.0 */
		    case 'z':					/* doc 2.0 */
			if (optarg) gzip = atoi(optarg);
			if ((gzip < 0) || (gzip > 9)) gzip = 6;
			break;
		    default:
			usage(prog);
			return(1);
		}
		optarg = 0;
	}
	if (opts.quiet) putenv("BK_QUIET_TRIGGERS=YES");
	if (opts.autoOnly && !opts.automerge) {
		fprintf(stderr, "pull: -s and -i cannot be used together\n");
		usage(prog);
		return (1);
	}
	if (getenv("_BK_TRANSACTION")) opts.transaction = 1;
	if (proj_isComponent(0)) {
		unless (opts.transaction || opts.port) {
			fprintf(stderr,
			    "pull: component pulls are not allowed\n");
			return (1);
		}
	} else if (opts.port) {
		fprintf(stderr,
			"port: can only port to an ensemble component.\n");
		return (1);
	} else if (proj_isProduct(0)) {
		opts.product = 1;
		unless (opts.transaction) opts.pass1 = 1;
	}

	/*
	 * Get pull parent(s)
	 * Must do this before we chdir()
	 */
	if (av[optind]) {
		while (av[optind]) {
			urls = addLine(urls, parent_normalize(av[optind++]));
		}
	}

	if (proj_cd2root()) {
		fprintf(stderr, "pull: cannot find package root.\n");
		exit(1);
	}
	unless (eula_accept(EULA_PROMPT, 0)) {
		fprintf(stderr, "pull: failed to accept license, aborting.\n");
		exit(1);
	}

	if (sane(0, 0) != 0) return (1);

	unless (opts.port || urls) {
		urls = parent_pullp();
		unless (urls) {
			freeLines(envVar, free);
			getMsg("missing_parent", 0, 0, stderr);
			return (1);
		}
	}

	unless (urls) {
err:		freeLines(envVar, free);
		usage(prog);
		return (1);
	}

	if (opts.port) {
		p = aprintf("%s/BitKeeper/log/PORTAL",
		    proj_root(proj_product(0)));
		unless (exists(p)) {
			fprintf(stderr, "port: destination is not a portal.\n");
			free(p);
			return (1);
		}
		free(p);
	}

	/*
	 * pull from each parent
	 */
	EACH (urls) {
		r = remote_parse(urls[i], REMOTE_BKDURL);
		unless (r) goto err;
		if (opts.debug) r->trace = 1;
		r->gzip_in = gzip;
		unless (opts.quiet || opts.pass1) {
			if (i > 1)  printf("\n");
			fromTo(prog, r, 0);
		}
		/*
		 * retry if parent is locked
		 */
		for (;;) {
			rc = pull(av, r, envVar);
			if (rc != -2) break;
			if (try == 0) break;
			if (try != -1) --try;
			unless(opts.quiet) {
				fprintf(stderr,
				    "pull: remote locked, trying again...\n");
			}
			disconnect(r, 2);
			assert(r->pid == 0);
			sleep(min((j++ * 2), 10));
		}
		remote_free(r);
		if ((rc == 2) && opts.local && !opts.quiet) {
			sys("bk", "changes", "-L", urls[i], SYS);
		}
		if (rc == -2) rc = 1; /* if retry failed, set exit code to 1 */
		if (rc) break;
	}
	freeLines(urls, free);
	freeLines(opts.av_pull, free);
	freeLines(opts.av_clone, free);
	return (rc);
}

void
fromTo(char *op, remote *f, remote *t)
{
	char	*from, *to;
	remote	*tmp;
	int	width;

	assert(f || t);
	if (f) {
		from = remote_unparse(f);
	} else {
		tmp = remote_parse(proj_root(0), REMOTE_BKDURL);
		from = remote_unparse(tmp);
		remote_free(tmp);
	}
	if (t) {
		to = remote_unparse(t);
	} else {
		tmp = remote_parse(proj_root(0), REMOTE_BKDURL);
		to = remote_unparse(tmp);
		remote_free(tmp);
	}
	width = strlen(op) - 3;
	if (width < 0) width = 0;
	putchar(toupper(op[0])); /* force op to upper-case */
	printf("%s %s\n%*s -> %s\n", op+1, from, width, "", to);
	fflush(stdout);
	free(from);
	free(to);
}

private int
send_part1_msg(remote *r, char probe_list[], char **envVar)
{
	char	buf[MAXPATH];
	FILE    *f;
	int	rc;

	bktmp(buf, "pull1");
	f = fopen(buf, "w");
	assert(f);
	sendEnv(f, envVar, r, 0);
	add_cd_command(f, r);
	fprintf(f, "pull_part1");
	if (opts.rev) fprintf(f, " -r%s", opts.rev);
	if (opts.port) fprintf(f, " -P");
	if (opts.transaction) fprintf(f, " -T");
	fputs("\n", f);
	fclose(f);
	rc = send_file(r, buf, 0);
	unlink(buf);
	return (rc);
}

private int
pull_part1(char **av, remote *r, char probe_list[], char **envVar)
{
	char	*p;
	int	rc;
	FILE	*f;
	char	buf[MAXPATH];

	if (bkd_connect(r)) return (-1);
	if (send_part1_msg(r, probe_list, envVar)) return (-1);

	if (r->type == ADDR_HTTP) skip_http_hdr(r);
	if (getline2(r, buf, sizeof (buf)) <= 0) return (-1);
	if ((rc = remote_lock_fail(buf, !opts.quiet))) {
		return (rc); /* -2 means lock busy */
	} else if (streq(buf, "@SERVER INFO@")) {
		if (getServerInfoBlock(r)) return (-1);
		getline2(r, buf, sizeof(buf));
	} else {
		drainErrorMsg(r, buf, sizeof(buf));
		disconnect(r, 2);
		exit(1);
	}
	if (getenv("BKD_LEVEL") &&
	    (atoi(getenv("BKD_LEVEL")) > getlevel())) {
	    	fprintf(stderr, "pull: cannot pull to lower level "
		    "repository (remote level == %s)\n", getenv("BKD_LEVEL"));
		disconnect(r, 2);
		return (1);
	}
	if (opts.rev && !bkd_hasFeature("pull-r")) {
		notice("no-pull-dash-r", 0, "-e");
		disconnect(r, 2);
		return (1);
	}
	if ((bp_hasBAM() ||
	    ((p = getenv("BKD_BAM")) && streq(p, "YES"))) &&
	    !bkd_hasFeature("BAMv2")) {
		fprintf(stderr,
		    "pull: please upgrade the remote bkd to a "
		    "BAMv2 aware version (4.1.1 or later).\n");
		disconnect(r, 2);
		return (1);
	}
	if (opts.port && (p = getenv("BKD_PRODUCT_KEY")) &&
	    streq(p, proj_rootkey(proj_product(0)))) {
		fprintf(stderr,
		    "port: may not port components with identical products\n");
		disconnect(r, 2);
		return (1);
	}
	if (opts.port && !bkd_hasFeature("SAMv1")) {
		fprintf(stderr,
		    "port: remote bkd too old to support 'bk port'\n");
		disconnect(r, 2);
		return (1);
	}
	if (get_ok(r, buf, 1)) {
		disconnect(r, 2);
		return (1);
	}
	if (opts.dont) putenv("BK_STATUS=DRYRUN");
	if (trigger(av[0], "pre")) {
		disconnect(r, 2);
		return (1);
	}
	bktmp(probe_list, "pullprobe");
	f = fopen(probe_list, "w");
	assert(f);
	while (getline2(r, buf, sizeof(buf)) > 0) {
		fprintf(f, "%s\n", buf);
		if (streq("@END PROBE@", buf)) break;
	}
	fclose(f);
	if (r->type == ADDR_HTTP) disconnect(r, 2);
	return (0);
}

private int
send_keys_msg(remote *r, char probe_list[], char **envVar)
{
	char	msg_file[MAXPATH], buf[MAXPATH * 2];
	FILE	*f;
	char	**l;
	int	i, status, rc;

	bktmp(msg_file, "pullmsg");
	f = fopen(msg_file, "w");
	assert(f);
	sendEnv(f, envVar, r, 0);

	/*
	 * No need to do "cd" again if we have a non-http connection
	 * becuase we already did a "cd" in pull part 1
	 */
	if (r->type == ADDR_HTTP) add_cd_command(f, r);
	fprintf(f, "pull_part2");
	fprintf(f, " -z%d", r->gzip);
	if (opts.dont) fprintf(f, " -n");
	for (rc = opts.list; rc--; ) fprintf(f, " -l");
	if (opts.quiet || opts.pass1) fprintf(f, " -q");
	if (opts.rev) fprintf(f, " -r%s", opts.rev);
	if (opts.delay) fprintf(f, " -w%d", opts.delay);
	if (opts.debug) fprintf(f, " -d");
	if (opts.port) fprintf(f, " '-P%s'", proj_rootkey(0));
	if (opts.transaction) fprintf(f, " -T");
	if (opts.update_only) fprintf(f, " -u");
	if (proj_isProduct(0) &&
	    (l = file2Lines(0, "BitKeeper/log/COMPONENTS"))) {
		fprintf(f, " -s\n");
		fprintf(f, "@COMPONENTS@\n");
		EACH(l) fprintf(f, "%s\n", l[i]);
		fprintf(f, "@END@\n");
		freeLines(l, free);
	} else {
		fputs("\n", f);
	}
	fclose(f);

	sprintf(buf, "bk _listkey %s %s -q < '%s' >> '%s'",
	    opts.fullPatch ? "-F" : "",
	    opts.port ? "-A" : "",
	    probe_list, msg_file);
	status = system(buf);
	rc = WEXITSTATUS(status);
	if (opts.debug) fprintf(stderr, "listkey returned %d\n", rc);
	switch (rc) {
	    case 0:
		break;
	    case 1:
		fprintf(stderr,
		    "You are trying to pull from an unrelated package.\n"
		    "Please check the pathnames and try again.\n");
		unlink(msg_file);
		return (-1);
	    default:
		unlink(msg_file);
		return (-1);
	}

	rc = send_file(r, msg_file, 0);
	unlink(msg_file);
	return (rc);
}

private int
pull_part2(char **av, remote *r, char probe_list[], char **envVar)
{
	int	rc = 0, n, i;
	FILE	*info;
	char	buf[MAXPATH * 2];

	if ((r->type == ADDR_HTTP) && bkd_connect(r)) {
		return (-1);
	}
	if (send_keys_msg(r, probe_list, envVar)) {
		putenv("BK_STATUS=PROTOCOL ERROR");
		rc = 1;
		goto done;
	}

	if (r->type == ADDR_HTTP) skip_http_hdr(r);
	getline2(r, buf, sizeof (buf));
	if (remote_lock_fail(buf, !opts.quiet)) {
		return (-1);
	} else if (streq(buf, "@SERVER INFO@")) {
		if (getServerInfoBlock(r)) goto err;
		getline2(r, buf, sizeof(buf));
	}
	if (get_ok(r, buf, 1)) {
 err:		putenv("BK_STATUS=PROTOCOL ERROR");
		rc = 1;
		goto done;
	}

	/*
	 * Read the verbose status if we asked for it
	 */
	getline2(r, buf, sizeof(buf));
	i = n = 0;
	info = fmem_open();
	if (streq(buf, "@REV LIST@")) {
		while (getline2(r, buf, sizeof(buf)) > 0) {
			if (streq(buf, "@END@")) break;
			unless (i++) {
				if (opts.dont) {
					fprintf(info, "%s\n",
					    "---------------------- "
					    "Would receive the following csets "
					    "----------------------");
				} else {
					fprintf(info, "%s\n",
					    "---------------------- "
					    "Receiving the following csets "
					    "-----------------------");
					opts.gotsome = 1;
				}
			}
			fprintf(info, "%s", &buf[1]);
			if (!isdigit(buf[1]) || (strlen(&buf[1]) > MAXREV)) {
				fprintf(info, "\n");
				continue;
			}
			n += strlen(&buf[1]) + 1;
			if (n >= (80 - MAXREV)) {
				fprintf(info, "\n");
				n = 0;
			} else {
				fprintf(info, " ");
			}
		}
		if (n) fprintf(info, "\n");
		/* load up the next line */
		getline2(r, buf, sizeof(buf));
		if (i) {
			fprintf(info, "%s\n",
			    "--------------------------------------"
			    "-------------------------------------");
		}
	}

	/*
	 * See if we can't update because of local csets/tags.
	 */
	if (streq(buf, "@NO UPDATE BECAUSE OF LOCAL CSETS OR TAGS@")) {
		fclose(info);
		putenv("BK_STATUS=LOCAL_WORK");
		rc = 2;
		opts.local = 1;
		goto done;
	}

	/*
	 * Dump the status now that we know we are going to get it.
	 */
	fputs(fmem_getbuf(info, 0), stderr);
	fclose(info);

	/*
	 * check remote trigger
	 */
	if (streq(buf, "@TRIGGER INFO@")) {
		if (getTriggerInfoBlock(r, !opts.quiet)) {
			putenv("BK_STATUS=REMOTE TRIGGER FAILURE");
			rc = 2;
			goto done;
		}
		getline2(r, buf, sizeof (buf));
	}

	if (opts.dont) {
		rc = 0;
		if (!opts.quiet && streq(buf, "@NOTHING TO SEND@")) {
			fprintf(stderr, "Nothing to pull.\n");
		}
		putenv("BK_STATUS=DRYRUN");
		goto done;
	}

	if (streq(buf, "@PATCH@")) {
		if (i = takepatch(r)) {
			fprintf(stderr,
			    "Pull failed: takepatch exited %d.\n", i);
			putenv("BK_STATUS=TAKEPATCH FAILED");
			rc = 1;
			goto done;
		}
		if (opts.port) touch("RESYNC/SCCS/d.ChangeSet", 0666);
		putenv("BK_STATUS=OK");
		rc = 0;
	}  else if (strneq(buf, "@UNABLE TO UPDATE BAM SERVER", 28)) {
		unless (opts.quiet) {
			chop(buf);
			fprintf(stderr,
			    "Unable to update remote BAM server %s.\n",
			    &buf[29]);	/* it's "SERVER $URL */
		}
		putenv("BK_STATUS=FAILED");
		rc = 1;
	}  else if (streq(buf, "@NOTHING TO SEND@")) {
		unless (opts.quiet) {
			fprintf(stderr, "Nothing to pull.\n");
		}
		putenv("BK_STATUS=NOTHING");
		rc = 0;
	} else if (streq(buf, "@ENSEMBLE@")) {
		/* we're pulling from a product */
		repos	*repos;

		unless (r->rf) r->rf = fdopen(r->rfd, "r");
		unless (repos = ensemble_fromStream(0, r->rf)) {
			fprintf(stderr, "Could not read ensemble list\n");
			putenv("BK_STATUS=FAILED");
			rc = 1;
			goto done;
		}
		repository_unlock(0);
		if (rc = pull_ensemble(repos, r)) {
			putenv("BK_STATUS=FAILED");
		} else {
			putenv("BK_STATUS=OK");
		}
		ensemble_free(repos);
	} else {
		fprintf(stderr, "protocol error: <%s>\n", buf);
		while (getline2(r, buf, sizeof(buf)) > 0) {
			fprintf(stderr, "protocol error: <%s>\n", buf);
		}
		rc = 1;
		putenv("BK_STATUS=PROTOCOL ERROR");
	}

done:	unlink(probe_list);
	if (r->type == ADDR_HTTP) disconnect(r, 2);
	return (rc);
}

private int
pull_ensemble(repos *rps, remote *r)
{
	char	*url;
	char	**vp;
	char	*name, *path;
	char	**comps = 0;
	MDBM	*idDB;
	FILE	*idfile;
	int	i, rc = 0, missing = 0, product;
	char	idname[MAXPATH];

	url = remote_unparse(r);
	putenv("_BK_TRANSACTION=1");
	idDB = loadDB(IDCACHE, 0, DB_IDCACHE);
	EACH_REPO (rps) {
		if (rps->present) continue;
		fprintf(stderr,
		    "pull: %s is missing in %s\n", rps->path, url);
		missing++;
	}
	if (missing) {
		fprintf(stderr,
		    "pull: update aborted due to %d missing components.\n",
		    missing);
		rc = 1;
		goto out;
	}
	EACH_REPO (rps) {
		product = 0;
		proj_cd2product();
		if (streq(rps->path, ".")) {
			name = "Product";
			product = 1;
		} else {
			name = rps->path;
			product = 0;
		}
		unless (opts.quiet) {
			printf("#### %s ####\n", name);
			fflush(stdout);
		}
		vp = addLine(0, strdup("bk"));
		if (rps->new) {
			vp = addLine(vp, strdup("clone"));
			EACH(opts.av_clone) {
				vp = addLine(vp, strdup(opts.av_clone[i]));
			}
		} else {
			unless (product) {
				/* we can't assume the component is in the same
				 * path here than in the remote location */
				unless (path = key2path(rps->rootkey, idDB)) {
					fprintf(stderr, "Could not find "
					    "component '%s'\n", rps->path);
					goto err;
				}
				csetChomp(path);
				if (chdir(path)) {
err:					fprintf(stderr, "Could not chdir to "
					    " component '%s'\n", path);
					free(path);
					fprintf(stderr, 
					    "pull: update aborted.\n");
					rc = 1;
					break;
				}
				comps = addLine(comps, strdup(rps->path));
				comps = addLine(comps, path);
			}
			vp = addLine(vp, strdup("pull"));
			if (product && !opts.noresolve) {
				vp = addLine(vp, strdup("-R"));
			}
			EACH(opts.av_pull) {
				vp = addLine(vp, strdup(opts.av_pull[i]));
			}
		}
		vp = addLine(vp, aprintf("-r%s", rps->deltakey));
		vp = addLine(vp, aprintf("%s/%s", url, rps->path));
		if (rps->new) vp = addLine(vp, strdup(rps->path));
		vp = addLine(vp, 0);
		if (spawnvp(_P_WAIT, "bk", &vp[1])) {
			fprintf(stderr, "Pulling %s failed\n", name);
			rc = 1;
		}
		freeLines(vp, free);
		if (rc) break;
	}
	mdbm_close(idDB);
	if (rc) goto out;
	/* Now we need to make it such that the resolver in the
	 * product will work */
	unless (opts.noresolve) {
		unless (opts.quiet) {
			printf("#### Resolve in product ####\n");
			fflush(stdout);
		}
		proj_cd2product();
		if (chdir(ROOT2RESYNC)) {
			fprintf(stderr,
			    "Could not find product's RESYNC directory\n");
			rc = 1;
			goto out;
		}
		mkdirp("BitKeeper/log");
		touch("BitKeeper/log/PRODUCT", 0644);
		/*
		 * Copy all the component's ChangeSet files to the
		 * product's RESYNC directory
		 */
		bktmp(idname, "idfile");
		unless (idfile = fopen(idname, "w")) {
			perror("idname");
			rc = 1;
			goto out;
		}
		EACH (comps) {
			char	*from, *to;
			char	*dfile_to, *dfile_from;
			char	*t;

			to = aprintf("%s/%s", comps[i], CHANGESET);
			t = strrchr(to, '/');
			*(++t) = 'd';
			dfile_to = strdup(to);
			*t = 's';
			i++;
			from = aprintf("%s/%s/%s", RESYNC2ROOT,
			    comps[i], CHANGESET);
			t = strrchr(from, '/');
			*(++t) = 'd';
			dfile_from = strdup(from);
			*t = 's';

			if (fileCopy(from, to)) {
				fprintf(stderr, "Could not copy '%s' to "
				    "'%s'\n", from, to);
				rc = 1;
			}
			if (exists(dfile_from)) {
				touch(dfile_to, 0644);
				unlink(dfile_from);
			}
			fputs(to, idfile);
			fputc('\n', idfile);
			free(dfile_from);
			free(dfile_to);
			free(from);
			free(to);
			if (rc) goto out;
		}
		fclose(idfile);
		idcache_update(idname);
		unlink(idname);
		chdir(RESYNC2ROOT);
	}
out:	if (comps) freeLines(comps, free);
	free(url);
	putenv("_BK_TRANSACTION=");
	return (rc);
}

private int
pull(char **av, remote *r, char **envVar)
{
	int	rc, i;
	char	*p;
	int	got_patch;
	char	key_list[MAXPATH];

	assert(r);
	if (rc = pull_part1(av, r, key_list, envVar)) return (rc);
	rc = pull_part2(av, r, key_list, envVar);
	got_patch = ((p = getenv("BK_STATUS")) && streq(p, "OK"));
	if (!rc && got_patch &&
	    (bp_hasBAM() || ((p = getenv("BKD_BAM")) && streq(p, "YES")))) {
		chdir(ROOT2RESYNC);
		rc = bkd_BAM_part3(r, envVar, opts.quiet,
		    "- < " CSETS_IN);
		chdir(RESYNC2ROOT);
		if (rc) {
			fprintf(stderr, "BAM fetch failed, aborting pull.\n");
			system("bk abort -f");
			exit(1);
		}
	}
	if (got_patch) {
		/*
		 * We are about to run resolve, fire pre trigger
		 */
		putenv("BK_CSETLIST=BitKeeper/etc/csets-in");
		if ((i = trigger("resolve", "pre"))) {
			putenv("BK_STATUS=LOCAL TRIGGER FAILURE");
			rc = 2;
			if (i == 2) {
				system("bk abort -fp");
			} else {
				system("bk abort -f");
			}
			goto done;
		}
		resolve_comments(r);
		unless (opts.noresolve) {
			putenv("FROM_PULLPUSH=YES");
			if (resolve()) {
				rc = 1;
				putenv("BK_STATUS=CONFLICTS");
				goto done;
			}
		}
	}
done:	putenv("BK_RESYNC=FALSE");
	unless (opts.noresolve) trigger(av[0], "post");
	/*
	 * XXX This is a workaround for a csh fd leak:
	 * Force a client side EOF before we wait for server side EOF.
	 * Needed only if remote is running csh; csh has a fd leak
	 * which causes it fail to send us EOF when we close stdout
	 * and stderr.  Csh only sends us EOF and the bkd exit, yuck !!
	 */
	disconnect(r, 1);

	/*
	 * Wait for remote to disconnect
	 * This is important when trigger/error condition 
	 * short circuit the code path
	 */
	wait_eof(r, opts.debug);
	disconnect(r, 2);
	return (rc);
}

private	int
takepatch(remote *r)
{
	int	n, status, pfd;
	pid_t	pid;
	FILE	*f;
	char	*cmds[10];

	cmds[n = 0] = "bk";
	cmds[++n] = "takepatch";
	if (opts.quiet) {
		;
	} else if (opts.nospin) {
		cmds[++n] = "-mvv";
	} else {
		cmds[++n] = "-mvvv";
	}
	if (opts.collapsedups) cmds[++n] = "-D";
	cmds[++n] = 0;
	pid = spawnvpio(&pfd, 0, 0, cmds);
	f = fdopen(pfd, "wb");
	gunzipAll2fh(r->rfd, f, &(opts.in), &(opts.out));
	fclose(f);

	n = waitpid(pid, &status, 0);
	if (n != pid) {
		perror("WAITPID");
		fprintf(stderr, "Waiting for %d\n", pid);
	}

	unless (opts.quiet) {
		if (r->gzip) {
			fprintf(stderr, "%s uncompressed to %s, ",
			    psize(opts.in), psize(opts.out));
			fprintf(stderr,
			    "%.2fX expansion\n",
			    (double)opts.out/opts.in);
		} else {
			fprintf(stderr, "%s transferred\n", psize(opts.out));
		}
	}
	if (WIFEXITED(status)) return (WEXITSTATUS(status));
	if (WIFSIGNALED(status)) return (-WTERMSIG(status));
	return (100);
}

private void
resolve_comments(remote *r)
{
	FILE	*f;
	char	*u, *c;
	char	*h = sccs_gethost();
	char	buf[MAXPATH];

	getcwd(buf, sizeof(buf));
	if (r->host) {
		u = remote_unparse(r);
	} else {
		u = aprintf("%s:%s", h, r->path);
	}
	c = aprintf("Merge %s\ninto  %s:%s\n", u, h, buf);
	free(u);
	sprintf(buf, "%s/%s", ROOT2RESYNC, CHANGESET);
	assert(exists(buf));
	u = strrchr(buf, '/');
	u[1] = 'c';
	if (f = fopen(buf, "w")) {
		fputs(c, f);
		fclose(f);
	} else {
		perror(buf);
	}
	free(c);
}

private	int
resolve(void)
{
	int	i, status;
	char	*cmd[20];

	cmd[i = 0] = "bk";
	cmd[++i] = "resolve";
	if (opts.quiet) cmd[++i] = "-q";
	if (opts.textOnly) cmd[++i] = "-t";
	if (opts.autoOnly) cmd[++i] = "-s";
	if (opts.automerge) cmd[++i] = "-a";
	if (opts.debug) cmd[++i] = "-d";
	cmd[++i] = 0;
	unless (opts.quiet) {
		fprintf(stderr, "Running resolve to apply new work ...\n");
	}
	/*
	 * Since resolve ignores signals we need to ignore signal
	 * while it is running so that no one hits ^C and leaves it
	 * orphaned.
	 */
	sig_ignore();
	status = spawnvp(_P_WAIT, "bk", cmd);
	sig_default();
	unless (WIFEXITED(status)) return (100);
	return (WEXITSTATUS(status));
	return (0);
}
