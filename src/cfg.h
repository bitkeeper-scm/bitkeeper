#ifndef	_CFG_H_
#define	_CFG_H_
#include "system.h"
#include "sccs.h"
#include "proj.h"
#include "bam.h"

// config variables types
enum {
	CFG_BOOL,		/* Boolean */
	CFG_INT,		/* Integer */
	CFG_SIZE,		/* Size (K, M, G) */
	CFG_STR,		/* String */
};

// config variables access via enum
#define CONFVAR(def, ...) CFG_##def,
enum {
#include "confvars.h"
};
#undef CONFVAR

#define	CFG_MAX_ALIASES	3	/* will get compile warning if too small */

char	*cfg_str(project *p, int idx);
int	cfg_bool(project *p, int idx);
u64	cfg_size(project *p, int idx);
i64	cfg_int(project *p, int idx);

#endif	/* _CFG_H_ */
