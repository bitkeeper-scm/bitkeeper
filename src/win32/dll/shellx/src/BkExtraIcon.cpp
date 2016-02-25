/*
 * Copyright 2008-2009,0 BitMover, Inc
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

#include "stdafx.h"
#include "BkShellX.h"
#include "BkExtraIcon.h"
#include "system.h"

STDMETHODIMP
CBkExtraIcon::GetOverlayInfo(LPWSTR pwszIconFile, int cchMax,
    int *pIndex, DWORD *pdwFlags)
{
	cchMax -= MultiByteToWideChar(CP_ACP, NULL, bkdir, -1,
	    pwszIconFile, cchMax) + 1;
	wcsncat_s(pwszIconFile, cchMax, L"\\icons\\BkExtra.ico", cchMax);
	*pIndex = 0;
	*pdwFlags = ISIOI_ICONFILE;
	return S_OK;

};

STDMETHODIMP
CBkExtraIcon::GetPriority(int *pPriority)
{
	*pPriority = 0;
	return S_OK;
}

STDMETHODIMP
CBkExtraIcon::IsMemberOf(LPCWSTR pwszPath, DWORD dwAttrib)
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
	if (state == BK_EXTRA) return (S_OK);
	return (S_FALSE);
}
