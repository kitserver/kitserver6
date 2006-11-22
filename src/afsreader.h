#ifndef _JUCE_AFSREADER_
#define _JUCE_AFSREADER_

#include <stdio.h>

// AFS folder signature
#define AFSSIG 0x00534641

// Error codes for function return values
#define AFS_OK 0
#define AFS_FOPEN_FAILED -1
#define AFS_HEAPALLOC_FAILED -2
#define AFS_ITEM_NOT_FOUND -3

typedef struct _AFSDIRHEADER {
	DWORD dwSig; // 4-byte signature: "AFS\0"
	DWORD dwNumFiles; // number of files in the afs
} AFSDIRHEADER;

typedef struct _AFSITEMINFO {
	DWORD dwOffset; // offset from the beginning of afs
	DWORD dwSize; // size of the item
} AFSITEMINFO;

typedef struct _AFSNAMEINFO {
	char szFileName[32]; // Zero-terminated filename string
	BYTE other[16]; // misc info
} AFSNAMEINFO;

char* GetAfsErrorText(DWORD errorCode);
DWORD GetUniSig(char* afsFileName, char* uniFileName);
DWORD GetItemInfo(char* afsFileName, char* uniFileName, AFSITEMINFO* itemInfo, DWORD* base);
DWORD GetItemInfoById(char* afsFileName, int id, AFSITEMINFO* itemInfo, DWORD* base);
DWORD GetKitInfo(char* afsFileName, AFSITEMINFO* itemInfoArray, DWORD numKits);
DWORD GetBallInfo(char* afsFileName, AFSITEMINFO* itemInfoArray, DWORD numBalls);
DWORD GetBallSigs(char* afsFileName, AFSITEMINFO* itemInfoArray, DWORD* m, DWORD* t, 
int* which, DWORD numBalls);
DWORD ReadItemInfoById(FILE* f, DWORD id, AFSITEMINFO* itemInfo, DWORD base);

#endif
