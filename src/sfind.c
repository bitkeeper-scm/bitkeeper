#include "system.h"
#include "sccs.h"
WHATSTR("@(#)%K%");

#define	FTW_F		1			/* regular file */
#define	FTW_D		2			/* directory */
#define	FTW_NS		5			/* unstatable object */

/*
 * sfiles - find SCCS files
 *
 * TODO - make it find only files locked by a particular user
 */
char *sfiles_usage = "\n\
usage: sfiles [-acCdglpPrx] [directories]\n\n\
    -a		when used with -C, list all revs, not just the tip\n\
    -c		list only changed files (locked and modified)\n\
    -C		list leaves which are not in a changeset as file:1.3\n\
    -d		list directories under SCCS control (have SCCS subdir)\n\
    -D		list directories with no (or empty) SCCS subdirs\n\
    -g		list the gfile name, not the sfile name\n\
    -l		list only locked files\n\
    -p		(paranoid) opens each file to make sure it is an SCCS file\n\
		but only if the pathname to the file is not ../SCCS/s.*\n\
    -P		(paranoid) opens each file to make sure it is an SCCS file\n\
    -r		rebuild the id to pathname cache\n\
    -u		list only unlocked files\n\
    -x		list files which have no revision control files\n\
    		Note: revision control files must look like SCCS/s.*, not\n\
		foo/bar/blech/s.*\n\
\n\
    The -i and -r options can take an optional project root but not any other\n\
    directories.\n\
\n";

int	aFlg, cFlg, Cflg, dFlg, gFlg, lFlg, pFlg, Pflg, rFlg, vFlg, xFlg, uFlg;
int	Dflg;
int	error;
FILE	*pending_cache;
FILE	*id_cache;
MDBM	*idDB;
int	dups;
int	mixed;		/* running in mixed long/short mode */
int	file(char *f, int (*func)());
int	func(const char *filename, const struct stat *sb, int flag);
int	hasDiffs(char *file);
int	isSccs(char *s);
void	rebuild(void);
int	caches(const char *filename, const struct stat *sb, int flag);
char	*name(char *);
sccs	*cset;

int
main(int ac, char **av)
{
	int	c, i;
	char	*path;
	
	if (ac > 1 && streq("--help", av[1])) {
usage:		fprintf(stderr, "%s", sfiles_usage);
		exit(0);
	}
	while ((c = getopt(ac, av, "acCdDglpPruvx")) != -1) {
		switch (c) {
		    case 'a': aFlg++; break;
		    case 'c': cFlg++; break;
		    case 'C': Cflg++; rFlg++; break;
		    case 'd': dFlg++; break;
		    case 'D': Dflg++; break;
		    case 'g': gFlg++; break;
		    case 'l': lFlg++; break;
		    case 'p': pFlg++; break;
		    case 'P': Pflg++; break;
		    case 'r': rFlg++; break;
		    case 'v': vFlg++; break;
		    case 'u': uFlg++; break;
		    case 'x': xFlg++; break;
		    default: goto usage;
		}
	}
	if (xFlg && (aFlg|cFlg|Cflg|dFlg|Dflg|lFlg|pFlg|Pflg|uFlg)) {
		fprintf(stderr, "sfiles: -x must be standalone.\n");
		exit(1);
	}
	if (rFlg || Cflg) {
		if (av[optind]) {
			fprintf(stderr, "%s: -r must be stand alone.\n", av[0]);
			exit(1);
		}
		/* perror is in sccs_root, don't do it twice */
		unless (sccs_cd2root(0, 0) == 0) {
			purify_list();
			exit(1);
		}
		rebuild();
		purify_list();
		exit(dups ? 1 : 0);
	}
	if (!av[optind]) {
		path = xFlg ? "." : sPath(".", 1);
		lftw(path, func, 15);
	} else {
		for (i = optind; i < ac; ++i) {
			if (isdir(av[i])) {
				path =  xFlg ? av[i] : sPath(av[i], 1);
				lftw(path, func, 15);
			} else {
				path =  xFlg ? av[i] : sPath(av[i], 0);
				file(path, func);
			}
		}
	}
	purify_list();
	exit(0);
}

/*
 * Handle a single file.
 * Convert to s.file first, if possible.
 * XXX - this is incomplete.
 */
int
file(char *f, int (*func)())
{
	struct	stat sb;
	char	*sfile, *gfile;

	sfile = name2sccs(f);
	gfile = sccs2name(sfile);
	if (lstat(sfile, &sb) == -1) {
		if ((lstat(gfile, &sb) == 0) && xFlg) {
			printf("%s\n", f);
		}
	} else if (!xFlg) {
		func(sfile, &sb);
	}
	free(sfile);
	free(gfile);
	return (0);
}

