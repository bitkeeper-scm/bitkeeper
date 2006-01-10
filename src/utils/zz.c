#include "system.h"
#include "../zlib/zlib.h"

int
main(int ac, char **av)
{
	GZIP	*gz = gzopen(av[1], "wb");
	int	n;
	char	buf[BUFSIZ];

	setmode(0, _O_BINARY);
	while ((n = read(0, buf, sizeof(buf))) > 0) {
		gzwrite(gz, buf, n);
	}
	gzclose(gz);
	exit(0);
}
