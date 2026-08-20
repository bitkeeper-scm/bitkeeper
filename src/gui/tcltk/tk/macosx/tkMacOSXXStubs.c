/*
 * tkMacOSXXStubs.c --
 *
 *	This file contains most of the X calls called by Tk. Many of these
 *	calls are just stubs and either don't make sense on the Macintosh or
 *	their implementation just doesn't do anything. Other calls will
 *	eventually be moved into other files.
 *
 * Copyright © 1995-1997 Sun Microsystems, Inc.
 * Copyright © 2001-2009 Apple Inc.
 * Copyright © 2005-2009 Daniel A. Steffen <das@users.sourceforge.net>
 * Copyright © 2014 Marc Culler.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#define XLIB_ILLEGAL_ACCESS
#include "tkMacOSXPrivate.h"
#include "tkMacOSXEvent.h"

#include <IOKit/IOKitLib.h>
#include <IOKit/hidsystem/IOHIDShared.h>

/*
 * Because this file is still under major development Debugger statements are
 * used through out this file. The define TCL_DEBUG will decide whether the
 * debugger statements actually call the debugger or not.
 */

#ifndef TCL_DEBUG
#   define Debugger()
#endif

#define ROOT_ID 10

/*
 * Declarations of static variables used in this file.
 */

/* The unique Macintosh display. */
static TkDisplay *gMacDisplay = NULL;
/* The default name of the Macintosh display. */
static const char *macScreenName = ":0";
/* Timestamp showing the last reset of the inactivity timer. */
static Time lastInactivityReset = 0;


/*
 * Forward declarations of procedures used in this file.
 */

static XID		MacXIdAlloc(Display *display);
static int		DefaultErrorHandler(Display *display,
			    XErrorEvent *err_evt);


/*
 *----------------------------------------------------------------------
 *
 * TkMacOSXDisplayChanged --
 *
 *	Called to set up initial screen info or when an event indicated
 *	display (screen) change.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	May change info regarding the screen.
 *
 *----------------------------------------------------------------------
 */

