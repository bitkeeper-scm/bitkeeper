/* Copyright (c) 2000 L.W.McVoy */
#include "bkd.h"
#include "resolve.h"
#include "nested.h"

private	int	abort_patch(int leavepatch);
private int	send_abort_msg(remote *r);
private int	remoteAbort(remote *r);
private int	abortComponents(int leavepatch);

/*
 * Abort a pull/resync by deleting the RESYNC dir and the patch file in
 * the PENDING dir.
 */
int
abort_main(int ac, char **av)
{
	int	c, force = 0, leavepatch = 0, quiet = 0;
	char	buf[MAXPATH];

	while ((c = getopt(ac, av, "fpq")) != -1) {
		switch (c) {
		    case 'f': force = 1; break; 	/* doc 2.0 */
		    case 'p': leavepatch = 1; break; 	/* undoc? 2.0 */
		    case 'q': quiet = 1; break;
		    default:
			system("bk help -s abort");
			return (1);
		}
	}
	if (av[optind]) {
		remote	*r;
		int	ret;

		r = remote_parse(av[optind], REMOTE_BKDURL);
		unless (r) {
			fprintf(stderr, "Cannot parse \"%s\"\n", av[optind]);
			return (1);
		}
		ret = remoteAbort(r);
		remote_free(r);
		return (ret);
	}
	proj_cd2root();
	/*
	 * product is write locked, which means RESYNC may be there.
	 * Need to do more of a test to see if it's just a lock.
	 * XXX: Is there a test to distinguish?  isRealResync(path) ?
	 */
	unless (exists(ROOT2RESYNC "/BitKeeper")) {
		unless (quiet) {
			fprintf(stderr, "abort: no RESYNC directory.\n");
		}
		return (0);
	}
	unless (force) {
		prompt("Abort update? (y/n)", buf);
		switch (buf[0]) {
		    case 'y':
		    case 'Y':
			break;
		    default:
			fprintf(stderr, "Not aborting.\n");
			return (0);
		}
	}

	return(abort_patch(leavepatch));
}

private	int
abort_patch(int leavepatch)
{
	char	buf[MAXPATH];
	char	pendingFile[MAXPATH];
	int	rc;
	FILE	*f;

	/*
	 * XXX: RESYNC will exist in product because lock.
	 * move isResync(0) to main where it is already checking
	 * where we are?
	 */
	if (proj_isResync(0)) chdir(RESYNC2ROOT);
	unless (exists(ROOT2RESYNC)) {
		fprintf(stderr, "abort: can't find RESYNC dir\n");
		fprintf(stderr, "abort: nothing removed.\n");
		return (1);
	}

	if (proj_isProduct(0)) {
		if (rc = abortComponents(leavepatch)) return (rc);
	}
	/*
	 * Get the patch file name from RESYNC before deleting RESYNC.
	 */
	sprintf(buf, "%s/%s", ROOT2RESYNC, "BitKeeper/tmp/patch");

	/* One of our regressions makes an empty BitKeeper/tmp/patch */
	pendingFile[0] = 0;
	if (f = fopen(buf, "r")) {
		if (fnext(pendingFile, f)) chop(pendingFile);
		fclose(f);
	} else {
		fprintf(stderr, "Warning: no BitKeeper/tmp/patch\n");
	}

	unless (rc = rmtree("RESYNC")) {
		if (!leavepatch && pendingFile[0]) unlink(pendingFile);
		rmdir(ROOT2PENDING);
		unlink(BACKUP_LIST);
		unlink(PASS4_TODO);
		unlink(APPLIED);
	}
	return (rc);
}

private int
send_abort_msg(remote *r)
{
	char	buf[MAXPATH];
	FILE	*f;
	int	rc;

	bktmp(buf, "abort");
	f = fopen(buf, "w");
	assert(f);
	sendEnv(f, 0, r, SENDENV_NOREPO);
	add_cd_command(f, r);
	fprintf(f, "abort\n");
	fclose(f);

	rc = send_file(r, buf, 0);
	unlink(buf);
	return (rc);
}

