/* Copyright (c) 2000 L.W.McVoy */
#include "system.h"
#include "sccs.h"
WHATSTR("@(#)%K%");

int
cat_main(int ac, char **av)
{
	sccs	*s;
	char	*name;
	int	errors = 0;

	debug_main(av);
	if (ac == 2 && streq("--help", av[1])) {
		system("bk help cat");
		return (1);
	}
	for (name = sfileFirst("cat", &av[1], 0); name; name = sfileNext()) {
		unless (s = sccs_init(name, INIT_SAVEPROJ, bk_proj)) continue;
		if (access(s->gfile, R_OK) == 0) {
			errors |= cat(s->gfile);
			sccs_free(s);
			continue;
		}
		unless (HASGRAPH(s)) {
			sccs_free(s);
			continue;
		}
		if (sccs_get(s, 0, 0, 0, 0, SILENT|PRINT, "-")) errors |= 1;
		sccs_free(s);
	}
	sfileDone();
	return (errors);
}
