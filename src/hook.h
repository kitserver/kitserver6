// hook.h
#ifndef _HOOK_
#define _HOOK_

#include "d3dfont.h"
#include "dxutil.h"
#include "d3d8types.h"
#include "d3d8.h"
#include "d3dx8tex.h"

//#define KeyNextMenu VK_PRIOR
//#define KeyPrevMenu VK_NEXT
#define VERSION_JUCE 0
#define VERSION_ROBBIE 1

typedef struct _CALLLINE {
	DWORD num;
	DWORD* addr;
} CALLLINE;

typedef struct _PLAYER_RECORD {
	BYTE number;
	BYTE unknown1[3];
	DWORD* texMain;
	BYTE unknown2[6];
	WORD playerId;
	BYTE formOrientation; //something with the formation
	BYTE posInTeam;
	BYTE team;
	BYTE unknown3[0x240-19];
} PLAYER_RECORD;

typedef struct _TEXTURE_INFO {
	DWORD id;
	WORD refCount;
	WORD dummy1;	//not used
	IDirect3DTexture8* tex;
	DWORD unknown1;
	DWORD unknown2;
	BYTE unknown3;
	BYTE unknown4;
	BYTE unknown5;	//set to 1
	BYTE unknown6;
	DWORD unknown7;
	DWORD unknown8;
	DWORD unknown9;	//set to 0->tex is always returned by PesGetTexture
	DWORD unknown10;
	DWORD unknown11;
	BYTE unknown12;
	BYTE unknown13;
	WORD dummy2;	//not used
	DWORD unknown14;
	DWORD unknown15;
} TEXTURE_INFO;

#define VTAB_GETDEVICECAPS 13
#define VTAB_CREATEDEVICE 15
#define VTAB_RESET 14
#define VTAB_PRESENT 15
#define VTAB_CREATETEXTURE 20
#define VTAB_SETRENDERSTATE 50
#define VTAB_SETTEXTURE 61
#define VTAB_DELETESTATEBLOCK 56
#define VTAB_UNLOCKRECT 17

typedef void (*PFNREADFILE)(HANDLE hFile, LPVOID lpBuffer, DWORD nNumberOfBytesToRead,
  LPDWORD lpNumberOfBytesRead, LPOVERLAPPED lpOverlapped);

typedef IDirect3D8* (STDMETHODCALLTYPE *PFNDIRECT3DCREATE8PROC)(UINT sdkVersion);

/* IDirect3DDevice8 method-types */
typedef HRESULT (STDMETHODCALLTYPE *PFNGETDEVICECAPSPROC)(IDirect3D8* self, UINT Adapter,
    D3DDEVTYPE DeviceType, D3DCAPS8* pCaps);

typedef HRESULT (STDMETHODCALLTYPE *PFNCREATEDEVICEPROC)(IDirect3D8* self, UINT Adapter,
    D3DDEVTYPE DeviceType, HWND hFocusWindow, DWORD BehaviorFlags,
    D3DPRESENT_PARAMETERS *pPresentationParameters, 
    IDirect3DDevice8** ppReturnedDeviceInterface);

typedef HRESULT (STDMETHODCALLTYPE *PFNCREATETEXTUREPROC)(IDirect3DDevice8* self, 
UINT width, UINT height, UINT levels, DWORD usage, D3DFORMAT format, D3DPOOL pool, 
IDirect3DTexture8** ppTexture);

typedef HRESULT (STDMETHODCALLTYPE *PFNSETRENDERSTATEPROC)(IDirect3DDevice8* self, 
D3DRENDERSTATETYPE State, DWORD Value);

typedef HRESULT (STDMETHODCALLTYPE *PFNSETTEXTURESTAGESTATEPROC)(IDirect3DDevice8* self, 
DWORD Stage, D3DTEXTURESTAGESTATETYPE Type, DWORD Value);

typedef HRESULT (STDMETHODCALLTYPE *PFNSETTEXTUREPROC)(IDirect3DDevice8* self, 
DWORD stage, IDirect3DBaseTexture8* pTexture);

typedef HRESULT (STDMETHODCALLTYPE *PFNUPDATETEXTUREPROC)(IDirect3DDevice8* self,
IDirect3DBaseTexture8* pSrc, IDirect3DBaseTexture8* pDest);

