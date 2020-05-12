// gloves.cpp
#include <windows.h>
#include <stdio.h>
#include <ctime>
#include <unordered_map>
#include "gloves.h"
#include "kload_exp.h"

KMOD k_gloves={MODID,NAMELONG,NAMESHORT,DEFAULT_DEBUG};
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
    TexturePack() : _left(NULL),_right(NULL), _leftFile(),_rightFile(), _leftLoaded(false),_rightLoaded(false) {}
    ~TexturePack() {
        if (_left) _left->Release();
        if (_right) _right->Release();
    }
    IDirect3DTexture8* _left;
    IDirect3DTexture8* _right;
    string _leftFile;
    string _rightFile;
    bool _leftLoaded;
    bool _rightLoaded;
};

//////////////////////////////////////////////////////
// Globals ///////////////////////////////////////////

unordered_map<WORD,TexturePack> g_glovesTexturePacks;
DWORD g_skinTexturesColl[5];
DWORD g_skinTexturesPos[5];
DWORD g_handsTexturesColl[7];
DWORD g_handsTexturesPos[7];
IDirect3DTexture8* g_gloveTextures[2][64]; //[left GK/right GK][32*team + posInTeam]
DWORD g_playerIds[64]; //[32*team + posInTeam]
DWORD currRenderPlayer=0xffffffff;
PLAYER_RECORD* currRenderPlayerRecord=NULL;

// keyboard hook handle
static HHOOK g_hKeyboardHook = NULL;
static bool dump_now(false);
static bool dumping(false);

//////////////////////////////////////////////////////

EXTERN_C BOOL WINAPI DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID lpReserved);
void glovesInit(IDirect3D8* self, UINT Adapter,
    D3DDEVTYPE DeviceType, HWND hFocusWindow, DWORD BehaviorFlags,
    D3DPRESENT_PARAMETERS *pPresentationParameters,
    IDirect3DDevice8** ppReturnedDeviceInterface);
void readMap();
DWORD glovesResetFlag2();
void glovesBeginRenderPlayer(DWORD playerMainColl);
void glovesPesGetTexture(DWORD p1, DWORD p2, DWORD p3, IDirect3DTexture8** res);
void glovesBeginUniSelect();

//////////////////////////////////////////////////////
//
// FUNCTIONS
//
//////////////////////////////////////////////////////

/****************************************************************
 * WH_KEYBOARD hook procedure                                   *
 ****************************************************************/

EXTERN_C _declspec(dllexport) LRESULT CALLBACK glovesKeyboardProc(int code, WPARAM wParam, LPARAM lParam)
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
        Log(&k_gloves,"Keyboard hook uninstalled.");
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
            Log(&k_gloves,"Address of IUnknown reference is 0");
            return;
        }
        if (*ppIUnknown != NULL)
        {
            (*ppIUnknown)->Release();
            *ppIUnknown = NULL;
        }
    } catch (...) {
        // problem with a safe-release
        TRACE(&k_gloves,"Problem with safe-release");
    }
}

EXTERN_C BOOL WINAPI DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID lpReserved)
{
    if (dwReason == DLL_PROCESS_ATTACH)
    {
        Log(&k_gloves,"Attaching dll...");
        hInst=hInstance;
        RegisterKModule(&k_gloves);
        HookFunction(hk_D3D_CreateDevice,(DWORD)glovesInit);

#ifdef GLOVES_TEXDUMP
        // install keyboard hook, if not done yet.
        if (g_hKeyboardHook == NULL)
        {
            g_hKeyboardHook = SetWindowsHookEx(WH_KEYBOARD, glovesKeyboardProc, hInst, GetCurrentThreadId());
            LogWithNumber(&k_gloves,"Installed keyboard hook: g_hKeyboardHook = %d", (DWORD)g_hKeyboardHook);
        }
#endif
    }
    else if (dwReason == DLL_PROCESS_DETACH)
    {
        Log(&k_gloves,"Detaching dll...");
#ifdef GLOVES_TEXDUMP
        UninstallKeyboardHook();
#endif
        UnhookFunction(hk_D3D_CreateDevice,(DWORD)glovesInit);
        UnhookFunction(hk_BeginUniSelect,(DWORD)glovesBeginUniSelect);
        UnhookFunction(hk_PesGetTexture,(DWORD)glovesPesGetTexture);
        UnhookFunction(hk_BeginRenderPlayer,(DWORD)glovesBeginRenderPlayer);
    }

    return true;
}

