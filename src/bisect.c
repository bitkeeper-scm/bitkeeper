/*
 * Copyright 2014-2016 BitMover, Inc
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
#include "range.h"

#define	BISECT_NO_MORE		 0
#define	BISECT_TRY_NEXT		 1
#define	BISECT_NO_ANSWER	 2

#define	VALIDATE_START		0x1
#define	VALIDATE_STOP		0x2

#define	SCRIPT_SUCCESS		0
#define	SCRIPT_FAIL		1
#define	SCRIPT_UNTESTABLE	2
#define	SCRIPT_ABORT		3

typedef struct {
	char	*aliases;	/* which aliases to include for the clone */
	char	*script;	/* script to run on each iteration */
	project	*repo;		/* repository we're bisecting */
	char	*dir;		/* where we're working */
	RANGE	rargs;		/* range to bisect */
	ser_t	*skip;		/* revs to skip */
	int	*score;		/* scratchpad for scores */
	char	unit;		/* 's', 'm', 'd', 'M', 'Y' */
	u32	quiet:1;	/* be silent */
	u32	validate:2;	/* do verification of endpoints */
	u32	kill:1;		/* treat killed scripts as failure */
	u32	keys:1;		/* print keys not ages */
} opts;

private	int	bisect(opts *op, sccs *s, ser_t *leftrevs, ser_t rightrev,
    ser_t *next, int *nleft);
private int	bisect_try(opts *op, char *gca, char *md5key);
private void	ago(opts *op, sccs *s, ser_t d, int columns);
private void	info(opts *op, char *fmt, ...);

