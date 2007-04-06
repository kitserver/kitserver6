// afsreplace.cpp

#include <windows.h>
#include <stdio.h>
#include "log.h"
#include "manage.h"
#include "hook.h"
#include "kload.h"
#include "kload_config.h"
#include "numpages.h"
#include "afsreplace.h"

#include <hash_map>

extern KMOD k_kload;
extern PESINFO g_pesinfo;
//extern KLOAD_CONFIG g_config;

#define CODELEN 10
enum {
    C_NEWFILEFROMAFS_CS, C_ALLOCATEBUFFERS_CS, C_AFTERFILEFROMAFS_CS, C_FILEFROMAFS_JUMPHACK,
    C_UNIDECODE_CS, C_UNPACK, C_UNPACK_CS, C_MULTI_UNPACK_CS,
    C_READFILE_CS, C_ALLOCMEM_CS,
};
static DWORD codeArray[][CODELEN] = {
    // PES6
    {
        0x65b63f, 0x45a607, 0x65b668, 0,
        0x8b1983, 0x8cd490, 0x65b176, 0x8cd469,
        0x44c014, 0x65b152,
    },
    // PES6 1.10
    {
        0x65b831, 0x45a657, 0x65b8a7, 0x65b85b,
        0x8b1a63, 0x8cd580, 0x65b216, 0x8cd559,
        0x44c064, 0x65b1f2,
    },
    // WE2007
    {
        0, 0, 0, 0,
        0, 0, 0, 0,
        0, 0,
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

typedef DWORD  (*ORGUNPACK)(DWORD, DWORD, DWORD, DWORD, DWORD*);
ORGUNPACK orgUnpack = NULL;

BYTE g_rfCode[6];
bool bReadFileHooked = false;
bool bAfterFileFromAFSHooked = false;
bool bAfterFileFromAFSJumpHackHooked = false;
BYTE _shortJumpHack[][2] = {
    //PES6
    {0, 0},
    //PES6 1.10
    {0xeb, 0x4a},
    //WE2007
    {0x0, 0x0},
};

CRITICAL_SECTION _ar_cs;
typedef void (*afsreplace_callback_t)(GETFILEINFO* gfi);
vector<afsreplace_callback_t> _afsreplace_vec;
typedef void (*unpackgetsize_callback_t)(GETFILEINFO* gfi, DWORD part, DWORD* size);
vector<unpackgetsize_callback_t> _unpackgetsize_vec;
typedef void (*unpack_callback_t)(GETFILEINFO* gfi, DWORD part, DWORD decBuf, DWORD size);
vector<unpack_callback_t> _unpack_vec;
typedef void (*unidecode_callback_t)(DWORD decBuf);
vector<unidecode_callback_t> _unidecode_vec;

// This is possible since the functions which need it are subfunctions
// of NewFileFromAFS() and are called before AfterFileFromAFS()
GETFILEINFO* lastGetFileInfo = NULL;
DWORD nextUniqueId = 0;
std::hash_map<DWORD,GETFILEINFO*> g_Files;
std::hash_map<DWORD,GETFILEINFO*>::iterator g_FilesIterator;

std::hash_map<DWORD,GETFILEINFO*> g_addressMap;

#define HASREPLACEBUFFER (lastGetFileInfo->replaceBuf != NULL && lastGetFileInfo->replaceSize > 0)
#define NEEDSUNPACK (lastGetFileInfo->needsUnpack)
#define MAP_FIND(map,key) map[key]
#define MAP_CONTAINS(map,key) (map.find(key)!=map.end())

//--------------------------------------------
//----------- END OF DEFINITIONS ------------
//--------------------------------------------


KEXPORT void InitAfsReplace()
{
	InitializeCriticalSection(&_ar_cs);
    _afsreplace_vec.clear();
    _unpackgetsize_vec.clear();
    _unpack_vec.clear();
    _unidecode_vec.clear();
	
    // Determine the game version
    int v = GetPESInfo()->GameVersion;
    if (v != -1)
    {
        memcpy(code, codeArray[v], sizeof(code));
        memcpy(data, dataArray[v], sizeof(data));
        
        orgUnpack = (ORGUNPACK)code[C_UNPACK];

        RegisterGetNumPagesCallback(afsGetNumPages);
    }
    
    return;
}

KEXPORT void HookAfsReplace()
{
	MasterHookFunction(code[C_NEWFILEFROMAFS_CS], 0, afsNewFileFromAFS);
	MasterHookFunction(code[C_ALLOCATEBUFFERS_CS], 5, afsAllocateBuffers);
	HookReadFile();
	MasterHookFunction(code[C_ALLOCMEM_CS], 3, afsAllocMem);
	MasterHookFunction(code[C_UNPACK_CS], 5, afsUnpack);
	MasterHookFunction(code[C_UNIDECODE_CS], 1, afsUniDecode);
	
	// hook AfterFileFromAFS
	if (code[C_AFTERFILEFROMAFS_CS] != 0)
	{
		BYTE* bptr = (BYTE*)(code[C_AFTERFILEFROMAFS_CS]);
		DWORD* ptr = (DWORD*)(code[C_AFTERFILEFROMAFS_CS] + 1);
		DWORD newProtection = PAGE_EXECUTE_READWRITE, protection;
	
	    if (VirtualProtect(bptr, 6, newProtection, &protection)) {
	    	bptr[0]=0xe8; //call
	    	bptr[5]=0xc3; //ret
            ptr[0] = (DWORD)afsAfterFileFromAFS - (DWORD)(code[C_AFTERFILEFROMAFS_CS] + 5);
	        bAfterFileFromAFSHooked = true;
	        Log(&k_kload,"AfterFileFromAFS HOOKED at code[C_AFTERFILEFROMAFS_CS]");
	    };

        // install short jump hack, if needed
        // (we need this when the correct location doesn't have enough
        // space to fit a hook instruction, so we need to jump to a different
        // place instead)
        if (code[C_FILEFROMAFS_JUMPHACK] != 0) {
            bptr = (BYTE*)(code[C_FILEFROMAFS_JUMPHACK]);
            if (VirtualProtect(bptr, 2, newProtection, &protection)) {
                memcpy(bptr, _shortJumpHack[GetPESInfo()->GameVersion], 2);
                bAfterFileFromAFSJumpHackHooked = true;
                Log(&k_kload,"FileFromAFS Short-Jump-Hack installed.");
            }
        }
	}
	
	return;
}

KEXPORT void UnhookAfsReplace()
{
	MasterUnhookFunction(code[C_NEWFILEFROMAFS_CS], afsNewFileFromAFS);
	MasterUnhookFunction(code[C_ALLOCATEBUFFERS_CS], afsAllocateBuffers);
	
	// free all file buffers
	for (g_FilesIterator = g_Files.begin(); g_FilesIterator != g_Files.end();
				g_FilesIterator++) {
		try {
			LogWithNumber(&k_kload, "Buffer with %d bytes was still allocated!", 
										(g_FilesIterator->second)->replaceSize);
			makeReplaceBuffer(0, g_FilesIterator->second);
			HeapFree(GetProcessHeap(),0,(void*)g_FilesIterator->second);
		} catch (...) {}
	}
	g_Files.clear();
	
	DeleteCriticalSection(&_ar_cs);
	return;
}

void HookReadFile()
{
	// hook code[C_READFILE]
	if (code[C_READFILE_CS] != 0)
	{
	    BYTE* bptr = (BYTE*)code[C_READFILE_CS];
	    // save original code for CALL ReadFile
	    memcpy(g_rfCode, bptr, 6);
	
	    DWORD protection = 0;
	    DWORD newProtection = PAGE_EXECUTE_READWRITE;
	    if (VirtualProtect(bptr, 8, newProtection, &protection)) {
	        bptr[0] = 0xe8; bptr[5] = 0x90; // NOP
	        DWORD* ptr = (DWORD*)(code[C_READFILE_CS] + 1);
	        ptr[0] = (DWORD)afsReadFile - (DWORD)(code[C_READFILE_CS] + 5);
	        bReadFileHooked = true;
	        Log(&k_kload,"ReadFile HOOKED at code[C_READFILE_CS]");
	    }
	}
	return;
}

void UnhookReadFile()
{
	// unhook ReadFile
	if (bReadFileHooked)
	{
		BYTE* bptr = (BYTE*)code[C_READFILE_CS];
		memcpy(bptr, g_rfCode, 6);
		Log(&k_kload,"ReadFile UNHOOKED");
	}
	return;
}

KEXPORT void RegisterAfsReplaceCallback(void* callback, void* unpack_callback, void* unpackgetsize_callback)
{
    EnterCriticalSection(&_ar_cs);
    _afsreplace_vec.push_back((afsreplace_callback_t)callback);
    LogWithNumber(&k_kload, "RegisterAfsReplaceCallback(%08x)", (DWORD)callback);
    if (unpack_callback) {
		_unpack_vec.push_back((unpack_callback_t)unpack_callback);
	    LogWithNumber(&k_kload, "RegisterUnpackCallback(%08x)", (DWORD)unpack_callback);
	    if (unpackgetsize_callback) {
			_unpackgetsize_vec.push_back((unpackgetsize_callback_t)unpackgetsize_callback);
	    	LogWithNumber(&k_kload, "RegisterUnpackGetSizeCallback(%08x)", (DWORD)unpackgetsize_callback);
	    }
	}
    LeaveCriticalSection(&_ar_cs);
}

KEXPORT void RegisterUniDecodeCallback(void* callback)
{
    EnterCriticalSection(&_ar_cs);
    _unidecode_vec.push_back((unidecode_callback_t)callback);
    LogWithNumber(&k_kload, "RegisterUniDecodeCallback(%08x)", (DWORD)callback);
    LeaveCriticalSection(&_ar_cs);
}

// From here, there is one call per loaded file. The values
// inside the structure can be changed, so that various scenarios
// are possible:
// - replacing a file,
// - changing the file which is loaded to another one from the AFS,
// - both (like for fserv, where the fileId is not valid but a special id)
// - mark a file to be observed until it is unpacked
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
	lastGetFileInfo->needsUnpack = false;
	
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
	if (lastGetFileInfo->fileId < 0x10000) {
		MessageBox(0,"","",0);
		LogWithNumber(&k_kload,"#sound: %d",lastGetFileInfo->fileId);
	}
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
	if (!HASREPLACEBUFFER && !NEEDSUNPACK) return false;
	
	DWORD bufHeader = 0;
	
	try {
		bufHeader = *( *( *(p3 + 1) + 2) + 1);
		lastGetFileInfo->firstPage = *((DWORD*)p3 + 12);
	 } catch (...) {
	 	// we can't replace the data later without this information, so
	 	// free the buffer
	 	if (HASREPLACEBUFFER) {
	 		makeReplaceBuffer(0);
	 	}
	 	Log(&k_kload, "Couldn't find address for buffer header.");
	 	return res;
	}
	
	// if something already exists here, free it
	// this also means that this file wasn't replaced
	if (MAP_CONTAINS(g_Files, bufHeader)) {
		try {
			GETFILEINFO* oldBufHeader = g_Files[bufHeader];
			if (oldBufHeader->replaceBuf) {
				LogWithNumber(&k_kload, "Buffer with %d bytes freed.", oldBufHeader->replaceSize);
				makeReplaceBuffer(0, oldBufHeader);
			}
			HeapFree(GetProcessHeap(),0,(void*)oldBufHeader);
		} catch (...) {}
		g_Files.erase(bufHeader);
	}
	// in ReadFile, we can find the GETFILEINFO for the loaded file with that
	g_Files[bufHeader] = lastGetFileInfo;
	
	return res;
}

// By now, the calls to NewFileFromAFS(), GetNumPages() and AllocateBuffers() are
// finished
DWORD afsAfterFileFromAFS(DWORD retAddr, DWORD infoBlock)
{
	DWORD res;
	__asm mov res, eax
	if (!lastGetFileInfo) return res;
	
	// For replaced files, the memory isn't freed yet since
	// we need the structure later in ReadFile()
	if (!HASREPLACEBUFFER && !NEEDSUNPACK) {
		HeapFree(GetProcessHeap(),0,(void*)lastGetFileInfo);
	}
	lastGetFileInfo = NULL;
	
	return res;
}



// the calls to ReadFile can happen independently from the functions above

/**
 * Monitors the file pointer.
 */
BOOL STDMETHODCALLTYPE afsReadFile(HANDLE hFile, LPVOID lpBuffer, DWORD nNumberOfBytesToRead,
  LPDWORD lpNumberOfBytesRead,  LPOVERLAPPED lpOverlapped)
{
	DWORD lastBufHeader = 0;
	
	// important to connect this call to the corresponding GETFILEINFO structure
	__asm mov lastBufHeader, esi
    //LogWithNumber(&k_kload, "afsReadFile: lastBufHeader = %08x", lastBufHeader);

	// call original function	
	BOOL result=ReadFile(hFile, lpBuffer, nNumberOfBytesToRead, lpNumberOfBytesRead, lpOverlapped);

	// check if we have some data to replace this file
	if (MAP_CONTAINS(g_Files, lastBufHeader)) {
		GETFILEINFO* gfi = g_Files[lastBufHeader];
		
		// find out which page we are reading
		DWORD currPage = *(DWORD*)(lastBufHeader + 0x1c);
		
		if (gfi->replaceBuf != NULL && gfi->replaceSize > 0) {
			// replace with contents of the buffer
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
				if (!gfi->needsUnpack) {
					HeapFree(GetProcessHeap(),0,(void*)gfi);
				}
				g_Files.erase(lastBufHeader);
			}
		}
		
		if (gfi->needsUnpack && (currPage == gfi->firstPage)) {
			//	remember the buffer address for unpacking (where it is the src)
			g_addressMap[(DWORD)lpBuffer] = gfi;
			
			// no longer need to watch for this file in ReadFile
			if (gfi->replaceBuf == NULL || gfi->replaceSize == 0) {
				g_Files.erase(lastBufHeader);
			}
		}
	}
	
	// reset
	lastBufHeader = 0;

	return result;
}



