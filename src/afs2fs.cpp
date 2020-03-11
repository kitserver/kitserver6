/* AFS2FS module */
//#define UNICODE

#include <windows.h>
#include <stdio.h>
#include <sys/stat.h>
#include "kload_exp.h"
#include "afsreader.h"

#include "afs2fs.h"
#include "afs2fs_addr.h"
#include "utf8.h"

#include <map>
#include <list>
#include <hash_map>
#include <wchar.h>

#define SWAPBYTES(dw) \
    ((dw<<24 & 0xff000000) | (dw<<8  & 0x00ff0000) | \
    (dw>>8  & 0x0000ff00) | (dw>>24 & 0x000000ff))


// VARIABLES
HINSTANCE hInst = NULL;
KMOD k_afs = {MODID, NAMELONG, NAMESHORT, DEFAULT_DEBUG};

// cache
#define DEFAULT_FILENAMELEN 64
#define MAX_ITEMS 16000
#define MAX_FOLDERS 10

#define BUFLEN 1024

typedef struct _INFO_CACHE_ENTRY_STRUCT
{
    const char* rootDir;
    char fileName[1];
} INFO_CACHE_ENTRY_STRUCT;

typedef struct _FAST_INFO_CACHE_STRUCT
{
    bool initialized; 
    int numEntries;
    size_t entrySize;
    BYTE* entries;
} FAST_INFO_CACHE_STRUCT;

class config_t 
{
public:
    config_t() : _fileNameLen(DEFAULT_FILENAMELEN) {}
    list<string> _roots;
    int _fileNameLen;
};

hash_map<string,int> g_maxItems;
hash_map<string,BYTE*> _info_cache;
FAST_INFO_CACHE_STRUCT _fast_info_cache[MAX_FOLDERS];

#define MAX_IMGDIR_LEN 4096
config_t _config;

// FUNCTIONS
void initModule(IDirect3D8* self, UINT Adapter,
    D3DDEVTYPE DeviceType, HWND hFocusWindow, DWORD BehaviorFlags,
    D3DPRESENT_PARAMETERS *pPresentationParameters, 
    IDirect3DDevice8** ppReturnedDeviceInterface);

void afsAfsReplace(GETFILEINFO* gfi);
bool ReadConfig(config_t& config, const char* cfgFile);

// FUNCTION POINTERS

bool GetBinFileName(DWORD afsId, DWORD binId, char* filename, int maxLen)
{
    if (afsId < 0 || MAX_FOLDERS-1 < afsId) return false; // safety check
    if (!_fast_info_cache[afsId].initialized)
    {
        BIN_SIZE_INFO* pBST = ((BIN_SIZE_INFO**)data[BIN_SIZES_TABLE])[afsId];
        if (pBST) 
        {
            hash_map<string,BYTE*>::iterator it;
            for (it = _info_cache.begin(); it != _info_cache.end(); it++)
            {
                if (stricmp(it->first.c_str(), pBST->relativePathName)==0)
                    _fast_info_cache[afsId].entries = it->second;
                    _fast_info_cache[afsId].numEntries = pBST->numItems;
            }
        }
        if (k_afs.debug && pBST)
            LogWithNumberAndString(&k_afs, "initialized _fast_info_cache entry for afsId=%d (%s)", afsId, pBST->relativePathName);
        _fast_info_cache[afsId].entrySize = sizeof(char*) + sizeof(char)*_config._fileNameLen;
        _fast_info_cache[afsId].initialized = true;
    }

    BYTE* base = _fast_info_cache[afsId].entries;
    if (binId >= _fast_info_cache[afsId].numEntries || !base)
        return false;
    INFO_CACHE_ENTRY_STRUCT* entry = (INFO_CACHE_ENTRY_STRUCT*)(_fast_info_cache[afsId].entries + binId*(sizeof(char*) + sizeof(char)*_config._fileNameLen));
    if (entry->fileName[0]=='\0')
        return false;

    BIN_SIZE_INFO* pBST = ((BIN_SIZE_INFO**)data[BIN_SIZES_TABLE])[afsId];
    char *afsDir = pBST->relativePathName;
    _snprintf(filename, maxLen, "%s\\%s\\%s", 
            entry->rootDir, afsDir, entry->fileName);
    return true;
}

int GetNumItems(string& folder)
{
    int result = MAX_ITEMS;
    hash_map<string,int>::iterator it = g_maxItems.find(folder);
    if (it == g_maxItems.end())
    {
        // get number of files inside the corresponding AFS file
        string afsFile(GetPESInfo()->pesdir);
        afsFile += "dat\\";
        FILE* f = fopen((afsFile + folder).c_str(),"rb");
        if (f) {
            AFSDIRHEADER afsDirHdr;
            ZeroMemory(&afsDirHdr,sizeof(AFSDIRHEADER));
            fread(&afsDirHdr,sizeof(AFSDIRHEADER),1,f);
            if (afsDirHdr.dwSig == AFSSIG)
            {
                g_maxItems.insert(pair<string,int>(folder, afsDirHdr.dwNumFiles));
                result = afsDirHdr.dwNumFiles;
            }
            fclose(f);
        }
        else
        {
            // can't open for reading, then just reserve a big enough cache
            g_maxItems.insert(pair<string,int>(folder, MAX_ITEMS));
        }
    }
    else
        result = it->second;

    return result;
}

