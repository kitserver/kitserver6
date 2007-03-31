// afsreplace.cpp

#include <windows.h>
#include <stdio.h>
#include "log.h"
#include "manage.h"
#include "hook.h"
#include "kload.h"
#include "kload_config.h"
#include "numpages.h"
//#include "input.h"
//#include "keycfg.h"
#include "afsreplace.h"

#include <hash_map>

extern KMOD k_kload;
extern PESINFO g_pesinfo;
//extern KLOAD_CONFIG g_config;

#define CODELEN 3
enum {
    C_NEWFILEFROMAFS_CS, C_ALLOCATEBUFFERS_CS, C_AFTERFILEFROMAFS_CS,
};
static DWORD codeArray[][CODELEN] = {
    // PES6
    {
        0x65b63f, 0x45a607, 0x65b668,
    },
    // PES6 1.10
    {
        0x65b831, 0, 0x65b8a7,
    },
    // WE2007
    {
        0, 0, 0,
    },
};

#define DATALEN 1
enum {
    AFS_PAGELEN_TABLE,
};
static DWORD dataArray[][DATALEN] = {
    // PES6
    {
        0x3b5cbc0,
    },
    // PES6 1.10
    {
        0x3b5dbc0,
    },
	// WE2007
    {
        0,
    },
};

static DWORD code[CODELEN];
static DWORD data[DATALEN];

CRITICAL_SECTION _ar_cs;
typedef void (*afsreplace_callback_t)(GETFILEINFO* gfi);
vector<afsreplace_callback_t> _afsreplace_vec;

// This is possible since the functions which need it are subfunctions
// of NewFileFromAFS() and are called before AfterFileFromAFS()
GETFILEINFO* lastGetFileInfo = NULL;
DWORD nextUniqueId = 0;
std::hash_map<DWORD,GETFILEINFO*> g_Files;
std::hash_map<DWORD,GETFILEINFO*>::iterator g_FilesIterator;

DWORD g_lastBufHeader = 0;

#define HASREPLACEBUFFER (lastGetFileInfo->replaceBuf != NULL && lastGetFileInfo->replaceSize > 0)
#define MAP_FIND(map,key) map[key]
#define MAP_CONTAINS(map,key) (map.find(key)!=map.end())

//--------------------------------------------
//----------- END OF DEFINITIONS ------------
//--------------------------------------------


KEXPORT void InitAfsReplace()
{
	InitializeCriticalSection(&_ar_cs);
    _afsreplace_vec.clear();
	
    // Determine the game version
    int v = GetPESInfo()->GameVersion;
    if (v != -1)
    {
        memcpy(code, codeArray[v], sizeof(code));
        memcpy(data, dataArray[v], sizeof(data));
    }
    
    RegisterGetNumPagesCallback(afsGetNumPages);
    return;
}

KEXPORT void HookAfsReplace()
{
	MasterHookFunction(code[C_NEWFILEFROMAFS_CS], 0, afsNewFileFromAFS);
	MasterHookFunction(code[C_ALLOCATEBUFFERS_CS], 5, afsAllocateBuffers);
	HookFunction(hk_FileFromAFS, (DWORD)afsAfterFileFromAFS);
	HookFunction(hk_ReadFile, (DWORD)afsBeforeReadFile);
	HookFunction(hk_AfterReadFile, (DWORD)afsAfterReadFile);
	
	return;
}

KEXPORT void UnhookAfsReplace()
{
	MasterUnhookFunction(code[C_NEWFILEFROMAFS_CS], afsNewFileFromAFS);
	MasterUnhookFunction(code[C_ALLOCATEBUFFERS_CS], afsAllocateBuffers);
	UnhookFunction(hk_FileFromAFS, (DWORD)afsAfterFileFromAFS);
	UnhookFunction(hk_ReadFile, (DWORD)afsBeforeReadFile);
	UnhookFunction(hk_AfterReadFile, (DWORD)afsAfterReadFile);
	
	// free all file buffers
	for (g_FilesIterator = g_Files.begin(); g_FilesIterator != g_Files.end();
				g_FilesIterator++) {
		LogWithNumber(&k_kload, "Buffer with %d bytes was still allocated!", 
									(g_FilesIterator->second)->replaceSize);
		makeReplaceBuffer(0, g_FilesIterator->second);
		HeapFree(GetProcessHeap(),0,(void*)g_FilesIterator->second);
	}
	g_Files.clear();
	
	DeleteCriticalSection(&_ar_cs);
	return;
}

KEXPORT void RegisterAfsReplaceCallback(void* callback)
{
    EnterCriticalSection(&_ar_cs);
    _afsreplace_vec.push_back((afsreplace_callback_t)callback);
    LogWithNumber(&k_kload, "RegisterAfsReplaceCallback(%08x)", (DWORD)callback);
    LeaveCriticalSection(&_ar_cs);
}

