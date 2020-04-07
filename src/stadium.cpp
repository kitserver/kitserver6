// stadium.cpp
#include <windows.h>
#include <stdio.h>
#include "stadium.h"
#include "kload_exp.h"
#include "hooklib.h"
#include "numpages.h"
#include "input.h"

#include <map>
#include <string>

#define MAXFILENAME 4096
#define MAXLINELEN 2048
#define HOME 0
#define AWAY 1
#define CLUB_HOUSE 0

KMOD k_stadium={MODID,NAMELONG,NAMESHORT,DEFAULT_DEBUG};

HINSTANCE hInst;
HHOOK g_hKeyboardHook=NULL;
STAD_CFG _stad_cfg;

///// Graphics //////////////////

struct CUSTOMVERTEX { 
	FLOAT x,y,z,w;
	DWORD color;
};

struct CUSTOMVERTEX2 { 
	FLOAT x,y,z,w;
	DWORD color;
	FLOAT tu, tv;
};


CUSTOMVERTEX2 g_preview[] = {
	{0.0f, 0.0f, 0.0f, 1.0f, 0xff4488ff, 0.0f, 0.0f}, //1
	{0.0f, 64.0f, 0.0f, 1.0f, 0xff4488ff, 0.0f, 1.0f}, //2
	{128.0f, 0.0f, 0.0f, 1.0f, 0xff4488ff, 1.0f, 0.0f}, //3
	{128.0f, 64.0f, 0.0f, 1.0f, 0xff4488ff, 1.0f, 1.0f}, //4
};

CUSTOMVERTEX g_preview_outline[] = {
	{0.0f, 0.0f, 0.0f, 1.0f, 0xffffffff}, //1
	{0.0f, 66.0f, 0.0f, 1.0f, 0xffffffff}, //2
	{130.0f, 0.0f, 0.0f, 1.0f, 0xffffffff}, //3
	{130.0f, 66.0f, 0.0f, 1.0f, 0xffffffff}, //4
};

CUSTOMVERTEX g_preview_outline2[] = {
	{0.0f, 0.0f, 0.0f, 1.0f, 0xff000000}, //1
	{0.0f, 68.0f, 0.0f, 1.0f, 0xff000000}, //2
	{132.0f, 0.0f, 0.0f, 1.0f, 0xff000000}, //3
	{132.0f, 68.0f, 0.0f, 1.0f, 0xff000000}, //4
};

// Stadium preview
static IDirect3DVertexBuffer8* g_pVB_preview = NULL;
static IDirect3DVertexBuffer8* g_pVB_preview_outline = NULL;
static IDirect3DVertexBuffer8* g_pVB_preview_outline2 = NULL;

static IDirect3DTexture8* g_preview_tex = NULL;
static IDirect3DDevice8* g_device = NULL;

////////////////////////////////

#define D3DFVF_CUSTOMVERTEX (D3DFVF_XYZRHW|D3DFVF_DIFFUSE)
#define D3DFVF_CUSTOMVERTEX2 (D3DFVF_XYZRHW|D3DFVF_DIFFUSE|D3DFVF_TEX1)

static bool g_needsRestore = TRUE;
static bool g_newStad = false;
static DWORD g_dwSavedStateBlock = 0L;
static DWORD g_dwDrawOverlayStateBlock = 0L;

static CRITICAL_SECTION g_cs;
static bool bOthersHooked = false;
static BYTE g_rfCode[] = {0,0,0,0,0,0};

typedef DWORD (*GETFILEFROMAFS)(DWORD,DWORD);
GETFILEFROMAFS GetFileFromAFS = NULL;

typedef DWORD (*GETSTRINGPROC)(DWORD,DWORD);
GETSTRINGPROC GetString = NULL;

typedef DWORD (*GETSTRING2PROC)(DWORD,DWORD,DWORD,DWORD,DWORD,DWORD,DWORD,DWORD);
GETSTRING2PROC GetString2 = NULL;

typedef DWORD (*WRITENUMPROC)(char* dest, char* format, DWORD num);
//WRITENUMPROC WriteBuilt = NULL;
//WRITENUMPROC WriteCapacity = NULL;

typedef DWORD (*SETLCMPROC)(DWORD);
SETLCMPROC SetLCM = NULL;

/* hook manager and hook points */
hook_point g_hp_GetFileFromAFS;
hook_point g_hp_WriteBuilt;
hook_point g_hp_WriteCapacity;


//#define MAP_FIND(map,key) ((map.count(key)>0) ? map[key] : NULL)
#define MAP_FIND(map,key) map[key]
#define MAP_CONTAINS(map,key) (map.find(key)!=map.end())

#define CODELEN 16
enum {
    C_GETSTRING, C_GETSTRING_CS,
    C_GETSTRING2, C_GETSTRING2_CS,
    C_WRITENUM, C_WRITENUM_CS1, C_WRITENUM_CS2,
    C_SETLCM_CS, C_SETLCM, C_STADIUMCHANGE, C_ADVANCESTADIUM,
    C_READLCM_CS, C_READLCM,
};
static DWORD codeArray[][CODELEN] = {
	// PES6
    {
     0x9e71a0, 0x9f26cf,
     0x78cc70, 0x7836a3,
     0x41be25, 0x9f2717, 0x9f27c8,
     0x9f33f8, 0x9f2a40, 0x9f2ec9, 0x9f2ecf,
     0x9c4061, 0x866fe0,
    },
    // PES6 1.10
    {
     0x9e7320, 0x9f282f,
     0x79cec0, 0x783873,
     0x41be75, 0x9f2877, 0x9f2928,
     0x9f3558, 0x9f2ba0, 0x9f3029, 0x9f302f,
     0x9c41f1, 0x867110,
    },
    // WE2007
    {
     0x9e79a0, 0x9f2ecf,
     0x79cc70, 0x783723,
     0x41beb5, 0x9f2f17, 0x9f2fc8,
     0x9f3bf8, 0x9f3240, 0x9f36c9, 0x9f36cf,
     0x9c4851, 0x8676f0,
    },
};

#define DATALEN 18
enum {
    NUM_FILES, NUM_STADS, STAD_FIRST, NOU_CAMP_SHIFT_ID, SHIFT, 
    ADBOARD_TEX_FIRST, NUM_ADBOARD_TEX, DELLA_ALPI_ADBOARDS,
    TEAM_IDS, SAVED_TEAM_HOME, SAVED_TEAM_AWAY, DELLA_ALPI,
    STADIUM_TEXT_TABLE, STADIUM_TEXT_LEN, RANDOM_STADIUM_FLAG,
    ISVIEWSTADIUMMODE,
    HOME_CROWD, AWAY_CROWD,
};
static DWORD dtaArray[][DATALEN] = {
	// PES6
	{66, 38, 6941, 7611, 4, 
     6915, 25, 6923,
     0x3be0940, 0x1131fd4, 0x1131fd8, 8001,
     0x1131fec, 61, 0x3be6d94,
     0x3a7ce20,
     0x3a7f2ac, 0x3a7f454,
    },
	// PES6 1.10
	{66, 38, 6941, 7611, 4, 
     6915, 25, 6923,
     0x3be1940, 0x1132fd4, 0x1132fd8, 8001,
     0x1132fec, 61, 0x3be7d94,
     0x3a7de20,
     0x3a802ac, 0x3a80454,
    },
	// WE2007
	{66, 38, 7349, 8019, 4, 
     7320, 28, 7329,
	 0x3bdb3c0, 0x112ca5c, 0x112ca60, 8409,
     0x112ca74, 61, 0x3be1814,
     0x3a778a0,
     0x3a79d2c, 0x3a79ed4,
    },
};

