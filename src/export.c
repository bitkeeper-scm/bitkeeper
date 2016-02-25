/*
 * Copyright 2000-2011,2016 BitMover, Inc
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "system.h"
#include "sccs.h"

typedef struct {
	u32	hflag:1;	/* -h: disable patch header */
	u32	tflag:1;	/* -T: set output modtime */
	u32	sysfiles:1;	/* -a: write system files */
	u32	standalone:1;	/* -S: standalone */

	char	*diff_style;	/* -dSTYLE: u c p */
	char	*rev;		/* -rREV */
	char	**aliases;	/* -sALIAS */
	char	**includes;	/* -iPAT */
	char	**excludes;	/* -xPAT */
} Opts;

private int	export_patch(Opts *opts);

int
export_main(int ac,  char **av)
{
	int	i, c, count, ret = 0;
	int	vflag = 0, kflag = 0, wflag = 0;
	char	buf[MAXLINE], buf1[MAXPATH];
	char	*src, *dst;
	char	src_path[MAXPATH], dst_path[MAXPATH];
	sccs	*s;
	ser_t	d;
	FILE	*f;
	char	*type = 0;
	char	**rav;
	Opts	*opts;
	longopt	lopts[] = {
		{ "subset;" , 's' },
		{ "standalone", 'S'},
		{ 0, 0 }
	};

	opts = new(Opts);
	opts->diff_style = "u";
	while ((c = getopt(ac, av, "ad|hkpt:Twvi:x:r:Ss;", lopts)) != -1) {
		switch (c) {
		    case 'a': opts->sysfiles = 1; break;
		    case 'v':	vflag = 1; break;		/* doc 2.0 */
		    case 'q':					/* undoc 2.0 */
				break; /* no op; for interface consistency */
		    case 'd':					/* doc 2.0 */
			opts->diff_style = notnull(optarg);
			break;
		    case 'h':					/* doc 2.0 */
			opts->hflag = 1; /*disable patch header*/
			break;
		    case 'k':	kflag = 1; break;		/* doc 2.0 */
		    case 'p':   opts->diff_style = "up"; break;
		    case 'r':	unless (opts->rev) {		/* doc 2.0 */
					opts->rev = optarg;
				} else {
					fprintf(stderr,
					    "bk export: only one -r allowed\n");
					usage();
				}
				break;
		    case 'S': opts->standalone = 1; break;
		    case 's':
			opts->aliases = addLine(opts->aliases, optarg);
			break;
		    case 't':	if (type) usage();		/* doc 2.0 */
				type = optarg;
				unless (streq(type, "patch") ||
				    streq(type, "plain")) {
					usage();
				}
				break;
		    case 'T':	opts->tflag = 1; break;		/* doc 2.0 */
		    case 'w':	wflag = 1; break;		/* doc 2.0 */
		    case 'i':					/* doc 3.0 */
			opts->includes = addLine(opts->includes, strdup(optarg));
			break;
		    case 'x':					/* doc 3.0 */
			opts->excludes = addLine(opts->excludes, strdup(optarg));
			break;
		    default: bk_badArg(c, av);
		}
	}
	unless (type) type = "plain";
	if (opts->standalone && opts->aliases) usage();
	if (streq(type, "patch")) return (export_patch(opts));

	count =  ac - optind;
	switch (count) {
	    case 1: src = "."; dst = av[optind]; break;
	    case 2: src = av[optind++]; dst = av[optind]; break;
	    default: usage();
	}

	unless (isdir_follow(src)) {
		fprintf(stderr,
		    "export: %s does not exist or is not a directory.\n", src);
		exit(1);
	}
	if (mkdirp(dst) != 0) {
		fprintf(stderr, "cannot mkdir %s\n", dst);
		exit(1);
	}
	fullname(dst, dst_path);
	chdir(src);
	rav = addLine(0, strdup("bk"));
	rav = addLine(rav, strdup("rset"));
	if (opts->standalone) rav = addLine(rav, strdup("-S"));
	rav = addLine(rav, aprintf("-Hhl%s", opts->rev ? opts->rev : "+"));
	if (opts->sysfiles) {
		rav = addLine(rav, strdup("-a"));
	} else {
		rav = addLine(rav, strdup("--hide-bk"));
	}
	if (opts->aliases) {
		EACH(opts->aliases) {
			rav = addLine(rav, aprintf("-s%s", opts->aliases[i]));
		}
	}
	rav = addLine(rav, 0);
	f = popenvp(rav+1, "r");
	assert(f);
	freeLines(rav, free);

	bk_nested2root(opts->standalone);
	fullname(".", src_path);

	while (fgets(buf, sizeof(buf), f)) {
		char	*rev, *t, *historic;
		int	flags = 0;
		struct	stat sb;
		char	output[MAXPATH];

		/*
		 * src/Notes/DAEMON|src/DAEMON|1.2
		 * ^                ^          ^
		 * buf              historic   rev
		 * buf is current path, historic is path as of 1.2
		 */

		chop(buf);
		rev = strrchr(buf, '|');
		assert(rev);
		*rev++ = 0;
		historic = strchr(buf, '|');
		assert(historic);
		*historic++ = 0;

		if (opts->excludes &&
		    match_globs(historic, opts->excludes, 0)) {
			continue;
		}
		if (opts->includes &&
		    !match_globs(historic, opts->includes, 0)) {
			continue;
		}

		sprintf(buf1, "%s/%s", src_path, buf);
		t = name2sccs(buf1);
		s = sccs_init(t, SILENT);
		free(t);
		assert(s && HASGRAPH(s));
		d = sccs_findrev(s, rev);
		assert(d);
		sprintf(output, "%s/%s", dst_path, historic);
		unless (vflag) flags |= SILENT;
		unless (kflag) flags |= GET_EXPAND;
		if (opts->tflag) flags |= GET_DTIME;
		flags |= GET_PERMS;
		mkdirf(output);
		if (sccs_get(s, rev, 0, 0, 0, flags, output, 0)) {
			fprintf(stderr, "cannot export to %s\n", output);
			ret = 1;
			sccs_free(s);
			goto out;
		}
		sccs_free(s);
		if (wflag && !lstat(output, &sb) && S_ISREG(sb.st_mode)) {
			chmod(output, sb.st_mode | S_IWUSR);
		}
	}
out:
	pclose(f);
	if (opts->aliases) freeLines(opts->aliases, 0);
	if (opts->includes) freeLines(opts->includes, free);
	if (opts->excludes) freeLines(opts->excludes, free);
	free(opts);
	return (ret);
}

