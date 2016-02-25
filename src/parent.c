/*
 * Copyright 2000-2006,2008-2010,2013,2015-2016 BitMover, Inc
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

#include "bkd.h"

#undef	PARENT
#define	PARENT		"BitKeeper/log/parent"
#define	PUSH_PARENT	"BitKeeper/log/push-parent"
#define	PULL_PARENT	"BitKeeper/log/pull-parent"

private	void	add(char *which, char *url, int *rc);
private	void	rm(char *which, char *url, int *rc);
private	char	**readf(char *file);
private	int	record(void);
private char	**parent_pullp_keep_rel(void);
private char	**parent_pushp_keep_rel(void);
private	int	print(void);
private	char	*normalize(char *url, int check);

private	struct {
	u32	add:1;			/* add parent[s] */
	u32	annotate:1;		/* annotate w/ parent type */
	u32	normalize:1;		/* normalize URL */
	u32	one:1;			/* print the first parent only */
	u32	print:1;		/* print the parent pointer[s] */
	u32	pullonly:1;		/* pull parents only */
	u32	pushonly:1;		/* push parents only */
	u32	quiet:1;		/* Don't list changes */
	u32	rm:1;			/* remove parent[s] */
	u32	set:1;			/* set parents absolutely */

	MDBM	*parents;		/* $parents{url} = i|o|b */
	char	**mods;			/* changes are recorded here */
} opts;

int
parent_main(int ac,  char **av)
{
	int	c, changed_dir, rc = 0;
	int	i, printnormal = 0;
	char	*rootdir, **pull, **push, *which, *p;

	opts.normalize = 1;
	opts.annotate = 1;
	/*
	 * Add support for an alternative interface:
	 * bk parent add [-inoq] <repository> [<repository>]
	 * bk parent rm [-ioq] <repository> [<repository>]
	 * bk parent set [-inoq] <repository> [<repository>]
	 * bk parent list [-1ioq]
	 */
	if (av[1] && (av[1][0] != '-') && (!isdir(av[1]))) {
		if (streq(av[1], "add")) opts.add = 1;
		if (streq(av[1], "rm")) opts.rm = 1;
		if (streq(av[1], "remove")) opts.rm = 1;
		if (streq(av[1], "delete")) opts.rm = 1;
		if (streq(av[1], "set")) opts.set = 1;
		if (streq(av[1], "list")) opts.annotate = 0;
		if (opts.add || opts.rm || opts.set || !opts.annotate) {
			av[1] = av[0];
			av++;
			ac--;
		}
	}
	while ((c = getopt(ac, av, "1ailNnopqrs", 0)) != -1) {
		switch (c) {
		    case '1': opts.one = 1; break;
		    case 'a': opts.add = 1; break;
		    case 'i': opts.pushonly = 0; opts.pullonly = 1; break;
		    case 'p': /* compat, fall through, don't doc */
		    case 'l': opts.annotate = 0; break;
		    case 'N': printnormal = 1; break;
		    case 'n': opts.normalize = 0; break;
		    case 'o': opts.pushonly = 1; opts.pullonly = 0; break;
		    case 'q': opts.quiet = 1; break;
		    case 'r': opts.rm = 1; break;
		    case 's': opts.set = 1; break;
		    default: bk_badArg(c, av);
		}
	}
	if (printnormal) {
		for (i = optind; i < ac; i++) {
			if (streq(av[i], "-")) {
				unless (i == (ac - 1)) {
					fprintf(stderr,
					    "parent: '-' must be last\n");
					exit (1);
				}
			}
		}
		i = optind;
		while (which =
		    (av[i] && streq(av[i], "-")) ? fgetline(stdin) : av[i++]) {
			unless (p = normalize(which, 1)) return (1);
			puts(p);
			free(p);
		}
		return (0);
	}

	if (proj_cd2product() && proj_cd2root()) {
		fprintf(stderr, "parent: cannot find package root.\n");
		return (1);
	}
	rootdir = proj_cwd();
	changed_dir = strcmp(start_cwd, rootdir);

	if (!opts.normalize && changed_dir) {
		fprintf(stderr, "parent: -n legal only from package root\n");
		return (1);
	}

	opts.parents = mdbm_mem();
	pull = parent_pullp_keep_rel();
	EACH(pull) mdbm_store_str(opts.parents, pull[i], "i", MDBM_INSERT);
	push = parent_pushp_keep_rel();
	EACH(push) {
		if (mdbm_fetch_str(opts.parents, push[i])) {
			mdbm_store_str(opts.parents, push[i], "b",MDBM_REPLACE);
		} else {
			mdbm_store_str(opts.parents, push[i], "o", MDBM_INSERT);
		}
	}

	/*
	 * Exactly one arg with either no parents or a single
	 * pull/push parent implies -s.
	 */
	unless (opts.add || opts.rm || opts.print || opts.set) {
		if (av[optind] && !av[optind+1]) {
			if (sameLines(pull, push) && (nLines(pull) == 1)) {
				opts.set = 1;
			}
			unless (nLines(pull) || nLines(push)) opts.set = 1;
		}
	}

	/* No options means print */
	unless (opts.set || opts.add || opts.rm) opts.print = 1;

	freeLines(pull, free);
	freeLines(push, free);

	if ((opts.rm + opts.print + opts.add + opts.set) > 1) {
		ttyprintf("rm=%u print=%u add=%u set=%u\n",
		    opts.rm, opts.print, opts.add, opts.set);
		usage();
	}
	if (opts.print && av[optind]) usage();

	/* Print */
	if (opts.print) {
		rc = print();
		goto out;
	}

	if (opts.pullonly) {
		which = "i";
	} else if (opts.pushonly) {
		which = "o";
	} else {
		which = "b";
	}

	/* remove */
	if (opts.rm) {
		if (av[optind]) {
			while (av[optind]) {
				rm(which, av[optind], &rc);
				optind++;
			}
		} else {
			mdbm_close(opts.parents);
			opts.parents = 0;
		}
		unless (rc) rc = record();
		goto out;
	}

	if (opts.set) {
		mdbm_close(opts.parents);
		opts.parents = mdbm_mem();
	}

	/* set/add */
	unless (av[optind]) usage();
	while (av[optind]) {
		add(which, av[optind], &rc);
		optind++;
	}
	unless (rc) rc = record();

out:	
	freeLines(opts.mods, free);
	exit(rc);
}

