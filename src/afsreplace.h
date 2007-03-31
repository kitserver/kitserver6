// afsreplace.h

typedef struct _INFOBLOCK {
	BYTE reserved1[0x54];
	DWORD FileID; //0x54
	BYTE reserved2[8];
	DWORD src; //0x60
	DWORD dest; //0x64
} INFOBLOCK;

typedef struct _GETFILEINFO {
	DWORD uniqueId;
	bool isProcessed;
	DWORD fileId;
	DWORD oldFileId;
	void* replaceBuf;
	DWORD replaceSize;
	DWORD firstPage;
} GETFILEINFO;


//-------------- FUNCTIONS ---------------
DWORD afsNewFileFromAFS();
bool afsGetNumPages(DWORD fileId, DWORD afsId, DWORD* retval);
DWORD afsAllocateBuffers(DWORD p1, DWORD p2, DWORD*** p3, DWORD p4, DWORD p5);
DWORD afsAfterFileFromAFS();

void afsBeforeReadFile(HANDLE hFile, LPVOID lpBuffer, DWORD nNumberOfBytesToRead,
  LPDWORD lpNumberOfBytesRead,  LPOVERLAPPED lpOverlapped);
void afsAfterReadFile(HANDLE hFile, LPVOID lpBuffer, DWORD nNumberOfBytesToRead,
  LPDWORD lpNumberOfBytesRead,  LPOVERLAPPED lpOverlapped);

// EXPORTED:
KEXPORT void InitAfsReplace();
KEXPORT void HookAfsReplace();
KEXPORT void UnhookAfsReplace();
KEXPORT void RegisterAfsReplaceCallback(void* callback);

KEXPORT DWORD splitFileId(DWORD fileId, DWORD* afsId=NULL);
KEXPORT DWORD getAfsFileSize(DWORD fileId, DWORD afsId=1);
KEXPORT void* makeReplaceBuffer(DWORD size, GETFILEINFO* gfi1=(GETFILEINFO*)0xffffffff);
KEXPORT bool loadReplaceFile(char* filename);
