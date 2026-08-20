/*
 * tkMacOSXConfig.c --
 *
 *	This module implements the Macintosh system defaults for
 *	the configuration package.
 *
 * Copyright © 1997 Sun Microsystems, Inc.
 * Copyright © 2001 Apple Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#include "tkInt.h"


/*
 *----------------------------------------------------------------------
 *
 * TkpGetSystemDefault --
 *
 *	Given a dbName and className for a configuration option,
 *	return a string representation of the option.
 *
 * Results:
 *	Returns a Tcl_Obj* with the string identifier that identifies
 *	this option. Returns NULL if there are no system defaults
 *	that match this pair.
 *
 * Side effects:
 *	None, once the package is initialized.
 *
 *----------------------------------------------------------------------
 */

Tcl_Obj *
TkpGetSystemDefault(
    Tk_Window tkwin,			/* A window to use. */
    const char *dbName,			/* The option database name. */
    const char *className)		/* The name of the option class. */
{
    (void)tkwin;
    (void)dbName;
    (void)className;

    return NULL;
}

/*
 * Local Variables:
 * mode: objc
 * c-basic-offset: 4
 * fill-column: 79
 * coding: utf-8
 * End:
 */