private int
print(void)
{
	kvpair	kv;
	char	*p, **l = 0;
	int	i;

	EACH_KV(opts.parents) {
		p = "";
		switch (kv.val.dptr[0]) {
		    case 'b':
			if (opts.annotate) p = "Push/pull parent: ";
			break;
		    case 'i':
		    	if (opts.pushonly) continue;
			if (opts.annotate) p = "Pull parent: ";
			break;
		    case 'o':
		    	if (opts.pullonly) continue;
			if (opts.annotate) p = "Push parent: ";
			break;
		    default:
			fprintf(stderr, "Internal error.\n");
			continue;
		}
		l = addLine(l, aprintf("%s%s\n", p, kv.key.dptr));
	}
	if (l) {
		unless (opts.quiet) {
			sortLines(l, 0);
			EACH(l) {
				fputs(l[i], stdout);
				if (opts.one) break;
			}
		}
		freeLines(l, free);
	} else {
		unless (opts.quiet) {
			printf("This repository has no ");
			if (opts.pushonly) printf("push ");
			if (opts.pullonly) printf("pull ");
			printf("parent.\n");
		}
		return (1);
	}
	return (0);
}

private void
add(char *which, char *url, int *rc)
{
	char	*p, *m;
	char	*parent = "?";

// ttyprintf("ADD %s %s\n", which, url);

	switch (*which) {
	    case 'i': parent = "pull parent"; break;
	    case 'o': parent = "push parent"; break;
	    case 'b': parent = "parent"; break;
	}
	unless (url = normalize(url, 1)) {
		*rc = 1;
		return;
	}
	unless (p = mdbm_fetch_str(opts.parents, url)) {
		mdbm_store_str(opts.parents, url, which, MDBM_INSERT);
		m = aprintf("Add %s %s\n", parent, url);
		opts.mods = addLine(opts.mods, m);
		free(url);
		return;
	}
	switch (*which) {
	    case 'i':
		if (*p == 'o') {
			mdbm_store_str(opts.parents, url, "b", MDBM_REPLACE);
			m = aprintf("Add %s %s\n", parent, url);
			opts.mods = addLine(opts.mods, m);
		}
		break;

	    case 'o':
		if (*p == 'i') {
			mdbm_store_str(opts.parents, url, "b", MDBM_REPLACE);
			m = aprintf("Add %s %s\n", parent, url);
			opts.mods = addLine(opts.mods, m);
		}
		break;

	    case 'b':
		mdbm_store_str(opts.parents, url, "b", MDBM_REPLACE);
		m = aprintf("Add %s %s\n", parent, url);
		opts.mods = addLine(opts.mods, m);
		break;
	}
	free(url);
}

