/* Copyright (c) 1998 L.W.McVoy */

/*
 * globals because it was easier for getComments.
 */
char	**saved;	/* saved copy of comments across files */
char	*comment;
int	gotComment;
int	sccs_getComments(char *file, char *rev, delta *n);

private void
commentsDone(char **s)
{
	int	i;

	if (!s) return;
	EACH(s) free(s[i]);
	free(s);
}

private delta *
getComments(delta *d)
{
	int	i;

	unless (d) d = calloc(1, sizeof(*d));
	if (!comment && gotComment) return (d);
	if (!comment) {
		if (saved) {
			EACH(saved) {
				d->comments =
				    addLine(d->comments, strdup(saved[i]));
			}
			return (d);
		}
		if (sccs_getComments("Group comments", 0, d)) {
			return (0);
		}
		EACH(d->comments) {
			saved = addLine(saved, strdup(d->comments[i]));
		}
	} else {
		d = sccs_parseArg(d, 'C', comment, 0);
	}
	if (d && (d->flags & D_ERROR)) {
		sccs_freetree(d);
		return (0);
	}
	return (d);
}
