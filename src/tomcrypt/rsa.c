#include "mycrypt.h"

#ifdef MRSA

int rsa_make_key(prng_state *prng, int wprng, int size, long e, rsa_key *key)
{
   mp_int p, q, tmp1, tmp2, tmp3;
   int res;   

   _ARGCHK(prng != NULL);
   _ARGCHK(key != NULL);

   if ((size < (1024/8)) || (size > (4096/8))) {
      crypt_error = "Invalid key size in rsa_make_key()."; 
      return CRYPT_ERROR;
   }

   if ((e < 3) || (!(e & 1))) {
      crypt_error = "Invalid value of e in rsa_make_key().";
      return CRYPT_ERROR;
   }
 
   if (prng_is_valid(wprng) != CRYPT_OK) {
      return CRYPT_ERROR;
   }

   if (mp_init_multi(&p, &q, &tmp1, &tmp2, &tmp3, NULL) != MP_OKAY) {
      crypt_error = "Out of memory in rsa_make_key()."; 
      return CRYPT_ERROR;
   }

   /* make primes p and q (optimization provided by Wayne Scott) */
   if (mp_set_int(&tmp3, e) != MP_OKAY) { goto error; }            /* tmp3 = e */

   /* make prime "p" */
   do {
       if (rand_prime(&p, size/2, prng, wprng) != CRYPT_OK) { res = CRYPT_ERROR; goto done; }
       if (mp_sub_d(&p, 1, &tmp1) != MP_OKAY)              { goto error; }  /* tmp1 = p-1 */
       if (mp_gcd(&tmp1, &tmp3, &tmp2) != MP_OKAY)         { goto error; } /* tmp2 = gcd(p-1, e) */
   } while (mp_cmp_d(&tmp2, 1) != 0);
       
   /* make prime "q" */
   do {
       if (rand_prime(&q, size/2, prng, wprng) != CRYPT_OK) { res = CRYPT_ERROR; goto done; }
       if (mp_sub_d(&q, 1, &tmp1) != MP_OKAY)              { goto error; } /* tmp1 = q-1 */
       if (mp_gcd(&tmp1, &tmp3, &tmp2) != MP_OKAY)         { goto error; } /* tmp2 = gcd(q-1, e) */
   } while (mp_cmp_d(&tmp2, 1) != 0);

   /* tmp1 = lcm(p-1, q-1) */
   if (mp_sub_d(&p, 1, &tmp2) != MP_OKAY)                  { goto error; } /* tmp2 = p-1 */
                                                                           /* tmp1 = q-1 (previous do/while loop) */
   if (mp_lcm(&tmp1, &tmp2, &tmp1) != MP_OKAY)             { goto error; } /* tmp1 = lcm(p-1, q-1) */

   /* make key */
   if (mp_init_multi(&key->e, &key->d, &key->N, &key->dQ, &key->dP, 
                     &key->qP, &key->pQ, &key->p, &key->q, NULL) != MP_OKAY) {
      goto error;
   }

   mp_set_int(&key->e, e);                                          /* key->e =  e */
   if (mp_invmod(&key->e, &tmp1, &key->d) != MP_OKAY) goto error2;  /* key->d = 1/e mod lcm(p-1,q-1) */
   if (mp_mul(&p, &q, &key->N) != MP_OKAY) goto error2;             /* key->N = pq */

/* optimize for CRT now */
   /* find d mod q-1 and d mod p-1 */
   if (mp_sub_d(&p, 1, &tmp1) != MP_OKAY)                  { goto error2; } /* tmp1 = q-1 */
   if (mp_sub_d(&q, 1, &tmp2) != MP_OKAY)                  { goto error2; } /* tmp2 = p-1 */

   if (mp_mod(&key->d, &tmp1, &key->dP) != MP_OKAY)        { goto error2; } /* dP = d mod p-1 */
   if (mp_mod(&key->d, &tmp2, &key->dQ) != MP_OKAY)        { goto error2; } /* dQ = d mod q-1 */
  
   if (mp_invmod(&q, &p, &key->qP) != MP_OKAY)             { goto error2; } /* qP = 1/q mod p */
   if (mp_mulmod(&key->qP, &q, &key->N, &key->qP))         { goto error2; } /* qP = q * (1/q mod p) mod N */

   if (mp_invmod(&p, &q, &key->pQ) != MP_OKAY)             { goto error2; } /* pQ = 1/p mod q */    
   if (mp_mulmod(&key->pQ, &p, &key->N, &key->pQ))         { goto error2; } /* pQ = p * (1/p mod q) mod N */

   if (mp_copy(&p, &key->p) != MP_OKAY)                    { goto error2; }
   if (mp_copy(&q, &key->q) != MP_OKAY)                    { goto error2; }
 
   res = CRYPT_OK;
   key->type = PK_PRIVATE_OPTIMIZED;
   goto done;
error2:
   mp_clear_multi(&key->d, &key->e, &key->N, &key->dQ, &key->dP, 
                  &key->qP, &key->pQ, &key->p, &key->q, NULL);
error:
   res = CRYPT_ERROR;
   crypt_error = "Out of memory in rsa_make_key().";
done:
   mp_clear_multi(&tmp3, &tmp2, &tmp1, &p, &q, NULL);
   return res;
}

