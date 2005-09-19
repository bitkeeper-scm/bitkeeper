/*
 * SvcMgr2 -- an utility to make a service from any executable.
 * 
 * (c) 2004-2005 Anton Kovalenko
 * 
 * Do what you want with this software, but don't blame me.
 * 
 * It can forward SCM requests to the script's stdin and read log messages from 
 * the script's stdout.
 * 
 * It can also use named pipes svcmgr.scm.in.<svcname> and
 * svcmgr.scm.out.<svcname> to communicate with the process.
 * 
 */

#ifdef UNICODE
#define _UNICODE 1
#endif

#define WSLINKAGE static
#include <tchar.h>
#include <windows.h>
#include <ctype.h>
#include <process.h>
#include <io.h>

#include <stdio.h>
#include <stdlib.h>
#include <locale.h>
#include <fcntl.h>
#include <stdarg.h>

char    USAGE[] =
#include "usage.h"
    ;

WSLINKAGE int util_main(int argc, LPTSTR * argv);
WSLINKAGE BOOL main_loop();
WSLINKAGE BOOL reg_read_param();
WSLINKAGE BOOL start_process();

/*
 * SECTION: generic re-usable functions to make programming C a little easier 
 */

/*
 * SUBSECTION: registry 
 */

/*
 * reg_read is a wrapper for RegQueryValueEx that takes care of malloc and
 * realloc, and autoexpands REG_EXPAND_SZ values when requested. Non-obvious
 * argument: val_ptr is a pointer to pointer, to make the caller's variable
 * point at malloc'ed storage. Returns: type of value 
 */
static int
do_nothing(FILE * fd, ...)
{
	return (0);
}

#ifdef DBGLOGGING
#define DBG con_ftprintf
#else
#define DBG do_nothing
#endif

WSLINKAGE VOID eventlog_error(LPTSTR mgtag);

#define REG_READ_INITIAL_BUFSIZE 32
WSLINKAGE DWORD
reg_read(HKEY hkey, LPCTSTR vname, LPBYTE * val_ptr, BOOL autoexpand)
{
	DWORD   vtype = 0, bfrSize = REG_READ_INITIAL_BUFSIZE;
	LPTSTR  not_expanded;
	DWORD   exp_cnt, exp_rq;
	LONG    result;

	*val_ptr = malloc(REG_READ_INITIAL_BUFSIZE);
	result = RegQueryValueEx(hkey, vname, NULL, &vtype, *val_ptr, &bfrSize);
	if (result == ERROR_MORE_DATA) {
		*val_ptr = realloc(*val_ptr, bfrSize);
		result =
		    RegQueryValueEx(hkey, vname, NULL, &vtype, *val_ptr,
		    &bfrSize);
	}
	if (result != ERROR_SUCCESS) {
		free(*val_ptr);
		*val_ptr = NULL;
		return (0);
	}
	if (autoexpand && (vtype == REG_EXPAND_SZ)) {
		not_expanded = (LPTSTR) * val_ptr;
		exp_cnt = _tcslen(not_expanded) + 1;
		*val_ptr = malloc(exp_cnt * sizeof(TCHAR));
		exp_rq =
		    ExpandEnvironmentStrings(not_expanded, (LPTSTR) * val_ptr,
		    exp_cnt);
		if (exp_rq > exp_cnt) {
			*val_ptr = realloc(*val_ptr, exp_rq * sizeof(TCHAR));
			ExpandEnvironmentStrings(not_expanded,
			    (LPTSTR) * val_ptr, exp_rq);
		}
		free(not_expanded);
	}
	return (vtype);
}

/*
 * RegOpenKeyEx/RegCreateKeyEx/RegConnectRegistry wrapper 
 */
#define reg_close(hkey) RegCloseKey(hkey)	// Just for consistency
WSLINKAGE HKEY reg_open(LPCTSTR machine, HKEY parent, LPCTSTR keypath, int mode)
{
	HKEY    result = NULL, real_parent = parent;
	REGSAM  sam = ((mode == O_RDONLY) ? KEY_READ : KEY_READ | KEY_WRITE);

	// if machine is given, parent is a predefined key, like
	// HKEY_LOCAL_MACHINE. real_parent will be remote.
	if (machine &&
	    (ERROR_SUCCESS !=
		RegConnectRegistry(machine, parent, &real_parent))) {
		return (NULL);
	}
	if (mode == O_CREAT) {
		if (ERROR_SUCCESS !=
		    RegCreateKeyEx(real_parent, keypath, 0, NULL,
		    REG_OPTION_NON_VOLATILE, sam, NULL, &result, NULL)) {
			return (NULL);
		}
	} else {
		if (ERROR_SUCCESS !=
		    RegOpenKeyEx(real_parent, keypath, 0, sam, &result)) {
			return (NULL);
		}
	}
	if (machine) reg_close(real_parent);
	return (result);
}

/*
 * SUBSECTION: file streams & handles 
 */

/*
 * Duplicate handle pointed by "what", making it noninheritable. The resulting
 * handle is stored in the place of old one 
 */
WSLINKAGE BOOL fh_dup_noinherit(HANDLE * what)
{
	HANDLE  me, newh;
	BOOL    result;

	me = GetCurrentProcess();
	result = DuplicateHandle(me,
	    *what, me, &newh, 0, FALSE, DUPLICATE_SAME_ACCESS);
	if (result) {
		CloseHandle(*what);
		*what = newh;
	}
	return (result);
}

/*
 * Create unidirectional anonymous pipe 
 */
WSLINKAGE BOOL fh_pipe(HANDLE * this_end, HANDLE * that_end, BOOL we_write)
{
	SECURITY_ATTRIBUTES sa;
	HANDLE *wr_end, *rd_end;

	if (we_write) {
		wr_end = this_end;
		rd_end = that_end;
	} else {
		wr_end = that_end;
		rd_end = this_end;
	}
	memset(&sa, 0, sizeof(SECURITY_ATTRIBUTES));
	sa.nLength = sizeof(SECURITY_ATTRIBUTES);
	sa.lpSecurityDescriptor = NULL;
	sa.bInheritHandle = TRUE;
	if (CreatePipe(rd_end, wr_end, &sa, 0)) {
		return (fh_dup_noinherit(this_end));
	} else {
		eventlog_error(TEXT("SvcMgr (CreatePipe)"));
		return (0);
	}
}

/*
 * build a CRT stream upon a file handle 
 */
WSLINKAGE FILE * fh_to_stream(HANDLE fh, int for_open, const char *for_fopen)
{
	int     crfh;
	FILE   *stream;

	if (!fh) return (NULL);
	crfh = _open_osfhandle((long)fh, for_open | _O_NOINHERIT);
	stream = fdopen(crfh, for_fopen);
	return (stream);
}

/*
 * for the CRT stream, return the underlying file handle 
 */
WSLINKAGE HANDLE stream_to_fh(FILE * stream)
{
	return ((HANDLE) _get_osfhandle(fileno(stream)));
}

/*
 * SUBSECTION: asynchronous fgets 
 */
typedef struct _tag_async_fgets_t {
	HANDLE  hFile, hReady, hProcessed, hWorkerThread;
	LPVOID  data;
} async_fgets_t;

/*
 * The routine that does fgets in a separate thread, signalling when data is
 * ready and then waiting for someone to process the data 
 */
static DWORD WINAPI
fh_async_fgets_thread(LPVOID clientData)
{
	async_fgets_t *cd = (async_fgets_t *) clientData;
	FILE   *fp;
	TCHAR  *buf, *r;

	cd->data = buf = calloc(4096, sizeof(TCHAR));
	fp = fh_to_stream(cd->hFile, _O_RDONLY | _O_TEXT, "rt");
	setvbuf(fp, NULL, _IONBF, 0);

	for (;;) {
		if (!cd->data) break;
		r = _fgetts(buf, 4096, fp);
		if (!r) break;
		buf[4095] = 0;
		// signal that data is ready
		SetEvent(cd->hReady);
		// wait for the main thread to process the data
		WaitForSingleObject(cd->hProcessed, INFINITE);
	}
	return (ERROR_SUCCESS);
}

/*
 * Create a reader thread for the handle 
 */
WSLINKAGE async_fgets_t * fh_async_fgets_start(HANDLE hFile)
{
	async_fgets_t *afg;

	if (!hFile) return (NULL);
	afg = calloc(1, sizeof(async_fgets_t));
	afg->hFile = hFile;
	afg->hReady = CreateEvent(NULL, FALSE, FALSE, NULL);
	afg->hProcessed = CreateEvent(NULL, FALSE, FALSE, NULL);
	afg->hWorkerThread =
	    CreateThread(NULL, 0, fh_async_fgets_thread, (LPVOID) afg, 0, NULL);
	return (afg);
}

/*
 * Terminate the reader 
 */
WSLINKAGE VOID fh_async_fgets_terminate(async_fgets_t * afg)
{
	if (afg) {
		TerminateThread(afg->hWorkerThread, 0);
		CloseHandle(afg->hWorkerThread);
	}
}

/*
 * SUBSECTION: strings 
 */

