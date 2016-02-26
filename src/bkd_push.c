/*
 * Copyright 1999-2014 BitMover, Inc
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
#include "bam.h"
#include "nested.h"

/*
 * Important, if we have error, we must close fd1 or exit
 */
int
cmd_push_part1(int ac, char **av)
{
	char	*p, **aliases;
	int	i, c, status;
	size_t	n;
	int	debug = 0, gzip = 0, product = 0;
	FILE	*fout;
	sccs	*cset;

	while ((c = getopt(ac, av, "dnPz|", 0)) != -1) {
		switch (c) {
		    case 'z': break;
			gzip = optarg ? atoi(optarg) : Z_BEST_SPEED;
			if (gzip < 0 || gzip > 9) gzip = Z_BEST_SPEED;
			break;
		    case 'd': debug = 1; break;
		    case 'P': product = 1; break;
		    case 'n': putenv("BK_STATUS=DRYRUN"); break;
		    default: break;	/* ignore and pray */
		}
	}
	if (debug) fprintf(stderr, "cmd_push_part1: sending server info\n");
	setmode(0, _O_BINARY); /* needed for gzip mode */

	if (getenv("BKD_LEVEL") && (atoi(getenv("BKD_LEVEL")) > getlevel())) {
		/* they got sent the level so they are exiting already */
		return (1);
	}
	p = getenv("BK_REMOTE_PROTOCOL");
	unless (p && streq(p, BKD_VERSION)) {
		out("ERROR-protocol version mismatch, want: ");
		out(BKD_VERSION); 
		out(", got ");
		out(p ? p : "");
		out("\n");
		return (1);
	}
	if ((bp_hasBAM() || ((p = getenv("BK_BAM")) && streq(p, "YES"))) &&
	    !bk_hasFeature(FEAT_BAMv2)) {
		out("ERROR-please upgrade your BK to a BAMv2 aware version "
		    "(4.1.1 or later)\n");
		return (1);
	}
	unless(isdir("BitKeeper")) { /* not a packageg root */
		if (debug) {
			fprintf(stderr, "cmd_push_part1: not package root\n");
		}
		out("ERROR-Not at package root\n");
		out("@END@\n");
		return (1);
	}

	if (trigger(av[0], "pre")) return (1);
	if (debug) fprintf(stderr, "cmd_push_part1: calling listkey\n");

	unless (cset = sccs_csetInit(INIT_MUSTEXIST)) {
		/* Historical response from bk _listkey is 3 */
		out("ERROR-listkey failed (status=3)\n");
		return (1);
	}
	Opts.use_stdio = 1;	/* bkd.c: getav() - set up to drain buf */
	fout = fmem();
	status = listkey(cset, 0, stdin, fout);
	sccs_free(cset);
	if (status > 1) {
		error("listkey failed (status=%d)\n", status);
		fclose(fout);
		return (1);
	}

	if (product && proj_isProduct(0) && (aliases = nested_here(0))) {
		out("@HERE@\n");
		EACH(aliases) {
			out(aliases[i]);
			out("\n");
		}
		freeLines(aliases, free);
		aliases = 0;
	}

	out("@OK@\n");
	p = fmem_peek(fout, &n);
	unless (writen(1, p, n) == n) {
		fclose(fout);
		return (1);
	}
	fclose(fout);
	if (debug) fprintf(stderr, "cmd_push_part1: done\n");
	return (0);
}