int
bisect_main(int ac, char **av)
{
	sccs	*s = 0;
	int	c, rc = 2;
	ser_t	*leftrevs = 0, right;
	ser_t	next, last, gca, bottom = 0;
	int	n, ret, i;
	int	columns;
	opts	opts = {0};
	char	*p;
	longopt	lopts[] = {
		{ "cmd:", 310},		/* command to run */
		{ "dir:", 320 },	/* change work area */
		{ "keys", 330},		/* print keys not ages */
		{ "kill-is-error", 350}, /* killing the script is error */
		{ "search|", 355},	 /* find a good starting point */
		{ "validate|", 360 },	/* what to validate*/
		{ "quiet", 'q'},	/* be quiet */
		{ "subset;" , 's' },	/* just clone these components */
		{ 0, 0 }
	};
	char	*bottomKey = 0, *autoselect = 0;
	char	cwd[MAXPATH];
	char	md5key[MD5LEN], gcakey[MD5LEN];

	columns = (p = getenv("COLUMNS")) ? strtoul(p, 0, 10) : 80;
	columns -= 3; // for the "# " part
	getcwd(cwd, MAXPATH);
	opts.validate = (VALIDATE_START|VALIDATE_STOP);
	while ((c = getopt(ac, av, "qr:s:", lopts)) != -1) {
		switch (c) {
		    case 'q': opts.quiet = 1; break;
		    case 'r':
			if (range_addArg(&opts.rargs, optarg, 0)) usage();
			break;
		    case 's':
			p = aprintf("%s -s%s",
			    opts.aliases ? opts.aliases : "", optarg);
			FREE(opts.aliases);
			opts.aliases = p;
			break;
		    case 310:	/* --cmd */
			unless (executable(optarg)) {
				fprintf(stderr, "%s: %s not executable.\n",
				    prog, optarg);
				goto out;
			}
			opts.script = fullname(optarg, 0);
			break;
		    case 320:	/* --dir */
			opts.dir = fullname(optarg, 0);
			if (exists(opts.dir)) {
				fprintf(stderr, "%s: %s already exists.\n",
				    prog, opts.dir);
				goto out;
			}
			break;
		    case 330:	/* --keys */
			opts.keys = 1;
			break;
		    case 350:	/* --kill-is-error */
			opts.kill = 1;
			break;
		    case 355:	/* --search */
			opts.unit = optarg ? optarg[0] : 'M';
			unless (strchr("YyMWwDdhms", opts.unit)) {
				fprintf(stderr, "%s: only one "
				    "of 'YyMWwDdhms' allowed "
				    "in --search\n", prog);
				goto out;
			}
			break;
		    case 360:	/* --validate */
			if (!optarg || streq(optarg, "both")) {
				opts.validate = (VALIDATE_START|VALIDATE_STOP);
			} else if (streq(optarg, "start")) {
				opts.validate = VALIDATE_START;
			} else if (streq(optarg, "stop")) {
				opts.validate = VALIDATE_STOP;
			} else if (streq(optarg, "none")) {
				opts.validate = 0;
			} else {
				fprintf(stderr, "%s: invalid option "
				    "to --validate. It must be one of "
				    "'both', 'start', 'stop', or 'none'",
				    prog);
				goto out;
			}
			break;
		    default: bk_badArg(c, av);
		}
	}
	unless (opts.script) usage();
	bk_nested2root(0);

	unless (opts.dir) {
		opts.dir = proj_fullpath(0, "BitKeeper/tmp/bisect");
		if (exists(opts.dir)) {
			fprintf(stderr, "%s: Aborting. "
			    "`%s' already exists.\n", prog, opts.dir);
			exit(2);
		}
		opts.dir = strdup(opts.dir);
	}
	unless (s = sccs_csetInit(0)) {
		perror("ChangeSet");
		goto out;
	}

	unless (opts.rargs.rstart || opts.unit) usage();
	if (opts.rargs.rstart && opts.unit) usage();

	unless (opts.rargs.rstop) opts.rargs.rstop = "+";

	/* now start the bisection */
	info(&opts, "Setting up work area in: %s", opts.dir);
	/* none of these operations should run user's triggers */
	putenv("BK_NO_TRIGGERS=1");

	systemf("bk clone %s -q '%s' '%s'",
	    opts.aliases ? opts.aliases : "-sTHERE",
	    proj_root(opts.repo), opts.dir);

	if (chdir(opts.dir)) {
		perror(opts.dir);
		goto out;
	}

	unless (opts.rargs.rstart) {
		/* no rstart? go find one */
		undoLimit(s, &bottomKey);
		if (bottomKey) {
			fprintf(stderr,
			    "%s: Warning: this is a partitioned tree "
			    "that can't be rolled back to before\n"
			    "%s.\n" "Using the partition tip as the "
			    "lowest revision.\n", prog, bottomKey);
			bottom = sccs_findKey(s, bottomKey);
		}
		info(&opts, "Finding a good starting point");
		i = 1;
		last = 0;
		unless (opts.unit) opts.unit = 'M';

		while (1) {
			RANGE	rng = {0};

			p = aprintf("-%d%c..%s", i,
			    opts.unit, opts.rargs.rstop);
			T_TMP("range == %s", p);
			range_addArg(&rng, p, 1);
			if (range_process("rng", s, RANGE_ENDPOINTS, &rng)) {
				fprintf(stderr, "%s: invalid range\n", prog);
				goto out;
			}
			FREE(p);
			unless (last) last = s->rstop;
			if ((s->rstart <= 3) || (s->rstart < bottom)) {
				/* We're back at 1.2 so punt */
				fprintf(stderr, "%s: Could not find a revision"
				    " where the script returns zero. "
				    "Aborting\n", prog);
				goto clean;
			}

			T_TMP("last == %d, rstart = %d", last, s->rstart);
			gca = sccs_gca(s, last, s->rstart, 0, 0);
			assert(gca);
			sccs_md5delta(s, gca, gcakey);
			assert(s->rstart);
			sccs_md5delta(s, s->rstart, md5key);
			ago(&opts, s, s->rstart, columns);
			ret = bisect_try(&opts, gcakey, md5key);
			T_TMP("ret == %d", ret);
			unless (ret) {
				/* found it */
				opts.rargs.rstart = strdup(REV(s, s->rstart));
				autoselect = opts.rargs.rstart;
				info(&opts,
				    "Using %s as the starting point",
				    md5key);
				break;
			}
			i *= 2;
			last = s->rstart;
		}
	}
	if (opts.rargs.rstart && !*opts.rargs.rstart) {
		/* range endpoint doesn't like .., giving +..+ as output */
		opts.rargs.rstart = "1.2";
	}

	if (range_process("bisect", s, RANGE_ENDPOINTS, &opts.rargs)) {
		fprintf(stderr, "%s: invalid range\n", prog);
		goto out;
	}

	/* don't use 1.1 as there is no BitKeeper/etc/config file! */
	if (s->rstart < 3) s->rstart = 3;

	/* see if we have a problem we can solve */
	unless (isReachable(s, s->rstop, s->rstart)) {
		fprintf(stderr, "Revisions '%s' and '%s' are on "
		    "completely\n"
		    "different branches (i.e. there are no "
		    "revisions in between).\n"
		    "Cannot bisect. Sorry.\n",
		    REV(s, s->rstart), REV(s, s->rstop));
		goto out;
	}

	unless (opts.quiet) {
		if (opts.rargs.rstart) {
			char	md5start[MAXKEY], md5stop[MAXKEY];

			sccs_md5delta(s, s->rstart, md5start);
			sccs_md5delta(s, s->rstop, md5stop);
			info(&opts, "Bisecting from %s to %s",
			    md5start, md5stop);
		}
	}

	if (opts.validate) {
		int	errors = 0;

		info(&opts, "Validating exit codes of %s", opts.script);
		sccs_md5delta(s, s->rstart, md5key);
		if ((opts.validate & VALIDATE_START) && autoselect) {
			info(&opts, "Skipping %s since it has already "
			    "been verified", md5key);
		} else if (opts.validate & VALIDATE_START) {
			/*
			 * Check that the user provided script returns
			 * sane values for the endpoints before
			 * embarking on a pointless search.
			 */
			info(&opts, "Validating exit status at %s", md5key);
			p = sccs_prsbuf(s,
			    s->rstart, 0,
			    ":MD5KEY: $first(:C:){(:C:)}");
			info(&opts, "%.*s", columns, p);
			free(p);
			ret = bisect_try(&opts, 0, md5key);
			if (ret) {
				fprintf(stderr, "%s: your script should "
				    "return zero for revision %s\n",
				    prog, md5key);
				errors++;
			}
		}
		if (opts.validate & VALIDATE_STOP) {
			sccs_md5delta(s, s->rstop, md5key);
			info(&opts, "Validating exit status at %s", md5key);
			p = sccs_prsbuf(s,
			    s->rstop, 0, ":MD5KEY: $first(:C:){(:C:)}");
			info(&opts, "%.*s", columns, p);
			free(p);
			ret = bisect_try(&opts, 0, md5key);
			unless (ret) {
				fprintf(stderr,
				    "%s: your script should return non-zero "
				    "for revision %s\n", prog, md5key);
				errors++;
			}
		}
		if (errors) {
			getMsg2("bisect_script_error",
			    REV(s, s->rstart), REV(s, s->rstop), '#', stderr);
			goto clean;
		}
	}
	addArray(&leftrevs, &s->rstart);
	right = s->rstop;
	last = right;
	opts.score = calloc(TABLE(s) + 1, sizeof(int)); // [0..TABLE(s)] valid
	srand(time(0));
	while ((ret = bisect(&opts, s, leftrevs, right, &next, &n))
	    == BISECT_TRY_NEXT) {
		ago(&opts, s, next, columns);
		info(&opts, "%10d revisions left to try.", n);
		gca = sccs_gca(s, last, next, 0, 0);
		sccs_md5delta(s, next, md5key);
		sccs_md5delta(s, gca, gcakey);
		if ((ret = bisect_try(&opts, gcakey, md5key)) < 0) {
			fprintf(stderr, "%s: errors when trying "
			    "revision %s.\n"
			    "%s not removed.\n",
			    prog, md5key, opts.dir);
			goto out;
		}
		switch(ret) {
		    case SCRIPT_SUCCESS:
    			addArray(&leftrevs, &next);
			break;
		    case SCRIPT_ABORT:
			fprintf(stderr, "%s: aborting bisect, "
			    "script returned %d\n"
			    "%s not removed.\n",
			    prog, ret, opts.dir);
			goto out;
		    case SCRIPT_UNTESTABLE:
			addArray(&opts.skip, &next);
			break;
		    case SCRIPT_FAIL:
		    default:
			right = next;
			break;
		}
		last = next;
	}

	FREE(opts.skip);

	if (ret == BISECT_NO_ANSWER) {
		rc = 1;
		fprintf(stderr, "# No more changesets to try. "
		    "Sorry I couldn't find it!\n");
	} else {
		assert(ret == BISECT_NO_MORE);
		rc = 0;
		chdir(cwd);
		sccs_md5delta(s, next, md5key);
		fprintf(stderr, "\n# Found it! [%s]:\n", md5key);
		systemf("bk changes -r'%s'", md5key);
	}
clean:	info(&opts, "Cleaning up");
	/* Need to go back to the original proj root to avoid rmtree(cwd) */
	chdir(cwd);
	if (rmtree(opts.dir)) {
		fprintf(stderr, "Could not delete work area at:\n%s\n",
		    opts.dir);
		rc = 2;
	}
	info(&opts, "Bisect has finished.");
out:	FREE(opts.dir);
	FREE(opts.score);
	FREE(opts.script);
	FREE(opts.aliases);
	FREE(bottomKey);
	FREE(autoselect);
	sccs_free(s);
	return (rc);
}

