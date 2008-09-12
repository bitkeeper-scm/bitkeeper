#include "sccs.h"

#define	WA	"BitKeeper/tmp/partition.d"
#define	WA2PROD	"../../.."

private	void	createInitialModules(void);
private	void	push_hash2lines(hash *h, char *key, char *line);

extern	char	*prog;	/* XXX: time to make this a global like bin? */

int
cpartition_main(int ac, char **av)
{
	int	c, i;
	int	flags = 0;
	sccs	*s, *cset;
	project	*proj;
	char	*map = 0;
	char	**maplist = 0;
	char	*gonelist = 0;
	char	*from, *to;
	char	*t, *p, *cmd, *rand;
	FILE	*fmap = 0, *fgonelist = 0, *f, *fout;
	int	rc = 1;
	hash	*ser2keys;
	struct	stat sb;
	char	**serials;
	char	**lines;
	char	*quiet = "";
	char	buf[MAXLINE];

	while ((c = getopt(ac, av, "G;m;q")) != -1) {
		switch (c) {
		    case 'G': gonelist = optarg; break;
		    case 'm': map = optarg; break;
		    case 'q': flags |= SILENT; quiet = "-q"; break;
		    default:
usage:			system("bk help -s partition");
			return (1);
		}
	}
	unless (map) {
		fprintf(stderr, "%s: must specify mapfile\n", prog);
		return (1);
	}
	unless (exists(map)) {
		fprintf(stderr, "%s: mapfile '%s' does not exist\n",
		    prog, map);
		return (1);
	}
	if (gonelist && !exists(gonelist)) {
		fprintf(stderr, "%s: -G%s is not a file\n", prog, gonelist);
		return (1);
	}
	from = av[optind];
	to = av[optind+1];
	unless (from && to && !av[optind+2]) goto usage;

	if (exists(to)) {
		fprintf(stderr, "%s: destination '%s' exists\n",
		    prog, to);
		return (1);
	}
	verbose((stderr, "### Cloning product\n"));
	if (sys("bk", "clone", "-ql", from, to, SYS)) {
		/* try without the hard link */
		if (sys("bk", "clone", "-q", from, to, SYS)) {
			fprintf(stderr,
			    "%s: bk clone '%s' '%s' failed\n",
			    prog, from, to);
			return (1);
		}
	}
	unless (fmap = fopen(map, "r")) {
		perror(map);
		return (1);
	}
	if (gonelist && !(fgonelist = fopen(gonelist, "r"))) {
		perror(gonelist);
		goto out;
	}
	if (chdir(to)) {
		perror(to);
		goto out;
	}
	if (mkdirp(WA)) {
		perror(WA);
		goto out;
	}
	/*
	 * Get a local copy of gone and map files
	 */
	unless (f = fopen(WA "/map", "w")) {
		perror(WA "/map");
		goto out;
	}
	while (p = fgetline(fmap)) {
		/* skip comments and blank lines */
		unless (p[0] && (p[0] != '#')) continue;
		if (IsFullPath(p)) {
			fprintf(stderr,
			    "partition: no absolute paths in map\n");
			fclose(f);
			goto out;
		}
		maplist = addLine(maplist, strdup(p));
		fputs(p, f);
		fputc('\n', f);
	}
	fclose(fmap);
	fmap = 0;
	fclose(f);
	sortLines(maplist, string_sort);
	unless (f = fopen(WA "/gonelist", "w")) {
		perror(WA "/gonelist");
		goto out;
	}
	if (fgonelist) {
		while (p = fgetline(fgonelist)) {
			fputs(p, f);
			fputc('\n', f);
		}
		fclose(fgonelist);
		fgonelist = 0;
	}
	fclose(f);

	/*
	 * Note; gone list could vary over time if the gone file
	 * were consulted, or missing files ignores.
	 * Since we want to be able to run partition on many repos
	 * and have them communicate, pass in a gonelist.
	 */
	proj_rootkey(0);
	f = popen("bk _sort < " WA "/gonelist |"
	    "cat BitKeeper/log/ROOTKEY - | bk crypto -hX -", "r");
	assert(f);
	rand = fgetline(f);
	assert(strlen(rand) == 32);
	rand[16] = 0;

	verbose((stderr, "### Removing unwanted files\n"));
	cmd = aprintf("bk csetprune %s -S -k'%s' < " WA "/gonelist",
	    quiet, rand);
	pclose(f);
	if (system(cmd)) {
		fprintf(stderr, "%s: csetprune failed\n", prog);
		free(cmd);
		goto out;
	}
	free(cmd);

	/*
	 * Make a copy of the repo to generate all the components from
	 */
	if (sys("bk", "clone", "-ql", ".", WA "/repo", SYS)) {
		fprintf(stderr,
		    "%s: bk clone . %s failed\n", prog, WA "/repo");
		goto out;
	}

	proj_rootkey(0);
	f = popen("echo \"The Big Cheese\" | cat - BitKeeper/log/ROOTKEY "
	    "| bk crypto -hX -", "r");
	assert(f);
	rand = fgetline(f);
	assert(strlen(rand) == 32);
	rand[16] = 0;
	cmd = aprintf("bk csetprune %s -XNSE -k'%s' -C%s\n",
	    quiet, rand, WA "/map");
	pclose(f);
	if (system(cmd)) {
		fprintf(stderr, "%s: product csetprune failed\n", prog);
		free(cmd);
		goto out;
	}
	free(cmd);

	/*
	 * cycle through the components and prune each one
	 * easier to build and prune in a static location and
	 * move to slot at the end of things
	 */
	EACH(maplist) {
		verbose((stderr, "### Cloning component %s\n", maplist[i]));
		if (lstat(maplist[i], &sb)) {
			if (rmtree(maplist[i])) {
				fprintf(stderr,
				    "%s: rmtree %s failed\n",
				    prog, maplist[i]);
				goto out;
			}
			verbose((stderr,
			    "    Fix 'bk -r names' blocking %s\n",
			    maplist[i]));
		}
		cmd = aprintf("bk clone -ql %s %s\n",
		    WA "/repo",
		    WA "/new");
		if (system(cmd)) {
			perror(cmd);
			free(cmd);
			goto out;
		}
		free(cmd);

		if (chdir(WA "/new")) {
			perror(prog);
			goto out;
		}
		proj_rootkey(0);
		cmd = aprintf("echo \"%s\" | cat - BitKeeper/log/ROOTKEY |"
		    "bk crypto -hX -", maplist[i]);
		f = popen(cmd, "r");
		free(cmd);
		assert(f);
		rand = fgetline(f);
		assert(strlen(rand) == 32);
		rand[16] = 0;
		cmd = aprintf("bk csetprune %s -XNS -C../map -c\"%s\" "
		    "-k'%s'",
		    quiet, maplist[i], rand);
		pclose(f);
		if (system(cmd)) {
			perror(cmd);
			free(cmd);
			goto out;
		}
		free(cmd);

		if (chdir("../" WA2PROD) ||
		    mkdirf(maplist[i]) ||
		    rename(WA "/new", maplist[i])) {
			perror(prog);
			goto out;
		}
	}
	goto out;	/* XXX: got to here */

	/*
	 * clean up fake product-- it was used to be able to
	 * run check in the components as we were building them
	 *
	 * We are done using 'prod' as a clone src, so chop it up
	 */
	verbose((stderr, "### Fixing product\n"));
	chdir(WA2PROD);

	sprintf(buf, "%s/prod.before", WA);
	if (fileCopy(CHANGESET, buf)) {
		perror(buf);
		goto out;
	}

	proj_rootkey(0);
	f = popen("cat BitKeeper/log/ROOTKEY | bk crypto -hX -", "r");
	assert(f);
	t = fgetline(f);
	assert(strlen(t) == 32);
	t[16] = 0;

	sprintf(buf, "bk csetprune -S -k'%s' -N %s < %s/path.0",
	    t, quiet, WA);
	pclose(f);
	if (system(buf)) {
		perror(buf);
		goto out;
	}
	sprintf(buf, "bk -r surgery -q -m%s/map -P", WA);
	unless (f = popen(buf, "r")) {
		perror(buf);
		goto out;
	}
	while (p = fgetline(f)) {
		t = strchr(p, '\t');
		t++;
		push_hash2lines(ser2keys, p, t);
	}
	unlink("BitKeeper/log/COMPONENT");

	createInitialModules();

	/* add cset entry of modules in 1.1 */
	cset = sccs_csetInit(SILENT);
	p = buf + sprintf(buf, "%d", sccs_findrev(cset, "1.1")->serial);
	*p++ = 0;
	s = sccs_init("BitKeeper/etc/SCCS/s.modules", SILENT);
	assert(s);
	sccs_sdelta(s, sccs_ino(s), p);
	t = p + strlen(p);
	*t++ = ' ';
	sccs_sdelta(s, sccs_top(s), t);
	sccs_free(s);
	sccs_free(cset);
	push_hash2lines(ser2keys, buf, p);

	/*
	 * Clean up fake Product which let me run surgery in comps
	 * leaving it out for now to show the fix to proj_product
	 * to not walk up the tree, stopping at first.
	 * rm -fr BitKeeper
	 */

	/* Do the transform and look at it */
	concat_path(buf, WA, "prod.before");
	f = fopen(buf, "r");
	concat_path(buf, WA, "prod.after");
	fout = fopen(buf, "w");
	while (p = fgetline(f)) {
		fputs(p, fout);
		fputc('\n', fout);
		if ((p[0] == '\001') && (p[1] == 'T')) break;
	}
	fclose(f);
	serials = 0;
	push_hash2lines(ser2keys, "1", 0);
	EACH_HASH(ser2keys) serials = addLine(serials, ser2keys->kptr);
	sortLines(serials, number_sort);
	reverseLines(serials);

	EACH(serials) {
		fprintf(fout, "\001I %s\n", serials[i]);
		lines = *(char ***)hash_fetchStr(ser2keys, serials[i]);
		EACH_INDEX(lines, c) fprintf(fout, "%s\n", lines[c]);
		fprintf(fout, "\001E %s\n", serials[i]);
	}
	fclose(fout);
	hash_free(ser2keys);

	/* Okay, wire it in */
	verbose((stderr, "### Loading components\n"));
	concat_path(buf, WA, "prod.after");
	unlink(CHANGESET);
	fileCopy(buf, CHANGESET);
	system("bk admin -z ChangeSet");
	system("bk _scompress -q ChangeSet");
	system("bk checksum -f ChangeSet");
	proj_rootkey(0);
	f = popen("cat BitKeeper/log/ROOTKEY | bk crypto -hX -", "r");
	assert(f);
	t = fgetline(f);
	assert(strlen(t) == 32);
	t[16] = 0;

	sprintf(buf, "bk newroot %s -k'%s'", quiet, t);
	pclose(f);
	system(buf);

	/* Give the modules file a cset mark */
	system("bk cset -M1.1");
	touch("BitKeeper/log/PRODUCT", 0444);
	proj = proj_init(".");
	reverseLines(maplist);
	rmEmptyDirs(1);
	EACH(maplist) {
		p = aprintf("path.%d", i);
		unless (exists(p)) continue;
		verbose((stderr, "### Setting up %s", maplist[i]));
		sprintf(buf, "comp.%d", i);
		chdir(buf);
		if (exists("BitKeeper/log/COMPONENT")) {
			/*
			 * only if it is a component should we keep it
			 * cases where it is not is a sub repo with no
			 * 1.2 cset in it, such as happens with the
			 * deep nest file in the regression.
			 */
			f = fopen("BitKeeper/log/COMPONENT", "w");
			fprintf(f, "%s\n", maplist[i]);
			fclose(f);
			sprintf(buf, "bk admin -C'%s' ChangeSet",
			    proj_rootkey(proj));
			system(buf);
		}
		chdir(proj_root(proj));
		mkdirf(maplist[i]);
		sprintf(buf, "%s/comp.%d", WA, i);
		if (rename(buf, maplist[i])) {
			perror(maplist[i]);
			goto out;
		}
		/*
		 * This is needed to wire deep nest stuff
		 * if bk -r isn't run in the deep, then the
		 * uppermost doesn't know about it
		 */
		sprintf(buf, "bk -r'%s' check -ac", maplist[i]);
		if (system(buf)) {
			perror(buf);
			goto out;
		}
	}
	system("bk -r names");
	sprintf(buf, "bk -r check -ac%s", (flags & SILENT) ? "" : "v");
	if (system(buf)) {
		perror(buf);
		goto out;
	}
	rmtree(WA);
	verbose((stderr, "partioning complete\n"));
	rc = 0;
out:
	if (fgonelist) fclose(fgonelist);
	if (fmap) fclose(fmap);
	return (rc);
}