static char* FILE_NAMES[] = {
    "1_day_fine\\crowd_a0.str",
    "1_day_fine\\crowd_a1.str",
    "1_day_fine\\crowd_a2.str",
    "1_day_fine\\crowd_a3.str",
    "1_day_fine\\crowd_h0.str",
    "1_day_fine\\crowd_h1.str",
    "1_day_fine\\crowd_h2.str",
    "1_day_fine\\crowd_h3.str",
    "1_day_fine\\stad1_main.bin",
    "1_day_fine\\stad2_entrance.bin",
    "1_day_fine\\stad3_adboards.bin",
    "2_day_rain\\crowd_a0.str",
    "2_day_rain\\crowd_a1.str",
    "2_day_rain\\crowd_a2.str",
    "2_day_rain\\crowd_a3.str",
    "2_day_rain\\crowd_h0.str",
    "2_day_rain\\crowd_h1.str",
    "2_day_rain\\crowd_h2.str",
    "2_day_rain\\crowd_h3.str",
    "2_day_rain\\stad1_main.bin",
    "2_day_rain\\stad2_entrance.bin",
    "2_day_rain\\stad3_adboards.bin",
    "3_day_snow\\crowd_a0.str",
    "3_day_snow\\crowd_a1.str",
    "3_day_snow\\crowd_a2.str",
    "3_day_snow\\crowd_a3.str",
    "3_day_snow\\crowd_h0.str",
    "3_day_snow\\crowd_h1.str",
    "3_day_snow\\crowd_h2.str",
    "3_day_snow\\crowd_h3.str",
    "3_day_snow\\stad1_main.bin",
    "3_day_snow\\stad2_entrance.bin",
    "3_day_snow\\stad3_adboards.bin",
    "4_night_fine\\crowd_a0.str",
    "4_night_fine\\crowd_a1.str",
    "4_night_fine\\crowd_a2.str",
    "4_night_fine\\crowd_a3.str",
    "4_night_fine\\crowd_h0.str",
    "4_night_fine\\crowd_h1.str",
    "4_night_fine\\crowd_h2.str",
    "4_night_fine\\crowd_h3.str",
    "4_night_fine\\stad1_main.bin",
    "4_night_fine\\stad2_entrance.bin",
    "4_night_fine\\stad3_adboards.bin",
    "5_night_rain\\crowd_a0.str",
    "5_night_rain\\crowd_a1.str",
    "5_night_rain\\crowd_a2.str",
    "5_night_rain\\crowd_a3.str",
    "5_night_rain\\crowd_h0.str",
    "5_night_rain\\crowd_h1.str",
    "5_night_rain\\crowd_h2.str",
    "5_night_rain\\crowd_h3.str",
    "5_night_rain\\stad1_main.bin",
    "5_night_rain\\stad2_entrance.bin",
    "5_night_rain\\stad3_adboards.bin",
    "6_night_snow\\crowd_a0.str",
    "6_night_snow\\crowd_a1.str",
    "6_night_snow\\crowd_a2.str",
    "6_night_snow\\crowd_a3.str",
    "6_night_snow\\crowd_h0.str",
    "6_night_snow\\crowd_h1.str",
    "6_night_snow\\crowd_h2.str",
    "6_night_snow\\crowd_h3.str",
    "6_night_snow\\stad1_main.bin",
    "6_night_snow\\stad2_entrance.bin",
    "6_night_snow\\stad3_adboards.bin",
    "adboards_tex\\default.bin",
};

#define STAD_MAIN(x) (x==8 || x==19 || x==30 || x==41 || x==52 || x==63)
#define STAD_CROWD_A0(x) (x==0 || x==11 || x==22 || x==33 || x==44 || x==55)
#define STAD_CROWD_H0(x) (x==4 || x==15 || x==26 || x==37 || x==48 || x==59)
#define ADBOARDS 66

static char* WEATHER_NAMES[] = {
	"Random weather",
	"Day / Fine",
	"Day / Rain",
	"Day / Snow",
	"Night / Fine",
	"Night / Rain",
	"Night / Snow",
};

static DWORD code[CODELEN];
static DWORD dta[DATALEN];

static std::map<DWORD,bool> g_AFS_idMap;
static std::map<WORD,std::string> g_HomeStadiumMap;
static std::map<std::string,STADINFO*> g_stadiumMap;
static std::map<std::string,STADINFO*>::iterator g_stadiumMapIterator;

static bool hasGdbStadiums = false;
static char g_stadiumText[61];
static bool viewGdbStadiums = false;
static BYTE weather = 0;
static bool isViewStadiumMode = false;
static bool needsReload=false;
static BYTE savedStadiumChange[6];
static BYTE savedAdvanceStadium[5];
static bool savedChangeStadiumCode=false;

static DWORD g_stadId = 0xffffffff;
static bool g_gameChoice = true;
static bool g_homeTeamChoice = false;
static bool isSelectMode=false;

void SafeRelease(LPVOID ppObj);
EXTERN_C BOOL WINAPI DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID lpReserved);
void InitStadiumServer();
DWORD stadGetString(DWORD param0, DWORD text);
DWORD stadGetString2(DWORD ppText, DWORD,DWORD,DWORD,DWORD,DWORD,DWORD,DWORD);
DWORD stadWriteBuilt(char* dest, char* format, DWORD num);
DWORD stadWriteCapacity(char* dest, char* format, DWORD num);
DWORD stadSetLCM(DWORD p1);
DWORD stadReadLCM(DWORD p1, DWORD p2, DWORD p3, DWORD p4);
void CheckViewStadiumMode();
int GetStadId(DWORD id);
int GetFileId(DWORD id);
static void InitStadiumMaps();
static WORD GetTeamId(int which);
void FindAdboardsFile(char* filename);
void FindStadiumFile(int stadId, DWORD stadFileId, char* filename);
void stadAfsReplace(GETFILEINFO* gfi);
void CheckInput();

void stadKeyboardProc(int code1, WPARAM wParam, LPARAM lParam);
void stadShowMenu();
void stadBeginUniSelect();
void stadEndUniSelect();
void stadPresent(IDirect3DDevice8* self, CONST RECT* src, CONST RECT* dest, HWND hWnd, LPVOID unused);
void stadReset(IDirect3DDevice8* self, LPVOID params);
void stadCreateDevice(IDirect3D8* self, UINT Adapter,
    D3DDEVTYPE DeviceType, HWND hFocusWindow, DWORD BehaviorFlags,
    D3DPRESENT_PARAMETERS *pPresentationParameters, 
    IDirect3DDevice8** ppReturnedDeviceInterface);

HRESULT InvalidateDeviceObjects(IDirect3DDevice8* dev);
HRESULT DeleteDeviceObjects(IDirect3DDevice8* dev);
HRESULT RestoreDeviceObjects(IDirect3DDevice8* dev);
void DrawPreview(IDirect3DDevice8* dev);

// Calls IUnknown::Release() on an instance
void SafeRelease(LPVOID ppObj)
{
    try {
        IUnknown** ppIUnknown = (IUnknown**)ppObj;
        if (ppIUnknown == NULL)
        {
            Log(&k_stadium,"Address of IUnknown reference is 0");
            return;
        }
        if (*ppIUnknown != NULL)
        {
            (*ppIUnknown)->Release();
            *ppIUnknown = NULL;
        }
    } catch (...) {
        // problem with a safe-release
        TRACE(&k_stadium,"Problem with safe-release");
    }
}

/**
 * Read stadium information into structure
 */
static void ReadStadInfo(char* folderName, STADINFO* stadInfo)
{
	char filename[BUFLEN];
	ZeroMemory(filename, BUFLEN);
    sprintf(filename, "%sGDB\\stadiums\\%s\\info.txt", GetPESInfo()->gdbDir, folderName);

    //initialize to defaults
    stadInfo->built = 1991;
    stadInfo->capacity = 54321;
    strcpy(stadInfo->city,"\0");

    FILE* f = fopen(filename, "rt");
    if (f) {
        // go line by line
        char buf[MAXLINELEN];
        while (!feof(f))
        {
            ZeroMemory(buf, MAXLINELEN);
            fgets(buf, MAXLINELEN, f);
            if (lstrlen(buf) == 0) break;

            // strip off comments
            char* comm = strstr(buf, "#");
            if (comm != NULL) comm[0] = '\0';

            // read values
            char key[255];
            char city[255];
            bool processed=false;
            int value;
            
            
            if (sscanf(buf, "%s = %s", key, &city) == 2)
            	if (stricmp(key,"city")==0) {
            		strcpy(stadInfo->city,city);
            		processed=true;
            	};
            
            if (!processed)
	            if (sscanf(buf, "%s = %d", key, &value) == 2) {
	                if (stricmp(key,"built")==0) {
	                    stadInfo->built = value;
	                } else if (stricmp(key,"capacity")==0) {
	                    stadInfo->capacity = value;
	                }
	            }
        }
        fclose(f);
    }
}

/**
 * Initialize home-stadium map.
 */
