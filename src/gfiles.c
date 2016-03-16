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

// LMXXX - The output should be stuffed into a lines array and sorted on a per
// directory basis.  We get it in the right order and then we screw it up.
// After beta.
#include "system.h"
#include "sccs.h"
#include "bam.h"
#include "range.h"

#define	TSTATE	0	/* type: d/D/i/j/r/s/x */
#define	LSTATE	1	/* lock state: l,u */
#define	CSTATE	2	/* change state: c, ' ' */
#define	PSTATE	3	/* pending state: p,' ' */
#define	GSTATE	4	/* checked out state, 'G', ' ' */
#define	NSTATE	5	/* name state: n,' ' */
#define	YSTATE	6	/* comments status: y, ' ' */

#define	NO_OUTPUT	100

typedef struct winfo winfo;
typedef	char	STATE[8];

private int	chk_diffs(sccs *s);
private void	do_print(char state[6], char *gfile, char *rev);
private void	file(char *f);
private void	print_it(STATE state, char *file, char *rev);
private	void	print_summary(void);
private void	uprogress(void);
private	int	sccsdir(char *dir, void *data);
private void	walk(char *dir);
private	void	load_project(char *dir);
private	void	free_project(void);
private void	load_ignore(project *p);
private	void	ignore_file(char *file);
private	void	print_components(char *frompath);
private	void	walk_deepComponents(char *path, filefn *fn, void *data);
private	int	fastWalk(char **dirs);

typedef struct {
	u32	Aflg:1;			/* -pA: show all pending deltas */
	u32	all:1;			/* -a: disable ignore list */
	u32	cfiles:1;		/* -y: list files with comments */
	u32	Cflg:1;			/* -pC: want file<BK_FS>rev format */
	u32	changed:1;		/* -c: list changed files */
	u32	dfile:1;		/* trust d.file for pending deltas */
	u32	dirs:1;			/* -d: list directories with BK files */
	u32	extras:1;		/* -x: list extra files */
	u32	fixdfile:1;		/* -P: fix up the dfile tag */
	u32	gfile:1;		/* -g: print gfile name */
	u32	gotten:1;		/* -G: list checked out files */
	u32	notgotten:1;		/* -^G: only non-checked out sfiles */
	u32	gui:1;			/* --gui: interleaved progress/files */
	u32	ignored:1;		/* -i: list ignored (extra) files */
	u32	junk:1;			/* -j: list junk in SCCS dirs */
	u32	locked:1;		/* -l: list locked files */
	u32	names:1;		/* -n: list files in wrong path */
	u32	null:1;			/* -0: put nulls after sfiles */
	u32	onelevel:1;		/* -1: this dir only, don't recurs */
	u32	pending:1;		/* -p: list pending files */
	u32	progress:1;		/* -o: send progress to stdout */
	u32	recurse:1;		/* -r: recurse over repo boundary */
	u32	sfiles:1;		/* -s: we want sfiles */
	u32	verbose:1;		/* -v: show markers */
	u32	subrepos:1;		/* -R: we want subrepo roots */
	u32	summarize:1;		/* -S: summarize output only */
	u32	timestamps:1;		/* whether to use the timestamp DB */
	u32	unlocked:1;		/* -u: list unlocked files */
	u32	useronly:1;		/* -U: list user files only */
	u32	xdirs:1;		/* -D: list directories w/ no BK */
	u32	skip_comps:1;		/* -h: skip comp csets in prod */
	u32	atRoot:1;		/* running at root of repo? */
	u32	saw_locked:1;		/* saw a pfile somewhere? */
	u32	saw_pending:1;		/* saw a dfile somewhere? */
	u32	no_bkskip:1;		/* ignore .bk_skip */
	u32	error:1;		/* an error happened */
	u32	cold:1;			/* prefault each sfile printed */

	char	*relpath;		/* --replath: print relative paths */
	FILE	*out;			/* -o<file>: send output here */
	char	*glob;			/* only files which match this */
} options;

/* don't use -z, bk.c needs it. */

private hash	*timestamps = 0;
private options	opts;
private	project	*proj, *prodproj;

private	char	**ignore;	/* list of file pathname globs to ignore */
private	char	**ignorebase;	/* list of file basename globs to ignore */
private	char	**prunedirs;	/* set of dirs to prune */
private	char	**components;	/* list of subcomponents */

private u32	d_count, s_count, x_count; /* progress counter */
private u32	d_last, c_last, s_last, x_last; /* progress counter */
private u32	R_count, D_count, C_count, c_count, n_count, p_count, i_count;

private char *
hasfile(char *file, char type, MDBM *sDB)
{
	file[0] = type;
	return (mdbm_fetch_str(sDB, file));
}

private inline sccs *
init(char *name, int flags, MDBM *sDB, MDBM *gDB)
{
        sccs    *s;

	if (sDB) {
		char *p = basenm(name);

		flags |= INIT_NOSTAT;
		assert(*p == 's');
		if (hasfile(p, 'c', sDB)) flags |= INIT_HAScFILE;
		if (hasfile(p, 'p', sDB)) flags |= INIT_HASpFILE;
		if (hasfile(p, 'x', sDB)) flags |= INIT_HASxFILE;
		if (gDB && mdbm_fetch_str(gDB, &p[2])) flags |= INIT_HASgFILE;
		*p = 's'; /* because hasfile() stomps */
	}
	if (strneq(name, "./", 2)) name += 2;
	s = sccs_init(name, flags);
        return (s);
}

private void
willneed(char *sfile)
{
#ifdef	MADV_WILLNEED
	int	fd;
	size_t	len;
	char	*map;

	if ((fd = open(sfile, O_RDONLY, 0)) < 0) return;
	len = fsize(fd);
	if ((map = mmap(0, len, PROT_READ, MAP_SHARED, fd, 0)) != MAP_FAILED) {
		madvise(map, len, MADV_WILLNEED);
		munmap(map, len);
	}
	close(fd);
#endif
}

private int
fastprint(char *file, char type, void *data)
{
	filecnt	*fc = data;
	char	*p;
               
	file += 2;
	if (fc) {
		++fc->tot;
		unless (strneq(file, "BitKeeper/", 10)) ++fc->usr;
	}
	if (opts.gfile) {
		if (p = strstr(file, "/SCCS/s.")) {
			*++p = 0;
			fputs(file, stdout);
			*p = 'S';
			p += 7;
		} else {
			assert(strneq(file, "SCCS/s.", 7));
			p = file + 7;
		}
		puts(p);
	} else {
		puts(file);
	}
	if (opts.cold) willneed(file);
	return (0);
}

