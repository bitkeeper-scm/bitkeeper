/*
 * Copyright 2000-2008,2011,2016 BitMover, Inc
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

//#define	RCS_DEBUG
#include "system.h"
#include "sccs.h"
#include "rcs.h"

private	int	eatlog(RCS *rcs, MMAP *m);
private	int	eatdelta(RCS *rcs, MMAP *m);
private	int	eatsym(RCS *rcs, MMAP *m);
private	void	rcsmkgraph(RCS *rcs);
private	void	dates(RCS *rcs);
private	void	select_branch(RCS *rcs, char *branch);
private	void	doit(RCS *r);
private	char	**build_branchdeps(MDBM *m, char **list, char *sym);
private	void	tagsniff(RCS *r);
private	void	branchgraph(RCS *r);
private	void	add_symdep(char *from, char *to);
private	void	rcs_table(RCS *rcs);
private	void	rcs_meta(RCS *rcs);
private	rdelta	*rcs_defbranch(const RCS *rcs);

private	int	listtags = 0;
private	MDBM	*syms;
private	int	debug = 0;

#define	LASTTIME	2000000000u

int
rcsparse_main(int ac, char **av)
{
	int	i;
	int	c;
	char	*cvsbranch = 0;
	int	graph = 0;
	void	(*work)(RCS *rcs);

	while ((c = getopt(ac, av, "b;gdt", 0)) != -1) {
		switch (c) {
		    case 'b': cvsbranch = optarg; break;
		    case 'd': debug = 1; break;
		    case 'g': graph = 1; break;
		    case 't': listtags = 1; break;
		    default: bk_badArg(c, av);
		}
	}
	syms = mdbm_mem();
	if (listtags) {
		work = tagsniff;
	} else if (graph) {
		work = branchgraph;
	} else {
		work = doit;
	}
	if (av[optind] && streq("-", av[optind]) && !av[optind + 1]) {
		char	buf[MAXPATH];

		while (fgets(buf, sizeof(buf), stdin)) {
			RCS	*r;
			chop(buf);
			r = rcs_init(buf, cvsbranch);
			if (r) {
				work(r);
				rcs_free(r);
			}
		}
	} else if (av[optind]) {
		for (i = optind; av[i]; ++i) {
			RCS	*r;
			r = rcs_init(av[i], cvsbranch);
			if (r) {
				work(r);
				rcs_free(r);
			}
		}
	} else {
		usage();
	}
	if (listtags) {
		kvpair	kv;
		EACH_KV (syms) {
			time_t	s,e;
			s = ((time_t *)kv.val.dptr)[0];
			e = ((time_t *)kv.val.dptr)[1];
			printf("%s %u %u", kv.key.dptr,
			    (unsigned)s, (unsigned)e);
			if (s == 0) {
				printf(" out of view X\n");
			} else if (e <= s) {
				printf(" invalid X\n");
			} else {
				printf("\n");
			}
		}
		/*
		 * If -b option was passed, then take that list of branches
		 * and append the timestamps when those branches could have
		 * been created.
		 */
		if (cvsbranch) {
			char	**branches = splitLine(cvsbranch, ",", 0);
			int	i;

			printf("=");
			EACH (branches) {
				time_t	s,e;
				datum	k,v;
				k.dptr = aprintf("%s_BASE", branches[i]);
				k.dsize = strlen(k.dptr) + 1;
				v = mdbm_fetch(syms, k);
				unless (v.dptr) {
					fprintf(stderr,
					    "ERROR: can't find info for %s!\n",
					    k.dptr);
					exit(1);
				}
				s = ((time_t *)v.dptr)[0];
				e = ((time_t *)v.dptr)[1];
				unless (i == 1) printf(",");
				printf("%s:%u-%u", branches[i],
				    (unsigned)s, (unsigned)e);
			}
			printf("\n");
		}
	} else if (graph && cvsbranch) {
		char	**list = 0;
		char	*p;
		kvpair	kv;
		list = build_branchdeps(syms, list, cvsbranch);
		reverseLines(list);
		p = joinLines(",", list);
		printf("%s\n", p);
		free(p);
		freeLines(list, free);
		EACH_KV (syms) {
			MDBM	*deps = *(MDBM **)kv.val.dptr;
			mdbm_close(deps);
		}
	} else {
		fputc('\n', stderr);
	}
	mdbm_close(syms);
	return (0);
}

/*
 * Given a MDBM that has a pointer to a MDBM which is the set of dependancies
 * for each branch tag, do a recursive walk to find the path to a branch.
 * The path is added to the 'list' argument and returned.
 */
private char **
build_branchdeps(MDBM *m, char **list, char *sym)
{
	MDBM	*deps;
	datum	k, v;

	k.dptr = sym;
	k.dsize = strlen(k.dptr) + 1;
	v = mdbm_fetch(m, k);
	if (v.dptr) {
		deps = *(MDBM **)v.dptr;
		EACH_KEY (deps) {
			list = build_branchdeps(m, list, k.dptr);
		}
		mdbm_close(deps);
		mdbm_delete_str(m, sym);
		list = addLine(list, strdup(sym));
	}
	return (list);
}


