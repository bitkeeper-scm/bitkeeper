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
	$pager = $ENV{'PAGER'} || "less";
	$editor = $ENV{'EDITOR'} || "vim";
	$dev_null = "nul" if 0;
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

# Get Process exit status
sub exitStatus
{
	0x00ff & $_[0];
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


sub cpio_out
{
	local($q, $list) = ($_[0], $_[1]);

	# win32 note: can not re-direct stderr
	# must use "--quiet" option
	$cpioq = $q ? ' --quiet' : ' -v';
	system("${BIN}cpio -o -Hcrc $cpioq < $list")
		and die "cpio unsuccessful exit $?\n";
}

sub cpio_in
{
	# Can not use -m option yet, need to fix cpio to
	# close file before chmod()
	system("${BIN}cpio -id --quiet")
		and die "cpio exited unsuccessfully\n";
}
