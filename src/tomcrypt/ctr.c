#include "mycrypt.h"

#ifdef CTR

int ctr_start(int cipher, const unsigned char *count, const unsigned char *key, int keylen, 
              int num_rounds, symmetric_CTR *ctr)
{
   int x;

   /* bad param? */
   if (cipher_is_valid(cipher) == CRYPT_ERROR) {
      return CRYPT_ERROR;
   }

   /* setup cipher */
   if (cipher_descriptor[cipher].setup(key, keylen, num_rounds, &ctr->key) == CRYPT_ERROR) {
      return CRYPT_ERROR;
   }

   /* copy ctr */
   ctr->blocklen = cipher_descriptor[cipher].block_length;
   ctr->cipher   = cipher;
   ctr->padlen   = 0;
   for (x = 0; x < ctr->blocklen; x++) {
       ctr->ctr[x] = count[x];
   }
   cipher_descriptor[ctr->cipher].ecb_encrypt(ctr->ctr, ctr->pad, &ctr->key);
   return CRYPT_OK;
}

void ctr_encrypt(const unsigned char *pt, unsigned char *ct, int len, symmetric_CTR *ctr)
{
   int x;

   while (len--) {
      /* is the pad empty? */
      if (ctr->padlen == ctr->blocklen) {
         /* increment counter */
         for (x = 0; x < ctr->blocklen; x++) {
            ctr->ctr[x] = (ctr->ctr[x] + 1) & 255;
            if (ctr->ctr[x] != 0) {
               break;
            }
         }

         /* encrypt it */
         cipher_descriptor[ctr->cipher].ecb_encrypt(ctr->ctr, ctr->pad, &ctr->key);
         ctr->padlen = 0;
      }
      *ct++ = *pt++ ^ ctr->pad[ctr->padlen++];
   }
}

void ctr_decrypt(const unsigned char *ct, unsigned char *pt, int len, symmetric_CTR *ctr)
{
   ctr_encrypt(ct, pt, len, ctr);
}

#endif

static const char __attribute__((unused)) *ID_TAG = "ctr.c";

