// bootserv.cpp
#include <windows.h>
#include <stdio.h>
#include "bootserv.h"
#include "kload_exp.h"

KMOD k_boot={MODID,NAMELONG,NAMESHORT,DEFAULT_DEBUG};
HINSTANCE hInst;

#define DATALEN 6
enum {
    EDITMODE_CURRPLAYER_MOD, EDITMODE_CURRPLAYER_ORG, EDITPLAYER_ID,
    PLAYERS_LINEUP, LINEUP_RECORD_SIZE, LINEUP_BASEID,
};
DWORD dataArray[][DATALEN] = {
    // PES6
    {
        0x112e1c0, 0x112e1c8, 0x112e24a,
        0x3bdcbcc, 0x240, 0x2487,
    },
    // PES6 1.10
    {
        0x112f1c0, 0x112f1c8, 0x112f24a,
        0x3bddbcc, 0x240, 0x2487,
    },
};
DWORD data[DATALEN];

#define CODELEN 2
enum {
    C_COPYPLAYERDATA_CS,
    C_RESETFLAG2_CS,
};
DWORD codeArray[][CODELEN] = {
    // PES6
    {
        0xa4b4b2,
        0x9c15a7,
    },
    // PES6 1.10
    {
        0xa4b612,
        0x9c1737,
    },
};
DWORD code[CODELEN];

//////////////////////////////////////////////////////
// Globals ///////////////////////////////////////////

class TexturePack {
public:
    TexturePack() : _big(NULL),_small(NULL),_textureFile(),_bigLoaded(false),_smallLoaded(false) {}
    ~TexturePack() {
        if (_big) _big->Release();
        if (_small) _small->Release();
    }
    IDirect3DTexture8* _big;
    IDirect3DTexture8* _small;
    string _textureFile;
    bool _bigLoaded;
    bool _smallLoaded;
};

map<WORD,TexturePack> g_bootTexturePacks;
map<DWORD,IDirect3DTexture8*> g_bootTextures;
map<DWORD,DWORD> g_bootTexturesPos;

//////////////////////////////////////////////////////

EXTERN_C BOOL WINAPI DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID lpReserved);
void bootInit(IDirect3D8* self, UINT Adapter,
    D3DDEVTYPE DeviceType, HWND hFocusWindow, DWORD BehaviorFlags,
    D3DPRESENT_PARAMETERS *pPresentationParameters, 
    IDirect3DDevice8** ppReturnedDeviceInterface);
void DumpTexture(IDirect3DTexture8* const ptexture);
void ReplaceTextureLevel(IDirect3DTexture8* srcTexture, IDirect3DTexture8* repTexture, UINT level);
void ReplaceBootTexture(IDirect3DTexture8* srcTexture, IDirect3DTexture8* repTexture, UINT width);
KEXPORT void bootUnlockRect(IDirect3DTexture8* self,UINT Level);
KEXPORT DWORD bootCopyPlayerData(DWORD p0, DWORD p1, DWORD p2);
void readMap();
void bootBeginUniSelect();
DWORD bootResetFlag2();
void bootPesGetTexture(DWORD p1, DWORD p2, DWORD p3, IDirect3DTexture8** res);
void bootOnSetLodLevel(DWORD p1, DWORD level);
void bootGetLodTexture(DWORD p1, DWORD* res);


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
        //UnhookFunction(hk_D3D_UnlockRect,(DWORD)bootUnlockRect);
        UnhookFunction(hk_BeginUniSelect,(DWORD)bootBeginUniSelect);
        UnhookFunction(hk_PesGetTexture,(DWORD)bootPesGetTexture);
        UnhookFunction(hk_OnSetLodLevel,(DWORD)bootOnSetLodLevel);
        UnhookFunction(hk_GetLodTexture,(DWORD)bootGetLodTexture);
        
        g_bootTextures.clear();
        g_bootTexturesPos.clear();
	}

	return true;
}

