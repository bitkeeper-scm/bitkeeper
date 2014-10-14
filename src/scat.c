#include "sccs.h"

/*
 * This works even if there isn't a gfile.
 */
int
scat_main(int ac, char **av)
{
	sccs	*s;
	char	*sfile;
	size_t	len;
	char	*buf;

	unless (av[1] && !av[2]) {
		fprintf(stderr, "usage: %s sfile\n", prog);
		return (1);
	}
	sfile = name2sccs(av[1]);
	unless (s = sccs_init(sfile, SILENT|INIT_NOCKSUM|INIT_MUSTEXIST)) {
		fprintf(stderr, "%s: can't open sfile %s\n", prog, sfile);
		return (1);
	}
	buf = sccs_scat(s, &len);
	fwrite(buf, 1, len, stdout);
	sccs_free(s);
	return (0);
}

char *
sccs_scat(sccs *s, size_t *len)
{
	char	*ret;
	int	orig = s->encoding_out;

	assert(!s->mem_out);

	s->encoding_out = sccs_encoding(s, 0, 0);
	s->encoding_out &= ~(E_FILEFORMAT|E_COMP);
	s->mem_out = 1;
	sccs_newchksum(s);
	ret = fmem_close(s->outfh, len);
	s->outfh = 0;
	s->mem_out = 0;
	s->encoding_out = orig;
	return (ret);
}
