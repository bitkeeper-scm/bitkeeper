#include "mycrypt.h"

void zeromem(void *dst, unsigned long len)
{
 unsigned char *mem = (unsigned char *)dst;
 while (len--)
    *mem++ = 0;
}

void burn_stack(unsigned long len)
{
   unsigned char buf[32];
   zeromem(buf, sizeof(buf));
   if (len > sizeof(buf))
      burn_stack(len - sizeof(buf));
}

static const char *ID_TAG = "mem.c";

