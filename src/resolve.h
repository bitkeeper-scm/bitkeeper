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

typedef struct {
	int	debug:1;	/* debugging messages */
	int	pass1:1;	/* move to RENAMES */
	int	pass2:1;	/* move back to RESYNC */
	int	pass3:1;	/* resolve perm/flags/content conflicts */
	int	pass4:1;	/* move from RESYNC to repository */
	int	automerge:1;	/* automerge everything */
	int	edited:1;	/* set if edited files found */
	int	resolveNames:1;	/* resolve (or don't resolve) conflicts */
	int	noconflicts:1;	/* do not handle conflicts */
	int	hadconflicts:1;	/* set if conflicts are found */
	int	errors:1;	/* set if there are errors, don't apply */
	int	quiet:1;	/* no output except for errors */
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

typedef struct {
	char	*local;
	char	*gca;
	char	*remote;
} names;

void	automerge(opts *opts, char *name, char *rfile, names *revs);
void	resolve_cleanup(opts *opts, int what);
void	commit(opts *opts);
void	conflict(opts *opts, char *rfile);
int	create(opts *opts, sccs *s, delta *d);
int	edit(opts *opts, sccs *s, names *revs);
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
int	rename_file(opts *opts, sccs *s, names *names, char *mfile);
void	saveKey(opts *opts, char *key, char *file);
int	slotTaken(opts *opts, char *slot);
