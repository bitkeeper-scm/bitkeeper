/*
 * Copyright 2009-2010,2015-2016 BitMover, Inc
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

#include "sccs.h"

/*
 * Code to maintain:
 *   $HOME/.bk/XX/<realhost>/path.log
 *     (XXX from file_fanout())
 *
 *  Format:
 *      <path>|<atime>|<mtime>|<md5rootkey>
 *
 * Pre bk-7.0 had only <path>.
 */
#define	FIELDS	3	/* number of data fields after path */

/*
 * XXX the current code assumes nothing in this stuct
 * needs to be deallocated
 */
typedef struct {
	char	*path;
	char	*data;
	time_t	time;
} repolog;

typedef struct {
	u32	checkall:1;	/* recalcuation timestamps for all repos */
	u32	verbose:1;	/* include |mtime|atime */
	u32	quiet:1;	/* print no output */
	u32	raw:1;		/* raw data without looking on disk */
	u32	msort:1;	/* sort on mtime */
	u32	asort:1;	/* sort on mtime */
	u32	tsort:1;	/* some sort on time sort */
} ropts;

private	hash	*getData(int *modsp);
private	void	writeData(hash *reposdb);
private	int	stringSort(const void *a, const void *b);
private	int	timeSort(const void *a, const void *b);
private	char	*reposPath(char *filename);
private	int	checkAll(hash *reposdb, int *modsp);
private	int	checkRepo(char *path);
private	void	printRepos(ropts *opts, hash *reposdb, int *modsp);
private	char	*getRepoData(char *repo);

int
repos_main(int ac, char **av)
{
	int	c;
	hash	*reposdb;
	char	*p;
	int	mod = 0;
	ropts	opts = {0};
	longopt	lopts[] = {
		{ "check-all", 310 },
		{ "pathlog", 320 },
		{ "raw", 330 },

		/* aliases */
		{ "atime", 'a' },
		{ "mtime", 'm' },
		{ 0, 0 }
	};

	while ((c = getopt(ac, av, "ac:mqv", lopts)) != -1) {
		switch(c) {
		    case 'a': opts.asort = 1; break;
		    case 'm': opts.msort = 1; break;
		    case 'q': opts.quiet = 1; break;
		    case 'c': checkRepo(optarg); break;
		    case 'v': opts.verbose = 1; break;
		    case 310:	// --check-all
			opts.checkall = 1; break;
		    case 320:   // --pathlog
			/* Undocumented: for regressions to hack on file */
			p = reposPath("path.log");
			printf("%s\n", p);
			free(p);
			return (0);
		    case 330:   // --raw
			opts.raw = 1; break;
		    default:	bk_badArg(c, av);
		}
	}
	if (av[optind]) usage();
	if (opts.asort && opts.msort) {
		fprintf(stderr, "ERROR: only one type of sort allowed\n");
		usage();
	}
	if (opts.asort || opts.msort) opts.tsort = 1;

	reposdb = getData(&mod);
	if (opts.checkall) checkAll(reposdb, &mod);
	unless (opts.quiet) printRepos(&opts, reposdb, &mod);
	if (mod) writeData(reposdb);
	hash_free(reposdb);
	return (0);
}

/*
 * Read the path.log
 */
private hash *
getData(int *modsp)
{
	char	*pathlog;
	int	mods = 0;
	FILE	*f;
	char	*t, *p, *freeme = 0;
	int	i;
	char	**del = 0;
	hash	*reposdb = hash_new(HASH_MEMHASH);

	pathlog = reposPath("path.log");

	if (f = fopen(pathlog, "r")) {
		while (t = fgetline(f)) {
			if ((p = strchr(t, '|')) &&
			    (strcnt(p, '|') >= FIELDS)) {
				/* |atime|mtime|md5rootkey... */
				*p++ = 0;
			} else {
				if (p) *p = 0;
				/* data missing, add it */
				mods = 1;
				unless (p = getRepoData(t)) {
					/* repo missing */
					del = addLine(del, strdup(t));
					continue;
				}
				freeme = p;
			}
			hash_storeStrStr(reposdb, t, p);
			FREE(freeme);
		}
		fclose(f);
	}
	EACH(del) hash_deleteStr(reposdb, del[i]);
	freeLines(del, free);
	if (mods && modsp) *modsp = mods;
	return (reposdb);
}

private int
stringSort(const void *a, const void *b)
{
	repolog	*rl, *rr;

	rl = (repolog *)a;
	rr = (repolog *)b;

	return (strcmp(rl->path, rr->path));
}