/*
 * look for branches that depend on other branches
 *  ie.  A.B.C.D.0.E -> A.B.0.C
 */
private void
branchgraph(RCS *r)
{
	sym	*sy;

	for (sy = r->symbols; sy; sy = sy->next) {
		char	*rev = strdup(sy->rev);
		char	*s;
		char	*t;

		/* is branch symbol? */
		t = strrchr(rev, '.');
		unless (t > rev + 4 && t[-2] == '.' && t[-1] == '0') {
			free(rev);
			continue;
		}
		add_symdep(sy->name, 0);

		t[-2] = 0;   /* A.B.C.D */
		for (t=t-3; *t != '.'; t--);  /* t just after C */
		for (s=t-1; s > rev && *s != '.'; s--); /* s just after B */

		/* is branch of branch? */
		if (s > rev) {
			char	*b_rev;
			sym	*b_sy;

			*s = 0;
			*t = 0;
			b_rev = aprintf("%s.0.%s", rev, s+1);

			for (b_sy = r->symbols; b_sy; b_sy = b_sy->next) {
				if (streq(b_sy->rev, b_rev)) break;
			}
			if (b_sy) {
				if (debug) {
					printf("-%s %s\n",
					    sy->name, b_sy->name);
				}
				add_symdep(sy->name, b_sy->name);
			}
		}
		free(rev);
	}
}

/*
 * Add to the list of dependancies for a given branch symbol
 */
private void
add_symdep(char *from, char *to)
{
	datum	k, v;
	MDBM	*m;

	k.dptr = from;
	k.dsize = strlen(k.dptr) + 1;

	v = mdbm_fetch(syms, k);
	if (v.dptr) {
		m = *(MDBM **)v.dptr;
	} else {
		m = mdbm_mem();
		v.dptr = (char *)&m;
		v.dsize = sizeof(m);
		mdbm_store(syms, k, v, MDBM_INSERT);
	}
	if (to) mdbm_store_str(m, to, "", MDBM_INSERT);
}

private	void
tagsniff(RCS *r)
{
	sym	*sym;

	for (sym = r->symbols; sym; sym = sym->next) {
		rdelta	*d;
		time_t	stime[2];
		datum	k, v;

		/*
		 * Walk on the deltas on the current branch
		 * looking for the symbol
		 */
		for (d = r->tree; d; d = d->kid) {
			if (streq(sym->rev, d->rev)) break;
		}
		k.dptr = sym->name;
		k.dsize = strlen(k.dptr) + 1;
		if (d) {
			time_t	s, e;
			int	len;

			s = d->date;
			e = d->kid ? d->kid->date : LASTTIME;

			/*
			 * A branch tag found on a 1.1 rev *might*
			 * indicate the file was created after
			 * the branch was tagged, so we can't use that
			 * delta time to restrict the search range.
			 */
			len = strlen(sym->name);
			if (len > 5 &&
			    streq(&sym->name[len - 5], "_BASE") &&
			    streq(d->rev, "1.1")) {
				s = 1;
			}

			if (debug) {
				printf("-%s %u %u |%s\n",
				    sym->name,
				    (unsigned)s, (unsigned)e,
				    r->rcsfile);
			}

			/* found a match */
			v = mdbm_fetch(syms, k);
			if (v.dptr) {
				/* seen before */
				stime[0] = ((time_t *)v.dptr)[0];
				stime[1] = ((time_t *)v.dptr)[1];

				if (stime[0]) {
					if (s > stime[0]) stime[0] = s;
					if (e < stime[1]) stime[1] = e;
				}
			} else {
				stime[0] = s;
				stime[1] = e;
			}
		} else {
			stime[0] = stime[1] = 0;
		}
		v.dptr = (void *)&stime;
		v.dsize = sizeof(stime);
		mdbm_store(syms, k, v, MDBM_REPLACE);
	}
}

private	void
doit(RCS *r)
{
	int	i;
	static	int len = 0;
	static	int n = 0;

	/* print progress... */
	fputc('\r', stderr);
	fprintf(stderr, "%d ", ++n);
	fprintf(stderr, "%s", r->rcsfile);
	for (i = strlen(r->rcsfile); i < len; i++) fputc(' ', stderr);
	len = strlen(r->rcsfile);

	printf("== %s ==\n", r->rcsfile);
	rcs_meta(r);
	//rcs_prs(r);
	rcs_table(r);
}

private	int
advance(MMAP *m, char what)
{
	char	*p = m->where;

#ifdef	RCS_DEBUG
	if (p < m->end) fprintf(stderr, "ADV at '%c', want '%c'\n", *p, what);
#endif
	while ((p < m->end) && (*p != what)) {
		rcsdebug((stderr, "SKIP '%c'\n", *p));
		++p;
	}
	if ((p >= m->end) || (*p != what)) return (0);
	m->where = ++p;
	return (1);
}

private	void
skip_white(MMAP *m)
{
	char	*p = m->where;

	while ((p < m->end) && isspace(*p)) p++;
	m->where = p;
}

