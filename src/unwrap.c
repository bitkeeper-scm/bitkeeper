#include "system.h"
#include "sccs.h" 

extern char *bin;

int
unwrap_main(int ac,  char **av)
{
	char buf[MAXLINE];

	platformInit();  
	while (fgets(buf, sizeof(buf), stdin)) {
		if (strneq(buf, "# Patch vers:", 13)) {
			fputs(buf, stdout);
			while (fgets(buf, sizeof(buf), stdin)) {
				fputs(buf, stdout);
			}
			return (0);
		} else if (strneq(buf, "## Wrapped with", 15)) {
			char wrap_path[MAXLINE], wrap[MAXPATH];

			unless (sscanf(&buf[16], "%s ##", wrap) == 1) {
				fprintf(stderr,  "can not extract wrapper\n");
				exit(1);
			}
			sprintf(wrap_path, "%sun%swrap", bin, wrap);
			if (executable(wrap_path)) {
				char *av[2] = {wrap_path, 0};
				return (spawnvp_ex(_P_WAIT, wrap_path, av));
			} else {
				FILE *f = fopen(DEV_TTY, "wb");
				fprintf(f,
				   "bk receive: don't have %s wrapper\n", wrap);
				fclose(f);
				return(1);
			}
		} 
	}
	/* we should never get here */
	return (1);
}
