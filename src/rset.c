/*
 * Copyright 2000-2016 BitMover, Inc
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

#include "sccs.h"
#include "nested.h"
#include "range.h"

typedef struct {
	u32	show_all:1;	/* -a show deleted files */
	u32	show_gone:1;	/* --show-gone: display gone files */
	u32	show_diffs:1;	/* -r output in rev diff format */
	u32	show_path:1;	/* -h show PATHNAME(s, d) */
	u32	hide_bk:1;	/* --hide-bk: hide BitKeeper/ files */
	u32	hide_cset:1;	/* -H hide ChangeSet file from file list */
	u32	hide_comp:1;	/* -H also hide component csets */
	u32	BAM:1;		/* -B only list BAM files */
	u32	nested:1;	/* -P recurse into nested components */
	u32	standalone:1;	/* -S standalone */
	u32	elide:1;	/* elide files with no diffs (e.g. reverted) */
	u32	prefix:1;	/* add component prefix */
	u32	compat:1;	/* --compat: old bugs made new */
	u32	chksum:1;	/* checksum diff */
	u32	stats:1;	/* output internal work number(s) */
	u32	no_print:1;	/* for getting stats, squelch data */
	u32	subdirs:1;	/* with --dir show subdirs */
	char	**nav;
	char	**aliases;	/* -s limit nested output to aliases */
	char	*limit_dir;
	nested	*n;		/* only if -s (and therefore, -P) */
	RANGE	rargs;		/* revisions */
	sccs	*s;		/* pass through */
	hash	*seen_dir;
} Opts;

/*
 * Cset info accumulated in a weave walk
 */
typedef struct {
	char	*left1;		/* cset rev of newest lower bound */
	char	*left2;		/* cset rev of oldest lower bound */
	char	*right;		/* cset rev of upper bound */
	char	*leftpath1;	/* historical path */
	char	*leftpath2;	/* historical path */
	char	*rightpath;	/* historical path */
	char	*curpath;	/* current path */
	hash	*keys;		/* hash of rkoff -> struct rfile */
	int	weavelines;	/* stats: count how many weave lines read */

	/* used by --checksum to get some integrity check */
	sum_t	wantsum;	/* used in chksum calc - sum of right */
	sum_t	sum;		/* computed sum for --checksum */
} rset;

/* per-file companion struct to above; the value in '->keys' hash */
typedef struct {
	u32	rkoff;		/* offset to rootkey */
	u32	tipoff;		/* newest dk for this rootkey (for curpath) */
	u32	left1;		/* newest lowerbound key */
	u32	left2;		/* oldest lowerbound key */
	u32	right;		/* upper bound key */
	u8	seen;		/* LEFT1, LEFT2, RIGHT bit map */
#define	LEFT1	0x01
#define	LEFT2	0x02
#define	RIGHT	0x04
#define	LAST	0x08
} rfile;

private void	rel_diffs(Opts *opts, rset *data);
private void	rel_list(Opts *opts, rset *data);
private	sum_t	dodiff(sccs *s, u32 rkoff, u32 loff, u32 roff);
private rset	*weaveExtract(sccs *s,
		    ser_t left1, ser_t left2, ser_t right, Opts *opts);
private	rset	*initrset(sccs *s,
		    ser_t left1, ser_t left2, ser_t right, Opts *opts);
private	void	freeRset(rset *data);
private	int	deltaSkip(sccs *cset, MDBM *sDB, u32 rkoff, u32 dkoff);
private	char	*dirSlash(char *path);

