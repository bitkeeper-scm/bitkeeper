/*
 * Copyright 2000-2008,2013,2016 BitMover, Inc
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
#include "../bkd.h"

private char **
_get_http_proxy_env(char **proxies)
{
	char	*p, *q, *r, *h, *cr, buf[MAXLINE];

	/* http://[use:password@]proxy.host:8080 */
	p = getenv("http_proxy"); 
	if (p && *p) {
		// the http:// is optional
		r = p;
		if (strneq("http://", p, 7)) p += 7;
		unless (q = strchr(p, ':')) {
			fprintf(stderr,
			    "bk: ignoring malformed proxy '%s'\n", r);
			goto skip;
		}
		
		*q++ = 0;
		r = strrchr(p, '@'); /* look for optional user:passwd@ */
		if (r) {
			*r++ = 0;
			cr = p;
			h = r;
		} else {
			cr = NULL;
			h = p;
		}
		/*
		 * When we get here:
		 * 	 h points to host
		 * 	 q points to port
		 * 	 cr points to user:passwd
	 	 */
		sprintf(buf, "PROXY %s:%-d", h, atoi(q));
		if (cr) {
			strcat(buf, " ");
			strcat(buf, cr);
		}
		/* buf should look like: PROXY host:port [cred] */
		q[-1] = ':'; 
		if (r) r[-1] = '@';
		proxies = addLine(proxies, strdup(buf));
	}

skip:

	p = getenv("HTTP_PROXY_HOST"); 
	q = getenv("HTTP_PROXY_PORT"); 
	if (p && *p) {
		sprintf(buf, "PROXY %s:%s", p, q ? q : "8000");
		proxies = addLine(proxies, strdup(buf));
	}

	return (proxies);
}

private char **
_get_socks_proxy(char **proxies)
{
	char	*p, *q, buf[MAXLINE];

	q = getenv("SOCKS_PORT");
	p = getenv("SOCKS_HOST"); 
	if (p && *p) {
		sprintf(buf, "SOCKS %s:%s", p, q ? q : "1080");
		proxies = addLine(proxies, strdup(buf));
	}

	p = getenv("SOCKS_SERVER"); /* <host>:<port> */
	if (p && *p && (q = strchr(p, ':'))) {
		*q++ = 0;
		sprintf(buf, "SOCKS %s:%s", p, q);
		q[-1] = ':';
		proxies = addLine(proxies, strdup(buf));
	} else if (p && *p) {
		fprintf(stderr, "bk: ignoring malformed socks proxy %s\n", p);
	}
	return (proxies);
}

#ifdef WIN32
private char	**_get_http_autoproxy(char **proxies, char *host);
private char	**_get_http_autoproxyurl(char **, char *host, char *url);

private char **
addProxy(char *type, char *line, char **proxies)
{
	char	*q, buf[MAXLINE], proxy_host[MAXPATH];
	int	proxy_port;

	unless (line) return (proxies);
	unless (q = strchr(line, ':')) {
		fprintf(stderr, "bk: ignoring malformed proxy %s\n", line);
		return (proxies);
	}
	*q = 0;
	strcpy(proxy_host, line);
	proxy_port = atoi(++q);
	sprintf(buf, "%s %s:%d", type, proxy_host, proxy_port);
	return (addLine(proxies, strdup(buf)));
}

/*
 * Get proxy from windows registry
 * Note: This works for both IE and netscape on win32
 */
private char **
_get_http_proxy_reg(char **proxies, char *host)
{
#define KEY "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Internet Settings"
	char	*p, *q;
	DWORD	*proxyEnable;
	char	*buf;

	/*
	 * These proxy settings from the registry don't appear to be
	 * offically supported by Microsoft, but show up all over the
	 * place on the web about how to discover this information.
	 * It appears that officially for WinINet you should use
	 * InternetQueryOption().
	 */
	if (buf = reg_get(KEY "\\Connections",
		    "DefaultConnectionSettings", 0)) {
		/* Automatically detect settings */
		if (buf[8] & 0x8) {
			proxies = _get_http_autoproxy(proxies, host);
		}
	}
	if (buf = reg_get(KEY, "AutoConfigURL", 0)) {
		/* Use automatic configuration script */
		if (buf[0]) {
			proxies = _get_http_autoproxyurl(proxies, host, buf);
		}
	}
	proxyEnable = reg_get(KEY, "ProxyEnable", 0);
	unless (proxyEnable && *proxyEnable) goto done;

	if (buf = reg_get(KEY, "ProxyOverride", 0)) {
		q = buf;
		while (q) {
			if (p = strchr(q, ';')) *p++ = 0;
			if (*q && match_one(host, q, 1)) goto done;
			q = p;
		}
	}

	unless (buf = reg_get(KEY, "ProxyServer", 0)) goto done;

	/*
	 * We support 3 froms:
	 * 1) host:port
	 * 2) http=host:port
	 * 3) http://host:port
	 * form 1 can only exist in a single field line
	 * form 2 and 3 can be part of a multi-field line
	 */
	if (getenv("BK_HTTP_PROXY_DEBUG")) {
		fprintf(stderr, "ProxyServer= \"%s\"\n", buf);
	}
	q = buf;
	while (q) {
		if (p = strchr(q, ';')) *p++ = 0;
		if (strneq("http://", q, 7)) {
			proxies = addProxy("PROXY", &q[7], proxies);
		} else if (strneq("http=", q, 5)) {
			proxies = addProxy("PROXY", &q[5], proxies);
		} else if (strneq("socks=", q, 6)) {
			proxies = addProxy("SOCKS", &q[6], proxies);
		} else if (!p) {
			proxies = addProxy("PROXY", q, proxies);
		}
		q = p;
	}
done:	return (proxies);
}
#endif

