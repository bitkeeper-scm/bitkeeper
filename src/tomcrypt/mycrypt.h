#ifndef CRYPT_H_
#define CRYPT_H_
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <ctype.h>
#include <limits.h>

#ifdef __cplusplus
extern "C" {
#endif

/* version */
#define CRYPT   0x0062

/* max size of either a cipher/hash block or symmetric key [largest of the two] */
#define MAXBLOCKSIZE           128

/* fix for MSVC ...evil! */
#ifdef _MSC_VER
   #define CONST64(n) n ## ui64
   typedef unsigned __int64 ulong64;
#else
   #define CONST64(n) n ## ULL
   typedef unsigned long long ulong64;
#endif

#include "mycrypt_cfg.h"

/* ---- ERRORS ---- */
enum {
   CRYPT_OK=0,
   CRYPT_ERROR
};
extern char *crypt_error;

/* ---- HELPER MACROS ---- */
#define STORE32L(x, y)                                                                     \
     { (y)[3] = (unsigned char)(((x)>>24)&255); (y)[2] = (unsigned char)(((x)>>16)&255);   \
       (y)[1] = (unsigned char)(((x)>>8)&255); (y)[0] = (unsigned char)((x)&255); }

#define LOAD32L(x, y)                            \
     { x = ((unsigned long)((y)[3] & 255)<<24) | \
           ((unsigned long)((y)[2] & 255)<<16) | \
           ((unsigned long)((y)[1] & 255)<<8)  | \
           ((unsigned long)((y)[0] & 255)); }

#define STORE64L(x, y)                                                                     \
     { (y)[7] = (unsigned char)(((x)>>56)&255); (y)[6] = (unsigned char)(((x)>>48)&255);   \
       (y)[5] = (unsigned char)(((x)>>40)&255); (y)[4] = (unsigned char)(((x)>>32)&255);   \
       (y)[3] = (unsigned char)(((x)>>24)&255); (y)[2] = (unsigned char)(((x)>>16)&255);   \
       (y)[1] = (unsigned char)(((x)>>8)&255); (y)[0] = (unsigned char)((x)&255); }

#define LOAD64L(x, y)                                                       \
     { x = (((ulong64)((y)[7] & 255))<<56)|(((ulong64)((y)[6] & 255))<<48)| \
           (((ulong64)((y)[5] & 255))<<40)|(((ulong64)((y)[4] & 255))<<32)| \
           (((ulong64)((y)[3] & 255))<<24)|(((ulong64)((y)[2] & 255))<<16)| \
           (((ulong64)((y)[1] & 255))<<8)|(((ulong64)((y)[0] & 255))); }

#define STORE32H(x, y)                                                                     \
     { (y)[0] = (unsigned char)(((x)>>24)&255); (y)[1] = (unsigned char)(((x)>>16)&255);   \
       (y)[2] = (unsigned char)(((x)>>8)&255); (y)[3] = (unsigned char)((x)&255); }

#define LOAD32H(x, y)                            \
     { x = ((unsigned long)((y)[0] & 255)<<24) | \
           ((unsigned long)((y)[1] & 255)<<16) | \
           ((unsigned long)((y)[2] & 255)<<8)  | \
           ((unsigned long)((y)[3] & 255)); }

#define STORE64H(x, y)                                                                     \
   { (y)[0] = (unsigned char)(((x)>>56)&255); (y)[1] = (unsigned char)(((x)>>48)&255);     \
     (y)[2] = (unsigned char)(((x)>>40)&255); (y)[3] = (unsigned char)(((x)>>32)&255);     \
     (y)[4] = (unsigned char)(((x)>>24)&255); (y)[5] = (unsigned char)(((x)>>16)&255);     \
     (y)[6] = (unsigned char)(((x)>>8)&255); (y)[7] = (unsigned char)((x)&255); }

#define LOAD64H(x, y)                                                      \
   { x = (((ulong64)((y)[0] & 255))<<56)|(((ulong64)((y)[1] & 255))<<48) | \
         (((ulong64)((y)[2] & 255))<<40)|(((ulong64)((y)[3] & 255))<<32) | \
         (((ulong64)((y)[4] & 255))<<24)|(((ulong64)((y)[5] & 255))<<16) | \
         (((ulong64)((y)[6] & 255))<<8)|(((ulong64)((y)[7] & 255))); }

#define BSWAP(x)  ( ((x>>24)&0x000000FFUL) | ((x<<24)&0xFF000000UL)  | \
                    ((x>>8)&0x0000FF00UL)  | ((x<<8)&0x00FF0000UL) )