int
rset_main(int ac, char **av)
{
	int	c;
	comp	*cp;
	int	i;
	char	*p;
	char	*freeme = 0;
	char	s_cset[] = CHANGESET;
	sccs	*s = 0;
	int	rc = 1;
	rset	*data = 0;
	Opts	*opts;
	longopt	lopts[] = {
		{ "checksum", 304 },	// bk rset --show-gone --checksum -ra,b
		{ "elide", 305},	// don't show files without diffs
		{ "hide-bk", 310 },	// strip all BitKeeper/ files (export)
		{ "compat", 320 },	// compat/broken output (undoc)
		{ "prefix", 330 },	// add component prefix (doc)?
		{ "show-gone", 350 },	// undoc, not sure
		{ "stats", 360 },	// undoc, how much we read
		{ "no-print", 370 },	// don't print anything (use w/ stats)
		{ "dir;", 380 },
		{ "subdirs", 390 },
		{ "subset;", 's' },	// aliases
		{ "standalone", 'S' },	// this repo only
		{ 0, 0 }
	};

	opts = new(Opts);
	while ((c = getopt(ac, av, "5aBhHl;PR;r;Ss;U", lopts)) != -1) {
		unless (c == 'r' || c == 'P' || c == 'l' || c == 's' ||
		    c == 'S') {
			opts->nav = bk_saveArg(opts->nav, av, c);
		}
		switch (c) {
		case '5':	break; /* ignored, always on */
		case 'B':	opts->BAM = 1; break;		/* undoc */
		case 'a':					/* doc 2.0 */
				opts->show_all = 1;  /* show deleted files */
				break;
		case 'h':					/* doc 2.0 */
				opts->show_path = 1; /* show historic path */
				break;
		case 'H':					/* undoc 2.0 */
				opts->hide_cset = 1; /* hide ChangeSet file */
				opts->hide_comp = 1; /* hide components  */
				break;
		case 'l':	if (opts->rargs.rstart) usage();
				goto range;
		case 'P':	break;			/* old, compat */
		case 'r':	if (opts->rargs.rstart && !opts->show_diffs) {
					usage();
				}
				opts->show_diffs = 1;		/* doc 2.0 */
range:				if (range_addArg(&opts->rargs, optarg, 0)) {
					usage();
				}
				break;
		case 'R':	if (opts->rargs.rstart) usage();
				opts->show_diffs = 1;
				goto range;
	        case 'S':	opts->standalone = 1; break;
		case 's':	opts->aliases = addLine(opts->aliases,
					strdup(optarg));
				break;
		case 'U':	opts->hide_cset = 1;
				opts->hide_comp = 1;
				opts->hide_bk = 1; break;
				break;
		case 304:  // --checksum
				opts->chksum = 1;
				break;
		case 305:  // --elide
				opts->elide = 1; break;
		case 310:  // --hide-bk
				opts->hide_bk = 1; break;
		case 320:  // --compat
				opts->compat = 1; break;
		case 330:  // --prefix
				opts->prefix = 1; break;
		case 350:  // --show-gone
				opts->show_gone = 1; break;
		case 360:  // --stats
				opts->stats = 1; break;
		case 370:  // --no-print
				opts->no_print = 1;
				opts->hide_cset = 1;
				break;
		case 380: // --dir=LIMIT_DIR
				opts->limit_dir = optarg;
				break;
		case 390: // --subdirs
				opts->subdirs = 1;
				break;
		default:        bk_badArg(c, av);
		}
	}
	/* must have either a -l or -r */
	unless (opts->rargs.rstart) usage();
	if (opts->show_diffs) {
		/* compat: only lower bound and has 1 comma: really a range */
		if (!opts->rargs.rstop &&
		    (p = strchr(opts->rargs.rstart, ','))) {
			*p++ = 0;
			opts->rargs.rstop = p;
		}
	} else if (opts->rargs.rstop) {
		/* -l (!show_diffs) and range (stop defined) is illegal */
		usage();
	}
	unless (opts->rargs.rstop) {
		/* for -l and -r single arg, make it the upper bound */
		opts->rargs.rstop = opts->rargs.rstart;
		opts->rargs.rstart = 0;
	}
	/* weed out illegal upper bound; must be single rev */
	if (opts->rargs.rstop && strchr(opts->rargs.rstop, ',')) {
		usage();
	}
	if (opts->subdirs && !opts->limit_dir) usage();
	if (opts->standalone && opts->aliases) usage();
	opts->nested = bk_nested2root(opts->standalone);
	if (opts->nested && opts->limit_dir) {
		// If you know what directory you want, then running over
		// all components is stupid
		fprintf(stderr, "%s: --dir=DIR requires -S\n", prog);
		goto out;
	}
	/* Let them use -sHERE by default but ignore it in non-products. */
	if (!opts->nested && opts->aliases) {
		EACH(opts->aliases) {
			unless (streq(opts->aliases[i], ".") ||
			    strieq(opts->aliases[i], "HERE")) {
				fprintf(stderr,
				    "%s: -sALIAS only allowed in product\n",
				    prog);
				goto out;
			}
		}
		freeLines(opts->aliases, free);
		opts->aliases = 0;
	}
	opts->s = s = sccs_init(s_cset, SILENT);
	assert(s);
	s->idDB = loadDB(IDCACHE, 0, DB_IDCACHE);
	assert(s->idDB);
	/* special case - if diff and no lower bound, then parents */
	if (opts->show_diffs && !opts->rargs.rstart) {
		char	*revP = 0, *revM = 0;

		if (sccs_parent_revs(s, opts->rargs.rstop, &revP, &revM)) {
			goto out;
		}
		if (revM && !opts->chksum) {
			assert(revP);
			opts->rargs.rstart = freeme =
			    aprintf("%s,%s", revP, revM);
			free(revP);
		} else if (revP) {
			opts->rargs.rstart = freeme = revP;
		}
		free(revM);
	}
	/* hack: librange doesn't like an empty lower bound for endpoints */
	unless (opts->rargs.rstart) opts->rargs.rstart = "1.0";
	if (range_process(
	    prog, s, (RANGE_ENDPOINTS|RANGE_RSTART2), &opts->rargs)) {
		goto out;
	}

	unless (data =
	    weaveExtract(s, s->rstart, s->rstart2, s->rstop, opts)) {
		goto out;
	}

	if (opts->aliases) {
		unless ((opts->n = nested_init(s, 0, 0, NESTED_PENDING)) &&
		    !nested_aliases(opts->n, 0, &opts->aliases, start_cwd, 1)) {
			goto out;
		}
		c = 0;
		EACH_STRUCT(opts->n->comps, cp, i) {
			if (cp->alias && !C_PRESENT(cp)) {
				unless (c) {
					c = 1;
					fprintf(stderr,
					    "%s: error, the following "
					    "components are not populated:\n",
					    prog);
				}
				fprintf(stderr, "\t./%s\n", cp->path);
			}
		}
		if (c) goto out;
		unless (opts->n->product->alias) opts->hide_cset = 1;
	}
	if (opts->show_diffs) {
		rel_diffs(opts, data);
	} else {
		rel_list(opts, data);
	}
	if (opts->chksum) {
		if (data->sum != data->wantsum) {
			fprintf(stderr,
			    "### Checksum in %s for %s\n",
			    data->curpath ? data->curpath : "repo",
			    data->right);
			fprintf(stderr,
			    "want sum\t%u\ngot sum\t\t%u\n",
			    data->wantsum, data->sum);
			rc = 1;
			goto out;
		}
	}
	if (opts->stats) {
		fprintf(stderr, "%8u weave data lines read in %s for %s\n",
		    data->weavelines,
		    data->curpath ? data->curpath : "repo",
		    data->right);
	}
	rc = 0;
out:
	if (freeme) free(freeme);
	if (data) freeRset(data);
	if (opts->s) sccs_free(opts->s);
	if (opts->aliases) freeLines(opts->aliases, free);
	if (opts->n) nested_free(opts->n);
	free(opts);
	return (rc);
}