typedef HRESULT (STDMETHODCALLTYPE *PFNPRESENTPROC)(IDirect3DDevice8* self, 
CONST RECT* src, CONST RECT* dest, HWND hWnd, LPVOID unused);

typedef HRESULT (STDMETHODCALLTYPE *PFNRESETPROC)(IDirect3DDevice8* self, LPVOID);

typedef HRESULT (STDMETHODCALLTYPE *PFNCOPYRECTSPROC)(IDirect3DDevice8* self,
IDirect3DSurface8* pSourceSurface, CONST RECT* pSourceRectsArray, UINT cRects,
IDirect3DSurface8* pDestinationSurface, CONST POINT* pDestPointsArray);

typedef HRESULT (STDMETHODCALLTYPE *PFNAPPLYSTATEBLOCKPROC)(IDirect3DDevice8* self, DWORD token);

typedef HRESULT (STDMETHODCALLTYPE *PFNBEGINSCENEPROC)(IDirect3DDevice8* self);

typedef HRESULT (STDMETHODCALLTYPE *PFNENDSCENEPROC)(IDirect3DDevice8* self);

typedef HRESULT (STDMETHODCALLTYPE *PFNGETSURFACELEVELPROC)(IDirect3DTexture8* self,
UINT level, IDirect3DSurface8** ppSurfaceLevel);

typedef HRESULT (STDMETHODCALLTYPE *PFNUNLOCKRECT)(IDirect3DTexture8* self, UINT Level);


typedef DWORD  (*UNIDECRYPT)(DWORD, DWORD);
typedef DWORD  (*UNIDECODE)(DWORD, DWORD);
typedef void   (*CUNIDECODE)(DWORD, DWORD, DWORD);
typedef DWORD  (*UNPACK)(DWORD, DWORD, DWORD, DWORD, DWORD*);
typedef void   (*CUNPACK)(DWORD, DWORD, DWORD, DWORD, DWORD*, DWORD);
typedef DWORD  (*CHECKTEAM)(DWORD);
typedef DWORD  (*SETKITATTRIBUTES)(DWORD);
typedef DWORD  (*FINISHKITPICK)(DWORD);
typedef DWORD  (*ALLOCMEM)(DWORD, DWORD, DWORD);
typedef DWORD  (*RESETFLAGS)(DWORD);
typedef DWORD  (*GETTEAMINFO)(DWORD);
typedef void   (*CGETTEAMINFO)(DWORD,DWORD);
typedef DWORD  (*BEGINUNISELECT)();
typedef DWORD  (*ENDUNISELECT)();
typedef DWORD  (*SETLODMIXERDATA)();
typedef DWORD  (*GETPLAYERINFO_OLD)();
typedef void   (*CGETPLAYERINFO_OLD)(DWORD,DWORD*,DWORD,DWORD*);
typedef DWORD  (*ALLOCMEM)(DWORD,DWORD,DWORD);
typedef bool   (*CALLOCMEM)(DWORD,DWORD,DWORD*);
typedef HRESULT (STDMETHODCALLTYPE *CCREATETEXTURE)(IDirect3DDevice8*,UINT,UINT,UINT,DWORD,D3DFORMAT,
				 D3DPOOL,IDirect3DTexture8**,DWORD,bool*);
typedef void   (*FILEFROMAFS)(DWORD);
typedef void   (*FREEMEMORY)(DWORD);
typedef bool   (*CFREEMEMORY)(DWORD);
typedef void   (*CALCAFSFACEID)(DWORD,DWORD,DWORD*);
typedef void   (*PROCESSPLAYERDATA)(DWORD,DWORD*);
typedef void   (*DRAWKITSELECTINFO)(IDirect3DDevice8*);
typedef void   (*CINPUT)(int,WPARAM,LPARAM);
typedef DWORD  (*UNISPLIT)(DWORD);
typedef void   (*CUNISPLIT)(DWORD,DWORD,DWORD);
typedef void   (*UNLOCKRECT)(IDirect3DTexture8*,UINT);
typedef IDirect3DTexture8*  (STDMETHODCALLTYPE *PES_GETTEXTURE)(DWORD);
typedef void   (*CPES_GETTEXTURE)(DWORD,DWORD,DWORD,IDirect3DTexture8**);
typedef void   (*CBEGINRENDERPLAYER)(DWORD);
typedef void   (*ALLVOID)();

