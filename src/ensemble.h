#ifndef _NESTED_H
#define _NESTED_H

typedef struct {
	void	**repos;		/* addlines of pointers to repo's */
	char	*rootkey;		/* points at repos[i]->rootkey */
	char	*deltakey;		/* ditto */
	char	*path;			/* ditto */
	int	index;			/* used by the iterator */
} repos;

repos*	ensemble_list(sccs *sc, char *rev, int product_too);
repos*	ensemble_first(repos *repos);
repos*	ensemble_next(repos *repos);
repos*	ensemble_find(repos *repos, char *rootkey);
void	ensemble_free(repos *repos);

int	ensemble_each(int quiet, int ac, char **av);

#define	EACH_REPO(c)	for (ensemble_first(c); (c)->index; ensemble_next(c))

#endif /* _NESTED_H */