/*
 * See if there was a change in the file or not. Since the data is
 * most likely coming from weaveExtract(), a file where nothing
 * changed will have the same deltakey in left1 and right with no
 * left2, or the same in all keys. That means no change so it should
 * not be in the diff.
 */
#define	UNCHANGED(file) (							\
      ((((file)->left1 == (file)->right) &&				\
       !(file)->left2) ||						\
	(((file)->left2 == (file)->right) &&				\
	 (!(file)->left1 || ((file)->left1 == (file)->left2))))		\
)									\

/*
 * List the rset output for a cset range left1,left2..right
 * For compactness, keep things in heap offset form.
 */
rset_df *
rset_diff(sccs *cset, ser_t left1, ser_t left2, ser_t right, int showgone)
{
	Opts	opts = {0};
	rset	*data = 0;
	rfile	*file;
	rset_df	*diff = 0, *d;

	opts.show_gone = showgone;
	data = weaveExtract(cset, left1, left2, right, &opts);
	assert(data);

	EACH_HASH(data->keys) {
		file = (rfile *)data->keys->vptr;
		if (UNCHANGED(file)) continue;
		d = addArray(&diff, 0);
		d->rkoff = file->rkoff;
		d->dkleft1 = file->left1;
		d->dkleft2 = file->left2;
		d->dkright = file->right;
	}

	freeRset(data);
	return (diff);
}

/*
 * Get checksum of cset d relative to cset base (or root if no base)
 */
u32
rset_checksum(sccs *cset, ser_t d, ser_t base)
{
	Opts	opts = {0};
	rset	*data = 0;
	rfile	*file;
	u32	sum = 0;

	opts.chksum = 1;

	if (base && (base = sccs_getCksumDelta(cset, base))) {
		sum = SUM(cset, base);
	}

	data = weaveExtract(cset, base, 0, d, &opts);
	assert(data);

	EACH_HASH(data->keys) {
		file = (rfile *)data->keys->vptr;
		sum += dodiff(cset, file->rkoff, file->left1, file->right);
	}
	sum = (sum_t)sum;	/* truncate */

	freeRset(data);
	return (sum);
}

/*
 * return true if the keys v1 and v2 is the same
 * XXX TODO: We may need to call samekeystr() in slib.c
 */
private int
is_same(sccs *s, u32 v1, u32 v2)
{
	if (v1 == v2) return (1);
	if (!v2) return (0);
	if (!v1) return (0);
	/* check for poly, since delta keys are not unique in heap */
	return (streq(HEAP(s, v1), HEAP(s, v2)));
}

/*
 * return true if file is deleted
 */
int
isNullFile(char *rev, char *file)
{
	if (strneq(file, "BitKeeper/deleted/", strlen("BitKeeper/deleted/"))) {
		return (1);
	}
	if (streq(rev, "1.0")) return (1);
	return (0);
}

/*
 * Return a malloced string with the pathname where a file
 * lives currently.
 * Returns 0 on error
 * Returns INVALID for a non-present component
 */
