/* Copyright (c) 2000 Andrew Chang */ 
#include "system.h"
#include "sccs.h"

#define LSTATE 0	/* lock state	 */
#define CSTATE 1	/* change state	 */
#define PSTATE 2	/* pending state */
 
WHATSTR("@(#)%K%");


private	char *sfind_usage = "\n\
usage: sfiles [-aAcCdDglkpPRrux] [directories]\n\n\
    -a		examine all files, even if listed in BitKeeper/etc/ignore\n\
    -A		when used with -p, list all revs, not just the tip\n\
    -c		list changed files (locked and modified)\n\
    -C		list leaves which are not in a changeset as file:1.3\n\
    -e		list everything in quick scan mode\n\
    -E		list everything in detail scan mode\n\
    -j		list junk file under the SCCC directory\n\
    -g		list the gfile name, not the sfile name\n\
    -l		list locked files (p.file and/or z.file)\n\
    -m		annotate the output with state markers\n\
    -n		list unchanged (no-change) files\n\
    -u		list unlocked files\n\
    -p		list file with pending delta(s)\n\
    -x		list files which have no revision control files\n\
		Note 1: files in BitKeeper/log/ are ignored\n\
    		Note 2: revision control files must look like SCCS/s.*,\n\
		not foo/bar/blech/s.*\n\
";

typedef struct {
	u32     show_markers:1;		/* show markers		*/
	u32     aflg:1;			/* disable ignore list	*/
	u32     Aflg:1;			/* use with -p, show	*/
					/* all pending deltas	*/
	u32     jflg:1;			/* show SCCS/junk file 	*/
	u32     sflg:1;			/* show all sfile	*/
	u32     cflg:1;     		/* show changed files	*/
	u32     lflg:1;     		/* show locked files	*/
	u32     nflg:1;     		/* show no-changed file	*/
	u32     uflg:1;     		/* show unlocked files	*/
	u32     pflg:1;     		/* show pending files	*/
	u32     xflg:1;     		/* show xtra files	*/
	u32     gflg:1;     		/* print gfile name	*/
	u32     Cflg:1;     		/* want file@rev format	*/
} options;

typedef struct _q_item {
	char	*path;
	struct	_q_item *next;
} q_item;

typedef struct {
	q_item	*first;
	q_item	*last;
} fifo;

private project *proj;
private options	opts;
private globv	ignore; 

private void do_print(char state[4], char *file, char *rev);
private void walk(char *dir, int level);
private void file(char *f);
private void sccsdir(char *dir, int level);
private int chk_diffs(sccs *s);


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
init(char *name, int flags, MDBM *sDB)
{
        sccs    *s;

	if (sDB) {
		char *p = basenm(name);

		flags != INIT_NOSTAT;
		assert(*p == 's');
		if (hasfile(p, 'c', sDB)) flags |= INIT_HAScFILE;
		if (hasfile(p, 'p', sDB)) flags |= INIT_HASpFILE;
		if (hasfile(p, 'x', sDB)) flags |= INIT_HASxFILE;
		if (hasfile(p, 'z', sDB)) flags |= INIT_HASzFILE;
		*p = 's'; /* because hasfile() stomps */
	}
	s = sccs_init(name, flags|INIT_SAVEPROJ, proj);
        if (s && !proj) proj = s->proj;
        return (s);
}


