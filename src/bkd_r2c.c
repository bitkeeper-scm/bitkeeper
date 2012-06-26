#include "system.h"
#include "sccs.h"
#include "range.h"

private char * r2c(char *file, char *rev);

int
r2c_main(int ac, char **av)
{
	int	c;
	char	*rev = 0, *file = 0;
	char	*p;
	int	rc = 1;
	int	product = 1;
	MDBM	*idDB, *goneDB;
	char	*sfile;
	longopt	lopts[] = {
		{ "standalone", 'S' },		/* treat comps as standalone */
		{ 0, 0 }
	};

	while ((c = getopt(ac, av, "PRr;S", lopts)) != -1) {
		switch (c) {
		    case 'P':	break;			// do not doc
		    case 'R':				// do not doc
		    case 'S': product = 0; break;
		    case 'r': rev = strdup(optarg); break;
		    default:
			usage();
			break;
		}
	}
	unless ((file = av[optind]) && !av[optind+1]) usage();
	sfile = name2sccs(file);
	if (!isreg(sfile) && isKey(file)) {
		proj_cd2root();
		idDB = loadDB(IDCACHE, 0, DB_IDCACHE);
		goneDB = loadDB(GONE, 0, DB_GONE);

		file = key2path(file, idDB, goneDB, 0);
		mdbm_close(idDB);
		mdbm_close(goneDB);
		unless (file) goto out;
	}
	unless (p = r2c(file, rev)) goto out; /* will chdir to file repo */
	free(rev);
	rev = p;
	if (product && proj_isComponent(0)) {
		p = proj_relpath(proj_product(0), proj_root(0));
		file = aprintf("%s/ChangeSet", p);
		free(p);
		proj_cd2product();
		unless (p = r2c(file, rev)) {
			free(file);
			goto out;
		}
		free(file);
		free(rev);
		rev = p;
	}
	printf("%s\n", rev);
	rc = 0;
out:	if (rev) free(rev);
	free(sfile);
	return (rc);
}

/*
 * Run r2c on file@rev and return the appropriate
 * rev. If run on a component ChangeSet file, it
 * will return the rev of the product's ChangeSet
 * that corresponds to this component's ChangeSet
 * rev.
 *
 * Errors are printed on err (if not null).
 */
private char *
r2c(char *file, char *rev)
{
	int	len;
	char	*name, *t;
	delta	*e;
	FILE	*f = 0;
	sccs	*s = 0, *cset = 0;
	char	*ret = 0, *key = 0, *shortkey = 0;
	char	tmpfile[MAXPATH] = {0};
	char	buf[MAXKEY*2];
	RANGE	rargs = {0};

	name = name2sccs(file);
	unless (s = sccs_init(name, INIT_NOCKSUM|INIT_MUSTEXIST)) {
		fprintf(stderr, "%s: cannot init %s\n", prog, file);
		goto out;
	}
	if (chdir(proj_root(s->proj))) {
		fprintf(stderr, "%s: cannot find package root.\n", prog);
		goto out;
	}
	if (CSET(s) && proj_isComponent(s->proj)) {
		/* go to product */
		if (proj_cd2product()) goto out;
	}
	unless (e = sccs_findrev(s, rev)) {
		fprintf(stderr, "%s: cannot find rev %s in %s\n",
		    prog, rev, file);
		goto out;
	}
	unless (e = sccs_csetBoundary(s, e)) {
		fprintf(stderr,
		    "%s: cannot find cset marker at or below %s in %s\n",
		    prog, rev, file);
		goto out;
	}
	sccs_sdelta(s, e, buf);
	key = strdup(buf);
	if (t = sccs_iskeylong(buf)) {
		*t = 0;
		shortkey = strdup(buf);
	} else {
		shortkey = 0;
	}
	strcpy(buf, CHANGESET);
	unless (cset = sccs_init(buf, INIT_NOCKSUM)) {
		fprintf(stderr, "%s: cannot init ChangeSet\n", prog);
		goto out;
	}
	unless (bktmp(tmpfile, "r2c")) {
		fprintf(stderr, "%s: could not create %s\n", prog, tmpfile);
		goto out;
	}
	if (range_process("r2c", cset, RANGE_SET, &rargs)) goto out;
	if (sccs_cat(cset, PRINT|GET_NOHASH|GET_REVNUMS, tmpfile)) {
		unless (BEEN_WARNED(s)) {
			fprintf(stderr, "%s: annotate of ChangeSet failed.\n",
			    prog);
		}
		goto out;
	}
	unless (f = fopen(tmpfile, "r")) {
		perror(tmpfile);
		goto out;
	}
	len = strlen(key);
	while (fnext(buf, f)) {
		/* 1.5\tkey key */
		t = separator(buf); assert(t); t++;
		if (strneq(t, key, len) && t[len] == '\n') {
			t = strchr(buf, '\t'); assert(t); *t = 0;
			ret = aprintf("%s", buf);
			goto out;
		}
	}
	unless (shortkey) {
notfound:	if (shortkey) {
			fprintf(stderr,
			    "%s: cannot find either of\n\t%s\n\t%s\n",
			    prog, key, shortkey);
		} else {
			fprintf(stderr,
			    "%s: cannot find\n\t%s\n", prog, key);
		}
		goto out;
	}
	rewind(f);
	len = strlen(shortkey);
	while (fnext(buf, f)) {
		/* 1.5 key key */
		t = separator(buf); assert(t); t++;
		if (strneq(t, shortkey, len) && t[len] == '\n') {
			t = strchr(buf, '\t'); assert(t); *t = 0;
			ret = aprintf("%s", buf);
			goto out;
		}
	}
	goto notfound;
out:	free(name);
	if (key) free(key);
	if (shortkey) free(shortkey);
	sccs_free(s);
	sccs_free(cset);
	if (f) fclose(f);
	if (tmpfile[0]) unlink(tmpfile);
	return (ret);
}

