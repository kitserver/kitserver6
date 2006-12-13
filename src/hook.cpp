// hook.cpp

#include <windows.h>
#include <stdio.h>
#include "log.h"
#include "manage.h"
#include "hook.h"
#include "kload.h"
#include "kload_config.h"
#include "numpages.h"
#include "input.h"
#include "keycfg.h"

extern KMOD k_kload;
extern PESINFO g_pesinfo;
extern KLOAD_CONFIG g_config;

#define CODELEN 46

enum {
	C_UNIDECODE, C_UNIDECODE_CS,C_UNIDECODE_CS2,
	C_UNPACK, C_UNPACK_CS,
	C_ALLOCMEM, C_ALLOCMEM_CS,
	C_READFILE_CS, C_GETCLUBTEAMINFO, C_GETNATIONALTEAMINFO, 
	C_GETCLUBTEAMINFO_CS, C_GETNATIONALTEAMINFO_CS,
	C_GETCLUBTEAMINFO_CS2, C_GETNATIONALTEAMINFO_CS2,
	C_GETCLUBTEAMINFO_CS_ML1, C_GETNATIONALTEAMINFO_CS_EXIT_EDIT,
	C_GETCLUBTEAMINFO_CS_ML2, C_GETCLUBTEAMINFO_CS3,
    C_BEGINUNISELECT, C_BEGINUNISELECT_CS,
    C_ENDUNISELECT, C_ENDUNISELECT_CS,
    C_LODMIXER_HOOK_ORG, C_LODMIXER_HOOK_CS,
    C_LODMIXER_HOOK_ORG2, C_LODMIXER_HOOK_CS2,
    C_GETPLAYERINFO, C_GETPLAYERINFO_JMP,
    C_FILEFROMAFS, C_FREEMEMORY, C_FREEMEMORY_JMP,
    C_PROCESSPLAYERDATA_JMP, C_CALL050,
    C_UNISPLIT, C_UNISPLIT_CS1, C_UNISPLIT_CS2,
    C_UNISPLIT_CS3, C_UNISPLIT_CS4, C_UNISPLIT_CS5,
    C_UNISPLIT_CS6, C_UNISPLIT_CS7, C_UNISPLIT_CS8,
    C_GETTEAMINFO, C_GETTEAMINFO_CS,
    C_CLEANINPUTTABLE_HOOK,
    C_FILEFROMAFS_JUMPHACK,
};

// Code addresses.
DWORD codeArray[][CODELEN] = { 
    // PES6
	{ 0x8cd500, 0x8b1983, 0,
	  0x8cd490, 0x65b176,
	  0, 0,
	  0x44c014, 0, 0, //maybe 0x865240 (0x9690a5), 0x8654d0 (0x9690b8)
	  0, 0,
	  0, 0,
	  0, 0, 
	  0, 0,
      0x9cfeb0, 0x9d078c,
      0x9ed490, 0x9cfd27,
      0x6d3b60, 0x6b8099,
      0, 0,
      0, 0,
      0x65b668, 0x876f20, 0x45bc5c,
      0x5fe506, 0x40c848,
      0, 0, 0,
      0, 0, 0,
      0, 0, 0,
      0x865240, 0x804cda,
      0x9cd4f2,
      0
    },
    // PES6 1.10
	{ 0x8cd5f0, 0x8b1a63, 0,
	  0x8cd580, 0x65b216,
	  0, 0,
	  0x44c064, 0, 0, 
	  0, 0,
	  0, 0,
	  0, 0, 
	  0, 0,
      0x9d0040, 0x9d091c,
      0x9ed5f0, 0x9cfeb7,
      0x6d3cc0, 0x6b81b9,
      0, 0,
      0, 0,
      0x65b8a7, 0x877060, 0x45bc9c,
      0x5fe566, 0x40c898,
      0, 0, 0,
      0, 0, 0,
      0, 0, 0,
      0x865370, 0x804e5a,
      0x9cd682,
      0x65b85b,
    },
};

// data addresses

#define DATALEN 2
enum { INPUT_TABLE, STACK_SHIFT };
DWORD dataArray[][DATALEN] = {
    //PES6
    {
        0x3a71254, 0,
    },
    //PES6 1.10
    {
        0x3a72254, 0x2c,
    },
};

BYTE _shortJumpHack[][2] = {
    //PES6
    {0,0},
    //PES6 1.10
    {0xeb,0x4a},
};

#define GPILEN 19

DWORD gpiArray[][GPILEN] = {
	//PES6
	{
		0,0,0,0,0,
		0,0,0,0,0,
		0,0,0,0,0,
		0,0,0,0
	},
	//PES6 1.10
	{
		0,0,0,0,0,
		0,0,0,0,0,
		0,0,0,0,0,
		0,0,0,0
	},
};

DWORD code[CODELEN];
DWORD data[DATALEN];
DWORD gpi[GPILEN];

typedef struct _ENCBUFFERHEADER {
	DWORD dwSig;
	DWORD dwEncSize;
	DWORD dwDecSize;
	BYTE other[20];
} ENCBUFFERHEADER;

/* function pointers */
PFNGETDEVICECAPSPROC g_orgGetDeviceCaps = NULL;
PFNCREATEDEVICEPROC g_orgCreateDevice = NULL;
PFNCREATETEXTUREPROC g_orgCreateTexture = NULL;
PFNSETRENDERSTATEPROC g_orgSetRenderState = NULL;
PFNSETTEXTUREPROC g_orgSetTexture = NULL;
PFNSETTEXTURESTAGESTATEPROC g_orgSetTextureStageState = NULL;
PFNPRESENTPROC g_orgPresent = NULL;
PFNRESETPROC g_orgReset = NULL;
PFNCOPYRECTSPROC g_orgCopyRects = NULL;
PFNAPPLYSTATEBLOCKPROC g_orgApplyStateBlock = NULL;
PFNBEGINSCENEPROC g_orgBeginScene = NULL;
PFNENDSCENEPROC g_orgEndScene = NULL;
PFNGETSURFACELEVELPROC g_orgGetSurfaceLevel = NULL;
PFNUPDATETEXTUREPROC g_orgUpdateTexture = NULL;
PFNDIRECT3DCREATE8PROC g_orgDirect3DCreate8 = NULL;
PFNUNLOCKRECT g_orgUnlockRect = NULL;

HHOOK g_hKeyboardHook=NULL;
extern HINSTANCE hInst;

BOOL g_bGotFormat = false;
UINT g_bbWidth = 0;
UINT g_bbHeight = 0;
double stretchX=1;
double stretchY=1;
DWORD g_frame_tex_count=0;
IDirect3DBaseTexture8* g_lastTexture=NULL;
BOOL g_needsRestore = TRUE;
bool g_fontInitialized = false;
CD3DFont* g_font12 = NULL;
CD3DFont* g_font16 = NULL;
CD3DFont* g_font20 = NULL;

BYTE g_codeFragment[5] = {0,0,0,0,0};
BYTE g_rfCode[6];
BYTE g_gpiJmpCode[4];
BYTE g_FreeMemoryJmpCode[4];
DWORD g_savedProtection = 0;
IDirect3DDevice8* g_device = NULL;
BYTE lcmStadium=0xff;

bool IsKitSelectMode=false;
int usedKitInfoMenu=0;
DWORD numKitInfoMenus=0;
char menuTitle[BUFLEN]="\0";
DWORD lastAddedDrawKitSelectInfo=0xFFFFFFFF;
DWORD* OnShowMenuFuncs=NULL;
DWORD* OnHideMenuFuncs=NULL;

bool bUniDecodeHooked = false;
bool bUnpackHooked = false;
bool bReadFileHooked = false;
bool bGetNationalTeamInfoHooked = false;
bool bGetNationalTeamInfoExitEditHooked = false;
bool bGetClubTeamInfoHooked = false;
bool bBeginUniSelectHooked = false;
bool bEndUniSelectHooked = false;
bool bAllocMemHooked=false;
bool bSetLodMixerDataHooked = false;
bool bGetPlayerInfoHooked = false;
bool bGetPlayerInfoJmpHooked = false;
bool bFileFromAFSHooked = false;
bool bFileFromAFSJumpHackHooked = false;
bool bFreeMemoryHooked = false;
bool bProcessPlayerDataHooked = false;
bool bUniSplitHooked = false;

UNIDECODE UniDecode = NULL;
UNPACK Unpack = NULL;
GETTEAMINFO GetNationalTeamInfo = NULL;
GETTEAMINFO GetClubTeamInfo = NULL;
BEGINUNISELECT BeginUniSelect = NULL;
ENDUNISELECT EndUniSelect = NULL;
ALLOCMEM AllocMem=NULL;
SETLODMIXERDATA SetLodMixerData = NULL;
SETLODMIXERDATA SetLodMixerData2 = NULL;
GETPLAYERINFO oGetPlayerInfo = NULL;
FREEMEMORY oFreeMemory = NULL;
UNISPLIT UniSplit = NULL;

CALLLINE l_D3D_Create={0,NULL};
CALLLINE l_D3D_GetDeviceCaps={0,NULL};
CALLLINE l_D3D_CreateDevice={0,NULL};
CALLLINE l_D3D_Present={0,NULL};
CALLLINE l_D3D_Reset={0,NULL};
CALLLINE l_D3D_CreateTexture={0,NULL};
CALLLINE l_D3D_AfterCreateTexture={0,NULL};
CALLLINE l_ReadFile={0,NULL};
CALLLINE l_BeginUniSelect={0,NULL};
CALLLINE l_EndUniSelect={0,NULL};
CALLLINE l_Unpack={0,NULL};
CALLLINE l_UniDecode={0,NULL};
CALLLINE l_GetClubTeamInfo={0,NULL};
CALLLINE l_GetNationalTeamInfo={0,NULL};
CALLLINE l_GetClubTeamInfoML1={0,NULL};
CALLLINE l_GetClubTeamInfoML2={0,NULL};
CALLLINE l_GetNationalTeamInfoExitEdit={0,NULL};
CALLLINE l_AllocMem={0,NULL};
CALLLINE l_SetLodMixerData={0,NULL};
CALLLINE l_GetPlayerInfo={0,NULL};
CALLLINE l_BeforeUniDecode={0,NULL};
CALLLINE l_FileFromAFS={0,NULL};
CALLLINE l_BeforeFreeMemory={0,NULL};
CALLLINE l_ProcessPlayerData={0,NULL};
CALLLINE l_DrawKitSelectInfo={0,NULL};
CALLLINE l_Input={0,NULL};
CALLLINE l_OnShowMenu={0,NULL};
CALLLINE l_OnHideMenu={0,NULL};
CALLLINE l_UniSplit={0,NULL};
CALLLINE l_AfterReadFile={0,NULL};
CALLLINE l_D3D_UnlockRect={0,NULL};

