// afsreplace.h

typedef struct _GETFILEINFO {
	DWORD uniqueId;
	bool isProcessed;
	DWORD fileId;
	DWORD oldFileId;
	void* replaceBuf;
	DWORD replaceSize;
	DWORD firstPage;
	// unpacking
	bool needsUnpack;
} GETFILEINFO;

#ifndef _INFOBLOCK_
#define _INFOBLOCK_
typedef struct _INFOBLOCK {
	BYTE reserved1[0x54];
	DWORD FileID; //0x54
	BYTE reserved2[8];
	DWORD src; //0x60
	DWORD dest; //0x64
} INFOBLOCK;
#endif

#ifndef _ENCBUFFERHEADER_
#define _ENCBUFFERHEADER_
typedef struct _ENCBUFFERHEADER {
	DWORD dwSig;
	DWORD dwEncSize;
	DWORD dwDecSize;
	BYTE other[20];
} ENCBUFFERHEADER;
#endif



//-------------- FUNCTIONS ---------------
void HookReadFile();
void UnhookReadFile();

DWORD afsNewFileFromAFS();
bool afsGetNumPages(DWORD fileId, DWORD afsId, DWORD* retval);
DWORD afsAllocateBuffers(DWORD p1, DWORD p2, DWORD*** p3, DWORD p4, DWORD p5);
DWORD afsAfterFileFromAFS();

BOOL STDMETHODCALLTYPE afsReadFile(HANDLE hFile, LPVOID lpBuffer, DWORD nNumberOfBytesToRead,
  LPDWORD lpNumberOfBytesRead,  LPOVERLAPPED lpOverlapped);
  
void doUnpackGetSizeCallbacks(DWORD srcAddress, DWORD part, DWORD* size);
void doUnpackCallbacks(DWORD decBuf);
DWORD afsAllocMem(DWORD infoBlock, DWORD param2, DWORD size);
DWORD afsUnpack(DWORD addr1, DWORD addr2, DWORD size1, DWORD zero, DWORD* size2);
DWORD afsUniDecode(DWORD addr);

// EXPORTED:
KEXPORT void InitAfsReplace();
KEXPORT void HookAfsReplace();
KEXPORT void UnhookAfsReplace();
KEXPORT void RegisterAfsReplaceCallback(void* callback, void* unpack_callback=NULL, void* unpackgetsize_callback=NULL);
KEXPORT void RegisterUniDecodeCallback(void* callback);

KEXPORT DWORD splitFileId(DWORD fileId, DWORD* afsId=NULL);
KEXPORT DWORD getAfsFileSize(DWORD fileId, DWORD afsId=1);
KEXPORT void* makeReplaceBuffer(DWORD size, GETFILEINFO* gfi1=(GETFILEINFO*)0xffffffff);
KEXPORT bool loadReplaceFile(char* filename);

KEXPORT DWORD MemUnpack(DWORD addr1, DWORD addr2, DWORD size1, DWORD* size2);
KEXPORT DWORD AFSMemUnpack(DWORD FileID, DWORD Buffer);
