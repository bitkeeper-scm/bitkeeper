#include "system.h"
#include "sccs.h"
WHATSTR("@(#)%K%");

static void	print_name(char *);

/*
 * g2sccs - convert gfile names to sfile names
 */
int
g2sccs_main(int ac, char **av)
{
	int	i;

	if (ac > 1) {
		for (i = 1; i < ac; ++i) {
			print_name(av[i]);
		}
	} else {
		char	buf[MAXPATH];

		while (fnext(buf, stdin)) {
			chop(buf);
			print_name(buf);
		}
	}
	return (0);
}

static void
print_name(char *name)
{
	name = name2sccs(name);
	printf("%s\n", name);
	free(name);
}

#ifdef OLD
/*
 * Take a file name such as foo.c and return SCCS/s.foo.c
 * Also works for /full/path/foo.c -> /fullpath/SCCS/s.foo.c.
 * It's up to the caller to free() the resulting name.
 */
static char	*
name2sccs(char *name)
{
	int	len = strlen(name);
	char	*s, *newname;

	/* maybe it has the SCCS in it already */
	s = rindex(name, '/');
	if ((s >= name + 4) && strneq(s - 4, "SCCS/", 5)) {
		if ((s[1] != 's') && (s[2] == '.')) {
			switch (s[1]) {
			    case 'p':
			    case 'r':
			    case 'x':
			    case 'z':
			    	break;
			    default:
				assert(name == "Bad name");
			}
			name = strdup(name);
			s = strrchr(name, '/');
			s[1] = 's';
			return (name);
		} else {
			return (strdup(name));
		}
	}
	newname = malloc(len + 8);
	assert(newname);
	strcpy(newname, name);
	if ((s = rindex(newname, '/'))) {
		s++;
		strcpy(s, "SCCS/s.");
		s += 7;
		strcpy(s, rindex(name, '/') + 1);
	} else {
		strcpy(s = newname, "SCCS/s.");
		s += 7;
		strcpy(s, name);
	}
	return (newname);
}

static char
chop(register char *s)
{
	char	c;

	while (*s++);
	c = s[-2];
	s[-2] = 0;
	return (c);
}
#endif /* OLD */