private int
export_patch(Opts *opts)
{
	FILE	*f, *f1;
	int	i, status_rset, status_gpatch;
	char	**rav;
	char	buf[MAXLINE];

	rav = addLine(0, strdup("bk"));
	rav = addLine(rav, strdup("rset"));
	if (opts->standalone) rav = addLine(rav, strdup("-S"));
	rav = addLine(rav, aprintf("-Hhr%s", opts->rev ? opts->rev : "+"));
	if (opts->sysfiles) {
		rav = addLine(rav, strdup("-a"));
	} else {
		rav = addLine(rav, strdup("--hide-bk"));
	}
	if (opts->aliases) {
		EACH(opts->aliases) {
			rav = addLine(rav, aprintf("-s%s", opts->aliases[i]));
		}
	}
	rav = addLine(rav, 0);
	f = popenvp(rav+1, "r");
	freeLines(rav, free);

	bk_nested2root(opts->standalone);

	sprintf(buf, "bk gnupatch -d%s %s %s",
	    opts->diff_style, opts->hflag ? "-h" : "", opts->tflag ? "-T" : "");
	f1 = popen(buf, "w");
	while (fgets(buf, sizeof(buf), f)) {
		char	*fstart, *fend, *rev1;

		chop(buf);
		/*
		 * We want to match on any of the names and rset gives us
		 * <current>|<start>|<srev>|<end>|<erev>
		 */
		if (opts->includes || opts->excludes) {
			char	**names = splitLine(buf, "|", 0);
			int	skip = 0;

			if (opts->excludes && (    /* any of the 3 below */
			    match_globs(names[1], opts->excludes, 0) ||
			    match_globs(names[2], opts->excludes, 0) ||
			    match_globs(names[4], opts->excludes, 0))) {
				skip = 1;
			}
			if (opts->includes && (    /* any of the 3 below */
			    !match_globs(names[1], opts->includes, 1) ||
			    !match_globs(names[2], opts->includes, 1) ||
			    !match_globs(names[4], opts->includes, 1))) {
				skip = 1;
			}
			freeLines(names, free);
			if (skip) continue;
		}

		/*
		 * Skip BitKeeper/ files (but pass deletes..)
	  	 *
		 * Email Note from Wayne Scott:
		 * In general the only changes to files in the deleted
		 * directory are when they are moved to that directory. 
		 * This code requires that its starting location or the ending
		 * location must be outside of bitkeeper.  We don't need
		 * patches with changes for files that are deleted. We just
		 * need the patch to have the delete.
		 */
		fstart = strchr(buf, '|') + 1;
		rev1 = strchr(fstart, '|') + 1;
		fend = strchr(rev1, '|') + 1;
		fend[-1] = 0;
		if (strchr(rev1, '+')) {
			fprintf(stderr,
			    "export: Can't make patch of merge node\n");
			return (1);
		}
		fend[-1] = '|';
		if (strneq(fstart, "BitKeeper/", 10) &&
		    strneq(fend, "BitKeeper/", 10)) continue;
		fprintf(f1, "%s\n", buf);
	}
	status_rset = pclose(f);
	status_gpatch = pclose(f1);
	unless ((WIFEXITED(status_rset) && WEXITSTATUS(status_rset) == 0) &&
	    (WIFEXITED(status_gpatch) && WEXITSTATUS(status_gpatch) == 0)) {
		return (1);
	}
	return (0);
}
