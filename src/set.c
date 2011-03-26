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

private void	print(sccs *s, delta *d);

enum {
	AND,		/* intersection:		A & B */
	AND_NOT,	/* difference:			A & ~B */
	OR,		/* union:			A | B */
	XOR,		/* symmetric difference:	A ^ B */
	ELEMENT,	/* member:			A & M; M is element */
	LIST,		/* list sets which contain rev */
	SET,		/* list the revision as a set */
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

	bzero(&opts, sizeof(opts));
	opts.format = REV;
	while ((c = getopt(ac, av, "adeklnor;st|x", 0)) != -1) {
		switch (c) {
		    case 'a': if (opts.op) usage(); opts.op = AND; break;
		    case 'd': if (opts.op) usage(); opts.op = AND_NOT; break;
		    case 'e': if (opts.op) usage(); opts.op = ELEMENT; break;
		    case 'l': if (opts.op) usage(); opts.op = LIST; break;
		    case 'o': if (opts.op) usage(); opts.op = OR; break;
		    case 's': if (opts.op) usage(); opts.op = SET; break;
		    case 'x': if (opts.op) usage(); opts.op = XOR; break;
		    case 'k': opts.format = KEY; break;
		    case 'n': opts.name = 1; break;
		    case 'r':
			unless (r1) {
				r1 = optarg;
			} else unless (r2) {
				r2 = optarg;
			} else {
				usage();
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
			    default: usage();
			}
			break;
		    default: bk_badArg(c, av);
		}
	}
	if (opts.name && (opts.format == TAG)) {
		fprintf(stderr, "set: -n must be rev|key output, not tags.\n");
		usage();
	}
	if (r2 && streq(r2, "-")) {
		opts.read_stdin = 1;
		r2 = 0;
	}
	if (av[optind]) {
		name = name2sccs(av[optind]);
		s = sccs_init(name, INIT_NOCKSUM);
		free(name);
	} else {
		char	cset[] = CHANGESET;

		if (proj_cd2root()) {
			fprintf(stderr, "set: cannot find project root\n");
			exit(1);
		}
		s = sccs_init(cset, INIT_NOCKSUM);
	}
	unless (s && HASGRAPH(s)) {
		perror(av[optind] ? av[optind] : CHANGESET);
		if (s) sccs_free(s);
		exit(1);
	}
	switch (opts.op) {
	    case AND:
		set_and(s, set_get(s, r1), set_get(s, r2), print); break;
	    case AND_NOT:
		set_diff(s, set_get(s, r2), set_get(s, r1), print); break;
	    case OR:
		set_or(s, set_get(s, r1), set_get(s, r2), print); break;
	    case XOR:
		set_xor(s, set_get(s, r1), set_get(s, r2), print); break;
	    case ELEMENT:
		set_member(s, r1, set_get(s, r2), print); break;
	    case LIST:
		set_list(s, r1, print); break;
	    case SET:
		set_set(s, r1, print); break;
	}
	sccs_free(s);
	exit(0);
}

private	u8 *
stdin_set(sccs *s)
{
	delta	*d;
	u8	*map;
	char	buf[MAXKEY];

	map = calloc(s->nextserial, sizeof(u8));
	while (fnext(buf, stdin)) {
		chop(buf);
		unless (d = sccs_findrev(s, buf)) {
			fprintf(stderr,
			    "set: cannot find %s in %s\n", buf, s->gfile);
			sccs_free(s);
			exit(1);
		}
		map[d->serial] = 1;
	}
	return (map);
}

