#include "rcs.h"

private	char	*sccsname(char *path);
private	int	rcs2bk(RCS *rcs, char *sfile);
private	int	create(char *sfile, int flags, RCS *rcs);
private	int	encoding(char *file);
private	mode_t	mode(char *file);
private	int	newDelta(RCS *rcs, rdelta *d, sccs *s, int rev, int flags);
private int	verifyFiles(RCS *rcs, rdelta *d, char *g);
private	void	doit(char *file, char *cvsbranch);
private	rdelta *first_rdelta(RCS *rcs);

private	int	verify;
private	int	undos;
private	int	verbose;
private	int	Flags;
private	char	*co_prog;
private	char	*cutoff;

extern	int	in_rcs_import;

int
rcs2bk_main(int ac, char **av)
{
	int	c, i;
	char	buf[MAXPATH];
	char	*cvsbranch = 0;

	Flags = SILENT;
	verify = 0;
	verbose = 2;
	co_prog = cutoff = 0;

	while ((c = getopt(ac, av, "b;c;dhqu")) != -1) {
		switch (c) {
		    case 'b': cvsbranch = optarg; break;	/* undoc 3.0 */
		    case 'c': cutoff = optarg; break;		/* doc 2.0 */
		    case 'd': Flags = 0; break;			/* undoc? 2.0 */
		    case 'q': if (verbose) verbose--; break;	/* doc 2.0 */
		    case 'h': verify++; break;			/* doc 2.0 */
		    case 'u': undos++; break;			/* doc 2.0 */
		    default:
			    system("bk help -s rcs2bk");
			    return (1);
		}
	}
	unless (co_prog = getenv("BK_RCS_CO")) {
		/* We typically want /usr/local/bin/co, it's more recent */
		if (executable("/usr/local/bin/co")) {
			co_prog = "/usr/local/bin/co";
		} else {
			co_prog = whichp("co", 0, 1);
		}
	}
	unless (co_prog && executable(co_prog)) {
		fprintf(stderr,
"rcs2bk needs the RCS program co, which was not found in your PATH.\n");
		exit(1);
	}

	if (av[optind] && streq("-", av[optind]) && !av[optind+1]) {
		while (fgets(buf, sizeof(buf), stdin)) {
			chop(buf);
			doit(buf, cvsbranch);
		}
	} else {
		for (i = optind; av[i]; ++i) {
			doit(av[i], cvsbranch);
		}
	}
	return (0);
}

