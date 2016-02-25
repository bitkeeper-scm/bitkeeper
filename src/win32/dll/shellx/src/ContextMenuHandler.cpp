/*
 * Copyright 2001-2002,2007-2009,0 BitMover, Inc
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

#include "stdafx.h"
#include "BkShellX.h"
#include "CloneDialog.h"
#include "ParentDialog.h"
#include "VersionDialog.h"
#include "Iconhelp.h"
#include "ContextMenuHandler.h"
#include "system.h"

/*
 * Filter the selection by flag. It returns a tmpfile
 * with just the files/dirs in the selection that matched
 * the criteria.
 */
static char*
filterSel(selection *sel, int filter)
{
	int	res;
	FILE	*f;
	char	tmp[MAX_PATH];

	res = GetTempPath(sizeof(tmp), tmp);
	if (res > 0) {
		strcat(tmp, "shellxXXXXXX");
	} else {	
		TRACE("GetTempPath failed, returned: %d", res);
		sprintf(tmp, "c:\\tmp\\shellxXXXXXX");
	} 
	mktemp(tmp);
	unless (f = fopen(tmp, "w")) {
		TRACE("fopen failed: %s", tmp);
		return (0);
	}
	EACH_HASH(sel->files) {
		char	*fn = (char*)sel->files->kptr;
		int	flg = *(int*)sel->files->vptr;

		if (flg & filter) fprintf(f, "%s\n", fn);
	}
	fclose(f);
	return (strdup(tmp));
}

static void
do_clean(selection *sel)
{
	char *tf, *cmd;

	TRACE(0);
	if (SEL_BACKGROUND(sel)) {
		BkExec("bk clean", sel->path, SW_HIDE);
		return;
	}
	unless (tf = filterSel(sel, BK_READONLY | BK_EDITED)) return;
	cmd = aprintf("bk clean - < \"%s\" & del \"%s\"", tf, tf);
	BkExec(cmd, sel->path, SW_HIDE);
	free(cmd);
	free(tf);
}

static void
do_check(selection *sel)
{
	char	*dir;

	TRACE(0);
	if (SEL_SINGLEDIR(sel)) {
		hash_first(sel->files);
		dir = (char*)sel->files->kptr;
	} else {
		dir = sel->path;
	}
	BkExec("bk -r check -av & if not errorlevel 1 pause & exit", dir, SW_NORMAL);
}

static void
do_changesL(selection *sel)
{
	char	*dir;

	TRACE(0);
	if (SEL_SINGLEDIR(sel)) {
		hash_first(sel->files);
		dir = (char*)sel->files->kptr;
	} else {
		dir = sel->path;
	}
	BkExec("bk changes -L", dir, SW_NORMAL);
}

static void
do_changesR(selection *sel)
{
	char	*dir;

	TRACE(0);
	if (SEL_SINGLEDIR(sel)) {
		hash_first(sel->files);
		dir = (char*)sel->files->kptr;
	} else {
		dir = sel->path;
	}
	BkExec("bk changes -R", dir, SW_NORMAL);
}

static void
do_citool(selection *sel)
{
	char	*dir;

	TRACE(0);
	if (SEL_SINGLEDIR(sel)) {
		hash_first(sel->files);
		dir = (char*)sel->files->kptr;
	} else {
		dir = sel->path;
	}
	BkExec("bk citool", dir, SW_HIDE);
}

static void
do_clone(selection *sel)
{
	CloneDialog dlg;
	char	*targetDir, *parent, *rev;
	char	**cmd = 0;
	char	*c, *dir;

	TRACE(0);

	if (SEL_SINGLEDIR(sel) && SEL_PROJROOT(sel)) {
		hash_first(sel->files);
		dir = (char*)sel->files->kptr;
		c = aprintf("%s_copy", dir);
		dlg.setParent(dir);
		dlg.setTarget(c);
		free(c);
	}

	if (dlg.DoModal() == -1) return;

	// could use some validation if valid parent, cloneDir and revision
	rev = dlg.getRev();
	parent = dlg.getParent();
	targetDir = dlg.getTarget();

	cmd = addLine(cmd, strdup("bk clone"));

	if (rev && !streq(rev, "")) {
		cmd = addLine(cmd, aprintf("-r%s", rev));
	}

	cmd = addLine(cmd, aprintf("\"%s\"", parent));

	if (targetDir && !streq(targetDir, "")) {
		cmd = addLine(cmd, aprintf("\"%s\"", targetDir));
	}

	c = joinLines(" ", cmd);
	if (SEL_BACKGROUND(sel)) {
		dir = sel->path;
	} else {
		// assert(SEL_SINGLEDIR(sel)); ?
		hash_first(sel->files);
		dir = (char*)sel->files->kptr;
	}
	BkExec(c, dir, SW_NORMAL);
	free(c);
	freeLines(cmd, free);
}

