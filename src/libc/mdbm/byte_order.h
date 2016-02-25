/*
 * Copyright 2011,2016 BitMover, Inc
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

#include <sys/param.h>

/* provide our own version of translation functions */
/* rename it to _name to avoid conflict with host header file */
uint16	_htons(uint16 s);
uint16	_ntohs(uint16 s);
uint32	_htonl(uint32 s);
uint32	_ntohl(uint32 s);

/* also provide versions of a couple BSD endian functions */
u16	_le16toh(u16 x);
u16	_htole16(u16 x);
u32	_le32toh(u32 x);
u32	_htole32(u32 x);

#if !defined(__BYTE_ORDER) && defined(_BYTE_ORDER)
#define	__BYTE_ORDER _BYTE_ORDER
#define	__LITTLE_ENDIAN _LITTLE_ENDIAN
#define	__BIG_ENDIAN _BIG_ENDIAN
#endif

#if !defined(__BYTE_ORDER) && defined(BYTE_ORDER)
#define	__BYTE_ORDER BYTE_ORDER
#define	__LITTLE_ENDIAN LITTLE_ENDIAN
#define	__BIG_ENDIAN BIG_ENDIAN
#endif

#ifndef __BYTE_ORDER
#error "Must have __BYTE_ORDER defined"
#endif

#if __BYTE_ORDER == __LITTLE_ENDIAN
#define	IS_LITTLE_ENDIAN() 1

#undef  le16toh
#define le16toh(x) (x)
#undef  htole16
#define htole16(x) (x)
#undef  le32toh
#define le32toh(x) (x)
#undef  htole32
#define htole32(x) (x)


#elif __BYTE_ORDER == __BIG_ENDIAN

#define	IS_LITTLE_ENDIAN() 0

# ifndef le16toh
#  define le16toh(x) _le16toh(x)
# endif
# ifndef htole16
#  define htole16(x) _htole16(x)
# endif
# ifndef le32toh
#  define le32toh(x) _le32toh(x)
# endif
# ifndef htole32
#  define htole32(x) _htole32(x)
# endif

#else

#error "__BYTE_ORDER invalid"

/* a test that the optimizer should be able to evaluate at compile time */
/* just here for documentation */
#define	IS_LITTLE_ENDIAN() ({ u32 var = 0x01020304; (*(u8 *)&var == 0x04); })

#endif
