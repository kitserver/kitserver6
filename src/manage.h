// manage.h
#ifndef _MANAGE_
#define _MANAGE_

typedef struct _KMOD {
	DWORD id;
	LPTSTR NameLong;
	LPTSTR NameShort;
	DWORD debug;
} KMOD;

typedef struct _PESINFO {
	LPTSTR mydir;
	LPTSTR pesdir;
	LPTSTR processfile;
	LPTSTR shortProcessfile;
	LPTSTR shortProcessfileNoExt;
	LPTSTR logName;
	LPTSTR gdbDir;
	int GameVersion;
	HINSTANCE hProc;
	UINT bbWidth;
	UINT bbHeight;
	double stretchX;
	double stretchY;
} PESINFO;

enum {gvPES6PC,gvPES6PC110};

#endif
