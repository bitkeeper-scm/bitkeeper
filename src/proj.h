#ifndef	_PROJ_H_
#define	_PROJ_H_

typedef struct	project project;

int		proj_cd2root(void);
int		proj_chdir(char *newdir);
char*		proj_cwd(void);
MDBM*		proj_config(project *p);
void		proj_free(project *p);
project*	proj_init(char *dir);
int		proj_leaseOK(project *p, int *newok);
char*		proj_license(project *p);
u32		proj_licensebits(project *p);
char*		proj_md5rootkey(project *p);
char*		proj_relpath(project *p, char *path);
void		proj_reset(project *p);
char*		proj_root(project *p);
char*		proj_rootkey(project *p);
project* 	proj_fakenew(void);

#define		chdir	proj_chdir

#endif
