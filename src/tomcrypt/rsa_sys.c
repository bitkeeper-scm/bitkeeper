#include "mycrypt.h"

#ifdef MRSA 

/* Encrypted Message Format:

offset    |  length   |    Contents
----------------------------------------------------------------------
0         |    1      |  cipher ID
1         |    4      |  length of RSA encrypted value (big endian format)
5         |    q      |  the rsa_pad()'ed RSA encrypted value (big endian format)
5+q       |    p      |  The CTR IV value used (varies in size based on cipher used)
9+q+p     |    4      |  length of message (big endian format)
9+q+p     |    j      |  ciphertext
----------------------------------------------------------------------

*/

int rsa_encrypt(const unsigned char *in,  unsigned long len, 
                      unsigned char *out, unsigned long *outlen,
                      prng_state *prng, int wprng, int cipher, 
                      rsa_key *key)
{
   unsigned char sym_IV[MAXBLOCKSIZE], sym_key[MAXBLOCKSIZE], rsa_in[4096], rsa_out[4096];
   symmetric_CTR ctr;
   unsigned long x, y, blklen, rsa_size;
   int keylen;

   /* are the parameters valid? */
   if (prng_is_valid(wprng) == CRYPT_ERROR ||
      cipher_is_valid(cipher) == CRYPT_ERROR) {
      return CRYPT_ERROR;
   }

   /* setup the CTR key */
   keylen = 32;                                                             /* default to 256-bit keys */
   if (cipher_descriptor[cipher].keysize(&keylen) == CRYPT_ERROR) {
      crypt_error = "Could not get suggested key size in rsa_encrypt().";
      return CRYPT_ERROR;
   }

   blklen = cipher_descriptor[cipher].block_length;
   if (prng_descriptor[wprng].read(sym_key, keylen, prng) != (unsigned long)keylen) {
      crypt_error = "Error reading PRNG in rsa_encrypt()."; 
      return CRYPT_ERROR;
   }
   if (prng_descriptor[wprng].read(sym_IV, blklen, prng) != blklen) {
      crypt_error = "Error reading PRNG in rsa_encrypt()."; 
      return CRYPT_ERROR;
   }

   /* setup CTR mode */
   if (ctr_start(cipher, sym_IV, sym_key, keylen, 0, &ctr) == CRYPT_ERROR) {
      return CRYPT_ERROR;
   }

   /* rsa_pad the symmetric key */
   y = sizeof(rsa_in); 
   if (rsa_pad(sym_key, keylen, rsa_in, &y, wprng, prng) == CRYPT_ERROR) {
      return CRYPT_ERROR;
   }
   
   /* rsa encrypt it */
   rsa_size = sizeof(rsa_out);
   if (rsa_exptmod(rsa_in, y, rsa_out, &rsa_size, PK_PUBLIC, key) == CRYPT_ERROR) {
      return CRYPT_ERROR;
   }

   /* check size */
   if (*outlen < (PACKET_SIZE+9+rsa_size+blklen+len)) { 
      crypt_error = "Buffer overrun in rsa_encrypt().";
      return CRYPT_ERROR; 
   }

   /* now lets make the header */
   y = PACKET_SIZE;
   out[y++] = cipher_descriptor[cipher].ID;

   /* store the size of the RSA value */
   STORE32L(rsa_size, (out+y));
   y += 4;

   /* store the rsa value */
   for (x = 0; x < rsa_size; x++, y++) {
       out[y] = rsa_out[x];
   }

   /* store the IV used */
   for (x = 0; x < blklen; x++, y++) {
       out[y] = sym_IV[x];
   }
       
   /* store the length */
   STORE32L(len, (out+y));
   y += 4;

   /* encrypt the message */
   ctr_encrypt(in, out+y, len, &ctr);
   y += len;

   /* store the header */
   packet_store_header(out, PACKET_SECT_RSA, PACKET_SUB_ENCRYPTED, y);
   
   /* clean up */
   zeromem(sym_key, sizeof(sym_key));
   zeromem(sym_IV, sizeof(sym_IV));
   zeromem(&ctr, sizeof(ctr));
   zeromem(rsa_in, sizeof(rsa_in));
   zeromem(rsa_out, sizeof(rsa_out));
   *outlen = y;
   return CRYPT_OK;
}

