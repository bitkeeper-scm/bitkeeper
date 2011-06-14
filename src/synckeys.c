/*
 * Copyright (c) 2000, Larry McVoy & Andrew Chang
 */
#include "bkd.h"
#include "logging.h"
#include "range.h"

/*
 * Send the probe keys for D deltas.
 * Design supports multiple LODs.
 */
private void
lod_probekey(sccs *s, delta *d, int syncRoot, FILE *f)
{
	int	i, j;
	char	key[MAXKEY];

	/*
	 * Phase 1, send the probe keys.
	 * NB: must be in most recent to least recent order.
	 */
	for (i = 1; d && (d != s->tree); i *= 2) {
		for (j = i; d && --j; d = PARENT(s, d));
		if (d) {
			assert(!TAG(d));
			sccs_sdelta(s, d, key);
			fprintf(f, "%s\n", key);
		}
	}

	/*
	 * Always send the root key because we want to force a match.
	 * No match is a error condition.
	 */
	if (syncRoot) {
		/*
		 * We want to send the sync rootkey instead of the
		 * current rootkey.
		 */
		sccs_syncRoot(s, key);
	} else {
		sccs_sdelta(s, sccs_ino(s), key);
	}
	fprintf(f, "%s\n", key);
}

private void
tag_probekey(sccs *s, FILE *f)
{
	delta	*d;
	int	i, j;
	char	key[MAXKEY];

	for (d = s->table; d; d = NEXT(d)) {
		/* Optimization: only tagprobe if tag with parent tag */
		if (d->ptag && !(d->flags & D_GONE)) break;
	}
	unless (d) return;

	fputs("@TAG PROBE@\n", f);
	for (i = 1; d; i *= 2) {
		for (j = i; d->ptag && --j; d = sfind(s, d->ptag));
		sccs_sdelta(s, d, key);
		fprintf(f, "%s\n", key);
		unless (d->ptag) return;
	}
}

int
probekey_main(int ac, char **av)
{
	sccs	*s;
	char	*rev = 0;
	int	rc, c, syncRoot = 0;

	while ((c = getopt(ac, av, "Sr;", 0)) != -1) {
		switch (c) {
		    case 'S': syncRoot = 1; break;
		    case 'r': rev = optarg; break;
		    default: bk_badArg(c, av);
		}
	}
	unless ((s = sccs_csetInit(0)) && HASGRAPH(s)) {
		out("ERROR-Can't init changeset\n@END@\n");
		return (1);
	}
	rc = probekey(s, rev, syncRoot, stdout);
	sccs_free(s);
	return (rc);
}

int
probekey(sccs *s, char *rev, int syncRoot, FILE *f)
{
	delta	*d;

	if (rev) {
		unless (d = sccs_findrev(s, rev)) {
			fprintf(f, "ERROR-Can't find revision %s\n", rev);
			return (1);
		}
		range_gone(s, d, D_GONE);
	} else {
		d = sccs_top(s);
	}
	fputs("@LOD PROBE@\n", f);
	lod_probekey(s, d, syncRoot, f);
	tag_probekey(s, f);
	fputs("@END PROBE@\n", f);

	return (0);
}
/*
 * This one must be called after we have done sccs_color and must
 * not stop at an already colored node because a node can be both
 * a regular and/or a tag node.
 */
void
sccs_tagcolor(sccs *s, delta *d)
{
	if (d->flags & D_BLUE) return;
	d->flags |= D_BLUE;
	if (d->ptag) sccs_tagcolor(s, sfind(s, d->ptag));
	if (d->mtag) sccs_tagcolor(s, sfind(s, d->mtag));
	d->flags |= D_RED;
}

/*
 * Given a delta pointer 'd', return the tag
 */
private char *
sccs_d2tag(sccs *s, delta *d)
{
	symbol	*sym;

	unless (d->flags & D_SYMBOLS) return (NULL);
	EACHP_REVERSE(s->symlist, sym) {
		if (SERIAL(s, d) == sym->ser) {
			assert(!strchr(SYMNAME(s, sym), '|'));
			return (SYMNAME(s, sym));
		}
	}
	return (NULL); /* we should never get here */
}

