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
    TexturePack() : _big(NULL),_gloves(NULL),_gk_glove_left(NULL),_gk_glove_right(NULL),
        _textureFile(),_glovesFile(),_gkGloveLeftFile(),_gkGloveRightFile(),
        _bigLoaded(false),_glovesLoaded(false),_gkGloveLeftLoaded(false),_gkGloveRightLoaded(false) {}
    ~TexturePack() {
        if (_big) _big->Release();
        if (_gloves) _gloves->Release();
        if (_gk_glove_left) _gk_glove_left->Release();
        if (_gk_glove_right) _gk_glove_right->Release();
    }
    IDirect3DTexture8* _big;
    IDirect3DTexture8* _gloves;
    IDirect3DTexture8* _gk_glove_left;
    IDirect3DTexture8* _gk_glove_right;
    string _textureFile;
    string _glovesFile;
    string _gkGloveLeftFile;
    string _gkGloveRightFile;
    bool _bigLoaded;
    bool _glovesLoaded;
    bool _gkGloveLeftLoaded;
    bool _gkGloveRightLoaded;
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
IDirect3DTexture8* g_gloveTextures[2][64]; //[left GK/right GK][32*team + posInTeam]
DWORD g_skinPlayerIds[64]; //[32*team + posInTeam]
DWORD currRenderPlayer=0xffffffff;
PLAYER_RECORD* currRenderPlayerRecord=NULL;

// keyboard hook handle
static HHOOK g_hKeyboardHook = NULL;
static bool dump_now(false);
static bool dumping(false);

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

/****************************************************************
 * WH_KEYBOARD hook procedure                                   *
 ****************************************************************/

EXTERN_C _declspec(dllexport) LRESULT CALLBACK skinKeyboardProc(int code, WPARAM wParam, LPARAM lParam)
{
    if (code < 0) // do not process message
        return CallNextHookEx(g_hKeyboardHook, code, wParam, lParam);

    switch (code)
    {
        case HC_ACTION:
            /* process the key events */
            if (lParam & 0x80000000)
            {
                KEYCFG* keyCfg = GetInputCfg();
                if (wParam == keyCfg->keyboard.keyAction2) {
                    dump_now = true;
                    dumping = false;
                }
            }
            break;
    }

    // We must pass the all messages on to CallNextHookEx.
    return CallNextHookEx(g_hKeyboardHook, code, wParam, lParam);
}

/* remove keyboard hook */
void UninstallKeyboardHook(void)
{
    if (g_hKeyboardHook != NULL)
    {
        UnhookWindowsHookEx( g_hKeyboardHook );
        Log(&k_skin,"Keyboard hook uninstalled.");
        g_hKeyboardHook = NULL;
    }
}

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

#ifdef SKIN_TEXDUMP
        // install keyboard hook, if not done yet.
        if (g_hKeyboardHook == NULL)
        {
            g_hKeyboardHook = SetWindowsHookEx(WH_KEYBOARD, skinKeyboardProc, hInst, GetCurrentThreadId());
            LogWithNumber(&k_skin,"Installed keyboard hook: g_hKeyboardHook = %d", (DWORD)g_hKeyboardHook);
        }
