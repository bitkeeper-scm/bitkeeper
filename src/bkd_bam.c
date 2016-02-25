/*
 * Copyright 2007-2013,2016 BitMover, Inc
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
#include "bkd.h"
#include "bam.h"
#include "nested.h"

private FILE	*server(int recurse);
private	int	havekeys_deltas(void);

/*
 * Simple key sync.
 * Receive a list of keys on stdin and return a list of
 * keys not found locally.
 * With -B, if the key is a 4 tuple, ignore the first one, that's the size,
 * but send it back so we can calculate the total amount to be sent.
 *
 * When calling this for a remote BAM server, the command line should
 * look like this:
 *     bk -q@URL -Lr -Bstdin havekeys -B [-l] -
 *  (read lock, compress both ways, buffer input)
 */
int
havekeys_main(int ac, char **av)
{
	char	*dfile, *key;
	int	c;
	int	rc = 0, BAM = 0, recurse = 1, DELTA = 0;
	FILE	*f = 0;
	char	buf[MAXLINE];

	while ((c = getopt(ac, av, "BDFlq", 0)) != -1) {
		switch (c) {
		    case 'B': BAM = 1; break;
		    case 'D': DELTA = 1; break;
		    case 'F': break; /* ignored, was 'print found' */
		    case 'l': recurse = 0; break;
		    case 'q': break;	/* ignored for now */
		    default: bk_badArg(c, av);
		}
	}
	unless (av[optind] && streq(av[optind], "-")) usage();
	if (proj_cd2root()) {
		fprintf(stderr, "%s: must be run in a bk repository.\n",av[0]);
		return (1);
	}
	if (BAM + DELTA != 1) {
		fprintf(stderr, "%s: only -B or -D supported.\n", av[0]);
		return (1);
	}
	if (getenv("_BK_VIA_REMOTE") &&
	    (key = getenv("BK_REPO_ID")) && streq(key, proj_repoID(0))) {
		fprintf(stderr, "%s: Cannot connect to myself.\n", prog);
		return (33);	/* see comment before remote_bk() for info */
	}

	if (DELTA) return (havekeys_deltas());

	/*
	 * What this should do is on the first missing key, popen another
	 * havekeys to our server (if we have one, else FILE * is just stdout)
	 * and then fputs the missing keys to that one which will filter through
	 * both our local BAM pool and our servers and send back the list of 
	 * keys that neither of us have.
	 *
	 * XXX - the optimization we want is to know if the server is local
	 * and do the work directly.
	 * XXX - need to make sure this is locked.
	 * XXX - what if the server has a server?  Should we keep going?
	 * If we do we need to remember all the places we looked so we
	 * don't loop and deadlock.  Fun!
	 *
	 * XXX - this is going to fail when we can't lock the bp server.
	 * In that case, annoying though it is, I believe we want to have
	 * cached all the keys and send them to stdout and get them locally.
	 * We want to make sure we lock and unlock as fast as possible.
	 */
	while (fnext(buf, stdin)) {
		chomp(buf);

		if (buf[0] == '|') {
			key = strchr(buf+1, '|') + 1;
		} else {
			key = buf;
		}
		if (dfile = bp_lookupkeys(0, key)) {
			free(dfile);
		} else {
			/* not here */
			unless (f) f = server(recurse);
			// XXX - need to check error status.
			fprintf(f, "%s\n", buf);
		}
	}
	if (f && (f != stdout) && pclose(f)) {
		// XXX - should send all the keys we don't have.
		fprintf(stderr, "havekeys: server error\n");
		rc = 1;
	}
	fflush(stdout);
	return (rc);
}

/*
 * Figure out if we have a server and if we do go run a havekeys there.
 * XXX - this doesn't handle loops other than just failing miserably
 * if the server is locked.
 * XXX - doesn't handle -F
 */
private FILE *
server(int recurse)
{
	char	*p;
	FILE	*f;
	char	buf[MAXPATH];

	unless (recurse) return (stdout);
	unless (bp_serverID(buf, 1)) return (stdout);
	p = aprintf("bk -q@'%s' -Lr -Bstdin havekeys -B -l -",
	    bp_serverURL(buf));
	f = popen(p, "w");
	free(p);
	return (f ? f : stdout);
}


