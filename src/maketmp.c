#include "system.h"
#include "sccs.h"

int
maketmp(char *template) 
{
	int fd;
	
	if ((fd = mkstemp(template)) < 0) {
		fprintf(stderr, "can'nt create tmp file: %s\n", template);
		return 0;
	}
	close(fd);
	return (1);
}