#define ROL(x, y) ( (((x)<<((y)&31)) | (((x)&0xFFFFFFFFUL)>>(32-((y)&31)))) & 0xFFFFFFFFUL)
#define ROR(x, y) ( ((((x)&0xFFFFFFFFUL)>>((y)&31)) | ((x)<<(32-((y)&31)))) & 0xFFFFFFFFUL)

#define ROL64(x, y) \
    ( (((x)<<((ulong64)(y)&63)) | \
      (((x)&CONST64(0xFFFFFFFFFFFFFFFF))>>((ulong64)64-((y)&63)))) & CONST64(0xFFFFFFFFFFFFFFFF))

#define ROR64(x, y) \
    ( ((((x)&CONST64(0xFFFFFFFFFFFFFFFF))>>((ulong64)(y)&CONST64(63))) | \
      ((x)<<((ulong64)(64-((y)&CONST64(63)))))) & CONST64(0xFFFFFFFFFFFFFFFF))

#undef MAX
#define MAX(x, y) ( ((x)>(y))?(x):(y) )
#undef MIN
#define MIN(x, y) ( ((x)<(y))?(x):(y) )

/* ---- SYMMETRIC KEY STUFF -----
 *
 * We put each of the ciphers scheduled keys in their own structs then we put all of 
 * the key formats in one union.  This makes the function prototypes easier to use.
 */
#ifdef BLOWFISH
struct blowfish_key {
   unsigned long S[4][256];
   unsigned long K[18];
};
#endif

#ifdef RC5
struct rc5_key {
   int rounds;
   unsigned long K[50];
};
#endif

#ifdef RC6
struct rc6_key {
   unsigned long K[44];
};
#endif

#ifdef SAFERP
struct saferp_key {
   unsigned char K[33][16];
   long rounds;
};
#endif

#ifdef SERPENT
struct serpent_key {
   unsigned long K[132];
};
#endif

#ifdef RIJNDAEL
struct rijndael_key {
   unsigned long eK[64], dK[64], k_len;
};
#endif

#ifdef XTEA
struct xtea_key {
   unsigned long K[4];
};
#endif

#ifdef TWOFISH
#ifndef TWOFISH_SMALL
   struct twofish_key {
      unsigned long S[4][256], K[40];
   };
#else
   struct twofish_key {
      unsigned long K[40];
      unsigned char S[32], start;
   };
#endif
#endif

#ifdef SAFER
#define SAFER_K64_DEFAULT_NOF_ROUNDS     6
#define SAFER_K128_DEFAULT_NOF_ROUNDS   10
#define SAFER_SK64_DEFAULT_NOF_ROUNDS    8
#define SAFER_SK128_DEFAULT_NOF_ROUNDS  10
#define SAFER_MAX_NOF_ROUNDS            13
#define SAFER_BLOCK_LEN                  8
#define SAFER_KEY_LEN     (1 + SAFER_BLOCK_LEN * (1 + 2 * SAFER_MAX_NOF_ROUNDS))
typedef unsigned char safer_block_t[SAFER_BLOCK_LEN];
typedef unsigned char safer_key_t[SAFER_KEY_LEN];
struct safer_key { safer_key_t key; };
#endif

#ifdef RC2
struct rc2_key { unsigned xkey[64]; };
#endif

#ifdef DES
struct des_key {
    unsigned long ek[32], dk[32];
};

struct des3_key {
    unsigned long ek[3][32], dk[3][32];
};

#endif

typedef union Symmetric_key {
#ifdef DES
   struct des_key des;
   struct des3_key des3;
#endif
#ifdef RC2
   struct rc2_key rc2;
#endif
#ifdef SAFER
   struct safer_key safer;
#endif
#ifdef TWOFISH
   struct twofish_key  twofish;
#endif
#ifdef BLOWFISH
   struct blowfish_key blowfish;
#endif
#ifdef RC5
   struct rc5_key      rc5;
#endif
#ifdef RC6
   struct rc6_key      rc6;
#endif
#ifdef SAFERP
   struct saferp_key   saferp;
#endif
#ifdef SERPENT
   struct serpent_key  serpent;
#endif
#ifdef RIJNDAEL
   struct rijndael_key rijndael;
#endif
#ifdef XTEA
   struct xtea_key     xtea;
#endif
} symmetric_key;

/* A block cipher ECB structure */
typedef struct {
   int                 cipher, blocklen;
   symmetric_key       key;
} symmetric_ECB;

/* A block cipher CFB structure */
typedef struct {
   int                 cipher, blocklen, padlen;
   unsigned char       IV[MAXBLOCKSIZE], pad[MAXBLOCKSIZE];
   symmetric_key       key;
} symmetric_CFB;

