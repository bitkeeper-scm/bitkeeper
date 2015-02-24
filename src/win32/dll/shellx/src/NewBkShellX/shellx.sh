SHELLX="BkShellX.dll"
OLD_SHELLX="OldBkShellX.dll"
NEW_SHELLX="NewBkShellX.dll"

PWD=`bk pwd`
BIN=`bk bin`
BIN=`bk pwd "$BIN"`

usage()
{
    echo "Options:"
    echo
    echo "new       - install the new version of the shell extension"
    echo "old       - install the old version of the shell extension"
    echo "status    - show the status of the shell extension"
    echo "uninstall - remove the shell extension"
}

status()
{
    INSTALLED=""
    INSTALLED_SHELLX=""
    if test ! -f $NEW_SHELLX; then
	INSTALLED="new"
	UNINSTALL="old"
	INSTALLED_SHELLX=$NEW_SHELLX
    elif test ! -f $OLD_SHELLX; then
	INSTALLED="old"
	UNINSTALL="new"
	INSTALLED_SHELLX=$OLD_SHELLX
    fi


    if test "$INSTALLED" = ""; then
	STATUS="There is no shell extension currently installed."
    else
	STATUS="The $INSTALLED shell extension is currently installed."
    fi
}

rm_dll()
{
    rm -f $SHELLX 2> /dev/null
}

clean_dll()
{
    rm_dll

    if test "$?" != "0"; then
	echo ""
	echo "Could not remove the current shell extension DLL."
	echo ""
	echo "The DLL is probably still in use by another program."
	echo "Please log off of your Windows session and then log"
	echo "back in to remove the DLL from any running processes."
	echo "Once you have done that, come back and run the $1 again."
	exit 1
    fi
}

restart_explorer()
{
    echo "Restarting explorer..."
    taskkill -F -IM explorer.exe > /dev/null
    start explorer.exe &
}

register()
{
    if test -f $SHELLX; then
	echo "Registering $SHELLX..."
	regsvr32 -s $SHELLX
	restart_explorer
    fi
}

unregister()
{
    if test -f $SHELLX; then
	echo "Unregistering $SHELLX..."
	regsvr32 -s -u $SHELLX
	restart_explorer
    fi
}

install()
{
    if test "$INSTALL" = "new"; then
	INSTALL_SHELLX=$NEW_SHELLX
    elif test "$INSTALL" = "old"; then
	INSTALL_SHELLX=$OLD_SHELLX
    else
	usage
	exit 1
    fi

    if test "$INSTALL" = "$INSTALLED"; then
	echo "The $INSTALLED shell extension is already installed."
	return
    fi
    
    if test -f $SHELLX; then
	if test ! -f $INSTALLED_SHELLX; then
	    echo "Uninstalling the $INSTALLED shell extension."
	    cp -f $SHELLX $INSTALLED_SHELLX
	    unregister
	    echo
	fi
	clean_dll
    fi

    mv $INSTALL_SHELLX $SHELLX

    echo "Installing the $INSTALL shell extension."
    register

    echo
    echo "The $INSTALL shell extension has been successfully installed."
}

uninstall()
{
    if test "$INSTALLED" = ""; then
	echo $STATUS
	return
    fi

    cp $SHELLX $INSTALLED_SHELLX

    echo "Uninstalling the $UNINSTALL shell extension."
    unregister

    rm_dll

    echo
    echo "The $UNINSTALL shell extension has been successfully uninstalled."
}

main()
{
    status

    case "$ans" in
	new|old)
	    INSTALL="$ans"
	    install
	    ;;
	un*)
	    uninstall
	    ;;
	st*)
	    echo $STATUS
	    ;;
	ex*|q*)
	    exit
	    ;;
    esac
}

if test "$PWD" != "$BIN"; then
    cp -fR $NEW_SHELLX shellx.bat shellx.sh Icons "$BIN"
    cd "$BIN"
fi

case "$1" in
    new|old|un*|st*)
	ans="$1"
	main
	;;

    help|usage)
	usage
	exit
	;;

    *)
	status

	if test -f $NEW_SHELLX -a -f $OLD_SHELLX -a $NEW_SHELLX -nt $SHELLX;then
	    ## We've already installed the new shellx, and we
	    ## have a newer vesion now.
	    INSTALLED="old"
	    echo "Uninstalling the $INSTALLED shell extension."
	    unregister
	    echo
	fi

	if test "$INSTALLED" != "new"; then
	    INSTALL="new"
	    install
            sleep 2
	    exit
	fi

	while test 1
	do
	    usage
	    echo "quit"
	    echo
	    read -p ">> " ans
	    echo

	    main
	    echo
	done
	;;
esac
