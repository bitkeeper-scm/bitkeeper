/* portable way to get secure random bits to feed a PRNG */
#include "mycrypt.h"
#ifndef WIN32
#include <unistd.h>
#include <sys/time.h>
#include <sys/times.h>
#endif

/* on *NIX read /dev/random */
static unsigned long rng_nix(unsigned char *buf, unsigned long len, 
                             void (*callback)(void))
{
    FILE *f;
    int x;

#ifdef TRY_URANDOM_FIRST
    f = fopen("/dev/urandom", "rb");
    if (f == NULL)
#endif
       f = fopen("/dev/random", "rb");

    if (f == NULL) {
       return 0;
    }
 
    x = fread(buf, 1, len, f);
    fclose(f);
    return x;
}

/* on ANSI C platforms with 100 < CLOCKS_PER_SEC < 10000 */
#ifdef CLOCKS_PER_SEC

#define ANSI_RNG

static unsigned long rng_ansic(unsigned char *buf, unsigned long len, 
                               void (*callback)(void))
{
   clock_t t1;
   int l, acc, bits, a, b;

   if (CLOCKS_PER_SEC < 100 || CLOCKS_PER_SEC > 10000) {
      return 0;
   }

   l = len;
   bits = 8;
   acc  = a = b = 0;
   while (len--) {
       if (callback != NULL) callback();
       while (bits--) {
          do {
             t1 = clock(); while (t1 == clock()) a ^= 1;
             t1 = clock(); while (t1 == clock()) b ^= 1;
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
	p = add_entropy((unsigned long)sbrk(0), p, buf, len);
	p = add_entropy((unsigned long)&len, p, buf, len);
	p = add_entropy(getuid(), p, buf, len);
	return (len);
}
#endif

unsigned long rng_get_seedbytes(unsigned char *buf, unsigned long len,
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

int rng_make_prng(int bits, int wprng, prng_state *prng, 
                  void (*callback)(void))
{
   unsigned char buf[256];
   
   /* check parameter */
   if (prng_is_valid(wprng) == CRYPT_ERROR) {
      return CRYPT_ERROR;
   }

   if (bits < 64 || bits > 1024) {
      crypt_error = "Invalid state size in rng_make_prng().";
      return CRYPT_ERROR;
   }

   if (prng_descriptor[wprng].start(prng) == CRYPT_ERROR) {
      return CRYPT_ERROR;
   }

   bits = ((bits/8)+(bits&7?1:0)) * 2;
   if (rng_get_seedbytes(buf, bits, callback) != (unsigned long)bits) {
      crypt_error = "Error reading rng in rng_make_prng().";
      return CRYPT_ERROR;
   }

   if (prng_descriptor[wprng].add_entropy(buf, bits, prng) == CRYPT_ERROR) {
      return CRYPT_ERROR;
   }

   if (prng_descriptor[wprng].ready(prng) == CRYPT_ERROR) {
      return CRYPT_ERROR;
   }

   zeromem(buf, sizeof(buf));
   return CRYPT_OK;
}

static const char __attribute__((unused)) *ID_TAG = "bits.c";