private int
timeSort(const void *a, const void *b)
{
	repolog	*rl, *rr;
	time_t	l, r;
	time_t	diff;

	rl = (repolog *)a;
	rr = (repolog *)b;
	l = rl->time;
	r = rr->time;
	diff = (r - l);	/* note: new to old */
	if (diff) return ((diff > 0) ? 1 : -1);	/* map time_t to int */
	return (stringSort(a, b));
}

private int
checkRepo(char *repo)
{
	project	*proj;

	unless (isdir(repo)) return (1);
	if (!(proj = proj_init(repo)) || proj_isComponent(proj)) {
		char	*p;
		hash	*h;
		int	mod = 0;
		char	path[MAXPATH], fullpath[MAXPATH];

		if (win32() || (proj && proj_isCaseFoldingFS(proj))) {
			p = proj ? proj_root(proj) : fullname(repo, fullpath);
			getRealName(p, 0, path);
		} else {
			fullname(repo, path);
		}
		if (proj) proj_free(proj);
		h = getData(&mod);
		if (!hash_deleteStr(h, path) || mod) writeData(h);
		hash_free(h);
		return (1);
	}
	repos_update(proj);
	proj_free(proj);
	return (0);
}

/*
 * Update the on-disk repo DB:
 *   It lives at `bk dotbk`/repos/HH/`bk gethost -r`/path.log
 *
 * HH is the first two chars of the hash of the hostname so we spread
 * out the files.
 */
void
repos_update(project *proj)
{
	char	*p, *t;
	char	*pathlog, *lock = 0;
	FILE	*f;
	hash	*h = 0;
	int	lines = 0;
	int	uniq = 0;
	char	*data;
	char	path[MAXPATH];

	if (getenv("_BK_REPOS_SKIP")) return;
	/*
	 * Don't bother for RESYNC or components, that's just noise.
	 */
	if (proj_isResync(proj) || proj_isComponent(proj)) return;

	p = proj_root(proj);
	if (proj_isCaseFoldingFS(proj)) {
		getRealName(p, 0, path);
	} else {
		strcpy(path, p);
	}

	/* repos in BitKeeper/tmp can be ignored (bisect, partition, etc) */
	if (strstr(path, "/BitKeeper/tmp/")) return;

	data = getRepoData(path);

	pathlog = reposPath("path.log");
	if (f = fopen(pathlog, "r")) {
		/*
		 * read existing path.log file
		 * keep track of the number of uniq lines and remember
		 * the last line of this pathname
		 */
		h = hash_new(HASH_MEMHASH);
		while (t = fgetline(f)) {
			++lines;
			if (p = strchr(t, '|')) *p = 0;
			unless (hash_fetchStrStr(h, t)) ++uniq;
			hash_storeStrStr(h, t, p ? p+1 : "");
		}
		fclose(f);
		if ((t = hash_fetchStrStr(h, path)) && begins_with(t, data)) {
			int	len = strlen(data);

			/* the latest is already the right data */
			if (!t[len] || (t[len] == '|')) goto out;
		}
	} else {
		if (mkdirf(pathlog)) goto out;
	}
	lock = aprintf("%s.lock", pathlog);
	sccs_lockfile(lock, 10, 1);
	if ((lines < 1000) || (2*uniq > lines)) {
		/*
		 * Not too much junk in file so just append new line
		 * to the end
		 */
		if (f = fopen(pathlog, "a")) {
			fprintf(f, "%s|%s\n", path, data);
			fclose(f);
		}
	} else {
		/*
		 * Need to remove junk
		 */
		if (f = fopen(pathlog, "w")) {
			hash_storeStrStr(h, path, data);
			EACH_HASH(h) {
				t = h->kptr;
				fprintf(f, "%s", t);
				if (*(p = h->vptr)) fprintf(f, "|%s", p);
				fputc('\n', f);
			}
			fclose(f);
		}
	}
	sccs_unlockfile(lock);
out:	free(lock);
	free(data);
	hash_free(h);
	free(pathlog);
}

private char *
reposPath(char *filename)
{
	char	*p, *rp;

	p = file_fanout(sccs_realhost());
	rp = aprintf("%s/repos/%s/%s", getDotBk(), p, filename);
	free(p);

	return (rp);
}

