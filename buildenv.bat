@echo off
rem    Copyright 2004-2006,2009,2016 BitMover, Inc
rem 
rem    Licensed under the Apache License, Version 2.0 (the "License");
rem    you may not use this file except in compliance with the License.
rem    You may obtain a copy of the License at
rem 
rem        http://www.apache.org/licenses/LICENSE-2.0
rem 
rem    Unless required by applicable law or agreed to in writing, software
rem    distributed under the License is distributed on an "AS IS" BASIS,
rem    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
rem    See the License for the specific language governing permissions and
rem    limitations under the License.


rem ember to set the "Start in:" field of the shortcut.
rem A value similar to C:\msys\1.0\bin is what the "Start in:" field needs
rem to represent.

rem ember value of GOTO: is used to know recursion has happened.
if "%1" == "GOTO:" goto %2

rem ember command.com only uses the first eight characters of the label.
goto _WindowsNT

rem ember that we only execute here if we are in command.com.
:_Windows

if "x%COMSPEC%" == "x" set COMSPEC=command.com
start %COMSPEC% /e:4096 /c %0 GOTO: _Resume %0 %1 %2 %3 %4 %5 %6 %7 %8 %9
goto EOF

rem ember that we execute here if we recursed.
:_Resume
for %%F in (1 2 3) do shift

rem ember that we get here even in command.com.
:_WindowsNT

if "x%MSYSTEM%" == "x" set MSYSTEM=MINGW32
if "%1" == "MSYS" set MSYSTEM=MSYS

if NOT "x%DISPLAY%" == "x" set DISPLAY=
set BK_USEMSYS=1
rem  just in case bk is not already on our PATH
rem PATH="%PATH%;C:\Program Files\BitKeeper"
cd src
bk get -S ./update_buildenv
bk sh ./update_buildenv -i
set HOME=%CD%
if exist r:\build\buildenv\bin GOTO USE_R
if exist c:\build\buildenv\bin c:\build\buildenv\bin\sh --login -i
goto EOF

:USE_R
if exist r:\temp set TMP=r:\temp
r:\build\buildenv\bin\sh --login -i

:EOF
rem or could do a 'exit' like it used to.
cd ..