/*
 * If the URL matches one of the lines, remove that entry.
 * Set *rc if it isn't found.
 */
private void
rm(char *which, char *url, int *rc)
{
	char	*m = 0, *p, *turl;

	unless (url = normalize(url, 0)) {
		*rc = 1;
		return;
	}
	turl = url;
	unless (p = mdbm_fetch_str(opts.parents, url)) {
		/* try again */
		if (strneq(url, "file://", 7) &&
		    (p = mdbm_fetch_str(opts.parents, url + 7))) {
			url += 7;
		} else {
			*rc = 1;
			fprintf(stderr, "parent: Can't remove '%s'.\n", url);
			goto done;
		}
	}
// ttyprintf("RM which=%s p=%s %s\n", which, p, url);
	switch (*which) {
	    case 'i':
		if (*p == 'i') {
			mdbm_delete_str(opts.parents, url);
			m = aprintf("Remove pull parent %s\n", url);
			opts.mods = addLine(opts.mods, m);
		} else if (*p == 'b') {
			mdbm_store_str(opts.parents, url, "o", MDBM_REPLACE);
			m = aprintf("Changed %s to push parent\n", url);
			opts.mods = addLine(opts.mods, m);
		}
		break;

	    case 'o':
		if (*p == 'o') {
			mdbm_delete_str(opts.parents, url);
			m = aprintf("Remove push parent %s\n", url);
			opts.mods = addLine(opts.mods, m);
		} else if (*p == 'b') {
			mdbm_store_str(opts.parents, url, "i", MDBM_REPLACE);
			m = aprintf("Changed %s to pull parent\n", url);
			opts.mods = addLine(opts.mods, m);
		}
		break;

	    case 'b':
		switch (*p) {
		    case 'i': m = "pull"; break;
		    case 'o': m = "push"; break;
		    case 'b': m = "pull/push"; break;
		}
	    	mdbm_delete_str(opts.parents, url);
		m = aprintf("Remove %s parent %s\n", m, url);
		opts.mods = addLine(opts.mods, m);
		break;
	}
done:	free(turl);
}

private int
record(void)
{
	int	i, rc = 0;
	char	*p, **pull = 0, **push = 0;
	kvpair	kv;

	unless (opts.parents) {
		if (exists(p = PARENT)) rc = unlink(p);
		if (exists(p = PUSH_PARENT)) rc = unlink(p);
		if (exists(p = PULL_PARENT)) rc = unlink(p);
		unless (opts.quiet) printf("Remove all parent pointers.\n");
		return (rc ? 1 : 0);
	}

	EACH_KV(opts.parents) {
		p = 0;
// ttyprintf("RECORD %s->%s\n", kv.key.dptr, kv.val.dptr);
		switch (*kv.val.dptr) {
		    case 'i': pull = addLine(pull, kv.key.dptr); break;
		    case 'o': push = addLine(push, kv.key.dptr); break;
		    case 'b':
			pull = addLine(pull, kv.key.dptr);
		    	push = addLine(push, kv.key.dptr);
			break;
		}
	}

	unless (push) {
		unlink(PUSH_PARENT);
		unlink(PARENT);
	}
	unless (pull) {
		unlink(PULL_PARENT);
		unlink(PARENT);
	}

	if (sameLines(pull, push) && (nLines(pull) == 1)) {
		if (lines2File(pull, PARENT)) {
			rc = 1;
			perror(PARENT);
		}
		unlink(PULL_PARENT);
		unlink(PUSH_PARENT);
	} else {
		unlink(PARENT);
		if (lines2File(pull, PULL_PARENT)) {
			rc = 1;
			perror(PULL_PARENT);
		}
		if (lines2File(push, PUSH_PARENT)) {
			rc = 1;
			perror(PUSH_PARENT);
		}
	}

