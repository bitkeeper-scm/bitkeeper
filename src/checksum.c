#include "system.h"
#include "sccs.h"
#include "zlib/zlib.h"
WHATSTR("@(#)%K%");

private int	do_chksum(int fd, int off, int *sump);
private int	chksum_sccs(char **files, char *offset);
private int	do_file(char *file, int off);

/*
 * checksum - check and/or regenerate the checksums associated with a file.
 *
 * Copyright (c) 1999-2001 L.W.McVoy
 */
int
checksum_main(int ac, char **av)
{
	sccs	*s;
	delta	*d;
	int	doit;
	char	*name;
	int	fix = 0, diags = 0, bad = 0, do_sccs = 0, ret = 0;
	int	c;
	char	*off;
	project	*proj = 0;

	if (ac > 1 && streq("--help", av[1])) {
		system("bk help checksum");
		return (0);
	}
	while ((c = getopt(ac, av, "cfs|v")) != -1) {
		switch (c) {
		    case 'c': break;	/* obsolete */
		    case 'f': fix = 1; break;			/* doc 2.0 */
		    case 's': do_sccs = 1; off = optarg; break;
		    case 'v': diags++; break;			/* doc 2.0 */
		    default:  system("bk help -s checksum");
			      return (1);
		}
	}
	
	if (do_sccs) return (chksum_sccs(&av[optind], off));
	
	for (name = sfileFirst("checksum", &av[optind], 0);
	    name; name = sfileNext()) {
		s = sccs_init(name, INIT_SAVEPROJ, proj);
		unless (s) continue;
		unless (proj) proj = s->proj;
		unless (HASGRAPH(s)) {
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
				c = sccs_resum(s, d, diags, fix);
				if (c & 1) doit++;
				if (c & 2) bad++;
			}
		}
		if (diags) {
			fprintf(stderr,
			    "%s: %d bad delta checksums\n", s->gfile, bad);
		}
		if ((doit || !s->cksumok) && fix) {
			unless (sccs_restart(s)) { perror("restart"); exit(1); }
			if (sccs_admin(
			    s, 0, NEWCKSUM, 0, 0, 0, 0, 0, 0, 0, 0)) {
			    	ret = 2;
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
	return (ret ? ret : (doit ? 1 : 0));
}

int
sccs_resum(sccs *s, delta *d, int diags, int fix)
{
	int	i;

	if (LOGS_ONLY(s)) return (0);

	unless (d) d = sccs_top(s);

	if (S_ISLNK(d->mode)) {
		u8	*t;
		sum_t	sum = 0;
		delta	*e;

		/* don't complain about these, old BK binaries did this */
		e = getSymlnkCksumDelta(s, d);
		if (!fix && !e->sum) return (0);

		for (t = d->symlink; *t; sum += *t++);
		if ((e->flags & D_CKSUM) && (e->sum == sum)) return (0);
		unless (fix) {
			fprintf(stderr, "Bad symlink checksum %d:%d in %s|%s\n",
			    e->sum, sum, s->gfile, d->rev);
			return (2);
		} else {
			if (diags > 1) {
				fprintf(stderr, "Corrected %s:%s %d->%d\n",
				    s->sfile, d->rev, d->sum, sum);
			}
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
		int	new = 0;

		if (d->flags & D_CKSUM) return (0);
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
		unless (fix) {
			fprintf(stderr, "%s:%s actual=<none> sum=%d\n",
			    s->gfile, d->rev, new);
			return (2);
		}
		if (diags > 1) {
			fprintf(stderr, "Derived %s:%s -> %d\n",
			    s->sfile, d->rev, (sum_t)new);
		}
		d->sum = (sum_t)new;
		d->flags |= D_CKSUM;
		return (1);
	}

	if (sccs_get(s,
	    d->rev, 0, 0, 0, GET_SUM|GET_SHUTUP|SILENT|PRINT, "-")) {
		unless (BEEN_WARNED(s)) {
			fprintf(stderr,
			    "get of %s:%s failed, skipping it.\n",
			    s->gfile, d->rev);
		}
		return (4);
	}
	if ((d->flags & D_CKSUM) && (d->sum == s->dsum)) return (0);
	unless (fix) {
		fprintf(stderr,
		    "Bad checksum %d:%d in %s|%s\n",
		    d->sum, s->dsum, s->gfile, d->rev);
		return (2);
	}
	if (diags > 1) {
		fprintf(stderr, "Corrected %s:%s %d->%d\n",
		    s->sfile, d->rev, d->sum, s->dsum);
	}
	d->sum = s->dsum;
	d->flags |= D_CKSUM;
	return (1);
	assert("Not reached" == 0);
}


/*
 * Calculate the same checksum as is used in BitKeeper.
 */
private int
chksum_sccs(char **files, char *offset)
{
	int	sum, i;
	int	off = 0;
	char	buf[MAXPATH];

	if (offset) off = atoi(offset);
	unless (files[0]) {
		if (do_chksum(0, off, &sum)) return (1);
		printf("%d\n", sum);
	} else if (streq("-", files[0]) && !files[1]) {
		while (fnext(buf, stdin)) {
			chop(buf);
			if (do_file(buf, off)) return (1);
		}
	} else for (i = 0; files[i]; ++i) {
		if (do_file(files[i], off)) return (1);
	}
	return (0);
}

private int
do_file(char *file, int off)
{
	int	sum, fd;

	fd = open(file, 0, 0);
	if (fd == -1) {
		perror(file);
		return (1);
	}
	if (do_chksum(fd, off, &sum)) {
		close(fd);
		return (1);
	}
	close(fd);
	printf("%-20s %d\n", file, sum);
	return (0);
}

private int
do_chksum(int fd, int off, int *sump)
{
	u8	 buf[16<<10];
	register unsigned char *p;
	register int i;
	u16	 sum = 0;

	while (off--) {
		if (read(fd, buf, 1) != 1) return (1);
	}
	while ((i = read(fd, buf, sizeof(buf))) > 0) {
		for (p = buf; i--; sum += *p++);
	}
	*sump = (int)sum;
	return (0);
}
