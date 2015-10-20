/*
 * liblines - interfaces for autoexpanding data structures
 *
 */
#define	LSIZ(s)				p2int(s[0])
#define	p2int(p)			((int)(long)(p))
#define	int2p(i)			((void *)(long)(i))
#define	EACH_INDEX(s, i)	for (i=1; (s) && (i < LSIZ(s)) && (s)[i]; i++)
#define	EACH(s)				EACH_INDEX(s, i)

char	**addLine(char **space, void *line);
char	**allocLines(int n);
void	freeLines(char **space, void(*freep)(void *ptr));
