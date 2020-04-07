// skins.cpp
#include <windows.h>
#include <stdio.h>
#include <ctime>
#include <unordered_map>
#include "skins.h"
#include "kload_exp.h"

KMOD k_skin={MODID,NAMELONG,NAMESHORT,DEFAULT_DEBUG};
HINSTANCE hInst;

#define DATALEN 13
enum {
    EDITMODE_CURRPLAYER_MOD, EDITMODE_CURRPLAYER_ORG,
    EDITMODE_FLAG, EDITPLAYERMODE_FLAG, EDITPLAYER_ID,
    PLAYERS_LINEUP, PLAYERID_IN_PLAYERDATA,
    LINEUP_RECORD_SIZE, LINEUP_BLOCKSIZE, PLAYERDATA_SIZE, STACK_SHIFT,
    EDITPLAYERBOOT_FLAG, EDITBOOTS_FLAG,
};
DWORD dtaArray[][DATALEN] = {
    // PES6
    {
        0x112e1c0, 0x112e1c8,
        0x1108488, 0x11084e8, 0x112e24a,
        0x3bdcbc0, 0x3bcf586,
        0x240, 0x60, 0x348, 0,
        0x11308dc, 0x1108830,
    },
    // PES6 1.10
    {
        0x112f1c0, 0x112f1c8,
        0x1109488, 0x11094e8, 0x112f24a,
        0x3bddbc0, 0x3bd0586,
        0x240, 0x60, 0x348, 0 /*0x1b4*/,
        0x11318dc, 0x1109830,
    },
    // WE2007
    {
        0x1128c28, 0x1128c2c,
        0x1102f08, 0x1102f50, 0x1128cb2,
        0x3bd7640, 0x3bca006,
        0x240, 0x60, 0x348, 0,
        0x112b364, 0x1103298,
    },
};
DWORD dta[DATALEN];

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
    // WE2007
    {
        0xa4bd62,
        0x9c1d97,
    },
};
DWORD code[CODELEN];

class TexturePack {
public:
    TexturePack() : _big(NULL),_gloves(NULL),_textureFile(),_glovesFile(),_bigLoaded(false),_glovesLoaded(false) {}
    ~TexturePack() {
        if (_big) _big->Release();
        if (_gloves) _gloves->Release();
    }
    IDirect3DTexture8* _big;
    IDirect3DTexture8* _gloves;
    string _textureFile;
    string _glovesFile;
    bool _bigLoaded;
    bool _glovesLoaded;
};

//////////////////////////////////////////////////////
// Globals ///////////////////////////////////////////

unordered_map<WORD,TexturePack> g_skinTexturePacks;
DWORD g_skinTexturesColl[5];
DWORD g_handsTexturesColl[5];
DWORD g_neckTexturesColl[5];
DWORD g_skinTexturesPos[5];
DWORD g_handsTexturesPos[5];
DWORD g_neckTexturesPos[5];
IDirect3DTexture8* g_skinTextures[2][64]; //[no-gloves/gloves][32*team + posInTeam]
DWORD g_skinPlayerIds[64]; //[32*team + posInTeam]
DWORD currRenderPlayer=0xffffffff;
PLAYER_RECORD* currRenderPlayerRecord=NULL;


//////////////////////////////////////////////////////

EXTERN_C BOOL WINAPI DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID lpReserved);
void skinInit(IDirect3D8* self, UINT Adapter,
    D3DDEVTYPE DeviceType, HWND hFocusWindow, DWORD BehaviorFlags,
    D3DPRESENT_PARAMETERS *pPresentationParameters,
    IDirect3DDevice8** ppReturnedDeviceInterface);
void readSkinsMap();
DWORD skinResetFlag2();
void skinBeginRenderPlayer(DWORD playerMainColl);
void skinPesGetTexture(DWORD p1, DWORD p2, DWORD p3, IDirect3DTexture8** res);
void skinBeginUniSelect();

