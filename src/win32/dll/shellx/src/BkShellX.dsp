# Microsoft Developer Studio Project File - Name="BkShellX" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Dynamic-Link Library" 0x0102

CFG=BkShellX - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "BkShellX.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "BkShellX.mak" CFG="BkShellX - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "BkShellX - Win32 Debug" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE "BkShellX - Win32 Unicode Debug" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE "BkShellX - Win32 Release MinSize" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE "BkShellX - Win32 Release MinDependency" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE "BkShellX - Win32 Unicode Release MinSize" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE "BkShellX - Win32 Unicode Release MinDependency" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName "BkShellX"
# PROP Scc_LocalPath "."
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "BkShellX - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MTd /W3 /Gm /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /Yu"stdafx.h" /FD /GZ /c
# ADD CPP /nologo /MTd /W3 /Gm /GX /ZI /Od /D "_DEBUG" /D "_MBCS" /D "WIN32" /D "_WINDOWS" /D "_USRDLL" /D "_ATL_NO_UUIDOF" /Yu"stdafx.h" /FD /GZ /c
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /dll /debug /machine:I386 /pdbtype:sept
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib comctl32.lib /nologo /subsystem:windows /dll /debug /machine:I386 /out:"BkShellX.dll" /pdbtype:sept
# Begin Custom Build - Performing registration
OutDir=.\Debug
TargetPath=.\BkShellX.dll
InputPath=.\BkShellX.dll
SOURCE="$(InputPath)"

"$(OutDir)\regsvr32.trg" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	regsvr32 /s /c "$(TargetPath)" 
	echo regsvr32 exec. time > "$(OutDir)\regsvr32.trg" 
	
# End Custom Build

!ELSEIF  "$(CFG)" == "BkShellX - Win32 Unicode Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "DebugU"
# PROP BASE Intermediate_Dir "DebugU"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "DebugU"
# PROP Intermediate_Dir "DebugU"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MTd /W3 /Gm /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_USRDLL" /D "_UNICODE" /Yu"stdafx.h" /FD /GZ /c
# ADD CPP /nologo /MTd /W3 /Gm /GX /ZI /Od /D "_DEBUG" /D "_UNICODE" /D "WIN32" /D "_WINDOWS" /D "_USRDLL" /D "_ATL_NO_UUIDOF" /Yu"stdafx.h" /FD /GZ /c
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /dll /debug /machine:I386 /pdbtype:sept
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /dll /debug /machine:I386 /pdbtype:sept
# Begin Custom Build - Performing registration
OutDir=.\DebugU
TargetPath=.\DebugU\BkShellX.dll
InputPath=.\DebugU\BkShellX.dll
SOURCE="$(InputPath)"

"$(OutDir)\regsvr32.trg" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	if "%OS%"=="" goto NOTNT 
	if not "%OS%"=="Windows_NT" goto NOTNT 
	regsvr32 /s /c "$(TargetPath)" 
	echo regsvr32 exec. time > "$(OutDir)\regsvr32.trg" 
	goto end 
	:NOTNT 
	echo Warning : Cannot register Unicode DLL on Windows 95 
	:end 
	
# End Custom Build

!ELSEIF  "$(CFG)" == "BkShellX - Win32 Release MinSize"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "ReleaseMinSize"
# PROP BASE Intermediate_Dir "ReleaseMinSize"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "ReleaseMinSize"
# PROP Intermediate_Dir "ReleaseMinSize"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MT /W3 /O1 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "_ATL_DLL" /D "_ATL_MIN_CRT" /Yu"stdafx.h" /FD /c
# ADD CPP /nologo /MD /W3 /GX /O1 /D "NDEBUG" /D "_MBCS" /D "_ATL_DLL" /D "WIN32" /D "_WINDOWS" /D "_USRDLL" /D "_ATL_NO_UUIDOF" /Yu"stdafx.h" /FD /c
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /dll /machine:I386
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib comctl32.lib /nologo /subsystem:windows /dll /machine:I386
# Begin Custom Build - Performing registration
OutDir=.\ReleaseMinSize
TargetPath=.\ReleaseMinSize\BkShellX.dll
InputPath=.\ReleaseMinSize\BkShellX.dll
SOURCE="$(InputPath)"