int
gfiles_main(int ac, char **av)
{
	int	c, i;
	int	not = 0;
	filecnt	fc;
	int	rc;
	int	no_cache = 0;		/* don't use fast path */
	int	did_fast = 0;		/* did fast sfiles walk? */
	char	*path, *s, buf[MAXPATH];
	longopt	lopts[] = {
		{ "relpath:", 300 },
		{ "gui", 301 },
		{ "no-progress", 302 },	// use after --gui for no P|
		{ "no-bkskip", 305 },
		{ "no-cache", 310 },
		{ "cold", 315 },
		{ 0, 0 },
	};

	// can we lose the sfind alias yet?
	unless (streq(av[0], "sfiles") || streq(av[0], "sfind")) opts.gfile = 1;

	if (av[1] && streq(av[1], "--cold")) { /* for the fast case below */
		opts.cold = 1;
		av++, ac--;
	}
	if (ac == 1)  {
		opts.sfiles = 1;
		memset(&fc, 0, sizeof(fc));
		walksfiles(".", fastprint, &fc);
		if (isdir(BKROOT) && !proj_isResync(0)) repo_nfilesUpdate(&fc);
		return (0);
	}

	while ((c = getopt(ac, av,
		    "^01acCdDeEgGhijlno:p|P|rRsSuUvxy", lopts)) != -1) {
		switch (c) {
		    case '^':	not = 1; break;
		    case '0':	opts.null = 1; break;		/* doc */
		    case '1':	opts.onelevel = 1; break;
		    case 'a':	opts.all = 1; break;		/* doc 2.0 */
		    case 'c':	opts.changed = opts.timestamps = 1;
		    		break;
		    case 'C':	opts.changed = 1;
				opts.timestamps = 0;
				break;
		    case 'd':	opts.dirs = 1; break;
		    case 'D':	opts.xdirs = 1; break;
		    case 'S':	opts.summarize = 1; /* fall through */
		    case 'e':	/* all of these do the same thing, which is */
		    case 'E':	/* to show everything in verbose format */
				opts.cfiles = opts.changed = opts.dirs =
				opts.extras = opts.gotten = opts.ignored =
				opts.junk = opts.locked = opts.names = 
				opts.pending = opts.sfiles = opts.subrepos =
				opts.unlocked = opts.xdirs = 1;
				/* fall through */
		    case 'v':	opts.verbose = 1; break;
		    case 'g':	opts.gfile = 1; break;
		    case 'G':					/* doc */
			if (not) {
				not = 0;
				opts.notgotten = 1;
			} else {
				opts.gotten = 1;
			}
			break;
		    case 'h':	opts.skip_comps = 1; break;
		    case 'i':	opts.ignored = 1; break;
		    case 'j':	opts.junk = 1; break;		/* doc 2.0 */
		    		/* XXX - should list BitKeeper/tmp stuff */
		    case 'l':   opts.locked = 1; break;		/* doc 2.0 */
		    case 'n':   opts.names = 1; break;		/* doc 2.0 */
		    case 'o':					/* doc 2.0 */
				unless (opts.out = fopen(optarg, "w")) {
		    			perror(optarg);
					exit(1);
				}
				setlinebuf(opts.out);
				opts.progress = 1;
				break;
		    case 'P':	opts.fixdfile = 1;	  	/* undoc 2.0 */
				/* fall thru */
		    case 'p':	opts.pending =1;		/* doc 2.0 */
				for (s = optarg; s && *s; s++) {
					if (*s == 'A') {
						opts.Aflg = 1;
						opts.Cflg = 1;
					} else if (*s == 'C') {
						opts.Cflg = 1;
					} else {
						usage();
					}
				}
				break;
		    case 'R':	opts.subrepos = 1; break;
		    case 'r':	opts.recurse = 1; opts.skip_comps = 1; break;
		    case 's':	opts.sfiles = 1; break;
		    case 'u':	opts.unlocked = 1; break;	/* doc 2.0 */
		    case 'U':	opts.useronly = 1; break;	/* doc 2.0 */
		    case 'x':	opts.extras = 1; break;		/* doc */
		    case 'y':	opts.cfiles = 1; break;		/* doc */
		    case 300: /* --relpath */
			opts.relpath = fullname(optarg, 0);
			break;
		    case 301: /* --gui */
		        opts.gui = opts.progress = 1;
			break;
		    case 302: /* --no-progress */
		        opts.progress = 0;
			break;
		    case 305: /* --no-bkskip */
			opts.no_bkskip = 1;
			break;
		    case 310: /* --no-cache */
			no_cache = 1;
			break;
		    case 315: /* --cold */
			opts.cold = 1;
			break;
		    default: bk_badArg(c, av);
		}
		if (not && (c != '^')) {
			fprintf(stderr, "%s: no ^ form of -%c\n", prog, c);
			usage();
		}
	}

	unless (opts.out) opts.out = stdout;
	if (opts.progress || opts.gui) setlinebuf(stdout);
	fflush(opts.out); /* for win32 */
	C_count = c_count = p_count = d_count = s_count = x_count = 0;
	if (opts.summarize) opts.verbose = 0;

	/*
	 * If user did not select any option,
	 * setup a default mode for them
	 */
	unless (opts.cfiles || opts.changed ||
	    opts.gotten || opts.notgotten ||opts.ignored ||
	    opts.locked || opts.names || opts.pending || opts.unlocked ||
	    opts.dirs || opts.extras || opts.junk || opts.subrepos ||
	    opts.xdirs) {
		opts.sfiles = 1;
	}

	for (i = optind; av[i]; i++);
	i--;
	if (streq(av[i], "-")) i--;
	if ((i >= optind) && sfiles_glob(av[i])) {
		opts.glob = strdup(av[i]);
		if (av[i+1]) {
			av[i] = av[i+1];
			av[i+1] = 0;
		} else {
			av[i] = 0;
		}
	}
	unless (av[optind]) {
		char	**dirs;
		u32	state;

		path = ".";
		/* flag running at the root of repo */
		opts.atRoot = isdir(BKROOT);

		/*
		 * We we are only needing pending, changed or edited
		 * sfiles then we can use the scandirs file to only walk
		 * part of the repository.
		 */
		if (opts.atRoot && !no_cache &&
		    !(opts.sfiles || opts.onelevel || opts.dirs ||
			opts.xdirs || opts.cfiles || opts.extras ||
			opts.gotten || opts.notgotten || opts.ignored ||
			opts.junk || opts.names || opts.subrepos ||
			opts.unlocked || opts.fixdfile) &&
		    !proj_isResync(0)) {
			state = 0;
			if (opts.pending) state |= DS_PENDING;
			if (opts.locked || opts.changed) state |= DS_EDITED;
			if (state && (dirs = proj_scanDirs(0, state))) {
				rc = fastWalk(dirs);
				freeLines(dirs, 0);
				did_fast = 1;
				goto out;
			}
		}
		walk(path);
	} else if (streq("-", av[optind])) {
		setmode(0, _O_TEXT); /* read file list in text mode */
		while (fnext(buf, stdin)) {
			chop(buf);
			localName2bkName(buf, buf);
			path = buf;
                        if (isdir(path)) {
				walk(path);
			} else {
				/*
				 * Kind of hokey but we load the proj based on
				 * the first filename we see and hope that they
				 * give us a set of files all in the same proj.
				 */
				unless (proj) {
					s = dirname_alloc(path);
					load_project(s);
					free(s);
				}
                                file(path);
			}
		}
	} else {
                for (i = optind; av[i]; ++i) {
                        localName2bkName(av[i], av[i]);
                        if (isdir(av[i])) {
                                path = av[i];
                                walk(path);
                        } else {
                                path = av[i];
				unless (proj) {
					s = dirname_alloc(path);
					load_project(s);
					free(s);
				}
                                file(path);
                        }
                }
	}
	rc = 0;
out:
	if (opts.out != stdout) fclose(opts.out);
	if (opts.progress) uprogress();
	if (opts.relpath) free(opts.relpath);
	if (opts.atRoot && !opts.onelevel && !opts.recurse) {
		u32	state = 0;
		/*
		 * This was a normal sfiles that walked the entire
		 * repo.  So we can set or clear the modified flag for
		 * this component if pfiles were found.
		 */
		if (opts.pending && !opts.saw_pending) state |= DS_PENDING;
		if ((!did_fast || opts.locked || opts.changed) &&
		    !opts.saw_locked) {
			state |= DS_EDITED;
		}

		if ((state & DS_PENDING) && proj_isComponent(0) &&
		    xfile_exists(CHANGESET, 'd')) {
			/*
			 * The component cset file has a dfile so it
			 * is pending in the product so we don't want
			 * to clear the pending marks for this
			 * component yet.
			 */
			state &= ~DS_PENDING;
			proj_dirstate(0, ".", DS_PENDING, 1);
		}
		if (state) proj_dirstate(0, "*", state, 0);
	}
	free_project();
	if (!rc && opts.error) rc = 1;
	return (rc);
}

private int
isPathPruned(char *dir, hash *checked)
{
	char	buf[MAXPATH];

	buf[0] = '/';		/* prune patterns start with '/' */
	strcpy(buf+1, dir);
	dir = buf+1;

	do {
		if (hash_fetchStrSet(checked, dir)) return (0);

		if (match_globs(buf, prunedirs, 0)) return (1);
		strcat(dir, "/.bk_skip");
		if (exists(dir)) return (1);
		dirname(dir);	/* remove .bk_skip */
		hash_insertStrSet(checked, dir);
		dir = dirname(dir); /* remove one directory */
	} while (!streq(dir, "."));
	return (0);
}