void bootInit(IDirect3D8* self, UINT Adapter,
    D3DDEVTYPE DeviceType, HWND hFocusWindow, DWORD BehaviorFlags,
    D3DPRESENT_PARAMETERS *pPresentationParameters, 
    IDirect3DDevice8** ppReturnedDeviceInterface) 
{
    Log(&k_boot, "Initializing bootserver...");
    memcpy(code, codeArray[GetPESInfo()->GameVersion], sizeof(code));
    memcpy(data, dataArray[GetPESInfo()->GameVersion], sizeof(data));

    //HookFunction(hk_D3D_UnlockRect,(DWORD)bootUnlockRect);
    HookFunction(hk_BeginUniSelect,(DWORD)bootBeginUniSelect);
    MasterHookFunction(code[C_COPYPLAYERDATA_CS], 3, bootCopyPlayerData);
    MasterHookFunction(code[C_RESETFLAG2_CS], 0, bootResetFlag2);
    HookFunction(hk_PesGetTexture,(DWORD)bootPesGetTexture);
    HookFunction(hk_OnSetLodLevel,(DWORD)bootOnSetLodLevel);
    HookFunction(hk_GetLodTexture,(DWORD)bootGetLodTexture);

    // read the map
    readMap();
}

void readMap()
{
	char tmp[BUFLEN];
	char str[BUFLEN];
	char *comment=NULL;
	char sfile[BUFLEN];
	WORD number=0;
	
	strcpy(tmp,GetPESInfo()->gdbDir);
	strcat(tmp,"GDB\\boots\\map.txt");
	
	FILE* cfg=fopen(tmp, "rt");
	if (cfg==NULL) {
		Log(&k_boot,"readMap: Couldn't find boots map!");
		return;
	};
	
	while (true) {
		ZeroMemory(str, BUFLEN);
		fgets(str, BUFLEN-1, cfg);
		
		// skip comments
		comment=NULL;
		comment = strstr(str, "#");
		if (comment != NULL) comment[0] = '\0';
		
		// parse line
		ZeroMemory(sfile,BUFLEN);
		if (sscanf(str,"%d,\"%[^\"]\"",&number,sfile)==2) {
            TexturePack pack;
            pack._textureFile = sfile;
            // add to the map
            g_bootTexturePacks.insert(pair<WORD,TexturePack>(number,pack));
        }

		if (feof(cfg)) break;
	};
	fclose(cfg);
    LogWithNumber(&k_boot, "readMap: g_bootTexturePacks.size() = %d", g_bootTexturePacks.size());
}

void releaseBootTextures()
{
    // release the boot textures, so that we don't consume too much memory for boots
    for (map<WORD,TexturePack>::iterator it = g_bootTexturePacks.begin();
            it != g_bootTexturePacks.end();
            it++) {
        if (it->second._big) {
            it->second._big->Release();
            it->second._big = NULL;
            it->second._bigLoaded = false;
        }
        if (it->second._small) {
            it->second._small->Release();
            it->second._small = NULL;
            it->second._smallLoaded = false;
        }
        LogWithNumber(&k_boot, "Released boot textures for player %d", it->first);
    }
    
    g_bootTextures.clear();
    g_bootTexturesPos.clear();
}

void bootBeginUniSelect()
{
	Log(&k_boot,"bootBeginUniSelect: releasing boot textures");
    releaseBootTextures();
}

DWORD bootResetFlag2()
{
	Log(&k_boot,"bootBeginUniSelect: releasing boot textures");
    releaseBootTextures();

    // call original
    DWORD result = MasterCallNext();
    return result;
}

