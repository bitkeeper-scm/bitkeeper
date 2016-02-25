/*
 * Copyright 2009,2015-2016 BitMover, Inc
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

#include "../sccs.h"

#ifndef WIN32
private void
reject(void)
{
	fprintf(stderr, "_startmenu: not supported on this platform.\n");
	exit(1);
}

int
__startmenu_generic(void)
{
	reject();
	return (0);
}

void *
__startmenu_generic_ptr(void)
{
	reject();
	return (0);
}

#else

#include <shlobj.h>

/*
 * List of start menu items
 *
 * This list should never shrink (because the uninstall function will
 * attempt to remove each one in turn.)
 * 
 * When a release decides to remove a menu item, switch the enabled
 * field to 0 and the menu item will not be created by install.
 */
private struct smenu {
	char	*menuname;
	char	*target;
	char	*icon;
	char	*cmd;
	char	*wdenv;
	int	enabled;
} menulist[] = {
	{
		.menuname = "BitKeeper",
		.target = "http://bitkeeper.com/start",
		.icon = "%s/bk.ico",
		.cmd = "",
		.wdenv = "HOMEPATH",
		.enabled = 1
	},
	{
		.menuname = "BitKeeper Documentation",
		.target = "%s/bkg.exe",
		.icon = "%s/bk.ico",
		.cmd = "helptool",
		.wdenv = 0,
		.enabled = 1
	},
	/* things from bk-7.0-alpha to delete */
	{
		.menuname = "../BitKeeper",
		.target = "%s/bk.exe",
		.icon = "%s/bk.ico",
		.cmd = "explorer",
		.wdenv = "HOMEPATH",
		.enabled = 0
	},
	{
		.menuname = "../BitKeeper Documentation",
		.target = "%s/bk.exe",
		.icon = "%s/bk.ico",
		.cmd = "helptool",
		.wdenv = 0,
		.enabled = 0
	},
	{
		.menuname = 0
	}
};

char *
bkmenupath(u32 user, int create, int isthere)
{
	LPITEMIDLIST	id;
	int		flag;
	char		*mpath;
	char		pmenupath[MAX_PATH];

	if (user) {
		flag = CSIDL_PROGRAMS;
	} else {
		flag = CSIDL_COMMON_PROGRAMS;
	}
	unless (SHGetSpecialFolderLocation(NULL, flag, &id) == S_OK) {
		fprintf(stderr, "_startmenu: could not find your %s menu.\n",
		    user ? "user start" : "start");
		return (0);
	}
	/* API fills a buffer of minimum Windows MAX_PATH size (260) */
	unless (SHGetPathFromIDList(id, pmenupath)) {
		return (0);
	}

	mpath = aprintf("%s/BitKeeper", pmenupath);
	localName2bkName(mpath, mpath);

	if (create && !exists(mpath) && mkdirp(mpath)) {
		fprintf(stderr, "_startmenu: could not create %s\n", mpath);
		free(mpath);
		return (0);
	}
	if (isthere && !exists(mpath)) {
		free(mpath);
		return (0);
	}
	return (mpath);
}

int
startmenu_set(u32 user, char *menupath, char *target, char *icon, char *args,
	char *cwd)
{
	int		ret = 0;
	char		*tpath = 0, *linkpath = 0;
	IShellLink	*sl = 0;
	IPersistFile	*pf = 0;
	WCHAR		wpath[MAX_PATH];

	CoInitialize(NULL);

	unless (tpath = bkmenupath(user, 1, 1)) return (1);
	linkpath = aprintf("%s/%s.lnk", tpath, menupath);

	if (exists(linkpath)) {
		fprintf(stderr, "_startmenu: %s already exists\n", linkpath);
		ret = 1;
		goto out;
	}

	if (CoCreateInstance(&CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER,
	    &IID_IShellLink, (LPVOID *) &sl) != S_OK) {
		fprintf(stderr,
		    "_startmenu: CoCreateInstance fails, error %ld\n",
		    GetLastError());
		ret = 1;
		goto out;
	}

	if (sl->lpVtbl->QueryInterface(sl,
	    &IID_IPersistFile, (void **)&pf) != S_OK) {
		fprintf(stderr,
		    "_startmenu: QueryInterface failed, error %ld\n",
		    GetLastError());
		ret = 1;
		goto out;
	}

	sl->lpVtbl->SetPath(sl, target);
	sl->lpVtbl->SetArguments(sl, args);
	sl->lpVtbl->SetIconLocation(sl, icon, 0);
	if (cwd) sl->lpVtbl->SetWorkingDirectory(sl, cwd);

	MultiByteToWideChar(CP_ACP, 0, linkpath, -1, wpath, MAX_PATH);
	if (pf->lpVtbl->Save(pf, wpath, TRUE) != S_OK) {
		fprintf(stderr,
		    "_startmenu: Save failed, error %ld\n", GetLastError());
		ret = 1;
		goto out;
	}

out:
	if (pf) pf->lpVtbl->Release(pf);
	if (sl) sl->lpVtbl->Release(sl);
	if (tpath) free(tpath);
	if (linkpath) free(linkpath);
	CoUninitialize();
	return (ret);
}