/*
 * Called on the server side of a keysync operation.  It read the log2
 * probe on stdin and returns the closest match and a list of other
 * keys on stdout.
 *
 * Exit codes from listkey matter:
 *   0 - success
 *   1 - didn't find matching rootkey / empty probe
 *   2 - no data on stdin
 *   3 - can't open cset file / bad data on stdin
 *   5 - bad args
 */
int
listkey_main(int ac, char **av)
{
	sccs	*s;
	delta	*d = 0;
	int	i, c, debug = 0, quiet = 0, nomatch = 1;
	int	sndRev = 0;
	int	ForceFullPatch = 0; /* force a "makepatch -r.." */
	int	syncRoot = 0;
	char	key[MAXKEY], rootkey[MAXKEY], synckey[MAXKEY];
	char	s_cset[] = CHANGESET;
	char	**lines = 0;
	char	*tag;
	int	sum;
	char	*note;

#define OUT(s)  unless(ForceFullPatch) out(s)

	while ((c = getopt(ac, av, "dqrFS", 0)) != -1) {
		switch (c) {
		    case 'd':	debug = 1; break;
		    case 'q':	quiet = 1; break;
		    case 'r':	sndRev = 1; break;
		    case 'F':   ForceFullPatch = 1; break;
		    case 'S':   syncRoot = 1; break;
		    default: bk_badArg(c, av);
		}
	}
	unless ((s = sccs_init(s_cset, 0)) && HASGRAPH(s)) {
		fprintf(stderr, "Can't init changeset\n");
		return(3); /* cset error */
	}
	sccs_sdelta(s, sccs_ino(s), rootkey);
	if (syncRoot) {
		s->keydb_nopath = 1;
		sccs_syncRoot(s, synckey);
		if (streq(rootkey, synckey)) syncRoot = 0;
	}

	/*
	 * Phase 1, get the probe keys.
	 */
	if (getline(0, key, sizeof(key)) <= 0) {
		unless (quiet) {
			fprintf(stderr, "Expected \"@LOD PROBE@\", Got EOF\n");
		}
		sccs_free(s);
		return (2);
	}
	unless (streq("@LOD PROBE@", key)) {
		unless (quiet) {
			fprintf(stderr,
			    "Expected \"@LOD PROBE@\", Got \"%s\"\n", key);
		}
		sccs_free(s);
		return(3); /* protocol error or repo locked */
	}

	if (debug) fprintf(stderr, "listkey: looking for match key\n");

	/*
	 * Save the data in a lines list and then reprocess it.
	 * We need two passes because we need to know if the root key
	 * matched.
	 */
	sum = i = 0;
	while (getline(0, key, sizeof(key)) > 0) {
		lines = addLine(lines, strdup(key));
		++i;
		sum += strlen(key);
		if (streq("@END PROBE@", key) || streq("@TAG PROBE@", key)) {
			break;
		}
	}
	unless (lines && lines[1]) goto mismatch;	/* sort of */
	note = aprintf("%u(%u)", sum, i);
	cmdlog_addnote("keysin", note);
	free(note);

	/*
	 * Make sure that one of the keys match the root key and that the
	 * next item is one of the @ commands.
	 */
	nomatch = 1;
	EACH(lines) {
		if (syncRoot && streq(lines[i], synckey)) {
			free(lines[i]);
			lines[i] = strdup(rootkey);
		}
		unless (streq(lines[i], rootkey)) continue;
		unless (lines[i+1]) break;
		if (streq(lines[i+1], "@END PROBE@") ||
		    streq(lines[i+1], "@TAG PROBE@") ||
		    streq(lines[i+1], "@LOD PROBE@")) {
		    	nomatch = 0;
		}
	}
	if (nomatch) {
mismatch:	if (debug) fprintf(stderr, "listkey: no match key\n");
		out("@NO MATCH@\n");
		out("@END@\n");
		sccs_free(s);
		return (1); /* package key mismatch */
	}

	/*
	 * Now do the real processing.
	 */
	nomatch = 1;
	EACH(lines) {
		if (streq("@LOD PROBE@", lines[i])) {
			d = 0;
			continue;
		}
		if (!d && (d = sccs_findKey(s, lines[i]))) {
			sccs_color(s, d);
			if (debug) {
				fprintf(stderr, "listkey: found a match key\n");
			}
			if (nomatch) out("@LOD MATCH@\n");	/* aka first */
			if (sndRev) {
				assert(d->r[0]);
				OUT(REV(s, d));
				OUT("|");
				if (tag = sccs_d2tag(s, d)) out(tag);
				OUT("|");
			}
			if (syncRoot && (d == sccs_ino(s))) {
				OUT(synckey);
			} else {
				OUT(lines[i]);
			}
			OUT("\n");
			nomatch = 0;
		}
	}

	if (streq("@TAG PROBE@", lines[--i])) {
		d = 0;
		while (getline(0, key, sizeof(key)) > 0) {
			if (streq("@END PROBE@", key)) break;
			if (!d && (d = sccs_findKey(s, key))) {
				sccs_tagcolor(s, d);
				out("@TAG MATCH@\n");
				if (sndRev) {
					assert(d->r[0]);
					OUT(REV(s, d));
					OUT("|");
					if (tag = sccs_d2tag(s, d)) out(tag);
					out("|");
				}
				sccs_sdelta(s, d, key);
				OUT(key);
				OUT("\n");
			}
		}
	}
	out("@END MATCHES@\n");
	freeLines(lines, free);

	/*
	 * Phase 2, send the non marked keys.
	 */
	sum = i = 0;
	for (d = s->table; d; d = NEXT(d)) {
		if (d->flags & D_RED) continue;
		if (sndRev) {
			assert(d->r[0]);
			OUT(REV(s, d));
			OUT("|");
			if (tag = sccs_d2tag(s, d)) {
				OUT(tag);
			}
			OUT("|");
		}
		sccs_sdelta(s, d, key);
		OUT(key);
		OUT("\n");
		++i;
		sum += strlen(key);
	}
	out("@END@\n");
	sccs_free(s);
	note = aprintf("%u(%u)", sum, i);
	cmdlog_addnote("keysout", note);
	free(note);
	return (0);
}

