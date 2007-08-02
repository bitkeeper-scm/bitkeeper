#include "../sccs.h"

private	FILE *dfd;	/* Debug FD */

#ifdef WIN32

#include <shlobj.h>

#define	HKLM		"HKEY_LOCAL_MACHINE"
#define	HKCU		"HKEY_CURRENT_USER"
#define	ARPCACHEKEY	HKLM "\\Software\\Microsoft\\Windows" \
			"\\CurrentVersion\\App Management\\ARPCache"
#define	BMKEY		HKLM "\\Software\\BitMover"
#define BKKEY		HKLM "\\Software\\BitMover\\BitKeeper"
#define UNINSTALLKEY	HKLM "\\Software\\Microsoft\\Windows" \
			"\\CurrentVersion\\Uninstall"
#define	RUNONCEKEY	HKLM "\\Software\\Microsoft\\Windows" \
			"\\CurrentVersion\\RunOnce"
#define	SCCPKEY		HKLM "\\Software\\SourceCodeControlProvider"
#define	SCCBUSTEDKEY	HKLM "\\Software\\SourceControlProvider"
#define	SYSENVKEY	HKLM "\\System\\CurrentControlSet" \
			"\\Control\\Session Manager\\Environment"
#define	USRENVKEY	HKCU "\\Environment"

private void	delete_onReboot(char *path);
private	char	*path_sansBK(char *path);
private void	remove_shortcuts(void);
private void	unregister_shellx(char *path);

