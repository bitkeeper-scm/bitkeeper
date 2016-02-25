/*
 * Copyright 2000-2016 BitMover, Inc
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

#include "system.h"
#include "sccs.h"

#define	stdlog	opts->log ? opts->log : stderr
#define	CLEAN_RESYNC	0x01	/* blow away the RESYNC dir */
#define	CLEAN_PENDING	0x02	/* blow away the PENDING dir */
#define	CLEAN_OK	0x04	/* quietly exit 0 */
#define	CLEAN_MVRESYNC	0x08	/* mv RESYNC RESYNC-YYYY-MM-DD-%03d */
#define	CLEAN_NOSHOUT	0x10	/* No shouting */
#define	CLEAN_ABORT	0x20	/* run bk abort -qf */
#define	SHOUT() \
	fputs("===================== ERROR ========================\n", stderr);
#define	SHOUT2() \
	fputs("====================================================\n", stderr);
#define	SFILE_CONFLICT		1
#define	GFILE_CONFLICT		2
#define	DIR_CONFLICT		3
#define	RESYNC_CONFLICT		4
#define	GONE_SFILE_CONFLICT	5
#define	COMP_CONFLICT		6
#define	LOCAL			1
#define	REMOTE			2
#define	BACKUP_LIST		"BitKeeper/tmp/resolve_backup_list"
#define	BACKUP_SFIO		"BitKeeper/tmp/resolve_backup_sfio"
#define	PASS4_TODO		"BitKeeper/tmp/resolve_sfiles"
#define	RESOLVE_LOCK		(ROOT2RESYNC "/BitKeeper/tmp/resolve_lock")
#define	AUTO_MERGE		"Auto merged"
#define	SCCS_MERGE		"SCCS merged"

/* passed around everywhere to record state */
typedef struct {
	u32	debug:1;	/* debugging messages */
	u32	pass1:1;	/* move to RENAMES */
	u32	pass2:1;	/* move back to RESYNC */
	u32	pass3:1;	/* resolve perm/flags/content conflicts */
	u32	pass4:1;	/* move from RESYNC to repository */
	u32	automerge:1;	/* automerge everything */
	u32	edited:1;	/* set if edited files found */
	u32	resolveNames:1;	/* resolve (or don't resolve) conflicts */
	u32	noconflicts:1;	/* do not handle conflicts */
	u32	errors:1;	/* set if there are errors, don't apply */
	u32	quiet:1;	/* no output except for errors or progress */
	u32	verbose:1;	/* all output, no progress */
	u32	progress:1;	/* output progress bars */
	u32	textOnly:1;	/* no GUI tools allowed */
	u32	remerge:1;	/* redo already merged content conflicts */
	u32	force:1;	/* for forcing commits with unmerged changes */
	u32	advance:1;	/* advance after merging if commit works */
	u32	from_pullpush:1;/* set if we are being called from pull/push */
	u32	partial:1;	/* partial resolve - don't commit changeset */
	u32	autoOnly:1;	/* do as much as possible automatically &exit */
	u32	batch:1;	/* command line version of autoOnly */
	u32	standalone:1;	/* operate in this component only */
	u32	nested:1;	/* is this a nested resolve? */
	u32	moveup:1;	/* move component up */
	u32	fullCheck:1;	/* did pass4 pass a full check? */
	int	hadConflicts;	/* conflicts during automerge */
	int	pass;		/* which pass are we in now */
	char	*comment;	/* checkin comment for commit */
	char	*mergeprog;	/* program to merge with */
	int	renames;	/* count of renames processed in pass 1 */
	int	renames2;	/* count of renames processed in pass 2 */
	int	renamed;	/* count of renames manually resolved */
	int	resolved;	/* count of files resolved in pass 3 */
	int	comps;		/* count of comps resolved in comp_resolve */
	int	applied;	/* count of files processed in pass 4 */
	MDBM	*rootDB;	/* db{ROOTKEY} = pathname in RESYNC */
	MDBM	*idDB;		/* for the local repository, not RESYNC */
	MDBM	*checkoutDB;	/* Save the original checkout state files */
	MDBM	*goneDB;	/* resolved gone files */
	FILE	*log;		/* if set, log to here */
	char	**includes;	/* list of globs indicating files to resolve */
	char	**excludes;	/* list of globs indicating files to skip */
	char	**notmerged;	/* list of files that didn't automerge */
	char	**nav;		/* argv for component resolves */
	char	**aliases;	/* aliases for nested resolve*/
	char	**complist;	/* sorted list of union of components */
} opts;

