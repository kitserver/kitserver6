#include <windows.h>
#include <stdio.h>

#include "aconfig.h"
#include "afsreader.h"

KSERV_CONFIG g_config;

// Simple AFS-reader test.
// Searches for a given filename in the AFS.
// (First item that matches the filename is returned.)
void main(int argc, char* argv[])
{
	char afsFileName[1024];
	ZeroMemory(afsFileName, 1024);

	if (argc < 2)
	{
		printf("Usage: %s <file-id> [<afs-file>]\n", argv[0]);
		return;
	}

	// if specific filename not specified, assume default AFS
	if (argc > 2)
		lstrcpy(afsFileName, argv[2]);
	else
		lstrcpy(afsFileName, "0_text.afs");

	AFSITEMINFO itemInfo;
	ZeroMemory(&itemInfo, sizeof(AFSITEMINFO));
	DWORD base = 0;
	
	int id = atoi(argv[1]);
	DWORD result = GetItemInfoById(afsFileName, id, &itemInfo, &base);
	if (result != AFS_OK)
	{
		printf("ERROR: %s\n", GetAfsErrorText(result));
		return;
	}

	printf("base = %08x\n", base);
	printf("itemInfo.dwOffset = %08x\n", itemInfo.dwOffset);
	printf("itemInfo.dwSize = %08x\n", itemInfo.dwSize);
	printf("\n");
	printf("absolute offset = %08x\n", base + itemInfo.dwOffset);
}