private	int
fastWalk(char **dirs)
{
	int	i;
	char	**comps;
	hash	*checked;
	char	buf[MAXPATH];

	load_project(".");
	opts.onelevel = 1;
	checked = hash_new(HASH_MEMHASH);
	EACH(dirs) {
		unless (isdir(dirs[i])) continue;
		if (isPathPruned(dirs[i], checked)) continue;

		walk(dirs[i]);
	}
	opts.onelevel = 0;
	hash_free(checked);

	if (proj_isProduct(0) && opts.pending) {
		free_project();
		comps = proj_scanComps(0, DS_PENDING);
		EACH(comps) {
			if (streq(comps[i], ".")) continue;
			concat_path(buf, comps[i], GCHANGESET);
			file(buf);
		}
	}
	return (0);
}

/* only called from file().  Used to get state of sfile */
private sccs *
chk_sfile(char *name, STATE state)
{
	char	*s, *relp, *gname;
	ser_t	d;
	int	rc;
	sccs	*sc = 0;

	s = strrchr(name, '/');

#define	DOIT	(opts.verbose || opts.changed || opts.names)
	if (s[1] == 's') {
// LMXXX - we could optimize this further.  If they are not looking for
// opts.sfiles or opts.changed or opts.locked or opts.names then we won't
// need to init the sfile.
		if (DOIT) {
			sc = init(name, INIT_NOCKSUM, 0, 0);
			unless (sc && HASGRAPH(sc)) {
				sccs_free(sc);
				sc = 0;
			}
		}
		if (xfile_exists(name, 'c')) state[YSTATE] = 'y';
		state[TSTATE] = 's';
		if (xfile_exists(name, 'p')) {
			state[LSTATE] = 'l';
			if (DOIT && sc) {
				if ((rc = chk_diffs(sc)) < 0) opts.error = 1;
				if (rc) state[CSTATE] = 'c';
			}
		} else {
			state[LSTATE] = 'u';
		}
		if (DOIT && sc) {
			d = sccs_top(sc);
			gname = sccs2name(name);
			relp = proj_relpath(proj, gname);
			free(gname);
			unless (HAS_PATHNAME(sc, d) && patheq(relp, PATHNAME(sc, d))) {
				state[NSTATE] = 'n';
			}
			free(relp);
		}
	}
	return (sc);
}

private int
pending_print(sccs *s, ser_t d, void *token)
{
	char	**data = token;

	if (FLAGS(s, d) & D_CSET) return (-1);	/* stopping condition */
	do_print(data[0], data[1], REV(s, d));
	return (0);
}

/*
 * return true if a sfile has pending deltas
 *
 * Not used by sfiles, but put here next to the other code that does
 * something similar.
 */
int
sccs_isPending(char *gfile)
{
	char	*sfile = name2sccs(gfile);
	sccs	*s;
	int	rc = 0;

	unless (xfile_exists(gfile, 'd')) goto out; /* look for dfile */
	unless (s = sccs_init(sfile, SILENT|INIT_NOCKSUM|INIT_MUSTEXIST)) {
		goto out;
	}
	unless (FLAGS(s, sccs_top(s)) & D_CSET) rc = 1;
	sccs_free(s);
out:	free(sfile);
	return (rc);
}

private void
chk_pending(sccs *s, char *gfile, STATE state, MDBM *sDB, MDBM *gDB)
{
	ser_t	d;
	int	local_s = 0, printed = 0;
	char	buf[MAXPATH];

	if (streq(gfile, "./ChangeSet")) {
		state[PSTATE] = ' ';
		do_print(state, gfile, 0);
		return;
	}
	if (opts.dfile) {
		if (sDB) {
			strcpy(buf, "d.");
			strcpy(&buf[2], basenm(gfile));
			unless (mdbm_fetch_str(sDB, buf)) {
				state[PSTATE] = ' ';
				do_print(state, gfile, 0);
				return;
			}
		} else {
			unless (xfile_exists(gfile, 'd')) {
				state[PSTATE] = ' ';
				do_print(state, gfile, 0);
				return;
			}
		}
	}

	unless (s) {
		char *sfile = name2sccs(gfile);
		s = init(sfile, INIT_NOCKSUM, sDB, gDB);
		unless (s && HASGRAPH(s)) {
			fprintf(stderr, "gfiles: %s: bad sfile\n", sfile);
			free(sfile);
			if (s) sccs_free(s);
			return;
		}
		free(sfile);
		local_s = 1;
	}

	/*
	 * check for pending deltas
	 */
	state[PSTATE] = ' ';
	unless (d = sccs_top(s))  goto out;	
	if (FLAGS(s, d) & D_CSET) goto out;

	/*
	 * If it is out of view, we need to look at all leaves and see if
	 * there is a problem or not.
	 */
	if (s->defbranch && streq(s->defbranch, "1.0")) {
		for (d = TABLE(s); d >= TREE(s); d--) {
			unless (!TAG(s, d) && sccs_isleaf(s, d)) {
				continue;
			}
			unless (FLAGS(s, d) & D_CSET) break;
		}
		unless (d) goto out;
		fprintf(stderr,
		    "Warning: not in view file %s skipped.\n", s->gfile);
		goto out;
	}

	assert(!(FLAGS(s, d) & D_CSET));
	state[PSTATE] = 'p';
	if (opts.Aflg) {
		char	*data[2] = {state, gfile};

		/* get the nodes not covered by D_CSET */
		range_walkrevs(s, 0, L(d), 0, pending_print, data);
		printed = 1;
	} else if (opts.Cflg) {
		do_print(state, gfile, REV(s, d));
		printed = 1;
	}

out:	unless (printed) do_print(state, gfile, 0);
	/*
	 * Do not sccs_free() if it is passed in from outside
	 */
	if (state[PSTATE] == 'p') {
		/* add missing dfile */
		if (opts.fixdfile) updatePending(s);
	} else {
		/* No pending delta, remove redundant d.file */
		xfile_delete(gfile, 'd');
	}
	if (local_s) sccs_free(s);
}

/*
 * process a file on the sfiles command line or read from stdin.
 * The global 'proj' should be set to the project that matches these files.
 */
private void
file(char *f)
{
	char	name[MAXPATH], buf[MAXPATH];
	char    *s, *sfile;
	STATE	state = "       ";
	sccs	*sc = 0;

	if (strlen(f) >= sizeof(name)) {
		fprintf(stderr, "gfiles: name too long: [%s]\n", f);
		return;
	}
	strcpy(name, f);
	if (s = rindex(name, '/')) s -= 4;      /* point it at start of SCCS/ */
	unless (s >= name) s = 0;
	/*
	 * There are three possible condition
	 * a) f is a sfile
	 * b) f is a regular or new gfile
	 * c) f is a junk file in the SCCS directory
	 */
	if (s && strneq(s, "SCCS/", 5) && ((s == name) || (s[-1] == '/'))) {
		*s = 0;
		strcpy(buf, name);	/* not including SCCS/whatever */
		*s = 'S';
		s += 4;			/* points at '/' */

		switch (sccs_filetype(f)) {
		    case 'c':
			s[1] = 's';
			if (exists(name)) {
		    		state[YSTATE] = 'y';
				state[TSTATE] = 's';
			} else {
				state[TSTATE] = 'j';
			}
			s[1] = 'c';
			strcat(buf, s+1);
			break;
		    case 's':
			state[TSTATE] = 's';
			sc = chk_sfile(f, state);
			strcat(buf, s+3);
			break;
		    default:
			state[TSTATE] = 'j';
			strcpy(buf, f);
			break;
		}
	} else {
		/*
		 * TODO: we need to check for the case where
		 * the pwd is a SCCS dir
		 * This can fool the current code into wronly treating them
		 * as xtras.
		 */
		/* this file is not in SCCS/s. form */
		sfile = name2sccs(f);
		unless (exists(sfile)) {
			state[TSTATE] = 'x';
			s = strrchr(sfile, '/');
			if (xfile_exists(sfile, 'c')) state[YSTATE] = 'y';
		} else {
			sc = chk_sfile(sfile, state);
		}
		free(sfile);
		strcpy(buf, f);
	}
	if (proj && sc && (sc->proj != proj)) {
		fprintf(stderr,
		    "gfiles: error file %s is from a different repository\n"
		    "than other files. %s vs %s\n",
		    sc->gfile, proj_root(sc->proj), proj_root(proj));
	}
	/*
	 * When we get here buf contains the gname (for the sfile case).
	 */
	if (state[TSTATE] == 's') {
		if (exists(buf)) state[GSTATE] = 'G';
		if (opts.pending || opts.verbose) {
			/* chk_pending calls do_print */
			chk_pending(sc, buf, state, 0, 0);
		} else {
			do_print(state, buf, 0);
		}
	} else  if (state[TSTATE] == 'x' || state[TSTATE] == 'j') {
		/* XXX - does not work with SCCS/c.* */
		if (exists(buf)) {
			state[LSTATE] = state[PSTATE] = ' ';
			do_print(state, buf, 0);
		}
	} else {
		state[PSTATE] = ' ';
		do_print(state, buf, 0);
	}
	if (sc) sccs_free(sc);
}

