#include "system.h"
#include "sccs.h"

extern char *bin;

int
unwrap_main(int ac,  char **av)
{
	char	buf[MAXLINE];
	int	n;

	if (ac == 2 && streq("--help", av[1])) {
		system("bk help unwrap");
		return (0);
	}

	/* Has to be a getline because we don't want stdin eating part of
	 * the input that we (may) want to send to the unwrap child.
	 */
	while (getline(0, buf, sizeof(buf)) > 0) {
		if (strneq(buf, "# Patch vers:", 13)) {
			out("\n");
			out(buf);
			out("\n");
			while ((n = read(0, buf, sizeof(buf))) > 0) {
				writen(1, buf, n);
			}
			return (0);
		} else if (strneq(buf, "## Wrapped with", 15)) {
			char	wrap_path[MAXLINE], wrap[MAXPATH];

			unless (sscanf(&buf[16], "%s ##", wrap) == 1) {
				fprintf(stderr,  "cannot extract wrapper\n");
				exit(1);
			}
			sprintf(wrap_path, "%s/un%swrap", bin, wrap);
			if (executable(wrap_path)) {
				char	*av[2] = {wrap_path, 0};

				return (spawnvp_ex(_P_WAIT, wrap_path, av));
			} else {
				FILE *f = fopen(DEV_TTY, "wb");
				fprintf(f,
					"bk receive: don't have %s wrapper\n",
					wrap);
				fclose(f);
				return (1);
			}
		}
	}
	/* we should never get here */
	return (1);
}
