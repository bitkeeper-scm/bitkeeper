/* Copyright (c) 2000 Andrew Chang */ 
#include "system.h"
#include "sccs.h"

#define LSTATE	0	/* lock state: l,u,j,x */
#define CSTATE	1	/* change state: c,' ' */
#define PSTATE	2	/* pending state: p,' ' */
#define NSTATE	3	/* name state: n,' ' */
 
WHATSTR("@(#)%K%");


typedef struct {
	u32     show_markers:1;		/* -v: show markers */
	u32     all:1;			/* -a: disable ignore list */
	u32	locked:1;		/* -l: list locked files */
	u32	unlocked:1;		/* -u: list unlocked files */
	u32	junk:1;			/* -j: list junk in SCCS dirs */
	u32	extras:1;		/* -x: list extra files */
	u32	modified:1;		/* -c: list changed files */
	u32	names:1;		/* -n: list files in wrong path */
	u32	nflg:1;			/* -sn: list unchanged files */
					/* XXX -  seems wrong */
	u32	pending:1;		/* -p: list pending files */
	u32     gfile:1;     		/* print gfile name	*/
	u32     Aflg:1;			/* use with -p show	*/
					/* all pending deltas	*/
	u32     Cflg:1;     		/* want file<BK_FS>rev format	*/
	u32     splitRoot:1;   		/* split root mode	*/
	u32     dfile:1;   		/* use d.file to find 	*/
					/* pending delta	*/
	u32	fixdfile:1;		/* fix up  the dfile tag */
	u32	progress:1;		/* if set, send progress to stdout */
	FILE	*out;			/* send output here */
	u32     summarize:1;     	/* summarize output only */
	u32     useronly:1;     	/* list user file only 	*/
} options;

typedef struct _q_item {
	char	*path;
	struct	_q_item *next;
} q_item;

typedef struct {
	q_item	*first;
	q_item	*last;
} fifo;

private	jmp_buf	sfind_exit;
private project *proj;
private options	opts;
private globv	ignore; 
private u32	d_count, s_count, x_count; /* progress counter */
private u32	s_last, x_last; /* progress counter */
private u32	c_count, n_count, p_count;

private void	do_print(char state[5], char *file, char *rev);
private void	walk(char *dir, int level);
private void	file(char *f);
private void	sccsdir(char *dir, int level, DIR *sccs_dh, char buf[MAXPATH]);
private void	progress(int force);
private int	chk_diffs(sccs *s);


private void
enqueue(fifo *q, char *path)
{
	q_item	*t = (q_item *) malloc(sizeof(q_item));
	
	assert(t);
	t->path = strdup(path);
	t->next = 0;
	if (q->first == NULL) {
		q->first = t;
	} else {
		q->last->next = t;
	}
	q->last = t;
}

private char *
dequeue(fifo *q)
{
	char	*t;
	q_item	*i;

	if (q->first == NULL) return NULL;
	i = q->first;
	q->first = (q->first == q->last) ? NULL : q->first->next; 
	t = i->path;
	free(i);
	return (t);
}

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

		assert(gDB);
		flags |= INIT_NOSTAT;
		assert(*p == 's');
		if (hasfile(p, 'c', sDB)) flags |= INIT_HAScFILE;
		if (hasfile(p, 'p', sDB)) flags |= INIT_HASpFILE;
		if (hasfile(p, 'x', sDB)) flags |= INIT_HASxFILE;
		if (hasfile(p, 'z', sDB)) flags |= INIT_HASzFILE;
		if (mdbm_fetch_str(gDB, &p[2])) flags |= INIT_HASgFILE;
		*p = 's'; /* because hasfile() stomps */
	}
	unless (opts.splitRoot) flags |= INIT_ONEROOT;
	if (strneq(name, "./", 2)) name += 2;
	s = sccs_init(name, flags|INIT_SAVEPROJ, proj);
        if (s && !proj) proj = s->proj;
        return (s);
}

