#include "mycrypt.h"

#ifdef CBC

int cbc_start(int cipher, const unsigned char *IV, const unsigned char *key, 
              int keylen, int num_rounds, symmetric_CBC *cbc)
{
   int x;

   /* bad param? */
   if (cipher_is_valid(cipher) == CRYPT_ERROR) {
      return CRYPT_ERROR;
   }

   /* setup cipher */
   if (cipher_descriptor[cipher].setup(key, keylen, num_rounds, &cbc->key) == CRYPT_ERROR) {
      return CRYPT_ERROR;
   }

   /* copy IV */
   cbc->blocklen = cipher_descriptor[cipher].block_length;
   cbc->cipher   = cipher;
   for (x = 0; x < cbc->blocklen; x++) {
       cbc->IV[x] = IV[x];
   }
   return CRYPT_OK;
}

int cbc_encrypt(const unsigned char *pt, unsigned char *ct, symmetric_CBC *cbc)
{
   int x;
   unsigned char tmp[MAXBLOCKSIZE];

   /* xor IV against plaintext */
   for (x = 0; x < cbc->blocklen; x++) {
       tmp[x] = pt[x] ^ cbc->IV[x];
   }

   /* encrypt */
   if (cipher_is_valid(cbc->cipher) == CRYPT_ERROR) {
       return CRYPT_ERROR;
   }
   cipher_descriptor[cbc->cipher].ecb_encrypt(tmp, ct, &cbc->key);

   /* store IV [ciphertext] for a future block */
   for (x = 0; x < cbc->blocklen; x++) 
       cbc->IV[x] = ct[x];
#ifdef CLEAN_STACK
   zeromem(tmp, sizeof(tmp));
#endif
   return CRYPT_OK;
}

int cbc_decrypt(const unsigned char *ct, unsigned char *pt, symmetric_CBC *cbc)
{
   int x;
   unsigned char tmp[MAXBLOCKSIZE], tmp2[MAXBLOCKSIZE];

   /* decrypt the block from ct into tmp */
   if (cipher_is_valid(cbc->cipher) == CRYPT_ERROR) {
       return CRYPT_ERROR;
   }
   cipher_descriptor[cbc->cipher].ecb_decrypt(ct, tmp, &cbc->key);

   /* xor IV against the plaintext of the previous step */
   for (x = 0; x < cbc->blocklen; x++) { 
       /* copy CT in case ct == pt */
       tmp2[x] = ct[x]; 

       /* actually decrypt the byte */
       pt[x] = tmp[x] ^ cbc->IV[x]; 
   }

   /* replace IV with this current ciphertext */ 
   for (x = 0; x < cbc->blocklen; x++) {
       cbc->IV[x] = tmp2[x];
   }
#ifdef CLEAN_STACK
   zeromem(tmp, sizeof(tmp));
   zeromem(tmp2, sizeof(tmp2));
#endif
   return CRYPT_OK;
}

#endif

