#include "system.h"
#include "sccs.h"
WHATSTR("@(#)%K%");

/*
 * files - find regular files and symbolic links
 */
private	char *files_usage = "\n\
usage: files [directories]\n\
\n";
private	void	walk(char *path);
private	void	files(char *p, char *pwd);

int
files_main(int ac, char **av)
{
	int	i;
	char	pwd[MAXPATH];

	debug_main(av);
	platformSpecificInit(NULL);
	if ((ac > 1) && streq("--help", av[1])) {
		fprintf(stderr, "%s", files_usage);
		exit(0);
	}
	unless (getcwd(pwd, sizeof(pwd))) {
		perror("getcwd");
		exit(1);
	}
	unless (av[1]) {
		files(0, pwd);
	} else {
		for (i = 1; i < ac; ++i) {
			files(av[i], pwd);
			chdir(pwd);
		}
	}
	purify_list();
	exit(0);
}

/*
 * Get the full pathname into the buffer and call the routine.
 */
private void
files(char *p, char *pwd)
{
	char	path[MAXPATH];

	if (!p || streq(p, ".")) {
		path[0] = 0;
		walk(path);
	} else {
		strcpy(path, p);
		walk(path);
	}
}

/*
 * Walk a directory tree recursively.  Does not follow symlinks.  
 *
 * path points to the buffer in which the pathname is constructed.  It
 * is shared among all recursive instances.  
 */
private	void
walk(char *path)
{
	char		*end;		/* points at the null */
	int		len;		/* length of the path before we added */
	DIR		*d;
	struct dirent	*e;
	struct stat	sb;
#ifndef WIN32
	ino_t		lastInode = 0;
#endif

	if ((d = opendir(".")) == NULL) {
		perror(path);
		return;
	}
	if (path[0]) {
		end = path + (len = strlen(path));
	} else {
		end = 0;
		len = 0;
	}
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

		if ((strlen(e->d_name) + len + 5) > MAXPATH) {
			fprintf(stderr, "files: path too long\n[%s/%s]\n",
				path, e->d_name);
			continue;
		}
		if (end) {
			*end = '/';
			strcpy(end + 1, e->d_name);
		} else {
			strcpy(path, e->d_name);
		}
		if (lstat(e->d_name, &sb)) {
			/* Just ignore it, someone deleted it */
			continue;
		} else if (S_ISDIR(sb.st_mode)) {
			chdir(e->d_name);
			walk(path);
			chdir("..");
		} else if (S_ISREG(sb.st_mode) || S_ISLNK(sb.st_mode)) {
			printf("%s\n", path);
		}
	}
	closedir(d);
}
