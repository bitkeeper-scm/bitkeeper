#include "system.h"
#include "sccs.h"
#include "resolve.h"

private	char	*getgfile(sccs *s, char *rev);
private	int	do_merge(char *files[3]);
private	int	do_setmerge(char *files[3]);

/* usage bk merge L G R M */
int
merge_main(int ac, char **av)
{
	int	freefiles = 0;
	int	c, i, ret;
	int	setmerge = 0;
	sccs	*s;
	char	*sname;
	names	*n;
	char	*files[3];

	while ((c = getopt(ac, av, "s")) != -1) {
		switch(c) {
		    case 's': setmerge = 1; break;
		    default:
usage:			system("bk help merge");
			exit(1);
		}
	}
	if (av[optind] && !av[optind+1]) {
		unless (sname = name2sccs(av[optind])) goto usage;
		unless (s = sccs_init(sname, 0)) goto usage;
		unless (n = res_getnames(sccs_Xfile(s, 'r'), 'r')) goto usage;
		files[0] = getgfile(s, n->local);
		/* XXX is this the "right" gca? */
		files[1] = getgfile(s, n->gca);
		files[2] = getgfile(s, n->remote);
		freenames(n, 1);
		free(sname);
		sccs_free(s);
		freefiles = 1;
	} else {
		for (i = 0; i < 3; i++) {
			files[i] = av[optind + i];
			unless (files[i]) goto usage;
		}
		unless (av[optind + 3] && !av[optind + 4]) goto usage;
		/* redirect stdout to file */
		freopen(av[optind + 3], "w", stdout);
	}
	if (setmerge) {
		ret = do_setmerge(files);
	} else {
		ret = do_merge(files);
	}
	if (freefiles) {
		for (i = 0; i < 3; i++) {
			unlink(files[i]);
			free(files[i]);
		}
	}
	return (ret);
}

private int
do_merge(char *files[3])
{
	int	rc;
	char	*new_av[8];

	new_av[0] = "bk";
	new_av[1] = "diff3";
	new_av[2] = "-E";
	new_av[3] = "-am";
	new_av[4] = files[0];
	new_av[5] = files[1];
	new_av[6] = files[2];
	new_av[7] = 0;

	rc = spawnvp_ex(_P_WAIT, new_av[0], new_av);

	unless (WIFEXITED(rc)) return (-1);
	return (WEXITSTATUS(rc));
}

private int
do_setmerge(char *files[3])
{
	MDBM	*db = mdbm_mem();
	int	i;
	u8	*p;
	FILE	*f;
	char	**lines = 0;
	kvpair	kv;
	char	buf[MAXLINE];

	for (i = 0; i < 3; i++) {
		unless (f = fopen(files[i], "r")) {
			fprintf(stderr, "merge: can't open %s\n", files[i]);
			exit(1);
		}
		while (fnext(buf, f)) {
			chomp(buf);

			/* XXX p = hash_fetchAlloc(db, buf, 0, 1) */
			unless (p = mdbm_fetch_str(db, buf)) {
				mdbm_store_str(db, buf, "", MDBM_INSERT);
				p = mdbm_fetch_str(db, buf);
				assert(p && (p[0] == 0));
			}
			*p |= (1<<i);
		}
		fclose(f);
	}
	EACH_KV(db) {
		int	val, g, l ,r;
		val = *(u8 *)kv.val.dptr;
		l = (val >> 0) & 1;
		g = (val >> 1) & 1;
		r = (val >> 2) & 1;
		if (g ? (l & r) : (l | r)) lines = addLine(lines, kv.key.dptr);
	}
	sortLines(lines, 0);	/* not needed, but nice to people */
	EACH (lines) {
		printf("%s\n", lines[i]);
	}
	freeLines(lines, 0);
	mdbm_close(db);
	return (0);
}

private char *
getgfile(sccs *s, char *rev)
{
	char	*tmpf = bktmp(0, 0);
	char	*inc = 0, *exc = 0;
	int	flags = SILENT|PRINT;

	if (sccs_get(s, rev, 0, inc, exc, flags, tmpf)) {
		fprintf(stderr, "Fetch of rev %s of %s failed!\n",
		    rev, s->gfile);
		exit(1);
	}
	return (tmpf);
}
