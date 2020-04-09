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
#include "afsreplace.h"

extern KMOD k_kload;
extern PESINFO g_pesinfo;
extern KLOAD_CONFIG g_config;
int bootserverVersion=0;

static const DWORD GAMECAM_SHIFT = 0x30;

#define CODELEN 40

enum {
    C_GETCLUBTEAMINFO, C_GETNATIONALTEAMINFO,
    C_GETCLUBTEAMINFO_CS, C_GETNATIONALTEAMINFO_CS,
    C_GETCLUBTEAMINFO_CS2, C_GETNATIONALTEAMINFO_CS2,
    C_GETCLUBTEAMINFO_CS_ML1, C_GETNATIONALTEAMINFO_CS_EXIT_EDIT,
    C_GETCLUBTEAMINFO_CS_ML2, C_GETCLUBTEAMINFO_CS3,
    C_BEGINUNISELECT, C_BEGINUNISELECT_CS,
    C_ENDUNISELECT, C_ENDUNISELECT_CS,
    C_LODMIXER_HOOK_ORG, C_LODMIXER_HOOK_CS,
    C_LODMIXER_HOOK_ORG2, C_LODMIXER_HOOK_CS2,
    C_GETPLAYERINFO_OLD, C_GETPLAYERINFO_JMP_OLD,
    C_FREEMEMORY, C_FREEMEMORY_JMP,
    C_PROCESSPLAYERDATA_JMP, C_CALL050,
    C_UNISPLIT, C_UNISPLIT_CS1, C_UNISPLIT_CS2,
    C_UNISPLIT_CS3, C_UNISPLIT_CS4, C_UNISPLIT_CS5,
    C_UNISPLIT_CS6, C_UNISPLIT_CS7, C_UNISPLIT_CS8,
    C_GETTEAMINFO, C_GETTEAMINFO_CS,
    C_CLEANINPUTTABLE_HOOK,
    C_PES_GETTEXTURE, C_PES_GETTEXTURE_CS1,
    C_BEGIN_RENDERPLAYER_CS, C_BEGIN_RENDERPLAYER_JUMPHACK,
};

// Code addresses.
DWORD codeArray[][CODELEN] = {
    // PES6
    {
      0, 0, //maybe 0x865240 (0x9690a5), 0x8654d0 (0x9690b8)
      0, 0,
      0, 0,
      0, 0,
      0, 0,
      0x9cfeb0, 0x9d078c,
      0x9ed490, 0x9cfd27,
      0x6d3b60, 0x6b8099,
      0, 0,
      0, 0,
      0, 0, //0x876f20, 0x45bc5c, // not used
      0x5fe506, 0x40c848,
      0, 0, 0,
      0, 0, 0,
      0, 0, 0,
      0x865240, 0x804cda,
      0x9cd4f2,
      0x408c20, 0x402723,
      0x8acf93, 0x8acfeb,
    },
    // PES6 1.10
    {
      0, 0,
      0, 0,
      0, 0,
      0, 0,
      0, 0,
      0x9d0040, 0x9d091c,
      0x9ed5f0, 0x9cfeb7,
      0x6d3cc0, 0x6b81b9,
      0, 0,
      0, 0,
      0, 0, //0x877060, 0x45bc9c,
      0x5fe566, 0x40c898,
      0, 0, 0,
      0, 0, 0,
      0, 0, 0,
      0x865370, 0x804e5a,
      0x9cd682,
      0x408d90, 0x402723,
      0x8ad0f3, 0x8ad14b,
    },
    // WE2007
    {
      0, 0,
      0, 0,
      0, 0,
      0, 0,
      0, 0,
      0x9d06a0, 0x9d0f7c,
      0x9edc90, 0x9d0517,
      0x6d3b30, 0x6b8039,
      0, 0,
      0, 0,
      0, 0, //0x0, 0x0,
      0x5fe576, 0x40c808,
      0, 0, 0,
      0, 0, 0,
      0, 0, 0,
      0x865950, 0x804caa, // no need to find these, not used
      0x9cdce2,
      0x408d20, 0x402713,
      0x8ad813, 0x8ad86b,
    },
};

// dta addresses
#define DATALEN 11
enum { INPUT_TABLE, STACK_SHIFT, RESMEM1, RESMEM2, RESMEM3,
    PLAYERS_LINEUP, LINEUP_RECORD_SIZE, PLAYERDATA_BASE, GAME_MODE, EDITMODE_FLAG,
    EDITPLAYER_ID };

