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
#define CRYPT   0x0069
#define SCRYPT  "0.69"

/* max size of either a cipher/hash block or symmetric key [largest of the two] */
#define MAXBLOCKSIZE           128

#include <mycrypt_cfg.h>
#include <mycrypt_macros.h>
#include <mycrypt_cipher.h>
#include <mycrypt_hash.h>
#include <mycrypt_prng.h>
#include <mycrypt_pk.h>
#include <mycrypt_gf.h>
#include <mycrypt_misc.h>
#include <mycrypt_kr.h>

#include <mycrypt_argchk.h>


#ifdef __cplusplus
   }
#endif

#endif /* CRYPT_H_ */

