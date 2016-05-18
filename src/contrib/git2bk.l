#!/usr/libexec/bitkeeper/gui/bin/tclsh -L

/*
 * Copyright 2015-2016 BitMover, Inc
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

/*
 * This is a little git to BK importer written in L.  It operates on a
 * git repo and creates BK history in place so the repo is both git and
 * bk (seems weird but you clone it when you are done to get a BK only
 * repo).
 *
 * Default is to import tagged csets only (the importer runs at about
 * 15 seconds/cset for the linux kernel and more than half of that is git).
 *
 * To make it run as fast as possible:
 * A) Run with /tmp and the repo in SSD or ramdisk
 * B) Run on the fastest CPU possible (4x 4Ghz is better than 12x 3Ghz)
 * C) More cores do help so if you have an 8 core 4Ghz machine, use that.
 *
 * Incremental imports
 *	Update the git repo with this syntax:
 *	git pull <url> master:master
 *	The master:master is needed to get the tags, without that no tags;
 *	adjust as necessary if you are doing a different branch.
 *
 *	Run the import again. An easy test is to run the import and then
 *	bk undo the last 20 csets, run the import again again.
 */

/*
 * TODO
 *	In the NTP tree an undo -fsaNTP_4_3_41 or 4_3_25 and then an import
 *	barfs.  Don't know why but it is repeatable.
 *
 *	This does not handle all git repos; when we tested it about 1/3 of
 *	them fail.  If anyone figures out why and has a fix, please send
 *	a patch.
 */

typedef struct {
	string	user;		// username who did the check in
	string	host;		// hostname where check in was done
	string	date;		// 2007-05-13 03:21:54 -0700
	string	cmts[];		// array of comments for the commit
} delta;

delta	log{string};		// cache of the entire log
int	imported{string};	// for --list-skipped
string	q = "-q";		// -v turns this off and makes bk/git noisy
string	host = "git2bk.arpa";
string	start;			// --start=rev
string	branch;			// so we can switch back to head
string	tagpats[];		// --tag=<regexp> - try and get these tags
string	tagskip[];		// --tagskip=<regexp> - try to not get these 
string	tags{string};		// tags{gitrev} = tagname
string	skip[];			// list of directories/files to not import
string	tmpdir = "/tmp";	// --tmp=/hfs/tmp
int	nospaces = 0;		// --strip-spaces removes leading spaces
int	n = 0, done = 0;
int	stride = 1;		// --stride=%d
int	debug = 0;
int	dryrun = 0;		// --dryrun
int	verify = 0;		// --verify compares plain exports
int	findcset = 0;		// --findcset, skip commits
				// bk -r _findcset -i -B  -t1
int	repack = 0;		// --repack=%d, repack that often
int	tagged = 1;		// do tagged only csets, override w/ --all
int	listskipped = 0;	// --list-skipped

