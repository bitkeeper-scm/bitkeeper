/* Copyright (c) 1997 L.W.McVoy */  
#include "sccs.h"
#include "dirent.h"
WHATSTR("%W%");   

#include "comments.c"
			
int
sccs_mv(char *name, char *dest, int isDir, int isDelete)
{
	char	path[MAXPATH];
	char	path2[MAXPATH];
	char 	buf[1024], *p, *q;
	char	*gfile, *sfile, *nrev = 0;
	sccs	*s;
	delta	*d;
	int	error = 0, wasEdited = 0;
	pfile	pf;
	int	flags = SILENT|FORCE;; 

	s = sccs_init(name, NOCKSUM);
	unless (HAS_SFILE(s)) {
		fprintf(stderr, "sccsmv: not an SCCS file: %s\n", name);
		sccs_free(s);
		return (1);
	}
	if (isDir) {
		sprintf(path, "%s/SCCS/s.%s", dest, basenm(s->gfile));
		sfile = path;
		sprintf(path2, "%s/%s", dest, basenm(s->gfile));
		gfile = path2;
	} else {
		char *t;
		t = name2sccs(dest);
		sfile = strdup(sPath(t, 0));
		gfile = sccs2name(t);
		free(t);
	}
	if (exists(sfile)) {
		fprintf(stderr, "sccsmv: destination %s exists\n", sfile);
		return (1);
	}
	if (exists(gfile)) {
		fprintf(stderr, "sccsmv: destination %s exists\n", gfile);
		return (1);
	}
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
		if (isEmptyDir(s->sfile)) rmDir(s->sfile);
		q = strrchr(s->sfile, '/');
		*p = '/';
		if (q) {
			*q = 0;
			if (isEmptyDir(s->sfile)) rmDir(s->sfile);
			*q = '/';
		} else {
			if (isEmptyDir(".")) rmDir(".");
		}
	}

	/*
	 * XXX TODO: we should store the rename comment 
	 * somewhere, such as a .comment file ?
	 */
	if (HAS_PFILE(s) && !isDelete) goto out;
	sccs_free(s);
	/* For split root config; We recompute sfile here */  
	free(sfile);
	sfile = name2sccs(dest);
	unless (s = sccs_init(sfile, 0)) { error++; goto out; }
	unless (HAS_PFILE(s)) {
		if (sccs_get(s, 0, 0, 0, 0, SILENT|EDIT, "-")) {
			error = 1;
			goto out;
		}
		s = sccs_restart(s);
	}
	if (isDelete) {
		sprintf(buf, "Delete"); 
	} else {
		sprintf(buf, "Rename: %s -> %s", name, dest); 
	}
	comment = buf;
	gotComment = 1;
	unless (s && (d = getComments(0))) {
		error = 1;
		goto out;
	}
	if (sccs_delta(s, flags, d, 0, 0) == -1) error = 1;
out:	if (s) sccs_free(s);
	unless (isDir) { free(sfile); free(gfile); }
	if (gotComment) commentsDone(saved);
	return (error);
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
				sprintf(cmd, "mkdir -p %s", dest);
				if (system(cmd)) return (1);
			}
			*p = '/';
		}
		sprintf(cmd, "mv %s %s", src, dest);
		if (system(cmd)) return (1);
	}
	return (0);
}

int rmDir(char *dir)
{
	rmdir(dir);
	if (exists(dir)) {
		char cmd[1024];
		sprintf(cmd, "cd /; rmdir %s", fullname(dir, 0));
		system(cmd);
	}
}

isEmptyDir(char *dir)
{
        DIR *d;
        struct dirent *e;

        d = opendir(dir);
        unless (d) {
                perror(dir);
                return 0;
        }

        while ( e = readdir(d)) {
                if (streq(e->d_name, ".") ||
                    streq(e->d_name, "..")) {
                        continue;
                }
                closedir(d);
                return 0;
        }
        closedir(d);
        return 1;
}          
