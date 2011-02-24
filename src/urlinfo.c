#include "sccs.h"
#include "nested.h"
#include "bkd.h"

private	urlinfo	*urlinfo_fetchAlloc(nested *n, char *url);
private	int	sortUrls(const void *a, const void *b);

private	dev_t	local_dev;	/* st_dev of "." for sortUuls() */

/*
 * load urllist file into memory, this also loads the urlinfo
 * if it isn't loaded already.
 *
 * Data loaded from file is merged into existing data
 */
void
urlinfo_load(nested *n, remote *base)
{
	hash	*urllist;
	char	*rk, *url, **urls;
	comp	*c;
	char	*t;
	int	i;
	project	*proj;
	char	buf[MAXPATH];

	if (base && (base->type == ADDR_FILE)) base = 0;

	assert(!n->urls);
	assert(!n->list_loaded);
	if (base) n->list_dirty = 1;
	unless (proj = proj_isResync(n->proj)) proj = n->proj;
	concat_path(buf, proj_root(proj), NESTED_URLLIST);
	urllist = hash_fromFile(0, buf);
	EACH_HASH(urllist) {
		rk = (char *)urllist->kptr;
		urls = splitLine((char *)urllist->vptr, "\n", 0);

		/* skip bad components */
		unless (c = nested_findKey(n, rk)) continue;

		EACH(urls) {
			url = urls[i];

			TRACE("%s: %s", c ? c->path : "?", url);

			/* strip old timestamps */
			if (t = strchr(url, '|')) *t = 0;

			/*
			 * in clone when loading someone else's urllist we
			 * normalize all urls
			 */
			if (base) {
				t = remoteurl_normalize(base, url);
				free(url);
				url = t;
			}

			/* urlinfo_fetchAlloc will fill in n->urlinfo */
			urlinfo_addURL(n, c, url);
			free(url);
		}
		freeLines(urls, 0);
	}
	if (urllist) hash_free(urllist);
	n->list_loaded = 1;
}

/* build n->urls array */
void
urlinfo_buildArray(nested *n)
{
	struct	stat sb;

	freeLines(n->urls, 0);
	n->urls = 0;
	unless (n->list_loaded) urlinfo_load(n, 0);
	EACH_HASH(n->urlinfo) {
		n->urls = addLine(n->urls, (char *)n->urlinfo->vptr);
	}
	local_dev = lstat(".", &sb) ? 0 : sb.st_dev;
	sortLines(n->urls, sortUrls);
}

/*
 * called in places where the user is allowed to specify urls on the command
 * line with -@URL.
 * We override the n->urls order to put the desired urls first.
 * Also if the parents are not already present we put them in the end
 * of the list.
 *
 * Also if we haven't yet probed the url we mark all components as
 * being possibly present.
 */
void
urlinfo_urlArgs(nested *n, char **urls)
{
	int	i, j;
	comp	*c;
	urlinfo	*data;

	unless (n->urls) urlinfo_buildArray(n);

	EACH(urls) {
		data = urlinfo_fetchAlloc(n, urls[i]);

		unless (data->checked) {
			/* mark all components */
			EACH_STRUCT(n->comps, c, j) {
				urlinfo_addURL(n, c, urls[i]);
			}
			n->list_dirty = 1;
		}
		/* remove from n->urls */
		EACH_INDEX(n->urls, j) {
			if (data == (urlinfo *)n->urls[j]) {
				removeLineN(n->urls, j, 0);
				break;
			}
		}
		/* add to beginning */
		n->urls = unshiftLine(n->urls, data);
	}

	urls = parent_allp();
	EACH(urls) {
		data = urlinfo_fetchAlloc(n, urls[i]);

		unless (data->checked) {
			/* mark all components */
			EACH_STRUCT(n->comps, c, j) {
				urlinfo_addURL(n, c, urls[i]);
			}
		}
		EACH_INDEX(n->urls, j) {
			if (data == (urlinfo *)n->urls[j]) {
				data = 0; /* already in list, skip */
				break;
			}
		}
		if (data) {
			/* add to end */
			n->urls = addLine(n->urls, data);
		}
	}
	freeLines(urls, free);	/* free parent list, not orig urls */
}

/*
 * translate the n->urls list into the URLLIST and URLINFO files on disk
 */