static void InitStadiumMaps()
{
    // step 1: enumarate all stadiums
	WIN32_FIND_DATA fData;
	char pattern[MAXLINELEN];
	ZeroMemory(pattern, MAXLINELEN);

    sprintf(pattern, "%sGDB\\stadiums\\*", GetPESInfo()->gdbDir);

	HANDLE hff = FindFirstFile(pattern, &fData);
	if (hff != INVALID_HANDLE_VALUE) {
        while(true)
        {
            // check if this is a directory
            if (fData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                // check for system folders
                if (fData.cFileName[0]!='.') {
                    LogWithString(&k_stadium, "found stadium: {%s}", fData.cFileName);

                    //built/capacity structure
                    STADINFO* stadInfo = new STADINFO();
                    ReadStadInfo(fData.cFileName, stadInfo);

                    // store in the home-stadium map
                    g_stadiumMap[std::string(fData.cFileName)] = stadInfo;
                }
            }
            // proceed to next file
            if (!FindNextFile(hff, &fData)) break;
        }
        FindClose(hff);
    }

    // initialize stadium iterator
    g_stadiumMapIterator = g_stadiumMap.begin();

    g_gameChoice = true;
    if (_stad_cfg.mode == STAD_HOME_TEAM) {
        // set the home team flag
        g_homeTeamChoice = true;
        g_gameChoice = false;

    } else if (_stad_cfg.mode == STAD_SELECT) {
        // try to find the stadium name in the map
        map<std::string,STADINFO*>::iterator it = g_stadiumMap.find(_stad_cfg.stadName);
        if (it != g_stadiumMap.end()) {
            g_stadiumMapIterator = it;
            g_gameChoice = false;
        }
    }

    // step 2: read map.txt
    char mapFile[MAXFILENAME];
    ZeroMemory(mapFile,MAXFILENAME);
	memcpy(code, codeArray[GetPESInfo()->GameVersion], sizeof(code));

    sprintf(mapFile, "%sGDB\\stadiums\\map.txt", GetPESInfo()->gdbDir);
    FILE* map = fopen(mapFile, "rt");
    if (map == NULL) {
        LogWithString(&k_stadium, "Unable to find stadium-map: %s", mapFile);
        return;
    }

	// go line by line
    char buf[MAXLINELEN];
	while (!feof(map))
	{
		ZeroMemory(buf, MAXLINELEN);
		fgets(buf, MAXLINELEN, map);
		if (lstrlen(buf) == 0) break;

		// strip off comments
		char* comm = strstr(buf, "#");
		if (comm != NULL) comm[0] = '\0';

        // find team id
        WORD teamId = 0xffff;
        if (sscanf(buf, "%d", &teamId)==1) {
            LogWithNumber(&k_stadium, "teamId = %d", teamId);
            char* foldername = NULL;
            // look for comma
            char* pComma = strstr(buf,",");
            if (pComma) {
                // what follows is the filename.
                // It can be contained within double quotes, so 
                // strip those off, if found.
                char* start = NULL;
                char* end = NULL;
                start = strstr(pComma + 1,"\"");
                if (start) end = strstr(start + 1,"\"");
                if (start && end) {
                    // take what inside the quotes
                    end[0]='\0';
                    foldername = start + 1;
                } else {
                    // just take the rest of the line
                    foldername = pComma + 1;
                }

                LogWithString(&k_stadium, "foldername = {%s}", foldername);

                // store in the home-stadium map
                g_HomeStadiumMap[teamId] = std::string(foldername);
            }
        }
    }
    fclose(map);
}

/**
 * Return currently selected (home or away) team ID.
 */
WORD GetTeamId(int which)
{
    BYTE* mlData;
    if (dta[TEAM_IDS]==0) return 0xffff;
    WORD id = ((WORD*)dta[TEAM_IDS])[which];
    if (id==0x126 || id==0x127) {
        WORD id1,id2;
        switch (id) {
            case 0x126:
                // saved team (home)
                id1 = *(WORD*)(*(BYTE**)dta[SAVED_TEAM_HOME] + 0x36);
                id2 = *(WORD*)(*(BYTE**)dta[SAVED_TEAM_HOME] + 0x40);
                if (id1 != 0) {
                    id = id1;
                } else {
                    id = id2;
                }
                break;
            case 0x127:
                // saved team (away)
                id1 = *(WORD*)(*(BYTE**)dta[SAVED_TEAM_AWAY] + 0x36);
                id2 = *(WORD*)(*(BYTE**)dta[SAVED_TEAM_AWAY] + 0x40);
                if (id1 != 0) {
                    id = id1;
                } else {
                    id = id2;
                }
                break;
        }
    }
    return id;
}

// entry point
EXTERN_C BOOL WINAPI DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID lpReserved)
{
	if (dwReason == DLL_PROCESS_ATTACH)
	{
		Log(&k_stadium,"Attaching dll...");
		
		hInst=hInstance;
        InitializeCriticalSection(&g_cs);

        //check game version
		switch (GetPESInfo()->GameVersion) {
            case gvPES6PC: //support for PES6 PC
            case gvPES6PC110: //support for PES6 PC 1.10
            case gvWE2007PC: //support for WE:PES 2007 PC
				break;
            default:
                Log(&k_stadium,"Your game version is currently not supported!");
                return false;
		}

		RegisterKModule(&k_stadium);
		
		HookFunction(hk_D3D_Create,(DWORD)InitStadiumServer);

		HookFunction(hk_DrawKitSelectInfo,(DWORD)stadShowMenu);
        HookFunction(hk_OnShowMenu,(DWORD)stadBeginUniSelect);
        HookFunction(hk_OnHideMenu,(DWORD)stadEndUniSelect);
        
		HookFunction(hk_D3D_CreateDevice,(DWORD)stadCreateDevice);
		HookFunction(hk_D3D_Present,(DWORD)stadPresent);
		HookFunction(hk_D3D_Reset,(DWORD)stadReset);
        
		HookFunction(hk_Input,(DWORD)stadKeyboardProc);

        //load settings
        ZeroMemory(&_stad_cfg, sizeof(STAD_CFG));
        char stadCfg[BUFLEN];
        ZeroMemory(stadCfg, BUFLEN);
        sprintf(stadCfg, "%s\\stadium.dat", GetPESInfo()->mydir);
        FILE* f = fopen(stadCfg, "rb");
        if (f) {
            fread(&_stad_cfg, sizeof(STAD_CFG), 1, f);
            fclose(f);
        } else {
            _stad_cfg.mode = STAD_GAME_CHOICE;
        }
	}
	else if (dwReason == DLL_PROCESS_DETACH)
	{
		Log(&k_stadium,"Detaching dll...");
        DeleteCriticalSection(&g_cs);

		//save settings
        ZeroMemory(_stad_cfg.stadName, sizeof(_stad_cfg.stadName));
        if (g_homeTeamChoice) {
            _stad_cfg.mode = STAD_HOME_TEAM;
        } else if (g_gameChoice) {
            _stad_cfg.mode = STAD_GAME_CHOICE;
        } else {
            _stad_cfg.mode = STAD_SELECT;
            strcpy(_stad_cfg.stadName, g_stadiumMapIterator->first.c_str());
        }
        char stadCfg[BUFLEN];
	    ZeroMemory(stadCfg, BUFLEN);
	    sprintf(stadCfg, "%s\\stadium.dat", GetPESInfo()->mydir);
	    FILE* f = fopen(stadCfg, "wb");
	    if (f) {
	        fwrite(&_stad_cfg, sizeof(STAD_CFG), 1, f);
	        fclose(f);
	    }

        MasterUnhookFunction(code[C_GETSTRING_CS], stadGetString);
        Log(&k_stadium, "GetString unhooked.");
        MasterUnhookFunction(code[C_GETSTRING2_CS], stadGetString2);
        Log(&k_stadium, "GetString2 unhooked.");

        MasterUnhookFunction(code[C_WRITENUM_CS1], stadWriteBuilt);
        Log(&k_stadium, "WriteBuilt unhooked.");
        MasterUnhookFunction(code[C_WRITENUM_CS2], stadWriteCapacity);
        Log(&k_stadium, "WriteCapacity unhooked.");
			
        MasterUnhookFunction(code[C_SETLCM_CS], stadSetLCM);
        Log(&k_stadium, "SetLCM unhooked.");

        MasterUnhookFunction(code[C_READLCM_CS], stadReadLCM);
        Log(&k_stadium, "ReadLCM unhooked.");

		UnhookFunction(hk_D3D_Create,(DWORD)InitStadiumServer);

		UnhookFunction(hk_DrawKitSelectInfo,(DWORD)stadShowMenu);
        UnhookFunction(hk_OnShowMenu,(DWORD)stadBeginUniSelect);
        UnhookFunction(hk_OnHideMenu,(DWORD)stadEndUniSelect);   
        
        UnhookFunction(hk_D3D_Present,(DWORD)stadPresent);
	};

	return true;
}