#define offsetof(s,i)	((char*)&((s*)NULL)->i-(char*)NULL)
/*
 * Find the array element that contains a pointer to the given string at a
 * given offset. This function may be used to find a structure in an array. Use 
 * offsetof() macro to get the offset for a structure element 
 */
WSLINKAGE void *
str_field_lookup(LPCTSTR what, void *where, int itemsize, int off)
{
	char   *cwhere = (char *)where;
	LPCTSTR candidate;

	if (!what) return (NULL);
	for (; (candidate = *(LPCTSTR *) (cwhere + off)); cwhere += itemsize) {
		DBG(stderr, TEXT("matching %s against %s\n"), what, candidate);
		if (!_tcsicmp(what, candidate)) return (cwhere);
	}
	return (NULL);
}

/*
 * Remove final newline, if any 
 */
WSLINKAGE void
str_chop(LPTSTR what)
{
	int     idx;

	if (!what) return;
	if (!*what) return;
	idx = _tcslen(what) - 1;
	if (what[idx] == TEXT('\n')) what[idx] = 0;
}

/*
 * Dynamic sprintf, that malloc()'s the necessary space itself 
 */
WSLINKAGE LPTSTR str_vprintf(LPCTSTR fmt, va_list args)
{
	LPTSTR  buf;
	size_t  bsz = 64;

	buf = calloc(bsz, sizeof(TCHAR));
	if (!buf) return (NULL);
	while (0 > _vsntprintf(buf, bsz - 1, fmt, args)) {
		bsz *= 2;
		buf = realloc(buf, bsz * sizeof(TCHAR));
		if (!buf) return (NULL);
	}
	return (buf);
}

WSLINKAGE LPTSTR str_printf(LPCTSTR fmt,...)
{
	LPTSTR  s;
	va_list args;

	va_start(args, fmt);
	s = str_vprintf(fmt, args);
	va_end(args);
	return (s);
}

/*
 * SUBSECTION: open-use-close dynamic string buffer 
 */
typedef struct {
	DWORD   buf;
	LPTSTR  msg, ptr;
} str_msg_t;

WSLINKAGE void
str_msg_start(str_msg_t * mb, DWORD sz)
{
	mb->buf = sz;
	mb->msg = mb->ptr = calloc(mb->buf, sizeof(TCHAR));
	mb->buf--;
}

WSLINKAGE void
str_msg_add(str_msg_t * mb, LPCTSTR fmt, ...)
{
	int     nch;
	va_list args;

	va_start(args, fmt);
	nch = _vsntprintf(mb->ptr, mb->buf, fmt, args);
	if (nch == -1) return;
	va_end(args);
	mb->ptr += nch;
	mb->buf -= nch;
}

#define str_msg_free(mb) free((mb)->msg);

/*
 * SUBSECTION: windows [may-be] console output 
 */

WSLINKAGE int
con_ftprintf(FILE * stream, LPCTSTR fmt, ...)
{
	LPTSTR  s;
	DWORD   ncw;
	va_list args;

	va_start(args, fmt);
	s = str_vprintf(fmt, args);
	if (!s) goto br;
	if (!WriteConsole(stream_to_fh(stream), s, _tcslen(s), &ncw, NULL)) {
		ncw = _fputts(s, stream);
	}
	free(s);
br:	va_end(args);
	return (ncw);
}

/*
 * SUBSECTION: parsing command-line options 
 */
typedef struct _tag_option_t {
	LPCTSTR name, nature;
	int     (*convertor) (void *, int *, LPCTSTR **);
	VOID   *cvcd;
	VOID   *data;
} option_t;

typedef struct _tag_keyword_t {
	LPCTSTR kw;
	DWORD   value;
} keyword_t;

WSLINKAGE DWORD lookup_kw(LPCTSTR what, keyword_t * kws)
{
	keyword_t *r;

	r = str_field_lookup(what,
	    kws, sizeof(keyword_t), offsetof(keyword_t, kw));
	if (!r) {
		return (0);
	} else {
		return (r->value);
	}
}

WSLINKAGE LPCTSTR which_kw(DWORD val, keyword_t * kws, LPCTSTR dflt)
{
	int     i;

	for (i = 0; kws[i].kw; ++i) {
		if (kws[i].value == val) return (kws[i].kw);
	}
	return (dflt);
}

#define OPT_OK 0
#define OPT_WRONGOPTION 1
#define OPT_INSUFFICIENT 2
#define OPT_BADFMT 3

static LPCTSTR
cvt_fetch(int *pargc, LPCTSTR ** pargv)
{
	LPCTSTR r;

	if (!*pargc) return (NULL);
	r = **pargv;
	++*pargv;
	--*pargc;
	return (r);
}

#define CVT_PUT(t,expr) do {if (opt->data) *(t*)opt->data = (expr);} while(0)
#define CVT_PROLOGUE LPCTSTR s; if (!(s=cvt_fetch(pargc,pargv))) \
return OPT_INSUFFICIENT

#define OD_defgroup(name) static option_t name[]={

#define OD_endgroup OD_eoo }

#define OD_string(txt,var) \
    { TEXT(txt),NULL,(LPVOID)cvtSTRING,NULL,&var },
#define OD_stringtable(txt,var) \
    { TEXT(txt),NULL,(LPVOID)cvtSTRINGTABLE,NULL,&var },
#define OD_dword(txt,var) \
    { TEXT(txt),NULL,(LPVOID)cvtDWORD,NULL,&var },
#define OD_true(txt,var) \
    { TEXT(txt),NULL,(LPVOID)cvtCONST,(LPVOID)TRUE,&var },
#define OD_false(txt,var) \
    { TEXT(txt),NULL,(LPVOID)cvtCONST,(LPVOID)FALSE,&var },
#define OD_choice(txt,var,kw,nature) \
    { TEXT(txt),TEXT(nature),(LPVOID)cvtCHOICE,(LPVOID)kw,&var },
#define OD_terminator \
    { (LPCTSTR)TEXT("--"),0,NULL,NULL },
#define OD_eoo \
    { NULL,NULL,NULL,NULL,NULL }
    /* ; */

static int
cvtDWORD(option_t * opt, int *pargc, LPCTSTR ** pargv)
{
	CVT_PROLOGUE;
	CVT_PUT(DWORD, _ttoi(s));
	return (0);
}

static int
cvtSTRING(option_t * opt, int *pargc, LPCTSTR ** pargv)
{
	CVT_PROLOGUE;
	CVT_PUT(LPCTSTR, s);
	DBG(stderr, TEXT("Option %s is %s"), opt->name, s);
	return (0);
}

static int
cvtCHOICE(option_t * opt, int *pargc, LPCTSTR ** pargv)
{
	keyword_t *variants = opt->cvcd;
	DWORD   x, i;
	str_msg_t mb;

	CVT_PROLOGUE;
	if ((x = lookup_kw(s, variants))) {
		CVT_PUT(DWORD, x);
		return (0);
	}
	/*
	 * build up an error message 
	 */
	str_msg_start(&mb, 1024);
	str_msg_add(&mb, TEXT("Bad %s %s: must be "), opt->nature, s);
	for (i = 0; variants[i].kw; ++i) {
		if (i == 0) {
			str_msg_add(&mb, TEXT("%s"), variants[i].kw);
		} else {
			str_msg_add(&mb,
			    variants[i + 1].kw ? TEXT(", %s") : TEXT(" or %s"),
			    variants[i].kw);
		}
	}
	con_ftprintf(stderr, TEXT("%s.\n"), mb.msg);
	str_msg_free(&mb);
	return (OPT_BADFMT);
}

static int
cvtSTRINGTABLE(option_t * opt, int *pargc, LPCTSTR ** pargv)
{
	LPTSTR  stab;
	int     i;

	CVT_PROLOGUE;

	stab = calloc(_tcslen(s) + 2, sizeof(TCHAR));

	for (i = 0; s[i]; ++i) {
		switch (s[i]) {
		    case TEXT(','):
			stab[i] = TEXT('\000');
			break;
		    default:
			stab[i] = s[i];
		}
	}
	stab[i++] = TEXT('\000');
	stab[i] = TEXT('\000');
	CVT_PUT(LPTSTR, stab);
	return (0);
}

static int
cvtCONST(option_t * opt, int *pargc, LPCTSTR ** pargv)
{
	CVT_PUT(BOOL, (BOOL) opt->cvcd);
	return (0);
}

WSLINKAGE DWORD parse_options(int *pargc, LPTSTR ** pargv, option_t ** opts)
{
	option_t *theopt;
	int     i, r;

	while (*pargc) {
		for (i = 0, theopt = NULL; opts[i]; ++i) {
			if (theopt =
			    str_field_lookup(**pargv, opts[i],
			    sizeof(option_t), offsetof(option_t, name))) {
				break;
			}
		}
		if (!theopt) {
			DBG(stderr, TEXT("Wrong %s\n"), **pargv);
			return (OPT_WRONGOPTION);
		}
		++*pargv;
		--*pargc;
		if (!theopt->convertor) return (0);
		if (r = theopt->convertor(theopt, pargc, (LPCTSTR **) pargv)) {
			return (r);
		}
	}
	return (0);
}

