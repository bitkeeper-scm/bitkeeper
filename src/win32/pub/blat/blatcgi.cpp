// CGI Support by Gilles Vollant <info@winimage.com>, February 1999
// Added in Blat Version 1.8.3

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <time.h>

#ifdef WIN32
	#define __far far
	#define huge far
	#define __near near
#endif

/***************************************************************************/
// begin of CGI stuff
/***************************************************************************/

/*
The BLAT CGI accept both GET and FORM CGI usage
Usage :

1)
  call from Web server
	 http://server/scripts/blat.exe/linecommand.txt

	 http://linecommand.txt must be a signe line text file, with the BLAT command line, like (without the '"')
	 "- -t myname@mydomain.com -server smtp.mydomain.com -f myname@mydomain.com"

2)
	 http://server/scripts/blat.exe
	 Your HTTP request must contain somes var with the command line.:
		  TO :            the -t parameters of Blat
		  CC :            the -c parameters of Blat
		  BCC :           the -b parameters of Blat
		  SENDER :        the -f parameters of Blat
		  FROM :          the -i parameters of Blat
		  ORGANISATION :  the -o parameters of Blat
		  SERVER :        the -server parameters of Blat
		  SUBJECT :       the -s parameters of Blat
		  PORT :          the -port parameters of Blat
		  HOSTNAME :      the -hostname parameters of Blat
		  TIMEOUT :       the -ti parameter of Blat
		  XHEADER :       the -xheader parameter of Blat
	 These Variable are Boolean (if present and set TO "1" or "Y", added the option,
										  if not esent or set TO "N" or "0", remove the option)

		  NOH :           the -noh parameters of Blat
		  NOH2 :          the -noh2 parameters of Blat
		  MIME :          the -mime parameters of Blat
		  UUENCODE :      the -uuencode parameters of Blat
		  BASE64 :        the -base64 parameters of Blat
		  REQUESTRECEIPT :the -r parameter of Blat
		  NOTIFY :        the -d parameter of Blat

You can prefix these var name bu "Blat_" (ie using BLAT_SUBJECT instead SUBJECT) if you don't want see
 the variable content in the message.

Blat_success and Blat_failure will contain the URL for success and failure message

Example of HTML page using blat

<HTML><BODY>
GET test:<br>

<FORM ACTION="/scripts/blat.exe" METHOD="GET">
Enter here your email
	 <INPUT TYPE="text" SIZE="20" NAME="From" VALUE="myname@mydomain.com">
<br>
	 <INPUT TYPE="text" SIZE="20" NAME="TO" VALUE="myname2@winimage.com">
<br>
	 <INPUT TYPE="text" SIZE="20" NAME="Blat_Subject" VALUE="le sujet est 'quot' !">
<br>
	 <INPUT TYPE="text" SIZE="20" NAME="Var2" VALUE="Var_2">
<br>
	 <INPUT TYPE="text" SIZE="20" NAME="Var3" VALUE="Var_3">
<br>
	 <INPUT TYPE="hidden" NAME="Blat_success" VALUE="http://localhost/good.htm">
	 <INPUT TYPE="hidden" NAME="Blat_failure" VALUE="http://localhost/bad.htm">
	 <INPUT TYPE="hidden" NAME="Blat_Mime" VALUE="Y">
	 <INPUT TYPE="submit" VALUE="submit"><BR>
</form></BODY></HTML>

TODO :
 - improve documentation

*/
typedef struct {
	const char * szEntry;
	const char * szBlatOption;
	BOOL  fWithParameter;
} BLATVAROPTION;

const BLATVAROPTION bvo[] = {
	{"TO","-t",TRUE},
	{"CC","-c",TRUE},
	{"BCC","-b",TRUE},
	{"SENDER","-f",TRUE},
	{"FROM","-i",TRUE},
	{"ORGANISATION","-o",TRUE},
	{"SERVER","-server",TRUE},
//    {"PROFILE","-p",TRUE},  // removed for security consideration (-p
	{"SUBJECT","-s",TRUE},
	{"PORT","-port",TRUE},
	{"HOSTNAME","-hostname",TRUE},
	{"TIMEOUT","-ti",TRUE},
	{"XHEADER","-x",TRUE},
	{"NOH","-noh",FALSE},
	{"NOH2","-noh2",FALSE},
	{"MIME","-mime",FALSE},
	{"UUENCODE","-uuencode",FALSE},
	{"BASE64","-base64",FALSE},
	{"REQUESTRECEIPT","-r",FALSE},
	{"NOTIFY","-d",FALSE},
	{NULL,NULL,FALSE},};

