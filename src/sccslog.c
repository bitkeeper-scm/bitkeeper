/*
 * Copyright 1998-2003,2005-2011,2014-2016 BitMover, Inc
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

/*
 * sccslog - take a list of s.files and generate a time sorted sccslog.
 */
#include "system.h"
#include "sccs.h"
#include "range.h"

typedef	struct {
	time_t	date;
	char	*pathname;
	char	*key;
	char	*output;
} data;

private	int	forwards(const void *a, const void *b);
private	int	compar(const void *a, const void *b);
private	void	printConsolidatedLog(FILE *);
private	void	printSortedLog(FILE *);
private	void	printlog(FILE *);
private	void	pdelta(sccs *s, ser_t d, FILE *f);
private	void	sccslog(sccs *s);
private	void	freelog(void);
private	int	isBlank(char *p);
private	char	**str2line(char **lines, char *prefix, char *str);
private	char	**db2line(MDBM *db);
private	void	saveComment(MDBM *db, char *comment, char *gfile);
private	void	freeComment(MDBM *db);


private	char	**list;
private	int	ChangeSet;	/* if set, ChangeSet is one of the files */

private	struct {
	u32	uncommitted:1;	/* -A: select all uncomitted deltas in a file */
	u32	changeset:1;	/* -C: comment format used by bk commit */
	u32	rmdups:1;	/* -D: factor out duplicate comments */
	u32	forwards:1;	/* -f: print in oldest..newest order */
	u32	indentOpt:1;	/* 1 if -i was present */
	u32	basenames:1;	/* -b: do basenames */
	u32	sort:1;		/* -s: time sorted pathname|rev output */
	int	indent;		/* -i: indent amount */
	char	*dspec;		/* -d: override default format */
	u32	prs_flags;	/* -n: newline */
} opts;

