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

#pragma once
#include "resource.h"
#include "system.h"
#include "bk_shellx_version.h"
#include <atlhost.h>

class CVersionDialog: public CAxDialogImpl<CVersionDialog>
{
public:
	enum {IDD = IDD_VERSIONDIALOG};

	CVersionDialog() {}
	~CVersionDialog() {}

	BEGIN_MSG_MAP(CVersionDialog)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		MESSAGE_HANDLER(WM_CLOSE, OnCloseDialog)
		COMMAND_HANDLER(IDOK, BN_CLICKED, OnClickedOK)
		CHAIN_MSG_MAP(CAxDialogImpl<CVersionDialog>)
	END_MSG_MAP()

	LRESULT
	OnInitDialog(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
	{
		char *out1, *out2, *out3;

		CAxDialogImpl<CVersionDialog>::OnInitDialog(uMsg,
		    wParam, lParam, bHandled);
		bHandled = TRUE;
		out1 = cmd2buf(bkexe, "version");
		out2 = aprintf("%s\n\n%s", BK_SHELLX_VERSION_STR, out1);
		out3 = dosify(out2);
		SetDlgItemText(IDC_BK_VERSION, _T(out3));
		free(out1);
		free(out2);
		free(out3);
		return (1);
	}

	LRESULT
	OnCloseDialog(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
	{
		EndDialog(IDD_VERSIONDIALOG);
		return (0);
	}

	LRESULT
	OnClickedOK(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
	{
		EndDialog(wID);
		return (0);
	}
};