//////////////////////////////////////////////////////
//
// FUNCTIONS
//
//////////////////////////////////////////////////////

// Calls IUnknown::Release() on an instance
void SafeRelease(LPVOID ppObj)
{
    try {
        IUnknown** ppIUnknown = (IUnknown**)ppObj;
        if (ppIUnknown == NULL)
        {
            Log(&k_skin,"Address of IUnknown reference is 0");
            return;
        }
        if (*ppIUnknown != NULL)
        {
            (*ppIUnknown)->Release();
            *ppIUnknown = NULL;
        }
    } catch (...) {
        // problem with a safe-release
        TRACE(&k_skin,"Problem with safe-release");
    }
}

EXTERN_C BOOL WINAPI DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID lpReserved)
{
    if (dwReason == DLL_PROCESS_ATTACH)
    {
        Log(&k_skin,"Attaching dll...");
        hInst=hInstance;
        RegisterKModule(&k_skin);
        HookFunction(hk_D3D_CreateDevice,(DWORD)skinInit);
        SetBootserverVersion(VERSION_ROBBIE);
    }
    else if (dwReason == DLL_PROCESS_DETACH)
    {
        Log(&k_skin,"Detaching dll...");
        UnhookFunction(hk_D3D_CreateDevice,(DWORD)skinInit);
        UnhookFunction(hk_BeginUniSelect,(DWORD)skinBeginUniSelect);
        UnhookFunction(hk_PesGetTexture,(DWORD)skinPesGetTexture);
        UnhookFunction(hk_BeginRenderPlayer,(DWORD)skinBeginRenderPlayer);
    }

    return true;
}

void skinInit(IDirect3D8* self, UINT Adapter,
    D3DDEVTYPE DeviceType, HWND hFocusWindow, DWORD BehaviorFlags,
    D3DPRESENT_PARAMETERS *pPresentationParameters,
    IDirect3DDevice8** ppReturnedDeviceInterface)
{
    Log(&k_skin, "Initializing skinserver...");
    memcpy(code, codeArray[GetPESInfo()->GameVersion], sizeof(code));
    memcpy(dta, dtaArray[GetPESInfo()->GameVersion], sizeof(dta));

    HookFunction(hk_BeginUniSelect,(DWORD)skinBeginUniSelect);
    MasterHookFunction(code[C_RESETFLAG2_CS], 0, skinResetFlag2);
    HookFunction(hk_PesGetTexture,(DWORD)skinPesGetTexture);
    HookFunction(hk_BeginRenderPlayer,(DWORD)skinBeginRenderPlayer);

    // read the skins map
    readSkinsMap();

    // seed the random generator
    time_t timer;
    srand(time(&timer));
}

void readSkinsMap()
{
    char tmp[BUFLEN];
    char str[BUFLEN];
    char *comment=NULL;
    char sfile[BUFLEN];
    char gfile[BUFLEN];
    WORD number=0;

    strcpy(tmp,GetPESInfo()->gdbDir);
    strcat(tmp,"GDB\\skins\\map.txt");

    FILE* cfg=fopen(tmp, "rt");
    if (cfg==NULL) {
        Log(&k_skin,"readMap: Couldn't find skins map!");
        return;
    }

    while (true) {
        ZeroMemory(str, BUFLEN);
        fgets(str, BUFLEN-1, cfg);

        // skip comments
        comment=NULL;
        comment = strstr(str, "#");
        if (comment != NULL) comment[0] = '\0';

        // parse line
        ZeroMemory(sfile,BUFLEN);
        ZeroMemory(gfile,BUFLEN);
        if (sscanf(str,"%d , \"%[^\"]\" , \"%[^\"]\"",&number,sfile,gfile)==3) {
            TexturePack pack;
            pack._textureFile = sfile;
            pack._glovesFile = gfile;
            // add to the map
            g_skinTexturePacks.insert(pair<WORD,TexturePack>(number,pack));
        }
        else if (sscanf(str,"%d , \"%[^\"]\"",&number,sfile)==2) {
            TexturePack pack;
            pack._textureFile = sfile;
            // add to the map
            g_skinTexturePacks.insert(pair<WORD,TexturePack>(number,pack));
        }

        if (feof(cfg)) break;
    }
    fclose(cfg);
    LogWithNumber(&k_skin, "readMap: g_skinTexturePacks.size() = %d", g_skinTexturePacks.size());
}

