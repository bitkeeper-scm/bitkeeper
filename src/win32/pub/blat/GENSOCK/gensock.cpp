// -*- C++ -*-
// generic socket DLL, winsock version
// disclaimer:  a C programmer wrote this.

// $Id: gensock.cpp 1.15 1994/11/23 22:38:10 rushing Exp $

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

extern "C" {
#include <winsock.h>
#include "gensock.h"
}

#define SOCKET_BUFFER_SIZE	512

/* This is for NT */
#ifdef WIN32

#ifdef GENSOCK_STATIC_LINK
extern HANDLE	dll_module_handle;
#else
HANDLE	dll_module_handle;
#endif
#define	GET_CURRENT_TASK	dll_module_handle
#define	TASK_HANDLE_TYPE	HANDLE
#define GENSOCK_EXPORT

/* This is for WIN16 */
#else
HINSTANCE dll_module_handle;
#define	GET_CURRENT_TASK	GetCurrentTask()
#define	TASK_HANDLE_TYPE	HTASK
#define GENSOCK_EXPORT		_export
#endif

int  init_winsock (void);
void deinit_winsock (void);
int globaltimeout=30;

//
//
//

#ifdef _DEBUG
void complain (char * message)
{
  OutputDebugString (message);
}
#else
void complain (char * message)
{
//  MessageBox (NULL, message, "GENSOCK.DLL Error", MB_OK|MB_ICONHAND);
	printf("%s\n", message);
}
#endif

//
// ---------------------------------------------------------------------------
// container for a buffered SOCK_STREAM.

class connection
{
 private:
  SOCKET	the_socket;
  char *	in_buffer;
  char *	out_buffer;
  unsigned int	in_index;
  unsigned int	out_index;
  unsigned int	in_buffer_total;
  unsigned int	out_buffer_total;
  unsigned int	last_winsock_error;
  TASK_HANDLE_TYPE		owner_task;
  fd_set	fds;
  struct timeval	timeout;

 public:

  connection (void);
  ~connection (void);

  int 		get_connected (char * hostname, char * service);
  SOCKET 	get_socket(void) { return (the_socket); }
  TASK_HANDLE_TYPE		get_owner_task(void) { return (owner_task); }
  int		get_buffer(int wait);
  int		close (void);
  int		getachar (int wait, char * ch);
  int		put_data (char * data, unsigned long length);
  int		put_data_buffered (char * data, unsigned long length);
  int		put_data_flush (void);
};

connection::connection (void)
{
  the_socket = 0;
  in_index = 0;
  out_index = 0;
  in_buffer_total = 0;
  out_buffer_total = 0;
  in_buffer = 0;

  in_buffer = new char[SOCKET_BUFFER_SIZE];
  out_buffer = new char[SOCKET_BUFFER_SIZE];

  last_winsock_error = 0;
}

connection::~connection (void)
{
  delete [] in_buffer;
  delete [] out_buffer;
}

int
gensock_is_a_number (char * string)
{
  while (*string) {
    if (!isdigit (*string)) {
      return (0);
    }
    string++;
  }
  return (1);
}

//
// ---------------------------------------------------------------------------
//

