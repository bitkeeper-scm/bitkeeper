#include "system.h"
#include "sccs.h"
WHATSTR("@(#)%K%");

/*
 * sfiles - find SCCS files
 *
 * TODO - make it find only files locked by a particular user
 */
private	char *sfiles_usage = "\n\
usage: sfiles [-aAcCdDglkpPRrux] [directories]\n\n\
    -a		examine all files, even if listed in BitKeeper/etc/ignore\n\
    -A		when used with -C, list all revs, not just the tip\n\
    -c		list only changed files (locked and modified)\n\
    -C		list leaves which are not in a changeset as file:1.3\n\
    -d		list directories under SCCS control (have SCCS subdir)\n\
    -D		list directories with no (or empty) SCCS subdirs\n\
    -g		list the gfile name, not the sfile name\n\
    -l		list only locked files (p.file and/or z.file)\n\
    -k		update the host wide keys database\n\
    -p		(paranoid) opens each file to make sure it is an SCCS file\n\
		but only if the pathname to the file is not ../SCCS/s.*\n\
    -P		(paranoid) opens each file to make sure it is an SCCS file\n\
    -r		rebuild the id to pathname cache\n\
    -R		when used with -C, list files as foo.c:1.3..1.5\n\
    -u		list only unlocked files\n\
    -x		list files which have no revision control files\n\
		Note 1: files in BitKeeper/log/ are ignored\n\
    		Note 2: revision control files must look like SCCS/s.*,\n\
		not foo/bar/blech/s.*\n\
\n\
    The -r option can take an optional project root but not any other\n\
    directories.\n\
\n";

/*
 * XXX - what should be done here is all the flags should be a bitmask
 * and then we can easily check for the combinations we accept.
 */
private	int	aFlg, cFlg, Cflg, dFlg, gFlg, lFlg, pFlg, Pflg, rFlg, vFlg;
private	int	Dflg, Aflg, Rflg, kFlg, xFlg, uFlg;
private	FILE	*id_cache;
private	void	keys(char *file);
private	MDBM	*idDB;
private	int	dups;
private	int	mixed;		/* running in mixed long/short mode */
private	int	hasDiffs(char *file);
private	int	isSccs(char *s);
private	void	rebuild(void);
private	char	*name(char *);
private	sccs	*cset;
typedef	void	(*lftw_func)(const char *path, int mode);
private	void	file(char *f, lftw_func func);
private	void	lftw(const char *dir, lftw_func func);
private	void	process(const char *filename, int mode);
private	void	caches(const char *filename, int mode);
private	project	*proj = 0;

int
sfiles_main(int ac, char **av)
{
	int	c, i;
	char	*path;

	debug_main(av);
	platformSpecificInit(NULL);
	if ((ac > 1) && streq("--help", av[1])) {
usage:		fprintf(stderr, "%s", sfiles_usage);
		return (0);
	}
	while ((c = getopt(ac, av, "aAcCdDgklpPrRuvx")) != -1) {
		switch (c) {
		    case 'a': aFlg++; break;
		    case 'A': Aflg++; break;
		    case 'c': cFlg++; break;
		    case 'C': Cflg++; break;
		    case 'd': dFlg++; break;
		    case 'D': Dflg++; break;
		    case 'g': gFlg++; break;
		    case 'k': kFlg++; break;
		    case 'l': lFlg++; break;
		    case 'p': pFlg++; break;
		    case 'P': Pflg++; break;
		    case 'r': rFlg++; break;
		    case 'R': Rflg++; break;
		    case 'v': vFlg++; break;
		    case 'u': uFlg++; break;
		    case 'x': xFlg++; break;
		    default: goto usage;
		}
	}
	if (xFlg && (Aflg|cFlg|Cflg|dFlg|Dflg|lFlg|pFlg|Pflg|uFlg)) {
		fprintf(stderr, "sfiles: -x must be standalone.\n");
		return (1);
	}
	if (rFlg || Cflg) {
		if (av[optind]) {
			fprintf(stderr, "%s: -%c must be stand alone.\n",
				av[0], rFlg ? 'r' : 'C');
			return (1);
		}
		/* perror is in sccs_root, don't do it twice */
		unless (sccs_cd2root(0, 0) == 0) {
			purify_list();
			return (1);
		}
		rebuild();
		if (proj) proj_free(proj);
		purify_list();
		return (dups ? 1 : 0);
	}
	if (!av[optind]) {
		path = xFlg ? "." : sPath(".", 1);
		lftw(path, process);
	} else if (streq("-", av[optind])) {
		char	buf[MAXPATH];

		while (fnext(buf, stdin)) {
			chop(buf);
			path = xFlg ? buf : sPath(buf, 1);
			file(path, process);
		}
	} else {
		/*
		 * XXX - why can't we used sfileFirst/next to expand these?
		 */
		for (i = optind; i < ac; ++i) {
			localName2bkName(av[i], av[i]);
			if (isdir(av[i])) {
				path =  xFlg ? av[i] : sPath(av[i], 1);
				lftw(path, process);
			} else {
				path =  xFlg ? av[i] : sPath(av[i], 0);
				file(path, process);
			}
		}
	}
	if (proj) proj_free(proj);
	purify_list();
	return (0);
}

