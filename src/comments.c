/* Copyright (c) 1998-2000 L.W.McVoy */
#include "system.h"
#include "sccs.h"

private	char	**saved;	/* saved copy of comments across files */
private	char	*comment;	/* a placed to parse the comment from */
private	int	gotComment;	/* seems redundant but it isn't, this is
				 * is how we know we have a null comment.
				 */

void
comments_save(char *s)
{
	comment = s;
	gotComment = 1;
}

int
comments_got()
{
	return (gotComment);
}

void
comments_done()
{
	int	i;

	if (!saved) return;
	EACH(saved) free(saved[i]);
	free(saved);
	saved = 0;
	gotComment = 0;
	comment = 0;
	/* XXX - NULL comment as well? */
}

delta *
comments_get(delta *d)
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
