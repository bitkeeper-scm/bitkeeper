#include "system.h"
#include "sccs.h"

extern char *pager;

int
pending_main(int ac, char **av)
{
	int 	rc;
	char	buf[MAXLINE], *p;
	char	*dspec =
":DPN:@:I:, :Dy:-:Dm:-:Dd: :T::TZ:, :P:$if(:HT:){@:HT:}\\n\
$each(:C:){  (:C:)} \\n$each(:SYMBOL:){  TAG: (:SYMBOL:)\\n}";

	if (sccs_cd2root(0, 0) == -1) {
		fprintf(stderr, "pending: can not find project root\n");
		exit(1);
	}
#ifdef WIN32
	sprintf(buf, "BK_YEAR4=1");
	putenv(p=strdup(buf));
	sprintf(buf, "bk sfiles -CA | bk prs -h '-d%s' - | %s",
								dspec, pager);
	rc = system(buf);
	sprintf(buf, "BK_YEAR4=");
	putenv(buf);
	free(p);
	return (rc);
#else
	sprintf(buf, "bk sfiles -CA | BK_YEAR4=1 bk prs -h '-d%s' - | %s",
								dspec, pager);
	return (system(buf));
#endif
}