"$(OutDir)\regsvr32.trg" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	regsvr32 /s /c "$(TargetPath)" 
	echo regsvr32 exec. time > "$(OutDir)\regsvr32.trg" 
	
# End Custom Build

!ELSEIF  "$(CFG)" == "BkShellX - Win32 Release MinDependency"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "ReleaseMinDependency"
# PROP BASE Intermediate_Dir "ReleaseMinDependency"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "ReleaseMinDependency"
# PROP Intermediate_Dir "ReleaseMinDependency"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MT /W3 /O1 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "_ATL_STATIC_REGISTRY" /D "_ATL_MIN_CRT" /Yu"stdafx.h" /FD /c
# ADD CPP /nologo /MD /W3 /GX /O1 /D "NDEBUG" /D "_MBCS" /D "_ATL_STATIC_REGISTRY" /D "WIN32" /D "_WINDOWS" /D "_USRDLL" /D "_ATL_NO_UUIDOF" /Yu"stdafx.h" /FD /c
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /dll /machine:I386
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib comctl32.lib /nologo /subsystem:windows /dll /machine:I386
# SUBTRACT LINK32 /pdb:none
# Begin Custom Build - Performing registration
OutDir=.\ReleaseMinDependency
TargetPath=.\ReleaseMinDependency\BkShellX.dll
InputPath=.\ReleaseMinDependency\BkShellX.dll
SOURCE="$(InputPath)"

"$(OutDir)\regsvr32.trg" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	regsvr32 /s /c "$(TargetPath)" 
	echo regsvr32 exec. time > "$(OutDir)\regsvr32.trg" 
	
# End Custom Build

!ELSEIF  "$(CFG)" == "BkShellX - Win32 Unicode Release MinSize"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "ReleaseUMinSize"
# PROP BASE Intermediate_Dir "ReleaseUMinSize"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "ReleaseUMinSize"
# PROP Intermediate_Dir "ReleaseUMinSize"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MT /W3 /O1 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_USRDLL" /D "_UNICODE" /D "_ATL_DLL" /D "_ATL_MIN_CRT" /Yu"stdafx.h" /FD /c
# ADD CPP /nologo /MD /W3 /GX /O1 /D "NDEBUG" /D "_UNICODE" /D "_ATL_DLL" /D "WIN32" /D "_WINDOWS" /D "_USRDLL" /D "_ATL_NO_UUIDOF" /Yu"stdafx.h" /FD /c
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /dll /machine:I386
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /dll /machine:I386
# Begin Custom Build - Performing registration
OutDir=.\ReleaseUMinSize
TargetPath=.\ReleaseUMinSize\BkShellX.dll
InputPath=.\ReleaseUMinSize\BkShellX.dll
SOURCE="$(InputPath)"

"$(OutDir)\regsvr32.trg" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	if "%OS%"=="" goto NOTNT 
	if not "%OS%"=="Windows_NT" goto NOTNT 
	regsvr32 /s /c "$(TargetPath)" 
	echo regsvr32 exec. time > "$(OutDir)\regsvr32.trg" 
	goto end 
	:NOTNT 
	echo Warning : Cannot register Unicode DLL on Windows 95 
	:end 
	
# End Custom Build

!ELSEIF  "$(CFG)" == "BkShellX - Win32 Unicode Release MinDependency"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "ReleaseUMinDependency"
# PROP BASE Intermediate_Dir "ReleaseUMinDependency"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "ReleaseUMinDependency"
# PROP Intermediate_Dir "ReleaseUMinDependency"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MT /W3 /O1 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_USRDLL" /D "_UNICODE" /D "_ATL_STATIC_REGISTRY" /D "_ATL_MIN_CRT" /Yu"stdafx.h" /FD /c
# ADD CPP /nologo /MD /W3 /GX /O1 /D "NDEBUG" /D "_UNICODE" /D "_ATL_STATIC_REGISTRY" /D "WIN32" /D "_WINDOWS" /D "_USRDLL" /D "_ATL_NO_UUIDOF" /Yu"stdafx.h" /FD /c
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /dll /machine:I386
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /dll /machine:I386
# Begin Custom Build - Performing registration
OutDir=.\ReleaseUMinDependency
TargetPath=.\ReleaseUMinDependency\BkShellX.dll
InputPath=.\ReleaseUMinDependency\BkShellX.dll
SOURCE="$(InputPath)"

