#include "system.h"
#include "sccs.h"

extern char *pager;

int
pending_main(int ac, char **av)
{
	char	buf[MAXLINE], *p;
	char	*dspec =
":DPN:@:I:, :Dy:-:Dm:-:Dd: :T::TZ:, :P:$if(:HT:){@:HT:}\\n\
$each(:C:){  (:C:)} \\n$each(:SYMBOL:){  TAG: (:SYMBOL:)\\n}";

	if (sccs_cd2root(0, 0) == -1) {
		fprintf(stderr, "pending: cannot find project root\n");
		exit(1);
	}
	sprintf(buf, "bk sfiles -CA | bk prs -Yh '-d%s' - | %s", dspec, pager);
	return (system(buf));
}