int rsa_decrypt(const unsigned char *in,  unsigned long len, 
                      unsigned char *out, unsigned long *outlen, 
                      rsa_key *key)
{
   unsigned char sym_IV[MAXBLOCKSIZE], sym_key[MAXBLOCKSIZE], rsa_in[4096], rsa_out[4096];
   symmetric_CTR ctr;
   unsigned long x, y, z, keylen, blklen, rsa_size;
   int cipher;

   /* right key type? */
   if (key->type != PK_PRIVATE && key->type != PK_PRIVATE_OPTIMIZED) {
      crypt_error = "Cannot decrypt with public key in rsa_decrypt().";
      return CRYPT_ERROR;
   }

   /* check the header */
   if (packet_valid_header((unsigned char *)in, PACKET_SECT_RSA, PACKET_SUB_ENCRYPTED) == CRYPT_ERROR) {
      crypt_error = "Invalid header for input in rsa_decrypt().";
      return CRYPT_ERROR;
   }

   /* grab cipher name */
   y = PACKET_SIZE;
   cipher = find_cipher_id(in[y++]);
   if (cipher == -1) {
      crypt_error = "Invalid cipher name for rsa_decrypt().";
      return CRYPT_ERROR;
   }
   keylen = MIN(cipher_descriptor[cipher].max_key_length, 32);
   blklen = cipher_descriptor[cipher].block_length;

   /* grab length of the rsa key */
   LOAD32L(rsa_size, (in+y))
   y += 4;

   /* read it in */
   for (x = 0; x < rsa_size; x++, y++) {
       rsa_in[x] = in[y];
   }

   /* decrypt it */
   x = sizeof(rsa_out);
   if (rsa_exptmod(rsa_in, rsa_size, rsa_out, &x, PK_PRIVATE, key) == CRYPT_ERROR) 
      return CRYPT_ERROR;

   /* depad it */
   z = sizeof(sym_key);
   if (rsa_depad(rsa_out, x, sym_key, &z) == CRYPT_ERROR) {
      return CRYPT_ERROR;
   }

   /* read the IV in */
   for (x = 0; x < blklen; x++, y++)
       sym_IV[x] = in[y];

   /* setup CTR mode */
   if (ctr_start(cipher, sym_IV, sym_key, keylen, 0, &ctr) == CRYPT_ERROR) {
      return CRYPT_ERROR;
   }

   /* get len */
   LOAD32L(len, (in+y));
   y += 4;

   /* check size */
   if (*outlen < len) { 
      crypt_error = "Buffer overrun in rsa_decrypt()."; 
      return CRYPT_ERROR; 
   }

   /* decrypt the message */
   ctr_decrypt(in+y, out, len, &ctr);
   
   /* clean up */
   zeromem(sym_key, sizeof(sym_key));
   zeromem(sym_IV, sizeof(sym_IV));
   zeromem(&ctr, sizeof(ctr));
   zeromem(rsa_in, sizeof(rsa_in));
   zeromem(rsa_out, sizeof(rsa_out));
   *outlen = len;
   return CRYPT_OK;
}

/* Signature Message Format 
offset    |  length   |    Contents
----------------------------------------------------------------------
0         |    1      | hash ID
1         |    4      | length of rsa_pad'ed signature
5         |    p      | the rsa_pad'ed signature
*/