static void
do_commandprompt(selection *sel)
{
	char	*dir;

	TRACE(0);
	if (SEL_SINGLEDIR(sel)) {
		hash_first(sel->files);
		dir = (char*)sel->files->kptr;
	} else {
		dir = sel->path;
	}
	ShellExecute(0, 0, "cmd.exe", 0, dir, SW_NORMAL);
}

static void
do_rm(selection *sel)
{
	int	ans;
	char	*tf, *cmd;

	TRACE(0);
	ans = MessageBox(GetActiveWindow(),
	    "Are you sure you want to delete the selected files?",
	    "Delete Files", MB_YESNO | MB_ICONWARNING);
	if (ans == IDYES) {
		// we ignore extras and ignored files, they can
		// use the normal explorer delete command to
		// delete those. We also ignore files with diffs,
		// they need to discard the diffs first and then
		// remove those
		unless (tf = filterSel(sel, BK_READONLY | BK_EDITED)) return;
		cmd = aprintf("bk rm - < \"%s\" & del \"%s\"", tf, tf);
		BkExec(cmd, sel->path, SW_HIDE);
		free(cmd);
		free(tf);
	}
}

static void
do_diff(selection *sel)
{
	char	*cmd;
	char	*tf;

	TRACE(0);
	if (SEL_BACKGROUND(sel)) {
		BkExec("bk difftool", sel->path, SW_HIDE);
	} else if (SEL_SINGLEDIR(sel)) {
		char	*dir;

		hash_first(sel->files);
		dir = (char*)sel->files->kptr;
		BkExec("bk difftool", dir, SW_HIDE);
	} else if (SEL_HASMODIFIED(sel)) {
		unless (tf = filterSel(sel, BK_MODIFIED)) return;
		cmd = aprintf("bk difftool - < \"%s\" & del \"%s\"", tf, tf);
		BkExec(cmd, sel->path, SW_HIDE);
		free(cmd);
		free(tf);
	}
}

static void
do_get(selection *sel)
{
	TRACE(0);
	if (SEL_BACKGROUND(sel)) {
		BkExec("bk get -S", sel->path, SW_HIDE);
	} else if (SEL_SINGLEDIR(sel)) {
		char	*dir;

		hash_first(sel->files);
		dir = (char*)sel->files->kptr;
		BkExec("bk get -S", dir, SW_HIDE);
	}
}

static void
do_getRecursive(selection *sel)
{
	TRACE(0);
	if (SEL_SINGLEDIR(sel)) {
		char	*dir;

		hash_first(sel->files);
		dir = (char*)sel->files->kptr;
		BkExec("bk -Ur. get -S", dir, SW_NORMAL);
	}
}

static void
do_edit(selection *sel)
{
	char	*tf, *cmd;

	TRACE(0);
	if (SEL_BACKGROUND(sel)) {
		BkExec("bk edit -S", sel->path, SW_HIDE);
	} else {
		// XXX: bk get -S - can't handle directories
		// so we just ignore them.
		unless (tf = filterSel(sel, BK_READONLY)) return;
		cmd = aprintf("bk edit -S - < \"%s\" & del \"%s\"", tf, tf);
		BkExec(cmd, sel->path, SW_HIDE);
		free(cmd);
		free(tf);
	}
}

static void
do_editRecursive(selection *sel)
{
	TRACE(0);
	if (SEL_SINGLEDIR(sel)) {
		char	*dir;

		hash_first(sel->files);
		dir = (char*)sel->files->kptr;
		BkExec("bk -Ur. edit -S", dir, SW_NORMAL);
	}
}