// various unpacking routines
GETFILEINFO* last_unpackGfi = NULL;
DWORD last_unpackPart = 0;
DWORD last_unpackSize = 0;

// first unpacking call to modules: changing size of decoded buffer
void doUnpackGetSizeCallbacks(DWORD srcAddress, DWORD part, DWORD* size)
{
	last_unpackGfi = NULL;
	last_unpackPart = 0;
	last_unpackSize = 0;

	// look if our source buffer was filled by ReadFile
	if (!MAP_CONTAINS(g_addressMap, srcAddress)) return;
	last_unpackGfi = g_addressMap[srcAddress];
    g_addressMap.erase(srcAddress);
    last_unpackPart = part;
    last_unpackGfi->isProcessed = false;

	for (vector<unpackgetsize_callback_t>::iterator it = _unpackgetsize_vec.begin(); it != _unpackgetsize_vec.end(); it++) {
		(*it)(last_unpackGfi, part, size);
	}
	
	last_unpackSize = *size;
	return;
}

// second call: decoding is done, now the data can be replaced
// this calls should be done right after the other, so the replacing data
// can already be prepared during the first call
void doUnpackCallbacks(DWORD decBuf)
{
	if (!last_unpackGfi) return; // shouldn't happen anyway
	
	for (vector<unpack_callback_t>::iterator it = _unpack_vec.begin(); it != _unpack_vec.end(); it++) {
		(*it)(last_unpackGfi, last_unpackPart, decBuf, last_unpackSize);
	}
	
	// free memory	
	HeapFree(GetProcessHeap(),0,(void*)last_unpackGfi);
	last_unpackGfi = NULL;
	last_unpackPart = 0;
	last_unpackSize = 0;
    return;
}