int rsa_sign(const unsigned char *in,  unsigned long inlen, 
                   unsigned char *out, unsigned long *outlen, 
                   int hash, rsa_key *key)
{
   unsigned long hashlen, rsa_size, x, y, z;
   unsigned char rsa_in[4096], rsa_out[4096];

   /* type of key? */
   if (key->type != PK_PRIVATE && key->type != PK_PRIVATE_OPTIMIZED) {
      crypt_error = "Cannot sign with public key in rsa_sign().";
      return CRYPT_ERROR;
   }

   /* are the parameters valid? */
   if (hash_is_valid(hash)  == CRYPT_ERROR) {
      return CRYPT_ERROR;
   }

   /* hash it */
   hashlen = hash_descriptor[hash].hashsize;
   z = sizeof(rsa_in);
   if (hash_memory(hash, in, inlen, rsa_in, &z) == CRYPT_ERROR) {
      return CRYPT_ERROR;
   }

   /* pad it */
   x = sizeof(rsa_in);
   if (rsa_signpad(rsa_in, hashlen, rsa_out, &x) == CRYPT_ERROR) {
      return CRYPT_ERROR;
   }

   /* sign it */
   rsa_size = sizeof(rsa_in);
   if (rsa_exptmod(rsa_out, x, rsa_in, &rsa_size, PK_PRIVATE, key) == CRYPT_ERROR) {
      return CRYPT_ERROR;
   }

   /* check size */
   if (*outlen < (PACKET_SIZE+rsa_size)) {
      crypt_error = "Buffer Overrun in rsa_sign()."; 
      return CRYPT_ERROR;
   }

   /* now lets output the message */
   y = PACKET_SIZE;
   out[y++] = hash_descriptor[hash].ID;

   /* output the len */
   STORE32L(rsa_size, (out+y));
   y += 4;

   /* store the signature */
   for (x = 0; x < rsa_size; x++, y++) {
       out[y] = rsa_in[x];
   }

   /* store header */
   packet_store_header(out, PACKET_SECT_RSA, PACKET_SUB_SIGNED, y);

   /* clean up */
   zeromem(rsa_in, sizeof(rsa_in));
   zeromem(rsa_out, sizeof(rsa_out));
   *outlen = y;
   return CRYPT_OK;
}

int rsa_verify(const unsigned char *sig, const unsigned char *msg,
                     unsigned long inlen, int *stat,
                     rsa_key *key)
{
   unsigned long hashlen, rsa_size, x, y, z, w;
   int hash;
   unsigned char rsa_in[4096], rsa_out[4096];

   /* always be incorrect by default */
   *stat = 0;

   /* verify header */
   if (packet_valid_header((unsigned char *)sig, PACKET_SECT_RSA, PACKET_SUB_SIGNED) == CRYPT_ERROR) {
      crypt_error = "Invalid header for input in rsa_verify().";
      return CRYPT_ERROR;
   }

   /* grab hash name */
   y = PACKET_SIZE;
   hash = find_hash_id(sig[y++]);
   if (hash == -1) {
      crypt_error = "Invalid hash ID for rsa_verify().";
      return CRYPT_ERROR;
   }
   hashlen = hash_descriptor[hash].hashsize;

   /* get the len */
   LOAD32L(rsa_size, (sig+y));
   y += 4;

   /* load the signature */
   for (x = 0; x < rsa_size; x++, y++) {
       rsa_in[x] = sig[y];
   }

   /* exptmod it */
   x = sizeof(rsa_in);
   if (rsa_exptmod(rsa_in, rsa_size, rsa_out, &x, PK_PUBLIC, key) == CRYPT_ERROR) {
      return CRYPT_ERROR;
   }

   /* depad it */
   z = sizeof(rsa_in);
   if (rsa_signdepad(rsa_out, x, rsa_in, &z) == CRYPT_ERROR) {
      return CRYPT_ERROR;
   }

   /* check? */
   w = sizeof(rsa_out);
   if (hash_memory(hash, msg, inlen, rsa_out, &w) == CRYPT_ERROR) {
      return CRYPT_ERROR;
   }

   if ((z == hashlen) && (!memcmp(rsa_in, rsa_out, hashlen))) {
      *stat = 1;
   }

   zeromem(rsa_in, sizeof(rsa_in));
   zeromem(rsa_out, sizeof(rsa_out));
   return CRYPT_OK;
}