// From here, there is one call per loaded file. The values
// inside the structure can be changed, so that various scenarios
// are possible:
// - replacing a file,
// - changing the file which is loaded to another one from the AFS,
// - both (like for fserv, where the fileId is not valid but a special id)
DWORD afsNewFileFromAFS()
{
	DWORD infoBlock, res;
	__asm mov infoBlock, esi
	INFOBLOCK* ib = (INFOBLOCK*)infoBlock;
	
	lastGetFileInfo = (GETFILEINFO*)HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,sizeof(GETFILEINFO));
	
	// Fill the structure
	lastGetFileInfo->uniqueId = nextUniqueId;
	lastGetFileInfo->isProcessed = false;
	lastGetFileInfo->fileId = lastGetFileInfo->oldFileId = ib->FileID;
	lastGetFileInfo->replaceBuf = NULL;
	lastGetFileInfo->replaceSize = 0;
	lastGetFileInfo->firstPage = 0;
	
	nextUniqueId = (nextUniqueId == 0xffffffff)?0 : nextUniqueId + 1;
	
	// This is the call to the modules where parameters can be changed
	// only isProcessed and fileId should be changed directly, to replace
	// a file with other data, use makeReplaceBuffer() or loadReplaceFile()
	for (vector<afsreplace_callback_t>::iterator it = _afsreplace_vec.begin(); it != _afsreplace_vec.end(); it++) {
		(*it)(lastGetFileInfo);
	}
/*
	lastGetFileInfo->fileId = 0x10000 + 6949;
	
	if (lastGetFileInfo->fileId == 0x10000 + 6949) {
		loadReplaceFile("E:\\Pro Evolution Soccer 6\\kitserver\\GDB\\stadiums\\ZZZ - n4d4 arena\\1_day_fine\\stad1_main.bin");
	}
*/
	if (lastGetFileInfo->fileId != lastGetFileInfo->oldFileId) {
		ib->FileID = lastGetFileInfo->fileId;
		
		LogWithTwoNumbers(&k_kload, "Replace fileId: 0x%x -> 0x%x", lastGetFileInfo->oldFileId,
				lastGetFileInfo->fileId);
	}
		
	__asm mov esi, infoBlock
	return MasterCallNext();
}

// Change the size here, if a replacing buffer is given
bool afsGetNumPages(DWORD fileId, DWORD afsId, DWORD* retval)
{
	if (!lastGetFileInfo) return false;
	if (!HASREPLACEBUFFER) return false;

	*retval = lastGetFileInfo->replaceSize / 0x800 + 1;
	return true;
}

// If we need to replace data, save the address of a header which later connects
// the GETFILEINFO structure to the file read in ReadFile()
DWORD afsAllocateBuffers(DWORD p1, DWORD p2, DWORD*** p3, DWORD p4, DWORD p5)
{
	DWORD res = MasterCallNext(p1, p2, p3, p4, p5);
	if (!lastGetFileInfo) return res;
	// we don't need this if we don't want to replace the file
	if (!HASREPLACEBUFFER) return false;
	
	DWORD bufHeader = 0;
	
	try {
		bufHeader = *( *( *(p3 + 1) + 2) + 1);
		lastGetFileInfo->firstPage = *((DWORD*)p3 + 12);
	 } catch (...) {
	 	// we can't replace the data later without this information, so
	 	// free the buffer
	 	makeReplaceBuffer(0);
	 	Log(&k_kload, "Couldn't find address for buffer header.");
	 	return res;
	}
	
	// if something already exists here, free it
	// this also means that this file wasn't replaced
	if (MAP_CONTAINS(g_Files, bufHeader)) {
		GETFILEINFO* oldBufHeader = g_Files[bufHeader];
		LogWithNumber(&k_kload, "Buffer with %d bytes freed.", oldBufHeader->replaceSize);
		makeReplaceBuffer(0, oldBufHeader);
		HeapFree(GetProcessHeap(),0,(void*)oldBufHeader);
		g_Files.erase(bufHeader);
	}
	// in ReadFile, we can find the GETFILEINFO for the loaded file with that
	g_Files[bufHeader] = lastGetFileInfo;
	
	return res;
}

// By now, the calls to NewFileFromAFS(), GetNumPages() and AllocateBuffers() are
// finished
DWORD afsAfterFileFromAFS()
{
	DWORD res;
	__asm mov res, eax
	if (!lastGetFileInfo) return res;
	
	// For replaced files, the memory isn't freed yet since
	// we need the structure later in ReadFile()
	if (!HASREPLACEBUFFER) {
		HeapFree(GetProcessHeap(),0,(void*)lastGetFileInfo);
	}
	lastGetFileInfo = NULL;
	
	return res;
}



// the calls to ReadFile can happen independently from the functions above

