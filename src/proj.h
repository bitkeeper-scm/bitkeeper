#ifndef	_PROJ_H_
#define	_PROJ_H_

typedef struct	project project;

char*		proj_bkl(project *p);
u32		proj_bklbits(project *p);
int		proj_cd2root(void);
int		proj_chdir(char *newdir);
MDBM*		proj_config(project *p);
char*		proj_configval(project *p, char *key);
int		proj_configbool(project *p, char *key);

/*
 * defines for proj_checkout() 
 * bitfields in case we add CO_TIMESTAMPS
 */
#define	CO_NONE	0	/* don't do any checkout */
#define	CO_GET	1	/* do a get */
#define	CO_EDIT	2	/* do an edit */
#define	CO_LAST	4	/* preserve previous checkout state */

int		proj_checkout(project *p);
char*		proj_cwd(void);
void		proj_free(project *p);
char*		proj_fullpath(project *p, char *path);
int		proj_isCaseFoldingFS(project *p);
int		proj_isResync(project *p);
int		proj_leaseChecked(project *p, int write);
char*		proj_md5rootkey(project *p);
char*		proj_relpath(project *p, char *path);
void		proj_reset(project *p);
char*		proj_root(project *p);
char*		proj_rootkey(project *p);
int		proj_samerepo(char *source, char *dest);
project*	proj_init(char *dir);
project* 	proj_fakenew(void);

#define		chdir	proj_chdir

#endif