int
sccslog_main(int ac, char **av)
{
	sccs	*s;
	char	*name;
	int	errors = 0;
	int	local = 0;
	int	standalone = 0;
	char	*url = 0;
	int	c, flags = SILENT;
	RANGE	rargs = {0};
	longopt	lopts[] = {
		{ "dspecf;", 300 },		/* let user pass in dspec */
		{ "standalone", 'S' },		/* alias */
		{ 0, 0 }
	};

	opts.prs_flags = PRS_ALL;
	setmode(1, _O_TEXT);
	while ((c = getopt(ac, av, "AbCc;d|Dfi;L|nr|sS", lopts)) != -1) {
		switch (c) {
		    case 'A': opts.uncommitted = 1; break;	/* doc 2.0 */
		    case 'b': opts.basenames = 1; break;	/* doc 2.0 */
		    case 'C': opts.changeset = 1; break;	/* doc 2.0 */
		    case 'D': opts.rmdups = 1; break;
		    case 'd':
			if (opts.dspec) usage();
			opts.dspec = strdup(optarg);
			break;
		    case 'f': opts.forwards = 1; break;
		    case 'i':					/* doc 2.0 */
			opts.indent = atoi(optarg);
			opts.indentOpt = 1;
			break;
		    case 'L':
			local = 1;
			url = optarg;
			break;
		    case 'n': opts.prs_flags |= PRS_LF; break;
		    case 's': opts.sort = 1; break;		/* doc 2.0 */
		    case 'c':
			if (range_addArg(&rargs, optarg, 1)) usage();
			break;
		    case 'r':
			if (range_addArg(&rargs, optarg, 0)) usage();
			break;
		    case 'S':
		    	standalone = 1;
			break;
		    case 300:	/* --dspecf */
			if (opts.dspec) usage();
			unless (opts.dspec = loadfile(optarg, 0)) {
				fprintf(stderr,
				    "%s: cannot load file \"%s\"\n",
				    prog, optarg);
				return (1);
			}
			break;
		    default: bk_badArg(c, av);
		}
	}
	if (local && opts.changeset) usage();
	if (local) {
		if (opts.changeset) usage();
		if (range_urlArg(&rargs, url, standalone) ||
		    range_addArg(&rargs, "+", 0)) {
			return (1);
		}
	} else if (standalone) {
		fprintf(stderr,
		    "%s: -S only can be used if -L is also used\n", prog);
		return (1);
	}
	if (opts.dspec) dspec_collapse(&opts.dspec, 0, 0);
	if (local && !av[optind]) {
		char	*slopts = aprintf("rm%s", standalone ? "S" : "");

		name = sfiles_local(rargs.rstart, slopts);
		free(slopts);
	} else {
		name = sfileFirst(av[0], &av[optind], 0);
	}
	while (name) {
		unless ((s = sccs_init(name, INIT_NOCKSUM|flags)) &&
		    HASGRAPH(s)) {
next:			sccs_free(s);
			name = sfileNext();
			continue;
		}
		do {
			if (opts.uncommitted) {
				ser_t	d;

				/* find latest cset mark */
				for (d = TABLE(s); d >= TREE(s); d--) {
					if (FLAGS(s, d) & D_CSET) break;
				}
				/* and walk all revs not included in that... */
				range_walkrevs(s, L(d), 0, 0,
				    walkrevs_setFlags, (void *)D_SET);
				s->state |= S_SET;
			} else if (rargs.rstart || sfileRev()) {
				if (range_process("sccslog", s,
					flags|RANGE_SET, &rargs)) {
					goto next;
				}
			}
		} while ((name = sfileNext()) && streq(s->sfile, name));
		sccslog(s);
		verbose((stderr, "%s: %d deltas\n", s->sfile, nLines(list)));
		sccs_free(s);
	}
	if (sfileDone()) errors = 1;
	verbose((stderr, "Total %d deltas\n", nLines(list)));
	if (nLines(list) > 0) {
		pid_t	pid;

		pid = mkpager();
		if (ChangeSet && !opts.indentOpt) opts.indent = 2;
		verbose((stderr, "Sorting...."));
		sortLines(list, opts.forwards ? forwards : compar);
		verbose((stderr, "done.\n"));
		printlog(stdout);
		fclose(stdout);
		freelog();
		if (pid > 0) waitpid(pid, 0, 0);
	}
	return (errors);
}

/*
 * Note that these two are identical except for the d1/d2 assignment.
 */
private	int
compar(const void *a, const void *b)
{
	data	*d1, *d2;

	d1 = *((data**)a);
	d2 = *((data**)b);
	if (d2->date != d1->date) return (d2->date - d1->date);
	unless (ChangeSet) return (strcmp(d2->key, d1->key));
	if (streq(d1->pathname, GCHANGESET)) return (-1);
	if (streq(d2->pathname, GCHANGESET)) return (1);
	return (strcmp(d2->key, d1->key));
}

private	int
forwards(const void *a, const void *b)
{
	return (compar(b, a));
}

private void
printConsolidatedLog(FILE *f)
{
	int	i;
	data	*d;
	MDBM	*db = mdbm_mem();
	char	**lines;

	assert(db);
	EACH(list) {
		d = (data *)list[i];
		unless (d->output && !isBlank(d->output)) continue;
		saveComment(db, d->output, d->pathname);
	}

	lines = db2line(db);
	EACH (lines) {
		fprintf(f, "%s\n", lines[i]);
	}
	freeLines(lines, free);
	freeComment(db);
}

private	void
printSortedLog(FILE *f)
{
	int	i, j;
	data	*d;

	EACH_INDEX(list, j) {
		d = (data *)list[j];

		unless (opts.forwards && ChangeSet) {
			fputs(d->output, f);
			continue;
		}
		if (streq(GCHANGESET, d->pathname)) {
			fputs(d->output, f);
			continue;
		}
		/*
		 * We're on a regular file, go find the ChangeSet,
		 * do that first, then work forward to the ChangeSet.
		 */
		EACH_START(j+1, list, i) {
			d = (data *)list[i];
			if (streq(d->pathname, GCHANGESET)) {
				fputs(d->output, f);
				break;
			}
		}
		EACH_START(j, list, j) {
			if (j >= i) break;
			d = (data *)list[j];
			fputs(d->output, f);
		}
	}
}