/*
 * SECTION: The Real SvcMgr
 * */

/*
 * SvcMgr can communicate with an underlying process in the following ways: 
 */
#define IPCMETHOD_BLIND 0	// No IPC, TerminateProcess for stop.
#define IPCMETHOD_PIPE 1	// Communicate via named pipe
#define IPCMETHOD_STDIO 2	// Communicate via stdin/out/err
#define IPCMETHOD_QSTDIO 3	// Communicate via stdin/out/err
#define IPCMETHOD_NO_CHANGE 1000

/*
 * SvcMgr invokation: svcmgr subcommand ?servicename? ?options? 
 */

#define CMD_HELP	12345
#define CMD_INSTALL	1
#define CMD_UNINSTALL	2
#define CMD_STOP	3
#define CMD_START	4
#define CMD_PARAMCHANGE	5
#define CMD_CONFIGURE	6
#define CMD_PAUSE	7
#define CMD_CONTINUE	8
#define CMD_RESTART	9
#define CMD_STATUS	10
#define CMD_USERCONTROL	11
#define CMD_SHOW	12

WSLINKAGE keyword_t kw_starttype[] = {
    { TEXT("auto"), SERVICE_AUTO_START },
    { TEXT("demand"), SERVICE_DEMAND_START },
    { TEXT("disabled"), SERVICE_DISABLED },
    { TEXT("boot"), SERVICE_BOOT_START },
    { TEXT("system"), SERVICE_SYSTEM_START },
    { NULL, 0 }
};

WSLINKAGE keyword_t kw_errorcontrol[] = {
    { TEXT("ignore"), SERVICE_ERROR_IGNORE },
    { TEXT("normal"), SERVICE_ERROR_NORMAL },
    { TEXT("severe"), SERVICE_ERROR_SEVERE },
    { TEXT("critical"), SERVICE_ERROR_CRITICAL },
    { NULL, 0 }
};

WSLINKAGE keyword_t kw_ipcmethod[] = {
    { TEXT("blind"), IPCMETHOD_BLIND },
    { TEXT("pipe"), IPCMETHOD_PIPE },
    { TEXT("stdio"), IPCMETHOD_STDIO },
    { TEXT("qstdio"), IPCMETHOD_QSTDIO },
    { NULL, 0 }
};

/*
 * Global variables, for BOTH service and utility 
 */

WSLINKAGE LPTSTR sv_name = NULL;
WSLINKAGE SERVICE_STATUS sv_status;
WSLINKAGE LPTSTR command_line = NULL;
WSLINKAGE DWORD ipc_method = IPCMETHOD_BLIND;
WSLINKAGE LPTSTR ipc_method_name = NULL;
WSLINKAGE int is_in_service = 0;

WSLINKAGE BOOL pass_control(LPSERVICE_MAIN_FUNCTION main_fun);
WSLINKAGE void WINAPI sv_main(DWORD dwArgc, LPTSTR * lpszArgv);
WSLINKAGE VOID WINAPI sv_ctrl(DWORD dwCtrlCode);
WSLINKAGE VOID eventlog_msg(int type, LPTSTR msg);
WSLINKAGE VOID eventlog_open(), eventlog_close();
WSLINKAGE VOID eventlog_error(LPTSTR mgtag);
WSLINKAGE BOOL my_status(DWORD stat, DWORD exitcode, DWORD whint);
WSLINKAGE VOID sv_start(DWORD argc, LPTSTR * argv);
WSLINKAGE VOID ipc_setup(int after);
WSLINKAGE VOID sv_report(LPTSTR msg, int isError);
WSLINKAGE VOID sv_handle_escapes(LPTSTR msg);
WSLINKAGE VOID puts_control();

/*
 * The main function will try to StartServiceCtrlDispatcher if no arguments is 
 * given. If there are some arguments, it calls util_main that implements
 * SvcMgr command-line utility 
 */
#ifdef UNICODE
#define	MAIN	wmain
#else
#define	MAIN	main
#endif
int     _cdecl
MAIN(int argc, LPTSTR * argv)
{
	setlocale(LC_ALL, "");
	if (argc == 1) {
		if (!pass_control(sv_main)) {
			con_ftprintf(stderr,
			    TEXT("Start this program with ServiceStart(),\n"
			    "or use \"%s -help\" for help.\n"), argv[0]);
			return (1);
		} else {
			return (0);
		}
	}
	exit(util_main(argc, argv));
}

/*
 * GNU compiler (mingw gcc) appears not to support wmain()
 * for console applications. So we use GetCommandLine and CommandLineToArgvW
 * to get unicode argv and argc.
 * */
#if defined(__GNUC__) && defined(UNICODE)
int
main(int argc, char **argv)
{
	int     real_argc;
	LPTSTR *real_argv = CommandLineToArgvW(GetCommandLineW(), &real_argc);

	return (wmain(real_argc, real_argv));
}
#endif
/*
 * Service implementation 
 */

/*
 * Global variables, for service only 
 */
SERVICE_STATUS_HANDLE sv_sh;
HANDLE  event_from_scm = NULL;
HANDLE  eventlog = NULL;
DWORD   control_code;
PROCESS_INFORMATION pinfo;
STARTUPINFO sinfo;
HANDLE  ipc_write = NULL, ipc_read = NULL, ipc_rderr = NULL;
FILE   *ipc_write_stream = NULL;
DWORD   accepts = SERVICE_ACCEPT_STOP;

/*
 * Tries to StartServiceCtrlDispatcher, returning its result 
 */
WSLINKAGE BOOL
pass_control(LPSERVICE_MAIN_FUNCTION main_fun)
{
	SERVICE_TABLE_ENTRY dispatchTable[2];

	// according to MSDN, the service name in dispatch table is
	// ignored for SERVICE_WIN32_OWN_PROCESS.
	dispatchTable[0].lpServiceName = TEXT("DOES_NOT_MATTER");
	dispatchTable[0].lpServiceProc = main_fun;
	dispatchTable[1].lpServiceName = NULL;
	dispatchTable[1].lpServiceProc = NULL;
	return (StartServiceCtrlDispatcher(dispatchTable));
}

WSLINKAGE void
sv_set_args(DWORD argc, LPTSTR * argv)
{
	LPTSTR  arg;
	DWORD   i, j, p, bufSize = 1;
	LPTSTR  strargs;

	// compute buffer size
	for (i = 0; i < argc; ++i) {
		bufSize += _tcslen(argv[i]) * 2 + 3;
	}
	strargs = malloc(bufSize * sizeof(TCHAR));
	for (i = 0, p = 0; i < argc; ++i) {
		arg = argv[i];
		if (i) strargs[p++] = TEXT(' ');
		strargs[p++] = TEXT('\"');
		for (j = 0; arg[j]; ++j) {
			if ((arg[j] == TEXT('\\')) || (arg[j] == TEXT('\"'))) {
				strargs[p++] = TEXT('\\');
			}
			strargs[p++] = arg[j];
		}
		strargs[p++] = TEXT('\"');
	}
	strargs[p++] = TEXT('\000');
	SetEnvironmentVariable(TEXT("ServiceArgs"), strargs);
	free(strargs);
}

WSLINKAGE void WINAPI
sv_main(DWORD dwArgc, LPTSTR * lpszArgv)
{
	is_in_service = 1;
	sv_name = _tcsdup(lpszArgv[0]);
	setlocale(LC_ALL, "C");
	sv_set_args(dwArgc, lpszArgv);
	setlocale(LC_ALL, "");
	eventlog_open();
	sv_sh = RegisterServiceCtrlHandler(sv_name, sv_ctrl);
	if (!sv_sh) goto cleanup;
	event_from_scm = CreateEvent(NULL, FALSE, FALSE, NULL);
	sv_status.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
	sv_status.dwServiceSpecificExitCode = 0;
	if (my_status(SERVICE_START_PENDING, NO_ERROR, 3000)) {
		sv_start(dwArgc, lpszArgv);
	}
cleanup:
	eventlog_close();
	free(sv_name);
	if (sv_sh) my_status(SERVICE_STOPPED, NO_ERROR, 0);
}

WSLINKAGE VOID
eventlog_open()
{
	eventlog = RegisterEventSource(NULL, sv_name);
}
WSLINKAGE VOID
eventlog_close()
{
	DeregisterEventSource(eventlog);
}

WSLINKAGE VOID
eventlog_msg(int type, LPTSTR msg)
{
	LPTSTR  strings[2];

	strings[0] = msg;
	ReportEvent(eventlog,
	    (short)type, 0, 1, NULL, 1, 0, (LPCTSTR *) strings, NULL);
}

WSLINKAGE VOID
eventlog_error(LPTSTR mgtag)
{
	DWORD   err;
	LPTSTR  msg;
	LPTSTR  etext = NULL;
	DWORD   etext_len;

	err = GetLastError();
	etext_len = FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER |
	    FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ARGUMENT_ARRAY,
	    NULL, err, LANG_NEUTRAL, (LPTSTR)&etext, 0, NULL);
	if (!etext_len) etext[0] = 0;
	msg = str_printf(TEXT("%s error: %s (%d) "), mgtag, etext, err);
	eventlog_msg(EVENTLOG_ERROR_TYPE, msg);
	free(msg);
}