private void
parse_select(char *sets)
{
	char *p;

	p = sets;
	/* parse set 1 */
	while (*p && (*p != ',')) {
		switch (*p) {
		    case 'l': opts.locked = 1; break;
		    case 'u': opts.unlocked = 1; break;
		    case 'j': opts.junk = 1; break;
		    case 'x': opts.extras = 1; break;
		    default: fprintf(stderr, "unknown set 1 flag %c\n", *p);
		}
		p++;
	}
	unless (*p++ == ',') return;
	/* parse set 2 */
	while (*p && (*p != ',')) {
		switch (*p) {
		    case 'c': opts.modified = 1; break;
		    case 'n': opts.nflg = 1; break;
		    default: fprintf(stderr, "unknown set 2 flag %c\n", *p);
		}
		p++;
	}
	unless (*p++ == ',') return;
	/* parse set 3 */
	while (*p && (*p != ',')) {
		switch (*p) {
		    case 'p': opts.pending = 1; break;
		    default: fprintf(stderr, "unknown set 3 flag %c\n", *p);
		}
		p++;
	}
	unless (*p++ == ',') return;
	while (*p && (*p != ',')) {
		switch (*p) {
		    case 'n': opts.names = 1; break;
		    default: fprintf(stderr, "unknown set 4 flag %c\n", *p);
		}
		p++;
	}
}

int
sfind_main(int ac, char **av)
{
        int     c, i, sfiles_compat = 0; 
	char	*path, *s, buf[MAXPATH];

	if (setjmp(sfind_exit)) return (1); /* error exit */

	if ((ac > 1) && streq("--help", av[1])) {
		system("bk help sfiles");
		return (0);
	}                

	while ((c = getopt(ac, av, "aAcCdDeEgijklno:p|P|rRs:SuUvx")) != -1) {
		switch (c) {
		    case 'a':	opts.all = 1; break;		/* doc 2.0 */
		    case 'A':					/* undoc? 2.0 */
				opts.pending = opts.Aflg = 1; break;
		    case 'c':	opts.modified = 1; break;	/* doc 2.0 */
		    case 'C':					/* undoc? 2.0 */
				opts.pending = opts.Cflg = 1; break;
		    case 'd':	sfiles_compat = 1; break;	/* doc 2.0 */
		    case 'D':	sfiles_compat = 1; break;	/* doc 2.0 */
		    case 'i':	/* see below */	/* doc 2.0 */
		    case 'E':	opts.modified = 1;		/* doc 2.0 */
				opts.nflg = 1;
				opts.names = 1;
				opts.pending = 1;
				/* fall thru */
		    case 'e':	opts.junk = 1;			/* doc 2.0 */
				opts.extras = 1;
				opts.locked = 1;
				unless (c == 'i') opts.unlocked = 1;
				opts.show_markers = 1;
				break;
		    case 'g':	opts.gfile = 1; break;		/* doc 2.0 */
		    case 'j':	opts.junk = 1; break;		/* doc 2.0 */
		    case 'k':	sfiles_compat = 1; break;	/* undoc? 2.0 */
		    case 'l':   opts.locked = 1; break;		/* doc 2.0 */
		    case 'n':   opts.names = 1; break;		/* doc 2.0 */
		    case 'o':					/* doc 2.0 */
				unless (opts.out = fopen(optarg, "w")) {
		    			perror(optarg);
					exit(1);
				}
				opts.progress = 1;
				break;
		    case 'P':	opts.fixdfile = 1;	  	/* undoc 2.0 */
				/* fall thru */
		    case 'p':	opts.pending =1;		/* doc 2.0 */
				for (s = optarg; s && *s; s++) {
					if (*s == 'A') {
						opts.Aflg = 1;
						opts.Cflg = 1;
					} else if (*s == 'C') opts.Cflg = 1;
					else goto usage;
				}
				break;
		    case 'r':	sfiles_compat = 1; break;	/* undoc? 2.0 */
		    case 'R':	sfiles_compat = 1; break;	/* undoc? 2.0 */
		    case 's':	parse_select(optarg); break;	/* undoc? 2.0 */
		    case 'S':	opts.summarize = 1; break;	/* doc 2.0 */
		    case 'u':	opts.unlocked = 1; break;	/* doc 2.0 */
		    case 'U':	opts.useronly = 1; break;	/* doc 2.0 */
		    case 'v':	opts.show_markers = 1; break;	/* doc 2.0 */
		    case 'x':					/* doc 2.0 */
				opts.extras = opts.junk = 1; break;
		    default: 	
usage:				system("bk help -s sfiles");
				return (1);
		}
	}
	
	if (sfiles_compat) {
		getoptReset(); 
		return (sfiles_main(ac, av));
	}

	unless (opts.out) opts.out = stdout;
	fflush(opts.out); /* for win32 */
	c_count = p_count = d_count = s_count = x_count = 0;

	/*
	 * If user did not select any option,
	 * setup a default mode for them
	 */
	if (!opts.unlocked && !opts.locked && !opts.junk && !opts.extras &&
	    !opts.modified && !opts.nflg && !opts.pending && !opts.names) {
		opts.unlocked = 1;
		opts.locked = 1;
	}

	if (!av[optind]) {
		path = ".";
		if (opts.names && !exists(BKROOT)) {
			fprintf(stderr,
			    "sfiles -n must be run at project root.\n");
			exit(1);
		}
		walk(path, 0);
	} else if (streq("-", av[optind])) {
		setmode(0, _O_TEXT); /* read file list in text mode */
		while (fnext(buf, stdin)) {
			chop(buf);
			path = buf;
                        if (isdir(path)) {
				walk(path, 0);
			} else {
                                file(path);
			}
		}
	} else {
		if (opts.names && !exists(BKROOT)) {
			fprintf(stderr,
			    "sfiles -n must be run at project root.\n");
			exit(1);
		}
                for (i = optind; i < ac; ++i) {
                        localName2bkName(av[i], av[i]);
                        if (isdir(av[i])) {
                                path =  av[i];
                                walk(path, 0);
                        } else {
                                path =  av[i];
                                file(path);
                        }
                }
	}
	if (opts.out) fclose(opts.out);
	if (opts.progress) progress(2);
	return (0);
}

