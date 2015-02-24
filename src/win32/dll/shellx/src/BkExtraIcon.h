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
