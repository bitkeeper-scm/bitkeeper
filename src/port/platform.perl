#
# %W% Copyright (c) 1999 Andrew Chang
#
sub platformInit
{
	local($bin) = $_[0];

	$SIG{'HUP'} = $SIG{'TERM'} = $SIG{'INT'} = 'IGNORE';
	$ENV{'PATH'} = "$bin:$ENV{'PATH'}";
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

return 1;