void InitStadiumServer()
{
    int i,j;

	memcpy(code, codeArray[GetPESInfo()->GameVersion], sizeof(code));
	memcpy(dta, dtaArray[GetPESInfo()->GameVersion], sizeof(dta));
    
    RegisterAfsReplaceCallback(stadAfsReplace);
    Log(&k_stadium, "stadAfsReplace hooked");

	char tmp[BUFLEN];
	strcpy(tmp,GetPESInfo()->pesdir);
	strcat(tmp,GetPESInfo()->AFS_0_text);

    FILE* f = fopen(tmp,"rb");
    if (!f) {
        Log(&k_stadium, "InitStadiumServer: problem opening 0_text.afs for reading.");
        return;
    }

    // mark ids for stadium files
	for (i=0; i<dta[NUM_STADS]; i++) {
        for (j=0; j<dta[NUM_FILES]; j++) {
            DWORD id = dta[STAD_FIRST] + i*dta[NUM_FILES] + j;
            id = (id > dta[NOU_CAMP_SHIFT_ID])?(id + dta[SHIFT]):id;
            // store id in id-map
            g_AFS_idMap[id] = true;
        }
    }

    // mark ids for adboard textures
	for (i=0; i<dta[NUM_ADBOARD_TEX]; i++) {
        DWORD id = dta[ADBOARD_TEX_FIRST] + i;
        // store id in id-map
        g_AFS_idMap[id] = true;
    }

    fclose(f);

    // initialize with info from "map.txt"
    InitStadiumMaps();
    
    // check we have any stadiums in GDB
    if (g_stadiumMap.begin() != g_stadiumMap.end())
    	hasGdbStadiums=true;

	UnhookFunction(hk_D3D_Create,(DWORD)InitStadiumServer);

	return;
}

int GetStadId(DWORD id)
{
    int sid = id - dta[STAD_FIRST];
    sid = (id > dta[NOU_CAMP_SHIFT_ID])?(sid - dta[SHIFT]):sid;
    return sid / dta[NUM_FILES];
}

int GetFileId(DWORD id)
{
    int sid = id - dta[STAD_FIRST];
    sid = (id > dta[NOU_CAMP_SHIFT_ID])?(sid - dta[SHIFT]):sid;
    return sid % dta[NUM_FILES];
}

int GetStadiumBase(DWORD stadId)
{
    int base = dta[STAD_FIRST] + stadId * dta[NUM_FILES];
    base = (base > dta[NOU_CAMP_SHIFT_ID])?(base + dta[SHIFT]):base;
    LogWithTwoNumbers(&k_stadium, "GetStadiumBase(%d): base = %d", stadId, base);
    return base;
}

void FindAdboardsFile(char* filename)
{
	LCM* lcm=(LCM*)dta[TEAM_IDS];
    // force full stadium reload next time
    BYTE* randomStad = (BYTE*)dta[RANDOM_STADIUM_FLAG];
    *randomStad = *randomStad | 0x01;

    if (g_gameChoice || isViewStadiumMode) {
        return;
    }

    //if (g_gameChoice && !(isViewStadiumMode && viewGdbStadiums)) {
    //    return 0; //game choice stadium
    //}

    //if (g_homeTeamChoice && !(isViewStadiumMode && viewGdbStadiums)) {
    if (g_homeTeamChoice) {
        WORD teamId = GetTeamId(HOME);
        LogWithNumber(&k_stadium, "FindAdboardsFile: home team = %d", teamId);
        std::map<WORD,std::string>::iterator it = g_HomeStadiumMap.find(teamId);
        if (it !=  g_HomeStadiumMap.end()) {
            LogWithString(&k_stadium, "FindAdboardsFile: has a home stadium: %s", 
                    (char*)it->second.c_str());

            sprintf(filename,"%sGDB\\stadiums\\%s\\%s", 
                    GetPESInfo()->gdbDir, (char*)it->second.c_str(), FILE_NAMES[ADBOARDS]);
        }
    } else {
        sprintf(filename,"%sGDB\\stadiums\\%s\\%s", 
                GetPESInfo()->gdbDir, g_stadiumMapIterator->first.c_str(), FILE_NAMES[ADBOARDS]);
    }

    //sprintf(filename,"%sGDB\\stadiums\\Santiago Bernabeu\\%s", 
    //        GetPESInfo()->gdbDir, FILE_NAMES[ADBOARDS]);
    return;
}

const char* GetStadFileName(int stadId, DWORD stadFileId)
{
    return FILE_NAMES[stadFileId];
}

void FindStadiumFile(int stadId, DWORD stadFileId, char* filename)
{
    if (isViewStadiumMode) {
        if (!viewGdbStadiums) {
            return; // not a gdb stadium - in "View Stadiums"
        }
    } else if (g_gameChoice) {
        return; //game choice stadium - not in "View Stadiums"
    }

    LogWithNumber(&k_stadium, "g_gameChoice = %d", g_gameChoice);
    LogWithNumber(&k_stadium, "isViewStadiumMode = %d", isViewStadiumMode);
    LogWithNumber(&k_stadium, "viewGdbStadiums = %d", viewGdbStadiums);

    if (g_homeTeamChoice && !(isViewStadiumMode && viewGdbStadiums)) {
        WORD teamId = GetTeamId(HOME);
        LogWithNumber(&k_stadium, "FindStadiumFile: home team = %d", teamId);
        std::map<WORD,std::string>::iterator it = g_HomeStadiumMap.find(teamId);
        if (it !=  g_HomeStadiumMap.end()) {
            LogWithString(&k_stadium, "FindStadiumFile: has a home stadium: %s", 
                    (char*)it->second.c_str());

            sprintf(filename,"%sGDB\\stadiums\\%s\\%s", 
                    GetPESInfo()->gdbDir, (char*)it->second.c_str(), GetStadFileName(stadId, stadFileId));
        }
    } else {
        sprintf(filename,"%sGDB\\stadiums\\%s\\%s", 
                GetPESInfo()->gdbDir, g_stadiumMapIterator->first.c_str(), GetStadFileName(stadId, stadFileId));
    }

    //sprintf(filename,"%sGDB\\stadiums\\Santiago Bernabeu\\%s", 
    //        GetPESInfo()->gdbDir, FILE_NAMES[stadFileId]);
    return;
}

void DumpBuffer(char* filename, LPVOID buf, DWORD len)
{
    FILE* f = fopen(filename,"wb");
    if (f) {
        fwrite(buf, len, 1, f);
        fclose(f);
    }
}

