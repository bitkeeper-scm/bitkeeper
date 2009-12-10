/* Copyright (c) 1997 L.W.McVoy */
#include "system.h"
#include "sccs.h"

int
rmdel_main(int ac, char **av)
{
	sccs	*s;
	int	c, flags = 0;
	char	*name, *rev = 0;
	delta	*d, *e;

	while ((c = getopt(ac, av, "qr;")) != -1) {
		switch (c) {
		    case 'q': flags |= SILENT; break;	/* doc 2.0 */
		    case 'r': rev = optarg; break;	/* doc 2.0 */
		    default:
			system("bk help -s rmdel");
			return (1);
		}
	}

	/*
	 * Too dangerous to do autoexpand.
	 * XXX - might want to insist that there is only one file.
	 */
	unless (name = sfileFirst("rmdel", &av[optind], SF_NODIREXPAND)) {
		return (1);
	}
	if (sfileNext()) {
		fprintf(stderr, "rmdel: only one file at a time\n");
		return (1);
	}

	unless ((s = sccs_init(name, flags)) && HASGRAPH(s)) {
		fprintf(stderr, "rmdel: can't init %s\n", name);
		return (1);
	}
	
	name = rev ? rev : sfileRev();
	unless (d = sccs_findrev(s, name)) {
		fprintf(stderr, "rmdel: can't find %s%c%s\n", s->gfile, BK_FS, name);
err:		sccs_free(s);
		sfileDone();
		return (1);
	}

	for (e = d; e; e = KID(e)) {
		if (e->flags & D_CSET) {
			fprintf(stderr,
			    "rmdel: can't remove committed delta %s:%s\n",
			    s->gfile, e->rev);
			goto err;
		}
	}

	if (sccs_clean(s, SILENT)) {
		fprintf(stderr,
		    "rmdel: can't operate on edited %s\n", s->gfile);
		goto err;
	}

	/*
	 * XXX - BitKeeper doesn't really support removed deltas.
	 * It does not propogate them.  What we should do is detect
	 * if we are in BK mode and switch to stripdel.
	 */
	if (sccs_rmdel(s, d, flags)) {
		unless (BEEN_WARNED(s)) {
			fprintf(stderr, "rmdel of %s failed.\n", name);
		}
		goto err;
	}
	sccs_free(s);
	sfileDone();
	return (0);
}
