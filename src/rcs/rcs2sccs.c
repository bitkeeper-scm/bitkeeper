#include "rcs.h"

extern	int	isascii_main(int, char**);

private	char	*sccsname(char *path);
private	int	rcs2sccs(RCS *rcs, char *sfile);
private	int	create(char *sfile, int flags, int enc);
private	int	encoding(char *file);
private	int	newDelta(RCS *rcs, rdelta *d, sccs *s, int rev, int flags);
private int	verifyFiles(RCS *rcs, rdelta *d, char *g);
private	void	doit(char *file);

private	int	verify = 0;
private	int	Flags = SILENT;

int
main(int ac, char **av)
{
	int	c, i;
	char	buf[MAXPATH];

	while ((c = getopt(ac, av, "dv")) != -1) {
		switch (c) {
		    case 'd': Flags = 0; break;
		    case 'v': verify++; break;
		    default:
		    	fprintf(stderr, "Usage: %s [-v] files\n", av[0]);
			exit(1);
		}
	}
	if (av[optind] && streq("-", av[optind]) && !av[optind+1]) {
		while (fgets(buf, sizeof(buf), stdin)) {
			chop(buf);
			doit(buf);
		}
	} else {
		for (i = optind; av[i]; ++i) {
			doit(av[i]);
		}
	}
	purify_list();
	return (0);
}

private void
doit(char *file)
{
	RCS	*r;
	char	*sfile;

	sfile = sccsname(file);
	if (exists(sfile)) {
		fprintf(stderr, "Skipping file %s\n", sfile);
		free(sfile);
		return;
	}
	unless (r = rcsparse(file)) {
		fprintf(stderr, "Can't parse %s\n", file);
		exit(1);
	}
	if (rcs2sccs(r, sfile)) exit(1);
	rcsfree(r);
	free(sfile);
}

/*
 * Take a file name and return an SCCS filename.
 */
private	char	*
sccsname(char *path)
{
	char	*t = strrchr(path, ',');
	char	*s;
	
	if (t && (t[1] == 'v') && (t[2] == 0)) {
		*t = 0;
		s = name2sccs(path);
		*t = ',';
	} else {
		s = name2sccs(path);
	}
	return (s);

}

private	int
rcs2sccs(RCS *rcs, char *sfile)
{
	rdelta	*d;
	sccs	*s;
	int	len;
	int	rev = 1;
	char	*g = sccs2name(sfile);

	if (exists(g)) unlink(g);	// DANGER
	if (create(sfile, Flags, encoding(rcs->file))) return (1);
	printf("%s 1.0", g);
	fflush(stdout);
	len = 3;
	for (d = defbranch(rcs); d; d = d->kid, rev++) {
		unless (s = sccs_init(sfile, 0, 0)) return (1);
		if (newDelta(rcs, d, s, rev, Flags)) {
			sccs_free(s);
			free(g);
			return (1);
		}
		while (len--) putchar('\b');
		d->sccsrev = strdup(s->table->rev);
		printf("%s", s->table->rev);
		fflush(stdout);
		len = strlen(s->table->rev);
		sccs_free(s);
	}
	unless (verify) {
		printf(" converted\n");
		free(g);
		return (0);
	}

	printf(" converted;  ");
	len = 0;
	for (d = defbranch(rcs); d; d = d->kid, rev++) {
		while (len--) putchar('\b');
		printf("%s<->%s", d->rev, d->sccsrev);
		fflush(stdout);
		len = strlen(d->sccsrev) + strlen(d->rev) + 3;
		if (verifyFiles(rcs, d, g)) {
			free(g);
			return (1);
		}
	}
	printf(" verified.\n");
	free(g);
	return (0);
}

private int
verifyFiles(RCS *rcs, rdelta *d, char *g)
{
	char	cmd[MAXPATH*3];
	int	ret;

	if (exists(g)) unlink(g);	// DANGER
	sprintf(cmd, "co -q -kk -r%s %s && bk get -kpqr%s %s | diff %s -",
	    d->rev, g, d->sccsrev, g, g);
	ret = system(cmd);
	if (exists(g)) unlink(g);	// DANGER
	return (ret);
}