int
uninstall(char *path, int upgrade)
{
	char	*data, *envpath, *old_ver, *uninstall_cmd;
	char	**keys = 0, **values = 0;
	int	i;
	char	buf[MAXPATH];

	/* set up logging */
	dfd = efopen("BK_UNINSTALL_LOG");

	if (dfd) fprintf(dfd, "Uninstalling BK at %s\n", path);

	/* First get the uninstaller for the installed version */
	if (old_ver = reg_get(BKKEY, "rel", 0)) {
		if (dfd) fprintf(dfd, "Old version = %s\n", old_ver);
		sprintf(buf, "%s\\%s", UNINSTALLKEY, old_ver);
		if (uninstall_cmd = reg_get(buf, "UninstallString", 0)) {
			if (dfd) fprintf(dfd,
			    "Uninstall cmd = %s\n", uninstall_cmd);

#if 0
			/*
			 * This bit of code should be: if bk version > 3.2.5
			 * and we're upgrading: stop services and the installer
			 * needs to restart them. If we're just uninstalling,
			 * then delete services. -ob
			 */
			if (strcmp(old_ver, "bk-3.2.5") > 0) {
				/*
				 * The 'bk service' API was introduced in
				 * bk-3.2.5.  if they're running anything
				 * later, we can stop, save, and restart the
				 * services
				 */
				saved_services = 1;
				save_services();
				delete_services();
			}
#endif
			/* If the uninstaller is bkuninstall, don't even
			 * bother running it.  We can do a much better job at
			 * uninstalling.
			 */
			unless (strstr(uninstall_cmd, "bkuninstall")) {
				if (strstr(uninstall_cmd, "UNWISE.EXE")) {
					/* XXX: I don't know how the WISE
					 * installer is supposed to work.
					 * I'll just run it here but
					 * still don't trust it.
					 */
					if (system(uninstall_cmd)) {
						if (dfd) fprintf(dfd,
						    "Failed to run %s\n",
						    uninstall_cmd);
					}
				}
			}
			free(uninstall_cmd);
		}
		/* Unregister ShellX DLL */
		unregister_shellx(path);
		free(old_ver);
	}

	/* get a temporary pathname */
	sprintf(buf, "%s.old%d", path, getpid());
	if (dfd) fprintf(dfd, "moving %s -> %s\n", path, buf);
	if (rename(path, buf)) {
		/* punt! we won't be able to uninstall */
		fprintf(stderr, "Could not uninstall BitKeeper "
		    "at %s\n", path);
		if (dfd) fprintf(dfd, "Could not move %s to %s\nexiting.\n",
		    path, buf);
		if (dfd) fclose(dfd);
		return (1);
	}
	/* Make win32 layer be quiet and not retry */
	win32flags_clear(WIN32_RETRY | WIN32_NOISY);
	if ((path == bin) || rmtree(buf)) {
		/* schedule it to be removed on reboot */
		fprintf(stderr, "Could not delete %s: will be deleted on the "
		    "next reboot.\n", buf);
		if (dfd) fprintf(dfd, "Could not delete %s: "
		    "will be deleted on the next reboot.\n", buf);
		delete_onReboot(buf);
	}

	/* Search for all uninstall entries and blow them away */
	if (keys = reg_keys(UNINSTALLKEY)) {
		EACH(keys) {
			if (strneq(keys[i], "bk-", 3)) {
				sprintf(buf, "%s\\%s", UNINSTALLKEY, keys[i]);
				if (dfd) {
					fprintf(dfd,
					    "Deleting Registry %s...", buf);
				}
				if (reg_delete(buf, 0)) {
					if (dfd) fprintf(dfd, "ok\n");
				} else {
					if (dfd) fprintf(dfd, "failed\n");
				}
			}
		}
		freeLines(keys, free);
	}

	/* We don't want to blow away the registry on upgrades since the
	 * user might have customized it
	 */
	if (upgrade) {
		if (dfd) fprintf(dfd, "Upgrading, so my work here is done\n");
		goto out;
	}

	if (dfd) fprintf(dfd, "Cleaning PATHs\n");
	/* remove BK from user and system PATH */
	if (envpath = reg_get(USRENVKEY, "Path", 0)) {
		reg_set(USRENVKEY, "Path", REG_EXPAND_SZ,
		    path_sansBK(envpath), 0);
		if (dfd) fprintf(dfd,
		    "Updated User PATH: %s\n", envpath);
		free(envpath);
	}
	if (envpath = reg_get(SYSENVKEY, "Path", 0)) {
		reg_set(SYSENVKEY, "Path", REG_EXPAND_SZ,
		    path_sansBK(envpath), 0);
		if (dfd) fprintf(dfd,
		    "Updated System PATH: %s\n", envpath);
		free(envpath);
	}

	if (dfd) fprintf(dfd, "Broadcasting to environment\n");
	reg_broadcast("Environment", 0);
	/* clean up registry */
	if (dfd) fprintf(dfd, "Deleting Registry %s\n", BKKEY);
	/* Blow away BitMover key */
	if (reg_delete(BMKEY, 0)) {
		if (dfd) fprintf(dfd, "Could not delete %s\n", BMKEY);
	}
	/* Search for all Scc plugin entries and delete them */
	if (data = reg_get(SCCPKEY, "ProviderRegKey", 0)) {
		if (match_one(data, "*bitkee*", 1)) {
			if (reg_delete(SCCPKEY, "ProviderRegKey")) {
				if (dfd) {
					fprintf(dfd,
					    "Could not delete key %s\n",
					    SCCPKEY " ProviderRegKey");
				}
			}
		}
		free(data);
	}
	if (values = reg_values(SCCPKEY "\\InstalledSCCProviders")) {
		EACH(values) {
			if (match_one(values[i], "*bitkee*", 1)) {
				if (reg_delete(SCCPKEY
				    "\\InstalledSCCProviders", values[i])) {
					if (dfd) {
						fprintf(dfd,
						    "Could not delete "
						    "value %s %s\n",
						    SCCPKEY
						    "\\InstalledSCCProviders",
						    values[i]);
					}
				}
			}
		}
		freeLines(values, free);
	}
	if (values = reg_values(SCCPKEY "\\InstalledSCCProviders")) {
		EACH(values) {
			if (match_one(values[i], "*bitkee*", 1)) {
				if (reg_delete(SCCPKEY
				    "\\InstalledSCCProviders", values[i])) {
					if (dfd) {
						fprintf(dfd,
						    "Could not delete "
						    "value %s %s\n",
						    SCCPKEY
						    "\\InstalledSCCProviders",
						    values[i]);
					}
				}
			}
		}
		freeLines(values, free);
	}
	/* blow away a busted key that we introduced in bk-4.0.1 */
	if (keys = reg_keys(SCCBUSTEDKEY)) {
		if (reg_delete(SCCBUSTEDKEY, 0)) {
			if (dfd) fprintf(dfd,
			    "Could not delete key %s\n", SCCBUSTEDKEY);
		}
		freeLines(keys, free);
	}
	/* Other registry entries?? */
	if (keys = reg_keys(ARPCACHEKEY)) {
		EACH(keys) {
			if (strneq(keys[i], "bk-", 3)) {
				sprintf(buf, "%s\\%s", ARPCACHEKEY, keys[i]);
				if (dfd) {
					fprintf(dfd,
					    "Deleting Registry %s...", buf);
				}
				if (reg_delete(buf, 0)) {
					if (dfd) fprintf(dfd, "ok\n");
				} else {
					if (dfd) fprintf(dfd, "failed\n");
				}
			}
		}
		freeLines(keys, free);
	}
	/* Remove Start Menu shortcuts */
	remove_shortcuts();
	/* Finally remove the BitKeeper_nul file creted by getnull.c */
	if (GetTempPath(sizeof(buf), buf)) {
		strcat(buf, "/BitKeeper_nul");
		if (unlink(buf)) {
			if (dfd) fprintf(dfd, "Could not remove %s\n", buf);
		}
	}
out:
	if (dfd) fclose(dfd);
	return (0);
}