void rsa_free(rsa_key *key)
{
   _ARGCHK(key != NULL);
   mp_clear_multi(&key->e, &key->d, &key->N, &key->dQ, &key->dP, 
                  &key->qP, &key->pQ, &key->p, &key->q, NULL);
}

int rsa_exptmod(const unsigned char *in,  unsigned long inlen,
                      unsigned char *out, unsigned long *outlen, int which,
                      rsa_key *key)
{
   mp_int tmp, tmpa, tmpb;
   unsigned long x;
   int res;

   _ARGCHK(in != NULL);
   _ARGCHK(out != NULL);
   _ARGCHK(outlen != NULL);
   _ARGCHK(key != NULL);

   if (which == PK_PRIVATE && (key->type != PK_PRIVATE && key->type != PK_PRIVATE_OPTIMIZED)) {
      crypt_error = "Invalid key type in rsa_exptmod().";
      return CRYPT_ERROR;
   }

   /* init and copy into tmp */
   if (mp_init_multi(&tmp, &tmpa, &tmpb, NULL) != MP_OKAY)                { goto error; }
   if (mp_read_unsigned_bin(&tmp, (unsigned char *)in, inlen) != MP_OKAY) { goto error; }
   
   /* sanity check on the input */
   if (mp_cmp(&key->N, &tmp) == MP_LT) {
      crypt_error = "Invalid sized input for rsa_exptmod().";
      goto error2;
   }

   /* are we using the private exponent and is the key optimized? */
   if (which == PK_PRIVATE && key->type == PK_PRIVATE_OPTIMIZED) {
      /* tmpa = tmp^dP mod p */
      if (mp_exptmod(&tmp, &key->dP, &key->p, &tmpa) != MP_OKAY)    { goto error; }

      /* tmpb = tmp^dQ mod q */
      if (mp_exptmod(&tmp, &key->dQ, &key->q, &tmpb) != MP_OKAY)    { goto error; }

      /* tmp = tmpa*qP + tmpb*pQ mod N */
      if (mp_mulmod(&tmpa, &key->qP, &key->N, &tmpa) != MP_OKAY)    { goto error; }
      if (mp_mulmod(&tmpb, &key->pQ, &key->N, &tmpb) != MP_OKAY)    { goto error; }
      if (mp_addmod(&tmpa, &tmpb, &key->N, &tmp) != MP_OKAY)        { goto error; }
   } else {
      /* exptmod it */
      if (mp_exptmod(&tmp, which==PK_PRIVATE?&key->d:&key->e, &key->N, &tmp) != MP_OKAY) { goto error; }
   }

   /* read it back */
   x = mp_raw_size(&tmp)-1;
   if (x > *outlen) {
      crypt_error = "Buffer overflow in rsa_exptmod().";
      res = CRYPT_ERROR;
      goto done;
   }
   *outlen = x;

   /* convert it */
   mp_to_unsigned_bin(&tmp, out);

   /* clean up and return */
   res = CRYPT_OK;
   goto done;
error:
   crypt_error = "Out of memory in rsa_exptmod().";
error2:
   res = CRYPT_ERROR;
done:
   mp_clear_multi(&tmp, &tmpa, &tmpb, NULL);
   return res;
}

int rsa_signpad(const unsigned char *in,  unsigned long inlen, 
                      unsigned char *out, unsigned long *outlen)
{
   unsigned long x, y;

   _ARGCHK(in != NULL);
   _ARGCHK(out != NULL);
   _ARGCHK(outlen != NULL);

   if (*outlen < (3 * inlen)) {
      crypt_error = "Output overflow in rsa_signpad().";
      return CRYPT_ERROR;
   }
   for (y = x = 0; x < inlen; x++)
       out[y++] = 0xFF;
   for (x = 0; x < inlen; x++)
       out[y++] = in[x];
   for (x = 0; x < inlen; x++)
       out[y++] = 0xFF;
   *outlen = 3 * inlen;
   return CRYPT_OK;
}