static void
do_help(selection *sel)
{
	CIconhelp	dlg;

	TRACE(0);
	dlg.DoModal();
	return;
}

static void
do_new(selection *sel)
{
	char	*tf, *cmd;

	TRACE(0);
	if (SEL_HASEXTRAS(sel)) {
		unless (tf = filterSel(sel, BK_EXTRA)) return;
		cmd = aprintf("bk new - < \"%s\" & del \"%s\"", tf, tf);
		BkExec(cmd, sel->path, SW_HIDE);
		free(cmd);
		free(tf);
	}
}

static void
do_pull(selection *sel)
{
	ParentDialog dlg;
	char	*dir, *parent, *repo_parent, *cmd;

	TRACE(0);
	if (SEL_SINGLEDIR(sel)) {
		hash_first(sel->files);
		dir = (char*)sel->files->kptr;
	} else {
		dir = sel->path;
	}
	repo_parent = getRepoParent(dir);
	dlg.setParent(repo_parent);
	free(repo_parent);
	if (dlg.DoModal() == -1) return;

	parent = dlg.getParent();

	cmd = aprintf("bk pull \"%s\"", parent);
	BkExec(cmd, dir, SW_NORMAL);
	free(cmd);
	if (dlg.saveNewParent()) {
		cmd = aprintf("bk parent \"%s\"", parent);
		BkExec(cmd, dir, SW_HIDE);
		free(cmd);
	}
}

static void
do_version(selection *sel)
{
	CVersionDialog	dlg;

	TRACE(0);
	dlg.DoModal();
	return;
}

static void
do_push(selection *sel)
{
	ParentDialog dlg;
	char	*dir, *parent, *repo_parent, *cmd;

	TRACE(0);
	if (SEL_SINGLEDIR(sel)) {
		hash_first(sel->files);
		dir = (char*)sel->files->kptr;
	} else {
		dir = sel->path;
	}
	repo_parent = getRepoParent(dir);
	dlg.setParent(repo_parent);
	free(repo_parent);
	if ( dlg.DoModal() == -1) return;

	parent = dlg.getParent();

	cmd = aprintf("bk push \"%s\"", parent);
	BkExec(cmd, dir, SW_NORMAL);
	free(cmd);

	if (dlg.saveNewParent()) {
		cmd = aprintf("bk parent \"%s\"", parent);
		BkExec(cmd, dir, SW_HIDE);
		free(cmd);
	}
}

static void
do_revtool(selection *sel)
{
	TRACE(0);
	if (SEL_BACKGROUND(sel)) {
		BkExec("bk revtool", sel->path, SW_HIDE);
	} else if (SEL_SINGLEDIR(sel)) {
		char	*dir;

		hash_first(sel->files);
		dir = (char*)sel->files->kptr;
		BkExec("bk revtool", dir, SW_HIDE);
	} else if (SEL_SINGLEFILE(sel)) {
		char	*fn;
		char	*cmd;

		hash_first(sel->files);
		fn = (char*)sel->files->kptr;
		cmd = aprintf("bk revtool \"%s\"", fn);
		BkExec(cmd, sel->path, SW_HIDE);
		free(cmd);
	}
}

static void
do_setuptool(selection *sel)
{
	char	*dir;

	TRACE(0);
	if (SEL_SINGLEDIR(sel)) {
		hash_first(sel->files);
		dir = (char*)sel->files->kptr;
	} else {
		dir = sel->path;
	}
	BkExec("bk setuptool -e", dir, SW_HIDE);
}

static void
do_unedit(selection *sel)
{
	int	ans;
	char	*tf, *cmd;

	TRACE(0);
	ans = MessageBox(GetActiveWindow(),
	    "Are you sure you want to lose any changes you have made to the"
	    " selected files?",
	    "Discard Changes", MB_YESNO | MB_ICONWARNING);
	if (ans == IDYES) {
		unless (tf = filterSel(sel, BK_MODIFIED)) return;
		cmd = aprintf("bk unedit - < \"%s\" & del \"%s\"", tf, tf);
		BkExec(cmd, sel->path, SW_HIDE);
		free(cmd);
		free(tf);
	}
}

