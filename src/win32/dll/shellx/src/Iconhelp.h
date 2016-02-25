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
#include <atlhost.h>

class CIconhelp:
	public CAxDialogImpl<CIconhelp>
{
public:
	enum {IDD = IDD_ICONHELP};

	CIconhelp() {}
	~CIconhelp() {}

	BEGIN_MSG_MAP(CIconhelp)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		MESSAGE_HANDLER(WM_CLOSE, OnCloseDialog)
		COMMAND_HANDLER(IDC_MOREHELP, BN_CLICKED, OnClickedMoreHelp)
		CHAIN_MSG_MAP(CAxDialogImpl<CIconhelp>)
	END_MSG_MAP()

	LRESULT
	OnInitDialog(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
	{
		CAxDialogImpl<CIconhelp>::OnInitDialog(uMsg,
		    wParam, lParam, bHandled);
		bHandled = TRUE;
		return (1);
	}

	LRESULT
	OnClickedMoreHelp(WORD wNotifyCode, WORD wID,
	    HWND hWndCtl, BOOL& bHandled)
	{
		EndDialog(wID);
		BkExec("bk helptool", ".", SW_HIDE);
		return (0);
	}

	LRESULT
	OnCloseDialog(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
	{
		EndDialog(IDD_ICONHELP);
		return (0);
	}
};
