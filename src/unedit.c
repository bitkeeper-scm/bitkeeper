/* Copyright (c) 1997 L.W.McVoy */
#include "system.h"
#include "sccs.h"

/*
 * This works even if there isn't a gfile.
 */
int
unedit_main(int ac, char **av)
{
	sccs	*s = 0;
	int	sflags = SF_NODIREXPAND;
	int	ret = 0;
	char	*name;

	/*
	 * Too dangerous to unedit everything automagically,
	 * make 'em spell it out.
	 */
	optind = 1;
	unless (name = sfileFirst("unedit", &av[optind], sflags)) {
		fprintf(stderr,
		  "unedit: must have explicit list when discarding changes.\n");
			return(1);
	}
	while (name) {
		s = sccs_init(name, SILENT|INIT_NOCKSUM);
		if (s) {
			if (sccs_unedit(s, SILENT)) ret = 1;
			sccs_free(s);
		}
		name = sfileNext();
	}
	if (sfileDone()) ret = 1;
	return (ret);
}