ContextMenuHandler::ContextMenuHandler(void)
{
	TRACE("Constructor");
	sel = (selection *)calloc(1, sizeof(selection));
}

ContextMenuHandler::~ContextMenuHandler(void)
{
	TRACE("Destructor");

	offsetmap::iterator it = _itemsByOffset.begin();

	while (it != _itemsByOffset.end()) {
		free((*it).second);
		++it;
	}
	_itemsByCmd.clear();
	_itemsByOffset.clear();
	if (sel->files) hash_free(sel->files);
	if (sel->path) free(sel->path);
	free(sel);
}

static bool
illegalFolder(char *folder)
{
	int i;
	TCHAR buf[MAX_PATH];
	TCHAR start[MAX_PATH];
	LPITEMIDLIST pidl = NULL;
	static int csidl[] =
	{
		CSIDL_BITBUCKET,
		CSIDL_CDBURN_AREA,
		CSIDL_COMMON_FAVORITES,
		CSIDL_COMMON_STARTMENU,
		CSIDL_COMPUTERSNEARME,
		CSIDL_CONNECTIONS,
		CSIDL_CONTROLS,
		CSIDL_COOKIES,
		CSIDL_FAVORITES,
		CSIDL_FONTS,
		CSIDL_HISTORY,
		CSIDL_INTERNET,
		CSIDL_INTERNET_CACHE,
		CSIDL_NETHOOD,
		CSIDL_NETWORK,
		CSIDL_PRINTERS,
		CSIDL_PRINTHOOD,
		CSIDL_RECENT,
		CSIDL_SENDTO,
		CSIDL_STARTMENU,
		0
	};

	for (i = 0; csidl[i]; ++i) {
		pidl = NULL;
		if (SHGetFolderLocation(NULL, csidl[i], NULL, 0, &pidl)!=S_OK) {
			continue;
		}
		unless (SHGetPathFromIDList(pidl, buf)) {
			CoTaskMemFree(pidl);
			continue;
		}
		CoTaskMemFree(pidl);
// 		TRACE("%s == %s", folder, buf);
		if (patheq(folder, buf)) return (true);
	}

	if (SHGetFolderLocation(NULL, CSIDL_STARTMENU, NULL, 0,
		&pidl) != S_OK) return (false);
	unless (SHGetPathFromIDList(pidl, start)) {
		CoTaskMemFree(pidl);
		return (false);
	}
// 	TRACE("%s == %s", folder, start);
	if (pathneq(folder, start, strlen(start))) return (true);

	if (SHGetFolderLocation(NULL, CSIDL_COMMON_STARTMENU, NULL, 0,
		&pidl) != S_OK) return (false);
	unless (SHGetPathFromIDList(pidl, start)) {
		CoTaskMemFree(pidl);
		return (false);
	}
// 	TRACE("%s == %s", folder, start);
	if (pathneq(folder, start, strlen(start))) return (true);
	return (false);
}