void stadAfsReplace(GETFILEINFO* gfi)
{
	if (!bOthersHooked) {
        MasterHookFunction(code[C_GETSTRING_CS], 2, stadGetString);
        Log(&k_stadium, "hooked GetString");
        MasterHookFunction(code[C_GETSTRING2_CS], 8, stadGetString2);
        Log(&k_stadium, "hooked GetString2");

        MasterHookFunction(code[C_WRITENUM_CS1], 3, stadWriteBuilt);
        Log(&k_stadium, "hooked WriteBuilt");
        MasterHookFunction(code[C_WRITENUM_CS2], 3, stadWriteCapacity);
        Log(&k_stadium, "hooked WriteCapacity");

        MasterHookFunction(code[C_SETLCM_CS], 1, stadSetLCM);
        Log(&k_stadium, "hooked SetLCM");
        MasterHookFunction(code[C_READLCM_CS], 4, stadReadLCM);
        Log(&k_stadium, "hooked ReadLCM");

        bOthersHooked = true;
    }
	
	if (gfi->isProcessed) return;
	
	DWORD afsId = 0, fileId = 0;
	char filename[BUFLEN];
	ZeroMemory(filename, BUFLEN);
	fileId = splitFileId(gfi->fileId, &afsId);
	
	if (afsId == 1) { // 0_text.afs
        if (MAP_CONTAINS(g_AFS_idMap, fileId)) {
			LogWithTwoNumbers(&k_stadium,"stadAfsReplace: afsId=%d, fileId=%d", afsId, fileId);

            if (fileId < dta[STAD_FIRST]) {
                // adboard textures
                if (g_stadId != 0xffffffff) {
                    // we know the stadium id

                    // force Della Alpi adboards
                    //fileId = dta[DELLA_ALPI_ADBOARDS];

                    FindAdboardsFile(filename);
                    LogWithString(&k_stadium,"OnReadFile: file: %s", FILE_NAMES[ADBOARDS]);
                }
            } else {
                // stadium files

                // check if stadium file exists
                int stadId = GetStadId(fileId);
                int stadFileId = GetFileId(fileId);

                LogWithTwoNumbers(&k_stadium,"stadAfsReplace: stadId=%d, stadFileId=%d", stadId, stadFileId);

                // force Della Alpi stadium
                //fileId = stadFileId + dta[DELLA_ALPI];

                // remember current stadium ID
                if (STAD_MAIN(stadFileId)) {
                    g_stadId = stadId;
                }
                
                FindStadiumFile(stadId, stadFileId, filename);
                
	            LogWithNumber(&k_stadium,"OnReadFile: stadium: %d", stadId);
	            LogWithString(&k_stadium,"OnReadFile: file: %s", FILE_NAMES[stadFileId]);
            }
			if (strlen(filename)>0) {
            	loadReplaceFile(filename);
                gfi->isProcessed = true;
	        }
		}
	}
	
	CheckViewStadiumMode();
	return;
}



void stadBeginUniSelect()
{
    //Log(&k_stadium, "stadBeginUniSelect");
    
    isSelectMode=true;
    dksiSetMenuTitle("Stadium selection");
    
    // invalidate preview texture
    SafeRelease( &g_preview_tex );
    g_newStad = true;

    return;
}

void stadEndUniSelect()
{
    //Log(&k_stadium, "stadEndUniSelect");
    
    isSelectMode=false;

    return;
}

void stadKeyboardProc(int code1, WPARAM wParam, LPARAM lParam)
{
	
	if (code1 < 0)
		return;
		
	if ((code1!=HC_ACTION) || !(lParam & 0x80000000))
		return;

    KEYCFG* keyCfg = GetInputCfg();
	if (isViewStadiumMode) {
		if (wParam == keyCfg->keyboard.keySwitch1 && hasGdbStadiums) {
			viewGdbStadiums = !viewGdbStadiums;
			needsReload=true;
		
		} else if (wParam == keyCfg->keyboard.keySwitch2) {
			weather++;
			if (weather>6)
				weather=0;
			
			needsReload=true;

		} else if (wParam == keyCfg->keyboard.keyNext && hasGdbStadiums && viewGdbStadiums) {
            g_stadiumMapIterator++;
            if (g_stadiumMapIterator == g_stadiumMap.end()) {
                g_stadiumMapIterator = g_stadiumMap.begin();
            }
			needsReload=true;
        } else if (wParam == keyCfg->keyboard.keyPrev && hasGdbStadiums && viewGdbStadiums) {
	        if (g_stadiumMapIterator == g_stadiumMap.begin()) {
	            g_stadiumMapIterator = g_stadiumMap.end();
	        }
	        g_stadiumMapIterator--;
			needsReload=true;
        }
		
		if (needsReload) {
			if (!savedChangeStadiumCode) {
				memcpy(savedStadiumChange,(BYTE*)(code[C_STADIUMCHANGE]),6);
				memcpy(savedAdvanceStadium,(BYTE*)code[C_ADVANCESTADIUM],5);
				savedChangeStadiumCode=true;
			};
			
			//by nopping out these commands, we force a full
			//reload of the stadium with going to the next one
			memset((BYTE*)(code[C_STADIUMCHANGE]),0x90,6);
			memset((BYTE*)(code[C_ADVANCESTADIUM]),0x90,5);
		};
		
		return;
	};	
	

	if (isSelectMode && hasGdbStadiums) {
		if (wParam == keyCfg->keyboard.keyNext) {
	        if (g_gameChoice || g_homeTeamChoice) {
	            g_stadiumMapIterator = g_stadiumMap.begin();
	        } else {
	            g_stadiumMapIterator++;
	            if (g_stadiumMapIterator == g_stadiumMap.end()) {
	                g_stadiumMapIterator = g_stadiumMap.begin();
	            }
	        }
	        g_gameChoice = false;
	        g_homeTeamChoice = false;

            // invalidate preview texture
            SafeRelease( &g_preview_tex );
            g_newStad = true;
	
		} else if (wParam == keyCfg->keyboard.keyPrev) {
	        if (g_gameChoice || g_homeTeamChoice) {
	            g_stadiumMapIterator = g_stadiumMap.end();
	        }
	        if (g_stadiumMapIterator == g_stadiumMap.begin()) {
	            g_stadiumMapIterator = g_stadiumMap.end();
	        }
	        g_stadiumMapIterator--;
	        g_gameChoice = false;
	        g_homeTeamChoice = false;

            // invalidate preview texture
            SafeRelease( &g_preview_tex );
            g_newStad = true;
	
		} else if (wParam == keyCfg->keyboard.keyReset) {
			//reset/homeTeamChoice
	        g_gameChoice = !g_gameChoice;
	        g_homeTeamChoice = !g_gameChoice;

            // invalidate preview texture
            SafeRelease( &g_preview_tex );
            g_newStad = true;
	
		} else if (wParam == keyCfg->keyboard.keyRandom) {
            //random
            LARGE_INTEGER num;
            QueryPerformanceCounter(&num);
            int iterations = num.LowPart % MAX_ITERATIONS;
            for (int j=0;j<iterations;j++) {
                g_stadiumMapIterator++;
                if (g_stadiumMapIterator == g_stadiumMap.end()) {
                    g_stadiumMapIterator = g_stadiumMap.begin();
                }
            }
            g_gameChoice = false;
            g_homeTeamChoice = false;

            // invalidate preview texture
            SafeRelease( &g_preview_tex );
            g_newStad = true;
		};
		
		return;
	};
	
	return;
};

