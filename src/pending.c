#include "system.h"
#include "sccs.h"

extern char *pager;

int
pending_main(int ac, char **av)
{
	char	buf[MAXLINE], *tmp;
	int	c, quiet = 0;
	char	*dspec =
":DPN:@:I:, :Dy:-:Dm:-:Dd: :T::TZ:, :P:$if(:HT:){@:HT:}\\n\
$each(:C:){  (:C:)\n}$each(:SYMBOL:){  TAG: (:SYMBOL:)\\n}\n";

	while ((c = getopt(ac, av, "q")) != -1) { 
		switch (c) {
		    case 'q': quiet = 1; break;
		    default:
			system("bk help -s pending");
			return (1);
		}
	}
	if (av[optind]) chdir(av[optind]);
	if (sccs_cd2root(0, 0) == -1) {
		fprintf(stderr, "pending: cannot find project root\n");
		exit(1);
	}
	tmp = bktmp(0, "pending");
	sysio(0, tmp, 0, "bk", "sfiles", "-pCA", SYS);
	unless (size(tmp) > 0) {
		unlink(tmp);
		free(tmp);
		return (1);
	}
	unless (quiet) {
		sprintf(buf, "bk prs -Yh '-d%s' - < %s | %s", dspec, tmp, pager);
		system(buf);
	}
	unlink(tmp);
	free(tmp);
	return (0);	/* return YES to the shell, there are pending files */
}