int
startmenu_get(u32 user, char *menu)
{
	HRESULT		res;
	int		idx, ret = 0;
	char		*tpath = 0, *linkpath = 0;
	IShellLink	*sl = 0;
	IPersistFile	*pf = 0;
	WCHAR		wpath[MAX_PATH];
	char		target[MAX_PATH];
	char		iconpath[MAX_PATH];
	char		args[MAX_PATH];

	CoInitialize(NULL);

	unless (tpath = bkmenupath(user, 0, 1)) return (1);
	linkpath = aprintf("%s/%s.lnk", tpath, menu);

	if (!exists(linkpath)) {
		fprintf(stderr, "_startmenu: %s not found\n", linkpath);
		ret = 1;
		goto out;
	}

	if (CoCreateInstance(&CLSID_ShellLink, NULL,
	    CLSCTX_INPROC_SERVER, &IID_IShellLink, (LPVOID *) &sl) != S_OK) {
		fprintf(stderr,
		    "_startmenu: CoCreateInstance failed, error %ld\n",
		    GetLastError());
		ret = 1;
		goto out;
	}

	if (sl->lpVtbl->QueryInterface(sl,
	    &IID_IPersistFile, (void **)&pf) != S_OK) {
		fprintf(stderr,
		    "_startmenu: QueryInterface failed, error %ld\n",
		    GetLastError());
		ret = 1;
		goto out;
	}

	MultiByteToWideChar(CP_ACP, 0, linkpath, -1, wpath, MAX_PATH);
	if (pf->lpVtbl->Load(pf, wpath, STGM_READ) != S_OK) {
		fprintf(stderr, "_startmenu: Load failed, error %ld\n",
		    GetLastError());
		ret = 1;
		goto out;
	}

#if 0
	res = sl->lpVtbl->Resolve(sl, 0, SLR_NO_UI);
	if (res != S_OK) {
		fprintf(stderr, "_startmenu: Resolve failed, error %ld\n",
		    GetLastError());
		ret = 1;
		goto out;
	}
#endif

	/*
	 * Ugh...  this will get the target if it's a file but not an URL
	 * if the target is an URL then GetPath() returns S_FALSE and a
	 * null string.  Sigh.
	 */
	res = sl->lpVtbl->GetPath(sl, target, MAX_PATH, NULL, SLGP_UNCPRIORITY);
	if (res == S_FALSE) {
		printf("\"%s\" points at a URL which we cannot extract\n", menu);
		/* not an error, no need to set ret */
		goto out;
	} else if (res != S_OK) {
		fprintf(stderr, "_startmenu: GetPath failed, error %ld\n",
		    GetLastError());
		ret = 1;
		goto out;
	}

	if (sl->lpVtbl->GetIconLocation(sl, iconpath, MAX_PATH, &idx) != S_OK){
		fprintf(stderr,
		    "_startmenu: GetIconLocation failed, error %ld\n",
		    GetLastError());
		ret = 1;
		goto out;
	}

	if (sl->lpVtbl->GetArguments(sl, args, MAX_PATH) != S_OK) {
		fprintf(stderr,
		    "_startmenu: GetArguments failed, error %ld\n",
		    GetLastError());
		ret = 1;
		goto out;
	}

	if (user) printf("-u ");
	if (iconpath && strlen(iconpath)) printf("-i\"%s\" ", iconpath);
	printf("\"%s\" ", menu);
	printf("\"%s\" ", target);
	if (strlen(args)) printf("\"%s\"", args);
	printf("\n");

out:
	if (pf) pf->lpVtbl->Release(pf);
	if (sl) sl->lpVtbl->Release(sl);
	if (tpath) free(tpath);
	if (linkpath) free(linkpath);
	CoUninitialize();
	return (ret);
}

int
startmenu_rm(u32 user, char *menu)
{
	int	ret = 0;
	char	*tpath = 0, *linkpath = 0;

	CoInitialize(NULL);

	assert(menu);
	unless (tpath = bkmenupath(user, 0, 1)) {
		ret = 1;
		goto out;
	}
	linkpath = aprintf("%s/%s.lnk", tpath, menu);

	if (exists(linkpath) && unlink(linkpath)) {
		fprintf(stderr,
		    "_startmenu: %s: unlink failed, error %ld\n",
		    linkpath, GetLastError());
		ret = 1;
	}

out:
	if (linkpath) free(linkpath);
	if (tpath) free(tpath);
	CoUninitialize();
	return (ret);
}

