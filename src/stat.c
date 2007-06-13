#include "system.h"
#include "sccs.h"

private	void	print_sb(struct stat *sb, char *fn);
private int	do_stat(int which, int ac, char **av);

#define	USE_STAT	0
#define	USE_LSTAT	1

int
lstat_main(int ac, char **av)
{
	return (do_stat(USE_LSTAT, ac, av));
}

int
stat_main(int ac, char **av)
{
	return (do_stat(USE_STAT, ac, av));
}

private int
do_stat(int which, int ac, char **av)
{
	int	i, error = 0, rval = 0;
	char	buf[MAXPATH];
	struct	stat sb;

	if (av[1]) {
		for (i = 1; i < ac; i++) {
			if (which == USE_STAT) {
				error = stat(av[i], &sb);
			} else {
				error = lstat(av[i], &sb);
			}
			unless (error) print_sb(&sb, av[i]);
			rval |= error;
		}
	} else {
		while (fnext(buf, stdin)) {
			unless (chomp(buf)) {
				fprintf(stderr, "Bad filename '%s'\n", buf);
				rval = 1;
				continue;
			}
			if (which == USE_STAT) {
				error = stat(buf, &sb);
			} else {
				error = lstat(buf, &sb);
			}
			unless (error) print_sb(&sb, buf);
			rval |= error;
		}
	}
	return (rval);
}

private void
print_sb(struct stat *sb, char *fn)
{
	char	fmtbuf[128];

	bzero(fmtbuf, sizeof(fmtbuf));
	/* build the format string according to sizes */
#ifdef __FreeBSD__
/* FreeBSD 2.2.8 didn't have %llu, so we need to use %qu */
#define	szfmt(x)	switch (sizeof(x)) { \
			case 2:						\
			case 4: strcat(fmtbuf, "%u|"); break;		\
			case 8: strcat(fmtbuf, "%qu|"); break;		\
			default: fprintf(stderr, "weird size?\n");	\
				 return;				\
		}
#else
#define	szfmt(x)	switch (sizeof(x)) { \
			case 2:						\
			case 4: strcat(fmtbuf, "%u|"); break;		\
			case 8: strcat(fmtbuf, "%llu|"); break;		\
			default: fprintf(stderr, "weird size?\n");	\
				 return;				\
		}
#endif
	szfmt(sb->st_dev); szfmt(sb->st_ino);
	strcat(fmtbuf, "%o|"); /* st_mode goes in octal */
	szfmt(sb->st_nlink); szfmt(sb->st_uid); szfmt(sb->st_gid);
	szfmt(sb->st_rdev); szfmt(sb->st_size); szfmt(sb->st_atime);
	szfmt(sb->st_mtime); szfmt(sb->st_ctime);
	strcat(fmtbuf, "%s\n");
	printf(fmtbuf,
	    sb->st_dev, sb->st_ino, sb->st_mode,
	    sb->st_nlink, sb->st_uid, sb->st_gid,
	    sb->st_rdev, sb->st_size, sb->st_atime,
	    sb->st_mtime, sb->st_ctime,
	    fn);
}
