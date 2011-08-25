/* Copyright (c) 2001 L.W.McVoy */
#include "system.h"
#include "sccs.h"
#include "bam.h"
#include "progress.h"

private int	newroot(char *ranbits, int bk4, char *comment, char *who);
private void	update_rootlog(sccs *s, char *key, char *comment, char *who);

int
newroot_main(int ac, char **av)
{
	int	c, bk4 = 0;
	char	*ranbits = 0;
	char	*comments = 0;
	char	*who = 0;
	u8	*p;

	while ((c = getopt(ac, av, "4k:qvw:y:", 0)) != -1) {
		switch (c) {
		    case '4': bk4 = 1; break;
		    case 'k': ranbits = optarg; break;
		    case 'q': /* nop */; break;
		    case 'v': /* nop */; break;
		    case 'w': who = optarg; break;
		    case 'y': comments = optarg; break;
		    default: bk_badArg(c, av);
		}
	}
	unless (comments) comments = "newroot command";
	if (strchr(comments, '\n')) {
		fprintf(stderr, "ERROR: -y comment must only be one line\n");
		usage();
	}
	if (ranbits && !strneq("B:", ranbits, 2)) {
		if (strlen(ranbits) > 16) {
k_err:			fprintf(stderr,
			    "ERROR: -k option can have at most 16 lower case "
			    "hex digits\n");
			usage();
		}
		for (p = ranbits; *p; p++) {
			unless (isxdigit(*p)) break;
			if (isupper(*p)) break;
		}
		if (*p) goto k_err;
	}
	return (newroot(ranbits, bk4, comments, who));
}

/*
 * Generate a new ROOTKEY
 * Update the csetfile pointer in all files.
 * XXX - what if we are a component, should newroot be allowed?
 * Prolly not.
 */
private int
newroot(char *ranbits, int bk4, char *comments, char *who)
{
	sccs	*s;
	int	rc = 0;
	char	*p, *oldbamdir;
	char	*origrand;
	ticker	*tick = 0;
	char	cset[] = CHANGESET;
	char	buf[MAXPATH];
	char	key[MAXKEY];

	if (proj_cd2root()) {
		fprintf(stderr, "Cannot find package root.\n");
		exit(1);
	}
	if (proj_isComponent(0)) {
		fprintf(stderr, "May not newroot components.\n");
		exit(1);
	}
	unless ((s = sccs_init(cset, 0)) && HASGRAPH(s)) {
		fprintf(stderr, "Cannot init ChangeSet.\n");
		exit(1);
	}

	oldbamdir = bp_dataroot(0, 0);
	if (bp_hasBAM()) {
		/*
		 * If we change the repository rootkey will be invaliding
		 * bam data on any remote BAM server.  So all BAM data needs
		 * to be made local.
		 */
		if (system("bk bam server -rq")) {
			fprintf(stderr, "%s: failed to make BAM data local\n",
			    prog);
			return (1);
		}
	}
	unless (bk4) sccs_defRootlog(s);

	if (ranbits) {
		/* We're called from BAM convert, append the random bits */
		origrand = RANDOM(s, s->tree);
		if (strneq(ranbits, "B:", 2)) {
			p = origrand;
			if (strneq("B:",p, 2)) p = strrchr(p, ':') + 1;
			sprintf(buf, "%s%s", ranbits, p);
			assert(strlen(buf) < MAXPATH - 1);
		} else {
			p = buf;
			if (strneq("B:", origrand, 2)) {
				p = strrchr(origrand, ':');
				assert(p);
				strncpy(buf, origrand,
				    p - origrand + 1);
				p = buf + (p - origrand + 1);
			}
			if ((p - buf + strlen(ranbits)) > (MAXPATH - 1)) {
				fprintf(stderr, "Rootkey too long\n");
				exit(1);
			}
			strcpy(p, ranbits);
		}
	} else {
		randomBits(buf);
	}
	if (streq(buf, RANDOM(s, s->tree))) {
		fprintf(stderr, "newroot: error: new key matches old\n");
		exit (1);
	}
	RANDOM_SET(s, s->tree, buf);
	sccs_sdelta(s, sccs_ino(s), key);
	unless (bk4) update_rootlog(s, key, comments, who);
	sccs_newchksum(s);
	sccs_free(s);
	unlink("BitKeeper/log/ROOTKEY");
	unlink("BitKeeper/log/CSETFILE"); /* bk before 5.0 used this */
	proj_reset(0);

	/* move BAM data if location changed */
	if (isdir(oldbamdir)) {
		p = bp_dataroot(0, 0);
		unless (streq(oldbamdir, p)) rename(oldbamdir, p);
		free(p);
	}
	free(oldbamdir);
	if (bk4) {
		if (systemf("bk -r admin -C'%s'", key)) {
			fprintf(stderr,
			    "%s: admin -C failed\n", prog);
			rc = 1;
		}
	}

	if (tick) progress_done(tick, rc ? "FAILED" : "OK");
	return (rc);
}

