/*
 * Copyright (c) 2000, Larry McVoy & Andrew Chang
 */
#include "bkd.h"
#include "logging.h"

/*
 * Send the probe keys for this lod.
 * XXX - all the lods will overlap near the top, if we have a lot of
 * lods, this may become an issue.
 */
private void
lod_probekey(sccs *s, delta *d, FILE *f)
{
	int	i, j;
	char	key[MAXKEY];

	/*
	 * Phase 1, send the probe keys.
	 * NB: must be in most recent to least recent order.
	 */
	for (i = 1; d && (d != s->tree); i *= 2) {
		for (j = i; d && --j; d = d->parent);
		if (d) {
			assert(d->type == 'D');
			sccs_sdelta(s, d, key);
			fprintf(f, "%s\n", key);
		}
	}

	/*
	 * Always send the root key because we want to force a match.
	 * No match is a error condition.
	 */
	sccs_sdelta(s, sccs_ino(s), key);
	fprintf(f, "%s\n", key);
}

private void
tag_probekey(sccs *s, FILE *f)
{
	delta	*d;
	int	i, j;
	char	key[MAXKEY];

	for (d = s->table; d && !d->ptag; d = d->next);
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
	int	i;
	sccs	*s;
	delta	*d;
	char	s_cset[] = CHANGESET;
	char	rev[MAXREV+1];

	unless ((s = sccs_init(s_cset, 0, 0)) && HASGRAPH(s)) {
		fprintf(stderr, "Can't init changeset\n");
		exit(1);
	}
	for (i = 1; i <= 0xffff; ++i) {
		sprintf(rev, "%d.1", i);
		unless (d = findrev(s, rev)) break;
		while (d->kid && (d->kid->type == 'D')) d = d->kid;
		fputs("@LOD PROBE@\n", stdout);
		lod_probekey(s, d, stdout);
	}
	tag_probekey(s, stdout);
	fputs("@END PROBE@\n", stdout);
	sccs_free(s);
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
	for (sym = s->symbols; sym; sym = sym->next) {
		if (d == sym->d) {
			assert(!strchr(sym->symname, '|'));
			return (sym->symname);
		}
	}
	return (NULL); /* we should never get here */
}

/*
 * Record a rev that is shared with the client.  This is used in the
 * openlogging tree to recover from a failed resolve.  If a patch
 * fails we 'bk undo' the tree back to a point where we know one of
 * our recent clients was at.  Since there is no locking here we use
 * a rename() to change the file cleanly.
 * XXX: problem for Windows?
 */
private void
addLogKey(delta *d)
{
	char	*file = aprintf(LOG_KEYS ".%d", getpid());
	FILE	*f = fopen(file, "w");

	fprintf(f, "%s\n", d->rev);
	fclose(f);
	rename(file, LOG_KEYS);
	free(file);
}

/*
 * Called on the server side of a keysync operation.  It read the log2
 * probe on stdin and returns the closest match and a list of other
 * keys on stdout.
 *
 * When this is called on openlogging.org for part1 of the meta push
 * operation, the repository will not be locked.  So this function
 * needs to be safe in that case.  Only open ChangeSet once, don't
 * read other files, etc...
 */