IDirect3DTexture8* getBootTexture(WORD playerId, bool big)
{
	UINT Width=big?512:128, Height=big?256:64;
    IDirect3DTexture8* bootTexture = NULL;
    map<WORD,TexturePack>::iterator it = g_bootTexturePacks.find(playerId);
    if (it != g_bootTexturePacks.end()) {
        // map has an entry for this player
        if (big && it->second._bigLoaded) {
            // already looked up and the texture should be loaded
            return it->second._big;

        } else if (!big && it->second._smallLoaded) {
            // already looked up and the texture should be loaded
            return it->second._small;

        } else {
            // haven't tried to load the textures for this player yet.
            // Do it now.
            IDirect3DTexture8* temp;
            char filename[BUFLEN];
            sprintf(filename,"%sGDB\\boots\\%s", GetPESInfo()->gdbDir, it->second._textureFile.c_str());
            if (SUCCEEDED(D3DXCreateTextureFromFileEx(
                            GetActiveDevice(), filename,
                            256, 256, 1, 0, 
                            D3DFMT_A8R8G8B8, D3DPOOL_MANAGED,
                            D3DX_FILTER_NONE, D3DX_FILTER_NONE,
                            0, NULL, NULL, &temp))) {
                // create canvas
                if (SUCCEEDED(D3DXCreateTextureFromFileEx(
                                GetActiveDevice(), "kitserver\\bcanvas.png",
                                Width, Height, 1, 0, 
                                D3DFMT_A8R8G8B8, D3DPOOL_MANAGED,
                                D3DX_FILTER_NONE, D3DX_FILTER_NONE,
                                0, NULL, NULL, &bootTexture))) {
                    // copy the temp texture to canvas 3 times
                    ReplaceBootTexture(bootTexture, temp, Width);
                }

                // store in the map
                if (big) {
                    it->second._big = bootTexture;
                    LogWithNumber(&k_boot, "getBootTexture: loaded big texture for player %d", playerId);
                } else {
                    it->second._small = bootTexture;
                    LogWithNumber(&k_boot, "getBootTexture: loaded small texture for player %d", playerId);
                }

            } else {
                LogWithString(&k_boot, "D3DXCreateTextureFromFileEx FAILED for %s", filename);
            }

            // update loaded flags, so that we only load each texture once
            if (big) {
                it->second._bigLoaded = true;
            } else {
                it->second._smallLoaded = true;
            }
        }
    }
    return bootTexture;
}

DWORD bootCopyPlayerData(DWORD p0, DWORD p1, DWORD p2)
{
	DWORD playerNumber, addr;
	__asm mov playerNumber, ebx
	__asm mov addr, edi
	
	DWORD result = MasterCallNext(p0,p1,p2);

    BYTE* pBootType = (BYTE*)(addr-0x1c+1);
    /*
    if (0xba <= playerNumber && playerNumber <= 0xc3) { // France first 11
    */
    map<WORD,TexturePack>::iterator it = g_bootTexturePacks.find(playerNumber);
    if (it != g_bootTexturePacks.end()) {
        *pBootType = 0;
        LogWithTwoNumbers(&k_boot,"setting boot-type to 0 at %08x -> player %d",
                (DWORD)pBootType, playerNumber);
    }
	return result;
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

            bool isBig = desc.Width==512;
            IDirect3DTexture8* bootTexture = getBootTexture(playerId, isBig);
            if (bootTexture) {
                // replace the texture
                ReplaceTextureLevel(self, bootTexture, 0);
            }
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
                            D3DX_DEFAULT, 0))) {
                //LogWithNumber(&k_boot,"ReplaceTextureLevel: replacing level %d COMPLETE", level);

            } else {
                //LogWithNumber(&k_boot,"ReplaceTextureLevel: replacing level %d FAILED", level);
            }
            src->Release();
        }
        dest->Release();
    }

    //DumpTexture(srcTexture);
}