BOOL GetFileSize(LPCSTR lpFn,DWORD &dwSize)
{
	WIN32_FIND_DATA ffblk;
	HANDLE hFind;
	char szName[MAX_PATH+1];


	lstrcpy(szName,lpFn);
	dwSize = 0;

	if ( (hFind = FindFirstFile(szName,&ffblk)) == INVALID_HANDLE_VALUE )
		return FALSE;
	else {
		dwSize = ffblk.nFileSizeLow;
	}
	FindClose(hFind);
	return TRUE;
}

//
// Works like _getenv(), but uses win32 functions instead.
//
char * GetEnv(LPSTR lpszEnvVar) {

	char *var,dummy;
	DWORD dwLen;

#ifdef DEBUGCGI
	{
		char *szT="From=myname@mydomain.com&TO=myname2@winimage.com&Blat_Subject=le+sujet+CGI+DEBUG+est+%27quot%27+%21+%22rr%22r%22&Var2=Var_2&Var3=Var_3&TEXT=-+-f+%25FromEmail%25+-t+info@winimage.com+-s+Blat_Cgi_Test";
		if ( lstrcmpi(lpszEnvVar,"QUERY_STRING")==0 ) {
			char*var=(char*)malloc(lstrlen(szT)+10);
			lstrcpy(var,szT);
			return var;
		}
	}
#endif
	if ( !lpszEnvVar ) {
		var = (char*)malloc(1);
		*var='\0';
		return var;
	}

	dwLen =GetEnvironmentVariable(lpszEnvVar,&dummy,1);

	if ( dwLen == 0 ) {
		var = (char*)malloc(dwLen+1);
		*var='\0';
	} else {
		var = (char*)malloc(dwLen+2);
		if ( !var ) {
			var=(char*)malloc(1);
			*var='\0';
		} else {
			(void)GetEnvironmentVariable(lpszEnvVar,var,dwLen+1);
		}
	}

	return var;
}

BOOL AddStringBuffer(LPSTR &lpAnswer,DWORD &dwSizeAnswer,LPCSTR lpSrc)
{
	BOOL fRet;

	if ( lpSrc == NULL )
		lpSrc = "";

	DWORD dwIln = strlen(lpSrc);
	LPSTR lpNewAnswer;
	if ( lpAnswer == NULL )
		lpNewAnswer = (LPSTR)malloc(dwSizeAnswer+dwIln+0x10);
	else
		lpNewAnswer	= (LPSTR)realloc(lpAnswer,dwSizeAnswer+dwIln+0x10);

	fRet = (lpNewAnswer!=NULL);
	if ( fRet ) {
		lpAnswer=lpNewAnswer;
		strcpy(lpAnswer+dwSizeAnswer,lpSrc);
		dwSizeAnswer+=dwIln;
	}
	return fRet;
}


char* ReadPostData()
{
	DWORD dwData=0;
	DWORD dwStep=4096;
	DWORD dwReadThis,dwThisStep;
	DWORD dwTotalBytes;
	LPSTR lpszContentLenght = GetEnv("CONTENT_LENGTH");
	dwTotalBytes=atol(lpszContentLenght);
	free(lpszContentLenght);
	char *pszRep = (char*)malloc(0x10);
	dwData = 0;

	do {
		dwThisStep = min(dwTotalBytes-dwData,dwStep);
		dwReadThis=dwThisStep;
		pszRep=(char*)realloc(pszRep,dwData + dwStep + 0x10);
		if ( dwThisStep>0 ) {
			if ( !ReadFile(GetStdHandle(STD_INPUT_HANDLE),pszRep+dwData,dwReadThis,
								&dwReadThis,NULL) )
				dwReadThis=0;
		}
		dwData+=dwReadThis;

	} while ( (dwReadThis==dwThisStep) && (dwData<dwTotalBytes) );

	*(pszRep+dwData)='\0';

	return pszRep;
}

static int hextoint( char c )
{
	if ( c >= '0' && c <= '9' ) return c-'0';
	else								 return toupper(c)-'A'+10;
}