private void
pdelta(sccs *s, ser_t d, FILE *f)
{
	int	indent;
	char	*p, *t;
	int	len;
	char	buf[MAXLINE];

	if (opts.dspec) {
		sccs_prsdelta(s, d, opts.prs_flags, opts.dspec, f);
		return;
	}
	if (opts.sort) {
		fprintf(f, "%s|%s\n", PATHNAME(s, d), REV(s, d));
		return;
	}
	if (HAS_PATHNAME(s, d) && streq(PATHNAME(s, d), "ChangeSet")) {
		indent = 0;
	} else {
		indent = opts.indent;
	}
	if (opts.changeset) {
		t = COMMENTS(s, d);
		while (p = eachline(&t, &len)) {
			if (indent) fprintf(f, "%*s", indent, "");
			if (HAS_PATHNAME(s, d)) {
				fprintf(f, "%-8s\t", basenm(PATHNAME(s, d)));
			}
			fprintf(f, "%.*s\n", len, p);
		}
		return;
	}
	if (indent) fprintf(f, "%*s", indent, "");
	if (HAS_PATHNAME(s, d)) {
		unless (opts.basenames) {
			fprintf(f, "%s %s\n  ", PATHNAME(s, d), REV(s, d));
			if (indent) fprintf(f, "%*s", indent, "");
		} else {
			fprintf(f, "%s %s ", basenm(PATHNAME(s, d)), REV(s, d));
		}
	}
	delta_strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", s, d);
	fprintf(f, "%s %s", buf, USERHOST(s, d));
	fprintf(f, " +%d -%d\n", ADDED(s, d), DELETED(s, d));
	t = COMMENTS(s, d);
	while (p = eachline(&t, &len)) {
		if (indent) fprintf(f, "%*s", indent, "");
		fprintf(f, "  %.*s\n", len, p);
	}
	fprintf(f, "\n");
}

private	void
printlog(FILE *f)
{
	if (opts.rmdups) {
		printConsolidatedLog(f);
	} else {
		printSortedLog(f);
	}
}

/*
 * Save the info.
 * If no revisions specified, save the whole tree.
 * If one revision specified (r1 == r2), save just that.
 * Otherwise, save everything from r1 down, pruning at r2.
 */
private	void
sccslog(sccs *s)
{
	ser_t	d;
	data	*nd;
	FILE	*f;
	char	key[MAXKEY];

	f = fmem();
	if (CSET(s)) ChangeSet = 1;
	for (d = TABLE(s); d >= TREE(s); d--) {
		if (SET(s) && !(FLAGS(s, d) & D_SET)) continue;
		if (TAG(s, d)) continue;

		nd = new(data);
		nd->date = DATE(s, d);
		nd->pathname =
		    strdup(HAS_PATHNAME(s, d) ? PATHNAME(s, d) : s->gfile);
		sccs_sdelta(s, d, key);
		nd->key = strdup(key);

		if (opts.rmdups) {
			if (opts.dspec) {
				nd->output = sccs_prsbuf(s, d,
				    opts.prs_flags, opts.dspec);
			} else {
				nd->output = strdup(COMMENTS(s, d));
				if (nd->output &&
				    (streq(REV(s, d), "1.1") ||
				     streq(REV(s, d), "1.0")) &&
				    strneq(nd->output,
					"BitKeeper file ", 15)) {
					free(nd->output);
					nd->output = strdup("new file");
				}
			}
		} else {
			pdelta(s, d, f);
			nd->output = fmem_dup(f, 0);
			ftrunc(f, 0);
		}

		list = addLine(list, nd);
	}
	fclose(f);
}