void HookDirect3DCreate8();
void HookReadFile();
void UnhookReadFile();
void HookOthers();
void UnhookOthers();
void UnhookKeyb();
bool HookProc(DWORD proc, DWORD proc_cs, DWORD newproc, char* sproc, char* sproc_cs);
bool UnhookProc(bool flag, DWORD proc, DWORD proc_cs, char* sproc, char* sproc_cs);
bool HookProcAtAddr(DWORD proc, DWORD proc_cs, DWORD newproc, char* sproc, char* sproc_cs);
bool UnhookProcAtAddr(bool flag, DWORD proc, DWORD proc_cs, char* sproc, char* sproc_cs);
void FixReservedMemory();

DWORD NewGetClubTeamInfo(DWORD id);
DWORD NewGetClubTeamInfoML1(DWORD id);
DWORD NewGetClubTeamInfoML2(DWORD id);
DWORD NewGetNationalTeamInfo(DWORD id);
DWORD NewGetNationalTeamInfoExitEdit(DWORD id);
DWORD NewAllocMem(DWORD infoBlock, DWORD param2, DWORD size);
DWORD NewUnpack(DWORD addr1, DWORD addr2, DWORD size1, DWORD zero, DWORD* size2);
KEXPORT DWORD MemUnpack(DWORD addr1, DWORD addr2, DWORD size1, DWORD* size2);
KEXPORT DWORD AFSMemUnpack(DWORD FileID, DWORD Buffer);
DWORD NewUniDecode(DWORD addr, DWORD size);
DWORD NewUniSplit(DWORD id);
DWORD NewBeginUniSelect();
DWORD NewEndUniSelect();
DWORD NewSetLodMixerData(DWORD dummy);
DWORD NewGetPlayerInfoOld();
KEXPORT DWORD GetPlayerInfo(DWORD PlayerNumber,DWORD Mode);
void NewFileFromAFS(DWORD retAddr, DWORD infoBlock);
void NewFreeMemory(DWORD addr);
void NewProcessPlayerData();
IDirect3DTexture8* STDMETHODCALLTYPE NewPesGetTexture(DWORD p1);
void NewBeginRenderPlayer();
KEXPORT bool isEditMode();
KEXPORT DWORD editPlayerId();
KEXPORT bool isTrainingMode();
KEXPORT bool isWatchReplayMode();
KEXPORT bool isMLMode();
KEXPORT PLAYER_RECORD* playerRecord(BYTE pos);
KEXPORT DWORD getRecordPlayerId(BYTE pos);
KEXPORT IDirect3DTexture8* GetTextureFromColl(DWORD texColl, DWORD which);
KEXPORT void SetTextureToColl(DWORD texColl, DWORD which, IDirect3DTexture8* tex);
KEXPORT IDirect3DTexture8* GetPlayerTexture(DWORD playerPos, DWORD texCollType, DWORD which, DWORD lodLevel);

BOOL STDMETHODCALLTYPE NewReadFile(HANDLE hFile, LPVOID lpBuffer, DWORD nNumberOfBytesToRead,
  LPDWORD lpNumberOfBytesRead,
  LPOVERLAPPED lpOverlapped);
  
IDirect3D8* STDMETHODCALLTYPE NewDirect3DCreate8(UINT sdkVersion);

HRESULT STDMETHODCALLTYPE NewGetDeviceCaps(IDirect3D8* self, UINT Adapter,
    D3DDEVTYPE DeviceType, D3DCAPS8* pCaps);

HRESULT STDMETHODCALLTYPE NewCreateDevice(IDirect3D8* self, UINT Adapter,
    D3DDEVTYPE DeviceType, HWND hFocusWindow, DWORD BehaviorFlags,
    D3DPRESENT_PARAMETERS *pPresentationParameters, 
    IDirect3DDevice8** ppReturnedDeviceInterface);

KEXPORT IDirect3DDevice8* GetActiveDevice();

KEXPORT void SetActiveDevice(IDirect3DDevice8* n_device);