private char *
findPath(Opts *opts, rfile *file)
{
	char	*path;
	char	*t;
	char	*rkey = HEAP(opts->s, file->rkoff);

	/* path == path where file lives currently */
	if (path = mdbm_fetch_str(opts->s->idDB, rkey)) {
		path = strdup(path);
	} else {
		if (opts->show_gone) {
			/* or path where file would be if it was here */
			t = HEAP(opts->s, file->tipoff);
			assert(t);
			unless (path = key2path(t, 0, 0, 0)) {
				fprintf(stderr, "rset: Can't find %s\n", t);
				return (0);
			}
		} else {
			unless (path =
			    key2path(rkey, 0, 0, 0)) {
				fprintf(stderr,
				    "rset: Can't find %s\n", rkey);
				return (0);
			}
			if (streq(path, GCHANGESET) && proj_isProduct(0) &&
			    !streq(rkey, proj_rootkey(0))) {
				/*
				 * skip missing components
				 * This assumes missing components
				 * won't be in the idcache.
				 */
				free(path);
				return (INVALID);
			}
		}
	}
	return (path);
}
private int
isIdenticalData(sccs *cset, char *file, u32 start, u32 start2, u32 end)
{
	sccs	*s;
	ser_t	d1, d2;
	FILE	*file_1 = 0, *file_2 = 0;
	char	*f1d, *f2d;
	size_t	f1s, f2s;
	char	*sfile;
	int	same = 0;

	sfile = name2sccs(file);
	s = sccs_init(sfile, INIT_MUSTEXIST|INIT_NOCKSUM);
	free(sfile);
	unless (s) return (0);
	unless (start2) {
		/* try to avoid generating files .. */
		d1 = sccs_findrev(s, HEAP(cset, start));
		d2 = sccs_findrev(s, HEAP(cset, end));
		unless (d1 && d2) goto out;
		d1 = sccs_getCksumDelta(s, d1);
		d2 = sccs_getCksumDelta(s, d2);
		/* .. if we know they will be the same */
		if (d1 == d2) {
			same = 1;
			goto out;
		}
		/* .. or if we know they will be different */
		if (SUM(s, d1) != SUM(s, d2)) goto out;
	}
	file_1 = fmem();
	if (sccs_get(s, HEAP(cset, start),
	    start2 ? HEAP(cset, start2) : 0,
	    0, 0, SILENT, 0, file_1)) {
		goto out;
	}
	file_2 = fmem();
	if (sccs_get(s, HEAP(cset, end), 0, 0, 0, SILENT, 0, file_2)) {
		goto out;
	}
	f1d = fmem_peek(file_1, &f1s);
	f2d = fmem_peek(file_2, &f2s);
	same = ((f1s == f2s) && !memcmp(f1d, f2d, f1s));
out:
	sccs_free(s);
	if (file_1) fclose(file_1);
	if (file_2) fclose(file_2);
	return (same);
}


/*
 * Convert root/start/end keys to sfile|path1|rev1|path2|rev2 format
 * If we are rset -l<rev> then start is null, and end is <rev>.
 * If we are rset -r<rev1>,<rev2> then start is <rev1> and end is <rev2>.
 */
