#include "system.h"
#include "sccs.h" 

extern char *editor, *pager, *bin; 

main(int ac,  char **av)
{
	char buf[MAXLINE];
	char help_out[MAXPATH];
	FILE *f;
	int i = 0;

	platformInit();  
	if (ac == 1) {
		sprintf(buf, "%sgethelp help | %s", bin, pager);
		system(buf);
		return (0);
	}
	sprintf(help_out, "%s/bk_help%d", TMP_PATH, getpid());
	while (av[++i]) {
		sprintf(buf, "%sgethelp help_%s %s > %s",
						bin, av[i], bin, help_out);
		if (system(buf) != 0) {
#ifdef WIN32
			sprintf(buf, "%s%s.exe", bin, av[i]);
#else
			sprintf(buf, "%s%s", bin, av[i]);
#endif
			if (is_command(buf)) {
				f = fopen(help_out, "ab");
				fprintf( f,
	"		-------------- %s help ---------------\n\n", av[i]);
				fclose(f);
				sprintf(buf, "%s%s --help 2>&1", bin, av[i]);
				system(buf);
			} else {
				f = fopen(help_out, "ab");
				fprintf(f, "No help for %s, check spelling.\n",
									av[i]);
				fclose(f);
			}
		}
	}
	sprintf(buf, "%s %s", pager, help_out);
	system(buf);
	unlink(help_out);
	return (0);
}

int
is_command(char *file)
{
#ifdef WIN32
	retun (exists(file));
#else
	return(executable(file));
#endif
}