/*
 * Used both for r.files and m.files.
 * See res_getnames() and freenames().
 */
typedef struct {
	char	*local;
	char	*gca;
	char	*remote;
} names;

/*
 * Used for differences other than contents or renames, i.e., perms.
 */
typedef struct {
	ser_t	local;
	ser_t	gca;
	ser_t	remote;
} deltas;

typedef struct resolve resolve;

typedef	int (*rfunc)(resolve *rs);

/* there has to be one with "?" as the index */
typedef	struct {
	char	*spec;		/* string which goes with this func */
	char	*name;		/* name of function */
	char	*help;		/* help string for this func */
	rfunc	func;		/* function to call */
} rfuncs;

/*
 * State about the resolve; instantiated and freed for each pass.
 */
struct resolve {
	opts	*opts;		/* so we don't have to pass both */
	sccs	*s;		/* the sccs file we are resolving */
	char	*key;		/* root key of this sfile */
	ser_t	d;		/* Top of LOD of the sccs file */
	char	*dname;		/* name2sccs(PATHNAME(s, d)) */
	names	*revs;		/* revisions being resolved */
	names	*rnames;	/* tmpfile names for revisions being resolved */
	names	*gnames;	/* gfile names being resolved */
	names	*snames;	/* sfile names being resolved */
	names	*tnames;	/* if present, the checked out L/G/R */
	int	n;		/* number of tries on this file */
	char	*prompt;	/* whatever should be the prompt */
	char	*pager;		/* $PAGER or more */
	char	*editor;	/* $EDITOR or vi */
	rfuncs	*funcs;		/* the ops vector */
	void	*opaque;	/* a pointer on which to hang state */
	char	*shell;		/* shell command, if ! */
	/* the following tell us which resolve loop we are in */
	u32	res_gcreate:1;	/* new file conflicts with local gfile */
	u32	res_screate:1;	/* new file conflicts with gfile */
	u32	res_dirfile:1;	/* pathname component conflicts w/ file */
	u32	res_resync:1;	/* conflict in the RESYNC dir */
	u32	res_contents:1;	/* content conflict */
};


names	*res_getnames(sccs *s, int type);
char	*mode2a(mode_t m);
int	more(resolve *rs, char *file);
resolve	*resolve_init(opts *opts, sccs *s);
int	automerge(resolve *rs, names *n, int identical);
int	resolve_automerge(sccs *s, ser_t local, ser_t remote);
int	c_revtool(resolve *rs);
int	c_merge(resolve *rs);
void 	flags_delta(resolve *,char *, ser_t, int, int);
int	edit(resolve *rs);
void	freenames(names *names, int free_struct);
int	get_revs(resolve *rs, names *n);
void	mode_delta(resolve*, char *, ser_t d, mode_t, int which);
int	move_remote(resolve *rs, char *sfile);
int	ok_local(sccs *s, int check_pending);
void	type_delta(resolve *, char *, ser_t, ser_t, int);
int	res_abort(resolve *rs);
int	res_clear(resolve *rs);
int	res_diff(resolve *rs);
int	res_difftool(resolve *rs);
int	res_h(resolve *rs);
int	res_hl(resolve *rs);
int	res_hr(resolve *rs);
int	res_mr(resolve *rs);
int	res_quit(resolve *rs);
int	res_revtool(resolve *rs);
int	res_sdiff(resolve *rs);
int	res_vl(resolve *rs);
int	res_vr(resolve *rs);
void	resolve_cleanup(opts *opts, int what);
int	resolve_contents(resolve *rs);
int	resolve_create(resolve *rs, int type);
void	resolve_free(resolve *rs);
int	resolve_loop(char *name, resolve *rs, rfuncs *rf);
int	resolve_renames(resolve *rs);
int	resolve_filetypes(resolve *rs);
int	resolve_modes(resolve *rs);
int	resolve_flags(resolve *rs);
int	c_shell(resolve *rs);
int	c_helptool(resolve *rs);
int	c_quit(resolve *rs);
void	saveKey(opts *opts, char *key, char *file);
int	slotTaken(resolve *rs, char *slot, char **why);
void	do_delta(opts *opts, sccs *s, char *comment);
void	export_revs(resolve *rs);
void	resolve_tags(opts *opts);
void	resolve_dump(resolve *rs);
int	do_diff(resolve *rs, char *left, char *right, int wait);
int	resolve_binary(resolve *rs);
void	restore_checkouts(opts *opts);
int	gc_sameFiles(resolve *rs);
int	comp_overlap(char **complist, char *path);