void CheckInput()
{
    DWORD* inputs = GetInputTable();
    KEYCFG* keyCfg = GetInputCfg();

    if (isViewStadiumMode) {
        for (int n=0; n<8; n++) {
            if (INPUT_EVENT(inputs, n, FUNCTIONAL, keyCfg->gamepad.keySwitch1) && hasGdbStadiums) {
                viewGdbStadiums = !viewGdbStadiums;
                needsReload=true;
            
            } else if (INPUT_EVENT(inputs, n, FUNCTIONAL, keyCfg->gamepad.keySwitch2)) {
                weather++;
                if (weather>6)
                    weather=0;
                
                needsReload=true;

            } else if (INPUT_EVENT(inputs, n, FUNCTIONAL, keyCfg->gamepad.keyNext) && hasGdbStadiums && viewGdbStadiums) {
                g_stadiumMapIterator++;
                if (g_stadiumMapIterator == g_stadiumMap.end()) {
                    g_stadiumMapIterator = g_stadiumMap.begin();
                }
                needsReload=true;
            } else if (INPUT_EVENT(inputs, n, FUNCTIONAL, keyCfg->gamepad.keyPrev) && hasGdbStadiums && viewGdbStadiums) {
                if (g_stadiumMapIterator == g_stadiumMap.begin()) {
                    g_stadiumMapIterator = g_stadiumMap.end();
                }
                g_stadiumMapIterator--;
                needsReload=true;
            }
            
            if (needsReload) {
                if (!savedChangeStadiumCode) {
                    memcpy(savedStadiumChange,(BYTE*)(code[C_STADIUMCHANGE]),6);
                    memcpy(savedAdvanceStadium,(BYTE*)code[C_ADVANCESTADIUM],5);
                    savedChangeStadiumCode=true;
                }
                
                //by nopping out these commands, we force a full
                //reload of the stadium with going to the next one
                memset((BYTE*)(code[C_STADIUMCHANGE]),0x90,6);
                memset((BYTE*)(code[C_ADVANCESTADIUM]),0x90,5);
            }
        }
    }

    if (isSelectMode && hasGdbStadiums) {
        for (int n=0; n<8; n++) {
            if (INPUT_EVENT(inputs, n, FUNCTIONAL, keyCfg->gamepad.keyNext)) {
                if (g_gameChoice || g_homeTeamChoice) {
                    g_stadiumMapIterator = g_stadiumMap.begin();
                } else {
                    g_stadiumMapIterator++;
                    if (g_stadiumMapIterator == g_stadiumMap.end()) {
                        g_stadiumMapIterator = g_stadiumMap.begin();
                    }
                }
                g_gameChoice = false;
                g_homeTeamChoice = false;

                // invalidate preview texture
                SafeRelease( &g_preview_tex );
                g_newStad = true;
        
            } else if (INPUT_EVENT(inputs, n, FUNCTIONAL, keyCfg->gamepad.keyPrev)) {
                if (g_gameChoice || g_homeTeamChoice) {
                    g_stadiumMapIterator = g_stadiumMap.end();
                }
                if (g_stadiumMapIterator == g_stadiumMap.begin()) {
                    g_stadiumMapIterator = g_stadiumMap.end();
                }
                g_stadiumMapIterator--;
                g_gameChoice = false;
                g_homeTeamChoice = false;

                // invalidate preview texture
                SafeRelease( &g_preview_tex );
                g_newStad = true;
        
            } else if (INPUT_EVENT(inputs, n, FUNCTIONAL, keyCfg->gamepad.keyReset)) {
                //reset/homeTeamChoice
                g_gameChoice = !g_gameChoice;
                g_homeTeamChoice = !g_gameChoice;

                // invalidate preview texture
                SafeRelease( &g_preview_tex );
                g_newStad = true;
        
            } else if (INPUT_EVENT(inputs, n, FUNCTIONAL, keyCfg->gamepad.keyRandom)) {
                //random
                LARGE_INTEGER num;
                QueryPerformanceCounter(&num);
                int iterations = num.LowPart % MAX_ITERATIONS;
                for (int j=0;j<iterations;j++) {
                    g_stadiumMapIterator++;
                    if (g_stadiumMapIterator == g_stadiumMap.end()) {
                        g_stadiumMapIterator = g_stadiumMap.begin();
                    }
                }
                g_gameChoice = false;
                g_homeTeamChoice = false;

                // invalidate preview texture
                SafeRelease( &g_preview_tex );
                g_newStad = true;
            }
        }
    }

}

void stadShowMenu()
{
	SIZE size;
	DWORD color = 0xffffffff; // white
    char text[512];

    ZeroMemory(text, sizeof(text));
	if (g_gameChoice) {
		color = 0xffc0c0c0; // gray if stadium is game choice
        strcpy(text, "Stadium: game choice");

    } else if (g_homeTeamChoice) {
		color = 0xffffffc0; // pale yellow if stadium is home-team choice
        WORD teamId = GetTeamId(HOME);
        std::map<WORD,std::string>::iterator hit = g_HomeStadiumMap.find(teamId);
        if (hit !=  g_HomeStadiumMap.end()) {
            sprintf(text, "Home team stadium: %s", hit->second.c_str());
        } else {
            strcpy(text, "Stadium: home team");
        }

    } else {
		//if possible add some preview images of the stadium here
		sprintf(text, "Stadium: %s", g_stadiumMapIterator->first.c_str());
    }
		
	KGetTextExtent(text, 12, &size);
	//draw shadow
	if (!g_gameChoice) {
		KDrawText(
                (GetPESInfo()->bbWidth-size.cx)/2+3*GetPESInfo()->stretchX,GetPESInfo()->bbHeight*0.77+
                3*GetPESInfo()->stretchY,0xff000000,12,text,true
        );
    }
    
	//print stadium label
	KDrawText((GetPESInfo()->bbWidth-size.cx)/2,GetPESInfo()->bbHeight*0.77,color,12,text,true);

    //draw stadium preview
    DrawPreview(g_device);

    // check input
    CheckInput();

	return;
};

void stadCreateDevice(IDirect3D8* self, UINT Adapter,
    D3DDEVTYPE DeviceType, HWND hFocusWindow, DWORD BehaviorFlags,
    D3DPRESENT_PARAMETERS *pPresentationParameters, 
    IDirect3DDevice8** ppReturnedDeviceInterface)
{
    g_device = *ppReturnedDeviceInterface;
    return;
}

void stadPresent(IDirect3DDevice8* self, CONST RECT* src, CONST RECT* dest, HWND hWnd, LPVOID unused)
{
    g_device = self;
	if (!isViewStadiumMode) return;

    // check for user input
    CheckInput();
	
	if (viewGdbStadiums) {
		KDrawText(659,42,0xff000000,16,"1: Stadiums from GDB");
		KDrawText(661,44,0xff000000,16,"1: Stadiums from GDB");
		KDrawText(659,44,0xff000000,16,"1: Stadiums from GDB");
		KDrawText(661,42,0xff000000,16,"1: Stadiums from GDB");
		KDrawText(660,43,0xffffffc0,16,"1: Stadiums from GDB");
	} else {
		KDrawText(659,42,0xffffffc0,16,"1: Ingame stadiums");
		KDrawText(661,44,0xffffffc0,16,"1: Ingame stadiums");
		KDrawText(659,44,0xffffffc0,16,"1: Ingame stadiums");
		KDrawText(661,42,0xffffffc0,16,"1: Ingame stadiums");
		KDrawText(660,43,0xff000000,16,"1: Ingame stadiums");
	};
	
	DWORD color=0xffffffc0;
	DWORD color2=0xff000000;
	
	if (weather==0) {
		color=0xff000000;
		color2=0xffffffc0;
	};

	char buf[1024];
	sprintf(buf,"2: %s",WEATHER_NAMES[weather]);

	KDrawText(659,78,color2,16,buf);
	KDrawText(661,80,color2,16,buf);
	KDrawText(661,78,color2,16,buf);
	KDrawText(659,80,color2,16,buf);
	KDrawText(660,79,color,16,buf);
	
	//print city
	if (viewGdbStadiums && strlen(g_stadiumMapIterator->second->city)>0) {
		sprintf(buf,"City: %s",g_stadiumMapIterator->second->city);
		color=0xff000000; //black font
		color2=0xffffffff; //white border
		KDrawText(52,117,color2,20,buf);
		KDrawText(52,121,color2,20,buf);
		KDrawText(50,119,color2,20,buf);
		KDrawText(54,119,color2,20,buf);
		KDrawText(52,119,color,20,buf);
	};

	return;
};

bool IsStadiumText(char* text) 
{
    DWORD base = *((DWORD*)dta[STADIUM_TEXT_TABLE]);
    for (int i=0; i<dta[NUM_STADS]; i++) {
        if (base + STADIUM_TEXT_LEN*i == (DWORD)text) return true;
    }
    return false;
}

/* New Reset function */
void stadReset(IDirect3DDevice8* self, LPVOID params)
{
	Log(&k_stadium,"stadReset: cleaning-up.");

	InvalidateDeviceObjects(self);
	DeleteDeviceObjects(self);

    g_needsRestore = TRUE;
    g_device = self;
	return;
}

void SetPosition(CUSTOMVERTEX2* dest, CUSTOMVERTEX2* src, int n, int x, int y) 
{
    FLOAT xratio = GetPESInfo()->bbWidth / 1024.0;
    FLOAT yratio = GetPESInfo()->bbHeight / 768.0;
    for (int i=0; i<n; i++) {
        dest[i].x = (FLOAT)(int)((src[i].x + x) * xratio);
        dest[i].y = (FLOAT)(int)((src[i].y + y) * yratio);
    }
}

void SetPosition(CUSTOMVERTEX* dest, CUSTOMVERTEX* src, int n, int x, int y) 
{
    FLOAT xratio = GetPESInfo()->bbWidth / 1024.0;
    FLOAT yratio = GetPESInfo()->bbHeight / 768.0;
    for (int i=0; i<n; i++) {
        dest[i].x = (FLOAT)(int)((src[i].x + x) * xratio);
        dest[i].y = (FLOAT)(int)((src[i].y + y) * yratio);
    }
}

