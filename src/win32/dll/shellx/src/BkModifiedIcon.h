#ifndef _BKMODIFIEDICON_H_
#define _BKMODIFIEDICON_H_

#include "resource.h"

class ATL_NO_VTABLE CBkModifiedIcon :
	public CComObjectRootEx<CComSingleThreadModel>,
	public CComCoClass<CBkModifiedIcon, &CLSID_BkModifiedIcon>,
	public IDispatchImpl<IBkModifiedIcon, &IID_IBkModifiedIcon,
		&LIBID_BkShellXLib>,
	public IShellIconOverlayIdentifier
{
public:
	CBkModifiedIcon() {}

	DECLARE_REGISTRY_RESOURCEID(IDR_BKMODIFIEDICON)
	DECLARE_PROTECT_FINAL_CONSTRUCT()

	BEGIN_COM_MAP(CBkModifiedIcon)
		COM_INTERFACE_ENTRY(IBkModifiedIcon)
		COM_INTERFACE_ENTRY(IDispatch)
		COM_INTERFACE_ENTRY(IShellIconOverlayIdentifier)
	END_COM_MAP()

	STDMETHODIMP GetOverlayInfo(LPWSTR pwszIconFile, int cchMax,
	    int *pIndex, DWORD *pdwFlags);
	STDMETHODIMP GetPriority(int *pPriority);
	STDMETHODIMP IsMemberOf(LPCWSTR pwszPath, DWORD dwAttrib);
};

#endif	// _BKMODIFIEDICON_H_
