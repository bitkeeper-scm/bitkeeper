#include "sccs.h"
#ifdef WIN32
#include "uwtlib/ftw.h"
#else
#include <ftw.h>
#endif
WHATSTR("%W% %@%");

/*
 * sfiles - find SCCS files
 *
 * TODO - make it find only files locked by a particular user
 */
char *sfiles_usage = "\n\
usage: sfiles [-aclpP] [-i[root]] [-r[root]] [directories]\n\n\
    -a		when used with -i, list all the ids of all the leaves\n\
    -c		list only changed files (locked and modified)\n\
    -g		list the gfile name, not the sfile name\n\
    -i[root]	list the files as their identifiers\n\
    -l		list only locked files\n\
    -p		(paranoid) opens each file to make sure it is an SCCS file\n\
		but only if the pathname to the file is not ../SCCS/s.*\n\
    -P		(paranoid) opens each file to make sure it is an SCCS file\n\
    -r[root]	rebuild the id to pathname cache, and pending changeset cache\n\
    -x		list files which have no revision control files\n\
    		Note: revision control files must look like SCCS/s.*, not\n\
		foo/bar/blech/s.*\n\
\n\
    The -i and -r options can take an optional project root but not any other\n\
    directories.\n\
\n";

int	aFlg, cFlg, gFlg, iFlg, lFlg, pFlg, PFlg, rFlg, vFlg, xFlg;
int	error;
FILE	*pending_cache;
FILE	*id_cache;
MDBM	*idDB;
int	dups;
int	file(char *f, int (*func)(), int depth);
int	func(const char *filename, const struct stat *sb, int flag);
int	hasDiffs(char *file);
int	isSccs(char *s);
void	rebuild(void);
int	caches(const char *filename, const struct stat *sb, int flag);
int	ids(const char *filename, const struct stat *sb, int flag);
char	*name(char *);

int
main(int ac, char **av)
{
	int	c, i;
	char	*root = 0;
	
	if (ac > 1 && streq("--help", av[1])) {
usage:		fprintf(stderr, "%s", sfiles_usage);
		exit(0);
	}
	while ((c = getopt(ac, av, "acgi|lpPr|vx")) != -1) {
		switch (c) {
		    case 'a': aFlg++; break;
		    case 'c': cFlg++; break;
		    case 'g': gFlg++; break;
		    case 'i': iFlg++; root = optarg; break;
		    case 'l': lFlg++; break;
		    case 'p': pFlg++; break;
		    case 'P': PFlg++; break;
		    case 'r': rFlg++; root = optarg; break;
		    case 'v': vFlg++; break;
		    case 'x': xFlg++; break;
		    default: goto usage;
		}
	}
	if (rFlg) {
		if (av[optind]) {
			fprintf(stderr, "%s: -r must be stand alone.\n", av[0]);
			exit(1);
		}
		/* perror is in sccs_root, don't do it twice */
		unless (sccs_root(0) == 0) exit(1);
		rebuild();
		exit(dups ? 1 : 0);
	}
	if (iFlg) {
		if (av[optind]) {
			fprintf(stderr, "%s: -i must be stand alone.\n", av[0]);
			exit(1);
		}
		/* perror is in sccs_root, don't do it twice */
		unless (sccs_root(root) == 0) exit(1);
		ftw(".", ids, 15);
		exit(0);
	}
	if (!av[optind]) {
		ftw(".", func, 15);
	} else {
		for (i = optind; i < ac; ++i) {
			if (isdir(av[i])) {
				ftw(av[i], func, 15);
			} else {
				file(av[i], func, 15);
			}
		}
	}
	exit(0);
}

/*
 * Handle a single file or a diretory.
 */
