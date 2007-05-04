#include "system.h"
#include "sccs.h"

int
do_checkout(sccs *s)
{
	int	getFlags;

	switch(proj_checkout(s->proj)) {
	    case CO_GET: getFlags = GET_EXPAND; break;
	    case CO_EDIT: getFlags = GET_EDIT; break;
	    case CO_LAST:
		/* XXX - this counts on these flags and it is possible
		 * that they are not set.
		 */
		unless (HAS_GFILE(s)) return (0);
		if (HAS_PFILE(s)) {
			getFlags = GET_EDIT;
		} else {
			getFlags = GET_EXPAND;
		}
		break;
	    default: return (0);
	}
	s = sccs_restart(s);
	unless (s) return (-1);
	if (sccs_get(s, 0, 0, 0, 0, SILENT|getFlags, "-")) return (-1);
	return (0);
}
