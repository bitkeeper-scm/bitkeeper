/*
 * resolve_generic.c - support code for the resolver
 */
#include "resolve.h"

/*
 * Set up for a resolver.
 */
resolve	*
resolve_init(opts *opts, sccs *s)
{
	resolve	*rs = calloc(1, sizeof(*rs));
	char	buf[MAXKEY];

	rs->opts = opts;
	rs->s = s;
	sccs_sdelta(rs->s, sccs_ino(rs->s), buf);
	rs->key = strdup(buf);
	rs->d = sccs_top(s);
	assert(rs->d);	/* XXX: not all files have a 1.0 .  What to do? */
	rs->dname = sccs_setpathname(s);
	s->spathname = 0;  /* storing name in dname, so free this */
	if (rs->snames = getnames(sccs_Xfile(rs->s, 'm'), 'm')) {
		rs->gnames         = calloc(1, sizeof(names));
		rs->gnames->local  = sccs2name(rs->snames->local);
		rs->gnames->gca    = sccs2name(rs->snames->gca);
		rs->gnames->remote = sccs2name(rs->snames->remote);
	}
	rs->revs = getnames(sccs_Xfile(rs->s, 'r'), 'r');
	unless (rs->pager = getenv("PAGER")) rs->pager = PAGER;
	unless (rs->editor = getenv("EDITOR")) rs->editor = "vi";
	if (opts->debug) {
		fprintf(stderr, "rs_init(%s)", rs->d->pathname);
		if (rs->snames) fprintf(stderr, " snames");
		if (rs->revs) fprintf(stderr, " revs");
		fprintf(stderr, "\n");
	}
	return (rs);
}

private void
pnames(char *prefix, names *n)
{
	if (n->local) printf("%s local: %s\n", prefix, n->local);
	if (n->gca) printf("%s gca: %s\n", prefix, n->gca);
	if (n->remote) printf("%s remote: %s\n", prefix, n->remote);
}

/*
 * Debugging, dump everything that we can.
 */
void
resolve_dump(resolve *rs)
{
	printf("=== resolve dump of 0x%x ===\n", rs);
	if (rs->prompt) printf("prompt: %s\n", rs->prompt);
	if (rs->s) printf("sfile: %s\n", rs->s->sfile);
	if (rs->key) printf("key: %s\n", rs->key);
	if (rs->d) printf("delta: %s\n", rs->d->rev);
	if (rs->dname) printf("dname: %s\n", rs->dname);
	if (rs->revs) pnames("revs", rs->revs);
	if (rs->rnames) pnames("rnames", rs->rnames);
	if (rs->gnames) pnames("gnames", rs->gnames);
	if (rs->snames) pnames("snames", rs->snames);
	if (rs->tnames) pnames("tnames", rs->tnames);
	printf("n: %d\n", rs->n);
	if (rs->opaque) printf("opaque: %x\n", rs->opaque);
	if (rs->res_gcreate) printf("gcreate conflict\n");
	if (rs->res_screate) printf("screate conflict\n");
	if (rs->res_dirfile) printf("dirfile conflict\n");
	if (rs->res_resync) printf("resync conflict\n");
	if (rs->res_contents) printf("contents conflict\n");
}

void
resolve_free(resolve *rs)
{
	if (rs->gnames) freenames(rs->gnames, 1);
	if (rs->snames) freenames(rs->snames, 1);
	if (rs->revs) freenames(rs->revs, 1);
	free(rs->dname);
	sccs_free(rs->s);
	free(rs->key);
	bzero(rs, sizeof(*rs));
	free(rs);
}

int
resolve_loop(char *name, resolve *rs, rfuncs *rf)
{
	int	i, ret, bang = -1;
	char	buf[100];

	rs->funcs = rf;
	if (rs->opts->debug) {
		fprintf(stderr, "%s\n", name);
		fprintf(stderr, "\tsccs: %s\n", rs->s->gfile);
		fprintf(stderr, "\ttot: %s@%s\n", rs->d->pathname, rs->d->rev);
	}
	rs->n = 0;
	for (i = 0; rf[i].spec && !streq(rf[i].spec, "!"); i++);
	if (rf[i].spec) bang = i;
	while (1) {
		fprintf(stderr, "(%s) %s>> ", name, rs->prompt);
		getline(0, buf, sizeof(buf));
		unless (buf[0]) strcpy(buf, "?");
		if (streq(buf, "dump")) {
			resolve_dump(rs);
			continue;
		}
		if (streq(buf, "debug")) {
			rs->opts->debug = !rs->opts->debug;
			continue;
		}

again:		/* 100 tries for the same file means we're hosed.  */
		if (++rs->n == 100) return (ELOOP);
		for (i = 0; rf[i].spec && !streq(rf[i].spec, buf); i++);
		if (!rf[i].spec && (buf[0] == '!') && (bang >= 0)) {
			i = bang;
			rs->shell = &buf[1];
		} else {
			rs->shell = 0;
		}
		unless (rf[i].spec) {
			strcpy(buf, "?");
			goto again;
		}
		if (rs->opts->debug) {
			fprintf(stderr,
			    "[%s] Calling %s on %s\n", buf, rf[i].name, name);
		}
		if (ret = rf[i].func(rs)) {
			if (rs->opts->debug) {
				fprintf(stderr,
				    "%s returns %d\n", rf[i].name, ret);
			}
			return (ret);
		}
		if (rs->opts->debug) {
			fprintf(stderr, "%s returns 0\n", rf[i].name);
		}
	}
}