void HookDirect3DCreate8()
{
	BYTE g_jmp[5] = {0,0,0,0,0};
	// Put a JMP-hook on Direct3DCreate8
	Log(&k_kload,"JMP-hooking Direct3DCreate8...");
	HMODULE hD3D8 = GetModuleHandle("d3d8");
	if (!hD3D8) {
	    hD3D8 = LoadLibrary("d3d8");
	}
	if (hD3D8) {
		g_orgDirect3DCreate8 = (PFNDIRECT3DCREATE8PROC)GetProcAddress(hD3D8, "Direct3DCreate8");
		
		// unconditional JMP to relative address is 5 bytes.
		g_jmp[0] = 0xe9;
		DWORD addr = (DWORD)NewDirect3DCreate8 - (DWORD)g_orgDirect3DCreate8 - 5;
		TRACE2(&k_kload,"JMP %08x", addr);
		memcpy(g_jmp + 1, &addr, sizeof(DWORD));
		
		memcpy(g_codeFragment, g_orgDirect3DCreate8, 5);
		DWORD newProtection = PAGE_EXECUTE_READWRITE;
		if (VirtualProtect(g_orgDirect3DCreate8, 8, newProtection, &g_savedProtection))
		{
		    memcpy(g_orgDirect3DCreate8, g_jmp, 5);
		    Log(&k_kload,"JMP-hook planted.");
		}
	};
	return;
};

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
	        ptr[0] = (DWORD)NewReadFile - (DWORD)(code[C_READFILE_CS] + 5);
	        bReadFileHooked = true;
	        Log(&k_kload,"ReadFile HOOKED at code[C_READFILE_CS]");
	    }
	}
	return;
};

void UnhookReadFile()
{
	// unhook ReadFile
	if (bReadFileHooked)
	{
		BYTE* bptr = (BYTE*)code[C_READFILE_CS];
		memcpy(bptr, g_rfCode, 6);
		Log(&k_kload,"ReadFile UNHOOKED");
	}
	ClearLine(&l_ReadFile);
	ClearLine(&l_AfterReadFile);
	return;
};

void HookOthers()
{
	bBeginUniSelectHooked = HookProc(C_BEGINUNISELECT, C_BEGINUNISELECT_CS, (DWORD)NewBeginUniSelect,
	                    "C_BEGINUNISELECT", "C_BEGINUNISELECT_CS");
	
    bEndUniSelectHooked = HookProc(C_ENDUNISELECT, C_ENDUNISELECT_CS, (DWORD)NewEndUniSelect,
        "C_ENDUNISELECT", "C_ENDUNISELECT_CS");
        
	bUnpackHooked = HookProc(C_UNPACK, C_UNPACK_CS, (DWORD)NewUnpack,"C_UNPACK", "C_UNPACK_CS");
	
    bUniDecodeHooked = HookProc(C_UNIDECODE, C_UNIDECODE_CS, (DWORD)NewUniDecode,
        	"C_UNIDECODE", "C_UNIDECODE_CS") &&
		HookProc(C_UNIDECODE, C_UNIDECODE_CS2, (DWORD)NewUniDecode,
	        "C_UNIDECODE", "C_UNIDECODE_CS2");

    // hook code[C_GETNATIONALTEAMINFO]
    bGetNationalTeamInfoHooked = 
        HookProc(C_GETNATIONALTEAMINFO, C_GETNATIONALTEAMINFO_CS, (DWORD)NewGetNationalTeamInfo,
            "C_GETNATIONALTEAMINFO", "C_GETNATIONALTEAMINFO_CS") &&
        HookProc(C_GETNATIONALTEAMINFO, C_GETNATIONALTEAMINFO_CS2, (DWORD)NewGetNationalTeamInfo,
            "C_GETNATIONALTEAMINFO", "C_GETNATIONALTEAMINFO_CS2");

    // hook code[C_GETCLUBTEAMINFO]
    bGetClubTeamInfoHooked = 
        HookProc(C_GETCLUBTEAMINFO, C_GETCLUBTEAMINFO_CS, (DWORD)NewGetClubTeamInfo,
            "C_GETCLUBTEAMINFO", "C_GETCLUBTEAMINFO_CS") &&
        HookProc(C_GETCLUBTEAMINFO, C_GETCLUBTEAMINFO_CS2, (DWORD)NewGetClubTeamInfo,
            "C_GETCLUBTEAMINFO", "C_GETCLUBTEAMINFO_CS2") &&
        HookProc(C_GETCLUBTEAMINFO, C_GETCLUBTEAMINFO_CS3, (DWORD)NewGetClubTeamInfo,
            "C_GETCLUBTEAMINFO", "C_GETCLUBTEAMINFO_CS3") &&
        HookProc(C_GETCLUBTEAMINFO, C_GETCLUBTEAMINFO_CS_ML1, (DWORD)NewGetClubTeamInfoML1,
            "C_GETCLUBTEAMINFO", "C_GETCLUBTEAMINFO_CS_ML1") &&
        HookProc(C_GETCLUBTEAMINFO, C_GETCLUBTEAMINFO_CS_ML2, (DWORD)NewGetClubTeamInfoML2,
            "C_GETCLUBTEAMINFO", "C_GETCLUBTEAMINFO_CS_ML2");

    // hook code[C_GETNATIONALTEAMINFO]
    bGetNationalTeamInfoExitEditHooked = HookProc(C_GETNATIONALTEAMINFO,
    	C_GETNATIONALTEAMINFO_CS_EXIT_EDIT, (DWORD)NewGetNationalTeamInfoExitEdit,
            "C_GETNATIONALTEAMINFO", "C_GETNATIONALTEAMINFO_CS_EXIT_EDIT");

	// hook AllocMem
	bAllocMemHooked=HookProc(C_ALLOCMEM,C_ALLOCMEM_CS,(DWORD)NewAllocMem,
		"C_ALLOCMEM","C_ALLOCMEM_CS");
		
	// hook code[C_LODMIXER_HOOK_ORG] + code[C_LODMIXER_HOOK_ORG2]
	bSetLodMixerDataHooked = HookProc(C_LODMIXER_HOOK_ORG, C_LODMIXER_HOOK_CS, 
	        (DWORD)NewSetLodMixerData,"C_LODMIXER_HOOK_ORG", "C_LODMIXER_HOOK_CS") &&
		HookProc(C_LODMIXER_HOOK_ORG2, C_LODMIXER_HOOK_CS2, 
	        (DWORD)NewSetLodMixerData,"C_LODMIXER_HOOK_ORG2", "C_LODMIXER_HOOK_CS2");
	        
	        
	// hook UniSplit
	bUniSplitHooked = HookProc(C_UNISPLIT, C_UNISPLIT_CS1, 
	        (DWORD)NewUniSplit,"C_UNISPLIT", "C_UNISPLIT_CS1") &&
		HookProc(C_UNISPLIT, C_UNISPLIT_CS2, 
	        (DWORD)NewUniSplit,"C_UNISPLIT", "C_UNISPLIT_CS2") &&
		HookProc(C_UNISPLIT, C_UNISPLIT_CS3, 
	        (DWORD)NewUniSplit,"C_UNISPLIT", "C_UNISPLIT_CS3") &&
		HookProc(C_UNISPLIT, C_UNISPLIT_CS4, 
	        (DWORD)NewUniSplit,"C_UNISPLIT", "C_UNISPLIT_CS4") &&
		HookProc(C_UNISPLIT, C_UNISPLIT_CS5, 
	        (DWORD)NewUniSplit,"C_UNISPLIT", "C_UNISPLIT_CS5") &&
	    HookProc(C_UNISPLIT, C_UNISPLIT_CS6, 
	        (DWORD)NewUniSplit,"C_UNISPLIT", "C_UNISPLIT_CS6") &&
		HookProc(C_UNISPLIT, C_UNISPLIT_CS7,
	        (DWORD)NewUniSplit,"C_UNISPLIT", "C_UNISPLIT_CS7") &&
		HookProc(C_UNISPLIT, C_UNISPLIT_CS8, 
	        (DWORD)NewUniSplit,"C_UNISPLIT", "C_UNISPLIT_CS8");
	        
	        	        
	// hook GetPlayerInfo
	char tmp[BUFLEN];
	bGetPlayerInfoHooked=true;
	for (int i=0;i<GPILEN;i++)
		if (gpi[i]!=0) {
			sprintf(tmp,"C_GETPLAYERINFO_CS%d",i+1);
			bGetPlayerInfoHooked&=HookProcAtAddr(code[C_GETPLAYERINFO], gpi[i], 
	    	    (DWORD)NewGetPlayerInfo,"C_GETPLAYERINFO", tmp);
		};
	
	DWORD* ptr;
	BYTE* bptr;
	DWORD protection=0,newProtection=PAGE_EXECUTE_READWRITE;
	
	// hook jmp to GetPlayerInfo
	if (code[C_GETPLAYERINFO_JMP] != 0)
	{
		ptr = (DWORD*)(code[C_GETPLAYERINFO_JMP] + 1);
	    // save original code for JMP GetPlayerInfo
	    memcpy(g_gpiJmpCode, ptr, 4);
	
	    if (VirtualProtect(ptr, 4, newProtection, &protection)) {
	        ptr[0] = (DWORD)NewGetPlayerInfo - (DWORD)(code[C_GETPLAYERINFO_JMP] + 5);
	        bGetPlayerInfoJmpHooked = true;
	        Log(&k_kload,"Jump to GetPlayerInfo HOOKED at code[C_GETPLAYERINFO_JMP]");
	    };
	}

	// hook FileFromAFS
	if (code[C_FILEFROMAFS] != 0)
	{
		bptr = (BYTE*)(code[C_FILEFROMAFS]);
		ptr = (DWORD*)(code[C_FILEFROMAFS] + 1);
	
	    if (VirtualProtect(bptr, 6, newProtection, &protection)) {
	    	bptr[0]=0xe8; //call
	    	bptr[5]=0xc3; //ret
            ptr[0] = (DWORD)NewFileFromAFS - (DWORD)(code[C_FILEFROMAFS] + 5);
	        bFileFromAFSHooked = true;
	        Log(&k_kload,"FileFromAFS HOOKED at code[C_FILEFROMAFS]");
	    };

        // install short jump hack, if needed
        // (we need this when the correct location doesn't have enough
        // space to fit a hook instruction, so we need to jump to a different
        // place instead)
        if (code[C_FILEFROMAFS_JUMPHACK] != 0) {
            bptr = (BYTE*)(code[C_FILEFROMAFS_JUMPHACK]);
            if (VirtualProtect(bptr, 2, newProtection, &protection)) {
                memcpy(bptr, _shortJumpHack[GetPESInfo()->GameVersion], 2);
                bFileFromAFSJumpHackHooked = true;
                Log(&k_kload,"FileFromAFS Short-Jump-Hack installed.");
            }
        }
	};

	// hook jmp to FreeMemory
	if (code[C_FREEMEMORY_JMP] != 0)
	{
		ptr = (DWORD*)(code[C_FREEMEMORY_JMP] + 1);
	    // save original code for JMP FreeMemory
	    memcpy(g_FreeMemoryJmpCode, ptr, 4);
	
	    if (VirtualProtect(ptr, 4, newProtection, &protection)) {
	        ptr[0] = (DWORD)NewFreeMemory - (DWORD)(code[C_FREEMEMORY_JMP] + 5);
	        bFreeMemoryHooked = true;
	        Log(&k_kload,"Jump to FreeMemory HOOKED at code[C_FREEMEMORY_JMP]");
	    };
	};

	// hook ProcessPlayerData
	bProcessPlayerDataHooked = MasterHookFunction(code[C_PROCESSPLAYERDATA_JMP],
													0, NewProcessPlayerData);
	Log(&k_kload,"Jump to ProcessPlayerData HOOKED at code[C_PROCESSPLAYERDATA_JMP]");

    // hook num-pages
    HookGetNumPages();
	return;
};