void afsBeforeReadFile(HANDLE hFile, LPVOID lpBuffer, DWORD nNumberOfBytesToRead,
  LPDWORD lpNumberOfBytesRead,  LPOVERLAPPED lpOverlapped)
{
	// important to connect this call to the corresponding GETFILEINFO structure
	__asm mov g_lastBufHeader, esi
    //LogWithNumber(&k_kload, "afsReadFile: g_lastBufHeader = %08x", g_lastBufHeader);
    
	return;
}


void afsAfterReadFile(HANDLE hFile, LPVOID lpBuffer, DWORD nNumberOfBytesToRead,
  LPDWORD lpNumberOfBytesRead,  LPOVERLAPPED lpOverlapped)
{
	// check if we have some data to replace this file
	if (MAP_CONTAINS(g_Files, g_lastBufHeader)) {
		GETFILEINFO* gfi = g_Files[g_lastBufHeader];
		
		// find out which page we are reading
		DWORD currPage = *(DWORD*)(g_lastBufHeader + 0x1c);
		DWORD readingPos = (currPage - gfi->firstPage) * 0x800;
		DWORD bytesLeft = gfi->replaceSize - readingPos;
		DWORD bytesToCopy = min(nNumberOfBytesToRead, bytesLeft);

		LogWithTwoNumbers(&k_kload, "Replacing bytes 0x%x to 0x%x...", readingPos,
								readingPos + bytesToCopy - 1);

		memcpy(lpBuffer, (BYTE*)gfi->replaceBuf + readingPos, bytesToCopy);
		
		// if this was the last part, free the memory
		if (bytesToCopy == bytesLeft) {
			LogWithNumber(&k_kload, "Last part -> buffer freed", gfi->replaceSize);
			makeReplaceBuffer(0, gfi);
			HeapFree(GetProcessHeap(),0,(void*)gfi);
			g_Files.erase(g_lastBufHeader);
		}
	}
	
	// reset
	g_lastBufHeader = 0;

	return;
}

//
//------- EXPORTED FUNCTIONS --------

// splits the fileId from the infoBlock into fileId and afsId
KEXPORT DWORD splitFileId(DWORD fileId, DWORD* afsId)
{
	if (afsId) {
		*afsId = (BYTE)((fileId>>16) & 0xff);
	}
	return (fileId & 0xffff);
}

// returns the size which is usually reserved for a specific file
KEXPORT DWORD getAfsFileSize(DWORD fileId, DWORD afsId)
{
	DWORD* g_pageLenTable = (DWORD*)data[AFS_PAGELEN_TABLE];
	DWORD* pPageLenTable = (DWORD*)(g_pageLenTable[afsId] + 0x11c);
	return pPageLenTable[fileId];
}

// allocate memory, where the data for replacement is stored
// if this is called multiple times for one call to NewFileFromAFS,
// only the last buffer will be used, all others will be freed.
// the used buffer will be freed after ReadFile completed for all
// parts
KEXPORT void* makeReplaceBuffer(DWORD size, GETFILEINFO* gfi1)
{
	GETFILEINFO* gfi = ((DWORD)gfi1 == 0xffffffff)?lastGetFileInfo : gfi1;
	if (!gfi) return NULL;
	
	void* addr = NULL;

	// free buffer if already reserved before
	if (gfi->replaceBuf != NULL) {
		HeapFree(GetProcessHeap(),0,gfi->replaceBuf);
	}
	
	gfi->replaceBuf = NULL;
	gfi->replaceSize = 0;

	// if the size is 0, don't make a buffer
	if (size > 0) {
		addr = HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,size);
		gfi->replaceBuf = addr;
		gfi->replaceSize = size;
	}
		
	return addr;
}

// set a specific as replacement
KEXPORT bool loadReplaceFile(char* filename)
{
	if (!lastGetFileInfo) return false;
	DWORD bytesRead = 0;
	
	HANDLE hFile = CreateFile(filename,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,
			FILE_ATTRIBUTE_NORMAL,NULL);
	if (hFile == INVALID_HANDLE_VALUE) {
		LogWithString(&k_kload, "Couldn't open %s", filename);
		return false;
	}
	DWORD fileSize = GetFileSize(hFile, NULL);
	if (fileSize == 0) {
		LogWithString(&k_kload, "Size is 0 for %s", filename);
		CloseHandle(hFile);
		return false;
	}
	void* repBuf = makeReplaceBuffer(fileSize);
	if (!repBuf) {
		LogWithString(&k_kload, "Couldn't alloc memory for  %s", filename);
		CloseHandle(hFile);
		return false;
	}
	
	if (ReadFile(hFile, repBuf, fileSize, &bytesRead, NULL) == 0) {
		LogWithString(&k_kload, "Couldn't read %s", filename);
		CloseHandle(hFile);
		makeReplaceBuffer(0);
		return false;
	}
	
	LogWithNumberAndString(&k_kload, "Successfully loaded %d bytes of %s", bytesRead, filename);
	CloseHandle(hFile);	
	return true;
}