private sccs *
chk_sfile(char *name, char state[5])
{
	char	*s;
	sccs	*sc = 0;

	s = strrchr(name, '/');

	if (s[1] == 's') {
		s[1] = 'p';
		if (exists(name)) {
			state[LSTATE] = 'l';
			s[1] = 's';
			if (opts.modified || opts.names) {
			    sc = init(name, INIT_NOCKSUM, 0, 0);
			}
			if (opts.modified && sc && chk_diffs(sc)) { 
				state[CSTATE] = 'c';
			}
			if (opts.names && sc) {
				delta	*d = sccs_top(sc);

				unless (streq(sc->gfile, d->pathname)) {
					state[NSTATE] = 'n';
				}
			}
		} else {
			s[1] = 'z';
			if (exists(name)) {
				state[LSTATE] = 'l';
			} else {
				state[LSTATE] = 'u';
			}
			s[1] = 's';
			if (opts.names && 
			    (sc = init(name, INIT_NOCKSUM, 0, 0))) {
				delta	*d = sccs_top(sc);

				unless (streq(sc->gfile, d->pathname)) {
					state[NSTATE] = 'n';
				}
				sccs_free(sc);
				sc = 0;
			}
		}
	}
	return (sc);
}

private void
chk_pending(sccs *s, char *gfile, char state[5], MDBM *sDB, MDBM *gDB)
{
	delta	*d;
	int	local_s = 0, printed = 0;
	char	buf[MAXPATH], *dfile = 0, *p;


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
			dfile = name2sccs(gfile);
			p = basenm(dfile);
			*p = 'd';
			unless (exists(dfile)) {
				free(dfile);
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
			fprintf(stderr, "sfiles: %s: bad sfile\n", sfile);
			if (s) sccs_free(s);
			if (dfile) free(dfile);
			return;
		}
		free(sfile);
		local_s = 1;
	}

	unless (dfile) {
		dfile = name2sccs(gfile);
		p = basenm(dfile);
		*p = 'd';
	}
	
	/*
	 * check for pending deltas
	 */                                    
	state[PSTATE] = ' ';
	unless (d = sccs_getrev(s, "+", 0, 0))  goto out;	
	if (d->flags & D_CSET) goto out;

	/*
	 * If it is out of view, we need to look at all leaves and see if
	 * there is a problem or not.
	 */
	if (s->defbranch && streq(s->defbranch, "1.0")) {
		for (d = s->table; d; d = d->next) {
			unless ((d->type == 'D') && sccs_isleaf(s, d)) {
				continue;
			}
			unless (d->flags & D_CSET) break;
		}
		unless (d) goto out;
		fprintf(stderr,
		    "Warning: not in view file %s skipped.\n", s->gfile);
		goto out;
	}

	assert(!(d->flags & D_CSET));
	state[PSTATE] = 'p';
	if (opts.Aflg) {
		do {
			do_print(state, gfile, d->rev);
			d = d->parent;
		} while (d && !(d->flags & D_CSET));
		printed = 1;
	} else if (opts.Cflg) {
		do_print(state, gfile, d->rev);
		printed = 1;
	} 