void UnhookOthers()
{
    // hook GetNumPagesForFileInAFS
    UnhookGetNumPages();

		// unhook code[C_UNPACK]
        bUnpackHooked =  !UnhookProc(bUnpackHooked, C_UNPACK, C_UNPACK_CS,
        	"C_UNPACK", "C_UNPACK_CS");
		ClearLine(&l_Unpack);

		// unhook code[C_GETNATIONALTEAMINFO]
        bGetNationalTeamInfoHooked = !(
            UnhookProc(bGetNationalTeamInfoHooked, 
                    C_GETNATIONALTEAMINFO, C_GETNATIONALTEAMINFO_CS, 
                    "C_GETNATIONALTEAMINFO", "C_GETNATIONALTEAMINFO_CS") &&
            UnhookProc(bGetNationalTeamInfoHooked, 
                    C_GETNATIONALTEAMINFO, C_GETNATIONALTEAMINFO_CS2, 
                    "C_GETNATIONALTEAMINFO", "C_GETNATIONALTEAMINFO_CS2"));
		ClearLine(&l_GetNationalTeamInfo);

		// unhook code[C_GETCLUBTEAMINFO]
        bGetClubTeamInfoHooked = !(
            UnhookProc(bGetClubTeamInfoHooked, 
                    C_GETCLUBTEAMINFO, C_GETCLUBTEAMINFO_CS, 
                    "C_GETCLUBTEAMINFO", "C_GETCLUBTEAMINFO_CS") &&
            UnhookProc(bGetClubTeamInfoHooked, 
                    C_GETCLUBTEAMINFO, C_GETCLUBTEAMINFO_CS2, 
                    "C_GETCLUBTEAMINFO", "C_GETCLUBTEAMINFO_CS2") &&
            UnhookProc(bGetClubTeamInfoHooked, 
                    C_GETCLUBTEAMINFO, C_GETCLUBTEAMINFO_CS_ML1, 
                    "C_GETCLUBTEAMINFO", "C_GETCLUBTEAMINFO_CS_ML1") &&
            UnhookProc(bGetClubTeamInfoHooked, 
                    C_GETCLUBTEAMINFO, C_GETCLUBTEAMINFO_CS_ML2, 
                    "C_GETCLUBTEAMINFO", "C_GETCLUBTEAMINFO_CS_ML2") &&
            UnhookProc(bGetClubTeamInfoHooked, 
                    C_GETCLUBTEAMINFO, C_GETCLUBTEAMINFO_CS3, 
                    "C_GETCLUBTEAMINFO", "C_GETCLUBTEAMINFO_CS3"));
		ClearLine(&l_GetClubTeamInfo);
		ClearLine(&l_GetClubTeamInfoML1);
		ClearLine(&l_GetClubTeamInfoML2);
		
		// unhook code[C_GETNATIONALTEAMINFO]
	    bGetNationalTeamInfoExitEditHooked = !UnhookProc(bGetNationalTeamInfoExitEditHooked,
	    	C_GETNATIONALTEAMINFO,C_GETNATIONALTEAMINFO_CS_EXIT_EDIT,
	            "C_GETNATIONALTEAMINFO", "C_GETNATIONALTEAMINFO_CS_EXIT_EDIT");

		// unhook code[C_UNIDECODE]
        bUniDecodeHooked = !(UnhookProc(bUniDecodeHooked, C_UNIDECODE, C_UNIDECODE_CS, 
                    "C_UNIDECODE", "C_UNIDECODE_CS") &&
			UnhookProc(bUniDecodeHooked, C_UNIDECODE, C_UNIDECODE_CS2, 
                    "C_UNIDECODE", "C_UNIDECODE_CS2"));
               
		ClearLine(&l_BeforeUniDecode);
		ClearLine(&l_UniDecode);

		// unhook code[C_BEGINUNISELECT]
        bBeginUniSelectHooked = 
            !UnhookProc(bBeginUniSelectHooked, C_BEGINUNISELECT, C_BEGINUNISELECT_CS, 
                    "C_BEGINUNISELECT", "C_BEGINUNISELECT_CS");
		ClearLine(&l_BeginUniSelect);

		// unhook code[C_ENDUNISELECT]
        bEndUniSelectHooked = 
            !UnhookProc(bEndUniSelectHooked, C_ENDUNISELECT, C_ENDUNISELECT_CS, 
                    "C_ENDUNISELECT", "C_ENDUNISELECT_CS");
		ClearLine(&l_EndUniSelect);
		
		// unhook AllocMem
		bAllocMemHooked=!UnhookProc(bAllocMemHooked,C_ALLOCMEM,C_ALLOCMEM_CS,
			"C_ALLOCMEM","C_ALLOCMEM_CS");
		ClearLine(&l_AllocMem);
		
		// unhook code[C_LODMIXER_HOOK_ORG]
		bSetLodMixerDataHooked = !(UnhookProc(bSetLodMixerDataHooked,C_LODMIXER_HOOK_ORG,
					C_LODMIXER_HOOK_CS, "C_LODMIXER_HOOK_ORG", "C_LODMIXER_HOOK_CS") &&
			UnhookProc(bSetLodMixerDataHooked,C_LODMIXER_HOOK_ORG2,
					C_LODMIXER_HOOK_CS2, "C_LODMIXER_HOOK_ORG2", "C_LODMIXER_HOOK_CS2"));
		ClearLine(&l_SetLodMixerData);
		
		// unhook UniSplit
		bUniSplitHooked = !(UnhookProc(bUniSplitHooked,C_UNISPLIT, C_UNISPLIT_CS1, 
		        "C_UNISPLIT", "C_UNISPLIT_CS1") &&
			UnhookProc(bUniSplitHooked,C_UNISPLIT, C_UNISPLIT_CS2, 
		        "C_UNISPLIT", "C_UNISPLIT_CS2") &&
			UnhookProc(bUniSplitHooked,C_UNISPLIT, C_UNISPLIT_CS3, 
		        "C_UNISPLIT", "C_UNISPLIT_CS3") &&
			UnhookProc(bUniSplitHooked,C_UNISPLIT, C_UNISPLIT_CS4, 
		        "C_UNISPLIT", "C_UNISPLIT_CS4") &&
			UnhookProc(bUniSplitHooked,C_UNISPLIT, C_UNISPLIT_CS5, 
		        "C_UNISPLIT", "C_UNISPLIT_CS5") &&
		    UnhookProc(bUniSplitHooked,C_UNISPLIT, C_UNISPLIT_CS6, 
		        "C_UNISPLIT", "C_UNISPLIT_CS6") &&
			UnhookProc(bUniSplitHooked,C_UNISPLIT, C_UNISPLIT_CS7, 
		        "C_UNISPLIT", "C_UNISPLIT_CS7") &&
			UnhookProc(bUniSplitHooked,C_UNISPLIT, C_UNISPLIT_CS8, 
		        "C_UNISPLIT", "C_UNISPLIT_CS8"));
		ClearLine(&l_UniSplit);	        

		
		// unhook GetPlayerInfo
		char tmp[BUFLEN];
		bGetPlayerInfoHooked=false;
		for (int i=0;i<GPILEN;i++)
			if (gpi[i]!=0) {
				sprintf(tmp,"C_GETPLAYERINFO_CS%d",i+1);
				bGetPlayerInfoHooked|=(!UnhookProcAtAddr(true,code[C_GETPLAYERINFO], gpi[i], 
		    	    "C_GETPLAYERINFO", tmp));
			};
		
		// unhook jmp to GetPlayerInfo
		if (bGetPlayerInfoJmpHooked)
		{
		    DWORD* ptr = (DWORD*)(code[C_GETPLAYERINFO_JMP] + 1);
		    memcpy(ptr,g_gpiJmpCode,4);
	        bGetPlayerInfoHooked = false;
	        Log(&k_kload,"Jump to GetPlayerInfo UNHOOKED");
		}
		
		ClearLine(&l_GetPlayerInfo);
		
		//unhook ProcessPlayerData
		bProcessPlayerDataHooked=MasterUnhookFunction(code[C_PROCESSPLAYERDATA_JMP], 
														NewProcessPlayerData);
		Log(&k_kload,"Jump to ProcessPlayerData UNHOOKED");

	return;
};

void UnhookKeyb()
{
	if (g_hKeyboardHook != NULL) {
		UnhookWindowsHookEx(g_hKeyboardHook);
		Log(&k_kload,"Keyboard hook uninstalled.");
		g_hKeyboardHook = NULL;
	};
	
	SAFE_DELETE( g_font12 );
	SAFE_DELETE( g_font16 );
	SAFE_DELETE( g_font20 );
	
	TRACE(&k_kload,"g_font SAFE_DELETED.");
	return;
};