private	int
newDelta(RCS *rcs, rdelta *d, sccs *s, int rev, int flags)
{
	MMAP	*init;
	char	*t, *q;
	sym	*sy;
	char	buf[16<<10];

	sprintf(buf, "co -q -p -kk -r%s %s > %s", d->rev, rcs->file, s->gfile);
	if (system(buf) != 0) {
		fprintf(stderr, "[%s] failed\n", buf);
		return (1);
	}

	/* bk get $Q -eg $gfile */
	if (sccs_get(s, 0, 0, 0, 0, flags|GET_EDIT|GET_SKIPGET, "-")) {
		fprintf(stderr, "Edit of %s failed\n", s->gfile);
		return (1);
	}
	sccs_restart(s);

	sprintf(buf, "D 1.%d %s-0:00 %s\n", rev, d->sdate, d->author);
	if (d->comments) {
		q = strchr(buf, '\n');
		strcat(q, "c ");
		q += 3;
		assert(q[-1] && !q[0]);
		for (t = d->comments; *t; ) {
			if (((*q++ = *t++) == '\n') && *t) {
				*q++ = 'c';
				*q++ = ' ';
			}
		}
		*q = 0;
	} else {
		q = &buf[strlen(buf)];
	}
	if (d->dateFudge) {
		sprintf(q, "F %u\n", (unsigned int)d->dateFudge);
		q = strchr(q, '\n'); assert(q && !q[1]); q++;
	}
	sprintf(q, "P %s\n", s->gfile);
	q = strchr(q, '\n'); assert(q && !q[1]); q++;
	for (sy = rcs->symbols; sy; sy = sy->next) {
		unless (streq(sy->rev, d->rev)) continue;
		sprintf(q, "S %s\n", sy->name);
		q = strchr(q, '\n'); assert(q && !q[1]); q++;
	}
	strcpy(q, "------------------------------------------------\n");
//fprintf(stderr, "%s", buf);
	init = mrange(buf, &buf[strlen(buf)], "b");
	
	/* bk delta $Q -cRI.init $gfile */
	if (sccs_delta(s, flags|DELTA_FORCE|DELTA_PATCH, 0, init, 0, 0)) {
		fprintf(stderr, "Delta of %s failed\n", s->gfile);
		return (1);
	}
	mclose(init);

	return (0);
}

private	int
create(char *sfile, int flags, int enc)
{
	static	u16 seq;
	sccs	*s = sccs_init(sfile, 0, 0);
	char	*g = sccs2name(sfile);
	char	r[20];
	MMAP	*init;
	char	buf[8192];

	if (exists(g)) unlink(g);	// DANGER
	// XXX permissions
	close(creat(g, 0664));
	randomBits(r);
	sprintf(buf,
"D 1.0 70/01/01 03:09:62 BK\n\
c RCS to BitKeeper\n\
K %u\n\
P %s\n\
R %s\n\
X 0x3\n\
------------------------------------------------\n",
	    ++seq, g, r);
//fprintf(stderr, "%s", buf);
	init = mrange(buf, &buf[strlen(buf)], "b");

	/* bk delta $Q $enc -ciI.onezero $gfile */
	s->encoding = enc;
	check_gfile(s, 0);
	if (sccs_delta(s, flags|NEWFILE|INIT_NOCKSUM, 0, init, 0, 0)) {
		fprintf(stderr, "Create of %s failed\n", g);
		return (1);
	}
	sccs_free(s);
	free(g);
	mclose(init);
	return (0);
}

private	int
encoding(char *file)
{
	char	*av[3];

	av[0] = "isascii";
	av[1] = file;
	av[2] = 0;
	if (isascii_main(2, av) == 0) {
		return (E_ASCII);
	} 
	return (E_UUENCODE);
}
