#include "system.h"

#ifndef WIN32
void
reject(void)
{
	fprintf(stderr, "_startmenu: not supported on this platform.\n");
	exit(1);
}

int
startmenu_list(u32 user, char *path)
{
	reject();
	return (0);
}

int
startmenu_rm(u32 user, char *path)
{
	reject();
	return (0);
}

int
startmenu_get(u32 user, char *path)
{
	reject();
	return (0);
}

int
startmenu_set(u32 user, char *linkpath, char *target, char *args)
{
	reject();
	return (0);
}
#else

#include <shlobj.h>

private char *
bkmenupath(u32 user, int create)
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
	SHGetSpecialFolderLocation(NULL, flag, &id);
	SHGetPathFromIDList(id, pmenupath);

	mpath = aprintf("%s/BitKeeper", pmenupath);
	localName2bkName(mpath, mpath);

	if (create && !exists(mpath) && mkdirp(mpath)) {
		fprintf(stderr, "_startmenu: could not create %s\n", mpath);
		free(mpath);
		return (0);
	}
	if (!exists(mpath)) {
		free(mpath);
		return (0);
	}
	return (mpath);
}

int
startmenu_set(u32 user, char *menupath, char *target, char *icon, char *args)
{
	int		ret = 0;
	char		*tpath = 0, *linkpath = 0;
	IShellLink	*sl = 0;
	IPersistFile	*pf = 0;
	WCHAR		wpath[MAX_PATH];

	CoInitialize(NULL);

	unless (tpath = bkmenupath(user, 1)) return (1);
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

	unless (tpath = bkmenupath(user, 0)) return (1);
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
	char	*tpath = 0, *linkpath = 0, *dirpath = 0;

	CoInitialize(NULL);

	unless (tpath = bkmenupath(user, 0)) return (1);
	if (menu) {
		dirpath = aprintf("%s/%s", tpath, menu);
	} else {
		dirpath = strdup(tpath);
	}
	linkpath = aprintf("%s.lnk", dirpath);

	if (isdir(dirpath)) {
		if (rmtree(dirpath)) {
			fprintf(stderr,
			    "_startmenu: %s: rmtree failed, error %ld\n",
			    dirpath, GetLastError());
			ret = 1;
		}
	} else if (unlink(linkpath)) {
		fprintf(stderr,
		    "_startmenu: %s: unlink failed, error %ld\n",
		    linkpath, GetLastError());
		ret = 1;
	}
	if (dirpath) free(dirpath);
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

	unless (tpath = bkmenupath(user, 0)) return (1);
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

#endif	/* WIN32 */