"$(OutDir)\regsvr32.trg" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	if "%OS%"=="" goto NOTNT 
	if not "%OS%"=="Windows_NT" goto NOTNT 
	regsvr32 /s /c "$(TargetPath)" 
	echo regsvr32 exec. time > "$(OutDir)\regsvr32.trg" 
	goto end 
	:NOTNT 
	echo Warning : Cannot register Unicode DLL on Windows 95 
	:end 
	
# End Custom Build

!ENDIF 

# Begin Target

# Name "BkShellX - Win32 Debug"
# Name "BkShellX - Win32 Unicode Debug"
# Name "BkShellX - Win32 Release MinSize"
# Name "BkShellX - Win32 Release MinDependency"
# Name "BkShellX - Win32 Unicode Release MinSize"
# Name "BkShellX - Win32 Unicode Release MinDependency"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=.\BitKeeper.cpp
# End Source File
# Begin Source File

SOURCE=.\BkEditIcon.cpp
# End Source File
# Begin Source File

SOURCE=.\BkExtraIcon.cpp
# End Source File
# Begin Source File

SOURCE=.\BkReadOnlyIcon.cpp
# End Source File
# Begin Source File

SOURCE=.\BkRootIcon.cpp
# End Source File
# Begin Source File

SOURCE=.\BkShellX.cpp
# End Source File
# Begin Source File

SOURCE=.\BkShellX.def
# End Source File
# Begin Source File

SOURCE=.\BkShellX.idl
# ADD MTL /tlb ".\BkShellX.tlb" /h "BkShellX.h" /iid "BkShellX_i.c" /Oicf
# End Source File
# Begin Source File

SOURCE=.\BkShellX.rc
# End Source File
# Begin Source File

SOURCE=.\ContextMenuHandler.cpp
# End Source File
# Begin Source File

SOURCE=.\ContextMenuItemBase.cpp
# End Source File
# Begin Source File

SOURCE=.\CtxBkSubMenu.cpp
# End Source File
# Begin Source File

SOURCE=.\CtxCi.cpp
# End Source File
# Begin Source File

SOURCE=.\CtxCitool.cpp
# End Source File
# Begin Source File

SOURCE=.\CtxClean.cpp
# End Source File
# Begin Source File

SOURCE=.\CtxClean.h
# End Source File
# Begin Source File

SOURCE=.\CtxClone.cpp
# End Source File
# Begin Source File

SOURCE=.\CtxDiffTool.cpp
# End Source File
# Begin Source File

SOURCE=.\CtxEdit.cpp
# End Source File
# Begin Source File

SOURCE=.\CtxGet.cpp
# End Source File
# Begin Source File

SOURCE=.\CtxHelpTool.cpp
# End Source File
# Begin Source File

SOURCE=.\CtxMove.cpp
# End Source File
# Begin Source File

SOURCE=.\CtxNew.cpp
# End Source File
# Begin Source File

SOURCE=.\CtxPull.cpp
# End Source File
# Begin Source File

SOURCE=.\CtxPush.cpp
# End Source File
# Begin Source File

SOURCE=.\CtxRevtool.cpp
# End Source File
# Begin Source File

SOURCE=.\CtxSetupTool.cpp
# End Source File
# Begin Source File

SOURCE=.\CtxUnedit.cpp
# End Source File
# Begin Source File

SOURCE=.\dlldatax.c
# PROP Exclude_From_Scan -1
# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=.\FileFolderSplitter.cpp
# End Source File
# Begin Source File

SOURCE=.\OverWriteDlg.cpp
# End Source File
# Begin Source File

SOURCE=.\propSheet.cpp
# End Source File
# Begin Source File

SOURCE=.\StdAfx.cpp
# ADD CPP /Yc"stdafx.h"
# End Source File
# Begin Source File

SOURCE=.\Unicode.cpp
# End Source File
# Begin Source File

SOURCE=.\Util.cpp
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=.\BitKeeper.h
# End Source File
# Begin Source File