int
main(_argused int ac, string av[])
{
	string	c, buf;
	int	rcs = 0;
	int	i, want;
	int	renamelimit;	// so we can restore it
	int	listrevs = 0;	// --list-revs: print the revs and exit
	string	stop;		// --stop=rev exits at that rev (testing)
	string	revs[];		// ordered list of revs for this branch
	string	parent;		// parent of the current cset
	string	tmp[];
	string	lopts[] = {
		"all",		// include all changesets, not just tags
		"branch:",	// override default of --branch=master
		"debug",	// like it says
		"dry-run",	// not sure if this is useful
		"dryrun",	// UNDOC: alias
		"findcset",	// UNDOC: experimental, don't use
		"help",		// this help
		"host:",	// override default --host=git2bk.arpa
		"list-revs",	// sort of like dryrun, list and exit
		"list-skipped",	// list all skipped changesets
		"rcs",		// UNDOC: support RCS keywords
		"repack:",	// UNDOC: experimental, don't use
		"skip:",	// --skip=skip_this_file, can repeat
		"start:",	// UNDOC: experimental, don't use
		"stop:",	// UNDOC: ditto
		"stride:",	// do every Nth cset
		"strip-spaces",	// UNDOC: I dunno what this is, some Wayne thing?
		"tag:",		// wanted tag pattern (regexp, can repeat)
		"tagskip:",	// unwanted tag pattern (regexp, can repeat)
		"tmp:",		// override /tmp
		"verify|",	// --verify[=%d] verify every %d, default 1
		};


	branch = "master";

	while (c = getopt(av, "dv", lopts)) {
		switch (c) {
		    case "all":
		    	tagged = 0;
			break;
		    case "branch":
			branch = optarg;
			break;
		    case "d":
		    case "debug":
			debug++;
			break;
		    case "dryrun":
		    case "dry-run":
			dryrun = 1;
			break;
		    case "findcset":
			findcset = 1;
			break;
		    case "help":
		    	help();
			exit(0);
		    case "host":
			host = optarg;
			break;
		    case "list-revs":
			listrevs = 1;
			break;
		    case "list-skipped":
			listskipped = 1;
			break;
		    case "rcs":
			rcs = 1;
			break;
		    case "repack":
			repack = (int)optarg;
			break;
		    case "skip":
			push(&skip, optarg);
			break;
		    case "start":
			start = optarg;
			break;
		    case "stop":
			stop = optarg;
			break;
		    case "stride":
			stride = (int)optarg;
			break;
		    case "strip-spaces":
			nospaces = 1;
			break;
		    case "tag":
			push(&tagpats, optarg);
			break;
		    case "tagskip":
			push(&tagskip, optarg);
			break;
		    case "tmp":
			tmpdir = optarg;
			break;
		    case "v": q = ""; break;
		    case "verify":
			verify = 1;
			if (defined(optarg)) verify = (int)optarg;
			break;
		    default: die("bad optarg");
		}
	}
	/* git may have these if we are coming from a bk->GIT tree */
	push(&skip, "BitKeeper/etc/config");
	push(&skip, "BitKeeper/etc/ignore");
	push(&skip, "BitKeeper/etc/gone");

	if (listskipped && (tagged || (stride > 1))) {
		die("listskipped needs --all and not --stride>1");
	}
	if (tagged && (stride > 1)) die("--tagged or --stride but not both");

	if (av[optind]) chdir(av[optind]);

	unless (isdir(".git")) die("not in a git repo.\n");
	if (exists(".gitmodules")) die("submodule imports not supported.\n");

	versions();

	// restored below
	renamelimit = (int)`get config --get diff.renameLimit`;
	sys("git config diff.renameLimit 5000");
	sys("git checkout -f -q ${branch} --");

	fprintf(stderr, "### Importing branch: %s in %s ###\n", branch, `pwd`);
	revs = longest_path(&parent);
	if (tagged && (length(tags) <= 0)) {
		    fprintf(stderr, "No tagged revisions found. Exiting.\n");
		    exit(1);
	}
	for (i = 0; defined(revs[i]); i++) {
		if (tagged && !tags{revs[i]}) continue;
		want = 1;
		foreach (buf in tagskip) {
			if (tags{revs[i]} =~ /${buf}/) {
				if (debug) warn("Skipping %s\n", tags{revs[i]});
				want = 0;
				break;
			}
		}
		unless (want) continue;
		push(&tmp, revs[i]);
		n++;
		if (listrevs) {
			warn("%s %s\n",
			    revs[i], tags{revs[i]} ? tags{revs[i]} : "");
		}
		if (stop && (revs[i] == stop)) break;
	}
	if (tagged) {
		if (debug) warn("%d => %d tagged\n", length(revs), length(tmp));
		revs = tmp;
	}
	if (listrevs) exit(0);

	if (setup(revs[0], rcs)) die("unable to complete setup");

	for (i = 0; defined(revs[i]); i += stride) {
		if ((i > 0) && tagged && !tags{revs[i]}) continue;
		assert(cset(parent, revs[i]) == 0);
		if (repack && !(done % repack) && !dryrun) {
			sys("bk -?_BK_FORCE_REPACK=YES repocheck");
		}
		if (stop && (revs[i] == stop)) break;
		parent = revs[i];
	}
	unless (dryrun) sys("bk repocheck");
	sys("git checkout -f -q ${branch} --");	// restores HEAD
	if (listskipped) list_skipped();
	if (renamelimit > 0) sys("git config diff.renameLimit ${renamelimit}");
	return (0);
}

