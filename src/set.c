#include "system.h"
#include "sccs.h"
#include "range.h"

/*
 * set - set operations for BitKeeper
 *
 * union:		bk set -o <set A> <set B> [file]
 * intersection:	bk set -a <set A> <set B> [file]
 * difference:		bk set -d <set A> <set B> [file]
 * symmetric diff (xor)	bk set -x <set A> <set B> [file]
 * element:		bk set -e <rev> <set B> [file]
 * list:		bk set -l <rev> [file]
 *
 * Specifying sets:
 *	-r<rev|key|tag> means the set of elements implied by that revision
 *	"-" with a list of rev|key|tags on stdin, one per line, means
 *	the set is the specified list and nothing else.
 *	Each revision may be specified as a key, rev, and/or tag.
 *	The first four operators accept exactly 2 sets.  You can make up
 *	more complicated operators by daisy chaining the commands.  If it
 *	turns out that we need to use temp files to store two derived sets,
 *	then we'll add -f<temp file> -f<temp file2>.
 *	The default file is the ChangeSet file if not specified.
 *
 * output format:
 *	default	list all answers as revisions, not as tags.
 *	-k	list all answers as keys, not as tags or revisions.
 *	-t	list all answers as tags where possible, else revs.
 *	-tt	list only those answers which are tagged; list tag names.
 *	-tr	as above, but list revisions instead of tags.
 *	-tk	as above, but list keys instead of tags.
 *	-n	prefix output with the filename, i.e., ChangeSet|1.3
 */

private void	usage(int op);
private void	print(sccs *s, delta *d);
private	ser_t	*getset(sccs *s, char *rev);
private void	set_list(sccs *s, char *rev);
private void	set_member(sccs *s, char *rev, ser_t *map);
private void	set_diff(sccs *s, ser_t *a, ser_t *b);
private void	set_and(sccs *s, ser_t *a, ser_t *b);
private void	set_or(sccs *s, ser_t *a, ser_t *b);
private void	set_xor(sccs *s, ser_t *a, ser_t *b);

enum {
	AND,		/* intersection:		A & B */
	AND_NOT,	/* difference:			A & ~B */
	OR,		/* union:			A | B */
	XOR,		/* symmetric difference:	A ^ B */
	ELEMENT,	/* member:			A & M; M is element */
	LIST,		/* list sets which contain rev */
};

enum {
	REV,
	KEY,
	TAG,
};

private	struct {
	u32	op;		/* what are we doing */
	u32	format;		/* REV, KEY, etc */
	u32	tags:1;		/* list only tags */
	u32	name:1;		/* prefix the name */
	u32	read_stdin:1;	/* one set is there */
} opts;

int
set_main(int ac, char **av)
{
	sccs	*s;
	int	c;
	char	*name, *r1 = 0, *r2 = 0;

	if (ac == 2 && streq("--help", av[1])) {
		system("bk help set");
		return (0);
	}
	bzero(&opts, sizeof(opts));
	opts.format = REV;
	while ((c = getopt(ac, av, "adeklnor;t|x")) != -1) {
		switch (c) {
		    case 'a': if (opts.op) usage(1); opts.op = AND; break;
		    case 'd': if (opts.op) usage(1); opts.op = AND_NOT; break;
		    case 'e': if (opts.op) usage(1); opts.op = ELEMENT; break;
		    case 'l': if (opts.op) usage(1); opts.op = LIST; break;
		    case 'o': if (opts.op) usage(1); opts.op = OR; break;
		    case 'x': if (opts.op) usage(1); opts.op = XOR; break;
		    case 'k': opts.format = KEY; break;
		    case 'n': opts.name = 1; break;
		    case 'r':
			unless (r1) {
				r1 = optarg;
			} else unless (r2) {
				r2 = optarg;
			} else {
				usage(0);
			}
			break;
		    case 't': 
			unless (optarg) {
				opts.format = TAG;
				break;
			}
			switch (optarg[0]) {
			    case 'r': opts.tags = 1; opts.format = REV; break;
			    case 'k': opts.tags = 1; opts.format = KEY; break;
			    case 't': opts.tags = 1; opts.format = TAG; break;
			    default: usage(0);
			}
			break;
		    default: usage(0);
		}
	}
	if (opts.name && (opts.format == TAG)) {
		fprintf(stderr, "set: -n must be rev|key output, not tags.\n");
		usage(0);
	}
	if (r2 && streq(r2, "-")) {
		opts.read_stdin = 1;
		r2 = 0;
	}
	if (av[optind]) {
		name = name2sccs(av[optind]);
		s = sccs_init(name, INIT_NOCKSUM, 0);
		free(name);
	} else {
		char	cset[] = CHANGESET;

		if (sccs_cd2root(0, 0)) {
			fprintf(stderr, "set: cannot find project root\n");
			exit(1);
		}
		s = sccs_init(cset, INIT_NOCKSUM, 0);
	}
	unless (s && HASGRAPH(s)) {
		perror(av[optind] ? av[optind] : CHANGESET);
		if (s) sccs_free(s);
		exit(1);
	}
	switch (opts.op) {
	    case AND:
		set_and(s, getset(s, r1), getset(s, r2)); break;
	    case AND_NOT:
		set_diff(s, getset(s, r2), getset(s, r1)); break;
	    case OR:
		set_or(s, getset(s, r1), getset(s, r2)); break;
	    case XOR:
		set_xor(s, getset(s, r1), getset(s, r2)); break;
	    case ELEMENT:
		set_member(s, r1, getset(s, r2)); break;
	    case LIST:
		set_list(s, r1); break;
	}
	sccs_free(s);
	exit(0);
}

