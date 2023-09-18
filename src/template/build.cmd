:: Copyright (c) Microsoft. All rights reserved.
:: 
:: This code is licensed under the MIT License.
:: THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF
:: ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED
:: TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
:: PARTICULAR PURPOSE AND NONINFRINGEMENT.
::
:: This script updates NDL generated code from template files
@echo off

where dotnet >NUL || goto :MissingDotnet
where t4 >NUL || goto :MissingT4

call :generate ndl nblchain || goto :EOF
call :generate ndl nblqueue || goto :EOF
call :generate ndl nblclassify || goto :EOF
call :generate ndl mdl || goto :EOF
call :generate ndl oidrequest || goto :EOF
call :generate compat fileio || goto :EOF

echo Done

goto :EOF

:generate
echo Generating %2.h....

:: We make 2 passes through the file, so we can generate a Table of Contents
t4 %~dp0\%2.tt -o %TMP%\%2-2.tt && t4 %TMP%\%2-2.tt -o %~dp0\..\include\ndis\%1\%2.h
del /q %TMP%\%2-2.tt
goto :EOF

:MissingDotnet
echo The `dotnet` command was not found.
echo Ensure you have a recent dotnet runtime installed from here:
echo https://dotnet.microsoft.com/download
goto :EOF

:MissingT4
echo The `t4` command was not found.
echo Ensure you have this tool installed by running:
echo     dotnet tool install -g dotnet-t4
goto :EOF