void
help(void)
{
	string	header, c, help, buf;
	string	script = "${`bk bin`}/contrib/git2bk.l";
	FILE	f = fopen(script, "r");
	int	in_header = 0, in_opts = 0;

	fprintf(stderr, "usage: bk little `bk bin`/contrib/git2bk [options] path/to/git/repo\n");
	fprintf(stderr, "\nOptions:\n\t");
	while (buf = <f>) {
		switch (in_header) {
		    case 0:
			if (buf =~ m|/\*|) in_header = 1;
			break;
		    case 1:
			if (buf =~ m|\*/|) {
				in_header = -1;
			} else {
				buf =~ s|^ \*||;
				buf =~ s|^ ||;
				header .= buf;
				header .= "\n";
			}
			break;
		}

		unless (in_opts) {
			if (buf =~ /string\s+lopts\[\] = {/) in_opts = 1;
			continue;
		}
		if (buf =~ /^\s+};/) break;
		/* "branch:",	// override default of --branch=master */
		if (buf =~ /UNDOC:/) continue;
		unless (buf =~ m|\s+"([^"]+)".*// (.*)|) continue;
		c = $1;
		help = $2;
		if (c =~ /(.*)[:;]$/) {
			c = $1;
			buf = format("--%s=<arg> ", c);
		} else if (c =~ /(.*)\|$/) {
			c = $1;
			buf = format("--%s[=<arg>] ", c);
		} else {
			buf = format("--%s ", c);
		}
		fprintf(stderr, "%-20s // %s \n\t", buf, help);
	}
	fprintf(stderr, "\n%s", header);
	fprintf(stderr,
	    "\nNote: this is community contributed unsupported software.\n"
	    "Source is in ${`bk bin`}/contrib/git2bk.l\n"
	    "Please send any patches to dev@bitkeeper.com.\n");
	exit(0);

}

/*
 * Make sure we have a BK that does the right thing 
 * Make sure we have a git that works.
 */
void
versions(void)
{
	string	git = `git --version`;

	unless (defined(git)) {
		fprintf(stderr, "No git installed?\n");
		exit(1);
	}
	/* anything 2.x or later should be fine */
	unless (git =~ /version [23456789]/) {
		git =~ /version 1\.(\d+)\./;
		/* 1.8 and later should be fine */
		unless ((int)$1 > 7) {
			unless (git =~ /version 1\.\d+\.(\d+)/) {
				fprintf(stderr, "Unable to parse: %s\n", git);
				exit(1);
			}
			// I don't remember why I needed this one, I think
			// it had to do with longest path.  ob might know.
			unless ((int)$1 >= 4) {
				fprintf(stderr, "git 1.7.4 or later needed.\n");
				exit(1);
			}
		}
	}
}

/*
 * Create an empty bk repo and the intial git repo
 * We want to end up with .bk next to .git
 */
int
setup(string start_rev, int rcs)
{
	FILE	f;
	int	i;

	if (dryrun) return (0);

	if (isdir(".bk")) {
		env();
		system("bk repocheck -q");	// fixes checkout:edit
		sys("bk repocheck");		// has to work
		return (0);
	}

	/*
	 * Set up a repo inside the git repo
	 * Wayne wants the date to be the first cset, how do I get that?
	 */
	if (!defined(start_rev) || (start_rev == "")) {
		fprintf(stderr, "FAILED with start_rev emtpy\n");
		return (1);
	}
	putenv("BK_DATE_TIME_ZONE=1970-01-01 01:00:00-0");
	putenv("BK_USER=git2bk");
	putenv("BK_HOST=${host}");
	putenv("_BK_NO_UNIQ=1");
	putenv("BK_RANDOM=%s", sprintf("%.16s", start_rev));
	f = fopen(".bk_config", "w");
	fprintf(f, "checkout:edit\n");
	fprintf(f, "clock_skew:on\n");
	fprintf(f, "partial_check:on\n");
	fprintf(f, "compression:gzip\n");
	if (rcs) fprintf(f, "keyword:rcs\n");
	fclose(f);

	/*
	 * Since we're running in a git tree we can't init "." so create
	 * a subrepo called .bk_empty and then plop it on top of the git repo.
	 */
	unless (sys("bk setup -f -c.bk_config .bk_empty") == 0) {
		return (1);
	}
	sys("tar -C.bk_empty -cf- . | tar -xf-");
	for (i = 0; i < 10; ++i) {
		if (system("rm -rf .bk_config .bk_empty") == 0) break;
		sleep(1);
	}

	env();

	/*
	 * would 'touch .git/.bk_skip' be better than adding a cset to
	 * the user's data?
	 *
	 * Also a lot of other code would be simplified if you would
	 * also tell git to ignore BitKeeper and .bk.
	 */

	// tell bk to ignore git's data
	f = fopen(".git/.bk_skip", "w");
	fclose(f);

	// tell git to ignore bk's data
	f = fopen(".git/info/exclude", "w");
	fprintf(f, "BitKeeper/\n");
	fprintf(f, ".bk/\n");
	fclose(f);

	return (0);
}