private void
process(Opts *opts, rset *data, rfile *file)
{
	comp	*c;
	char	*path0 = 0, *path1 = 0, *path1_2 = 0, *path2 = 0;
	u32	start, start2, end;
	char	*comppath, *comppath2;
	char	*here, *t;
	int	i;
	u32	rkoff = file->rkoff;
	char	smd5[MD5LEN], smd5_2[MD5LEN], emd5[MD5LEN];

	end = file->right;
	if (file->left1) {
		start = file->left1;
		start2 = file->left2;
		comppath = data->leftpath1;
		comppath2 = data->leftpath2;
	} else {
		start = file->left2;
		start2 = 0;
		comppath = data->leftpath2;
		comppath2 = 0;
	}
	/* collapse duplicates on left side, except when comp path is diff */
	if (start && start2 &&
	    (!opts->show_path || !comppath || streq(comppath, comppath2)) &&
	    is_same(opts->s, start, start2)) {
		start2 = 0;
	}

	if (start && end && !start2) {
		if (is_same(opts->s, start, end)) return;
	}
	if (opts->n) { /* opts->n only set if in PRODUCT and with -s aliases*/
		/* we're rummaging through all product files/components */
		if (c = nested_findKey(opts->n, HEAP(opts->s, rkoff))) {
			/* this component is not selected by -s */
			unless (c->alias) return;
		} else {
			/* if -s^PRODUCT skip product files */
			unless (opts->n->product->alias) return;
		}
	}
	/* path0 == path where file lives currently */
	unless (path0 = findPath(opts, file)) return;
	if (path0 == INVALID) {
		/*
		 * skip missing components
		 * This assumes missing components
		 * won't be in the idcache.
		 */
		end = 0;
		path0 = 0;
		goto done;
	}
	if (opts->show_diffs) {
		if (start) {
			sccs_key2md5(HEAP(opts->s, start), smd5);
		} else {
			start = rkoff;
			strcpy(smd5, "1.0");
			comppath = data->leftpath1;
		}
		path1 = key2path(HEAP(opts->s, start), 0, 0, 0);
		if (start2) {
			sccs_key2md5(HEAP(opts->s, start2), smd5_2);
			path1_2 = key2path(HEAP(opts->s, start2), 0, 0, 0);
		}
	}
	if (end) {
		sccs_key2md5(HEAP(opts->s, end), emd5);
	} else {
		end = rkoff;
		strcpy(emd5, "1.0");
	}
	path2 = key2path(HEAP(opts->s, end), 0, 0, 0);
	if (opts->no_print) goto done;
	if (opts->BAM && !weave_isBAM(opts->s, rkoff)) goto done;
	if (!opts->show_all && sccs_metafile(path0)) goto done;
	if (opts->hide_comp && streq(basenm(path0), GCHANGESET)) goto done;
	unless (opts->show_all) {
		if (opts->show_diffs) {
			if (isNullFile(smd5, path1) &&
			    (!start2 || isNullFile(smd5_2, path1_2)) &&
			    isNullFile(emd5, path2)) {
				goto done;
			}
			if (opts->hide_bk &&
			    strneq(path1, "BitKeeper/", 10) &&
			    (!start2 || strneq(path1_2, "BitKeeper/", 10)) &&
			    strneq(path2, "BitKeeper/", 10)) {
				goto done;
			}
		} else {
			if (isNullFile(emd5, path2)) goto done;
			if (opts->hide_bk && strneq(path2, "BitKeeper/", 10)) {
				goto done;
			}
		}
	}
	if (opts->elide && !streq(basenm(path0), GCHANGESET) &&
	    isIdenticalData(opts->s, path0, start, start2, end)) {
		goto done;
	}
	if (opts->limit_dir) {
		char	*dir = dirname_alloc(path2);
		int	match = streq(dir, opts->limit_dir);
		int	c;
		char	*p;

		c = 0;
		if (!match && opts->subdirs &&
		    (streq(opts->limit_dir, ".") ||
		     ((c = paths_overlap(opts->limit_dir, dir)) &&
			 !opts->limit_dir[c]))) {
			if (dir[c] == '/') c++;
			if (p = strchr(dir+c, '/')) *p = 0;
			unless (opts->seen_dir) {
				opts->seen_dir = hash_new(HASH_MEMHASH);
			}
			if (hash_insertStr(opts->seen_dir, dir+c, 0)) {
				printf("|%s\n", dir+c);
			}
		}
		free(dir);
		unless (match) goto done;
	}
	if (data->curpath) fputs(data->curpath, stdout);
	printf("%s|", path0);
	if (opts->show_diffs) {
		if (opts->show_path) {
			if (comppath) fputs(comppath, stdout);
			printf("%s|%s|", path1, smd5);
			if (start2) {
				if (comppath2) fputs(comppath2, stdout);
				printf("%s|%s|", path1_2, smd5_2);
			}
		} else {
			fputs(smd5, stdout);
			if (start2) {
				printf(",%s", smd5_2);
			}
			fputs("..", stdout);
		}
	}
	if (opts->show_path) {
		if (data->rightpath) fputs(data->rightpath, stdout);
		printf("%s|", path2);
	}
	printf("%s\n", emd5);
	fflush(stdout);

done:	if (opts->nested && end && componentKey(HEAP(opts->s, end))) {
		char	**av;

		/*
		 * call rset recursively here
		 * We silently skip missing components
		 */
		dirname(path0);
		here = strdup(proj_cwd());
		if (chdir(path0)) {
			/* components should all be present */
			free(here);
			goto clear;
		}
		av = addLine(0, strdup("bk"));
		av = addLine(av, strdup("rset"));
		av = addLine(av, strdup("-HS")); /* we already printed cset */
		av = addLine(av, strdup("--prefix"));
		EACH(opts->nav) av = addLine(av, strdup(opts->nav[i]));
		if (opts->show_diffs) {
			if (start2) {
				t = aprintf("-r%s,%s..%s", smd5, smd5_2, emd5);
			} else {
				t = aprintf("-r%s..%s", smd5, emd5);
			}
		} else {
			t = aprintf("-l%s", emd5);
		}
		av = addLine(av, t);
		av = addLine(av, 0);

		fflush(stdout);
		spawnvp(_P_WAIT, "bk", av+1);
		chdir(here);
		free(here);
		freeLines(av, free);
	}
clear:	if (path0) free(path0);
	if (path1) free(path1);
	if (path1_2) free(path1_2);
	if (path2) free(path2);
}

private	sum_t
dodiff(sccs *s, u32 rkoff, u32 loff, u32 roff)
{
	sum_t	sum = 0;
	u8	*p;
	u8	*root_key = HEAP(s, rkoff);
	u8	*left =  loff ? HEAP(s, loff) : 0;
	u8	*right =  roff ? HEAP(s, roff) : 0;

	p = right;
	while (*p) sum += *p++;

	if (left) {
		p = left;
		while (*p) sum -= *p++;
	} else {
		/* new file */
		p = root_key;
		while (*p) sum += *p++;
		sum += ' ' + '\n';
	}
	return (sum);
}

typedef struct {
	char	*path;		/* thing to sort */
	rfile	*file;		/* thing to keep */
	u32	component:1;	/* is this a component? */
} rline;