private	char *
path_sansBK(char *path)
{
	char	**components = 0;
	char	*p;
	int	i;

	components = splitLine(path, ";", 0);
	EACH(components) {
		/* We use bitkee instead of bitkeeper due to short paths.
		 * E.g. c:\progra~1\bitkee~1
		 */
		if (match_one(components[i], "*bitkee*", 1) ||
		    patheq(components[i], bin)) {
			removeLineN(components, i, free);
		}
	}
	path[0] = 0;
	if (p = joinLines(";", components)) {
		strcpy(path, p);
		free(p);
	}
	freeLines(components, free);
	return (path);
}

private void
delete_onReboot(char *path)
{
	char	*id, *cmd;

	id = aprintf("BitKeeper%d", getpid());
	cmd = aprintf("cmd.exe /C RD /S /Q \"%s\"", path);
	reg_set(RUNONCEKEY, id, 0, cmd, 0);
	free(id);
	free(cmd);
}

private void
remove_shortcuts(void)
{
	LPITEMIDLIST	sfolder;
	int		i;
	int		ids[2] = {CSIDL_STARTMENU, CSIDL_COMMON_STARTMENU};
	char		fpath[MAX_PATH];
	char		buf[MAX_PATH];

	for (i = 0; i < 2; i++) {
		if (SHGetSpecialFolderLocation(0, ids[i], &sfolder) == S_OK) {
			if (SHGetPathFromIDList(sfolder, fpath)) {
				sprintf(buf, "%s\\%s", fpath,
				    "Programs\\BitKeeper");
				if (exists(buf)) {
					if (dfd) {
						fprintf(dfd,
						    "Removing Shortcut %s\n",
						    buf);
					}
					rmtree(buf);
				}
			}
		}
	}
}

private void
unregister_shellx(char *path)
{
	char	*cmd;

	cmd = aprintf("regsvr32.exe /u /s \"%s\\%s\"", path, "bkshellx.dll");
	if (system(cmd)) {
		fprintf(stderr, "failed to unregister shellx\n");
	}
	free(cmd);
}

#else

private	int rmlink(char *link, char *path);

/*
 * The uninstall for unix is basically rm -rf <DEST>
 */