private void
print_summary(void)
{
	fprintf(opts.out, "%6d files under revision control.\n", s_count);
	if (R_count) {
		fprintf(opts.out,
		    "%6d subrepositories.\n", R_count);
	}
	if (opts.extras) {
		fprintf(opts.out,
		    "%6d files not under revision control (and not ignored).\n",
		    x_count);
	}
	if (i_count) {
		fprintf(opts.out,
		    "%6d ignored files not under revision control.\n", i_count);
	}
	if (d_count) {
		fprintf(opts.out,
		    "%6d directories containing revision controlled files.\n",
		    d_count);
	}
	if (D_count) {
		fprintf(opts.out,
		    "%6d directories with no revision controlled files.\n",
		    D_count);
	}
	if (opts.gotten) {
		fprintf(opts.out, "%6d files checked out.\n", C_count);
	}
	if (opts.changed) {
		fprintf(opts.out,
		    "%6d files modified and not checked in.\n", c_count);
	}
	if (opts.pending) {
		fprintf(opts.out,
		    "%6d files with checked in, but not committed, deltas.\n",
		    p_count);
	}
	if (opts.names) {
		fprintf(opts.out,
		    "%6d files in incorrect locations.\n", n_count);
	}
}

/*
 * This struct contains the information that is passed sfiles_walk() by
 * walkdir() as it is traversing the repository.  Information from the
 * walkdir() is collected here and then processed once per directory with
 * a call to sccsdir().
 */
struct winfo {
	MDBM	*gDB;		/* all files in the current directory */
	int	seenfirst;	/* flag for -1 (one level) */
	int	rootlen;	/* length of directory passed to walkdir */
	char	*proj_prefix;	/* prefex needed to get sfile relpath */
};

private void
winfo_free(winfo *wi)
{
	mdbm_close(wi->gDB);
	wi->gDB = 0;
}

private int
sfiles_walk(char *file, char type, void *data)
{
	winfo	*wi = (winfo *)data;
	char	*p;
	int	n, nonsccs;
	char	buf[MAXPATH];

	p = strrchr(file, '/');
	if ((p - file) < wi->rootlen) p = 0;
	if (type == 'd') {
		if (p && patheq(p, "/SCCS")) return (-1);
		if (p &&((p-file) > wi->rootlen) && patheq(p+1, "BitKeeper")) {
			/*
			 * Do not cross into other package roots
			 * (e.g. RESYNC).
			 */
			concat_path(buf, file, "etc");
			if (exists(buf)) {
				*p = 0;
				do_print("R      ", file, 0);
				*p = '/';
				unless (opts.recurse) {
					winfo_free(wi);
					return (-2);
				}
			}
		}
		if (proj) {
			concat_path(buf, wi->proj_prefix,
			    p ? (file + wi->rootlen + 1) : "");
			if (match_globs(buf, prunedirs, 0)) {
				// if remap and dir exists in .bk, trouble?
				return (-1);
			}
		}
		if (opts.dirs || opts.xdirs) {
			n = strlen(file);
			if (n > wi->rootlen) {
				concat_path(buf, file, BKROOT);
				if (exists(buf)) {
					do_print("R      ", file, 0);
					return (-1);
				}
			}
			strcpy(&file[n], "/SCCS");
			nonsccs = !exists(file) || emptyDir(file);
			file[n] = 0;
			if (opts.xdirs && nonsccs) {
				do_print("D      ", file, 0);
			}
			if (opts.dirs && !nonsccs) {
				do_print("d      ", file, 0);
			}
		}
		if (opts.onelevel) {
			if (wi->seenfirst) return (-1);
			wi->seenfirst = 1;
		}
	} else {
		assert(p);
		p++;
		if (!opts.no_bkskip && patheq(p, BKSKIP)) {
			/* abort current dir */
			// if remap and dir exists in .bk, trouble?
			winfo_free(wi);
			return (-2);
		}
		unless (wi->gDB) wi->gDB = mdbm_mem();
		mdbm_store_str(wi->gDB,
		    p, (writableReg(file) ? "1" : ""), MDBM_INSERT);
	}
	return (0);
}

private void
load_ignore(project *p)
{
	char	*file;

	ignore = ignorebase = prunedirs = 0;

	/* add default pruned dirs */
	prunedirs = addLine(prunedirs, strdup("/" BAM_ROOT));
	prunedirs = addLine(prunedirs, strdup("/BitKeeper/log"));
	prunedirs = addLine(prunedirs, strdup("/BitKeeper/tmp"));
	prunedirs = addLine(prunedirs, strdup("/BitKeeper/writer"));
	prunedirs = addLine(prunedirs, strdup("/BitKeeper/readers"));
	prunedirs = addLine(prunedirs, strdup("/PENDING"));

	/*
	 * Normally RESYNC is ignored anyway because it is a new repo, but
	 * when it just contains .bk_nl then it needs to be skipped.
	 */
	prunedirs = addLine(prunedirs, strdup("/RESYNC"));

	/* add default ignore patterns */
	ignore = addLine(ignore, strdup("/BitKeeper/etc/level"));
	ignore = addLine(ignore, strdup("/BitKeeper/etc/csets-out"));
	ignore = addLine(ignore, strdup("/BitKeeper/etc/csets-in"));

	if (opts.all) return;


	file = aprintf("%s/BitKeeper/etc/ignore", proj_root(p));
	ignore_file(file);
	free(file);
	file = aprintf("%s/ignore", getDotBk());
	ignore_file(file);
	free(file);
}

private void
ignore_file(char *file)
{
	char	*dir, *pat, *sfile;
	int	len, isbase, isprune, process = 0;
	FILE	*ignoref;
	char	buf[MAXLINE];

	/* Try and get it if the directory is writable */
	unless (exists(file)) {
		dir = dirname_alloc(file);
		unless (exists(dir)) {
			free(dir);
			return;
		}
		if (writable(dir)) get(file, SILENT);
		free(dir);
	}
	if (exists(file)) {
		ignoref = fopen(file, "r");
	} else {
		sfile = name2sccs(file);
		unless (exists(sfile)) {
			free(sfile);
			return;
		}
		free(sfile);

		/*
		 * Note that for "sccs sfiles" instead "bk sfiles" this didn't
		 * work.  Don't know why and don't really care.
		 */
		sprintf(buf, "bk cat '%s'", file);
		ignoref = popen(buf, "r");
		process = 1;
	}
	unless (ignoref) return;
	while (fnext(buf, ignoref)) {
		chomp(buf);

		isbase = (strchr(buf, '/') == 0);

		pat = buf;
		if (*pat == '/') ++pat;	/* skip leading slash */
		if (strneq(pat, "./", 2)) pat += 2; /* skip leading ./ */

		unless (len = strlen(pat)) continue; /* blank lines */
		if ((len > 7) && streq(pat + len - 7, " -prune")) {
			pat[len-7] = 0;
			isprune = 1;
			isbase = 0;
		} else {
			isprune = 0;
		}
		if (isbase || (pat[0] == '*')) {
			pat = strdup(pat);
		} else {
			pat = aprintf("/%s", pat);
		}
		if (isprune) {
			prunedirs = addLine(prunedirs, pat);
		} else if (isbase) {
			ignorebase = addLine(ignorebase, pat);
		} else {
			ignore = addLine(ignore, pat);
		}
	}
	if (process) {
		pclose(ignoref);
	} else {
		fclose(ignoref);
	}
}

/*
 * maintain the global 'proj' pointer to point and the project for this
 * directory.
 */