/* creates vertex buffers */
HRESULT InitVB(IDirect3DDevice8* dev)
{
	VOID* pVertices;

	// create vertex buffers
	// preview
	if (FAILED(dev->CreateVertexBuffer(sizeof(g_preview), D3DUSAGE_WRITEONLY, 
					D3DFVF_CUSTOMVERTEX2, D3DPOOL_MANAGED, &g_pVB_preview)))
	{
		Log(&k_stadium,"CreateVertexBuffer() failed.");
		return E_FAIL;
	}
	Log(&k_stadium,"CreateVertexBuffer() done.");

	if (FAILED(g_pVB_preview->Lock(0, sizeof(g_preview), (BYTE**)&pVertices, 0)))
	{
		Log(&k_stadium,"g_pVB_preview->Lock() failed.");
		return E_FAIL;
	}
	memcpy(pVertices, g_preview, sizeof(g_preview));
	SetPosition((CUSTOMVERTEX2*)pVertices, g_preview, sizeof(g_preview)/sizeof(CUSTOMVERTEX2), 
            512-64, 630);
	g_pVB_preview->Unlock();

	// preview outline
	if (FAILED(dev->CreateVertexBuffer(sizeof(g_preview_outline), D3DUSAGE_WRITEONLY, 
					D3DFVF_CUSTOMVERTEX2, D3DPOOL_MANAGED, &g_pVB_preview_outline)))
	{
		Log(&k_stadium,"CreateVertexBuffer() failed.");
		return E_FAIL;
	}
	Log(&k_stadium,"CreateVertexBuffer() done.");

	if (FAILED(g_pVB_preview_outline->Lock(0, sizeof(g_preview_outline), (BYTE**)&pVertices, 0)))
	{
		Log(&k_stadium,"g_pVB_preview_outline->Lock() failed.");
		return E_FAIL;
	}
	memcpy(pVertices, g_preview_outline, sizeof(g_preview_outline));
	SetPosition((CUSTOMVERTEX*)pVertices, g_preview_outline, sizeof(g_preview_outline)/sizeof(CUSTOMVERTEX), 
          512-65, 629);
	g_pVB_preview_outline->Unlock();

	// preview outline2
	if (FAILED(dev->CreateVertexBuffer(sizeof(g_preview_outline2), D3DUSAGE_WRITEONLY, 
					D3DFVF_CUSTOMVERTEX2, D3DPOOL_MANAGED, &g_pVB_preview_outline2)))
	{
		Log(&k_stadium,"CreateVertexBuffer() failed.");
		return E_FAIL;
	}
	Log(&k_stadium,"CreateVertexBuffer() done.");

	if (FAILED(g_pVB_preview_outline2->Lock(0, sizeof(g_preview_outline2), (BYTE**)&pVertices, 0)))
	{
		Log(&k_stadium,"g_pVB_preview_outline2->Lock() failed.");
		return E_FAIL;
	}
	memcpy(pVertices, g_preview_outline2, sizeof(g_preview_outline2));
	SetPosition((CUSTOMVERTEX*)pVertices, g_preview_outline2, sizeof(g_preview_outline2)/sizeof(CUSTOMVERTEX), 
            512-66, 628);
	g_pVB_preview_outline2->Unlock();

    return S_OK;
}

void DrawPreview(IDirect3DDevice8* dev)
{
    if (g_gameChoice) {
        return;
    }

	if (g_needsRestore) 
	{
		if (FAILED(RestoreDeviceObjects(dev)))
		{
			Log(&k_stadium,"DrawPreview: RestoreDeviceObjects() failed.");
            return;
		}
		Log(&k_stadium,"DrawPreview: RestoreDeviceObjects() done.");
        g_needsRestore = FALSE;
        D3DVIEWPORT8 vp;
        dev->GetViewport(&vp);
        LogWithNumber(&k_stadium,"VP: %d",vp.X);
        LogWithNumber(&k_stadium,"VP: %d",vp.Y);
        LogWithNumber(&k_stadium,"VP: %d",vp.Width);
        LogWithNumber(&k_stadium,"VP: %d",vp.Height);
        LogWithDouble(&k_stadium,"VP: %f",vp.MinZ);
        LogWithDouble(&k_stadium,"VP: %f",vp.MaxZ);
	}

    char* stadName = (char*)g_stadiumMapIterator->first.c_str();
    if (g_homeTeamChoice) {
        WORD teamId = GetTeamId(HOME);
        std::map<WORD,std::string>::iterator hit = g_HomeStadiumMap.find(teamId);
        if (hit !=  g_HomeStadiumMap.end()) {
            stadName = (char*)hit->second.c_str();

        } else {
            return;
        }
    }

    //print stadium information
    std::map<std::string,STADINFO*>::iterator it = g_stadiumMap.find(stadName);
    if (it != g_stadiumMap.end()) {
        char text[512] = {0};
        DWORD color = 0xffffffff; // white

        sprintf(text,"Built: %d",it->second->built);
        KDrawText(26,638,0xff000000,12,text);
        KDrawText(24,636,color,12,text);
        
        sprintf(text,"Capacity: %d",it->second->capacity);
        KDrawText(26,658,0xff000000,12,text);
        KDrawText(24,656,color,12,text);

        if (strlen(it->second->city)>0) {
            sprintf(text,"City: %s",it->second->city);
            KDrawText(26,678,0xff000000,12,text);
            KDrawText(24,676,color,12,text);
        }
    }

	// render
	dev->BeginScene();

	// setup renderstate
	//dev->CaptureStateBlock( g_dwSavedStateBlock );
	//dev->ApplyStateBlock( g_dwDrawOverlayStateBlock );
    
    if (!g_preview_tex && g_newStad) {
        char buf[2048];
        sprintf(buf, "%s\\GDB\\stadiums\\%s\\preview.png", GetPESInfo()->gdbDir, stadName);
        if (FAILED(D3DXCreateTextureFromFileEx(dev, buf, 
                    0, 0, 1, 0, D3DFMT_UNKNOWN, D3DPOOL_MANAGED,
                    D3DX_FILTER_NONE, D3DX_FILTER_NONE,
                    0, NULL, NULL, &g_preview_tex))) {
            // try "preview.bmp"
            sprintf(buf, "%s\\GDB\\stadiums\\%s\\preview.bmp", GetPESInfo()->gdbDir, stadName);
            if (FAILED(D3DXCreateTextureFromFileEx(dev, buf, 
                        0, 0, 1, 0, D3DFMT_UNKNOWN, D3DPOOL_MANAGED,
                        D3DX_FILTER_NONE, D3DX_FILTER_NONE,
                        0, NULL, NULL, &g_preview_tex))) {
                Log(&k_stadium,"FAILED to load image for stadium preview.");
            }
        }
        g_newStad = false;
    }
    if (g_preview_tex) {
        // outline
        dev->SetVertexShader( D3DFVF_CUSTOMVERTEX );
        dev->SetTexture(0, NULL);
        dev->SetStreamSource( 0, g_pVB_preview_outline2, sizeof(CUSTOMVERTEX));
        dev->DrawPrimitive( D3DPT_TRIANGLESTRIP, 0, 2);
        dev->SetStreamSource( 0, g_pVB_preview_outline, sizeof(CUSTOMVERTEX));
        dev->DrawPrimitive( D3DPT_TRIANGLESTRIP, 0, 2);

        // texture
        dev->SetVertexShader( D3DFVF_CUSTOMVERTEX2 );
        dev->SetTexture(0, g_preview_tex);
        dev->SetStreamSource( 0, g_pVB_preview, sizeof(CUSTOMVERTEX2));
        dev->DrawPrimitive( D3DPT_TRIANGLESTRIP, 0, 2);
    }

	// restore the modified renderstates
	//dev->ApplyStateBlock( g_dwSavedStateBlock );

	dev->EndScene();
}

void DeleteStateBlocks(IDirect3DDevice8* dev)
{
	// Delete the state blocks
	try
	{
        //LogWithNumber(&k_stadium,"dev = %08x", (DWORD)dev);
        DWORD* vtab = (DWORD*)(*(DWORD*)dev);
        //LogWithNumber(&k_stadium,"vtab = %08x", (DWORD)vtab);
        if (vtab && vtab[VTAB_DELETESTATEBLOCK]) {
            //LogWithNumber(&k_stadium,"vtab[VTAB_DELETESTATEBLOCK] = %08x", (DWORD)vtab[VTAB_DELETESTATEBLOCK]);
            if (g_dwSavedStateBlock) {
                dev->DeleteStateBlock( g_dwSavedStateBlock );
                Log(&k_stadium,"g_dwSavedStateBlock deleted.");
            }
            if (g_dwDrawOverlayStateBlock) {
                dev->DeleteStateBlock( g_dwDrawOverlayStateBlock );
                Log(&k_stadium,"g_dwDrawOverlayStateBlock deleted.");
            }
        }
	}
	catch (...)
	{
        // problem deleting state block
	}

	g_dwSavedStateBlock = 0L;
	g_dwDrawOverlayStateBlock = 0L;
}

