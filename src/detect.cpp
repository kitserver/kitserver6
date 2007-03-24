// detect.cpp

#include "windows.h"
#include <stdio.h>
#include "detect.h"
#include "imageutil.h"

char* GAME[] = { 
	"PES6 PC",
	"PES6 PC 1.10",
    "WE-PES 2007 PC",
};
char* GAME_GUID[] = {
    "{20418978-752D-491c-AF1E-A2EDE086E000}",
    "{20418978-752D-491c-AF1E-A2EDE086E000}",
    "{EEF9975D-2F76-42c6-AC44-8F84C6B4E68D}",
};
DWORD GAME_GUID_OFFSETS[] = { 
    0x77dca8, 
    0x77eca8,
    0x77eca8,
};

// Returns the game version id
int GetGameVersion(void)
{
	HMODULE hMod = GetModuleHandle(NULL);
	for (int i=0; i<sizeof(GAME_GUID)/sizeof(char*); i++)
	{
		char* guid = (char*)((DWORD)hMod + GAME_GUID_OFFSETS[i]);
		if (strncmp(guid, GAME_GUID[i], lstrlen(GAME_GUID[i]))==0)
			return i;
	}
	return -1;
}

// Returns the game version id
int GetGameVersion(char* filename)
{
	char guid[] = "{00000000-0000-0000-0000-000000000000}";

	FILE* f = fopen(filename, "rb");
	if (f == NULL)
		return -1;

	// check for regular exes
	for (int i=0; i<sizeof(GAME_GUID)/sizeof(char*); i++)
	{
		fseek(f, GAME_GUID_OFFSETS[i], SEEK_SET);
		fread(guid, lstrlen(GAME_GUID[i]), 1, f);
		if (strncmp(guid, GAME_GUID[i], lstrlen(GAME_GUID[i]))==0)
		{
			fclose(f);
			return i;
		}
	}

	// unrecognized.
	return -1;
}