WSLINKAGE BOOL
my_status(DWORD stat, DWORD exitcode, DWORD whint)
{
	static DWORD checkpoint = 1;

	if (stat == SERVICE_START_PENDING) {
		sv_status.dwControlsAccepted = 0;
	} else {
		sv_status.dwControlsAccepted = accepts;
	}
	sv_status.dwCurrentState = stat;
	sv_status.dwWin32ExitCode = exitcode;
	sv_status.dwWaitHint = whint;
	if ((stat == SERVICE_RUNNING) || (stat == SERVICE_STOPPED)) {
		sv_status.dwCheckPoint = 0;
	} else {
		sv_status.dwCheckPoint = checkpoint++;
	}
	return (SetServiceStatus(sv_sh, &sv_status));
}

WSLINKAGE VOID WINAPI
sv_ctrl(DWORD dwCtrlCode)
{
	control_code = dwCtrlCode;
	switch (control_code) {
	    case SERVICE_CONTROL_STOP:
		my_status(SERVICE_STOP_PENDING, NO_ERROR, 0);
		break;
	    case SERVICE_CONTROL_PAUSE:
		my_status(SERVICE_PAUSE_PENDING, NO_ERROR, 0);
		break;
	    case SERVICE_CONTROL_CONTINUE:
		my_status(SERVICE_CONTINUE_PENDING, NO_ERROR, 0);
		break;
	    default:
		my_status(sv_status.dwCurrentState, NO_ERROR, 0);
		break;
	}
	if (event_from_scm) SetEvent(event_from_scm);
}

WSLINKAGE VOID
sv_start(DWORD argc, LPTSTR * argv)
{
	reg_read_param() && start_process() && main_loop();
}

WSLINKAGE BOOL
reg_read_param()
{
	BOOL    ok = TRUE;
	LPTSTR  path =
	    str_printf(TEXT("System\\CurrentControlSet\\Services\\%s\\SvcMgr"),
	    sv_name);
	HKEY    hkey = reg_open(NULL, HKEY_LOCAL_MACHINE, path, O_RDONLY);

	if (!hkey) goto error;
	if (reg_read(hkey,
	    TEXT("CommandLine"), (LPBYTE *) & command_line, TRUE) != REG_SZ) {
		goto error;
	}
	reg_read(hkey, TEXT("IpcMethod"), (LPBYTE *) & ipc_method_name, FALSE);
	ipc_method = lookup_kw(ipc_method_name, kw_ipcmethod);
cleanup:
	if (hkey) reg_close(hkey);
	free(path);
	return (ok);
error:
	ok = FALSE;
	goto cleanup;
}

WSLINKAGE BOOL
start_process()
{
	memset(&sinfo, 0, sizeof(STARTUPINFO));
	sinfo.cb = sizeof(STARTUPINFO);
	SetEnvironmentVariable(TEXT("ServiceName"), sv_name);
	SetEnvironmentVariable(TEXT("ServiceIpcMethod"), ipc_method_name);
	ipc_setup(0);
	if (!CreateProcess(NULL, command_line, NULL, NULL,
	    TRUE, CREATE_NEW_PROCESS_GROUP | CREATE_NO_WINDOW,
	    NULL, NULL, &sinfo, &pinfo)) {
		/*
		 * we might end up with redirected std handles, but it's okey
		 * as we do nothing expect reporting SERVICE_STOPPED and
		 * exiting 
		 */
		eventlog_error(TEXT("SvcMgr (CreateProcess)"));
		return (FALSE);
	}

	ipc_setup(1);
	my_status(SERVICE_RUNNING, NO_ERROR, 0);
	return (TRUE);
}

WSLINKAGE BOOL
main_loop()
{
	HANDLE  objects[5];
	DWORD   objc = 0;
	async_fgets_t *afg_out, *afg_err;

	objects[objc++] = event_from_scm;
	objects[objc++] = pinfo.hProcess;
	if ((afg_out = fh_async_fgets_start(ipc_read))) {
		objects[objc++] = afg_out->hReady;
	}
	if ((afg_err = fh_async_fgets_start(ipc_rderr))) {
		objects[objc++] = afg_err->hReady;
	}
	if ((ipc_write_stream =
		fh_to_stream(ipc_write, O_WRONLY | O_TEXT, "wt"))) {
		setvbuf(ipc_write_stream, NULL, _IONBF, 0);
	}
	for (;;) {
		switch (
		    WaitForMultipleObjects(objc, objects, FALSE, INFINITE)) {
		    case WAIT_OBJECT_0:
			// We received the control code, so let's
			// report it via svc_Ipc_write (if any).
			puts_control();
			break;
		    case WAIT_OBJECT_0 + 1:
			// The process exited
			fh_async_fgets_terminate(afg_out);
			fh_async_fgets_terminate(afg_err);
			return (TRUE);
		    case WAIT_OBJECT_0 + 2:
			// something received from stdout or pipe
			sv_report((LPTSTR) afg_out->data, 0);
			SetEvent(afg_out->hProcessed);
			break;
		    case WAIT_OBJECT_0 + 3:
			// something received from stderr
			eventlog_msg(EVENTLOG_ERROR_TYPE,
			    TEXT("SvcMgr: stderr"));
			sv_report((LPTSTR) afg_err->data, 1);
			SetEvent(afg_err->hProcessed);
			break;
		}
	}
	return (TRUE);
}

WSLINKAGE LPTSTR pipenames[2];

/*
 * When named pipes are used, we shouldn't hang up forever if the underlying
 * process was unable to open the pipes 
 */
WSLINKAGE HANDLE npw_events[2];
static DWORD WINAPI
npipes_watchdog(LPVOID clientData)
{
	DWORD   wfmo;

	wfmo = WaitForMultipleObjects(2, npw_events, FALSE, 30000);
	if (wfmo != WAIT_OBJECT_0) {
		/*
		 * Something went wrong. We connect pipes to release ipc_setup 
		 */
		CloseHandle(
		    CreateFile(pipenames[0], FILE_WRITE_DATA | FILE_READ_DATA,
		    0, NULL, OPEN_EXISTING, 0, NULL));
		CloseHandle(
		    CreateFile(pipenames[1], FILE_WRITE_DATA | FILE_READ_DATA,
		    0, NULL, OPEN_EXISTING, 0, NULL));
		/*
		 * If the process is not terminated yet 
		 */
		if (wfmo != WAIT_OBJECT_0 + 1) {
			TerminateProcess(pinfo.hProcess, 0);
		}
	} else {
		CloseHandle(npw_events[0]);
	}
	return (0);
}
WSLINKAGE VOID
ipc_setup(int after)
{
	HANDLE  dog;
	SECURITY_ATTRIBUTES sa;

	if (ipc_method == IPCMETHOD_PIPE) {
		if (!after) {
			// It's invoked before CreateProcess
			pipenames[0] =
			    str_printf(TEXT("\\\\.\\pipe\\svcmgr.scm.out.%s"),
			    sv_name);
			pipenames[1] =
			    str_printf(TEXT("\\\\.\\pipe\\svcmgr.scm.in.%s"),
			    sv_name);
			// We don't free them to avoid race condition
			memset(&sa, 0, sizeof(SECURITY_ATTRIBUTES));
			sa.nLength = sizeof(SECURITY_ATTRIBUTES);
			sa.lpSecurityDescriptor = NULL;
			sa.bInheritHandle = FALSE;
			ipc_read =
			    CreateNamedPipe(pipenames[0],
			    PIPE_ACCESS_DUPLEX,
			    PIPE_TYPE_BYTE | PIPE_WAIT, 1, 256, 256, 0, &sa);
			ipc_write =
			    CreateNamedPipe(pipenames[1],
			    PIPE_ACCESS_DUPLEX,
			    PIPE_TYPE_BYTE | PIPE_WAIT, 1, 256, 256, 0, &sa);
		} else {
			// It's OK
			npw_events[0] = CreateEvent(NULL, FALSE, FALSE, NULL);
			npw_events[1] = pinfo.hProcess;
			dog = CreateThread(NULL,0,npipes_watchdog,NULL,0, NULL);
			ConnectNamedPipe(ipc_read, NULL);
			ConnectNamedPipe(ipc_write, NULL);
			SetEvent(npw_events[0]);
			CloseHandle(dog);
		}
	} else if ((ipc_method == IPCMETHOD_STDIO) ||
	    (ipc_method == IPCMETHOD_QSTDIO)) {
		if (!after) {
			fh_pipe(&ipc_write, &sinfo.hStdInput, TRUE);
			fh_pipe(&ipc_read, &sinfo.hStdOutput, FALSE);
			fh_pipe(&ipc_rderr, &sinfo.hStdError, FALSE);
			sinfo.dwFlags |= STARTF_USESTDHANDLES;
		} else {
			CloseHandle(sinfo.hStdInput);
			CloseHandle(sinfo.hStdOutput);
			CloseHandle(sinfo.hStdError);
		}
	}
}