private int
checkAll(hash *reposdb, int *modsp)
{
	int	i, len, mods = 0;
	char	*path, *t;
	char	**del = 0;
	char	*data;
	project	*p;

	EACH_HASH(reposdb) {
		path = reposdb->kptr;
		p = proj_init(path);
		if (!p || !streq(path, proj_root(p)) || proj_isComponent(p)) {
			if (p) proj_free(p);
			del = addLine(del, path);
			continue;
		}
		proj_free(p);
		data = getRepoData(path);
		len = strlen(data);
		t = reposdb->vptr;
		/* if beginning matches, leave unknown data alone */
		unless (strneq(data, t, len) &&
		    (!t[len] || (t[len] == '|'))) {
			mods = 1;
			hash_storeStrStr(reposdb, path, data);
		}
		free(data);
	}
	if (del) {
		EACH(del) hash_deleteStr(reposdb, del[i]);
		freeLines(del, 0);
		mods = 1;
	}
	if (mods && modsp) *modsp = 1;
	return (0);
}

private	void
printRepos(ropts *opts, hash *reposdb, int *modsp)
{
	int	i, j;
	int	mods = 0;
	repolog	*rl, *list = 0;
	char	*p;
	char	**del = 0;
	char	buf[MAXPATH];

	EACH_HASH(reposdb) {
		rl = addArray(&list, 0);
		rl->path = reposdb->kptr;
		rl->data = reposdb->vptr;
		if (opts->tsort) {
			p = rl->data;	/* points at atime */
			assert(p && *p);
			if (opts->msort) {
				p = strchr(p, '|');
				assert(p);
				p++;
			}
			rl->time = strtoll(p, 0, 10);
		}
	}
	sortArray(list, opts->tsort ? timeSort : stringSort);

	EACH(list) {
		rl = &list[i];

		concat_path(buf, rl->path, BKROOT);
		unless (opts->raw) {
			/* sanitize data with disk */
			unless (isdir(buf)) {
				/* remove any missing repositories */
				del = addLine(del, rl->path);
				continue;
			}
		}
		if (opts->verbose) {
			/* only include the fields we know about */
			p = rl->data;
			for (j = 0; p && (j < FIELDS); j++) {
				p = strchr(p+1, '|');
			}
			if (p) *p = 0;
			printf("%s|%s\n", rl->path, rl->data);
			if (p) *p = '|';
		} else {
			printf("%s\n", rl->path);
		}
	}
	if (del) {
		mods = 1;
		EACH(del) hash_deleteStr(reposdb, del[i]);
		freeLines(del, 0);
	}
	free(list);
	if (mods && modsp) *modsp = mods;
}

private time_t
getMtime(char *repo)
{
	char	*tipfile = 0, **tips = 0;
	i64	mtime = -1;

	tipfile = aprintf("%s/BitKeeper/log/TIP", repo);
	if ((tips = file2Lines(0, tipfile)) && (nLines(tips) >= 4)) {
		mtime = strtoll(tips[4], 0, 10);
	}
	free(tipfile);
	freeLines(tips, free);
	return (mtime);
}

private time_t
getAtime(char *repo)
{
	time_t	atime, t;
	char	*p;

	p = aprintf("%s/BitKeeper/log/cmd_log", repo);
	atime = mtime(p);
	free(p);

	p = aprintf("%s/BitKeeper/log/scandirs", repo);
	t = mtime(p);
	free(p);

	if (t > atime) atime = t;

	return (atime);
}

private char *
getRepoData(char *repo)
{
	project	*proj;
	char	*ret;

	unless (proj = proj_init(repo)) return (0);
	if (proj_isComponent(proj) || proj_isResync(proj)) {
		proj_free(proj);
		return (0);
	}
	ret = aprintf("%lld|%lld|%s",
	    (u64)getAtime(repo), (u64)getMtime(repo), proj_md5rootkey(proj));
	proj_free(proj);
	return (ret);
}

/*
 * write the data and a db from the data
 */
private void
writeData(hash *reposdb)
{
	char	*lock, *pathlog, *path, *data;
	FILE	*f;

	pathlog = reposPath("path.log");
	if (!exists(pathlog) && mkdirf(pathlog)) {
		free(pathlog);
		return;
	}
	lock = aprintf("%s.lock", pathlog);
	unless (sccs_lockfile(lock, 10, 1)) {
		f = fopen(pathlog, "w");
		EACH_HASH(reposdb) {
			path = reposdb->kptr;
			data = reposdb->vptr;
			fprintf(f, "%s|%s\n", path, data);
		}
		fclose(f);
		sccs_unlockfile(lock);
	}
	free(lock);
	free(pathlog);
}