int rsa_pad(const unsigned char *in,  unsigned long inlen, 
                  unsigned char *out, unsigned long *outlen, 
                  int wprng, prng_state *prng)
{
   unsigned char buf[2048];
   unsigned long x;

   _ARGCHK(in != NULL);
   _ARGCHK(out != NULL);
   _ARGCHK(outlen != NULL);
   _ARGCHK(prng != NULL);

   /* is output big enough? */
   if (*outlen < (3 * inlen)) { 
      crypt_error = "Output overflow in rsa_pad()."; 
      return CRYPT_ERROR; 
   }

   /* get random padding required */
   if (prng_is_valid(wprng) != CRYPT_OK) {
      return CRYPT_ERROR; 
   }

   /* check inlen */
   if (inlen > 512) {
      crypt_error = "Invalid sized input for rsa_pad().";
      return CRYPT_ERROR;
   }

   if (prng_descriptor[wprng].read(buf, inlen*2-2, prng) != (inlen*2 - 2))  {
       crypt_error = "Cannot read PRNG in function rsa_pad().";
       return CRYPT_ERROR;
   }

   /* pad it like a sandwitch (sp?) 
    *
    * Looks like 0xFF R1 M R2 0xFF
    * 
    * Where R1/R2 are random and exactly equal to the length of M minus one byte.  
    */
   for (x = 0; x < inlen-1; x++) { 
       out[x+1] = buf[x]; 
   }

   for (x = 0; x < inlen; x++) {
       out[x+inlen] = in[x];
   }

   for (x = 0; x < inlen-1; x++) {
       out[x+inlen+inlen] = buf[x+inlen-1];
   }

   /* last and first bytes are 0xFF */
   out[0] = 0xFF;
   out[inlen+inlen+inlen-1] = 0xFF;

   /* clear up and return */
#ifdef CLEAN_STACK
   zeromem(buf, sizeof(buf));
#endif
   *outlen = inlen*3;
   return CRYPT_OK;
}

int rsa_signdepad(const unsigned char *in,  unsigned long inlen, 
                    unsigned char *out, unsigned long *outlen)
{
   unsigned long x;

   _ARGCHK(in != NULL);
   _ARGCHK(out != NULL);
   _ARGCHK(outlen != NULL);

   if (*outlen < inlen/3) { 
      crypt_error = "Output not big enough in rsa_signdepad()."; 
      return CRYPT_ERROR; 
   }

   /* check padding bytes */
   for (x = 0; x < inlen/3; x++) {
       if (in[x] != 0xFF || in[x+(inlen/3)+(inlen/3)] != 0xFF) {
          crypt_error = "Invalid padding format for rsa_signdepad().";
          return CRYPT_ERROR;
       }
   }
   for (x = 0; x < inlen/3; x++) 
       out[x] = in[x+(inlen/3)];
   *outlen = inlen/3;
   return CRYPT_OK;
}

int rsa_depad(const unsigned char *in,  unsigned long inlen, 
                    unsigned char *out, unsigned long *outlen)
{
   unsigned long x;

   _ARGCHK(in != NULL);
   _ARGCHK(out != NULL);
   _ARGCHK(outlen != NULL);

   if (*outlen < inlen/3) { 
      crypt_error = "Output not big enough in rsa_depad()."; 
      return CRYPT_ERROR; 
   }
   for (x = 0; x < inlen/3; x++) 
       out[x] = in[x+(inlen/3)];
   *outlen = inlen/3;
   return CRYPT_OK;
}

#define OUTPUT_BIGNUM(num, buf2, y, z)         \
{                                              \
      z = mp_raw_size(num);                    \
      STORE32L(z, buf2+y);                     \
      y += 4;                                  \
      mp_toraw(num, buf2+y);                   \
      y += z;                                  \
}


#define INPUT_BIGNUM(num, in, x, y)                              \
{                                                                \
     /* load value */                                            \
     LOAD32L(x, in+y);                                           \
     y += 4;                                                     \
                                                                 \
     /* sanity check... */                                       \
     if (x > 1024) {                                             \
        crypt_error = "Invalid size of data in rsa_import().";   \
        goto error2;                                             \
     }                                                           \
                                                                 \
     /* load it */                                               \
     if (mp_read_raw(num, (unsigned char *)in+y, x) != MP_OKAY) {\
        crypt_error = "Out of memory in rsa_import().";          \
        goto error2;                                             \
     }                                                           \
     y += x;                                                     \
}