/* these are smaller routines written by Clay Culver.  They do the same function as the rsa_encrypt/decrypt 
 * except that they are used to RSA encrypt/decrypt a single value and not a packet.
 */
int rsa_encrypt_key(const unsigned char *inkey, unsigned long inlen,
                    unsigned char *outkey, unsigned long *outlen,
                    prng_state *prng, int wprng, rsa_key *key)
{
   unsigned char rsa_in[4096], rsa_out[4096];
   unsigned long x, y, rsa_size;

   /* are the parameters valid? */
   if (prng_is_valid(wprng) == CRYPT_ERROR) {
      return CRYPT_ERROR; 
   }

   /* rsa_pad the symmetric key */
   y = sizeof(rsa_in); 
   if (rsa_pad(inkey, inlen, rsa_in, &y, wprng, prng) == CRYPT_ERROR) {
      return CRYPT_ERROR;
   }
   
   /* rsa encrypt it */
   rsa_size = sizeof(rsa_out);
   if (rsa_exptmod(rsa_in, y, rsa_out, &rsa_size, PK_PUBLIC, key) == CRYPT_ERROR) {
      return CRYPT_ERROR;
   }

   /* check size */
   if (*outlen < (7+rsa_size)) { 
      crypt_error = "Buffer overrun in rsa_encrypt_key().";
      return CRYPT_ERROR; 
   }

   /* now lets make the header */
   y = PACKET_SIZE;
   
   /* store the size of the RSA value */
   STORE32L(rsa_size, (outkey+y));
   y += 4;

   /* store the rsa value */
   for (x = 0; x < rsa_size; x++, y++) {
       outkey[y] = rsa_out[x];
   }

   /* store header */
   packet_store_header(outkey, PACKET_SECT_RSA, PACKET_SUB_ENC_KEY, y);

   /* clean up */
   zeromem(rsa_in, sizeof(rsa_in));
   zeromem(rsa_out, sizeof(rsa_out));
   *outlen = y;
   return CRYPT_OK;
}

int rsa_decrypt_key(const unsigned char *in, unsigned char *outkey, 
                    unsigned long *keylen, rsa_key *key)
{
   unsigned char sym_key[MAXBLOCKSIZE], rsa_in[4096], rsa_out[4096];
   unsigned long x, y, z, i, rsa_size;

   /* right key type? */
   if (key->type != PK_PRIVATE && key->type != PK_PRIVATE_OPTIMIZED) {
      crypt_error = "Cannot decrypt with public key in rsa_decrypt_key().";
      return CRYPT_ERROR;
   }

   /* check the header */
   if (packet_valid_header((unsigned char *)in, PACKET_SECT_RSA, PACKET_SUB_ENC_KEY) == CRYPT_ERROR) {
      crypt_error = "Invalid header for input in rsa_decrypt_key().";
      return CRYPT_ERROR;
   }

   /* grab length of the rsa key */
   y = PACKET_SIZE;
   LOAD32L(rsa_size, (in+y))
   y += 4;

   /* read it in */
   for (x = 0; x < rsa_size; x++, y++) {
       rsa_in[x] = in[y];
   }

   /* decrypt it */
   x = sizeof(rsa_out);
   if (rsa_exptmod(rsa_in, rsa_size, rsa_out, &x, PK_PRIVATE, key) == CRYPT_ERROR) 
      return CRYPT_ERROR;

   /* depad it */
   z = sizeof(sym_key);
   if (rsa_depad(rsa_out, x, sym_key, &z) == CRYPT_ERROR) {
      return CRYPT_ERROR;
   }

   /* check size */
   if (*keylen < z) { 
      crypt_error = "Buffer overrun in rsa_decrypt_key()."; 
      return CRYPT_ERROR; 
   }

   for (i = 0; i < z; i++) {
     outkey[i] = sym_key[i];
   }
   
   /* clean up */
   zeromem(sym_key, sizeof(sym_key));
   zeromem(rsa_in, sizeof(rsa_in));
   zeromem(rsa_out, sizeof(rsa_out));
   *keylen = z;
   return CRYPT_OK;
}