bool HookProc(DWORD proc, DWORD proc_cs, DWORD newproc, char* sproc, char* sproc_cs)
{
    if (code[proc] != 0 && code[proc_cs] != 0)
    {
        BYTE* bptr = (BYTE*)code[proc_cs];
        DWORD protection = 0;
        DWORD newProtection = PAGE_EXECUTE_READWRITE;
        if (VirtualProtect(bptr, 8, newProtection, &protection)) {
            DWORD* ptr = (DWORD*)(code[proc_cs] + 1);
            ptr[0] = newproc - (DWORD)(code[proc_cs] + 5);
            LogWithTwoStrings(&k_kload,"code[%s] HOOKED at code[%s]", sproc, sproc_cs);
            return true;
        }
    }
    return false;
}

bool UnhookProc(bool flag, DWORD proc, DWORD proc_cs, char* sproc, char* sproc_cs)
{
    if (flag && code[proc] !=0 && code[proc_cs] != 0)
    {
        BYTE* bptr = (BYTE*)code[proc_cs];
        DWORD protection = 0;
        DWORD newProtection = PAGE_EXECUTE_READWRITE;
        if (VirtualProtect(bptr, 8, newProtection, &protection)) {
            DWORD* ptr = (DWORD*)(code[proc_cs] + 1);
            ptr[0] = (DWORD)code[proc] - (DWORD)(code[proc_cs] + 5);
            LogWithTwoStrings(&k_kload,"code[%s] UNHOOKED at code[%s]", sproc, sproc_cs);
            return true;
        }
    }
    return false;
}

bool HookProcAtAddr(DWORD proc, DWORD proc_cs, DWORD newproc, char* sproc, char* sproc_cs)
{
    if (proc != 0 && proc_cs != 0)
    {
        BYTE* bptr = (BYTE*)proc_cs;
        DWORD protection = 0;
        DWORD newProtection = PAGE_EXECUTE_READWRITE;
        if (VirtualProtect(bptr, 8, newProtection, &protection)) {
            DWORD* ptr = (DWORD*)(proc_cs + 1);
            ptr[0] = newproc - (DWORD)(proc_cs + 5);
            LogWithTwoStrings(&k_kload,"%s HOOKED at %s", sproc, sproc_cs);
            return true;
        }
    }
    return false;
}

bool UnhookProcAtAddr(bool flag, DWORD proc, DWORD proc_cs, char* sproc, char* sproc_cs)
{
    if (flag && proc !=0 && proc_cs != 0)
    {
        BYTE* bptr = (BYTE*)proc_cs;
        DWORD protection = 0;
        DWORD newProtection = PAGE_EXECUTE_READWRITE;
        if (VirtualProtect(bptr, 8, newProtection, &protection)) {
            DWORD* ptr = (DWORD*)(proc_cs + 1);
            ptr[0] = (DWORD)proc - (DWORD)(proc_cs + 5);
            LogWithTwoStrings(&k_kload,"%s UNHOOKED at %s", sproc, sproc_cs);
            return true;
        }
    }
    return false;
}

/**
 * This function calls GetClubTeamInfo() function
 * Parameter:
 *   id   - team id
 * Return value:
 *   address of the KITPACKINFO structure
 */
DWORD NewGetClubTeamInfo(DWORD id)
{
	DWORD id2=id & 0xFFFF;
	
	// call the hooked function
	DWORD result = GetClubTeamInfo(id);

	CGETTEAMINFO NextCall=NULL;
	for (int i=0;i<(l_GetClubTeamInfo.num);i++)
	if (l_GetClubTeamInfo.addr[i]!=0) {
		NextCall=(CGETTEAMINFO)l_GetClubTeamInfo.addr[i];
		NextCall(id2,result);
	};
	
	return result;
};

DWORD NewGetClubTeamInfoML1(DWORD id)
{
	DWORD id2=id & 0xFFFF;
	
	// call the hooked function
	DWORD result = GetClubTeamInfo(id);
	
	CGETTEAMINFO NextCall=NULL;
	for (int i=0;i<(l_GetClubTeamInfoML1.num);i++)
	if (l_GetClubTeamInfoML1.addr[i]!=0) {
		NextCall=(CGETTEAMINFO)l_GetClubTeamInfoML1.addr[i];
		NextCall(id2,result);
	};
	
	return result;
};

DWORD NewGetClubTeamInfoML2(DWORD id)
{
	DWORD id2=id & 0xFFFF;
	
	// call the hooked function
	DWORD result = GetClubTeamInfo(id);
	
	CGETTEAMINFO NextCall=NULL;
	for (int i=0;i<(l_GetClubTeamInfoML2.num);i++)
	if (l_GetClubTeamInfoML2.addr[i]!=0) {
		NextCall=(CGETTEAMINFO)l_GetClubTeamInfoML2.addr[i];
		NextCall(id2,result);
	};
	
	return result;
};

DWORD NewGetNationalTeamInfo(DWORD id)
{
	DWORD id2=id & 0xFFFF;
	
	// call the hooked function
	DWORD result = GetNationalTeamInfo(id);
	
	CGETTEAMINFO NextCall=NULL;
	for (int i=0;i<(l_GetNationalTeamInfo.num);i++)
	if (l_GetNationalTeamInfo.addr[i]!=0) {
		NextCall=(CGETTEAMINFO)l_GetNationalTeamInfo.addr[i];
		NextCall(id2,result);
	};
	
	return result;
};

DWORD NewGetNationalTeamInfoExitEdit(DWORD id)
{
	DWORD id2=id & 0xFFFF;
	
	// call the hooked function
	DWORD result = GetNationalTeamInfo(id);
	
	CGETTEAMINFO NextCall=NULL;
	for (int i=0;i<(l_GetNationalTeamInfoExitEdit.num);i++)
	if (l_GetNationalTeamInfoExitEdit.addr[i]!=0) {
		NextCall=(CGETTEAMINFO)l_GetNationalTeamInfoExitEdit.addr[i];
		NextCall(id2,result);
	};
	
	return result;
};

/**
 * This function is seemingly responsible for allocating memory
 * for decoded buffer (when unpacking BIN files)
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
DWORD NewAllocMem(DWORD infoBlock, DWORD param2, DWORD size)
{
	CALLOCMEM NextCall=NULL;
	DWORD size2=size;
	bool doAllocMem=true;
	
	for (int i=0;i<(l_AllocMem.num);i++)
	if (l_AllocMem.addr[i]!=0) {
		NextCall=(CALLOCMEM)l_AllocMem.addr[i];
		doAllocMem&=NextCall(infoBlock, param2, &size2);
	};

	DWORD result=0;
	if (doAllocMem)
		AllocMem(infoBlock, param2, size2);
	return result;
};

/**
 * This function calls the unpack function for non-kits.
 * Parameters:
 *   addr1   - address of the encoded buffer (without header)
 *   addr2   - address of the decoded buffer
 *   size1   - size of the encoded buffer (minus header)
 *   zero    - always zero
 *   size2   - pointer to size of the decoded buffer
 */
DWORD NewUnpack(DWORD addr1, DWORD addr2, DWORD size1, DWORD zero, DWORD* size2)
{
	// call target function
	DWORD result = Unpack(addr1, addr2, size1, zero, size2);
	
	CUNPACK NextCall=NULL;
	for (int i=0;i<(l_Unpack.num);i++)
	if (l_Unpack.addr[i]!=0) {
		NextCall=(CUNPACK)l_Unpack.addr[i];
		NextCall(addr1, addr2, size1, zero, size2, result);
	};
	
	return result;
};

KEXPORT DWORD MemUnpack(DWORD addr1, DWORD addr2, DWORD size1, DWORD* size2)
{
	return Unpack(addr1, addr2, size1, 0, size2);
};

KEXPORT DWORD AFSMemUnpack(DWORD FileID, DWORD Buffer)
{
	ENCBUFFERHEADER *e;
	char tmp[BUFLEN];
	DWORD FileInfo[2];
	DWORD NBW=0;
	
	strcpy(tmp,g_pesinfo.pesdir);
	strcat(tmp,"dat\\0_text.afs");
	
	HANDLE file=CreateFile(tmp,GENERIC_READ,3,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,0);
	if (file==INVALID_HANDLE_VALUE) return 0;
	SetFilePointer(file,8*(FileID+1),0,0);
	ReadFile(file,&(FileInfo[0]),8,&NBW,0);
	
	LPVOID srcbuf=HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,FileInfo[1]);
	SetFilePointer(file,FileInfo[0],0,0);
	ReadFile(file,srcbuf,FileInfo[1],&NBW,0);
	CloseHandle(file);
	e=(ENCBUFFERHEADER*)srcbuf;

	DWORD result=Unpack((DWORD)srcbuf+0x20, Buffer, e->dwEncSize, 0, &(e->dwDecSize));
	
	HeapFree(GetProcessHeap(),0,srcbuf);
	
	return result;
};

/**
 * This function calls kit BIN decode function
 * Parameters:
 *   addr   - address of the encoded buffer header
 *   size   - size of the encoded buffer
 * Return value:
 *   address of the decoded buffer
 */
DWORD NewUniDecode(DWORD addr, DWORD size)
{
	CUNIDECODE NextCall=NULL;
	for (int i=0;i<(l_BeforeUniDecode.num);i++)
	if (l_BeforeUniDecode.addr[i]!=0) {
		NextCall=(CUNIDECODE)l_BeforeUniDecode.addr[i];
		NextCall(addr,size,0);
	};

//Log(&k_kload,"NewUniDecode called.");
	
	// call the hooked function
	DWORD result = UniDecode(addr, size);
	
	NextCall=NULL;
	for (i=0;i<(l_UniDecode.num);i++)
	if (l_UniDecode.addr[i]!=0) {
		NextCall=(CUNIDECODE)l_UniDecode.addr[i];
		NextCall(addr,size,result);
	};
	
	return result;
};

