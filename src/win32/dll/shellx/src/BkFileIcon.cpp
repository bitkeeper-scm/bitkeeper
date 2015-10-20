#include "stdafx.h"
#include "BkShellX.h"
#include "BkFileIcon.h"
#include "system.h"

STDMETHODIMP
CBkFileIcon::GetOverlayInfo(LPWSTR pwszIconFile, int cchMax,
    int *pIndex, DWORD *pdwFlags)
{
	cchMax -= MultiByteToWideChar(CP_ACP, NULL, bkdir, -1,
	    pwszIconFile, cchMax) + 1;
	wcsncat_s(pwszIconFile, cchMax, L"\\icons\\BkFile.ico", cchMax);
	*pIndex = 0;
	*pdwFlags = ISIOI_ICONFILE;
	return (S_OK);
};

STDMETHODIMP
CBkFileIcon::GetPriority(int *pPriority)
{
	*pPriority = 0;
	return (S_OK);
}

STDMETHODIMP
CBkFileIcon::IsMemberOf(LPCWSTR pwszPath, DWORD dwAttrib)
{
	int	state;
	struct	stat sb;
	char	buf[MAXPATH];

	WideCharToMultiByte(CP_ACP, NULL, pwszPath, -1,
	    buf, MAXPATH, NULL, NULL);
	TRACE("%s", buf);
	unless (validDrive(buf)) return (S_FALSE);
	if (stat(buf, &sb)) return (S_FALSE);
	if (S_ISDIR(sb.st_mode)) return (S_FALSE);
	state = cache_fileStatus(buf, &sb);
	if (state == BK_EDITED) return (S_OK);
	return (S_FALSE);
}
