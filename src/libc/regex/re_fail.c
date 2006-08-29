#ifdef vms
#include stdio
#else
#include <stdio.h>
#include <stdlib.h>
#endif

/* 
 * re_fail:
 *	default internal error handler for re_exec.
 *
 *	should probably do something like a longjump to recover
 *	gracefully.
 */ 
void	
re_fail(char *s, char c)
{
	fprintf(stderr, "%s [opcode %o]\n", s, c);
	exit(1);
}