int rsa_sign_hash(const unsigned char *in,  unsigned long inlen, 
                        unsigned char *out, unsigned long *outlen, 
                        rsa_key *key)
{
   unsigned long rsa_size, x, y;
   unsigned char rsa_in[4096], rsa_out[4096];

   /* type of key? */
   if (key->type != PK_PRIVATE && key->type != PK_PRIVATE_OPTIMIZED) {
      crypt_error = "Cannot sign with public key in rsa_sign().";
      return CRYPT_ERROR;
   }

   /* pad it */
   x = sizeof(rsa_out);
   if (rsa_signpad(in, inlen, rsa_out, &x) == CRYPT_ERROR) {
      return CRYPT_ERROR;
   }

   /* sign it */
   rsa_size = sizeof(rsa_in);
   if (rsa_exptmod(rsa_out, x, rsa_in, &rsa_size, PK_PRIVATE, key) == CRYPT_ERROR) {
      return CRYPT_ERROR;
   }

   /* check size */
   if (*outlen < (PACKET_SIZE+rsa_size)) {
      crypt_error = "Buffer Overrun in rsa_sign_hash()."; 
      return CRYPT_ERROR;
   }

   /* now lets output the message */
   y = PACKET_SIZE;

   /* output the len */
   STORE32L(rsa_size, (out+y));
   y += 4;

   /* store the signature */
   for (x = 0; x < rsa_size; x++, y++) {
       out[y] = rsa_in[x];
   }

   /* store header */
   packet_store_header(out, PACKET_SECT_RSA, PACKET_SUB_SIGNED, y);

   /* clean up */
   zeromem(rsa_in, sizeof(rsa_in));
   zeromem(rsa_out, sizeof(rsa_out));
   *outlen = y;
   return CRYPT_OK;
}

int rsa_verify_hash(const unsigned char *sig, const unsigned char *md,
                          int *stat, rsa_key *key)
{
   unsigned long rsa_size, x, y, z;
   unsigned char rsa_in[4096], rsa_out[4096];

   /* always be incorrect by default */
   *stat = 0;

   /* verify header */
   if (packet_valid_header((unsigned char *)sig, PACKET_SECT_RSA, PACKET_SUB_SIGNED) == CRYPT_ERROR) {
      crypt_error = "Invalid header for input in rsa_verify().";
      return CRYPT_ERROR;
   }

   /* get the len */
   y = PACKET_SIZE;
   LOAD32L(rsa_size, (sig+y));
   y += 4;

   /* load the signature */
   for (x = 0; x < rsa_size; x++, y++) {
       rsa_in[x] = sig[y];
   }

   /* exptmod it */
   x = sizeof(rsa_in);
   if (rsa_exptmod(rsa_in, rsa_size, rsa_out, &x, PK_PUBLIC, key) == CRYPT_ERROR) {
      return CRYPT_ERROR;
   }

   /* depad it */
   z = sizeof(rsa_in);
   if (rsa_signdepad(rsa_out, x, rsa_in, &z) == CRYPT_ERROR) {
      return CRYPT_ERROR;
   }

   /* check? */
   if (!memcmp(rsa_in, md, z)) {
      *stat = 1;
   }

   zeromem(rsa_in, sizeof(rsa_in));
   zeromem(rsa_out, sizeof(rsa_out));
   return CRYPT_OK;
}



#endif