private void
load_project(char *dir)
{
	project	*newproj;
	char	*p;
	char	tmp[MAXPATH];

	newproj = proj_init(dir);
	if (newproj != proj) {
		free_project();
		proj = newproj;
		prodproj = proj_isEnsemble(proj) ? proj_product(proj) : 0;
		load_ignore(proj);
		unless (opts.fixdfile) {
			/* use parent if RESYNC */
			unless (newproj = proj_isResync(proj)) newproj = proj;
			concat_path(tmp, proj_root(newproj), DFILE);
			unless (opts.dfile = exists(tmp)) {
				if (p = getenv("_BK_SLOW_WALK")) touch(p, 0666);
			}
		}
	} else {
		if (newproj) proj_free(newproj);
	}
}

private void
free_project(void)
{
	if (proj) {
		proj_free(proj);
		proj = prodproj = 0;
		freeLines(ignore, free);
		freeLines(ignorebase, free);
		freeLines(prunedirs, free);
		ignore = ignorebase = prunedirs = 0;
	}
}

/* called on directories on stdin or the command line */
private void
walk(char *indir)
{
	char	*p;
	winfo	wi = {0};
	char	dir[MAXPATH];

	cleanPath(indir, dir);
	if (streq(dir, "/")) strcpy(dir, "/.");
	load_project(dir);
	unless (proj) {
		/*
		 * Dir is not a BitKeeper repository,
		 * turn off all BitKeeper specific feature.
		 */
		opts.all = 1;
	}
#if 0
	/*
	 * XXX TODO: should we reset the progress counter ?
	 */
	s_count = x_count = 0;
#endif

	wi.rootlen = strlen(dir);
	if (proj) {
		p = proj_relpath(proj, dir);
		if (streq(p, ".")) {
			wi.proj_prefix = strdup("/");
		} else {
			wi.proj_prefix = aprintf("/%s", p);
		}
		free(p);
	}
	walkdir(dir,
	    (walkfns){ .file = sfiles_walk, .dir = sccsdir }, &wi);
	if (proj) free(wi.proj_prefix);
	unless (opts.onelevel) print_components(dir);
	if (opts.timestamps && timestamps && proj) {
		dumpTimestampDB(proj, timestamps);
		hash_free(timestamps);
		timestamps = 0;
	}
	if (opts.summarize) print_summary();

	/*
	 * We only enable fast scan mode if we started at the root
	 */
	if (opts.fixdfile && isdir("BitKeeper/etc/SCCS")) {
		enableFastPendingScan();
	}
}

private void
uprogress(void)
{
	char	buf[100];

	if ((s_last == s_count) && (x_last == x_count) &&
	    (d_last == d_count) && (c_last == c_count)) {
	    	return;
	}
	sprintf(buf, "%s%d %d %d %d\n",
	    opts.gui ? "P|" : "", s_count, x_count, d_count, c_count);
	/* If we get an error, it usually means that we are to die */
	if (write(1, buf, strlen(buf)) != strlen(buf)) exit(1);
	s_last = s_count;
	x_last = x_count;
	d_last = d_count;
	c_last = c_count;
}

private int
chk_diffs(sccs *s)
{
	int	rc, different;

	unless (s) return (0);
	if ((rc = sccs_hasDiffs(s, 0, 1)) < 0) return (rc);
	different = (rc >= 1);
	if (timestamps) {
		updateTimestampDB(s, timestamps, different);
	}
	return (different);
}

private int
isIgnored(char *file)
{
	char	*p;
	struct	stat sbuf;
	char	gfile[MAXPATH];

	/* if not in a repo, don't bother */
	unless (proj_root(proj)) return (0);

	/* If this file is not from our repo, then no */
	unless (p = proj_relpath(proj, file)) return (0);
	sprintf(gfile, "/%s", p);
	free(p);

	/* If this is a subcomponent, then no */
	if ((p = strrchr(file, '/')) && streq(p+1, "ChangeSet")) return (0);

	if (match_globs(gfile, ignore, 0) ||		 /* pathname match */
	    match_globs(basenm(gfile), ignorebase, 0) || /* basename match */
	    lstat(file, &sbuf) ||			 /* gone? */
	    !(sbuf.st_mode & S_IFREG|S_IFLNK)) {	 /* not reg file */
		return (1);
	}
	if (getenv("BK_IGNOREDIRS")) {
		/* check if directory is ignored */
		while (p = strrchr(gfile, '/')) {
			*p = 0;
			if (match_globs(gfile, ignore, 0) ||
			    match_globs(basenm(gfile), ignorebase, 0)) {
				return (1);
			}
		}
	}
	return (0);
}

private int
hidden(char *file)
{
	return (strstr(file, "BitKeeper/etc/SCCS/x.") != 0);
}

private int
isBkFile(char *gfile)
{
	char	*rp = 0;
	int	ret;
	char	buf[MAXPATH];

	/* ChangeSet is reserved at the root of any repo */
	if (streq(basenm(gfile), "ChangeSet")) {
		strcpy(buf, gfile);
		concat_path(buf, dirname(buf), BKROOT); /* yes, this works */
		if (isdir(buf)) return (1);
	}
	if (opts.atRoot) {
		/* gfile is already a relative path from root */
	} else if (proj && strstr(gfile, "BitKeeper/")) {
		unless (rp = proj_relpath(proj, gfile)) return (0);
		gfile = rp;
	} else {
		/* not in root or no BitKeeper in pathname */
		return (0);
	}
	ret = (strneq(gfile, "BitKeeper/", 10) &&
	    !strneq(gfile, "BitKeeper/triggers/", 19));
	if (rp) free(rp);
	return (ret);
}

private int
print_file(char *name)
{
	int	rc;
	char	*rel, *aname = 0;

	if (opts.relpath) {
		aname = aprintf("%s/%s", proj_cwd(), name);
		rel = relpath(opts.relpath, aname);
		free(aname);
		aname = rel;
	} else {
		rel = name;
	}
	if (opts.gui && !opts.verbose) fputs("F|", opts.out);
	rc = fputs(rel, opts.out);
	if (aname) free(aname);
	return (rc);
}

/* ONLY call this from do_print */
private void
print_it(STATE state, char *gfile, char *rev)
{
	char	*sfile;

	gfile =  strneq("./",  gfile, 2) ? &gfile[2] : gfile;
	if ((opts.useronly) && isBkFile(gfile)) return;

	if (opts.verbose) {
		if (opts.gui) fputs("F|", opts.out);
		if (state[TSTATE] == 'j') {
			assert(streq(state, "j      "));
			if (fprintf(opts.out, "j------ ") != 8) {
error:				if (opts.out == stdout) {
					/* SIGPIPE mostly, just exit */
					exit(0);
				} else {
					/* file write failed, old action */
					perror("output error");
					exit(1);
				}
			}
		} else {
			STATE	tmp;
			char	*p;

			strcpy(tmp, state);
			for (p = tmp; *p; p++) if (*p == ' ') *p = '-';
			if (fprintf(opts.out, "%s ", tmp) != 8) goto error;
		}
	}
	/* if gfile or !sfile then print gfile style name */
	if (opts.gfile || (state[TSTATE] != 's')) {
		if (print_file(gfile) < 0) goto error;
	} else {
		sfile = name2sccs(gfile);
		if (print_file(sfile) < 0) goto error;
		free(sfile);
	}
	if (rev) {
		int rlen = strlen(rev) + 1;
		if (fprintf(opts.out, "%c%s", BK_FS, rev) != rlen) goto error;
	}
	if (fputc((opts.null ? '\0' : '\n'), opts.out) < 0) goto error;
}

