/* Copyright (c) 1997-2000 L.W.McVoy */
#include "system.h"
#include "sccs.h"
WHATSTR("@(#)%K%");

#undef	BLOCK
#define	BLOCK	0x0001
#define	PLOCK	0x0002
#define	XLOCK	0x0004
#define	ZLOCK	0x0008
#define	RLOCK	0x0010
#define	STALE	0x0020
#define	WLOCK	0x0040

#define	REPO	(RLOCK|STALE|WLOCK)

private	int	doit(sccs *s, char which);
private int	repo(u32 flags);
private void	dont(sccs *s, char c);

int
unlock_main(int ac, char **av)
{
	char	*name;
	int	c, force = 0, flags = 0;
	sccs	*s = 0;
	project	*proj = 0;

	debug_main(av);
	if (ac > 1 && streq("--help", av[1])) {
		system("bk help unlock");
		return (0);
	}
	while ((c = getopt(ac, av, "bfprswxz")) != -1) {
		switch (c) {
		    case 'b': flags |= BLOCK; break;	/* doc 2.0 */
		    case 'f': force = 1; break;		/* doc 2.0 */
		    case 'p': flags |= PLOCK; break;	/* doc 2.0 */
		    case 'r': flags |= RLOCK; break;	/* doc 2.0 */
		    case 's': flags |= STALE; break;	/* doc 2.0 */
		    case 'w': flags |= WLOCK; break;	/* doc 2.0 */
		    case 'x': flags |= XLOCK; break;	/* doc 2.0 */
		    case 'z': flags |= ZLOCK; break;	/* doc 2.0 */
			break;
		    default:
usage:			system("bk help -s unlock");
			return (1);
		}
	}

	unless (flags) flags = PLOCK;
	
	if ((flags & REPO) && (flags & ~REPO)) {
		fprintf(stderr,
		    "unlock: may not mix file and repository unlocks.\n");
		goto usage;
	}
	
	if (flags & REPO) return (repo(flags));

	/*
	 * Too dangerous to unlock everything automagically,
	 * make 'em spell it out.
	 */
	unless (name = sfileFirst("unlock", &av[optind], SF_NODIREXPAND)) {
		fprintf(stderr,
		    "unlock: must have explicit list when discarding locks.\n");
		goto usage;
		return(1);
	}
	c = 0;
	while (name) {
		s = sccs_init(name, SILENT|INIT_NOCKSUM|INIT_SAVEPROJ, proj);
		if (s) {
			unless (proj) proj = s->proj;
			if (flags & BLOCK) c |= doit(s, 'b'); else dont(s, 'b');
			if (flags & PLOCK) {
				if (!force && HAS_GFILE(s)) {
					fprintf(stderr,
					"unlock: %s exists, not  unlocking.\n",
					s->gfile);
					c = 1;
				} else {
					c |= doit(s, 'p');
				}
			}
			if (flags & XLOCK) c |= doit(s, 'x'); else dont(s, 'x');
			if (flags & ZLOCK) c |= doit(s, 'z'); else dont(s, 'z');
			sccs_free(s);
		}
		name = sfileNext();
	}
	sfileDone();
	if (proj) proj_free(proj);
	return (c);
}

private void 
dont(sccs *s, char c)
{
	char	*lock = sccs_Xfile(s, c);

	if (exists(lock)) {
		fprintf(stderr, "Warning: %c-lock still on %s\n", c, s->gfile);
	}
}

private int
doit(sccs *s, char which)
{
	char	*lock = sccs_Xfile(s, which);

	if (unlink(lock) && (errno != ENOENT)) {
    		perror(lock);
		return (1);
	}
	return (0);
}

private int
repo(u32 flags)
{
	int	error = 0;

	if (flags & RLOCK) {
		if (repository_rdunlock(1)) {
			fprintf(stderr, "read unlock failed.\n");
			repository_lockers(0);
			error++;
		}
	}

	if (flags & WLOCK) {
		if (repository_wrunlock(1)) {
			fprintf(stderr, "write unlock failed.\n");
			repository_lockers(0);
			error++;
		}
	}

	if (flags & STALE) {
		repository_rdunlock(0);
		repository_wrunlock(0);
	}

	return (error);
}
