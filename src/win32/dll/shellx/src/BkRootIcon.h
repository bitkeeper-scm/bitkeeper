#ifndef _BKROOTICON_H_
#define _BKROOTICON_H_

#include "resource.h"

class ATL_NO_VTABLE CBkRootIcon :
	public CComObjectRootEx<CComSingleThreadModel>,
	public CComCoClass<CBkRootIcon, &CLSID_BkRootIcon>,
	public IDispatchImpl<IBkRootIcon, &IID_IBkRootIcon,
		&LIBID_BkShellXLib>,
	public IShellIconOverlayIdentifier
{
public:
	CBkRootIcon() {}

	DECLARE_REGISTRY_RESOURCEID(IDR_BKROOTICON)
	DECLARE_PROTECT_FINAL_CONSTRUCT()

	BEGIN_COM_MAP(CBkRootIcon)
		COM_INTERFACE_ENTRY(IBkRootIcon)
		COM_INTERFACE_ENTRY(IDispatch)
		COM_INTERFACE_ENTRY(IShellIconOverlayIdentifier)
	END_COM_MAP()

	STDMETHODIMP GetOverlayInfo(LPWSTR pwszIconFile, int cchMax,
	    int *pIndex, DWORD *pdwFlags);
	STDMETHODIMP GetPriority(int *pPriority);
	STDMETHODIMP IsMemberOf(LPCWSTR pwszPath, DWORD dwAttrib);
};

#endif	// _BKROOTICON_H_