int
connection::get_connected (char FAR * hostname, char FAR * service)
{
  struct hostent FAR *	hostentry;
  struct servent FAR *	serventry;
  unsigned long 	ip_address;
  struct sockaddr_in	sa_in;
  int			our_port;
  int			_not = 0;
  int			retval, err_code;
  unsigned long		ioctl_blocking = 1;
  char			message[512];

  // if the ctor couldn't get a buffer
  if (!in_buffer || !out_buffer)
    return (ERR_CANT_MALLOC);

  // --------------------------------------------------
  // resolve the service name
  //

  // If they've specified a number, just use it.
  if (gensock_is_a_number (service)) {
    char * tail;
    our_port = (int) strtol (service, &tail, 10);
    if (tail == service) {
      return (ERR_CANT_RESOLVE_SERVICE);
    } else {
      our_port = htons (our_port);
    }
  } else {
    // we have a name, we must resolve it.
    serventry = getservbyname (service, (LPSTR)"tcp");

    if (serventry)
      our_port = serventry->s_port;
    else {
      retval = WSAGetLastError();
      // Chicago beta is throwing a WSANO_RECOVERY here...
      if ((retval == WSANO_DATA) || (retval == WSANO_RECOVERY)) {
	return (ERR_CANT_RESOLVE_SERVICE);
      } else {
	return (retval - 5000);
      }
    }
  }

  // --------------------------------------------------
  // resolve the hostname/ipaddress
  //

  if ((ip_address = inet_addr (hostname)) != INADDR_NONE) {
    sa_in.sin_addr.s_addr = ip_address;
  }
  else {
    if ((hostentry = gethostbyname(hostname)) == NULL) {
      return (ERR_CANT_RESOLVE_HOSTNAME);
    }
    sa_in.sin_addr.s_addr = *(long far *)hostentry->h_addr;
  }


  // --------------------------------------------------
  // get a socket
  //

  if ((the_socket = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET) {
    return (ERR_CANT_GET_SOCKET);
  }

  sa_in.sin_family = AF_INET;
  sa_in.sin_port = our_port;

  // set socket options.  DONTLINGER will give us a more graceful disconnect

  setsockopt(the_socket,
	     SOL_SOCKET,
	     SO_DONTLINGER,
	     (char *) &_not, sizeof(_not));

  // get a connection

  if ((retval = connect (the_socket,
			 (struct sockaddr *)&sa_in,
			 sizeof(struct sockaddr_in))==SOCKET_ERROR)) {
    switch ((err_code = WSAGetLastError())) {
      /* twiddle our thumbs until the connect succeeds */
    case WSAEWOULDBLOCK:
      break;
    case WSAECONNREFUSED:
      return (ERR_CONNECTION_REFUSED);
      break;
    default:
      wsprintf(message, "unexpected error %d from winsock", err_code);
      complain(message);
      return (ERR_CANT_CONNECT);
      break;
    }
  }

  owner_task = GET_CURRENT_TASK;

  // Make this a non-blocking socket
  ioctlsocket (the_socket, FIONBIO, &ioctl_blocking);

  // make the FD_SET and timeout structures for later operations...

  FD_ZERO (&fds);
  FD_SET  (the_socket, &fds);

  // normal timeout, can be changed by the wait option.
  timeout.tv_sec = globaltimeout;
  timeout.tv_usec = 0;

  return (0);
}


//
//---------------------------------------------------------------------------
//
// The 'wait' parameter, if set, says to return WAIT_A_BIT
// if there's no data waiting to be read.

int
connection::get_buffer(int wait)
{
  int retval;
  int bytes_read = 0;
  unsigned long ready_to_read = 0;

  // Use select to see if data is waiting...

  FD_ZERO (&fds);
  FD_SET  (the_socket, &fds);

  // if wait is set, we are polling, return immediately
  if (wait) {
    timeout.tv_sec = 0;
  }
  else {
    timeout.tv_sec = globaltimeout;
  }

  if ((retval = select (0, &fds, NULL, NULL, &timeout))
      == SOCKET_ERROR) {
    char what_error[256];
    int error_code = WSAGetLastError();

    if (error_code == WSAEINPROGRESS && wait) {
      return (WAIT_A_BIT);
    }

    wsprintf (what_error,
	      "connection::get_buffer() unexpected error from select: %d",
	      error_code);
    complain (what_error);
  }

  // if we don't want to wait
  if (!retval && wait) {
    return (WAIT_A_BIT);
  }

  // we have data waiting...
  bytes_read = recv (the_socket,
		     in_buffer,
		     SOCKET_BUFFER_SIZE,
		     0);

  // just in case.

  if (bytes_read == 0) {
    // connection terminated (semi-) gracefully by the other side
    return (ERR_NOT_CONNECTED);
  }

  if (bytes_read == SOCKET_ERROR) {
    char what_error[256];
    int ws_error = WSAGetLastError();
    switch (ws_error) {
      // all these indicate loss of connection (are there more?)
    case WSAENOTCONN:
    case WSAENETDOWN:
    case WSAENETUNREACH:
    case WSAENETRESET:
    case WSAECONNABORTED:
    case WSAECONNRESET:
      return (ERR_NOT_CONNECTED);
      break;

    case WSAEWOULDBLOCK:
      return (WAIT_A_BIT);
      break;

    default:
      wsprintf (what_error,
		"connection::get_buffer() unexpected error: %d",
		ws_error);
      complain (what_error);
    }
  }

  // reset buffer indices.
  in_buffer_total = bytes_read;
  in_index = 0;
  return (0);

}

//
//---------------------------------------------------------------------------
// get a character from this connection.
//

int
connection::getachar(int wait, char FAR * ch)
{
  int retval;

  if (in_index >= in_buffer_total) {
    if ((retval = get_buffer(wait)))
      return (retval);
  }
  *ch = in_buffer[in_index++];
  return (0);
}


//
//---------------------------------------------------------------------------
// FIXME: should try to handle the fact that send can only take
// an int, not an unsigned long.

int
connection::put_data (char * data, unsigned long length)
{
  int num_sent;
  int retval;

  FD_ZERO (&fds);
  FD_SET  (the_socket, &fds);

  timeout.tv_sec = globaltimeout;

  while (length > 0) {
    if ((retval = select (0, NULL, &fds, NULL, &timeout)) == SOCKET_ERROR) {
      char what_error[256];
      int error_code = WSAGetLastError();

      wsprintf (what_error,
		"connection::put_data() unexpected error from select: %d",
		error_code);
      complain (what_error);
    }

    num_sent = send (the_socket,
		     data,
		     length > 1024 ? 1024 : (int)length,
		     0);

    if (num_sent == SOCKET_ERROR) {
      char what_error[256];
      int ws_error = WSAGetLastError();
      switch (ws_error) {
	// this is the only error we really expect to see.
      case WSAENOTCONN:
	return (ERR_NOT_CONNECTED);
	break;

	// seems that we can still get a block
      case WSAEWOULDBLOCK:
      case WSAEINPROGRESS:
	break;

      default:
	wsprintf (what_error,
		  "connection::put_data() unexpected error from send(): %d",
		  ws_error);
	complain (what_error);
	return (ERR_SENDING_DATA);
      }
    }
    else {
      length -= num_sent;
      data += num_sent;
    }
  }

  return (0);
}

//
//
// buffered output
//

int
connection::put_data_buffered (char * data, unsigned long length)
{
  unsigned int sorta_sent = 0;
  int retval;

  while (length) {
    if ((out_index + length) < SOCKET_BUFFER_SIZE) {
      // we won't overflow, simply copy into the buffer
      memcpy (out_buffer + out_index, data, (size_t) length);
      out_index += (unsigned int) length;
      length = 0;
    }
    else {
      unsigned int orphaned_chunk = SOCKET_BUFFER_SIZE - out_index;
      // we will overflow, handle it
      memcpy (out_buffer + out_index, data, orphaned_chunk);
      // send this buffer...
      if ((retval = put_data (out_buffer, SOCKET_BUFFER_SIZE))) {
	return (retval);
      }
      length -= orphaned_chunk;
      out_index = 0;
      data += orphaned_chunk;
    }
  }

  return (0);
}

int
connection::put_data_flush (void)
{
  int retval;

  if ((retval = put_data (out_buffer, out_index)))
    return (retval);
  else
    out_index = 0;

  return(0);
}

//
//---------------------------------------------------------------------------
//

int
connection::close (void)
{
  if (closesocket(the_socket) == SOCKET_ERROR)
    return (ERR_CLOSING);
  else
    return (0);
}


//
//---------------------------------------------------------------------------
// we keep lists of connections in this class

class connection_list
{
private:
  connection * 		data;
  connection_list * 	next;

public:
  connection_list 	(void);
  ~connection_list	(void);
  void push 		(connection & conn);

  // should really use pointer-to-memberfun for these
  connection * find	(SOCKET sock);
  int how_many_are_mine	(void);

  void remove		(socktag sock);
};

connection_list::connection_list (void)
{
  next = 0;
}

connection_list::~connection_list(void)
{
  delete data;
}

// add a new connection to the list

void
connection_list::push (connection & conn)
{
  connection_list * new_conn;

  new_conn = new connection_list();

  new_conn->data = data;
  new_conn->next = next;

  data = &conn;
  next = new_conn;

}

int
connection_list::how_many_are_mine(void)
{
  TASK_HANDLE_TYPE	current_task = GET_CURRENT_TASK;
  connection_list * iter = this;
  int num = 0;

  while (iter->data) {
    if (iter->data->get_owner_task() == current_task)
      num++;
    iter = iter->next;
  }
  return (num);
}

// find a particular socket's connection object.

connection *
connection_list::find (SOCKET sock)
{
  connection_list * iter = this;

  while (iter->data) {
    if (iter->data->get_socket() == sock)
      return (iter->data);
    iter = iter->next;
  }
  return (0);
}

void
connection_list::remove (socktag sock)
{
  // at the end
  if (!data)
    return;

  // we can assume next is valid because
  // the last node is always {0,0}
  if (data == sock) {
    delete data;
    data = next->data;
    next = next->next;	// 8^)
    return;
  }

  // recurse
  next->remove(sock);
}

//
// ---------------------------------------------------------------------------
// global variables (shared by all DLL users)

connection_list global_socket_list;
int	network_initialized;

//
//---------------------------------------------------------------------------
//

#ifndef GENSOCK_STATIC_LINK
#ifndef WIN32

// the DLL entry routine
int FAR PASCAL LibMain (HINSTANCE hinstance,
			WPARAM data_seg,
			LPARAM heap_size,
			LPSTR command_line)
{
  network_initialized = 0;
  dll_module_handle = hinstance;
  return (1);
}

#else

extern "C" {
  INT APIENTRY
    LibMain (HANDLE 	hInst,
	     ULONG		reason_called,
	     LPVOID		reserved)
      {

	switch (reason_called) {
	case DLL_PROCESS_ATTACH:
	  /* init */
	  dll_module_handle = hInst;
	  break;
	case DLL_THREAD_ATTACH:
	  break;
	case DLL_THREAD_DETACH:
	  break;
	case DLL_PROCESS_DETACH:
	  break;

	default:
	  break;
	}
	return (1);
      }

  /*
   * This wrapper is the actual entry point for the DLL.  It ensures
   * that the C RTL is correctly [de]initialized.
   */

BOOL WINAPI _CRT_INIT (HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpReserved);

BOOL WINAPI
dll_entry_point (HINSTANCE hinstDLL,
		 DWORD fdwReason,
		 LPVOID lpReserved)
{
  /* Init the C run-time before calling any of your code */

  switch (fdwReason) {
  case DLL_PROCESS_ATTACH:
  case DLL_THREAD_ATTACH:
    if (!_CRT_INIT (hinstDLL, fdwReason, lpReserved))
      return (FALSE);
    else
      LibMain (hinstDLL, fdwReason, lpReserved);
    break;

  case DLL_PROCESS_DETACH:
  case DLL_THREAD_DETACH:
    if (!_CRT_INIT(hinstDLL, fdwReason, lpReserved))
      return(FALSE);
    break;
  }
  return (TRUE);
}

}
#endif
#endif

// ---------------------------------------------------------------------------
// C/DLL interface
//

int FAR PASCAL GENSOCK_EXPORT
gensock_connect (char FAR * hostname,
		 char FAR * service,
		 socktag FAR * pst)
{
  int retval;
  connection * conn = new connection;

  if (!conn)
    return (ERR_INITIALIZING);

  // if this task hasn't opened any sockets yet, then
  // call WSAStartup()

  if (global_socket_list.how_many_are_mine() < 1)
    init_winsock();

  global_socket_list.push(*conn);

  if ((retval = conn->get_connected (hostname, service))) {
    gensock_close(conn);
    *pst = 0;
    return (retval);
  }
  *pst = (void FAR *) conn;

  return (0);
}

//
//
//

int FAR PASCAL GENSOCK_EXPORT
gensock_getchar (socktag st, int wait, char FAR * ch)
{
  connection * conn;
  int retval = 0;

  conn = (connection *) st;
  if (!conn)
    return (ERR_NOT_A_SOCKET);

  if ((retval = conn->getachar(wait, ch)))
    return (retval);
  else
    return (0);
}


//---------------------------------------------------------------------------
//
//

int FAR PASCAL GENSOCK_EXPORT
gensock_put_data (socktag st, char FAR * data, unsigned long length)
{
  connection * conn;
  int retval = 0;

  conn = (connection *) st;

  if (!conn)
    return (ERR_NOT_A_SOCKET);

  if ((retval = conn->put_data(data, length)))
    return (retval);

  return (0);
}

//---------------------------------------------------------------------------
//
//

int FAR PASCAL GENSOCK_EXPORT
gensock_put_data_buffered (socktag st, char FAR * data, unsigned long length)
{
  connection * conn;
  int retval = 0;

  conn = (connection *) st;

  if (!conn)
    return (ERR_NOT_A_SOCKET);

  if ((retval = conn->put_data_buffered (data, length)))
    return (retval);

  return (0);
}

//---------------------------------------------------------------------------
//
//

int FAR PASCAL GENSOCK_EXPORT
gensock_put_data_flush (socktag st)
{
  connection * conn;
  int retval = 0;

  conn = (connection *) st;

  if (!conn)
    return (ERR_NOT_A_SOCKET);

  if ((retval = conn->put_data_flush() ))
    return (retval);

  return (0);
}

//---------------------------------------------------------------------------
//
//
int FAR PASCAL GENSOCK_EXPORT
gensock_gethostname (char FAR * name, int namelen)
{
  int retval;
  if ((retval = gethostname(name, namelen))) {
    return (retval - 5000);
  }
  else return (0);
}

//---------------------------------------------------------------------------
//
//

int FAR PASCAL GENSOCK_EXPORT
gensock_close (socktag st)
{
  connection * conn;
  int retval;

  conn = (connection *) st;

  if (!conn)
    return (ERR_NOT_A_SOCKET);

  if ((retval = conn->close()))
    return (retval);

  global_socket_list.remove((connection *)st);

  if (global_socket_list.how_many_are_mine() < 1) {
    deinit_winsock();
  }

  return (0);
}

//---------------------------------------------------------------------------
//
//

int
init_winsock(void)
{
  int retval;
  WSADATA winsock_data;
  WORD version_required = 0x0101; /* Version 1.1 */

  retval = WSAStartup (version_required, &winsock_data);

  switch (retval) {
  case 0:
    /* successful */
    break;
  case WSASYSNOTREADY:
    return (ERR_SYS_NOT_READY);
    break;
  case WSAEINVAL:
    return (ERR_EINVAL);
    break;
  case WSAVERNOTSUPPORTED:
    return (ERR_VER_NOT_SUPPORTED);
    break;
  }
  network_initialized = 1;
  return (0);
}

void
deinit_winsock(void)
{
  network_initialized = 0;
  WSACleanup();
}