void
env(void)
{
	putenv("BK_CONFIG=clock_skew:2!;compression:none!;checkout:edit!");
}

/*
 * Import a GIT commit.
 * We get the updates, then 
 * - for each file that is not checked out, git deleted it so we delete it
 * - for each modified/extra we check those in with the comment/user/date
 *   from the log message.
 */
int
cset(string parent, string rev)
{
	FILE	f;
	string	buf, tmp, out, out2, err;
	string	rm[];
	int	rc, do_commit = 0;

	if (++done > n/stride) return (0);

	imported{rev} = 1;
	fprintf(stderr, "### GIT ${rev} %d/%d ###\n", done, n/stride);
	if (dryrun && tags{rev}) fprintf(stderr, "Tag: %s\n", tags{rev});
	getlog(rev);

	if (dryrun) return (0);

	// In case this is a BK tree, we don't want git's idea of the BK files
	unlink("BitKeeper/etc/config");
	unlink("BitKeeper/etc/ignore");
	unlink("BitKeeper/etc/gone");
	// Do a -f since git doesn't seem to want to checkout on top of a
	// touched but unchanged file without it.  We get these after a
	// rename because we checkout the file.
	unless (sys("git checkout -q -f ${rev} --") == 0) return (1);
	foreach (buf in skip) system("rm -rf ./${buf}");

	tmp = "BitKeeper/tmp/comments";
	f = fopen(tmp, "w");
	foreach (buf in log{rev}.cmts) {
		fprintf(f, "%s\n", buf);
	}
	// Needed so findcset will clump nicely.
	if (findcset) fprintf(f, "GIT: %s\n", rev);
	fclose(f);

	/* tell BK to cons up random bits from the rest of the key */
	putenv("BK_RANDOM=cons");
	putenv("BK_USER=%s", log{rev}.user);
	putenv("BK_HOST=%s", defined(log{rev}.host) ? log{rev}.host : host);
	putenv("BK_DATE_TIME_ZONE=%s", log{rev}.date);
	if (parent) {
		types();
		renames(parent, rev);
	}
	system("bk -U^G", undef, &rm, undef);
	rmExtras(rm);
	system("bk -axcU", undef, "BitKeeper/tmp/list", undef);
	if (size("BitKeeper/tmp/list") > 0) {
		do_delta("cat BitKeeper/tmp/list | bk -j ci ${q} -alY${tmp} -");
		do_commit = 1;
	} else {
		/*
		 * This catches a cset that has nothing in it except deletes.
		 */
		system("bk pending", undef, "BitKeeper/tmp/list", undef);
		if (size("BitKeeper/tmp/list") > 0) {
			do_commit = 1;
		}
	}
	unless (do_commit) {
		warn("Nothing to commit:\n");
		system("git log -1 ${rev} | cat");
	}
	if (do_commit && !findcset) {
		// Add the git hash to the cset comments only.
		f = fopen(tmp, "a");
		fprintf(f, "GIT: %s\n", rev);
		fclose(f);
		sys("bk commit ${q} -Y${tmp}");
	}
	unlink("BitKeeper/tmp/list");
	if (tags{rev} && !findcset) {
		// fix up bad tags
		if (tags{rev} =~ /^[0-9]/) tags{rev} = "git_${tags{rev}}";
		if (tags{rev} =~ m|/|) tags{rev} =~ s|/|.|g;
		sys("bk tag " . tags{rev});
	}
	unless (verify) return (0);

	/*
	 * Make sure we are in sync with Git.
	 */
	system("git status -s", undef, &out, &err);
	out .= err;
	err = undef;
	foreach (buf in split(/\n/, out)) {
		if (buf =~ m|^\?\? \.bk/$|) continue;
		if (buf =~ m|^\?\? BitKeeper/|) continue;
		if (buf =~ m| D BitKeeper/|) continue;
		if (buf =~ /^ M (.*)/) {
			rc = system("git diff --ignore-space-at-eol '${$1}'",
				    undef, &out2, undef);
			unless (rc) continue;
		}
		err .= buf . "\n";
	}
	if (defined(err)) die("GIT: %s", err);

	/*
	 * Make sure we are in sync with BK
	 */
	system("bk -cxgr", undef, &out, &err);
	out .= err;
	if (length(out) > 0) {
		/*
		 * Tcl has a binhex file that switches \n to \r, clean it.
		 */
		foreach (buf in split(/\n/, out)) {
			system("bk clean ${buf}");
			system("bk edit -q ${buf}");
		}
		system("bk -cxgr", undef, &out, &err);
		out .= err;
		if (length(out) > 0) die("BK: %s", out);
	}

	if (verify && !(done % verify)) {
		/* Be really paranoid and compare the plain trees */
		string	bkplain = sprintf("${tmpdir}/bk-plain.%d", pid());
		string	gitplain = sprintf("${tmpdir}/git-plain.%d", pid());
		string	bkfiles[], gitfiles[];
		int	found{string};
		int	i;
		string	savedir = pwd();

		fprintf(stderr, "### VERIFYING ${rev[0..5]} ###\n");
		system("rm -rf ${bkplain} ${gitplain}");
		system("bk export -tplain -kr+ ${bkplain}");
		cd(bkplain);
		system("find . -type f", undef, &bkfiles, undef);
		if (mkdir(gitplain)) die("could not mkdir ${gitplain}");
		cd(gitplain);
		putenv("GIT_DIR=${savedir}/.git");
		system("git checkout -qf ${rev} --");
		unset("::env(GIT_DIR)");
		system("find . -type f", undef, &gitfiles, undef);
		for (i = 0; defined(bkfiles[i]); i++) {
			cmp("${gitplain}/${bkfiles[i]}", 
			    "${bkplain}/${bkfiles[i]}");
			found{bkfiles[i]} = 1;
		}
		for (i = 0; defined(gitfiles[i]); i++) {
			if (found{gitfiles[i]}) continue;
			cmp("${gitplain}/${gitfiles[i]}", 
			    "${bkplain}/${gitfiles[i]}");
			found{gitfiles[i]} = 1;
		}
		for (i = 0; defined(bkfiles[i]); i++) {
			unless (found{bkfiles[i]}) {
				die("only in bk ${bkfiles[i]}");
			}
		}
		for (i = 0; defined(gitfiles[i]); i++) {
			unless (found{gitfiles[i]}) {
				die("only in git ${bkfiles[i]}");
			}
		}
		cd(savedir);
	}
	return (0);
}

