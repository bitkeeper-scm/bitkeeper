#include "system.h"
#include "sccs.h"
WHATSTR("@(#)%K%");
/*
 * rechksum - regenerate the checksums associated with a file.
 *
 * Foreach delta {
 *	get -k the file
 *	verify the checksum both 7bit and 8bit
 *	if the 7bit one is right, insert the 8bit one
 *	if neither is right, scream.
 * }
 *
 * Copyright (c) 1999 L.W.McVoy
 */

private	int	resum(sccs *s, delta *d, int, int flags, int old7bit, int dont);
private	int	sumit(char *path, int *old, int *new, int old7bit);

int
rechksum_main(int ac, char **av)
{
	sccs	*s;
	delta	*d;
	int	doit;
	char	*name;
	int	flags = 0;
	int	old = 0;
	int	dont = 0;
	int	verbose = 0;
	int	c;
	int	exit = 0;
	project	*proj = 0;

	if (ac > 1 && streq("--help", av[1])) {
		system("bk help rechksum");
		return (0);
	}
	while ((c = getopt(ac, av, "cfov")) != -1) {
		switch (c) {
		    case 'c': dont++; break;	/* doc 2.0 */
		    case 'f': flags |= GET_FORCE; break;	/* doc 2.0 */
		    case 'o': old++; break;	/* doc 2.0 */
		    case 'v': verbose++; break;	/* doc 2.0 */
		    default:  system("bk help -s rechksum");
			      return (1);
		}
	}
	for (name = sfileFirst("rechksum", &av[optind], 0);
	    name; name = sfileNext()) {
		s = sccs_init(name, INIT_SAVEPROJ, proj);
		unless (s) continue;
		unless (proj) proj = s->proj;
		unless (s->tree) {
			fprintf(stderr, "%s: can't read SCCS info in \"%s\".\n",
			    av[0], s->sfile);
			continue;
		}
		unless (BITKEEPER(s)) {
			fprintf(stderr,
			    "%s: \"%s\" is not a BitKeeper file, ignored\n",
			    av[0], s->sfile);
			continue;
		}
		for (doit = 0, d = s->table; d; d = d->next) {
			if (d->type == 'D') {
				doit += resum(s, d, verbose, flags, old, dont);
			}
		}
		if (verbose) {
			fprintf(stderr, "%s: %d bad delta checksums\n",
			    s->gfile, doit);
		}
		if (doit && !dont) {
			fprintf(stderr, "Redid %d in %s\n", doit, s->sfile);
			unless (sccs_restart(s)) { perror("restart"); exit(1); }
			if (sccs_admin(
			    s, 0, NEWCKSUM, 0, 0, 0, 0, 0, 0, 0, 0)) {
			    	exit = 2;
				unless (BEEN_WARNED(s)) {
					fprintf(stderr,
					    "admin -z of %s failed.\n",
					    s->sfile);
				}
			}
		}
		sccs_free(s);
	}
	sfileDone();
	if (proj) proj_free(proj);
	return (exit ? exit : (doit ? 1 : 0));
}

private	int
resum(sccs *s, delta *d, int verbose, int flags, int old7bit, int dont)
{
	int	i, old, new;
	int	encoding = s->encoding;
	char	path[MAXPATH];

	unless (s = sccs_restart(s)) {
		perror("restart");
		exit(1);
	}

	if (S_ISLNK(d->mode)) {
		u8	*t;
		u16	sum = 0;

		/* don't complain about these, old BK binaries did this */
		if (dont && !d->sum) return (0);

		for (t = d->symlink; *t; sum += *t++);
		if (sum == d->sum) return (0);
		unless ((flags & GET_FORCE) && !dont) {
			fprintf(stderr,
			  "Bad symlink checksum %d:%d in %s:%s NOT corrected\n",
			    d->sum, sum, s->sfile, d->rev);
			return (0);
		} else {
			d->sum = sum;
			d->flags |= D_CKSUM;
			return (1);
		}
	}

	/*
	 * If there is no content change, then if no checksum, cons one up
	 * from the data in the delta table.
	 */
	unless (d->added || d->deleted || d->include || d->exclude) {
		if (d->flags & D_CKSUM) return (0);
		if (verbose>1) fprintf(stderr, "%s:%s\n", s->sfile, d->rev);
		new = 0;
		new = adler32(new, d->sdate, strlen(d->sdate));
		new = adler32(new, d->user, strlen(d->user));
		if (d->pathname) {
			new = adler32(new, d->pathname, strlen(d->pathname));
		}
		if (d->hostname) {
			new = adler32(new, d->hostname, strlen(d->hostname));
		}
		EACH(d->comments) {
			new = adler32(new,
			    d->comments[i], strlen(d->comments[i]));
		}
		if (dont) {
			fprintf(stderr, "%s:%s actual=<none> sum=%d\n",
			    s->gfile, d->rev, new);
			return (1);
		}
		if (verbose>1) {
			fprintf(stderr, "Converting %s:%s\n", s->sfile, d->rev);
		}
		d->sum = (sum_t)new;
		d->flags |= D_CKSUM;
		return (1);
	}

	if (verbose>1) fprintf(stderr, "%s:%s\n", s->sfile, d->rev);

	/* expand the file in the form that we checksum it */
	if (s->encoding == E_UUENCODE) s->encoding = E_ASCII;

	/* flags can't have EXPAND in them */
	gettemp(path, "resum");
	if (sccs_get(s, d->rev, 0, 0, 0, GET_SHUTUP|SILENT|PRINT, path)) {
		unless (BEEN_WARNED(s)) {
			fprintf(stderr,
			    "co of %s:%s failed, skipping it.\n",
			    s->gfile, d->rev);
		}
		s->encoding = encoding;
		return (0);
	}

	s->encoding = encoding;

	if (sumit(path, &old, &new, old7bit)) {
		unlink(path);
		return (0);
	}
	unlink(path);

	if (d->sum == new) return (0);
	if (dont) {
		fprintf(stderr, "%s:%s actual=%d v2=%d v1=%d\n",
		    s->gfile, d->rev, d->sum, new, old);
		return (1);
	}
	if (d->sum != old) {
		if ((d->flags & D_CKSUM) && !(flags & GET_FORCE)) {
			fprintf(stderr,
			    "Bad checksum %d:%d:%d in %s:%s NOT corrected\n",
			    d->sum, old, new, s->sfile, d->rev);
			return (0);
		}
		d->sum = new;
		d->flags |= D_CKSUM;
		return (1);
	} else {
		if (verbose>1) {
			fprintf(stderr, "Converting %s:%s\n", s->sfile, d->rev);
		}
		d->sum = new;
		d->flags |= D_CKSUM;
		return (1);
	}
	assert("Not reached" == 0);
}

private	int
sumit(char *path, int *old, int *new, int old7bit)
{
	unsigned char buf[16<<10];
	register unsigned char *u;
	register char *s;
	register int i;
	unsigned short usum = 0;
	unsigned short sum = 0;
	int fd, save, bytes = 0;

	*old = *new = 0;
	if ((fd = open(path, 0, GROUP_MODE)) == -1) {
		perror(path);
		return (-1);
	}
	while ((i = read(fd, buf, sizeof(buf))) > 0) {
		bytes += i;
		save = i;
		for (u = buf; i--; usum += *u++);
		for (s = buf, i = save; i--; sum += *s++);
	}
	*old = sum;
	*new = usum;
	close(fd);
	return (bytes ? 0 : 1);
}
