#include "system.h"
#include "sccs.h"

private int export_patch(char *, char *, char **, char **, int, int);

int
export_main(int ac,  char **av)
{
	int	i, c, count, trim = 0;
	int	vflag = 0, hflag = 0, kflag = 0, tflag = 0, wflag = 0;
	char	*rev = NULL, *diff_style = "u";
	char	file_rev[MAXPATH];
	char	buf[MAXLINE], buf1[MAXPATH];
	char	**includes = 0;
	char	**excludes = 0;
	char	*src, *dst;
	char	*p, *q, *p_sav;
	char	src_path[MAXPATH], dst_path[MAXPATH];
	sccs	*s;
	delta	*d;
	FILE	*f;
	char	*type = 0;
	int	sysfiles = 0;

	if (ac == 2 && streq("--help", av[1])) {
		system("bk help export");
		return (1);
	}

	while ((c = getopt(ac, av, "d|hkp:t:Twvi:x:r:S")) != -1) {
		switch (c) {
		    case 'v':	vflag = 1; break;		/* doc 2.0 */
		    case 'q':					/* undoc 2.0 */
				break; /* no op; for interface consistency */
		    case 'd':
			diff_style = notnull(optarg); break;	/* doc 2.0 */
		    case 'h':					/* doc 2.0 */
			hflag = 1; break; /*disable patch header*/
		    case 'k':	kflag = 1; break;		/* doc 2.0 */
		    case 'p':	trim = atoi(optarg); break;
		    case 'S':   sysfiles = 1; break;		/* undoc 2.2 */
		    case 'r':	rev = optarg; break;		/* doc 2.0 */
		    case 't':	if (type) goto usage;		/* doc 2.0 */
				type = optarg; 
				if (!streq(type, "patch") &&
				    !streq(type, "plain")) {
					goto usage;
				}
				break;
		    case 'T':	tflag = 1; break;		/* doc 2.0 */
		    case 'w':	wflag = 1; break;		/* doc 2.0 */
		    case 'i':					/* doc 3.0 */
			includes = addLine(includes, strdup(optarg));
			break;
		    case 'x':					/* doc 3.0 */
			excludes = addLine(excludes, strdup(optarg));
			break;
		    default :
usage:			system("bk help -s export");
			exit(1);
		}
	}

	unless (type) type = "plain";
	if (streq(type, "patch")) {
		if (sccs_cd2root(0, 0) == -1) {
			fprintf(stderr, "export: can not find package root.\n");
			exit(1);
		}
		return (export_patch(diff_style,
		    rev, includes, excludes, hflag, tflag));
	}

	count =  ac - optind;
	switch (count) {
	    case 1: src = "."; dst = av[optind]; break;
	    case 2: src = av[optind++]; dst = av[optind]; break;
	    default: goto usage;
	}

	unless (isdir(src)) {
		fprintf(stderr, "export: %s does not exist.\n", src);
		exit(1);
	}

	if (mkdirp(dst) != 0) {
		fprintf(stderr, "cannot mkdir %s\n", dst);
		exit(1);
	}
	strcpy(dst_path, fullname(dst, 0));
	chdir(src);
	if (sccs_cd2root(0, 0) == -1) {
		fprintf(stderr, "Cannot find package root.\n");
		exit(1);
	}
	strcpy(src_path, fullname(".", 0));

	bktmp(file_rev, "file_rev");
	if (rev) {
		sprintf(buf, "bk rset -hl'%s' > '%s'", rev, file_rev);
	} else {
		sprintf(buf, "bk rset -hl+ > '%s'", file_rev);
	}
	system(buf);

	f = fopen(file_rev, "rt");
	assert(f);
	while (fgets(buf, sizeof(buf), f)) {
		char	*t, output[MAXPATH];
		int	flags = 0;
		struct	stat sb;

		/* trim off |1.5\n */
		chop(buf);
		p = strchr(buf, BK_FS);
		assert(p);
		*p++ = '\0';

		if (excludes && match_globs(buf, excludes, 0)) continue;
		if (includes && !match_globs(buf, includes, 0)) continue;

		/* skip BitKeeper and deleted files */
		if (!sysfiles && strneq(p, "BitKeeper/", 10)) continue;
		if (streq(buf, "ChangeSet")) continue;
		sprintf(buf1, "%s/%s", src_path, buf);
		t = name2sccs(buf1);
		s = sccs_init(t, SILENT, 0);
		free(t);
		assert(s && HASGRAPH(s));
		q = strchr(p, BK_FS); 
		assert(q);
		*q++ = '\0';
		d = sccs_findrev(s, q);
		assert(d);
		p_sav = p;
		for (i = 0; i < trim; i++) {
			p = strchr(p, '/');
			unless (p) {
				fprintf(stderr,
				    "%s: Cannot trim path; path too short. "
				    "File skipped.\n",
				    p_sav);
				goto next;
			}
			p++;
		}
		sprintf(output, "%s/%s", dst_path, p);
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
		if (sccs_get(s, q, 0, 0, 0, flags, "-")) {
			fprintf(stderr, "cannot export to %s\n", output);
		}
		sccs_free(s);
		if (wflag) {
			if (stat(output, &sb)) {
				perror(output);
			} else {
				chmod(output, sb.st_mode | S_IWUSR);
			}
		}
next:		;
	}
	fclose(f);
	unlink(file_rev);
	return (0);
}

private int
export_patch(char *diff_style,
	char *rev, char **includes, char **excludes, int hflag, int tflag)
{
	FILE	*f, *f1;
	char	buf[MAXLINE], file_rev[MAXPATH];
	int	status;

	bktmp(file_rev, "file_rev");
	sprintf(buf, "bk rset -hr'%s' > '%s'", rev ? rev : "+", file_rev);
	status = system(buf);
	unless (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
		unlink(file_rev);
		return (1);
	}
	sprintf(buf, "bk gnupatch -d%s %s %s",
	    diff_style, hflag ? "-h" : "", tflag ? "-T" : "");
	f1 = popen(buf, "w");
	f = fopen(file_rev, "rt");
	assert(f);
	while (fgets(buf, sizeof(buf), f)) {
		char	*fstart, *fend;

		chop(buf);
		/*
		 * We want to match on any of the names and rset gives us
		 * <current>|<start>|<srev>|<end>|<erev>
		 */
		if (includes || excludes) {
			char	**names = splitLine(buf, "|", 0);
			int	skip = 0;

			if (excludes && (    /* any of the 3 below */
			    match_globs(names[1], excludes, 0) ||
			    match_globs(names[2], excludes, 0) ||
			    match_globs(names[4], excludes, 0))) {
				skip = 1;
			}
			if (includes && (    /* any of the 3 below */
			    !match_globs(names[1], includes, 1) ||
			    !match_globs(names[2], includes, 1) ||
			    !match_globs(names[4], includes, 1))) {
				skip = 1;
			}
			freeLines(names, free);
			if (skip) continue;
		}

		/* Skip BitKeeper/ files (but pass deletes..) */
		fstart = strchr(buf, BK_FS) + 1;
		fend = strchr(fstart, BK_FS) + 1;
		fend = strchr(fend, BK_FS) + 1;
		if (strneq(fstart, "BitKeeper/", 10) &&
		    strneq(fend, "BitKeeper/", 10)) continue;
		fprintf(f1, "%s\n", buf);
	}
	fclose(f);
	unlink(file_rev);
	return (pclose(f1));
}
