#include "system.h"
#include "sccs.h"

extern char *bin, *pager;

int
pending_main(int ac, char **av)
{
	char	buf[MAXLINE];
	char	*dspec =
":DPN:@:I:, :Dy:-:Dm:-:Dd: :T::TZ:, :P:$if(:HT:){@:HT:}\\n\
$each(:C:){  (:C:)} \\n$each(:SYMBOL:){  TAG: (:SYMBOL:)\\n}";

	platformInit();
	if (sccs_cd2root(0, 0) == -1) {
		fprintf(stderr, "pending: can not find project root\n");
		exit(1);
	}
	sprintf(buf, "%sbk sfiles -CA | BK_YEAR4=1 %sbk prs -h '-d%s' - | %s",
							bin, bin, dspec, pager);
	return (system(buf));
}
