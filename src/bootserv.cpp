// bootserv.cpp
#include <windows.h>
#include <stdio.h>
#include "bootserv.h"
#include "kload_exp.h"

KMOD k_boot={MODID,NAMELONG,NAMESHORT,DEFAULT_DEBUG};

HINSTANCE hInst;

#define DATALEN 7
enum {
    EDITMODE_CURRPLAYER_MOD, EDITMODE_CURRPLAYER_ORG,
    EDITMODE_FLAG, EDITPLAYER_ID,
    PLAYERS_LINEUP, LINEUP_RECORD_SIZE, LINEUP_BASEID,
};
DWORD dataArray[][DATALEN] = {
    // PES6
    {
        0x112e1c0, 0x112e1c8,
        0x1108488, 0x112e24a,
        0x3bdcbcc, 0x240, 0x2487,
    },
    // PES6 1.10
    {
        0x112f1c0, 0x112f1c8,
        0x1109488, 0x112f24a,
        0x3bddbcc, 0x240, 0x2487,
    },
};

DWORD data[DATALEN];

//////////////////////////////////////////////////////

EXTERN_C BOOL WINAPI DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID lpReserved);
void bootInit(IDirect3D8* self, UINT Adapter,
    D3DDEVTYPE DeviceType, HWND hFocusWindow, DWORD BehaviorFlags,
    D3DPRESENT_PARAMETERS *pPresentationParameters, 
    IDirect3DDevice8** ppReturnedDeviceInterface);
void DumpTexture(IDirect3DTexture8* const ptexture);
void ReplaceTextureLevel(IDirect3DTexture8* srcTexture, IDirect3DTexture8* repTexture, UINT level);
void ReplaceTexture(IDirect3DTexture8* srcTexture, IDirect3DTexture8* repTexture, UINT levels);
KEXPORT void bootUnlockRect(IDirect3DTexture8* self,UINT Level);
//HRESULT STDMETHODCALLTYPE bootCreateTexture(IDirect3DDevice8* self, UINT width, UINT height,
//    UINT levels, DWORD usage, D3DFORMAT format, D3DPOOL pool, IDirect3DTexture8** ppTexture, 
//    DWORD src, bool* IsProcessed);

//////////////////////////////////////////////////////
//
// FUNCTIONS
//
//////////////////////////////////////////////////////

EXTERN_C BOOL WINAPI DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID lpReserved)
{
	if (dwReason == DLL_PROCESS_ATTACH)
	{
		Log(&k_boot,"Attaching dll...");
		hInst=hInstance;
		RegisterKModule(&k_boot);
		HookFunction(hk_D3D_CreateDevice,(DWORD)bootInit);
		
	}
	else if (dwReason == DLL_PROCESS_DETACH)
	{
		Log(&k_boot,"Detaching dll...");
		UnhookFunction(hk_D3D_CreateDevice,(DWORD)bootInit);
        UnhookFunction(hk_D3D_UnlockRect,(DWORD)bootUnlockRect);
	}

	return true;
}

void bootInit(IDirect3D8* self, UINT Adapter,
    D3DDEVTYPE DeviceType, HWND hFocusWindow, DWORD BehaviorFlags,
    D3DPRESENT_PARAMETERS *pPresentationParameters, 
    IDirect3DDevice8** ppReturnedDeviceInterface) 
{
    Log(&k_boot, "Initializing bootserver...");
    memcpy(data, dataArray[GetPESInfo()->GameVersion], sizeof(data));
    HookFunction(hk_D3D_UnlockRect,(DWORD)bootUnlockRect);
    //HookFunction(hk_D3D_CreateTexture,(DWORD)bootCreateTexture);
}

bool isEditMode()
{
    return *(BYTE*)data[EDITMODE_FLAG] == 1;
}

