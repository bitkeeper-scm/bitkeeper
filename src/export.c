#include "system.h"
#include "sccs.h"

int
export_main(int ac,  char **av)
{
	int	c, count;
	int	vflag = 0, hflag = 0, kflag = 0, tflag = 0, wflag = 0;
	char	*rev = NULL, *diff_style = NULL;
	char	file_rev[MAXPATH];
	char	buf[MAXLINE], buf1[MAXPATH];
	char	include[MAXLINE] = "", exclude[MAXLINE] =  "";
	char	*src, *dst;
	char	*p, *q;
	char	src_path[MAXPATH], dst_path[MAXPATH];
	sccs	*s;
	delta	*d;
	FILE	*f;
	char	*type = 0;

	while ((c = getopt(ac, av, "d:Dhkt:Twvi:x:r:")) != -1) {
		switch (c) {
		    case 'v':	vflag = 1; break;
		    case 'd':	diff_style = optarg; break;
		    case 'h':	hflag = 1; break; /* disbale patch header */
		    case 'k':	kflag = 1; break;
		    case 'r':	rev = optarg; break;
		    case 't':	if (type) goto usage;
				type = optarg; 
				if (!streq(type, "patch") &&
				    !streq(type, "plain")) {
					goto usage;
				}
				break;
		    case 'T':	tflag = 1; break;
		    case 'w':	wflag = 1; break;
		    case 'i':	sprintf(include, "| grep -E '%s' ",  optarg);
				break;
		    case 'x':	sprintf(exclude, "| grep -E -v '%s' ",  optarg);
				break;
		    default :
usage:			fprintf(stderr,
		"usage: bk export [-tplain|patch] [-TDkwv] [-i<pattern>] [-x<pattern>]\n");
			fprintf(stderr,
				"\t[-r<rev> | -d<date>] [source] dest\n");
			exit(1);
		}
	}

	unless (type) type = "plain";
	if (streq(type, "patch")) {
		unless (diff_style) diff_style = "u";
		sprintf(buf, "bk mkrev -r%s | bk gnupatch -d%c %s %s",
		    rev, diff_style[0], hflag ? "-h" : "", tflag ? "-T" : "");
		return (system(buf));
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
	strcpy(dst_path, fullname(dst, 0));
	chdir(src);
	if (sccs_cd2root(0, 0) == -1) {
		fprintf(stderr, "Can not find package root.\n");
		exit(1);
	}
	strcpy(src_path, fullname(".", 0));

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
		int	flags = 0;

		chop(buf);
		p = strchr(buf, '@');
		assert(p);
		*p++ = '\0';
		if (streq(buf, "ChangeSet")) continue;
		sprintf(buf1, "%s/%s", src_path, buf);
		q = name2sccs(buf1);
		s = sccs_init(q, SILENT, 0);
		assert(s && s->tree);
		free(q);
		d = findrev(s, p);
		assert(d);
		/*
		 * Do not export file under the BitKeeper directory
		 */
		if ((strlen(d->pathname) >= 10) &&
		    strneq("BitKeeper/", d->pathname, 10)) {
			sccs_free(s);
			continue;
		}
		sprintf(output, "%s/%s", dst_path, d->pathname);
		unless (vflag) flags |= SILENT;
		unless (kflag) flags |= GET_EXPAND;
		if (tflag) flags |= GET_DTIME;
		mkdirf(output);
		/*
		 * This is stolen from the get -G code
		 * XXX - why do we have output then?
		 */
		free(s->gfile);
		s->gfile = strdup(output);
		if (sccs_get(s, p, 0, 0, 0, flags, "-")) {
			fprintf(stderr, "can not export to %s\n", output);
		}
		sccs_free(s);
		if (wflag) chmod(output, 0644);
	}
	fclose(f);
	unlink(file_rev);
	return (0);
}
