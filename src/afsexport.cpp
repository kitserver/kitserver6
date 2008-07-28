#include <windows.h>
#include <stdio.h>

#include "aconfig.h"
#include "afsreader.h"

KSERV_CONFIG g_config;

void ExportFile(char* afsFileName, int id, const char* nameMask)
{
	AFSITEMINFO itemInfo;
	ZeroMemory(&itemInfo, sizeof(AFSITEMINFO));
	DWORD base = 0;

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
	sprintf(szFileName, nameMask, id);
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

// Simple AFS-reader exporter.
// Exports a file from AFS by id
void main(int argc, char* argv[])
{
	char afsFileName[1024];
    char nameMask[128];
	ZeroMemory(afsFileName, sizeof(afsFileName));
	ZeroMemory(nameMask, sizeof(nameMask));

	if (argc < 3)
	{
		printf("Usage: %s <file-id(s)> <afs-file> [<name-mask>]\n", argv[0]);
		return;
	}

    lstrcpy(afsFileName, argv[2]);

	// if specific nameMask not specified, assume default mask
	if (argc > 3)
		lstrcpy(nameMask, argv[3]);
	else
		lstrcpy(nameMask, "id_%00005d.bin");

    char* splitter = strchr(argv[1], '-');
    if (!splitter) {
        // single file
        int id = atoi(argv[1]);
        ExportFile(afsFileName, id, nameMask);

    } else {
        // range of files
        int last = atoi(splitter+1);
        splitter[0] = '\0';
        int first = atoi(argv[1]);
        for (int id=first; id<=last; id++) {
            ExportFile(afsFileName, id, nameMask);
        }
    }
}
