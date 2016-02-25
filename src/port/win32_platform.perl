# Copyright 1999-2000,2016 BitMover, Inc
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

# @(#) %K%
# Platform specific setup for perl scripts
# Copyright (c) 1999 Andrew Chang
#
sub platformInit
{

	$SIG{'TERM'} = $SIG{'INT'} = 'IGNORE';
	$tmp = "$ENV{'TEMP'}\\";
	$tmp =~ s?/?\\?g;	# win32 quirk: redirectded file inside 
				# a open() call must have back slash
	$dev_null = "nul";
	$tty = "con";
	$exe = ".exe";
	$pager = $ENV{'PAGER'} || "less";
	$editor = $ENV{'EDITOR'} || "vi";
	$bk_fs = "\@";

	# needed for -w mode
	if (0) {
	    $tmp = $dev_null = $tty = $pager = $editor = $exe = $bk_fs = "";
	}
}

sub cd2root
{
	$dir = ".";
	while (! -d "$dir/BitKeeper/etc") {
		# XX TODO use driveType interface
		# to get root.
		return if (!defined((stat($dir))[0]));
		$dir = "../" . $dir;
	}
	chdir($dir);
}

# Create process in BackGround
# We need this becuase NT shell do'nt understand the "&" syntax
sub bg_system
{
	use Win32::Process;
	use Win32;
	local($cmd, $args) = ($_[0], $_[1]);
	local($proc);
	$cmd = "$cmd.exe" if (-f "$cmd.exe");
	Win32::Process::Create($proc, $cmd, "$cmd $args" , 0, DETACHED_PROCESS, ".");
}

# Get Process exit status
sub exitStatus
{
	# XXX for some reason the exit status processing
	# is different if we use a binary wrapper.
	#0x00ff & $_[0]; # for perl binary warpper
	$_[0] >> 8;
}

# Simulate a unix exec, new program must run in foreground,
# regular perl exec on win32 resulted in a background process
sub doExec
{
	local ($cmd) = "";	
	local ($bin) = "";	

	foreach (@_) {$cmd = "$cmd $_" };
	exit(system($cmd))
		if (-x "$_[0].exe" || -x "$_[0]");
	foreach (split(/;/, $ENV{'PATH'})) {
		if (-x "$_/$_[0].exe" || -x "$_[0]") {
			$bin = "$_/$_[0]";
			last;
		}
	}
	return -2 if ($bin eq "");
	exit(system($cmd));

}

# Convert path to "standard" format
# This should match the localName2bkName()
# function defined in win32.h
sub localName2bkName
{
	local($path) = $_[0];

	$path =~ s|\\|/|g;
	return $path;
}


# compute absolute path name
sub getAbspath
{
	my $abspath = `getfullpath $_[0]`;

	chomp ($abspath);
	if ($abspath ne "") {
		return ($abspath);
	} else {
		# If we get here,$_[0] is not a existing directory
		# We construct the full path by hand
		my $pwd = `pwd`;
		chomp $pwd;
		return ($pwd . '/' . $1);
	}
}

sub is_windows { 1; }