STDMETHODIMP
ContextMenuHandler::Initialize(THIS_ LPCITEMIDLIST lpFolder,
    LPDATAOBJECT lpdobj, HKEY hkeyProgID)
{
	UINT	    nfiles;
	STGMEDIUM   md;
	unsigned int i, files, dirs, status;
	char	*p;
	FORMATETC   fe = {CF_HDROP, NULL, DVASPECT_CONTENT, -1, TYMED_HGLOBAL};
	char buf[MAXPATH];

	TRACE("Initialize");

	unless (lpdobj || lpFolder) return (S_OK);
	if (SEL_INVALID(sel)) return (S_OK);
	sel->files = hash_new(HASH_MEMHASH);
	sel->flags = 0;
	files = dirs = 0;
	if (lpdobj) {
		TRACE("Found selection");
		unless (SUCCEEDED(lpdobj->GetData(&fe, &md))) return (S_OK);
		// Get the file name from the CF_HDROP.
		if (nfiles = DragQueryFile((HDROP)md.hGlobal, -1, NULL, 0)) {
			for (i = 0; i < nfiles; ++i) {
				DragQueryFile((HDROP)md.hGlobal,
				    i, buf, MAXPATH);
				if (isdir(buf)) {
					status = BK_DIR;
					sel->flags |= status;
					// see if this directory is a bk repo
					p = aprintf("%s\\%s", buf, BKROOT);
					if (exists(p)) {
						TRACE("%s is PROJ_ROOT", buf);
						sel->flags |= BK_PROJROOT;
					}
					free(p);
					hash_store(sel->files,
					    buf, (int)strlen(buf)+1,
					    &status, sizeof(int));
					dirs++;
					TRACE("D: %s", buf);
				} else {
					status = cache_fileStatus(buf, 0);
					sel->flags |= (status | BK_FILE);
					hash_store(sel->files,
					    buf,(int)strlen(buf)+1,
					    &status, sizeof(int));
					files++;
					TRACE("F: %s -> 0x%08x", buf, status);
				}
			}
		}
		// Figure out where we are. This code assumes it's
		// IMPOSSIBLE to select combinations of files/folders
		// that are in different directories. Various experiments
		// seem to confirm this, but if this assumption is false,
		// this next bit is horribly broken.
		if (files || dirs) {
			char	*p;

			hash_first(sel->files);
			p = (char *)sel->files->kptr;
			if ((dirs == 1) && illegalFolder(p)) {
				// this is handled specially because
				// the user might have clicked on
				// something like the "Start Menu",
				// which will just add one directory
				sel->flags |= BK_INVALID;
				sel->path = strdup(p);
			} else {
				sel->path = dirname(p);
				if (files > 1) sel->flags |= BK_MULTIFILE;
				if (dirs > 1) sel->flags |= BK_MULTIDIR;
			}
		}
		ReleaseStgMedium(&md);
	} else if (lpFolder) { /* folder background */
		TRACE("background");
		// Extract directory name and store for later use
		unless (SUCCEEDED(SHGetPathFromIDList(lpFolder, buf))) {
			return (S_OK);
		}
		sel->path = strdup(buf);
		sel->flags |= BK_BACKGROUND;
		p = aprintf("%s\\%s", buf, BKROOT);
		if (exists(p)) {
			TRACE("%s is PROJ_ROOT", buf);
			sel->flags |= BK_PROJROOT;
		}
		free(p);
	}
	TRACE("path = %s", sel->path);
	return (S_OK);
}

