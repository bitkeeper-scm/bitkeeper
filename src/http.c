/*
 * Copyright (c) 2000, Andrew Chang
 */    
#include "system.h"
#include "sccs.h"
#include "bkd.h"

#define	SEND_SUCCESS	0
#define	SEND_FAILURE	1
#define	SEND_BOGUS	2
#define	SEND_NOCONNECT	3

#define	BINARY	"application/octet-stream"

extern void save_cached_proxy(char *proxy);

#define	SOCKS_REQUEST_GRANTED		90
#define	SOCKS_REQUEST_REJECT		91
#define	SOCKS_REQUEST_IDENT_FAILED	92
#define	SOCKS_REQUEST_IDENT_CONFLICT	93

/* socks4 client request */
struct	socks_op {
	unsigned char vn;
	unsigned char cd;
	unsigned char dstport[2];
	unsigned char dstip[4];
	unsigned char userid[10];
};

/* socks4 server reply */
struct	socks_reply {
	unsigned char vn;
	unsigned char cd;
	unsigned char dstport[2];
	unsigned char dstip[4];
};



/*
 * Convert "s" to base64m
 * Caller should free memory when done
 */
private char *
str2base64(const char *s)
{
	int	i, len, len2;
	char	*buf;
	unsigned char *p;
	static char tbl[64] = {
		'A','B','C','D','E','F','G','H',
		'I','J','K','L','M','N','O','P',
		'Q','R','S','T','U','V','W','X',
		'Y','Z','a','b','c','d','e','f',
		'g','h','i','j','k','l','m','n',
		'o','p','q','r','s','t','u','v',
		'w','x','y','z','0','1','2','3',
		'4','5','6','7','8','9','+','/'
	};

	len = strlen(s);
	len2 = (4 * ((len + 2) / 3));
	buf = p = malloc(1 + len2);

	for (i = 0; i < len; i += 3)
	{
		*p++ = tbl[s[0] >> 2];
		*p++ = tbl[((s[0] & 3) << 4) + (s[1] >> 4)];
		*p++ = tbl[((s[1] & 0xf) << 2) + (s[2] >> 6)];
		*p++ = tbl[s[2] & 0x3f];
		s += 3;
	}

	/* Pad it if necessary...  */
	if (i == len + 1) {
		*(p - 1) = '=';
	} else if (i == len + 2) {
    		*(p - 1) = *(p - 2) = '=';
	}
  	*p = '\0';
	return (buf);
}

/*
 * Make a Proxy-Authorzation header
 * Caller should free memory when done
 */
private char *
proxyAuthHdr(const char *cred)
{
	char	*t1, *t2;

	t1 = str2base64(cred);
	t2 = aprintf("Proxy-Authorization: BASIC %s\n", t1);
	free(t1);
	return (t2);
}

/*
 * Parse url to host and path
 */
void
parse_url(char *url, char *host, char *path)
{
	char	buf[MAXPATH], *p;

	strcpy(buf, url);
	p = strchr(&buf[7], '/');
	if (p) *p = 0;
	strcpy(host, &buf[7]);
	if (p) {
		*p = '/';
		strcpy(path, p);
	} else {
		path[0] = 0;
	}
}

unsigned long
host2ip(char *host, int trace)
{
	struct	hostent *hostp;
	struct	sockaddr_in inaddr;

	if ((host == NULL) || (*host == '\0')) {
		if (trace) fprintf(stderr, "host2ip: bad args\n");
		return (INADDR_ANY);
	}

	memset ((char *) &inaddr, 0, sizeof(inaddr));
	if ((inaddr.sin_addr.s_addr = inet_addr(host)) == -1) {
		/*
		 * XXX TODO: This should time out, in case we do not have
		 * a network connection, or the DNS server is down
		 */
		if (trace) {
			fprintf(stderr, "looking up IP address for %s\n", host);
		}
		if ((hostp = gethostbyname(host)) == NULL) {
			if (trace) {
				fprintf(stderr, "%s IP: lookup failed\n", host);
			}
			errno = EINVAL;
			return (-1);
		}
		if (hostp->h_addrtype != AF_INET) {
			if (trace) {
				fprintf(stderr, "%s not a IP address\n", host);
			}
			errno = EINVAL;
			return (-1);
		}
		memcpy((char *) &inaddr.sin_addr, (char *) hostp->h_addr,
			sizeof(inaddr.sin_addr));
		if (trace) {
			fprintf(stderr, "%s => %s\n",
					    host, inet_ntoa(inaddr.sin_addr));
		}
	}
	return (inaddr.sin_addr.s_addr);
}