private	char	*
mwhere(MMAP *m)
{
	if (m->where >= m->end) return (0);
	rcsdebug((stderr, "W=%.10s\n", m->where));
	return (m->where);
}

private void
expect(MMAP *m, const char *string)
{
	char	*p = m->where;
	const char	*s = string;

	while (*s && (*s++ == *p++)) {
		if (p == m->end) {
			fprintf(stderr,
			    "Unexpected EOF on RCS log (looking for %s)\n",
			    string);
			exit(1);
		}
	}
	if (*s != '\0') {
		fprintf(stderr,
"Unexpected input in RCS log (looking for %s) Got:\n%.200s\n",
		    string, m->where);
		exit(1);
	}
	m->where = p;
}

private char *
dup_tochar(MMAP *m, char c)
{
	char	*p = m->where;
	char	*s;
	int	len;
	while ((p < m->end) && *p != c) p++;
	len = p - m->where;
	s = malloc(len + 1);
	strncpy(s, m->where, len);
	s[len] = 0;
	p++;
	m->where = p;
	return s;
}

private void
rcs_rootkey(RCS *rcs)
{
	rdelta	*d;
	u32	randbits = 0;
	char	*str;
	struct	tm *tp;

	d = rcs->tree;
	while (d->dead && d->kid) d = d->kid;

	/*
	 * Generate the random bits for this file.  The key point is
	 * that the final rootkey needs to be unique, but
	 * deterministic so that if a user does another incremental
	 * import, they will get the same data.
	 *
	 * We can only use the information from the 1.1 delta of the
	 * input file, and nominally only comments on that delta are
	 * not already included in the key.  But we include a hash of
	 * the rest of the bits in the key so that this file is less
	 * likely to have conflicts when moved to the delted
	 * directory.  (People can dupicate RCS files in a CVS
	 * tree...)
	 */
	str = rcs->text;
	if (str) randbits = adler32(randbits, str, strlen(str));
	str = d->comments;
	if (str) randbits = adler32(randbits, str, strlen(str));

	tp = utc2tm(d->date-1);
	str = aprintf("%s@%s|%s|%02d%02d%02d%02d%02d%02d",
			d->author,
			sccs_gethost(),
			rcs->workfile,
			tp->tm_year + 1900,
			tp->tm_mon + 1,
			tp->tm_mday,
			tp->tm_hour,
			tp->tm_min,
			tp->tm_sec);
	randbits = adler32(randbits, str, strlen(str));
	rcs->rootkey = aprintf("%s|%05u|%08x",
	    str, randbits & 0xffff, randbits);
	free(str);
}