int
func(const char *filename, const struct stat *sb, int flag)
{
	register char *file = (char *)filename;
	register char *s;
	char	*sfile, *gfile;

	if ((file[0] == '.') && (file[1] == '/')) file += 2;
	if (dFlg || Dflg) {
		if (S_ISDIR(sb->st_mode)) {
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
				    	return (0);;
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
		return (0);
	}
	if (S_ISDIR(sb->st_mode)) return (0);
	if (xFlg) {
		sfile = name2sccs(file);
		gfile = sccs2name(sfile);
		if (streq(gfile, file) && !exists(sfile)) {
			printf("%s\n", gfile);
		}
		free(sfile);
		free(gfile);
		return (0);
	}
	unless (is_sccs(file)) return 0;
	s = strrchr(file, '/');
	assert(s);
	s++;
	if (lFlg || cFlg || uFlg) {
		*s = 'p';
		if (exists(file)) {
			*s = 's';
			if (uFlg) return(0);
			if (lFlg || (cFlg && hasDiffs(file))) {
				printf("%s\n", name(file));
			}
		} else if (uFlg) {
			*s = 's';
			printf("%s\n", name(file));
		}
		*s = 's';
		return (0);
	}
	/* XXX - this should be first. */
	if (Pflg) {
		if (!isSccs(file)) return (0);
	} else if (pFlg) {
		if ((s-file < 5) || (s[-2] != 'S') || (s[-3] != 'C') ||
		    (s[-4] != 'C') || (s[-5] != 'S')) {
			if (!isSccs(file)) return (0);
		}
	}
	printf("%s\n", name(file));
	return (0);
}

char	*
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

int
hasDiffs(char *file)
{
	sccs	*s = sccs_init(file, INIT_NOCKSUM, 0);

	if (!s) return (0);
	if (sccs_hasDiffs(s, 0) >= 1) {
		sccs_free(s);
		return (1);
	}
	sccs_free(s);
	return (0);
}

int
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
 * rebuild the caches.
 */
void
rebuild()
{
	int	i;

	unless (cset = sccs_init(CHANGESET, 0, 0)) {
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

	/* Used to be unlink(IDCACHE) which I took away hoping not to
	 * to tickle Linux 2.[23].x NFS bugs.
	 */
	unless ((i = open(IDCACHE_LOCK, O_CREAT|O_EXCL, 0666)) > 0) {
		fprintf(stderr, "sfiles: can't lock id cache\n");
		exit(1);
	}
	close(i);	/* unlink it when we are done */
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
c:	lftw(".", caches, 15);
	if (id_cache) {
		fclose(id_cache);
		unlink(IDCACHE_LOCK);
	}
	sccs_free(cset);
	mdbm_close(idDB);
}

visit(delta *d)
{
	d->flags |= D_VISITED;
	if (d->parent) visit(d->parent);
}

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

int
caches(const char *filename, const struct stat *sb, int flag)
{
	char	*file = (char *)filename;
	sccs	*sc;
	delta	*d, *e;
	char	buf[MAXPATH*2];
	char	*t;
	int	n;

	if (S_ISDIR(sb->st_mode)) return (0);
	if ((file[0] == '.') && (file[1] == '/')) file += 2;
	unless (is_sccs(file)) return (0);
	unless (sc = sccs_init(file, INIT_NOCKSUM, 0)) return (0);
	unless (HAS_SFILE(sc) && sc->cksumok) {
		sccs_free(sc);
		return (0);
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

	unless (Cflg) {
		sccs_free(sc);
		return (0);
	}

	/* find the leaf of the current LOD and check it */
	sc->state |= S_RANGE2;
	unless (d = sccs_getrev(sc, 0, 0, 0)) {
		sccs_free(sc);
		return (0);
	}

	/*
	 * If it's marked, we're done.
	 */
	if (d->flags & D_CSET) {
		sccs_free(sc);
		return (0);
	}

	printf("%s:%s\n", sc->sfile, d->rev);
	while (aFlg && d->parent && !(d->parent->flags & D_CSET)) {
		d = d->parent;
		printf("%s:%s\n", sc->sfile, d->rev);
	}

	sccs_free(sc);
	return (0);
}

_ftw_get_flag(const char *dir, struct stat *sb)
{

	if (lstat(dir, sb) != 0) return FTW_NS;
	if ((S_ISDIR(sb->st_mode))) return FTW_D;

	/* everything else is considered a file */
	return FTW_F;
}


/*
 * Walk a directory tree recusivly, 
 * A tree pointed to by a sym-link is ignored
 * TODO: handle the "depth" parameter
 */
int
lftw(const char *dir,
	int(*func)(const char *file, struct stat *sb, int flag),
	int depth)
{
	DIR	*d;
	struct	dirent *e;
	struct	stat sbuf;
	char	tmp_buf[MAXPATH];
	char	*slash = "/";
	int	flag, rc = 0, first_time = 1;
	long	lastInode = 0;

	flag = _ftw_get_flag(dir, &sbuf);

	if ((rc = (*func)(dir, &sbuf, flag)) != 0) return rc;

	if (flag != FTW_D) return 0;

	/*
	 * if we get here, the top level node is a directory
	 * now we process its children
	 */
	/* CSTYLED */
	for (;;) {
		if (first_time) {
			int len = strlen(dir);
			first_time = 0;
			if ((len > 1) && (dir[len -1] == '/')) slash = "";
			if ((d = opendir(dir)) == NULL) {
				perror(dir);
				goto done;
			}
		}
		e = readdir(d);
		if (e == NULL) goto done;
		if ((strcmp(e->d_name, ".") == 0) ||
		    (strcmp(e->d_name, "..") == 0))
			continue;  /* skip "." && ".." */

		/*
		 * Linux 2.3.x NFS bug, skip repeats.
		 */
		if (lastInode == e->d_ino) continue;
		lastInode = e->d_ino;

		/* now we do the real work */
		sprintf(tmp_buf, "%s%s%s", dir, slash, e->d_name);
		flag = _ftw_get_flag(tmp_buf, &sbuf);
		if (S_ISDIR(sbuf.st_mode)) {
			/*
			 * Do not let sfind cross into other project roots.
			 */
			char	root[MAXPATH];
			
			sprintf(root, "%s/BitKeeper/etc", tmp_buf);
			unless (isdir(root)) {
				lftw(tmp_buf, func, 0);
			}
		} else {
			if ((rc = (*func)(tmp_buf, &sbuf, flag)) != 0) {
				goto done;
			}
		}
	}
done:
	closedir(d);
	return rc;
}
