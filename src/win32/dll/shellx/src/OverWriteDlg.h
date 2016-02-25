/*
 * Copyright 2004,2016 BitMover, Inc
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
