#include "mycrypt.h"

#ifdef CFB

int cfb_start(int cipher, const unsigned char *IV, const unsigned char *key, 
              int keylen, int num_rounds, symmetric_CFB *cfb)
{
   int x;

   if (cipher_is_valid(cipher) == CRYPT_ERROR) {
      return CRYPT_ERROR;
   }

   /* copy data */
   cfb->cipher = cipher;
   cfb->blocklen = cipher_descriptor[cipher].block_length;
   for (x = 0; x < cfb->blocklen; x++)
       cfb->IV[x] = IV[x];

   /* init the cipher */
   if (cipher_descriptor[cipher].setup(key, keylen, num_rounds, &cfb->key) == CRYPT_ERROR) {
      return CRYPT_ERROR;
   }

   /* encrypt the IV */
   cipher_descriptor[cfb->cipher].ecb_encrypt(cfb->IV, cfb->IV, &cfb->key);
   cfb->padlen = 0;

   return CRYPT_OK;
}

void cfb_encrypt(const unsigned char *pt, unsigned char *ct, int len, symmetric_CFB *cfb)
{
   while (len--) {
       if (cfb->padlen == cfb->blocklen) {
          cipher_descriptor[cfb->cipher].ecb_encrypt(cfb->pad, cfb->IV, &cfb->key);
          cfb->padlen = 0;
       }
       cfb->pad[cfb->padlen] = (*ct = *pt ^ cfb->IV[cfb->padlen]);
       ++pt; 
       ++ct;
       ++cfb->padlen;
   }
}

void cfb_decrypt(const unsigned char *ct, unsigned char *pt, int len, symmetric_CFB *cfb)
{
   while (len--) {
       if (cfb->padlen == cfb->blocklen) {
          cipher_descriptor[cfb->cipher].ecb_encrypt(cfb->pad, cfb->IV, &cfb->key);
          cfb->padlen = 0;
       }
       cfb->pad[cfb->padlen] = *ct;
       *pt = *ct ^ cfb->IV[cfb->padlen];
       ++pt; 
       ++ct;
       ++cfb->padlen;
   }
}

#endif

static const char *ID_TAG = "cfb.c"; 
 