void releaseTextures()
{
    // release the skin textures, so that we don't consume too much memory for skins
    unordered_map<WORD,TexturePack>::iterator it;
    for (it = g_skinTexturePacks.begin();
            it != g_skinTexturePacks.end();
            it++) {
        if (it->second._big) {
            //SafeRelease(&it->second._big);
            it->second._big = NULL;
            it->second._bigLoaded = false;
            LogWithNumber(&k_skin, "Released skin texture for player %d", it->first);
        }
        if (it->second._gloves) {
            //SafeRelease(&it->second._gloves);
            it->second._gloves = NULL;
            it->second._glovesLoaded = false;
            LogWithNumber(&k_skin, "Released with-gloves skin texture for player %d", it->first);
        }

    }

    ZeroMemory(g_skinTextures, 64*4*2);
    ZeroMemory(g_skinPlayerIds, 64*4);
}

void skinBeginUniSelect()
{
    Log(&k_skin,"skinBeginUniSelect: releasing skin textures");
    releaseTextures();
}

DWORD skinResetFlag2()
{
    Log(&k_skin,"skinResetFlag2: releasing skin textures");
    releaseTextures();

    // call original
    DWORD result = MasterCallNext();
    return result;
}

// color 1 is alpha-blended over color 2
DWORD blendColors(DWORD color1, DWORD color2)
{
	BYTE alpha1 =  (color1 & 0xff000000) >> 24;

	if (alpha1 == 0) return color2;		// transparent overlay
	if (alpha1 == 255) return ((color1 & 0xffffff) | (color2 & 0xff000000));	// opaque overlay

	BYTE redValue = ((color1 & 0xff0000) * alpha1 + (color2 & 0xff0000) * (255 - alpha1)) / 0xff0000;
	BYTE greenValue = ((color1 & 0xff00) * alpha1 + (color2 & 0xff00) * (255 - alpha1)) / 0xff00;
	BYTE blueValue = ((color1 & 0xff) * alpha1 + (color2 & 0xff) * (255 - alpha1)) / 0xff;

	DWORD alpha2 =  color2 & 0xff000000;	// keep the original alpha

	return (alpha2 | (redValue << 16) | (greenValue << 8) | blueValue);
}

void applyOverlay(D3DLOCKED_RECT* dest, const char* overlayfilename, D3DSURFACE_DESC* desc)
{
	UINT width = desc->Width;
	UINT height = desc->Height;

	IDirect3DTexture8* pOvTexture = NULL;
	D3DLOCKED_RECT rectOv;

	// create overlay texture
	if (!SUCCEEDED(D3DXCreateTextureFromFileEx(
            GetActiveDevice(), overlayfilename,
            width, height,
            1, desc->Usage, desc->Format, desc->Pool,
            D3DX_DEFAULT, D3DX_DEFAULT,
            0, NULL, NULL, &pOvTexture))) {
        LogWithTwoNumbers(&k_skin, "Failed to load overlay texture of (%d,%d)", width, height);
		return;
	}

	if (!SUCCEEDED(pOvTexture->LockRect(0, &rectOv, NULL, 0))) {
		pOvTexture->Release();
		return;
	}

	BYTE* pDestRow = (BYTE*)dest->pBits;
	BYTE* pSrcRow = (BYTE*)rectOv.pBits;
	for (int y = 0; y < height; y++) {
		DWORD* pDestPixel = (DWORD*)pDestRow;
		DWORD* pSrcPixel = (DWORD*)pSrcRow;
		for (int x = 0; x < width; x++) {
			*pDestPixel = blendColors(*pSrcPixel, *pDestPixel);
			pDestPixel++;
			pSrcPixel++;
		}
		pDestRow += dest->Pitch;
		pSrcRow += rectOv.Pitch;
	}

	pOvTexture->UnlockRect(0);
	pOvTexture->Release();
}