/**
 * This function is seemingly responsible for allocating memory
 * for decoded buffer (called before Unpack)
 * Parameters:
 *   infoBlock  - address of some information block
 *                What is of interest of in that block: infoBlock[60]
 *                contains an address of encoded (src) BIN.
 *   param2     - unknown param. Possibly count of buffers to allocate
 *   size       - size in bytes of the block needed.
 * Returns:
 *   address of newly allocated buffer. Also, this address is stored
 *   at infoBlock[64] location, which is IMPORTANT.
 */
DWORD afsAllocMem(DWORD infoBlock, DWORD param2, DWORD size)
{
	DWORD newSize = size;
	DWORD srcAddr = ((INFOBLOCK*)infoBlock)->src;
	
	doUnpackGetSizeCallbacks(srcAddr, 1, &newSize); // always first part
	if (newSize != size) {
		newSize = max(newSize, size);
		
		((ENCBUFFERHEADER*)srcAddr)->dwDecSize = newSize;
	}
	
	DWORD result = MasterCallNext(infoBlock, param2, newSize);
	return result;
};

/**
 * This function calls the unpack function for non-kits (single file).
 * Parameters:
 *   addr1   - address of the encoded buffer (without header)
 *   addr2   - address of the decoded buffer
 *   size1   - size of the encoded buffer (minus header)
 *   zero    - always zero
 *   size2   - pointer to size of the decoded buffer
 */