int
sfind_main(int ac, char **av)
{
        int     c, i; 
	char	*root, *path, buf[MAXPATH];

	if ((ac > 1) && streq("--help", av[1])) {
usage:		fprintf(stderr, "%s", sfind_usage);
		return (0);
	}                

	while ((c = getopt(ac, av, "aAcCeEgjlmpux")) != -1) {
		switch (c) {
		    case 'a':	opts.aflg = 1; break;
		    case 'A':	opts.Aflg = 1; break;
		    case 'c':	opts.cflg = 1; break;
		    case 'g':	opts.gflg = 1; break;
		    case 'j':	opts.jflg = 1; break;
		    case 'l':	opts.lflg = 1; break;
		    case 'n':	opts.nflg = 1; break;
		    case 'C':	opts.Cflg = 1; 
				opts.pflg = 1;
				break; /* backward compat */
		    case 'p':	opts.pflg = 1; break; /* replace old -C */
		    case 'u':	opts.uflg = 1; break; 
		    case 'x':	opts.xflg = 1; break;
		    case 'm':	opts.show_markers = 1; break;
		    case 'E':	opts.cflg = 1;	/* detail scan everything */
				opts.pflg = 1;
				/* fall thru */
		    case 'e':	opts.jflg = 1;	/* quick scan everything */
				opts.lflg = 1;
				opts.nflg = 1;
				opts.xflg = 1;
				break;
		    default: 	goto usage;
		}
	}

	/*
	 * If user did not select any option,
	 * setup a default mode for them
	 */
	if (!opts.cflg && !opts.jflg && !opts.lflg && !opts.pflg &&
			    !opts.nflg && !opts.uflg && !opts.xflg) {
		opts.uflg = 1;
		opts.lflg = 1;
	}


	if (!opts.aflg && (root = sccs_root(0))) {
		FILE	*ignoref; 

		sprintf(buf, "%s/BitKeeper/etc/ignore", root);
		unless (exists(buf)) get(buf, SILENT, "-");
		if (ignoref = fopen(buf, "r")) {
			ignore = read_globs(ignoref, 0);
			fclose(ignoref);
		}
		free(root);
		root = 0;
	}           
	

	if (!av[optind]) {
		path = ".";
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

	if (ignore) free_globs(ignore);  
	if (proj) proj_free(proj);
	return (0);
}

private sccs *
chk_sfile(char *name, char state[4])
{
	char	*s;
	sccs	*sc = 0;

	s = strrchr(name, '/');

	if (s[1] == 's') {
		s[1] = 'p';
		if (exists(name)) {
			state[LSTATE] = 'l';
			s[1] = 's';
			if (opts.cflg && 
			    (sc = init(name, INIT_NOCKSUM, 0)) &&
			    chk_diffs(sc)) { 
				state[CSTATE] = 'c';
			} else {
				state[CSTATE] = 'n';
			}
		} else {
			s[1] = 'z';
			if (exists(name)) {
				state[LSTATE] = 'l';
				state[CSTATE] = 'n';
			} else {
				state[LSTATE] = 'u';
				state[CSTATE] = 'n';
			}
			s[1] = 's';
		}
	}
	return (sc);
}

private void
chk_pending(sccs *s, char *gfile, char state[4], MDBM *sDB)
{
	delta	*d;
	char	*rev;
	int	local_s = 0;

	unless (s) {
		char *sfile = name2sccs(gfile);
		s = init(sfile, INIT_NOCKSUM, sDB);
		free(sfile);
		assert(s);
		local_s = 1;
	}
	
	/*
	 * check for pending deltas
	 * TODO: need to handle LOD
	 */                                    
	d = sccs_top(s);
	assert(d);
	state[PSTATE] = (d->flags & D_CSET) ? ' ' : 'p';
	if (opts.Aflg) {
		if (d->flags & D_CSET) {
			do_print(state, gfile, 0);
		} else {
			while (d) {
				if (d->flags & D_CSET) break;
				do_print(state, gfile, d->rev);
				d = d->parent;
			}
		}
	} else {
		if (opts.Cflg) {
			rev = (state[PSTATE] == 'p') ? d->rev : NULL;
			do_print(state, gfile, rev);
		} else {
			do_print(state, gfile, 0);
		}
	}

	/*
	 * Do not sccs_close() if it is passed in from outside 
	 */
	if (local_s) sccs_close(s);
}

private void
file(char *f)
{
	char	name[MAXPATH], buf[MAXPATH];
	char    *s, *sfile, state[4] = "???";
	sccs	*sc = 0;

	strcpy(name, f);
	s = rindex(name, '/');
	/*
	 * There are tree possible condition
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
		/* this file is a gname */
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
	 * When we get here. buf contain the gname
	 * Now we check for pending deltas
	 */
	if (opts.pflg && state[CSTATE] != 'x' &&  state[CSTATE] != 'j') {
		chk_pending(sc, buf, state, 0);
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
	if (sc) sccs_close(sc);
}



private void
walk(char *dir, int level)
{
	struct dirent   *e;
	DIR	*dh;
	char	*p, *buf;
	fifo	dlist = {0, 0};
#ifndef WIN32
        ino_t	lastInode = 0;
#endif                                 

	buf = malloc(MAXPATH);
	assert(buf);
	concat_path(buf, dir, "SCCS");
	unless (exists(buf)) {
		/*
		 * TODO: we need to check for the caes where
		 * the pwd is a SCCS dir
		 * This can fool the current code into wronly treating them
		 * as xtras.
		 */
		if ((dh = opendir(dir)) == NULL) {
			perror(dir);
			return;
		}
		while ((e = readdir(dh)) != NULL) { 
#ifndef WIN32
			/*
			 * Linux 2.3.x NFS bug, skip repeats.
			 */
			if (lastInode == e->d_ino) continue;
			lastInode = e->d_ino;
#endif
			if (streq(e->d_name, ".") || streq(e->d_name, "..")) {
				continue;
			}
			if (!opts.aflg && match_globs(e->d_name, ignore)) {
				debug((stderr, "SKIP\t%s\n", e->d_name));
				continue;
			}
			concat_path(buf, dir, e->d_name);
			unless (isdir(buf)) {
				do_print(" x ", buf, 0);
			} else {
				enqueue(&dlist, buf);
			}
		}
		closedir(dh);
		free(buf);
		while (p = dequeue(&dlist)) {
			walk(p, level + 1);
			free(p);
		}
	} else {
		free(buf);
		sccsdir(dir, level);
	}
}

private int
chk_diffs(sccs *s)
{
	if (!s) return (0);
	if (sccs_hasDiffs(s, 0) >= 1) return (1);
	return (0);  
}

private void
print_it(char state[4], char *file, char *rev)
{
	char *sfile = 0, *gfile;

	gfile =  strneq("./",  file, 2) ? &file[2] : file;

	/*
	 * HACK to hide stuff in the log directory
	 * this assumes the sfind is ran from project root
	 */
	if (strneq("BitKeeper/log/", gfile, 14)) return;

	if (opts.show_markers) printf("%s ", state);
	if (opts.gflg || (state[CSTATE] == 'x') || (state[CSTATE] == 'j'))  {
		fputs(gfile, stdout); 			 /* print gfile name */
	} else {
		fputs(sfile = name2sccs(gfile), stdout); /* print sfile name */
	}
	if (rev) printf("@%s", rev);
	fputs("\n", stdout);
	if (sfile) free(sfile);
}

private void
do_print(char state[4], char *file, char *rev)
{
	switch (state[LSTATE]) {
	    case 'l':	if (opts.lflg) goto print; break;
	    case 'u':	if (opts.uflg) goto print; break;
	}

	switch (state[CSTATE]) {
	    case 'c':	if (opts.cflg) goto print; break;
	    case 'n':	if (opts.nflg) goto print; break;
	    case 'j':	if (opts.jflg) goto print; break;
	    case 'x':	if (opts.xflg) goto print; break;
	}
	if ((state[PSTATE] == 'p') && opts.pflg) goto print;
	return;

print:	print_it(state, file, rev);
}

/*
 * Called for each directory that has an SCCS subdirectory
 */
private void
sccsdir(char *dir, int level)
{
	MDBM	*gDB = mdbm_open(NULL, 0, 0, GOOD_PSIZE);
	MDBM	*sDB = mdbm_open(NULL, 0, 0, GOOD_PSIZE);
	fifo	dlist = {0, 0};
	fifo	slist = {0, 0};
	struct dirent   *e;
	DIR	*dh; /* dir handle */
	int 	dir_len = strlen(dir);
	char	*p, *gfile, *buf;
	datum	k;
	kvpair  kv;
	struct 	stat sb;
	sccs	*s = 0;
	q_item	*i;
#ifndef WIN32
        ino_t	lastInode = 0;
#endif                                 

	buf = malloc(MAXPATH);
	assert(buf);

	/*
	 * Get all the gfiles
	 */
	dh = opendir(dir);
	while (e = readdir(dh)) {
#ifndef WIN32
		/*
		 * Linux 2.3.x NFS bug, skip repeats.
		 */
		if (lastInode == e->d_ino) continue;
		lastInode = e->d_ino;
#endif
		if (streq(e->d_name, ".") || streq(e->d_name, "..")) continue;
		if (patheq(e->d_name, "SCCS")) continue;

		/*
		 * Do not decent into another project root. e.g RESYNC
		 */
		if ((level > 0)  && patheq(e->d_name, "BitKeeper")) {
			fprintf(stderr, "Warning: sub-root %s ignored\n", dir);
			return;
		}

		if (!opts.aflg && match_globs(e->d_name, ignore)) {
			debug((stderr, "SKIP\t%s\n", e->d_name));
			continue;
		}

		/*
		 * Skip files with paths that are too long
		 * pad = "/SCCS/s." = 8
		 */
		if ((dir_len + 8 + strlen(e->d_name)) >= MAXPATH) {
			fprintf(stderr,
			    "Warning: %s/%s name too long, skiped\n",
							    dir, e->d_name);
			continue;
		}

		concat_path(buf, dir, e->d_name);
		if (isdir(buf)) {
			enqueue(&dlist, buf);
		} else {
			mdbm_store_str(gDB, e->d_name, "", MDBM_INSERT);
		}
	}
	closedir(dh);

	/*
	 * Get all the SCCS/?.files
	 * TODO compute sPath() for split root config
	 */
	concat_path(buf, dir, "SCCS");
	dh = opendir(buf);
	while (e = readdir(dh)) {
#ifndef WIN32
		/*
		 * Linux 2.3.x NFS bug, skip repeats.
		 */
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
			    "Warning: %s name too long, skiped\n", dir);
			continue;
		}

		if (strneq("s.", e->d_name, 2)) enqueue(&slist, e->d_name);
		mdbm_store_str(sDB, e->d_name, "", MDBM_INSERT);
	}
	closedir(dh);

	/*
	 * First eliminate as much as we can from SCCS dir;
	 * the leftovers in the gDB should be extras.
	 */
	while (p = dequeue(&slist)) {

		char	*file;
		char	state[4] = "???";

		s = 0;
		file = p;
		unless (strneq("s.", file, 2)) continue;
		gfile = &file[2];
		mdbm_delete_str(gDB, gfile);

		/*
		 * look for p.file,
		 */
		file[0] = 'p';
		if (mdbm_fetch_str(sDB, file)) {
			state[LSTATE] = 'l';
			file[0] = 's';
			concat_path(buf, dir, "SCCS");
			concat_path(buf, buf, file);
			if (opts.cflg &&
			    (s = init(buf, INIT_NOCKSUM, sDB)) &&
			    chk_diffs(s)) {
				state[CSTATE] = 'c';
			} else {
				state[CSTATE] = 'n';
			}
		} else {
			file[0] = 'z';
			if (mdbm_fetch_str(sDB, file)) {
				state[LSTATE] = 'l';
			} else {
				state[LSTATE] = 'u';
			}
			state[CSTATE] = 'n';
		}

		
		file[0] = 'x';
		mdbm_delete_str(sDB, file); /* remove x.file entry */
		
		/*
		 * check for pending deltas
		 * TODO: need to handle LOD
		 */
		concat_path(buf, dir, gfile);
		if (opts.pflg) {
			chk_pending(s, buf, state, sDB);
		} else {
			state[PSTATE] = ' ';
			do_print(state, buf, 0);
		}

		if (s) sccs_close(s);
		free(p);
	}

	/*
	 * Check the sDB for "junk" file
	 * XXX TODO: Do we consider the r.file and m.file "junk" file?
	 */
	if (opts.jflg) {
		concat_path(buf, dir, "SCCS");
		for (k = mdbm_firstkey(sDB); k.dsize != 0;
						    k = mdbm_nextkey(sDB)) {
			char buf1[MAXPATH];

			if (strneq("x.", k.dptr, 2)) continue;
			if (strneq("s.", k.dptr, 2)) continue;
			if (strneq("c.", k.dptr, 2)) continue;
			if (strneq("p.", k.dptr, 2)) continue;
			if (strneq("z.", k.dptr, 2)) continue;
			concat_path(buf1, buf, k.dptr);
			do_print(" j ", buf1, 0);
		}
	}

	/*
	 * Everything left in the gDB is extra
	 */
	if (opts.xflg) {
		for (kv = mdbm_first(gDB); kv.key.dsize != 0;
							kv = mdbm_next(gDB)) {
			concat_path(buf, dir, kv.key.dptr);
			do_print(" x ", buf, 0);
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
			fprintf(stderr,
			"Warning: %s should not be a directory\n", i->path);
		}
	}

	/*
 	 * Free up all remaining memory before we recurse down to
	 * lower level directries
	 */
	mdbm_close(sDB);
	free(buf);
	

	/*
	 * Process the directories
	 */
	while (p = dequeue(&dlist)) {
		walk(p, level + 1);
		free(p);
	}
}