private void
do_print(STATE buf, char *gfile, char *rev)
{
	STATE	state;
	int	doit;
	char	*sname;

	gfile =  strneq("./",  gfile, 2) ? &gfile[2] : gfile;
	strcpy(state, buf);	/* So we have writable strings */
	if (opts.glob) {
		char	*p;

		if (p = strrchr(gfile, '/')) {
			p++;
		} else {
			p = gfile;
		}
		/* XXX - make this conditional on !directory */
		if (streq("s.", p)) p += 2;
		unless (match_one(p, opts.glob, 0)) return;
	}

	if (state[PSTATE] == 'p') p_count++;
	if (state[NSTATE] == 'n') n_count++;
	if (state[GSTATE] == 'G') C_count++;
    	if (state[CSTATE] == 'c') c_count++; 

	switch (state[TSTATE]) {
	    case 'j': break;
	    case 'x':
	    	if (isIgnored(gfile)) {
			state[TSTATE] = 'i';
			i_count++;
		} else {
			x_count++;
		}
		break;
	    case 's': s_count++; break;
	    case 'd': d_count++; break;
	    case 'D': D_count++; break;
	    case 'R': R_count++; break;
	    default:
		fprintf(stderr, "TSTATE='%c'\n", state[TSTATE]);
		assert("state[TSTATE] unknown" == 0);
	}
	if (opts.progress &&
	    (((s_count - s_last) > 100) || ((x_count - x_last) > 100))) {
		uprogress();
	}
	if (opts.summarize) return; /* skip the detail */

	if ((state[TSTATE] == 'R') && prodproj && !opts.subrepos) {
		/*
		 * found a subrepo while walking nested tree when
		 * subrepos were not enabled.
		 */
		components =
		    addLine(components, proj_relpath(prodproj, gfile));
		/* won't print anything below */
	}
	doit = ((state[TSTATE] == 'd') && opts.dirs) ||
	    ((state[TSTATE] == 'D') && opts.xdirs) ||
	    ((state[TSTATE] == 'i') && opts.ignored) ||
	    ((state[TSTATE] == 'j') && opts.junk && !hidden(gfile)) ||
	    ((state[TSTATE] == 'R') && opts.subrepos) ||
	    ((state[TSTATE] == 's') && opts.sfiles) ||
	    ((state[TSTATE] == 'x') && opts.extras) ||
	    ((state[LSTATE] == 'l') && opts.locked) ||
	    ((state[LSTATE] == 'u') && opts.unlocked) ||
	    ((state[CSTATE] == 'c') && opts.changed) ||
	    ((state[PSTATE] == 'p') && opts.pending) ||
	    ((state[GSTATE] == 'G') && opts.gotten) ||
	    (opts.notgotten &&
		(state[TSTATE] == 's') && (state[GSTATE] != 'G')) ||
	    ((state[NSTATE] == 'n') && opts.names) ||
	    ((state[YSTATE] == 'y') && opts.cfiles);
	if (doit) {
		if (opts.cold) {
			sname = name2sccs(gfile);
			willneed(sname);
			free(sname);
		}
		print_it(state, gfile, rev);
	}
}

/*
 * Called for each directory traversed by sfiles_walk()
 */
private int
sccsdir(char *dir, void *data)
{
	winfo	*wi = (winfo *)data;
	char	**slist = 0;
	char	**gfiles = 0;
	MDBM	*gDB = wi->gDB;
	MDBM	*sDB = 0;
	char	*relp, *p, *gfile;
	kvpair  kv;
	sccs	*s = 0;
	char	**t;
	ser_t	d;
	int	i;
	int	rc;
	int	saw_locked = 0;
	int	saw_pending = 0;
	char	buf[MAXPATH];
	char	buf1[MAXPATH];

	if (opts.progress) uprogress();

	if (t = sdir_getdir(proj, dir)) {
		char	*sfile;

		sDB = mdbm_mem();
		EACH(t) {
			sfile = t[i];

			if (isdigit(sfile[0]) &&
			    patheq(sfile+1, ".ChangeSet")) {
				/* ignore heap files */
				continue;
			}
			if (pathneq("s.", sfile, 2)) {
				slist = addLine(slist, strdup(sfile));
			}
			mdbm_store_str(sDB, sfile, "", MDBM_INSERT);
		}
		freeLines(t, free);
	}

	/*
	 * First eliminate as much as we can from SCCS dir;
	 * the leftovers in the gDB should be extras.
	 *
	 * Note: We use the fifo to loop thru the sfile
	 * because if we use mdbm_frist/next, we cannot delete
	 * entry while we are in the first/next loop, it screw up
	 * the mdbm internal index.
	 */
	sortLines(slist, 0);	/* walkdir() has a funny sort */
	EACH (slist) {
		char	*file;
		char	*magicPfile;
		u32	flags = INIT_NOCKSUM;
		STATE	state = "       ";

		p = slist[i];
		s = 0;
		file = p;
		gfile = &file[2];

		state[TSTATE] = 's';
		if (magicPfile = mdbm_fetch_str(gDB, gfile)) {
			state[GSTATE] = 'G';
			flags = INIT_HASgFILE;
			unless (*magicPfile) magicPfile = 0;
		}

		/*
		 * look for p.file,
		 */
		file[0] = 'p';
		if (magicPfile || mdbm_fetch_str(sDB, file)) {
			char *gfile;	/* a little bit of scope hiding ... */
			char *sfile;

			state[LSTATE] = 'l';
			saw_locked = 1;
			file[0] = 's';
			concat_path(buf, dir, "SCCS");
			concat_path(buf, buf, file);
			sfile = buf;
			if (strneq(sfile, "./", 2)) sfile += 2;
			gfile = sccs2name(sfile);
// LMXXX - we could optimize this further.  If they are not looking for
// opts.sfiles or opts.changed or opts.locked then we won't need this info.
			if ((opts.verbose || opts.changed) && opts.timestamps &&
			    proj && !timestamps) {
				timestamps = generateTimestampDB(proj);
			}
			if ((opts.verbose || opts.changed) &&
			    (flags & INIT_HASgFILE) &&
			    (!timestamps ||
				!timeMatch(proj, gfile, sfile,
				    timestamps)) &&
			    (s = init(buf, flags, sDB, 0))) {
				if ((rc = chk_diffs(s)) < 0) opts.error = 1;
				if (rc) state[CSTATE] = 'c';
			}
			free(gfile);
			if (opts.names) {
				unless (s) s = init(buf, flags, sDB, 0);
				d = sccs_top(s);
				relp = proj_relpath(s->proj, s->gfile);
				unless (HAS_PATHNAME(s, d) &&
				    patheq(relp, PATHNAME(s, d))) {
					state[NSTATE] = 'n';
				}
				free(relp);
			}
		} else {
			/*
			 * LMXXX - note that if we get here and the gfile
			 * is writable it may have diffs and we are not
			 * listing them.
			 */
			state[LSTATE] = 'u';
			file[0] = 's';
			concat_path(buf, dir, "SCCS");
			concat_path(buf, buf, file);
			if (opts.names &&
			    (s = init(buf, flags, sDB, 0))) {
				d = sccs_top(s);
				relp = proj_relpath(s->proj, s->gfile);
				unless (HAS_PATHNAME(s, d) &&
				    patheq(relp, PATHNAME(s, d))) {
					state[NSTATE] = 'n';
				}
				free(relp);
				sccs_free(s);
				s = 0;
			}
		}
		file[0] = 'c';
		if (mdbm_fetch_str(sDB, file)) state[YSTATE] = 'y';
		file[0] = 's';
		concat_path(buf, dir, gfile);
		if (opts.pending || opts.verbose) {
			/*
			 * check for pending deltas
			 */
			chk_pending(s, buf, state, sDB, gDB);
			if (state[PSTATE] == 'p') saw_pending = 1;
		} else {
			state[PSTATE] = ' ';
			do_print(state, buf, 0);
		}
		if (s) sccs_free(s);
	}

	/*
	 * Check the sDB for "junk" file
	 * XXX TODO: Do we consider the r.file and m.file "junk" file?
	 */
	if (opts.verbose || opts.junk) {
		concat_path(buf, dir, "SCCS");
		EACH_KV (sDB) {
			/*
			 * Ignore x.files in the SCCS dir if the matching
			 * s.file exists.
			 * Ignore SCCS/c.JUNK if JUNK exists.
			 * Ignore SCCS/c.JUNK if s.JUNK exists.
			 */
			if (kv.key.dptr[1] == '.') {
				switch(kv.key.dptr[0]) {
				    case 's': continue;
				    case 'd': case 'p': case 'x':
					strcpy(buf1, kv.key.dptr);
					buf1[0] = 's';
					if (mdbm_fetch_str(sDB, buf1)) {
						continue;
					}
					break;
				    case 'c':
					strcpy(buf1, kv.key.dptr + 2);
				    	if (mdbm_fetch_str(gDB, buf1)) {
						continue;
					}
					strcpy(buf1, kv.key.dptr);
					buf1[0] = 's';
				    	if (mdbm_fetch_str(sDB, buf1)) {
						continue;
					}
					break;
				}
			}
			concat_path(buf1, buf, kv.key.dptr);
			do_print("j      ", buf1, 0);
		}
	}

	/*
	 * Everything left in the gDB is extra
	 */
	if (opts.extras || opts.ignored || opts.verbose) {
		EACH_KV(gDB) gfiles = addLine(gfiles, kv.key.dptr);
		sortLines(gfiles, 0);
		EACH(gfiles) {
			buf[0] = 's'; buf[1] = '.';
			strcpy(&buf[2], gfiles[i]);
			if (mdbm_fetch_str(sDB, buf)) continue;
			concat_path(buf, dir, gfiles[i]);
			buf1[0] = 'c'; buf1[1] = '.';
			strcpy(&buf1[2], gfiles[i]);
			if (mdbm_fetch_str(sDB, buf1)) {
				do_print("x-----y", buf, 0);
			} else {
				do_print("x------", buf, 0);
			}
		}
		freeLines(gfiles, 0);
	}

	/* maintain dirstate */
	if (opts.atRoot) {
		if (strneq(dir, "./", 2)) dir += 2;
		proj_dirstate(0, dir, DS_EDITED, saw_locked);
		if (opts.pending || opts.verbose) {
			proj_dirstate(0, dir, DS_PENDING, saw_pending);
		}
		opts.saw_locked |= saw_locked;
		opts.saw_pending |= saw_pending;
	}
	freeLines(slist, free);
	mdbm_close(sDB);
	winfo_free(wi);
	return (0);
}

