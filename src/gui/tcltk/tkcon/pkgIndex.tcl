# pkgIndex.tcl to use tkcon as a package via 'package require tkcon'
#
# 'tkcon show' will do all that is necessary to display tkcon
#
# Defaults to:
#  * the main interp as the "slave"
#  * hiding tkcon when you click in the titlebar [X]
#  * using '.tkcon' as the root toplevel
#  * not displaying itself at 'package require' time
#
package ifneeded tkcon 2.5 [list source [file join $dir tkcon.tcl]]