void
cmp(string git, string bk)
{
	string	bkbuf, gitbuf;
	FILE	bkf, gitf;

	unless (gitf = fopen(git, "r")) die("git file ${git} not found");
	fconfigure(gitf, translation: "auto");
	unless (bkf = fopen("${bk}", "r")) die("bk file ${bk} not found");
	fconfigure(bkf, translation: "auto");
	read(bkf, &bkbuf);
	read(gitf, &gitbuf);
	unless (bkbuf == gitbuf) die("DIFF: %s %s\n", bk, git);
	fclose(gitf);
	fclose(bkf);
}

/*
 * Load up the log, we'll use it for our commits.
 * commit 97ed77243efe3d2baccbe1bdcbdcb84efb16781e
 * Author: kennykb <>
 * Date:   Wed Dec 1 16:42:38 2010 +0000
 *
 *     merge
 *
 * etc.
 */
void
getlog(string rev)
{
	FILE	f;
	string	cmts[];
	string	buf, c, buf2;
	string	strip = undef;

	f = popen("git log -n 1 ${rev}", "r");
	unless (buf = <f>) die("git log ${rev}");
	unless (buf =~ /^commit /) die(buf);
	buf = <f>;
	if (buf =~ /^Merge:/) buf = <f>;
	if (buf =~ /Author: .* <(.*)@(.*)>/) {
		log{rev}.host = $2;
	} else if (buf =~
	    /Author: .*([a-zA-Z0-9._-]+)[ -_][aA][tT][ -_]([a-zA-Z0-9.-]+)>/) {
		log{rev}.host = $2;
	} else if (buf =~ /Author: ([a-zA-Z0-9._-]+) <>/) {
		log{rev}.host = host;
	} else if (buf =~ /Author: .* <(.*)>/) {
		log{rev}.host = host;
	} else if (buf =~ /Author: <(.*)>/) {
		log{rev}.host = host;
	} else {
		log{rev}.host = host;
		$1 = "nobody";
	}
	log{rev}.user = $1;

	// Author:  <gerstung/heiko@pogo.udel.edu/laptop-heiko.py.meinberg.de>
	if (log{rev}.user =~ m|(.*)/.*|) {
		log{rev}.user = $1;
	}
	if (log{rev}.host =~ m|(.*)/.*|) {
		log{rev}.host = $1;
	}

	// cjolley@394f415c-e224-0410-a11c-cb241aa5d150?  WTF?
	unless (log{rev}.host =~ /\./) log{rev}.host = host;

	// Author: Eric Sesterhenn <[mailto:snakebyte@gmx.de]>
	if (log{rev}.host =~ /\[mailto:(.*)\]/) {
		log{rev}.host = $1;
		log{rev}.host =~ s/^\s*//;
		log{rev}.host =~ s/\s*$//;
	}

	// sane: bad host name: "xayide.techfak.uni-bielefeld.de[ro]".
	if (log{rev}.host =~ /(.*)\[.*\]/) {
		log{rev}.host = $1;
	}

	buf = <f>;
	unless (buf =~ /^Date:\s+(.*)/) {
		die("expected rev/date: ${buf}\n");
	}
	log{rev}.date = date($1);
	buf = <f>;	// toss the blank line
	undef(cmts);	// toss previous comments
	while (buf = <f>) {
		if (nospaces) {
			unless (strip) {
				buf =~ /^(\s+)/;
				strip = $1;
			}
			buf =~ s/^${strip}//;
		} else {
			buf =~ s/^    //;
		}
		/*
		 * Wayne says he needed this in his importer,
		 * it's translating all the control chars.
		 * 077 is ?
		 */
		if (buf =~ /[\001-\010\013-\037]/) {
			buf2 = "";
			foreach (c in buf) {
				if (c =~ /[\001-\010\013-\037]/) {
					c = sprintf("\\%03o", ord(c));
				}
				buf2 .= c;
			}
			buf = buf2;
		}
		push(&cmts, buf);
	}
	pclose(f);

	/*
	 * Lose trailing blank lines, they serve no purpose.
	 * But bk doesn't like empty comments files so add something.
	 */
	while (cmts[END] =~ /^\s*$/) pop(&cmts);
	unless (defined(cmts[0])) cmts[0] = "(no comments)";
	log{rev}.cmts = cmts;
}