private int
havekeys_deltas(void)
{
	sccs	*s;
	MDBM	*idDB;
	char	*rootkey, *key;
	char	*path, *t;
	char	**pcsets = 0;	/* product csets */
	hash	*ccsets;	/* component csets */
	int	ncsets = 0;	/* count in ccsets */
	int	i;

	ccsets = hash_new(HASH_MEMHASH);

	/*
	 * read 'rootkey deltakey' on stdin and return the list of
	 * keys that _can_ be found.
	 * Note: this is the reverse of 'bk havekeys -B'
	 */
	idDB = loadDB(IDCACHE, 0, DB_IDCACHE);
	while (rootkey = fgetline(stdin)) {
		if (key = separator(rootkey)) *key++ = 0;

		// I think Wayne just wants to parse out the path part
		// This works because all components are in the idcache,
		// because all have a rootkey of ChangeSet and are elsewhere.
		path = key2path(rootkey, 0, 0, 0);
		if (streq(basenm(path), GCHANGESET)) {
			free(path);
			if (streq(proj_rootkey(0), rootkey)) {
				key[-1] = ' ';
				pcsets = addLine(pcsets, strdup(rootkey));
				continue;
			}
			unless (path = mdbm_fetch_str(idDB, rootkey)) continue;
			path = name2sccs(path);
			if (exists(path)) {
				/* only save present files */
				key[-1] = ' ';
				hash_storeStr(ccsets, rootkey, 0);
				++ncsets;
			}
			free(path);
			continue;
		}
		free(path);

		/* must be the rootkey for a normal file */
		s = sccs_keyinit(0, rootkey, SILENT|INIT_NOCKSUM, idDB);
		if (s) {
			if (key && sccs_findKey(s, key)) {
				/* found rootkey & deltakey */
				key[-1] = ' ';
				printf("%s\n", rootkey);
			} else {
				/* found rootkey, but not deltakey */
				printf("%s\n", rootkey);
			}
			sccs_free(s);
		}
	}
	mdbm_close(idDB);

	unless (pcsets || ncsets) return (0);

	/* product cset file */
	s = sccs_csetInit(SILENT|INIT_NOCKSUM);
	EACH(pcsets) {
		rootkey = pcsets[i];
		if (key = separator(rootkey)) *key++ = 0;

		if (sccs_findKey(s, key)) {
			/* found rootkey & deltakey */
			key[-1] = ' ';
			printf("%s\n", rootkey);
		} else {
			/* found rootkey, but not deltakey */
			printf("%s\n", rootkey);
		}
	}
	freeLines(pcsets, free);

	if (ncsets) {
		sccs_rdweaveInit(s);
		while (t = sccs_nextdata(s)) {
			if (*t == '\001') continue;

			/* find component deltas */
			if (hash_fetchStr(ccsets, t)) {
				printf("%s\n", t);
				hash_deleteStr(ccsets, t);
				unless (--ncsets) break; /* found all? */
			}
		}
		sccs_rdweaveDone(s);

		/*
		 * For any remaining lines, we have the component, but
		 * not the delta
		 */
		EACH_HASH(ccsets) {
			rootkey = ccsets->kptr;
			if (key = separator(rootkey)) *key++ = 0;
			printf("%s\n", rootkey);
			if (key) key[-1] = ' ';
		}
		hash_free(ccsets);
	}
	sccs_free(s);
	return (0);
}

/*
 * An optional last part of the bkd connection for clone and pull that
 * examines the incoming data and requests BAM keys that are missing.
 * from the local server.
 * This extra pass is only called if BKD_BAM=YES indicating that the
 * remote bkd has BAM data, and if we share the same BAM server then
 * we won't request data.
 */
