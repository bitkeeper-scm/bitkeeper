//#define	RCS_DEBUG
#include "rcs.h"

private	int	eatlog(RCS *rcs, MMAP *m);
private	int	eatdelta(RCS *rcs, MMAP *m);
private	int	eatsym(RCS *rcs, MMAP *m);
private	void	mkgraph(RCS *rcs);
private	void	dates(RCS *rcs);
private	void	doit(char *f);
private	void	rcs_table(RCS *rcs);
private	void	rcs_meta(RCS *rcs);

int
rcsparse_main(int ac, char **av)
{
	int	i;
	char	buf[MAXPATH];

	if (ac == 2 && streq("--help", av[1])) {
		system("bk help rcsparse");
		return (0);
	}

	if (av[1] && streq("-", av[1]) && !av[2]) {
		while (fgets(buf, sizeof(buf), stdin)) {
			chop(buf);
			doit(buf);
		}
	} else {
		for (i = 1; av[i]; ++i) {
			doit(av[i]);
		}
	}
	fputc('\n', stderr);
	return (0);
}

private	void
doit(char *f)
{
	RCS	*r;
	int	i;
	static	int len = 0;
	static	int n = 0;

	if (1) {
		fputc('\r', stderr);
		fprintf(stderr, "%d ", ++n);
		fprintf(stderr, "%s", f);
		for (i = strlen(f); i < len; i++) fputc(' ', stderr);
		len = strlen(f);
	}
	if (r = rcs_init(f)) {
		printf("== %s ==\n", r->file);
		rcs_meta(r);
		//rcs_branch(r);
		//rcs_prs(r);
		rcs_table(r);
		rcs_free(r);
	}
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

RCS	*
rcs_init(char *file)
{
	MMAP	*m;
	char	*p, *t;
	char	buf[32<<10];
	RCS	*rcs;

	unless ((t = strrchr(file, ',')) && (t[1] == 'v') && !t[2]) {
		fprintf(stderr, "Not an RCS file '%s'\n", file);
		return (0);
	}
	unless (m = mopen(file, "b")) {
		fprintf(stderr, "Cannot mmap %s\n", file);
err:		perror(file);
		exit(1);
	}

	new(rcs);
	rcs->file = strdup(file);
	rcs->kk = "-kk";

	/* head */
	skip_white(m);
	unless (p = mwhere(m)) goto err;
	unless (strneq("head", p, 4)) goto err;
	unless (advance(m, ';')) goto err;
	skip_white(m);
	
	/* branch? */
	unless (p = mwhere(m)) goto err;
	if (strneq(p, "branch", 6)) {
		m->where += 6;
		skip_white(m);
		unless (p = mwhere(m)) goto err;
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
	unless (p = mwhere(m)) goto err;
	unless (strneq("access", p, 6)) goto err;
	unless (advance(m, ';')) goto err;
	skip_white(m);

	/* symbols */
	unless (p = mwhere(m)) goto err;
	unless (strneq("symbols", p, 7)) goto err;
	m->where += 7;
	while (eatsym(rcs, m));

	/* skip locks */
	skip_white(m);
	unless (p = mwhere(m)) goto err;
	unless (strneq("locks", p, 5)) goto err;
	unless (advance(m, ';')) goto err;

	skip_white(m);
	unless (p = mwhere(m)) goto err;
	if (strneq("strict", p, 6)) {
		unless (advance(m, ';')) goto err;
		skip_white(m);
		unless (p = mwhere(m)) goto err;
	}

	/* skip everything until we get a number */
	skip_white(m);
	unless (p = mwhere(m)) goto err;
	if (strneq(p, "comment", 7)) {
		unless (advance(m, '@')) goto err;
		for ( ;; ) {
			unless (advance(m, '@')) goto err;
			if (*m->where != '@') break;
			m->where++;
		}
		unless (advance(m, ';')) goto err;
	}
	skip_white(m);
	unless (p = mwhere(m)) goto err;

	if (strneq(p, "expand", 6)) {
		unless (advance(m, '@')) goto err;
		skip_white(m);
		if (strneq(m->where, "b@", 2)) {
			rcs->kk = "-kb";
		} else if (strneq(m->where, "v@", 2)) {
			rcs->kk = "-kv";
		} else if (strneq(m->where, "kv@", 3)) {
			rcs->kk = "-kkv";
		} else if (strneq(m->where, "kvl@", 4)) {
			rcs->kk = "-kkvl";
		} else if (strneq(m->where, "k@", 2)) {
			rcs->kk = "-kk";
		} else if (strneq(m->where, "o@", 2)) {
			rcs->kk = "-ko";
		} else if (strneq(m->where, "v@", 2)) {
			rcs->kk = "-kv";
		} else {
			fprintf(stderr,
			    "\n!!! Warning: unknown expand statement in %s\n",
			    file);
		}
		unless (advance(m, '@')) goto err;
		unless (advance(m, ';')) goto err;
	}
	skip_white(m);
	unless (p = mwhere(m)) goto err;

	while (eatdelta(rcs, m));

	/* MKS has extended stuff */
	unless (p = mwhere(m)) goto err;
	if (strneq(p, "ext", 3)) {
		unless (advance(m, '@')) goto err;
		unless (advance(m, '@')) goto err;
		skip_white(m);
	}

	/* desc */
	unless (p = mwhere(m)) goto err;
	unless (strneq(p, "desc", 4)) goto err;
	unless (advance(m, '@')) goto err;
	unless (p = mwhere(m)) goto err;
	for (t = buf; p < m->end; p++) {
		if ((*p == '@') && (p[1] != '@')) {
			*t = 0;
			rcs->text = strdup(buf);
			m->where = p + 1;
			break;
		} else {
			*t++ = *p;
			if (*p == '@') p++;	/* unquote it */
		}
	}

	while (eatlog(rcs, m));
	mclose(m);
	mkgraph(rcs);
	dates(rcs);
	return (rcs);
}

rdelta	*
rcs_findit(RCS *rcs, char *rev)
{
	rdelta	*d;

	for (d = rcs->table; d && !streq(rev, d->rev); d = d->next);
	return (d);
}

rdelta	*
rcs_defbranch(RCS *rcs)
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
mkgraph(RCS *rcs)
{
	rdelta	*d, *k, *p, *start;
	int	n = rcs->n;
	char	*t;
	char	buf[1024];

	for (d = rcs->table; d && d->next; d = d->next);
	while (d->snext) d = d->prev;
	assert(d);
	rcs->tree = d;
	d->head = 1;
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
		d->head = 1;
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

private void
rcs_branch(RCS *rcs)
{
	rdelta	*d;

	for (d = rcs_defbranch(rcs); d; d = d->kid) {
		printf("%s ", d->rev);
	}
	printf("\n");
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
			if (d->head && !d->printed) break;
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
		d->printed = 1;
		d->date = sccs_date2time(d->sdate, "0:00");
		if (d->parent && (d->parent->date >= d->date)) {
			d->dateFudge = (d->parent->date - d->date) + 1;
			d->date += d->dateFudge;
		}
	}
	while (n) {
		for (d = rcs->table; d; d = d->next) {
			if (d->head && !d->printed) break;
		}
		while (d) {
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

	for (d = rcs->table; d; d = d->next) {
		assert(d->rev);
		assert(d->author);
		printf("D %s %s %s\n", d->rev, d->sdate, d->author);
		//if (d->snext) printf("N %s\n", d->snext);
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
	}
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
	char	buf[32<<10];
	int	l = 0;

	skip_white(m);
	unless (p = mwhere(m)) return (0);
	unless (isdigit(*p)) {
err:		perror("EOF in log?");
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
	assert(d);

	skip_white(m);
	unless (p = mwhere(m)) goto err;
	unless (strneq(p, "log", 3)) goto err;
	m->where += 3;
	skip_white(m);
	unless (p = mwhere(m)) goto err;
	unless (*p++ == '@') goto err;
	for (t = buf; p < m->end; p++) {
		if ((*p == '@') && (p[1] != '@')) {
			unless (t[-1] == '\n') *t++ = '\n';
			*t = 0;
			d->comments = strdup(buf);
			m->where = p + 1;
			break;
		} 
		if ((++l < 1023) && (*p != '\r')) *t++ = *p;
		if (*p == '@') p++;	/* unquote it */
	}
	if (l >= 1023) {
		fprintf(stderr,
		    "%s: Truncated log message line to 1024 bytes\n",
		    rcs->file);
	}

	unless (advance(m, '@')) goto err;
	for ( ;; ) {
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

	skip_white(m);
	unless (p = mwhere(m)) {
err:		perror("EOF in delta?");
		exit(1);
	}
	rcsdebug((stderr, "DELTA %.10s\n", p));
	unless (isdigit(*p)) return (0);

	new(d);
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

	/* skip state */
	skip_white(m);
	advance(m, ';');

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
	char	sym[4096];
	char	rev[1024];


	skip_white(m);
	unless (p = mwhere(m)) {
err:		perror("EOF in symbols?");
		exit(1);
	}
	if (*p == ';') goto done;
		
	for (t = sym; (p < m->end) && !isspace(*p) && (*p != ':'); *t++ = *p++);
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
	new(s);
	s->name = strdup(sym);
	s->rev = strdup(rev);
	s->next = rcs->symbols;
	rcs->symbols = s;

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
	free(r->file);
	free(r);
}