private void
doit(char *file, char *cvsbranch)
{
	RCS	*r;
	char	*sfile;
	sccs	*s;
	delta	*d;

	sfile = sccsname(file);
	if (exists(sfile)) {
		fprintf(stderr, "Skipping file %s\n", sfile);
		free(sfile);
		return;
	}
	if (undos) {
		char	path[MAXPATH];
		FILE	*f = fopen(file, "r");
		int	convert = 1;

		while (fnext(path, f)) {
			if (strneq("date", path, 4)) break;
			if (strneq("expand", path, 6)) {
				if (strstr(path, "@b@")) convert = 0;
			}
		}
		fclose(f);
		if (convert) {
			mode_t	m = mode(file);

			sprintf(path, "%s%u", file, getpid());
			rename(file, path);
			sysio(0, file, 0, "bk", "undos", "-n", path, SYS);
			unlink(path);
			chmod(file, m);
		}
	}
	unless (r = rcs_init(file, cvsbranch)) {
		fprintf(stderr, "Can't parse %s\n", file);
		exit(1);
	}
	/*
	 * File created on a branch will never create a sccs file
	 * and can be ignored.
	 */
	if (first_rdelta(r)) {
		if (rcs2bk(r, sfile)) exit(1);
		/*
		 * After file is completed, determine if the sfile needs
		 * to be moved to the BitKeeper/deleted directory.
		 */
		s = sccs_init(sfile, 0);
		d = sccs_top(s);
		unless (streq(d->pathname, s->gfile)) {
			char	*p, *q;
			int	ret;

			if (sccs_clean(s, SILENT)) {
				sccs_clean(s, 0);
				fprintf(stderr, "Failed to clean %s\n",
				    s->gfile);
				exit(1);
			}
			sccs_close(s);
			q = name2sccs(d->pathname);
			ret = rename(sfile, q);
			assert(ret == 0);
			/*
			 * move the d.file
			 */
			p = sccs_Xfile(s, 'd');
			if (exists(p)) {
				char	*t = rindex(q, '/') + 1;

				*t = 'd';
				ret = rename(p, q);
				assert(ret == 0);
			}
			free(q);
		}
		sccs_free(s);
	}
	rcs_free(r);
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

private rdelta *
first_rdelta(RCS *rcs)
{
	rdelta	*d;

	d = rcs->tree;
	while (d->dead && d->kid) d = d->kid;

	return (d->dead ? 0 : d);
}

private	int
rcs2bk(RCS *rcs, char *sfile)
{
	rdelta	*d, *stop = 0;
	sccs	*s;
	int	len = 0, rev = 1;
	char	*g = sccs2name(sfile);
	int	ret;
	sym	*sym;

	/*
	 * If we are only doing a partial, make sure the tag is here,
	 * otherwise this file shouldn't be done at all.
	 */
	if (cutoff) {
		for (sym = rcs->symbols; sym; sym = sym->next) {
			if (streq(sym->name, cutoff)) break;
		}
		unless (sym && (stop = rcs_findit(rcs, sym->rev))) {
			unless (Flags & SILENT) {
				fprintf(stderr,
				    "%s not found in %s, skipping this file\n",
					cutoff, rcs->rcsfile);
			}
			free(g);
			return (0);
		}
		for (d = first_rdelta(rcs); d && (d != stop); d = d->kid);
		unless (d == stop) {
			fprintf(stderr,
			"%s is not on the trunk in %s, skipping this file.\n",
			    cutoff, rcs->rcsfile);
			free(g);
			return (0);
		}
		stop = stop->kid;	/* we want stop, so go one more */
	}

	in_rcs_import = 1;
	if (exists(g)) unlink(g);	// DANGER
	if (create(sfile, Flags, rcs)) return (1);
	if (verbose > 1) {
		printf("%s 1.0", g);
		fflush(stdout);
		len = 3;
	}
	unless (s = sccs_init(sfile, 0)) return (1);
	for (d = first_rdelta(rcs); d && (d != stop); d = d->kid, rev++) {
		if (newDelta(rcs, d, s, rev, Flags)) {
			sccs_free(s);
			free(g);
			return (1);
		}
		sccs_restart(s);
		d->sccsrev = strdup(s->table->rev);
		if (verbose > 1) {
			while (len--) putchar('\b');
			printf("%s", s->table->rev);
			fflush(stdout);
			len = strlen(s->table->rev);
		}
	}
	sccs_free(s);
	in_rcs_import = 0;
	unless (verify) {
		if (verbose > 1) {
			printf(" converted\n");
		} else if (verbose) {
			printf("%s %d converted\n", g, rev-1);
		}
		free(g);
		return (0);
	}

	if (verbose > 1) {
		printf(" converted;  ");
		len = 0;
	}
	unless (s = sccs_init(sfile, 0)) return (1);
	for (d = first_rdelta(rcs); d && (d != stop); d = d->kid) {
		if (verbose > 1) {
			while (len--) putchar('\b');
			printf("%s<->%s", d->rev, d->sccsrev);
			fflush(stdout);
			len = strlen(d->sccsrev) + strlen(d->rev) + 3;
		}
		if (verifyFiles(rcs, d, g)) {
			free(g);
			sccs_free(s);
			return (1);
		}
	}
	if (verbose > 1) {
		printf(" verified.\n");
	} else if (verbose) {
		printf("%s %d converted and verified\n", g, rev-1);
	}
	free(g);
	ret = 0;
	if (do_checkout(s)) ret = 1;
	sccs_free(s);
	return (ret);
}

private int
verifyFiles(RCS *rcs, rdelta *d, char *g)
{
	char	tmpfile[MAXPATH];
	char	*cmd;
	int	ret;

	if (exists(g)) unlink(g);	// DANGER
	/* our version of diff cannot handlle "-" */
	bktmp(tmpfile, "rcs2bk");
	cmd = aprintf("%s -q '%s' '-p%s' '%s,v' > %s",
	    co_prog, rcs->kk, d->rev, g, tmpfile);
	system(cmd);
	free(cmd);
	cmd = aprintf("bk _get -kq '-r%s' '%s'", d->sccsrev, g);
	system(cmd);
	free(cmd);

	ret = sys("diff", "--ignore-trailing-cr", tmpfile, g, SYS);

	unlink(tmpfile);
	if (exists(g)) unlink(g);	// DANGER
	return (ret);
}

private	int
newDelta(RCS *rcs, rdelta *d, sccs *s, int rev, int flags)
{
	MMAP	*init;
	char	*t, *q;
	sym	*sy;
	pid_t	pid;
	static	char *buf = 0;
	static	int buflen;

	unless (buf) buf = malloc(buflen = 64<<10);
#ifdef	WIN32
	unlink(s->gfile); //DANGER
	sprintf(buf, "-r%s", d->rev);
	if (sysio(0, s->gfile, 0, co_prog, "-q",
			"-p", rcs->kk, buf, rcs->rcsfile, SYS) != 0) {
		fprintf(stderr, "[%s] failed\n", buf);
		return (1);
	}
#else
	if (pid = fork()) {
		if (pid == -1) {
			perror("fork");
			exit(1);
		}
		waitpid(pid, 0, 0);
	} else {
		char	*av[100];
		int	i;

		close(1);
		unlink(s->gfile);	// DANGER
		creat(s->gfile, 0666);
		av[i = 0] = co_prog;
		av[++i] = "-q";
		av[++i] = "-p";
		av[++i] = rcs->kk;
		sprintf(buf, "-r%s", d->rev);
		av[++i] = buf;
		av[++i] = rcs->rcsfile;
		av[++i] = 0;
		execv(co_prog, av);
		perror(co_prog);
		exit(1);
	}
#endif

	/* bk get $Q -eg $gfile */
	if (sccs_get(s, 0, 0, 0, 0, flags|GET_EDIT|GET_SKIPGET, "-")) {
		fprintf(stderr, "Edit of %s failed\n", s->gfile);
		return (1);
	}
	sccs_restart(s);

again:
	sprintf(buf, "D 1.%d %s-0:00 %s \n", rev, d->sdate, d->author);
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
			if (q >= &buf[buflen / 2]) {
realloc:
				fprintf(stderr, "Buffer overflow, realloc.\n");
				free(buf);
				buflen <<= 1;
				buf = malloc(buflen);
				goto again;
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
	if (d->dead) {
		char	*p1, *p2;
		char	*rmName = sccs_rmName(s);

		p1 = sccs2name(rmName);
		free(rmName);
		p2 = proj_relpath(0, p1);
		free(p1);
		sprintf(q, "P %s\n", p2);
		free(p2);
	} else {
		sprintf(q, "P %s\n", s->gfile);
	}
	if (q >= &buf[buflen / 2]) goto realloc;
	q = strchr(q, '\n'); assert(q && !q[1]); q++;
	for (sy = rcs->symbols; sy; sy = sy->next) {
		unless (streq(sy->rev, d->rev)) continue;
		if (q >= &buf[buflen / 2]) goto realloc;
		sprintf(q, "S %s\n", sy->name);
		q = strchr(q, '\n'); assert(q && !q[1]); q++;
	}
	strcpy(q, "------------------------------------------------\n");
	init = mrange(buf, &buf[strlen(buf)], "b");
//fprintf(stderr, "%.*s\n", init->size, init->mmap);

	/* bk delta $Q -cRI.init $gfile */
	if (sccs_delta(s, flags|DELTA_FORCE|DELTA_PATCH, 0, init, 0, 0)) {
		fprintf(stderr, "Delta of %s failed\n", s->gfile);
		return (1);
	}
	mclose(init);

	return (0);
}

private	int
create(char *sfile, int flags, RCS *rcs)
{
	sccs	*s = sccs_init(sfile, 0);
	char	*g = sccs2name(sfile);
	int	enc = encoding(rcs->rcsfile);
	int	expand;
	mode_t	m = mode(rcs->rcsfile);
	MMAP	*init;
	char	*f, *t;
	char	buf[32<<10];
	char	*delta_csum;
	char	*random;
	char	*date;

	assert(rcs->rootkey);
	delta_csum = index(rcs->rootkey, '|') + 1;
	assert(delta_csum);
	delta_csum = index(delta_csum, '|') + 1;
	assert(delta_csum);
	delta_csum = index(delta_csum, '|') + 1;
	assert(delta_csum);
	random = index(delta_csum, '|') + 1;
	assert(random);

	if (exists(g)) unlink(g);	// DANGER
	close(creat(g, m));

	t = index(rcs->rootkey, '|');
	*t = 0;
	date = index(t+1, '|') + 1;
	sprintf(buf,
"D 1.0 %.2s/%.2s/%.2s %.2s:%.2s:%.2s %s \n\
c RCS to BitKeeper\n\
K %.5s\n\
O 0%o\n\
P %s\n\
R %.8s\n",
		&date[2],
		&date[4],
		&date[6],
		&date[8],
		&date[10],
		&date[12],
		rcs->rootkey,	/* user@host */
		delta_csum, m, g, random);
	*t = '|';		/* restore key */
	t = &buf[strlen(buf)];
	if (rcs->text) {
		*t++ = 'T';
		*t++ = ' ';
		for (f = rcs->text; f && *f; f++) {
			if (*f == '\n') {
				*t++ = '\n';
				*t++ = 'T';
				*t++ = ' ';
			} else {
				*t++ = *f;
			}
		}
		unless (t[-1] == '\n') *t++ = '\n';
		*t = 0;
	}
	expand = streq(rcs->kk, "-kv") ||
	    streq(rcs->kk, "-kk") || streq(rcs->kk, "-kkv");
	sprintf(t,
		"X 0x%x\n------------------------------------------------\n",
		X_DEFAULT | (expand ? X_RCS : 0));
	init = mrange(buf, &buf[strlen(buf)], "b");

	/* bk delta $Q $enc -ciI.onezero $gfile */
	s->encoding = enc;
	check_gfile(s, 0);
	if (sccs_delta(s,
	    flags|NEWFILE|INIT_NOCKSUM|DELTA_PATCH, 0, init, 0, 0)) {
		fprintf(stderr, "Create of %s failed\n", g);
		return (1);
	}
	sccs_sdelta(s, sccs_ino(s), buf);
	if (strcmp(buf, rcs->rootkey) != 0) {
		printf("missmatch:\n\t%s\n!=\t%s\n", buf, rcs->rootkey);
		exit(1);
	}
	sccs_free(s);
	free(g);
	mclose(init);
	return (0);
}

/*
 * Look at the file and get the execute bits.
 */
private	mode_t
mode(char *file)
{
	struct	stat	sbuf;

	if (stat(file, &sbuf) == -1) return 0;
	sbuf.st_mode |= 0664;
	return (sbuf.st_mode & 0777);
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
