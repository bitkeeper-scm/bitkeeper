/*
 * Copyright 2000-2005,2008-2013,2016 BitMover, Inc
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
#include "resolve.h"
#include "nested.h"
#include "progress.h"

typedef struct {
	u32	leavepatch:1;
	u32	quiet:1;
	u32	nested:1;
} options;

private	int	abort_patch(options *opts);
private int	send_abort_msg(remote *r);
private int	remoteAbort(remote *r);
private int	abortComponents(options *opts, int *which, int *num);

/*
 * Abort a pull/resync by deleting the RESYNC dir and the patch file in
 * the PENDING dir.
 */
int
abort_main(int ac, char **av)
{
	int	c, force = 0;
	int	standalone = 0;
	int	rc = 1;
	options	*opts;
	char	buf[MAXPATH];
	longopt	lopts[] = {
		{ "standalone", 'S' },
		{ 0, 0 }
	};

	opts = new(options);
	while ((c = getopt(ac, av, "fpqS", lopts)) != -1) {
		switch (c) {
		    case 'f': force = 1; break; 	/* doc 2.0 */
		    case 'p': opts->leavepatch = 1; break;
		    case 'q': opts->quiet = 1; break;
		    case 'S': standalone = 1; break;
		    default: bk_badArg(c, av);
		}
	}
	if (av[optind]) {
		remote	*r;

		r = remote_parse(av[optind], REMOTE_BKDURL);
		unless (r) {
			fprintf(stderr, "Cannot parse \"%s\"\n", av[optind]);
			goto out;
		}
		if (standalone) usage();
		rc = remoteAbort(r);
		remote_free(r);
		goto out;
	}
	opts->nested = bk_nested2root(standalone);
	c = CMD_WRLOCK|CMD_IGNORE_RESYNC;
	if (opts->nested) c |= CMD_NESTED_WRLOCK;
	cmdlog_lock(c);

	/*
	 * product is write locked, which means RESYNC may be there.
	 * Need to do more of a test to see if it's just a lock.
	 * XXX: Is there a test to distinguish?  isRealResync(path) ?
	 */
	unless (exists(ROOT2RESYNC "/BitKeeper")) {
		unless (opts->quiet) {
			fprintf(stderr, "abort: no RESYNC directory.\n");
		}
		rc = 0;
		goto out;
	}
	unless (force) {
		prompt("Abort update? (y/n)", buf);
		switch (buf[0]) {
		    case 'y':
		    case 'Y':
			break;
		    default:
			fprintf(stderr, "Not aborting.\n");
			goto out;
		}
	}

	rc = abort_patch(opts);
out:	free(opts);
	return (rc);
}