private void
usage(int op)
{
	if (op) fprintf(stderr, "set: only one operation allowed.\n");
	system("bk help -s set");
	exit(1);
}

private	ser_t*
stdin_set(sccs *s)
{
	delta	*d;
	ser_t	*map;
	char	buf[MAXKEY];

	map = calloc(s->nextserial, sizeof(ser_t));
	while (fnext(buf, stdin)) {
		chop(buf);
		unless (d = sccs_getrev(s, buf, 0, 0)) {
			fprintf(stderr,
			    "set: cannot find %s in %s\n", buf, s->gfile);
			sccs_free(s);
			exit(1);
		}
		map[d->serial] = 1;
	}
	return (map);
}

private	ser_t*
getset(sccs *s, char *rev)
{
	delta	*d;
	ser_t	*map;
	int	i;

	unless (rev) return (stdin_set(s));

	unless (d = sccs_getrev(s, rev, 0, 0)) {
		fprintf(stderr, "set: cannot find %s in %s\n", rev, s->gfile);
		sccs_free(s);
		exit(1);
	}
	map = sccs_set(s, d, 0, 0);
	for (i = 0; i < s->nextserial; ++i) {
		map[i] &= 1;
	}
	return (map);
}

/*
 * List elements in A but not in B.
 */
private void
set_diff(sccs *s, ser_t *a, ser_t *b)
{
	int	i;
	delta	*d;

	for (i = 1; i < s->nextserial; ++i) {
		unless (a[i] && !b[i]) continue;
		d = sfind(s, i);
		assert(d);
		unless (d->type == 'D') continue;
		if (opts.tags && !(d->flags & D_SYMBOLS)) continue;
		print(s, d);
	}
	free(a);
	free(b);
}

/*
 * List elements in A and B.
 */
private void
set_and(sccs *s, ser_t *a, ser_t *b)
{
	int	i;
	delta	*d;

	for (i = 1; i < s->nextserial; ++i) {
		unless (a[i] && b[i]) continue;
		d = sfind(s, i);
		assert(d);
		unless (d->type == 'D') continue;
		if (opts.tags && !(d->flags & D_SYMBOLS)) continue;
		print(s, d);
	}
	free(a);
	free(b);
}

/*
 * List elements in A or B.
 */
private void
set_or(sccs *s, ser_t *a, ser_t *b)
{
	int	i;
	delta	*d;

	for (i = 1; i < s->nextserial; ++i) {
		unless (a[i] || b[i]) continue;
		d = sfind(s, i);
		assert(d);
		unless (d->type == 'D') continue;
		if (opts.tags && !(d->flags & D_SYMBOLS)) continue;
		print(s, d);
	}
	free(a);
	free(b);
}

/*
 * List A xor B.
 */
private void
set_xor(sccs *s, ser_t *a, ser_t *b)
{
	int	i;
	delta	*d;

	for (i = 1; i < s->nextserial; ++i) {
		unless (a[i] ^ b[i]) continue;
		d = sfind(s, i);
		assert(d);
		unless (d->type == 'D') continue;
		if (opts.tags && !(d->flags & D_SYMBOLS)) continue;
		print(s, d);
	}
	free(a);
	free(b);
}


/*
 * If rev is a member of the set, print it.
 */
private void
set_member(sccs *s, char *rev, ser_t *map)
{
	delta	*d;

	unless (d = sccs_getrev(s, rev, 0, 0)) {
		fprintf(stderr, "set: cannot find %s in %s\n", rev, s->gfile);
		sccs_free(s);
		exit(1);
	}
	unless (map[d->serial] == 1) {
		free(map);
		return;
	}
	print(s, d);
}

/*
 * Print all revs which contain rev
 */
private void
set_list(sccs *s, char *rev)
{
	delta	*d, *e;
	ser_t	*map;

	unless (d = sccs_getrev(s, rev, 0, 0)) {
		fprintf(stderr, "set: cannot find %s in %s\n", rev, s->gfile);
		sccs_free(s);
		exit(1);
	}
	for (e = s->table; e; e = e->next) {
		unless (e->type == 'D') continue;
		if (e->flags & D_SET) continue;
		if (opts.tags && !(e->flags & D_SYMBOLS)) continue;
		map = sccs_set(s, e, 0, 0);
		unless (map[d->serial] == 1) {
			free(map);
			if (e == d) break;
			continue;
		}
		print(s, e);
		free(map);
		if (e == d) break;
	}
}

private void
print(sccs *s, delta *d)
{
	if (opts.name) {
		assert(opts.format != TAG);
		printf("%s|", s->gfile);
	}
	switch (opts.format) {
	    case TAG:
		if (d->flags & D_SYMBOLS) {
			symbol	*sym;

			for (sym = s->symbols; sym; sym = sym->next) {
				unless (sym->d == d) continue;
				printf("%s\n", sym->symname);
			}
			break;
		}
		/* fall through */
	    case REV: printf("%s\n", d->rev); break;
	    case KEY: sccs_pdelta(s, d, stdout); printf("\n"); break;
	}
}
