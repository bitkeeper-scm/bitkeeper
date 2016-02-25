/*
 * Copyright 2001,2015-2016 BitMover, Inc
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

#include "../sccs.h"

/*
 * Copyright (c) 2001 Andrew Chang       All rights reserved.
 */

#ifndef WIN32
unsigned long
ns_sock_host2ip(char *host, int trace)
{
	char	*p;
	int	nscount_sav;
	unsigned long ip, nsaddr_sav;

	p = getenv("SOCKS_NS");
	if (p && *p) {
		gethostbyname("localhost"); /* force init the res structure */
		res_init();
		/* save the original values */
		nsaddr_sav = _res.nsaddr_list[0].sin_addr.s_addr;
		nscount_sav = _res.nscount;
		_res.nsaddr_list[0].sin_addr.s_addr = inet_addr(p);
		_res.nscount = 1;
		ip = host2ip(host, trace);
		/* restore the original values */
		_res.nsaddr_list[0].sin_addr.s_addr = nsaddr_sav;
		_res.nscount = nscount_sav;
		return ip;
	} else {
		return (host2ip(host, trace));
	}
}
#else
unsigned long
ns_sock_host2ip(char *host, int trace)
{
	/* we do not support ns_socks on win32 */
	return (host2ip(host, trace));
}
#endif