int
listkey_main(int ac, char **av)
{
	sccs	*s;
	delta	*d = 0;
	int	i, c, debug = 0, quiet = 0, nomatch = 1;
	int	sndRev = 0;
	int	metaOnly = 0;
	int	matched_tot = 0;
	int	ForceFullPatch = 0; /* force a "makepatch -r1.0.." */
	char	key[MAXKEY], rootkey[MAXKEY];
	char	s_cset[] = CHANGESET;
	char	**lines = 0;
	char	*tag;
	int	sum;
	char	*note;

#define OUT(s)  unless(ForceFullPatch) out(s)

	while ((c = getopt(ac, av, "deqrF")) != -1) {
		switch (c) {
		    case 'd':	debug = 1; break;
		    case 'e':	metaOnly = 1; break;
		    case 'q':	quiet = 1; break;
		    case 'r':	sndRev = 1; break;
		    case 'F':   ForceFullPatch = 1; break;
		    default:	fprintf(stderr,
					"usage: bk _listkey [-d] [-q]\n");
				return (5);
		}
	}
	unless ((s = sccs_init(s_cset, 0, 0)) && HASGRAPH(s)) {
		fprintf(stderr, "Can't init changeset\n");
		return(3); /* cset error */
	}
	sccs_sdelta(s, sccs_ino(s), rootkey);

	/*
	 * Phase 1, get the probe keys.
	 */
	if (getline(0, key, sizeof(key)) <= 0) {
		unless (quiet) {
			fprintf(stderr, "Expected \"@LOD PROBE\"@, Got EOF\n");
			return (2);
		}
	}
	unless (streq("@LOD PROBE@", key)) {
		unless (quiet) {
			fprintf(stderr,
			    "Expected \"@LOD PROBE\"@, Got \"%s\"\n", key);
		}
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
	note = aprintf("keysin=%u(%u)", sum, i);
	cmdlog_addnote(note);
	free(note);

	/*
	 * Make sure that one of the keys match the root key and that the
	 * next item is one of the @ commands.
	 */
	nomatch = 1;
	EACH(lines) {
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
			if (nomatch && exists(LOG_TREE)) addLogKey(d);
			if (i == 1) matched_tot = 1;
			sccs_color(s, d);
			if (debug) {
				fprintf(stderr, "listkey: found a match key\n");
			}
			if (nomatch) out("@LOD MATCH@\n");	/* aka first */
			if (sndRev) {
				assert(d->rev);
				OUT(d->rev);
				OUT("|");
				if (tag = sccs_d2tag(s, d)) out(tag);
				OUT("|");
			}
			OUT(lines[i]);
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
					assert(d->rev);
					OUT(d->rev);
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
	freeLines(lines);

	/*
	 * Phase 2, send the non marked keys.
	 */
	sum = i = 0;
	for (d = s->table; d; d = d->next) {
		if (d->flags & D_RED) continue;
		/*
		 * In a logging tree the fact that we have multiple
		 * tips means that this list can get really huge.  In
		 * the case of someone doing a logging push and their
		 * top-of-trunk has already been logged, we skip this
		 * section.  They don't need it to build a patch.  We
		 * can't just skip it in general, because the push
		 * case uses these keys to compute the number of csets
		 * on the server that are missing in the local tree.
		 */
		if (matched_tot && metaOnly && d->type == 'D') continue;
		
		if (sndRev) {
			assert(d->rev);
			OUT(d->rev);
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
	note = aprintf("keysout=%u(%u)", sum, i);
	cmdlog_addnote(note);
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
prunekey(sccs *s, remote *r, int outfd, int flags,
	int quiet, int *local_only, int *remote_csets, int *remote_tags)
{
	char	key[MAXKEY + 512] = ""; /* rev + tag + key */
	delta	*d;
	int	rc = 0, rcsets = 0, rtags = 0, local = 0;
	char	*p, *k;
	char	**tags = NULL;

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
	if (streq(key, "@EMPTY TREE@")) {
		rc = -3;
		goto empty;
	}
	unless (streq(key, "@LOD MATCH@")) {
		unless (quiet) {
			fprintf(stderr,
			    "prunekey: protocol error: %s key\n", key);
		}
		return (-1);
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
		} else {
			if (sccs_istagkey(k)) {
				rtags++;
			} else {
				if (flags & PK_RREV) {
					p = strchr(key, '|');
					assert(p);
					*p++ = 0;
					write(outfd, key, strlen(key));
					write(outfd, "\n", 1);
					if (*p != '|') {
						k[-1] = 0;
						tags = addLine(tags, strdup(p));
					}
				}
				rcsets++;
			}
			if (flags & PK_RKEY) {
				write(outfd, k, strlen(k));
				write(outfd, "\n", 1);
			}
		}
	}

	/*
	 * Print remote tags
	 * XXX: TODO we need a way to handle multiple tags on the same delta
	 * May be we should extent the format 
	 * 	from "rev | tag | key"
	 * 	to   "rev | tag, tag .. | key"
	 * However, if we are just using it to feed "bk makepatch"
	 * or "bk change", we probably don't care.
	 */
	if (flags & PK_RREV) {
		int	i;
		EACH(tags) {
			write(outfd, tags[i], strlen(tags[i]));
			write(outfd, "\n", 1);
		}
		freeLines(tags);
	}

empty:	for (d = s->table; d; d = d->next) {
		if (d->flags & D_RED) continue;
		if (flags & PK_LSER) {
			sprintf(key, "%u\n", d->serial);
			write(outfd, key, strlen(key));
		}
		if (flags & PK_LREV) {
			if (d->type == 'D') {
				sprintf(key, "%s\n", d->rev);
				write(outfd, key, strlen(key));
			}
		}
		if (flags & PK_LKEY) {
			sccs_sdelta(s, d, key);
			write(outfd, key, strlen(key));
			write(outfd, "\n", 1);
		}
		local++;
	}
	if (flags & PK_LREV) { /* list the tags */
		for (d = s->table; d; d = d->next) {
			char	*tag;
			
			if (d->flags & D_RED) continue;
			unless (d->flags & D_SYMBOLS) continue;
			tag = sccs_d2tag(s, d);
			if (tag) {
				write(outfd, tag, strlen(tag));
				write(outfd, "\n", 1);
			}
			
		}
	}
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
	char	path[] = CHANGESET;
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
	sccs_cd2root(0, 0);
	s = sccs_init(path, 0, 0);
	prunekey(s, &r, -1, 0, 0, 0, 0, 0);
	s->state &= ~S_SET;
	for (d = s->table; d; d = d->next) {
		if (d->flags & D_RED) continue;
		s->rstart = s->rstop = d;
		sccs_prs(s, PRS_ALL, 0, dspec, stdout);
		d->flags &= ~D_SET;
	}
	sccs_free(s);
	exit(0);
}


private void
send_sync_msg(remote *r)
{
	char	*cmd, buf[MAXPATH];
	FILE 	*f;

	bktemp(buf);
	f = fopen(buf, "w");
	assert(f);
	sendEnv(f, NULL, r, 0);
	if (r->path) add_cd_command(f, r);
	fprintf(f, "synckeys");
	fputs("\n", f);
	fclose(f);

	cmd = aprintf("bk _probekey  >> %s", buf);
	system(cmd);
	free(cmd);

	send_file(r, buf, 0, 0);	
	unlink(buf);
}

private int
synckeys(remote *r, int flags)
{
	char	buf[MAXPATH], s_cset[] = CHANGESET;
	int	rc;
	sccs	*s;

	if (bkd_connect(r, 0, 1)) return (-1);
	send_sync_msg(r);
	if (r->rfd < 0) return (-1);

	if (r->type == ADDR_HTTP) skip_http_hdr(r);
	if (getline2(r, buf, sizeof(buf)) <= 0) return (-1);
	if ((rc = remote_lock_fail(buf, 1))) {
		return (rc); /* -2 means locked */
	} else if (streq(buf, "@SERVER INFO@")) {
		getServerInfoBlock(r);
		getline2(r, buf, sizeof(buf));
	} else {
		drainErrorMsg(r, buf, sizeof(buf));
	}
	if (get_ok(r, buf, 1)) return (-1);

	/*
	 * What we want is: "remote => bk _prunekey => stdout"
	 */
	s = sccs_init(s_cset, 0, 0);
	flags |= PK_REVPREFIX;
	rc = prunekey(s, r, 1, flags, 0, NULL, NULL, NULL);
	if (rc < 0) {
		switch (rc) {
		    case -2:
			printf(
"You are trying to sync to an unrelated package. The root keys for the\n\
ChangeSet file do not match.  Please check the pathnames and try again.\n");
			sccs_free(s);
			return (1); /* needed to force bkd unlock */
		    case -3:
			printf("You are syncing to an empty directory\n");
			sccs_free(s);
			return (1); /* empty dir */
			break;
		}
		disconnect(r, 2);
		sccs_free(s);
		return (-1);
	}
	sccs_free(s);
	if (r->type == ADDR_HTTP) disconnect(r, 2);
	return (0);
}


int
synckeys_main(int ac, char **av)
{
	int	c;
	remote  *r;
	int 	flags = 0;

	while ((c = getopt(ac, av, "l|r|")) != -1) {
		switch (c) {
		    case 'l':	if (optarg && (optarg[0] == 'k')) {
					flags |= PK_LKEY;
				} else {
					flags |= PK_LREV;
				}
				break;
		    case 'r':	if (optarg && (optarg[0] == 'k')) {
					flags |= PK_RKEY;
				} else {
					flags |= PK_RREV;
				}
				break;
		    default:  fprintf(stderr, "bad option %c\n", c);
			      exit(1);
		}
	}



	loadNetLib();
	has_proj("synckeys");
	r = remote_parse(av[optind], 0);
	assert(r);

	if (sccs_cd2root(0, 0)) { 
		fprintf(stderr, "synckeys: cannot find package root.\n"); 
		exit(1);
	}

	if ((bk_mode() == BK_BASIC) &&
	    !isLocalHost(r->host) && exists(BKMASTER)) {
		fprintf(stderr, 
		    "Cannot sync from master repository: %s", upgrade_msg);
		exit(1);
	}

	synckeys(r, flags);
	remote_free(r);
	return (0);
}
