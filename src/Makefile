CC=cl
LINK=link
RC=rc

CFLAGS=/nologo /MT /EHsc /wd4731 /wd4477 /D_WINNT_WIN32=0x501 /DKS_JMP_SHIFT=4 $(EXTRA_CFLAGS) /DUSE_HASHMAPS
LFLAGS=/NOLOGO /NODEFAULTLIB:libci.lib /LIBPATH:soft\dxsdk81\lib
LIBS=user32.lib gdi32.lib advapi32.lib
LIBSDLL=pngdib.obj libpng.lib zdll.lib $(LIBS)

INCLUDE=$(INCLUDE);soft\dxsdk81\include
LIB=$(LIB);soft\dxsdk81\lib

INCPNGDIB=soft\pngdib
LPPNGDIB=soft\pngdib 
LPLIBPNG=soft\libpng-1.2.8-lib\lib
LPZLIB=soft\zlib123-dll\lib

all: dlls apps
	if exist output (rd /S /Q output)
	mkdir output
	copy *.dll output
	copy setup.exe output
	copy lodcfg.exe output
	copy keyconf.exe output
	copy filedec.exe output
#	copy speedtest.exe output
#	copy vista_test.exe output
	copy soft\zlib123-dll\zlib1.dll output
	copy soft\libpng-1.2.8-bin\libpng13.dll output
	
all_dlls: dlls
	if exist output (rd /S /Q output)
	mkdir output
	copy *.dll output
	copy soft\zlib123-dll\zlib1.dll output
	copy soft\libpng-1.2.8-bin\libpng13.dll output
	
all_apps: apps
	if exist output (rd /S /Q output)
	mkdir output
	copy setup.exe output
	copy lodcfg.exe output
	copy keyconf.exe output
	copy filedec.exe output
#	copy speedtest.exe output
#	copy vista_test.exe output

dlls: pngdib kload.dll kserv.dll bserv.dll fserv.dll stadium.dll bootserv.dll gloves.dll skins.dll lodmixer.dll dxtools.dll psc.dll clock.dll afs2fs.dll roster.dll speeder.dll
apps: setup.exe lodcfg.exe keyconf.exe filedec.exe afstest.exe afswalk.exe afsexport.exe # speedtest.exe vista_test.exe

!if "$(debug)"=="1"
EXTRA_CFLAGS=/DDEBUG
!else
EXTRA_CFLAGS=/DMYDLL_RELEASE_BUILD
!endif

diag: diag.exe
setup: setup.exe
afstest: afstest.exe
afswalk: afswalk.exe
afsexport: afsexport.exe
lodcfg: lodcfg.exe
stadimp: stadimp.exe
filedec: filedec.exe
speedtest: speedtest.exe

pngdib:
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
afsreplace.obj: afsreplace.cpp afsreplace.h kload_config.obj hook.obj
lodcfg.obj: lodcfg.h lodcfg.cpp lodcfgui.h
lodcfgui.obj: lodcfgui.h lodcfgui.cpp
hooklib.obj: hooklib.h hooklib.cpp

kload.lib: kload.obj detect.obj d3dfont.obj imageutil.obj log.obj kload_config.obj hook.obj numpages.obj afsreplace.obj input.obj keycfg.obj hooklib.obj dxutil.obj
kload.dll: kload.lib kload.res
	$(LINK) $(LFLAGS) /out:kload.dll /DLL kload.obj d3dfont.obj dxutil.obj detect.obj imageutil.obj log.obj kload_config.obj hook.obj numpages.obj afsreplace.obj input.obj keycfg.obj hooklib.obj kload.res $(LIBS) d3dx8.lib winmm.lib
kload.obj: kload.cpp kload.h kload_config.h hook.h log.h keycfg.h
detect.obj: detect.cpp detect.h
kload.res: 
	$(RC) -r -fo kload.res kload.rc

kserv_std.lib: kserv_std.obj gdb.obj kserv_config.obj dxutil.obj crc32.obj afsreader.obj
kserv.dll: kserv_std.lib kserv_std.res kload.lib
	$(LINK) $(LFLAGS) /out:kserv.dll /DLL kserv_std.obj gdb.obj kserv_config.obj crc32.obj afsreader.obj dxutil.obj kserv_std.res $(LIBSDLL) d3dx8.lib winmm.lib /LIBPATH:$(LPPNGDIB) /LIBPATH:$(LPLIBPNG) /LIBPATH:$(LPZLIB) kload.lib
kserv_std.obj: kserv_std.cpp kserv.h shared.h kserv_config.h gdb.h lodcfg.h
kserv_std.res: 
	$(RC) -r -fo kserv_std.res kserv_std.rc

