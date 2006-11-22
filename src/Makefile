#CC="c:\program files\microsoft visual studio .net 2003\vc7\bin\cl"
#LINK="c:\program files\microsoft visual studio .net 2003\vc7\bin\link"
#RC="c:\program files\microsoft visual studio .net 2003\vc7\bin\rc"

CC="c:\program files\microsoft visual studio\vc98\bin\cl"
LINK="c:\program files\microsoft visual studio\vc98\bin\link"
RC="c:\program files\microsoft visual studio\common\msdev98\bin\rc"
INC=/I "c:\program files\microsoft visual studio\vc98\include" /I "c:\juce\dxsdk9\include"
LIB=/LIBPATH:"c:\program files\microsoft visual studio\vc98\lib" /LIBPATH:"c:\juce\dxsdk9\lib"

CFLAGS=/nologo /GX $(EXTRA_CFLAGS)
LFLAGS=/NOLOGO /NODEFAULTLIB:libci.lib
#LFLAGS=/NOLOGO
LIBS=$(LIB) user32.lib gdi32.lib advapi32.lib libcpmt.lib
LIBSDLL=pngdib.obj libpng.lib zdll.lib $(LIBS)

#CC=cl
#RC=rc
#CFLAGS=/nologo /GX $(EXTRA_CFLAGS)
#LINK=link
#LFLAGS=/NOLOGO /NODEFAULTLIB:libci.lib
#LIBS=user32.lib gdi32.lib advapi32.lib libcpmt.lib
#LIBSDLL=pngdib.obj libpng.lib zdll.lib $(LIBS)

INCPNGDIB=soft\pngdib
LPPNGDIB=soft\pngdib 
LPLIBPNG=soft\libpng-1.2.8-lib\lib
LPZLIB=soft\zlib123-dll\lib

all: dlls apps
#dlls: kload.dll kserv.dll kserv_std.dll bserv.dll bserv_preview.dll fserv.dll stadium.dll lodmixer.dll dxtools.dll savegames.dll bootserv.dll
dlls: kload.dll kserv.dll bserv.dll stadium.dll lodmixer.dll dxtools.dll
apps: setup.exe lodcfg.exe keybind.exe afstest.exe afswalk.exe afsexport.exe

release:
	$(MAKE) EXTRA_CFLAGS=/DMYDLL_RELEASE_BUILD
diag: diag.exe
setup: setup.exe
afstest: afstest.exe
afswalk: afswalk.exe
afsexport: afsexport.exe
lodcfg: lodcfg.exe
stadimp: stadimp.exe
filedec: filedec.exe

pngdib.obj:
	(cd soft\pngdib && $(MAKE))

kload_config.obj: kload_config.cpp kload_config.h
kserv_config.obj: kserv_config.cpp kserv_config.h
gdb.obj: gdb.cpp gdb.h
imageutil.obj: imageutil.cpp imageutil.h
d3dfont.obj: d3dfont.cpp d3dfont.h dxutil.h
dxutil.obj: dxutil.cpp dxutil.h
regtools.obj: regtools.cpp regtools.h
alog.obj: alog.cpp alog.h
log.obj: log.cpp log.h
crc32.obj: crc32.cpp crc32.h
afsreader.obj: afsreader.cpp afsreader.h
hook.obj: hook.cpp hook.h kload_config.obj
lodcfg.obj: lodcfg.h lodcfg.cpp lodcfgui.h
lodcfgui.obj: lodcfgui.h lodcfgui.cpp
hooklib.obj: hooklib.h hooklib.cpp

kload.lib: kload.obj detect.obj d3dfont.obj imageutil.obj log.obj kload_config.obj hook.obj numpages.obj input.obj keycfg.obj hooklib.obj dxutil.obj
kload.dll: kload.lib kload.res
	$(LINK) $(LFLAGS) /out:kload.dll /DLL kload.obj d3dfont.obj dxutil.obj detect.obj imageutil.obj log.obj kload_config.obj hook.obj numpages.obj input.obj keycfg.obj hooklib.obj kload.res $(LIBS) d3dx8.lib winmm.lib
kload.obj: kload.cpp kload.h
detect.obj: detect.cpp detect.h
kload.res: 
	$(RC) -r -fo kload.res kload.rc

