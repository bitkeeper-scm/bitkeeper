#include "bkd.h"

#define	PARENT		"BitKeeper/log/parent"
#define	PUSH_PARENT	"BitKeeper/log/push-parent"
#define	PULL_PARENT	"BitKeeper/log/pull-parent"

private	void	add(char *which, char *url, int *rc);
private	void	rm(char *which, char *url, int *rc);
private	char	**readf(char *file);
private	int	record(void);
private	int	print(void);

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
	int	c, rc = 0;
	int	i;
	char	**pull, **push, *which;

	if (ac == 2 && streq("--help", av[1])) {
		system("bk help parent");
		return (0);
	}

	if (proj_cd2root()) {
		fprintf(stderr, "parent: cannot find package root.\n");
		return (1);
	}

	opts.normalize = 1;
	opts.annotate = 1;
	while ((c = getopt(ac, av, "1anilopqrs")) != -1) {
		switch (c) {
		    case '1': opts.one = 1; break;
		    case 'a': opts.add = 1; break;
		    case 'i': opts.pushonly = 0; opts.pullonly = 1; break;
		    case 'p': /* compat, fall through, don't doc */
		    case 'l': opts.annotate = 0; break;
		    case 'n': opts.normalize = 0; break;
		    case 'o': opts.pushonly = 1; opts.pullonly = 0; break;
		    case 'q': opts.quiet = 1; break;
		    case 'r': opts.rm = 1; break;
		    case 's': opts.set = 1; break;
		    default:
usage:			system("bk help -s parent"); return (1);
		}
	}

	opts.parents = mdbm_mem();
	pull = parent_pullp();
	EACH(pull) mdbm_store_str(opts.parents, pull[i], "i", MDBM_INSERT);
	push = parent_pushp();
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
		goto usage;
	}
	if (opts.print && av[optind]) goto usage;

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
	unless (av[optind]) goto usage;
	while (av[optind]) {
		add(which, av[optind], &rc);
		optind++;
	}
	rc = record();

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

void
add(char *which, char *url, int *rc)
{
	char	*p, *m = 0;
	remote	*r = 0;

// ttyprintf("ADD %s %s\n", which, url);

	switch (*which) {
	    case 'i': m = "pull parent"; break;
	    case 'o': m = "push parent"; break;
	    case 'b': m = "parent"; break;
	}
	unless (r = remote_parse(url, 0)) {
		fprintf(stderr, "Invalid parent address: %s\n", url);
		*rc = 1;
		return;
	}
	if (opts.normalize && 
	    (isLocalHost(r->host) || (r->type == ADDR_FILE))) {
		/*
		 * Skip the path check if the url has a port address because
		 * a) Virtual host support means we can't tell if it is valid
		 * b) Some sites may be port forwarding which means the path
		 *    is not really local.  XXX - only if ssh?
		 */
		if (r->path && IsFullPath(r->path) && !r->port) {
			p = aprintf("%s/BitKeeper/etc", r->path);
			unless (isdir(p)) {
				fprintf(stderr, "Not a repository: %s\n", url);
				free(p);
				*rc = 1;
				return;
			}
			free(p);
		}
		if (r->host) {
			free(r->host);
			r->host = strdup(sccs_realhost());
		}
	}
	if (opts.normalize) {
		url = remote_unparse(r);
	} else {
		url = strdup(url);
	}
	remote_free(r);

	unless (p = mdbm_fetch_str(opts.parents, url)) {
		mdbm_store_str(opts.parents, url, which, MDBM_INSERT);
		m = aprintf("Add %s %s\n", m, url);
		opts.mods = addLine(opts.mods, m);
		free(url);
		return;
	}
	switch (*which) {
	    case 'i':
		if (*p == 'o') {
			mdbm_store_str(opts.parents, url, "b", MDBM_REPLACE);
			m = aprintf("Add %s %s\n", m, url);
			opts.mods = addLine(opts.mods, m);
		}
		break;

	    case 'o':
		if (*p == 'i') {
			mdbm_store_str(opts.parents, url, "b", MDBM_REPLACE);
			m = aprintf("Add %s %s\n", m, url);
			opts.mods = addLine(opts.mods, m);
		}
		break;

	    case 'b':
		mdbm_store_str(opts.parents, url, "b", MDBM_REPLACE);
		m = aprintf("Add %s %s\n", m, url);
		opts.mods = addLine(opts.mods, m);
		break;
	}
	free(url);
}

/*
 * If the URL matches one of the lines, remove that entry.
 * Set *rc if it isn't found.
 */
void
rm(char *which, char *url, int *rc)
{
	char	*m = 0, *p;

	unless (p = mdbm_fetch_str(opts.parents, url)) {
		*rc = 1;
		return;
	}
// ttyprintf("RM which=%s p=%s %s\n", which, p, url);
	switch (*which) {
	    case 'i':
		if (*p == 'i') {
			mdbm_delete_str(opts.parents, url);
			m = aprintf("Remove pull parent %s\n", url);
			opts.mods = addLine(opts.mods, m);
		}
		if (*p == 'b') {
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
		}
		if (*p == 'b') {
			mdbm_store_str(opts.parents, url, "i", MDBM_REPLACE);
			m = aprintf("Changed %s to pull parent\n", url);
			opts.mods = addLine(opts.mods, m);
		}
		break;

	    case 'b':
	    	mdbm_delete_str(opts.parents, url);
		switch (*p) {
		    case 'i': m = "pull"; break;
		    case 'o': m = "push"; break;
		    case 'b': m = "pull/push"; break;
		}
		m = aprintf("Remove %s parent %s\n", m, url);
		opts.mods = addLine(opts.mods, m);
		break;
	}
}

private int
record(void)
{
	int	i, rc = 0;
	char	*p, **pull = 0, **push = 0;
	kvpair	kv;

	unless (opts.parents) {
		rc |= unlink(PARENT);
		rc |= unlink(PUSH_PARENT);
		rc |= unlink(PULL_PARENT);
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

	if (lines2File(pull, PULL_PARENT)) {
		rc = 1;
		perror(PULL_PARENT);
	}
	if (lines2File(push, PUSH_PARENT)) {
		rc = 1;
		perror(PUSH_PARENT);
	}
	if (sameLines(pull, push) && (nLines(pull) == 1)) {
		if (lines2File(pull, PARENT)) {
			rc = 1;
			perror(PARENT);
		}
	} else {
		unlink(PARENT);
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
	return (readf(PULL_PARENT));
}

char	**
parent_pushp(void)
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

private	char **
readf(char *file)
{
	char	**p = file2Lines(0, file);

	p = file2Lines(p, PARENT);
	uniqLines(p, free);
	if (p && !p[1]) {
		freeLines(p, free);
		p = 0;
	}
	return (p);
}