/*
 * Try running the user script in the work area.
 * gca is the gca between the original repo and the workarea.
 * md5key is the key to try.
 */
private int
bisect_try(opts *op, char *gca, char *md5key)
{
	int	ret;
	char	*cmd;
	sccs	*s;

	unless (s = sccs_csetInit(0)) {
		perror("ChangeSet");
		return (-1);
	}
	T_TMP("PATH = %s/%s", proj_root(s->proj), s->sfile);
	ret = sccs_findMD5(s, md5key);
	sccs_free(s);
	if (ret) {
		systemf("bk undo -q -sfa'%s'", md5key);
	} else {
		if (gca) systemf("bk undo -q -sfa'%s'", gca);
		systemf("bk pull -q --unsafe -r'%s'", md5key);
	}

	/* run the user script */
	safe_putenv("BK_BISECT_KEY=%s", md5key);
	cmd = aprintf("'%s'", op->script);
	ret = system(cmd);
	FREE(cmd);
	unless (WIFEXITED(ret)) {
		fprintf(stderr, "%s: running script failed.\n", prog);
		/*
		 * If the script fails, treat it as a bug, this allows
		 * users to kill a script that is hanging and keep the
		 * bisection going.
		 */
		return (op->kill ? 1 : -1);
	}
	ret = WEXITSTATUS(ret);
	return (ret);
}