int
startmenu_list(u32 user, char *menu)
{
	char	**files;
	int	i;
	int	ret = 0;
	char	*tpath = 0, *dirpath = 0, *linkpath = 0;

	CoInitialize(NULL);

	unless (tpath = bkmenupath(user, 0, 1)) return (1);
	if (menu) {
		dirpath = aprintf("%s/%s", tpath, menu);
	} else {
		dirpath = strdup(tpath);
	}
	linkpath = aprintf("%s.lnk", dirpath);

	if (exists(linkpath)) {
		printf("%s\n", linkpath);
		ret = 1;
		goto out;
	} else if (!exists(dirpath)) {
		fprintf(stderr, "_startmenu: %s: not found\n", dirpath);
		ret = 1;
		goto out;
	} if (!isdir(dirpath)) {
		fprintf(stderr,
		    "_startmenu: %s: exists but is not a directory\n",
		    dirpath);
		ret = 1;
		goto out;
	}

	files = getdir(dirpath);
	EACH (files) {
		printf("%s\n", files[i]);
	}
	freeLines(files, free);

out:
	if (tpath) free(tpath);
	if (dirpath) free(dirpath);
	if (linkpath) free(linkpath);
	CoUninitialize();
	return (ret);
}

void
startmenu_install(char *dest)
{
	struct smenu	*smp;
	char		*target, *icon, *t, *home = 0;
	char		buf[MAXPATH];

	/*
	 * This hack is necessary because our MSYS mashes
	 * HOMEPATH to be \
	 *
	 * If, in a command prompt window:
	 *
	 *   echo %HOMEPATH%
	 *   bk sh -c "echo $HOMEPATH"
	 *
	 * yield similar results then it may be OK to remove.
	 */
	if (SUCCEEDED(SHGetFolderPath(0, CSIDL_PROFILE, 0, 0, buf))) {
		home = buf;
	}

	for (smp = menulist; smp->menuname; smp++) {
		unless (smp->enabled) continue;
		target = aprintf(smp->target, dest);
		icon = aprintf(smp->icon, dest);
		if (smp->wdenv && streq(smp->wdenv, "HOMEPATH")) {
			t = home;
		} else if (smp->wdenv) {
			t = getenv(smp->wdenv);
		} else {
			t = 0;
		}
		startmenu_set(0, smp->menuname, target, icon, smp->cmd, t);
		free(icon);
		free(target);
	}
}

void
startmenu_uninstall(FILE *log)
{
	int		i;
	char		*bkmenu;
	struct smenu	*smp;
	char		buf[MAXPATH];

	/* Make win32 layer be quiet and not retry */
	win32flags_clear(WIN32_RETRY | WIN32_NOISY);
	/*
	 * Remove *all* Start Menu shortcuts
	 * (including any legacy items)
	 */
	for (i = 0; i < 2; i++) {
		/*
		 * Remove everything in our list
		 * (there may be items outside our BitKeeper
		 * subdir)
		 */
		for (smp = menulist; smp->menuname; smp++) {
			startmenu_rm(i, smp->menuname);
		}
		/*
		 * Prior to 7.0 alpha, and now, things go in a
		 * BitKeeper subdir; nuke it -- should get anything
		 * missed by the above loop (which will be a bunch
		 * of things from bk versions <= 6)
		 */
		unless (bkmenu = bkmenupath(i, 0, 1)) continue;
		if (!isdir(bkmenu) || !rmtree(bkmenu)) goto next;

		/* rmtree failed; try renaming and deleting on reboot */
		sprintf(buf, "%s.old%d", bkmenu, getpid());
		if (rename(bkmenu, buf)) {
			/* hmm, rename failed too */
			fprintf(stderr,
			    "Could not delete or rename BitKeeper "
			    "start menu directory:\n%s\n",
			    bkmenu);
			if (log) fprintf(log,
			    "Could not delete or rename BitKeeper "
			    "start menu directory:\n%s\n",
			    bkmenu);
			goto next;
		}
		fprintf(stderr,
		    "Could not delete BitKeeper start menus:\n"
		    "\t%s\nWill be deleted on next reboot.\n",
		    buf);
		if (log) fprintf(log,
		    "Could not delete BitKeeper start menus:\n"
		    "\t%s\nWill be deleted on next reboot.\n",
		    buf);
		delete_onReboot(buf);
 next:		free(bkmenu);
	}
}

#endif	/* WIN32 */
