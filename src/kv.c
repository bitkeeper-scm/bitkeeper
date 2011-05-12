#include "sccs.h"

/* bk _getkv <kvfile> <key> > DATA */
/* bk _getkv <kvfile> | while read key; do bk _getkv <kvfile> $key > x; done */
int
getkv_main(int ac, char **av)
{
	char	*file;
	char	*key;
	hash	*h;
	FILE	*f;
	int	ret = 2;

	unless (av[1] && (!av[2] || !av[3])) {
		fprintf(stderr, "usage: %s <kvfile> <key>\n", prog);
		return (1);
	}
	file = av[1];
	key = av[2];

	f = streq(file, "-") ? stdin : fopen(file, "r");
	unless (f) {
		fprintf(stderr, "%s: unable to open %s\n", prog, file);
		return (1);
	}
	if (h = hash_fromStream(0, f)) {
		unless (key) {
			EACH_HASH(h) puts(h->kptr);
			ret = 0;
		} else if (hash_fetchStr(h, key)) {
			write(1, h->vptr, h->vlen-1);
			ret = 0;
		}
		hash_free(h);
	}
	unless (streq(file, "-")) fclose(f);
	return (ret);

}

/* bk _setkv <kvfile> <key> < DATA */
int
setkv_main(int ac, char **av)
{
	char	*file;
	char	*key;
	hash	*h;
	int	i;
	FILE	*f;
	char	*t;
	size_t	len;
	char	buf[MAXLINE];

	unless (av[1] && av[2] && !av[3]) {
		fprintf(stderr, "usage: %s <kvfile> <key>\n", prog);
		return (1);
	}
	file = av[1];
	key = av[2];

	h = hash_new(HASH_MEMHASH);
	h = hash_fromFile(h, file);

	f = fmem();
	while ((i = read(0, buf, sizeof(buf))) > 0) {
		fwrite(buf, 1, i, f);
	}
	t = fmem_peek(f, &len);
	hash_store(h, key, strlen(key)+1, t, len+1);
	fclose(f);

	if (hash_toFile(h, file)) {
		perror(file);
		return (1);
	}
	return (0);

}