private void
createInitialModules(void)
{
	sccs	*s;
	delta	*d;
	char	*t;
	FILE	*f;

	/*
	 *
	 * make an entry for the new modules file in the 1.1 cset
	 * Hmm, create time will be a bit wacky, won't it?
	 * XXX: fix create time here just like the findcset code
	 * messes with create time.  For now, it will just be wacky
	 */
	s = sccs_init("BitKeeper/etc/SCCS/s.config", SILENT);
	assert(s);
	d = sccs_findrev(s, "1.1");
	assert(d);
	t = sccs_prsbuf(s, d, PRS_FORCE, ":D: :T::TZ:");
	safe_putenv("BK_DATE_TIME_ZONE=%s", t+2);
	free(t);
	if (t = strchr(d->user, '/')) *t = 0; /* trim realuser */
	safe_putenv("BK_USER=%s", d->user);
	if (t) *t = '/';
	if (t = strchr(d->hostname, '/')) *t = 0; /* trim realhost */
	safe_putenv("BK_HOST=%s", d->hostname);
	if (t) *t = '/';
	sccs_free(s);

	f = popen("bk prs -r1.1 -hnd':D: :T::TZ: :USER: :HOST:' "
	    "BitKeeper/etc/confige | bk crypto -hX -", "r");
	assert(f);
	t = fgetline(f);
	assert(strlen(t) == 32);
	t[16] = 0;
	safe_putenv("BK_RANDOM=%s", t);
	pclose(f);

	touch("BitKeeper/etc/modules", 0666);
	putenv("_BK_NO_UNIQ=1");
	system("bk new -qP BitKeeper/etc/modules");
}

private void
push_hash2lines(hash *h, char *key, char *line)
{
	char	***lines;

	/* add empty list if needed and load h->vptr */
	hash_insert(h, key, strlen(key)+1, 0, sizeof(char **));

	lines = h->vptr;
	if (line) {
		*lines = addLine(*lines, strdup(line));
	}
}