void pasteGloves(IDirect3DTexture8* pSkinTexture, const char* overlayfilename)
{
	if (!pSkinTexture) return;

	D3DLOCKED_RECT rect;
	D3DSURFACE_DESC desc;

	if (!SUCCEEDED(pSkinTexture->LockRect(0, &rect, NULL, 0))) {
        LogWithNumber(&k_skin, "LockRect failed for skin texture %08x", (DWORD)pSkinTexture);
        return;
    }
	pSkinTexture->GetLevelDesc(0, &desc);

	if (overlayfilename != NULL) {
		Log(&k_skin, "Applying gloves overlay...");
		applyOverlay(&rect, overlayfilename, &desc);
	}

	pSkinTexture->UnlockRect(0);
}

IDirect3DTexture8* getSkinTexture(WORD playerId, bool overlay_gloves)
{
    IDirect3DTexture8* skinTexture = NULL;
    unordered_map<WORD,TexturePack>::iterator it = g_skinTexturePacks.find(playerId);
    if (it != g_skinTexturePacks.end()) {
        // map has an entry for this player
        if (it->second._bigLoaded && !overlay_gloves) {
            // already looked up and the texture should be loaded
            return it->second._big;

        } else if (it->second._glovesLoaded && overlay_gloves) {
            // already looked up and the texture should be loaded
            return it->second._gloves;

        } else {
            // haven't tried to load the textures for this player yet.
            // Do it now.
            char filename[BUFLEN];
            sprintf(filename,"%sGDB\\skins\\%s", GetPESInfo()->gdbDir, it->second._textureFile.c_str());
            //if (SUCCEEDED(D3DXCreateTextureFromFile(GetActiveDevice(), filename, &skinTexture))) {
            if (SUCCEEDED(D3DXCreateTextureFromFileEx(GetActiveDevice(), filename,
                            0, 0, 1, 0,
                            D3DFMT_A8R8G8B8, D3DPOOL_MANAGED,
                            D3DX_FILTER_NONE, D3DX_FILTER_NONE,
                            0, NULL, NULL, &skinTexture))) {
                // store in the map
                if (overlay_gloves) {
                    it->second._gloves = skinTexture;
                    LogWithNumber(&k_skin, "getSkinTexture: loaded with-gloves skin texture for player %d", playerId);
                }
                else {
                    it->second._big = skinTexture;
                    LogWithNumber(&k_skin, "getSkinTexture: loaded skin texture for player %d", playerId);
                }
            } else {
                LogWithString(&k_skin, "D3DXCreateTextureFromFileEx FAILED for %s", filename);
            }

            if (overlay_gloves) {
                char glv_name[BUFLEN];
                if (it->second._glovesFile.empty()) {
                    sprintf(glv_name,"%sGDB\\skins\\gloves.png", GetPESInfo()->gdbDir);
                } else {
                    sprintf(glv_name,"%sGDB\\skins\\%s", GetPESInfo()->gdbDir, it->second._glovesFile.c_str());
                }
                pasteGloves(skinTexture, glv_name);
            }

            // update loaded flags, so that we only load each texture once
            if (overlay_gloves) {
                it->second._glovesLoaded = true;
            }
            else {
                it->second._bigLoaded = true;
            }
        }
    }
    return skinTexture;
}

bool isEditPlayerMode()
{
    return *(BYTE*)dta[EDITPLAYERMODE_FLAG] == 1;
}

