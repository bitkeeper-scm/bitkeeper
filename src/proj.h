/*
 * Copyright 2003,2005-2016 BitMover, Inc
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

#ifndef	_PROJ_H_
#define	_PROJ_H_

#include "progress.h"

typedef struct	project project;


int		proj_cd2root(void);
int		proj_cd2product(void);
int		proj_chdir(char *newdir);
MDBM*		proj_config(project *p);

/*
 * defines for proj_checkout() 
 * bitfields in case we add CO_TIMESTAMPS
 */
#define	CO_NONE		0x01	/* don't do any checkout */
#define	CO_GET		0x02	/* do a get */
#define	CO_EDIT		0x04	/* do an edit */
#define	CO_LAST		0x08	/* preserve previous checkout state */
#define	CO_BAM_NONE	0x10	/* don't do any checkout */
#define	CO_BAM_GET	0x20	/* do a get */
#define	CO_BAM_EDIT	0x40	/* do an edit */
#define	CO_BAM_LAST	0x80	/* preserve previous checkout state */

#define	CO(s)	(BAM(s) ? \
			(proj_checkout(s->proj) >> 4) : \
			(proj_checkout(s->proj) & 0xf))
#define	proj_isEnsemble(p) \
		(proj_isProduct(p) || proj_isComponent(p))

int		proj_checkout(project *p);
char*		proj_csetFile(project *p);
char*		proj_cwd(void);
void		proj_free(project *p);
char*		proj_fullpath(project *p, char *path);
int		proj_isCaseFoldingFS(project *p);
int		proj_isComponent(project *p);
int		proj_isProduct(project *p);
project*	proj_product(project *p);
project*	proj_findProduct(project *p);
project*	proj_isResync(project *p);
char*		proj_md5rootkey(project *p);
char*		proj_syncroot(project *p);
char*		proj_repoID(project *p);
char*		proj_relpath(project *p, char *path);
void		proj_reset(project *p);
void		proj_flush(project *p);
char*		proj_root(project *p);
char*		proj_rootkey(project *p);
int		proj_samerepo(char *source, char *dest, int quiet);
project*	proj_init(char *dir);
project*	proj_fakenew(void);
void		proj_saveCO(sccs *s);
void		proj_saveCOkey(project *p, char *key, int co);
int		proj_restoreCO(sccs *s);
int		proj_restoreAllCO(project *p, MDBM *idDB, ticker *tick,
		    int dtime);
MDBM*		proj_BAMindex(project *p, int write);
int		proj_sync(project *p);
char*		proj_comppath(project *p);
int		proj_idxsock(project *p);
int		proj_hasOldSCCS(project *p);
int		proj_remapDefault(int doremap);
int		proj_hasDeltaTriggers(project *p);
char*		proj_cset2key(project *p, char *csetrev, char *rootkey);
char*		proj_tipkey(project *p);
char*		proj_tipmd5key(project *p);
char*		proj_tiprev(project *p);

#define	DS_PENDING	1
#define	DS_EDITED	2
void		proj_dirstate(project *p, char *dir, u32 state, int set);
char		**proj_scanDirs(project *p, u32 state);
char		**proj_scanComps(project *p, u32 state);

#define		chdir	proj_chdir

/* the features.c subset of project* */
typedef	struct {
	u32	bits;	/* bitfield of current features in repo */
	u32	new:1;	/* force write of features */
} p_feat;

p_feat*		proj_features(project *p);

#endif
