/*
 * Copyright 2000,2005-2006,2009-2011,2015-2016 BitMover, Inc
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "system.h"
#include "sccs.h"
#include "resolve.h"

typedef struct {
	u32	setmerge:1;
	u32	hashmerge:1;
} Opts;

private	char	*getgfile(sccs *s, char *rev);
private	int	do_hashmerge(Opts *opts, char *files[3]);
private	int	do_merge(char *files[3]);
private	int	do_setmerge(char *files[3]);
private char	*merge_strings(Opts *opts, char *l, char *g, char *r);


/* usage bk merge L G R M */
int
merge_main(int ac, char **av)
{
	int	freefiles = 0;
	int	c, i, ret;
	sccs	*s;
	ser_t	l, g, r;
	char	*inc, *exc;
	char	*sname;
	Opts	*opts;
	char	*files[3];

	opts = new(Opts);
	while ((c = getopt(ac, av, "hs", 0)) != -1) {
		switch(c) {
		    case 'h': opts->hashmerge = 1; break;
		    case 's': opts->setmerge = 1; break;
		    default: bk_badArg(c, av);
		}
	}
	if (av[optind] && !av[optind+1]) {
		unless (sname = name2sccs(av[optind])) usage();
		unless (s = sccs_init(sname, 0)) usage();
		unless (sccs_findtips(s, &l, &r)) usage();
		unless (g = sccs_gca(s, l, r, &inc, &exc)) usage();
		if (inc) free(inc);
		if (exc) free(exc);
		files[0] = getgfile(s, REV(s, l));
		files[1] = getgfile(s, REV(s, g));
		files[2] = getgfile(s, REV(s, r));
		free(sname);
		sccs_free(s);
		freefiles = 1;
	} else {
		for (i = 0; i < 3; i++) {
			files[i] = av[optind + i];
			unless (files[i]) usage();
		}
		unless (av[optind + 3] && !av[optind + 4]) usage();
		/* redirect stdout to file */
		freopen(av[optind + 3], "w", stdout);
	}
	if (opts->hashmerge) {
		ret = do_hashmerge(opts, files);
	} else if (opts->setmerge) {
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

	rc = spawnvp(_P_WAIT, new_av[0], new_av);

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
	char	*tmpf = bktmp(0);
	char	*inc = 0, *exc = 0;
	int	flags = SILENT;

	if (sccs_get(s, rev, 0, inc, exc, flags, tmpf, 0)) {
		fprintf(stderr, "Fetch of rev %s of %s failed!\n",
		    rev, s->gfile);
		exit(1);
	}
	return (tmpf);
}

/* compare hash data */
#define	hdcmp(h1, h2) (((h1)->vlen == (h2)->vlen) && \
	    !memcmp((h1)->vptr, (h2)->vptr, (h1)->vlen))

private int
do_hashmerge(Opts *opts, char *files[3])
{
	hash	*h[3], *hout;
	FILE	*f;
	int	i;

	for (i = 0; i < 3; i++) {
		unless (f = fopen(files[i], "r")) {
			fprintf(stderr, "%s: unable to open %s\n",
			    prog, files[i]);
			return (2);
		}
		h[i] = hash_new(HASH_MEMHASH);
		hash_fromStream(h[i], f);
		fclose(f);
	}
	hout = hash_new(HASH_MEMHASH);
	EACH_HASH(h[0]) {
		assert(h[0]->vlen > 0); /* needed for hdcmp */
		hash_fetchStr(h[1], h[0]->kptr);	/* get gca */
		/* key exists on left */
		if (hash_fetchStr(h[2], h[0]->kptr)) {
			/* and on right */
			if (hdcmp(h[0], h[2])) {
				/* l==r so keep l */
				hash_store(hout,
				    h[0]->kptr, h[0]->klen,
				    h[0]->vptr, h[0]->vlen);
			} else if (hdcmp(h[0], h[1])) {
				/* l==g so keep r */
				hash_store(hout,
				    h[2]->kptr, h[2]->klen,
				    h[2]->vptr, h[2]->vlen);
			} else if (hdcmp(h[2], h[1])) {
				/* r==g so keep l */
				hash_store(hout,
				    h[0]->kptr, h[0]->klen,
				    h[0]->vptr, h[0]->vlen);
			} else {
				char	*t;

				// conflict
				t = merge_strings(opts,
				    h[0]->vptr, h[1]->vptr, h[2]->vptr);
				hash_storeStr(hout, h[0]->kptr, t);
				free(t);
			}
			/* remove from h[2] for loop below */
			hash_deleteStr(h[2], h[0]->kptr);
		} else {
			/* not on right */

			if (hdcmp(h[0], h[1])) {
				/* right deleted, left unmodified => delete */
			} else {	
				/* new or modified from gca => keep */
				hash_store(hout,
				    h[0]->kptr, h[0]->klen,
				    h[0]->vptr, h[0]->vlen);
			}
		}
	}

	/* now pickup stuff in r and not in l */
	EACH_HASH(h[2]) {
		hash_fetchStr(h[1], h[2]->kptr);

		if (hdcmp(h[2], h[1])) {
			/* left deleted, right unmodified => delete */
		} else {	
			/* new or modified from gca => keep */
			hash_store(hout,
			    h[2]->kptr, h[2]->klen,
			    h[2]->vptr, h[2]->vlen);
		}
	}
	for (i = 0; i < 2; i++) hash_free(h[i]);

	hash_toStream(hout, stdout);
	hash_free(hout);
	return (0);
}

private char *
merge_strings(Opts *opts, char *l, char *g, char *r)
{
	char	*files[3], *of;
	int	fd1, i;
	char	*out;

	files[0] = bktmp(0);
	Fprintf(files[0], "%s", l);
	files[1] = bktmp(0);
	if (g) Fprintf(files[1], "%s", g);
	files[2] = bktmp(0);
	Fprintf(files[2], "%s", r);

	of = bktmp(0);
	fflush(stdout);
	fd1 = dup(1);
	close(1);
	open(of, O_WRONLY|O_CREAT, 0666);

	if (opts->setmerge) {
		do_setmerge(files);
	} else {
		do_merge(files);
	}
	fflush(stdout);
	close(1);
	dup2(fd1, 1);
	close(fd1);

	out = loadfile(of, 0);
	for (i = 0; i < 3; i++) {
		unlink(files[i]);
		free(files[i]);
	}
	unlink(of);
	free(of);
	return (out);
}