static void url_decode( char * cp )
{

	for ( ; *cp; cp++ ) {
		if ( *cp == '+' )	*cp = ' ';
		else
			if ( *cp == '%' ) {
			*cp = hextoint(*(cp+1)) * 16 + hextoint(*(cp+2));
			memmove( cp+1, cp+3, strlen(cp+3)+1 );
		}
	}
}

DWORD SearchNextPos(LPCSTR lpszParamCgi,BOOL fSearchEqual)
{
	char cSup;
	DWORD dwNext=0;
	if ( fSearchEqual )
		cSup='=';
	else
		cSup='\0';
	while ( (*(lpszParamCgi+dwNext)!='&') && (*(lpszParamCgi+dwNext)!='\0') && (*(lpszParamCgi+dwNext)!=cSup) )
		dwNext++;
	return dwNext;
}

DWORD SearchNextPercent(LPCSTR lpszParamCgi)
{
	DWORD dwNext=0;

	while ( (*(lpszParamCgi+dwNext)!='%') && (*(lpszParamCgi+dwNext)!='\0') )
		dwNext++;
	return dwNext;
}

LPSTR SearchVar(LPCSTR lpszParamCgi,LPCSTR lpszVarName,BOOL fMimeDecode)
{
	LPSTR lpAlloc,lpszProvCmp,lpszVarForCmp;
	DWORD dwVarNameLen=lstrlen(lpszVarName);
	DWORD dwLineLen=lstrlen(lpszParamCgi);
	DWORD dwPos=0;
	LPSTR lpContent=NULL;

	lpAlloc = (LPSTR)malloc((dwVarNameLen+0x10)*2);
	lpszProvCmp=lpAlloc;
	lpszVarForCmp=lpAlloc+(((dwVarNameLen+7)/4)*4);
	lstrcpy(lpszVarForCmp,lpszVarName);
	lstrcat(lpszVarForCmp,"=");
	*(lpszProvCmp+dwVarNameLen+1)='\0';

	while ( dwPos<dwLineLen ) {
		DWORD dwNextPos=SearchNextPos(lpszParamCgi+dwPos,FALSE);
		if ( dwPos+dwVarNameLen>=dwLineLen )
			break;
		memcpy(lpszProvCmp,lpszParamCgi+dwPos,dwVarNameLen+1);
		if ( lstrcmpi(lpszProvCmp,lpszVarForCmp)==0 ) {
			DWORD dwLenContent=dwNextPos-(dwVarNameLen+1);
			lpContent=(LPSTR)malloc(dwLenContent+0x10);
			memcpy(lpContent,lpszParamCgi+dwPos+dwVarNameLen+1,dwLenContent);
			*(lpContent+dwLenContent)='\0';
			if ( fMimeDecode )
				url_decode(lpContent);
			break;
		}
		dwPos+=dwNextPos+1;
	}

	free(lpAlloc);

	return lpContent;
}