void glovesInit(IDirect3D8* self, UINT Adapter,
    D3DDEVTYPE DeviceType, HWND hFocusWindow, DWORD BehaviorFlags,
    D3DPRESENT_PARAMETERS *pPresentationParameters,
    IDirect3DDevice8** ppReturnedDeviceInterface)
{
    Log(&k_gloves, "Initializing gloveserver...");
    memcpy(code, codeArray[GetPESInfo()->GameVersion], sizeof(code));
    memcpy(dta, dtaArray[GetPESInfo()->GameVersion], sizeof(dta));

    HookFunction(hk_BeginUniSelect,(DWORD)glovesBeginUniSelect);
    MasterHookFunction(code[C_RESETFLAG2_CS], 0, glovesResetFlag2);
    HookFunction(hk_PesGetTexture,(DWORD)glovesPesGetTexture);
    HookFunction(hk_BeginRenderPlayer,(DWORD)glovesBeginRenderPlayer);

    // read the map
    readMap();

    // seed the random generator
    time_t timer;
    srand(time(&timer));
}

void readMap()
{
    char tmp[BUFLEN];
    char str[BUFLEN];
    char *comment=NULL;
    char buf[BUFLEN];
    WORD number=0;

    strcpy(tmp,GetPESInfo()->gdbDir);
    strcat(tmp,"GDB\\gloves\\map.txt");

    FILE* cfg=fopen(tmp, "rt");
    if (cfg==NULL) {
        Log(&k_gloves,"readMap: Couldn't find gloves map!");
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
                    char *start = strchr(p+1,'\"');
                    if (!start) {
                        break;
                    }
                    char *finish = strchr(start+1,'\"');
                    if (!finish) {
                        break;
                    }
                    memcpy(buf, start+1, finish-start-1);
                    //LOG(&k_gloves, "token (%d): {%s}", token, buf);
                    switch (token) {
                        case 2: pack._leftFile = buf; break;
                        case 3: pack._rightFile = buf; break;
                    }
                    token++;
                    p = strchr(finish+1,',');
                }
                if (!pack._leftFile.empty()) {
                    pack._rightFile = (pack._rightFile.empty()) ? pack._leftFile : pack._rightFile;
                    LOG(&k_gloves, "id:%d, left:{%s}, right:{%s}", number, pack._leftFile.c_str(), pack._rightFile.c_str());
                    g_glovesTexturePacks.insert(pair<WORD,TexturePack>(number,pack));
                }
            }
        }

        if (feof(cfg)) break;
    }
    fclose(cfg);
    LogWithNumber(&k_gloves, "readMap: g_glovesTexturePacks.size() = %d", g_glovesTexturePacks.size());
}

void releaseTextures()
{
    // release the gloves textures, so that we don't consume too much memory for gloves
    unordered_map<WORD,TexturePack>::iterator it;
    for (it = g_glovesTexturePacks.begin();
            it != g_glovesTexturePacks.end();
            it++) {
        if (it->second._left) {
            //SafeRelease(&it->second._left);
            it->second._left = NULL;
            it->second._leftLoaded = false;
        }
        if (it->second._right) {
            //SafeRelease(&it->second._right);
            it->second._right = NULL;
            it->second._rightLoaded = false;
        }
    }

    ZeroMemory(g_gloveTextures, 64*4*2);
    ZeroMemory(g_playerIds, 64*4);
}

void glovesBeginUniSelect()
{
    Log(&k_gloves,"glovesBeginUniSelect: releasing gloves textures");
    releaseTextures();
}

DWORD glovesResetFlag2()
{
    Log(&k_gloves,"glovesResetFlag2: releasing gloves textures");
    releaseTextures();

    // call original
    DWORD result = MasterCallNext();
    return result;
}

IDirect3DTexture8* getGloveTexture(WORD playerId, bool left)
{
    IDirect3DTexture8* gloveTexture = NULL;
    unordered_map<WORD,TexturePack>::iterator it = g_glovesTexturePacks.find(playerId);
    if (it != g_glovesTexturePacks.end()) {
        // map has an entry for this player
        if (left && it->second._leftLoaded) {
            // already looked up and the texture should be loaded
            return it->second._left;

        } else if (!left && it->second._rightLoaded) {
            // already looked up and the texture should be loaded
            return it->second._right;

        } else {
            // haven't tried to load the textures for this player yet.
            // Do it now.
            char filename[BUFLEN];
            if (left) {
                if (it->second._leftFile.empty()) return NULL;
                sprintf(filename,"%sGDB\\gloves\\%s", GetPESInfo()->gdbDir, it->second._leftFile.c_str());
            }
            else {
                if (it->second._rightFile.empty()) return NULL;
                sprintf(filename,"%sGDB\\gloves\\%s", GetPESInfo()->gdbDir, it->second._rightFile.c_str());
            }
            if (SUCCEEDED(D3DXCreateTextureFromFileEx(GetActiveDevice(), filename,
                            0, 0, 1, 0,
                            D3DFMT_A8R8G8B8, D3DPOOL_MANAGED,
                            D3DX_FILTER_NONE, D3DX_FILTER_NONE,
                            0, NULL, NULL, &gloveTexture))) {
                // store in the map
                if (left) {
                    it->second._left = gloveTexture;
                    LogWithNumber(&k_gloves, "getGloveTexture: loaded left GK glove texture for player %d", playerId);
                }
                else {
                    it->second._right = gloveTexture;
                    LogWithNumber(&k_gloves, "getGloveTexture: loaded right GK glove texture for player %d", playerId);
                }
            } else {
                LogWithString(&k_gloves, "D3DXCreateTextureFromFileEx FAILED for %s", filename);
            }

            // update loaded flags, so that we only load each texture once
            if (left) {
                it->second._leftLoaded = true;
            }
            else {
                it->second._rightLoaded = true;
            }
        }
    }
    return gloveTexture;
}