private	int
abort_patch(options *opts)
{
	char	buf[MAXPATH];
	char	pendingFile[MAXPATH];
	int	rc;
	int	which = 0, num = 0;
	ticker	*tick = 0;
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

	if (opts->nested && (rc = abortComponents(opts, &which, &num))) {
		return (rc);
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

	if (proj_isEnsemble(0) && !opts->quiet) {
		tick = progress_start(PROGRESS_BAR, 1000);
	}
	unless (rc = rmtree("RESYNC")) {
		if (!opts->leavepatch && pendingFile[0]) unlink(pendingFile);
		rmdir(ROOT2PENDING);
		unlink(BACKUP_LIST);
		unlink(PASS4_TODO);
	}
	if (tick) {
		if (proj_isProduct(0)) title = aprintf("%u/%u .", which, num);
		progress_end(PROGRESS_BAR, rc ? "FAILED":"OK", PROGRESS_MSG);
		if (proj_isProduct(0)) FREE(title);
	}
	return (rc);
}

private int
send_abort_msg(remote *r)
{
	char	buf[MAXPATH];
	FILE	*f;
	int	rc;

	bktmp(buf);
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

	if (bkd_connect(r, 0)) return (1);
	if (send_abort_msg(r)) return (1);
	if (r->type == ADDR_HTTP) skip_http_hdr(r);

	getline2(r, buf, sizeof (buf));
	/*
	 * only 5.0 and newer sends server info and error
	 */
	if (streq("@SERVER INFO@", buf)) {
		if (getServerInfo(r, 0)) {
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
abortComponents(options *opts, int *which, int *num)
{
	nested	*n = 0;
	sccs	*s = 0;
	comp	*c;
	char	**csets_in;
	char	**list = 0;
	int	i;
	int	errors = 0;

	*num = 0;
	*which = 1;
	/* sanity check - already checked in abort_main() */
	assert(exists(ROOT2RESYNC "/BitKeeper"));

	START_TRANSACTION();
	if (chdir(ROOT2RESYNC)) perror(ROOT2RESYNC);
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
	if (chdir(RESYNC2ROOT)) perror(RESYNC2ROOT);
	unless (n = nested_init(s, 0, csets_in, NESTED_PULL|NESTED_FIXIDCACHE)) {
		errors++;
		error("nested_init failed\n");
		goto out;
	}

	EACH_STRUCT(n->comps, c, i) {
		if (!c->included || !C_PRESENT(c)) continue;
		proj_cd2product();
		if (chdir(c->path)) continue;
		/* not part of this pull */
		if (exists("RESYNC/BitKeeper/log/port")) continue;
		(*num)++;
	}

	EACH_STRUCT(n->comps, c, i) {
		T_NESTED("check(%s) p%d i%d\n",
		    c->path, C_PRESENT(c), c->included);
		if (c->product || !c->included || !C_PRESENT(c)) continue;

		proj_cd2product();
		if (chdir(c->path)) {
			error("abort: failed to find component %s\n", c->path);
			errors++;
			continue;
		}
		/* not part of this pull */
		if (exists("RESYNC/BitKeeper/log/port")) continue;
		if (c->new) {
			ticker	*tick = 0;
			int	e;

			unless (streq(proj_rootkey(0), c->rootkey)) {
				T_NESTED("wrong rootkey in %s\n", c->path);
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
			unless (opts->quiet) {
				tick = progress_start(PROGRESS_BAR, 1000);
			}
			T_NESTED("rmcomp(%s)\n", c->path);
			if (e = nested_rmcomp(n, c)) {
				fprintf(stderr, "failed rmcomp in %s\n", c->path);
				error("failed to remove %s\n", c->path);
				errors++;
			}
			if (tick) {
				title = aprintf("%u/%u %s",
				    (*which)++, *num, c->path);
				progress_end(PROGRESS_BAR,
				    e ? "FAILED" : "OK", PROGRESS_MSG);
				FREE(title);
			}
			continue;
		}
		/* not new */
		list = addLine(list, aprintf("%s/" CHANGESET, c->path));
		if (isdir(ROOT2RESYNC)) {
			if (systemf("bk -?BK_NO_REPO_LOCK=YES "
			    "--title='%u/%u %s' abort -Sf%s%s",
				(*which)++, *num, c->path,
			    opts->leavepatch ? "p" : "", opts->quiet ? "q" : "")) {
				error("abort: component %s failed\n", c->path);
				errors++;
				progress_end(PROGRESS_BAR,
				    "FAILED", PROGRESS_MSG);
				goto out;
			}
		} else {
			/*
			 * XXX what makes sure the user hasn't added new
			 * csets after the pull that failed?
			 */
			if (systemf("bk "
			    "--title='%u/%u %s' undo -%sSsfa'%s'",
				(*which)++, *num, c->path,
			    opts->quiet ? "q" : "", c->lowerkey)) {
				error("abort: failed to revert %s\n", c->path);
				errors++;
			}
		}
	}
	/* Possibly fix (unpoly-ize) cset marks in product */
	proj_cd2product();
	run_check(opts->quiet, 0, list, "-u", 0);

out:	if (n) nested_free(n);
	if (s) sccs_free(s);
	if (list) freeLines(list, free);
	if (csets_in) freeLines(csets_in, free);
	proj_cd2product();
	/*
	 * The failed operation (pull) could have brough in extra
	 * components, removed some, etc. We restore sanity by
	 * trusting the HERE file.
	 */
	if (system("bk -?BK_NO_REPO_LOCK=YES here set --unsafe -q here")) {
		error("abort: bk here failed\n");
		errors++;
	}
	STOP_TRANSACTION();
	return (errors);
}
