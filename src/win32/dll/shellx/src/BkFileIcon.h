#ifndef _BKFILEICON_H_
#define _BKFILEICON_H_

#include "resource.h"

class ATL_NO_VTABLE CBkFileIcon:
	public CComObjectRootEx<CComSingleThreadModel>,
	public CComCoClass<CBkFileIcon, &CLSID_BkFileIcon>,
	public IDispatchImpl<IBkFileIcon, &IID_IBkFileIcon,
		&LIBID_BkShellXLib>,
	public IShellIconOverlayIdentifier
{
public:
	CBkFileIcon() {}

	DECLARE_REGISTRY_RESOURCEID(IDR_BKFILEICON)
	DECLARE_PROTECT_FINAL_CONSTRUCT()

	BEGIN_COM_MAP(CBkFileIcon)
		COM_INTERFACE_ENTRY(IBkFileIcon)
		COM_INTERFACE_ENTRY(IDispatch)
		COM_INTERFACE_ENTRY(IShellIconOverlayIdentifier)
	END_COM_MAP()

	STDMETHODIMP GetOverlayInfo(LPWSTR pwszIconFile, int cchMax,
	    int *pIndex, DWORD *pdwFlags);
	STDMETHODIMP GetPriority(int *pPriority);
	STDMETHODIMP IsMemberOf(LPCWSTR pwszPath, DWORD dwAttrib);
};

#endif	// _BKFILEICON_H_