private void
update_rootlog(sccs *s, char *key, char *comments, char *who)
{
	char	**oldtext;
	int	i;
	time_t	now = time(0);

	oldtext = s->text;
	s->text = 0;
	EACH (oldtext) {
		s->text = addLine(s->text, oldtext[i]);
		unless (streq(oldtext[i], "@ROOTLOG")) continue;

		if (who) {
			s->text = addLine(s->text, strdup(who));
		} else {
			s->text = addLine(s->text, aprintf("%s@%s %s%s",
				sccs_user(), sccs_host(),
				time2date(now), sccs_zone(now)));
		}
		s->text = addLine(s->text, strdup(comments));
		s->text = addLine(s->text, strdup(key));
	}
	if (oldtext) freeLines(oldtext, 0);
}

/*
 * Giving a sccs* for the repositories ChangeSet file, return the rootkey
 * from the last transform operation.
 * This means walking the ROOTLOG from newest to oldest and returning
 * the first rootkey found that is not an attach or detach (or any later
 * transform we do that is "invisible").
 */
void
sccs_syncRoot(sccs *s, char *key)
{
	int	i, x, keepit = 0;
	char	*p;

	key[0] = 0;

	/* find start of ROOTLOG */
	EACH(s->text) {
		if (streq(s->text[i], "@ROOTLOG")) goto foundit;
	}
	goto nolog;
foundit:
	x = 0;
	EACH_START(i+1, s->text, i) {
		/*
		 * the rootlog consists of 3 line for each entry:
		 *
		 *  i   <user>@<host> <data>
		 *  i+1 <comments>
		 *  i+2 <rootkey>
		 *
		 * we want to find the first record where the comments
		 * don't start with 'attach' or 'detach'
		 * Note: add a (syncok) marker in case we need a backwards
		 * compat way to newroot something that won't stop
		 * using port.
		 */
		x++;
		p = s->text[i];
		switch(x) {
		    case 1:
			if (*p == '@') goto nolog;
		    	break;
		    case 2:
			unless (strneq(p, "attach ", 7) ||
			    streq(p, "detach") ||
			    strstr(p, "(syncok)")) {
			    	keepit = 1;
			}
		    	break;
		    case 3:
			x = 0;
			if (keepit) {
				strcpy(key, p);
				return;
			}
		    	break;
		}
	}
nolog:	/* no ROOTLOG, just return old rootkey */
	sccs_sdelta(s, sccs_ino(s), key);
}


/*
 * Created the original ROOTLOG if it is missing
 */
int
sccs_defRootlog(sccs *cset)
{
	int	i;
	char	who[MAXKEY];
	char	key[MAXKEY];

	/* create initial ROOTLOG, if needed. */
	EACH (cset->text) {
		if (streq(cset->text[i], "@ROOTLOG")) {
			i = -1;
			break;
		}
	}
	unless (i == -1) {
		cset->text = addLine(cset->text, strdup("@ROOTLOG"));
		sccs_sdelta(cset, sccs_ino(cset), key);
		sprintf(who, "%s %s%s",
		    USERHOST(cset, cset->tree),
		    delta_sdate(cset, cset->tree),
		    ZONE(cset, cset->tree));
		update_rootlog(cset, key, "original", who);
		return (1);
	}
	return (0);
}