/*
 * Enable fast scan mode for pending file (base on d.file)
 * Caller must ensure we at at the project root
 */
void
enableFastPendingScan(void)
{
	unless (exists(DFILE)) touch(DFILE, 0666);
}

/*
 * This struct contains the data that is passed to findsfiles() by walkdir()
 * when called by walksfiles().
 */
typedef struct sinfo sinfo;
struct sinfo {
	filefn	*fn;		/* call this function on each sfile */
	void	*data;		/* pass this to the fn() */
	int	rootlen;	/* the len of the dir passed to walksfiles() */
	char	*proj_prefix;	/* the prefix needed to make a relpath */
	u32	is_clone:1;	/* special clone walkfn */
	u32	skip_etc:1;	/* skip BitKeeper/etc */
};

private int
findsfiles_sdir(char *dir, void *data)
{
	sinfo	*si = (sinfo *)data;
	char	**sdir;
	char	*t;
	int	ret, i;
	char	buf[MAXPATH];

	sdir = sdir_getdir(proj, dir);
	ret = 0;
	EACH(sdir) {
		t = sdir[i];
		unless (pathneq(t, "s.", 2) ||
		    (si->is_clone && pathneq(t, "d.", 2))) {
			continue;
		}

		/*
		 * skip SCCS/s.ChangeSet at root of
		 * repo in clone (we insert it manually)
		 */
		if (si->is_clone && streq(dir, ".") &&
		    patheq(t, "s.ChangeSet")) {
			continue;
		}
		concat_path(buf, dir, "SCCS");
		concat_path(buf, buf, t);
		ret = si->fn(buf, 'f', si->data);
		if (ret) break;
	}
	freeLines(sdir, free);
	return (ret);
}


private int
findsfiles(char *file, char type, void *data)
{
	char	*p = strrchr(file, '/');
	sinfo	*si = (sinfo *)data;
	char	buf[MAXPATH];
	char	tmp[MAXPATH];

	unless (p) return (0);
	if (type == 'd') {
		if (patheq(p+1, "SCCS")) return (-1);
		if ((p - file > si->rootlen) && patheq(p+1, "BitKeeper")) {
			/*
			 * Do not cross into other package roots
			 * (e.g. RESYNC).
			 */
			strcat(file, "/etc");
			if (exists(file)) {
				project	*tmp;

				*p = 0;
				tmp = proj_init(file);
				if (!si->is_clone && (proj == prodproj) &&
				    tmp && proj_isComponent(tmp)) {
					/* this is a component */
					sprintf(p, "/SCCS/s.ChangeSet");
					si->fn(file, 0, si->data);
					*p = 0;

					/* also list deep components here */
					walk_deepComponents(file,
					    si->fn, si->data);
				}
				if (tmp) proj_free(tmp);
				strcpy(p, "/BitKeeper"); /* restore file */
				return (-2);
			}
		}
		if (si->is_clone && si->skip_etc &&
		    streq(file + 2, "BitKeeper/etc")) {
			return (-1);
		}
		if (prunedirs) {
			concat_path(buf, si->proj_prefix,
			    file + si->rootlen + 1);
			if (match_globs(buf, prunedirs, 0)) {
				sprintf(tmp, "%s/.bk%s%s",
				    proj_root(0), si->proj_prefix,
				    file + si->rootlen + 1);
				/* we allow BKTMP for park */
				if (isdir(tmp) && !streq(buf+1, BKTMP)) {
					die("sfiles: -prune %s when %s exists\n",
					    buf + 1,
					    tmp + strlen(proj_root(0)) + 1);
				}
				return (-1);
			}
		}
	} else {
		if (!opts.no_bkskip && patheq(p+1, BKSKIP)) {
			/*
			 * Skip directory containing a .bk_skip file
			 */
			*p = 0;	/* file == just directory */
			sprintf(tmp, "%s/.bk%s",
			    proj_root(0), si->proj_prefix);
			concat_path(tmp, tmp, file + si->rootlen);
			if (isdir(tmp)) {
				die("sfiles: %s/.bk_skip when %s exists\n",
				    tmp + strlen(proj_root(0)) + strlen("/.bk/"),
				    tmp + strlen(proj_root(0)) + 1);
			}
			return (-2);
		} else if (si->is_clone &&
		    pathneq(file+2, BAM_ROOT, strlen(BAM_ROOT))) {
			/*
			 * lclone wants all the BAM files.  Under
			 * clone the default prunedirs will prune this
			 * directory, but lclone will traverse it.
			 * We want to skip the symlink'ed BAM directories
			 * for syncroot, those will get recreated.
			 */
			unless (type == 'l') {
				return (si->fn(file, type, si->data));
			}
		}
	}
	return (0);
}

int
walksfiles(char *dir, filefn *fn, void *data)
{
	char	*p;
	sinfo	si = {0};
	int	rc;

	si.fn = fn;
	si.data = data;
	si.rootlen = strlen(dir);
	load_project(dir);
	if (proj) {
		p = proj_relpath(proj, dir);
		if (streq(p, ".")) {
			si.proj_prefix = strdup("/");
		} else {
			si.proj_prefix = aprintf("/%s", p);
		}
		free(p);
	}
	rc = walkdir(dir,
	    (walkfns){ .file = findsfiles, .dir = findsfiles_sdir }, &si);
	if (proj) free(si.proj_prefix);
	freeLines(components, free);	/* set by walk_deepComponents() */
	components = 0;
	free_project();
	return (rc);
}

