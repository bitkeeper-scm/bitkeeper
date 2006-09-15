#include "system.h"
#include "sccs.h"

int
do_checkout(sccs *s)
{
	int	getFlags;

	switch(proj_checkout(s->proj)) {
	    case CO_GET: getFlags = GET_EXPAND; break;
	    case CO_EDIT: getFlags = GET_EDIT; break;
	    default: return (0);
	}
	s = sccs_restart(s);
	unless (s) return (-1);
	if (sccs_get(s, 0, 0, 0, 0, SILENT|getFlags, "-")) {
		return (-1);
	}
	return (0);
}
