#ifndef _BKIGNOREDICON_H_
#define _BKIGNOREDICON_H_

#include "resource.h"

class ATL_NO_VTABLE CBkIgnoredIcon :
	public CComObjectRootEx<CComSingleThreadModel>,
	public CComCoClass<CBkIgnoredIcon, &CLSID_BkIgnoredIcon>,
	public IDispatchImpl<IBkIgnoredIcon, &IID_IBkIgnoredIcon,
	    &LIBID_BkShellXLib>,
	public IShellIconOverlayIdentifier
{
public:
	CBkIgnoredIcon() {}

	DECLARE_REGISTRY_RESOURCEID(IDR_BKIGNOREDICON)
	DECLARE_PROTECT_FINAL_CONSTRUCT()

	BEGIN_COM_MAP(CBkIgnoredIcon)
		COM_INTERFACE_ENTRY(IBkIgnoredIcon)
		COM_INTERFACE_ENTRY(IDispatch)
		COM_INTERFACE_ENTRY(IShellIconOverlayIdentifier)
	END_COM_MAP()

	STDMETHODIMP GetOverlayInfo(LPWSTR pwszIconFile, int cchMax,
	    int *pIndex, DWORD *pdwFlags);
	STDMETHODIMP GetPriority(int *pPriority);
	STDMETHODIMP IsMemberOf(LPCWSTR pwszPath, DWORD dwAttrib);
};

#endif	// _BKIGNOREDICON_H_
