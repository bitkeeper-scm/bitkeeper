/* 
 * qebind.h --
 *
 *	This module is the header for quasi-events.
 *
 * Copyright (c) 2002-2008 Tim Baker
 *
 * RCS: @(#) $Id$
 */

#ifndef INCLUDED_QEBIND_H
#define INCLUDED_QEBIND_H

typedef struct QE_BindingTable_ *QE_BindingTable;

/* Pass to QE_BindEvent */
typedef struct QE_Event {
	int type;
	int detail;
	ClientData clientData;
} QE_Event;

typedef struct QE_ExpandArgs {
	QE_BindingTable bindingTable;
	char which;
	ClientData object;
	Tcl_DString *result;
	int event;
	int detail;
	ClientData clientData;
} QE_ExpandArgs;

typedef void (*QE_ExpandProc)(QE_ExpandArgs *args);
extern QE_BindingTable bindingTable;
extern int debug_bindings;

extern int QE_BindInit(Tcl_Interp *interp);
extern QE_BindingTable QE_CreateBindingTable(Tcl_Interp *interp);
extern void QE_DeleteBindingTable(QE_BindingTable bindingTable);
extern int QE_InstallEvent(QE_BindingTable bindingTable, char *name, QE_ExpandProc expand);
extern int QE_InstallDetail(QE_BindingTable bindingTable, char *name, int eventType, QE_ExpandProc expand);
extern int QE_CreateBinding(QE_BindingTable bindingTable,
	ClientData object, char *eventString, char *command, int append);
extern int QE_DeleteBinding(QE_BindingTable bindingTable,
	ClientData object, char *eventString);
extern int QE_GetBinding(QE_BindingTable bindingTable,
	ClientData object, char *eventString);
extern int QE_GetAllBindings(QE_BindingTable bindingTable,
	ClientData object);
extern int QE_GetEventNames(QE_BindingTable bindingTable);
extern int QE_GetDetailNames(QE_BindingTable bindingTable, char *eventName);
extern int QE_BindEvent(QE_BindingTable bindingTable, QE_Event *eventPtr);
extern void QE_ExpandDouble(double number, Tcl_DString *result);
extern void QE_ExpandNumber(long number, Tcl_DString *result);
extern void QE_ExpandString(char *string, Tcl_DString *result);
extern void QE_ExpandEvent(QE_BindingTable bindingTable, int eventType, Tcl_DString *result);
extern void QE_ExpandDetail(QE_BindingTable bindingTable, int event, int detail, Tcl_DString *result);
extern void QE_ExpandPattern(QE_BindingTable bindingTable, int eventType, int detail, Tcl_DString *result);
extern void QE_ExpandUnknown(char which, Tcl_DString *result);
extern int QE_BindCmd(QE_BindingTable bindingTable, int objOffset, int objc,
	Tcl_Obj *CONST objv[]);
extern int QE_ConfigureCmd(QE_BindingTable bindingTable, int objOffset, int objc,
	Tcl_Obj *CONST objv[]);
extern int QE_GenerateCmd(QE_BindingTable bindingTable, int objOffset, int objc,
	Tcl_Obj *CONST objv[]);
extern int QE_InstallCmd(QE_BindingTable bindingTable, int objOffset, int objc,
	Tcl_Obj *CONST objv[]);
extern int QE_UnbindCmd(QE_BindingTable bindingTable, int objOffset, int objc,
	Tcl_Obj *CONST objv[]);
extern int QE_UninstallCmd(QE_BindingTable bindingTable, int objOffset, int objc,
	Tcl_Obj *CONST objv[]);
extern int QE_LinkageCmd(QE_BindingTable bindingTable, int objOffset, int objc,
	Tcl_Obj *CONST objv[]);

#endif /* INCLUDED_QEBIND_H */