DWORD NewUniSplit(DWORD id)
{
	DWORD oldESI, oldEBX, oldEBP;
	__asm mov oldEBP, ebp
	__asm mov oldESI, esi
	__asm mov oldEBX, ebx
	
	DWORD Caller=*(DWORD*)(oldEBP+4);
	
	DWORD result=UniSplit(id);
	
	CUNISPLIT NextCall=NULL;
	for (int i=0;i<(l_UniSplit.num);i++)
	if (l_UniSplit.addr[i]!=0) {
		NextCall=(CUNISPLIT)l_UniSplit.addr[i];
		//if (*(DWORD*)(&id-4)
		if (Caller<code[C_UNISPLIT_CS5])
			NextCall(id, result, oldESI);
		else
			NextCall(id, result, oldEBX);
	};
	
	return result;
};

DWORD NewBeginUniSelect()
{
	DWORD result = BeginUniSelect();
	
	IsKitSelectMode=true;
	SetNewDrawKitInfoMenu(0,true);

    // hook inputs
    HookGameInput(code[C_CLEANINPUTTABLE_HOOK], data[INPUT_TABLE]);
	
	ALLVOID NextCall=NULL;
	for (int i=0;i<(l_BeginUniSelect.num);i++)
	if (l_BeginUniSelect.addr[i]!=0) {
		NextCall=(ALLVOID)l_BeginUniSelect.addr[i];
		NextCall();
	};

	return result;
};

DWORD NewEndUniSelect()
{
	DWORD result = EndUniSelect();
	
	IsKitSelectMode=false;

    // unhook inputs
    UnhookGameInput(code[C_CLEANINPUTTABLE_HOOK]);
	
	ALLVOID NextCall=NULL;
	for (int i=0;i<(l_EndUniSelect.num);i++)
	if (l_EndUniSelect.addr[i]!=0) {
		NextCall=(ALLVOID)l_EndUniSelect.addr[i];
		NextCall();
	};
	
	if (OnHideMenuFuncs!=NULL && OnHideMenuFuncs[usedKitInfoMenu] != 0) {
			NextCall=(ALLVOID)OnHideMenuFuncs[usedKitInfoMenu];
			NextCall();
		};
	
	return result;
};

DWORD NewSetLodMixerData(DWORD dummy)
{
	DWORD result;
	if (*(&dummy-1)==code[C_LODMIXER_HOOK_CS]+5)
		result=SetLodMixerData();
	else
		result=SetLodMixerData2();
	
	ALLVOID NextCall=NULL;
	for (int i=0;i<(l_SetLodMixerData.num);i++)
	if (l_SetLodMixerData.addr[i]!=0) {
		NextCall=(ALLVOID)l_SetLodMixerData.addr[i];
		NextCall();
	};
		
	return result;
};

//Parameters are stored in EAX and ECX
DWORD NewGetPlayerInfo()
{
	DWORD PlayerNumber, Mode, Caller=0;
	DWORD oldEBP=0;
	
	__asm {
		mov PlayerNumber, eax
		mov Mode, ecx
		mov oldEBP, ebp
	}
	
	Caller=*(DWORD*)(oldEBP+4);
	
	//make sure that the parameters are still the same
	__asm {
		mov eax, PlayerNumber
		mov ecx, Mode
	}
	
	DWORD result=GetPlayerInfo(PlayerNumber,Mode);
	
	//Find out who's calling
	for (int i=0;i<GPILEN;i++)
		if (Caller==gpi[i]+5) {
			Caller=i;
			break;
		};
	
	//LogWithNumber(&k_kload,"Caller is %x",Caller);

	PlayerNumber&=0xFFFF;

	CGETPLAYERINFO NextCall=NULL;
	for (i=0;i<(l_GetPlayerInfo.num);i++)
	if (l_GetPlayerInfo.addr[i]!=0) {
		NextCall=(CGETPLAYERINFO)l_GetPlayerInfo.addr[i];
		NextCall(Caller,&PlayerNumber,Mode,&result);
	};
	
	return result;
};

KEXPORT DWORD GetPlayerInfo(DWORD PlayerNumber,DWORD Mode)
{
	__asm mov eax, PlayerNumber
	__asm mov ecx, Mode
	return oGetPlayerInfo();
};

void NewFileFromAFS(DWORD retAddr, DWORD infoBlock)
{
	//Log(&k_kload,"NewFileFromAFS CALLED.");
	FILEFROMAFS NextCall=NULL;
	for (int i=0;i<(l_FileFromAFS.num);i++)
	if (l_FileFromAFS.addr[i]!=0) {
		NextCall=(FILEFROMAFS)l_FileFromAFS.addr[i];
		NextCall(infoBlock);
	};
	//Log(&k_kload,"NewFileFromAFS done.");
	return;
};

void NewFreeMemory(DWORD addr)
{
	//TRACE(&k_kload,"NewFreeMemory CALLED.");
	bool doFreeMemory=true;
	
	CFREEMEMORY NextCall=NULL;
	for (int i=0;i<(l_BeforeFreeMemory.num);i++)
	if (l_BeforeFreeMemory.addr[i]!=0) {
		NextCall=(CFREEMEMORY)l_BeforeFreeMemory.addr[i];
		doFreeMemory&=NextCall(addr);
	};
	
	if (doFreeMemory)
		oFreeMemory(addr);
		
	//TRACE(&k_kload,"NewFreeMemory done.");
	return;
};

void NewProcessPlayerData()
{
	//Log(&k_kload,"NewProcessPlayerData CALLED.");
	DWORD oldESI;
	DWORD PlayerNumber;
	
	__asm mov oldESI, esi
	PlayerNumber=*(WORD*)(oldESI+0x3C);
	DWORD addr=**(DWORD**)(oldESI+4);
	DWORD *FaceID=(DWORD*)(addr+0x40);
	(*FaceID)&=0xFFFF;

	PROCESSPLAYERDATA NextCall=NULL;
	for (int i=0;i<(l_ProcessPlayerData.num);i++)
	if (l_ProcessPlayerData.addr[i]!=0) {
		NextCall=(PROCESSPLAYERDATA)l_ProcessPlayerData.addr[i];
		NextCall(oldESI,&PlayerNumber);
	};
	//Log(&k_kload,"NewProcessPlayerData done.");
	return;
};

/**
 * Monitors the file pointer.
 */
BOOL STDMETHODCALLTYPE NewReadFile(HANDLE hFile, LPVOID lpBuffer, DWORD nNumberOfBytesToRead,
  LPDWORD lpNumberOfBytesRead, LPOVERLAPPED lpOverlapped)
{
	PFNREADFILE NextCall=NULL;
	
	for (int i=0;i<(l_ReadFile.num);i++)
	if (l_ReadFile.addr[i]!=0) {
		NextCall=(PFNREADFILE)l_ReadFile.addr[i];
		NextCall(hFile, lpBuffer, nNumberOfBytesToRead, lpNumberOfBytesRead, lpOverlapped);
	};

    //Log(&k_kload, "NewReadFile called.");
	
	// call original function		
	BOOL result=ReadFile(hFile, lpBuffer, nNumberOfBytesToRead, lpNumberOfBytesRead, lpOverlapped);
	
	for (i=0;i<(l_AfterReadFile.num);i++)
	if (l_AfterReadFile.addr[i]!=0) {
		NextCall=(PFNREADFILE)l_AfterReadFile.addr[i];
		NextCall(hFile, lpBuffer, nNumberOfBytesToRead, lpNumberOfBytesRead, lpOverlapped);
	};
	
	return result;
}

/**
 * Tracker for Direct3DCreate8 function.
 */
IDirect3D8* STDMETHODCALLTYPE NewDirect3DCreate8(UINT sdkVersion)
{
	ALLVOID NextCall=NULL;
	
	Log(&k_kload,"NewDirect3DCreate8 called.");
	BYTE* dest = (BYTE*)g_orgDirect3DCreate8;

	// put back saved code fragment
	dest[0] = g_codeFragment[0];
	*((DWORD*)(dest + 1)) = *((DWORD*)(g_codeFragment + 1));

    Log(&k_kload,"calling InitGetNumPages()...");
    InitGetNumPages();

	for (int i=0;i<(l_D3D_Create.num);i++)
		if (l_D3D_Create.addr[i]!=0) {
			LogWithNumber(&k_kload,"NewDirect3DCreate8: calling function %x",l_D3D_Create.addr[i]);
			NextCall=(ALLVOID)l_D3D_Create.addr[i];
			NextCall();
		};

	// call the original function.
	IDirect3D8* result = g_orgDirect3DCreate8(sdkVersion);

    if (!g_device) {
        DWORD* vtable = (DWORD*)(*((DWORD*)result));

        // hook CreateDevice method
        g_orgCreateDevice = (PFNCREATEDEVICEPROC)vtable[VTAB_CREATEDEVICE];
        DWORD protection = 0;
		DWORD newProtection = PAGE_EXECUTE_READWRITE;
		if (VirtualProtect(vtable+VTAB_CREATEDEVICE, 4, newProtection, &protection))
		{
            vtable[VTAB_CREATEDEVICE] = (DWORD)NewCreateDevice;
			Log(&k_kload,"CreateDevice hooked.");
		}

        // hook GetDeviceCaps method
        g_orgGetDeviceCaps = (PFNGETDEVICECAPSPROC)vtable[VTAB_GETDEVICECAPS];
        protection = 0;
		newProtection = PAGE_EXECUTE_READWRITE;
		if (VirtualProtect(vtable+VTAB_GETDEVICECAPS, 4, newProtection, &protection))
		{
            vtable[VTAB_GETDEVICECAPS] = (DWORD)NewGetDeviceCaps;
			Log(&k_kload,"GetDeviceCaps hooked.");
		}
    }

    HookReadFile();
    HookOthers();
    if (g_hKeyboardHook == NULL) {
		g_hKeyboardHook = SetWindowsHookEx(WH_KEYBOARD, KeyboardProc, hInst, GetCurrentThreadId());
		LogWithNumber(&k_kload,"Installed keyboard hook: g_hKeyboardHook = %d", (DWORD)g_hKeyboardHook);
	};

	return result;
};

/**
 * GetDeviceCaps function.
 */
HRESULT STDMETHODCALLTYPE NewGetDeviceCaps(IDirect3D8* self, UINT Adapter,
   D3DDEVTYPE DeviceType, D3DCAPS8* pCaps)
{
    Log(&k_kload,"NewGetDeviceCaps called.");
    HRESULT result = g_orgGetDeviceCaps(self, Adapter, DeviceType, pCaps);

    if (g_config.emulateHW_TnL) {
        BOOL hwTnL = (pCaps->DevCaps & D3DDEVCAPS_HWTRANSFORMANDLIGHT) ? 1 : 0;
        LogWithNumber(&k_kload,"device reported HW TnL: %d", hwTnL);

        // emulate hw TnL
        pCaps->DevCaps |= D3DDEVCAPS_HWTRANSFORMANDLIGHT; 
        Log(&k_kload,"emulating HW TnL.");
    }
    return result;
}