	unless (opts.quiet) {
		sortLines(opts.mods, 0);
		EACH(opts.mods) fputs(opts.mods[i], stdout);
	}

	freeLines(pull, 0);
	freeLines(push, 0);
	return (rc);
}

char	**
parent_pullp(void)
{
	int	i;
	remote	*r = 0;
	char	**p = readf(PULL_PARENT);

	EACH(p) {
		if ((r = remote_parse(p[i], REMOTE_ROOTREL)) &&
		    (r->type == ADDR_FILE) && r->notUrl) {
			free(p[i]);
			p[i] = strdup(r->path);
		}
		if (r) remote_free(r);
	}
	return (p);
}

char	**
parent_pushp(void)
{
	int	i;
	remote	*r = 0;
	char	**p = readf(PUSH_PARENT);

	EACH(p) {
		if ((r = remote_parse(p[i], REMOTE_ROOTREL)) &&
		    (r->type == ADDR_FILE) && r->notUrl) {
			free(p[i]);
			p[i] = strdup(r->path);
		}
		if (r) remote_free(r);
	}
	return (p);
}

private char	**
parent_pullp_keep_rel(void)
{
	return (readf(PULL_PARENT));
}

private char	**
parent_pushp_keep_rel(void)
{
	return (readf(PUSH_PARENT));
}

char	**
parent_allp(void)
{
	char	**ip = parent_pullp();
	char	**op = parent_pushp();
	int	i;

	EACH(op) ip = addLine(ip, op[i]);
	freeLines(op, 0);
	uniqLines(ip, free);
	return (ip);
}

/*
 * Normalize only the file type url
 * If we cannot normalize it, return the original url
 */
char *
parent_normalize(char *url)
{
	remote	*r = 0;
	char	*ret = 0;

	if ((r = remote_parse(url, REMOTE_BKDURL))
	    && (r->type ==  ADDR_FILE) && r->path) {
		ret = r->path;
		r->path = 0;
		if (r->params) {
			r->path = ret;
			ret = remote_unparse(r);
		}
	}
	if (r) remote_free(r);
	return (ret ? ret : strdup(url));
}

/*
 * A private version of the above -- used for formatting parents
 * Includes a checker to block new entries from being added.
 * Disable checker to remove parent even though repo doesn't exist.
 */
private	char *
normalize(char *url, int check)
{
	remote	*r = 0;
	char	*p, *newurl = 0;

	localName2bkName(url, url);

	unless (r = remote_parse(url, REMOTE_BKDURL)) {
		fprintf(stderr, "Invalid parent address: %s\n", url);
		goto done;
	}
	if (opts.normalize && 
	    (isLocalHost(r->host) || (r->type == ADDR_FILE))) {
		/*
		 * Skip the path check if the url has a port address because
		 * a) Virtual host support means we can't tell if it is valid
		 * b) Some sites may be port forwarding which means the path
		 *    is not really local.  XXX - only if ssh?
		 */
		if (check && r->path && IsFullPath(r->path) && !r->port) {
			p = aprintf("%s/BitKeeper/etc", r->path);
			unless (isdir(p)) {
				fprintf(stderr,
				    "Not a repository: '%s'\n", url);
				free(p);
				goto done;
			}
			free(p);
		}
		if (r->host) {
			free(r->host);
			r->host = strdup(sccs_realhost());
		}
	}
	if (opts.normalize) {
		newurl = remote_unparse(r);
	} else {
		newurl = strdup(url);
	}
done:	if (r) remote_free(r);
	return (newurl);
}

private	char **
readf(char *file)
{
	char	**list;
	project	*proj;
	char	buf[MAXPATH];

	if (proj_isComponent(0)) {
		proj = proj_product(0);
	} else {
		proj = 0;
	}
	sprintf(buf, "%s/%s", proj_root(proj), file);
	list = file2Lines(0, buf);
	sprintf(buf, "%s/%s", proj_root(proj), PARENT);
	list = file2Lines(list, buf);
	uniqLines(list, free);
	if (list && !list[1]) {
		freeLines(list, free);
		list = 0;
	}
	return (list);
}
