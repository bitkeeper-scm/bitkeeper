#include "system.h"
#include "sccs.h"

#define	stdlog	opts->log ? opts->log : stderr
#define	INIT	(INIT_SAVEPROJ|INIT_NOCKSUM)
#define	CLEAN_RESYNC	1	/* blow away the RESYNC dir */
#define	CLEAN_PENDING	2	/* blow away the PENDING dir */
#define	CLEAN_OK	4	/* quietly exit 0 */
#define	SHOUT() \
	fputs("===================== ERROR ========================\n", stderr);
#define	SHOUT2() \
	fputs("====================================================\n", stderr);
#define	SFILE_CONFLICT	1
#define	GFILE_CONFLICT	2
#define	RESYNC_CONFLICT	3

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
	u32	quiet:1;	/* no output except for errors */
	u32	textOnly:1;	/* no GUI tools allowed */
	u32	remerge:1;	/* redo already merged content conflicts */
	u32	force:1;	/* for forcing commits with unmerged changes */
	u32	advance:1;	/* advance after merging if commit works */
	u32	verbose:1;	/* be verbose on gets, etc */
	u32	hadConflicts:1;	/* conflicts during automerge */
	int	pass;		/* which pass are we in now */
	char	*comment;	/* checkin comment for commit */
	char	*mergeprog;	/* program to merge with */
	int	renames;	/* count of renames processed in pass 1 */
	int	renames2;	/* count of renames processed in pass 2 */
	int	resolved;	/* count of files resolved in pass 3 */
	int	applied;	/* count of files processed in pass 4 */
	MDBM	*rootDB;	/* db{ROOTKEY} = pathname in RESYNC */
	MDBM	*idDB;		/* for the local repository, not RESYNC */
	project	*local_proj;	/* for the local repository, not RESYNC */
	project	*resync_proj;	/* for RESYNC project */
	FILE	*log;		/* if set, log to here */
} opts;

/*
 * Used both for r.files and m.files.
 * See getnames() and freenames().
 */
typedef struct {
	char	*local;
	char	*gca;
	char	*remote;
} names;

typedef	int (*rfunc)();

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
typedef	struct resolve {
	opts	*opts;		/* so we don't have to pass both */
	sccs	*s;		/* the sccs file we are resolving */
	char	*key;		/* root key of this sfile */
	delta	*d;		/* Top of LOD of the sccs file */
	char	*dname;		/* name2sccs(d->pathname) */
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
	/* the following tell us which resolve loop we are in */
	u32	res_gcreate:1;	/* new file conflicts with local gfile */
	u32	res_screate:1;	/* new file conflicts with gfile */
	u32	res_resync:1;	/* conflict in the RESYNC dir */
	u32	res_contents:1;	/* content conflict */
} resolve;


void	automerge(resolve *rs);
void	resolve_cleanup(opts *opts, int what);
int	resolve_create(resolve *rs, int type);
int	resolve_contents(resolve *rs);
int	resolve_renames(resolve *rs);
void	commit(opts *opts);
void	conflict(opts *opts, char *rfile);
int	create(resolve *rs);
int	edit(resolve *rs);
void	freeStuff(opts *opts);
void	freenames(names *names, int free_struct);
names	*getnames(char *path, int type);
int	nameOK(opts *opts, sccs *s);
void	pass1_renames(opts *opts, sccs *s);
int	pass2_renames(opts *opts);
int	pass3_resolve(opts *opts);
int	pass4_apply(opts *opts);
int	passes(opts *opts);
int	pending();
int	pendingRenames();
int	rename_file(resolve *rs);
void	saveKey(opts *opts, char *key, char *file);
int	slotTaken(opts *opts, char *slot);
resolve	*resolve_init(opts *opts, sccs *s);
void	resolve_free(resolve *rs);
int	resolve_loop(char *name, resolve *rs, rfuncs *rf);
void	rev(resolve *r, char *sfile, delta *d, char *rfile, int which);
int	getline(int in, char *buf, int size);
int	prompt(char *msg, char *buf);
int	confirm(char *msg);
int	move_remote(resolve *rs, char *sfile);
int	res_abort(resolve *rs);
char	*res_getlocal(char *gfile);
int	res_diff(resolve *rs);
int	res_sdiff(resolve *rs);
int	res_difftool(resolve *rs);
int	res_mr(resolve *rs);
int	res_vl(resolve *rs);
int	res_vr(resolve *rs);
int	res_hl(resolve *rs);
int	res_hr(resolve *rs);
int	res_quit(resolve *rs);
int	res_clear(resolve *rs);
int	ok_local(sccs *s, int check_pending);
int	get_revs(resolve *rs, names *n);
