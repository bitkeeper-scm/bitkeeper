#ifndef	_PROJ_H_
#define	_PROJ_H_

typedef struct project project;

/*
 * This file contains a series of accessor functions for the project
 * struct that caches information about the current repository.  There
 * is a global project struct that always matches the current
 * directory.  This struct is private to proj.c and it used whenever 0
 * is passed to these functions.
 */

/*
 * Return a project struct for the project that contains the given
 * directory.
 */
project	*proj_init(char *dir);
void	proj_free(project *p);

/*
 * Return the full path to the root directory in the current project.
 * Data is calculated the first time this function in called. And the
 * old data is returned from then on.
 */
char	*proj_root(project *p);

/*
 * chdir to the root of the current tree we are in
 */
int	proj_cd2root(void);

/*
 * When given a pathname to a file, this function returns the pathname
 * to the file relative to the current project.  If the file is not under
 * the current project then, NULL is returned.
 * The returned path is allocated with malloc() and the user must free().
 */
char	*proj_relpath(project *p, char *path);

/*
 * Return a populated MDBM for the config file in the current project.
 */
MDBM	*proj_config(project *p);

/*
 * Return the root key of the ChangeSet file in the current project.
 */
char	*proj_csetrootkey(project *p);
char	*proj_csetmd5rootkey(project *p);

/*
 * Return a hash where we can stash random data that gets cached per repo.
 */
HASH	*proj_hash(project *p);

/*
 * proj_chdir() is a wrapper for chdir() that also updates
 * the current default project.
 * We map chdir() to this function by default.
 */
int	proj_chdir(char *newdir);
#define	chdir proj_chdir

/*
 * Clear any data cached for the current project root.
 * Call this function whenever the current data is made invalid.
 * When passed an explicit project then only that project is cleared.
 * Otherwise, all projects are flushed.
 */
void	proj_reset(project *p);


project *proj_fakenew(void);

#endif