/*
 * Git does: Wed Jun 17 18:19:11 1998
 * 1998-01-11 20:00:00-08          (RCS -zLT)
 */
string m2d{string} = {
    "Jan" => "01",
    "Feb" => "02",
    "Mar" => "03",
    "Apr" => "04",
    "May" => "05",
    "Jun" => "06",
    "Jul" => "07",
    "Aug" => "08",
    "Sep" => "09",
    "Oct" => "10",
    "Nov" => "11",
    "Dec" => "12",
};

string
date(string git)
{
		// Wed Jun 17 18:19:11 1998 +0000
		//     $1  $2 $3       $4   $5
	unless (git =~ /\w+ (\w+) (\d+) ([0-9:]+) (\d+)\s+(.*)/) die(git);
	return (sprintf("%d-%s-%02d %s\n", (int)$4, m2d{$1}, (int)$2, $3, $5));
}

int
sys(string command)
{
	int	rc = system(command);

	if (rc || debug) fprintf(stderr, "%s = %d\n", command, rc);
	if (rc) {
		warn("Caller: %s\n", caller(1));
		exit(rc);
	}
	return (rc);
}

/*
 * For every file under BK control, see if it changed types.  If it did,
 * rm the old file, we'll let the rest of the code add stuff as needed.
 */
