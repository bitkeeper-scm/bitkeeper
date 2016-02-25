/*
 * Copyright 2008,2016 BitMover, Inc
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

#ifndef _BKREADONLYICON_H_
#define _BKREADONLYICON_H_

#include "resource.h"

class ATL_NO_VTABLE CBkReadonlyIcon :
	public CComObjectRootEx<CComSingleThreadModel>,
	public CComCoClass<CBkReadonlyIcon, &CLSID_BkReadonlyIcon>,
	public IDispatchImpl<IBkReadonlyIcon, &IID_IBkReadonlyIcon,
		&LIBID_BkShellXLib>,
	public IShellIconOverlayIdentifier
{
public:
	CBkReadonlyIcon() {}

	DECLARE_REGISTRY_RESOURCEID(IDR_BKREADONLYICON)
	DECLARE_PROTECT_FINAL_CONSTRUCT()

	BEGIN_COM_MAP(CBkReadonlyIcon)
		COM_INTERFACE_ENTRY(IBkReadonlyIcon)
		COM_INTERFACE_ENTRY(IDispatch)
		COM_INTERFACE_ENTRY(IShellIconOverlayIdentifier)
	END_COM_MAP()

	STDMETHODIMP GetOverlayInfo(LPWSTR pwszIconFile, int cchMax,
	    int *pIndex, DWORD *pdwFlags);
	STDMETHODIMP GetPriority(int *pPriority);
	STDMETHODIMP IsMemberOf(LPCWSTR pwszPath, DWORD dwAttrib);
};

#endif	// _BKREADONLYICON_H_
