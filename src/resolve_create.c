/*
 * resolve_create.c - resolver for created files
 */
#include "resolve.h"

/*
 * Given an SCCS file, resolve the create.
 *	sfile conflicts can be handled by adding the sfile to the patch.
 *	gfile conflicts are not handled yet.
 */
resolve_create(opts *opts, sccs *s, int type)
{
	delta	*d = sccs_getrev(s, "+", 0, 0);
	char	*sfile = name2sccs(d->pathname);
	int	ret;

	if (type == GFILE_CONFLICT) {
		ret = resolve_gfile_create(opts, s, d, sfile);
	} else {
		ret = resolve_sfile_create(opts, s, d, sfile);
	}
	free(sfile);
	return (ret);
}

/*
 * Called when we have a gfile in the repository but no corresponding sfile.
 */
resolve_gfile_create(opts *opts, sccs *s, delta *d, char *sfile)
{
	int	explain = 0;
	int	help = 1;
	char	*pager;
	int	n = 0;
	char	buf[MAXPATH];
	char	cmd[MAXPATH*2];

	if (opts->debug) fprintf(stdlog, "gfile_create(%s)\n", d->pathname);

	pager = getenv("PAGER") ? getenv("PAGER") : "more";

	while (1) {
		if (++n == 100) exit(1);
		if (explain) {
			explain = 0;
			fprintf(stderr,
"There is a local working file:\n\
\t%s\n\
which has no associated revision history file.\n\
The patch you are importing has a file which wants to be in the same\n\
place as the local file.  Your choices are:\n\
a) do not move the local file, which means that the entire patch will\n\
   be aborted, discarding any other merges you may have done.  You can\n\
   then check in the local file and retry the patch.  This is the best\n\
   choice if you have done no merge work yet.\n\
b) remove the local file after making sure it is not something that you\n\
   need.  There are commands you can run now to show you both files.\n\
c) move the local file to some other pathname.\n\
d) move the remote file to some other pathname.\n\
\n\
The choices b, c, or d will allow the file in the patch to be created\n\
and you to continue with the rest of the patch.\n\
\n\
Warning: choices b and c are not recorded because there is no SCCS file\n\
associated with the local file.  So if the rest of the resolve does not\n\
complete for some reason, it is up to you to go find that file and move it\n\
back by hand, if that is what you want.\n\n", d->pathname);
		}

		if (help) {
			help = 0;
			fprintf(stderr,
"---------------------------------------------------------------------------\n\
Local file: ``%s'' conflicts with new file in patch.\n\
Local file is not under revision control.\n\
---------------------------------------------------------------------------\n\
\nCommands are:\n\
\n\
?	- print this help\n\
a	- abort the patch\n\
d	- diff the local file against the remote file\n\
e	- explain the choices\n\
ml	- move the local file to someplace else\n\
mr	- move the remote file to someplace else\n\
r	- remove the local file\n\
vl	- view the local file\n\
vr	- view the remote file\n\n", d->pathname);
		}

		fprintf(stderr, "%s>> ", d->pathname);
		getline(0, buf, sizeof(buf));
		if (streq(buf, "?")) {
			help = 1;
		} else if (streq(buf, "a")) {
			if (confirm("Abort patch?")) {
				resolve_cleanup(opts, CLEAN_RESYNC);
			}
		} else if (streq(buf, "d")) {
			sprintf(cmd, "bk get -ksp %s | bk diff %s/%s - | %s",
			    s->gfile, RESYNC2ROOT, d->pathname, pager);
			system(cmd);
		} else if (streq(buf, "e")) {
			explain = 1;
		} else if (streq(buf, "ml")) {
			char	*sfile2;

			unless (prompt("Move local file to:", buf)) continue;
			sfile2 = name2sccs(buf);
			sprintf(cmd, "%s/%s", RESYNC2ROOT, buf);
			if (exists(cmd)) {
				fprintf(stderr, "%s exists already\n", buf);
				continue;
			}
			sprintf(cmd, "%s/%s", RESYNC2ROOT, sfile2);
			free(sfile2);
			if (exists(cmd)) {
				fprintf(stderr, "%s exists already\n", buf);
				continue;
			}
			chdir(RESYNC2ROOT);
			if (rename(d->pathname, buf)) {
				perror("rename");
				exit(1);
			}
			chdir(ROOT2RESYNC);
			return (EAGAIN);
		} else if (streq(buf, "mr")) {
			names	names;

			unless (prompt("Move remote file to:", buf)) continue;
			if (sccs_filetype(buf) != 's') {
				char	*t = name2sccs(buf);

				strcpy(buf, t);
				free(t);
			}
			if (exists(buf)) {
				fprintf(stderr, "%s exists already\n", buf);
				continue;
			}
			if (move_remote(opts, s, buf)) {
				perror("move_remote");
				exit(1);
			}
			return (0);
		} else if (streq(buf, "r")) {
			unless (confirm("Remove local file?")) continue;
			sprintf(buf, "%s/%s", RESYNC2ROOT, d->pathname);
			unlink(buf);
			if (opts->log) fprintf(stdlog, "unlink(%s)\n", buf);
			return (EAGAIN);
		} else if (streq(buf, "vl")) {
		} else if (streq(buf, "vn")) {
		} else {
			help = 1;
		}
	}
}

resolve_sfile_create(opts *opts, sccs *s, delta *d, char *sfile)
{
	if (opts->debug) fprintf(stdlog, "sfile_create(%s)\n", d->pathname);
}