WSLINKAGE VOID
sv_report(LPTSTR msg, int isError)
{
	int     i;

	/*
	 * Translate ^L to \n to support multiline eventlog messages 
	 */
	for (i = 0; msg[i]; ++i) {
		if (msg[i] == TEXT('\014')) msg[i] = TEXT('\n');
	}
	if ((!isError) && (msg[0] == TEXT('\033'))) {
		sv_handle_escapes(msg + 1);
		return;
		/*
		 * Thus we support 'service escapes' on stdout 
		 */
	}
	/*
	 * add it to event log 
	 */
	if (ipc_method != IPCMETHOD_QSTDIO) {
		str_chop(msg);
		eventlog_msg(
		    isError ? EVENTLOG_ERROR_TYPE : EVENTLOG_SUCCESS, msg);
	}
}

WSLINKAGE VOID
sv_handle_escapes(LPTSTR msg)
{
	DWORD   evtype;

	switch (msg[0]) {
	    case TEXT('a'):	// ESC a: accept control codes
		switch (msg[1]) {
		    case TEXT('p'):
			accepts |= SERVICE_ACCEPT_PAUSE_CONTINUE;
			break;
		    case TEXT('c'):
			accepts |= SERVICE_ACCEPT_PARAMCHANGE;
			break;
		    case TEXT('s'):
			accepts |= SERVICE_ACCEPT_SHUTDOWN;
			break;
		    case TEXT('n'):
			accepts |= SERVICE_ACCEPT_NETBINDCHANGE;
			break;
		    case TEXT('r'):
			accepts = SERVICE_ACCEPT_STOP;
			break;
		    case TEXT('P'):
			accepts &= ~SERVICE_ACCEPT_PAUSE_CONTINUE;
			break;
		    case TEXT('C'):
			accepts &= ~SERVICE_ACCEPT_PARAMCHANGE;
			break;
		    case TEXT('S'):
			accepts &= ~SERVICE_ACCEPT_SHUTDOWN;
			break;
		    case TEXT('N'):
			accepts &= ~SERVICE_ACCEPT_NETBINDCHANGE;
			break;
		}
		break;
	    case TEXT('s'):	// ESC s: set status value
		switch (msg[1]) {
		    case TEXT('p'):
			sv_status.dwCurrentState = SERVICE_PAUSED;
			break;
		    case TEXT('P'):
			sv_status.dwCurrentState = SERVICE_PAUSE_PENDING;
			break;
		    case TEXT('C'):
			sv_status.dwCurrentState = SERVICE_CONTINUE_PENDING;
			break;
		    case TEXT('r'):
			sv_status.dwCurrentState = SERVICE_RUNNING;
			break;
		    case TEXT('s'):
			sv_status.dwCurrentState = SERVICE_START_PENDING;
			break;
		    case TEXT('S'):
			sv_status.dwCurrentState = SERVICE_STOP_PENDING;
			break;
		    default:
			return;
		}
		break;
	    case TEXT('e'):	// ESC e: log a message with type specified 
		str_chop(msg);
		switch (msg[1]) {
		    case TEXT('e'):
			evtype = EVENTLOG_ERROR_TYPE;
			break;
		    case TEXT('w'):
			evtype = EVENTLOG_WARNING_TYPE;
			break;
		    case TEXT('i'):
			evtype = EVENTLOG_INFORMATION_TYPE;
			break;
		    case TEXT('s'):
			evtype = EVENTLOG_SUCCESS;
			break;
		    case TEXT('a'):
			evtype = EVENTLOG_AUDIT_SUCCESS;
			break;
		    case TEXT('A'):
			evtype = EVENTLOG_AUDIT_FAILURE;
			break;
		    default:
			return;
		}
		eventlog_msg(evtype, msg + 2);
		return;
	}
	my_status(sv_status.dwCurrentState, NO_ERROR, 0);
}

#if 0
/*
 * For IPCMETHOD_BLIND, we no longer use TerminateProcess from the very start.
 * First we try to send WM_QUIT to the application's thread;
 * if it is still running after 10 sec, TerminateProcess is our friend.
 * */
WSLINKAGE int
please_shutdown()
{
	if (PostThreadMessage(pinfo.dwThreadId, WM_QUIT, 0, 0) &&
	    (WaitForSingleObject(pinfo.hProcess, 10000) != WAIT_TIMEOUT)) {
		return (1);	/* Successssss, my preciousssssss */
	} else {
		return (0);
	}
}
#endif

WSLINKAGE VOID
puts_control()
{
	LPTSTR  ctlname = NULL;
	TCHAR   ctbuff[32];
	DWORD   bwritten = 0;

	if ((!ipc_write_stream) && (control_code == SERVICE_CONTROL_STOP)) {
#if 0
		/*
		 * We attempt to shut down the process gracefully sending
		 * WM_CLOSE to all its windows. 
		 */
		if (!please_shutdown()) TerminateProcess(pinfo.hProcess, 0);
#else
		TerminateProcess(pinfo.hProcess, 0);
#endif
		return;
	}
	/*
	 * When ipc_method is IPCMETHOD_BLIND, ipc_write_stream is NULL
	 * and there is no communication between svcmgr and the managed
	 * process.  SCM has already called sv_ctrl() and it has called
	 * SetServiceStatus() so there is nothing more to do.
	 * Thanks to Ashok Singhal.
	 */
	if (ipc_write_stream == NULL) return;

	switch (control_code) {
	    case SERVICE_CONTROL_STOP:
		ctlname = TEXT("STOP");
		break;
	    case SERVICE_CONTROL_INTERROGATE:
		ctlname = TEXT("INTERROGATE");
		break;
	    case SERVICE_CONTROL_PAUSE:
		ctlname = TEXT("PAUSE");
		break;
	    case SERVICE_CONTROL_CONTINUE:
		ctlname = TEXT("CONTINUE");
		break;
	    case SERVICE_CONTROL_SHUTDOWN:
		ctlname = TEXT("SHUTDOWN");
		break;
	    case SERVICE_CONTROL_NETBINDADD:
		ctlname = TEXT("NETBINDADD");
		break;
	    case SERVICE_CONTROL_NETBINDREMOVE:
		ctlname = TEXT("NETBINDREMOVE");
		break;
	    case SERVICE_CONTROL_NETBINDDISABLE:
		ctlname = TEXT("NETBINDDISABLE");
		break;
	    case SERVICE_CONTROL_NETBINDENABLE:
		ctlname = TEXT("NETBINDENABLE");
		break;
	    case SERVICE_CONTROL_PARAMCHANGE:
		ctlname = TEXT("PARAMCHANGE");
		break;
	    default:
		_stprintf(ctbuff, TEXT("CODE%u"), control_code);
		ctlname = ctbuff;
	}
	bwritten = _ftprintf(ipc_write_stream, TEXT("%s\n"), ctlname);
	fflush(ipc_write_stream);
}

/*
 * Below is the code that enables SvcMgr to install, remove and manage 
 * the services.
 * */

/*
 * Global variables: for utility only 
 */

WSLINKAGE BOOL foreign, nowait, expand, interactive, force_foreign;
WSLINKAGE LPTSTR user, passwd, description;
WSLINKAGE QUERY_SERVICE_CONFIG qsc;

OD_defgroup(og_common)
    OD_terminator OD_endgroup;

OD_defgroup(og_sconf)
    OD_string("-displayname", qsc.lpDisplayName)
    OD_string("-description", description)
    OD_true("-interactive", interactive)
    OD_false("-noninteractive", interactive)
    OD_false("-nointeractive", interactive)
    OD_true("-expand", expand)
    OD_false("-noexpand", expand)
    OD_string("-image", qsc.lpBinaryPathName)
    OD_string("-binary", qsc.lpBinaryPathName)
    OD_choice("-errorcontrol", qsc.dwErrorControl, kw_errorcontrol,
    "error control mode")
    OD_choice("-start", qsc.dwStartType, kw_starttype, "service start type")
    OD_choice("-starttype", qsc.dwStartType, kw_starttype, "service start type")
    OD_string("-loadordergroup", qsc.lpLoadOrderGroup)
    OD_stringtable("-depends", qsc.lpDependencies)
    OD_choice("-ipc", ipc_method, kw_ipcmethod, "ipc method")
    OD_choice("-ipcmethod", ipc_method, kw_ipcmethod, "ipc method")
    OD_endgroup;

OD_defgroup(og_userpw)
    OD_string("-user", user)
    OD_string("-password", passwd)
    OD_endgroup;

OD_defgroup(og_userctl)
    OD_dword("-code", control_code)
    OD_endgroup;

OD_defgroup(og_nowait)
    OD_true("-nowait", nowait)
    OD_endgroup;

OD_defgroup(og_forceforeign)
    OD_true("-forceforeign", force_foreign)
    OD_endgroup;

static option_t 
*cmdo_install[]={og_common,og_sconf,og_userpw,NULL},
    *cmdo_uninstall[]={og_common,NULL},
    **cmdo_start=cmdo_uninstall,
    **cmdo_restart=cmdo_uninstall,
    **cmdo_status=cmdo_uninstall,
    **cmdo_paramchange=cmdo_uninstall,
    *cmdo_stop[]={og_common,og_nowait,NULL},
    **cmdo_pause=cmdo_stop,
    **cmdo_continue=cmdo_stop,
    *cmdo_usercontrol[]={og_common,og_userctl,NULL},
    *cmdo_configure[]={og_common,og_sconf,og_forceforeign,og_userpw,NULL},
    **cmdo_show=cmdo_uninstall
    ;