u8 *
set_get(sccs *s, char *rev)
{
	delta	*d;
	u8	*map;
	int	i;

	unless (rev) return (stdin_set(s));

	unless (d = sccs_findrev(s, rev)) {
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
void
set_diff(sccs *s, u8 *a, u8 *b, set_pfunc p)
{
	int	i;
	delta	*d;

	for (i = 1; i < s->nextserial; ++i) {
		unless (a[i] && !b[i]) continue;
		d = sfind(s, i);
		assert(d);
		unless (d->type == 'D') continue;
		if (opts.tags && !(d->flags & D_SYMBOLS)) continue;
		p(s, d);
	}
	free(a);
	free(b);
}

/*
 * List elements in A and B.
 */
void
set_and(sccs *s, u8 *a, u8 *b, set_pfunc p)
{
	int	i;
	delta	*d;

	for (i = 1; i < s->nextserial; ++i) {
		unless (a[i] && b[i]) continue;
		d = sfind(s, i);
		assert(d);
		unless (d->type == 'D') continue;
		if (opts.tags && !(d->flags & D_SYMBOLS)) continue;
		p(s, d);
	}
	free(a);
	free(b);
}

/*
 * List elements in A or B.
 */
void
set_or(sccs *s, u8 *a, u8 *b, set_pfunc p)
{
	int	i;
	delta	*d;

	for (i = 1; i < s->nextserial; ++i) {
		unless (a[i] || b[i]) continue;
		d = sfind(s, i);
		assert(d);
		unless (d->type == 'D') continue;
		if (opts.tags && !(d->flags & D_SYMBOLS)) continue;
		p(s, d);
	}
	free(a);
	free(b);
}

/*
 * List A xor B.
 */
void
set_xor(sccs *s, u8 *a, u8 *b, set_pfunc p)
{
	int	i;
	delta	*d;

	for (i = 1; i < s->nextserial; ++i) {
		unless (a[i] ^ b[i]) continue;
		d = sfind(s, i);
		assert(d);
		unless (d->type == 'D') continue;
		if (opts.tags && !(d->flags & D_SYMBOLS)) continue;
		p(s, d);
	}
	free(a);
	free(b);
}


/*
 * If rev is a member of the set, print it.
 */
void
set_member(sccs *s, char *rev, u8 *map, set_pfunc p)
{
	delta	*d;

	unless (d = sccs_findrev(s, rev)) {
		fprintf(stderr, "set: cannot find %s in %s\n", rev, s->gfile);
		sccs_free(s);
		exit(1);
	}
	unless (map[d->serial] == 1) {
		free(map);
		return;
	}
	p(s, d);
}

/*
 * Print all revs which contain rev
 */
void
set_list(sccs *s, char *rev, set_pfunc p)
{
	delta	*d, *e;
	u8	*map;

	unless (d = sccs_findrev(s, rev)) {
		fprintf(stderr, "set: cannot find %s in %s\n", rev, s->gfile);
		sccs_free(s);
		exit(1);
	}
	for (e = s->table; e; e = NEXT(e)) {
		unless (e->type == 'D') continue;
		if (e->flags & D_SET) continue;
		if (opts.tags && !(e->flags & D_SYMBOLS)) continue;
		map = sccs_set(s, e, 0, 0);
		unless (map[d->serial] == 1) {
			free(map);
			if (e == d) break;
			continue;
		}
		p(s, e);
		free(map);
		if (e == d) break;
	}
}

/*
 * Print the set implied by the rev.
 */
void
set_set(sccs *s, char *rev, set_pfunc p)
{
	delta	*d;
	u8	*map;
	int	i;

	unless (d = sccs_findrev(s, rev)) {
		fprintf(stderr, "set: cannot find %s in %s\n", rev, s->gfile);
		sccs_free(s);
		exit(1);
	}
	map = sccs_set(s, d, 0, 0);
	for (i = 1; i < s->nextserial; ++i) {
		if (map[i]) p(s, sfind(s, i));
	}
	free(map);
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
				unless (sym->ser == d->serial) continue;
				printf("%s\n", sym->symname);
			}
			break;
		}
		/* fall through */
	    case REV: printf("%s\n", REV(s, d)); break;
	    case KEY: sccs_pdelta(s, d, stdout); printf("\n"); break;
	}
}