int
connect_srv(char *srv, int port, int trace)
{
	struct	sockaddr_in inaddr;
	fd_set	wfds;
	int	fd;
	struct	timeval tv[1];

	if ((srv[0] == 0) || (port == 0)) {
		if (trace) fprintf(stderr, "connect_srv: bad args: %s %d\n",
								    srv, port);
		return (-1);
	}
	memset((char *) &inaddr, 0, sizeof(inaddr));

	inaddr.sin_addr.s_addr	= host2ip(srv, trace);
	inaddr.sin_family	= AF_INET;

	inaddr.sin_port = htons(port);

	if ((fd = socket(inaddr.sin_family, SOCK_STREAM, 0)) < 0) {
		if (trace) {
			fprintf(stderr,
			    "Cannot create tcp socket for %s:%d\n", srv, port);
			perror("socket");
			return (-1);
		}
	}

	if (connect(fd, (struct sockaddr *) & inaddr, sizeof(inaddr)) == -1) {
		(void) close(fd);
		if (trace) {
			fprintf(stderr, "Cannot connect %s:%d\n", srv, port);
		}
		return (-1);
	}

	/* wait for connection */
	FD_ZERO(&wfds);
	FD_SET(fd, &wfds);

	tv->tv_sec  = 30;
	tv->tv_usec = 0;

	if (select(fd + 1, NULL, &wfds, NULL, tv) <= 0) {
		if (trace) fprintf(stderr, "connect_srv: select failed\n");
		(void) close(fd);
		return (-1);
	}
	return (fd);
}

int
connect_socks4_srv(char *host, int port, char *url, int trace)
{
	unsigned char sbuf[BUFSIZ];
	char	web_host[MAXPATH], web_path[MAXPATH];
	struct	socks_op    c;
	struct	socks_reply *s = (struct socks_reply *)sbuf;
	int	web_ip, web_port = 80, n, sfd;


	/* build a socks4 request */
	parse_url(url, web_host, web_path);
	web_ip = htonl(ns_sock_host2ip(web_host, trace));

	c.vn = 4;
	c.cd = 1;
	c.dstport[0] = (web_port >> 8) & 0xff;
	c.dstport[1] = (web_port) & 0xff;
	c.dstip[0] = (web_ip >> 24) & 0xff;
	c.dstip[1] = (web_ip >> 16) & 0xff;
	c.dstip[2] = (web_ip >>  8) & 0xff;
	c.dstip[3] = (web_ip) & 0xff;
	strcpy(c.userid, "anonymous");

	sfd = connect_srv(host, port, trace);
	if (sfd < 0) return (-1);
	if ((n = send(sfd, (void *) &c, sizeof(c), 0)) != sizeof(c)) {
		if (trace) {
			fprintf(stderr, "SOCKS4 negotiation write failed...\n");
		}
		closesocket(sfd);
		return (-1);
	}

	if ((n = recv(sfd, sbuf, sizeof(sbuf), 0)) != sizeof(*s)) {
		if (trace) {
			fprintf(stderr,
			    "SOCKS4 negotiation read failed, n=%d,sbuf=%s\n",
			    n, sbuf);
		}
		closesocket(sfd);
		return (-1);
	}

	switch (s->cd) {
	case SOCKS_REQUEST_GRANTED:
		return (sfd);
		break;
	case SOCKS_REQUEST_REJECT:
		if (trace) fprintf(stderr, "SOCKS4 request rejected\n");
		errno = EINVAL;
		break;
	case SOCKS_REQUEST_IDENT_FAILED:
		if (trace) fprintf(stderr, "SOCKS4 ident failed\n");
		errno = EACCES;
		break;
	case SOCKS_REQUEST_IDENT_CONFLICT:
		if (trace) fprintf(stderr, "SOCKS4 ident conflict\n");
		errno = EACCES;
		break;
	default:
		errno = ENOENT;
		if (trace) {
			fprintf(stderr,
				"SOCKS request rejected for reason code %d\n",
				s->cd);
		}
	}

	closesocket(sfd);
	return (-1);
}

void
catch_alarm(int sig)
{
	/* no op */
}


