#include <windows.h>
#include <stdio.h>

#include "afsreader.h"
#include "log.h"
#include "manage.h"
#include "crc32.h"

#define MIN_ERROR_CODE -3
#define MAX_ERROR_CODE -1

//extern KMOD k_mydll;

// textual error messages
static char* unknownError = "UNKNOWN ERROR.";
static char* errorText[] = {
	"Cannot open AFS file.",
	"Heap allocation failed.",
	"Item not found in AFS.",
};

// Internal function prototypes
static DWORD FindItemInfoIndex(FILE* f, char* uniFileName, DWORD* pBase, DWORD* pIndex);

// Returns textual error message for a give code
char* GetAfsErrorText(DWORD code)
{
	if (code < MIN_ERROR_CODE || code > MAX_ERROR_CODE)
		return unknownError;

	int idx = -(int)code - 1;
	return errorText[idx];
}	

// Calculates CRC32 of encoded buffer of
// given uniform.
DWORD GetUniSig(char* afsFileName, char* uniFileName)
{
	DWORD base = 0;
	AFSITEMINFO itemInfo;
	ZeroMemory(&itemInfo, sizeof(AFSITEMINFO));

	// locate the file in AFS, using filename
	DWORD result = GetItemInfo(afsFileName, uniFileName, &itemInfo, &base);
	if (result != AFS_OK)
		return result;

	// Allocate buffer for the item
	BYTE* buffer = (BYTE*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, itemInfo.dwSize);
	if (buffer == NULL)
		return AFS_HEAPALLOC_FAILED;

	//TRACE2(&k_mydll,"GetUniSig: base offset = %08x", base);
	//TRACE2(&k_mydll,"GetUniSig: itemInfo.dwOffset = %08x", itemInfo.dwOffset);
	//TRACE2(&k_mydll,"GetUniSig: itemInfo.dwSize   = %08x", itemInfo.dwSize);

	FILE* f = fopen(afsFileName, "rb");
	if (f == NULL)
	{
		// release the buffer memory
		HeapFree(GetProcessHeap(), 0, buffer);
		return AFS_FOPEN_FAILED;
	}

	// Now, read the item into a buffer
	fseek(f, base + itemInfo.dwOffset, SEEK_SET);
	fread(buffer, itemInfo.dwSize, 1, f);

	// close the file
	fclose(f);

	// Calculate the signature
	DWORD sig = Sign(buffer);

	// release the buffer memory
	HeapFree(GetProcessHeap(), 0, buffer);
	return sig;
}

// Returns information about location of a file in AFS,
// given its name.
DWORD GetItemInfo(char* afsFileName, char* uniFileName, AFSITEMINFO* itemInfo, DWORD* pBase)
{
	FILE* f = fopen(afsFileName, "rb");
	if (f == NULL)
		return AFS_FOPEN_FAILED;

	DWORD index = 0;
	DWORD result = FindItemInfoIndex(f, uniFileName, pBase, &index);
	if (result != AFS_OK)
		return result;

	// fill-in the itemInfo
	fseek(f, *pBase + sizeof(AFSDIRHEADER) + sizeof(AFSITEMINFO) * index, SEEK_SET);
	fread(itemInfo, sizeof(AFSITEMINFO), 1, f);
	fclose(f);

	return result;
}

// Returns information about location of a file in AFS,
// given its id.
DWORD GetItemInfoById(char* afsFileName, int id, AFSITEMINFO* itemInfo, DWORD* pBase)
{
	FILE* f = fopen(afsFileName, "rb");
	if (f == NULL)
		return AFS_FOPEN_FAILED;

	DWORD index = id;
	*pBase = 0;

	// fill-in the itemInfo
	fseek(f, *pBase + sizeof(AFSDIRHEADER) + sizeof(AFSITEMINFO) * index, SEEK_SET);
	fread(itemInfo, sizeof(AFSITEMINFO), 1, f);
	fclose(f);

	return AFS_OK;
}

// Reads information about location of a file in AFS,
// given its id.
DWORD ReadItemInfoById(FILE* f, DWORD index, AFSITEMINFO* itemInfo, DWORD base)
{
	// fill-in the itemInfo
	fseek(f, base + sizeof(AFSDIRHEADER) + sizeof(AFSITEMINFO) * index, SEEK_SET);
	fread(itemInfo, sizeof(AFSITEMINFO), 1, f);
	return AFS_OK;
}

// Fills in the array of AFSITEMINFO structures with the corresponding info
// about all the uniforms. (numKits - maximum number of kits to store in array).
DWORD GetKitInfo(char* afsFileName, AFSITEMINFO* itemInfoArray, DWORD numKits)
{
	FILE* f = fopen(afsFileName, "rb");
	if (f == NULL)
		return AFS_FOPEN_FAILED;

	DWORD base = 0, index = 0;
	DWORD result = FindItemInfoIndex(f, "uni000ga.bin", &base, &index);
	if (result != AFS_OK)
		return result;

	// we now assume that all uniXXXyy.bin files are in the same AFS folder
	// and all come in order.
	fseek(f, base + sizeof(AFSDIRHEADER) + sizeof(AFSITEMINFO) * index, SEEK_SET);
	fread(itemInfoArray, sizeof(AFSITEMINFO) * numKits, 1, f); 

	fclose(f);

	return result;
}