out:	unless (printed) do_print(state, gfile, 0);
	/*
	 * Do not sccs_free() if it is passed in from outside 
	 */
	if (local_s) sccs_free(s);
	if (opts.fixdfile) {
		/* No pending delta, remove redundant d.file */
		unless (state[PSTATE] == 'p') {
			unlink(dfile);
		} else {
			touch(dfile, 0666);
		}
	}
	if (dfile) free(dfile);
}

private void
file(char *f)
{
	char	name[MAXPATH], buf[MAXPATH];
	char    *s, *sfile, state[5] = "    ";
	sccs	*sc = 0;

	if (strlen(f) >= sizeof(name)) {
		fprintf(stderr, "sfiles: name too long: [%s]\n", f);
		return;
	}
	strcpy(name, f);
	s = rindex(name, '/');
	/*
	 * There are three possible condition
	 * a) f is a sfile
	 * b) f is a regular or new gfile
	 * c) f is a junk file in the SCCS directory
	 */
	if (s && (name <= &s[-4]) && pathneq("SCCS", &s[-4], 4)) {
		/* this file is under a SCCS dir */
		unless (sccs_filetype(f)) {
			state[CSTATE] = 'j';
			strcpy(buf, f);
		} else {
			sc = chk_sfile(f, state);
			s = sccs2name(f);
			strcpy(buf, s);
			free(s);
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
			state[CSTATE] = 'x';
		} else {
			sc = chk_sfile(sfile, state);
		}
		free(sfile);
		strcpy(buf, f);
	}

	/*
	 * When we get here. buf contains the gname
	 * Now we check for pending deltas
	 */
	if (opts.pending && state[CSTATE] != 'x' &&  state[CSTATE] != 'j') {
		chk_pending(sc, buf, state, 0, 0);
	} else  {
		if (state[CSTATE] == 'x' || state[CSTATE] == 'j') {
			if (exists(buf)) {
				state[LSTATE] = state[PSTATE] = ' ';
				do_print(state, buf, 0);
			}
		} else {
			state[PSTATE] = ' ';
			do_print(state, buf, 0);
		}
	}
	if (sc) sccs_free(sc);
}

