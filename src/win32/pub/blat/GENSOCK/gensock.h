/* $Id: gensock.h 1.8 1995/01/25 23:28:11 rushing Exp $ */

#ifndef _GENSOCK_H
#define _GENSOCK_H

#define ERR_CANT_MALLOC			4001
#define ERR_SENDING_DATA		4002
#define ERR_INITIALIZING		4003
#define ERR_VER_NOT_SUPPORTED		4004
#define ERR_EINVAL			4005
#define ERR_SYS_NOT_READY		4006
#define ERR_CANT_RESOLVE_HOSTNAME	4007
#define ERR_CANT_GET_SOCKET		4008
#define ERR_READING_SOCKET		4009
#define ERR_NOT_A_SOCKET		4010
#define ERR_BUSY			4011
#define ERR_CLOSING			4012
#define WAIT_A_BIT			4013
#define ERR_CANT_RESOLVE_SERVICE	4014
#define ERR_CANT_CONNECT		4015
#define ERR_NOT_CONNECTED		4016
#define ERR_CONNECTION_REFUSED		4017

#define ERR_NO_ERROR_CODE		-5000	/* this is returned by misbehaving stacks that
						 * fail, but don't set an error code
						 */

typedef void FAR * socktag;

#ifdef __cplusplus
extern "C" {
#endif
/* function prototypes */

int	FAR PASCAL gensock_connect 	(char FAR * hostname,
					 char FAR * service,
					 socktag FAR * pst);

int	FAR PASCAL gensock_getchar 	(socktag st,
					 int wait,
					 char FAR * ch);

int	FAR PASCAL gensock_put_data 	(socktag st,
					 char FAR * data,
					 unsigned long length);

int	FAR PASCAL gensock_close	(socktag st);

int	FAR PASCAL gensock_gethostname	(char FAR * name, int namelen);

int	FAR PASCAL gensock_put_data_buffered (socktag st,
					      char FAR * data,
					      unsigned long length);

int	FAR PASCAL gensock_put_data_flush (socktag st);

extern int globaltimeout;

#ifdef __cplusplus
}
#endif

#endif // _GENSOCK_H