private int
http_connect_srv(char *type, char *host, int port, char *cgi_script, int trace)
{
	int	fd;
	char	bk_url[MAXPATH];


	alarm(10);
	signal(SIGALRM, catch_alarm);
	sprintf(bk_url, "http://%s/cgi-bin/%s", host, cgi_script);
	if (streq(type, "DIRECT")) {
		fd = connect_srv(host, port, trace);
	} else if (streq(type, "PROXY")) {
		fd = connect_srv(host, port, trace);
	} else if (streq(type, "SOCKS")) {
		fd = connect_socks4_srv(host, port, bk_url, trace);
	} else {
		fprintf(stderr, "unknown proxy type %s\n", type);
		fd = -1;
	}
	alarm(0);
	return (fd);
}

int
http_connect(remote *r, char *cgi_script)
{
	int	i, port;
	char	proxy_host[MAXPATH], type[50], *p, **proxies;
	char	cred[MAXLINE];

	/*
	 * Try proxy connection if available
	 */
	proxies = get_http_proxy();
	EACH(proxies) {
		if (r->trace) {
			fprintf(stderr, "trying %s\n", proxies[i]);
			fflush(stderr);
		}
		p = strchr(proxies[i], ':');
		assert(p);
		*p = ' ';
		cred[0] = 0;
		sscanf(proxies[i], "%s %s %d %s\n",
			type, proxy_host, &port, cred);
		*p = ':';
		r->rfd = http_connect_srv(type, proxy_host,
						port, cgi_script, r->trace);
		r->wfd = r->rfd;
		if (r->rfd >= 0) {
			if (r->trace) fprintf(stderr, "connected\n");
			if (r->cred) {
				free(r->cred);
				r->cred = 0;
			}
			/* Save the credential, needed in http_send() */
			if (cred[0]) r->cred = strdup(cred);
			freeLines(proxies);
			return (0);
		}
	}
	freeLines(proxies);

	/*
	 * Try direct connection
	 */
	if (r->trace) {
		fprintf(stderr, "Trying direct connection\n");
		fflush(stderr);
	}
	r->wfd = r->rfd = http_connect_srv("DIRECT", r->host,
						r->port, cgi_script, r->trace);
	if (r->rfd >= 0) {
		if (r->trace) fprintf(stderr, "Connected\n");
		return (0);
	}

	if (r->trace) fprintf(stderr, "connection failed\n");
	r->rfd = r->wfd = -1;
	return (-1);
}


int
http_send(remote *r, char *msg,
	size_t mlen, size_t extra, char *user_agent, char *cgi_script)
{
	unsigned int start, len, l;
	int	n = 0;
	char	bk_url[MAXPATH], *header = 0;
	char	*spin = "|/-\\";
	char	*proxy_auth = "";


	/*
	 * make HTTP header
	 */
	assert(r);
	assert(r->host);
	sprintf(bk_url, "http://%s:%d/cgi-bin/%s", r->host, r->port, cgi_script);
	if (r->cred) {
		if (r->trace){
			fprintf(stderr, "Proxy Authorization => (%s)\n",
			    r->cred);
		}
		proxy_auth = proxyAuthHdr(r->cred);
	}
	header = aprintf(
	    "POST %s HTTP/1.0\n"
	    "%s"			/* optional proxy authentication */
	    "User-Agent: %s\n"
	    "Accept: text/html\n"
	    "Host: %s:%d\n"
	    "Content-type: %s\n"
	    "Content-length: %u\n\n",
	    bk_url, proxy_auth, user_agent, 
	    r->host, r->port,
	    BINARY, mlen + extra);

	if (*proxy_auth) free(proxy_auth);
	if (r->trace) fprintf(stderr, "Sending http header:\n%s", header);

	len = strlen(header);
	if (write_blk(r, header, len) != len) {
		if (r->trace) fprintf(stderr, "Send failed\n");
err:		if (header) free(header);
		return (-1);
	}
	if (r->trace) fprintf(stderr, "Sending data file ");

	for (start = 0; start < mlen; ) {
		len = mlen - start;

		if (len > 4096) len = 4096;

		l = write_blk(r, msg + start, len);
		if (l <= 0) {
			perror("http_send");
			break;
		}
		start += l;
		if (r->trace) fprintf(stderr, "%c\b", spin[n++ % 4]);
	}
	if (r->trace) fputs(" \n", stderr);
	if (start < mlen)  {
		if (r->trace) {
			fprintf(stderr,
				"Send failed, wanted %d, got %d\n",
				mlen, start);
		}
		goto err;
	}
	free(header);
	return 0;
}