private char *
get_key(char *buf, int flags)
{
	char	*p;

	unless (flags & PK_REVPREFIX)  return (buf);
	p = strchr(buf, '|'); /* skip rev */
	assert(p);
	p = strchr(++p, '|'); /* skip tag */
	assert(p);
	return (++p);
}

private int
skipit(hash *skip, char *key)
{
	if (skip == NULL) return (0);
	if (hash_fetchStr(skip, key)) return (1);
	return (0);
}

/*
 * If there are multiple lods and a tag "lod" then we expect this:
 * @LOD MATCH@
 * <key>
 * <key>
 * ...
 * @TAG MATCH@
 * <tag key>
 * @END MATCHES@
 * <key>
 * <key>
 * ...
 * @END@
 */
int
prunekey(sccs *s, remote *r, hash *skip, int outfd, int flags,
	int quiet, int *local_only, int *remote_csets, int *remote_tags)
{
	delta	*d;
	int	rc = 0, rcsets = 0, rtags = 0, local = 0;
	char	*k;
	char	key[MAXKEY + 512] = ""; /* rev + tag + key */
	char	synckey[MAXKEY];
	/*
	 * Reopen stdin with a stdio stream.  We will be reading a LOT of
	 * data and it will all be processed with this process so it is
	 * much faster.
	 */
	unless (r->rf) r->rf = fdopen(r->rfd, "r");
	assert(r->rf);

	unless (getline2(r, key, sizeof(key)) > 0) {
		unless (quiet) {
			fprintf(stderr,
			    "prunekey: expected @CMD@, got nothing.\n");
		}
		return (-1);
	}
	if (streq(key, "@NO MATCH@")) {
		getline2(r, key, sizeof(key)); /* eat @END@ */
		return (-2);
	}
	if (strneq(key, "@FAIL@-", 7)) {
		unless (quiet) fprintf(stderr, "%s\n", &key[7]);
		return (-1);
	}
	if (streq(key, "@EMPTY TREE@")) goto empty;
	unless (streq(key, "@LOD MATCH@")) {
		unless (quiet) {
			fprintf(stderr,
			    "prunekey: protocol error: %s key\n", key);
		}
		return (-1);
	}

	if (flags & PK_SYNCROOT) {
		s->keydb_nopath = 1;
		sccs_syncRoot(s, synckey);
	}

	/* Work through the LOD key matches and color the graph. */
	for ( ;; ) {
		unless (getline2(r, key, sizeof(key)) > 0) {
			perror("prunekey: expected key | @");
			exit(2);
		}
		if (key[0] == '@') break;
		k = get_key(key, flags);
		d = sccs_findKey(s, k);
		if (!d && (flags & PK_SYNCROOT)) {
			if (streq(k, synckey)) d = sccs_ino(s);
		}
		/*
		 * If there is garbage on the wire,
		 * it is ok to drop a key, this just means we
		 * we send more csets then we need, not a big deal.
		 */
		unless (d) {
			fprintf(stderr, "prunekey: bad key: %s\n", key);
			continue;
		}
		sccs_color(s, d);
	}

	/* Work through the tag key matches and color the tag graph. */
	if (streq(key, "@TAG MATCH@")) {
		for ( ;; ) {
			unless (getline2(r, key, sizeof(key)) > 0) {
				perror("prunekey: expected key | @");
				exit(2);
			}
			if (key[0] == '@') break;
			k = get_key(key, flags);
			d = sccs_findKey(s, k);
			assert(d);
			sccs_tagcolor(s, d);
		}
	} else unless (streq(key, "@END MATCHES@")) {
		fprintf(stderr, "Protocol error, wanted @END MATCHES@\n");
		exit(2);
	}

	for ( ;; ) {
		unless (getline2(r, key, sizeof(key)) > 0) {
			perror("prunekey: expected key | @");
			exit(2);
		}
		if (streq("@END@", key)) break;
		k = get_key(key, flags);
		if (d = sccs_findKey(s, k)) {
			d->flags |= D_RED;
		} else if (!skipit(skip, k)) { 
			if (sccs_istagkey(k)) {
				rtags++;
			} else {
				rcsets++;
			}
			if (flags & PK_RKEY) {
				writen(outfd, k, strlen(k));
				write(outfd, "\n", 1);
			}
		}
	}


empty:	for (d = s->table; d; d = NEXT(d)) {
		/* reset sccs_tagcolor D_BLUE markings*/
		if (d->flags & D_BLUE) d->flags &= ~D_BLUE;
		if (d->flags & D_RED) {
			d->flags &= ~D_RED;
			continue;
		}
		if (flags & PK_LKEY) {
			sccs_md5delta(s, d, key);
			writen(outfd, key, strlen(key));
			write(outfd, "\n", 1);
		}
		local++;
	}
	if (flags & PK_SYNCROOT) s->keydb_nopath = 0;
	if (remote_csets) *remote_csets = rcsets;
	if (remote_tags) *remote_tags = rtags;
	if (local_only) *local_only = local;
	safe_putenv("BK_LOCALCSETS=%d", local);
	safe_putenv("BK_REMOTECSETS=%d", rcsets);
	rc = local; 

	return (rc);
}

