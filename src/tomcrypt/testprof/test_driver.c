#include <tomcrypt_test.h>

void run_cmd(int res, int line, char *file, char *cmd)
{
   if (res != CRYPT_OK) {
      fprintf(stderr, "%s (%d)\n%s:%d:%s\n", error_to_string(res), res, file, line, cmd);
      exit(EXIT_FAILURE);
   }
}

/* $Source: /cvs/libtom/libtomcrypt/testprof/test_driver.c,v $ */
/* $Revision: 1.1 $ */
/* $Date: 2006/06/07 22:05:09 $ */