/*
 * Win32 note:	We support socks proxy on win32, but we do not support
 *		socks DNS (yet).
 */ 
char **
get_http_proxy(char *host)
{
	char	**proxies = 0;
	
	proxies = _get_http_proxy_env(proxies);
	proxies = _get_socks_proxy(proxies);
#ifdef WIN32
	proxies = _get_http_proxy_reg(proxies, host);
#endif
	return (proxies);
}

#ifdef WIN32
#include <wininet.h>

/* jscript helper function */
DWORD __stdcall 
ResolveHostName(char *host, char *ipaddr, u32 *ipaddrlen)
{
	int	trace = getenv("BK_HTTP_PROXY_DEBUG") != 0;
	struct in_addr sin_addr;
	char	buf[16];

	sin_addr.s_addr = host2ip(host, trace);
	if (sin_addr.s_addr == -1) return (ERROR_INTERNET_NAME_NOT_RESOLVED);
	strcpy(buf, inet_ntoa(sin_addr));
	assert(ipaddr);
	assert(strlen(buf) < *ipaddrlen);
	strcpy(ipaddr, buf);
	return (0);
}

/* jscript helper function */
BOOL __stdcall
IsResolvable(char *host)
{
	char	buf[256];
	u32	size = sizeof(buf) - 1;

	return (ResolveHostName(host, buf, &size) ? FALSE : TRUE);
}

/* jscript helper function */
DWORD __stdcall 
GetIPAddress(char *ipaddr, u32 *ipaddrsize )
{
	char	host[256];

	if (gethostname(host, sizeof(host) - 1) != ERROR_SUCCESS) {
		return (ERROR_INTERNET_INTERNAL_ERROR);
	}
	return (ResolveHostName(host, ipaddr, ipaddrsize));
}


/* jscript helper function */
BOOL __stdcall 
IsInNet(char *ipaddr, char *dest, char *mask)
{
	u32	ipaddrn, destn, maskn;

	ipaddrn = inet_addr(ipaddr);
	destn = inet_addr(dest);
	maskn = inet_addr(dest);

	if ((destn == INADDR_NONE) || (ipaddrn == INADDR_NONE) ||
	    ((ipaddrn & maskn) != destn)) {
		return (FALSE);
	}
	return (TRUE);
}

#define  PROXY_AUTO_DETECT_TYPE_DHCP    1
#define  PROXY_AUTO_DETECT_TYPE_DNS_A   2

/*
 * This finds the URL of a wpad.dat javascript file on the local
 * network that provides the configuration details for proxies.
 *
 * This protocol is described here:
 *    http://en.wikipedia.org/wiki/Web_Proxy_Autodiscovery_Protocol
 *
 * And read Microsoft's documentation for the DetectAutoProxyUrl()
 * for details of their routines.
 * Note: This stuff is perfectly valid on UNIX machines, but we are
 * using Microsoft's libraries so this only works on Windows.  We don't
 * bother with a portable version as the next step requires a
 * javascript interperter and we don't ship one of those.
 */
private char **
_get_http_autoproxy(char **proxies, char *host)
{
	int	flags = getenv("BK_HTTP_PROXY_DEBUG") ? 0 : SILENT;
	static HMODULE	hModWI = 0;
	char	WPAD_url[1024]= "";
	static	BOOL (CALLBACK *DetectAutoProxyUrl)
		(char *proxyurl, u32 proxyurllen, u32 flags);

	unless (hModWI) {
		unless (hModWI = LoadLibrary("wininet.dll")) {
			verbose((stderr, "LoadLibrary(wininet.dll) failed\n"));
			goto out;
		}
		DetectAutoProxyUrl = 
			(void *)GetProcAddress(hModWI, "DetectAutoProxyUrl");
		unless (DetectAutoProxyUrl) {
			verbose((stderr,
			    "Failed to load address from wininet.dll\n"));
			goto out;
		}
	}
	unless (DetectAutoProxyUrl(WPAD_url, sizeof(WPAD_url), 
	    PROXY_AUTO_DETECT_TYPE_DHCP | PROXY_AUTO_DETECT_TYPE_DNS_A)) {
		verbose((stderr, "No WPAD found\n"));
		goto out;
	}
	verbose((stderr, "WPAD==%s\n", WPAD_url));

	proxies = _get_http_autoproxyurl(proxies, host, WPAD_url);
 out:
	return (proxies);
}

