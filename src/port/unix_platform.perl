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

# The nonsense with /dev/null is because there is no portable way to
# stop cpio from printing "42 blocks" at the end of its run.
sub cpio_out
{
	local($q, $list) = ($_[0], $_[1]);

	$cpioq = $q ? ' 2>/dev/null' : 'v';
	system("cpio -oc$cpioq < $list")
		&& die "cpio -o: unsuccessful exit $?\n";
}

sub cpio_in
{
	system("cpio -icdm 2>/dev/null") 
		&& die "cpio -i: unsuccessful exit $?\n";
}

