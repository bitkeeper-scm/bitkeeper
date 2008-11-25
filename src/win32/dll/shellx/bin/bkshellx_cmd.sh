PS1="\[\033]0;\w\007\033[32m\]\u@\h \[\033[33m\w\033[0m\]\n$ "
if [ ! -z  "$_BK_SHELLX_CMD" ] 
then
	echo "\$ ${_BK_SHELLX_CMD}"
	$_BK_SHELLX_CMD
fi