private inline sccs *
init(char *name, int flags)
{
	sccs	*s = sccs_init(name, flags|INIT_SAVEPROJ, proj);

	if (s && !proj) proj = s->proj;
	return (s);
}

/*
 * Skip files which we don't want to consider as crud.
 */
private	inline void
xprint(char *f)
{
	if (!strneq("BitKeeper/log/", f, 14)) printf("%s\n", f);
}

private	void
xfile(char *file)
{
	char	*sfile = name2sccs(file);
	char	*gfile = sccs2name(sfile);

	if (streq(gfile, file) && !exists(sfile)) {
		xprint(file);
	}
	free(sfile);
	free(gfile);
}

/*
 * Handle a single file.
 */
private	void
file(char *f, lftw_func func)
{
	struct	stat sb;
	char	*g = 0;

	/*
	 * Process this name as both an s.file and a gfile,
	 * whichever exists.
	 */
	if (sccs_filetype(f) == 's') {
		g = sccs2name(f);
	} else {
		g = name2sccs(f);
	}

	if (lstat(f, &sb)) {
		if (errno != ENOENT) perror(f);
	} else {
		func(f, (sb.st_mode & S_IFMT));
	}
	unless (g) return;

	if (lstat(g, &sb)) {
		if (errno != ENOENT) perror(g);
	} else {
		func(g, (sb.st_mode & S_IFMT));
	}
	free(g);
}

private	void
process(const char *filename, int mode)
{
	register char *file = (char *)filename;
	register char *s;

	debug((stderr, "sfind process(%s)\n", filename));
	if ((file[0] == '.') && (file[1] == '/')) file += 2;
	if (dFlg || Dflg) {
		if (S_ISDIR(mode)) {
			char	buf[MAXPATH];

			sprintf(buf, "%s/SCCS", file);
			if (dFlg) {
				if (exists(buf) && !emptyDir(buf)) {
					printf("%s\n", file);
				}
			} else {
				/*
				 * If we are an SCCS directory, we don't count.
				 */
				if (streq(file, "SCCS") ||
				    ((s = strrchr(file, '/')) &&
				    streq(s, "/SCCS"))) {
				    	return;
				}

				/*
				 * If there is no SCCS directory,
				 * or if the SCCS dir is empty
				 */
				unless (exists(buf) && !emptyDir(buf)) {
					printf("%s\n", file);
				}
			}
		}
		return;
	}
	if (S_ISDIR(mode)) return;
	s = strrchr(file, '/');

	if ((s >= file + 4) &&
	    strneq(s - 4, "SCCS/", 5) && !sccs_filetype(file)) {
		if (xFlg) xprint(file);
	    	return;
	}

	if (xFlg) {
		xfile(file);
		return;
	}
	unless (sccs_filetype(file) == 's') {
		unless (pFlg || Pflg) return;
		unless (file[0] == 's' && file[1] == '.' && isSccs(file)) {
			return;
		}
	}
	debug((stderr, "sfind process2(%s)\n", filename));
	s = strrchr(file, '/');
	if (s) s++;
	else s = file;
	if (kFlg) {
		keys(file);
		return;
	}
	if (lFlg || cFlg || uFlg) {
		*s = 'p';
		/* Yes, I mean assignment, not equality in "*s = 'z'" */
		if (exists(file) || ((*s = 'z') && exists(file))) {
			*s = 's';
			if (uFlg) return;
			if (lFlg || (cFlg && hasDiffs(file))) {
				printf("%s\n", name(file));
			}
		} else if (uFlg) {
			*s = 's';
			printf("%s\n", name(file));
		}
		*s = 's';
		return;
	}
	/* XXX - this should be first. */
	if (Pflg) {
		if (!isSccs(file)) return;
	} else if (pFlg) {
		if ((s-file < 5) || (s[-2] != 'S') || (s[-3] != 'C') ||
		    (s[-4] != 'C') || (s[-5] != 'S')) {
			if (!isSccs(file)) return;
		}
	}
	printf("%s\n", name(file));
}