int
bkd_BAM_part3(remote *r, char **envVar, int quiet, char *range)
{
	FILE	*f;
	int	i, bytes, rc = 1, err;
	u64	sfio;
	char	*p;
	char	cmd_file[MAXPATH];
	char	buf[BSIZE];	/* must match remote.c:doit()/buf */

	if ((r->type == ADDR_HTTP) && bkd_connect(r, 0)) {
		return (-1);
	}
	bktmp(cmd_file);
	f = fopen(cmd_file, "w");
	assert(f);
	sendEnv(f, envVar, r, 0);
	/*
	 * No need to do "cd" again if we have a non-http connection
	 * because we already did a "cd" in part 1
	 */
	if (r->type == ADDR_HTTP) add_cd_command(f, r);

	if (bp_fetchData() == 2) {
		/* we do want to recurse here, havekeys did */
		fprintf(f, "bk -zo0 -Bstdin sfio -oqB -\n");
	} else {
		fprintf(f, "bk -zo0 -Bstdin sfio -oqBl -\n");
	}
	if (rc = bp_sendkeys(f, range, &sfio, r->gzip)) {
		fclose(f);
		goto done;
	}
	fclose(f);
	rc = send_file(r, cmd_file, 0);
	if (rc) goto done;

	if (r->type == ADDR_HTTP) skip_http_hdr(r);
	unless (r->rf) r->rf = fdopen(r->rfd, "r");

	getline2(r, buf, sizeof (buf));
	if (streq("@SERVER INFO@", buf)) {
		if (getServerInfo(r, 0)) {
			rc = 1;
			goto done;
		}
	} else {
		/* Put it back */
		ungetc('\n', r->rf);
		for (i = strlen(buf)-1; i >= 0 ; i--) ungetc(buf[i], r->rf);
	}

	f = 0;
	buf[0] = 0;
	while (fnext(buf, r->rf) &&
	    (strneq(buf, "@STDERR=", 8) || strneq(buf, "@STDOUT=", 8))) {
		err = (buf[4] == 'E'); /* to stderr? */
		bytes = atoi(&buf[8]);
		if (bytes == 0) break;
		while (bytes > 0) {
			i = min(sizeof(buf), bytes);
			if  ((i = fread(buf, 1, i, r->rf)) <= 0) break;
			if (err) {
				fwrite(buf, 1, i, stderr);
			} else if (sfio) {
				unless (f) {
					p = aprintf("bk sfio -i%sBb%s -",
					    quiet ? "q":"", psize(sfio));
					unless (quiet || !progress_isMulti()) {
						progress_nlneeded();
					}
					f = popen(p, "w");
					free(p);
					assert(f);
				}
				fwrite(buf, 1, i, f);
			}
			bytes -= i;
		}
	}
	if (f && pclose(f)) {
		rc = 1;
		goto done;
	}
	unless (streq(buf, "@EXIT=0@\n")) {
		fprintf(stderr, "bad exit in part3: %s\n", buf);
		rc = 1;
		goto done;
	}
	// XXX error handling

	rc = 0;
done:	unlink(cmd_file);
	return (rc);
}

int
bp_sendkeys(FILE *fout, char *range, u64 *bytep, int gzip)
{
	FILE	*f;
	FILE	*zout;
	int	debug, len, space, where, ret;
	int	rc = 0;
	char	*cmd, *p;
	char	line[MAXLINE];
	char	buf[BSIZE];	/* must match remote.c:doit() buf */

	debug = ((p = getenv("_BK_BAM_DEBUG")) && *p);
	*bytep = 0;
	zout = fopen_zip(fout, "wh", gzip);
	if (ret = bp_fetchData()) {
		/*
		 * If we have a server then we want to recurse one level up
		 * to the server because we don't need the data here, we just
		 * need to be able to get it from somewhere.  So that's the
		 * R1 option to havekeys.
		 */
		cmd = aprintf("bk changes -SBv -nd'$if(:BAMHASH:)"
		    "{|:BAMSIZE:|:BAMHASH: :KEY: :MD5KEY|1.0:}' %s |"
		    "bk havekeys -B%s -", range, ret == 2 ? "" : "l");
		f = popen(cmd, "r");
		assert(f);
		space = sizeof(buf);
		where = 0;
		while (fnext(line, f)) {
			if (debug) ttyprintf("%s", line);
			p = strchr(line+1, '|'); /* skip size */
			*p++ = 0;
			*bytep += atoi(line+1);
			len = strlen(p);
			if (len > space) {
				fprintf(zout, "@STDIN=%u@\n", where);
				fwrite(buf, 1, where, zout);
				space = sizeof(buf);
				where = 0;
			}
			strcpy(&buf[where], p);
			where += len;
			space -= len;
		}
		if (pclose(f)) rc = 1;
		if (where) {
			fprintf(zout, "@STDIN=%u@\n", where);
			fwrite(buf, 1, where, zout);
		}
	}
	fprintf(zout, "@STDIN=0@\n");
	if (fclose(zout)) rc = 1;
	return (rc);
}
