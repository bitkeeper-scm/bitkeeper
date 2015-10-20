#ifndef	_CFG_H_
#define	_CFG_H_
#include "system.h"
#include "sccs.h"
#include "proj.h"
#include "bam.h"

// config variables access via enum
#define CONFVAR(def, ...) CFG_##def,
enum {
#include "confvars.h"
};
#undef CONFVAR

char	*cfg_def(int idx);
char	*cfg_alias(char *name);	/* return canonical name */
char	*cfg_str(project *p, int idx);
int	cfg_bool(project *p, int idx);
u64	cfg_size(project *p, int idx);
i64	cfg_int(project *p, int idx);
int	cfg_findVar(char *name);
MDBM	*cfg_loadSetup(MDBM *db);
void	cfg_printDefaults(MDBM *db, MDBM *defs, MDBM *merge);
#endif	/* _CFG_H_ */