STDMETHODIMP
ContextMenuHandler::QueryContextMenu(THIS_ HMENU menu, UINT indexMenu,
    UINT idCmdFirst, UINT idCmdLast, UINT uFlags)
{
	char	*inProj = 0;
	HMENU	submenu;
	int	singleFile = 0, singleDir = 0, items = 0;

	if ((uFlags & CMF_DEFAULTONLY) && (uFlags & CMF_NODEFAULT)) {
		return (E_FAIL);
	}

	// path was filled by Initialize above
	TRACE("sel->path = %s", sel->path);
	if (illegalFolder(sel->path) || !validDrive(sel->path)) return(E_FAIL);

	unless (inProj = rootDirectory(sel->path)) {
		// outside of a bk repo, only single directory or
		// background is allowed
		unless (SEL_BACKGROUND(sel) ||
		    (SEL_HASDIRS(sel) &&
		     !SEL_MULTIDIR(sel) &&
		     !SEL_HASFILES(sel))) {
			return (E_FAIL);
		}
	}

	_index  = indexMenu;
	_idcmd  = idCmdFirst;
	_offset = 0;

	AddMenuSeparator(menu);	// start bk area
	if (inProj || SEL_PROJROOT(sel)) {
		AddMenuItem(menu, "checkin",
		    "BK Checkin Tool...",
		    "Check in modified files",
		    MFS_ENABLED, 0, &do_citool);
	}
	if (inProj) {
		if (SEL_BACKGROUND(sel) || SEL_HASRO(sel)) {
			AddMenuItem(menu, "checkoutrw",
			    "BK Edit Files...",
			    "Checkout files in read/write mode",
			    MFS_ENABLED, 0, &do_edit);
		}
		if (SEL_HASMODIFIED(sel)) {
			AddMenuItem(menu, "diff",
			    "BK Diff File...",
			    "Show differences in the selected files",
			    MFS_ENABLED, 0, &do_diff);
		}
		if (SEL_BACKGROUND(sel) || SEL_SINGLEDIR(sel)) {
			AddMenuItem(menu, "diffdir",
			    "BK Diff Directory...",
			    "Show differences in the selected directory",
			    MFS_ENABLED, 0, &do_diff);
		}
	}
	submenu = CreateSubmenu(menu, "BitKeeper");
	if (inProj && SEL_BACKGROUND(sel)) {
			AddMenuItem(submenu, "checkoutrw",
			    "Checkout Files Read-Write...",
			    "Checkout files in the selected directory",
			    MFS_ENABLED, 1, &do_edit);
			AddMenuItem(submenu, "checkoutro",
			    "Checkout Files Read-Only...",
			    "Checkout files in the selected directory",
			    MFS_ENABLED, 1, &do_get);
			items++;
	}
	if ((SEL_PROJROOT(sel) && !SEL_BACKGROUND(sel)) ||
	    (inProj && SEL_SINGLEDIR(sel))) {
		AddMenuItem(submenu, "checkoutrw",
		    "Checkout Files Read-Write (recursive)...",
		    "Recursively checkout files in the selected "
		    "directory", MFS_ENABLED, 1, &do_editRecursive);
		AddMenuItem(submenu, "checkoutro",
		    "Checkout Files Read-Only (recursive)...",
		    "Recursively checkout files in the selected "
		    "directory", MFS_ENABLED, 1, &do_getRecursive);
		items++;
	}
	if (items) {
		items = 0;
		AddMenuSeparator(submenu);
	}
	if (inProj && (SEL_BACKGROUND(sel) || SEL_HASRO(sel) || SEL_HASEDITED(sel))) {
		AddMenuItem(submenu, "clean",
		    "Clean Unmodified Files",
		    "bk clean (delete) selected unmodified files",
		    MFS_ENABLED, 1, &do_clean);
		items++;
	}
	if (items) {
		items = 0;
		AddMenuSeparator(submenu);
	}
	if (SEL_PROJROOT(sel) || (inProj && SEL_BACKGROUND(sel)) ||
	    (SEL_SINGLEFILE(sel) &&
	    (SEL_HASRO(sel)  || SEL_HASEDITED(sel) || SEL_HASMODIFIED(sel)))) {
		AddMenuItem(submenu, "revtool",
		    "Revision Tool...",
		    "Launch the revision tool",
		    MFS_ENABLED, 1, &do_revtool);
		items++;
	}
	if (items) {
		items = 0;
		AddMenuSeparator(submenu);
	}
	if (inProj && SEL_HASEXTRAS(sel)) {
		AddMenuItem(submenu, "add",
		    "Add Files",
		    "Add new files to the local repository",
		    MFS_ENABLED, 1, &do_new);
		items++;
	}
	if (inProj && (SEL_HASRO(sel) || SEL_HASEDITED(sel))) {
		AddMenuItem(submenu, "delete",
		    "Delete Files",
		    "Delete selected files from the repository",
		    MFS_ENABLED, 1, &do_rm);
		items++;
	}
	if (inProj && SEL_HASMODIFIED(sel)) {
		AddMenuItem(submenu, "revert",
		    "Revert Changes",
		    "Revert changes in selected files",
		    MFS_ENABLED, 1, &do_unedit);
		items++;
	}
	if (items) {
		items = 0;
		AddMenuSeparator(submenu);
	}

	if ((inProj && SEL_BACKGROUND(sel)) || SEL_PROJROOT(sel)) {
		AddMenuItem(submenu, "changes",
		    "View Local Changesets...",
		    "View the list of changesets to be pushed"
		    " to parent",
		    MFS_ENABLED, 1, &do_changesL);
		AddMenuItem(submenu, "changes",
		    "View Remote Changesets...",
		    "View the list of changesets to be pulled "
		    "from parent",
		    MFS_ENABLED, 1, &do_changesR);
		AddMenuSeparator(submenu);
		AddMenuItem(submenu, "pull",
		    "Pull Changesets from Parent",
		    "Pull changesets from the parent repository",
		    MFS_ENABLED, 1, &do_pull);
		AddMenuItem(submenu, "push",
		    "Push Changesets to Parent",
		    "Push committed changesets to the "
		    "parent repository",
		    MFS_ENABLED, 1, &do_push);
		AddMenuSeparator(submenu);
		AddMenuItem(submenu, "check",
		    "Check for errors...",
		    "Check repository for problems.",
		    MFS_ENABLED, 1, &do_check);
		AddMenuSeparator(submenu);
	}
	if (SEL_BACKGROUND(sel) || SEL_SINGLEDIR(sel)) {
		if (SEL_SINGLEDIR(sel) && SEL_PROJROOT(sel)) {
			AddMenuItem(submenu, "clone",
			    "Clone this Repository...",
			    "Checkout a working copy from a repository",
			    MFS_ENABLED, 1, &do_clone);
		} else {
			AddMenuItem(submenu, "clone",
			    "Clone a Repository...",
			    "Checkout a working copy from a repository",
			    MFS_ENABLED, 1, &do_clone);
		}
		AddMenuItem(submenu, "create",
		    "Create a new Repository Here...",
		    "Create a new repository in the current directory",
		    MFS_ENABLED, 1, &do_setuptool);
		AddMenuSeparator(submenu);
		AddMenuItem(submenu, "console",
		    "Open Command Prompt...",
		    "Open a Windows command prompt in the current directory",
		    MFS_ENABLED, 1, &do_commandprompt);
		AddMenuSeparator(submenu);
	}
	AddMenuItem(submenu, "help",
	    "Help...",
	    "Get help on BitKeeper commands and tools",
	    MFS_ENABLED, 1, &do_help);
	AddMenuItem(submenu, "about",
	    "About BitKeeper plugin...",
	    "About the BitKeeper plugin.",
	    MFS_ENABLED, 1, &do_version);
	AddMenuSeparator(menu);	// end bk area
	return (MAKE_HRESULT(SEVERITY_SUCCESS, 0, _idcmd - idCmdFirst));
}

