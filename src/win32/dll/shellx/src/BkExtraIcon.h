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

#ifndef _BKEXTRAICON_H_
#define _BKEXTRAICON_H_

#include "resource.h"

class ATL_NO_VTABLE CBkExtraIcon :
	public CComObjectRootEx<CComSingleThreadModel>,
	public CComCoClass<CBkExtraIcon, &CLSID_BkExtraIcon>,
	public IDispatchImpl<IBkExtraIcon, &IID_IBkExtraIcon,
	    &LIBID_BkShellXLib>,
	public IShellIconOverlayIdentifier
{

public:
	CBkExtraIcon()	{}

	DECLARE_REGISTRY_RESOURCEID(IDR_BKEXTRAICON)
	DECLARE_PROTECT_FINAL_CONSTRUCT()

	BEGIN_COM_MAP(CBkExtraIcon)
		COM_INTERFACE_ENTRY(IBkExtraIcon)
		COM_INTERFACE_ENTRY(IDispatch)
		COM_INTERFACE_ENTRY(IShellIconOverlayIdentifier)
	END_COM_MAP()

	STDMETHODIMP GetOverlayInfo(LPWSTR pwszIconFile, int cchMax,
	    int *pIndex, DWORD *pdwFlags);
	STDMETHODIMP GetPriority(int *pPriority);
	STDMETHODIMP IsMemberOf(LPCWSTR pwszPath, DWORD dwAttrib);
};

#endif // _BKEXTRAICON_H_
