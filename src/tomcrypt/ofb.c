#include "mycrypt.h"

#ifdef OFB

int ofb_start(int cipher, const unsigned char *IV, const unsigned char *key, 
              int keylen, int num_rounds, symmetric_OFB *ofb)
{
   int x;

   if (cipher_is_valid(cipher) == CRYPT_ERROR) {
      return CRYPT_ERROR;
   }

   /* copy details */
   ofb->cipher = cipher;
   ofb->blocklen = cipher_descriptor[cipher].block_length;
   for (x = 0; x < ofb->blocklen; x++) {
       ofb->IV[x] = IV[x];
   }

   /* init the cipher */
   ofb->padlen = ofb->blocklen;
   return cipher_descriptor[cipher].setup(key, keylen, num_rounds, &ofb->key);
}

void ofb_encrypt(const unsigned char *pt, unsigned char *ct, int len, symmetric_OFB *ofb)
{
   while (len--) {
       if (ofb->padlen == ofb->blocklen) {
          cipher_descriptor[ofb->cipher].ecb_encrypt(ofb->IV, ofb->IV, &ofb->key);
          ofb->padlen = 0;
       }
       *ct++ = *pt++ ^ ofb->IV[ofb->padlen++];
   }
}

void ofb_decrypt(const unsigned char *ct, unsigned char *pt, int len, symmetric_OFB *ofb)
{
   ofb_encrypt(ct, pt, len, ofb);
}


#endif

static const char *ID_TAG = "ofb.c"; 
 
