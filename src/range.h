#ifndef	_RANGE_H
#define	_RANGE_H

typedef struct range RANGE;
struct	range {
	char	*rstart, *rstop;
	u32	isdate:1;
	u32	isrev:1;
};

#define RANGE_ENDPOINTS	0x10
#define	RANGE_SET	0x20

int	range_process(char *me, sccs *s, u32 flags, RANGE *rargs);
int	range_addArg(RANGE *rargs, char *arg, int isdate);

void	range_cset(sccs *s, delta *d);
time_t	range_cutoff(char *spec);
void	range_markMeta(sccs *s);
int	range_gone(sccs *s, delta *d, u32 dflags);

int	range_walkrevs(sccs *s, delta *from, delta *to,
    int (*fcn)(sccs *s, delta *d, void *token), void *token);
int	walkrevs_setFlags(sccs *s, delta *d, void *token);
int	walkrevs_printkey(sccs *s, delta *d, void *token);
int	walkrevs_printmd5key(sccs *s, delta *d, void *token);

#endif
