/* Copyright (c) 1997 L.W.McVoy */
#include "system.h"
#include "sccs.h"
WHATSTR("@(#)%K%");
char	*rmdel_help = "\n\
usage: rmdel [-q] [-r<rev>] file\n\n\
    -q		run quietly\n\
    -r<r>	rmdel revision <r>\n\n";

void	rmcaches(void);

/*
 * The weird setup is so that I can #include this file into sccssh.c
 */
int
rmdel_main(int ac, char **av, char *out)
{
	sccs	*s;
	int	c, flags = 0;
	char	*name, *rev = 0;
	delta	*d, *e;

	debug_main(av);
	if (ac == 2 && streq("--help", av[1])) {
		fprintf(stderr, rmdel_help);
		return (1);
	}
	while ((c = getopt(ac, av, "qr;")) != -1) {
		switch (c) {
		    case 'q': flags |= SILENT; break;
		    case 'r': rev = optarg; break;
		    default:
			fprintf(stderr,
			    "rmdel: usage error, try rmdel --help\n");
			return (1);
		}
	}

	/*
	 * Too dangerous to do autoexpand.
	 * XXX - might want to insist that there is only one file.
	 */
	unless (name = sfileFirst("rmdel", &av[optind], SF_NODIREXPAND)) {
		return (0);
	}
	if (sfileNext()) {
		fprintf(stderr, "rmdel: only one file at a time\n");
		return (1);
	}

	unless ((s = sccs_init(name, flags, 0)) && s->tree) {
		fprintf(stderr, "rmdel: can't init %s\n", name);
		return (1);
	}
	
	name = rev ? rev : sfileRev();
	unless (d = sccs_getrev(s, name, 0, 0)) {
		fprintf(stderr, "rmdel: can't find %s%c%s\n", s->gfile, BK_FS, name);
err:		sccs_free(s);
		sfileDone();
		purify_list();
		return (1);
	}

	for (e = d; e; e = e->kid) {
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
	purify_list();
	return (0);
}

int
main(int ac, char **av)
{
	return (rmdel_main(ac, av, "-"));
}
