#include "system.h"
#include "sccs.h"

int
fix_main(int ac,  char **av)
{
	int	c, i;
	char	buf[MAXLINE];
	char	fix_file[MAXPATH];
	char	*qflag = "-q", *p = 0;
	sccs	*s;
	delta	*d;

	if (ac == 2 && streq("--help", av[1])) {
		system("bk help fix");
		return (1);
	}
	while ((c = getopt(ac, av, "qv")) != -1) {
		switch (c) {
		    case 'q': break;	/* undoc 2.0 */
		    case 'v': qflag = ""; break;	/* doc 2.0 */
		    default :
			system("bk help -s fix");
			return (1);
		}
	}
	i =  optind - 1;
	while (av[++i]) {
		if (writable(av[i])) {
			printf("%s is already edited\n", av[i]);
			continue;
		}
		sprintf(fix_file, "%s-fix", av[i]);
		if (exists(fix_file)) {
			printf("%s exists, skipping that file", fix_file);
			continue;
		}
		if (sccs_filetype(av[i]) == 's') {
			p = strdup(av[i]);
		} else {
			p = name2sccs(av[i]);
		}
		get(p, SILENT|PRINT, fix_file);
		s = sccs_init(p, SILENT, 0);
		assert(s);
		d = findrev(s, NULL);
		assert(d);
		sprintf(buf, "bk stripdel %s -r%s %s", qflag, d->rev, av[i]);
		sccs_free(s);
		if (system(buf) == 0) {
			int gflags = SILENT|GET_SKIPGET|GET_EDIT;
			s = sccs_init(p, SILENT, 0);
			assert(s);
			if (sccs_get(s, 0, 0, 0, 0, gflags, "-")) {
				fprintf(stderr, "cannot lock %s\n", av[i]);
			}
			sccs_free(s);
			unlink(av[i]);
			if (rename(fix_file, av[i]) == -1) {
				perror(av[i]);
				free(p);
				exit(1);
			}
		} else {
			unlink(fix_file);
		}
	};
	if (p) free(p);
	return (0);
}
