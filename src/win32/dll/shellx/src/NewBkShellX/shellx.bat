@echo off
if not exist "shellx.sh" goto error
bk sh shellx.sh %1
goto end

:error
echo You must extract the contents of the .zip file to disk and run this
echo script from that directory.  This script cannot be run from inside
echo a .zip file or without the rest of the contents of the original .zip file.
pause

:end