HMENU
ContextMenuHandler::CreateSubmenu(HMENU hmenu, char *string)
{
	MENUITEMINFO mInfo;

	mInfo.cbSize     = sizeof(MENUITEMINFO);
	mInfo.wID	 = _idcmd++;
	mInfo.fMask      = MIIM_FTYPE|MIIM_STRING|MIIM_SUBMENU|MIIM_ID;
	mInfo.fType      = MFT_STRING;
	mInfo.hSubMenu   = CreatePopupMenu();
	mInfo.dwTypeData = (LPSTR)string;
	InsertMenuItem(hmenu, _index++, true, &mInfo);
	++_offset;
	return (mInfo.hSubMenu);
}

void
ContextMenuHandler::AddMenuItem(HMENU menu, char *icon, char *string,
    char *helpstring, int state, int inSubmenu, mfunc func)
{
	mentry	*m;
	MENUITEMINFO mInfo;

	mInfo.cbSize = sizeof(MENUITEMINFO);
	mInfo.wID    = _idcmd;
	mInfo.fMask  = MIIM_FTYPE|MIIM_ID|MIIM_STRING|MIIM_STATE|MIIM_BITMAP;
	mInfo.fType  = MFT_STRING|MFT_OWNERDRAW;
	mInfo.fState = state;
	mInfo.dwTypeData = (LPSTR)string;
	mInfo.hbmpItem = HBMMENU_CALLBACK;
	InsertMenuItem(menu, _index++, true, &mInfo);

	m = (mentry *)malloc(sizeof(mentry));
	bzero(m, sizeof(mentry));
	m->func = func;
	m->inSubmenu = inSubmenu;
	strncpy(m->icon, icon, sizeof(m->icon));
	strncpy(m->string, string, sizeof(m->string));
	strncpy(m->helpstring, helpstring, sizeof(m->helpstring));
	_itemsByCmd[_idcmd++] = m;
	_itemsByOffset[_offset++] = m;
}

void
ContextMenuHandler::AddMenuSeparator(HMENU menu)
{
	MENUITEMINFO mInfo;

	mInfo.cbSize = sizeof(MENUITEMINFO);
	mInfo.fMask  = MIIM_FTYPE;
	mInfo.fType  = MFT_SEPARATOR;
	InsertMenuItem(menu, _index++, true, &mInfo);
}