/*
 * After an autoproxy URL is found, this routine fetches the file and
 * executes the javascript call FindProxyForUrl()
 * See:
 *   http://en.wikipedia.org/wiki/Proxy_auto-config
 * for more details.
 *
 * This code is based on the example here:
 *    http://msdn.microsoft.com/en-us/library/aa383910(VS.85).aspx
 *
 * Note this code uses the WinInet API, The WinHTTP API is newer and
 * should support more proxy configurations.  However as far as I
 * could tell WinHTTP was one big wrapper that provided a "Open
 * connection to this URL" API without even returning a generic file
 * handle.   So to use in bk we would probably need to change our code
 * that talks with bkd's to exclusively use stdio and then build a
 * stdio-based wrapper around WinHTTP.
 */
private char **
_get_http_autoproxyurl(char **proxies, char *host, char *WPAD_url)
{
	int	flags = getenv("BK_HTTP_PROXY_DEBUG") ? 0 : SILENT;
	static HMODULE	hModJS = 0;
	char	*url = 0;
	char	*tmpf = 0;
	remote	*r;
	char	proxyBuffer[1024];
	char	*proxy = proxyBuffer;
	u32	dwProxyHostNameLength = sizeof(proxyBuffer);

	struct AutoProxyHelperVtbl {
		BOOL (__stdcall *pIsResolvable)(char *host);
		DWORD (__stdcall *pGetIPAddress)
		     (char *ipaddr, u32 *ipaddrsize);
		DWORD (__stdcall *pResolveHostName)
		     (char *host, char *ipaddr, u32 *);
		BOOL (__stdcall *pIsInNet)
		     (char *ipaddr, char *dest, char *mask);
	} Vtbl = {
		IsResolvable,
		GetIPAddress,
		ResolveHostName,
		IsInNet
	};
	struct AutoProxyHelperFunctions {
		const struct AutoProxyHelperVtbl *Vtbl;
	} HelperFunctions = { &Vtbl };
	// Declare function pointers for the three autoproxy functions
	static BOOL (CALLBACK *InternetInitializeAutoProxyDll)(
		DWORD Version, char *tmpf, char *mime,
		struct AutoProxyHelperFunctions *callbacks,
		void *AutoProxyScriptBuffer);
	static BOOL (CALLBACK *InternetDeInitializeAutoProxyDll)(
		char *mime, DWORD reserved);
	static BOOL (CALLBACK *InternetGetProxyInfo)(
		char *url, u32 urllen, 
		char *host, u32 hostlen,
		char **proxy, u32 *proxylen);

	unless (hModJS) {
		unless (hModJS = LoadLibrary("jsproxy.dll")) {
			verbose((stderr, "LoadLibrary(jsproxy.dll) failed\n"));
			goto out;
		}
		InternetInitializeAutoProxyDll =
			GetProcAddress(hModJS,
			    "InternetInitializeAutoProxyDll");
		InternetDeInitializeAutoProxyDll =
			GetProcAddress(hModJS,
			    "InternetDeInitializeAutoProxyDll");
		InternetGetProxyInfo =
			GetProcAddress(hModJS, "InternetGetProxyInfo");
		unless (InternetInitializeAutoProxyDll &&
		    InternetDeInitializeAutoProxyDll &&
		    InternetGetProxyInfo) {
			verbose((stderr,
			    "Failed to load addresses from jsproxy.dll\n"));
			goto out;
		}
	}

	unless (tmpf = bktmp(0)) goto out;
	r = remote_parse(WPAD_url, 0);
	unless (r && r->host) {
		fprintf(stderr,
		    "bk: no host in url returned by WPAD_url:\n\t'%s'\n",
		    WPAD_url);
		goto out;

	}
	r->rfd = r->wfd = connect_srv(r->host, r->port, r->trace);
	if (r->rfd < 0) {
		verbose((stderr, "Unable to connect to %s\n", WPAD_url));
		goto out;
	}
	r->isSocket = 1;
	if (http_fetch(r, tmpf)) {
		verbose((stderr, "Fetch of %s to %s failed\n",
		    WPAD_url, tmpf));
		goto out;
	}
	remote_free(r);
	unless (InternetInitializeAutoProxyDll(0, tmpf, NULL, 
	    &HelperFunctions, NULL)) {
		verbose((stderr, "InternetInitializeAutoProxyDll failed\n"));
		goto out;
	}
	url = aprintf("http://%s", host);
	unless (InternetGetProxyInfo(url, strlen(url), host, strlen(host),
	    &proxy, &dwProxyHostNameLength)) {
		verbose((stderr, "InternetGetProxyInfo(%s) failed\n",
		    url));
		goto out;
	}
	unless (InternetDeInitializeAutoProxyDll(0, 0)) goto out;
	verbose((stderr, "proxy==%s\n", proxy));
	unless (streq(proxy, "DIRECT")) {
		proxies = addLine(proxies, strdup(proxy));
	}
 out:
	if (url) free(url);
	if (tmpf) {
		unlink(tmpf);
		free(tmpf);
	}
	return (proxies);
}
#endif
