#include <windows.h>
#include <stdio.h>

#include "aconfig.h"
#include "afsreader.h"

#define MAXPATHLEN 8192

KSERV_CONFIG g_config;
int nonEmptyCount = 0;

// Recursive function to list information about current AFS dir
// parameters:
//     currDirName - (in) pointer to a buffer that contains current long name.
//     f           - (in) reference to open FILE object.
//     totalItems  - (out) number of items in current AFS dir
void ListItems(FILE* log, char* currDirName, FILE* f, int* totalItems)
{
	DWORD base = ftell(f);

	AFSDIRHEADER afs;
	ZeroMemory(&afs, sizeof(AFSDIRHEADER));
	fread(&afs, sizeof(AFSDIRHEADER), 1, f);

	// determine where list of filenames is
	AFSITEMINFO nameList;
	ZeroMemory(&nameList, sizeof(AFSITEMINFO));
	fseek(f, afs.dwNumFiles * sizeof(AFSITEMINFO), SEEK_CUR);
	fread(&nameList, sizeof(AFSITEMINFO), 1, f);

	fseek(f, base + sizeof(AFSDIRHEADER), SEEK_SET);
	for (int i=0; i<afs.dwNumFiles; i++)
	{
		AFSITEMINFO ii;
		ZeroMemory(&ii, sizeof(AFSITEMINFO));
		fread(&ii, sizeof(AFSITEMINFO), 1, f);
		DWORD curr = ftell(f);

		// determine the name of the item
		AFSNAMEINFO name;
		ZeroMemory(&name, sizeof(AFSNAMEINFO));
		fseek(f, base + nameList.dwOffset + i * sizeof(AFSNAMEINFO), SEEK_SET);
		fread(&name, sizeof(AFSNAMEINFO), 1, f);
		sprintf(name.szFileName, "id_%00005d.bin", i);

		// find out if this item is a dir itself.
		BOOL isDir = FALSE;
		AFSDIRHEADER hdr;
		ZeroMemory(&hdr, sizeof(AFSDIRHEADER));
		fseek(f, base + ii.dwOffset, SEEK_SET);
		fread(&hdr, sizeof(AFSDIRHEADER), 1, f);
		if (hdr.dwSig == AFSSIG)
			isDir = TRUE;

		// print information
		fprintf(log, "abs.offset = %08x, size = %08x: %s%s", 
				base + ii.dwOffset, ii.dwSize, 
				currDirName, name.szFileName);
		if (isDir) fprintf(log, "/");
		if (ii.dwSize > 0) {
			fprintf(log, " %sunnamed_%d", currDirName, nonEmptyCount++);
		}
		fprintf(log, "\n");

		// update item count
		*totalItems = *totalItems + 1;

		// recursively call self if this item is a subfolder.
		if (isDir)
		{
			char newDirName[MAXPATHLEN];
			ZeroMemory(newDirName, MAXPATHLEN);
			_snprintf(newDirName, MAXPATHLEN-1, "%s%s/", currDirName, name.szFileName);

			fseek(f, -sizeof(AFSDIRHEADER), SEEK_CUR);
			ListItems(log, newDirName, f, totalItems);
		}

		// set file pointer on next item
		fseek(f, curr, SEEK_SET);
	}
}

// Another test of AFS-reader API.
// Recursively navigates AFS and list information about all
// items in it.
void main(int argc, char* argv[])
{
	char afsFileName[1024];
	ZeroMemory(afsFileName, 1024);

	// if specific filename not specified, assume default AFS
	if (argc > 1)
		lstrcpy(afsFileName, argv[1]);
	else
		lstrcpy(afsFileName, "0_text.afs");

	FILE* f = fopen(afsFileName, "rb");
	if (f == NULL)
	{
		printf("ERROR: Cannot open AFS file: %s\n", afsFileName);
		return;
	}

	char logName[1024];
	ZeroMemory(logName, 1024);
	lstrcpy(logName, afsFileName);
	lstrcat(logName, ".log");

	FILE* log = fopen(logName, "wt");
	if (log == NULL) log = stdout;

	fprintf(log, "Item information for %s:\n", afsFileName);
	fprintf(log, "---------------------------------------------------------\n");

	int totalItems = 0;
	ListItems(log, "/", f, &totalItems);

	fprintf(log, "---------------------------------------------------------\n");
	fprintf(log, "Total items: %d\n", totalItems);
	fclose(log);

	fclose(f);
}