/* A block cipher OFB structure */
typedef struct {
   int                 cipher, blocklen, padlen;
   unsigned char       IV[MAXBLOCKSIZE];
   symmetric_key       key;
} symmetric_OFB;

/* A block cipher CBC structure */
typedef struct Symmetric_CBC {
   int                 cipher, blocklen;
   unsigned char       IV[MAXBLOCKSIZE];
   symmetric_key       key;
} symmetric_CBC;

/* A block cipher CTR structure */
typedef struct Symmetric_CTR {
   int                 cipher, blocklen, padlen;
   unsigned char       ctr[MAXBLOCKSIZE], pad[MAXBLOCKSIZE];
   symmetric_key       key;
} symmetric_CTR;

/* cipher descriptor table, last entry has "name == NULL" to mark the end of table */
extern  struct _cipher_descriptor {
   char *name;
   unsigned char ID;
   unsigned long  min_key_length, max_key_length, block_length, default_rounds;
   int  (*setup)(const unsigned char *key, int keylength, int num_rounds, symmetric_key *skey);
   void (*ecb_encrypt)(const unsigned char *pt, unsigned char *ct, symmetric_key *key);
   void (*ecb_decrypt)(const unsigned char *ct, unsigned char *pt, symmetric_key *key);
   int (*test)(void);
   int  (*keysize)(int *desired_keysize);
} cipher_descriptor[];

#ifdef BLOWFISH
extern int blowfish_setup(const unsigned char *key, int keylen, int num_rounds, symmetric_key *skey);
extern void blowfish_ecb_encrypt(const unsigned char *pt, unsigned char *ct, symmetric_key *key);
extern void blowfish_ecb_decrypt(const unsigned char *ct, unsigned char *pt, symmetric_key *key);
extern int blowfish_test(void);
extern int blowfish_keysize(int *desired_keysize);
extern const struct _cipher_descriptor blowfish_desc;
#endif

#ifdef RC5
extern int rc5_setup(const unsigned char *key, int keylen, int num_rounds, symmetric_key *skey);
extern void rc5_ecb_encrypt(const unsigned char *pt, unsigned char *ct, symmetric_key *key);
extern void rc5_ecb_decrypt(const unsigned char *ct, unsigned char *pt, symmetric_key *key);
extern int rc5_test(void);
extern int rc5_keysize(int *desired_keysize);
extern const struct _cipher_descriptor rc5_desc;
#endif

#ifdef RC6
extern int rc6_setup(const unsigned char *key, int keylen, int num_rounds, symmetric_key *skey);
extern void rc6_ecb_encrypt(const unsigned char *pt, unsigned char *ct, symmetric_key *key);
extern void rc6_ecb_decrypt(const unsigned char *ct, unsigned char *pt, symmetric_key *key);
extern int rc6_test(void);
extern int rc6_keysize(int *desired_keysize);
extern const struct _cipher_descriptor rc6_desc;
#endif

#ifdef RC2
extern int rc2_setup(const unsigned char *key, int keylen, int num_rounds, symmetric_key *skey);
extern void rc2_ecb_encrypt(const unsigned char *pt, unsigned char *ct, symmetric_key *key);
extern void rc2_ecb_decrypt(const unsigned char *ct, unsigned char *pt, symmetric_key *key);
extern int rc2_test(void);
extern int rc2_keysize(int *desired_keysize);
extern const struct _cipher_descriptor rc2_desc;
#endif

#ifdef SAFERP
extern int saferp_setup(const unsigned char *key, int keylen, int num_rounds, symmetric_key *skey);
extern void saferp_ecb_encrypt(const unsigned char *pt, unsigned char *ct, symmetric_key *key);
extern void saferp_ecb_decrypt(const unsigned char *ct, unsigned char *pt, symmetric_key *key);
extern int saferp_test(void);
extern int saferp_keysize(int *desired_keysize);
extern const struct _cipher_descriptor saferp_desc;
#endif

#ifdef SAFER
extern int safer_k64_setup(const unsigned char *key, int keylen, int num_rounds, symmetric_key *skey);
extern int safer_sk64_setup(const unsigned char *key, int keylen, int num_rounds, symmetric_key *skey);
extern int safer_k128_setup(const unsigned char *key, int keylen, int num_rounds, symmetric_key *skey);
extern int safer_sk128_setup(const unsigned char *key, int keylen, int num_rounds, symmetric_key *skey);
extern void safer_ecb_encrypt(const unsigned char *pt, unsigned char *ct, symmetric_key *key);
extern void safer_ecb_decrypt(const unsigned char *ct, unsigned char *pt, symmetric_key *key);