void
TkMacOSXDisplayChanged(
    Display *display)
{
    Screen *screen;
    NSArray *nsScreens;


    if (display == NULL || (((_XPrivDisplay)(display))->screens) == NULL) {
	return;
    }
    screen = (((_XPrivDisplay)(display))->screens);

    nsScreens = [NSScreen screens];
    if (nsScreens && [nsScreens count]) {
	NSScreen *s = [nsScreens objectAtIndex:0];
	NSRect bounds = [s frame];
	NSRect maxBounds = NSZeroRect;

	DefaultDepthOfScreen(screen) = NSBitsPerPixelFromDepth([s depth]);
	WidthOfScreen(screen) = bounds.size.width;
	HeightOfScreen(screen) = bounds.size.height;
	WidthMMOfScreen(screen) = (bounds.size.width * 254 + 360) / 720;
	HeightMMOfScreen(screen) = (bounds.size.height * 254 + 360) / 720;

	for (s in nsScreens) {
	    maxBounds = NSUnionRect(maxBounds, [s visibleFrame]);
	}
	*((NSRect *)screen->ext_data) = maxBounds;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TkMacOSXZeroScreenHeight --
 *
 *	Replacement for the tkMacOSXZeroScreenHeight variable to avoid
 *	caching values from NSScreen (fixes bug aea00be199).
 *
 * Results:
 *	Returns the height of screen 0 (the screen assigned the menu bar
 *	in System Preferences), or 0.0 if getting [NSScreen screens] fails.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

CGFloat
TkMacOSXZeroScreenHeight()
{
    NSArray *nsScreens = [NSScreen screens];
    if (nsScreens && [nsScreens count]) {
	NSScreen *s = [nsScreens objectAtIndex:0];
	NSRect bounds = [s frame];
	return bounds.size.height;
    }
    return 0.0;
}

/*
 *----------------------------------------------------------------------
 *
 * TkMacOSXZeroScreenTop --
 *
 *	Replacement for the tkMacOSXZeroScreenTop variable to avoid
 *	caching values from visibleFrame.
 *
 * Results:
 *	Returns how far below the top of screen 0 to draw
 *	(i.e. the height of the menu bar if it is always shown),
 *	or 0.0 if getting [NSScreen screens] fails.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

CGFloat
TkMacOSXZeroScreenTop()
{
    NSArray *nsScreens = [NSScreen screens];
    if (nsScreens && [nsScreens count]) {
	NSScreen *s = [nsScreens objectAtIndex:0];
	NSRect bounds = [s frame], visible = [s visibleFrame];
	return bounds.size.height - (visible.origin.y + visible.size.height);
    }
    return 0.0;
}

/*
 *----------------------------------------------------------------------
 *
 * TkpOpenDisplay --
 *
 *	Create the Display structure and fill it with device specific
 *	information.
 *
 * Results:
 *	Returns a Display structure on success or NULL on failure.
 *
 * Side effects:
 *	Allocates a new Display structure.
 *
 *----------------------------------------------------------------------
 */

TkDisplay *
TkpOpenDisplay(
    const char *display_name)
{
    Display *display;
    Screen *screen;
    int fd = 0;
    static NSRect maxBounds = {{0, 0}, {0, 0}};
    static char vendor[25] = "";
    NSArray *cgVers;

    if (gMacDisplay != NULL) {
	if (strcmp(DisplayString(gMacDisplay->display), display_name) == 0) {
	    return gMacDisplay;
	} else {
	    return NULL;
	}
    }

    NSAutoreleasePool *pool = [NSAutoreleasePool new];
    display = (Display *)ckalloc(sizeof(Display));
    screen = (Screen *)ckalloc(sizeof(Screen));
    bzero(display, sizeof(Display));
    bzero(screen, sizeof(Screen));

    display->resource_alloc = MacXIdAlloc;
    LastKnownRequestProcessed(display) = 0;
    display->qlen	    = 0;
    display->fd		    = fd;
    display->screens	    = screen;
    display->nscreens	    = 1;
    display->default_screen = 0;
    display->display_name   = (char *) macScreenName;

    cgVers = [[[NSBundle bundleWithIdentifier:@"com.apple.CoreGraphics"]
	    objectForInfoDictionaryKey:@"CFBundleShortVersionString"]
	    componentsSeparatedByString:@"."];
    if ([cgVers count] >= 2) {
	display->proto_major_version = [[cgVers objectAtIndex:1] integerValue];
    }
    if ([cgVers count] >= 3) {
	display->proto_minor_version = [[cgVers objectAtIndex:2] integerValue];
    }
    if (!vendor[0]) {
	snprintf(vendor, sizeof(vendor), "Apple AppKit %g",
		NSAppKitVersionNumber);
    }
    display->vendor = vendor;
    {
	int major, minor, patch;

#if MAC_OS_X_VERSION_MAX_ALLOWED < 101000
	Gestalt(gestaltSystemVersionMajor, (SInt32*)&major);
	Gestalt(gestaltSystemVersionMinor, (SInt32*)&minor);
	Gestalt(gestaltSystemVersionBugFix, (SInt32*)&patch);
#else
	NSOperatingSystemVersion systemVersion = [[NSProcessInfo processInfo] operatingSystemVersion];
	major = (int)systemVersion.majorVersion;
	minor = (int)systemVersion.minorVersion;
	patch = (int)systemVersion.patchVersion;
#endif
	display->release = major << 16 | minor << 8 | patch;
    }

    /*
     * These screen bits never change
     */
    RootWindowOfScreen(screen)	= ROOT_ID;
    DisplayOfScreen(screen)	= display;
    BlackPixelOfScreen(screen) = 0x00000000;
    WhitePixelOfScreen(screen) = 0x00FFFFFF;
    screen->ext_data	= (XExtData *) &maxBounds;

    DefaultVisualOfScreen(screen) = (Visual *)ckalloc(sizeof(Visual));
    DefaultVisualOfScreen(screen)->visualid     = 0;
    DefaultVisualOfScreen(screen)->c_class      = TrueColor;
    DefaultVisualOfScreen(screen)->red_mask     = 0x00FF0000;
    DefaultVisualOfScreen(screen)->green_mask   = 0x0000FF00;
    DefaultVisualOfScreen(screen)->blue_mask    = 0x000000FF;
    DefaultVisualOfScreen(screen)->bits_per_rgb = 24;
    DefaultVisualOfScreen(screen)->map_entries  = 256;

    /*
     * Initialize screen bits that may change
     */

    TkMacOSXDisplayChanged(display);

    gMacDisplay = (TkDisplay *)ckalloc(sizeof(TkDisplay));

    /*
     * This is the quickest way to make sure that all the *Init flags get
     * properly initialized
     */

    bzero(gMacDisplay, sizeof(TkDisplay));
    gMacDisplay->display = display;
    [pool drain];

    /*
     * Key map info must be available immediately, because of "send event".
     */
    TkpInitKeymapInfo(gMacDisplay);

    return gMacDisplay;
}

/*
 *----------------------------------------------------------------------
 *
 * TkpCloseDisplay --
 *
 *	Deallocates a display structure created by TkpOpenDisplay.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Frees memory.
 *
 *----------------------------------------------------------------------
 */

void
TkpCloseDisplay(
    TkDisplay *displayPtr)
{
    _XPrivDisplay display = (_XPrivDisplay)displayPtr->display;

    if (gMacDisplay != displayPtr) {
	Tcl_Panic("TkpCloseDisplay: tried to call TkpCloseDisplay on bad display");
    }

    gMacDisplay = NULL;
    if (display->screens != NULL) {
	if (DefaultVisualOfScreen(ScreenOfDisplay(display, 0)) != NULL) {
	    ckfree(DefaultVisualOfScreen(ScreenOfDisplay(display, 0)));
	}
	ckfree(display->screens);
    }
    ckfree(display);
}

/*
 *----------------------------------------------------------------------
 *
 * TkClipCleanup --
 *
 *	This procedure is called to cleanup resources associated with claiming
 *	clipboard ownership and for receiving selection get results. This
 *	function is called in tkWindow.c. This has to be called by the display
 *	cleanup function because we still need the access display elements.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Resources are freed - the clipboard may no longer be used.
 *
 *----------------------------------------------------------------------
 */

void
TkClipCleanup(
    TkDisplay *dispPtr)		/* display associated with clipboard */
{
    /*
     * Make sure that the local scrap is transfered to the global scrap if
     * needed.
     */

    [NSApp tkProvidePasteboard:dispPtr];

    if (dispPtr->clipWindow != NULL) {
	Tk_DeleteSelHandler(dispPtr->clipWindow, dispPtr->clipboardAtom,
		dispPtr->applicationAtom);
	Tk_DeleteSelHandler(dispPtr->clipWindow, dispPtr->clipboardAtom,
		dispPtr->windowAtom);

	Tk_DestroyWindow(dispPtr->clipWindow);
	Tcl_Release(dispPtr->clipWindow);
	dispPtr->clipWindow = NULL;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * MacXIdAlloc --
 *
 *	This procedure is invoked by Xlib as the resource allocator for a
 *	display.
 *
 * Results:
 *	The return value is an X resource identifier that isn't currently in
 *	use.
 *
 * Side effects:
 *	The identifier is removed from the stack of free identifiers, if it
 *	was previously on the stack.
 *
 *----------------------------------------------------------------------
 */

static XID
MacXIdAlloc(
    TCL_UNUSED(Display *))		/* Display for which to allocate. */
{
    static long int cur_id = 100;
    /*
     * Some special XIds are reserved
     *   - this is why we start at 100
     */

    return ++cur_id;
}

/*
 *----------------------------------------------------------------------
 *
 * DefaultErrorHandler --
 *
 *	This procedure is the default X error handler. Tk uses it's own error
 *	handler so this call should never be called.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	This function will call panic and exit.
 *
 *----------------------------------------------------------------------
 */

static int
DefaultErrorHandler(
    TCL_UNUSED(Display *),
    TCL_UNUSED(XErrorEvent *))
{
    /*
     * This call should never be called. Tk replaces it with its own error
     * handler.
     */

    Tcl_Panic("Warning hit bogus error handler!");
    return 0;
}

char *
XGetAtomName(
    Display *display,
    TCL_UNUSED(Atom))
{
    LastKnownRequestProcessed(display)++;
    return NULL;
}

XErrorHandler
XSetErrorHandler(
    TCL_UNUSED(XErrorHandler))
{
    return DefaultErrorHandler;
}

Window
XRootWindow(
    Display *display,
    TCL_UNUSED(int))
{
    LastKnownRequestProcessed(display)++;
    return ROOT_ID;
}

int
XGetGeometry(
    Display *display,
    Drawable d,
    Window *root_return,
    int *x_return,
    int *y_return,
    unsigned int *width_return,
    unsigned int *height_return,
    unsigned int *border_width_return,
    unsigned int *depth_return)
{
    TkWindow *winPtr = ((MacDrawable *)d)->winPtr;

    LastKnownRequestProcessed(display)++;
    *root_return = ROOT_ID;
    if (winPtr) {
	*x_return = Tk_X(winPtr);
	*y_return = Tk_Y(winPtr);
	*width_return = Tk_Width(winPtr);
	*height_return = Tk_Height(winPtr);
	*border_width_return = winPtr->changes.border_width;
	*depth_return = Tk_Depth(winPtr);
    } else {
	CGSize size = ((MacDrawable *)d)->size;
	*x_return = 0;
	*y_return =  0;
	*width_return = size.width;
	*height_return = size.height;
	*border_width_return = 0;
	*depth_return = 32;
    }
    return 1;
}

int
XChangeProperty(
    TCL_UNUSED(Display *),
    TCL_UNUSED(Window),
    TCL_UNUSED(Atom),
    TCL_UNUSED(Atom),
    TCL_UNUSED(int),
    TCL_UNUSED(int),
    TCL_UNUSED(_Xconst unsigned char *),
    TCL_UNUSED(int))
{
    Debugger();
    return Success;
}

int
XSelectInput(
    TCL_UNUSED(Display *),
    TCL_UNUSED(Window),
    TCL_UNUSED(long))
{
    Debugger();
    return Success;
}

int
XBell(
    TCL_UNUSED(Display *),
    TCL_UNUSED(int))
{
    NSBeep();
    return Success;
}

GContext
XGContextFromGC(
    TCL_UNUSED(GC))
{
    /*
     * TODO: currently a no-op
     */

    return 0;
}

Status
XSendEvent(
    TCL_UNUSED(Display *),
    TCL_UNUSED(Window),
    TCL_UNUSED(Bool),
    TCL_UNUSED(long),
    TCL_UNUSED(XEvent *))
{
    Debugger();
    return 0;
}

int
XClearWindow(
    TCL_UNUSED(Display *),
    TCL_UNUSED(Window))
{
    return Success;
}

int
XWarpPointer(
    TCL_UNUSED(Display *),
    TCL_UNUSED(Window),
    TCL_UNUSED(Window),
    TCL_UNUSED(int),
    TCL_UNUSED(int),
    TCL_UNUSED(unsigned int),
    TCL_UNUSED(unsigned int),
    TCL_UNUSED(int),
    TCL_UNUSED(int))
{
    return Success;
}

int
XQueryColor(
    TCL_UNUSED(Display *),
    TCL_UNUSED(Colormap),
    XColor* def_in_out)
{
    unsigned long p;
    unsigned char r, g, b;
    XColor *d = def_in_out;

    p		= d->pixel;
    r		= (p & 0x00FF0000) >> 16;
    g		= (p & 0x0000FF00) >> 8;
    b		= (p & 0x000000FF);
    d->red	= (r << 8) | r;
    d->green	= (g << 8) | g;
    d->blue	= (b << 8) | b;
    d->flags	= DoRed|DoGreen|DoBlue;
    d->pad	= 0;
    return Success;
}

int
XQueryColors(
    TCL_UNUSED(Display *),
    TCL_UNUSED(Colormap),
    XColor* defs_in_out,
    int ncolors)
{
    int i;
    unsigned long p;
    unsigned char r, g, b;
    XColor *d = defs_in_out;

    for (i = 0; i < ncolors; i++, d++) {
	p		= d->pixel;
	r		= (p & 0x00FF0000) >> 16;
	g		= (p & 0x0000FF00) >> 8;
	b		= (p & 0x000000FF);
	d->red		= (r << 8) | r;
	d->green	= (g << 8) | g;
	d->blue		= (b << 8) | b;
	d->flags	= DoRed|DoGreen|DoBlue;
	d->pad		= 0;
    }
    return Success;
}

int
XQueryTree(
    TCL_UNUSED(Display *),
    TCL_UNUSED(Window),
    TCL_UNUSED(Window *),
    TCL_UNUSED(Window *),
    TCL_UNUSED(Window **),
    TCL_UNUSED(unsigned int *))
{
    return 0;
}


int
XGetWindowProperty(
    Display *display,
    TCL_UNUSED(Window),
    TCL_UNUSED(Atom),
    TCL_UNUSED(long),
    TCL_UNUSED(long),
    TCL_UNUSED(Bool),
    TCL_UNUSED(Atom),
    Atom *actual_type_return,
    int *actual_format_return,
    unsigned long *nitems_return,
    unsigned long *bytes_after_return,
    TCL_UNUSED(unsigned char **))
{
    LastKnownRequestProcessed(display)++;
    *actual_type_return = None;
    *actual_format_return = *bytes_after_return = 0;
    *nitems_return = 0;
    return 0;
}

int
XRefreshKeyboardMapping(
    TCL_UNUSED(XMappingEvent *))
{
    /* used by tkXEvent.c */
    Debugger();
    return Success;
}

int
XSetIconName(
    Display* display,
    TCL_UNUSED(Window),
    TCL_UNUSED(const char *))
{
    /*
     * This is a no-op, no icon name for Macs.
     */
    LastKnownRequestProcessed(display)++;
    return Success;
}

int
XForceScreenSaver(
    Display* display,
    TCL_UNUSED(int))
{
    /*
     * This function is just a no-op. It is defined to reset the screen saver.
     * However, there is no real way to do this on a Mac. Let me know if there
     * is!
     */

    LastKnownRequestProcessed(display)++;
    return Success;
}

void
Tk_FreeXId(
    TCL_UNUSED(Display *),
    TCL_UNUSED(XID))
{
    /* no-op function needed for stubs implementation. */
}

int
XSync(
    Display *display,
    TCL_UNUSED(Bool))
{
    /*
     *  The main use of XSync is by the update command, which alternates
     *  between running an event loop to process all events without waiting and
     *  calling XSync on all displays until no events are left.  On X11 the
     *  call to XSync might cause the window manager to generate more events
     *  which would then get processed. Apparently this process stabilizes on
     *  X11, leaving the window manager in a state where all events have been
     *  generated and no additional events can be genereated by updating widgets.
     *
     *  It is not clear what the Aqua port should do when XSync is called, but
     *  currently the best option seems to be to do nothing.  (See ticket
     *  [da5f2266df].)
     */

    LastKnownRequestProcessed(display)++;
    return 0;
}

/*
 *----------------------------------------------------------------------
 *
 * TkGetServerInfo --
 *
 *	Given a window, this procedure returns information about the window
 *	server for that window. This procedure provides the guts of the "winfo
 *	server" command.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

void
TkGetServerInfo(
    Tcl_Interp *interp,		/* The server information is returned in this
				 * interpreter's result. */
    Tk_Window tkwin)		/* Token for window; this selects a particular
				 * display and server. */
{
    char buffer[5 + TCL_INTEGER_SPACE * 2];
    char buffer2[11 + TCL_INTEGER_SPACE];

    snprintf(buffer, sizeof(buffer), "CG%d.%d ",
	    ProtocolVersion(Tk_Display(tkwin)),
	    ProtocolRevision(Tk_Display(tkwin)));
    snprintf(buffer2, sizeof(buffer2), " Mac OS X %x",
	    VendorRelease(Tk_Display(tkwin)));
    Tcl_AppendResult(interp, buffer, ServerVendor(Tk_Display(tkwin)),
	    buffer2, NULL);
}

/*
 *----------------------------------------------------------------------
 *
 * XChangeWindowAttributes, XSetWindowBackground,
 * XSetWindowBackgroundPixmap, XSetWindowBorder, XSetWindowBorderPixmap,
 * XSetWindowBorderWidth, XSetWindowColormap
 *
 *	These functions are all no-ops. They all have equivalent Tk calls that
 *	should always be used instead.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
XChangeWindowAttributes(
    TCL_UNUSED(Display *),
    TCL_UNUSED(Window),
    TCL_UNUSED(unsigned long),
    TCL_UNUSED(XSetWindowAttributes *))
{
    return Success;
}

int
XSetWindowBackground(
    TCL_UNUSED(Display *),
    TCL_UNUSED(Window),
    TCL_UNUSED(unsigned long))
{
    return Success;
}

int
XSetWindowBackgroundPixmap(
    TCL_UNUSED(Display *),
    TCL_UNUSED(Window),
    TCL_UNUSED(Pixmap))
{
    return Success;
}

int
XSetWindowBorder(
    TCL_UNUSED(Display *),
    TCL_UNUSED(Window),
    TCL_UNUSED(unsigned long))
{
    return Success;
}

int
XSetWindowBorderPixmap(
    TCL_UNUSED(Display *),
    TCL_UNUSED(Window),
    TCL_UNUSED(Pixmap))
{
    return Success;
}

int
XSetWindowBorderWidth(
    TCL_UNUSED(Display *),
    TCL_UNUSED(Window),
    TCL_UNUSED(unsigned int))
{
    return Success;
}

int
XSetWindowColormap(
    TCL_UNUSED(Display *),
    TCL_UNUSED(Window),
    TCL_UNUSED(Colormap))
{
    Debugger();
    return Success;
}

Status
XStringListToTextProperty(
    TCL_UNUSED(char **),
    TCL_UNUSED(int),
    TCL_UNUSED(XTextProperty *))
{
    Debugger();
    return Success;
}

void
XSetWMClientMachine(
    TCL_UNUSED(Display *),
    TCL_UNUSED(Window),
    TCL_UNUSED(XTextProperty *))
{
    Debugger();
}

XIC
XCreateIC(TCL_UNUSED(XIM), ...)
{
    Debugger();
    return (XIC) 0;
}

#undef XVisualIDFromVisual
VisualID
XVisualIDFromVisual(
    Visual *visual)
{
    return visual->visualid;
}

#undef XSynchronize
XAfterFunction
XSynchronize(
    Display *display,
    TCL_UNUSED(Bool))
{
    LastKnownRequestProcessed(display)++;
    return NULL;
}

#undef XUngrabServer
int
XUngrabServer(
    TCL_UNUSED(Display *))
{
    return 0;
}

#undef XNoOp
int
XNoOp(
    Display *display)
{
    LastKnownRequestProcessed(display)++;
    return 0;
}

#undef XGrabServer
int
XGrabServer(
    TCL_UNUSED(Display *))
{
    return 0;
}

#undef XFree
int
XFree(
    void *data)
{
	if ((data) != NULL) {
		ckfree(data);
	}
    return 0;
}
#undef XFlush
int
XFlush(
    TCL_UNUSED(Display *))
{
    return 0;
}

/*
 *----------------------------------------------------------------------
 *
 * TkGetDefaultScreenName --
 *
 *	Returns the name of the screen that Tk should use during
 *	initialization.
 *
 * Results:
 *	Returns a statically allocated string.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

const char *
TkGetDefaultScreenName(
    TCL_UNUSED(Tcl_Interp *),
    const char *screenName)		/* If NULL, use default string. */
{
    if ((screenName == NULL) || (screenName[0] == '\0')) {
	screenName = macScreenName;
    }
    return screenName;
}

/*
 *----------------------------------------------------------------------
 *
 * Tk_GetUserInactiveTime --
 *
 *	Return the number of milliseconds the user was inactive.
 *
 * Results:
 *	The number of milliseconds the user has been inactive, or -1 if
 *	querying the inactive time is not supported.
 *
 * Side effects:
 *	None.
 *----------------------------------------------------------------------
 */

long
Tk_GetUserInactiveTime(
    TCL_UNUSED(Display *))
{
    io_registry_entry_t regEntry;
    CFMutableDictionaryRef props = NULL;
    CFTypeRef timeObj;
    long ret = -1l;
    uint64_t time;
    IOReturn result;

    regEntry = IOServiceGetMatchingService(0,
	    IOServiceMatching("IOHIDSystem"));

    if (regEntry == 0) {
	return -1l;
    }

    result = IORegistryEntryCreateCFProperties(regEntry, &props,
	    kCFAllocatorDefault, 0);
    IOObjectRelease(regEntry);

    if (result != KERN_SUCCESS || props == NULL) {
	return -1l;
    }

    timeObj = CFDictionaryGetValue(props, CFSTR("HIDIdleTime"));

    if (timeObj) {
	    CFNumberGetValue((CFNumberRef)timeObj,
		    kCFNumberSInt64Type, &time);
	    /* Convert nanoseconds to milliseconds. */
	    ret = (long) (time/kMillisecondScale);
    }
    /* Cleanup */
    CFRelease(props);

    /*
     * If the idle time reported by the system is larger than the elapsed
     * time since the last reset, return the elapsed time.
     */
    long elapsed = (long)(TkpGetMS() - lastInactivityReset);
    if (ret > elapsed) {
    	ret = elapsed;
    }

    return ret;
}

/*
 *----------------------------------------------------------------------
 *
 * Tk_ResetUserInactiveTime --
 *
 *	Reset the user inactivity timer
 *
 * Results:
 *	none
 *
 * Side effects:
 *	The user inactivity timer of the underlaying windowing system is reset
 *	to zero.
 *
 *----------------------------------------------------------------------
 */

void
Tk_ResetUserInactiveTime(
    TCL_UNUSED(Display *))
{
    lastInactivityReset = TkpGetMS();
}

/*
 * Local Variables:
 * mode: objc
 * c-basic-offset: 4
 * fill-column: 79
 * coding: utf-8
 * End:
 */
