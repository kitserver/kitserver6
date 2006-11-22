CC="c:\program files\microsoft visual studio\vc98\bin\cl"
LINK="c:\program files\microsoft visual studio\vc98\bin\link"
INC=/I "c:\program files\microsoft visual studio\vc98\include" /I "c:\juce\dxsdk9\include" /I "..\libpng-1.2.8-lib\include"
LIB=/LIBPATH:"c:\program files\microsoft visual studio\vc98\lib" /LIBPATH:"c:\juce\dxsdk9\lib"

CFLAGS=/nologo /GX $(EXTRA_CFLAGS)
LFLAGS=/NOLOGO /NODEFAULTLIB:libci.lib
LIBS=$(LIB) user32.lib gdi32.lib advapi32.lib libcpmt.lib
LIBSDLL=pngdib.obj libpng.lib zdll.lib $(LIBS)

ZLIBINC=/I "..\zlib123-dll\include"

all: pngdib.obj

pngdib.obj: pngdib.c pngdib.h

.c.obj:
	$(CC) $(CFLAGS) -c $(INC) $(ZLIBINC) $<

clean:
	del /Q /F *.exp *.lib *.obj *.res

clean-all:
	del /Q /F *.exp *.lib *.obj *.res *.log *~
