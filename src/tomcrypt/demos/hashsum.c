/*
 * Written by Daniel Richards <kyhwana@world-net.co.nz> 6/7/2002
 * hash.c: This app uses libtomcrypt to hash either stdin or a file
 * This file is Public Domain. No rights are reserved.
 * Compile with 'gcc hash.c -o hash -ltomcrypt -lm'
 * This example isn't really big enough to warrent splitting into
 * more functions ;)
*/

#include <mycrypt.h>

void register_algs();

int main(int argc, char **argv)
{
   int idx, x, y, z;
   hash_state hs;
   unsigned char in_buffer[256], hash_buffer[MAXBLOCKSIZE];

   /* You need to register algorithms before using them */
   register_algs();
   if (argc < 2) {
      printf("usage: ./hash algorithm file [file ...]\n");
      printf("Algorithms:\n");
      for (x = 0; hash_descriptor[x].name != NULL; x++) {
         printf(" %s\n", hash_descriptor[x].name);
      }
      exit(EXIT_SUCCESS);
   }

   idx = find_hash(argv[1]);
   if (idx == -1) {
      fprintf(stderr, "\nInvalid hash specified on command line.\n");
      return -1;
   }

   for (z = 2; z < argc; z++) {
      hash_file(idx,argv[z],hash_buffer);
      for (x = 0; x < (int)hash_descriptor[idx].hashsize; x++) {
          printf("%02x",hash_buffer[x]);
      }
      printf("  %s\n", argv[z]);
   }
   return EXIT_SUCCESS;
}

void register_algs(void) 
{
   register_hash(&sha512_desc);
   register_hash(&sha384_desc);
   register_hash(&sha256_desc);
   register_hash(&sha1_desc);
   register_hash(&md5_desc);
   register_hash(&md4_desc);
   register_hash(&tiger_desc);
}