#define	ERR(msg)	{ error = msg; goto err; }
RCS	*
rcs_init(char *file, char *cvsbranch)
{
	MMAP	*m;
	char	*p, *t;
	char	buf[32<<10];
	RCS	*rcs;
	char	*error = 0;

	unless ((t = strrchr(file, ',')) && (t[1] == 'v') && !t[2]) {
		fprintf(stderr, "Not an RCS file '%s'\n", file);
		return (0);
	}
	unless (m = mopen(file, "b")) {
		fprintf(stderr, "Cannot mmap %s\n", file);
err:		if (error) fprintf(stderr, "ERR(%s) ", error);
		perror(file);
		if (m && m->where) fprintf(stderr, "MMAP(%.20s)\n", m->where);
		exit(1);
	}

	rcs = new(RCS);
	rcs->rcsfile = strdup(file);
	rcs->workfile = strdup(file);
	t = strrchr(rcs->workfile, ',');
	*t = 0;

	rcs->kk = strdup("-kk");

	/* head */
	skip_white(m);
	unless (p = mwhere(m)) ERR("head mwhere");
	unless (strneq("head", p, 4)) ERR("head");
	unless (advance(m, ';')) ERR("head advance");
	skip_white(m);

	/* branch? */
	unless (p = mwhere(m)) goto err;
	if (strneq(p, "branch", 6)) {
		m->where += 6;
		skip_white(m);
		unless (p = mwhere(m)) ERR("branch where");
		if (*p == ';') {
			m->where = p + 1;
			goto acc;
		}
		for (t = buf; isdigit(*p) || (*p == '.'); *t++ = *p++);
		*t = 0;
		rcs->defbranch = strdup(buf);
		m->where = p + 1;
		skip_white(m);
	}

	/* skip access control */
acc:	skip_white(m);
	unless (p = mwhere(m)) ERR("access where");
	unless (strneq("access", p, 6)) ERR("access");
	unless (advance(m, ';')) ERR("access advance");
	skip_white(m);

	/* symbols */
	unless (p = mwhere(m)) ERR("sym where");
	unless (strneq("symbols", p, 7)) ERR("sym");
	m->where += 7;
	while (eatsym(rcs, m));

	/* skip locks */
	skip_white(m);
	unless (p = mwhere(m)) ERR("locks where");
	unless (strneq("locks", p, 5)) ERR("locks");
	unless (advance(m, ';')) ERR("locks advance");

	skip_white(m);
	unless (p = mwhere(m)) ERR("strict where");
	if (strneq("strict", p, 6)) {
		unless (advance(m, ';')) ERR("strict advance");
		skip_white(m);
		unless (p = mwhere(m)) ERR("strict where2");
	}

	/* skip everything until we get a number */
	skip_white(m);
	unless (p = mwhere(m)) ERR("comment where");
	if (strneq(p, "comment", 7)) {
		unless (advance(m, '@')) ERR("comment advance1");
		for (;;) {
			unless (advance(m, '@')) ERR("comment advance2");
			if (*m->where != '@') break;
			m->where++;
		}
		unless (advance(m, ';')) ERR("comment advance3");
	}
	skip_white(m);
	unless (p = mwhere(m)) ERR("expand where");

	if (strneq(p, "expand", 6)) {
		char	*kk;
		int	i;
		static	char	*good_expands[] = {
			"-kb", "-kv", "-kkv", "-kkvl", "-kk", "-ko", "-kv"
		};
		const int good_expands_size =
			sizeof(good_expands)/sizeof(char *);
		unless (advance(m, '@')) ERR("expand advance");
		skip_white(m);
		kk = dup_tochar(m, '@');
		rcs->kk = aprintf("-k%s", kk);
		free(kk);

		for (i = 0; i < good_expands_size; i++) {
			if (streq(rcs->kk, good_expands[i]))
				break;
		}
		if (i == good_expands_size) {
			fprintf(stderr,
			    "\n!!! Warning: unknown expand statement in %s\n",
			    file);
		}
		unless (advance(m, ';')) ERR("expand advance2");
	}
	skip_white(m);
	unless (p = mwhere(m)) ERR("before delta");

	while (eatdelta(rcs, m));

	/* MKS has extended stuff */
	unless (p = mwhere(m)) ERR("ext where");
	if (strneq(p, "ext", 3)) {
		unless (advance(m, '@')) ERR("ext advance");
		unless (advance(m, '@')) ERR("ext advance2");
		skip_white(m);
	}

	/* desc */
	unless (p = mwhere(m)) ERR("desc where");
	unless (strneq(p, "desc", 4)) ERR("desc");
	unless (advance(m, '@')) ERR("desc advance");
	unless (p = mwhere(m)) ERR("desc where2");
	for (t = buf; p < m->end; p++) {
		if ((*p == '@') && (p[1] != '@')) {
			*t = 0;
			if (t != buf) rcs->text = strdup(buf);
			m->where = p + 1;
			break;
		} else {
			*t++ = *p;
			if (*p == '@') p++;	/* unquote it */
		}
	}

	while (eatlog(rcs, m));
	mclose(m);
	rcsmkgraph(rcs);
	dates(rcs);
	select_branch(rcs, cvsbranch);
	rcs_rootkey(rcs);
	return (rcs);
}

private sym *
rcs_findsym(const RCS *rcs, char *symbol)
{
	sym	*s;
	for (s = rcs->symbols; s && !streq(symbol, s->name); s = s->next);
	return (s);
}

rdelta	*
rcs_findit(const RCS *rcs, char *rev)
{
	rdelta	*d;

	unless (isdigit(rev[0])) {
		sym	*s = rcs_findsym(rcs, rev);
		if (s) {
			rev = s->rev;
		} else {
			fprintf(stderr,
"Error in RCS parse, can't find symbol '%s' in file '%s'\n",
			    rev, rcs->rcsfile);
			exit(1);
		}
	}
	for (d = rcs->table; d && !streq(rev, d->rev); d = d->next);
	return (d);
}

private rdelta	*
rcs_defbranch(const RCS *rcs)
{
	rdelta	*d;
	char	buf[1000];

	unless (rcs->defbranch) return (rcs->tree);
	if (d = rcs_findit(rcs, rcs->defbranch)) return (d);
	sprintf(buf, "%s.1", rcs->defbranch);
	unless (d = rcs_findit(rcs, buf)) {
		fprintf(stderr, "Can't find defbranch %s\n", rcs->defbranch);
		return (0);
	}
	return (d);
}


/*
 * Put the deltas into a graph
 */
private	void
rcsmkgraph(RCS *rcs)
{
	rdelta	*d, *k, *p, *start;
	int	n = rcs->n;
	char	*t;
	char	buf[1024];

	for (d = rcs->table; d && d->next; d = d->next);

	unless (d) {
		fprintf(stderr, "rcsparse: ERROR %s has no revisions!!\n",
			rcs->rcsfile);
		exit(1);
	}
	while (d->snext) d = d->prev;
	assert(d);
	rcs->tree = d;
	d->ingraph = 1;
	n--;

	/*
	 * Walk through the rest of the list, inserting all the kids on
	 * the trunk and marking them.  They are in order, I believe.
	 */
	for (k = d->next; k; k = k->next) {
		if (streq(k->snext, d->rev)) {
			d->kid = k;
			k->parent = d;
			k->ingraph = 1;
			n--;
			d = k;
		}
	}

	/*
	 * Now walk through the list finding all branch heads and
	 * link in their kids.  We walk backwards because branches
	 * are in reverse order.
	 * First go forwards to find the start.
	 */
	for (d = rcs->table; d && !d->ingraph; d = d->next);
	start = d;
	while (n) {
		for (d = start; d->ingraph; d = d->prev);

		/*
		 * Go find our parent.
		 */
		strcpy(buf, d->rev);
		t = strrchr(buf, '.'); assert(t); *t = 0;
		t = strrchr(buf, '.'); assert(t); *t = 0;
		p = rcs_findit(rcs, buf);
		assert(p);
		d->parent = p;
		d->ingraph = 1;
		n--;

		while (n && d->snext) {
			assert(d->prev);
			if (streq(d->prev->rev, d->snext)) {	/* cool */
				d->kid = d->prev;
			} else {
				d->kid = rcs_findit(rcs, d->snext);
				assert(d->kid);
			}
			d->kid->parent = d;
			d = d->kid;
			d->ingraph = 1;
			n--;
		}
	}
}