void ReplaceBootTexture(IDirect3DTexture8* srcTexture, IDirect3DTexture8* repTexture, UINT width)
{
    IDirect3DSurface8* src = NULL;
    IDirect3DSurface8* dest = NULL;

    RECT destRect;
    RECT srcRect = {0,0,170,256};
    if (width == 512) {
        destRect.top=0; destRect.left=0;
        destRect.bottom=256; destRect.right=170;
    } else {
        destRect.top=0; destRect.left=0;
        destRect.bottom=64; destRect.right=170/4;
    }

    if (SUCCEEDED(srcTexture->GetSurfaceLevel(0, &dest))) {
        if (SUCCEEDED(repTexture->GetSurfaceLevel(0, &src))) {
            // need 3 copies of the boot texture
            for (int i=0; i<3; i++) {
                destRect.left = (width==512) ? (i*170) : (i*170/4);
                destRect.right = (width==512) ? ((i+1)*170) : ((i+1)*170/4);
                if (SUCCEEDED(D3DXLoadSurfaceFromSurface(
                                dest, NULL, &destRect,
                                src, NULL, &srcRect,
                                D3DX_DEFAULT, 0))) {
                } else {
                }
            }
            src->Release();
        }
        dest->Release();
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
    else
    {
    	LogWithString(&k_boot, "DumpTexture: Saved texture to %s", buf);
    };
}

// p2 can create the relationship between the player and the texture
void bootPesGetTexture(DWORD p1, DWORD p2, DWORD p3, IDirect3DTexture8** res)
{
	map<DWORD,IDirect3DTexture8*>::iterator it = g_bootTextures.find(p2);
    if (it != g_bootTextures.end()) {
    	if (p3==g_bootTexturesPos[p2]) {
    		*res=it->second;
    	};
    };
	return;
};

void bootOnSetLodLevel(DWORD p1, DWORD level)
{
	DWORD playerMainColl=0, playerNumber=0;
	DWORD* bodyColl=NULL;
	int maxI=21;
	D3DSURFACE_DESC desc;
	IDirect3DTexture8* bootTexture=NULL;
	
	if (p1==0) return;
	
	if (isEditMode()) {
        playerNumber = *(WORD*)data[EDITPLAYER_ID];
		maxI=0;
	};
	for (int i=0;i<=maxI;i++) {
		if (!isEditMode())
			playerNumber=*(WORD*)(data[PLAYERS_LINEUP] + data[LINEUP_RECORD_SIZE]*i + 2);
		if (playerNumber != 0) {
			playerMainColl=**(DWORD**)(data[PLAYERS_LINEUP] + data[LINEUP_RECORD_SIZE]*i - 8);
			playerMainColl=*(DWORD*)(playerMainColl+0x10);
			if (playerMainColl==p1) {
				//ok, this is the player to process
				//now calculate all possible p2 addresses for PesGetTexture
				//and set the right replacing texture for them
				bodyColl=*(DWORD**)(playerMainColl+0x14) + 1;

				for (int lod=0;lod<5;lod++) {
					//for lod 1-3 the big texture is used, for 4-5 the small one
					bootTexture = getBootTexture(playerNumber, lod<3);
					if (bootTexture != NULL) {
						g_bootTextures[*bodyColl] = bootTexture;
					
						//p3 is different depending on the lod level
						switch (lod) {
							case 0:
							case 1:
								g_bootTexturesPos[*bodyColl] = isTrainingMode()?3:4; break;
							case 2:
							case 3:
								g_bootTexturesPos[*bodyColl] = 3; break;
							case 4:
								g_bootTexturesPos[*bodyColl] = 2; break;
						};
					} else {
						//reset boot texture
						g_bootTextures.erase(*bodyColl);
						g_bootTexturesPos.erase(*bodyColl);
					};
					bodyColl+=2;
				};
			};
		};
	};
	return;
};

void bootGetLodTexture(DWORD p1, DWORD* res)
{
	// *res is later *bodyColl
	if (g_bootTextures.erase(*res)>0) {
		g_bootTexturesPos.erase(*res);
		LogWithNumber(&k_boot,"Erased bodyColl %x",*res);
	};
	return;
};