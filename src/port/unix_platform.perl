# Copyright 1999-2000,2015-2016 BitMover, Inc
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# @(#) awc@etp2.bitmover.com|src/port/unix_platform.perl|19991025205424
# Platform specific setup for perl scripts
# Copyright (c) 1999 Andrew Chang

sub platformInit
{
	$SIG{'HUP'} = $SIG{'TERM'} = $SIG{'INT'} = 'IGNORE';
	$tmp = "/tmp/";
	$dev_null = "/dev/null";
	$tty = "/dev/tty";
	$pager = $ENV{'PAGER'} || "more";
	$editor = $ENV{'EDITOR'} || "vi";
	$exe = "";   # win3 has ".exe" extension for executable, unix does'nt
	$bk_fs = "\@";

	# needed for -w mode
	if (0) {
	    $tmp = $dev_null = $tty = $pager = $editor = $exe = $bk_fs = "";
	}
}

sub cd2root
{
	local($dir, $slash);
	$slash = (stat("/"))[1];
	$dir = ".";
	while (! -d "$dir/BitKeeper/etc") {
		return if (stat($dir))[1] == $slash;
		$dir = "../" . $dir;
	}
	chdir($dir);
}

# create process in the background
sub bg_system
{
	local ($cmd, $args) = ($_[0], $_[1]);

	system("$cmd $args &");
}

# Get Process exit status
sub exitStatus
{
	$_[0] >> 8;
}

sub doExec
{
	local ($bin) = "";

	exec @_ if (-x "$_[0]");
	foreach (split(/:/, $ENV{'PATH'})) {
		if (-x "$_/$_[0]") {
			$bin = "$_/$_[0]";
			last;
		}
	}
	if ($bin eq "") {
		warn "Could not find $_[0]\n";
		return -2;
	}
	exec @_;
	# I'd like to return -1 here but perl doesn't like that.
}

# Convert path to "standard" format
# On unix, this is a no-op
# This should match the localName2bkName()
# function defined in unix.h
sub localName2bkName
{
        return $_[0];
}


# compute absolute path name
sub getAbspath
{
	local($abspath);
	$abspath = `cd $_[0] 2>/dev/null && pwd`;
	chop $abspath;
	if ($abspath ne "") {
		return ($abspath);
	} else {
		# If we get here,$_[0] is not a existing directory
		# We construct the full path by hand
		$abspath = `pwd`;
		chop $pwd;
		return ($pwd . '/' . $_[0]);
	}
}

# $^O was only added in perl 5.
sub is_windows { 0; }

# perl 4 bitches about unused functions.
sub these_functions_are_not_unused_so_shaddap
{
	&cd2root;
	&bg_system;
	&exitStatus;
	&doExec;
	&localName2bkName;
	&getAbspath;
	&is_windows;
	&these_functions_are_not_unused_so_shaddap;
}