struct	pair {
	ser_t	d;
	int	score;
};

private int
sortFn(const void *a, const void *b)
{
	struct pair *sa = (struct pair *)a;
	struct pair *sb = (struct pair *)b;

	return (sb->score - sa->score);
}

private int
bisect(opts *op, sccs *s,
    ser_t *leftrevs, ser_t rightrev, ser_t *next, int *nleft)
{
	int	i, n;
	int	*score;
	ser_t	d, e, d1;
	float	f;
	ser_t	highscore;
	ser_t	*candlist = 0;
	struct pair *slist = 0, *p;
	int	ret;

	assert(s && leftrevs && rightrev && next && nleft);
	score = op->score;
	assert(score);
	candlist = walkrevs_collect(s, leftrevs, L(rightrev), 0);
	assert(nLines(candlist));
	*nleft = nLines(candlist);
	if (*nleft == 1) {
		*next = candlist[1];
		*nleft = 0;
		ret = BISECT_NO_MORE;
		goto out;
	}
	removeArrayN(candlist, 1);	/* Tip already tested or assumed */
	(*nleft)--;
	/*
	 * Walk oldest to newest and incrementally compute how many of
	 * items in candlist are ancestors of each item in candlist.
	 * Saves walkrevs time by building on answer for parent.
	 * Note: score is not an addArray, but a C array, so 0 is legal.
	 */
	EACH_REVERSE(candlist) {
		wrdata	wd;

		d = candlist[i];
		FLAGS(s, d) |= D_SET;
		e = PARENT(s, d);	// works if no parent: e = 0
		n = score[e];		// note: e may be outside D_SET range
		walkrevs_setup(&wd, s, L(e), L(d), 0);
		while (d1 = walkrevs(&wd)) if (FLAGS(s, d1) & D_SET) n++;
		walkrevs_done(&wd);
		score[d] = n;
	}
	/* Find node nearest middle, ties go to newer (prefer undo to pull) */
	highscore = 0;
	EACH(candlist) {
		d = candlist[i];
		FLAGS(s, d) &= ~D_SET;
		n = score[d];
		score[d] = min(n, *nleft - n + 1);
		if (score[highscore] < score[d]) highscore = d;
	}
	assert(highscore);
	*next = highscore;
	ret = BISECT_TRY_NEXT;
	if (op->skip) {
		EACH(op->skip) {
			d = op->skip[i];
			if (score[d]) score[d] = 0;
		}
		if (score[highscore]) goto out;
		/*
		 * The best choice has been skipped, so choose one
		 * of the remaining options.
		 */
		EACH(candlist) {
			d = candlist[i];
			unless (score[d]) continue;
			p = addArray(&slist, 0);
			p->d = d;
			p->score = score[d];
		}
		unless ((*nleft = nLines(slist))) {
			ret = BISECT_NO_ANSWER;
			FREE(slist);
			goto out;
		}
		/*
		 * The idea here is that we sort by score (high to
		 * low) and generate a random number between 0 and 1,
		 * and square it to bias it towards 0
		 * (higher scores) and return that index (+1 of
		 * course).
		 *
		 * The rational is that if a particular changeset is
		 * untestable, then csets around it will have a high
		 * probability of also being untestable (and high
		 * scorers since they are around it), so we need to
		 * introduce some randomness to shake the bisection
		 * off the untestable region.
		 */
		sortArray(slist, sortFn);
		do {
			i = rand();
			f = (float)i/(float)RAND_MAX;
			f = f * f;	// bias towards 0
			i = (int)(f * nLines(slist)) + 1;
		} while ((i < 1) && (i > nLines(slist)));
		*next = slist[i].d;
		free(slist);
	}
out:	EACH(candlist) score[candlist[i]] = 0;
	FREE(candlist);
	return (ret);
}

private void
ago(opts *op, sccs *s, ser_t d, int columns)
{
	char	*age, *p, *t;

	if (op->keys) {
		t = sccs_prsbuf(s,
		    d, 0, ":MD5KEY: $first(:C:){(:C:)}");
	} else {
		age = sccs_prsbuf(s, d, 0, ":AGE:");
		p = sccs_prsbuf(s, d, 0, "$first(:C:){(:C:)}");
		t = aprintf("%10s ago: %s", age, p);
		free(age);
		free(p);
	}
	info(op, "%.*s", columns, t);
	free(t);
}

private void
info(opts *op, char *fmt, ...)
{
	va_list	ap;
	FILE	*f;

	if (op->quiet) return;
	f = fmem();
	va_start(ap, fmt);
	fprintf(f, "# ");
	vfprintf(f, fmt, ap);
	fprintf(f, "\n");
	va_end(ap);
	fputs(fmem_peek(f, 0), stderr);
	fclose(f);
}
