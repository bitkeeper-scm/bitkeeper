/*
 * Copyright 2002,2007-2008,0 BitMover, Inc
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

#ifndef _PARENTDIALOG_H_
#define _PARENTDIALOG_H_

#include "resource.h"
#include <atlhost.h>
#include "system.h"

class ParentDialog: public CAxDialogImpl<ParentDialog>
{
public:
	enum {IDD = IDD_PARENTDIALOG};

	ParentDialog()
	{
		parent = (char*)0;
		saveParent = 0;
	}

	~ParentDialog()
	{
		if (parent) free(parent);
	}

	BEGIN_MSG_MAP(CParentDialog)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		COMMAND_ID_HANDLER(IDOK, OnOK)
		COMMAND_ID_HANDLER(IDCANCEL, OnCancel)
		COMMAND_HANDLER(IDC_SAVE_AS_PARENT, BN_CLICKED,
		    OnClickedSave_as_parent)
		COMMAND_HANDLER(IDC_URL_HELP, BN_CLICKED, OnClickedUrl_help)
	END_MSG_MAP()

	inline void setParent(char *newParent);
	inline int saveNewParent(void);
	inline char* getParent(void);

private:
	char	*parent;
	int	saveParent;

	LRESULT
	OnInitDialog(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
	{
		SetDlgItemText(IDC_PARENT_PARENT, parent ? parent : "");
		return (0);
	}

	LRESULT
	OnOK(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
	{
		char tmp[MAX_PATH];

		ZeroMemory(&tmp, sizeof(tmp));
		if (GetDlgItemText(IDC_PARENT_PARENT, tmp, MAX_PATH)) {
			parent = strdup(tmp);
		}
		EndDialog(0);
		return (0);
	}

	LRESULT
	OnCancel(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
	{
		EndDialog(-1);
		return (0);
	}

	LRESULT
	OnClickedSave_as_parent(WORD wNotifyCode, WORD wID,
	    HWND hWndCtl, BOOL& bHandled)
	{
		saveParent = !saveParent;
		return (0);
	}

	LRESULT
	OnClickedUrl_help(WORD wNotifyCode, WORD wID,
	    HWND hWndCtl, BOOL& bHandled)
	{
		BkExec("bk helptool url", ".", SW_HIDE);
		return (0);
	}
};

inline char*
ParentDialog::getParent(void)
{
	return (parent);
}

inline void
ParentDialog::setParent(char* newParent)
{
	if (parent) free(parent);
	parent = strdup(newParent);
}

inline int
ParentDialog::saveNewParent(void)
{
	return (saveParent);
}

#endif	// _PARENTDIALOG_H_
