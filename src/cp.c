/*
 * Copyright 2001-2003,2005-2008,2010-2013,2015-2016 BitMover, Inc
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
private int	cp(char *from, char *to, int force, int quiet);
private	int	doLink(project *p1, project *p2);
/*
 * cp - copy a file to another file,
 * removing the changeset marks,
 * generating new random bits,
 * and storing a new pathname.
 */
int
cp_main(int ac, char **av)
{
	int	force = 0;
	int	quiet = 0;
	int	c;

	while ((c = getopt(ac, av, "fq", 0)) != -1) {
		switch (c) {
		    case 'f': force = 1; break;
		    case 'q': quiet = 1; break;
		    default: bk_badArg(c, av);
		}
	}
	unless ((ac - optind) == 2) {
		close(0);	/* XXX - what is this for? */
		usage();
	}
	return (cp(av[optind], av[optind+1], force, quiet));
}

private int
cp(char *from, char *to, int force, int quiet)
{
	sccs	*s;
	ser_t	d;
	char	buf[100];
	char	*sfile, *gfile, *tmp;
	char	*oldroot = 0;
	FILE	*f = 0;
	int	err;

	assert(from && to);
	unless (proj_samerepo(from, to, 1) || force) {
		fprintf(stderr, "bk cp: must be in same repo or use cp -f.\n");
		return (1);
	}
	sfile = name2sccs(from);
	unless (s = sccs_init(sfile, INIT_MUSTEXIST)) {
		fprintf(stderr, "%s: %s: No such file\n", prog, from);
		return (1);
	}
	unless (HASGRAPH(s) && s->cksumok) {
err:		sccs_free(s);
		return (1);
	}
	free(sfile);
	sfile = name2sccs(to);
	mkdirf(sfile);
	gfile = sccs2name(sfile);
	if (exists(sfile)) {
		fprintf(stderr, "%s: file exists\n", sfile);
		goto err;
	}
	if (exists(gfile)) {
		fprintf(stderr, "%s: file exists\n", gfile);
		goto err;
	}
	if (BAM(s)) {
		char	*cmd;
		u64	todo = 0;
		project	*src = proj_init(from);
		project	*dest = proj_init(to);

		for (d = TREE(s); d <= TABLE(s); d++) {
			unless (HAS_BAMHASH(s, d)) continue;
			todo += ADDED(s, d);
		}
		cmd = aprintf("bk --cd='%s' -Bstdin sfio -qoB%s - |"
		    "bk --cd='%s' sfio -%siBb%s - ",
		    proj_root(src),
		    doLink(src, dest) ? "L" : "",
		    proj_root(dest),
		    quiet ? "q" : "", psize(todo));
		proj_free(src);
		proj_free(dest);
		f = popen(cmd, "w");
		free(cmd);
		unless (f) {
			perror("cp");
			goto err;
		}
		oldroot = sccs_prsbuf(s, d, PRS_FORCE, ":MD5KEY|1.0:");
	}

	/*
	 * This code assumes we have only one set of random bits on the 1.0
	 * delta.  XXX - need to fix this if/when we support grafting.
	 */
	randomBits(buf);
	if (HAS_RANDOM(s, TREE(s))) {
		assert(!streq(buf, RANDOM(s, TREE(s))));
	}
	RANDOM_SET(s, TREE(s), buf);

	/*
	 * Try using the new filename as the original filename.
	 * Only necessary in long/short key trees like BitKeeper.
	 */
	tmp = _relativeName(gfile, 0, 0, 0, 0);
	for (d = TREE(s); d <= TABLE(s); d++) {
		if (HAS_BAMHASH(s, d)) {
			sccs_prsdelta(s, d, PRS_FORCE, ":BAMHASH: :KEY: ", f);
			fputs(oldroot, f);
			sccs_setPath(s, d, tmp);
			sccs_prsdelta(s, d, (PRS_FORCE|PRS_LF),
			    " :BAMHASH: :KEY: :MD5KEY|1.0:", f);
		} else {
			sccs_setPath(s, d, tmp);
		}
	}
	sccs_clearbits(s, D_CSET);
	sccs_writeHere(s, sfile);
	sccs_newchksum(s);
	if (BAM(s)) {
		FREE(oldroot);
		if (pclose(f)) {
			perror("sfio");
			goto err;
		}
	}
	sccs_free(s);
	err = sys("bk", "-?BK_NO_REPO_LOCK=YES", "edit", "-q", to, SYS);
	unless (WIFEXITED(err) && WEXITSTATUS(err) == 0) return (1);
	putenv("_BK_MV_OK=1");
	tmp = aprintf("-ybk cp %s %s", from, to);
	err = sys("bk", "-?BK_NO_REPO_LOCK=YES", "delta",
	    quiet ? "-qf" : "-f", tmp, to, SYS);
	free(tmp);
	unless (WIFEXITED(err) && WEXITSTATUS(err) == 0) return (1);
	return (0);
}

private	int
doLink(project *p1, project *p2)
{
	char	from[MAXPATH], to[MAXPATH];

	/* test for lclone */
	sprintf(from, "%s/" CHANGESET, proj_root(p1));
	sprintf(to, "%s/BitKeeper/tmp/hardlink-test.%u",
	    proj_root(p2), getpid());
	if (exists(from) && !link(from, to)) {
		unlink(to);
		return (1);
	}
	return (0);
}
