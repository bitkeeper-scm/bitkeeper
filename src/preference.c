#include "system.h"
#include "sccs.h"

int
do_checkout(sccs *s)
{
	int	getFlags = 0;
	char	*co;

	co = proj_configval(s->proj, "checkout");
	if (strieq(co, "get")) getFlags = GET_EXPAND;
	if (strieq(co, "edit")) getFlags = GET_EDIT;
	if (getFlags) {
		s = sccs_restart(s);
		unless (s) return (-1);
		if (sccs_get(s, 0, 0, 0, 0, SILENT|getFlags, "-")) {
			return (-1);
		}
	}
	return (0);
}
