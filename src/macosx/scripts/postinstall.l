/*
 * Copyright 2015-2016 BitMover, Inc
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

/*
 * Post-install L script for BitKeeper's OS X package installer.
 * This script is called by the shell script "postinstall" in
 * the same directory.
 */

int	noprompt, rc;
string	err, newbk;
FILE	logf;

void	create_bklink(void);
void	log(FMT fmt, ...args);
void	rm_oldbk(string path);
string	runit(string cmd);

int
main(_argused int ac, _argused string av[], string env{string})
{
	string	buf, c, dotbk, dstroot, oldbk, path, paths[], tmp;
	string	home = env{"HOME"};
	string	user = env{"USER"};
	FILE	f;

	wm("withdraw", ".");
	noprompt = exists("/tmp/_bk_install_no_prompt");

	/*
	 * The OS X installer sets DSTROOT to the path to the install
	 * directory desired by the user. Bail if it's not set.
	 */
	dstroot = getenv("DSTROOT");
	log("bk installer: user ${user} home ${home} "
	    "dstroot ${dstroot} noprompt ${noprompt}");
	unless (dstroot) exit(1);

	/*
	 * Make the user own the entire application bundle. bk
	 * installtool writes the config there.
	 */
	if (user) {
		runit("/usr/sbin/chown -R '${user}' '${dstroot}/BitKeeper.app'");
	}

	newbk = "${dstroot}/BitKeeper.app/Contents/Resources/bitkeeper/bk";
	dotbk = `sudo -u '${user}' '${newbk}' dotbk`;

	/* For the install log. */
	runit("sudo -u '${user}' '${newbk}' version");
	log("dotbk ${dotbk}");

	/*
	 * The preinstall script looks for a pre-existing config file and
	 * copies it to $INSTALLER_TEMP. Look for that.
	 */
	tmp = getenv("INSTALLER_TEMP");

	/*
	 * bk-7.0 puts the config file inside the bundle (since the
	 * bundle wasn't signed). If we found a config inside the
	 * bundle, we append it to the user's dotbk/config and put
	 * it there.
	 */
	if (f = fopen("${tmp}/config", "r")) {
		read(f, &buf);
		fclose(f);
		c = "";
		if (f = fopen("${dotbk}/config", "r")) {
			read(f, &c);
			fclose(f);
		}
		if (buf && (length(buf) > 0)) {
			runit("sudo -u '${user}' mkdir -p '${dotbk}'");
			c .= "\n";
			c .= '# Next section copied from `bk bin`/config';
			c .= "\n" . buf;
			Fprintf("${dotbk}/config", "%s", c);
			runit("chown -R '${user}' '${dotbk}/config'");
		}
	}

	/* Look for old versions of bk that we can uninstall. */
	paths = { "/usr/libexec/bitkeeper", "/usr/local/bitkeeper" };
	foreach (path in paths) {
		if (isdir(path)) rm_oldbk(path);
	}

	/*
	 * Create or update the bk symlink in /usr/bin if
	 * /usr/bin/bk does not exist, or
	 * /usr/bin/bk exists but doesn't point to our install dir.
	 */
	oldbk = runit("/usr/bin/readlink -n /usr/bin/bk");
	unless (oldbk == newbk) {
		create_bklink();
	}

	/* Call exit here, not return. */
	exit(0);
}

void
rm_oldbk(string path)
{
	string	ans, msg;

	unless (noprompt) {
		msg = "An old version of BitKeeper was found in\n${path}\n"
			"Would you like to remove it?";
		ans = tk_messageBox(default: "yes", icon: "info", type: "yesno",
				    title: "Old version of BitKeeper found",
				    message: msg);
		unless (ans == "yes") return;
	}
	unless (runit("/bin/rm -rf '${path}'") || noprompt) {
		msg = "Error ${rc} occurred: ${err}";
		tk_messageBox(default: "ok", icon: "error", type: "ok",
			      title: "Error", message: msg);
	}
}

string
runit(string cmd)
{
	string	out;

	/* Note that err is a global. */
	rc = system(cmd, undef, &out, &err);
	log("${cmd}: ${rc} '${out}' '${err}'");
	if (rc == 0) {
		return (out ? out : "");
	} else {
		return (undef);
	}
}

void
create_bklink(void)
{
	string	msg;
	FILE	f;

	if (File_writable("/usr/bin")) {
		unless (runit("'${newbk}' links /usr/bin")) {
			msg = "Unable to create symbolic link /usr/bin/bk. "
			      "You will need to add ${dirname(newbk)} to "
			      "your PATH manually.";
		}
	} else {
		/*
		 * On el Capitan, /usr/bin is not writable so use
		 * infrastructure provided by path_helper(8).
		 */
		if (f = fopen("/etc/paths.d/10-BitKeeper", "w")) {
			fprintf(f, "${dirname(newbk)}\n");
			fclose(f);
		} else {
			msg = "Unable to write into /etc/paths.d. "
			      "You will need to add ${dirname(newbk)} to "
			      "your PATH manually.";
		}
	}
	if (msg && !noprompt) {
		tk_messageBox(default: "ok", icon: "error", type: "ok",
			      title: "Error", message: msg);
	}
}

void
log(FMT fmt, ...args)
{
	string	d = Clock_format(Clock_seconds(), format: "%Y-%m-%d %H:%M:%S");

	unless (logf) logf = fopen("/tmp/bk-install-log.txt", "w");
	unless (logf) return;
	fprintf(logf, "${d}: ${fmt}\n", (expand)args);
}