/**
 * CreateDevice hijacker.
 */
HRESULT STDMETHODCALLTYPE NewCreateDevice(IDirect3D8* self, UINT Adapter,
    D3DDEVTYPE DeviceType, HWND hFocusWindow, DWORD BehaviorFlags,
    D3DPRESENT_PARAMETERS *pPresentationParameters, 
    IDirect3DDevice8** ppReturnedDeviceInterface) 
{
	DWORD* vtable;
	DWORD protection;
	DWORD newProtection;
	PFNCREATEDEVICEPROC NextCall=NULL;
	
	Log(&k_kload,"NewCreateDevice called.");

    if (g_config.forceSW_TnL) {
        BOOL swVP = (BehaviorFlags & D3DCREATE_SOFTWARE_VERTEXPROCESSING) ? 1 : 0;
        BOOL hwVP = (BehaviorFlags & D3DCREATE_HARDWARE_VERTEXPROCESSING) ? 1 : 0;
        BOOL miVP = (BehaviorFlags & D3DCREATE_MIXED_VERTEXPROCESSING) ? 1 : 0;
        LogWithNumber(&k_kload,"app requested SW vertex processing: %d", swVP);
        LogWithNumber(&k_kload,"app requested HW vertex processing: %d", hwVP);
        LogWithNumber(&k_kload,"app requested mixed vertex processing: %d", miVP);

        BehaviorFlags |= D3DCREATE_SOFTWARE_VERTEXPROCESSING;
        BehaviorFlags &= ~D3DCREATE_HARDWARE_VERTEXPROCESSING;
        BehaviorFlags &= ~D3DCREATE_MIXED_VERTEXPROCESSING;
        Log(&k_kload,"forcing SW TnL.");
    }

    // Set antialiasing
    //pPresentationParameters->SwapEffect = D3DSWAPEFFECT_DISCARD;
    //pPresentationParameters->MultiSampleType = D3DMULTISAMPLE_4_SAMPLES;

    HRESULT result = g_orgCreateDevice(self, Adapter, DeviceType, hFocusWindow,
            BehaviorFlags, pPresentationParameters, ppReturnedDeviceInterface);

    // enable antialising
    //(*ppReturnedDeviceInterface)->SetRenderState(D3DRS_MULTISAMPLEANTIALIAS, TRUE);

    g_device = *ppReturnedDeviceInterface;
    g_device->AddRef();
    
    if (!g_device) goto NoDevice;
    
    vtable = (DWORD*)(*((DWORD*)g_device));
	protection = 0;
	newProtection = PAGE_EXECUTE_READWRITE;

    // hook CreateTexture method
    if (!g_orgCreateTexture) {
        g_orgCreateTexture = (PFNCREATETEXTUREPROC)vtable[VTAB_CREATETEXTURE];
		if (VirtualProtect(vtable+VTAB_CREATETEXTURE, 4, newProtection, &protection))
		{
            vtable[VTAB_CREATETEXTURE] = (DWORD)NewCreateTexture;
			Log(&k_kload,"CreateTexture hooked.");
            LogWithNumber(&k_kload, "NewCreateTexture = %08x", (DWORD)NewCreateTexture);
		}
    }

    // hook Present method
    if (!g_orgPresent) {
        g_orgPresent = (PFNPRESENTPROC)vtable[VTAB_PRESENT];
		if (VirtualProtect(vtable+VTAB_PRESENT, 4, newProtection, &protection))
		{
            vtable[VTAB_PRESENT] = (DWORD)NewPresent;
			Log(&k_kload,"Present hooked.");
		}
    }

    // hook Reset method
    if (!g_orgReset) {
        g_orgReset = (PFNRESETPROC)vtable[VTAB_RESET];
		if (VirtualProtect(vtable+VTAB_RESET, 4, newProtection, &protection))
		{
            vtable[VTAB_RESET] = (DWORD)NewReset;
			Log(&k_kload,"Reset hooked.");
		}
    }

	NoDevice:
	for (int i=0;i<(l_D3D_CreateDevice.num);i++)
		if (l_D3D_CreateDevice.addr[i]!=0) {
			LogWithNumber(&k_kload,"NewCreateDevice: calling function %x",l_D3D_CreateDevice.addr[i]);
			NextCall=(PFNCREATEDEVICEPROC)l_D3D_CreateDevice.addr[i];
			NextCall(self, Adapter, DeviceType, hFocusWindow,
            	BehaviorFlags, pPresentationParameters, ppReturnedDeviceInterface);
		};

    return result;
};

KEXPORT IDirect3DDevice8* GetActiveDevice()
{
	return g_device;
};

KEXPORT void SetActiveDevice(IDirect3DDevice8* n_device)
{
	g_device=n_device;
	return;
};

void kloadRestoreDeviceObjects(IDirect3DDevice8* dev)
{
	if (!g_fontInitialized) 
	{
		g_font12->InitDeviceObjects(dev);
		g_font16->InitDeviceObjects(dev);
		g_font20->InitDeviceObjects(dev);
		g_fontInitialized = true;
	}
	g_font12->RestoreDeviceObjects();
	g_font16->RestoreDeviceObjects();
	g_font20->RestoreDeviceObjects();
	g_needsRestore = FALSE;
	return;
};

void kloadInvalidateDeviceObjects(IDirect3DDevice8* dev)
{
	TRACE(&k_kload,"kloadInvalidateDeviceObjects called.");
	if (dev == NULL)
	{
		TRACE(&k_kload,"kloadInvalidateDeviceObjects: nothing to invalidate.");
		return;
	}

    if (g_font12) g_font12->InvalidateDeviceObjects();
    if (g_font16) g_font16->InvalidateDeviceObjects();
    if (g_font20) g_font20->InvalidateDeviceObjects();
    
    return;
};

void kloadDeleteDeviceObjects(IDirect3DDevice8* dev)
{
	if (g_font12) g_font12->DeleteDeviceObjects();
	if (g_font16) g_font16->DeleteDeviceObjects();
	if (g_font20) g_font20->DeleteDeviceObjects();
	g_fontInitialized = false;

    return;
};

void kloadGetBackBufferInfo(IDirect3DDevice8* d3dDevice)
{
	TRACE(&k_kload,"kloadGetBackBufferInfo: called.");

	IDirect3DSurface8* g_backBuffer;
	// get the 0th backbuffer.
	if (SUCCEEDED(d3dDevice->GetBackBuffer(0, D3DBACKBUFFER_TYPE_MONO, &g_backBuffer)))
	{
		D3DSURFACE_DESC desc;
		g_backBuffer->GetDesc(&desc);
		g_bbWidth = desc.Width;
		g_bbHeight = desc.Height;
		stretchX=g_bbWidth/1024.0;
		stretchY=g_bbHeight/768.0;
		
		g_pesinfo.bbWidth=g_bbWidth;
		g_pesinfo.bbHeight=g_bbHeight;
		g_pesinfo.stretchX=stretchX;
		g_pesinfo.stretchY=stretchY;

		kloadInvalidateDeviceObjects(d3dDevice);
		kloadDeleteDeviceObjects(d3dDevice);
		g_font12=new CD3DFont( L"Arial",12*stretchY,D3DFONT_BOLD);
		g_font16=new CD3DFont( L"Arial",16*stretchY,D3DFONT_BOLD);
		g_font20=new CD3DFont( L"Arial",20*stretchY,D3DFONT_BOLD);
		kloadRestoreDeviceObjects(d3dDevice);
		Log(&k_kload,"kloadGetBackBufferInfo: got new back buffer format and info.");
		g_bGotFormat = true;
		
		// release backbuffer
		g_backBuffer->Release();
	}
	
	return;
};

CD3DFont* GetFont(DWORD fontSize)
{
	CD3DFont* g_font=NULL;
	switch (fontSize) {
	case 12:
		g_font=g_font12; break;
	case 16:
		g_font=g_font16; break;
	case 20:
		g_font=g_font20; break;
	};
	return g_font;
};

KEXPORT void KDrawTextW(FLOAT x,FLOAT y,DWORD dwColor,DWORD fontSize,WCHAR* strText,bool absolute)
{
	bool needsEndScene=false;
	
	CD3DFont* g_font=GetFont(fontSize);
	
	if (g_font==NULL) {
		LogWithNumber(&k_kload,"KDrawText: No font found for size %d!",fontSize);
		return;
	};
	
	needsEndScene=SUCCEEDED(g_device->BeginScene());
	if (absolute)
		g_font->DrawText(x,y,dwColor,strText);
	else
		g_font->DrawText(x*stretchX,y*stretchY,dwColor,strText);

	if (needsEndScene)
		g_device->EndScene();
	return;
};

KEXPORT void KDrawText(FLOAT x,FLOAT y,DWORD dwColor,DWORD fontSize,TCHAR* strText,bool absolute)
{
	//convert the string to unicode char by char
	int strLen=_mbstrlen(strText);
	WCHAR* tmp=new WCHAR[strLen+1];
	ZeroMemory(tmp,sizeof(WCHAR)*(strLen+1));
	for (int i=0;i<strLen;i++)
		mbtowc(&(tmp[i]),&(strText[i]),MB_CUR_MAX);

	KDrawTextW(x,y,dwColor,fontSize,tmp,absolute);
	
	delete tmp;
	return;
};

KEXPORT void KGetTextExtentW(WCHAR* strText,DWORD fontSize,SIZE* pSize)
{
	bool needsEndScene=true;
	CD3DFont* g_font=GetFont(fontSize);
	
	if (g_font==NULL) {
		LogWithNumber(&k_kload,"KGetTextExtent: No font found for size %d!",fontSize);
		return;
	};
	
	needsEndScene=SUCCEEDED(g_device->BeginScene());
	g_font->GetTextExtent(strText,pSize);
	if (needsEndScene)
		g_device->EndScene();
	return;
};

