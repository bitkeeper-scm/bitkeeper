#include "mycrypt.h"

char *crypt_error;

struct _cipher_descriptor cipher_descriptor[32] = {
{ NULL, 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL, NULL },
{ NULL, 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL, NULL },
{ NULL, 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL, NULL },
{ NULL, 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL, NULL },
{ NULL, 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL, NULL },
{ NULL, 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL, NULL },
{ NULL, 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL, NULL },
{ NULL, 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL, NULL },
{ NULL, 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL, NULL },
{ NULL, 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL, NULL },
{ NULL, 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL, NULL },
{ NULL, 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL, NULL },
{ NULL, 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL, NULL },
{ NULL, 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL, NULL },
{ NULL, 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL, NULL },
{ NULL, 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL, NULL },
{ NULL, 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL, NULL },
{ NULL, 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL, NULL },
{ NULL, 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL, NULL },
{ NULL, 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL, NULL },
{ NULL, 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL, NULL },
{ NULL, 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL, NULL },
{ NULL, 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL, NULL },
{ NULL, 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL, NULL },
{ NULL, 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL, NULL },
{ NULL, 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL, NULL },
{ NULL, 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL, NULL },
{ NULL, 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL, NULL },
{ NULL, 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL, NULL },
{ NULL, 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL, NULL },
{ NULL, 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL, NULL },
{ NULL, 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL, NULL } };

struct _hash_descriptor hash_descriptor[32] = {
{ NULL, 0, 0, 0, NULL, NULL, NULL, NULL },
{ NULL, 0, 0, 0, NULL, NULL, NULL, NULL },
{ NULL, 0, 0, 0, NULL, NULL, NULL, NULL },
{ NULL, 0, 0, 0, NULL, NULL, NULL, NULL },
{ NULL, 0, 0, 0, NULL, NULL, NULL, NULL },
{ NULL, 0, 0, 0, NULL, NULL, NULL, NULL },
{ NULL, 0, 0, 0, NULL, NULL, NULL, NULL },
{ NULL, 0, 0, 0, NULL, NULL, NULL, NULL },
{ NULL, 0, 0, 0, NULL, NULL, NULL, NULL },
{ NULL, 0, 0, 0, NULL, NULL, NULL, NULL },
{ NULL, 0, 0, 0, NULL, NULL, NULL, NULL },
{ NULL, 0, 0, 0, NULL, NULL, NULL, NULL },
{ NULL, 0, 0, 0, NULL, NULL, NULL, NULL },
{ NULL, 0, 0, 0, NULL, NULL, NULL, NULL },
{ NULL, 0, 0, 0, NULL, NULL, NULL, NULL },
{ NULL, 0, 0, 0, NULL, NULL, NULL, NULL },
{ NULL, 0, 0, 0, NULL, NULL, NULL, NULL },
{ NULL, 0, 0, 0, NULL, NULL, NULL, NULL },
{ NULL, 0, 0, 0, NULL, NULL, NULL, NULL },
{ NULL, 0, 0, 0, NULL, NULL, NULL, NULL },
{ NULL, 0, 0, 0, NULL, NULL, NULL, NULL },
{ NULL, 0, 0, 0, NULL, NULL, NULL, NULL },
{ NULL, 0, 0, 0, NULL, NULL, NULL, NULL },
{ NULL, 0, 0, 0, NULL, NULL, NULL, NULL },
{ NULL, 0, 0, 0, NULL, NULL, NULL, NULL },
{ NULL, 0, 0, 0, NULL, NULL, NULL, NULL },
{ NULL, 0, 0, 0, NULL, NULL, NULL, NULL },
{ NULL, 0, 0, 0, NULL, NULL, NULL, NULL },
{ NULL, 0, 0, 0, NULL, NULL, NULL, NULL },
{ NULL, 0, 0, 0, NULL, NULL, NULL, NULL },
{ NULL, 0, 0, 0, NULL, NULL, NULL, NULL },
{ NULL, 0, 0, 0, NULL, NULL, NULL, NULL } };

struct _prng_descriptor prng_descriptor[32] = {
{ NULL, NULL, NULL, NULL, NULL },
{ NULL, NULL, NULL, NULL, NULL },
{ NULL, NULL, NULL, NULL, NULL },
{ NULL, NULL, NULL, NULL, NULL },
{ NULL, NULL, NULL, NULL, NULL },
{ NULL, NULL, NULL, NULL, NULL },
{ NULL, NULL, NULL, NULL, NULL },
{ NULL, NULL, NULL, NULL, NULL },
{ NULL, NULL, NULL, NULL, NULL },
{ NULL, NULL, NULL, NULL, NULL },
{ NULL, NULL, NULL, NULL, NULL },
{ NULL, NULL, NULL, NULL, NULL },
{ NULL, NULL, NULL, NULL, NULL },
{ NULL, NULL, NULL, NULL, NULL },
{ NULL, NULL, NULL, NULL, NULL },
{ NULL, NULL, NULL, NULL, NULL },
{ NULL, NULL, NULL, NULL, NULL },
{ NULL, NULL, NULL, NULL, NULL },
{ NULL, NULL, NULL, NULL, NULL },
{ NULL, NULL, NULL, NULL, NULL },
{ NULL, NULL, NULL, NULL, NULL },
{ NULL, NULL, NULL, NULL, NULL },
{ NULL, NULL, NULL, NULL, NULL },
{ NULL, NULL, NULL, NULL, NULL },
{ NULL, NULL, NULL, NULL, NULL },
{ NULL, NULL, NULL, NULL, NULL },
{ NULL, NULL, NULL, NULL, NULL },
{ NULL, NULL, NULL, NULL, NULL },
{ NULL, NULL, NULL, NULL, NULL },
{ NULL, NULL, NULL, NULL, NULL },
{ NULL, NULL, NULL, NULL, NULL },
{ NULL, NULL, NULL, NULL, NULL } };