static keyword_t kw_commands[]={
    { TEXT("-help"), CMD_HELP },
    { TEXT("help"), CMD_HELP },
    { TEXT("--help"), CMD_HELP },
    { TEXT("-?"), CMD_HELP },
    { TEXT("/?"), CMD_HELP },
    { TEXT("/help"), CMD_HELP },
    { TEXT("install"), CMD_INSTALL },
    { TEXT("uninstall"), CMD_UNINSTALL },
    { TEXT("start"), CMD_START },
    { TEXT("stop"), CMD_STOP },
    { TEXT("pause"), CMD_PAUSE },
    { TEXT("continue"), CMD_CONTINUE },
    { TEXT("paramchange"), CMD_PARAMCHANGE },
    { TEXT("usercontrol"), CMD_USERCONTROL },
    { TEXT("restart"), CMD_RESTART },
    { TEXT("status"), CMD_STATUS },
    { TEXT("configure"), CMD_CONFIGURE },
    { TEXT("show"), CMD_SHOW },
    { TEXT("showconfig"), CMD_SHOW },
    { NULL, 0 }
};

static SC_HANDLE util_hScm, util_hSvc;
static LPTSTR util_Keypath = NULL, util_KeypathWs = NULL;
static LPTSTR util_KeypathEventLog = NULL, util_host = NULL;

WSLINKAGE void
util_mkregnames()
{
	if (!sv_name) return;

	util_Keypath =
	   str_printf(TEXT("System\\CurrentControlSet\\Services\\%s"), sv_name);
	util_KeypathWs =
	    str_printf(TEXT("System\\CurrentControlSet\\Services\\%s\\SvcMgr"),
	    sv_name);
	util_KeypathEventLog =
	    str_printf(TEXT
	    ("System\\CurrentControlSet\\Services\\eventlog\\Application\\%s"),
	    sv_name);
}

WSLINKAGE void
util_decompose_name(LPTSTR name)
{
	int     i;

	sv_name = name;
	if ((!name) || (name[0] != TEXT('\\')) || (name[1] != TEXT('\\'))) {
		return;
	}
	sv_name = name = _tcsdup(sv_name);
	for (i = 2; name[i] && (name[i] != TEXT('\\')); ++i);
	if (!name[i]) return;
	name[i] = 0;
	util_host = name;
	sv_name = name + i + 1;
}

/*
 * For each command line argument, finds a place in the command line
 * where it begins. Returns malloc'ed array of pointers to those places.
 * */
WSLINKAGE LPTSTR *
util_bind_argv(int org_argc, LPTSTR cmdLine)
{
	LPTSTR  p;
	LPTSTR *argv;
	int     argc, inquote, slashes;

	p = cmdLine;
	argv = calloc(org_argc + 2, sizeof(LPTSTR));
	for (argc = 0; argc < org_argc; argc++) {
		// skip spaces
		while (isspace(*p)) p++;
		if (*p == TEXT('\0')) break;
		inquote = 0;
		slashes = 0;
		// we found where the argument begins
		DBG(stderr, TEXT("argv[%d] is %s\n"), argc, p);
		argv[argc] = p;
		for (;;) {
			while (*p == TEXT('\\')) {
				slashes++;
				p++;
			}
			if (*p == TEXT('"')) {
				if ((slashes & 1) == 0) {
					if ((inquote) && (p[1] == TEXT('"'))) {
						p++;
					} else {
						inquote = !inquote;
					}
				}
				slashes >>= 1;
			}
			slashes = 0;
			if ((*p == TEXT('\0')) || (!inquote && isspace(*p))) {
				break;
			}
			p++;
		}
	}
	argv[argc] = NULL;
	return (argv);
}

WSLINKAGE void
util_validate_name()
{
	int     i;

	if (sv_name) {
		for (i = 0; sv_name[i]; ++i) {
			if (sv_name[i] == TEXT('\\')) sv_name[i] = TEXT('_');
		}
	}
}

WSLINKAGE BOOL
util_install(LPTSTR new_cmd_line)
{
	HKEY    wskey, svkey, evkey;
	DWORD   types_supported = 7;
	LPTSTR  ipcs[] =
	    { TEXT("blind"), TEXT("pipe"), TEXT("stdio"), TEXT("qstdio") };

	if (!new_cmd_line) {
		con_ftprintf(stderr, TEXT("No command line\n"));
		SetLastError(ERROR_SUCCESS);
		return (FALSE);
	}
	/*
	 * Useful defaults 
	 */
	if (!qsc.lpDisplayName) {
		qsc.lpDisplayName = (LPTSTR) calloc(1024, sizeof(TCHAR));
		_sntprintf(qsc.lpDisplayName, 1023,
		    TEXT("%s (managed by SvcMgr)"), sv_name);
	}
	util_hSvc = CreateService(util_hScm, sv_name,
	    qsc.lpDisplayName,
	    SERVICE_ALL_ACCESS,
	    (interactive ? SERVICE_INTERACTIVE_PROCESS : 0) |
	    SERVICE_WIN32_OWN_PROCESS,
	    qsc.dwStartType,
	    qsc.dwErrorControl,
	    qsc.lpBinaryPathName,
	    qsc.lpLoadOrderGroup, NULL, qsc.lpDependencies, user, passwd);
	if (!util_hSvc) return (FALSE);
	evkey =
	    reg_open(util_host, HKEY_LOCAL_MACHINE, util_KeypathEventLog,
	    O_CREAT);
	if (evkey) {
		RegSetValueEx(evkey, TEXT("EventMessageFile"), 0, REG_SZ,
		    (LPBYTE) qsc.lpBinaryPathName,
		    _tcslen(qsc.lpBinaryPathName) * sizeof(TCHAR));
		RegSetValueEx(evkey, TEXT("TypesSupported"), 0, REG_DWORD,
		    (LPBYTE) & types_supported, sizeof(DWORD));
		reg_close(evkey);
	}
	svkey = reg_open(util_host, HKEY_LOCAL_MACHINE, util_Keypath, O_RDWR);
	if (svkey) {
		DBG(stderr, TEXT("Opened registry svkey\n"));
		wskey = reg_open(NULL, svkey, TEXT("SvcMgr"), O_CREAT);
		if (wskey) {
			DBG(stderr, TEXT("Opened registry wskey\n"));
			if (new_cmd_line) {
				RegSetValueEx(wskey, TEXT("CommandLine"), 0,
				    expand ? REG_EXPAND_SZ : REG_SZ,
				    (LPBYTE) new_cmd_line,
				    _tcslen(new_cmd_line) * sizeof(TCHAR));
			}
			if (ipc_method != IPCMETHOD_NO_CHANGE) {
				RegSetValueEx(wskey, TEXT("IpcMethod"), 0,
				    REG_SZ, (LPBYTE) ipcs[ipc_method],
				    _tcslen(ipcs[ipc_method]) * sizeof(TCHAR));
			}
			reg_close(wskey);
		} else {
			return (FALSE);
		}
		if (description) {
			RegSetValueEx(svkey, TEXT("Description"), 0, REG_SZ,
			    (LPBYTE) description,
			    _tcslen(description) * sizeof(TCHAR));
		}
		reg_close(svkey);
	} else {
		return (FALSE);
	}
	return (TRUE);
}

WSLINKAGE BOOL
util_mustnotbeforeign(LPCTSTR msg)
{
	HKEY    wskey =
	    reg_open(util_host, HKEY_LOCAL_MACHINE, util_KeypathWs, O_RDONLY);
	if (!wskey) {
		if (force_foreign) {
			foreign = TRUE;
			return (TRUE);
		}
		con_ftprintf(stderr,
		    TEXT("%s: %s is not managed by SvcMgr.\n"
		    "Use -forceforeign to do it anyway.\n"), msg, sv_name);
		return (FALSE);
	} else {
		reg_close(wskey);
	}
	return (TRUE);
}

WSLINKAGE QUERY_SERVICE_CONFIG *
util_query_config()
{
	QUERY_SERVICE_CONFIG *tqsc;
	char    unuseful[4];
	DWORD   needed, needed2;

	QueryServiceConfig(util_hSvc, (LPVOID) unuseful, 1, &needed);
	if (GetLastError() != ERROR_INSUFFICIENT_BUFFER) return (NULL);
	tqsc = malloc(needed);
	if (!tqsc) return (NULL);
	if (!QueryServiceConfig(util_hSvc, tqsc, needed, &needed2)) {
		free(tqsc);
		return (NULL);
	} else {
		return (tqsc);
	}
}

