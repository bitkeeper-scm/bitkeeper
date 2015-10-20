#include "stdafx.h"
#include "BkShellX.h"
#include "BkRootIcon.h"
#include "system.h"

STDMETHODIMP
CBkRootIcon::GetOverlayInfo(LPWSTR pwszIconFile, int cchMax,
    int *pIndex, DWORD *pdwFlags)
{
	cchMax -= MultiByteToWideChar(CP_ACP, NULL, bkdir, -1,
	    pwszIconFile, cchMax) + 1;
	wcsncat_s(pwszIconFile, cchMax, L"\\icons\\BkRoot.ico", cchMax);
	*pIndex = 0;
	*pdwFlags = ISIOI_ICONFILE;
	return (S_OK);
}

STDMETHODIMP
CBkRootIcon::GetPriority(int *pPriority)
{
	*pPriority = 0;
	return (S_OK);
}

STDMETHODIMP
CBkRootIcon::IsMemberOf(LPCWSTR pwszPath, DWORD /* dwAttrib */)
{
	char	buf[MAXPATH];
	char	root[MAXPATH];

	WideCharToMultiByte(CP_ACP, NULL, pwszPath, -1,
	    buf, sizeof(buf), NULL, NULL);
	TRACE("%s", buf);
	unless (validDrive(buf)) return S_FALSE;
	sprintf(root, "%s/BitKeeper/etc", buf);
	if (isdir(buf) && exists(root)) return (S_OK);
	return S_FALSE;
}