/*
 * For debugging, set things up to get the key list.
 */
int
prunekey_main(int ac, char **av)
{
	remote	r;
	sccs	*s;
	delta	*d;
	char	*dspec =
		    "$if(:DT:=D){C}$unless(:DT:=D){:DT:} "
		    ":I: :D: :T::TZ: :P:@:HOST:"
		    // " :DS: :DP: :TAG_PSERIAL: :TAG_MSERIAL:"
		    "\n"
		    "$each(:TAG:){T (:TAG:)\n}"
		    "$each(:C:){  (:C:)\n}"
		    "\n";

	bzero(&r, sizeof(r));
	r.rfd = 0;
	r.wfd = -1;
	proj_cd2root();
	unless ((s = sccs_csetInit(0)) && HASGRAPH(s)) {
		fprintf(stderr, "prunekey: Can't open ChangeSet\n");
		if (s) sccs_free(s);
		return (-1);
	}
	prunekey(s, &r, NULL, -1, 0, 0, 0, 0, 0);
	s->state &= ~S_SET;
	for (d = s->table; d; d = NEXT(d)) {
		if (d->flags & D_RED) continue;
		s->rstart = s->rstop = d;
		sccs_prs(s, PRS_ALL, 0, dspec, stdout);
		d->flags &= ~D_SET;
	}
	sccs_free(s);
	return (0);
}


