#include "sccs.h"

private	int	catSfile(sccs *s);

/*
 * This works even if there isn't a gfile.
 */
int
scat_main(int ac, char **av)
{
	sccs	*s;
	char	*sfile;

	unless (av[1] && !av[2]) {
		fprintf(stderr, "usage: %s sfile\n", prog);
		return (1);
	}
	sfile = name2sccs(av[1]);
	unless (s = sccs_init(sfile, SILENT|INIT_NOCKSUM|INIT_MUSTEXIST)) {
		fprintf(stderr, "%s: can't open sfile %s\n", prog, sfile);
		return (1);
	}
	s->encoding_out = sccs_encoding(s, 0, 0);
	s->encoding_out &= ~(E_BK|E_COMP);
	catSfile(s);
	sccs_free(s);
	return (0);
}

private	int
catSfile(sccs *s)
{
	size_t	len;
	char	*buf;

	assert (!s->mem_in && !s->mem_out);

	sccs_encoding(s, 0, 0);
	s->mem_out = 1;
	sccs_newchksum(s);

	assert(s->mem_in);
	s->mem_in = s->mem_out = 0;
	fclose(s->outfh);
	s->outfh = 0;

	buf = fmem_close(s->fh, &len);
	s->fh = 0;
	fwrite(buf, 1, len, stdout);
	free(buf);
	return (0);
}
