#define   FSLAYER_NODEFINES
#include "system.h"

/*
 * This is only called by libc/fslayer/fslayer_rmIfRepo_stub.c
 * which is a stub function loaded by the installer.  The point
 * is in a non-remapped environment, like the installer, the callback
 * is a NOP, so rmtree() works as a normal 'rm -rf'.
 * BK never calls this.  It uses src/fslayer.c:fslayer_rmIfRepo().
 */
int
rmIfRepo(char *dir)
{
	return (0);
}