extern int safer_k64_test(void);
extern int safer_sk64_test(void);
extern int safer_sk128_test(void);

extern int safer_64_keysize(int *desired_keysize);
extern int safer_128_keysize(int *desired_keysize);
extern const struct _cipher_descriptor safer_k64_desc, safer_k128_desc, safer_sk64_desc, safer_sk128_desc;
#endif

#ifdef SERPENT
extern int serpent_setup(const unsigned char *key, int keylen, int num_rounds, symmetric_key *skey);
extern void serpent_ecb_encrypt(const unsigned char *pt, unsigned char *ct, symmetric_key *key);
extern void serpent_ecb_decrypt(const unsigned char *ct, unsigned char *pt, symmetric_key *key);
extern int serpent_test(void);
extern int serpent_keysize(int *desired_keysize);
extern const struct _cipher_descriptor serpent_desc;
#endif

#ifdef RIJNDAEL
extern int rijndael_setup(const unsigned char *key, int keylen, int num_rounds, symmetric_key *skey);
extern void rijndael_ecb_encrypt(const unsigned char *pt, unsigned char *ct, symmetric_key *key);
extern void rijndael_ecb_decrypt(const unsigned char *ct, unsigned char *pt, symmetric_key *key);
extern int rijndael_test(void);
extern int rijndael_keysize(int *desired_keysize);
extern const struct _cipher_descriptor rijndael_desc;
#endif

#ifdef XTEA
extern int xtea_setup(const unsigned char *key, int keylen, int num_rounds, symmetric_key *skey);
extern void xtea_ecb_encrypt(const unsigned char *pt, unsigned char *ct, symmetric_key *key);
extern void xtea_ecb_decrypt(const unsigned char *ct, unsigned char *pt, symmetric_key *key);
extern int xtea_test(void);
extern int xtea_keysize(int *desired_keysize);
extern const struct _cipher_descriptor xtea_desc;
#endif

#ifdef TWOFISH
extern int twofish_setup(const unsigned char *key, int keylen, int num_rounds, symmetric_key *skey);
extern void twofish_ecb_encrypt(const unsigned char *pt, unsigned char *ct, symmetric_key *key);
extern void twofish_ecb_decrypt(const unsigned char *ct, unsigned char *pt, symmetric_key *key);
extern int twofish_test(void);
extern int twofish_keysize(int *desired_keysize);
extern const struct _cipher_descriptor twofish_desc;
#endif

#ifdef DES
extern int des_setup(const unsigned char *key, int keylen, int num_rounds, symmetric_key *skey);
extern void des_ecb_encrypt(const unsigned char *pt, unsigned char *ct, symmetric_key *key);
extern void des_ecb_decrypt(const unsigned char *ct, unsigned char *pt, symmetric_key *key);
extern int des_test(void);
extern int des_keysize(int *desired_keysize);

extern int des3_setup(const unsigned char *key, int keylen, int num_rounds, symmetric_key *skey);
extern void des3_ecb_encrypt(const unsigned char *pt, unsigned char *ct, symmetric_key *key);
extern void des3_ecb_decrypt(const unsigned char *ct, unsigned char *pt, symmetric_key *key);
extern int des3_test(void);
extern int des3_keysize(int *desired_keysize);

extern const struct _cipher_descriptor des_desc, des3_desc;
#endif

#ifdef ECB
extern int ecb_start(int cipher, const unsigned char *key, 
                     int keylen, int num_rounds, symmetric_ECB *ecb);
extern void ecb_encrypt(const unsigned char *pt, unsigned char *ct, symmetric_ECB *ecb);
extern void ecb_decrypt(const unsigned char *ct, unsigned char *pt, symmetric_ECB *ecb);
#endif

#ifdef CFB
extern int cfb_start(int cipher, const unsigned char *IV, const unsigned char *key, 
                     int keylen, int num_rounds, symmetric_CFB *CFB);
extern void cfb_encrypt(const unsigned char *pt, unsigned char *ct, int len, symmetric_CFB *cfb);
extern void cfb_decrypt(const unsigned char *ct, unsigned char *pt, int len, symmetric_CFB *cfb);
#endif

#ifdef OFB
extern int ofb_start(int cipher, const unsigned char *IV, const unsigned char *key, 
                     int keylen, int num_rounds, symmetric_OFB *ofb);
extern void ofb_encrypt(const unsigned char *pt, unsigned char *ct, int len, symmetric_OFB *ofb);
extern void ofb_decrypt(const unsigned char *ct, unsigned char *pt, int len, symmetric_OFB *ofb);
#endif