int
cmd_push_part2(int ac, char **av)
{
	int	fd2, pfd, c, rc = 0;
	int	gzip = 0;
	int	status, debug = 0, nothing = 0, conflict = 0, product = 0;
	int	quiet = 0, verbose = 0;
	pid_t	pid;
	char	*p;
	char	bkd_nul = BKD_NUL;
	u64	sfio;
	FILE	*f;
	char	*takepatch[] = { "bk", "takepatch", "-c", 0, 0};
	int	tp_opt = 3;	/* index of first 0 */
	char	buf[4096];

	while ((c = getopt(ac, av, "dGnPqvz|", 0)) != -1) {
		switch (c) {
		    case 'z':
			gzip = optarg ? atoi(optarg) : Z_BEST_SPEED;
			if (gzip < 0 || gzip > 9) gzip = Z_BEST_SPEED;
			break;
		    case 'd': debug = 1; break;
		    case 'G': putenv("BK_NOTTY=1"); break;
		    case 'n': putenv("BK_STATUS=DRYRUN"); break;
		    case 'P': product = 1; break;
		    case 'q': quiet = 1; takepatch[tp_opt] = "-q"; break;
		    case 'v': verbose = 1; takepatch[tp_opt] = "-vv"; break;
		    default: break;	/* ignore and pray */
		}
	}
	unless (quiet || verbose) takepatch[tp_opt] = "--progress";

	if (debug) fprintf(stderr, "cmd_push_part2: checking package root\n");
	unless (isdir("BitKeeper")) {
		out("ERROR-Not at package root\n");
		rc = 1;
		goto done;
	}

	buf[0] = 0;
	getline(0, buf, sizeof(buf));
	if (streq(buf, "@ABORT@")) {
		/*
		 * Client pre-trigger canceled the push operation 
		 * This is a null event for us, do not goto done. 
		 * Just return without firing the post trigger
		 */
		if (proj_isProduct(0)) goto abort;
		return (0);
	}
	putenv("BK_STATUS=OK");
	if (streq(buf, "@NOTHING TO SEND@")) {
		nothing = 1;
		putenv("BK_STATUS=NOTHING");
	} else if (streq(buf, "@CONFLICT@")) {
		conflict = 1;
		putenv("BK_STATUS=CONFLICTS");
	}
	if ((nothing || conflict) && product) {
		char	*resync, *nlid;

		/*
		 * Kludge: abort here.
		 * XXX: should we abort on conflict too?
		 */
abort:		resync = aprintf("%s/%s", proj_root(0), ROOT2RESYNC);
		nlid = getenv("_BK_NESTED_LOCK");
		assert(nlid);
		nested_abort(0, nlid);
		if (rmtree(resync)) {
			out("ERROR-could not unlock remote");
		}
		free(resync);
		goto done;
	}
	if (nothing || conflict) goto done;
	if (!streq(buf, "@PATCH@")) {
		fprintf(stderr, "expect @PATCH@, got <%s>\n", buf);
		rc = 1;
		goto done;
	}

	/*
	 * Do takepatch
	 */
	if (debug) fprintf(stderr, "cmd_push_part2: calling takepatch\n");
	printf("@TAKEPATCH INFO@\n");
	fflush(stdout);
	/* Arrange to have stderr go to stdout */
	fd2 = dup(2); dup2(1, 2);
	pid = spawnvpio(&pfd, 0, 0, takepatch);
	f = fdopen(pfd, "wb");
	dup2(fd2, 2); close(fd2);
	/* stdin needs to be unbuffered when calling takepatch... */
	assert(!Opts.use_stdio);
	gunzipAll2fh(0, f, 0, 0);
	fclose(f);
	getline(0, buf, sizeof(buf));
	if (!streq("@END@", buf)) {
		fprintf(stderr, "cmd_push: warning: lost end marker\n");
	}

	if ((rc = waitpid(pid, &status, 0)) != pid) {
		perror("takepatch subprocess");
		rc = 254;
	}
	if (WIFEXITED(status)) {
		rc =  WEXITSTATUS(status);
	} else {
		rc = 253;
	}
	write(1, &bkd_nul, 1);
	if (rc) {
		printf("%c%d\n", BKD_RC, rc);
		fflush(stdout);
	}
	fputs("@END@\n", stdout);
	fflush(stdout);
	if (!WIFEXITED(status)) {
		putenv("BK_STATUS=SIGNALED");
		rc = 1;
		goto done;
	}
	if (WEXITSTATUS(status)) {
		putenv("BK_STATUS=CONFLICTS");
		rc = 1;
		goto done;
	}
	c = bp_hasBAM();
	if (c || ((p = getenv("BK_BAM")) && streq(p, "YES"))) {
		// send bp keys
		unless (c) touch(BAM_MARKER, 0664);
		putenv("BKD_DAEMON="); /* allow new bkd connections */
		printf("@BAM@\n");
		chdir(ROOT2RESYNC);
		rc = bp_sendkeys(stdout, "- < " CSETS_IN, &sfio, gzip);
		printf("@DATASIZE=%s@\n", psize(sfio));
		fflush(stdout);
		chdir(RESYNC2ROOT);
	} else if (product) {
		printf("@DELAYING RESOLVE@\n");
	} else {
		rc = bkd_doResolve(av[0], quiet, verbose);
	}
done:	return (rc);
}

