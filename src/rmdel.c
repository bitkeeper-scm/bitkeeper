/* Copyright (c) 1997 L.W.McVoy */
#include "system.h"
#include "sccs.h"
WHATSTR("@(#)%K%");
char	*rmdel_help = "\n\
usage: rmdel [-qs] [-r<rev>] file\n\n\
    This command is not extended to minimize the removing\n\
    much information which can not be reclaimed.\n\n\
    -D		destroy all information newer than this revision\n\
    -q		run quietly\n\
    -r<r>	rmdel revision <r>\n\
    -s		run quietly\n\n";

/*
 * The weird setup is so that I can #include this file into sccssh.c
 */
int
rmdel_main(int ac, char **av, char *out)
{
	sccs	*s;
	int	c, flags = 0, errors = 0;
	char	*name, *rev = 0;
	int	destroy = 0;
	int	invalidate = 0;
	delta	*d;
	char	lastname[MAXPATH];

	debug_main(av);
	if (ac == 2 && streq("--help", av[1])) {
		fprintf(stderr, rmdel_help);
		return (1);
	}
	while ((c = getopt(ac, av, "Dqr;s")) != -1) {
		switch (c) {
		    case 'D': destroy = 1; break;
		    case 'q': flags |= SILENT; break;
		    case 'r': rev = optarg; break;
		    case 's': flags |= SILENT; break;

		    default:
			fprintf(stderr,
			    "rmdel: usage error, try rmdel --help\n");
			return (1);
		}
	}
	name = sfileFirst("rmdel", &av[optind], 0);
	while (name) {
		unless (s = sccs_init(name, flags)) {
			name = sfileNext();
			continue;
		}
		if (!s->tree) {
			if (!(s->state & S_SFILE)) {
				if (streq(lastname, s->sfile)) goto next;
				fprintf(stderr, "rmdel: %s doesn't exist.\n",
				    s->sfile);
			} else {
				perror(s->sfile);
			}
			goto next;
		}
		name = rev ? rev : sfileRev();
		unless (d = sccs_getrev(s, name, 0, 0)) {
			fprintf(stderr,
			    "rmdel: can't find %s:%s\n", s->gfile, name);
			goto next;
		}

		/*
		 * If they wanted to destroy the delta and it is the root
		 * delta, then blow the entire file away.
		 */
		if (destroy && (d == s->tree)) {
			if (sccs_clean(s, SILENT)) {
				fprintf(stderr,
				    "rmdel: can't remove edited %s\n",
				    s->gfile);
				errors = 1;
				goto next;
			}
			/* see ya! */
			verbose((stderr, "rmdel: remove %s\n", s->sfile));
			unlink(s->sfile);
			invalidate++;
			strcpy(lastname, s->sfile);
			goto next;
		}
		lastname[0] = 0;

		if (sccs_rmdel(s, d, destroy, flags)) {
			unless (BEEN_WARNED(s)) {
				fprintf(stderr,
				    "rmdel of %s failed, skipping it.\n", name);
			}
			errors = 1;
		}
next:		sccs_free(s);
		name = sfileNext();
	}
	sfileDone();
#ifndef	NOPURIFY
	purify_list();
#endif
	if (invalidate) rmcaches();
	return (errors);
}

rmcaches()
{
	// XXX - needs to be updated when we move the cache to BitKeeper/caches
	if (sccs_cd2root(0, 0) == 0) {
		unlink("SCCS/x.id_cache");
	}
}

int
main(int ac, char **av)
{
	return (rmdel_main(ac, av, "-"));
}