int
urlinfo_write(nested *n)
{
	FILE	*f;
	int	i;
	char	*url, *rk;
	urlinfo	*data;
	comp	*c;
	hash	*urllist;
	char	***listp, **list;
	int	cnt;
	char	tmpf[MAXPATH];

	unless (isdir(BKROOT)) {
		fprintf(stderr, "urlinfo_write() not at root\n");
		return (-1);
	}

	unless (n->list_dirty) return (0); /* nothing to write */
	unless (n->list_loaded) urlinfo_load(n, 0);

	urllist = hash_new(HASH_MEMHASH);
	bktmp_local(tmpf, "urllist");
	unless (f = fopen(tmpf, "w")) {
		perror(tmpf);
		return (-1);
	}
	EACH_HASH(n->urlinfo) {
		url = n->urlinfo->kptr;
		data = (urlinfo *)n->urlinfo->vptr;
		cnt = 0;
		EACH_HASH(data->pcomps) {
			if (*(char *)data->pcomps->vptr != '1') continue;
			c = *(comp **)data->pcomps->kptr;

			unless (listp = hash_insert(urllist,
				c->rootkey, strlen(c->rootkey)+1,
				0, sizeof(char **))) {
				listp = (char ***)urllist->vptr;
			}
			TRACE("add %s to %s\n", data->url, c ? c->path : "?");
			*listp = addLine(*listp, data->url);
			++cnt;
		}
		unless (cnt) continue; /* unused */
		putc('@', f);
		webencode(f, url, strlen(url)+1);
		putc('\n', f);
		fprintf(f, "%ld\n", data->time);
		fprintf(f, "%d\n", data->gate);
		fprintf(f, "%s\n", data->repoID ? data->repoID : "");
		EACH(data->extra) {
			fprintf(f, "%s\n", data->extra[i]);
		}
	}
	fclose(f);
	if (fileMove(tmpf, NESTED_URLINFO)) {
		perror(NESTED_URLINFO);
		return (-1);
	}

	unless (f = fopen(tmpf, "w")) {
		perror(tmpf);
		return (-1);
	}
	EACH_HASH(urllist) {
		putc('@', f);
		rk = (char *)urllist->kptr;
		fputs(rk, f);  // webencode(f, rk, strlen(rk)+1);
		putc('\n', f);

		list = *(char ***)urllist->vptr;
		EACH(list) {
			fputs(list[i], f);
			putc('\n', f);
		}
		freeLines(list, 0);
	}
	hash_free(urllist);
	fclose(f);
	if (fileMove(tmpf, NESTED_URLLIST)) {
		perror(NESTED_URLLIST);
		return (-1);
	}
	n->list_dirty = 0;
	return (0);
}

/*
 * Sort URLs to make "local" URLs be favored over remote or network URLs
 * As a secondary sort, pick recently used URLs using data->time.
 */
private int
sortUrls(const void *a, const void *b)
{
	urlinfo	*url[2];
	remote	*r;
	int	val[2];
	int	i;
	struct	stat	sb;

	url[0] = *(urlinfo **)a;
	url[1] = *(urlinfo **)b;

	/*
	 * order:		FSLH	(not)File Ssh/rsh (not)Local Http
	 *    file		0000	lclone works
	 *    file other device 0001	clone other disk
	 *    bk://localhost	1000
	 *    http://localhost	1001
	 *    bk://host		1010
	 *    http://host	1011
	 *    ssh://localhost	1100
	 *    rsh://host	1110
	 *    ssh://host	1110	ties broken with timestamp
	 *    badurl           10000
	 *
	 * Ties are broken by looking at the timestamp or
	 * with a simple strcmp() of the URLs
	 */
	for (i = 0; i < 2; i++) {
		val[i] = 0;
		r = remote_parse(url[i]->url, 0);
		unless (r) {
			val[i] |= 0x10;
			continue;
		}
		if (r->host) {
			val[i] |= 8;
			if (r->type & (ADDR_RSH|ADDR_SSH|ADDR_NFS)) val[i] |= 4;
			unless (isLocalHost(r->host) ||
			    streq(r->host, sccs_realhost())){
				val[i] |= 2;
			}
			if (r->type & ADDR_HTTP) val[i] |= 1;
		} else {
			if (lstat(r->path, &sb) || (sb.st_dev != local_dev)) {
				val[i] |= 1;
			}
		}
		remote_free(r);
	}
	if (val[0] != val[1]) {
		return (val[0] - val[1]);
	} else if (url[0]->checkedGood != url[1]->checkedGood) {
		/* favor urls we have already probed */
		return (url[0]->checkedGood ? -1 : 1);
	} else if (url[0]->time != url[1]->time) {
		return (url[1]->time - url[0]->time);
	} else {
		return (strcmp(url[1]->url, url[0]->url));
	}
}