int find_cipher(const char *name)
{
   int x;
   for (x = 0; x < 32; x++) {
       if (cipher_descriptor[x].name != NULL && !strcmp(cipher_descriptor[x].name, name)) {
          return x;
       }
   }
   return -1;
}

int find_hash(const char *name)
{
   int x;
   for (x = 0; x < 32; x++) {
       if (hash_descriptor[x].name != NULL && !strcmp(hash_descriptor[x].name, name)) {
          return x;
       }
   }
   return -1;
}

int find_cipher_id(unsigned char ID)
{
   int x;
   for (x = 0; x < 32; x++) {
       if (cipher_descriptor[x].ID == ID) {
          return (cipher_descriptor[x].name == NULL) ? -1 : x;
       }
   }
   return -1;
}

int find_hash_id(unsigned char ID)
{
   int x;
   for (x = 0; x < 32; x++) {
       if (hash_descriptor[x].ID == ID) {
          return (hash_descriptor[x].name == NULL) ? -1 : x;
       }
   }
   return -1;
}

int find_prng(const char *name)
{
   int x;
   for (x = 0; x < 32; x++) {
       if ((prng_descriptor[x].name != NULL) && !strcmp(prng_descriptor[x].name, name)) {
          return x;
       }
   }
   return -1;
}

int register_cipher(const struct _cipher_descriptor *cipher)
{
   int x;

   /* is it already registered? */
   for (x = 0; x < 32; x++) {
       if (!memcmp(&cipher_descriptor[x], cipher, sizeof(struct _cipher_descriptor))) {
          return x;
       }
   }

   /* find a blank spot */
   for (x = 0; x < 32; x++) {
       if (cipher_descriptor[x].name == NULL) {
          memcpy(&cipher_descriptor[x], cipher, sizeof(struct _cipher_descriptor));
          return x;
       }
   }

   /* no spot */
   crypt_error = "No spot in cipher descriptor table.";
   return -1;
}

int unregister_cipher(const struct _cipher_descriptor *cipher)
{
   int x;

   /* is it already registered? */
   for (x = 0; x < 32; x++) {
       if (!memcmp(&cipher_descriptor[x], cipher, sizeof(struct _cipher_descriptor))) {
          cipher_descriptor[x].name = NULL;
          return CRYPT_OK;
       }
   }
   crypt_error = "Cipher not previously registered.";
   return CRYPT_ERROR;
}

int register_hash(const struct _hash_descriptor *hash)
{
   int x;

   /* is it already registered? */
   for (x = 0; x < 32; x++) {
       if (!memcmp(&hash_descriptor[x], hash, sizeof(struct _hash_descriptor))) {
          return x;
       }
   }

   /* find a blank spot */
   for (x = 0; x < 32; x++) {
       if (hash_descriptor[x].name == NULL) {
          memcpy(&hash_descriptor[x], hash, sizeof(struct _hash_descriptor));
          return x;
       }
   }

   /* no spot */
   crypt_error = "No spot in hash descriptor table.";
   return -1;
}

int unregister_hash(const struct _hash_descriptor *hash)
{
   int x;

   /* is it already registered? */
   for (x = 0; x < 32; x++) {
       if (!memcmp(&hash_descriptor[x], hash, sizeof(struct _hash_descriptor))) {
          hash_descriptor[x].name = NULL;
          return CRYPT_OK;
       }
   }
   crypt_error = "Hash not previously registered.";
   return CRYPT_ERROR;
}

int register_prng(const struct _prng_descriptor *prng)
{
   int x;
   
   /* is it already registered? */
   for (x = 0; x < 32; x++) {
       if (!memcmp(&prng_descriptor[x], prng, sizeof(struct _prng_descriptor))) {
          return x;
       }
   }

   /* find a blank spot */
   for (x = 0; x < 32; x++) {
       if (prng_descriptor[x].name == NULL) {
          memcpy(&prng_descriptor[x], prng, sizeof(struct _prng_descriptor));
          return x;
       }
   }

   /* no spot */
   crypt_error = "No spot in prng descriptor table.";
   return -1;
}

int unregister_prng(const struct _prng_descriptor *prng)
{
   int x;

   /* is it already registered? */
   for (x = 0; x < 32; x++) {
       if (!memcmp(&prng_descriptor[x], prng, sizeof(struct _prng_descriptor))) {
          prng_descriptor[x].name = NULL;
          return CRYPT_OK;
       }
   }
   crypt_error = "prng not previously registered.";
   return CRYPT_ERROR;
}

int cipher_is_valid(int idx)
{
   if (idx < 0 || idx > 32 || cipher_descriptor[idx].name == NULL) {
      crypt_error = "Invalid cipher index number used in function call.";
      return CRYPT_ERROR;
   }
   return CRYPT_OK;
}

int hash_is_valid(int idx)
{
   if (idx < 0 || idx > 32 || hash_descriptor[idx].name == NULL) {
      crypt_error = "Invalid hash index number used in function call.";
      return CRYPT_ERROR;
   }
   return CRYPT_OK;
}

int prng_is_valid(int idx)
{
   if (idx < 0 || idx > 32 || prng_descriptor[idx].name == NULL) {
      crypt_error = "Invalid prng index number used in function call.";
      return CRYPT_ERROR;
   }
   return CRYPT_OK;
}


static const char *ID_TAG = "crypt.c";

