#
# %W%  Copyright (c) 1999 Andrew Chang
#
sub platformInit
{
	local($bin) = $_[0];

	$SIG{'TERM'} = $SIG{'INT'} = 'IGNORE';
	# must use "C:/" syntax for PATH
	#$unix_bin=$ENV{'UNIX_BIN'};
	#$ENV{'PATH'} = "$bin;$unix_bin;$ENV{'PATH'}";
	$tmp = "$ENV{'TEMP'}\\";
	$tmp =~ s?/?\\?g;	# win32 quirk: redirectded file inside 
				# a open() call must have back slash
	$dev_null = "nul";
	$tty = "con";
	$pager = $ENV{'PAGER'} || "less";
	$editor = $ENV{'EDITOR'} || "vim";
}

sub cd2root
{
	$dir = ".";
	while (! -d "$dir/BitKeeper/etc") {
		# XX TODO use driveType interface
		# to get root.
		last if (!defined((stat($dir))[0]));
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

# Simulate a unix exec, new program must run in foreground,
# regular perl exec on win32 resulted in a background process
sub doExec
{
	local ($cmd) = "";	
	foreach (@_) {$cmd = "$cmd $_" };
	system($cmd);
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

return 1;

