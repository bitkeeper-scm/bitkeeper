#include "system.h"
#include "sccs.h" 

extern char *bin;

main(int ac,  char **av)
{
	int c, i;
	char buf[MAXLINE], opts[MAXLINE] = "";
	char fix_file[MAXPATH];
	char *qflag = "-q";
	char *p;
	sccs *s;
	delta *d;

	platformInit();  

	while ((c = getopt(ac, av, "qv")) != -1) {
		switch (c) { 
		    case 'q': break;
		    case 'v': qflag = ""; break;
		    default :
			fprintf(stderr, "unknow option <%c>\n", c);
			exit(1);
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
		sprintf(buf, "%sstripdel %s -r%s %s",
					bin, qflag, d->rev, av[i]);
		sccs_free(s);
		if (system(buf) == 0) {
			int gflags = SILENT|GET_SKIPGET|GET_EDIT;
			s = sccs_init(p, SILENT, 0);
			assert(s);
			if (sccs_get(s, 0, 0, 0, 0, gflags, "-")) {
				fprintf(stderr, "can not lock %s\n", av[i]);
			} 
			unlink(av[i]);
			if (rename( fix_file, av[i]) == -1) {
				perror(av[i]);
				free(p);
				exit(1);
			}
		} else {
			unlink(fix_file);
		}
	};
	free(p);
	return (0);
}
