#include "../system.h"
#include "../sccs.h"

/*
 * Copyright (c) 2001 Andrew Chang       All rights reserved.
 */

private char **
_get_http_proxy_env(char **proxies)
{
	char	*p, *q, *r, *h, *cr, buf[MAXLINE];

	p = getenv("http_proxy");  /* http://[use:password@]proxy.host:8080 */
	if (p && *p && strneq("http://", p, 7) && (q = strrchr(&p[7], ':'))) {
		*q++ = 0;
		p = &p[7];
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
	}
	return (proxies);
}

#ifdef WIN32
int
getReg(HKEY hive, char *key, char *valname, char *valbuf, int *lenp)
{
        int	rc;
        HKEY    hKey;
        DWORD   valType = REG_SZ;
	DWORD	len = *lenp;

	valbuf[0] = 0;
        rc = RegOpenKeyEx(hive, key, 0, KEY_QUERY_VALUE, &hKey);
        if (rc != ERROR_SUCCESS) return (0);

        rc = RegQueryValueEx(hKey,valname, NULL, &valType, valbuf, &len);
	*lenp = len;
        if (rc != ERROR_SUCCESS) return (0);
        RegCloseKey(hKey);
        return (1);
}

int
getRegDWord(HKEY hive, char *key, char *valname, DWORD *val)
{
        int	rc;
        HKEY    hKey;
        DWORD   valType = REG_DWORD;
	DWORD	buflen = sizeof (DWORD);

        rc = RegOpenKeyEx(hive, key, 0, KEY_QUERY_VALUE, &hKey);
        if (rc != ERROR_SUCCESS) return (0);

        rc = RegQueryValueEx(hKey,valname, NULL,
				&valType, (LPBYTE)val, &buflen);
        if (rc != ERROR_SUCCESS) return (0);
        RegCloseKey(hKey);
        return (1);
}

private char **
addProxy(char *type, char *line, char **proxies)
{
	char	*q, buf[MAXLINE], proxy_host[MAXPATH];
	int	proxy_port;

	unless (line) return proxies;
	q = strchr(line, ':');
	unless (q) {
		fprintf(stderr,
"===========================================================================\n"
"Unknown proxy entry: \"%s\"\n"
"If you believe this entry is legal, please file a bug report and\n"
"ask Bitmover to add support for it.\n"
"===========================================================================\n",
		 line);
		return (proxies);
	}
	*q = 0;
	strcpy(proxy_host, line);
	proxy_port = atoi(++q);
	sprintf(buf, "%s %s:%d", type, proxy_host, proxy_port);
	if (getenv("BK_HTTP_PROXY_DEBUG")) {
		fprintf(stderr, "Adding proxy: \"%s\"\n", buf);
	}
	return (addLine(proxies, strdup(buf)));
}

/*
 * Get proxy from windows registry
 * Note: This works for both IE and netscape on win32
 */
private char **
_get_http_proxy_reg(char **proxies, char *host)
{
#define KEY "Software\\Microsoft\\Windows\\CurrentVersion\\internet Settings"
	char	*p, *q;
	int	len;
	int	proxyEnable = 0;
	char	buf[MAXLINE];

	if (getRegDWord(HKEY_CURRENT_USER,
			KEY, "ProxyEnable", (DWORD *)&proxyEnable) == 0) {
		goto done;
	}
	unless (proxyEnable) goto done;

	len = sizeof(buf);  /* important */
	getReg(HKEY_CURRENT_USER, KEY, "AutoConfigURL", buf, &len);
	if (buf[0]) {
		/*
		 * We need a JavaScript interpretor to parse the auto-config
		 * pac file properly.  We killed pac file support on Unix
		 * long time ago. Thers is no reason to do differently on
		 * Windows. So just skip the pac and make the user set up
		 * their proxy with envronment variable.
		 */
		goto done;
	}

	len = sizeof(buf);  /* important */
	if (getReg(HKEY_CURRENT_USER, KEY, "ProxyOverride", buf, &len)) {
		q = buf;
		while (q) {
			if (p = strchr(q, ';')) *p++ = 0;
			if (*q && match_one(host, q, 1)) goto done;
			q = p;
		}
	}
	
	len = sizeof(buf);  /* important */
	if (getReg(HKEY_CURRENT_USER, KEY, "ProxyServer", buf, &len) == 0) {
		goto done;
	}
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