private void
print_summary()
{
	fprintf(opts.out, "%6d files under revision control.\n", s_count);
	if (opts.extras) {
		fprintf(opts.out,
		    "%6d files not under revision control.\n", x_count);
	}
	if (opts.modified) {
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
 * This function returns NULL if it cannot find project root
 */
private char *
find_root(char *dir, char *root)
{
	return (_relativeName(dir, 1, 0, 1, 0, 0, root));
}

private void
walk(char *dir, int level)
{
	struct dirent   *e;
	DIR	*dh, *sccs_dh;
	char	buf[MAXPATH], *p;
	fifo	dlist = {0, 0};
#ifndef WIN32	/* Linux 2.3.x NFS bug, skip repeats. */
        ino_t	lastInode = 0;
#endif                                 

	/*
	 * Special processing for .bk_skip file
	 */
	sprintf(buf, "%s/%s", dir, BKSKIP);
	if (exists(buf)) return;

	if (level == 0) {
		char tmp[MAXPATH];

		/*
		 * Find project root and put it in buf
		 */
		if (find_root(dir, buf)) {
			opts.splitRoot = hasRootFile(buf, tmp);
			if (!opts.all) {
				FILE	*ignoref; 

				sprintf(tmp, "%s/BitKeeper/etc/ignore", buf);
				unless (exists(tmp)) get(tmp, SILENT, "-");
				if (ignoref = fopen(tmp, "rt")) {
					ignore = read_globs(ignoref, 0);
					fclose(ignoref);
				}
			}           
			unless (opts.fixdfile) {
				sprintf(tmp, "%s/%s", buf, DFILE);
				opts.dfile = exists(tmp);
			}
		} else {
			/*
			 * Dir is not a BitKeeper repository,
			 * turn off all BitKeeper specific feature.
			 */
			opts.all = 1;
		}
		assert(proj == 0);
#if 0
		/*
		 * XXX TODO: should we reset the progress counter ?
		 */
		s_count = x_count = 0;
#endif
	}

	concat_path(buf, dir, "SCCS");
	/*
	 * TODO: for better performance in split root mode,
	 * we should be able to better optimized the sPath() code
	 */
	sccs_dh =  opendir(opts.splitRoot ? sPath(buf, 1) : buf);
	unless (sccs_dh) {
		/*
		 * TODO: we need to check for the caes where
		 * the pwd is a SCCS dir
		 * This can fool the current code into wronly treating them
		 * as xtras.
		 */
		if ((dh = opendir(dir)) == NULL) {
			perror(dir);
			goto done;
		}
		while ((e = readdir(dh)) != NULL) { 
#ifndef WIN32		/* Linux 2.3.x NFS bug, skip repeats. */
			if (lastInode == e->d_ino) continue;
			lastInode = e->d_ino;
#endif
			if (streq(e->d_name, ".") || streq(e->d_name, "..")) {
				continue;
			}
			concat_path(buf, dir, e->d_name);
			unless (isdir(buf)) {
				do_print("xxxx", buf, 0);
			} else {
				enqueue(&dlist, buf);
			}
		}
		closedir(dh);
		while (p = dequeue(&dlist)) {
			walk(p, level + 1);
			free(p);
		}
	} else {
		sccsdir(dir, level, sccs_dh, buf);
	}

done:	if (level == 0) {
		if (ignore) free_globs(ignore);  ignore = 0;
		if (proj) proj_free(proj); proj = 0;
		if (opts.summarize) print_summary();

		/*
		 * We only enable fast scan mode if we started at the root
		 */
		if (opts.fixdfile && isdir("BitKeeper/etc/SCCS")) {
			enableFastPendingScan();
		}
	}
	if (opts.progress) progress(0);
}

private void
progress(int force)
{
	char	buf[100];

	if (!force && (s_last == s_count) && (x_last == x_count)) return;
	sprintf(buf, "%d %d %d %d\n", s_count, x_count, d_count, c_count);
	/* If we get an error, it usually means that we are to die */
	if (write(1, buf, strlen(buf)) != strlen(buf)) exit(1);
	s_last = s_count;
	x_last = x_count;
	if (force == 2) {
		usleep(300000);		/* let TK update */
	}
}

private int
chk_diffs(sccs *s)
{
	if (!s) return (0);
	if (sccs_hasDiffs(s, 0, 1) >= 1) return (1);
	return (0);  
}

int
isIgnored(char *file)
{
	char *gfile, *p, *q, save;
	struct stat sbuf;
	int len;

	gfile =  strneq("./",  file, 2) ? &file[2] : file;
	unless (opts.all) {
		if (match_globs(file, ignore)) {
			debug((stderr, "SKIP\t%s\n", file));
			return (1);
		}

		/*
		 * For backward compat with "bk sfiles"
		 * trimed "./" and match against ignore list.
		 */
		if ((gfile !=  file) && match_globs(gfile, ignore)) {
			debug((stderr, "SKIP\t%s\n", gfile));
			return (1);
		}

		/*
		 * For backward compat with "bk sfiles"
		 * match basename/dirname against ignore list.
		 */
		p = gfile;
		while (p) {
			for (q = p; *q && (*q != '/'); q++);
			save = *q;
			*q = 0;
			if (match_globs(p, ignore)) {
				*q = save;
				debug((stderr, "SKIP\t%s\n", gfile));
				return (1);
			}
			*q = save;
			if (save == 0) break;
			p = ++q;
		}

		/* ignore special file e.g. char/block/fifo file */
		if (lstat(gfile, &sbuf)) {
			perror(gfile);
			return (1);
		}
		unless (sbuf.st_mode && S_IFREG|S_IFLNK) return (1);
	}

	/*
	 * HACK to hide stuff in the log and locks directory
	 * This assumes that sfind is ran from project root
	 * If you run "bk sfind" under <project root>/BitKeeper directory,
	 * these file will show up. It is probably OK.
	 */
	len = strlen(gfile);
	if ((len >= 14) && strneq("BitKeeper/log/", gfile, 14)) return (1);
	if ((len >= 17) && strneq("BitKeeper/writer/", gfile, 17)) return (1);
	if ((len >= 18) && strneq("BitKeeper/readers/", gfile, 18)) return (1);
	if ((len >= 8) && strneq("PENDING/", gfile, 8)) return (1);
	
	return (0);
}

private int
isTagFile(char *file)
{
	char *gfile;

	gfile =  strneq("./",  file, 2) ? &file[2] : file;
	return (strneq("BitKeeper/etc/SCCS/x.", gfile, 21));
}

private void
print_it(char state[5], char *file, char *rev)
{
	char *sfile, *gfile;

	gfile =  strneq("./",  file, 2) ? &file[2] : file;
	if (opts.useronly) {
		if (streq(gfile, "ChangeSet") ||
		    (strlen(gfile) > 10) && strneq(gfile, "BitKeeper/", 10)) {
			return;
		}
	}
	if (opts.show_markers) {
		if (state[CSTATE] == 'j') {
			assert(streq(state, " j "));
			if (fprintf(opts.out, "jjjj ") != 5) {
error:				perror("output error");
				fflush(stderr);
				longjmp(sfind_exit, 1); /* back to sfind_main */
			}
		} else {
			if (fprintf(opts.out, "%s ", state) != 5) {
				goto error;
			}
		}
	}
	if (opts.gfile || (state[CSTATE] == 'x') || (state[CSTATE] == 'j'))  {
		if (fputs(gfile, opts.out) < 0) goto error;
	} else {
		sfile = name2sccs(gfile);
		/*
		 * TODO: for better performance in split root mode,
		 * we should be able to better optimized the sPath() code
		 * e.g. we could pass project struct into sPath()
		 */
		if (fputs(opts.splitRoot ?
				sPath(sfile, 0) : sfile, opts.out) < 0) {
			goto error;
		}
		free(sfile);
	}
	if (rev) {
		int rlen = strlen(rev) + 1;
		if (fprintf(opts.out, "%c%s", BK_FS, rev) != rlen) {
			goto error;
		}
	}
	if (fputs("\n", opts.out) < 0) goto error;
}

private void
do_print(char state[5], char *file, char *rev)
{

	if (state[PSTATE] == 'p') p_count++;
	if (state[NSTATE] == 'n') n_count++;
	switch (state[CSTATE]) {
	    case 'j': break;
	    case 'x': unless (isIgnored(file)) x_count++; break;
	    case 'c': c_count++;
	    	/* fall through */
	    default: s_count++; break;
	}
	if (opts.progress && 
	    (((s_count - s_last) > 100) || ((x_count - x_last) > 100))) { 
		progress(1);
	}
	if (opts.summarize) return; /* skip the detail */
	if ((state[PSTATE] == 'p') && opts.pending) goto print;

	switch (state[LSTATE]) {
	    case 'l':	if (opts.locked) goto print; break;
	    case 'u':	if (opts.unlocked) goto print; break;
	}

	switch (state[CSTATE]) {
	    case 'c':	if (opts.modified) goto print; break;
	    case 'n':	if (opts.nflg) goto print; break;
	    case 'j':	if (opts.junk && !isTagFile(file)) goto print; break;
	    case 'x':	if (opts.extras && !isIgnored(file)) goto print; break;
	}

	if ((state[NSTATE] == 'n') && opts.names) goto print;
	return;

print:	print_it(state, file, rev);
}

char *
append_rev(MDBM *db, char *name, char *rev, char *buf)
{
	char *t;

	t = mdbm_fetch_str(db, name);
	unless (t) return (rev);
	sprintf(buf, "%s,%s", t, rev);
	assert(strlen(buf) < MAXPATH);
	return (buf);
}

/*
 * Called for each directory that has an SCCS subdirectory
 */
private void
sccsdir(char *dir, int level, DIR *sccs_dh, char buf[MAXPATH])
{
	MDBM	*gDB = mdbm_open(NULL, 0, 0, GOOD_PSIZE);
	MDBM	*sDB = mdbm_open(NULL, 0, 0, GOOD_PSIZE);
	fifo	dlist = {0, 0};
	fifo	slist = {0, 0};
	struct dirent   *e;
	DIR	*dh; /* dir handle */
	int 	dir_len = strlen(dir);
	char	*p, *gfile;
	datum	k;
	sccs	*s = 0;
	q_item	*i;
#ifndef WIN32	/* Linux 2.3.x NFS bug, skip repeats. */
        ino_t	lastInode = 0;
#endif                                 

	/*
	 * Get all the SCCS/?.files
	 */
	while (e = readdir(sccs_dh)) {
#ifndef WIN32	/* Linux 2.3.x NFS bug, skip repeats. */
		if (lastInode == e->d_ino) continue;
		lastInode = e->d_ino;
#endif
		if (streq(e->d_name, ".") || streq(e->d_name, "..")) continue;
		/*
		 * Skip files with paths that are too long
		 * pad = "/SCCS/" = 6
		 */
		if ((dir_len + 6 + strlen(e->d_name)) >= MAXPATH) {
			fprintf(stderr,
			    "Warning: %s/%s name too long, skipped\n",
			    dir, e->d_name);
			continue;
		}

		if (strneq("s.", e->d_name, 2)) {
			enqueue(&slist, e->d_name);
		} else {
			if (strneq("c.", e->d_name, 2) &&
			    (p = strrchr(e->d_name, '@'))) {
				/*
				 * Special handling for c.file@rev entry
				 * append the @rev part to the value field
				 * so we can print the correct file
				 * name if it turns out to be a junk file. 
				 */
				*p++ = 0;
				p = append_rev(sDB, e->d_name, p, buf);
			} else {
				p = "";
			} 
			mdbm_store_str(sDB, e->d_name, p, MDBM_REPLACE);
		}
	}
	closedir(sccs_dh);

	/*
	 * Get all the gfiles
	 */
	dh = opendir(dir);
	while (e = readdir(dh)) {
#ifndef WIN32	/* Linux 2.3.x NFS bug, skip repeats. */
		if (lastInode == e->d_ino) continue;
		lastInode = e->d_ino;
#endif
		if (streq(e->d_name, ".") || streq(e->d_name, "..")) continue;
		if (patheq(e->d_name, "SCCS")) continue;

		/*
		 * Allow people to prune their tree with ".bk_skip".
		 */
		if (streq(".bk_skip", e->d_name)) goto skip;

		/*
		 * Do not descend into another project root. e.g RESYNC
		 */
		if ((level > 0)  && patheq(e->d_name, "BitKeeper")) {
skip:			mdbm_close(gDB);
			mdbm_close(sDB);
			closedir(dh);
			return;
		}

		/*
		 * Skip files with paths that are too long
		 * pad = "/SCCS/s." = 8
		 */
		if ((dir_len + 8 + strlen(e->d_name)) >= MAXPATH) {
			fprintf(stderr,
			    "Warning: %s/%s name too long, skipped\n",
			    dir, e->d_name);
			continue;
		}

		concat_path(buf, dir, e->d_name);
		if (isdir(buf)) {
			enqueue(&dlist, e->d_name);
		} else {
			mdbm_store_str(gDB, e->d_name, "", MDBM_INSERT);
		}
	}
	closedir(dh);
	d_count++;
	if (opts.progress) progress(1);

	/*
	 * First eliminate as much as we can from SCCS dir;
	 * the leftovers in the gDB should be extras.
	 *
	 * Note: We use the fifo to loop thru the sfile
	 * because if we use mdbm_frist/next, we cannot delete
	 * entry while we are in the first/next loop, it screw up
	 * the mdbm internal index.
	 */
	while (p = dequeue(&slist)) {
		char 	*file;
		char	state[5] = "    ";

		s = 0;
		file = p;
		gfile = &file[2];

		/*
		 * look for p.file,
		 */
		file[0] = 'p';
		if (mdbm_fetch_str(sDB, file)) {
			state[LSTATE] = 'l';
			file[0] = 's';
			concat_path(buf, dir, "SCCS");
			concat_path(buf, buf, file);
			if (opts.modified &&
			    (s = init(buf, INIT_NOCKSUM, sDB, gDB)) &&
			    chk_diffs(s)) {
				state[CSTATE] = 'c';
			}
		} else {
			file[0] = 'z';
			if (mdbm_fetch_str(sDB, file)) {
				state[LSTATE] = 'l';
			} else {
				state[LSTATE] = 'u';
			}
			file[0] = 's';
			concat_path(buf, dir, "SCCS");
			concat_path(buf, buf, file);
			if (opts.names &&
			    (s = init(buf, INIT_NOCKSUM, sDB, gDB))) {
				delta	*d = sccs_top(s);

				unless (streq(s->gfile, d->pathname)) {
					state[NSTATE] = 'n';
				}
				sccs_free(s);
				s = 0;
			}
		}

		concat_path(buf, dir, gfile);
		if (opts.pending) {
			/*
			 * check for pending deltas
			 */
			chk_pending(s, buf, state, sDB, gDB);
		} else {
			state[PSTATE] = ' ';
			do_print(state, buf, 0);
		}

		file[0] = 'c';
		mdbm_delete_str(sDB, file); /* remove c.file entry */
		file[0] = 'd';
		mdbm_delete_str(sDB, file); /* remove d.file entry */
		file[0] = 'p';
		mdbm_delete_str(sDB, file); /* remove p.file entry */
		file[0] = 's';
		mdbm_delete_str(sDB, file); /* remove s.file entry */
		file[0] = 'x';
		mdbm_delete_str(sDB, file); /* remove x.file entry */
		file[0] = 'z';
		mdbm_delete_str(sDB, file); /* remove z.file entry */
		mdbm_delete_str(gDB, gfile);
		if (s) sccs_free(s);
		free(p);
	}

	/*
	 * Check the sDB for "junk" file
	 * XXX TODO: Do we consider the r.file and m.file "junk" file?
	 */
	if (opts.junk) {
		kvpair  kv;
		concat_path(buf, dir, "SCCS");
		for (kv = mdbm_first(sDB); kv.key.dsize != 0;
						    kv = mdbm_next(sDB)) {
			char buf1[MAXPATH];

			concat_path(buf1, buf, kv.key.dptr);
			if (kv.val.dsize == 0) {
				do_print(" j ", buf1, 0);
			} else {
				/*
				 * We only get here when we get 
				 * c.file@rev entries. Extract the @rev part
				 * from kv.val.ptr and append it to buf1 to
				 * reconstruct the correct file name.
				 * i.e c.file@rev
				 */
				p = kv.val.dptr;
				while (p) {
					char *q, *t, buf2[MAXPATH];

					q = strchr(p, ',');
					if (q) *q++ = 0;
					if (*p) {
						t = buf2;
						sprintf(buf2, "%s@%s", buf1, p);
					} else {
						t = buf1;
					}
					do_print(" j ", t, 0);
					p = q;
				}
			}
		}
	}

	/*
	 * Everything left in the gDB is extra
	 */
	if (opts.extras) {
		for (k = mdbm_firstkey(gDB); k.dsize != 0;
						k = mdbm_nextkey(gDB)) {
			concat_path(buf, dir, k.dptr);
			do_print("xxxx", buf, 0);
		}
	}
	mdbm_close(gDB);

	/*
	 * Make sure the directory in the gdir does not have a s.file
	 */
	strcpy(buf, "s.");
	for (i = dlist.first; i; i = i->next) {
		strcpy(&buf[2], i->path);
		if (mdbm_fetch_str(sDB, buf)) {
			concat_path(buf, dir, i->path);
			fprintf(stderr,
			"Warning: %s should not be a directory\n", buf);
		}
	}
	mdbm_close(sDB);
	

	/*
	 * Process the directories
	 */
	while (p = dequeue(&dlist)) {
		concat_path(buf, dir, p);
		walk(buf, level + 1);
		free(p);
	}
}

/*
 * Enable fast scan mode for pending file (base on d.file) 
 * Caller must ensure we at at the project root
 */
void
enableFastPendingScan()
{
	touch(DFILE, 0666);
}
