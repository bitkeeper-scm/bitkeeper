# @(#) %K%
# Platform specific setup for perl scripts
# Copyright (c) 1999 Andrew Chang

sub platformInit
{

	$SIG{'HUP'} = $SIG{'TERM'} = $SIG{'INT'} = 'IGNORE';
	$tmp = "/tmp/";
	$tty = "/dev/tty";
	$pager = $ENV{'PAGER'} || "more";
	$editor = $ENV{'EDITOR'} || "vi";
}

sub cd2root
{
	$slash = (stat("/"))[1];
	$dir = ".";
	while (! -d "$dir/BitKeeper/etc") {
		last if (stat($dir))[1] == $slash;
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

# Unix exec
# We need this becuase perl exec on win32 runs new program in "background"
sub doExec
{
	exec @_;
	die "doExec: exec failed: $!\n"; 
}

# Convert path to "standard" format
# On unix, this is a no-op
# This should match the localName2bkName()
# function defined in unix.h
sub localName2bkName
{
        return $_[0];
}        

# PORTABILITY NOTE: cpio -Hcrc is a SVR4ism.  We have
# to use it because only that format can handle huge
# filesystems.  We attempt to fall back to -c, then to
# the *really* obsolete default format.  The nonsense
# with /dev/null is because there is no portable way
# to stop cpio from printing "42 blocks" at the end of
# its run.  Thankfully, the destination end doesn't need
# any of this junk except the /dev/null bit.
sub cpio_out
{
	local($q, $list) = ($_[0], $_[1]);

	$cpioq = $q ? ' 2>/dev/null' : ' -v';
	system("cpio -o -Hcrc $cpioq < $list")
	and system("cpio -o -c $cpioq < $list")
	and system("cpio -o $cpioq < $list")
	and die "cpio unsuccessful exit $?\n";
}

sub cpio_in
{
	system("cpio -idm 2>/dev/null") 
		and die "cpio exited unsuccessfully\n";
}