#ifdef CBC
extern int cbc_start(int cipher, const unsigned char *IV, const unsigned char *key,
                     int keylen, int num_rounds, symmetric_CBC *cbc);
extern void cbc_encrypt(const unsigned char *pt, unsigned char *ct, symmetric_CBC *cbc);
extern void cbc_decrypt(const unsigned char *ct, unsigned char *pt, symmetric_CBC *cbc);
#endif

#ifdef CTR
extern int ctr_start(int cipher, const unsigned char *IV, const unsigned char *key, 
                     int keylen, int num_rounds, symmetric_CTR *ctr);
extern void ctr_encrypt(const unsigned char *pt, unsigned char *ct, int len, symmetric_CTR *ctr);
extern void ctr_decrypt(const unsigned char *ct, unsigned char *pt, int len, symmetric_CTR *ctr);
#endif
	
extern int find_cipher(const char *name);
extern int find_cipher_id(unsigned char ID);

extern int register_cipher(const struct _cipher_descriptor *cipher);
extern int unregister_cipher(const struct _cipher_descriptor *cipher);

extern int cipher_is_valid(int idx);

/* ---- HASH FUNCTIONS ---- */
#ifdef SHA512
struct sha512_state {
    ulong64  length, state[8];
    unsigned long curlen;
    unsigned char buf[128];
};
#endif

#ifdef SHA256
struct sha256_state {
    ulong64 length;
    unsigned long state[8], curlen;
    unsigned char buf[64];
};
#endif

#ifdef SHA1
struct sha1_state {
    ulong64 length;
    unsigned long state[5], curlen;
    unsigned char buf[64];
};
#endif

#ifdef MD5
struct md5_state {
    ulong64 length;
    unsigned long state[4], curlen;
    unsigned char buf[64];
};
#endif

#ifdef MD4
struct md4_state {
    ulong64 length;
    unsigned long state[4], curlen;
    unsigned char buf[64];
};
#endif

#ifdef TIGER
struct tiger_state {
    ulong64 state[3], length;
    unsigned long curlen;
    unsigned char buf[64];
};
#endif

typedef union Hash_state {
#ifdef SHA512
    struct sha512_state sha512;
#endif
#ifdef SHA256
    struct sha256_state sha256;
#endif
#ifdef SHA1
    struct sha1_state   sha1;
#endif
#ifdef MD5
    struct md5_state    md5;
#endif
#ifdef MD4
    struct md4_state    md4;
#endif
#ifdef TIGER
    struct tiger_state  tiger;
#endif
} hash_state;

extern struct _hash_descriptor {
    char *name;
    unsigned char ID;
    unsigned long hashsize;       /* digest output size in bytes  */
    unsigned long blocksize;      /* the block size the hash uses */
    void (*init)(hash_state *);
    void (*process)(hash_state *, const unsigned char *, unsigned long);
    void (*done)(hash_state *, unsigned char *);
    int  (*test)(void);
} hash_descriptor[];

#ifdef SHA512
extern void sha512_init(hash_state * md);
extern void sha512_process(hash_state * md, const unsigned char *buf, unsigned long len);
extern void sha512_done(hash_state * md, unsigned char *hash);
extern int  sha512_test(void);
extern const struct _hash_descriptor sha512_desc;
#endif

#ifdef SHA384
extern void sha384_init(hash_state * md);
extern void sha384_process(hash_state * md, const unsigned char *buf, unsigned long len);
extern void sha384_done(hash_state * md, unsigned char *hash);
extern int  sha384_test(void);
extern const struct _hash_descriptor sha384_desc;
#endif

#ifdef SHA256
extern void sha256_init(hash_state * md);
extern void sha256_process(hash_state * md, const unsigned char *buf, unsigned long len);
extern void sha256_done(hash_state * md, unsigned char *hash);
extern int  sha256_test(void);
extern const struct _hash_descriptor sha256_desc;
#endif

#ifdef SHA1
extern void sha1_init(hash_state * md);
extern void sha1_process(hash_state * md, const unsigned char *buf, unsigned long len);
extern void sha1_done(hash_state * md, unsigned char *hash);
extern int  sha1_test(void);
extern const struct _hash_descriptor sha1_desc;
#endif

#ifdef MD5
extern void md5_init(hash_state * md);
extern void md5_process(hash_state * md, const unsigned char *buf, unsigned long len);
extern void md5_done(hash_state * md, unsigned char *hash);
extern int  md5_test(void);
extern const struct _hash_descriptor md5_desc;
#endif

