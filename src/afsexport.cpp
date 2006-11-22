#include <windows.h>
#include <stdio.h>

#include "aconfig.h"
#include "afsreader.h"

KSERV_CONFIG g_config;

// Simple AFS-reader exporter.
// Exports a file from AFS by id
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

	FILE* f = fopen(afsFileName,"rb");
	fseek(f, itemInfo.dwOffset, SEEK_SET);
	char szFileName[32];
	ZeroMemory(szFileName, sizeof(szFileName));
	sprintf(szFileName, "id_%00005d.bin", id);
	FILE* out = fopen(szFileName, "wb");
	BYTE* buf = (BYTE*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, itemInfo.dwSize);
	if (fread(buf, itemInfo.dwSize, 1, f) == 1) {
		fwrite(buf, itemInfo.dwSize, 1, out);
	} else {
		printf("ERROR: Unable to read %d bytes from AFS-file\n", itemInfo.dwSize);
	}
	HeapFree(GetProcessHeap(), 0, buf);
	fclose(out);
	fclose(f);

	printf("File exported as %s\n", szFileName);
}