private int
sortpath(const void *a, const void *b)
{
	rline	*ra = (rline *)a;
	rline	*rb = (rline *)b;

	if (ra->component == rb->component) {
		return (strcmp(ra->path, rb->path));
	}
	return (ra->component - rb->component);
}

private rline *
addRline(rline *list, Opts *opts, rfile *file)
{
	char	*path;
	rline	*item;

	/* path where file lives currently */
	unless (path = findPath(opts, file)) return (0);
	if (path == INVALID) return (list);

	item = addArray(&list, 0);
	item->path = path;
	item->file = file;
	item->component = changesetKey(HEAP(opts->s, file->rkoff));
	return (list);
}

private char **
sortKeys(Opts *opts, rset *data)
{
	int	i;
	char	**keylist = 0;
	rline	*list = 0;
	hash	*db = data->keys;
	rfile	*file;

	EACH_HASH(db) {
		file = db->vptr;
		/*
		 * First round of pruning (more in process())
		 * Prune where left and right have same content.
		 * Keep where left1 == left2, but != right.
		 */
		if (UNCHANGED(file)) continue;
		list = addRline(list, opts, file);
	}
	sortArray(list, sortpath);
	EACH(list) {
		keylist = addLine(keylist, list[i].file);
		FREE(list[i].path);
	}
	FREE(list);
	return (keylist);
}

/*
 * Compute diffs of  rev1 and rev2
 */
private void
rel_diffs(Opts *opts, rset *data)
{
	char	**keylist;
	rfile	*file;
	int	i;

	/*
	 * OK, here is the 2 main loops where we produce the
	 * delta list. (via the process() function)
	 *
	 * Print the ChangeSet file first
	 * XXX This assumes the Changset file never moves.
	 * XXX If we move the ChangeSet file,
	 * XXX this needs to be updated.
	 */
	unless (opts->hide_cset) {
		unless (data->left1) data->left1 = strdup("1.0");
		fputs("ChangeSet|", stdout);
		unless (opts->show_path) {
			if (data->left1) fputs(data->left1, stdout);
			if (data->left2) {
				fputc(opts->compat ? '+' : ',', stdout);
				fputs(data->left2, stdout);
			}
			printf("..%s\n", data->right);
		} else {
			if (data->left1) {
				printf("ChangeSet|%s", data->left1);
			}
			if (data->left2) {
				if (opts->compat) {
					printf("+%s", data->left2);
				} else {
					printf("|ChangeSet|%s", data->left2);
				}
			}
			printf("|ChangeSet|%s\n", data->right);
		}
	}
	keylist = sortKeys(opts, data);

	EACH(keylist) {
		file = (rfile *)keylist[i];
		if (opts->chksum) {
			assert(!file->left2);
			data->sum += dodiff(
			    opts->s, file->rkoff, file->left1, file->right);
		}
		process(opts, data, file);
	}
	freeLines(keylist, 0);
}

private void
rel_list(Opts *opts, rset *data)
{
	char	**keylist;
	int	i;
	rfile	*file;

	unless (opts->hide_cset ||
	    (opts->limit_dir && !streq(opts->limit_dir, "."))) {
		unless (opts->show_path) {
			printf("ChangeSet|%s\n", opts->rargs.rstop);
		} else {
			printf("ChangeSet|ChangeSet|%s\n", opts->rargs.rstop);
		}
	}
	keylist = sortKeys(opts, data);
	EACH(keylist) {
		file = (rfile *)keylist[i];
		if (opts->chksum) {
			data->sum += dodiff(
			    opts->s, file->rkoff, 0, file->right);
		}
		process(opts, data, file);
	}
	freeLines(keylist, 0);
}

/*
 * For a given rev, compute the parent (revP) and the merge parent (RevM)
 */
int
sccs_parent_revs(sccs *s, char *rev, char **revP, char **revM)
{
	ser_t	d, p, m;

	d = sccs_findrev(s, rev);
	unless (d) {
		fprintf(stderr, "diffs: cannot find rev %s\n", rev);
		return (-1);
	}
	/*
	 * XXX TODO: Could we get meta delta in cset?, should we skip them?
	 */
	unless (p = PARENT(s, d)) {
		fprintf(stderr, "diffs: rev %s has no parent\n", rev);
		return (-1);
	}
	assert(!TAG(s, p)); /* parent should not be a meta delta */
	*revP = (p ? strdup(REV(s, p)) : NULL);
	if (MERGE(s, d)) {
		m = MERGE(s, d);
		*revM = (m ? strdup(REV(s, m)) : NULL);
	}
	return (0);
}

private	void
freeRset(rset *data)
{
	unless (data) return;

	free(data->left1);
	free(data->left2);
	free(data->right);
	free(data->leftpath1);
	free(data->leftpath2);
	free(data->rightpath);
	free(data->curpath);
	if (data->keys) hash_free(data->keys);
	free(data);
}