STDMETHODIMP
ContextMenuHandler::InvokeCommand(THIS_ LPCMINVOKECOMMANDINFO lpici)
{
	UINT offset;
	offsetmap::iterator it;

	// If lpVerb points to a command string, just bail. We want the offset.
	if (HIWORD(lpici->lpVerb)) return (E_INVALIDARG);

	offset = LOWORD(lpici->lpVerb);
	it = _itemsByOffset.find(offset);
	if (it == _itemsByOffset.end()) return (E_INVALIDARG);
	(*it->second->func)(sel);
	return (S_OK);
}

STDMETHODIMP
ContextMenuHandler::GetCommandString(THIS_ UINT_PTR offset, UINT uType,
    UINT *pwReserved, LPSTR pszName, UINT cchMax)
{
	offsetmap::iterator it = _itemsByOffset.find(offset);
	if (it == _itemsByOffset.end()) return (E_INVALIDARG);
	unless (uType == GCS_HELPTEXTA) return (E_INVALIDARG);
	lstrcpynA(pszName, it->second->helpstring, cchMax);
	return (S_OK);
}

STDMETHODIMP
ContextMenuHandler::HandleMenuMsg(THIS_ UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	LRESULT res;
	return (HandleMenuMsg2(uMsg, wParam, lParam, &res));
}

STDMETHODIMP
ContextMenuHandler::HandleMenuMsg2(THIS_ UINT uMsg, WPARAM wParam,
    LPARAM lParam, LRESULT *plResult)
{
	LRESULT res;
	MEASUREITEMSTRUCT *lpmis;
	DRAWITEMSTRUCT *lpdis;
	itemmap::iterator it;

	unless (plResult) plResult = &res;
	*plResult = false;

	switch (uMsg) {
	    case WM_MEASUREITEM:
		lpmis = (MEASUREITEMSTRUCT*)lParam;
		if (!lpmis || (lpmis->CtlType != ODT_MENU)) break;

		it = _itemsByCmd.find(lpmis->itemID);
		if (it == _itemsByCmd.end()) break;

		lpmis->itemWidth  = 2;
		lpmis->itemHeight = 16;
		if (it->second->inSubmenu) {
			SIZE size;
			HDC hdc = GetDC(NULL);

			GetTextExtentPoint32(hdc, it->second->string,
			    (int)strlen(it->second->string),
			    &size);
			lpmis->itemWidth  = size.cx;
			lpmis->itemHeight = size.cy;
			ReleaseDC(NULL, hdc);
		}
		if (lpmis->itemHeight < 16) lpmis->itemHeight = 16;
		*plResult = true;
		break;
	    case WM_DRAWITEM:
		lpdis = (DRAWITEMSTRUCT*)lParam;
		if ((lpdis==NULL)||(lpdis->CtlType != ODT_MENU)) break;
		unless (lpdis->itemAction & (ODA_DRAWENTIRE|ODA_SELECT)) break;

		it = _itemsByCmd.find(lpdis->itemID);
		if (it == _itemsByCmd.end()) break;

		if (lpdis->itemState & ODS_SELECTED) {
			SetBkColor(lpdis->hDC, GetSysColor(COLOR_HIGHLIGHT));
			SetTextColor(lpdis->hDC,
			    GetSysColor(COLOR_HIGHLIGHTTEXT));
		}
		if (lpdis->itemState & ODS_GRAYED) {
			SetTextColor(lpdis->hDC, GetSysColor(COLOR_GRAYTEXT));
		}

		ExtTextOut(lpdis->hDC, lpdis->rcItem.left + 18,
		    lpdis->rcItem.top, ETO_OPAQUE, &lpdis->rcItem,
		    it->second->string, (int)strlen(it->second->string), NULL);
		if (it->second->icon) {
			DrawIconEx(lpdis->hDC, lpdis->rcItem.left,
			    lpdis->rcItem.top +
			    (lpdis->rcItem.bottom - lpdis->rcItem.top - 16) / 2,
			    GetIcon(it->second->icon),
			    16, 16, 0, NULL, DI_NORMAL | DI_COMPAT);
		}
		*plResult = true;
		break;
	    default:
		break;
	}
	return (NOERROR);
}