private int
remoteAbort(remote *r)
{
	char	buf[MAXPATH];
	int	rc = 0;

	if (bkd_connect(r)) return (1);
	if (send_abort_msg(r)) return (1);
	if (r->type == ADDR_HTTP) skip_http_hdr(r);

	getline2(r, buf, sizeof (buf));
	/*
	 * only 5.0 and newer sends server info and error
	 */
	if (streq("@SERVER INFO@", buf)) {
		if (getServerInfo(r)) {
			rc = 1;
			goto out;
		}
		getline2(r, buf, sizeof (buf));
	}
	if (strneq("ERROR-", buf, 6)) {
		fprintf(stderr, "%s\n", &buf[6]);
		rc = 1;
		goto out;
	}
	/*
	 * pre and post 5.0 send this stuff ...
	 * pre responding a post ERROR-xx message just see that
	 * it is not ABORT info and returns without printing msg.
	 * see t.compat-3.2.8 in the new bkd section with locked
	 * repo for example.
	 */
	unless (streq("@ABORT INFO@", buf)) { /* protocol error */
		rc = 1;
		goto out;
	}

	while (getline2(r, buf, sizeof (buf)) > 0) {
		if (buf[0] == BKD_NUL) break;
		printf("%s\n", buf);
	}
	getline2(r, buf, sizeof (buf));
	if (buf[0] == BKD_RC) {
		rc = atoi(&buf[1]);
		getline2(r, buf, sizeof (buf));
	}
	unless (streq("@END@", buf)) { /* protocol error */
		rc = 1;
		goto out;
	}
out:	wait_eof(r, 0);
	disconnect(r);
	return (rc);
}

private int
abortComponents(int leavepatch)
{
	nested	*n = 0;
	sccs	*s = 0;
	comp	*c;
	char	**csets_in;
	int	i;
	int	errors = 0;

	/* sanity check - already checked in abort_main() */
	assert(exists(ROOT2RESYNC "/BitKeeper"));

	START_TRANSACTION();
	chdir(ROOT2RESYNC);
	unless (csets_in = file2Lines(0, CSETS_IN)) {
		errors++;
		error("failed to load csets-in\n");
		goto out;
	}
	unless (s =
	    sccs_init(CHANGESET, INIT_NOCKSUM|INIT_NOSTAT|INIT_MUSTEXIST)) {
		errors++;
		error("failed to init ChangeSet file\n");
		goto out;
	}
	chdir(RESYNC2ROOT);
	unless (n = nested_init(s, 0, csets_in, NESTED_PULL)) {
		errors++;
		error("nested_init failed\n");
		goto out;
	}

	EACH_STRUCT(n->comps, c, i) {
		TRACE("check(%s) p%d i%d\n",
		    c->path, c->present, c->included);
		if (c->product || !c->present || !c->included) continue;

		proj_cd2product();
		if (chdir(c->path)) {
			error("abort: failed to find component %s\n", c->path);
			errors++;
			continue;
		}
		if (c->new) {
			unless (streq(proj_rootkey(0), c->rootkey)) {
				TRACE("wrong rootkey in %s\n", c->path);
				/*
				 * This is not the component you're
				 * looking for.  You can go about your
				 * business.
				 */
				continue;
			}
			/*
			 * XXX: Should we check that c->deltakey matches
			 * the tip in case they added local csets?
			 */
			proj_cd2product();
			TRACE("rmcomp(%s)\n", c->path);
			if (nested_rmcomp(n, c)) {
				fprintf(stderr, "failed rmcomp in %s\n", c->path);
				error("failed to remove %s\n", c->path);
				errors++;
			}
			continue;
		}
		/* not new */
		if (isdir(ROOT2RESYNC)) {
			if (systemf("bk abort -f%s", leavepatch ? "p" : "")) {
				error("abort: component %s failed\n", c->path);
				errors++;
				goto out;
			}
		} else {
			/*
			 * XXX what makes sure the user hasn't added new
			 * csets after the pull that failed?
			 */
			if (systemf("bk undo -sfa'%s'", c->lowerkey)) {
				error("abort: failed to revert %s\n", c->path);
				errors++;
			}
		}
	}

out:	if (n) nested_free(n);
	if (s) sccs_free(s);
	if (csets_in) freeLines(csets_in, free);
	STOP_TRANSACTION();
	proj_cd2product();
	/*
	 * The failed operation (pull) could have brough in extra
	 * components, removed some, etc. We restore sanity by
	 * trusting the HERE file.
	 */
	if (system("bk populate -q")) {
		error("abort: bk populate failed\n");
		errors++;
	}
	return (errors);
}