/*
 * Try to walk a minimum amount of graph and weave to compute the rset.
 * Similar to walkrevs, but saves from hacking in yet another mode
 * which calls back for all nodes and exposes the counter.
 *
 * Loop exit condition has 2 counters that must both be zero:
 *
 *  1. when the only csets that remain are in the history of either
 *  none or all the tips (left1,left2,right).
 *  We only know the state of a cset when it is the current loop 'd', but
 *  we know no future cset can have anything other than all or none
 *  if there are no current csets that aren't none or all.
 *  'nongca' tracks how many current and future nodes are active
 *  in LEFT1, LEFT2 or RIGHT, but not all.  When it hits 0, it will
 *  never leave 0.
 *
 *  2. when there is no rootkey for which we have seen one side and
 *  and not the other.  This is different from counter 1 in two ways:
 *  It works on rootkeys as opposed to csets; and it tracks the left
 *  side as either (LEFT1 | LEFT2) and counter 1 tracks them independently.
 *  What this counter says is that if we find LEFT1 and not LEFT2
 *  and hit the region where counter 1, nongca == 0, then we know
 *  that when we find LEFT2 that it must be in the history of LEFT1,
 *  and would be ignored.  So we ignore it before finding it.
 * 
 * The code works if nongca and needOther use the same counter,
 * but then it is likely harder to read.
 */
private	rset *
weaveExtract(sccs *s, ser_t left1, ser_t left2, ser_t right, Opts *opts)
{
	rset	*data = 0, *ret = 0;
	ser_t	d, e, upper;
	u8	all = 0;
	u8	*state = 0;
	u8	active;
	u32	seen, newseen;
	u32	rkoff, dkoff;
	rfile	*file;
	MDBM	*sDB = 0;
	sccs	*sc;
	kvpair	kv;
	hash	*showgone = 0;
	int	nongca = 0;
	int	needOther = 0;

#define	NONGCA(_state)	(_state && (_state != all))	/* some but not all */

#define	MARK(_state, _bits, _counter)		\
	do {	if (NONGCA(_state)) _counter--;	\
		_state |= _bits;		\
		if (NONGCA(_state)) _counter++;	\
	} while (0)

#define	ONE_SIDE(_state)	/* something but not both */	\
    (_state && !((_state & (LEFT1|LEFT2)) && (_state & RIGHT)))

	/*
	 * The hack that puts 1.0 as a lower bound sucks, as component path
	 * is 'ChangeSet' and that messes things up.
	 */
	if (left1 == TREE(s)) left1 = 0;
	if (left2 == TREE(s)) left2 = 0;
	if (left1 && left2 && isReachable(s, left1, left2)) {
		if (left1 < left2) left1 = left2;
		left2 = 0;
	}
	if (!left1 && left2) {
		left1 = left2;
		left2 = 0;
	}

	unless (data = initrset(s, left1, left2, right, opts)) goto err;

	all |= RIGHT;
	all |= LEFT1;	/* if no left1, then diff against empty */
	if (left2) all |= LEFT2;

	state = calloc(TABLE(s) + 1, sizeof(u8));

	assert(right);
	MARK(state[right], RIGHT, nongca);
	upper = right;

	if (left1) {
		MARK(state[left1], LEFT1, nongca);
		if (left1 > upper) upper = left1;
	}
	if (left2) {
		MARK(state[left2], LEFT2, nongca);
		if (left2 > upper) upper = left2;
	}

	/*
	 * XXX: new files cause the table range '..upper' to be read,
	 * because there's no telling that an upper bound is a new file,
	 * and will have no lower bound.  'needOther' will never hit zero.
	 *
	 * Since new files have 'needOther > 0' all the time, the loop
	 * needs to also stop on end of table.
	 */
	sccs_rdweaveInit(s);
	if (opts->show_gone) {
		upper = TABLE(s);
		showgone = hash_new(HASH_U32HASH, sizeof(u32), sizeof(u32));
	} else {
		sDB = mdbm_mem();	/* a cache of sccs_init() for files */
	}

	for (d = upper; ((d >= TREE(s)) && (nongca || needOther)); d--) {
		if (TAG(s, d)) continue;	/* because of show_gone */

		if ((active = state[d])) {
			if (e = PARENT(s, d)) MARK(state[e], active, nongca);
			if (e = MERGE(s, d)) MARK(state[e], active, nongca);
		}
		/*
		 * show_gone needs to log the first (possibly bogus) dkey
		 * to guess at current path. Otherwise, prune inactive.
		 */
		unless (active || opts->show_gone) continue;

		cset_firstPair(s, d);
		while (e = cset_rdweavePair(s, RWP_ONE, &rkoff, &dkoff)) {
			assert(d == e);	/* end of using 'e'; used to fail */
			data->weavelines++;	/* stats */
			if (showgone) {
				hash_insertU32U32(showgone, rkoff, dkoff);
				unless (active) continue;
			}
			file = hash_fetch(data->keys, &rkoff, sizeof(rkoff));
			seen = file ? file->seen : 0;
			assert(!(seen & LAST));
			unless (dkoff) {
				if (file) {
					if (ONE_SIDE(seen)) needOther--;
					file->seen |= LAST;
				}
				continue;
			}
			unless (newseen = (active & ~seen)) continue;
			if (!nongca && (newseen == all)) continue;
			unless (opts->show_gone || opts->chksum) {
				if (deltaSkip(s, sDB, rkoff, dkoff)) continue;
			}
			unless (file) {
				file = hash_insert(data->keys,
				    &rkoff, sizeof(rkoff),
				    0, sizeof(rfile));
				file->rkoff = rkoff;
				if (showgone) {
					file->tipoff =
					   hash_fetchU32U32(showgone, rkoff);
				}
			}
			/* if new left and not in the history of the other */
			if ((newseen & (LEFT1|LEFT2)) &&
			    !(active & seen & (LEFT1|LEFT2))) {
				if (newseen & LEFT1) {
					file->left1 = dkoff;
				}
				if (newseen & LEFT2) {
					file->left2 = dkoff;
				}
				/* Original bk: if two, only use newest */
				if (opts->compat && left2) {
					assert(left1);
					newseen |= (LEFT1|LEFT2);
				}
			}
			if (newseen & RIGHT) {
				assert(!file->right);
				file->right = dkoff;
			}

			if (ONE_SIDE(seen)) needOther--;
			seen |= newseen;
			if (ONE_SIDE(seen)) needOther++;

			file->seen = seen;
			if (!needOther && !nongca) break; /* if done, stop */
		}
		/* if done processing a nongca cset, then one less remains */
		if (NONGCA(active)) nongca--;
	}
	ret = data;
	data = 0;
err:
	if (showgone) hash_free(showgone);
	sccs_rdweaveDone(s);
	free(state);
	freeRset(data);
	if (sDB) {
		EACH_KV(sDB) {
			memcpy(&sc, kv.val.dptr, sizeof (sccs *));
			if (sc) sccs_free(sc);
		}
		mdbm_close(sDB);
	}
	return (ret);
}