private	char	*
name(char *sfile)
{
	static	char	buf[MAXPATH];

	if (!gFlg) return (sfile);
	strcpy(buf, sfile);
	sfile = strrchr(buf, '/');
	assert(sfile);
	assert(sfile[1] == 's');
	assert(sfile[2] == '.');
	if ((sfile[-1] == 'S') && (sfile[-2] == 'C') && (sfile > buf + 3)) {
		strcpy(&sfile[-4], &sfile[3]);
	} else {
		strcpy(&sfile[1], &sfile[3]);
	}
	return (buf);
}

private	int
hasDiffs(char *file)
{
	sccs	*s = init(file, INIT_NOCKSUM);

	if (!s) return (0);
	if (sccs_hasDiffs(s, 0) >= 1) {
		sccs_free(s);
		return (1);
	}
	sccs_free(s);
	return (0);
}

private	int
isSccs(char *s)
{
	int	fd = open(s, 0, 0);
	char	buf[8];

	if ((fd == -1) || (read(fd, buf, 8) != 8)) return (0);
	close(fd);
	return ((buf[0] == '\001') && (buf[1] == 'h') &&
	    isdigit(buf[2]) && isdigit(buf[3]) && isdigit(buf[4]) &&
	    isdigit(buf[5]) && isdigit(buf[6]) && (buf[7] == '\n'));
}

/*
 * XXX - not lib safe.  Needs to have state reset.
 */
private	void
keys(char *file)
{
	sccs	*s;
	delta	*d;
	static	time_t	cutoff;
	static	char *host;

	unless (s = init(file, SILENT|INIT_NOCKSUM)) return;
	unless (s->table) {
		sccs_free(s);
		return;
	}
	unless (cutoff) cutoff = time(0) - uniq_drift();
	unless (host) host = sccs_gethost();
	unless (host) {
		fprintf(stderr, "sfiles: can not figure out host name\n");
		exit(1);
	}
	for (d = s->table; d; d = d->next) {
		if (d->date < cutoff) break;
		if (d->hostname && streq(d->hostname, host)) {
			u8	*p;
			u8	buf[MAXPATH+100];

			sccs_shortKey(s, sccs_ino(s), buf);
			printf("%s %lu\n", buf, d->date);
			break;
		}
	}
	sccs_free(s);
	return;
}

/*
 * rebuild the caches.
 */