bool isEditPlayerBootMode()
{
    return *(BYTE*)dta[EDITPLAYERBOOT_FLAG] == 1;
}

bool isEditBootsMode()
{
    return *(BYTE*)dta[EDITBOOTS_FLAG] == 1;
}

bool has_gloves = false;

void skinBeginRenderPlayer(DWORD playerMainColl)
{
    DWORD pmc=0, pinfo=0, playerNumber=0;
    DWORD* bodyColl=NULL;
    int minI=1, maxI=22;
    BYTE *pgloves;

    currRenderPlayer=0xffffffff;
    currRenderPlayerRecord=NULL;

    bool edit_mode = isEditMode();
    if (edit_mode) {
        playerNumber = editPlayerId();
        maxI=1;
    }

    for (int i=minI;i<=maxI;i++) {
        if (!edit_mode)
            playerNumber=getRecordPlayerId(i);

        if (playerNumber != 0) {
            pmc=*(playerRecord(i)->texMain);
            pinfo=*(DWORD*)(pmc+0x14);
            pgloves=(BYTE*)(pmc+0x1fe);
            pmc=*(DWORD*)(pmc+0x10);

            if (pmc==playerMainColl) {
                //PES is going to render this player now
                currRenderPlayer = playerNumber;
                currRenderPlayerRecord = playerRecord(i);
                bool is_goalkeeper = false;
                if (edit_mode) {
                    if (pinfo && ((BYTE*)pinfo)[0x11] == 1) {
                        is_goalkeeper = true;
                    }
                }
                else if (((BYTE*)currRenderPlayerRecord)[0x1fd] == 1) {
                    is_goalkeeper = true;
                }

                has_gloves = *pgloves;

                //LOG(&k_skin, ">>>>>>>>>>>>>>>> currRenderPlayer: %d, currRenderPlayerRecord: %p, pmc = %08x, pinfo = %08x (gk=%d) has_gloves=%d",
                //    currRenderPlayer, currRenderPlayerRecord, pmc, pinfo, is_goalkeeper, has_gloves);

                BYTE temp=32*currRenderPlayerRecord->team + currRenderPlayerRecord->posInTeam;
                if (currRenderPlayer != g_skinPlayerIds[temp]) {
                    g_skinPlayerIds[temp]=currRenderPlayer;
                    g_skinTextures[0][temp]=NULL;
                    g_skinTextures[1][temp]=NULL;
                }

                // skin
                bodyColl=*(DWORD**)(playerMainColl+0x14) + 1;
                for (int lod=0;lod<5;lod++) {
                    g_skinTexturesColl[lod]=*bodyColl;  //remember p2 value for this lod level
                    g_skinTexturesPos[lod] = 0;
                    bodyColl+=2;
                }

                // hands
                // for goalkeepers: keep the gloves texture
                bodyColl=*(DWORD**)(playerMainColl+0x18) + 2;
                for (int lod=0;lod<3;lod++) {
                    if ((*(DWORD*)(bodyColl+1) == 0x11) || (*(DWORD*)(bodyColl+1) == 0x12)) {
                        if (!is_goalkeeper) {
                            g_handsTexturesColl[lod]=*bodyColl;  //remember p2 value for this lod level
                            g_handsTexturesPos[lod] = 0;
                        }
                        else {
                            g_handsTexturesColl[lod]=0;
                            g_handsTexturesPos[lod]=0;
                        }
                    }
                    bodyColl+=7;
                }

                // neck
                bodyColl=*(DWORD**)(playerMainColl+0x1c) + 2;
                bodyColl+=10;
                for (int lod=2;lod<3;lod++) {
                    g_neckTexturesColl[lod]=*bodyColl;
                    g_neckTexturesPos[lod]=0;
                    bodyColl+=5;
                }
            }
        }
    }
    return;
}

