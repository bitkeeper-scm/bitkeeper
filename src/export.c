#include "system.h"
#include "sccs.h"

extern char *bin;

int
export_main(int ac,  char **av)
{
	int	c, count;
	int	vflag = 0, kflag = 0, tflag = 0, wflag = 0;
	char	*rev = NULL;
	char	s_cset[MAXPATH] = CHANGESET;
	char	file_rev[MAXPATH];
	char	buf[MAXLINE], buf1[MAXPATH];
	char	include[MAXLINE] = "", exclude[MAXLINE] =  "";
	char	*src, *dst;
	char	*p, *q;
	char	src_path[MAXPATH], dst_path[MAXPATH];
	sccs	*s;
	delta	*d;
	FILE	*f;

	platformInit();

	while ((c = getopt(ac, av, "Dktwvi:x:r:")) != -1) {
		switch (c) {
		    case 'v':	vflag = 1; break;
		    case 'k':	kflag = 1; break;
		    case 'r':	rev = optarg; break;
		    case 't':	tflag = 1; break;
		    case 'w':	wflag = 1; break;
		    case 'i':	sprintf(include, "| grep -E '%s' ",  optarg);
				break;
		    case 'x':	sprintf(exclude, "| grep -E -v '%s' ",  optarg);
				break;
		    default :
usage:			fprintf(stderr,
		"usage: bk export [-tDkwv] [-i<pattern>] [-x<pattern>]\n");
			fprintf(stderr,
				"\t[-r<rev> | -d<date>] [source] dest\n");
			exit(1);
		}
	}
	count =  ac - optind;
	switch (count) {
	    case 1: src = "."; dst = av[optind]; break;
	    case 2: src = av[optind++]; dst = av[optind]; break;
	    default: goto usage;
	}

	if (mkdirp(dst) != 0) {
		fprintf(stderr, "can not mkdir %s\n", dst);
		exit(1);
	}
	chdir(src);
	if (sccs_cd2root(0, 0) == -1) {
		fprintf(stderr, "Can not find project root.\n");
		exit(1);
	}
	strcpy(src_path, fullname(src, 0));
	strcpy(dst_path, fullname(dst, 0));

	sprintf(file_rev, "%s/bk_file_rev%d", TMP_PATH, getpid());
	if (rev) {
		sprintf(buf, "bk cset -D -t%s %s %s > %s",
					rev, include, exclude, file_rev);
	} else {
		sprintf(buf, "bk cset -D -t+  %s %s> %s",
						include, exclude, file_rev);
	}
	system(buf);
	f = fopen(file_rev, "rt");
	assert(f);
	while (fgets(buf, sizeof(buf), f)) {
		char	output[MAXPATH];
		int	flags = PRINT;

		chop(buf);
		p = strchr(buf, '@');
		assert(p);
		*p++ = '\0';
		if (streq(buf, "ChangeSet")) continue;
		sprintf(buf1, "%s/%s", src_path, buf);
		q = name2sccs(buf1);
		s = sccs_init(q, SILENT, 0);
		assert(s);
		free(q);
		d = findrev(s, p);
		assert(d);
		sprintf(output, "%s/%s", dst_path, d->pathname);
		unless (vflag) flags |= SILENT;
		unless (kflag) flags |= GET_EXPAND;
		if (tflag) flags |= GET_DTIME;
		mkdirf(output);
		if (sccs_get(s, p, 0, 0, 0, flags, output)) {
			fprintf(stderr, "can not export to %s\n", output);
		}
		sccs_free(s);
		if (wflag) chmod(output, 0644);
	}
	fclose(f);
	unlink(file_rev);
	return (0);
}
