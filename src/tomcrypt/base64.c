#include "mycrypt.h"

#ifdef BASE64

const static unsigned char *codes = 
      "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789+/";

const static unsigned char map[256] = {
255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
255, 255, 255, 255, 255, 255, 255,  62, 255, 255, 255,  63,
 52,  53,  54,  55,  56,  57,  58,  59,  60,  61, 255, 255,
255, 255, 255, 255, 255,  26,  27,  28,  29,  30,  31,  32,
 33,  34,  35,  36,  37,  38,  39,  40,  41,  42,  43,  44,
 45,  46,  47,  48,  49,  50,  51, 255, 255, 255, 255, 255,
255,   0,   1,   2,   3,   4,   5,   6,   7,   8,   9,  10,
 11,  12,  13,  14,  15,  16,  17,  18,  19,  20,  21,  22,
 23,  24,  25, 255, 255, 255, 255, 255, 255, 255, 255, 255,
255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
255, 255, 255, 255 };

int base64_encode(const unsigned char *in,  unsigned long len, 
                        unsigned char *out, unsigned long *outlen)
{
   unsigned long t, x, y;

   /* valid output size ? */
   if (*outlen < (1+4*((len/3)+2))) {
      crypt_error = "Buffer overrun in base64_encode().";
      return CRYPT_ERROR;
   }

   for (y = x = 0; x < len; ) {
       /* form a 24-bit word */
       t = in[x++];
       t = (t<<8)|((x>=len)?0:in[x++]);
       t = (t<<8)|((x>=len)?0:in[x++]);

       /* output 4 base 64 chars */
       out[y++] = codes[(t>>18)&63]; t <<= 6;
       out[y++] = codes[(t>>18)&63]; t <<= 6;
       out[y++] = codes[(t>>18)&63]; t <<= 6;
       out[y++] = codes[(t>>18)&63];

       /* output newline every 72 chars */
       if (!(y % 72))
          out[y++] = '\n';
   }

   /* append a NULL byte */
   out[y++] = '\0';

   /* return ok */
   *outlen = y;
   return CRYPT_OK;
}

int base64_decode(const unsigned char *in,  unsigned long len, 
                        unsigned char *out, unsigned long *outlen)
{
   unsigned long t, x, y, z;
   for (x = y = z = t = 0; x < len; x++) {
       if (map[in[x]] != 255) {
          t = (t<<6)|map[in[x]];
          if (++y == 4) {
             if (z == *outlen) { goto error; } out[z++] = (unsigned char)((t>>16)&255);
             if (z == *outlen) { goto error; } out[z++] = (unsigned char)((t>>8)&255);
             if (z == *outlen) { goto error; } out[z++] = (unsigned char)(t&255);
             y = t = 0;
          }
       }
   }
   *outlen = z;
   return CRYPT_OK;
error:
   crypt_error = "Buffer overflow in base64_decode().";
   return CRYPT_ERROR;
}

#endif

static const char *ID_TAG = "base64.c"; 
 