bserv_preview.lib: bserv_preview.obj crc32.obj
bserv.dll: bserv_preview.lib bserv_preview.res kload.lib
	$(LINK) $(LFLAGS) /out:bserv.dll /DLL bserv_preview.obj crc32.obj kload.lib bserv_preview.res $(LIBSDLL) d3dx8.lib winmm.lib /LIBPATH:$(LPPNGDIB) /LIBPATH:$(LPLIBPNG) /LIBPATH:$(LPZLIB) zdll.lib
bserv_preview.obj: bserv_preview.cpp bserv.h
bserv_preview.res: 
	$(RC) -r -fo bserv_preview.res bserv_preview.rc
	
fserv.lib: fserv.obj
fserv.dll: fserv.lib fserv_config.obj crc32.obj fserv.res kload.lib
	$(LINK) $(LFLAGS) /out:fserv.dll /DLL fserv.obj fserv_config.obj crc32.obj kload.lib fserv.res $(LIBS) d3dx8.lib
fserv.obj: fserv.cpp fserv.h fserv_config.h
fserv_config.obj: fserv_config.cpp fserv_config.h
fserv.res: 
	$(RC) -r -fo fserv.res fserv.rc

bootserv.lib: bootserv.obj
bootserv.dll: bootserv.lib bootserv.res kload.lib
	$(LINK) $(LFLAGS) /out:bootserv.dll /DLL bootserv.obj kload.lib d3dx8.lib bootserv.res $(LIBS)
bootserv.obj: bootserv.cpp bootserv.h

bootserv.res: 
	$(RC) -r -fo bootserv.res bootserv.rc

gloves.lib: gloves.obj
gloves.dll: gloves.lib gloves.res kload.lib
	$(LINK) $(LFLAGS) /out:gloves.dll /DLL gloves.obj kload.lib d3dx8.lib gloves.res $(LIBS)
gloves.obj: gloves.cpp gloves.h

gloves.res: 
	$(RC) -r -fo gloves.res gloves.rc
	
skins.lib: skins.obj
skins.dll: skins.lib skins.res kload.lib
	$(LINK) $(LFLAGS) /out:skins.dll /DLL skins.obj kload.lib d3dx8.lib skins.res $(LIBS)
skins.obj: skins.cpp skins.h

skins.res: 
	$(RC) -r -fo skins.res skins.rc
	
stadium.lib: stadium.obj crc32.obj afsreader.obj hooklib.obj
stadium.dll: stadium.lib stadium.res kload.lib
	$(LINK) $(LFLAGS) /out:stadium.dll /DLL stadium.obj crc32.obj afsreader.obj kload.lib hooklib.obj stadium.res $(LIBSDLL) d3dx8.lib /LIBPATH:$(LPPNGDIB) /LIBPATH:$(LPLIBPNG) /LIBPATH:$(LPZLIB)
stadium.obj: stadium.cpp stadium.h
stadium.res: 
	$(RC) -r -fo stadium.res stadium.rc

afs2fs.lib: afs2fs.obj crc32.obj afsreader.obj hooklib.obj
afs2fs.dll: afs2fs.lib afs2fs.res kload.lib
	$(LINK) $(LFLAGS) /out:afs2fs.dll /DLL afs2fs.obj crc32.obj afsreader.obj kload.lib hooklib.obj afs2fs.res $(LIBSDLL) d3dx8.lib /LIBPATH:$(LPPNGDIB) /LIBPATH:$(LPLIBPNG) /LIBPATH:$(LPZLIB)
afs2fs.obj: afs2fs.cpp afs2fs.h utf8.h

afs2fs.res: 
	$(RC) -r -fo afs2fs.res afs2fs.rc

lodmixer.lib: lodmixer.obj
lodmixer.dll: lodmixer.lib lodmixer.res kload.lib
	$(LINK) $(LFLAGS) /out:lodmixer.dll /DLL lodmixer.obj kload.lib lodmixer.res $(LIBS)
lodmixer.obj: lodmixer.cpp lodmixer.h

lodmixer.res: 
	$(RC) -r -fo lodmixer.res lodmixer.rc
	
dxtools.lib: dxtools.obj
dxtools.dll: dxtools.lib dxtools.res kload.lib
	$(LINK) $(LFLAGS) /out:dxtools.dll /DLL dxtools.obj kload.lib dxtools.res $(LIBS)
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

#teams.lib: teams.obj
#teams.dll: teams.lib teams.res
#	$(LINK) $(LFLAGS) /out:teams.dll /DLL teams.obj kload.lib teams.res $(LIBS)
#teams.obj: teams.cpp teams.h
#
#teams.res: 
#	$(RC) -r -fo teams.res teams.rc

psc.lib: psc.obj
psc.dll: psc.lib psc.res kload.lib
	$(LINK) $(LFLAGS) /out:psc.dll /DLL psc.obj kload.lib psc.res $(LIBS)
