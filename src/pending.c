#include "system.h"
#include "sccs.h"

extern char *bin, *pager;    

int
pending_main(int ac, char **av) 
{
	char buf[MAXLINE];

	platformInit();
	cd2root();
	sprintf(buf, "%sbk sfiles -CA | %sbk sccslog - | %s", bin, bin, pager);
	return(system(buf));
}