#kserv.lib: kserv.obj gdb.obj kserv_config.obj dxutil.obj crc32.obj afsreader.obj pngdib.obj
#kserv.dll: kserv.lib kserv.res
#	$(LINK) $(LFLAGS) /out:kserv.dll /DLL kserv.obj gdb.obj kserv_config.obj crc32.obj afsreader.obj dxutil.obj kserv.res $(LIBSDLL) d3dx8.lib winmm.lib /LIBPATH:$(LPPNGDIB) /LIBPATH:$(LPLIBPNG) /LIBPATH:$(LPZLIB) kload.lib
#kserv.obj: kserv.cpp kserv.h shared.h kserv_config.h gdb.h lodcfg.h
#kserv.res: 
#	$(RC) -r -fo kserv.res kserv.rc

kserv_std.lib: kserv_std.obj gdb.obj kserv_config.obj dxutil.obj crc32.obj afsreader.obj pngdib.obj
#kserv_std.dll: kserv_std.lib kserv_std.res
kserv.dll: kserv_std.lib kserv_std.res
#	$(LINK) $(LFLAGS) /out:kserv_std.dll /DLL kserv_std.obj gdb.obj kserv_config.obj crc32.obj afsreader.obj dxutil.obj kserv_std.res $(LIBSDLL) d3dx8.lib winmm.lib /LIBPATH:$(LPPNGDIB) /LIBPATH:$(LPLIBPNG) /LIBPATH:$(LPZLIB) kload.lib
	$(LINK) $(LFLAGS) /out:kserv.dll /DLL kserv_std.obj gdb.obj kserv_config.obj crc32.obj afsreader.obj dxutil.obj kserv_std.res $(LIBSDLL) d3dx8.lib winmm.lib /LIBPATH:$(LPPNGDIB) /LIBPATH:$(LPLIBPNG) /LIBPATH:$(LPZLIB) kload.lib
kserv_std.obj: kserv_std.cpp kserv.h shared.h kserv_config.h gdb.h lodcfg.h
kserv_std.res: 
	$(RC) -r -fo kserv_std.res kserv_std.rc

#bserv.lib: bserv.obj d3dfont.obj pngdib.obj
#bserv.dll: bserv.lib bserv.res
#	$(LINK) $(LFLAGS) /out:bserv.dll /DLL bserv.obj d3dfont.obj kload.lib bserv.res $(LIBSDLL) /LIBPATH:$(LPPNGDIB) /LIBPATH:$(LPLIBPNG) /LIBPATH:$(LPZLIB)
#bserv.obj: bserv.cpp bserv.h
#
#bserv.res: 
#	$(RC) -r -fo bserv.res bserv.rc
	
bserv_preview.lib: bserv_preview.obj d3dfont.obj pngdib.obj crc32.obj
#bserv_preview.dll: bserv_preview.lib bserv_preview.res
bserv.dll: bserv_preview.lib bserv_preview.res
#	$(LINK) $(LFLAGS) /out:bserv_preview.dll /DLL bserv_preview.obj d3dfont.obj crc32.obj kload.lib bserv_preview.res $(LIBSDLL) d3dx8.lib winmm.lib /LIBPATH:$(LPPNGDIB) /LIBPATH:$(LPLIBPNG) /LIBPATH:$(LPZLIB) zdll.lib
	$(LINK) $(LFLAGS) /out:bserv.dll /DLL bserv_preview.obj d3dfont.obj crc32.obj kload.lib bserv_preview.res $(LIBSDLL) d3dx8.lib winmm.lib /LIBPATH:$(LPPNGDIB) /LIBPATH:$(LPLIBPNG) /LIBPATH:$(LPZLIB) zdll.lib
bserv_preview.obj: bserv_preview.cpp bserv.h

bserv_preview.res: 
	$(RC) -r -fo bserv_preview.res bserv_preview.rc
	
#fserv.lib: fserv.obj
#fserv.dll: fserv.lib fserv.res
#	$(LINK) $(LFLAGS) /out:fserv.dll /DLL fserv.obj kload.lib fserv.res $(LIBS)
#fserv.obj: fserv.cpp fserv.h
#
#fserv.res: 
#	$(RC) -r -fo fserv.res fserv.rc

#bootserv.lib: bootserv.obj
#bootserv.dll: bootserv.lib bootserv.res
#	$(LINK) $(LFLAGS) /out:bootserv.dll /DLL bootserv.obj kload.lib d3dx8.lib bootserv.res $(LIBS)
#bootserv.obj: bootserv.cpp bootserv.h
#
#bootserv.res: 
#	$(RC) -r -fo bootserv.res bootserv.rc
	
	
stadium.lib: stadium.obj crc32.obj afsreader.obj hooklib.obj pngdib.obj
stadium.dll: stadium.lib stadium.res
	$(LINK) $(LFLAGS) /out:stadium.dll /DLL stadium.obj crc32.obj afsreader.obj kload.lib hooklib.obj stadium.res $(LIBSDLL) d3dx8.lib /LIBPATH:$(LPPNGDIB) /LIBPATH:$(LPLIBPNG) /LIBPATH:$(LPZLIB)