#endif
    }
    else if (dwReason == DLL_PROCESS_DETACH)
    {
        Log(&k_skin,"Detaching dll...");
#ifdef SKIN_TEXDUMP
        UninstallKeyboardHook();
#endif
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
    char buf[BUFLEN];
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
        char *p = strchr(str,',');
        if (p) {
            if (sscanf(str, "%d", &number)==1) {
                TexturePack pack;
                int token=2;
                while (p) {
                    ZeroMemory(buf, BUFLEN);
                    if (sscanf(p+1, "%*[^,\"]\"%[^\"]%*[^\n]", buf)==1) {
                        //LOG(&k_skin, "token (%d): {%s}", token, buf);
                        switch (token) {
                            case 2: pack._textureFile = buf; break;
                            case 3: pack._glovesFile = buf; break;
                            case 4: pack._gkGloveLeftFile = buf; break;
                            case 5: pack._gkGloveRightFile = buf; break;
                        }
                    }
                    token++;
                    p = strchr(p+1,',');
                }
                LOG(&k_skin, "id:%d, skin:{%s}, gloves:{%s}, GK-left:{%s}, GK-right:{%s}",
                    number, pack._textureFile.c_str(), pack._glovesFile.c_str(),
                    pack._gkGloveLeftFile.c_str(), pack._gkGloveRightFile.c_str());
                g_skinTexturePacks.insert(pair<WORD,TexturePack>(number,pack));
            }
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
        if (it->second._gk_glove_left) {
            //SafeRelease(&it->second._gkGloveLeft);
            it->second._gk_glove_left = NULL;
            it->second._gkGloveLeftLoaded = false;
        }
        if (it->second._gk_glove_right) {
            //SafeRelease(&it->second._gk_glove_right);
            it->second._gk_glove_right = NULL;
            it->second._gkGloveRightLoaded = false;
        }
    }

    ZeroMemory(g_skinTextures, 64*4*2);
    ZeroMemory(g_gloveTextures, 64*4*2);
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

        } else if (!it->second._textureFile.empty()) {
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

IDirect3DTexture8* getGkGloveTexture(WORD playerId, bool left)
{
    IDirect3DTexture8* gloveTexture = NULL;
    unordered_map<WORD,TexturePack>::iterator it = g_skinTexturePacks.find(playerId);
    if (it != g_skinTexturePacks.end()) {
        // map has an entry for this player
        if (left && it->second._gkGloveLeftLoaded) {
            // already looked up and the texture should be loaded
            return it->second._gk_glove_left;

        } else if (!left && it->second._gkGloveRightLoaded) {
            // already looked up and the texture should be loaded
            return it->second._gk_glove_right;

        } else {
            // haven't tried to load the textures for this player yet.
            // Do it now.
            char filename[BUFLEN];
            if (left) {
                if (it->second._gkGloveLeftFile.empty()) return NULL;
                sprintf(filename,"%sGDB\\skins\\%s", GetPESInfo()->gdbDir, it->second._gkGloveLeftFile.c_str());
            }
            else {
                if (it->second._gkGloveRightFile.empty()) return NULL;
                sprintf(filename,"%sGDB\\skins\\%s", GetPESInfo()->gdbDir, it->second._gkGloveRightFile.c_str());
            }
            if (SUCCEEDED(D3DXCreateTextureFromFileEx(GetActiveDevice(), filename,
                            0, 0, 1, 0,
                            D3DFMT_A8R8G8B8, D3DPOOL_MANAGED,
                            D3DX_FILTER_NONE, D3DX_FILTER_NONE,
                            0, NULL, NULL, &gloveTexture))) {
                // store in the map
                if (left) {
                    it->second._gk_glove_left = gloveTexture;
                    LogWithNumber(&k_skin, "getSkinTexture: loaded left GK glove texture for player %d", playerId);
                }
                else {
                    it->second._gk_glove_right = gloveTexture;
                    LogWithNumber(&k_skin, "getSkinTexture: loaded right GK glove texture for player %d", playerId);
                }
            } else {
                LogWithString(&k_skin, "D3DXCreateTextureFromFileEx FAILED for %s", filename);
            }

            // update loaded flags, so that we only load each texture once
            if (left) {
                it->second._gkGloveLeftLoaded = true;
            }
            else {
                it->second._gkGloveRightLoaded = true;
            }
        }
    }
    return gloveTexture;
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
int hand_count = 0;
IDirect3DTexture8 *skinTex = NULL;

void skinBeginRenderPlayer(DWORD playerMainColl)
{
    DWORD pmc=0, playerNumber=0;
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
            pgloves=(BYTE*)(pmc+0x1fe);
            pmc=*(DWORD*)(pmc+0x10);

            if (pmc==playerMainColl) {
                //PES is going to render this player now
                currRenderPlayer = playerNumber;
                currRenderPlayerRecord = playerRecord(i);
                has_gloves = *pgloves;
                hand_count = 0;

#ifdef SKIN_TEXDUMP
                if (dump_now) LOG(&k_skin, ">>>>>>>>>>>> currRenderPlayer: %d, currRenderPlayerRecord: %p, pmc = %08x, has_gloves=%d",
                    currRenderPlayer, currRenderPlayerRecord, pmc, has_gloves);

                // dump textures for current player
                if (dump_now) {
                    if (dumping) {
                        dump_now = false;
                        dumping = false;
                    }
                    else {
                        dumping = true;
                    }
                }
#endif

                BYTE temp=32*currRenderPlayerRecord->team + currRenderPlayerRecord->posInTeam;
                if (currRenderPlayer != g_skinPlayerIds[temp]) {
                    g_skinPlayerIds[temp]=currRenderPlayer;
                    g_skinTextures[0][temp]=NULL;
                    g_skinTextures[1][temp]=NULL;
                    g_gloveTextures[0][temp]=NULL;
                    g_gloveTextures[1][temp]=NULL;
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
                        g_handsTexturesColl[lod]=*bodyColl;  //remember p2 value for this lod level
                        g_handsTexturesPos[lod] = 0;
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

#ifdef SKIN_TEXDUMP
    if (dump_now) LOG(&k_skin, "skinPesGetTexture:: p1=%08x, p2=%08x, p3=%08x, *res=%p", p1, p2, p3, *res);

    if (dump_now) {
        char buf[BUFLEN];
        sprintf(buf,"%s\\%p.bmp", GetPESInfo()->mydir, *res);
        if (SUCCEEDED(D3DXSaveTextureToFile(buf, D3DXIFF_BMP, *res, NULL))) {
            //LOG(&k_skin, "Saved texture [%p] to: %s", *res, buf);
        }
        else {
            LOG(&k_skin, "FAILED to save texture to: %s", buf);
        }
    }
#endif

    for (int lod=0; lod<5; lod++) {
        if (p2==g_skinTexturesColl[lod] && p3==g_skinTexturesPos[lod]) {
            skinTex = *res;
#ifdef SKIN_TEXDUMP
            if (dump_now) LOG(&k_skin, "skinPesGetTexture:: ^^^ skin texture!! p1=%08x, p2=%08x, p3=%08x, *res=%p", p1, p2, p3, *res);
#endif

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
            hand_count += 1;
            // check for GK gloves
            if (*res != skinTex) {
#ifdef SKIN_TEXDUMP
                if (dump_now) LOG(&k_skin, "skinPesGetTexture:: ^^^ GK glove texture!! p1=%08x, p2=%08x, p3=%08x, *res=%p", p1, p2, p3, *res);
#endif
                if (has_gloves) {
                    int j=hand_count-1;
                    BYTE temp=32*currRenderPlayerRecord->team + currRenderPlayerRecord->posInTeam;
                    IDirect3DTexture8* gloveTexture=g_gloveTextures[j][temp];
                    if ((DWORD)gloveTexture != 0xffffffff) { //0xffffffff means: has no gdb skin
                        if (!gloveTexture) {
                            //no skin texture in cache yet
                            gloveTexture = getGkGloveTexture(currRenderPlayer, j==0);
                            if (!gloveTexture) {
                                //no texture assigned
                                gloveTexture = (IDirect3DTexture8*)0xffffffff;
                            } else {
                                *res=gloveTexture;
                            }
                            //cache the texture pointer
                            g_gloveTextures[j][temp]=gloveTexture;
                        } else {
                            //set texture
                            *res=gloveTexture;
                        }
                    }
                }
                // done for this texture
                break;
            }

#ifdef SKIN_TEXDUMP
            if (dump_now) LOG(&k_skin, "skinPesGetTexture:: ^^^ hand texture!! p1=%08x, p2=%08x, p3=%08x, *res=%p", p1, p2, p3, *res);
#endif

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
#ifdef SKIN_TEXDUMP
            if (dump_now) LOG(&k_skin, "skinPesGetTexture:: ^^^ neck texture!! p1=%08x, p2=%08x, p3=%08x, *res=%p", p1, p2, p3, *res);
#endif

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