void InitializeFileNameCache()
{
    Log(&k_afs, "Initializing filename cache...");
    for (list<string>::iterator lit = _config._roots.begin();
            lit != _config._roots.end();
            lit++)
    {
        WIN32_FIND_DATA fData;
        string pattern(lit->c_str());
        pattern += "\\*.afs";
        LogWithString(&k_afs,"processing {%s}",(char*)pattern.c_str());

        HANDLE hff = FindFirstFile(pattern.c_str(), &fData);
        if (hff == INVALID_HANDLE_VALUE) 
        {
            // none found.
            continue;
        }
        while(true)
        {
            // check if this is a directory
            if (fData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            {
                if (strcmp(fData.cFileName,".")==0 ||
                    strcmp(fData.cFileName,"..")==0)
                    continue;

                WIN32_FIND_DATA fData1;
                string folder(fData.cFileName);
                string folderpattern(*lit);
                folderpattern += "\\";
                folderpattern += folder + "\\*";

                string key(folder.c_str());
                LogWithString(&k_afs,"folderpattern = {%s}", (char*)folderpattern.c_str());

                HANDLE hff1 = FindFirstFile(folderpattern.c_str(), &fData1);
                if (hff1 != INVALID_HANDLE_VALUE) 
                {
                    while(true)
                    {
                        // check if this is a file
                        if (!(fData1.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
                            int binId = -1;
                            char* s = strchr(fData1.cFileName,'_');
                            if (s && sscanf(s+1,"%d",&binId)==1)
                            {
                                LogWithStringAndNumber(&k_afs, "folder={%s}, bin={%d}",(char*)folder.c_str(),binId);
                                if (binId >= 0)
                                {
                                    BYTE* entries = NULL;
                                    hash_map<string,BYTE*>::iterator cit = _info_cache.find(key);
                                    if (cit != _info_cache.end()) entries = cit->second;
                                    else 
                                    {
                                        entries = (BYTE*)HeapAlloc(
                                                GetProcessHeap(),
                                                HEAP_ZERO_MEMORY, 
                                                (sizeof(char*) + sizeof(char)*_config._fileNameLen)*GetNumItems(folder)
                                                );
                                        _info_cache.insert(pair<string,BYTE*>(key, entries));
                                    }

                                    if (binId >= GetNumItems(folder))
                                    {
                                        // binID too large
                                        LogWithStringAndNumber(&k_afs, "ERROR: bin ID for filename \"%s\" is too large. Maximum bin ID for this folder is: %d", (char*)fData1.cFileName, GetNumItems(folder)-1);
                                    }
                                    else if (strlen(fData1.cFileName) >= _config._fileNameLen)
                                    {
                                        // file name too long
                                        LogWithTwoStrings(&k_afs, "ERROR: filename too long: \"%s\" (in folder: %s)", 
                                                (char*)fData1.cFileName, (char*)folder.c_str());
                                        LogWithTwoNumbers(&k_afs, "ERROR: length = %d chars. Maximum allowed length: %d chars.", 
                                                strlen(fData1.cFileName), _config._fileNameLen-1);
                                    }
                                    else
                                    {
                                        INFO_CACHE_ENTRY_STRUCT* entry = (INFO_CACHE_ENTRY_STRUCT*)(entries 
                                                + binId*(sizeof(char*) + sizeof(char)*_config._fileNameLen));
                                        // put filename into cache
                                        strncpy(entry->fileName, fData1.cFileName, _config._fileNameLen-1);
                                        // put foldername into cache
                                        entry->rootDir = lit->c_str();
                                    }
                                }
                            }
                        }

                        // proceed to next file
                        if (!FindNextFile(hff1, &fData1)) break;
                    }
                    FindClose(hff1);
                }
            }

            // proceed to next file
            if (!FindNextFile(hff, &fData)) break;
        }
        FindClose(hff);

    } // for roots

    // print cache
    for (hash_map<string,int>::iterator it = g_maxItems.begin(); it != g_maxItems.end(); it++)
        LogWithStringAndNumber(&k_afs, "filename cache: {%s} : %d slots", (char*)it->first.c_str(), it->second);

    Log(&k_afs, "DONE initializing filename cache.");
}

/*******************/
/* DLL Entry Point */
/*******************/
EXTERN_C BOOL WINAPI DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID lpReserved)
{
	if (dwReason == DLL_PROCESS_ATTACH)
	{
		Log(&k_afs,"Attaching dll...");
		hInst=hInstance;

        //check game version
		switch (GetPESInfo()->GameVersion) {
            case gvPES6PC: //support for PES6 PC
            case gvPES6PC110: //support for PES6 PC 1.10
            case gvWE2007PC: //support for WE:PES 2007 PC
				break;
            default:
                Log(&k_afs,"Your game version is currently not supported!");
                return false;
		}

		RegisterKModule(&k_afs);
		HookFunction(hk_D3D_Create,(DWORD)initModule);
	}
	
	else if (dwReason == DLL_PROCESS_DETACH)
	{
        Log(&k_afs, "Unloading Afs2fs");
        for (hash_map<string,BYTE*>::iterator it = _info_cache.begin(); 
                it != _info_cache.end();
                it++)
            if (it->second) HeapFree(GetProcessHeap(), 0, it->second);
	}
	
	return true;
}

/**
 * Returns true if successful.
 */
bool ReadConfig(config_t& config, const char* cfgFile)
{
	FILE* cfg = fopen(cfgFile, "rt");
	if (cfg == NULL) return false;

	char str[BUFLEN];
	char name[BUFLEN];
	int value = 0;

	char *pName = NULL, *pValue = NULL, *comment = NULL;
	while (!feof(cfg))
	{
		ZeroMemory(str, BUFLEN);
		fgets(str, BUFLEN-1, cfg);

		// skip comments
		comment = strstr(str, "#");
		if (comment != NULL) comment[0] = '\0';

		// parse the line
		pName = pValue = NULL;
		ZeroMemory(name, BUFLEN); value = 0;
		char* eq = strstr(str, "=");
		if (eq == NULL || eq[1] == '\0') continue;

		eq[0] = '\0';
		pName = str; pValue = eq + 1;

		ZeroMemory(name, NULL); 
		sscanf(pName, "%s", name);

		if (strcmp(name, "afs.root")==0)
		{
            // strip quotes and EOLN
            char* eoln = strchr(pValue,'\n');
            if (eoln!=NULL) eoln[0]='\0';

            char* firstQuote = strchr(pValue,'"');
            if (firstQuote!=NULL && strlen(firstQuote)>1)
            {
                pValue = firstQuote+1;
                char* lastQuote = strchr(pValue,'"');
                if (lastQuote!=NULL)
                    lastQuote[0]='\0';
            }
            
			LogWithString(&k_afs,"ReadConfig: afs.root = (%s)", pValue);
            if (strlen(pValue)>1 && pValue[1]==':')
                config._roots.push_back(pValue);
            else
            {
                // expand relative path
                string root(GetPESInfo()->mydir);
                root += pValue;
                config._roots.push_back(root);
            }
		}
        else if (strcmp(name, "debug")==0)
        {
			if (sscanf(pValue, "%d", &value)!=1) continue;
			LogWithNumber(&k_afs,"ReadConfig: debug = (%d)", value);
            k_afs.debug = value;
        }
        else if (strcmp(name, "filename.length")==0)
        {
			if (sscanf(pValue, "%d", &value)!=1) continue;
			LogWithNumber(&k_afs,"ReadConfig: filename.length = (%d)", value);
            config._fileNameLen = max(value, 4);
        }
	}
	fclose(cfg);
	return true;
}

void initModule(IDirect3D8* self, UINT Adapter,
    D3DDEVTYPE DeviceType, HWND hFocusWindow, DWORD BehaviorFlags,
    D3DPRESENT_PARAMETERS *pPresentationParameters, 
    IDirect3DDevice8** ppReturnedDeviceInterface) 
{
    Log(&k_afs, "initializing AFS2FS module...");

    // default roots
    string defaultDatDir(GetPESInfo()->mydir);
    defaultDatDir += "dat";
    _config._roots.push_back(defaultDatDir);

    string configFile(GetPESInfo()->mydir);
    configFile += "afs2fs.cfg";
    ReadConfig(_config, configFile.c_str());

    memcpy(code, codeArray[GetPESInfo()->GameVersion], sizeof(code));
    memcpy(data, dataArray[GetPESInfo()->GameVersion], sizeof(data));

    // register callback
    RegisterAfsReplaceCallback(afsAfsReplace);
    Log(&k_afs, "afsAfsReplace hooked");

	UnhookFunction(hk_D3D_Create,(DWORD)initModule);

    InitializeFileNameCache();
    ZeroMemory(_fast_info_cache,sizeof(_fast_info_cache));

	TRACE(&k_afs, "Hooking done.");

    //__asm { int 3 }          // uncomment this for debugging as needed
}

/**
 * afsReplace callback
 */
void afsAfsReplace(GETFILEINFO* gfi)
{
	if (gfi->isProcessed) 
        return;
	
	DWORD afsId = 0, fileId = 0;
	fileId = splitFileId(gfi->fileId, &afsId);
	
    if (k_afs.debug > 1)
        LogWithTwoNumbers(&k_afs,"afsAfsReplace: afsId=%d, fileId=%d", afsId, fileId);

    char filename[1024];
    if (!GetBinFileName(afsId, fileId, filename, 1024))
        return;

    // replace file
    if (strlen(filename)>0) {
        if (!loadReplaceFile(filename))
        {
            LogWithTwoNumbers(&k_afs, "afsAfsReplace: replace FAILED for afsId=%d, fileId=%d", afsId, fileId);
        }
        gfi->isProcessed = true;
    }
}