stadium.obj: stadium.cpp stadium.h

stadium.res: 
	$(RC) -r -fo stadium.res stadium.rc

lodmixer.lib: lodmixer.obj
lodmixer.dll: lodmixer.lib lodmixer.res
	$(LINK) $(LFLAGS) /out:lodmixer.dll /DLL lodmixer.obj kload.lib lodmixer.res $(LIB)
lodmixer.obj: lodmixer.cpp lodmixer.h

lodmixer.res: 
	$(RC) -r -fo lodmixer.res lodmixer.rc
	
dxtools.lib: dxtools.obj
dxtools.dll: dxtools.lib dxtools.res
	$(LINK) $(LFLAGS) /out:dxtools.dll /DLL dxtools.obj kload.lib dxtools.res $(LIB)
dxtools.obj: dxtools.cpp dxtools.h

dxtools.res: 
	$(RC) -r -fo dxtools.res dxtools.rc
	
#savegames.lib: savegames.obj
#savegames.dll: savegames.lib savegames.res
#	$(LINK) $(LFLAGS) /out:savegames.dll /DLL savegames.obj kload.lib savegames.res $(LIBS)
#savegames.obj: savegames.cpp savegames.h SGF.h
#
#savegames.res: 
#	rc -r -fo savegames.res savegames.rc

setupgui.obj: setupgui.cpp setupgui.h
setup.exe: setup.obj detect.obj setupgui.obj imageutil.obj
	$(LINK) $(LFLAGS) /out:setup.exe setup.obj detect.obj setupgui.obj imageutil.obj $(LIBS)
setup.obj: setup.cpp setup.h setupgui.h

lodcfg.exe: lodcfg.obj detect.obj lodcfgui.obj imageutil.obj
	$(LINK) $(LFLAGS) /out:lodcfg.exe lodcfg.obj detect.obj lodcfgui.obj imageutil.obj $(LIBS)

keybindui.obj: keybindui.cpp keybindui.h
keybind.obj: keybind.cpp keybind.h keybindui.h
keybind.exe: keybind.obj keybindui.obj keycfg.obj
	$(LINK) $(LFLAGS) /out:keybind.exe keybind.obj keybindui.obj keycfg.obj $(LIBS)

afstest.obj: afstest.cpp afsreader.h
afstest.exe: afstest.obj afsreader.obj crc32.obj alog.obj aconfig.obj
	$(LINK) $(LFLAGS) /out:afstest.exe afstest.obj afsreader.obj crc32.obj alog.obj aconfig.obj $(LIB)

afswalk.obj: afswalk.cpp afsreader.h
afswalk.exe: afswalk.obj afsreader.obj crc32.obj alog.obj aconfig.obj
	$(LINK) $(LFLAGS) /out:afswalk.exe afswalk.obj afsreader.obj crc32.obj alog.obj aconfig.obj $(LIB)

afsexport.obj: afsexport.cpp afsreader.h
afsexport.exe: afsexport.obj afsreader.obj crc32.obj alog.obj aconfig.obj
	$(LINK) $(LFLAGS) /out:afsexport.exe afsexport.obj afsreader.obj crc32.obj alog.obj aconfig.obj $(LIB)
	
#stadimp.obj: stadimp.cpp
#stadimp.exe: stadimp.obj
#	$(LINK) $(LFLAGS) /out:stadimp.exe stadimp.obj $(LIBS)

#filedec.obj: filedec.cpp
#filedec.exe: filedec.obj
#	$(LINK) $(LFLAGS) /out:filedec.exe filedec.obj $(LIBS)
	
#diaggui.obj: diaggui.cpp diaggui.h
#diag.exe: diag.obj diaggui.obj
#	$(LINK) $(LFLAGS) /out:diag.exe diag.obj diaggui.obj $(LIBS)
#diag.obj: diag.cpp diag.h diaggui.h
	
.cpp.obj:
	$(CC) $(CFLAGS) -c $(INC) /I $(INCPNGDIB) $<

clean:
	del /Q /F *.exp *.lib *.obj *.res *.dll *.exe
	(cd soft\pngdib && $(MAKE) clean)

clean-all:
	del /Q /F *.exp *.lib *.obj *.res *.dll *.exe *.log *~
	(cd soft\pngdib && $(MAKE) clean-all)
	
