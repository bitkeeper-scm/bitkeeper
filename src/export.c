#include "system.h"
#include "sccs.h"

private int included(char *, char *);
private int excluded(char *, char *);
private int export_patch(char *, char *, char *, char *, int, int);

int
export_main(int ac,  char **av)
{
	int	i, c, count, trim = 0;
	int	vflag = 0, hflag = 0, kflag = 0, tflag = 0, wflag = 0;
	char	*rev = NULL, *diff_style = NULL;
	char	file_rev[MAXPATH];
	char	buf[MAXLINE], buf1[MAXPATH];
	char	include[MAXLINE] = "";
	char	exclude[MAXLINE] = "";
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

	while ((c = getopt(ac, av, "d:hkp:t:Twvi|x|r:S")) != -1) {
		switch (c) {
		    case 'v':	vflag = 1; break;		/* doc 2.0 */
		    case 'q':					/* undoc 2.0 */
				break; /* no op; for interface consistency */
		    case 'd':	diff_style = optarg; break;	/* doc 2.0 */
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
		    case 'i':	if (optarg && *optarg) {	/* doc 2.0 */
					strcpy(include, optarg);
				} else {
					include[0] = 0;
				}
				break;
		    case 'x':	if (optarg && *optarg) {	/* doc 2.0 */
					strcpy(exclude, optarg);
				} else {
					exclude[0] = 0;
				}
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
		    rev, include, exclude, hflag, tflag));
	}

	count =  ac - optind;
	switch (count) {
	    case 1: src = "."; dst = av[optind]; break;
	    case 2: src = av[optind++]; dst = av[optind]; break;
	    default: goto usage;
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

	sprintf(file_rev, "%s/bk_file_rev%u", TMP_PATH, getpid());
	if (rev) {
		sprintf(buf, "bk rset -hl%s > %s", rev, file_rev);
	} else {
		sprintf(buf, "bk rset -hl+ > %s", file_rev);
	}
	system(buf);

	f = fopen(file_rev, "rt");
	assert(f);
	while (fgets(buf, sizeof(buf), f)) {
		char	*t, output[MAXPATH];
		int	flags = 0;
		struct	stat sb;

		chop(buf);
		if (!included(buf, include)) continue;
		if (excluded(buf, exclude)) continue;
		p = strchr(buf, BK_FS);
		assert(p);
		*p++ = '\0';
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
		d = findrev(s, q);
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
export_patch(char *diff_style, char *rev,
			char *include, char *exclude, int hflag, int tflag)
{
	FILE	*f, *f1;
	char	buf[MAXLINE], file_rev[MAXPATH];
	int	status;

	unless (diff_style) diff_style = "u";
	sprintf(file_rev, "%s/bk_file_rev%u", TMP_PATH, getpid());
	sprintf(buf, "bk rset -hr%s > %s", rev ? rev : "+", file_rev);
	status = system(buf);
	unless (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
		unlink(file_rev);
		return (1);
	}
	sprintf(buf, "bk gnupatch -d%c %s %s",
	    diff_style[0], hflag ? "-h" : "", tflag ? "-T" : "");
	f1 = popen(buf, "w");
	f = fopen(file_rev, "rt");
	assert(f);
	while (fgets(buf, sizeof(buf), f)) {
		char	*fstart, *fend;

		chop(buf);
		if (!included(buf, include)) continue;
		if (excluded(buf, exclude)) continue;
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

private int
included(char *fname, char *include)
{
	unless (include && include[0]) return (1);
	return (match_one(fname, include, 0));
}

private int
excluded(char *fname, char *exclude)
{
	unless (exclude && exclude[0]) return (0);
	return (match_one(fname, exclude, 0));
}