WSLINKAGE BOOL
util_writeconf(LPCTSTR new_cmd_line)
{
	HKEY    wskey, svkey, evkey;
	BOOL    r;
	LPTSTR  ipcs[] =
	    { TEXT("blind"), TEXT("pipe"), TEXT("stdio"), TEXT("qstdio") };
	DWORD   svt = SERVICE_NO_CHANGE;
	QUERY_SERVICE_CONFIG *tqsc;

	if (new_cmd_line) {
		DBG(stderr, TEXT("command line trailer: %s\n"), new_cmd_line);
	}
	if (qsc.lpBinaryPathName || new_cmd_line) {
		if (!util_mustnotbeforeign(TEXT
			("Won't change -image or command line"))) {
			SetLastError(ERROR_SUCCESS);
			return (FALSE);
		}
	}
	DBG(stderr, TEXT("in writeconf()\n"));
	if (interactive != 2) {
		tqsc = util_query_config();
		if (!tqsc) return (FALSE);
		svt = tqsc->dwServiceType;
		free(tqsc);
		if (interactive) {
			svt |= SERVICE_INTERACTIVE_PROCESS;
		} else {
			svt &= ~SERVICE_INTERACTIVE_PROCESS;
		}
	}
	r = ChangeServiceConfig(util_hSvc, svt, qsc.dwStartType,
	    qsc.dwErrorControl, qsc.lpBinaryPathName, qsc.lpLoadOrderGroup,
	    NULL, qsc.lpDependencies, user, passwd, qsc.lpDisplayName);
	if (!r) return (FALSE);
	if (qsc.lpBinaryPathName && (!foreign)) {
		evkey =
		    reg_open(util_host, HKEY_LOCAL_MACHINE,
		    util_KeypathEventLog, O_RDWR);
		if (evkey) {
			RegSetValueEx(evkey, TEXT("EventMessageFile"), 0,
			    REG_SZ, (LPBYTE) qsc.lpBinaryPathName,
			    _tcslen(qsc.lpBinaryPathName) * sizeof(TCHAR));
			reg_close(evkey);
		}
	}
	svkey = reg_open(util_host, HKEY_LOCAL_MACHINE, util_Keypath, O_RDWR);
	if (svkey) {
		DBG(stderr, TEXT("Opened registry svkey\n"));
		wskey =
		    reg_open(NULL, svkey, TEXT("SvcMgr"),
		    force_foreign ? O_CREAT : O_RDWR);
		if (wskey) {
			DBG(stderr, TEXT("Opened registry wskey\n"));
			if (new_cmd_line) {
				RegSetValueEx(wskey, TEXT("CommandLine"), 0,
				    expand ? REG_EXPAND_SZ : REG_SZ,
				    (LPBYTE) new_cmd_line,
				    _tcslen(new_cmd_line) * sizeof(TCHAR));
			}
			if (ipc_method != IPCMETHOD_NO_CHANGE) {
				RegSetValueEx(wskey, TEXT("IpcMethod"), 0,
				    REG_SZ, (LPBYTE) ipcs[ipc_method],
				    _tcslen(ipcs[ipc_method]) * sizeof(TCHAR));
			}
			reg_close(wskey);
		}
	}
	if (description) DBG(stderr, TEXT("Description is %s\n"), description);
	if (description) {
		RegSetValueEx(svkey, TEXT("Description"), 0, REG_SZ,
		    (LPBYTE) description, _tcslen(description) * sizeof(TCHAR));
	}
	reg_close(svkey);
	return (TRUE);
}

WSLINKAGE BOOL
util_show()
{
	QUERY_SERVICE_CONFIG *tqsc;
	HKEY    svkey, wskey;
	DWORD   kt;
	LPTSTR  ipcm;
	LPTSTR  dptr;
	int     reg_unavailable = 0;

	tqsc = util_query_config();
	if (!tqsc) return (FALSE);

	con_ftprintf(stdout, TEXT("Configuration of %s:\n"), sv_name);
	if ((dptr = tqsc->lpDependencies)) {
		for (; dptr[0] || dptr[1]; ++dptr) {
			if (!dptr[0]) dptr[0] = TEXT(',');
		}
	}
	interactive =
	    ((tqsc->dwServiceType & SERVICE_INTERACTIVE_PROCESS) != 0);
	svkey = reg_open(util_host, HKEY_LOCAL_MACHINE, util_Keypath, O_RDONLY);
	if (svkey) {
		wskey = reg_open(NULL, svkey, TEXT("SvcMgr"), O_RDONLY);
		if (wskey) {
			reg_read(wskey, TEXT("IpcMethod"), (LPBYTE *) & ipcm,
			    FALSE);
			kt = reg_read(wskey, TEXT("CommandLine"),
			    (LPBYTE *) & command_line, FALSE);
			expand = (kt == REG_EXPAND_SZ);
			ipc_method = lookup_kw(ipcm, kw_ipcmethod);
			reg_close(wskey);
		} else {
			foreign = 1;
		}
		reg_read(svkey, TEXT("Description"), (LPBYTE *) & description,
		    FALSE);
		reg_close(svkey);
	} else {
		reg_unavailable = 1;
	}
	con_ftprintf(stdout, TEXT(
	    "-displayname %s\n"
	    "-%sinteractive\n"
	    "-binary %s\n"
	    "-start %s\n"
	    "-errorcontrol %s\n"
	    "-depends %s\n"
	    "-loadordergroup %s\n"
	    "-user %s\n"),
	    tqsc->lpDisplayName,
	    (interactive ? TEXT("") : TEXT("non")),
	    tqsc->lpBinaryPathName,
	    which_kw(tqsc->dwStartType, kw_starttype, TEXT("UNKNOWN")),
	    which_kw(tqsc->dwErrorControl, kw_errorcontrol, TEXT("UNKNOWN")),
	    tqsc->lpDependencies,
	    tqsc->lpLoadOrderGroup,
	    tqsc->lpServiceStartName);
	if (foreign) {
		con_ftprintf(stdout,
		    TEXT("Service %s is not managed by SvcMgr\n"), sv_name);
	} else {
		if (reg_unavailable) {
			con_ftprintf(stdout,
			    TEXT("WARNING: Can't open registry, "
			    "some information is unavailable\n"));
		} else {
			con_ftprintf(stdout, TEXT(
			    "-ipcmethod %s\n"
			    "*CommandLine %s\n"
			    "-%sexpand\n"),
			    which_kw(ipc_method, kw_ipcmethod, TEXT("UNKNOWN")),
			    command_line,
			    expand ? TEXT("") : TEXT("no"));
		}
	}
	if (description) {
		con_ftprintf(stdout, TEXT("  *Description: %s\n"), description);
	}
	return (TRUE);
}

