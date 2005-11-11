#include "system.h"
#include "sccs.h"

private void	print_stimes(char *f, struct utimbuf *ub);

int
stattest_main(int ac, char **av)
{
	struct	utimbuf	ub;
	time_t	now;

	touch("f1", 0666);
	ub.actime = 1130544000;	/* Sat Oct 29 00:00:00 UTC 2005 */
	ub.modtime = 1130587200; /* Sat Oct 29 12:00:00 UTC 2005 */
	if (utime("f1", &ub)) perror("f1 utime");
	print_stimes("f1", &ub);
	unlink("f1");

	touch("f2", 0666);
	ub.actime = 1130803200;	/* Tue Nov  1 00:00:00 UTC 2005 */
	ub.modtime = 1130846400; /* Tue Nov  1 12:00:00 UTC 2005 */
	if (utime("f2", &ub)) perror("f2 utime");
	print_stimes("f2", &ub);
	unlink("f2");

	touch("f3", 0666);
	now = time(0);
	ub.actime = now;
	ub.modtime = now;
	print_stimes("f3", &ub);
	unlink("f3");

	return (0);
}

private void
print_stimes(char *f, struct utimbuf *ub)
{
	struct	stat	sb;

	printf("%s:\n", f);
	if (lstat(f, &sb)) {
		printf("stat failed\n");
		return;
	}
	printf("%u(%d) %u(%d)\n",
	       sb.st_mtime, (int)(ub->modtime - sb.st_mtime),
	       sb.st_atime, (int)(ub->actime - sb.st_atime));
}