void
types(void)
{
	string	buf, file, dir;
	string	err[], fix[];

	system("bk -U log -r+ -nd:I:", undef, "/dev/null", &err);
	foreach (buf in err) {
		if (debug) fprintf(stderr, "TYPES: %s\n", buf);
		if (buf =~ m|(.*) has different file types, treating this file as read only.$|) {
			if (debug) fprintf(stderr, "MATCH: %s\n", $1);
			push(&fix, $1);
		}
		if (buf =~ m|unsupported file type: .* \((.*)\) \d+$|) {
			if (debug) fprintf(stderr, "MATCH: %s\n", $1);
			push(&fix, $1);
		}
	}
	foreach (file in fix) {
		sys("mv '${file}' .bk-save0");
		sys("bk rm '${file}'");
		dir = dirname(file);
		if ((length(dir) > 0) && !isdir(dir)) mkdir(dir);
		sys("mv .bk-save0 '${file}'");
	}
}

/*
 * Find renames from the git diff-tree output and do them in the bk tree.
 * This logic is taken from Wayne's bk-git-import perl script.
 */
void
renames(string fromRev, string toRev)
{
	FILE	f;
	string	cmd, s;
	int	first = 1;

	cmd = "git diff-tree -r -M ${fromRev} ${toRev}";
	unless (defined(f = popen(cmd, "r"))) die(cmd);
	while (defined(s = <f>)) {
		if (s =~ /^:(\d+) (\d+) \S+ \S+ ([R])\d*\t([^\t]+)(\t(.+))?$/) {
			if (($4 =~ m|^BitKeeper/etc/|) || 
			    ($6 =~ m|^BitKeeper/etc/|) ||
			    ($4 =~ m|^BitKeeper/triggers/|) ||
			    ($6 =~ m|^BitKeeper/triggers/|)) {
			    	continue;
			}
			if (first && debug) {
				warn("%s\n", cmd);
				first = 0;
			}
			if ($1 != $2) warn("TYPES: %s %s\n", $1, $2);
			if (debug) warn("%s\n", s);
			sys("rm -f '${$6}'");
			sys("bk mv '${$4}' '${$6}'");
			sys("bk edit -q '${$6}'");
		        sys("git checkout -f ${toRev} -- '${$6}'");
		}
	}
	pclose(f);
}

/*
 * If the bk rm -f fails, check for errors like
 *    unsupported file type: SCCS/s.file1 (file1) 0040775
 * which occurs when a regular file or symlink is replaced
 * by a directory without telling bk about it.  Move the
 * directory out of the way, bk rm the original file, then
 * move the directory back.
 */
int
rmExtras(string files[])
{
	string	file, sdir, dir, err, errs[];
	int	rc;

	unless (files) return (0);

	foreach (file in files) {
		if (file =~ m|(.*)/(.[^/]+)$|) {
			sdir = ".bk/" . $1 . "/SCCS";
		} else {
			sdir = ".bk/SCCS";
		}
		if (debug > 1) {
			if (exists(file)) sys("ls -l ${file}");
			sys("ls -l ${sdir}");
		}
		rc = system("bk rm -f ${file}", undef, undef, &errs);
		if (debug) fprintf(stderr, "rm %s = %d\n", file, rc);
		unless (rc) continue;
		foreach (err in errs) {
			if (debug) fprintf(stderr, "RM ERR: %s\n", err);
			if (err =~ /unsupported file type: .* \((.*)\) [0-9]+/){
				file = $1;
				sys("mv '${file}' .bk-save1");
				sys("bk rm '${file}'");
				dir = dirname(file);
				if ((length(dir) > 0) && !isdir(dir)) {
					mkdir(dir);
				}
				sys("mv .bk-save1 '${file}'");
			}
		}
	}
	return (sys("bk -U^G rm -f"));
}

/*
 * If the check-in fails due to an error like this
 *    file1 has different file types, treating this file as read only
 * or
 *    win/rc/SCCS/s.cursor8a.cur: file format is ascii, delta is binary.
 * because, for example, git changed a regular file to a symlink,
 * bk rm the file and re-add it.
 */