WSLINKAGE int
util_main(int argc, LPTSTR * argv)
{
	LPTSTR *bound_argv;
	int     orig_argc = argc;
	LPVOID  error_msg;
	DWORD   err_length;
	DWORD   err;
	LPCTSTR cmdkw;
	int     optpr;
	DWORD   cmd, i;
	BOOL    isOk = TRUE;
	DWORD   ascm = 0, asvc = 0;
	TCHAR   image[4096];
	struct {
		DWORD   cmd, ascm, asvc;
		option_t **cmdo;
	} util_acl_map[] = {
	    { CMD_INSTALL, SC_MANAGER_CREATE_SERVICE, 0, cmdo_install },
	    { CMD_UNINSTALL, STANDARD_RIGHTS_READ, DELETE, cmdo_uninstall },
	    { CMD_START, STANDARD_RIGHTS_READ,
	      SERVICE_START | SERVICE_QUERY_STATUS, cmdo_start },
	    { CMD_STOP, STANDARD_RIGHTS_READ,
	      SERVICE_STOP | SERVICE_QUERY_STATUS, cmdo_stop },
	    { CMD_PAUSE, STANDARD_RIGHTS_READ,
	      SERVICE_PAUSE_CONTINUE | SERVICE_QUERY_STATUS, cmdo_pause },
	    { CMD_CONTINUE, STANDARD_RIGHTS_READ,
	      SERVICE_PAUSE_CONTINUE | SERVICE_QUERY_STATUS, cmdo_continue },
	    { CMD_PARAMCHANGE, STANDARD_RIGHTS_READ,
	      SERVICE_PAUSE_CONTINUE | SERVICE_QUERY_STATUS, cmdo_paramchange },
	    { CMD_USERCONTROL, STANDARD_RIGHTS_READ,
	      SERVICE_USER_DEFINED_CONTROL, cmdo_usercontrol },
	    { CMD_RESTART, STANDARD_RIGHTS_READ,
	      SERVICE_STOP | SERVICE_START | SERVICE_QUERY_STATUS,
	      cmdo_restart },
	    { CMD_STATUS,
	      STANDARD_RIGHTS_READ, SERVICE_QUERY_STATUS, cmdo_status },
	    { CMD_CONFIGURE, STANDARD_RIGHTS_READ,
	      SERVICE_CHANGE_CONFIG | SERVICE_QUERY_CONFIG, cmdo_configure },
	    { CMD_SHOW, STANDARD_RIGHTS_READ, SERVICE_QUERY_CONFIG, cmdo_show },
	    { 0, 0, 0, 0 }
	};

	setlocale(LC_ALL, "C");
	bound_argv = util_bind_argv(argc, _tcsdup(GetCommandLine()));
	setlocale(LC_ALL, "");
	GetModuleFileName(NULL, image, 4095);
	/*
	 * defaults 
	 */
	qsc.dwStartType = SERVICE_DEMAND_START;
	qsc.dwErrorControl = SERVICE_ERROR_NORMAL;
	qsc.lpBinaryPathName = image;

	cmdkw = argv[1];
	cmd = lookup_kw(cmdkw, kw_commands);
	argv += 2;
	argc -= 2;
	if (argc) {
		util_decompose_name(*argv++);
		util_validate_name();
		--argc;
		util_mkregnames();
	}
	switch (cmd) {
	    case 0:
		_ftprintf(stderr, TEXT("Bad command %s\n"), cmdkw);
		return (1);
	    case CMD_HELP:
		_tprintf(TEXT("%S"), USAGE);
		return (0);
	}

	if (!sv_name) {
		_tprintf(TEXT("No service name given\n"));
		return (1);
	}
	for (i = 0; util_acl_map[i].cmd; ++i) {
		if (util_acl_map[i].cmd == cmd) {
			ascm = util_acl_map[i].ascm;
			asvc = util_acl_map[i].asvc;
			break;
		}
	}
	if (cmd == CMD_CONFIGURE) {
		qsc.dwServiceType =
		    qsc.dwStartType = qsc.dwErrorControl = SERVICE_NO_CHANGE;
		qsc.lpBinaryPathName = NULL;
		interactive = 2;
		ipc_method = IPCMETHOD_NO_CHANGE;
	}
	optpr = parse_options(&argc, &argv, util_acl_map[i].cmdo);
	switch (optpr) {
	    case OPT_INSUFFICIENT:
		_tprintf(TEXT("Option %s needs an argument\n"), argv[1]);
	    case OPT_BADFMT:
		return (1);
	}
	util_hScm = OpenSCManager(util_host, NULL, ascm);
	if (!util_hScm) {
		isOk = FALSE;
		goto reportError;
	}
	if (cmd != CMD_INSTALL) {
		util_hSvc = OpenService(util_hScm, sv_name, asvc);
		if (!util_hSvc) {
			isOk = FALSE;
			goto reportError;
		}
	}
	if (argc && (cmd != CMD_RESTART) && (cmd != CMD_START) &&
	    (cmd != CMD_INSTALL)
	    && (cmd != CMD_CONFIGURE) && (cmd != CMD_STATUS) &&
	    (cmd != CMD_SHOW)) {
		_tprintf(TEXT("Unrecognized option %s\n"), *argv);
		return (1);
	}

	switch (cmd) {
	    case CMD_CONFIGURE:
		DBG(stderr, TEXT("Taking bound_argv[%d]\n"),
		    orig_argc - argc);
		isOk = util_writeconf(bound_argv[orig_argc - argc]);
		break;
	    case CMD_START:
		isOk = StartService(util_hSvc, argc, (LPCTSTR *) argv);
		break;
	    case CMD_RESTART:
		nowait = 0;
	    case CMD_STOP:
		isOk =
		    ControlService(util_hSvc, SERVICE_CONTROL_STOP, &sv_status);
		    if (!isOk) goto reportError;
		    if (!nowait) {
			    while (sv_status.dwCurrentState != SERVICE_STOPPED){
				    Sleep(sv_status.dwWaitHint >
					1000 ? 1000 : sv_status.dwWaitHint);
				    isOk =
					QueryServiceStatus(util_hSvc,
					&sv_status);
				    if (!isOk) goto reportError;
			    }
		    }
		    break;
	    case CMD_PAUSE:
		isOk =
		    ControlService(util_hSvc, SERVICE_CONTROL_PAUSE,&sv_status);
		if (!isOk) goto reportError;
		if (!nowait) {
			while (sv_status.dwCurrentState != SERVICE_PAUSED) {
				Sleep(sv_status.dwWaitHint >
				    1000 ? 1000 : sv_status.dwWaitHint);
				isOk = QueryServiceStatus(util_hSvc,&sv_status);
				if (!isOk) goto reportError;
			}
		}
		break;
	    case CMD_CONTINUE:
		isOk = ControlService(util_hSvc, SERVICE_CONTROL_CONTINUE,
		    &sv_status);
		if (!isOk) goto reportError;
		if (!nowait) {
			while (sv_status.dwCurrentState != SERVICE_RUNNING) {
				Sleep(sv_status.dwWaitHint >
				    1000 ? 1000 : sv_status.dwWaitHint);
				isOk = QueryServiceStatus(util_hSvc,&sv_status);
				if (!isOk) goto reportError;
			}
		}
		break;
	    case CMD_STATUS:
		for (;;) {
			TCHAR	*p;

			isOk = QueryServiceStatus(util_hSvc, &sv_status);
			if (!isOk) {
				isOk = FALSE;
				goto reportError;
			}
			_tprintf(TEXT("Service:   "));
			p = sv_name;
			if (_tcsncmp(p, _T("BK."), 3) == 0) p += 3;
			_tprintf(TEXT("%s\nStatus:    "), p);
			switch (sv_status.dwCurrentState) {
			    case SERVICE_RUNNING:
				_tprintf(TEXT("RUNNING\n"));
				break;
			    case SERVICE_STOPPED:
				_tprintf(TEXT("STOPPED\n"));
				break;
			    case SERVICE_PAUSED:
				_tprintf(TEXT("PAUSED\n"));
				break;
			    case SERVICE_CONTINUE_PENDING:
				_tprintf(TEXT("CONTINUE PENDING\n"));
				break;
			    case SERVICE_START_PENDING:
				_tprintf(TEXT("START PENDING\n"));
				break;
			    case SERVICE_STOP_PENDING:
				_tprintf(TEXT("STOP PENDING\n"));
				break;
			    case SERVICE_PAUSE_PENDING:
				_tprintf(TEXT("PAUSE PENDING\n"));
				break;
			    default:
				_tprintf(TEXT("Status unknown: %u\n"),
				    sv_status.dwCurrentState);
				break;
			}
			_tprintf(TEXT("Accepts:  "));
			if (sv_status.dwControlsAccepted &
			    SERVICE_ACCEPT_STOP) {
				_tprintf(TEXT(" STOP"));
			}
			if (sv_status.dwControlsAccepted &
			    SERVICE_ACCEPT_PAUSE_CONTINUE) {
				_tprintf(TEXT(" PAUSE CONTINUE"));
			}
			if (sv_status.dwControlsAccepted &
			    SERVICE_ACCEPT_SHUTDOWN) {
				_tprintf(TEXT(" SHUTDOWN"));
			}
			if (sv_status.dwControlsAccepted &
			    SERVICE_ACCEPT_PARAMCHANGE) {
				_tprintf(TEXT(" PARAMCHANGE"));
			}
			if (sv_status.dwControlsAccepted &
			    SERVICE_ACCEPT_NETBINDCHANGE) {
				_tprintf(TEXT(" NETBINDADD NETBINDREMOVE "
				    "NETBINDENABLE NETBINDDISABLE"));
			}
			_tprintf(TEXT("\n"));
			if (argc <= 0) break;

			CloseServiceHandle(util_hSvc);
			sv_name = *argv++;
			--argc;
			util_hSvc = OpenService(util_hScm, sv_name, asvc);
			if (!util_hSvc) {
				isOk = FALSE;
				goto reportError;
			}
		};
		break;
	    case CMD_UNINSTALL:
		isOk = DeleteService(util_hSvc);
		break;
	    case CMD_INSTALL:
		isOk = util_install(bound_argv[orig_argc - argc]);
		break;
	    case CMD_PARAMCHANGE:
	    case CMD_USERCONTROL:
		control_code = (cmd == CMD_USERCONTROL ? control_code :
		    SERVICE_CONTROL_PARAMCHANGE);
		isOk = ControlService(util_hSvc, control_code, &sv_status);
		break;
	    case CMD_SHOW:
		for (;;) {
			isOk = util_show();
			if (!isOk) goto reportError;
			if (!argc) break;
			CloseServiceHandle(util_hSvc);
			sv_name = *argv++;
			--argc;
			util_hSvc = OpenService(util_hScm, sv_name, asvc);
			if (!util_hSvc) {
				isOk = FALSE;
				goto reportError;
			}
		}
		break;
	}
	if ((cmd == CMD_RESTART) && isOk) {
		isOk = StartService(util_hSvc, argc, (LPCTSTR *) argv);
	}
	err = GetLastError();
	CloseServiceHandle(util_hSvc);
	CloseServiceHandle(util_hScm);
	SetLastError(err);
reportError:
	if (!isOk) {
		err = GetLastError();
		if (err != ERROR_SUCCESS) {
			err_length =
			    FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER |
			    FORMAT_MESSAGE_FROM_SYSTEM |
			    FORMAT_MESSAGE_IGNORE_INSERTS, NULL, err,
			    LANG_NEUTRAL, (LPTSTR) & error_msg, 0, NULL);
			if (err_length) {
				con_ftprintf(stderr, TEXT("ERROR %d: %s"), err,
				    error_msg);
			}
			return (err);
		}
		return (1);
	}
	return (0);
}