void skinPesGetTexture(DWORD p1, DWORD p2, DWORD p3, IDirect3DTexture8** res)
{
    if (currRenderPlayer==0xffffffff) return;

    //LOG(&k_skin, "skinPesGetTexture:: p1=%08x, p2=%08x, p3=%08x, *res=%p", p1, p2, p3, *res);

    for (int lod=0; lod<5; lod++) {
        if (p2==g_skinTexturesColl[lod] && p3==g_skinTexturesPos[lod]) {
            //LOG(&k_skin, "skinPesGetTexture:: ^^^ skin texture!! p1=%08x, p2=%08x, p3=%08x, *res=%p", p1, p2, p3, *res);

            BYTE j=(has_gloves)?1:0;
            BYTE temp=32*currRenderPlayerRecord->team + currRenderPlayerRecord->posInTeam;
            IDirect3DTexture8* skinTexture=g_skinTextures[j][temp];
            if ((DWORD)skinTexture != 0xffffffff) { //0xffffffff means: has no gdb skin
                if (!skinTexture) {
                    //no skin texture in cache yet
                    skinTexture = getSkinTexture(currRenderPlayer, has_gloves);
                    if (!skinTexture) {
                        //no texture assigned
                        skinTexture = (IDirect3DTexture8*)0xffffffff;
                    } else {
                        *res=skinTexture;
                    }
                    //cache the texture pointer
                    g_skinTextures[j][temp]=skinTexture;
                } else {
                    //set texture
                    *res=skinTexture;
                }
            }
        }
    }

    for (int lod=0; lod<3; lod++) {
        if (p2==g_handsTexturesColl[lod] && p3==g_handsTexturesPos[lod]) {
            //LOG(&k_skin, "skinPesGetTexture:: ^^^ hand texture!! p1=%08x, p2=%08x, p3=%08x, *res=%p", p1, p2, p3, *res);

            BYTE j=(has_gloves)?1:0;
            BYTE temp=32*currRenderPlayerRecord->team + currRenderPlayerRecord->posInTeam;
            IDirect3DTexture8* skinTexture=g_skinTextures[j][temp];
            if ((DWORD)skinTexture != 0xffffffff) { //0xffffffff means: has no gdb skin
                if (!skinTexture) {
                    //no skin texture in cache yet
                    skinTexture = getSkinTexture(currRenderPlayer, has_gloves);
                    if (!skinTexture) {
                        //no texture assigned
                        skinTexture = (IDirect3DTexture8*)0xffffffff;
                    } else {
                        *res=skinTexture;
                    }
                    //cache the texture pointer
                    g_skinTextures[j][temp]=skinTexture;
                } else {
                    //set texture
                    *res=skinTexture;
                }
            }
        }
    }

    for (int lod=0; lod<3; lod++) {
        if (p2==g_neckTexturesColl[lod] && p3==g_neckTexturesPos[lod]) {
            //LOG(&k_skin, "skinPesGetTexture:: ^^^ neck texture!! p1=%08x, p2=%08x, p3=%08x, *res=%p", p1, p2, p3, *res);

            BYTE j=(has_gloves)?1:0;
            BYTE temp=32*currRenderPlayerRecord->team + currRenderPlayerRecord->posInTeam;
            IDirect3DTexture8* skinTexture=g_skinTextures[j][temp];
            if ((DWORD)skinTexture != 0xffffffff) { //0xffffffff means: has no gdb skin
                if (!skinTexture) {
                    //no skin texture in cache yet
                    skinTexture = getSkinTexture(currRenderPlayer, has_gloves);
                    if (!skinTexture) {
                        //no texture assigned
                        skinTexture = (IDirect3DTexture8*)0xffffffff;
                    } else {
                        *res=skinTexture;
                    }
                    //cache the texture pointer
                    g_skinTextures[j][temp]=skinTexture;
                } else {
                    //set texture
                    *res=skinTexture;
                }
            }
        }
    }

    return;
}
