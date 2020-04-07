@echo off
echo Setting kitserver compile environment
@call "c:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars32.bat"
set DXSDK=c:\dxsdk81
set INCLUDE=%INCLUDE%;%DXSDK%\include
set LIB=%LIB%;%DXSDK%\lib
echo Environment set