void kloadRestoreDeviceObjects(IDirect3DDevice8* dev);
void kloadInvalidateDeviceObjects(IDirect3DDevice8* dev);
void kloadDeleteDeviceObjects(IDirect3DDevice8* dev);
void kloadGetBackBufferInfo(IDirect3DDevice8* d3dDevice);
CD3DFont* GetFont(DWORD fontSize);
KEXPORT void KDrawTextW(FLOAT x,FLOAT y,DWORD dwColor,DWORD fontSize,WCHAR* strText,bool absolute=false);
KEXPORT void KDrawText(FLOAT x,FLOAT y,DWORD dwColor,DWORD fontSize,TCHAR* strText,bool absolute=false);
KEXPORT void KGetTextExtentW(WCHAR* strText,DWORD fontSize,SIZE* pSize);
KEXPORT void KGetTextExtent(TCHAR* strText,DWORD fontSize,SIZE* pSize);

HRESULT STDMETHODCALLTYPE NewCreateTexture(IDirect3DDevice8* self, UINT width, UINT height,
UINT levels, DWORD usage, D3DFORMAT format, D3DPOOL pool, IDirect3DTexture8** ppTexture);

KEXPORT HRESULT STDMETHODCALLTYPE OrgCreateTexture(IDirect3DDevice8* self, UINT width, UINT height,
UINT levels, DWORD usage, D3DFORMAT format, D3DPOOL pool, IDirect3DTexture8** ppTexture);

HRESULT STDMETHODCALLTYPE NewPresent(IDirect3DDevice8* self, CONST RECT* src, CONST RECT* dest,
	HWND hWnd, LPVOID unused);
	
HRESULT STDMETHODCALLTYPE NewReset(IDirect3DDevice8* self, LPVOID params);

HRESULT STDMETHODCALLTYPE NewSetTexture(IDirect3DDevice8* self, DWORD stage,
IDirect3DBaseTexture8* pTexture);

HRESULT STDMETHODCALLTYPE NewSetRenderState(IDirect3DDevice8* self,D3DRENDERSTATETYPE State,DWORD Value);

HRESULT STDMETHODCALLTYPE NewUnlockRect(IDirect3DTexture8* self,UINT Level);

void DrawKitSelectInfo();

KEXPORT void dksiSetMenuTitle(char* newTitle);

LRESULT CALLBACK KeyboardProc(int code1, WPARAM wParam, LPARAM lParam);

void SetNewDrawKitInfoMenu(int NewMenu, bool ForceShowMenuFunc);

KEXPORT BYTE GetLCMStadium();
KEXPORT void SetLCMStadium(BYTE newStadium);

void AddToLine(CALLLINE* cl,DWORD addr);
void RemoveFromLine(CALLLINE* cl,DWORD addr);
void ClearLine(CALLLINE* cl);
void BiggerLine(CALLLINE* cl);

enum HOOKS {
	hk_D3D_Create,
	hk_D3D_GetDeviceCaps,
	hk_D3D_CreateDevice,
	hk_D3D_Present,
	hk_D3D_Reset,
	hk_D3D_CreateTexture,
	hk_D3D_AfterCreateTexture,
	hk_ReadFile,
	hk_BeginUniSelect,
	hk_EndUniSelect,
	hk_Unpack,
	hk_UniDecode,
	hk_GetClubTeamInfo,
	hk_GetNationalTeamInfo,
	hk_GetClubTeamInfoML1,
	hk_GetClubTeamInfoML2,
	hk_GetNationalTeamInfoExitEdit,
	hk_AllocMem,
	hk_SetLodMixerData,
	hk_GetPlayerInfoOld,
	hk_BeforeUniDecode,
	hk_FileFromAFS,
	hk_BeforeFreeMemory,
	hk_CalcAFSFaceID,
	hk_ProcessPlayerData,
	hk_DrawKitSelectInfo,
	hk_Input,
	hk_OnShowMenu,
	hk_OnHideMenu,
	hk_UniSplit,
	hk_AfterReadFile,
	hk_D3D_UnlockRect,
	hk_PesGetTexture,
	hk_BeginRenderPlayer,
};

KEXPORT void HookFunction(HOOKS h,DWORD addr);
KEXPORT void UnhookFunction(HOOKS h,DWORD addr);
CALLLINE* LineFromID(HOOKS h);

void InitAddresses(int v);
KEXPORT void SetBootserverVersion(int version);

#endif