void
urlinfo_set(nested *n, char *url, int gate, char *repoID)
{
	urlinfo	*data = urlinfo_fetchAlloc(n, url);

	data->time = time(0);
	data->gate = gate;
	FREE(data->repoID);
	if (repoID) data->repoID = strdup(repoID);
	n->list_dirty = 1;
}

urlinfo	*
urlinfo_get(nested *n, char *url)
{
	if (strneq(url, "file://", 7)) url += 7;
	unless (n->list_loaded) urlinfo_load(n, 0);
	return ((urlinfo *)hash_fetchStr(n->urlinfo, url));
}

/*
 * Called from clone & pull after setting with info from our parent.
 * The env has the bkd information and c->remotePresent is correct
 */
void
urlinfo_setFromEnv(nested *n, char *url)
{
	urlinfo	*data;
	comp	*c;
	int	i;

	urlinfo_set(n, url,
	    (getenv("BKD_GATE") ? 1 : 0), getenv("BKD_REPO_ID"));
	data = urlinfo_fetchAlloc(n, url);
	hash_free(data->pcomps);
	data->pcomps = hash_new(HASH_MEMHASH);
	EACH_STRUCT(n->comps, c, i) {
		unless (c->remotePresent) continue;
		if (c->localchanges) continue;

		urlinfo_addURL(n, c, url);
	}
	data->checked = data->checkedGood = 1;
}


void
urlinfo_free(nested *n)
{
	urlinfo	*data;

	if (n->urlinfo) {
		EACH_HASH(n->urlinfo) {
			data = (urlinfo *)n->urlinfo->vptr;
			FREE(data->repoID);
			freeLines(data->extra, free);
			hash_free(data->pcomps);
		}
		hash_free(n->urlinfo);
		n->urlinfo = 0;
	}
	freeLines(n->urls, 0);
	n->list_dirty = 0;
}

/*
 * Remember a URL that has recently been shown to be valid for a given
 * component.
 */
void
urlinfo_addURL(nested *n, comp *c, char *url)
{
	urlinfo	*data;

	n->list_dirty = 1;

	/*
	 * We want to normalize the URL being saved.  The URLs either
	 * come from parent_normalize() or remote_unparse().  They
	 * might only differ on file:// so we remove it.
	 */
	if (strneq(url, "file://", 7)) url += 7;

	data = urlinfo_fetchAlloc(n, url);
	hash_store(data->pcomps, &c, sizeof(comp *), "1", 2);
}


/*
 * Mark that a give URL no longer is valid for a given rootkey.
 *
 * h is a hash of rootkey to a newline separated list of url's
 *
 * if c==0
 *    then remove url for all components
 * if url==0,
 *    then remove all urls for that component
 * else
 *    remove url from the list for this component
 */
void
urlinfo_rmURL(nested *n, comp *c, char *url)
{
	urlinfo	*data;

	/* gotta load before we can delete data */
	unless (n->list_loaded) urlinfo_load(n, 0);
	n->list_dirty = 1;
	assert(c || url);
	unless (url) {
		/* remove c from all URLs */
		EACH_HASH(n->urlinfo) {
			data = (urlinfo *)n->urlinfo->vptr;

			unless (data->checkedGood) {
				hash_delete(data->pcomps, &c, sizeof(comp *));
			}
		}
		return;
	}
	data = urlinfo_fetchAlloc(n, url);
	if (data->checked && !data->noconnect) return; /* we know better */
	if (c) {
		hash_delete(data->pcomps, &c, sizeof(comp *));
	} else {
		/* remove all comps from this URL */
		hash_free(data->pcomps);
		data->pcomps = hash_new(HASH_MEMHASH);
	}
}

