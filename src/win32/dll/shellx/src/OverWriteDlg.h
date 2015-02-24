#ifndef _OVERWRITEDLG_H_
#define _OVERWRITEDLG_H_

#include "resource.h"
#include <atlhost.h>

class COverWriteDlg: public CAxDialogImpl<COverWriteDlg>
{
public:
	enum {IDD = IDD_OVERWRITEDLG};

	COverWriteDlg() {}
	~COverWriteDlg() {}

	BEGIN_MSG_MAP(COverWriteDlg)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		COMMAND_ID_HANDLER(IDOK, OnOK)
		COMMAND_ID_HANDLER(IDCANCEL, OnCancel)
	END_MSG_MAP()

	LRESULT
	OnInitDialog(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
	{
		return (1);
	}

	LRESULT
	OnOK(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
	{
		EndDialog(wID);
		return (0);
	}

	LRESULT
	OnCancel(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
	{
		EndDialog(-1);
		return (0);
	}
};

#endif	// _OVERWRITEDLG_H_