// Fills in the array of AFSITEMINFO structures with the corresponding info
// about all the balls. (numBalls - maximum number of kits to store in array).
DWORD GetBallInfo(char* afsFileName, AFSITEMINFO* itemInfoArray, DWORD numBalls)
{
	FILE* f = fopen(afsFileName, "rb");
	if (f == NULL)
		return AFS_FOPEN_FAILED;

	DWORD base = 0, index = 0;
	DWORD result = FindItemInfoIndex(f, "ball00mdl.bin", &base, &index);
	if (result != AFS_OK)
		return result;

	// we now assume that all ball*.bin files are in the same AFS folder
	// and all come in order.
	fseek(f, base + sizeof(AFSDIRHEADER) + sizeof(AFSITEMINFO) * index, SEEK_SET);
	fread(itemInfoArray, sizeof(AFSITEMINFO) * numBalls, 1, f); 

	fclose(f);

	return result;
}

// Calculates signatures for ball files
// (numBalls - maximum number of kits to store in array).
DWORD GetBallSigs(char* afsFileName, AFSITEMINFO* itemInfoArray, 
		DWORD* mdls, DWORD* texs, int* which, DWORD numBalls)
{
	int mdl = 0, tex = 0;

	FILE* f = fopen(afsFileName, "rb");
	if (f == NULL)
		return AFS_FOPEN_FAILED;

	for (int i=0; i<numBalls; i++)
	{
		// Allocate buffer for the item
		BYTE* buffer = (BYTE*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, itemInfoArray[i].dwSize);
		if (buffer == NULL)
		{
			fclose(f);
			return AFS_HEAPALLOC_FAILED;
		}

		// Now, read the item into a buffer
		fseek(f, itemInfoArray[i].dwOffset, SEEK_SET);
		fread(buffer, itemInfoArray[i].dwSize, 1, f);

		// Calculate the signature
		DWORD sig = Sign(buffer);

		// free buffer
		HeapFree(GetProcessHeap(), 0, buffer);

		// put signature into models or textures array
		if (which[i] == 0) mdls[mdl++] = sig;
		else texs[tex++] = sig;
	}

	fclose(f);

	return AFS_OK;
}

// Returns information about where the corresponding AFSITEMINFO structure
// is located in the afs file. The answer consists of two parts: base, index.
// base   - is an offset to the beginning of AFS-folder that contains that AFSITEMINFO,
// index  - is the 0-based index of the AFSITEMINFO in the AFS-folder contents list.
static DWORD FindItemInfoIndex(FILE* f, char* uniFileName, DWORD* pBase, DWORD* pIndex)
{
	// remember current position
	DWORD afsBase = ftell(f);

	// read the header
	AFSDIRHEADER afh;
	fread(&afh, sizeof(AFSDIRHEADER), 1, f); 

	// skip the table of contents
	fseek(f, sizeof(AFSITEMINFO) * afh.dwNumFiles, SEEK_CUR);
	DWORD nameTableOffset = 0;
	fread(&nameTableOffset, sizeof(DWORD), 1, f);

	// seek the name-table
	fseek(f, afsBase + nameTableOffset, SEEK_SET);

	// allocate memory for name table read
	int nameTableSize = sizeof(AFSNAMEINFO) * afh.dwNumFiles;
	AFSNAMEINFO* fNames = (AFSNAMEINFO*)HeapAlloc(GetProcessHeap(), 
			HEAP_ZERO_MEMORY, nameTableSize);
	if (fNames == NULL)
		return AFS_HEAPALLOC_FAILED;

	fread(fNames, nameTableSize, 1, f); 
	
	// search for the filename
	int index = -1;
	for (int i=0; i<afh.dwNumFiles; i++)
	{
		if (strncmp(fNames[i].szFileName, uniFileName, 32)==0)
		{
			index = i;
			break;
		}
	}

	// free name-table memory
	HeapFree(GetProcessHeap(), 0, fNames);

	// check if we failed to find the file
	if (index == -1)
	{
		// File with given name was not found in this folder.
		// Recursively, navigate the child folders (if any exist) to
		// look for the file.
		fseek(f, afsBase + sizeof(AFSDIRHEADER), SEEK_SET);
		AFSITEMINFO* info = (AFSITEMINFO*)HeapAlloc(GetProcessHeap(),
				HEAP_ZERO_MEMORY, sizeof(AFSITEMINFO) * afh.dwNumFiles);
		if (info == NULL)
			return AFS_HEAPALLOC_FAILED;

		// read item info table
		fread(info, sizeof(AFSITEMINFO) * afh.dwNumFiles, 1, f);

		DWORD result = AFS_ITEM_NOT_FOUND;
		for (int k=0; k<afh.dwNumFiles; k++)
		{
			fseek(f, afsBase + info[k].dwOffset, SEEK_SET);

			AFSDIRHEADER fh;
			ZeroMemory(&fh, sizeof(AFSDIRHEADER));
			fread(&fh, sizeof(AFSDIRHEADER), 1, f);
			fseek(f, -sizeof(AFSDIRHEADER), SEEK_CUR);

			// check if this file is a child folder.
			if (fh.dwSig == AFSSIG)
			{
				result = FindItemInfoIndex(f, uniFileName, pBase, pIndex);
				if (result == AFS_OK) break;
			}
		}

		return result;
	}

	// set the base and index information.
	// The absolute offset of AFSITEMINFO structure in the file
	// can be calculated using this formula:
	//
	// absOffset = base + sizeof(AFSDIRHEADER) + sizeof(AFSITEMINFO) * index 
	//
	*pBase = afsBase;
	*pIndex = index;

	return AFS_OK;
}