private char *
tm2date(time_t tt)
{
	char	tmp[50];

	if (tt == LASTTIME) return (strdup("now"));

	strftime(tmp, sizeof(tmp), "%Y/%m/%d-%H:%M:%S", gmtime(&tt));
	return (strdup(tmp));
}

private void
select_branch(RCS *rcs, char *cvsbranch)
{
	rdelta	*v1111;

	/*
	 * Handle vendor branches
	 */
	if (!getenv("BK_IMPORT_NOVENDORBRANCH") &&
	    (v1111 = rcs_findit(rcs, "1.1.1.1"))) {
		rdelta	*v11 = rcs_findit(rcs, "1.1");
		rdelta	*v12 = rcs_findit(rcs, "1.2");
		rdelta	*v;

		if (!v12) {
			v11->kid = v1111;
		} else if (v12->date > v1111->date) {
			v11->kid = v1111;
			v = v1111;
			while (v) {
				rdelta	*k = v->kid;

				if (k == v12) break;	/* already linked */
				if (!k || k->date > v12->date) {
					v->kid = v12;
					v12->parent = v;
					break;
				}
				v = k;
			}
		}
	}

	/*
	 * At this point all deltas are in the table with rcs->tree
	 * pointing at the root of the trunk (1.1 usually) and the
	 * d->kid pointers following the trunk.
	 *
	 * At this point we will see if the user wants us to follow a
	 * different branch.  We find a rev on that branch and then
	 * walk backwards and reset the kid pointers.  There are two
	 * cases:
	 *
	 * CVS
	 *  In CVS a branch is indicated by a symbolic tag pointing at
	 *  rev of the base of the branch.  This branch may or may not
	 *  be present in the current file.  The tag uses a strange
	 *  notation of putting a '0' before the last element.  So if
	 *  the tag is 'X.Y.0.Z' we look for 'X.Y.Z.1'.  If that delta
	 *  is present then that branch is used.  If it is not, then
	 *  the rev 'X.Y' is the last revision we import.
	 *
	 *  Like CVS if file is missing the branch tag we assume the
	 *  file is skipped.
	 *
	 * RCS defbranch
	 *  In RCS, the default branch is a global state in the RCS
	 *  file that gives the branch where new work will be added by
	 *  default.  If the is present we find that rev and reset the
	 *  kid pointers above it.
	 *
	 * After this operation the following code walks the revs on
	 * the selected branch:
	 *     for (d = rcs->tree; d; d->kid) { ... }
	 */
	if (cvsbranch) {
		/* symbolic branch name.  Assume CVS */
		char	brev[MAXREV];
		char	*p;
		rdelta	*d;
		sym	*s = 0;
		rdelta	*lastd;
		time_t	branchtime_s = 0;
		time_t	branchtime_e = LASTTIME;
		time_t	stime;
		time_t	etime;
		char	*branchname = 0;
		char	**branches = splitLine(cvsbranch, ",", 0);
		int	i;

		/*
		 * If this is a branch of a branch, we need to setup
		 * the previous branch first, so we can use the kid
		 * pointers to find this branch.
		 */
		if (p = strchr(cvsbranch, ',')) select_branch(rcs, p+1);

		/* first time of first non-deleted delta on trunk */
		d = rcs->tree;
		while (d && d->dead) d = d->next;
		stime = d ? d->date : LASTTIME;

		EACH (branches) {
			if (branchname) free(branchname);

			/*
			 * Optionally the branch tag also as the time
			 * (as a time_t) when the branch was created.
			 * If this exists then it is used to determine
			 * what is done when the branch tag just
			 * doesn't exist in the file.  If the time is
			 * not included, then the file is always
			 * skipped.
			 */
			branchname = strdup(branches[i]);
			if (p = strchr(branchname, ':')) *p++ = 0;
			if ((s = rcs_findsym(rcs, branchname))) break;

			if (p) {
				branchtime_s = strtoul(p, &p, 10);
				if (*p) {
					assert(*p == '-');
					branchtime_e = strtoul(p+1, 0, 10);
				} else {
					branchtime_e = branchtime_s;
				}
			}
		}
		freeLines(branches, free);

		/* find end time on trunk */
		for (lastd = rcs->tree; lastd->kid; lastd = lastd->kid);
		etime = lastd->dead ? lastd->date : LASTTIME;

		/*
		 * If we found a branch tag, but it was from an
		 * eariler branch, then we assume this file must have
		 * been deleted before the final branch was tagged.
		 * (or created on eariler branch after target branch
		 * was tagged)
		 */
		if (s && i != 1) {
			unless (stime > branchtime_s || etime < branchtime_e) {
				fprintf(stderr,
"WARNING: file %s is missing branch %s,\n"
"         but it looks like it should have been active when that\n"
"         branch was tagged.\n",
				    rcs->rcsfile, cvsbranch);
			}
			s = 0;
		}

		if (s) {
			strcpy(brev, s->rev);
		} else if (stime > branchtime_s && etime >= branchtime_e ) {
			/* created after branch, skip */
			rcs->tree->dead = 1;
			rcs->tree->kid = 0;
			goto skip_branch;
		} else if (branchtime_s &&
			   (stime > branchtime_s) && (etime < branchtime_e)) {
			/*
			 * XXX if a file is created and deleted during the
			 * time window when the branch could have been tagged
			 * but it is not tagged, we really don't know what
			 * to do.  The file might have been before the
			 * branch or after it.
			 * We will assume the file was created after the
			 * branch because that is how findcset will label it.
			 */
			rcs->tree->dead = 1;
			rcs->tree->kid = 0;
			goto skip_branch;
		} else if (etime < branchtime_e) {
			/* deleted before branch, import */
			goto skip_branch;
		} else {
			/*
			 * check to see if file may have been deleted when
			 * this branch was tagged.
			 */
			for (d = rcs->tree; d; d = d->kid) {
				if (d->dead && d->kid &&
				    (d->date < branchtime_e) &&
				    (d->kid->date > branchtime_s)) {
					sprintf(brev, "%s.0.999", d->rev);
					break;
				}
			}
			unless (d) {
				char	*fs = tm2date(stime);
				char	*fe = tm2date(etime);
				char	*bs = tm2date(branchtime_s);
				char	*be = tm2date(branchtime_e);

				fprintf(stderr,
"ERROR: file %s doesn't have the %s branch tag,\n"
"but was active when that branch was created.\n"
"(file @ (%s to %s) vs branch @ (%s to %s))!\n",
				    rcs->rcsfile, branchname, fs, fe, bs, be);
				free(fs); free(fe); free(bs); free(be);
				exit(1);
			}
		}
		/* (brev = branch) =~ s/\.0\.(\d+)$/$1.1/ */
		p = strrchr(brev, '.');
		if (!p || p[-2] != '.' || p[-1] != '0') {
			fprintf(stderr,
"WARNING: Branch tag %s points at revision %s which is not\n"
"         in expected form of X.Y.0.Z in %s\n"
"         This usually means that tag is not a branch tag.\n",
			   branchname,  brev, rcs->rcsfile);
			/*
			 * Here we need to make a rev that will cause the
			 * code below to act like we have a branch point that
			 * was never added on.
			 * So we add a bogus .X.Y that will never match, and
			 * setup 'p' so that when that fails the 'p[-2] = 0'
			 * below will return us the original branch rev.
			 */
			p = brev + strlen(brev) + 2;
			strcat(brev, ".X.Y");
		} else {
			strcpy(p - 1, p + 1);  /* not portable?? (overlap) */
			strcat(p, ".1");
		}
		free(branchname);
		branchname = 0;
		d = rcs_findit(rcs, brev);
		unless (d) {
			p[-2] = 0;  /* X.Y */
			d = rcs_findit(rcs, brev);
			unless (d) {
				fprintf(stderr,
				    "ERROR: can't find %s in %s\n",
				    brev, rcs->rcsfile);
				exit(1);
			}
			d->kid = 0; /* this branch ends here! */
		}
		while (d->parent) {
			d->parent->kid = d;
			d = d->parent;
		}
	skip_branch:;
	} else if (rcs->defbranch) {
		rdelta	*d;
		d = rcs_defbranch(rcs);
		while (d->parent) {
			d->parent->kid = d;
			d = d->parent;
		}
	}
}