DWORD afsUnpack(DWORD addr1, DWORD addr2, DWORD size1, DWORD zero, DWORD* size2)
{
	DWORD result = MasterCallNext(addr1, addr2, size1, zero, size2);
	
	doUnpackCallbacks(addr2);

	return result;
}

/*
// unpacking function which is called for files with more than one subfile
// later!
DWORD afsMultipleUnpack(...)
{
	DWORD result = MasterCallNext();

	doUnpackCallbacks(dest);

	return result;
}
*/

/**
 * This function calls kit BIN decode function
 * Parameters:
 *   addr   - address of the encoded buffer header
 *   size   - size of the encoded buffer - DOESN'T EXIST
 * Return value:
 *   address of the decoded buffer
 */
DWORD afsUniDecode(DWORD addr)
{
	// the size is saved inside the encoded buffer header
	ENCBUFFERHEADER* ebh = (ENCBUFFERHEADER*)addr;
	DWORD size = ebh->dwDecSize;
	DWORD newSize = size;
	
	// part is 0 for UniDecode if we really need to make a difference
	// to Unpack. besides, there is only one subfile anyway
	doUnpackGetSizeCallbacks(addr, 0, &newSize);
	if (newSize != size) {
		newSize = max(newSize, size);
		ebh->dwDecSize = newSize;
	}
	
	// call the hooked function
	DWORD result = MasterCallNext(addr);
	
	for (vector<unidecode_callback_t>::iterator it = _unidecode_vec.begin(); it != _unidecode_vec.end(); it++) {
		(*it)(result);
	}
	
	doUnpackCallbacks(result);
	
	return result;
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
	if (!filename) return false;
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

KEXPORT DWORD MemUnpack(DWORD addr1, DWORD addr2, DWORD size1, DWORD* size2)
{
	return orgUnpack(addr1, addr2, size1, 0, size2);
}

KEXPORT DWORD AFSMemUnpack(DWORD FileID, DWORD Buffer)
{
	ENCBUFFERHEADER *e;
	char tmp[BUFLEN];
	DWORD FileInfo[2];
	DWORD NBW=0;
	
	strcpy(tmp,g_pesinfo.pesdir);
	strcat(tmp,g_pesinfo.AFS_0_text);
	
	HANDLE file=CreateFile(tmp,GENERIC_READ,3,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,0);
	if (file==INVALID_HANDLE_VALUE) return 0;
	SetFilePointer(file,8*(FileID+1),0,0);
	ReadFile(file,&(FileInfo[0]),8,&NBW,0);
	
	LPVOID srcbuf=HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,FileInfo[1]);
	SetFilePointer(file,FileInfo[0],0,0);
	ReadFile(file,srcbuf,FileInfo[1],&NBW,0);
	CloseHandle(file);
	e=(ENCBUFFERHEADER*)srcbuf;

	DWORD result=orgUnpack((DWORD)srcbuf+0x20, Buffer, e->dwEncSize, 0, &(e->dwDecSize));
	
	HeapFree(GetProcessHeap(),0,srcbuf);
	
	return result;
}