int
urlinfo_probeURL(nested *n, char *url, FILE *out)
{
	hash	*bkdEnv;
	char	*hkav[7] = {"bk", "-Lr", "-Bstdin", "havekeys", "-FD", "-", 0};
	int	rc;
	FILE	*fin, *fout;
	urlinfo	*data;
	comp	*c;
	int	i;
	char	*t, *p;

	n->list_dirty = 1;
	fprintf(out, "%s: searching %s...", prog, url);

	/* find deltakeys found locally */
	fin = fmem_open();
	EACH_STRUCT(n->comps, c, i) {
		fprintf(fin, "%s %s\n", c->rootkey, c->deltakey);
	}
	rewind(fin);
	fout = fmem_open();
	bkdEnv = hash_new(HASH_MEMHASH);
	rc = remote_cmd(hkav, url, fin, fout, 0, bkdEnv,
	    SILENT|REMOTE_GZ_SND|REMOTE_GZ_RCV);
	fclose(fin);
	rewind(fout);

	data = urlinfo_fetchAlloc(n, url);
	data->checked = 1;

	/*
	 * We have 4 possible exit status values to consider from
	 * havekeys (see comment before remote_bk() for more info):
	 *  0   the connection worked and we captured the data
	 *  16  we connected to the bkd fine, but the repository
	 *      is not there.  This URL is bogus and can be ignored.
	 *  8   The bkd_connect() failed
	 *  33	havekeys says this is myself
	 *  other  Another failure.
	 *
	 */
	if (rc == 16) {
		/* no repo at that pathname? */
		/* remove URL */
		hash_free(data->pcomps);
		data->pcomps = hash_new(HASH_MEMHASH);
		fprintf(out, "repo gone\n");
	} else if (rc == 8) {
		/* connect failure */
		data->noconnect = 1;
		fprintf(out, "connect failure\n");
	} else if (rc == 4) {
		/* bad url */
		hash_free(data->pcomps);
		data->pcomps = hash_new(HASH_MEMHASH);
		fprintf(out, "invalid url\n");
	} else if (rc == 2) {
		fprintf(out, "repo locked\n");
	} else if (rc == 33) {
		/* talking to myself */
		hash_free(data->pcomps);
		data->pcomps = hash_new(HASH_MEMHASH);
		fprintf(out, "link to myself\n");
	} else if (rc != 0) {
		/* some other failure */
		fprintf(out, "unknown failure (%d)\n", rc);
	} else {
		/* havekeys worked
		 * remove this url from urllist of repos that are
		 * still not found
		 */
		hash_free(data->pcomps);
		data->pcomps = hash_new(HASH_MEMHASH);

		while (t = fgetline(fout)) {
			if (p = separator(t)) *p = 0;
			unless (c = nested_findKey(n, t)) {
				if (p) p[-1] = ' ';
				fprintf(out, "failed\n");
				fprintf(out,
				    "%s: bad data from '%s'\n-> %s\n",
				    prog, url, t);
				fclose(fout);
				fclose(fin);
				goto out;
			}
			/*
			 * if p==0 then the remote has the component, but
			 * doesn't have the desired deltakey
			 */
			hash_store(data->pcomps,
			    &c, sizeof(comp *),
			    (p ? "1" : "0"), 2);
		}
		data->time = time(0);
		data->gate = (hash_fetchStr(bkdEnv, "BKD_GATE") ? 1 : 0);
		FREE(data->repoID);
		data->repoID = strdup(hash_fetchStr(bkdEnv, "BKD_REPO_ID"));
		data->checkedGood = 1;

		fprintf(out, "ok\n");
	}
out:	fclose(fout);
	hash_free(bkdEnv);
	return (rc);
}


private urlinfo *
urlinfo_fetchAlloc(nested *n, char *url)
{
	urlinfo	*data;
	char	**list;
	hash	*h;
	int	i, len;
	project	*proj;
	char	buf[MAXPATH];

	/*
	 * We want to normalize the URL being saved.  The URLs either
	 * come from parent_normalize() or remote_unparse().  They
	 * might only differ on file:// so we remove it.
	 */
	if (strneq(url, "file://", 7)) url += 7;

	/* load existing data on first access */
	unless (n->urlinfo) {
		n->urlinfo = hash_new(HASH_MEMHASH);

		unless (proj = proj_isResync(n->proj)) proj = n->proj;
		concat_path(buf, proj_root(proj), NESTED_URLINFO);
		h = hash_fromFile(0, buf);
		EACH_HASH(h) {
			data = urlinfo_fetchAlloc(n, h->kptr);

			list = splitLine(h->vptr, "\n", 0);
			len = nLines(list);
			if (len > 3) {
				/* save any extra data we don't understand */
				EACH_START(4, list, i) {
					data->extra =
					    addLine(data->extra, list[i]);
				}
				truncLines(list, 3);
				len = 3;
			}
			switch (len) {
			    case 3: data->repoID = strdup(list[3]);
			    case 2: data->gate = strtol(list[2], 0, 10);
			    case 1: data->time = strtoul(list[1], 0, 10);
			    case 0: break;
			}
			freeLines(list, free);
		}
		if (h) hash_free(h);
	}
	unless (data = (urlinfo *)hash_fetchStr(n->urlinfo, url)) {
		/* allocate new struct */
		data = hash_store(n->urlinfo,
		    url, strlen(url)+1,
		    0, sizeof(urlinfo));
		data->url = n->urlinfo->kptr;
		data->gate = -1;
		data->pcomps = hash_new(HASH_MEMHASH);
	}
	return (data);
}
