/*
 * Copyright 2000-2016 BitMover, Inc
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
#include "range.h"

/*
 * Send the probe keys for D deltas.
 * Design supports multiple LODs.
 */
private void
lod_probekey(sccs *s, ser_t d, u32 flags, FILE *f)
{
	int	i, j;
	char	key[MAXKEY];

	/*
	 * Phase 1, send the probe keys.
	 * NB: must be in most recent to least recent order.
	 */
	fputs("@LOD PROBE@\n", f);
	for (i = 1; d && (d != TREE(s)); i *= 2) {
		for (j = i; d && --j; d = PARENT(s, d));
		if (d) {
			assert(!TAG(s, d));
			sccs_pdelta(s, d, f);
			putc('\n', f);
		}
	}

	/*
	 * Always send the root key because we want to force a match.
	 * No match is a error condition.
	 */
	if (flags & SK_SYNCROOT) {
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
	ser_t	d;
	int	i, j;

	for (d = TABLE(s); d >= TREE(s); d--) {
		/* Optimization: only tagprobe if tag with parent tag */
		if (PTAG(s, d) && !(FLAGS(s, d) & D_GONE)) break;
	}
	unless (d) return;

	fputs("@TAG PROBE@\n", f);
	for (i = 1; d; i *= 2) {
		for (j = i; PTAG(s, d) && --j; d = PTAG(s, d));
		sccs_pdelta(s, d, f);
		putc('\n', f);
		unless (PTAG(s, d)) return;
	}
}

int
probekey_main(int ac, char **av)
{
	sccs	*s;
	char	*rev = 0;
	int	rc, c;
	u32	flags = 0;

	while ((c = getopt(ac, av, "Sr;", 0)) != -1) {
		switch (c) {
		    case 'S': flags |= SK_SYNCROOT; break;
		    case 'r': rev = optarg; break;
		    default: bk_badArg(c, av);
		}
	}
	unless ((s = sccs_csetInit(0)) && HASGRAPH(s)) {
		out("ERROR-Can't init changeset\n@END@\n");
		return (1);
	}
	rc = probekey(s, rev, flags, stdout);
	sccs_free(s);
	return (rc);
}

int
probekey(sccs *s, char *rev, u32 flags, FILE *fout)
{
	ser_t	d;

	// only support these flags
	assert((flags & ~(SK_SYNCROOT|SK_OKAY)) == 0);

	if (rev) {
		unless (d = sccs_findrev(s, rev)) {
			fprintf(fout, "ERROR-Can't find revision %s\n", rev);
			return (1);
		}
		while (TAG(s, d)) {
			d = PARENT(s, d);
			assert(d);
		}
		range_gone(s, L(d), D_GONE);
	} else {
		d = sccs_top(s);
	}
	if (flags & SK_OKAY) fputs("@OK@\n", fout);
	lod_probekey(s, d, flags, fout);
	tag_probekey(s, fout);
	fputs("@END PROBE@\n", fout);

	return (0);
}
/*
 * This one must be called after we have done sccs_color and must
 * not stop at an already colored node because a node can be both
 * a regular and/or a tag node.
 */
void
sccs_tagcolor(sccs *s, ser_t d)
{
	ser_t	e;
	int	j;

	if (FLAGS(s, d) & D_BLUE) return;
	FLAGS(s, d) |= D_BLUE;
	EACH_PTAG(s, d, e, j) sccs_tagcolor(s, e);
	FLAGS(s, d) |= D_RED;
}

/*
 * Given a delta pointer 'd', return the tag
 */
private char *
sccs_d2tag(sccs *s, ser_t d)
{
	symbol	*sym;

	unless (FLAGS(s, d) & D_SYMBOLS) return (NULL);
	EACHP_REVERSE(s->symlist, sym) {
		if (d == sym->ser) {
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
	int	c;
	int	rc;
	u32	flags = 0;

	while ((c = getopt(ac, av, "FqrS", 0)) != -1) {
		switch (c) {
		    case 'q':	flags |= SILENT; break;
		    case 'r':	flags |= SK_SENDREV; break;

			/* force a "makepatch -r.." */
		    case 'F':   flags |= SK_FORCEFULL; break;
		    case 'S':   flags |= SK_SYNCROOT; break;
		    default: bk_badArg(c, av);
		}
	}
	unless ((s = sccs_csetInit(0)) && HASGRAPH(s)) {
		fprintf(stderr, "Can't init changeset\n");
		return(3); /* cset error */
	}
	rc = listkey(s, flags, stdin, stdout);
	sccs_free(s);
	return (rc);
}

/*
 * reads probekey output on 'fin' and writes the list of
 * keys not implied by the probe to 'fout'
 *
 * We read this:
 * @LOD PROBE@
 * <key>
 * <key>
 * ...
 * <rootkey>
 * @TAG PROBE@		// optional
 * <key>
 * <key>
 * ...
 * <key>
 * @END PROBE@
 *
 * We produce this:
 * @LOD MATCH@
 * <key>
 * @TAG MATCH@		// optional
 * <tag key>
 * @END MATCHES@
 * <key>
 * <key>
 * ...
 * @END@
 */
int
listkey(sccs *s, u32 flags, FILE *fin, FILE *fout)
{
	int	i;
	ser_t	d = 0;
	ser_t	probed = 0;		/* first key in probe matched */
	ser_t	tagd = 0;		/* first key matched in tag probe */
	int	saw_rootkey = 0;
	char	*tag;
	int	sum;
	char	*note;
	char	*key;
	char	rootkey[MAXKEY], synckey[MAXKEY];
	char	buf[MAXKEY];

	// only support these flags
	assert((flags &
	    ~(SILENT|SK_SENDREV|SK_FORCEFULL|SK_SYNCROOT)) == 0);

	sccs_sdelta(s, sccs_ino(s), rootkey);
	if (flags & SK_SYNCROOT) {
		sccs_syncRoot(s, synckey);
		if (streq(rootkey, synckey)) flags &= ~SK_SYNCROOT;
	}

	/*
	 * Phase 1, get the probe keys.
	 */
	unless (key = fgetline(fin)) {
		unless (flags & SILENT) {
			fprintf(stderr, "Expected \"@LOD PROBE@\", Got EOF\n");
		}
		return (2);
	}
	unless (streq("@LOD PROBE@", key)) {
		unless (flags & SILENT) {
			fprintf(stderr,
			    "Expected \"@LOD PROBE@\", Got \"%s\"\n", key);
		}
		return (3); /* protocol error or repo locked */
	}
	TRACE("looking for match key");

	/* process lod probe */
	sum = i = 0;
	while (key = fgetline(fin)) {
		if (key[0] == '@') break;
		++i;
		sum += strlen(key);
		if ((flags & SK_SYNCROOT) && streq(key, synckey)) key = rootkey;
		unless (probed) probed = sccs_findKey(s, key);
		strcpy(buf, key); /* save last key */
	}
	if (streq(buf, rootkey)) saw_rootkey = 1;
	note = aprintf("%u(%u)", sum, i);
	cmdlog_addnote("keysin", note);
	free(note);

	if (streq(key, "@TAG PROBE@")) {
		while (key = fgetline(fin)) {
			if (key[0] == '@') break;
			unless (tagd) tagd = sccs_findKey(s, key);
		}
	}
	unless (streq("@END PROBE@", key)) {
		unless (flags & SILENT) {
			fprintf(stderr,
			    "Expected \"@END PROBE@\", Got \"%s\"\n",
			    (key ? key : "EOF"));
		}
		return (3); /* protocol error or repo locked */
	}
	unless (probed && saw_rootkey) {
		TRACE("no match key");
		fputs("@NO MATCH@\n", fout);
		fputs("@END@\n", fout);
		return (1); /* package key mismatch */
	}

	fputs("@LOD MATCH@\n", fout);
	unless (flags & SK_FORCEFULL) {
		if (flags & SK_SENDREV) {
			assert(R0(s, probed));
			fputs(REV(s, probed), fout);
			putc('|', fout);
			if (tag = sccs_d2tag(s, probed)) fputs(tag, fout);
			putc('|', fout);
		}
		if ((flags & SK_SYNCROOT) && (probed == sccs_ino(s))) {
			fputs(synckey, fout);
		} else {
			sccs_pdelta(s, probed, fout);
		}
		putc('\n', fout);
	}
	if (tagd && !(flags & SK_FORCEFULL)) {
		fputs("@TAG MATCH@\n", fout);
		if (flags & SK_SENDREV) {
			assert(R0(s, tagd));
			fputs(REV(s, tagd), fout);
			putc('|', fout);
			if (tag = sccs_d2tag(s, tagd)) {
				fputs(tag, fout);
			}
			fputc('|', fout);
		}
		sccs_pdelta(s, tagd, fout);
		putc('\n', fout);
	}
	fputs("@END MATCHES@\n", fout);

	/*
	 * Phase 2, send the non marked keys.
	 */
	unless (flags & SK_FORCEFULL) {
		sum = i = 0;
		sccs_color(s, probed);
		if (tagd) sccs_tagcolor(s, tagd);
		for (d = TABLE(s); d >= TREE(s); d--) {
			if (FLAGS(s, d) & D_RED) continue;
			if (flags & SK_SENDREV) {
				assert(R0(s, d));
				fputs(REV(s, d), fout);
				putc('|', fout);
				if (tag = sccs_d2tag(s, d)) {
					fputs(tag, fout);
				}
				putc('|', fout);
			}
			sccs_pdelta(s, d, fout);
			putc('\n', fout);
			++i;
			sum += strlen(buf);
		}
		note = aprintf("%u(%u)", sum, i);
		cmdlog_addnote("keysout", note);
		free(note);
	}
	fputs("@END@\n", fout);
	return (0);
}

private char *
get_key(char *buf, int flags)
{
	char	*p;

	unless (flags & SK_REVPREFIX)  return (buf);
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
 * We expect this:
 * @LOD MATCH@
 * <key>
 * <key>
 * ...
 * @TAG MATCH@		// optional
 * <tag key>
 * @END MATCHES@
 * <key>
 * <key>
 * ...
 * @END@
 */
int
prunekey(sccs *s, remote *r, hash *skip, int outfd, u32 flags,
	int *local_only, int *remote_csets, int *remote_tags)
{
	FILE	*f;
	ser_t	d;
	int	rc = 0, rcsets = 0, rtags = 0, local = 0;
	int	quiet = flags & SILENT;
	char	*k;
	char	*key;
	char	synckey[MAXKEY];
	char	buf[MAXKEY];

	// only support these flags
	assert(
	    (flags & ~(SILENT|SK_REVPREFIX|SK_LKEY|SK_RKEY|SK_SYNCROOT)) == 0);

#define	ERR(r)	rc = r; goto out
	T_CMD("prunekey");
	/*
	 * Reopen stdin with a stdio stream.  We will be reading a LOT of
	 * data and it will all be processed with this process so it is
	 * much faster.
	 */
	unless (r->rf) r->rf = fdopen(r->rfd, "r");
	assert(r->rf);
	f = r->rf;

	unless (key = fgetline(f)) {
		unless (quiet) {
			fprintf(stderr,
			    "prunekey: expected @CMD@, got nothing.\n");
		}
		ERR(-1);
	}
	if (streq(key, "@NO MATCH@")) {
		getline2(r, key, sizeof(key)); /* eat @END@ */
		ERR(-2);
	}
	if (strneq(key, "@FAIL@-", 7)) {
		unless (quiet) fprintf(stderr, "%s\n", &key[7]);
		ERR(-1);
	}
	if (streq(key, "@EMPTY TREE@")) goto empty;
	unless (streq(key, "@LOD MATCH@")) {
		unless (quiet) {
			fprintf(stderr,
			    "prunekey: protocol error: %s key\n", key);
		}
		ERR(-1);
	}
	if (flags & SK_SYNCROOT) sccs_syncRoot(s, synckey);

	/* Work through the LOD key matches and color the graph. */
	for ( ;; ) {
		unless (key = fgetline(f)) {
			perror("prunekey: expected key | @");
			ERR(-3);
		}
		if (key[0] == '@') break;
		k = get_key(key, flags);
		d = sccs_findKey(s, k);

		if (!d && (flags & SK_SYNCROOT)) {
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
			unless (key = fgetline(f)) {
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
		unless (key = fgetline(f)) {
			perror("prunekey: expected key | @");
			exit(2);
		}
		if (streq("@END@", key)) break;
		k = get_key(key, flags);
		if (d = sccs_findKey(s, k)) {
			FLAGS(s, d) |= D_RED;
		} else if (!skipit(skip, k)) {
			if (sccs_istagkey(k)) {
				rtags++;
			} else {
				rcsets++;
			}
			if (flags & SK_RKEY) {
				writen(outfd, k, strlen(k));
				write(outfd, "\n", 1);
			}
		}
	}


empty:	for (d = TABLE(s); d >= TREE(s); d--) {
		/* reset sccs_tagcolor D_BLUE markings*/
		if (FLAGS(s, d) & D_BLUE) FLAGS(s, d) &= ~D_BLUE;
		if (FLAGS(s, d) & D_RED) {
			FLAGS(s, d) &= ~D_RED;
			continue;
		}
		if (flags & SK_LKEY) {
			sccs_md5delta(s, d, buf);
			writen(outfd, buf, strlen(buf));
			write(outfd, "\n", 1);
		}
		local++;
	}
	if (remote_csets) *remote_csets = rcsets;
	if (remote_tags) *remote_tags = rtags;
	if (local_only) *local_only = local;
	safe_putenv("BK_LOCALCSETS=%d", local);
	safe_putenv("BK_REMOTECSETS=%d", rcsets);
	rc = local;

out:	T_CMD("prunekey = %d", rc);
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
	ser_t	d;
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
	prunekey(s, &r, NULL, -1, 0, 0, 0, 0);
	s->state &= ~S_SET;
	for (d = TABLE(s); d >= TREE(s); d--) {
		if (FLAGS(s, d) & D_RED) continue;
		s->rstart = s->rstop = d;
		sccs_prs(s, PRS_ALL, 0, dspec, stdout);
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

	bktmp(buf);
	f = fopen(buf, "w");
	assert(f);
	sendEnv(f, NULL, r, 0);
	add_cd_command(f, r);
	fprintf(f, "synckeys %s\n", (flags & SK_SYNCROOT) ? "-S" : "");
	fclose(f);

	f = fmem();
	probekey(s, 0, (flags & SK_SYNCROOT), f);
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
	flags |= SK_REVPREFIX;
	fflush(fout);
	i = prunekey(s, r, NULL, fileno(fout), flags, NULL, NULL, NULL);
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
		    case 'l': flags |= SK_LKEY; break;
		    case 'S': flags |= SK_SYNCROOT; break;
		    case 'r': flags |= SK_RKEY; break;
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