DWORD dtaArray[][DATALEN] = {
    //PES6
    {
        0x3a71254, 0, 0x8c7b6d, 0x8c7b82, 0x8c7b99,
        0x3bdc980, 0x240, 0x3bcf55c, 0x3be12c9, 0x1108488,
        0x112e24a,
    },
    //PES6 1.10
    {
        0x3a72254, 0 /*0x2c*/, 0x8c7c3d, 0x8c7c52, 0x8c7c69,
        0x3bdd980, 0x240, 0x3bd055c, 0x3be22c9, 0x1109488,
        0x112f24a,
    },
    //WE2007
    {
        0x3a6bcd4, 0, 0x8c836d, 0x8c8382, 0x8c8399,
        0x3bd7400, 0x240, 0x3bc9fdc, 0x3bdbd49, 0x1102f08,
        0x1128cb2,
    },
};

BYTE _shortJumpHack2[][2] = {
    //PES6
    {0xeb,0x37},
    //PES6 1.10
    {0,0}, //later! but where is this used?
    //WE2007
    {0x0,0x0}, //later!
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
    //WE2007
    {
        0,0,0,0,0,
        0,0,0,0,0,
        0,0,0,0,0,
        0,0,0,0
    },
};

DWORD code[CODELEN];
DWORD dta[DATALEN];
DWORD gpi[GPILEN];

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
bool g_fontInitialized = false;
CD3DFont* g_font12 = NULL;
CD3DFont* g_font16 = NULL;
CD3DFont* g_font20 = NULL;

BYTE g_codeFragment[5] = {0,0,0,0,0};
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

bool bGetNationalTeamInfoHooked = false;
bool bGetNationalTeamInfoExitEditHooked = false;
bool bGetClubTeamInfoHooked = false;
bool bBeginUniSelectHooked = false;
bool bEndUniSelectHooked = false;
bool bSetLodMixerDataHooked = false;
bool bGetPlayerInfoOldHooked = false;
bool bGetPlayerInfoOldJmpHooked = false;
bool bFreeMemoryHooked = false;
bool bProcessPlayerDataHooked = false;
bool bUniSplitHooked = false;
bool bPesGetTextureHooked = false;
bool bBeginRenderPlayerHooked = false;
bool bBeginRenderPlayerJumpHackHooked = false;

GETTEAMINFO GetNationalTeamInfo = NULL;
GETTEAMINFO GetClubTeamInfo = NULL;
BEGINUNISELECT BeginUniSelect = NULL;
ENDUNISELECT EndUniSelect = NULL;
SETLODMIXERDATA SetLodMixerData = NULL;
SETLODMIXERDATA SetLodMixerData2 = NULL;
GETPLAYERINFO_OLD oGetPlayerInfo = NULL;
FREEMEMORY oFreeMemory = NULL;
UNISPLIT UniSplit = NULL;
PES_GETTEXTURE orgPesGetTexture = NULL;


CALLLINE l_D3D_Create={0,NULL};
CALLLINE l_D3D_GetDeviceCaps={0,NULL};
CALLLINE l_D3D_CreateDevice={0,NULL};
CALLLINE l_D3D_Present={0,NULL};
CALLLINE l_D3D_Reset={0,NULL};
CALLLINE l_D3D_CreateTexture={0,NULL};
CALLLINE l_D3D_AfterCreateTexture={0,NULL};
CALLLINE l_BeginUniSelect={0,NULL};
CALLLINE l_EndUniSelect={0,NULL};
CALLLINE l_GetClubTeamInfo={0,NULL};
CALLLINE l_GetNationalTeamInfo={0,NULL};
CALLLINE l_GetClubTeamInfoML1={0,NULL};
CALLLINE l_GetClubTeamInfoML2={0,NULL};
CALLLINE l_GetNationalTeamInfoExitEdit={0,NULL};
CALLLINE l_SetLodMixerData={0,NULL};
CALLLINE l_GetPlayerInfoOld={0,NULL};
CALLLINE l_BeforeFreeMemory={0,NULL};
CALLLINE l_ProcessPlayerData={0,NULL};
CALLLINE l_DrawKitSelectInfo={0,NULL};
CALLLINE l_Input={0,NULL};
CALLLINE l_OnShowMenu={0,NULL};
CALLLINE l_OnHideMenu={0,NULL};
CALLLINE l_UniSplit={0,NULL};
CALLLINE l_D3D_UnlockRect={0,NULL};
CALLLINE l_PesGetTexture={0,NULL};
CALLLINE l_BeginRenderPlayer={0,NULL};