#ifdef MD4
extern void md4_init(hash_state * md);
extern void md4_process(hash_state * md, const unsigned char *buf, unsigned long len);
extern void md4_done(hash_state * md, unsigned char *hash);
extern int  md4_test(void);
extern const struct _hash_descriptor md4_desc;
#endif

#ifdef TIGER
extern void tiger_init(hash_state * md);
extern void tiger_process(hash_state * md, const unsigned char *buf, unsigned long len);
extern void tiger_done(hash_state * md, unsigned char *hash);
extern int  tiger_test(void);
extern const struct _hash_descriptor tiger_desc;
#endif

extern int find_hash(const char *name);
extern int find_hash_id(unsigned char ID);
extern int register_hash(const struct _hash_descriptor *hash);
extern int unregister_hash(const struct _hash_descriptor *hash);
extern int hash_is_valid(int idx);

extern int hash_memory(int hash, const unsigned char *data, unsigned long len, unsigned char *dst);
extern int hash_file(int hash, const char *fname, unsigned char *dst);

/* ---- PRNG Stuff ---- */
struct yarrow_prng {
    int                   cipher, hash;
    unsigned char         pool[MAXBLOCKSIZE], buf[MAXBLOCKSIZE];
    symmetric_CTR         ctr;
};

typedef union Prng_state {
    struct yarrow_prng    yarrow;
} prng_state;

extern struct _prng_descriptor {
    char *name;
    int (*start)(prng_state *);
    int (*add_entropy)(const unsigned char *, unsigned long, prng_state *);
    int (*ready)(prng_state *);
    unsigned long (*read)(unsigned char *, unsigned long len, prng_state *);
} prng_descriptor[];

#ifdef YARROW
extern int yarrow_start(prng_state *prng);
extern int yarrow_add_entropy(const unsigned char *buf, unsigned long len, prng_state *prng);
extern int yarrow_ready(prng_state *prng);
extern unsigned long yarrow_read(unsigned char *buf, unsigned long len, prng_state *prng);
extern const struct _prng_descriptor yarrow_desc;
#endif

#ifdef SPRNG
extern int sprng_start(prng_state *prng);
extern int sprng_add_entropy(const unsigned char *buf, unsigned long len, prng_state *prng);
extern int sprng_ready(prng_state *prng);
extern unsigned long sprng_read(unsigned char *buf, unsigned long len, prng_state *prng);
extern const struct _prng_descriptor sprng_desc;
#endif

extern int find_prng(const char *name);
extern int register_prng(const struct _prng_descriptor *prng);
extern int unregister_prng(const struct _prng_descriptor *prng);
extern int prng_is_valid(int idx);


/* Slow RNG you **might** be able to use to seed a PRNG with.  Be careful as this
 * might not work on all platforms as planned
 */
extern unsigned long rng_get_bytes(unsigned char *buf, unsigned long len, void (*callback)(void));

extern int rng_make_prng(int bits, int wprng, prng_state *prng, void (*callback)(void));

/* ---- NUMBER THEORY ---- */
#ifdef MPI

extern int is_prime(mp_int *, int *);
extern int rand_prime(mp_int *N, long len, prng_state *prng, int wprng);
extern mp_err mp_init_multi(mp_int* mp, ...);
extern void mp_clear_multi(mp_int* mp, ...);

#endif

/* ---- PUBLIC KEY CRYPTO ---- */

#define PK_PRIVATE            0        /* PK private keys */
#define PK_PUBLIC             1        /* PK public keys */
#define PK_PRIVATE_OPTIMIZED  2        /* PK private key [rsa optimized] */

/* ---- PACKET ---- */
#ifdef PACKET

extern void packet_store_header(unsigned char *dst, int section, int subsection, unsigned long length);
extern int packet_valid_header(unsigned char *src, int section, int subsection);

#endif


/* ---- RSA ---- */
#ifdef MRSA
typedef struct Rsa_key {
    int type;
    mp_int e, d, N, qP, pQ, dP, dQ, p, q;
} rsa_key;

extern int rsa_make_key(prng_state *prng, int wprng, int size, long e, rsa_key *key);

extern int rsa_exptmod(const unsigned char *in,  unsigned long inlen, 
                             unsigned char *out, unsigned long *outlen, int which, 
                             rsa_key *key);

extern int rsa_pad(const unsigned char *in,  unsigned long inlen, 
                         unsigned char *out, unsigned long *outlen, 
                         int wprng, prng_state *prng);

extern int rsa_signpad(const unsigned char *in,  unsigned long inlen, 
                             unsigned char *out, unsigned long *outlen, 
                             int wprng, prng_state *prng);