BOOL BuildMessageAndCmdLine(LPCSTR lpszParamCgi,LPCSTR lpszPathTranslated,LPSTR &lpszCmdBlat,LPSTR &lpszMessage)
{
	DWORD dwSize=0;
	LPSTR lpszParamFile=NULL;
	lpszMessage=NULL;
	DWORD dwMessageLen=0;

	if ( lstrlen(lpszPathTranslated)>0 )
		if ( GetFileSize(lpszPathTranslated,dwSize) )
			if ( dwSize>0 ) {
				HANDLE hf;
				if ( (hf = CreateFile(lpszPathTranslated,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,
											 FILE_FLAG_SEQUENTIAL_SCAN,NULL))!=NULL ) {
					DWORD dwSizeRead=0;
					lpszParamFile=(char*)malloc(dwSize+10);
					ReadFile(hf,lpszParamFile,dwSize,&dwSizeRead,NULL);
					*(lpszParamFile+dwSizeRead)='\0';
					CloseHandle(hf);
				}
			}

	if ( lpszParamFile==NULL ) {
		lpszParamFile=(LPSTR)malloc(1);
		*lpszParamFile='\0';
	}

	if ( lstrlen(lpszParamFile)==0 ) {
		LPSTR lpszNewParamFile=NULL;
		DWORD dwNewParamFileSize=0;
		AddStringBuffer(lpszNewParamFile,dwNewParamFileSize,"-");
		const BLATVAROPTION* pbvo=bvo;

		while ( pbvo->szBlatOption!=NULL ) {
			LPSTR lpszValue=NULL;
			char szVarNamePrefixed[128];


			wsprintf(szVarNamePrefixed,"Blat_%s",pbvo->szEntry);
			lpszValue=SearchVar(lpszParamCgi,pbvo->szEntry,TRUE);
			if ( lpszValue==NULL )
				lpszValue=SearchVar(lpszParamCgi,szVarNamePrefixed,TRUE);
			//printf("search '%s'='%s' : %s\n",pbvo->szEntry,szVarNamePrefixed,(lpszValue==NULL)?"N":"Y");
			if ( lpszValue!=NULL ) {
				DWORD dwLen=lstrlen(lpszValue);
				DWORD i;
				for ( i=0;i<dwLen;i++ )
					if ( (*(lpszValue+i))=='"' )
						(*(lpszValue+i))='\'';			 // to avoid security problem, like including other parameter like -attach
				if ( pbvo->fWithParameter ) {
					AddStringBuffer(lpszNewParamFile,dwNewParamFileSize," ");
					AddStringBuffer(lpszNewParamFile,dwNewParamFileSize,pbvo->szBlatOption);
					AddStringBuffer(lpszNewParamFile,dwNewParamFileSize," \"");
					AddStringBuffer(lpszNewParamFile,dwNewParamFileSize,lpszValue);
					AddStringBuffer(lpszNewParamFile,dwNewParamFileSize,"\"");
				} else {
					if ( dwLen>0 )
						if ( ((*lpszValue)!='N') && ((*lpszValue)!='n') && ((*lpszValue)!='0') ) {
							AddStringBuffer(lpszNewParamFile,dwNewParamFileSize," ");
							AddStringBuffer(lpszNewParamFile,dwNewParamFileSize,pbvo->szBlatOption);
						}
				}
				free(lpszValue);
			}

			pbvo++;
		}

		free(lpszParamFile);
		lpszParamFile=lpszNewParamFile;
	}
	lpszCmdBlat=lpszParamFile;

	//printf("command : \"%s\"\n",lpszCmdBlat);

	AddStringBuffer(lpszMessage,dwMessageLen,"");
	{
		DWORD dwPos=0;
		DWORD dwLineLen=lstrlen(lpszParamCgi);
		while ( dwPos<dwLineLen ) {
			char szNameForCmp[7];
			DWORD dwEndVar=SearchNextPos(lpszParamCgi+dwPos,FALSE);
			DWORD dwEndVarName=SearchNextPos(lpszParamCgi+dwPos,TRUE);
			BOOL fCopyCurrentVar=TRUE;
			if ( dwEndVarName>5 ) {
				memcpy(szNameForCmp,lpszParamCgi+dwPos,5);
				szNameForCmp[5]='\0';
				fCopyCurrentVar = (lstrcmpi(szNameForCmp,"Blat_")!=0);
			}
			if ( fCopyCurrentVar ) {
				LPSTR lpCurVar=(LPSTR)malloc(dwEndVar+0x10);
				memcpy(lpCurVar,lpszParamCgi+dwPos,dwEndVar);
				*(lpCurVar+dwEndVar)='\0';
				url_decode(lpCurVar);
				AddStringBuffer(lpszMessage,dwMessageLen,lpCurVar);
				AddStringBuffer(lpszMessage,dwMessageLen,"\x0d\x0a");
				free(lpCurVar);
			}
			dwPos+=dwEndVar+1;

		}
	}

	return TRUE;
}