private	void
freelog(void)
{
	data	*d;
	int	i;

	EACH(list) {
		d = (data *)list[i];
		free(d->pathname);
		free(d->key);
		free(d->output);
	}
	freeLines(list, free);
}

/*
 * Return true if blank line
 */
private int
isBlank(char *p)
{
	while (*p) {
		unless (isspace(*p++))  return (0);
	}
	return (1);
}

/*
 * Convert a string into line array
 */
private char **
str2line(char **lines, char *prefix, char *str)
{
	char	*p, *q;

	q = p = str;
	while (1) {
		if (!*q) {
			if ((&q[-1] >= str) && (q[-1] != '\n')) {
				lines = addLine(lines,
						aprintf("%s%s", prefix, p));
			}
			break;
		}
		if (*q == '\n') {
			*q = 0;
			lines = addLine(lines, aprintf("%s%s", prefix, p));
			*q++ = '\n';
			p = q;
		} else {
			q++;
		}
	}
	return (lines);
}

/*
 * Convert cset comment strore in a mdbm into lines format
 */
private char **
db2line(MDBM *db)
{
	kvpair	kv;
	char	**lines = 0;
	char	*comment, *gfiles, *lastg, *p, *q;
	char	*many = "Many files:";
	int	i, len = 0;
	MDBM	*gDB;

	lastg = "";
	EACH_KV(db) {
		/*
		 * Compute length of file list
		 */
		comment = kv.key.dptr;
		memcpy(&gDB, kv.val.dptr, sizeof(MDBM *));
		len = 0;
		EACH_KV(gDB) {
			len += strlen(kv.key.dptr) + 2;
		}
		len += 2;

		/*
		 * Extract gfile list and set it as the section header
		 * Skip gfile list if too long
		 */
		if (len <= 80) {
			p = gfiles = malloc(len);
			i = 1;
			EACH_KV(gDB) {
				q = kv.key.dptr;
				if (i++ > 1) {
					*p++ = ',';
					*p++ = ' ';
				}
				while (*q) *p++ = *q++;
			}
			*p++ = ':';
			*p = 0;
			assert(gfiles + len > p);

		} else {
			gfiles = strdup(many);
		}

		/*
		 * If gfile list is same as previous, skip
		 */
		if (!streq(lastg, gfiles)) {
			lines = addLine(lines, gfiles);
		}
		lastg = gfiles;

		/*
		 * Now extract the comment block
		 */
		lines = str2line(lines, "  ", comment);
	}
	return (lines);
}

/*
 * We dump the comments into a mdbm to factor out repeated comments
 * that came from different files.
 */
private void
saveComment(MDBM *db, char *comment_str, char *gfile)
{
	datum	k, v, tmp;
	MDBM	*gDB = 0;
	int	ret;

	k.dptr = comment_str;
	k.dsize = strlen(comment_str) + 1;
	tmp = mdbm_fetch(db, k);
	if (tmp.dptr) memcpy(&gDB, tmp.dptr, sizeof (MDBM *));
	unless (gDB) gDB = mdbm_mem();
	ret = mdbm_store_str(gDB, basenm(gfile), "", MDBM_INSERT);
	/* This should work, or it will be another file with the same
	 * basename.
	 */
	assert(ret == 0 || (ret == 1 && errno == EEXIST));
	unless (tmp.dptr) {
		v.dptr = (char *) &gDB;
		v.dsize = sizeof (MDBM *);
		ret = mdbm_store(db, k, v, MDBM_REPLACE);
	}
}

private void
freeComment(MDBM *db)
{
	kvpair	kv;
	MDBM	*gDB;

	EACH_KV(db) {
		memcpy(&gDB, kv.val.dptr, sizeof (MDBM *));
		mdbm_close(gDB);
	}
	mdbm_close(db);
}