KEXPORT void bootUnlockRect(IDirect3DTexture8* self, UINT Level) 
{
    DWORD oldEBP;
    __asm mov oldEBP,ebp;

    static int count = 0;

    int levels = self->GetLevelCount();
    D3DSURFACE_DESC desc;
    if (SUCCEEDED(self->GetLevelDesc(0, &desc))) {
        if (((desc.Width==512 && desc.Height==256 && Level==0) 
                    || (desc.Width==128 && desc.Height==64 && Level==0))
                && *(DWORD*)(oldEBP+0x3c)==0 
                && *(DWORD*)(oldEBP+0x34) + 8 == *(DWORD*)(oldEBP+0x38)
                && *(DWORD*)(oldEBP+0x2c)<0x0e000000
                && *(DWORD*)(oldEBP+0x90)<0x7000000
                && *(DWORD*)(oldEBP+0x98)!=0x4107b4
                ) {

            WORD playerId = 0xffff;
            if (isEditMode()) {
                // edit player
                playerId = *(WORD*)data[EDITPLAYER_ID];
            } else {
                // match or training
                int pos = *(WORD*)(*(DWORD*)(oldEBP+0x30)) - data[LINEUP_BASEID];
                playerId = *(WORD*)(data[PLAYERS_LINEUP] + data[LINEUP_RECORD_SIZE]*pos + 2);
            }
            LogWithNumber(&k_boot, "bootUnlockRect: playerId = %d", playerId);

            if (0xb9 <= playerId && playerId <= 0xc3) { // France's first 11
                // replace with adidas boots
                IDirect3DTexture8* bootTexture;
                char filename[100];
                sprintf(filename,"kitserver\\bootwork\\boot_%02d.png", (playerId - 0xb9)%11);
                if (SUCCEEDED(D3DXCreateTextureFromFileEx(
                                GetActiveDevice(), filename,
                                desc.Width, desc.Height, levels, desc.Usage, 
                                desc.Format, D3DPOOL_MANAGED,
                                D3DX_DEFAULT, D3DX_DEFAULT,
                                0, NULL, NULL, &bootTexture))) {
                    //DumpTexture(self);

                    LogWithNumber(&k_boot,"bootUnlockRect: self = %08x", (DWORD)self);
                    LogWithNumber(&k_boot,"bootUnlockRect: oldEBP = %08x", oldEBP);


                    // replace the texture
                    ReplaceTextureLevel(self, bootTexture, 0);
                    bootTexture->Release();

                } else {
                    LogWithString(&k_boot, "D3DXCreateTextureFromFileEx FAILED for %s", filename);
                }
            } // end if playerId
        }

    } else {
        Log(&k_boot, "GetLevelDesc FAILED");
    }
}

void ReplaceTextureLevel(IDirect3DTexture8* srcTexture, IDirect3DTexture8* repTexture, UINT level)
{
    IDirect3DSurface8* src = NULL;
    IDirect3DSurface8* dest = NULL;

    if (SUCCEEDED(srcTexture->GetSurfaceLevel(level, &dest))) {
        if (SUCCEEDED(repTexture->GetSurfaceLevel(level, &src))) {
            if (SUCCEEDED(D3DXLoadSurfaceFromSurface(
                            dest, NULL, NULL,
                            src, NULL, NULL,
                            D3DX_FILTER_NONE, 0))) {
                //LogWithNumber(&k_boot,"ReplaceTextureLevel: replacing level %d COMPLETE", level);

            } else {
                //LogWithNumber(&k_boot,"ReplaceTextureLevel: replacing level %d FAILED", level);
            }
            src->Release();
        }
        dest->Release();
    }
}

void ReplaceTexture(IDirect3DTexture8* srcTexture, IDirect3DTexture8* repTexture, UINT levels)
{
    LogWithNumber(&k_boot, "ReplaceTexture: replacing texture with levels count = %d", levels);

    for (int i=0; i<levels; i++) {
        ReplaceTextureLevel(srcTexture, repTexture, i);
    }
}

void DumpTexture(IDirect3DTexture8* const ptexture) 
{
    static int count = 0;
    char buf[BUFLEN];
    //sprintf(buf,"kitserver\\boot_tex%03d.bmp",count++);
    sprintf(buf,"kitserver\\%03d_tex_%08x.bmp",count++,(DWORD)ptexture);
    if (FAILED(D3DXSaveTextureToFile(buf, D3DXIFF_BMP, ptexture, NULL))) {
        LogWithString(&k_boot, "DumpTexture: failed to save texture to %s", buf);
    }
}