private int
send_sync_msg(remote *r, sccs *s, int flags)
{
	FILE 	*f;
	char	*probe;
	size_t	probelen;
	int	rc;
	char	buf[MAXPATH];

	bktmp(buf, "synccmds");
	f = fopen(buf, "w");
	assert(f);
	sendEnv(f, NULL, r, 0);
	add_cd_command(f, r);
	fprintf(f, "synckeys %s\n", (flags & PK_SYNCROOT) ? "-S" : "");
	fclose(f);

	f = fmem();
	probekey(s, 0, (flags & PK_SYNCROOT), f);
	probe = fmem_peek(f, &probelen);
	rc = send_file(r, buf, probelen);
	unlink(buf);
	unless (rc) {
		writen(r->wfd, probe, probelen);
		rc = send_file_extra_done(r);
	}
	fclose(f);
	return (rc);
}

/*
 * Ret	0 on success
 *	1 on error.
 *	2 to force bkd unlock
 *	3 empty dir
 *	4 package doesn't match
 */
int
synckeys(remote *r, sccs *s, int flags, FILE *fout)
{
	int	rc = 1, i;
	char	buf[MAXPATH];

	if (bkd_connect(r, 0)) return (1);
	if (send_sync_msg(r, s, flags)) goto out;
	if (r->rfd < 0) goto out;

	if (r->type == ADDR_HTTP) skip_http_hdr(r);
	if (getline2(r, buf, sizeof(buf)) <= 0) goto out;
	if ((i = remote_lock_fail(buf, 1))) {
		rc = -i;		/* 2 means locked */
		goto out;
	} else if (streq(buf, "@SERVER INFO@")) {
		if (getServerInfo(r, 0)) goto out;
		getline2(r, buf, sizeof(buf));
	} else {
		drainErrorMsg(r, buf, sizeof(buf));
		goto out;
	}
	if (get_ok(r, buf, 1)) goto out;

	/*
	 * What we want is: "remote => bk _prunekey => stdout"
	 */
	flags |= PK_REVPREFIX;
	fflush(fout);
	i = prunekey(s, r, NULL, fileno(fout), flags, 0, NULL, NULL, NULL);
	if (i < 0) {
		switch (i) {
		    case -2:	/* needed to force bkd unlock */
			getMsg("unrelated_repos",
			    "synchronize with", '=', stderr);
			i = -4;	/* -2 used for locking already */
			break;
		    case -3:	/* empty dir */
			getMsg("no_repo", 0, '=', stderr);
			break;
		}
		rc = -i;
		goto out;
	}
	rc = 0;
out:	disconnect(r);
	return (rc);
}

/* return same as synckeys */

int
synckeys_main(int ac, char **av)
{
	int	c, rc = 1;
	remote  *r = 0;
	sccs	*s;
	int	flags = 0;

	while ((c = getopt(ac, av, "lSr", 0)) != -1) {
		switch (c) {
		    case 'l': flags |= PK_LKEY; break;
		    case 'S': flags |= PK_SYNCROOT; break;
		    case 'r': flags |= PK_RKEY; break;
		    default: bk_badArg(c, av);
		}
	}

	has_proj("synckeys");
	unless (r = remote_parse(av[optind], REMOTE_BKDURL)) {
		fprintf(stderr, "synckeys: invalid url %s\n", av[optind]);
		exit(1);
	}

	if (proj_cd2root()) { 
		fprintf(stderr, "synckeys: cannot find package root.\n"); 
		goto out;
	}
	unless ((s = sccs_csetInit(0)) && HASGRAPH(s)) {
		fprintf(stderr, "synckeys: Unable to read SCCS/s.ChangeSet\n");
		goto out;
	}
	rc = synckeys(r, s, flags, stdout);
	sccs_free(s);
out:	if (r) remote_free(r);
	return (rc);
}