KEXPORT void KGetTextExtent(TCHAR* strText,DWORD fontSize,SIZE* pSize)
{
	//convert the string to unicode char by char
	int strLen=_mbstrlen(strText);
	WCHAR* tmp=new WCHAR[strLen+1];
	ZeroMemory(tmp,sizeof(WCHAR)*(strLen+1));
	for (int i=0;i<strLen;i++)
		mbtowc(&(tmp[i]),&(strText[i]),MB_CUR_MAX);

	KGetTextExtentW(tmp,fontSize,pSize);
	
	delete tmp;
	return;
};

/**
 * Tracker for IDirect3DDevice8::CreateTexture method.
 */
HRESULT STDMETHODCALLTYPE NewCreateTexture(IDirect3DDevice8* self, UINT width, UINT height,
UINT levels, DWORD usage, D3DFORMAT format, D3DPOOL pool, IDirect3DTexture8** ppTexture)
{
	HRESULT res=D3D_OK;
	DWORD oldEBP=0;
	DWORD src=0;
	CCREATETEXTURE NextCall=NULL;
	bool IsProcessed=false;
	
	__asm mov oldEBP, ebp

    // "STACK_SHIFT" is needed in case of PES6 PC 1.10, because
    // it appears that the calls to CreateTexture is done via some sort of proxy
    // dll (d3dw.dll), so we have to account for extra space occupied on the stack
    
	if (*(DWORD*)(oldEBP+4+data[STACK_SHIFT])==code[C_CALL050]+3) {
		src=*(DWORD*)(oldEBP+0x74+data[STACK_SHIFT]);
		if (src!=0 && !IsBadReadPtr((LPVOID)src,4)) {
			src=*(DWORD*)(src+0x18);
			//TRACE2(&k_kload,"src = %x",src);
		} else
			src=0;
	};
	
	for (int i=0;i<(l_D3D_CreateTexture.num);i++)
	if (l_D3D_CreateTexture.addr[i]!=0) {
		NextCall=(CCREATETEXTURE)l_D3D_CreateTexture.addr[i];
		res=NextCall(self, width, height, levels, usage, format, pool, ppTexture, src, &IsProcessed);
	};
	
	if (!IsProcessed)
		res = OrgCreateTexture(self, width, height, levels, usage, format, pool, ppTexture);


	for (i=0;i<(l_D3D_AfterCreateTexture.num);i++)
	if (l_D3D_AfterCreateTexture.addr[i]!=0) {
		NextCall=(CCREATETEXTURE)l_D3D_AfterCreateTexture.addr[i];
		NextCall(self, width, height, levels, usage, format, pool, ppTexture, src, &IsProcessed);
	};

	DWORD* vtable = (DWORD*)(*((DWORD*)g_device));
	DWORD protection = 0;
	DWORD newProtection = PAGE_EXECUTE_READWRITE;
	
	if (vtable[VTAB_SETRENDERSTATE] != (DWORD)NewSetRenderState) {

		if (!g_orgSetRenderState)
			g_orgSetRenderState = (PFNSETRENDERSTATEPROC)vtable[VTAB_SETRENDERSTATE];
		if (VirtualProtect(vtable+VTAB_SETRENDERSTATE, 4, newProtection, &protection))
		{
			vtable[VTAB_SETRENDERSTATE] = (DWORD)NewSetRenderState;
			Log(&k_kload,"SetRenderState hooked.");
		}
	};
	
	if (vtable[VTAB_SETTEXTURE] != (DWORD)NewSetTexture) {
		if (!g_orgSetTexture)
			g_orgSetTexture = (PFNSETTEXTUREPROC)vtable[VTAB_SETTEXTURE];
		if (VirtualProtect(vtable+VTAB_SETTEXTURE, 4, newProtection, &protection))
		{
			vtable[VTAB_SETTEXTURE] = (DWORD)NewSetTexture;
			Log(&k_kload,"SetTexture hooked.");
		}
	};
	
	if (*ppTexture!=NULL) {
		vtable = (DWORD*)(*((DWORD*)*ppTexture));
		if (vtable[VTAB_UNLOCKRECT] != (DWORD)NewUnlockRect) {
			g_orgUnlockRect=(PFNUNLOCKRECT)vtable[VTAB_UNLOCKRECT];
			
			DWORD protection = 0;
			DWORD newProtection = PAGE_EXECUTE_READWRITE;
			if (VirtualProtect(vtable+VTAB_UNLOCKRECT, 4, newProtection, &protection))
			{
	            vtable[VTAB_UNLOCKRECT] = (DWORD)NewUnlockRect;
				Log(&k_kload,"UnlockRect hooked.");
			};
		};
	};

	return res;
};

KEXPORT HRESULT STDMETHODCALLTYPE OrgCreateTexture(IDirect3DDevice8* self, UINT width, UINT height,
UINT levels, DWORD usage, D3DFORMAT format, D3DPOOL pool, IDirect3DTexture8** ppTexture)
{
	return g_orgCreateTexture(self, width, height, levels, usage, format, pool, ppTexture);
};

HRESULT STDMETHODCALLTYPE NewUnlockRect(IDirect3DTexture8* self,UINT Level)
{
	HRESULT result=g_orgUnlockRect(self,Level);
	
	UNLOCKRECT NextCall=NULL;
	for (int i=0;i<(l_D3D_UnlockRect.num);i++)
	if (l_D3D_UnlockRect.addr[i]!=0) {
		NextCall=(UNLOCKRECT)l_D3D_UnlockRect.addr[i];
		NextCall(self, Level);
	};
	
	return result;
};

/* New Present function */
HRESULT STDMETHODCALLTYPE NewPresent(IDirect3DDevice8* self, CONST RECT* src, CONST RECT* dest,
	HWND hWnd, LPVOID unused)
{
	int i;
	PFNPRESENTPROC NextCall=NULL;
	
	// determine backbuffer's format and dimensions, if not done yet.
	if (!g_bGotFormat) {
		kloadGetBackBufferInfo(self);
	};

	if (g_needsRestore || !g_fontInitialized) 
	{
		kloadRestoreDeviceObjects(self);
		Log(&k_kload,"NewPresent: RestoreDeviceObjects() done.");
	}
	
	for (i=0;i<(l_D3D_Present.num);i++)
	if (l_D3D_Present.addr[i]!=0) {
		//LogWithNumber(&k_kload,"NewPresent: calling function %x",l_D3D_Present.addr[i]);
		NextCall=(PFNPRESENTPROC)l_D3D_Present.addr[i];
		NextCall(self, src, dest, hWnd, unused);
	};

	//KDrawText(0,0,0xffffffff,20,"TEST: äöüÄÖÜßîêéàâáà");
	//Print additional information (labels,...)
	if (l_DrawKitSelectInfo.num>0)
		DrawKitSelectInfo();
	
	
	// CALL ORIGINAL FUNCTION ///////////////////
	HRESULT res = g_orgPresent(self, src, dest, hWnd, unused);

	return res;
};

/* New Reset function */
HRESULT STDMETHODCALLTYPE NewReset(IDirect3DDevice8* self, LPVOID params)
{
	LogWithNumber(&k_kload,"NewReset: CALLED. caller = %08x", (DWORD)(*(&self-4)));
	
	Log(&k_kload,"NewReset: cleaning-up.");

	kloadInvalidateDeviceObjects(self);
	kloadDeleteDeviceObjects(self);

	g_bGotFormat = false;
    g_needsRestore = TRUE;
    
	PFNRESETPROC NextCall=NULL;
	for (int i=0;i<(l_D3D_Reset.num);i++)
	if (l_D3D_Reset.addr[i]!=0) {
		LogWithNumber(&k_kload,"NewReset: calling function %x",l_D3D_Reset.addr[i]);
		NextCall=(PFNRESETPROC)l_D3D_Reset.addr[i];
		NextCall(self, params);
	};

    //D3DPRESENT_PARAMETERS* pPresentationParameters = (D3DPRESENT_PARAMETERS*)params;
    //pPresentationParameters->SwapEffect = D3DSWAPEFFECT_DISCARD;
    //pPresentationParameters->MultiSampleType = D3DMULTISAMPLE_4_SAMPLES;

	// CALL ORIGINAL FUNCTION
	HRESULT res = g_orgReset(self, params);

    //self->SetRenderState(D3DRS_MULTISAMPLEANTIALIAS, TRUE);

	TRACE(&k_kload,"NewReset: Reset() is done. About to return.");
	return res;
}

/**
 * Tracker for IDirect3DDevice8::SetTexture method.
 */
HRESULT STDMETHODCALLTYPE NewSetTexture(IDirect3DDevice8* self, DWORD stage,
IDirect3DBaseTexture8* pTexture)
{
	HRESULT res = S_OK;

	res = g_orgSetTexture(self, stage, pTexture);
	
	g_lastTexture=pTexture;

    /*
	// dump texture to BMP
	char name[BUFLEN];
	ZeroMemory(name, BUFLEN);
	sprintf(name, "E:\\texdump\\%s%05d.jpg", "tex-", g_frame_tex_count++);
	D3DXSaveTextureToFile(name, D3DXIFF_JPG, pTexture, NULL);
    */

	return res;
};


HRESULT STDMETHODCALLTYPE NewSetRenderState(IDirect3DDevice8* self,D3DRENDERSTATETYPE State,DWORD Value)
{
	HRESULT res = S_OK;
	//LogWithTwoNumbers(&k_kload,"NewSetRenderState: State = %d, Value = %d", State, Value);

	if (State == 5527)
		res = g_orgSetRenderState(self, State, 1);
	else
		res = g_orgSetRenderState(self, State, Value);

    /*
	//if (g_dumpTexturesMode) {
    {
		// dump texture to BMP
		char name[BUFLEN];
		ZeroMemory(name, BUFLEN);
		sprintf(name, "E:\\texdump\\%s%05d.bmp", "tex-", g_frame_tex_count++);
		D3DXSaveTextureToFile(name, D3DXIFF_BMP, pTexture, NULL);
	}
    */

	return res;
};

void CheckInput()
{
    DWORD* inputs = GetInputTable();
    KEYCFG* keyCfg = GetInputCfg();
    for (int n=0; n<8; n++) {
        if (INPUT_EVENT(inputs, n, FUNCTIONAL, keyCfg->gamepad.keyInfoPageNext)) {
            // switch information apge
			SetNewDrawKitInfoMenu(usedKitInfoMenu+1,false);
            Log(&k_kload, "[SELECT] PRESSED.");
        }
    }
}