SOURCE=.\BkCommandConstants.h
# End Source File
# Begin Source File

SOURCE=.\BkEditIcon.h
# End Source File
# Begin Source File

SOURCE=.\BkExtraIcon.h
# End Source File
# Begin Source File

SOURCE=.\BkReadOnlyIcon.h
# End Source File
# Begin Source File

SOURCE=.\BkRootIcon.h
# End Source File
# Begin Source File

SOURCE=.\CiDialog.h
# End Source File
# Begin Source File

SOURCE=.\CloneDialog.h
# End Source File
# Begin Source File

SOURCE=.\ContextMenuHandler.h
# End Source File
# Begin Source File

SOURCE=.\ContextMenuItemBase.h
# End Source File
# Begin Source File

SOURCE=.\CtxBkSubMenu.h
# End Source File
# Begin Source File

SOURCE=.\CtxCi.h
# End Source File
# Begin Source File

SOURCE=.\CtxCitool.h
# End Source File
# Begin Source File

SOURCE=.\CtxClone.h
# End Source File
# Begin Source File

SOURCE=.\CtxDiffTool.h
# End Source File
# Begin Source File

SOURCE=.\CtxEdit.h
# End Source File
# Begin Source File

SOURCE=.\CtxGet.h
# End Source File
# Begin Source File

SOURCE=.\CtxHelpTool.h
# End Source File
# Begin Source File

SOURCE=.\CtxMove.h
# End Source File
# Begin Source File

SOURCE=.\CtxNew.h
# End Source File
# Begin Source File

SOURCE=.\CtxPull.h
# End Source File
# Begin Source File

SOURCE=.\CtxPush.h
# End Source File
# Begin Source File

SOURCE=.\CtxRevtool.h
# End Source File
# Begin Source File

SOURCE=.\CtxSetupTool.h
# End Source File
# Begin Source File

SOURCE=.\CtxUnedit.h
# End Source File
# Begin Source File

SOURCE=.\dlldatax.h
# PROP Exclude_From_Scan -1
# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=.\FileFolderSplitter.h
# End Source File
# Begin Source File

SOURCE=.\IFileFolderSplitter.h
# End Source File
# Begin Source File

SOURCE=.\OverWriteDlg.h
# End Source File
# Begin Source File

SOURCE=.\ParentDialog.h
# End Source File
# Begin Source File

SOURCE=.\propSheet.h
# End Source File
# Begin Source File

SOURCE=.\Resource.h
# End Source File
# Begin Source File

SOURCE=.\StdAfx.h
# End Source File
# Begin Source File

SOURCE=.\stringizer.h
# End Source File
# Begin Source File

SOURCE=.\tstring.h
# End Source File
# Begin Source File

SOURCE=.\Unicode.h
# End Source File
# Begin Source File

SOURCE=.\Util.h
# End Source File
# Begin Source File

SOURCE=.\UtlCommon.h
# End Source File
# End Group
# Begin Group "Resource Files"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;rgs;gif;jpg;jpeg;jpe"
# Begin Source File

SOURCE=.\bitmap1.bmp
# End Source File
# Begin Source File

SOURCE=.\BkEdit.ico
# End Source File
# Begin Source File

SOURCE=.\BkEditIcon.rgs
# End Source File
# Begin Source File

SOURCE=.\bkExtra.ico
# End Source File
# Begin Source File

SOURCE=.\BkExtraIcon.rgs
# End Source File
# Begin Source File

SOURCE=.\BkReadOnly.ico
# End Source File
# Begin Source File

SOURCE=.\BkReadOnly2.ico
# End Source File
# Begin Source File

SOURCE=.\BkReadOnlyIcon.rgs
# End Source File
# Begin Source File

SOURCE=.\bkRoot.ico
# End Source File
# Begin Source File

SOURCE=.\BkRoot2.ico
# End Source File
# Begin Source File

SOURCE=.\BkRootIcon.rgs
# End Source File
# Begin Source File

SOURCE=.\BkShellExtHandler.rgs
# End Source File
# End Group
# End Target
# End Project
# Section BkShellX : {00000000-0000-0000-0000-800000800000}
# 	1:16:IDD_OVERWRITEDLG:1021
# End Section
