/* A safer version of chdir, which returns back to the
   initial working directory when the program exits.  */

#include <errno.h>
#include <stdlib.h>
#include <unistd.h>

static char *initial_wd;

static void
restore_wd (void)
{
  chdir (initial_wd);
}

int
chdir_safer (char const *dir)
{
  if (! initial_wd)
    {
      size_t s;
      for (s = 256;  ! (initial_wd = getcwd (0, s));  s *= 2)
	if (errno != ERANGE)
	  return -1;
      if (atexit (restore_wd) != 0)
	{
	  free (initial_wd);
	  initial_wd = 0;
	  return -1;
	}
    }

  return chdir (dir);
}
