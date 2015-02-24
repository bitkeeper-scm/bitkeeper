#ifndef _CONTEXTMENUHANDLER_H_
#define _CONTEXTMENUHANDLER_H_

#include "Resource.h"
#include "system.h"

typedef	struct selection selection;

typedef void(*mfunc)(selection *sel);

typedef struct	{
	char	icon[64];		/* Icon for the menu entry */
	char	string[64];		/* Text for the menu entry */
	char	helpstring[1024];	/* Hover help */
	int	inSubmenu;		/* Is it in a submenu? */
	mfunc	func;			/* What to do when selected */
} mentry;

struct selection {
	hash	*files;		/* files selected and their status */
	char	*path;		/* where we are, extracted from files above */
	int	flags;		/* what the user clicked on (see
				 * system.h's SEL_* macros */
};

typedef map<UINT, mentry*> itemmap;
typedef map<UINT_PTR, mentry*> offsetmap;

class IContextMenuItem;

class ATL_NO_VTABLE ContextMenuHandler:
	public CComObjectRootEx<CComSingleThreadModel>,
	public CComCoClass<ContextMenuHandler, &CLSID_ContextMenuHandler>,
	public IShellExtInit,
	public IContextMenu3
{
public:
	ContextMenuHandler();
	~ContextMenuHandler();

	DECLARE_REGISTRY_RESOURCEID(IDR_BKSHELLEXTHANDLER)

	DECLARE_PROTECT_FINAL_CONSTRUCT()

	BEGIN_COM_MAP(ContextMenuHandler)
		COM_INTERFACE_ENTRY(IContextMenu)
		COM_INTERFACE_ENTRY(IContextMenu2)
		COM_INTERFACE_ENTRY(IContextMenu3)
		COM_INTERFACE_ENTRY(IShellExtInit)
	END_COM_MAP()

	STDMETHOD(Initialize)(THIS_ LPCITEMIDLIST pidlFolder,
	    LPDATAOBJECT lpdobj, HKEY hkeyProgID);

	STDMETHOD(QueryContextMenu)(THIS_ HMENU menu, UINT indexMenu,
	    UINT idCmdFirst, UINT idCmdLast, UINT uFlags);

	STDMETHOD(InvokeCommand)(THIS_ LPCMINVOKECOMMANDINFO lpici);

	STDMETHOD(GetCommandString)(THIS_ UINT_PTR idCmd, UINT uType,
	    UINT *pwReserved, LPSTR pszName, UINT cchMax);

	STDMETHOD(HandleMenuMsg)(THIS_ UINT, WPARAM, LPARAM);

	STDMETHOD(HandleMenuMsg2)(THIS_ UINT, WPARAM, LPARAM, LRESULT*);

private:
	int	_idcmd;
	int	_index;
	int	_offset;
	itemmap	_itemsByCmd;
	offsetmap _itemsByOffset;
	selection	*sel;	  /* the selection */

	HMENU ContextMenuHandler::CreateSubmenu(HMENU menu, char *string);
	void ContextMenuHandler::AddMenuSeparator(HMENU menu);
	void ContextMenuHandler::AddMenuItem(HMENU menu, char *string,
	    char *icon, char *help, int state, int inSubmenu, mfunc func);
};

#endif	// _CONTEXTMENUHANDLER_H_
