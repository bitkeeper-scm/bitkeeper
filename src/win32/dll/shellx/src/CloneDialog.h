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

#ifndef _CLONEDIALOG_H_
#define _CLONEDIALOG_H_

#include "resource.h"
#include <atlwin.h>
#include "system.h"

class CloneDialog: public CDialogImpl<CloneDialog>
{
public:
	enum {IDD = IDD_CLONE};

	BEGIN_MSG_MAP( CloneDialog)
		COMMAND_HANDLER(IDOK, BN_CLICKED, OnClone)
		COMMAND_HANDLER(IDCANCEL, BN_CLICKED, OnCancel)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		COMMAND_HANDLER(IDC_CLONE_TARGET, EN_CHANGE, OnChangeTarget)
		COMMAND_HANDLER(IDC_REVERSE, BN_CLICKED, OnReverse)
		COMMAND_HANDLER(IDC_CLONE_HELP, BN_CLICKED, OnClickedClone_help)
		COMMAND_HANDLER(IDC_CLONE_DEMO, BN_CLICKED, OnClickedClone_demo)
	END_MSG_MAP()

	CloneDialog()
	{
		targetDirectory = parentDirectory = revision = (char*)0;
		demo = 0;
	}

	~CloneDialog()
	{
		if (targetDirectory) free(targetDirectory);
		if (parentDirectory) free(parentDirectory);
		if (revision) free(revision);
	}

	inline char* getParent(void);
	inline char* getTarget(void);
	inline char* getRev(void);
	inline void setParent(char*);
	inline void setTarget(char*);
	inline void setRevision(char*);

private:
	char* targetDirectory;
	char* parentDirectory;
	char* revision;
	int   demo;

	LRESULT
	OnClone(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
	{
		char buf[MAX_PATH];

		ZeroMemory(&buf, sizeof(buf));
		if (GetDlgItemText(IDC_CLONE_PARENT, buf, MAX_PATH)) {
			setParent(buf);
		}
		if (GetDlgItemText(IDC_CLONE_TARGET, buf, MAX_PATH)) {
			setTarget(buf);
		}
		if (GetDlgItemText(IDC_CLONE_REVISION, buf, MAX_PATH)) {
			setRevision(buf);
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
	OnReverse(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
	{
		char	parent[MAX_PATH];
		char	target[MAX_PATH];

		GetDlgItemText(IDC_CLONE_PARENT, parent, MAX_PATH);
		GetDlgItemText(IDC_CLONE_TARGET, target, MAX_PATH);

		/* swap the parent and the target */
		setTarget(parent);
		setParent(target);

		SetDlgItemText(IDC_CLONE_PARENT, parentDirectory);
		SetDlgItemText(IDC_CLONE_TARGET, targetDirectory);

		bHandled = TRUE;
		return (0);
	}

	LRESULT
	OnInitDialog(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
	{
		SetDlgItemText(IDC_CLONE_PARENT, parentDirectory);
		SetDlgItemText(IDC_CLONE_TARGET, targetDirectory);

		bHandled = TRUE;
		// TODO: Add Code for message handler. Call
		// DefWindowProc if necessary.
		return (0);
	}

	LRESULT
	OnChangeTarget(WORD wNotifyCode, WORD wID, HWND hWndCtl,
	    BOOL& bHandled)
	{
		char dir[MAX_PATH];

		GetDlgItemText(IDC_CLONE_TARGET, dir, MAX_PATH);
		setTarget(dir);

		bHandled = TRUE;
		return (0);
	}

	LRESULT
	OnClickedClone_help(WORD wNotifyCode, WORD wID, HWND hWndCtl,
	    BOOL& bHandled)
	{
		BkExec("bk helptool url", ".", SW_HIDE);
		return (0);
	}

	LRESULT
	OnClickedClone_demo(WORD wNotifyCode, WORD wID, HWND hWndCtl,
	    BOOL& bHandled)
	{
		demo = !demo;
		if (demo) {
			setParent("http://bkbits.net/u/bkdemo/bk_demo");
			setTarget("");
		} else {
			setParent("");
			setTarget("");
		}
		SetDlgItemText(IDC_CLONE_PARENT, parentDirectory);
		SetDlgItemText(IDC_CLONE_TARGET, targetDirectory);

		bHandled = TRUE;
		return (0);
	}
};

inline char*
CloneDialog::getParent(void)
{
	return (parentDirectory);
}

inline char*
CloneDialog::getTarget(void)
{
	return (targetDirectory);
}

inline char*
CloneDialog::getRev(void)
{
	return (revision);
}

inline void
CloneDialog::setParent(char *p)
{
	if (parentDirectory) free(parentDirectory);
	parentDirectory = strdup(p);
}

inline void
CloneDialog::setTarget(char *t)
{
	if (targetDirectory) free(targetDirectory);
	targetDirectory = strdup(t);
}

inline void
CloneDialog::setRevision(char *r)
{
	if (revision) free(revision);
	revision = strdup(r);
}
#endif	// _CLONEDIALOG_H_