#ifdef	THIS_CODE_IS_BUSTED
private	void
rcs_prs(RCS *rcs)
{
	rdelta	*d;
	int	n = rcs->n;

	for (d = rcs->tree; d; d = d->kid) {
		printf("%s ", d->rev);
		n--;
		d->printed = 1;
	}
	printf("\n");
	while (n) {
		for (d = rcs->table; d; d = d->next) {
			if (!d->printed) break;
		}
		while (d) {
			printf("%s ", d->rev);
			n--;
			d->printed = 1;
			d = d->kid;
		}
		printf("\n");
	}
}
#endif

private	void
dates(RCS *rcs)
{
	rdelta	*d;
	int	n = rcs->n;

	for (d = rcs->tree; d; d = d->kid) {
		n--;
		assert(!d->printed);
		d->printed = 1;
		d->date = sccs_date2time(d->sdate, "0:00");
		if (d->parent && (d->parent->date >= d->date)) {
			d->dateFudge = (d->parent->date - d->date) + 1;
			d->date += d->dateFudge;
		}
	}
	while (n) {
		for (d = rcs->table; d; d = d->next) {
			if (!d->printed) break;
		}
		while (d && !d->printed) {
			n--;
			d->printed = 1;
			d->date = sccs_date2time(d->sdate, "0:00");
			if (d->parent && (d->parent->date >= d->date)) {
				d->dateFudge = (d->parent->date - d->date) + 1;
				d->date += d->dateFudge;
			}
			d = d->kid;
		}
	}
}