DWORD WINAPI ReadCommandLine(LPSTR szParcLine,int & argc, char** &argv)
{

	DWORD dwCurLigne,dwCurCol;
	BOOL fInQuote = FALSE;
	//DWORD nNb;
	dwCurLigne = 1;
	dwCurCol = 0;
	DWORD dwCurLigAllocated=0x10;
	LPSTR lpszOldArgv=*argv;							 // exe name

	argv=(char**)malloc(sizeof(char*)*3);
	*argv=(char*)malloc(lstrlen(lpszOldArgv)+10);
	lstrcpy(*argv,lpszOldArgv);
	*(argv+1)=(char*)malloc(dwCurLigAllocated+10);
	*(argv+2)=NULL;
	**(argv+1)='\0';



	while ( ((*szParcLine) != '\0') && ((*szParcLine) != '\x0a') && ((*szParcLine) != '\x0d') ) {
		char c=(*szParcLine);
		if ( c=='"' )
			fInQuote = ! fInQuote;
		else
			if ( (c==' ') && (!fInQuote) ) {			 // && (dwCurLigne+1<MAXPARAM))
			argv=(char**)realloc(argv,sizeof(char*)*(dwCurLigne+0x10));
			dwCurLigne++;
			dwCurLigAllocated=0x10;
			*(argv+dwCurLigne)=(char*)malloc(dwCurLigAllocated+10);
			*(argv+dwCurLigne+1)=NULL;

			dwCurCol=0;
		} else {
			char * lpszCurLigne;
			if ( dwCurCol>=dwCurLigAllocated ) {
				dwCurLigAllocated+=0x20;
				*(argv+dwCurLigne)=(char*)realloc(*(argv+dwCurLigne),dwCurLigAllocated+10);
			}
			lpszCurLigne=*(argv+dwCurLigne);
			*(lpszCurLigne+dwCurCol)=c;
			dwCurCol++;
			*(lpszCurLigne+dwCurCol)='\0';
		}
		szParcLine++;
	}
	if ( dwCurCol>0 )
		dwCurLigne++;
	argc=dwCurLigne;
	return dwCurLigne;
}