extern int rsa_depad(const unsigned char *in,  unsigned long inlen, 
                           unsigned char *out, unsigned long *outlen);

extern int rsa_signdepad(const unsigned char *in,  unsigned long inlen,
                               unsigned char *out, unsigned long *outlen);


extern void rsa_free(rsa_key *key);

extern int rsa_encrypt(const unsigned char *in,  unsigned long len, 
                             unsigned char *out, unsigned long *outlen,
                             prng_state *prng, int wprng, int cipher, 
                             rsa_key *key);

extern int rsa_decrypt(const unsigned char *in,  unsigned long len, 
                             unsigned char *out, unsigned long *outlen, 
                             rsa_key *key);

extern int rsa_sign(const unsigned char *in, unsigned long inlen, 
                          unsigned char *out, unsigned long *outlen, int hash, 
                          prng_state *prng, int wprng, 
                          rsa_key *key);

extern int rsa_verify(const unsigned char *sig, const unsigned char *msg, 
                            unsigned long inlen, int *stat, 
                            rsa_key *key);

extern int rsa_encrypt_key(const unsigned char *inkey, unsigned long inlen,
                                 unsigned char *outkey, unsigned long *outlen,
                                 prng_state *prng, int wprng, rsa_key *key);

extern int rsa_decrypt_key(const unsigned char *in,  unsigned long len, 
                                 unsigned char *outkey, unsigned long *keylen,
                                 rsa_key *key);

extern int rsa_export(unsigned char *out, unsigned long *outlen, int type, rsa_key *key);
extern int rsa_import(const unsigned char *in, rsa_key *key);
#endif

/* ---- DH Routines ---- */
#ifdef MDH 

typedef struct Dh_key {
    int idx, type;
    mp_int x, y;
} dh_key;

extern int dh_test(void);
extern void dh_sizes(int *low, int *high);
extern int dh_get_size(dh_key *key);

extern int dh_make_key(int keysize, prng_state *prng, int wprng, dh_key *key);
extern void dh_free(dh_key *key);

extern int dh_export(unsigned char *out, unsigned long *outlen, int type, dh_key *key);
extern int dh_import(const unsigned char *in, dh_key *key);

extern int dh_shared_secret(dh_key *private_key, dh_key *public_key,
                            unsigned char *out, unsigned long *outlen);

extern int dh_encrypt(const unsigned char *in,  unsigned long len, 
                            unsigned char *out, unsigned long *outlen,
                            prng_state *prng, int wprng, int cipher, int hash, 
                            dh_key *key);

extern int dh_decrypt(const unsigned char *in,  unsigned long len, 
                            unsigned char *out, unsigned long *outlen, 
                            dh_key *key);

extern int dh_sign(const unsigned char *in,  unsigned long inlen, 
                         unsigned char *out, unsigned long *outlen, int hash, 
                         prng_state *prng, int wprng, 
                         dh_key *key);

extern int dh_verify(const unsigned char *sig, const unsigned char *msg, 
                           unsigned long inlen, int *stat, 
                           dh_key *key);

extern int dh_encrypt_key(const unsigned char *inkey, unsigned long keylen,
                                unsigned char *out,  unsigned long *len, 
                                prng_state *prng, int wprng, int hash, 
                                dh_key *key);

extern int dh_decrypt_key(const unsigned char *in,  unsigned long len, 
	                        unsigned char *outkey, unsigned long *keylen, 
                                dh_key *key);

#endif

/* ---- ECC Routines ---- */
#ifdef MECC
typedef struct {
    mp_int x, y;
} ecc_point;

typedef struct {
    int type, idx;
    ecc_point pubkey;
    mp_int k;
} ecc_key;

extern int ecc_test(void);
extern void ecc_sizes(int *low, int *high);
extern int ecc_get_size(ecc_key *key);

extern int ecc_make_key(int keysize, prng_state *prng, int wprng, ecc_key *key);
extern void ecc_free(ecc_key *key);

extern int ecc_export(unsigned char *out, unsigned long *outlen, int type, ecc_key *key);
extern int ecc_import(const unsigned char *in, ecc_key *key);

extern int ecc_shared_secret(ecc_key *private_key, ecc_key *public_key, 
                             unsigned char *out, unsigned long *outlen);

extern int ecc_encrypt(const unsigned char *in,  unsigned long len, 
                             unsigned char *out, unsigned long *outlen,
                             prng_state *prng, int wprng, int cipher, int hash, 
                             ecc_key *key);

extern int ecc_decrypt(const unsigned char *in,  unsigned long len,
                             unsigned char *out, unsigned long *outlen, 
                             ecc_key *key);

