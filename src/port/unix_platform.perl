# @(#) %K%
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

	# needed for -w mode
	if (0) { $tmp = $dev_null = $tty = $pager = $editor = $exe = ""; }
}

sub cd2root
{
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
	my $abspath = `cd $_[0] 2>/dev/null && pwd`;

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