int
bkd_doResolve(char *me, int quiet, int verbose)
{
	int	fd2, c, rc = 0;
	int	debug = 0;
	char	bkd_nul = BKD_NUL;
	char	*resolve[] = { "resolve", "-S", "-T", "-c", 0, 0, 0};
	int	resolve_opt = 4; /* index of 0 after "-c" above */

	/*
	 * Fire up the pre-trigger
	 */
	trigger_setQuiet(quiet);
	putenv("BK_CSETLIST=" CSETS_IN);
	if (c = trigger("remote resolve",  "pre")) {
		if (c == 2) {
			system("bk -?BK_NO_REPO_LOCK=YES abort -fp");
		} else {
			system("bk -?BK_NO_REPO_LOCK=YES abort -f");
		}
		return (1);
	}

	/*
	 * Do resolve
	 */
	if (debug) fprintf(stderr, "%s: calling resolve\n", me);
	printf("@RESOLVE INFO@\n");
	if (verbose) {
		printf("Running resolve to apply new work...\n");
		resolve[resolve_opt++] = "-v";
	} else {
		/*
		 * NOTE: -q is selected by !verbose,
		 * and -q only selects triggers
		 */
		resolve[resolve_opt++] = "-q";
	}

	resolve[resolve_opt] = 0;
	fflush(stdout);
	/* Arrange to have stderr go to stdout */
	fd2 = dup(2); dup2(1, 2);
	putenv("FROM_PULLPUSH=YES");
	getoptReset();
	rc = resolve_main(resolve_opt, resolve);
	dup2(fd2, 2); close(fd2);
	write(1, &bkd_nul, 1);
	if (rc) {
		printf("%c%d\n", BKD_RC, rc);
		fflush(stdout);
	}
	fputs("@END@\n", stdout);
	fflush(stdout);
	if (rc) rc = 1;
	return (rc);
}

/*
 * complete a push of BAM data
 */
int
cmd_push_part3(int ac, char **av)
{
	int	fd2, pfd, c, rc = 0;
	int	status, debug = 0;
	int	inbytes, outbytes;
	int	gzip = 0, product = 0, verbose = 0;
	int	quiet = 0;
	pid_t	pid;
	FILE	*f;
	char	*sfio[] = {"bk", "sfio", "-iqB", "-", 0};
	char	buf[4096];

	while ((c = getopt(ac, av, "dGPqvz|", 0)) != -1) {
		switch (c) {
		    case 'z':
			gzip = optarg ? atoi(optarg) : Z_BEST_SPEED;
			if (gzip < 0 || gzip > 9) gzip = Z_BEST_SPEED;
			break;
		    case 'd': debug = 1; break;
		    case 'G': putenv("BK_NOTTY=1"); break;
		    case 'P': product = 1; break;
		    case 'q': quiet = 1; break;
		    case 'v': verbose++; break;
		    default: break;	/* ignore and pray */
		}
	}

	if (debug) fprintf(stderr, "cmd_push_part3: checking package root\n");
	unless (isdir("BitKeeper")) {
		out("ERROR-Not at package root\n");
		rc = 1;
		goto done;
	}

	buf[0] = 0;
	getline(0, buf, sizeof(buf));
	if (streq(buf, "@BAM@")) {
		/*
		 * Do sfio
		 */
		if (debug) fprintf(stderr, "cmd_push_part3: calling sfio\n");
		fflush(stdout);
		/* Arrange to have stderr go to stdout */
		fd2 = dup(2); dup2(1, 2);
		pid = spawnvpio(&pfd, 0, 0, sfio);
		dup2(fd2, 2); close(fd2);
		inbytes = outbytes = 0;
		f = fdopen(pfd, "wb");
		/* stdin needs to be unbuffered when calling sfio */
		assert(!Opts.use_stdio);
		gunzipAll2fh(0, f, &inbytes, &outbytes);
		fclose(f);
		getline(0, buf, sizeof(buf));
		unless (streq("@END@", buf)) {
			fprintf(stderr,
			    "cmd_push: warning: lost end marker\n");
		}

		if ((rc = waitpid(pid, &status, 0)) != pid) {
			perror("sfio subprocess");
			rc = 254;
		}
		if (WIFEXITED(status)) {
			rc =  WEXITSTATUS(status);
		} else {
			rc = 253;
		}
		fputc(BKD_NUL, stdout);
		if (rc) {
			printf("%c%d\n", BKD_RC, rc);
			fflush(stdout);
			/*
			 * Clear out the push because we got corrupted data
			 * which failed the push.  It's not as bad as it might
			 * seem that we do this because we enter BAM data 
			 * directly into the BAM pool so we won't resend when
			 * they sort out the bad file.
			 */
			system("bk -?BK_NO_REPO_LOCK=YES abort -f");
			goto done;
		}
	} else if (streq(buf, "@NOBAM@")) {
		fputc(BKD_NUL, stdout);
	} else {
		fprintf(stderr, "expect @BAM@, got <%s>\n", buf);
		rc = 1;
		goto done;
	}
	fputs("@END@\n", stdout);
	fflush(stdout);

	if (product) {
		printf("@DELAYING RESOLVE@\n");
	} else if (isdir(ROOT2RESYNC)) {
		rc = bkd_doResolve(av[0], quiet, verbose);
	}
done:	return (rc);
}