int
do_delta(string command)
{
	string	file, dir, err, errs[];
	int	did_it{string};
	int	rc = system(command, undef, undef, &errs);

	if (debug) fprintf(stderr, "%s = %d\n", command, rc);
	unless (rc) return (0);

	foreach (err in errs) {
		if ((err =~ /(.*) has different file types/) ||
		    (err =~ /(.*): file format is ascii, delta is binary./)) {
			file = $1;
			if (file =~ m|(.*)SCCS/s.(.*)|) file = $1 . $2;
			if (did_it{file}) continue;
			sys("mv '${file}' .bk-save2");
			sys("bk rm '${file}'");
			dir = file;
			dir = dirname(file);
			if ((length(dir) > 0) && !isdir(dir)) mkdir(dir);
			sys("mv .bk-save2 '${file}'");
			sys("bk new -q '${file}'");
			did_it{file} = 1;
		}
	}
	return (sys(command));
}

/*
 * Given a GIT repository, calculate the longest path through the graph.
 * If tags are given, then weight paths containing the tags more heavily
 * so as to include as many tags as possible.
 */
string	node;
string[] edges{string};
string[] nodes;
int	dist{string};
string	previous{string};
int	weight{string};

string[]
longest_path(string &parent)
{
	FILE    f;
	string  range, buf, t, tag;
	string	maxnode, v, w, edg;
	string	inc = "";
	string  revs[];
	int	maxpath = 0;
	int	want;

	f = popen("git show-ref --tags --dereference", "r");
	while (buf = <f>) {
		want = 1;
		buf =~ m|refs/tags/(.*)|;
		tag = $1;
		tag =~ s/{}$//;
		tag =~ s/\^$//;
		if (tagpats[0]) {
			want = 0;
			foreach (t in tagpats) {
				if (tag =~ /${t}/) {
					want = 1;
					break;
				}
			}
		}
		if (want) {
			revs = split(buf);
			tags{revs[0]} = tag;
		}
	}
	if (debug) warn("%d tags\n", length(tags));
	pclose(f);
	if (start) {
		range = "${start}..${branch}";
	} else {
		range = "${branch}";
	}

	/*
	 * If BK is here already, we're doing incremental, go find the last
	 * rev converted.
	 * Stuff it in parent, the rename logic wants that.
	 * XXX - --start is overridden by this.
	 */
	if (exists(".bk")) {
		parent = `bk changes -r+ | grep GIT:`;
		if (debug) fprintf(stderr, "PREV: %s\n", parent);
		parent =~ s/\s*GIT: //;
		range = "${parent}..${branch}";
		inc = "--ancestry-path";
	}
	if (debug) fprintf(stderr, "RANGE: %s\n", range);
	f = popen("git rev-list --reverse "
	    "--topo-order --parents ${inc} ${range} --", "r");
	while (buf = <f>) {
		revs = split(buf);
		push(&nodes, revs[0]);
		dist{revs[0]} = 0;
		if (tags{revs[0]}) {
			/*
			 * Weight assigned to tags, basically how many
			 * more revs there need to be in another path
			 * before we are willing to sacrifice this
			 * tagged path.
			 */
			weight{revs[0]} = 100;
		} else {
			weight{revs[0]} = 1;
		}
		foreach (node in revs[1..END]) {
			push(&edges{node}, revs[0]);
		}
	}
	pclose(f);

	// longest path
	maxpath = 0;
	foreach (node in nodes) {
		v = node;
		foreach (edg in edges{node}) {
			w = edg;
			if (dist{w} <= dist{v} + weight{w}) {
				dist{w} = dist{v} + weight{w};
				previous{w} = v;
			}
			if (dist{w} > maxpath) {
				maxpath = dist{w};
				maxnode = w;
			}
		}
	}
	
	undef(revs);
	node = maxnode;
	while (node) {
		push(&revs, node);
		node = previous{node};
	}
	if (debug) warn("%d in longest path\n", length(revs));
	return (lreverse(revs));
}

void
list_skipped(void)
{
	FILE	f = popen("git log | grep '^commit '", "r");
	string	buf;
	string	skipped[];

	while (buf = <f>) {
		buf =~ /^commit (.*)/;
		unless (defined(imported{$1})) {
			push(&skipped, $1);
		}
	}
	fclose(f);

	// This is really ugly but I don't know how to feed git a list of revs
	foreach (buf in skipped) {
		system("git --no-pager log -1 ${buf}");
	}
}