/* generate a list of files to create the sfio in clone */
int
sfiles_clone_main(int ac, char **av)
{
	int	i, c;
	int	lclone = 0;
	int	modes = 0;	/* sfio sets modes so more stuff is ok */
	int	mark2 = 0;	/* stick a || between BK files and the rest */
	int	rc = 2;
	int	do_parents = 0;	/* if set, send parent files */
	sinfo	si = {0};
	char	buf[MAXPATH];
	char	*logfiles[] = {	/* files from BitKeeper/log to ship */
		"COMPONENT",
		"urllist",
		"NFILES",  // only with -m from here down
		"NFILES_PRODUCT",
		"ROOTKEY",
		"TIP",
		"checked",
		"HERE"		// unlinked or renamed RMT_HERE in (r)clone
	};
	char	*parents[] = {
		"parent",
		"pull-parent",
		"push-parent",
	};
	longopt	lopts[] = {
		{ "cold",   300 },
		{ 0, 0 }
	};


	while ((c = getopt(ac, av, "2Lmp", lopts)) != -1) {
		switch (c) {
		    case '2': mark2 = 1; break;
		    case 'L': lclone = 1; break;
		    case 'm': modes = 1; break;
		    case 'p': do_parents = 1; break;
		    case 300: opts.cold = 1; break;
		    default: bk_badArg(c, av);
		}
	}
	if (av[optind]) usage();
	load_project(".");
	/* in lclone mode descend into BAM */
	if (lclone) removeLine(prunedirs, "/" BAM_ROOT, free);
	si.fn = fastprint;
	si.rootlen = 1;
	si.proj_prefix = "/";
	si.is_clone = 1;

	for (i = 0; i < sizeof(logfiles)/sizeof(char *); i++) {
		if (!modes && (i == 2)) break;
		concat_path(buf, "BitKeeper/log", logfiles[i]);
		if (exists(buf)) puts(buf);
	}
	for (i = 0; do_parents && (i < sizeof(parents)/sizeof(char *)); i++) {
		concat_path(buf, "BitKeeper/log", parents[i]);
		if (exists(buf)) puts(buf);
	}

	/* just send whatever IDCACHE is local
	 * (may be BitKeeper/log or BitKeeper/etc)
	 */
	strcpy(buf, IDCACHE);
	if (exists(buf)) puts(buf);

	/*
	 * when doing a clone here are a couple more files to include
	 * from the BitKeeper/etc/SCCS dir
	 */
	concat_path(buf, "BitKeeper/etc/SCCS", "x.dfile");
	if (exists(buf)) puts(buf);

	rc = walkdir("./BitKeeper/etc",
	    (walkfns){ .file = findsfiles, .dir = findsfiles_sdir }, &si);
	if (mark2) puts("||");
	if (exists(CHANGESET_H1)) puts(CHANGESET_H1);
	if (exists(CHANGESET_H2)) puts(CHANGESET_H2);
	puts(CHANGESET);
	si.skip_etc = 1;
	unless (rc) {
		rc = walkdir(".",
		    (walkfns){ .file = findsfiles, .dir = findsfiles_sdir },
		    &si);
	}
	free_project();
	return (rc);
}

private void
print_components(char *path)
{
	project	*comp;
	int	i, cwd_len;
	char	*p, *cwd;
	STATE	state;
	char	buf[MAXPATH];
	char	frompath[MAXPATH];
	char	gfile[MAXPATH];

	if (opts.skip_comps) goto done;
	unless (proj && (proj == prodproj)) goto done;
	unless (opts.out) opts.out = stdout;

	fullname(path, frompath);
	cwd = frompath;
	cwd_len = strlen(cwd);
	components = file2Lines(components,
	    proj_fullpath(prodproj, "BitKeeper/log/deep-nests"));
	uniqLines(components, free);
	EACH (components) {
		p = proj_fullpath(prodproj, components[i]);
		unless (pathneq(p, cwd, cwd_len)) continue;
		p += cwd_len + 1;
		sprintf(gfile, "%s/%s", path, p);
		concat_path(buf, gfile, CHANGESET);
		unless (exists(buf)) continue;
		/*
		 * XXX: might this return 0?
		 * might it return a different comp if there
		 * is a cset file  but no repo?
		 */
		comp = proj_init(gfile);
		strcpy(state, "       ");
		state[TSTATE] = 's';
		if (proj_isComponent(comp)) {
			if (opts.pending) {
				if (xfile_exists(buf, 'd')) {
					state[PSTATE] = 'p';
					sprintf(buf, "%s/ChangeSet", gfile);
					chk_pending(0, buf, state, 0, 0);
					proj_free(comp);
					continue;
				}
			}
			if (opts.names) {
				unless (patheq(components[i],
					proj_comppath(comp))) {
					state[NSTATE] = 'n';
				}
			}
		} else {
			state[TSTATE] = 'x';
		}
		unless ((state[TSTATE] == 'x') && !opts.gfile) {
			sprintf(buf, "%s/ChangeSet", gfile);
		}
		do_print(state, buf, 0);
		proj_free(comp);
	}
done:
	freeLines(components, free);
	components = 0;
}

private void
walk_deepComponents(char *path, filefn *fn, void *data)
{
	int	i, x;
	char	*p;
	project	*comp;
	char	buf[MAXPATH];
	char	frompath[MAXPATH];
	char	gfile[MAXPATH];

	fullname(path, frompath);
	unless (components) {
		components = file2Lines(0,
		    proj_fullpath(prodproj, "BitKeeper/log/deep-nests"));
		uniqLines(components, free);
	}
	EACH(components) {
		p = proj_fullpath(prodproj, components[i]);
		unless ((x = paths_overlap(frompath, p)) &&
		    (frompath[x] == 0) && (p[x] != 0)) {
			continue;
		}
		p += x + 1;
		sprintf(gfile, "%s/%s", path, p);
		concat_path(buf, gfile, CHANGESET);
		unless (exists(buf)) continue;

		comp = proj_init(gfile);
		if (proj_isComponent(comp)) fn(buf, 'f', data);
		proj_free(comp);
	}
}

int
sfiles_local_main(int ac, char **av)
{
	char	*rev = 0;
	int	c;
	char	*t, *p, *r, *t1;
	char	**aliases = 0;
	FILE	*f, *f1;
	int	standalone = 0, norev = 0, nomods = 0, elide = 0, extras = 0;
	hash	*seen;
	char	**out = 0;
	int	i, rc = 1;
	char	buf[MAXLINE], arg[200];
	longopt	lopts[] = {
		{ "elide",   310 },
		{ "extras",  315 },
		{ "no-mods", 320 },
		{ "no-revs", 330 },
		{ 0, 0 }
	};

	while ((c = getopt(ac, av, "r;Ss;", lopts)) != -1) {
		switch (c) {
		    case 'r': rev = optarg; break;
		    case 'S': standalone = 1; break;
		    case 's':
			aliases = addLine(aliases, strdup(optarg));
			break;
		    case 310: elide = 1; break;
		    case 315: extras = 1; break;
		    case 320: nomods = 1; break;
		    case 330: norev = 1; break;
		    default: bk_badArg(c, av);
		}
	}
	bk_nested2root(standalone);
	if (aliases && standalone) usage();

	seen = hash_new(HASH_MEMHASH);
	/* do pending first */
	sprintf(buf, "bk %s%s%s",
	    standalone ? "sfiles -g" : "-A",
	    nomods ? "p" : "cp",
	    extras ? "x" : "");
	EACH(aliases) {
		sprintf(arg, " -s%s", aliases[i]);
		strcat(buf, arg);
	}
	f = popen(buf, "r");
	while (t = fgetline(f)) {
		if (streq(basenm(t), "ChangeSet")) {
			/* component is pending */
			char	*cmd, *dir, *file;

			dir = dirname(t);
			cmd = aprintf("bk --cd='%s' rset %s -SHr@+..",
			    dir, elide ? "--elide": "");
			f1 = popen(cmd, "r");
			while (t1 = fgetline(f1)) {
				p = strchr(t1, '|');
				*p = 0;
				file = aprintf("%s/%s", dir, t1);
				r = p + 1;
				if (p = strstr(r, "..")) *p = 0;
				/* stomp any mods, or simple pending */
				hash_storeStrStr(seen, file, r);
				free(file);
			}
			free(cmd);
			if (pclose(f1)) {
				fprintf(stderr, "%s: '%s' failed\n", prog, cmd);
				goto out;
			}
			continue;
		}
		hash_insertStrStr(seen, t, "@+");
	}
	if (pclose(f)) {
		fprintf(stderr, "%s: %s failed\n", prog, buf);
		goto out;
	}
	if (rev) {
		/* now override with rset output */
		sprintf(buf, "bk rset %s -%sHr'%s'..", elide ? "--elide" : "",
		    (standalone ? "S" : ""), rev);
		EACH(aliases) {
			sprintf(arg, " -s%s", aliases[i]);
			strcat(buf, arg);
		}
		f = popen(buf, "r");
		while (t = fgetline(f)) {
			p = strchr(t, '|');
			*p = 0;
			r = p + 1;
			if (p = strstr(r, "..")) *p = 0;
			hash_storeStrStr(seen, t, r);
		}
		if (pclose(f)) {
			fprintf(stderr, "%s: rset -r%s failed\n", prog, rev);
			goto out;
		}
	}
	EACH_HASH(seen) {
		char	*file = (char *)seen->kptr;
		char	*rev  = (char *)seen->vptr;

		p = norev ? strdup(file) : aprintf("%s|%s", file, rev);
		out = addLine(out, p);
	}
	sortLines(out, 0);
	EACH(out) puts(out[i]);
	rc = 0;
out:	freeLines(out, free);
	freeLines(aliases, free);
	hash_free(seen);
	return (rc);
}
