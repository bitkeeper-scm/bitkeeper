/* Copyright (c) 1999 Andrew Chang */  
#include "system.h"
#include "sccs.h"
WHATSTR("%W%");   

#include "comments.c"

char *getRelativeName(char *);
void rmDir(char *);
int mv(char *, char *);
			
int
sccs_mv(char *name, char *dest, int isDir, int isDelete)
{
	char 	*p, *q, *t, *destfile, *oldpath;
	char	*gfile, *sfile, *nrev = 0;
	char	buf[1024], commentBuf[MAXPATH*2];
	sccs	*s;
	delta	*d;
	int	error = 0, wasEdited = 0;
	pfile	pf;
	int	flags = SILENT|DELTA_FORCE; 

	unless (s = sccs_init(name, INIT_NOCKSUM, 0)) return (1);
	unless (HAS_SFILE(s)) {
		fprintf(stderr, "sccsmv: not an SCCS file: %s\n", name);
		sccs_free(s);
		return (1);
	}
	if (isDir) {
		sprintf(buf, "%s/%s", dest, basenm(s->gfile));
	} else {
		strcpy(buf, dest);
	}
	destfile = name2sccs(buf);

	t = name2sccs(destfile);
	sfile = strdup(sPath(t, 0));
	gfile = sccs2name(t);
	free(t);

	if (exists(sfile)) {
		fprintf(stderr, "sccsmv: destination %s exists\n", sfile);
		return (1);
	}
	if (exists(gfile)) {
		fprintf(stderr, "sccsmv: destination %s exists\n", gfile);
		return (1);
	}
	sccs_close(s); /* close the file before we move it - win32 restriction */
	oldpath = getRelativeName(name);
	if (isDelete) {
		sprintf(commentBuf, "Delete: %s", oldpath); 
	} else {
		char *newpath;

		newpath = getRelativeName(destfile);
		sprintf(commentBuf, "Rename: %s -> %s", oldpath, newpath); 
		free(newpath);
	}
	free(oldpath);

	error |= mv(s->sfile, sfile);
	if (!error && exists(s->gfile)) error = mv(s->gfile, gfile);
	if (HAS_PFILE(s) && !error) {
		p = strrchr(sfile, '/');
		p = p ? &p[1]: sfile;
		assert(*p == 's');
		*p = 'p';
		error = mv(s->pfile, sfile);
		*p = 's';
	}
	if (error) goto out;
	/*
	 * Remove the parent directory of "name",
	 * If it is empty after the moves.
	 * XXX TODO: for split root, check the G tree too..
	 */
	p = strrchr(s->sfile, '/');
	if (p) {
		*p = 0;
		if (emptyDir(s->sfile)) rmDir(s->sfile);
		q = strrchr(s->sfile, '/');
		*p = '/';
		if (q) {
			*q = 0;
			if (emptyDir(s->sfile)) rmDir(s->sfile);
			*q = '/';
		} else {
			if (emptyDir(".")) rmDir(".");
		}
	}

	/*
	 * XXX TODO: we should store the rename comment 
	 * somewhere, such as a .comment file ?
	 */
	if (HAS_PFILE(s) && !isDelete) goto out;
	sccs_free(s);
	/* For split root config; We recompute sfile here */  
	/* we do'nt want the sPath() adjustment		  */
	free(sfile);
	sfile = name2sccs(destfile);
	unless (s = sccs_init(sfile, 0, 0)) { error++; goto out; }
	unless (HAS_PFILE(s)) {
		if (sccs_get(s, 0, 0, 0, 0, SILENT|GET_EDIT, "-")) {
			error = 1;
			goto out;
		}
		s = sccs_restart(s);
	}

	comment = commentBuf;
	gotComment = 1;
	unless (s && (d = getComments(0))) {
		error = 1;
		goto out;
	}

	if (sccs_delta(s, flags, d, 0, 0) == -1) error = 1;
out:	if (s) sccs_free(s);
	free(destfile); free(sfile); free(gfile); 
	if (gotComment) commentsDone(saved);
	return (error);
}

char *
getRelativeName(char *name)
{
	char	*t, *rpath;

	/* TODO: we should cache the root value for faster lookup */
	t = sccs2name(name);
	rpath = strdup((char *) _relativeName(t, 0, 0, 0, 0));
	free(t);
	return rpath;
}

int
mv(char *src, char *dest)
{
	//fprintf(stderr, "moving %s -> %s\n", src, dest);
	if (rename(src, dest)) {	/* try mv(1) */
		char	*p, cmd[MAXPATH*2 + 5];

		p = strrchr(dest, '/');
		
		if (p) {
			*p = 0;
			if (!exists(dest)) {
				if (mkDir(dest)) return (1);
			}
			*p = '/';
		}
#ifdef WIN32
		/* win32 rename works across devices */
		if (rename(src, dest)) return (1);
#else
		if (rename(src, dest)) { 	/* try mv(1) */
			sprintf(cmd, "/bin/mv %s %s", src, dest);
			if (system(cmd)) return (1);
		}
	}
#endif
	return (0);
}

void
rmDir(char *dir)
{
	if (streq(".", dir) || samepath(".", dir)) return;
	//fprintf(stderr, "removing %s\n", dir);
	rmdir(dir);
}

