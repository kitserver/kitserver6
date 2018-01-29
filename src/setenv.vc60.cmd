@echo off
echo Setting kitserver compile environment
@call "c:\vs60ee\VC98\bin\vcvars32.bat"
set DXSDK=c:\dxsdk81
set STLPORT=c:\stlport-4.6.2
set INCLUDE=%STLPORT%\stlport;%DXSDK%\include;%INCLUDE%
set LIB=%STLPORT%\lib;%DXSDK%\lib;%LIB%
echo Environment set