private	void
rcs_table(RCS *rcs)
{
	rdelta	*d;
	char	*p;
	sym	*sym;

	for (d = rcs->table; d; d = d->next) {
		assert(d->rev);
		assert(d->author);
		printf("D %s %s %s\n", d->rev, d->sdate, d->author);
		//if (d->snext) printf("N %s\n", d->snext);
		if (d->parent) printf("P %s\n", d->parent->rev);
		if (d->kid) printf("K %s\n", d->kid->rev);
		if (0 && d->comments) {
			printf("C ");
			for (p = d->comments; *p; p++) {
				putchar(*p);
				if ((*p == '\n') && p[1]) {
					putchar('C');
					putchar(' ');
				}
			}
		}
		for (sym = rcs->symbols; sym; sym = sym->next) {
			unless (streq(sym->rev, d->rev)) continue;
			printf("S %s\n", sym->name);
		}
	}
	printf("tree = %s\n", rcs->tree->rev);
}

private	void
rcs_meta(RCS *r)
{
	fprintf(stderr, "%d deltas\n", r->n);
	if (r->defbranch) fprintf(stderr, "default branch: %s\n", r->defbranch);
	if (r->text) fprintf(stderr, "Text: %s\n", r->text);
}

/*
 * Eat a log entry and the text.
 */
private	int
eatlog(RCS *rcs, MMAP *m)
{
	rdelta	*d;
	char	*p, *t;
	int	l = 0;
	char	buf[MAXLINE];
	FILE	*log = 0;

	skip_white(m);
	unless (p = mwhere(m)) return (0);
	unless (isdigit(*p)) {
err:		fprintf(stderr, "EOF in log? file=%s\n", rcs->rcsfile);
		exit(1);
	}

	for (t = buf;
	    (p < m->end) && (isdigit(*p) || (*p == '.')); *t++ = *p++);
	if (p >= m->end) goto err;
	*t = 0;
	m->where = p;

	/*
	 * find the delta
	 */
	for (d = rcs->table; d && !streq(d->rev, buf); d = d->next);
	unless (d) {
		fprintf(stderr, "rcsparse: The RCS file %s appears\n"
		    "to be corrupted.  The deltatext for rev %s cannot be\n"
		    "found, but is described in the delta table.\n",
		    rcs->rcsfile, buf);
		exit(1);
	}

	skip_white(m);
	unless (p = mwhere(m)) goto err;
	unless (strneq(p, "log", 3)) goto err;
	m->where += 3;
	skip_white(m);
	unless (p = mwhere(m)) goto err;
	unless (*p++ == '@') goto err;
	log = fmem();
	for (t = buf; p < m->end; p++) {
		if ((*p == '@') && (p[1] != '@')) {
			unless (t == buf || t[-1] == '\n') *t++ = '\n';
			*t = 0;
			fputs(buf, log);
			d->comments = fmem_dup(log, 0);
			ftrunc(log, 0);
			m->where = p + 1;
			break;
		}
		if ((++l < 1023) && (*p != '\r')) *t++ = *p;
		if (*p == '\n') {
			l = 0;
			*t = 0;
			fputs(buf, log);
			t = buf;
		}
		if (l == 1023) {
			fprintf(stderr,
			    "%s: Truncated log line to 1024 bytes\n",
			    rcs->rcsfile);
		}
		if (*p == '@') p++;	/* unquote it */
	}
	fclose(log);

	unless (advance(m, '@')) goto err;
	for (;;) {
		unless (advance(m, '@')) goto err;
		if (m->where >= m->end) return (0);
		if (*m->where != '@') break;
		m->where++;
	}
	return (1);
}


/*
 * Eat a delta, returning 1 if there are more, 0 if we hit the ;
 */