private	void
rebuild()
{
	int	i;
	char	csetName[128] = CHANGESET;

	unless (cset = init(csetName, 0)) {
		perror("sfiles: can't init ChangeSet");
		exit(1);
	}
	unless (HAS_SFILE(cset) && cset->cksumok) {
		perror("sfiles: can't init ChangeSet");
		sccs_free(cset);
		exit(1);
	}
	mixed = !(cset->state & S_KEY2);

	if (Cflg) {
		if (csetIds(cset, 0)) {
			perror("sfiles: can't init ChangeSet DB");
			sccs_free(cset);
			exit(1);
		}
	}

	unless (rFlg) goto c;

	unless ((i = open(IDCACHE_LOCK, O_CREAT|O_EXCL, GROUP_MODE)) > 0) {
		fprintf(stderr, "sfiles: can't lock id cache\n");
		exit(1);
	}
	close(i);	/* unlink it when we are done */
	unlink(IDCACHE);
	unless (id_cache = fopen(IDCACHE, "w")) {
		perror(IDCACHE);
		exit(1);
	}
	fprintf(id_cache, "\
# This is a BitKeeper cache file.\n\
# If you suspect that the file is corrupted, simply remove it and \n\
# and it will be rebuilt as needed.  \n\
# The format of the file is <ID> <PATHNAME>\n\
# The file is used for performance during makepatch/takepatch commands.\n");
	idDB = mdbm_open(NULL, 0, 0, GOOD_PSIZE);
	assert(idDB);
c:	lftw(".", caches);
	if (id_cache) {
		fclose(id_cache);
		unlink(IDCACHE_LOCK);
		chmod(IDCACHE, 0666);
	}
	sccs_free(cset);
	mdbm_close(idDB);
}

private	void
visit(delta *d)
{
	d->flags |= D_VISITED;
	if (d->parent) visit(d->parent);
}

private	void
save(sccs *sc, MDBM *idDB, char *buf)
{
	if (mdbm_store_str(idDB, buf, sc->gfile, MDBM_INSERT)) {
		if (errno == EEXIST) {
			fprintf(stderr,
			    "Duplicate key '%s' for %s\n  Used by %s\n",
			    buf, sc->gfile, mdbm_fetch_str(idDB, buf));
			dups++;
		} else {
			perror("mdbm_store");
			exit(1);
		}
	}
}

private	void
pr(sccs *sc, delta *d)
{
	if (!d || !d->parent) return;
	unless (d->parent->flags & D_CSET) pr(sc, d->parent);
	printf("%s%c%s..%s\n",
	    gFlg ? sc->gfile : sc->sfile, BK_FS, d->parent->rev, d->rev);
}

private	void
caches(const char *filename, int mode)
{
	char	*file = (char *)filename;
	sccs	*sc;
	delta	*d;
	char	buf[MAXPATH*2];
	char	*t;

	if (S_ISDIR(mode)) return;
	if ((file[0] == '.') && (file[1] == '/')) file += 2;
	unless (sccs_filetype(file) == 's') return;
	unless (sc = init(file, INIT_NOCKSUM)) return;
	unless (HAS_SFILE(sc) && sc->cksumok) {
		sccs_free(sc);
		return;
	}
	if (sc->defbranch && streq(sc->defbranch, "1.0")) { /* not visible */
		sccs_free(sc);
		return;
	}
	if (vFlg) printf("%s\n", sc->gfile);

	if (rFlg) {
		delta	*ino = sccs_ino(sc);

		/* update the id cache only if root path != current path. */
		assert(ino->pathname);
		sccs_sdelta(sc, ino, buf);
		unless (streq(ino->pathname, sc->gfile)) {
			fprintf(id_cache, "%s %s\n", buf, sc->gfile);
		}
		save(sc, idDB, buf);
		if (mixed && (t = sccs_iskeylong(buf))) {
			*t = 0;
			unless (streq(ino->pathname, sc->gfile)) {
				fprintf(id_cache, "%s %s\n", buf, sc->gfile);
			}
			save(sc, idDB, buf);
			*t = '|';
		}
	}

	/* XXX - should this be (Cflg && !(sc->state & S_CSET)) ? */
	unless (Cflg) goto out;

	/* find the leaf of the current LOD and check it */
	unless (d = sccs_getrev(sc, "+", 0, 0)) goto out;

	/*
	 * If it's marked, we're done.
	 */
	if (d->flags & D_CSET) goto out;

	/*
	 * If we are looking for diff output and not -a style,
	 * go find the previous cset.
	 */
	if (Rflg && !Aflg) {
		delta	*p;

		printf("%s%c", gFlg ? sc->gfile : sc->sfile, BK_FS);
		for (p = d->parent; p && !(p->flags & D_CSET); p = p->parent);
		printf("%s..%s\n", p ? p->rev : sc->tree->rev, d->rev);
		goto out;
	}
	if (Rflg) {
		pr(sc, d);
		goto out;
	}

	do {
		printf("%s%c%s\n", gFlg ? sc->gfile : sc->sfile, BK_FS, d->rev);
		d = d->parent;
	} while (Aflg && d && !(d->flags & D_CSET));

out:
	sccs_free(sc);
}