void HookDirect3DCreate8()
{
    BYTE g_jmp[5] = {0,0,0,0,0};
    // Put a JMP-hook on Direct3DCreate8
    Log(&k_kload,"JMP-hooking Direct3DCreate8...");

    char d3dDLL[5];
    switch (g_pesinfo.GameVersion) {
    case gvPES6PC110:
        strcpy(d3dDLL,"d3dw");
        break;
    default:
        strcpy(d3dDLL,"d3d8");
        break;
    };

    HMODULE hD3D8 = GetModuleHandle(d3dDLL);
    if (!hD3D8) {
        hD3D8 = LoadLibrary(d3dDLL);
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

void HookOthers()
{
    bBeginUniSelectHooked = HookProc(C_BEGINUNISELECT, C_BEGINUNISELECT_CS, (DWORD)NewBeginUniSelect,
                        "C_BEGINUNISELECT", "C_BEGINUNISELECT_CS");

    bEndUniSelectHooked = HookProc(C_ENDUNISELECT, C_ENDUNISELECT_CS, (DWORD)NewEndUniSelect,
        "C_ENDUNISELECT", "C_ENDUNISELECT_CS");

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

/*
    // hook GetPlayerInfoOld
    char tmp[BUFLEN];
    bGetPlayerInfoOldHooked=true;
    for (int i=0;i<GPILEN;i++)
        if (gpi[i]!=0) {
            sprintf(tmp,"C_GETPLAYERINFO_OLD_CS%d",i+1);
            bGetPlayerInfoOldHooked&=HookProcAtAddr(code[C_GETPLAYERINFO_OLD], gpi[i],
                (DWORD)NewGetPlayerInfoOld,"C_GETPLAYERINFO_OLD", tmp);
        };
    */
    DWORD* ptr;
    BYTE* bptr;
    DWORD protection=0,newProtection=PAGE_EXECUTE_READWRITE;
    /*
    // hook jmp to GetPlayerInfoOld
    if (code[C_GETPLAYERINFO_JMP_OLD] != 0)
    {
        ptr = (DWORD*)(code[C_GETPLAYERINFO_JMP_OLD] + 1);
        // save original code for JMP GetPlayerInfoOld
        memcpy(g_gpiJmpCode, ptr, 4);

        if (VirtualProtect(ptr, 4, newProtection, &protection)) {
            ptr[0] = (DWORD)NewGetPlayerInfoOld - (DWORD)(code[C_GETPLAYERINFO_JMP_OLD] + 5);
            bGetPlayerInfoOldJmpHooked = true;
            Log(&k_kload,"Jump to GetPlayerInfoOld HOOKED at code[C_GETPLAYERINFO_JMP_OLD]");
        };
    }
    */

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

    if (bootserverVersion==VERSION_ROBBIE) {
        // hook PesGetTexure
        bPesGetTextureHooked = HookProc(C_PES_GETTEXTURE,C_PES_GETTEXTURE_CS1,(DWORD)NewPesGetTexture,
            "C_PES_GETTEXTURE","C_PES_GETTEXTURE_CS1");

        // hook BeginRenderPlayer
        if (code[C_BEGIN_RENDERPLAYER_CS] != 0 && code[C_BEGIN_RENDERPLAYER_JUMPHACK] != 0)
        {
            bptr = (BYTE*)(code[C_BEGIN_RENDERPLAYER_CS]);
            ptr = (DWORD*)(code[C_BEGIN_RENDERPLAYER_CS] + 1);

            if (VirtualProtect(bptr, 9, newProtection, &protection)) {
                bptr[0]=0xe8; //call
                bptr[5]=0x33; //xor ebx, ebx
                bptr[6]=0xdb;
                bptr[7]=0xeb; //jump back
                bptr[8]=code[C_BEGIN_RENDERPLAYER_JUMPHACK] - (code[C_BEGIN_RENDERPLAYER_CS] + 7);
                ptr[0] = (DWORD)NewBeginRenderPlayer - (DWORD)(code[C_BEGIN_RENDERPLAYER_CS] + 5);
                bBeginRenderPlayerHooked = true;
                Log(&k_kload,"BeginRenderPlayer HOOKED at code[C_BEGIN_RENDERPLAYER_CS]");
            };

            bptr = (BYTE*)(code[C_BEGIN_RENDERPLAYER_JUMPHACK]);
            if (VirtualProtect(bptr, 2, newProtection, &protection)) {
                bptr[0]=0xeb;
                bptr[1]=0x100 + code[C_BEGIN_RENDERPLAYER_CS] - (code[C_BEGIN_RENDERPLAYER_JUMPHACK] + 2);
                bBeginRenderPlayerJumpHackHooked = true;
                Log(&k_kload,"BeginRenderPlayer Short-Jump-Hack installed.");
            }
        };
    };

    // hook num-pages and AfsReplace
    //HookGetNumPages();
    HookAfsReplace();
    return;
};

void UnhookOthers()
{
    // hook GetNumPagesForFileInAFS and AfsReplace
    UnhookGetNumPages();
    UnhookAfsReplace();


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

        /*
        // unhook GetPlayerInfoOld
        char tmp[BUFLEN];
        bGetPlayerInfoOldHooked=false;
        for (int i=0;i<GPILEN;i++)
            if (gpi[i]!=0) {
                sprintf(tmp,"C_GETPLAYERINFO_OLD_CS%d",i+1);
                bGetPlayerInfoOldHooked|=(!UnhookProcAtAddr(true,code[C_GETPLAYERINFO_OLD], gpi[i],
                    "C_GETPLAYERINFO_OLD", tmp));
            };

        // unhook jmp to GetPlayerInfoOld
        if (bGetPlayerInfoOldJmpHooked)
        {
            DWORD* ptr = (DWORD*)(code[C_GETPLAYERINFO_JMP_OLD] + 1);
            memcpy(ptr,g_gpiJmpCode,4);
            bGetPlayerInfoOldHooked = false;
            Log(&k_kload,"Jump to GetPlayerInfoOld UNHOOKED");
        }

        ClearLine(&l_GetPlayerInfoOld);
        */
        //unhook ProcessPlayerData
        bProcessPlayerDataHooked=MasterUnhookFunction(code[C_PROCESSPLAYERDATA_JMP],
                                                        NewProcessPlayerData);
        Log(&k_kload,"Jump to ProcessPlayerData UNHOOKED");

        if (bootserverVersion==VERSION_ROBBIE) {
            // unhook PesGetTexure
            bPesGetTextureHooked = !UnhookProc(bPesGetTextureHooked,C_PES_GETTEXTURE,
                C_PES_GETTEXTURE_CS1,"C_PES_GETTEXTURE","C_PES_GETTEXTURE_CS1");

            ClearLine(&l_PesGetTexture);
            ClearLine(&l_BeginRenderPlayer);
        };

    return;
};

void UnhookKeyb()
{
    if (g_hKeyboardHook != NULL) {
        UnhookWindowsHookEx(g_hKeyboardHook);
        Log(&k_kload,"Keyboard hook uninstalled.");
        g_hKeyboardHook = NULL;
    };

    kloadDestroyFonts();
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
    HookGameInput(code[C_CLEANINPUTTABLE_HOOK], dta[INPUT_TABLE]);

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
DWORD NewGetPlayerInfoOld()
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
    int i;
    for (i=0;i<GPILEN;i++)
        if (Caller==gpi[i]+5) {
            Caller=i;
            break;
        };

    //LogWithNumber(&k_kload,"Caller is %x",Caller);

    PlayerNumber&=0xFFFF;

    CGETPLAYERINFO_OLD NextCall=NULL;
    for (i=0;i<(l_GetPlayerInfoOld.num);i++)
    if (l_GetPlayerInfoOld.addr[i]!=0) {
        NextCall=(CGETPLAYERINFO_OLD)l_GetPlayerInfoOld.addr[i];
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
    Log(&k_kload,"calling InitAfsReplace()...");
    InitAfsReplace();

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

    HookOthers();
    if (g_hKeyboardHook == NULL) {
        g_hKeyboardHook = SetWindowsHookEx(WH_KEYBOARD, KeyboardProc, hInst, GetCurrentThreadId());
        LogWithNumber(&k_kload,"Installed keyboard hook: g_hKeyboardHook = %d", (DWORD)g_hKeyboardHook);
    };
    FixReservedMemory();

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

void kloadMakeFonts(IDirect3DDevice8* dev)
{
    g_font12 = new CD3DFont(L"Arial",12*stretchY,D3DFONT_BOLD);
    g_font12->InitDeviceObjects(dev);
    g_font12->RestoreDeviceObjects();

    g_font16 = new CD3DFont(L"Arial",16*stretchY,D3DFONT_BOLD);
    g_font16->InitDeviceObjects(dev);
    g_font16->RestoreDeviceObjects();

    g_font20 = new CD3DFont(L"Arial",20*stretchY,D3DFONT_BOLD);
    g_font20->InitDeviceObjects(dev);
    g_font20->RestoreDeviceObjects();

    LogWithThreeNumbers(&k_kload,"Fonts created: g_font12=%08x, g_font16=%08x, g_font20=%08x",
        (DWORD)g_font12, (DWORD)g_font16, (DWORD)g_font20);
}

void kloadDestroyFonts()
{
    SAFE_DELETE(g_font12);
    SAFE_DELETE(g_font16);
    SAFE_DELETE(g_font20);
    Log(&k_kload, "Fonts destroyed");
}

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

    if (*(DWORD*)(oldEBP+4+dta[STACK_SHIFT])==code[C_CALL050]+3) {
        src=*(DWORD*)(oldEBP+0x74+dta[STACK_SHIFT]);
        if (src!=0 && !IsBadReadPtr((LPVOID)src,4)) {
            src=*(DWORD*)(src+0x18);
            //TRACE2(&k_kload,"src = %x",src);
        } else
            src=0;
    }
    else if (*(DWORD*)(oldEBP+4+dta[STACK_SHIFT]+GAMECAM_SHIFT)==code[C_CALL050]+3) {
        src=*(DWORD*)(oldEBP+0x74+dta[STACK_SHIFT]+GAMECAM_SHIFT);
        if (src!=0 && !IsBadReadPtr((LPVOID)src,4)) {
            src=*(DWORD*)(src+0x18);
            //TRACE2(&k_kload,"src = %x",src);
        } else
            src=0;
    }

    int i;
    for (i=0;i<(l_D3D_CreateTexture.num);i++)
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

    //if (vtable[VTAB_SETRENDERSTATE] != (DWORD)NewSetRenderState) {
    if (!g_orgSetRenderState) {
        g_orgSetRenderState = (PFNSETRENDERSTATEPROC)vtable[VTAB_SETRENDERSTATE];
        if (VirtualProtect(vtable+VTAB_SETRENDERSTATE, 4, newProtection, &protection))
        {
            vtable[VTAB_SETRENDERSTATE] = (DWORD)NewSetRenderState;
            Log(&k_kload,"SetRenderState hooked.");
        }
    };

    //if (vtable[VTAB_SETTEXTURE] != (DWORD)NewSetTexture) {
    if (!g_orgSetTexture) {
        g_orgSetTexture = (PFNSETTEXTUREPROC)vtable[VTAB_SETTEXTURE];
        if (VirtualProtect(vtable+VTAB_SETTEXTURE, 4, newProtection, &protection))
        {
            vtable[VTAB_SETTEXTURE] = (DWORD)NewSetTexture;
            Log(&k_kload,"SetTexture hooked.");
        }
    };

    if (*ppTexture!=NULL) {
        vtable = (DWORD*)(*((DWORD*)*ppTexture));
        //if (vtable[VTAB_UNLOCKRECT] != (DWORD)NewUnlockRect) {
        if (!g_orgUnlockRect) {
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
    }

    if (!g_fontInitialized) {
        kloadMakeFonts(self);
        g_fontInitialized = true;
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

    // possibly delayed hooking of getnumpages
    static bool numPagesHooked = false;
    if (!numPagesHooked) {
        numPagesHooked = HookGetNumPages();
    }

    return res;
};

/* New Reset function */
HRESULT STDMETHODCALLTYPE NewReset(IDirect3DDevice8* self, D3DPRESENT_PARAMETERS *params)
{
    LogWithNumber(&k_kload,"NewReset: CALLED. caller = %08x", (DWORD)(*(&self-4)));
    LogWithNumber(&k_kload,"NewReset: BackBufferWidth=%d", params->BackBufferWidth);
    LogWithNumber(&k_kload,"NewReset: BackBufferHeight=%d", params->BackBufferHeight);
    LogWithNumber(&k_kload,"NewReset: Windowed=%d", params->Windowed);

    Log(&k_kload,"NewReset: cleaning-up.");

    Log(&k_kload,"NewReset: destroying fonts");
    kloadDestroyFonts();

    g_bGotFormat = false;

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

    Log(&k_kload,"NewReset: calling original");

    // CALL ORIGINAL FUNCTION
    HRESULT res = g_orgReset(self, params);

    //self->SetRenderState(D3DRS_MULTISAMPLEANTIALIAS, TRUE);

    stretchX=params->BackBufferWidth/1024.0;
    stretchY=params->BackBufferHeight/768.0;
    kloadMakeFonts(self);

    Log(&k_kload,"NewReset: DONE");
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

IDirect3DTexture8* STDMETHODCALLTYPE NewPesGetTexture(DWORD p1)
{
    DWORD oldEBP;
    __asm mov oldEBP, ebp

    IDirect3DTexture8* res=orgPesGetTexture(p1);

    DWORD orgEBP=*(DWORD*)oldEBP;
    DWORD p2=*(DWORD*)(orgEBP+8), p3=*(DWORD*)(orgEBP+0xc);


    CPES_GETTEXTURE NextCall=NULL;
    for (int i=0;i<(l_PesGetTexture.num);i++)
    if (l_PesGetTexture.addr[i]!=0) {
        NextCall=(CPES_GETTEXTURE)l_PesGetTexture.addr[i];
        NextCall(p1, p2, p3, &res);
    };

    return res;
}

void NewBeginRenderPlayer()
{
    DWORD oldEAX, oldEDI;

    __asm {
        mov oldEAX, eax
        mov oldEDI, edi
    };

    CBEGINRENDERPLAYER NextCall=NULL;
    for (int i=0;i<(l_BeginRenderPlayer.num);i++)
    if (l_BeginRenderPlayer.addr[i]!=0) {
        NextCall=(CBEGINRENDERPLAYER)l_BeginRenderPlayer.addr[i];
        NextCall(oldEDI);
    };

    __asm {
        mov edi, oldEDI
        mov eax, oldEAX
        xor ebx, ebx
    };
    return;
};

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

void DumpTexture(IDirect3DTexture8* const ptexture)
{
    static int count = 0;
    char buf[BUFLEN];
    //sprintf(buf,"kitserver\\boot_tex%03d.bmp",count++);
    sprintf(buf,"kitserver\\%03d_tex_%08x.bmp",count++,(DWORD)ptexture);
    if (FAILED(D3DXSaveTextureToFile(buf, D3DXIFF_BMP, ptexture, NULL))) {
        LogWithString(&k_kload, "DumpTexture: failed to save texture to %s", buf);
    }
    else
    {
        LogWithString(&k_kload, "DumpTexture: Saved texture to %s", buf);
    };
}

KEXPORT bool isEditMode()
{
    return *(BYTE*)dta[EDITMODE_FLAG] == 1;
}

KEXPORT DWORD editPlayerId()
{
    DWORD playerId = *(WORD*)dta[EDITPLAYER_ID];
    return playerId;
};

KEXPORT bool isTrainingMode()
{
    if (isEditMode()) return false;
    return *(BYTE*)dta[GAME_MODE] == 5;
};

KEXPORT bool isWatchReplayMode()
{
    if (isEditMode()) return false;
    return *(BYTE*)dta[GAME_MODE] == 10;
};

KEXPORT bool isMLMode()
{
    if (isEditMode()) return false;
    return *(BYTE*)dta[GAME_MODE] == 4;
};

KEXPORT PLAYER_RECORD* playerRecord(BYTE pos)
{
    if (pos>22) pos=1;
    return (PLAYER_RECORD*)(dta[PLAYERS_LINEUP] + pos*dta[LINEUP_RECORD_SIZE]);
};

KEXPORT DWORD getRecordPlayerId(BYTE pos)
{
    DWORD id=0;
    if (pos>22) return 0;

    //if (!isWatchReplayMode()) {
        PLAYER_RECORD* rec = playerRecord(pos);
        id=*(WORD*)(dta[PLAYERDATA_BASE] + (rec->team*0x20 + rec->posInTeam)*0x348 + 0x2a);
    /*} else {
        id=195;
    };*/
    return id;
};

KEXPORT IDirect3DTexture8* GetTextureFromColl(DWORD texColl, DWORD which)
{
    IDirect3DTexture8* res=NULL;

    DWORD p1=*(DWORD*)(texColl+(*(DWORD*)(texColl+0x40))*4+0x38) + which*4;
    DWORD p2=*(DWORD*)(texColl+(*(DWORD*)(texColl+0x40))*4+0x34) + which*4;
    p1=*(DWORD*)p1;
    p2=*(DWORD*)p2;

    //sorry for using assembler here, but else ecx is used for the parameter
    __asm {
        push p1
        mov ecx, p2 //doesn't work without this!
        call orgPesGetTexture
        mov res, eax
    };

    return res;
};

KEXPORT void SetTextureToColl(DWORD texColl, DWORD which, IDirect3DTexture8* tex)
{
    DWORD* p1=*(DWORD**)(texColl+(*(DWORD*)(texColl+0x40))*4+0x38) + which*4;
    DWORD* p2=*(DWORD**)(texColl+(*(DWORD*)(texColl+0x40))*4+0x34) + which*4;

    TEXTURE_INFO* ti = (TEXTURE_INFO*)HeapAlloc(GetProcessHeap(),
                            HEAP_ZERO_MEMORY, sizeof(TEXTURE_INFO));

    if (*p2 == 0) return;
        //if (**(DWORD**)p2 != 0)
            memcpy((BYTE*)ti, (BYTE*)*p2, sizeof(TEXTURE_INFO));
    LogWithNumber(&k_kload, "  ---> %x", (DWORD)ti);

    ti->refCount = 0xffff;  //don't want PES to free our memory
    ti->tex = NULL;//tex;
    //ti->unknown5 = 1;

    *p1=(DWORD)ti;
    *p2=(DWORD)ti;
    return;
};

KEXPORT IDirect3DTexture8* GetPlayerTexture(DWORD playerPos, DWORD texCollType, DWORD which, DWORD lodLevel)
{
    IDirect3DTexture8* res=NULL;

    if (lodLevel>4 || texCollType>3) return NULL;

    DWORD playerMainColl=*(playerRecord(playerPos)->texMain);
    playerMainColl=*(DWORD*)(playerMainColl+0x10);

    DWORD texColl=0;
    DWORD bodyNumTexs[2][5]={
        //0: body
        {
            9, 9, 7, 7, 4,
        },
        //1: body (training)
        {
            6, 6, 6, 6, 4,
        }
    };

    switch (texCollType) {
        case 0:
        case 1:
            texColl=*(DWORD*)(playerMainColl+0x14);
            texColl+=lodLevel*8;
            texColl=*(DWORD*)(texColl + 4);
            break;
        case 2:
            //if (lodLevel>=*(BYTE*)(playerMainColl+5)) return NULL;
            texColl=*(DWORD*)(playerMainColl+0x18);
            texColl+=lodLevel*0x1c; //actually not really lod, but other parameter
            texColl=*(DWORD*)(texColl + 8);
            break;
        case 3:
            if (lodLevel>=*(BYTE*)(playerMainColl+6)) return NULL;
            texColl=*(DWORD*)(playerMainColl+0x1c);
            texColl+=lodLevel*0x14; //actually not really lod, but other parameter
            texColl=*(DWORD*)(texColl + 8);
            break;
    };

    if (texColl == 0) return NULL;
    //if (which>=*(DWORD*)(texColl+(*(DWORD*)(texColl+0x40))*4+0x34) - 4) return NULL;

    DWORD p1=*(DWORD*)(texColl+(*(DWORD*)(texColl+0x40))*4+0x38) + which*4;
    DWORD p2=*(DWORD*)(texColl+(*(DWORD*)(texColl+0x40))*4+0x34) + which*4;
    p1=*(DWORD*)p1;
    p2=*(DWORD*)p2;

    //sorry for using assembler here, but else ecx is used for the parameter
    __asm {
        push p1
        mov ecx, p2 //doesn't work without this!
        call orgPesGetTexture
        mov res, eax
    };

    LogWithThreeNumbers(&k_kload,"### %d %d %d",texCollType, which, lodLevel);

    return res;
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

    /*
    // Dump all textures of team 1 goalie, for all lod levels
    if (!IsKitSelectMode && code1 >= 0 && code1==HC_ACTION && lParam & 0x80000000) {
        KEYCFG* keyCfg = GetInputCfg();
        if (wParam == keyCfg->keyboard.keyInfoPageNext) {
            for (int j=0;j<5;j++) {
                for (int i=0;i<9;i++) {
                    //DumpTexture(GetPlayerTexture(1, isTrainingMode()?1:0, i, j));
                    //DumpTexture(GetPlayerTexture(1, 3, i, 2));
                };
            };
        }
    }
    */

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
        case hk_BeginUniSelect: cl = &l_BeginUniSelect; break;
        case hk_EndUniSelect: cl = &l_EndUniSelect; break;
        case hk_GetClubTeamInfo: cl = &l_GetClubTeamInfo; break;
        case hk_GetNationalTeamInfo: cl = &l_GetNationalTeamInfo; break;
        case hk_GetClubTeamInfoML1: cl = &l_GetClubTeamInfoML1; break;
        case hk_GetClubTeamInfoML2: cl = &l_GetClubTeamInfoML2; break;
        case hk_GetNationalTeamInfoExitEdit: cl = &l_GetNationalTeamInfoExitEdit; break;
        case hk_SetLodMixerData: cl = &l_SetLodMixerData; break;
        case hk_GetPlayerInfoOld: cl = &l_GetPlayerInfoOld; break;
        case hk_BeforeFreeMemory: cl = &l_BeforeFreeMemory; break;
        case hk_ProcessPlayerData: cl = &l_ProcessPlayerData; break;
        case hk_DrawKitSelectInfo: cl = &l_DrawKitSelectInfo; break;
        case hk_Input: cl = &l_Input; break;
        case hk_OnShowMenu: cl = &l_OnShowMenu; break;
        case hk_OnHideMenu: cl = &l_OnHideMenu; break;
        case hk_UniSplit: cl = &l_UniSplit; break;
        case hk_D3D_UnlockRect: cl = &l_D3D_UnlockRect; break;
        case hk_PesGetTexture: cl = &l_PesGetTexture; break;
        case hk_BeginRenderPlayer: cl = &l_BeginRenderPlayer; break;
    };
    return cl;
};

void InitAddresses(int v)
{
    // select correct addresses
    memcpy(code, codeArray[v], sizeof(code));
    memcpy(dta, dtaArray[v], sizeof(dta));
    memcpy(gpi, gpiArray[v], sizeof(gpi));

    // assign pointers
    BeginUniSelect = (BEGINUNISELECT)code[C_BEGINUNISELECT];
    EndUniSelect = (ENDUNISELECT)code[C_ENDUNISELECT];
    GetNationalTeamInfo = (GETTEAMINFO)code[C_GETNATIONALTEAMINFO];
    GetClubTeamInfo = (GETTEAMINFO)code[C_GETCLUBTEAMINFO];
    SetLodMixerData = (SETLODMIXERDATA)code[C_LODMIXER_HOOK_ORG];
    SetLodMixerData2 = (SETLODMIXERDATA)code[C_LODMIXER_HOOK_ORG2];
    oGetPlayerInfo = (GETPLAYERINFO_OLD)code[C_GETPLAYERINFO_OLD];
    oFreeMemory = (FREEMEMORY)code[C_FREEMEMORY];
    UniSplit = (UNISPLIT)code[C_UNISPLIT];
    orgPesGetTexture = (PES_GETTEXTURE)code[C_PES_GETTEXTURE];

    return;
};

void FixReservedMemory()
{
    //make PES reserve more memory to avoid problems with HD adboards
    if (dta[RESMEM1]==0 || dta[RESMEM2]==0 || dta[RESMEM3]==0)
        return;

    DWORD oldResMem=*(DWORD*)dta[RESMEM1];

    if (g_config.newResMem <= oldResMem)
        return;

    DWORD protection=0, newProtection=PAGE_EXECUTE_READWRITE;
    if (VirtualProtect((BYTE*)dta[RESMEM1], 0xff, newProtection, &protection)) {
        *(DWORD*)dta[RESMEM1]=g_config.newResMem;
        *(DWORD*)dta[RESMEM2]=g_config.newResMem>>2;
        *(DWORD*)dta[RESMEM3]=g_config.newResMem;
        LogWithNumber(&k_kload,"Increased reserved memory to %d bytes",g_config.newResMem);
    };


    return;
};

KEXPORT void SetBootserverVersion(int version)
{
    bootserverVersion=version;
    return;
};
