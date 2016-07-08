/*
 * Copyright 2000-2016 BitMover, Inc
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
int	oldrsa_encrypt_key(u8 *inkey, int inlen,
    u8 *outkey, unsigned long *outlen,
    prng_state *prng, int wprng, rsa_key *key);
int	oldrsa_decrypt_key(u8 *in, unsigned long *len,
    u8 *outkey, unsigned long *keylen,
    rsa_key *key);
int	oldrsa_sign_hash(u8 *hash, int hashlen, u8 *out, unsigned long *outlen,
    int hashid, rsa_key *key);
int	oldrsa_verify_hash(u8 *sig, int *siglen, u8 *hash, int hashlen,
    int *stat, int hashid, rsa_key *key);
int	oldrsa_import(u8 *in, rsa_key *key);
