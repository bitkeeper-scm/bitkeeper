/* LibTomCrypt, modular cryptographic library -- Tom St Denis
 *
 * LibTomCrypt is a library that provides various cryptographic
 * algorithms in a highly modular and flexible manner.
 *
 * The library is free for all purposes without any express
 * guarantee it works.
 *
 * Tom St Denis, tomstdenis@gmail.com, http://libtomcrypt.com
 */
#ifndef WIN32
#include <unistd.h>
#include <sys/time.h>
#include <sys/times.h>
#endif

#include "tomcrypt.h"

/** 
   @file rng_get_bytes.c
   portable way to get secure random bits to feed a PRNG (Tom St Denis)
*/

#ifdef DEVRANDOM
/* on *NIX read /dev/random */
static unsigned long rng_nix(unsigned char *buf, unsigned long len, 
                             void (*callback)(void))
{
#ifdef LTC_NO_FILE
    return 0;
#else
    FILE *f;
    unsigned long x;
#ifdef TRY_URANDOM_FIRST
    f = fopen("/dev/urandom", "rb");
    if (f == NULL)
#endif /* TRY_URANDOM_FIRST */
       f = fopen("/dev/random", "rb");

    if (f == NULL) {
       return 0;
    }
    
    /* disable buffering */
    if (setvbuf(f, NULL, _IONBF, 0) != 0) {
       fclose(f);
       return 0;
    }   
 
    x = (unsigned long)fread(buf, 1, (size_t)len, f);
    fclose(f);
    return x;
#endif /* LTC_NO_FILE */
}

#endif /* DEVRANDOM */

/* on ANSI C platforms with 100 < CLOCKS_PER_SEC < 10000 */
#if defined(CLOCKS_PER_SEC)

#define ANSI_RNG

static unsigned long rng_ansic(unsigned char *buf, unsigned long len, 
                               void (*callback)(void))
{
   clock_t t1;
   int l, acc, bits, a, b;

   if (XCLOCKS_PER_SEC < 100 || XCLOCKS_PER_SEC > 10000) {
      return 0;
   }

   l = len;
   bits = 8;
   acc  = a = b = 0;
   while (len--) {
       if (callback != NULL) callback();
       while (bits--) {
          do {
             t1 = XCLOCK(); while (t1 == XCLOCK()) a ^= 1;
             t1 = XCLOCK(); while (t1 == XCLOCK()) b ^= 1;
          } while (a == b);
          acc = (acc << 1) | a;
       }
       *buf++ = acc; 
       acc  = 0;
       bits = 8;
   }
   acc = bits = a = b = 0;
   return l;
}

#endif 

/* Try the Microsoft CSP */
#ifdef WIN32
#undef _WIN32_WINNT		/* defined in src/build.sh */
#define _WIN32_WINNT 0x0400
#include <windows.h>
#include <wincrypt.h>

static unsigned long rng_win32(unsigned char *buf, unsigned long len, 
                               void (*callback)(void))
{
   HCRYPTPROV hProv = 0;
   if (!CryptAcquireContext(&hProv, NULL, MS_DEF_PROV, PROV_RSA_FULL, 
                            (CRYPT_VERIFYCONTEXT | CRYPT_MACHINE_KEYSET)) && 
       !CryptAcquireContext (&hProv, NULL, MS_DEF_PROV, PROV_RSA_FULL, 
                            CRYPT_VERIFYCONTEXT | CRYPT_MACHINE_KEYSET | CRYPT_NEWKEYSET))
      return 0;

   if (CryptGenRandom(hProv, len, buf) == TRUE) {
      CryptReleaseContext(hProv, 0);
      return len;
   } else {
      CryptReleaseContext(hProv, 0);
      return 0;
   }
}

#endif /* WIN32 */

#ifndef WIN32
/*
 * XXX rng_fake() is not compiled on Windows because it is not needed
 *     and because gettimeofday() is not in the standard library.
 */

static unsigned char *add_entropy(
	unsigned long seed, unsigned char *p,
	unsigned char *buf, unsigned long len)
{
        while (seed) {
                *p++ ^= (seed & 255);
                seed >>= 8;
                if ((p - buf) == len) p = buf;
        }
        return (p);
}

/*
 * A "fake" random seed generator.
 * This routine generates entropy to be used to seed a pseudo random
 * number generator.  Since it is just used for seed's the data returned
 * does not need to be mangled.
 *
 * XXX this code is still pretty weak
 */
static unsigned long rng_fake(unsigned char *buf, unsigned long len)
{
	unsigned char	*p = buf;
	struct timeval	tv;
	struct tms	tms;

	p = add_entropy(getpid(), p, buf, len);
	gettimeofday(&tv, 0);
	p = add_entropy(tv.tv_sec, p, buf, len);
	p = add_entropy(tv.tv_usec, p, buf, len);
	p = add_entropy(times(&tms), p, buf, len); /* this works really well */
	p = add_entropy((unsigned long)rng_fake, p, buf, len);
	p = add_entropy((unsigned long)&len, p, buf, len);
	p = add_entropy(getuid(), p, buf, len);
	return (len);
}
#endif

/**
  Read the system RNG
  @param out       Destination
  @param outlen    Length desired (octets)
  @param callback  Pointer to void function to act as "callback" when RNG is slow.  This can be NULL
  @return Number of octets read
*/
unsigned long rng_get_bytes(unsigned char *buf, unsigned long len,
                            void (*callback)(void))
{
   int x = 0;

   x += rng_nix(buf+x, len-x, callback);   if (x==len) { return x; }
#ifdef WIN32
   x += rng_win32(buf+x, len-x, callback); if (x==len) { return x; }
#endif
#ifdef ANSI_RNG
   x += rng_ansic(buf+x, len-x, callback); if (x==len) { return x; }
#endif
#ifndef WIN32
   x += rng_fake(buf+x, len-x);	           if (x==len) { return x; }
#endif
   return 0;
}

/* $Source: /cvs/libtom/libtomcrypt/src/prngs/rng_get_bytes.c,v $ */
/* $Revision: 1.4 $ */
/* $Date: 2006/03/31 14:15:35 $ */