private	rset *
initrset(sccs *s, ser_t left1, ser_t left2, ser_t right, Opts *opts)
{
	rset	*data = new(rset);
	ser_t	d;
	int	dopath = (opts->prefix && proj_isComponent(s->proj));

	data->keys = hash_new(HASH_U32HASH, sizeof(u32), sizeof(rfile));
	if (opts->chksum) {
		if (left2 || !right) {
			fprintf(stderr,
			    "checksum - only one lower and upper bound\n");
			freeRset(data);
			return (0);
		}
		if (left1 && (d = sccs_getCksumDelta(s, left1))) {
			data->sum = SUM(s, d);
		}
		if (d = sccs_getCksumDelta(s, right)) {
			data->wantsum = SUM(s, d);
		}
	}

	if (dopath) data->curpath = dirSlash(PATHNAME(s, sccs_top(s)));
	if (left1) {
		d = left1;
		data->left1 = strdup(REV(s, d));
		if (dopath) data->leftpath1 = dirSlash(PATHNAME(s, d));
	}
	if (left2) {
		d = left2;
		data->left2 = strdup(REV(s, d));
		if (dopath) data->leftpath2 = dirSlash(PATHNAME(s, d));
	}
	if (right) {
		d = right;
		data->right = strdup(REV(s, d));
		if (dopath) data->rightpath = dirSlash(PATHNAME(s, d));
	}
	return (data);
}

/*
 * fail if
 *   - the file for rk is there, and delta dk is missing and gone
 *   - the rk is gone and the file is missing
 * goneDB normally doesn't use the hash value.  Here it stores a "1"
 * in place of the default "" to mean really gone (not in file system).
 */
private	int
deltaSkip(sccs *cset, MDBM *sDB, u32 rkoff, u32 dkoff)
{
	sccs	*s;
	char	*rkgone;
	int	rc = 0;
	char	*rk = HEAP(cset, rkoff);
	char	*dk = HEAP(cset, dkoff);

	unless (cset->goneDB) cset->goneDB = loadDB(GONE, 0, DB_GONE);

	unless ((rkgone = mdbm_fetch_str(cset->goneDB, rk)) ||
	    mdbm_fetch_str(cset->goneDB, dk)) {
		return (0);
	}
	if (rkgone && (*rkgone == '1')) return (1);
	if (s =
	    sccs_keyinitAndCache(cset->proj, rk, SILENT, sDB, cset->idDB)) {
		/* if file there, but key is missing, ignore line. */
		unless (sccs_findKey(s, dk)) rc = 1;
	} else if (rkgone) {
		/* if no file, mark goneDB that it was really gone */
		mdbm_store_str(cset->goneDB, rk, "1", MDBM_REPLACE);
		rc = 1;
	} else {
		// not rk gone but no keyinit -- let rset fail => rc = 0;
	}
	return (rc);
}

/*
 * dirname() + '/' || 0 if no dirname
 */
private	char *
dirSlash(char *path)
{
	char	*p;

	unless (p = strrchr(path, '/')) return (0);
	return (strndup(path, p - path + 1));
}