private	int
eatdelta(RCS *rcs, MMAP *m)
{
	rdelta	*d;
	char	*p, *t;
	char	buf[4096];
	char	*state;

	skip_white(m);
	unless (p = mwhere(m)) {
err:		perror("EOF in delta?");
		exit(1);
	}
	rcsdebug((stderr, "DELTA %.10s\n", p));
	unless (isdigit(*p)) return (0);

	d = new(rdelta);
	rcs->n++;
	for (t = buf;
	    (p < m->end) && (isdigit(*p) || (*p == '.')); *t++ = *p++);
	if (p >= m->end) goto err;
	*t = 0;
	d->rev = strdup(buf);
	m->where = p;
	skip_white(m);
	unless (p = mwhere(m)) goto err;
	unless (strneq(p, "date", 4)) goto err;
	m->where += 4;
	skip_white(m);
	unless (p = mwhere(m)) goto err;

	for (t = buf; (p < m->end) && !isspace(*p) && (*p != ';'); *t++ = *p++);
	if (p >= m->end) goto err;
	*t = 0;
	d->sdate = strdup(buf);
	/*
	 * Make sure the date and the time have a space between them.
	 * 99.12.1.10.43.55
	 * 2001.12.01.10.43.55
	 */
	for (t = d->sdate; isdigit(*t); t++);	/* t -> first . */
	for (t++; isdigit(*t); t++);		/* t -> second . */
	for (t++; isdigit(*t); t++);		/* t -> third . */
	*t = ' ';

	m->where = p;
	advance(m, ';');
	skip_white(m);
	unless (p = mwhere(m)) goto err;
	unless (strneq(p, "author", 6)) goto err;
	m->where += 6;
	skip_white(m);
	unless (p = mwhere(m)) goto err;

	for (t = buf; (p < m->end) && !isspace(*p) && (*p != ';'); *t++ = *p++);
	if (p >= m->end) goto err;
	*t = 0;
	d->author = strdup(buf);
	m->where = p;
	skip_white(m);
	advance(m, ';');

	/* record deleted state */
	skip_white(m);
	expect(m, "state ");
	state = dup_tochar(m, ';');
	if (streq(state, "dead")) d->dead = 1;
	free(state);

	/* skip branches, it's redundant */
	skip_white(m);
	advance(m, ';');

	/* next */
	skip_white(m);
	unless (p = mwhere(m)) goto err;
	unless (strneq(p, "next", 4)) goto err;
	m->where += 4;
	skip_white(m);
	unless (p = mwhere(m)) goto err;
	for (t = buf; (p < m->end) && !isspace(*p) && (*p != ';'); *t++ = *p++);
	if (t != buf) {
		*t = 0;
		d->snext = strdup(buf);
	}
	m->where = p;
	skip_white(m);
	advance(m, ';');
	skip_white(m);

	/* drop unused RCS data on the floor */
	while (strneq("commitid", m->where, 8) ||
	    strneq("owner", m->where, 5) ||
	    strneq("group", m->where, 5) ||
	    strneq("permissions", m->where, 11) ||
	    strneq("hardlinks", m->where, 9)) {
		advance(m, ';');
		skip_white(m);
	}

	rcsdebug((stderr, "DELTA_END %.10s\n", mwhere(m)));

	if (d->next = rcs->table) rcs->table->prev = d;
	rcs->table = d;

	return (1);
}

/*
 * Eat a symbol, returning 1 if there are more, 0 if we hit the ;
 */
private	int
eatsym(RCS *rcs, MMAP *m)
{
	sym	*s;
	char	*p, *t;
	char	buf[4096];
	char	rev[1024];


	skip_white(m);
	unless (p = mwhere(m)) {
err:		perror("EOF in symbols?");
		exit(1);
	}
	if (*p == ';') goto done;

	for (t = buf; (p < m->end) && !isspace(*p) && (*p != ':'); *t++ = *p++);
	if (p >= m->end) goto err;
	*t = 0;
	while ((p < m->end) && (*p != ':')) p++;
	p++;
	if (p >= m->end) goto err;
	while ((p < m->end) && isspace(*p)) p++;
	if (p >= m->end) goto err;
	for (t = rev;
	    (p < m->end) && (isdigit(*p) || (*p == '.')); *t++ = *p++);
	if (p >= m->end) goto err;
	*t = 0;
	s = new(sym);
	s->name = strdup(buf);
	s->rev = strdup(rev);
	s->next = rcs->symbols;
	rcs->symbols = s;

	/*
	 * Handle CVS branch tags
	 * If you find a tag for a rev of the form X.Y.0.Z, then
	 * create a new tag for <TAG>_BASE at X.Y
	 * This is so that findcset doesn't put anything else in with the
	 * delta that is the base of a branch.
	 */
	t = strrchr(rev, '.');
	if (t > rev + 4 && t[-2] == '.' && t[-1] == '0') {
		t[-2] = 0;
		strcat(buf, "_BASE");

		s = new(sym);
		s->name = strdup(buf);
		s->rev = strdup(rev);
		s->next = rcs->symbols;
		rcs->symbols = s;
	}

	while ((p < m->end) && isspace(*p)) p++;
	if (p >= m->end) goto err;
	if (*p == ';') {
done:		m->where = p + 1;
		return (0);
	}
	m->where = p;
	return (1);
}

void
rcs_free(RCS *r)
{
	rdelta	*d, *next;
	sym	*s, *snext;

	for (d = r->table; d; d = next) {
		next = d->next;
		if (d->comments) free(d->comments);
		if (d->snext) free(d->snext);
		if (d->sccsrev) free(d->sccsrev);
		free(d->rev);
		free(d->sdate);
		free(d->author);
		free(d);
	}
	for (s = r->symbols; s; s = snext) {
		snext = s->next;
		free(s->name);
		free(s->rev);
		free(s);
	}
	if (r->defbranch) free(r->defbranch);
	if (r->text) free(r->text);
	free(r->kk);
	free(r->rcsfile);
	free(r->workfile);
	free(r->rootkey);
	free(r);
}