/*
 * Walk a directory tree recursively.  Does not follow symlinks.  This
 * is a subroutine used exclusively by lftw() (below).  It processes
 * the current directory, and the callback function must not change
 * directories.
 *
 * path points to the buffer in which the pathname is constructed.  It
 * is shared among all recursive instances.  base points one past the
 * last slash in the pathname.  sb points to a stat structure; ignore
 * is the list of globs which match filenames to ignore; func is the
 * function to call with the file name; depth is whether to process a
 * directory name before or after its children.
 */
#define GFILE(s) ((isalpha((s)[0]) && (s)[1] == '.') ? (s)+2 : (s))

private	void
lftw_inner(char *path, char *base, struct stat *sb,
	   globv ignore, lftw_func func)
{
	DIR		*d;
	struct dirent	*e;
	int		mode, n;
#ifndef WIN32
	ino_t		lastInode = 0;
#endif

	if ((d = opendir(path)) == NULL) {
		perror(path);
		return;
	}
	if (base[-1] != '/') *base++ = '/';
	while ((e = readdir(d)) != NULL) {
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
		if (match_globs(GFILE(e->d_name), ignore)) {
			debug((stderr, "SKIP\t%s\n", e->d_name));
			continue;
		}
		if (base - path + strlen(e->d_name) + 2 > MAXPATH) {
			fprintf(stderr, "lftw: path too long\n[%s%s]\n",
				path, e->d_name);
			continue;
		}
		strcpy(base, e->d_name);

#ifdef DT_UNKNOWN
		if (e->d_type != DT_UNKNOWN) mode = DTTOIF(e->d_type);
		else
#endif
		if (lstat(path, sb)) {
			/*
			 * Do not print an error, A file entry may be deleted
			 * right by someone else after we read the directory block. 
			 * or the directory cache is slightly out of date.
			 * It is not an error.
			 * This seems to happen more often on multi-process system. 
			 */
			/* perror(path); */
			continue;
		} else mode = (sb->st_mode & S_IFMT);

		debug((stderr, "FUNC\t%s\n", e->d_name));
		func(path, mode);

		if (!S_ISDIR(mode)) continue;

		/* Do not cross into other project roots (e.g. RESYNC).  */
		n = strlen(base);
		strcat(base, "/" BKROOT);
		if (exists(path)) continue;

		/* Descend directory.  */
		base[n] = '\0';
		debug((stderr, "DIR\t%s\n", e->d_name));
		lftw_inner(path, base + n, sb, ignore, func);
	}
	closedir(d);
}

private	void
lftw(const char *dir, lftw_func func)
{
	FILE		*ignoref;
	globv		ignore = NULL;
	char		*root = 0;
	char		path[MAXPATH];
	struct stat	st;

	unless (aFlg || (root = sccs_root(0)) == NULL) {
		sprintf(path, "%s/BitKeeper/etc/ignore", root);
		if ((ignoref = fopen(path, "r")) != NULL) {
			ignore = read_globs(ignoref, 0);
			fclose(ignoref);
		}
		free(root);
		root = 0;
	}
if (root != 0) {
	fprintf(stderr, "aflg=%d\n", aFlg);
}

	strcpy(path, dir);
	/*
	 * Special case: process the starting path directly,
	 * even if it's dot.
	 */
	if (lstat(path, &st)) {
		perror(path);
		return;
	} else if (! S_ISDIR(st.st_mode)) {
		errno = ENOTDIR;
		perror(path);
		return;
	} else {
		func(path, (st.st_mode & S_IFMT));
	}
	lftw_inner(path, path+strlen(path), &st, ignore, func);

	if (ignore) free_globs(ignore);
}
