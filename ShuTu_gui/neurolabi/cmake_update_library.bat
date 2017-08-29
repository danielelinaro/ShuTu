echo %PATH%

REM Changing working folder back to current directory for Vista & 7 compatibility
%~d0
CD %~dp0
cd lib/Mingw
REM Folder changed

REM This script upzip's files...

> j_unzip.vbs ECHO '
>> j_unzip.vbs ECHO ' UnZip a file script
>> j_unzip.vbs ECHO '
>> j_unzip.vbs ECHO ' By Justin Godden 2010
>> j_unzip.vbs ECHO '
>> j_unzip.vbs ECHO ' It's a mess, I know!!!
>> j_unzip.vbs ECHO '
>> j_unzip.vbs ECHO.
>> j_unzip.vbs ECHO ' Dim ArgObj, var1, var2
>> j_unzip.vbs ECHO Set ArgObj = WScript.Arguments
>> j_unzip.vbs ECHO.
>> j_unzip.vbs ECHO If (Wscript.Arguments.Count ^> 0) Then
>> j_unzip.vbs ECHO. var1 = ArgObj(0)
>> j_unzip.vbs ECHO Else
>> j_unzip.vbs ECHO. var1 = ""
>> j_unzip.vbs ECHO End if
>> j_unzip.vbs ECHO.
>> j_unzip.vbs ECHO If var1 = "" then
>> j_unzip.vbs ECHO. strFileZIP = "example.zip"
>> j_unzip.vbs ECHO Else
>> j_unzip.vbs ECHO. strFileZIP = var1
>> j_unzip.vbs ECHO End if
>> j_unzip.vbs ECHO.
>> j_unzip.vbs ECHO 'The location of the zip file.
>> j_unzip.vbs ECHO REM Set WshShell = CreateObject("Wscript.Shell")
>> j_unzip.vbs ECHO REM CurDir = WshShell.ExpandEnvironmentStrings("%%cd%%")
>> j_unzip.vbs ECHO Dim sCurPath
>> j_unzip.vbs ECHO sCurPath = CreateObject("Scripting.FileSystemObject").GetAbsolutePathName(".")
>> j_unzip.vbs ECHO strZipFile = sCurPath ^& "\" ^& strFileZIP
>> j_unzip.vbs ECHO 'The folder the contents should be extracted to.
>> j_unzip.vbs ECHO outFolder = sCurPath ^& "\"
>> j_unzip.vbs ECHO.
>> j_unzip.vbs ECHO. WScript.Echo ( "Extracting file " ^& strFileZIP)
>> j_unzip.vbs ECHO.
>> j_unzip.vbs ECHO Set objShell = CreateObject( "Shell.Application" )
>> j_unzip.vbs ECHO Set objSource = objShell.NameSpace(strZipFile).Items()
>> j_unzip.vbs ECHO Set objTarget = objShell.NameSpace(outFolder)
>> j_unzip.vbs ECHO intOptions = 256
>> j_unzip.vbs ECHO objTarget.CopyHere objSource, intOptions
>> j_unzip.vbs ECHO.
>> j_unzip.vbs ECHO. WScrip.Echo ( "Extracted." )
>> j_unzip.vbs ECHO.
>> j_unzip.vbs ECHO ' This bit is for testing purposes
>> j_unzip.vbs ECHO REM Dim MyVar
>> j_unzip.vbs ECHO REM MyVar = MsgBox ( strZipFile, 65, "MsgBox Example"

rd /q/s 64
cscript //B j_unzip.vbs 64.zip

del /f j_unzip.vbs

cd /d "%~dp0"

rd /q/s __tmp_cmake_tmp__
md __tmp_cmake_tmp__
cd __tmp_cmake_tmp__

rd /q/s ../c/CMakeCache.txt

cmake ../c -DCMAKE_BUILD_TYPE=Debug -G "MinGW Makefiles"
mingw32-make clean
mingw32-make -j8 install
mingw32-make clean
cmake ../c -DCMAKE_BUILD_TYPE=Release -G "MinGW Makefiles"
mingw32-make -j8
mingw32-make install

cd ..
rd /q/s __tmp_cmake_tmp__

echo off
pause
echo The batch file is complete.