extern int ecc_sign(const unsigned char *in, unsigned long inlen, 
                          unsigned char *out, unsigned long *outlen, int hash, 
                          prng_state *prng, int wprng, 
                          ecc_key *key);

extern int ecc_verify(const unsigned char *sig, const unsigned char *msg, 
                            unsigned long inlen, int *stat, 
                            ecc_key *key);

extern int ecc_encrypt_key(const unsigned char *inkey, unsigned long keylen,
                                 unsigned char *out,  unsigned long *len, 
                                 prng_state *prng, int wprng, int hash, 
                                 ecc_key *key);

extern int ecc_decrypt_key(const unsigned char *in,  unsigned long len, 
                                 unsigned char *outkey, unsigned long *keylen, 
                                 ecc_key *key);

extern int compress_y_point(ecc_point *pt, int idx, int *res);
extern int expand_y_point(ecc_point *pt, int idx, int res);
#endif

/* ---- BASE64 Routines ---- */
#ifdef BASE64
extern int base64_encode(const unsigned char *in,  unsigned long len, 
                               unsigned char *out, unsigned long *outlen);

extern int base64_decode(const unsigned char *in,  unsigned long len, 
                               unsigned char *out, unsigned long *outlen);
#endif

/* ---- COIN Flips ---- */
enum { 
    CF_HOST=0,  /* HOST  makes up the challenge */
    CF_GUEST,   /* GUEST sends their guess back to the HOST */
    CF_WIN,     /* A winning outcome */
    CF_LOSE     /* A losing outcome */
};

extern int coin_toss(const unsigned char *shared_secret, 
                     int secret_len, int whoami, int *result);


/* ---- MEM routines ---- */
extern void zeromem(void *dst, unsigned long len);
extern void burn_stack(unsigned long len);

/* ---- GF(2^w) polynomial basis ---- */
#ifdef GF
#define   LSIZE    32   /* handle upto 1024-bit GF numbers */

typedef unsigned long gf_int[LSIZE];
typedef unsigned long *gf_intp;

extern void gf_copy(gf_intp a, gf_intp b);
extern void gf_zero(gf_intp a);
extern int gf_iszero(gf_intp a);
extern int gf_isone(gf_intp a);
extern int gf_deg(gf_intp a);

extern void gf_shl(gf_intp a, gf_intp b);
extern void gf_shr(gf_intp a, gf_intp b);
extern void gf_add(gf_intp a, gf_intp b, gf_intp c);
extern void gf_mul(gf_intp a, gf_intp b, gf_intp c);
extern void gf_div(gf_intp a, gf_intp b, gf_intp q, gf_intp r);

extern void gf_mod(gf_intp a, gf_intp m, gf_intp b);
extern void gf_mulmod(gf_intp a, gf_intp b, gf_intp m, gf_intp c);
extern void gf_invmod(gf_intp A, gf_intp M, gf_intp B);
extern void gf_sqrt(gf_intp a, gf_intp M, gf_intp b);
extern void gf_gcd(gf_intp A, gf_intp B, gf_intp c);
extern int gf_is_prime(gf_intp a);

extern int gf_size(gf_intp a);
extern void gf_toraw(gf_intp a, unsigned char *dst);
extern void gf_readraw(gf_intp a, unsigned char *str, int len);

#endif

/* ---- Additional ZLIB Functions ---- */
#ifdef GZIP
extern int pack_buffer(const unsigned char *in,  unsigned long inlen,
                             unsigned char *out, unsigned long *outlen);

extern int unpack_buffer(const unsigned char *in,  unsigned long inlen,
                               unsigned char *out, unsigned long *outlen);
#endif

#ifdef HMAC
typedef struct Hmac_state {
     hash_state md;
     int hash;
     unsigned long hashsize; // here for your reference
     hash_state hashstate;
     unsigned char key[MAXBLOCKSIZE];
} hmac_state;

extern int hmac_init(hmac_state *hmac, int hash, const unsigned char *key, unsigned long keylen);
extern void hmac_process(hmac_state *hmac, const unsigned char *buf, unsigned long len);
extern void hmac_done(hmac_state *hmac, unsigned char *hash);
extern int hmac_test(void);
extern int hmac_memory(int hash, const unsigned char *key, unsigned long keylen,
                       const unsigned char *data, unsigned long len, unsigned char *dst);
extern int hmac_file(int hash, const char *fname, const unsigned char *key,
                     unsigned long keylen, unsigned char *dst);
#endif

#ifdef __cplusplus
   }
#endif

#ifndef __GNUC__
#define __attribute__(x)
#endif

#endif /* CRYPT_H_ */