void DrawKitSelectInfo()
{
	char tmp[BUFLEN];
	
	if (!IsKitSelectMode) return;
	if (usedKitInfoMenu >= l_DrawKitSelectInfo.num)
		usedKitInfoMenu=0;

	DRAWKITSELECTINFO NextCall=NULL;
	NextCall=(DRAWKITSELECTINFO)l_DrawKitSelectInfo.addr[usedKitInfoMenu];
	if (NextCall==NULL) return;
	NextCall(g_device);
	
	//now print the actual menu title
	sprintf(tmp,"%s - [%d/%d]",menuTitle,usedKitInfoMenu+1,numKitInfoMenus);
	KDrawText(520,711,0xd0ffc000,20,tmp);

    // check input
    CheckInput();
	
	return;
};

KEXPORT void dksiSetMenuTitle(char* newTitle)
{
	strcpy(menuTitle,newTitle);
	return;
};


LRESULT CALLBACK KeyboardProc(int code1, WPARAM wParam, LPARAM lParam)
{
	if (IsKitSelectMode && code1 >= 0 && code1==HC_ACTION && lParam & 0x80000000) {
        KEYCFG* keyCfg = GetInputCfg();
		if (wParam == keyCfg->keyboard.keyInfoPageNext)
			SetNewDrawKitInfoMenu(usedKitInfoMenu+1,false);
		else if (wParam == keyCfg->keyboard.keyInfoPagePrev) {
			SetNewDrawKitInfoMenu(usedKitInfoMenu-1,false);
		};
	};
	
	
	CINPUT NextCall=NULL;
	for (int i=0;i<(l_Input.num);i++)
	if (l_Input.addr[i]!=0) {
		NextCall=(CINPUT)l_Input.addr[i];
		NextCall(code1,wParam,lParam);
	};

	return CallNextHookEx(g_hKeyboardHook, code1, wParam, lParam);
};

void SetNewDrawKitInfoMenu(int NewMenu, bool ForceShowMenuFunc)
{
	int lastKitInfoMenu=usedKitInfoMenu;
	
	usedKitInfoMenu=NewMenu;
	if (l_DrawKitSelectInfo.num<=0) {
		usedKitInfoMenu=0;
		return;
	} else if (usedKitInfoMenu<0) {
		usedKitInfoMenu=l_DrawKitSelectInfo.num-1;
	} else if (usedKitInfoMenu>=l_DrawKitSelectInfo.num) {
		usedKitInfoMenu=0;
	};

	ALLVOID NextCall=NULL;
	if (lastKitInfoMenu!=usedKitInfoMenu) {
		if (OnHideMenuFuncs[lastKitInfoMenu] != 0) {
			NextCall=(ALLVOID)OnHideMenuFuncs[lastKitInfoMenu];
			NextCall();
		};
	};
	
	if (lastKitInfoMenu!=usedKitInfoMenu || ForceShowMenuFunc) {
		dksiSetMenuTitle("\0");
		
		if (OnShowMenuFuncs[usedKitInfoMenu] != 0) {
			NextCall=(ALLVOID)OnShowMenuFuncs[usedKitInfoMenu];
			NextCall();
		};
	};
};


KEXPORT BYTE GetLCMStadium()
{
	return lcmStadium;
};

KEXPORT void SetLCMStadium(BYTE newStadium)
{
	lcmStadium=newStadium;
	return;
};

//-------------------------------------------
//===========================================
//-------------------------------------------

void AddToLine(CALLLINE* cl,DWORD addr)
{
	if (addr==0) return;
	
	for (int i=0;i<(cl->num);i++) {
		if (cl->addr[i]==0) continue;
		if (cl->addr[i]==addr) {
			if (cl==&l_DrawKitSelectInfo)
				lastAddedDrawKitSelectInfo=i;
			return;
		};
	};	

	BiggerLine(cl);
	cl->addr[cl->num-1]=addr;
	
	if (cl==&l_DrawKitSelectInfo) {
		lastAddedDrawKitSelectInfo=cl->num-1;
		numKitInfoMenus++;
	};
	
	return;
};

void RemoveFromLine(CALLLINE* cl,DWORD addr)
{
	if (addr==0) return;
	
	for (int i=0;i<(cl->num);i++) {
		if (cl->addr[i]==0) continue;
		if (cl->addr[i]==addr) {
			cl->addr[i]=0;
			if (cl==&l_DrawKitSelectInfo) {
				OnShowMenuFuncs[i]=0;
				OnHideMenuFuncs[i]=0;
				if (numKitInfoMenus>0)
					numKitInfoMenus--;
			};
		};
	};
	return;
};

void ClearLine(CALLLINE* cl)
{
	cl->num=0;
	delete cl->addr;
	cl->addr=NULL;
	return;
};

void BiggerLine(CALLLINE* cl)
{
	DWORD old=cl->num;
	DWORD num=old+1;
	DWORD *tmp=new DWORD[num];
	
	memcpy(tmp,cl->addr,old*sizeof(DWORD));
	delete cl->addr;
	cl->addr=tmp;
	cl->num=num;
	
	//Special handling for l_DrawKitSelectInfo
	if (cl==&l_DrawKitSelectInfo) {
		tmp=new DWORD[num];
		memcpy(tmp,OnShowMenuFuncs,old*sizeof(DWORD));
		delete OnShowMenuFuncs;
		OnShowMenuFuncs=tmp;
		
		tmp=new DWORD[num];
		memcpy(tmp,OnHideMenuFuncs,old*sizeof(DWORD));
		delete OnHideMenuFuncs;
		OnHideMenuFuncs=tmp;
		
		OnShowMenuFuncs[old]=0;
		OnHideMenuFuncs[old]=0;
	};
	
	return;
};

KEXPORT void HookFunction(HOOKS h,DWORD addr)
{
	if (h==hk_OnShowMenu) {
		if (lastAddedDrawKitSelectInfo!=0xFFFFFFFF)
			OnShowMenuFuncs[lastAddedDrawKitSelectInfo]=addr;
	} else if (h==hk_OnHideMenu) {
		if (lastAddedDrawKitSelectInfo!=0xFFFFFFFF)
			OnHideMenuFuncs[lastAddedDrawKitSelectInfo]=addr;
	} else {
		CALLLINE *cl=LineFromID(h);
		AddToLine(cl,addr);
	};
	return;
};

KEXPORT void UnhookFunction(HOOKS h,DWORD addr)
{
	if (h==hk_OnShowMenu || h==hk_OnHideMenu)
		return;

	CALLLINE *cl=LineFromID(h);
	RemoveFromLine(cl,addr);
	return;
};

CALLLINE* LineFromID(HOOKS h)
{
	CALLLINE *cl=NULL;
	switch (h) {
		case hk_D3D_GetDeviceCaps: cl = &l_D3D_GetDeviceCaps; break;
		case hk_D3D_Create: cl = &l_D3D_Create; break;
		case hk_D3D_CreateDevice: cl = &l_D3D_CreateDevice; break;
		case hk_D3D_Present: cl = &l_D3D_Present; break;
		case hk_D3D_Reset: cl = &l_D3D_Reset; break;
		case hk_D3D_CreateTexture: cl = &l_D3D_CreateTexture; break;
		case hk_D3D_AfterCreateTexture: cl = &l_D3D_AfterCreateTexture; break;
		case hk_ReadFile: cl = &l_ReadFile; break;
		case hk_BeginUniSelect: cl = &l_BeginUniSelect; break;
		case hk_EndUniSelect: cl = &l_EndUniSelect; break;
		case hk_Unpack: cl = &l_Unpack; break;
		case hk_UniDecode: cl = &l_UniDecode; break;
		case hk_GetClubTeamInfo: cl = &l_GetClubTeamInfo; break;
		case hk_GetNationalTeamInfo: cl = &l_GetNationalTeamInfo; break;
		case hk_GetClubTeamInfoML1: cl = &l_GetClubTeamInfoML1; break;
		case hk_GetClubTeamInfoML2: cl = &l_GetClubTeamInfoML2; break;
		case hk_GetNationalTeamInfoExitEdit: cl = &l_GetNationalTeamInfoExitEdit; break;
		case hk_AllocMem: cl = &l_AllocMem; break;
		case hk_SetLodMixerData: cl = &l_SetLodMixerData; break;
		case hk_GetPlayerInfo: cl = &l_GetPlayerInfo; break;
		case hk_BeforeUniDecode: cl = &l_BeforeUniDecode; break;
		case hk_FileFromAFS: cl = &l_FileFromAFS; break;
		case hk_BeforeFreeMemory: cl = &l_BeforeFreeMemory; break;
		case hk_ProcessPlayerData: cl = &l_ProcessPlayerData; break;
		case hk_DrawKitSelectInfo: cl = &l_DrawKitSelectInfo; break;
		case hk_Input: cl = &l_Input; break;
		case hk_OnShowMenu: cl = &l_OnShowMenu; break;
		case hk_OnHideMenu: cl = &l_OnHideMenu; break;
		case hk_UniSplit: cl = &l_UniSplit; break;
		case hk_AfterReadFile: cl = &l_AfterReadFile; break;
		case hk_D3D_UnlockRect: cl = &l_D3D_UnlockRect; break;
	};
	return cl;
};

void InitAddresses(int v)
{
	// select correct addresses
	memcpy(code, codeArray[v], sizeof(code));
    memcpy(data, dataArray[v], sizeof(data));
	memcpy(gpi, gpiArray[v], sizeof(gpi));

	// assign pointers	
	BeginUniSelect = (BEGINUNISELECT)code[C_BEGINUNISELECT];
	EndUniSelect = (ENDUNISELECT)code[C_ENDUNISELECT];
	Unpack = (UNPACK)code[C_UNPACK];
	UniDecode = (UNIDECODE)code[C_UNIDECODE];
	GetNationalTeamInfo = (GETTEAMINFO)code[C_GETNATIONALTEAMINFO];
	GetClubTeamInfo = (GETTEAMINFO)code[C_GETCLUBTEAMINFO];
	AllocMem=(ALLOCMEM)code[C_ALLOCMEM];
	SetLodMixerData = (SETLODMIXERDATA)code[C_LODMIXER_HOOK_ORG];
	SetLodMixerData2 = (SETLODMIXERDATA)code[C_LODMIXER_HOOK_ORG2];
	oGetPlayerInfo = (GETPLAYERINFO)code[C_GETPLAYERINFO];
	oFreeMemory = (FREEMEMORY)code[C_FREEMEMORY];
	UniSplit = (UNISPLIT)code[C_UNISPLIT];
	
	return;
};