int
uninstall(char *path, int upgrade)
{
	struct	stat	statbuf;
	char	**dirs = 0, **links = 0;
	char	*buf = 0;
	char	*me = "bk";
	char	*cmd;
	int	rc = 1;
	int	dobk = 0;
	int	i, j;

	/* set up logging */

	dfd = efopen("BK_UNINSTALL_LOG");

	if (chdir(path) || !(dirs = getdir("."))) {
		fprintf(stderr,
		    "You do not have permission cd to or read %s\n", path);
		if (dfd)fprintf(dfd,
		    "You do not have permission cd to or read %s\n", path);
		goto out;
	}
	if (lstat(".", &statbuf)) {
		perror(".");
		goto out;
	}
	if ((statbuf.st_mode & 0700) != 0700) {
		chmod(".", statbuf.st_mode | 0700);
	}
	EACH(dirs) {
		if (streq(dirs[i], me)) {
			dobk = 1;
			continue;
		}
		if (rmtree(dirs[i])) {
			fprintf(stderr,
			    "Could not remove some dirs in %s\n", dirs[i]);
			if (dfd) fprintf(dfd,
			    "Could not remove some dirs in %s\n", dirs[i]);
			free(dirs);
			goto out;
		}
	}
	free(dirs);
	dirs = 0;
	if (dobk) {
		if ((unlink)(me)) {
			unless (errno == ETXTBSY) {
				fprintf(stderr,
				    "Could not remove %s in %s\n", me, path);
				if (dfd) fprintf(dfd,
				    "Could not remove %s in %s\n", me, path);
				goto out;
			}
			/*
			 * launch a background process to remove 'bk'
			 * after this process dies.
			 */
			cmd = aprintf("( while kill -0 %lu; do sleep 1; done; "
			    "rm -f \"%s\"; cd ..; rmdir \"%s\" ) "
			    " > /dev/null 2> /dev/null &",
			    (long)getpid(), me, path);
			(system)(cmd);
			free(cmd);
		}
	}
	chdir("..");
	rmdir(path);	/* okay if it fails */

	if (upgrade) {
		/* preserve user's symlinks as they might be in a
		 * different path than /usr/bin
		 */
		rc = 0;
		goto out;
	}
	/*
	 * search for bk as a symlink in PATH and try removing all the
	 * symlinks that we installed
	 */
	dirs = splitLine(getenv("BK_OLDPATH"), ":", dirs);
	links = splitLine("admin get delta unget rmdel prs", " ", links);
	EACH(dirs) {
		buf = aprintf("%s/bk", dirs[i]);
		unless (isSymlnk(buf)) continue;
		if (dfd) fprintf(dfd, "trying %s...", buf);
		if (!rmlink(buf, path)) {
			if (dfd) fprintf(dfd, "removing\n");
			free(buf);
			EACH_INDEX(links, j) {
				buf = aprintf("%s/%s", dirs[i], links[j]);
				if (dfd) fprintf(dfd, "removing %s...", buf);
				if (rmlink(buf, path)) {
					if (dfd) fprintf(dfd, "failed\n");
				} else {
					if (dfd) fprintf(dfd, "removed\n");
				}
				free(buf);
			}
		} else {
			if (dfd) fprintf(dfd, "not a symlink to this bk\n");
			free(buf);
		}
	}
	rc = 0;
out:
	if (dfd) fclose(dfd);
	return (rc);
}

/* Remove a symlink only if it points to a given bkpath */
private int
rmlink(char *link, char *bkpath)
{
	char	buf[MAXPATH];
	char	*dir;
	struct	stat sb;
	int	len;

	if (lstat(link, &sb)) return (-1);
	unless (S_ISLNK(sb.st_mode)) return (-1);
	len = readlink(link, buf, sizeof(buf));
	if ((len == -1) || (len >= sizeof(buf))) return (-1);
	buf[len] = 0;
	dir = dirname(buf);
	unless (streq(dir, bkpath)) return (-1);
	return (unlink(link));
}
#endif