BOOL DoCgiWork(int & argc, char**  &argv,LPSTR &lpszMessage,
					LPSTR& lpszCgiSuccessUrl,LPSTR &lpszCgiFailureUrl,
					LPSTR& lpszFirstReceivedData,LPSTR &lpszOtherHeader)
{
	LPSTR lpszMethod;
	LPSTR lpszPost=NULL;
	LPSTR lpszParamCgi;
	LPSTR lpszQueryString;
	LPSTR lpszPathTranslated;
	LPSTR lpszCmdBlat=NULL;
	lpszMessage=NULL;

	lpszMethod = GetEnv("REQUEST_METHOD");
	if ( lstrcmpi(lpszMethod,"POST") )
		lpszPost=ReadPostData();
	else {
		lpszPost=(LPSTR)malloc(1);
		*lpszPost='\0';
	}
	free(lpszMethod);
	lpszQueryString=GetEnv("QUERY_STRING");
	lpszParamCgi=(LPSTR)malloc(lstrlen(lpszQueryString)+lstrlen(lpszPost)+10);
	if ( (lstrlen(lpszQueryString)>0) && (lstrlen(lpszPost)>0) )
		wsprintf(lpszParamCgi,"%s&%s",lpszQueryString,lpszPost);
	else
		wsprintf(lpszParamCgi,"%s%s",lpszQueryString,lpszPost);

	free(lpszQueryString);
	lpszQueryString=NULL;
	free(lpszPost);
	lpszPost=NULL;

	lpszPathTranslated=GetEnv("PATH_TRANSLATED");


	BuildMessageAndCmdLine(lpszParamCgi,lpszPathTranslated,lpszCmdBlat,lpszMessage);
	free(lpszPathTranslated);

	lpszCgiSuccessUrl=SearchVar(lpszParamCgi,"BLAT_SUCCESS",TRUE);
	lpszCgiFailureUrl=SearchVar(lpszParamCgi,"BLAT_FAILURE",TRUE);

	// now replace %__% by var
	{
		DWORD dwPos=0;
		DWORD dwLineLen=lstrlen(lpszCmdBlat);
		while ( dwPos<dwLineLen ) {
			if ( *(lpszCmdBlat+dwPos)=='%' ) {
				LPSTR lpVarNameForSearch;
				LPSTR lpContentVar;
				DWORD dwEnd=(SearchNextPercent(lpszCmdBlat+dwPos+1));
				if ( *(lpszCmdBlat+dwPos+1+dwEnd)=='\0' )
					break;
				lpVarNameForSearch=(LPSTR)malloc(dwEnd+0x10);
				memcpy(lpVarNameForSearch,lpszCmdBlat+dwPos+1,dwEnd);
				*(lpVarNameForSearch+dwEnd)='\0';

				lpContentVar=SearchVar(lpszParamCgi,lpVarNameForSearch,TRUE);
				if ( lpContentVar!=NULL ) {
					DWORD dwLenContentVar=lstrlen(lpContentVar);
					lpszCmdBlat=(LPSTR)realloc(lpszCmdBlat,dwLineLen+dwLenContentVar+0x10);
					memmove(lpszCmdBlat+dwPos+dwLenContentVar,lpszCmdBlat+dwPos+dwEnd+2,dwLineLen-(dwPos+dwEnd+1));
					memcpy(lpszCmdBlat+dwPos,lpContentVar,dwLenContentVar);
					dwLineLen=lstrlen(lpszCmdBlat);

					dwPos+=dwLenContentVar;
				} else
					dwPos+=dwEnd+1;


			} else dwPos++;
		}
	}

	ReadCommandLine(lpszCmdBlat,argc,argv);
	free(lpszCmdBlat);

	free(lpszParamCgi);
	{
		LPSTR lpszRemoteAddr=GetEnv("REMOTE_ADDR");
		//LPSTR lpszRemoteHost=GetEnv("REMOTE_HOST");
		LPSTR lpszServerName=GetEnv("SERVER_NAME");
		LPSTR lpszHttpVia=GetEnv("HTTP_VIA");
		LPSTR lpszHttpForwarded=GetEnv("HTTP_FORWARDED");
		LPSTR lpszHttpForwardedFor=GetEnv("HTTP_X_FORWARDED_FOR");
		LPSTR lpszHttpUserAgent = GetEnv("HTTP_USER_AGENT");
		LPSTR lpszHttpReferer = GetEnv("HTTP_REFERER");


		{
			lpszOtherHeader=(char*)malloc(lstrlen(lpszHttpUserAgent)+
													lstrlen(lpszHttpForwarded)+
													lstrlen(lpszHttpForwardedFor)+
													lstrlen(lpszHttpVia)+256);

			if ( (*lpszHttpUserAgent)!='\0' )
				wsprintf(lpszOtherHeader,"X-Web-Browser: Send using %s\r\n",lpszHttpUserAgent);
			else
				*lpszOtherHeader='\0';

			if ( (*lpszHttpForwarded)!='\0' )
				wsprintf(lpszOtherHeader+lstrlen(lpszOtherHeader),
							"X-Forwarded: %s\r\n",lpszHttpForwarded);

			if ( (*lpszHttpForwardedFor)!='\0' )
				wsprintf(lpszOtherHeader+lstrlen(lpszOtherHeader),
							"X-X-Forwarded-For: %s\r\n",lpszHttpForwardedFor);

			if ( (*lpszHttpVia)!='\0' )
				wsprintf(lpszOtherHeader+lstrlen(lpszOtherHeader),
							"X-Via: %s\r\n",lpszHttpVia);

			if ( (*lpszHttpReferer)!='\0' )
				wsprintf(lpszOtherHeader+lstrlen(lpszOtherHeader),
							"X-Referer: %s\r\n",lpszHttpReferer);

		}

		lpszFirstReceivedData=(char*)malloc(lstrlen(lpszRemoteAddr)+
														//lstrlen(lpszRemoteHost)+
														lstrlen(lpszServerName)+
														128);
		wsprintf(lpszFirstReceivedData,"Received: from %s by %s with HTTP; ",lpszRemoteAddr,lpszServerName);

		free(lpszRemoteAddr);
		//free(lpszRemoteHost);
		free(lpszServerName);
		free(lpszHttpForwardedFor);
		free(lpszHttpVia);
		free(lpszHttpUserAgent);
		free(lpszHttpReferer);
		free(lpszHttpForwarded);
	}

	/***
	{   // This very ugly code add all the server variable to the end of message. Only to help debug
		 #ifdef DEBUGCGI
		 lpszMessage = (char*)realloc(lpszMessage,lstrlen(lpszMessage)+65000);
		 LPSTR lpszCur=(LPSTR)GetEnvironmentStrings();
		 lstrcat(lpszMessage,"\r\n---\r\n");
		 LPSTR lpszWrite=lpszMessage+lstrlen(lpszMessage);
		 for (LPSTR lpszVariable = lpszCur; *lpszVariable; lpszVariable++)
		 { 
			  while (*lpszVariable)
					*(lpszWrite++)=(*lpszVariable++);
			  *(lpszWrite++)=0x0d;
			  *(lpszWrite++)=0x0a;
		 } 
		 *(lpszWrite++)=0x0;
		 #endif
	}
	****/
	return TRUE;
}

/***************************************************************************/
// end of CGI stuff
/***************************************************************************/