psc.obj: psc.cpp psc.h

psc.res: 
	$(RC) -r -fo psc.res psc.rc

clock.lib: clock.obj
clock.dll: clock.lib clock.res kload.lib
	$(LINK) $(LFLAGS) /out:clock.dll /DLL clock.obj kload.lib clock.res $(LIBS)
clock.obj: clock.cpp clock.h

clock.res: 
	$(RC) -r -fo clock.res clock.rc

roster.lib: roster.obj md5.obj
roster.dll: roster.lib roster.res kload.lib
	$(LINK) $(LFLAGS) /out:roster.dll /DLL roster.obj md5.obj kload.lib d3dx8.lib roster.res $(LIBS)
roster.obj: roster.cpp roster.h
md5.obj: md5.c md5.h

speeder.lib: speeder.obj apihijack.obj
speeder.dll: speeder.lib speeder.res kload.lib
	$(LINK) $(LFLAGS) /out:speeder.dll /DLL speeder.obj apihijack.obj kload.lib speeder.res $(LIBS)
speeder.obj: speeder.cpp speeder.h
apihijack.obj: apihijack.cpp apihijack.h

setupgui.obj: setupgui.cpp setupgui.h
setup.obj: setup.cpp setup.h setupgui.h
setup.exe: setup.obj detect.obj setupgui.obj imageutil.obj setup.res
	$(LINK) $(LFLAGS) /out:setup.exe setup.obj detect.obj setupgui.obj imageutil.obj setup.res $(LIBS) shell32.lib
	
setup.res: 
	$(RC) -r -fo setup.res setup.rc

lodcfg.exe: lodcfg.obj detect.obj lodcfgui.obj imageutil.obj
	$(LINK) $(LFLAGS) /out:lodcfg.exe lodcfg.obj detect.obj lodcfgui.obj imageutil.obj $(LIBS)

keyconfui.obj: keyconfui.cpp keyconfui.h
keyconf.obj: keyconf.cpp keyconf.h keyconfui.h
keyconf.exe: keyconf.obj keyconfui.obj keycfg.obj
	$(LINK) $(LFLAGS) /out:keyconf.exe keyconf.obj keyconfui.obj keycfg.obj $(LIBS)

afstest.obj: afstest.cpp afsreader.h
afstest.exe: afstest.obj afsreader.obj crc32.obj alog.obj aconfig.obj
	$(LINK) $(LFLAGS) /out:afstest.exe afstest.obj afsreader.obj crc32.obj alog.obj aconfig.obj $(LIBS)

afswalk.obj: afswalk.cpp afsreader.h
afswalk.exe: afswalk.obj afsreader.obj crc32.obj alog.obj aconfig.obj
	$(LINK) $(LFLAGS) /out:afswalk.exe afswalk.obj afsreader.obj crc32.obj alog.obj aconfig.obj $(LIBS)

afsexport.obj: afsexport.cpp afsreader.h
afsexport.exe: afsexport.obj afsreader.obj crc32.obj alog.obj aconfig.obj
	$(LINK) $(LFLAGS) /out:afsexport.exe afsexport.obj afsreader.obj crc32.obj alog.obj aconfig.obj $(LIBS)
	
#stadimp.obj: stadimp.cpp
#stadimp.exe: stadimp.obj
#	$(LINK) $(LFLAGS) /out:stadimp.exe stadimp.obj $(LIBS)

filedec.obj: filedec.cpp
filedec.exe: filedec.obj
	$(LINK) $(LFLAGS) /out:filedec.exe filedec.obj $(LIBS)
	
speedtest.obj: speedtest.cpp
speedtest.exe: speedtest.obj
	$(LINK) $(LFLAGS) /out:speedtest.exe speedtest.obj $(LIBS)
	
vista_test.obj: vista_test.cpp
vista_test.exe: vista_test.obj
	$(LINK) $(LFLAGS) /out:vista_test.exe vista_test.obj $(LIBS)
	
#diaggui.obj: diaggui.cpp diaggui.h
#diag.exe: diag.obj diaggui.obj
#	$(LINK) $(LFLAGS) /out:diag.exe diag.obj diaggui.obj $(LIBS)
#diag.obj: diag.cpp diag.h diaggui.h
	
.cpp.obj:
	$(CC) $(CFLAGS) -c $(INC) /I $(INCPNGDIB) $<

clean:
	del /Q /F *.exp *.lib *.obj *.res *.dll *.exe
	(cd soft\pngdib && $(MAKE) clean)
	if exist output (rd /S /Q output)

clean-all:
	del /Q /F *.exp *.lib *.obj *.res *.dll *.exe *.log *~
	(cd soft\pngdib && $(MAKE) clean-all)
	if exist output (rd /S /Q output)
	
