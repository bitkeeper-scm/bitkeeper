/* This is the build config file.
 *
 * With this you can setup what to inlcude/exclude automatically during any build.  Just comment
 * out the line that #define's the word for the thing you want to remove.  phew!
 */

#ifndef MYCRYPT_CFG_H
#define MYCRYPT_CFG_H

/* Controls endianess and size of registers.  Leave uncommented to get platform neutral [slower] code */

/* detect x86-32 machines somewhat */
#if (defined(_MSC_VER) && defined(WIN32))  || (defined(__GNUC__) && (defined(__DJGPP__) || defined(__CYGWIN__) || defined(__MINGW32__)))
   #define ENDIAN_LITTLE
   #define ENDIAN_32BITWORD
#endif


/* #define ENDIAN_LITTLE */
/* #define ENDIAN_BIG */

/* #define ENDIAN_32BITWORD */
/* #define ENDIAN_64BITWORD */

#if defined(ENDIAN_BIG) || defined(ENDIAN_LITTLE) && !(defined(ENDIAN_32BITWORD) || defined(ENDIAN_64BITWORD))
    #error You must specify a word size as well as endianess in mycrypt_cfg.h
#endif

#if !(defined(ENDIAN_BIG) || defined(ENDIAN_LITTLE))
   #define ENDIAN_NEUTRAL
#endif

/* Clean the stack after sensitive functions.  Not always required... 
 * With this defined most of the ciphers and hashes will clean their stack area
 * after usage with a (sometimes) huge penalty in speed.  Normally this is not
 * required if you simply lock your stack and wipe it when your program is done.
 */
/* #define CLEAN_STACK */

/* What algorithms to include? comment out and rebuild to remove em */
/* ciphers */
#define BLOWFISH
#define RC2
#define RC5
#define RC6
#define SERPENT
#define SAFERP  /* for the SAFER+ cipher */
#define SAFER   /* for the SAFER K64, K128, SK64 and SK128 ciphers */
#define RIJNDAEL
#define XTEA
#define TWOFISH
#define DES

/* Small Ram Variant of Twofish.  For this you must have TWOFISH defined.  This
 * variant requires about 4kb less memory but is considerably slower.  It is ideal
 * when high throughput is less important than conserving memory. By default it is
 * not defined which means the larger ram (about 4.2Kb used) variant is built.
 */
/* #define TWOFISH_SMALL */

/* Tell Twofish to use precomputed tables.  If you want to use the small table
 * variant of Twofish you may want to turn this on.  Essentially it tells Twofish to use
 * precomputed S-boxes (Q0 and Q1) as well as precomputed GF multiplications [in the MDS].
 * This speeds up the cipher somewhat.
 */
/* #define TWOFISH_TABLES */

/* modes */
#define CFB
#define OFB
#define ECB
#define CBC
#define CTR

/* hashes */
#define SHA512
#define SHA384
#define SHA256
#define TIGER
#define SHA1
#define MD5
#define MD4

#ifdef SHA384
   #ifndef SHA512
      #error The SHA384 hash requires SHA512 to be defined!
   #endif
#endif

/* base64 */
#define BASE64

/* prngs */
#define YARROW
#define SPRNG

#ifdef YARROW
   #ifndef CTR
      #error YARROW Requires CTR mode
   #endif
#endif

/* PK code */
#define MRSA     /* include RSA code ? */
#define PKCS     /* include the RSA PKCS code? */

#define MDH      /* Include Diffie-Hellman? */

#define MECC     /* Include ECC code? */

#define KR       /* Include keyring support? */

/* packet code */
#if defined(MRSA) || defined(MDH) || defined(MECC)
    #define PACKET

    /* size of a packet header in bytes */
    #define PACKET_SIZE            8

    /* Section tags */
    #define PACKET_SECT_RSA        0
    #define PACKET_SECT_DH         1
    #define PACKET_SECT_ECC        2

    /* Subsection Tags for the first three sections */
    #define PACKET_SUB_KEY         0
    #define PACKET_SUB_ENCRYPTED   1
    #define PACKET_SUB_SIGNED      2
    #define PACKET_SUB_ENC_KEY     3
#endif

/* Diffie-Hellman key settings you can omit ones you don't want to save space */
#ifdef MDH

#define DH512
#define DH768
#define DH1024
#define DH1280
#define DH1536
#define DH1792
#define DH2048
#define DH2560
#define DH3072
#define DH4096

#endif /* MDH */

/* ECC Key settings */
#ifdef MECC 

#define ECC160
#define ECC192
#define ECC224
#define ECC256
#define ECC384
#define ECC521

#endif /* MECC */

/* include GF math routines?  (not currently used by anything internally) */
#define GF

/* include large integer math routines? */
#define MPI

/* Use a small prime table?  It greatly reduces the size of prime.c at a little impact
 * in speed.
 */
#define SMALL_PRIME_TAB

/* Include the ZLIB compression library */
#define GZIP

#ifdef GZIP
   #ifndef OMIT_ZLIB
       #include "zlib.h"
   #endif
#endif

/* include HMAC support */
#define HMAC

#ifdef MPI
   #include "mpi.h"
#else
   #ifdef MRSA
      #error RSA requires the big int library 
   #endif
   #ifdef MECC
      #error ECC requires the big int library 
   #endif
   #ifdef MDH
      #error DH requires the big int library 
   #endif
#endif /* MPI */

/* Use /dev/urandom first on devices where /dev/random is too slow */
/* #define TRY_URANDOM_FIRST */

#endif /* MYCRYPT_CFG_H */

