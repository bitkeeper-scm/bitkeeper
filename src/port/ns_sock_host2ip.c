#include "../system.h"
#include "../sccs.h"

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
