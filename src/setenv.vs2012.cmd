@echo off
echo Setting kitserver compile environment
@call "c:\vs11\VC\bin\vcvars32.bat"
set DXSDK=c:\dxsdk81
set INCLUDE=%INCLUDE%;%DXSDK%\include
set LIB=%LIB%;%DXSDK%\lib
echo Environment set