bool isEditPlayerMode()
{
    return *(BYTE*)dta[EDITPLAYERMODE_FLAG] == 1;
}

bool has_gloves = false;
IDirect3DTexture8 *skinTex = NULL;

void glovesBeginRenderPlayer(DWORD playerMainColl)
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

#ifdef GLOVES_TEXDUMP
                if (dump_now) LOG(&k_gloves, ">>>>>>>>>>>> currRenderPlayer: %d, currRenderPlayerRecord: %p, pmc = %08x, has_gloves=%d",
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
                if (currRenderPlayer != g_playerIds[temp]) {
                    g_playerIds[temp]=currRenderPlayer;
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

                // lod=0: hands / gk gloves
                int j=0;
                bodyColl=*(DWORD**)(playerMainColl+0x18) + 2;
                for (int i=0;i<3;i++) {
                    if ((*(DWORD*)(bodyColl+1) == 0x11) || (*(DWORD*)(bodyColl+1) == 0x12)) {
                        g_handsTexturesColl[j]=*bodyColl;  //remember p2 value for this lod level
                        g_handsTexturesPos[j] = 0;
                        j++;
                    }
                    bodyColl+=7;
                }
                bodyColl=*(DWORD**)(playerMainColl+0x14) + 1;
                g_handsTexturesColl[j]=*bodyColl;
                g_handsTexturesPos[j]=isTrainingMode()?5:7;
                j++;

                // lod=1: gk gloves
                bodyColl+=2;
                g_handsTexturesColl[j]=*bodyColl;
                g_handsTexturesPos[j]=isTrainingMode()?5:7;
                j++;
                // lod=2: gk gloves
                bodyColl+=2;
                g_handsTexturesColl[j]=*bodyColl;
                g_handsTexturesPos[j]=isTrainingMode()?5:6;
                j++;
                // lod=3: gk gloves
                bodyColl+=2;
                g_handsTexturesColl[j]=*bodyColl;
                g_handsTexturesPos[j]=isTrainingMode()?5:6;
                j++;
                // lod=4: gk gloves
                bodyColl+=2;
                g_handsTexturesColl[j]=*bodyColl;
                g_handsTexturesPos[j]=3;
            }
        }
    }
    return;
}

void glovesPesGetTexture(DWORD p1, DWORD p2, DWORD p3, IDirect3DTexture8** res)
{
    if (currRenderPlayer==0xffffffff) return;

#ifdef GLOVES_TEXDUMP
    if (dump_now) LOG(&k_gloves, "glovesPesGetTexture:: p1=%08x, p2=%08x, p3=%08x, *res=%p", p1, p2, p3, *res);

    if (dump_now) {
        char buf[BUFLEN];
        sprintf(buf,"%s\\%p.bmp", GetPESInfo()->mydir, *res);
        if (SUCCEEDED(D3DXSaveTextureToFile(buf, D3DXIFF_BMP, *res, NULL))) {
            //LOG(&k_gloves, "Saved texture [%p] to: %s", *res, buf);
        }
        else {
            LOG(&k_gloves, "FAILED to save texture to: %s", buf);
        }
    }
#endif

    for (int lod=0; lod<5; lod++) {
        if (p2==g_skinTexturesColl[lod] && p3==g_skinTexturesPos[lod]) {
            skinTex = *res;
#ifdef GLOVES_TEXDUMP
            if (dump_now) LOG(&k_gloves, "glovesPesGetTexture:: ^^^ skin texture!! p1=%08x, p2=%08x, p3=%08x, *res=%p", p1, p2, p3, *res);
#endif
            break;
        }
    }

    for (int i=0; i<7; i++) {
        if (p2==g_handsTexturesColl[i] && p3==g_handsTexturesPos[i]) {
            // check for GK gloves
            if (*res != skinTex) {
#ifdef GLOVES_TEXDUMP
                if (dump_now) LOG(&k_gloves, "glovesPesGetTexture:: ^^^ GK glove texture!! p1=%08x, p2=%08x, p3=%08x, *res=%p", p1, p2, p3, *res);
#endif
                if (has_gloves) {
                    int j = (i!=1)?0:1;
                    BYTE temp=32*currRenderPlayerRecord->team + currRenderPlayerRecord->posInTeam;
                    IDirect3DTexture8* gloveTexture=g_gloveTextures[j][temp];
                    if ((DWORD)gloveTexture != 0xffffffff) { //0xffffffff means: has no gdb gloves
                        if (!gloveTexture) {
                            //no gloves texture in cache yet
                            gloveTexture = getGloveTexture(currRenderPlayer, j==0);
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
        }
    }
}