int
file(char *f, int (*func)(), int depth)
{
	sccs	*s;
	struct	stat sb;
	char	*sfile, *gfile;

	sfile = name2sccs(f);
	gfile = sccs2name(sfile);
	if (stat(sfile, &sb) == -1) {
		if ((stat(gfile, &sb) == 0) && xFlg) {
			printf("%s\n", f);
		}
		goto out;
	} else if (!xFlg) {
		func(sfile, &sb, 0);
	}
out:	free(sfile);
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
	if (xFlg) {
		if (S_ISDIR(sb->st_mode)) return (0);
		sfile = name2sccs(file);
		gfile = sccs2name(sfile);
		if (exists(gfile) && !exists(sfile)) printf("%s\n", gfile);
		free(sfile);
		free(gfile);
		return (0);
	}
	for (s = file; *s; s++);
	for ( ; s > file; s--) if (s[-1] == '/') break;		/* CSTYLED */
	if (!s || (s[0] != 's') || (s[1] != '.')) return (0);
	if (lFlg || cFlg) {
		*s = 'p';
		if (exists(file)) {
			*s = 's';
			if ((cFlg && hasDiffs(file)) || lFlg) {
				printf("%s\n", name(file));
			}
		}
		*s = 's';
		return (0);
	}
	if (PFlg) {
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
	static	char	buf[1024];

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
	sccs	*s = sccs_init(file, 0);

	if (!s) return (0);
	if (sccs_hasDiffs(s, 0) == 1) {
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
 * Print out ids
 */
int
ids(const char *filename, const struct stat *sb, int flag)
{
	register char *file = (char *)filename;
	register char *s;
	sccs	*sc;

	if ((file[0] == '.') && (file[1] == '/')) file += 2;
	for (s = file; *s; s++);
	for ( ; s > file; s--) if (s[-1] == '/') break;		/* CSTYLED */
	if ((s[0] != 's') || (s[1] != '.')) return (0);
	unless (sc = sccs_init(file, 0)) return (0);
	unless (HAS_SFILE(sc) && sc->cksumok) {
		sccs_free(sc);
		return (0);
	}
	sccs_ids(sc, aFlg ? 0 : TOP, stdout);
	sccs_free(sc);
	return (0);
}

/*
 * rebuild the caches.
 */
void
rebuild()
{
	int	i;
	unless ((i = open("SCCS/z.pending_cache", O_CREAT|O_EXCL, 0600)) > 0) {
		fprintf(stderr, "sfiles: can't lock pending cache\n");
		exit(1);
	}
	close(i);	/* unlink it when we are done */
	unless (pending_cache = fopen("SCCS/x.pending_cache", "w")) {
		perror("SCCS/x.pending_cache");
		exit(1);
	}
	unless ((i = open("SCCS/z.id_cache", O_CREAT|O_EXCL, 0600)) > 0) {
		fprintf(stderr, "sfiles: can't lock id cache\n");
		exit(1);
	}
	close(i);	/* unlink it when we are done */
	unless (id_cache = fopen("SCCS/x.id_cache", "w")) {
		perror("SCCS/x.id_cache");
		exit(1);
	}
	fprintf(id_cache, "\
# This is a BitKeeper cache file.\n\
# If you suspect that the file is corrupted, simply remove it and \n\
# and it will be rebuilt as needed.  \n\
# The format of the file is <ID> <PATHNAME>\n\
# The file is used for performance during mkpatch/tkpatch commands.\n");
	fprintf(pending_cache, "\
# This is a BitKeeper cache file.\n\
# If you suspect that the file is corrupted, or that there are files which\n\
# need to be here and aren't, rebuild the cache by running\n\
# `sfiles -r' while in a directory at or below this file.\n\
# The list of entries in this file represent files which have deltas which\n\
# are not yet part of a changeset.\n");
	idDB = mdbm_open(NULL, 0, 0, 4096);
	assert(idDB);
	mdbm_pre_split(idDB, 1<<10);
	ftw(".", caches, 15);
	fclose(pending_cache);
	fclose(id_cache);
	unlink("SCCS/z.id_cache");
	unlink("SCCS/z.pending_cache");
}

inline int
isleaf(register delta *d)
{
	if (d->type != 'D') return (0);
	for (d = d->kid; d; d = d->siblings) {
		if (d->type == 'D') return (0);
	}
	return (1);
}

int
caches(const char *filename, const struct stat *sb, int flag)
{
	register char *file = (char *)filename;
	register char *s;
	sccs	*sc;
	register delta *d;
	char	*path;
	datum	k, v;
	char	buf[1024];

	if ((file[0] == '.') && (file[1] == '/')) file += 2;
	for (s = file; *s; s++);
	for ( ; s > file; s--) if (s[-1] == '/') break;		/* CSTYLED */
	if ((s[0] != 's') || (s[1] != '.')) return (0);
	unless (sc = sccs_init(file, SHUTUP)) return (0);
	unless (HAS_SFILE(sc) && sc->cksumok) {
		sccs_free(sc);
		return (0);
	}
	/* If the file has any leaves needing changesets, list it */
	for (d = sc->table; d; d = d->next) {
		unless (isleaf(d)) continue;
		if (d->cset) continue;
		sccs_pdelta(sccs_ino(sc), pending_cache);
		fprintf(pending_cache, "\n");
		break;
	}
	/* update the id cache */
	sccs_sdelta(buf, sc->tree);
	if (sc->tree->pathname && streq(sc->tree->pathname, sc->gfile)) {
		path = sc->tree->pathname;
	} else {
		path = sc->gfile;
	}
	fprintf(id_cache, "%s %s\n", buf, path);
	k.dptr = buf;
	k.dsize = strlen(buf) + 1;
	v.dptr = path;
	v.dsize = strlen(path) + 1;
	if (mdbm_store(idDB, k, v, MDBM_INSERT)) {
		v = mdbm_fetch(idDB, k);
		fprintf(stderr,
		    "Duplicate id '%s' for %s\n  Used by %s\n",
		    buf, path, v.dptr);
			dups++;
	}
	if (vFlg) printf("%s\n", sc->gfile);
	sccs_free(sc);
	return (0);
}
