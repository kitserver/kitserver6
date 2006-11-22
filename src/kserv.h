#ifndef _DEFINED_MYDLL
#define _DEFINED_MYDLL

#include "afsreader.h"
#include "gdb.h"

#if _MSC_VER > 1000
#pragma once
#endif

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#ifdef _COMPILING_MYDLL
#define LIBSPEC __declspec(dllexport)
#else
#define LIBSPEC __declspec(dllimport)
#endif /* _COMPILING_MYDLL */

#define BUFLEN 4096  /* 4K buffer length */
#define MAX_FILEPATH BUFLEN

#define WM_APP_KEYDEF WM_APP + 1

#define VKEY_HOMEKIT 0
#define VKEY_AWAYKIT 1
#define VKEY_GKHOMEKIT 2
#define VKEY_GKAWAYKIT 3

LIBSPEC LRESULT CALLBACK KeyboardProc(int code, WPARAM wParam, LPARAM lParam);
LIBSPEC void RestoreDeviceMethods(void);

typedef struct _ENCBUFFERHEADER {
	DWORD dwSig;
	DWORD dwEncSize;
	DWORD dwDecSize;
	BYTE other[20];
} ENCBUFFERHEADER;

#define DEC_SIG_IMAGE 0x29857294
#define DEC_SIG_MODEL 0x20040520

typedef struct _DECBUFFERHEADER {
	DWORD dwSig;
	DWORD numTexs;
	DWORD dwDecSize;
	BYTE reserved1[4];
	WORD bitsOffset;
	WORD paletteOffset;
	WORD texWidth;
	WORD texHeight;
	BYTE reserved2[2];
	BYTE bitsPerPixel;
	BYTE reserver3[13];
	WORD blockWidth;
	WORD blockHeight;
	BYTE other[84];
} DECBUFFERHEADER;

#define AFS_FILENAME_LEN 32
typedef struct _TEAMBUFFERINFO {
	char gaFile[AFS_FILENAME_LEN];
    AFSITEMINFO ga;
	char gbFile[AFS_FILENAME_LEN];
	AFSITEMINFO gb;
	char paFile[AFS_FILENAME_LEN];
    AFSITEMINFO pa;
	char pbFile[AFS_FILENAME_LEN];
    AFSITEMINFO pb;
	char vgFile[AFS_FILENAME_LEN];
    AFSITEMINFO vg;
} TEAMBUFFERINFO;

// Attributes for player/goalkeeper record
typedef struct _KitAttr {
	BYTE model;
	BYTE cuffGK;
	BYTE cuffPL;
	BYTE collarGK;
	BYTE collarPL;
	BYTE shortsNumberLocationGK;
	BYTE shortsNumberLocationPL;
} KitAttr;

// Attributes record (one per team)
typedef struct _TeamAttr {
	BYTE numberType; // 0|1|2|3
	BYTE nameType; // 0|1
	BYTE nameShape; // 0|1 (0-straight, 1-curved)
	KitAttr home;
	KitAttr away;
} TeamAttr;

typedef struct _KitSlot {
	BYTE model;
	BYTE _unknown1;
	BYTE shortsNumberLocation;
	BYTE numberType;
	BYTE nameType;
	BYTE nameShape;
	BYTE _unknown2;
	BYTE isEdited;
	BYTE _unknown3;
	BYTE collar;
	WORD kitId;
	BYTE miscEditedData[18];
	RGBAColor shirtNameColor;
	RGBAColor shortsNumberColor;
	RGBAColor shirtNumberColor;
} KitSlot;

typedef struct _BigKitSlot {
	KitSlot kitSlot;
	BYTE _unknown[14];
} BigKitSlot;

typedef struct _TEXIMGPACKHEADER {
	DWORD numFiles;
	DWORD tocOffset;
	DWORD toc[1];
} TEXIMGPACKHEADER;

typedef struct _TEXIMGHEADER {
	BYTE unknown_1[8];
	DWORD dwSize;
	BYTE unknown_2[4];
	WORD dataOffset;
	WORD paletteOffset;
	WORD width;
	WORD height;
	BYTE bps;
} TEXIMGHEADER;

typedef struct _MEMITEMINFO {
	DWORD id;
	AFSITEMINFO afsItemInfo;
	DWORD address;
} MEMITEMINFO;

/*
typedef struct _KITINFO {
	BYTE unknown_1[0x2a];
	BYTE collar;
	BYTE unknown_2[7];
    BYTE nameLocation;
    BYTE nameType;
    BYTE nameShape;
    BYTE numberType;
    BYTE shortsNumberLocation;
    BYTE logoLocation;
    BYTE unknown_4[1];
	BYTE kitType;
	BYTE unknown_5;
	BYTE model;
	BYTE unknown_6[2];
} KITINFO;
*/

//I extended this structure a bit with information I found out using waterloo's tool.
//the used color format is a WORD value with 5 bits per color and the highest bit set (0x8000)
//as far as I remember, red and blue are swapped again, but I'm not sure about that
//(try it out or look at handling of radar.color attribute around line 5000!)
typedef struct _KITINFO {
	//shirtColors[0] is the base color and used for radar!
	WORD shirtColors[5];
	WORD shortsColors[4];
	WORD socksColors[3];
	//captain armband
	WORD captainColors[3];
	//border color
	WORD nameBC;
	//fill color
	WORD nameC;
	WORD numberBC;
	WORD numberC;
	WORD shortsnumBC;
	WORD shortsnumC;
	//collar is the first of 5 shirt layers, with only two values
	//the following 5 attributes specifie what type is used (same order as in PES5)
	//the colors (how much of them are used depends on selected type) can be set above
	BYTE collar;
	BYTE shirtLayers[4];
	BYTE shortsType;
	BYTE socksType;
	BYTE captainType;
    BYTE nameLocation;
    BYTE nameType;
    BYTE nameShape;
    BYTE numberType;
    BYTE shirtNumberLocation;
    BYTE shortsNumberLocation;
    BYTE logoLocation;
	BYTE unknown_2;
	BYTE kitType;
	BYTE unknown_3;
	BYTE model;
	BYTE unknown_4;
} KITINFO;

typedef struct _KITPACKINFO {
	KITINFO gkHome;
	KITINFO plHome;
	KITINFO gkAway;
	KITINFO plAway;
} KITPACKINFO;

typedef struct _TEAMKITINFO {
	DWORD teamId;
	KITPACKINFO kits;
    BYTE editInfo[0x128];
} TEAMKITINFO;

typedef struct _NATIONALKITINFO {
	KITPACKINFO kits;
	//BYTE mod;
	//DWORD address;
	//BYTE padding[0x63];
    BYTE padding[0x68];
} NATIONALKITINFO;

typedef struct _CLUBKITINFO {
	KITPACKINFO kits;
	//BYTE mod;
	//DWORD address;
	//BYTE padding[0x123];
	BYTE padding[0x128];
} CLUBKITINFO;

typedef struct _PNG_CHUNK_HEADER {
    DWORD dwSize;
    DWORD dwName;
} PNG_CHUNK_HEADER;

typedef struct _NEWKIT {
    BYTE* tex;
    RECT sizeBig;
    RECT sizeMedium;
    RECT sizeSmall;
} NEWKIT;

typedef struct _NEWKITFILES {
	char shirtName[BUFLEN];
	char shortsName[BUFLEN];
	char socksName[BUFLEN];
	char maskName[BUFLEN];
} NEWKITFILES;

/* ... more declarations as needed */
#undef LIBSPEC

#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif /* _DEFINED_MYDLL */