//-----------------------------------------------------------------------------
// Name: InvalidateDeviceObjects()
// Desc: Destroys all device-dependent objects
//-----------------------------------------------------------------------------
HRESULT InvalidateDeviceObjects(IDirect3DDevice8* dev)
{
	TRACE(&k_stadium,"InvalidateDeviceObjects called.");
	if (dev == NULL)
	{
		TRACE(&k_stadium,"InvalidateDeviceObjects: nothing to invalidate.");
		return S_OK;
	}

    // stadium preview
	SafeRelease( &g_pVB_preview );
	SafeRelease( &g_pVB_preview_outline );
	SafeRelease( &g_pVB_preview_outline2 );

	Log(&k_stadium,"InvalidateDeviceObjects: SafeRelease(s) done.");

    DeleteStateBlocks(dev);
    Log(&k_stadium,"InvalidateDeviceObjects: DeleteStateBlock(s) done.");
    return S_OK;
}

//-----------------------------------------------------------------------------
// Name: DeleteDeviceObjects()
// Desc: Destroys all device-dependent objects
//-----------------------------------------------------------------------------
HRESULT DeleteDeviceObjects(IDirect3DDevice8* dev)
{
    return S_OK;
}

//-----------------------------------------------------------------------------
// Name: RestoreDeviceObjects()
// Desc:
//-----------------------------------------------------------------------------
HRESULT RestoreDeviceObjects(IDirect3DDevice8* dev)
{
    HRESULT hr = InitVB(dev);
    if (FAILED(hr))
    {
		Log(&k_stadium,"InitVB() failed.");
        return hr;
    }
	Log(&k_stadium,"InitVB() done.");

	// Create the state blocks for rendering overlay graphics
	for( UINT which=0; which<2; which++ )
	{
		dev->BeginStateBlock();

        dev->SetTextureStageState( 0, D3DTSS_COLOROP, D3DTOP_SELECTARG1 );
        dev->SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_TEXTURE );
        dev->SetTextureStageState( 0, D3DTSS_ALPHAOP, D3DTOP_DISABLE );
        dev->SetVertexShader( D3DFVF_CUSTOMVERTEX2 );
        dev->SetTexture( 0, g_preview_tex );

		if( which==0 )
			dev->EndStateBlock( &g_dwSavedStateBlock );
		else
			dev->EndStateBlock( &g_dwDrawOverlayStateBlock );
	}

    return S_OK;
}

/**
 * Used in "View Stadiums" mode
 */
DWORD stadGetString(DWORD param, DWORD pText)
{
    Log(&k_stadium, "stadGetString CALLED.");
    char* text = (char*)pText;
	if (viewGdbStadiums) {
        if (!needsReload) {
            //advance iterator
            g_stadiumMapIterator++;
            if (g_stadiumMapIterator == g_stadiumMap.end()) {
                g_stadiumMapIterator = g_stadiumMap.begin();
            }
        }
        text = (char*)g_stadiumMapIterator->first.c_str();
    }
    LogWithString(&k_stadium, "stadGetString: text = %s", text);

    // call the original
    pText = (DWORD)text;
    DWORD result = MasterCallNext(param, pText);

    return result;
}

DWORD stadGetString2(DWORD ppText, DWORD p1, DWORD p2, DWORD p3, 
        DWORD p4, DWORD p5, DWORD p6, DWORD p7)
{
    Log(&k_stadium, "stadGetString2 CALLED.");
    LogWithString(&k_stadium, "stadGetString2: ppText = %s", *(char**)ppText);
    char* text = *(char**)ppText;
    strcpy(g_stadiumText, text);

    if (g_homeTeamChoice) {
        WORD teamId = GetTeamId(HOME);
        LogWithNumber(&k_stadium, "stadGetString2: home team = %d", teamId);
        std::map<WORD,std::string>::iterator it = g_HomeStadiumMap.find(teamId);
        if (it != g_HomeStadiumMap.end()) {
            LogWithString(&k_stadium, "stadGetString2: has a home stadium: %s", 
                    (char*)it->second.c_str());
            strcpy(text, (char*)it->second.c_str());
        }
    } else if (!g_gameChoice) {
        strcpy(text, g_stadiumMapIterator->first.c_str());
    }
    LogWithString(&k_stadium, "stadGetString2: ppText = %s", *(char**)ppText);

    // call the original
    DWORD result = MasterCallNext(ppText, p1,p2,p3,p4,p5,p6,p7);

    strcpy(text, g_stadiumText);
    //LogWithString(&k_stadium, "stadGetString2: ppText = %s", *(char**)ppText);

    return result;
}

DWORD stadWriteBuilt(char* dest, char* format, DWORD num)
{
    Log(&k_stadium, "stadWriteBuilt CALLED.");
    if (viewGdbStadiums) {
        num = g_stadiumMapIterator->second->built;
    }

    // call the original
    DWORD result = MasterCallNext(dest, format, num);

    return result;
}

DWORD stadWriteCapacity(char* dest, char* format, DWORD num)
{
    Log(&k_stadium, "stadWriteCapacity CALLED.");
    if (viewGdbStadiums) {
        num = g_stadiumMapIterator->second->capacity;
    }

    // call the original
    DWORD result = MasterCallNext(dest, format, num);

	if (needsReload) {
		//time to write the commands back
		memcpy((BYTE*)code[C_STADIUMCHANGE],savedStadiumChange,6);
		memcpy((BYTE*)code[C_ADVANCESTADIUM],savedAdvanceStadium,5);
		needsReload=false;
	};

	CheckViewStadiumMode();
    
    return result;
}

void CheckViewStadiumMode()
{	
	if (dta[ISVIEWSTADIUMMODE]!=0 && *(BYTE*)dta[ISVIEWSTADIUMMODE] != 0) {
		if (!isViewStadiumMode) {
			//reset parameters if reentering View Stadium mode
			viewGdbStadiums=false;
			weather=0;
			g_stadiumMapIterator = g_stadiumMap.begin();
		};
		isViewStadiumMode=true;
	} else
		isViewStadiumMode=false;
		
	return;
};

DWORD stadReadLCM(DWORD p1, DWORD p2, DWORD p3, DWORD p4)
{
    //Log(&k_stadium, "stadReadLCM: CALLED");

    // avoid Club House
	LCM* lcm=(LCM*)dta[TEAM_IDS];
    if (!g_gameChoice) {
        LogWithNumber(&k_stadium, "stadReadLCM: stadium = %d", lcm->stadium);
        if (lcm->stadium == CLUB_HOUSE) {
            lcm->stadium += 1;
        }
        Log(&k_stadium, "stadReadLCM: scrolling Club House");
    }

    return MasterCallNext(p1, p2, p3, p4);
}
	
DWORD stadSetLCM(DWORD p1)
{
    //Log(&k_stadium, "stadSetLCM: CALLED");
	stadEndUniSelect();

	LCM* lcm=(LCM*)dta[TEAM_IDS];
	
    DWORD result = MasterCallNext(p1);

    // avoid Club House
    if (!g_gameChoice) {
        LogWithNumber(&k_stadium, "stadSetLCM: stadium = %d", lcm->stadium);
        if (lcm->stadium == CLUB_HOUSE) {
            lcm->stadium += 1;
        }
        Log(&k_stadium, "stadSetLCM: scrolling Club House");
    }

	lcm->effects=1;
	
	if (weather==0) {
        return result;
    }
	
	//day or night
	if (weather<4)
		lcm->timeOfDay=0;
	else
		lcm->timeOfDay=1;
		
	//weather
	lcm->weather = (weather-1) % 3;
	
	//if snow, then we need winter, else summer
	if (lcm->weather==2)
		lcm->season=1;
	else
		lcm->season=0;
	
	//doesn't work
	//lcm->crowdStance=1;
	//lcm->homeCrowd=1;
	
	return result;
};