int rsa_export(unsigned char *out, unsigned long *outlen, int type, rsa_key *key)
{
   unsigned char buf2[5120];
   unsigned long y, z;

   _ARGCHK(out != NULL);
   _ARGCHK(outlen != NULL);
   _ARGCHK(key != NULL);

   /* type valid? */
   if (!(key->type == PK_PRIVATE || key->type == PK_PRIVATE_OPTIMIZED) && 
        (type == PK_PRIVATE || type == PK_PRIVATE_OPTIMIZED)) { 
      crypt_error = "Invalid key type in rsa_export()."; 
      return CRYPT_ERROR; 
   }

   /* start at offset y=PACKET_SIZE */
   y = PACKET_SIZE;

   /* output key type */
   buf2[y++] = type;

   /* output modulus */
   OUTPUT_BIGNUM(&key->N, buf2, y, z);
  
   /* output public key */
   OUTPUT_BIGNUM(&key->e, buf2, y, z);

   if (type == PK_PRIVATE || type == PK_PRIVATE_OPTIMIZED) {
      OUTPUT_BIGNUM(&key->d, buf2, y, z);
   }

   if (type == PK_PRIVATE_OPTIMIZED) {
      OUTPUT_BIGNUM(&key->dQ, buf2, y, z);
      OUTPUT_BIGNUM(&key->dP, buf2, y, z);
      OUTPUT_BIGNUM(&key->pQ, buf2, y, z);
      OUTPUT_BIGNUM(&key->qP, buf2, y, z);
      OUTPUT_BIGNUM(&key->p, buf2, y, z);
      OUTPUT_BIGNUM(&key->q, buf2, y, z);
   }

   /* check size */
   if (*outlen < y) { 
      crypt_error = "Buffer overrun in rsa_export()."; 
      return CRYPT_ERROR; 
   }

   /* store packet header */
   packet_store_header(buf2, PACKET_SECT_RSA, PACKET_SUB_KEY, y);

   /* copy to the user buffer */
   memcpy(out, buf2, y);
   *outlen = y;

   /* clear stack and return */
#ifdef CLEAN_STACK
   zeromem(buf2, sizeof(buf2));
#endif
   return CRYPT_OK;
}

int rsa_import(const unsigned char *in, rsa_key *key)
{
   unsigned long x, y;

   _ARGCHK(in != NULL);
   _ARGCHK(key != NULL);

   /* test packet header */
   if (packet_valid_header((unsigned char *)in, PACKET_SECT_RSA, PACKET_SUB_KEY) != CRYPT_OK) { 
      return CRYPT_ERROR;
   }

   /* init key */
   if (mp_init_multi(&key->e, &key->d, &key->N, &key->dQ, &key->dP, &key->qP, 
                     &key->pQ, &key->p, &key->q, NULL) != MP_OKAY) {
      crypt_error = "Out of memory in rsa_import().";
      return CRYPT_ERROR;
   }

   /* get key type */
   y = PACKET_SIZE;
   key->type = in[y++];

   /* load the modulus  */
   INPUT_BIGNUM(&key->N, in, x, y);

   /* load public exponent */
   INPUT_BIGNUM(&key->e, in, x, y);

   /* get private exponent */
   if (key->type == PK_PRIVATE || key->type == PK_PRIVATE_OPTIMIZED) {
      INPUT_BIGNUM(&key->d, in, x, y);
   }

   /* get CRT private data if required */
   if (key->type == PK_PRIVATE_OPTIMIZED) {
      INPUT_BIGNUM(&key->dQ, in, x, y);
      INPUT_BIGNUM(&key->dP, in, x, y);
      INPUT_BIGNUM(&key->pQ, in, x, y);
      INPUT_BIGNUM(&key->qP, in, x, y);
      INPUT_BIGNUM(&key->p, in, x, y);
      INPUT_BIGNUM(&key->q, in, x, y);
   }

   return CRYPT_OK;
error2:
   mp_clear_multi(&key->d, &key->e, &key->N, &key->dQ, &key->dP, 
                  &key->pQ, &key->qP, &key->p, &key->q, NULL);
   return CRYPT_ERROR;
}

#endif /* RSA */


