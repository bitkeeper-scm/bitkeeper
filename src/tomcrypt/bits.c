/* portable way to get secure random bits to feed a PRNG */
#include "mycrypt.h"
#ifndef WIN32
#include <unistd.h>
#include <sys/time.h>
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

#ifdef WIN32
#define	srandom(x)	srand(x)
#define	random(x)	rand(x)
#endif

/*
 * A "fake" random number generator for the BK regressions.
 * This one is a lot faster than /dev/random which might wait
 * a long time for data to be generated.
 */
static unsigned long rng_fake(unsigned char *buf, unsigned long len)
{
	unsigned long	seed;
	int	x = len;
	struct timeval tv;

	gettimeofday(&tv, 0);
	seed = tv.tv_usec;
	seed ^= getpid();
	seed = (seed << 7) | (seed >> 25);
#ifndef WIN32
	seed ^= (unsigned long)sbrk(0);
	seed = (seed << 7) | (seed >> 25);
#endif
	seed ^= (unsigned long)&seed;
	srandom(seed);
	while (x--) *buf++ = random();
	return (len);
}

unsigned long rng_get_bytes(unsigned char *buf, unsigned long len, 
                            void (*callback)(void))
{
   int x = 0;

   if (getenv("BK_REGRESSION")) {
	   x += rng_fake(buf+x, len-x);    if (x==len) { return x; }
   }

   x += rng_nix(buf+x, len-x, callback);   if (x==len) { return x; }
#ifdef WIN32
   x += rng_win32(buf+x, len-x, callback); if (x==len) { return x; }
#endif
#ifdef ANSI_RNG
   x += rng_ansic(buf+x, len-x, callback); if (x==len) { return x; }
#endif
   x += rng_fake(buf+x, len-x);	           if (x==len) { return x; }
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
   if (rng_get_bytes(buf, bits, callback) != (unsigned long)bits) {
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

