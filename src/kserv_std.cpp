/* kserv.cpp */
/* KITSERVER ENGINE */

#include <windows.h>
#define _COMPILING_MYDLL
#include <stdio.h>
#include <limits.h>
#include <math.h>

#include "kserv.h"
#include "kload_exp.h"
#include "shared.h"
#include "kserv_config.h"
#include "gdb.h"
#include "crc32.h"
#include "afsreader.h"
#include "input.h"

#include "d3dfont.h"
#include "dxutil.h"
#include "d3d8types.h"
#include "d3d8.h"
#include "d3dx8tex.h"

#include <pngdib.h>
#include <hash_map>
#include <string>
#include <vector>

//#include "allocator.h"

#define VTAB_CREATEDEVICE 15
#define VTAB_RESET 14
#define VTAB_PRESENT 15
#define VTAB_CREATETEXTURE 20
#define VTAB_SETTEXTURE 61
#define VTAB_DELETESTATEBLOCK 56

#define SWAPBYTES(dw) \
    (dw<<24 & 0xff000000) | (dw<<8  & 0x00ff0000) | \
    (dw>>8  & 0x0000ff00) | (dw>>24 & 0x000000ff)

#define SAFEFREE(ptr) if (ptr) { HeapFree(GetProcessHeap(),0,ptr); ptr=NULL; }
#define IS_CUSTOM_TEAM(id) (id == 0xffff) 

#define MAX_ITERATIONS 1000

//#define MAP_FIND(map,key) (((*(map)).count(key)>0) ? (*(map))[key] : NULL)
#define MAP_FIND(map,key) (((*(map)).find(key) != (*(map)).end()) ? (*(map))[key] : NULL)
#define MAP_CONTAINS(map,key) (map.find(key)!=map.end())

KMOD k_mydll={MODID,NAMELONG,NAMESHORT,DEFAULT_DEBUG};

/**************************************
Shared by all processes variables
**************************************/
#pragma data_seg(".HKT")
KSERV_CONFIG g_config;
#pragma data_seg()
#pragma comment(linker, "/section:.HKT,rws")

/************************** 
End of shared section 
**************************/

CRITICAL_SECTION g_cs;

//bool g_fontInitialized = false;
//CD3DFont* g_font = NULL;

WORD g_licensed_ordinals[] = {
    4,6,8,13,15,26,27,0x2c,0x35,0x37,0x38,
    0x40,0x4a,
    0x54,0x55,0x56,0x57,0x58,0x59,0x5a,0x5b,0x5c,0x5d,
    0x5e,0x5f,0x60,0x61,0x62,0x63,0x64,0x65,0x66,0x67,
    0x68,0x69,0x6a,0x6b,0x6c,0x6d,0x6e,0x6f,0x70,0x71,
    0x72,0x73,0x74,0x75,0x76,0x77,0x78,0x79,0x7a,0x7b,
    0x7c,0x7d,0x7e,0x7f,0x80,0x81,0x82,0x83,0x84,0x85,
    0x86,0x87,0x88,0x89,0x8a,0x8b,0x8c,0x8d,
    0x8e,0x8f,0x90,0x91,0x92,0x93,0x94,0x95,0x96,0x97,
    0x98,0x99,0x9a,0x9b,0x9c,0x9d,0x9e,0x9f,0xa0,0xa1,
    0xa5,0xa6,0xa7,0xa9,0xaa,0xab,0xac,0xad,0xae,0xaf,
    0xb0,0xb3,0xb4,
};

#define TD_CHECKED 0x01
#define TD_EXISTS 0x02

BYTE g_teamDir[256];
BOOL g_restored = false;
DWORD g_ML_kitPackInfoAddr = 0;

#define KI_GKHOME 0x01
#define KI_GKAWAY 0x02
#define KI_PLHOME 0x04
#define KI_PLAWAY 0x08

#define HOME 0
#define AWAY 1

#define STRIP_PL_SHIRT    0x01
#define STRIP_PL_SHORTS   0x02
#define STRIP_PL_SOCKS    0x04
#define STRIP_PL_SHIRT_C  0xfe
#define STRIP_PL_SHORTS_C 0xfd
#define STRIP_PL_SOCKS_C  0xfb
#define STRIP_GK_SHIRT    0x08
#define STRIP_GK_SHORTS   0x10
#define STRIP_GK_SOCKS    0x20
#define STRIP_GK_SHIRT_C  0xf7
#define STRIP_GK_SHORTS_C 0xef
#define STRIP_GK_SOCKS_C  0xdf

#define MAKE_BUFFER(s) char s[BUFLEN]; ZeroMemory(s,BUFLEN)
#define CLEAR_BUFFER(s) ZeroMemory(s,BUFLEN)

DWORD g_kitID = 0;
DWORD g_follows = 0;
DWORD g_frame_tex_count = 0;
DWORD g_frame_count = 0;
BOOL g_dumpTexturesMode = FALSE;
BOOL g_display2Dkits = FALSE;
BOOL g_lastDisplay2Dkits = FALSE;
bool g_kit_loading_enabled = false;
bool g_edit_mode = false;
bool g_unidecode_flag = false;
DWORD g_currentAfsId = false;
RGBAColor* savedRadarColor[2]={NULL, NULL};

typedef struct _TextureBinding {
    IDirect3DTexture8* srcTexture;
    IDirect3DTexture8* repTexture;
    UINT levels;
} TextureBinding;

hash_map<DWORD,DWORD> _texture_to_id;
hash_map<DWORD,DWORD> _source_to_id;
vector<TextureBinding> _textureBindings;

// textures for strip-selection
IDirect3DTexture8* g_home_shirt_tex = NULL;
IDirect3DTexture8* g_home_shorts_tex = NULL;
IDirect3DTexture8* g_home_socks_tex = NULL;
IDirect3DTexture8* g_away_shirt_tex = NULL;
IDirect3DTexture8* g_away_shorts_tex = NULL;
IDirect3DTexture8* g_away_socks_tex = NULL;

IDirect3DTexture8* g_gloves_left_tex = NULL;

using namespace std;
// map of textures for 2Dkits
hash_map<string,IDirect3DTexture8*> g_kitTextureMap;

// kit collection iterators
typedef StringKitMap::iterator KitIterator;

KitIterator g_homeShirtIterator;
KitIterator g_homeShortsIterator;
KitIterator g_homeSocksIterator;
KitIterator g_awayShirtIterator;
KitIterator g_awayShortsIterator;
KitIterator g_awaySocksIterator;

KitIterator g_homeShirtIteratorPL;
KitIterator g_homeShortsIteratorPL;
KitIterator g_homeSocksIteratorPL;
KitIterator g_awayShirtIteratorPL;
KitIterator g_awayShortsIteratorPL;
KitIterator g_awaySocksIteratorPL;
KitIterator g_homeShirtIteratorGK;
KitIterator g_homeShortsIteratorGK;
KitIterator g_homeSocksIteratorGK;
KitIterator g_awayShirtIteratorGK;
KitIterator g_awayShortsIteratorGK;
KitIterator g_awaySocksIteratorGK;

BYTE g_homeStrip = 0xff;
BYTE g_awayStrip = 0xff;

BITMAPINFO* g_shirtMaskTex = NULL;
BITMAPINFO* g_shirtMaskTexMip1 = NULL;
BITMAPINFO* g_shirtMaskTexMip2 = NULL;
BITMAPINFO* g_shortsMaskTex = NULL;
BITMAPINFO* g_shortsMaskTexMip1 = NULL;
BITMAPINFO* g_shortsMaskTexMip2 = NULL;
BITMAPINFO* g_socksMaskTex = NULL;
BITMAPINFO* g_socksMaskTexMip1 = NULL;
BITMAPINFO* g_socksMaskTexMip2 = NULL;
BITMAPINFO* g_testMaskTex = NULL;
DWORD* g_shirtMask = NULL;
DWORD* g_shirtMaskMip1 = NULL;
DWORD* g_shirtMaskMip2 = NULL;
DWORD* g_shortsMask = NULL;
DWORD* g_shortsMaskMip1 = NULL;
DWORD* g_shortsMaskMip2 = NULL;
DWORD* g_socksMask = NULL;
DWORD* g_socksMaskMip1 = NULL;
DWORD* g_socksMaskMip2 = NULL;
DWORD* g_testMask = NULL;

WORD _last_homeId = 0;
WORD _last_awayId = 0;

bool g_shirtChange[] = {false,false};

char* WHICH_TEAM[] = { "HOME", "AWAY" };

enum { TEXTYPE_NONE, TEXTYPE_BMP, TEXTYPE_PNG };

#define FILES_PER_TEAM 12

// KIT FILE TYPES
/*
enum {
    GA_KIT, GA_UNKNOWN1, GA_UNKNOWN2, GA_NUMBERS, GA_FONT,
    GB_KIT, GB_UNKNOWN1, GB_UNKNOWN2, GB_NUMBERS, GB_FONT,
    PA_SHIRT, PA_SHORTS, PA_SOCKS, PA_NUMBERS, PA_FONT,
    PB_SHIRT, PB_SHORTS, PB_SOCKS, PB_NUMBERS, PB_FONT,
};
*/
enum {
    GA_KIT, GA_NUMBERS, GA_FONT,
    GB_KIT, GB_NUMBERS, GB_FONT,
    PA_SHIRT, PA_NUMBERS, PA_FONT,
    PB_SHIRT, PB_NUMBERS, PB_FONT,

    GA_UNKNOWN1, GA_UNKNOWN2,
    GB_UNKNOWN1, GB_UNKNOWN2,
    PA_SHORTS, PA_SOCKS,
    PB_SHORTS, PB_SOCKS,
};

// KIT KEYS
enum {
    GA, GB, PA, PB,
};

string g_homeShirtKeyGK;
string g_homeShortsKeyGK;
string g_homeSocksKeyGK;
string g_homeShirtKeyPL;
string g_homeShortsKeyPL;
string g_homeSocksKeyPL;
string g_awayShirtKeyGK;
string g_awayShortsKeyGK;
string g_awaySocksKeyGK;
string g_awayShirtKeyPL;
string g_awayShortsKeyPL;
string g_awaySocksKeyPL;

#define GK_TYPE 0
#define PL_TYPE 1
#define GET_HOME_SHIRT_KEY(typ) ((typ==GK_TYPE)?g_homeShirtKeyGK:g_homeShirtKeyPL)
#define GET_HOME_SHORTS_KEY(typ) ((typ==GK_TYPE)?g_homeShirtKeyGK:g_homeShirtKeyPL)
#define GET_HOME_SOCKS_KEY(typ) ((typ==GK_TYPE)?g_homeShirtKeyGK:g_homeShirtKeyPL)
//#define GET_HOME_SHORTS_KEY(typ) ((typ==GK_TYPE)?g_homeShortsKeyGK:g_homeShortsKeyPL)
//#define GET_HOME_SOCKS_KEY(typ) ((typ==GK_TYPE)?g_homeSocksKeyGK:g_homeSocksKeyPL)
#define GET_AWAY_SHIRT_KEY(typ) ((typ==GK_TYPE)?g_awayShirtKeyGK:g_awayShirtKeyPL)
#define GET_AWAY_SHORTS_KEY(typ) ((typ==GK_TYPE)?g_awayShirtKeyGK:g_awayShirtKeyPL)
#define GET_AWAY_SOCKS_KEY(typ) ((typ==GK_TYPE)?g_awayShirtKeyGK:g_awayShirtKeyPL)
//#define GET_AWAY_SHORTS_KEY(typ) ((typ==GK_TYPE)?g_awayShortsKeyGK:g_awayShortsKeyPL)
//#define GET_AWAY_SOCKS_KEY(typ) ((typ==GK_TYPE)?g_awaySocksKeyGK:g_awaySocksKeyPL)

// global kit selection type
int typ = PL_TYPE;
BOOL g_modeSwitch = FALSE;
WORD g_last_home = 0xff;
WORD g_last_away = 0xff;

#define DATALEN 22
#define CODELEN 21

// data array names
enum {
	TEAM_IDS, TEAM_STRIPS, KIT_SLOTS, 
	ANOTHER_KIT_SLOTS, MLDATA_PTRS, TEAM_COLLARS_PTR,
	KIT_CHECK_TRIGGER, FIRST_ID, LAST_ID, 
    NATIONAL_TEAMS_ADDR, CLUB_TEAMS_ADDR, 
	ML_HOME_AREA, ML_AWAY_AREA,
    STRIPS_CONTROL, TEAM_STRIPS_HOME, TEAM_STRIPS_AWAY,
    LICENSED_LIST,
    SAVED_TEAM_HOME, SAVED_TEAM_AWAY,
    TRAINING_KIT, OFFLINE_FLAG,
    RADARCOLORS,
};

// Data addresses.
DWORD dataArray[][DATALEN] = {
    // PES6
    { 0x3be0940, 0x3be6078, 0,
      0, 0, 0,
      0, 5473, 6831, 
      0x113200c, 0x1132010, 0x1131fe4, 0x1131fe8,
      0x3a71338, 0x3a7133c, 0x3a7134c,
      0xd42e20,
      0x1131fd4, 0x1131fd8,
      6793, 0x3be6320,
      0x3be14a1,
    },
    // PES6 1.10
    { 0x3be1940, 0x3be7078, 0,
      0, 0, 0,
      0, 5473, 6831, 
      0x113300c, 0x1133010, 0x1132fe4, 0x1132fe8,
      0x3a72338, 0x3a7233c, 0x3a7234c,
      0xd43f00,
      0x1132fd4, 0x1132fd8,
      6793, 0x3be7320,
      0x3be24a1,
    },
    // WE2007
    { 0x3bdb3c0, 0x3be0af8, 0,
      0, 0, 0,
      0, 5877, 7235, 
      0x112ca94, 0x112ca98, 0x112ca6c, 0x112ca70,
      0x3a6bdb8, 0x3a6bdbc, 0x3a6bdcc,
      0xd3cf40,
      0x112ca5c, 0x112ca60,
      7197, 0x3be0da0,
      0x3bdbf21,
    },
};

enum { 
    C_GETTEAMINFO, C_GETTEAMINFO_CS,
    C_READTEAMINFO, C_READTEAMINFO_CS,
    C_READTEAMINFO2, C_READTEAMINFO2_CS,
    C_READTEAMINFO_MLSAVE, C_READTEAMINFO_MLSAVE_CS,
    C_ONREPLAYLOAD, C_ONREPLAYLOAD_CS,
    C_SETFLAG_CS,
    C_RESETFLAG_CS,
    C_RESETFLAG2_CS,
    C_WRITEKITINFO_CS, C_WRITEKITINFO_CS2, C_WRITEKITINFO_CS3, 
    C_WRITEKITINFO_CS4, C_WRITEKITINFO,
    C_PROCESSKIT_CS, C_PROCESSKIT,
    C_SETRADARCOLOR_CS,
};

DWORD codeArray[][CODELEN] = {
    // PES6
    {
        0x865240, 0x804cda,
        0x865100, 0xb3b720,
        0x864ef0, 0xb3b6a9,
        0xb35aa0, 0xb32417,
        0xb3b880, 0xb31dd0,
        0x9d9bd9,
        0x9d9c53,
        0x9c15a7,
        0x6b2487, 0x966e15, 0x966e62, 
        0x80843e, 0x865380,
        0x8b1a59, 0x8d1a60,
        0x9cc37c,
    },
    // PES6 1.10
    {
        0x865370, 0x804e5a,
        0x865230, 0xb3b8d0,
        0x865020, 0xb3b859,
        0xb35c50, 0xb325c7,
        0xb3ba30, 0xb31f80,
        0x9d9d69,
        0x9d9de3,
        0x9c1737,
        0x6b25a7, 0x966f95, 0x966fe2, 
        0x8085ae, 0x8654b0,
        0x8b1b39, 0x8d1b50,
        0x9cc50c,
    },
    // WE2007
    {
        0x865950, 0x804caa, 
        0x865810, 0xb3c230,
        0x865600, 0xb3c1b9,
        0xb365b0, 0xb32f27,
        0xb3c390, 0xb328e0,
        0x9da3f9,
        0x9da473,
        0x9c1d97,
        0x6b2467, 0x9675b5, 0x967602, 
        0x8083fe, 0x865a90,
        0x8b2259, 0x8d2230,
        0x9ccb6c,
    },
};

DWORD code[CODELEN];
DWORD data[DATALEN];

std::hash_map<DWORD,bool> g_AFS_idMap;

// saves modified kitpack info
std::hash_map<DWORD,TEAMKITINFO*> g_teamKitInfo;
std::hash_map<DWORD,TEAMKITINFO*>::iterator g_teamKitInfoIterator;

// Actual number of teams in the game
int g_numTeams = 0; 

TEAMBUFFERINFO g_teamInfo[256];
char g_afsFileName[BUFLEN];
UINT g_triggerLoad3rdKit = 0;

WORD g_currTeams[2] = {0xffff, 0xffff};

//////////////////////////////////////

// text labels for default strips
char* DEFAULT_LABEL[] = { "1st default", "2nd default", "auto-pick" };

// Graphics Database
GDB* gdb = NULL;

///// Graphics ////////////////

struct CUSTOMVERTEX { 
	FLOAT x,y,z,w;
	DWORD color;
};

struct CUSTOMVERTEX2 { 
	FLOAT x,y,z,w;
	DWORD color;
	FLOAT tu, tv;
};

// SHIRT
IDirect3DVertexBuffer8* g_pVB_home_shirt_left[4];
IDirect3DVertexBuffer8* g_pVB_home_sleeve_left[4];
IDirect3DVertexBuffer8* g_pVB_home_shirt_right[4];
IDirect3DVertexBuffer8* g_pVB_home_sleeve_right[4];
IDirect3DVertexBuffer8* g_pVB_home_shirt_outline[4];
IDirect3DVertexBuffer8* g_pVB_home_sleeve_left_outline[4];
IDirect3DVertexBuffer8* g_pVB_home_sleeve_right_outline[4];
// SHORTS
IDirect3DVertexBuffer8* g_pVB_home_shorts[4];
IDirect3DVertexBuffer8* g_pVB_home_shorts_2[4];
IDirect3DVertexBuffer8* g_pVB_home_shorts_outline[4];
IDirect3DVertexBuffer8* g_pVB_home_shorts_2_outline[4];
// SOCKS
IDirect3DVertexBuffer8* g_pVB_home_socks_shin_left[4];
IDirect3DVertexBuffer8* g_pVB_home_socks_foot_left[4];
IDirect3DVertexBuffer8* g_pVB_home_socks_left_outline[4];
IDirect3DVertexBuffer8* g_pVB_home_socks_shin_right[4];
IDirect3DVertexBuffer8* g_pVB_home_socks_foot_right[4];
IDirect3DVertexBuffer8* g_pVB_home_socks_right_outline[4];

// SHIRT
IDirect3DVertexBuffer8* g_pVB_away_shirt_left[4];
IDirect3DVertexBuffer8* g_pVB_away_sleeve_left[4];
IDirect3DVertexBuffer8* g_pVB_away_shirt_right[4];
IDirect3DVertexBuffer8* g_pVB_away_sleeve_right[4];
IDirect3DVertexBuffer8* g_pVB_away_shirt_outline[4];
IDirect3DVertexBuffer8* g_pVB_away_sleeve_left_outline[4];
IDirect3DVertexBuffer8* g_pVB_away_sleeve_right_outline[4];
// SHORTS
IDirect3DVertexBuffer8* g_pVB_away_shorts[4];
IDirect3DVertexBuffer8* g_pVB_away_shorts_2[4];
IDirect3DVertexBuffer8* g_pVB_away_shorts_outline[4];
IDirect3DVertexBuffer8* g_pVB_away_shorts_2_outline[4];
// SOCKS
IDirect3DVertexBuffer8* g_pVB_away_socks_shin_left[4];
IDirect3DVertexBuffer8* g_pVB_away_socks_foot_left[4];
IDirect3DVertexBuffer8* g_pVB_away_socks_left_outline[4];
IDirect3DVertexBuffer8* g_pVB_away_socks_shin_right[4];
IDirect3DVertexBuffer8* g_pVB_away_socks_foot_right[4];
IDirect3DVertexBuffer8* g_pVB_away_socks_right_outline[4];

// GLOVES
IDirect3DVertexBuffer8* g_pVB_gloves_left = NULL;
IDirect3DVertexBuffer8* g_pVB_gloves_right = NULL;

DWORD g_dwSavedStateBlock = 0L;
DWORD g_dwDrawOverlayStateBlock = 0L;

#define IHEIGHT 30.0f
#define IWIDTH IHEIGHT
#define INDX 8.0f
#define INDY 8.0f
float POSX = 0.0f;
float POSY = 0.0f;

// star coordinates.
float PI = 3.1415926f;
float R = ((float)IHEIGHT)/2.0f;
float d18 = PI/10.0f;
float d54 = d18*3.0f;
float y[] = { R*sin(d18), R, R*sin(d18), -R*sin(d54), -R*sin(d54), R*sin(d18), R*sin(d18) }; 
float x[] = { R*cos(d18), 0.0f, -R*cos(d18), -R*cos(d54), R*cos(d54) };
float x5 = x[4]*(y[1] - y[5])/(y[1] - y[4]);
float x6 = -x5;

CUSTOMVERTEX g_Vert0[] =
{
	{POSX+INDX+R + x[1], POSY+INDY+R - y[1], 0.0f, 1.0f, 0xff4488ff },
	{POSX+INDX+R + x6,   POSY+INDY+R - y[6], 0.0f, 1.0f, 0xff4488ff },
	{POSX+INDX+R + x5,   POSY+INDY+R - y[5], 0.0f, 1.0f, 0xff4488ff },
	{POSX+INDX+R + x[2], POSY+INDY+R - y[2], 0.0f, 1.0f, 0xff4488ff },
	{POSX+INDX+R + x[4], POSY+INDY+R - y[4], 0.0f, 1.0f, 0xff4488ff },
	{POSX+INDX+R + x5,   POSY+INDY+R - y[5], 0.0f, 1.0f, 0xff4488ff },
	{POSX+INDX+R + x[3], POSY+INDY+R - y[3], 0.0f, 1.0f, 0xff4488ff },
	{POSX+INDX+R + x[0], POSY+INDY+R - y[0], 0.0f, 1.0f, 0xff4488ff },
	{POSX+INDX+R + x6,   POSY+INDY+R - y[6], 0.0f, 1.0f, 0xff4488ff },
};

// 2D-kit polygons

#define X_2DKIT_LEFT 122
#define Y_2DKIT_LEFT 391
#define Y_2DKIT_LEFT_ONLINE 309
#define SOCK_SHIFT 32
#define AWAY_KIT_SHIFT 673

///////////////// SHIRT ////////////////////////////////

CUSTOMVERTEX2 g_2Dkit_shirt_left[][6] = {
    // wideback
    {
        {21.0f, 0.0f, 0.0f, 1.0f, 0xff4488ff, 41.0f/512, 0.0f/256}, //1
        {42.0f, 0.0f, 0.0f, 1.0f, 0xff4488ff, 78.0f/512, 0.0f/256}, //2
        {21.0f, 8.0f, 0.0f, 1.0f, 0xff4488ff, 41.0f/512, 12.0f/256}, //3
        {55.0f, 8.0f, 0.0f, 1.0f, 0xff4488ff, 92.0f/512, 12.0f/256}, //4
        {21.0f, 78.0f, 0.0f, 1.0f, 0xff4488ff, 41.0f/512, 144.0f/256}, //4
        {55.0f, 78.0f, 0.0f, 1.0f, 0xff4488ff, 92.0f/512, 144.0f/256}, //5
    },
    // narrowback
    {
        {21.0f, 0.0f, 0.0f, 1.0f, 0xff4488ff, 41.0f/512, 0.0f/256}, //1
        {42.0f, 0.0f, 0.0f, 1.0f, 0xff4488ff, 100.0f/512, 0.0f/256}, //2
        {21.0f, 8.0f, 0.0f, 1.0f, 0xff4488ff, 41.0f/512, 12.0f/256}, //3
        {55.0f, 8.0f, 0.0f, 1.0f, 0xff4488ff, 115.0f/512, 12.0f/256}, //4
        {21.0f, 78.0f, 0.0f, 1.0f, 0xff4488ff, 41.0f/512, 144.0f/256}, //4
        {55.0f, 78.0f, 0.0f, 1.0f, 0xff4488ff, 115.0f/512, 144.0f/256}, //5
    },
    // pes6 - with logo
    {
        {21.0f, 0.0f, 0.0f, 1.0f, 0xff4488ff, 75.0f/512, 0.0f/256}, //1
        {42.0f, 0.0f, 0.0f, 1.0f, 0xff4488ff, 130.0f/512, 0.0f/256}, //2
        {21.0f, 8.0f, 0.0f, 1.0f, 0xff4488ff, 75.0f/512, 8.0f/256}, //3
        {55.0f, 8.0f, 0.0f, 1.0f, 0xff4488ff, 157.0f/512, 8.0f/256}, //4
        {21.0f, 78.0f, 0.0f, 1.0f, 0xff4488ff, 75.0f/512, 90.0f/256}, //4
        {55.0f, 78.0f, 0.0f, 1.0f, 0xff4488ff, 157.0f/512, 90.0f/256}, //5
    },
    // pes6 template
    {
        {21.0f, 0.0f, 0.0f, 1.0f, 0xff4488ff, 75.0f/512, 0.0f/256}, //1
        {42.0f, 0.0f, 0.0f, 1.0f, 0xff4488ff, 130.0f/512, 0.0f/256}, //2
        {21.0f, 8.0f, 0.0f, 1.0f, 0xff4488ff, 75.0f/512, 8.0f/256}, //3
        {55.0f, 8.0f, 0.0f, 1.0f, 0xff4488ff, 157.0f/512, 8.0f/256}, //4
        {21.0f, 78.0f, 0.0f, 1.0f, 0xff4488ff, 75.0f/512, 90.0f/256}, //4
        {55.0f, 78.0f, 0.0f, 1.0f, 0xff4488ff, 157.0f/512, 90.0f/256}, //5
    },
};

CUSTOMVERTEX2 g_2Dkit_sleeve_left[][4] = {
    // wideback
    {
        {21.0f, 0.0f, 0.0f, 1.0f, 0xff4488ff, 336.0f/512, 153.0f/256}, //1
        {38.0f, 14.0f, 0.0f, 1.0f, 0xff4488ff, 397.0f/512, 153.0f/256}, //2
        {-3.0f, 25.0f, 0.0f, 1.0f, 0xff4488ff, 336.0f/512, 82.0f/256}, //3
        {16.0f, 43.0f, 0.0f, 1.0f, 0xff4488ff, 397.0f/512, 82.0f/256}, //4
    },
    // narrowback
    {
        {21.0f, 0.0f, 0.0f, 1.0f, 0xff4488ff, 349.0f/512, 159.0f/256}, //1
        {38.0f, 14.0f, 0.0f, 1.0f, 0xff4488ff, 349.0f/512, 256.0f/256}, //2
        {-3.0f, 25.0f, 0.0f, 1.0f, 0xff4488ff, 402.0f/512, 159.0f/256}, //3
        {16.0f, 43.0f, 0.0f, 1.0f, 0xff4488ff, 402.0f/512, 256.0f/256}, //4
    },
    // pes6 with logo
    {
        {21.0f, 0.0f, 0.0f, 1.0f, 0xff4488ff, 75.0f/512, 200.0f/256}, //1
        {38.0f, 14.0f, 0.0f, 1.0f, 0xff4488ff, 190.0f/512, 200.0f/256}, //2
        {-3.0f, 25.0f, 0.0f, 1.0f, 0xff4488ff, 75.0f/512, 160.0f/256}, //3
        {16.0f, 43.0f, 0.0f, 1.0f, 0xff4488ff, 190.0f/512, 160.0f/256}, //4
    },
    // pes6 template
    {
        {21.0f, 0.0f, 0.0f, 1.0f, 0xff4488ff, 75.0f/512, 200.0f/256}, //1
        {38.0f, 14.0f, 0.0f, 1.0f, 0xff4488ff, 190.0f/512, 200.0f/256}, //2
        {-3.0f, 25.0f, 0.0f, 1.0f, 0xff4488ff, 75.0f/512, 160.0f/256}, //3
        {16.0f, 43.0f, 0.0f, 1.0f, 0xff4488ff, 190.0f/512, 160.0f/256}, //4
    },
};

CUSTOMVERTEX2 g_2Dkit_shirt_right[][6] = {
    // wideback
    {
        {89.0f, 0.0f, 0.0f, 1.0f, 0xff4488ff, 143.0f/512, 0.0f/256}, //1
        {68.0f, 0.0f, 0.0f, 1.0f, 0xff4488ff, 106.0f/512, 0.0f/256}, //2
        {89.0f, 8.0f, 0.0f, 1.0f, 0xff4488ff, 143.0f/512, 12.0f/256}, //3
        {55.0f, 8.0f, 0.0f, 1.0f, 0xff4488ff, 92.0f/512, 12.0f/256}, //4
        {89.0f, 78.0f, 0.0f, 1.0f, 0xff4488ff, 143.0f/512, 144.0f/256}, //4
        {55.0f, 78.0f, 0.0f, 1.0f, 0xff4488ff, 92.0f/512, 144.0f/256}, //5
    },
    // narrowback
    {
        {89.0f, 0.0f, 0.0f, 1.0f, 0xff4488ff, 192.0f/512, 0.0f/256}, //1
        {68.0f, 0.0f, 0.0f, 1.0f, 0xff4488ff, 128.0f/512, 0.0f/256}, //2
        {89.0f, 8.0f, 0.0f, 1.0f, 0xff4488ff, 192.0f/512, 12.0f/256}, //3
        {55.0f, 8.0f, 0.0f, 1.0f, 0xff4488ff, 115.0f/512, 12.0f/256}, //4
        {89.0f, 78.0f, 0.0f, 1.0f, 0xff4488ff, 192.0f/512, 144.0f/256}, //4
        {55.0f, 78.0f, 0.0f, 1.0f, 0xff4488ff, 115.0f/512, 144.0f/256}, //5
    },
    // pes6 with logo
    {
        {89.0f, 0.0f, 0.0f, 1.0f, 0xff4488ff, 240.0f/512, 0.0f/256}, //1
        {68.0f, 0.0f, 0.0f, 1.0f, 0xff4488ff, 185.0f/512, 0.0f/256}, //2
        {89.0f, 8.0f, 0.0f, 1.0f, 0xff4488ff, 240.0f/512, 8.0f/256}, //3
        {55.0f, 8.0f, 0.0f, 1.0f, 0xff4488ff, 157.0f/512, 8.0f/256}, //4
        {89.0f, 78.0f, 0.0f, 1.0f, 0xff4488ff, 240.0f/512, 90.0f/256}, //4
        {55.0f, 78.0f, 0.0f, 1.0f, 0xff4488ff, 157.0f/512, 90.0f/256}, //5
    },
    // pes6 template
    {
        {89.0f, 0.0f, 0.0f, 1.0f, 0xff4488ff, 240.0f/512, 0.0f/256}, //1
        {68.0f, 0.0f, 0.0f, 1.0f, 0xff4488ff, 185.0f/512, 0.0f/256}, //2
        {89.0f, 8.0f, 0.0f, 1.0f, 0xff4488ff, 240.0f/512, 8.0f/256}, //3
        {55.0f, 8.0f, 0.0f, 1.0f, 0xff4488ff, 157.0f/512, 8.0f/256}, //4
        {89.0f, 78.0f, 0.0f, 1.0f, 0xff4488ff, 240.0f/512, 90.0f/256}, //4
        {55.0f, 78.0f, 0.0f, 1.0f, 0xff4488ff, 157.0f/512, 90.0f/256}, //5
    },
};

CUSTOMVERTEX2 g_2Dkit_sleeve_right[][4] = {
    // wideback
    {
        {89.0f, 0.0f, 0.0f, 1.0f, 0xff4488ff, 448.0f/512, 153.0f/256}, //1
        {72.0f, 14.0f, 0.0f, 1.0f, 0xff4488ff, 512.0f/512, 153.0f/256}, //2
        {113.0f, 25.0f, 0.0f, 1.0f, 0xff4488ff, 448.0f/512, 82.0f/256}, //3
        {94.0f, 43.0f, 0.0f, 1.0f, 0xff4488ff, 512.0f/512, 82.0f/256}, //4
    },
    // narrowback
    {
        {89.0f, 0.0f, 0.0f, 1.0f, 0xff4488ff, 459.0f/512, 159.0f/256}, //1
        {72.0f, 14.0f, 0.0f, 1.0f, 0xff4488ff, 459.0f/512, 256.0f/256}, //2
        {113.0f, 25.0f, 0.0f, 1.0f, 0xff4488ff, 512.0f/512, 159.0f/256}, //3
        {94.0f, 43.0f, 0.0f, 1.0f, 0xff4488ff, 512.0f/512, 256.0f/256}, //4
    },
    // pes6 with logo
    {
        {89.0f, 0.0f, 0.0f, 1.0f, 0xff4488ff, 268.0f/512, 200.0f/256}, //1
        {72.0f, 14.0f, 0.0f, 1.0f, 0xff4488ff, 385.0f/512, 200.0f/256}, //2
        {113.0f, 25.0f, 0.0f, 1.0f, 0xff4488ff, 268.0f/512, 160.0f/256}, //3
        {94.0f, 43.0f, 0.0f, 1.0f, 0xff4488ff, 385.0f/512, 160.0f/256}, //4
    },
    // pes6 template
    {
        {89.0f, 0.0f, 0.0f, 1.0f, 0xff4488ff, 268.0f/512, 200.0f/256}, //1
        {72.0f, 14.0f, 0.0f, 1.0f, 0xff4488ff, 385.0f/512, 200.0f/256}, //2
        {113.0f, 25.0f, 0.0f, 1.0f, 0xff4488ff, 268.0f/512, 160.0f/256}, //3
        {94.0f, 43.0f, 0.0f, 1.0f, 0xff4488ff, 385.0f/512, 160.0f/256}, //4
    },
};

CUSTOMVERTEX g_2Dkit_shirt_outline[][8] = {
    // wideback
    {
        {21.0f, 0.0f, 0.0f, 1.0f, 0xff000000}, //1
        {42.0f, 0.0f, 0.0f, 1.0f, 0xff000000}, //2
        {55.0f, 8.0f, 0.0f, 1.0f, 0xff000000}, //3
        {68.0f, 0.0f, 0.0f, 1.0f, 0xff000000}, //4
        {89.0f, 0.0f, 0.0f, 1.0f, 0xff000000}, //5
        {89.0f, 78.0f, 0.0f, 1.0f, 0xff000000}, //6
        {21.0f, 78.0f, 0.0f, 1.0f, 0xff000000}, //7
        {21.0f, 0.0f, 0.0f, 1.0f, 0xff000000}, //8
    },
    // narrowback
    {
        {21.0f, 0.0f, 0.0f, 1.0f, 0xff000000}, //1
        {42.0f, 0.0f, 0.0f, 1.0f, 0xff000000}, //2
        {55.0f, 8.0f, 0.0f, 1.0f, 0xff000000}, //3
        {68.0f, 0.0f, 0.0f, 1.0f, 0xff000000}, //4
        {89.0f, 0.0f, 0.0f, 1.0f, 0xff000000}, //5
        {89.0f, 78.0f, 0.0f, 1.0f, 0xff000000}, //6
        {21.0f, 78.0f, 0.0f, 1.0f, 0xff000000}, //7
        {21.0f, 0.0f, 0.0f, 1.0f, 0xff000000}, //8
    },
    // pes6 with logo
    {
        {21.0f, 0.0f, 0.0f, 1.0f, 0xff000000}, //1
        {42.0f, 0.0f, 0.0f, 1.0f, 0xff000000}, //2
        {55.0f, 8.0f, 0.0f, 1.0f, 0xff000000}, //3
        {68.0f, 0.0f, 0.0f, 1.0f, 0xff000000}, //4
        {89.0f, 0.0f, 0.0f, 1.0f, 0xff000000}, //5
        {89.0f, 78.0f, 0.0f, 1.0f, 0xff000000}, //6
        {21.0f, 78.0f, 0.0f, 1.0f, 0xff000000}, //7
        {21.0f, 0.0f, 0.0f, 1.0f, 0xff000000}, //8
    },
    // pes6 template
    {
        {21.0f, 0.0f, 0.0f, 1.0f, 0xff000000}, //1
        {42.0f, 0.0f, 0.0f, 1.0f, 0xff000000}, //2
        {55.0f, 8.0f, 0.0f, 1.0f, 0xff000000}, //3
        {68.0f, 0.0f, 0.0f, 1.0f, 0xff000000}, //4
        {89.0f, 0.0f, 0.0f, 1.0f, 0xff000000}, //5
        {89.0f, 78.0f, 0.0f, 1.0f, 0xff000000}, //6
        {21.0f, 78.0f, 0.0f, 1.0f, 0xff000000}, //7
        {21.0f, 0.0f, 0.0f, 1.0f, 0xff000000}, //8
    },
};

CUSTOMVERTEX g_2Dkit_sleeve_left_outline[][5] = {
    // wideback
    {
        {21.0f, 0.0f, 0.0f, 1.0f, 0xff000000}, //1
        {38.0f, 14.0f, 0.0f, 1.0f, 0xff000000}, //2
        {16.0f, 43.0f, 0.0f, 1.0f, 0xff000000}, //4
        {-3.0f, 25.0f, 0.0f, 1.0f, 0xff000000}, //3
        {21.0f, 0.0f, 0.0f, 1.0f, 0xff000000}, //1
    },
    // narrowback
    {
        {21.0f, 0.0f, 0.0f, 1.0f, 0xff000000}, //1
        {38.0f, 14.0f, 0.0f, 1.0f, 0xff000000}, //2
        {16.0f, 43.0f, 0.0f, 1.0f, 0xff000000}, //4
        {-3.0f, 25.0f, 0.0f, 1.0f, 0xff000000}, //3
        {21.0f, 0.0f, 0.0f, 1.0f, 0xff000000}, //1
    },
    // pes6 with logo
    {
        {21.0f, 0.0f, 0.0f, 1.0f, 0xff000000}, //1
        {38.0f, 14.0f, 0.0f, 1.0f, 0xff000000}, //2
        {16.0f, 43.0f, 0.0f, 1.0f, 0xff000000}, //4
        {-3.0f, 25.0f, 0.0f, 1.0f, 0xff000000}, //3
        {21.0f, 0.0f, 0.0f, 1.0f, 0xff000000}, //1
    },
    // pes6 template
    {
        {21.0f, 0.0f, 0.0f, 1.0f, 0xff000000}, //1
        {38.0f, 14.0f, 0.0f, 1.0f, 0xff000000}, //2
        {16.0f, 43.0f, 0.0f, 1.0f, 0xff000000}, //4
        {-3.0f, 25.0f, 0.0f, 1.0f, 0xff000000}, //3
        {21.0f, 0.0f, 0.0f, 1.0f, 0xff000000}, //1
    },
};

CUSTOMVERTEX g_2Dkit_sleeve_right_outline[][5] = {
    // wideback
    {
        {89.0f, 0.0f, 0.0f, 1.0f, 0xff000000}, //1
        {72.0f, 14.0f, 0.0f, 1.0f, 0xff000000}, //2
        {94.0f, 43.0f, 0.0f, 1.0f, 0xff000000}, //4
        {113.0f, 25.0f, 0.0f, 1.0f, 0xff000000}, //3
        {89.0f, 0.0f, 0.0f, 1.0f, 0xff000000}, //1
    },
    // narrowback
    {
        {89.0f, 0.0f, 0.0f, 1.0f, 0xff000000}, //1
        {72.0f, 14.0f, 0.0f, 1.0f, 0xff000000}, //2
        {94.0f, 43.0f, 0.0f, 1.0f, 0xff000000}, //4
        {113.0f, 25.0f, 0.0f, 1.0f, 0xff000000}, //3
        {89.0f, 0.0f, 0.0f, 1.0f, 0xff000000}, //1
    },
    // pes6 with logo
    {
        {89.0f, 0.0f, 0.0f, 1.0f, 0xff000000}, //1
        {72.0f, 14.0f, 0.0f, 1.0f, 0xff000000}, //2
        {94.0f, 43.0f, 0.0f, 1.0f, 0xff000000}, //4
        {113.0f, 25.0f, 0.0f, 1.0f, 0xff000000}, //3
        {89.0f, 0.0f, 0.0f, 1.0f, 0xff000000}, //1
    },
    // pes6 template
    {
        {89.0f, 0.0f, 0.0f, 1.0f, 0xff000000}, //1
        {72.0f, 14.0f, 0.0f, 1.0f, 0xff000000}, //2
        {94.0f, 43.0f, 0.0f, 1.0f, 0xff000000}, //4
        {113.0f, 25.0f, 0.0f, 1.0f, 0xff000000}, //3
        {89.0f, 0.0f, 0.0f, 1.0f, 0xff000000}, //1
    },
};

///////////////// SHORTS //////////////////////////////////

CUSTOMVERTEX2 g_2Dkit_shorts[][4] = {
    // wideback
    {
        {40.0f, 84.0f, 0.0f, 1.0f, 0xff4488ff, 120.0f/512, 144.0f/256}, //1
        {84.0f, 84.0f, 0.0f, 1.0f, 0xff4488ff, 172.0f/512, 144.0f/256}, //2
        {40.0f, 142.0f, 0.0f, 1.0f, 0xff4488ff, 120.0f/512, 256.0f/256}, //3
        {84.0f, 142.0f, 0.0f, 1.0f, 0xff4488ff, 172.0f/512, 256.0f/256}, //4
    },
    // narrowback
    {
        {40.0f, 84.0f, 0.0f, 1.0f, 0xff4488ff, 120.0f/512, 144.0f/256}, //1
        {84.0f, 84.0f, 0.0f, 1.0f, 0xff4488ff, 172.0f/512, 144.0f/256}, //2
        {40.0f, 142.0f, 0.0f, 1.0f, 0xff4488ff, 120.0f/512, 256.0f/256}, //3
        {84.0f, 142.0f, 0.0f, 1.0f, 0xff4488ff, 172.0f/512, 256.0f/256}, //4
    },
    // pes6 with logo
    {
        {40.0f, 84.0f, 0.0f, 1.0f, 0xff4488ff, 224.0f/512, 90.0f/256}, //1
        {84.0f, 84.0f, 0.0f, 1.0f, 0xff4488ff, 332.0f/512, 90.0f/256}, //2
        {40.0f, 142.0f, 0.0f, 1.0f, 0xff4488ff, 224.0f/512, 160.0f/256}, //3
        {84.0f, 142.0f, 0.0f, 1.0f, 0xff4488ff, 332.0f/512, 160.0f/256}, //4
    },
    // pes6 template
    {
        {40.0f, 84.0f, 0.0f, 1.0f, 0xff4488ff, 224.0f/512, 90.0f/256}, //1
        {84.0f, 84.0f, 0.0f, 1.0f, 0xff4488ff, 332.0f/512, 90.0f/256}, //2
        {40.0f, 142.0f, 0.0f, 1.0f, 0xff4488ff, 224.0f/512, 160.0f/256}, //3
        {84.0f, 142.0f, 0.0f, 1.0f, 0xff4488ff, 332.0f/512, 160.0f/256}, //4
    },
};
CUSTOMVERTEX2 g_2Dkit_shorts_2[][4] = {
    // wideback
    {
        {27.0f, 84.0f, 0.0f, 1.0f, 0xff4488ff, 33.0f/512, 144.0f/256}, //1
        {40.0f, 84.0f, 0.0f, 1.0f, 0xff4488ff, 70.0f/512, 144.0f/256}, //2
        {20.0f, 140.0f, 0.0f, 1.0f, 0xff4488ff, 33.0f/512, 256.0f/256}, //3
        {40.0f, 142.0f, 0.0f, 1.0f, 0xff4488ff, 87.0f/512, 256.0f/256}, //4
    },
    // narrowback
    {
        {27.0f, 84.0f, 0.0f, 1.0f, 0xff4488ff, 33.0f/512, 144.0f/256}, //1
        {40.0f, 84.0f, 0.0f, 1.0f, 0xff4488ff, 70.0f/512, 144.0f/256}, //2
        {20.0f, 140.0f, 0.0f, 1.0f, 0xff4488ff, 33.0f/512, 256.0f/256}, //3
        {40.0f, 142.0f, 0.0f, 1.0f, 0xff4488ff, 87.0f/512, 256.0f/256}, //4
    },
    // pes6 with logo
    {
        {27.0f, 84.0f, 0.0f, 1.0f, 0xff4488ff, 68.0f/512, 90.0f/256}, //1
        {40.0f, 84.0f, 0.0f, 1.0f, 0xff4488ff, 116.0f/512, 90.0f/256}, //2
        {20.0f, 140.0f, 0.0f, 1.0f, 0xff4488ff, 68.0f/512, 160.0f/256}, //3
        {40.0f, 142.0f, 0.0f, 1.0f, 0xff4488ff, 157.0f/512, 160.0f/256}, //4
    },
    // pes6 template
    {
        {27.0f, 84.0f, 0.0f, 1.0f, 0xff4488ff, 68.0f/512, 90.0f/256}, //1
        {40.0f, 84.0f, 0.0f, 1.0f, 0xff4488ff, 116.0f/512, 90.0f/256}, //2
        {20.0f, 140.0f, 0.0f, 1.0f, 0xff4488ff, 68.0f/512, 160.0f/256}, //3
        {40.0f, 142.0f, 0.0f, 1.0f, 0xff4488ff, 157.0f/512, 160.0f/256}, //4
    },
};
CUSTOMVERTEX g_2Dkit_shorts_outline[][5] = {
    // wideback
    {
        {40.0f, 84.0f, 0.0f, 1.0f, 0xff000000}, //1
        {84.0f, 84.0f, 0.0f, 1.0f, 0xff000000}, //2
        {84.0f, 142.0f, 0.0f, 1.0f, 0xff000000}, //4
        {40.0f, 142.0f, 0.0f, 1.0f, 0xff000000}, //3
        {40.0f, 84.0f, 0.0f, 1.0f, 0xff000000}, //1
    },
    // narrowback
    {
        {40.0f, 84.0f, 0.0f, 1.0f, 0xff000000}, //1
        {84.0f, 84.0f, 0.0f, 1.0f, 0xff000000}, //2
        {84.0f, 142.0f, 0.0f, 1.0f, 0xff000000}, //4
        {40.0f, 142.0f, 0.0f, 1.0f, 0xff000000}, //3
        {40.0f, 84.0f, 0.0f, 1.0f, 0xff000000}, //1
    },
    // pes6 with logo
    {
        {40.0f, 84.0f, 0.0f, 1.0f, 0xff000000}, //1
        {84.0f, 84.0f, 0.0f, 1.0f, 0xff000000}, //2
        {84.0f, 142.0f, 0.0f, 1.0f, 0xff000000}, //4
        {40.0f, 142.0f, 0.0f, 1.0f, 0xff000000}, //3
        {40.0f, 84.0f, 0.0f, 1.0f, 0xff000000}, //1
    },
    // pes6 template
    {
        {40.0f, 84.0f, 0.0f, 1.0f, 0xff000000}, //1
        {84.0f, 84.0f, 0.0f, 1.0f, 0xff000000}, //2
        {84.0f, 142.0f, 0.0f, 1.0f, 0xff000000}, //4
        {40.0f, 142.0f, 0.0f, 1.0f, 0xff000000}, //3
        {40.0f, 84.0f, 0.0f, 1.0f, 0xff000000}, //1
    },
};
CUSTOMVERTEX g_2Dkit_shorts_2_outline[][5] = {
    // wideback
    {
        {27.0f, 84.0f, 0.0f, 1.0f, 0xff000000}, //1
        {40.0f, 84.0f, 0.0f, 1.0f, 0xff000000}, //2
        {40.0f, 142.0f, 0.0f, 1.0f, 0xff000000}, //4
        {20.0f, 140.0f, 0.0f, 1.0f, 0xff000000}, //3
        {27.0f, 84.0f, 0.0f, 1.0f, 0xff000000}, //1
    },
    // narrowback
    {
        {27.0f, 84.0f, 0.0f, 1.0f, 0xff000000}, //1
        {40.0f, 84.0f, 0.0f, 1.0f, 0xff000000}, //2
        {40.0f, 142.0f, 0.0f, 1.0f, 0xff000000}, //4
        {20.0f, 140.0f, 0.0f, 1.0f, 0xff000000}, //3
        {27.0f, 84.0f, 0.0f, 1.0f, 0xff000000}, //1
    },
    // pes6 with logo
    {
        {27.0f, 84.0f, 0.0f, 1.0f, 0xff000000}, //1
        {40.0f, 84.0f, 0.0f, 1.0f, 0xff000000}, //2
        {40.0f, 142.0f, 0.0f, 1.0f, 0xff000000}, //4
        {20.0f, 140.0f, 0.0f, 1.0f, 0xff000000}, //3
        {27.0f, 84.0f, 0.0f, 1.0f, 0xff000000}, //1
    },
    // pes6 template
    {
        {27.0f, 84.0f, 0.0f, 1.0f, 0xff000000}, //1
        {40.0f, 84.0f, 0.0f, 1.0f, 0xff000000}, //2
        {40.0f, 142.0f, 0.0f, 1.0f, 0xff000000}, //4
        {20.0f, 140.0f, 0.0f, 1.0f, 0xff000000}, //3
        {27.0f, 84.0f, 0.0f, 1.0f, 0xff000000}, //1
    },
};

///////////////// SOCKS //////////////////////////////////

CUSTOMVERTEX2 g_2Dkit_socks_shin_left[][4] = {
    // wideback
    {
        {26.0f, 156.0f, 0.0f, 1.0f, 0xff000000, 395.0f/512, 0.0f/256}, //1
        {51.0f, 156.0f, 0.0f, 1.0f, 0xff000000, 433.0f/512, 0.0f/256}, //2
        {26.0f, 206.0f, 0.0f, 1.0f, 0xff000000, 395.0f/512, 66.0f/256}, //3
        {51.0f, 201.0f, 0.0f, 1.0f, 0xff000000, 433.0f/512, 61.0f/256}, //4
    },
    // narrowback
    {
        {26.0f, 156.0f, 0.0f, 1.0f, 0xff000000, 395.0f/512, 0.0f/256}, //1
        {51.0f, 156.0f, 0.0f, 1.0f, 0xff000000, 433.0f/512, 0.0f/256}, //2
        {26.0f, 206.0f, 0.0f, 1.0f, 0xff000000, 395.0f/512, 66.0f/256}, //3
        {51.0f, 201.0f, 0.0f, 1.0f, 0xff000000, 433.0f/512, 61.0f/256}, //4
    },
    // pes6 with logo
    {
        {26.0f, 156.0f, 0.0f, 1.0f, 0xff000000, 454.0f/512, 215.0f/256}, //1
        {51.0f, 156.0f, 0.0f, 1.0f, 0xff000000, 454.0f/512, 239.0f/256}, //2
        {26.0f, 206.0f, 0.0f, 1.0f, 0xff000000, 402.0f/512, 215.0f/256}, //3
        {51.0f, 201.0f, 0.0f, 1.0f, 0xff000000, 410.0f/512, 239.0f/256}, //4
    },
    // pes6 template
    {
        {26.0f, 156.0f, 0.0f, 1.0f, 0xff000000, 512.0f/512, 215.0f/256}, //1
        {51.0f, 156.0f, 0.0f, 1.0f, 0xff000000, 512.0f/512, 239.0f/256}, //2
        {26.0f, 206.0f, 0.0f, 1.0f, 0xff000000, 412.0f/512, 215.0f/256}, //3
        {51.0f, 201.0f, 0.0f, 1.0f, 0xff000000, 420.0f/512, 239.0f/256}, //4
    },
};
CUSTOMVERTEX2 g_2Dkit_socks_foot_left[][7] = {
    // wideback
    {
        {26.0f, 206.0f, 0.0f, 1.0f, 0xff000000, 395.0f/512, 66.0f/256}, //1
        {50.0f, 201.0f, 0.0f, 1.0f, 0xff000000, 433.0f/512, 61.0f/256}, //2
        {52.0f, 201.0f, 0.0f, 1.0f, 0xff000000, 435.0f/512, 61.0f/256}, //3
        {52.0f, 208.0f, 0.0f, 1.0f, 0xff000000, 435.0f/512, 65.0f/256}, //4
        {33.0f, 225.0f, 0.0f, 1.0f, 0xff000000, 380.0f/512, 75.0f/256}, //5
        {12.0f, 225.0f, 0.0f, 1.0f, 0xff000000, 375.0f/512, 75.0f/256}, //6
        {8.0f, 221.0f, 0.0f, 1.0f, 0xff000000, 370.0f/512, 70.0f/256}, //7
    },
    // narrowback
    {
        {26.0f, 206.0f, 0.0f, 1.0f, 0xff000000, 395.0f/512, 66.0f/256}, //1
        {50.0f, 201.0f, 0.0f, 1.0f, 0xff000000, 433.0f/512, 61.0f/256}, //2
        {52.0f, 201.0f, 0.0f, 1.0f, 0xff000000, 435.0f/512, 61.0f/256}, //3
        {52.0f, 208.0f, 0.0f, 1.0f, 0xff000000, 435.0f/512, 65.0f/256}, //4
        {33.0f, 225.0f, 0.0f, 1.0f, 0xff000000, 380.0f/512, 75.0f/256}, //5
        {12.0f, 225.0f, 0.0f, 1.0f, 0xff000000, 375.0f/512, 75.0f/256}, //6
        {8.0f, 221.0f, 0.0f, 1.0f, 0xff000000, 370.0f/512, 70.0f/256}, //7
    },
    // pes6 with logo
    {
        {26.0f, 206.0f, 0.0f, 1.0f, 0xff000000, 402.0f/512, 215.0f/256}, //1
        {50.0f, 201.0f, 0.0f, 1.0f, 0xff000000, 410.0f/512, 239.0f/256}, //2
        {52.0f, 201.0f, 0.0f, 1.0f, 0xff000000, 410.0f/512, 241.0f/256}, //3
        {52.0f, 208.0f, 0.0f, 1.0f, 0xff000000, 403.0f/512, 241.0f/256}, //4
        {33.0f, 225.0f, 0.0f, 1.0f, 0xff000000, 392.0f/512, 219.0f/256}, //5
        {12.0f, 225.0f, 0.0f, 1.0f, 0xff000000, 392.0f/512, 215.0f/256}, //6
        {8.0f, 221.0f, 0.0f, 1.0f, 0xff000000, 400.0f/512, 209.0f/256}, //7
    },
    // pes6 template
    {
        {26.0f, 206.0f, 0.0f, 1.0f, 0xff000000, 412.0f/512, 215.0f/256}, //1
        {50.0f, 201.0f, 0.0f, 1.0f, 0xff000000, 420.0f/512, 239.0f/256}, //2
        {52.0f, 201.0f, 0.0f, 1.0f, 0xff000000, 420.0f/512, 241.0f/256}, //3
        {52.0f, 208.0f, 0.0f, 1.0f, 0xff000000, 413.0f/512, 241.0f/256}, //4
        {33.0f, 225.0f, 0.0f, 1.0f, 0xff000000, 402.0f/512, 219.0f/256}, //5
        {12.0f, 225.0f, 0.0f, 1.0f, 0xff000000, 402.0f/512, 215.0f/256}, //6
        {8.0f, 221.0f, 0.0f, 1.0f, 0xff000000, 410.0f/512, 209.0f/256}, //7
    },
};
CUSTOMVERTEX2 g_2Dkit_socks_shin_right[][4] = {
    // wideback
    {
        {26.0f, 156.0f, 0.0f, 1.0f, 0xff000000, 395.0f/512, 0.0f/256}, //1
        {51.0f, 156.0f, 0.0f, 1.0f, 0xff000000, 433.0f/512, 0.0f/256}, //2
        {26.0f, 206.0f, 0.0f, 1.0f, 0xff000000, 395.0f/512, 66.0f/256}, //3
        {51.0f, 201.0f, 0.0f, 1.0f, 0xff000000, 433.0f/512, 61.0f/256}, //4
    },
    // narrowback
    {
        {26.0f, 156.0f, 0.0f, 1.0f, 0xff000000, 395.0f/512, 0.0f/256}, //1
        {51.0f, 156.0f, 0.0f, 1.0f, 0xff000000, 433.0f/512, 0.0f/256}, //2
        {26.0f, 206.0f, 0.0f, 1.0f, 0xff000000, 395.0f/512, 66.0f/256}, //3
        {51.0f, 201.0f, 0.0f, 1.0f, 0xff000000, 433.0f/512, 61.0f/256}, //4
    },
    // pes6 with logo
    {
        {26.0f, 156.0f, 0.0f, 1.0f, 0xff000000, 454.0f/512, 215.0f/256}, //1
        {51.0f, 156.0f, 0.0f, 1.0f, 0xff000000, 454.0f/512, 239.0f/256}, //2
        {26.0f, 206.0f, 0.0f, 1.0f, 0xff000000, 402.0f/512, 215.0f/256}, //3
        {51.0f, 201.0f, 0.0f, 1.0f, 0xff000000, 410.0f/512, 239.0f/256}, //4
    },
    // pes6 template
    {
        {26.0f, 156.0f, 0.0f, 1.0f, 0xff000000, 512.0f/512, 215.0f/256}, //1
        {51.0f, 156.0f, 0.0f, 1.0f, 0xff000000, 512.0f/512, 239.0f/256}, //2
        {26.0f, 206.0f, 0.0f, 1.0f, 0xff000000, 402.0f/512, 215.0f/256}, //3
        {51.0f, 201.0f, 0.0f, 1.0f, 0xff000000, 420.0f/512, 239.0f/256}, //4
    },
};
CUSTOMVERTEX2 g_2Dkit_socks_foot_right[][7] = {
    // wideback
    {
        {26.0f, 206.0f, 0.0f, 1.0f, 0xff000000, 395.0f/512, 66.0f/256}, //1
        {50.0f, 201.0f, 0.0f, 1.0f, 0xff000000, 433.0f/512, 61.0f/256}, //2
        {52.0f, 201.0f, 0.0f, 1.0f, 0xff000000, 435.0f/512, 61.0f/256}, //3
        {52.0f, 208.0f, 0.0f, 1.0f, 0xff000000, 435.0f/512, 65.0f/256}, //4
        {33.0f, 225.0f, 0.0f, 1.0f, 0xff000000, 380.0f/512, 75.0f/256}, //5
        {12.0f, 225.0f, 0.0f, 1.0f, 0xff000000, 375.0f/512, 75.0f/256}, //6
        {8.0f, 221.0f, 0.0f, 1.0f, 0xff000000, 370.0f/512, 70.0f/256}, //7
    },
    // narrowback
    {
        {26.0f, 206.0f, 0.0f, 1.0f, 0xff000000, 395.0f/512, 66.0f/256}, //1
        {50.0f, 201.0f, 0.0f, 1.0f, 0xff000000, 433.0f/512, 61.0f/256}, //2
        {52.0f, 201.0f, 0.0f, 1.0f, 0xff000000, 435.0f/512, 61.0f/256}, //3
        {52.0f, 208.0f, 0.0f, 1.0f, 0xff000000, 435.0f/512, 65.0f/256}, //4
        {33.0f, 225.0f, 0.0f, 1.0f, 0xff000000, 380.0f/512, 75.0f/256}, //5
        {12.0f, 225.0f, 0.0f, 1.0f, 0xff000000, 375.0f/512, 75.0f/256}, //6
        {8.0f, 221.0f, 0.0f, 1.0f, 0xff000000, 370.0f/512, 70.0f/256}, //7
    },
    // pes6 with logo
    {
        {26.0f, 206.0f, 0.0f, 1.0f, 0xff000000, 402.0f/512, 215.0f/256}, //1
        {50.0f, 201.0f, 0.0f, 1.0f, 0xff000000, 410.0f/512, 239.0f/256}, //2
        {52.0f, 201.0f, 0.0f, 1.0f, 0xff000000, 410.0f/512, 241.0f/256}, //3
        {52.0f, 208.0f, 0.0f, 1.0f, 0xff000000, 403.0f/512, 241.0f/256}, //4
        {33.0f, 225.0f, 0.0f, 1.0f, 0xff000000, 392.0f/512, 219.0f/256}, //5
        {12.0f, 225.0f, 0.0f, 1.0f, 0xff000000, 392.0f/512, 215.0f/256}, //6
        {8.0f, 221.0f, 0.0f, 1.0f, 0xff000000, 400.0f/512, 209.0f/256}, //7
    },
    // pes6 template
    {
        {26.0f, 206.0f, 0.0f, 1.0f, 0xff000000, 412.0f/512, 215.0f/256}, //1
        {50.0f, 201.0f, 0.0f, 1.0f, 0xff000000, 420.0f/512, 239.0f/256}, //2
        {52.0f, 201.0f, 0.0f, 1.0f, 0xff000000, 420.0f/512, 241.0f/256}, //3
        {52.0f, 208.0f, 0.0f, 1.0f, 0xff000000, 413.0f/512, 241.0f/256}, //4
        {33.0f, 225.0f, 0.0f, 1.0f, 0xff000000, 402.0f/512, 219.0f/256}, //5
        {12.0f, 225.0f, 0.0f, 1.0f, 0xff000000, 402.0f/512, 215.0f/256}, //6
        {8.0f, 221.0f, 0.0f, 1.0f, 0xff000000, 410.0f/512, 209.0f/256}, //7
    },
};
CUSTOMVERTEX g_2Dkit_socks_left_outline[][10] = {
    // wideback
    {
        {26.0f, 156.0f, 0.0f, 1.0f, 0xff000000}, //1
        {50.0f, 156.0f, 0.0f, 1.0f, 0xff000000}, //2
        {50.0f, 201.0f, 0.0f, 1.0f, 0xff000000}, //2
        {52.0f, 201.0f, 0.0f, 1.0f, 0xff000000}, //3
        {52.0f, 208.0f, 0.0f, 1.0f, 0xff000000}, //4
        {33.0f, 225.0f, 0.0f, 1.0f, 0xff000000}, //5
        {12.0f, 225.0f, 0.0f, 1.0f, 0xff000000}, //6
        {8.0f, 221.0f, 0.0f, 1.0f, 0xff000000}, //7
        {26.0f, 206.0f, 0.0f, 1.0f, 0xff000000}, //1
        {26.0f, 156.0f, 0.0f, 1.0f, 0xff000000}, //1
    },
    // narrowback
    {
        {26.0f, 156.0f, 0.0f, 1.0f, 0xff000000}, //1
        {50.0f, 156.0f, 0.0f, 1.0f, 0xff000000}, //2
        {50.0f, 201.0f, 0.0f, 1.0f, 0xff000000}, //2
        {52.0f, 201.0f, 0.0f, 1.0f, 0xff000000}, //3
        {52.0f, 208.0f, 0.0f, 1.0f, 0xff000000}, //4
        {33.0f, 225.0f, 0.0f, 1.0f, 0xff000000}, //5
        {12.0f, 225.0f, 0.0f, 1.0f, 0xff000000}, //6
        {8.0f, 221.0f, 0.0f, 1.0f, 0xff000000}, //7
        {26.0f, 206.0f, 0.0f, 1.0f, 0xff000000}, //1
        {26.0f, 156.0f, 0.0f, 1.0f, 0xff000000}, //1
    },
    // wideback with logo
    {
        {26.0f, 156.0f, 0.0f, 1.0f, 0xff000000}, //1
        {50.0f, 156.0f, 0.0f, 1.0f, 0xff000000}, //2
        {50.0f, 201.0f, 0.0f, 1.0f, 0xff000000}, //2
        {52.0f, 201.0f, 0.0f, 1.0f, 0xff000000}, //3
        {52.0f, 208.0f, 0.0f, 1.0f, 0xff000000}, //4
        {33.0f, 225.0f, 0.0f, 1.0f, 0xff000000}, //5
        {12.0f, 225.0f, 0.0f, 1.0f, 0xff000000}, //6
        {8.0f, 221.0f, 0.0f, 1.0f, 0xff000000}, //7
        {26.0f, 206.0f, 0.0f, 1.0f, 0xff000000}, //1
        {26.0f, 156.0f, 0.0f, 1.0f, 0xff000000}, //1
    },
    // pes6 template
    {
        {26.0f, 156.0f, 0.0f, 1.0f, 0xff000000}, //1
        {50.0f, 156.0f, 0.0f, 1.0f, 0xff000000}, //2
        {50.0f, 201.0f, 0.0f, 1.0f, 0xff000000}, //2
        {52.0f, 201.0f, 0.0f, 1.0f, 0xff000000}, //3
        {52.0f, 208.0f, 0.0f, 1.0f, 0xff000000}, //4
        {33.0f, 225.0f, 0.0f, 1.0f, 0xff000000}, //5
        {12.0f, 225.0f, 0.0f, 1.0f, 0xff000000}, //6
        {8.0f, 221.0f, 0.0f, 1.0f, 0xff000000}, //7
        {26.0f, 206.0f, 0.0f, 1.0f, 0xff000000}, //1
        {26.0f, 156.0f, 0.0f, 1.0f, 0xff000000}, //1
    },
};
CUSTOMVERTEX g_2Dkit_socks_right_outline[][10] = {
    // wideback
    {
        {26.0f, 156.0f, 0.0f, 1.0f, 0xff000000}, //1
        {50.0f, 156.0f, 0.0f, 1.0f, 0xff000000}, //2
        {50.0f, 201.0f, 0.0f, 1.0f, 0xff000000}, //2
        {52.0f, 201.0f, 0.0f, 1.0f, 0xff000000}, //3
        {52.0f, 208.0f, 0.0f, 1.0f, 0xff000000}, //4
        {33.0f, 225.0f, 0.0f, 1.0f, 0xff000000}, //5
        {12.0f, 225.0f, 0.0f, 1.0f, 0xff000000}, //6
        {8.0f, 221.0f, 0.0f, 1.0f, 0xff000000}, //7
        {26.0f, 206.0f, 0.0f, 1.0f, 0xff000000}, //1
        {26.0f, 156.0f, 0.0f, 1.0f, 0xff000000}, //1
    },
    // narrowback
    {
        {26.0f, 156.0f, 0.0f, 1.0f, 0xff000000}, //1
        {50.0f, 156.0f, 0.0f, 1.0f, 0xff000000}, //2
        {50.0f, 201.0f, 0.0f, 1.0f, 0xff000000}, //2
        {52.0f, 201.0f, 0.0f, 1.0f, 0xff000000}, //3
        {52.0f, 208.0f, 0.0f, 1.0f, 0xff000000}, //4
        {33.0f, 225.0f, 0.0f, 1.0f, 0xff000000}, //5
        {12.0f, 225.0f, 0.0f, 1.0f, 0xff000000}, //6
        {8.0f, 221.0f, 0.0f, 1.0f, 0xff000000}, //7
        {26.0f, 206.0f, 0.0f, 1.0f, 0xff000000}, //1
        {26.0f, 156.0f, 0.0f, 1.0f, 0xff000000}, //1
    },
    // wideback with logo
    {
        {26.0f, 156.0f, 0.0f, 1.0f, 0xff000000}, //1
        {50.0f, 156.0f, 0.0f, 1.0f, 0xff000000}, //2
        {50.0f, 201.0f, 0.0f, 1.0f, 0xff000000}, //2
        {52.0f, 201.0f, 0.0f, 1.0f, 0xff000000}, //3
        {52.0f, 208.0f, 0.0f, 1.0f, 0xff000000}, //4
        {33.0f, 225.0f, 0.0f, 1.0f, 0xff000000}, //5
        {12.0f, 225.0f, 0.0f, 1.0f, 0xff000000}, //6
        {8.0f, 221.0f, 0.0f, 1.0f, 0xff000000}, //7
        {26.0f, 206.0f, 0.0f, 1.0f, 0xff000000}, //1
        {26.0f, 156.0f, 0.0f, 1.0f, 0xff000000}, //1
    },
    // pes6 template
    {
        {26.0f, 156.0f, 0.0f, 1.0f, 0xff000000}, //1
        {50.0f, 156.0f, 0.0f, 1.0f, 0xff000000}, //2
        {50.0f, 201.0f, 0.0f, 1.0f, 0xff000000}, //2
        {52.0f, 201.0f, 0.0f, 1.0f, 0xff000000}, //3
        {52.0f, 208.0f, 0.0f, 1.0f, 0xff000000}, //4
        {33.0f, 225.0f, 0.0f, 1.0f, 0xff000000}, //5
        {12.0f, 225.0f, 0.0f, 1.0f, 0xff000000}, //6
        {8.0f, 221.0f, 0.0f, 1.0f, 0xff000000}, //7
        {26.0f, 206.0f, 0.0f, 1.0f, 0xff000000}, //1
        {26.0f, 156.0f, 0.0f, 1.0f, 0xff000000}, //1
    },
};

////////////////// GLOVES //////////////////////////////////////

CUSTOMVERTEX2 g_gloves_left[] = {
	{0.0f, 0.0f, 0.0f, 1.0f, 0xff4488ff, 0.0f, 0.0f}, //1
	{0.0f, 64.0f, 0.0f, 1.0f, 0xff4488ff, 0.0f, 1.0f}, //2
	{64.0f, 0.0f, 0.0f, 1.0f, 0xff4488ff, 1.0f, 0.0f}, //3
	{64.0f, 64.0f, 0.0f, 1.0f, 0xff4488ff, 1.0f, 1.0f}, //4
};

/////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////

////////////////////////////////

#define D3DFVF_CUSTOMVERTEX (D3DFVF_XYZRHW|D3DFVF_DIFFUSE)
#define D3DFVF_CUSTOMVERTEX2 (D3DFVF_XYZRHW|D3DFVF_DIFFUSE|D3DFVF_TEX1)

IDirect3DSurface8* g_backBuffer;
D3DFORMAT g_bbFormat;
BOOL g_bGotFormat = false;
BOOL g_needsRestore = TRUE;
int bpp = 0;

void MaskKitTexture(BITMAPINFO* tex, DWORD id);
typedef void (*MASKFUNCPROC)(BITMAPINFO* tex, DWORD id);

/* IDirect3DDevice8 method-types */
typedef HRESULT (STDMETHODCALLTYPE *PFNCREATETEXTUREPROC)(IDirect3DDevice8* self, 
UINT width, UINT height, UINT levels, DWORD usage, D3DFORMAT format, D3DPOOL pool, 
IDirect3DTexture8** ppTexture);
typedef HRESULT (STDMETHODCALLTYPE *PFNSETTEXTUREPROC)(IDirect3DDevice8* self, 
DWORD stage, IDirect3DBaseTexture8* pTexture);
typedef HRESULT (STDMETHODCALLTYPE *PFNSETTEXTURESTAGESTATEPROC)(IDirect3DDevice8* self, 
DWORD Stage, D3DTEXTURESTAGESTATETYPE Type, DWORD Value);
typedef HRESULT (STDMETHODCALLTYPE *PFNUPDATETEXTUREPROC)(IDirect3DDevice8* self,
IDirect3DBaseTexture8* pSrc, IDirect3DBaseTexture8* pDest);
typedef HRESULT (STDMETHODCALLTYPE *PFNCOPYRECTSPROC)(IDirect3DDevice8* self,
IDirect3DSurface8* pSourceSurface, CONST RECT* pSourceRectsArray, UINT cRects,
IDirect3DSurface8* pDestinationSurface, CONST POINT* pDestPointsArray);
typedef HRESULT (STDMETHODCALLTYPE *PFNAPPLYSTATEBLOCKPROC)(IDirect3DDevice8* self, DWORD token);
typedef HRESULT (STDMETHODCALLTYPE *PFNBEGINSCENEPROC)(IDirect3DDevice8* self);
typedef HRESULT (STDMETHODCALLTYPE *PFNENDSCENEPROC)(IDirect3DDevice8* self);
typedef HRESULT (STDMETHODCALLTYPE *PFNGETSURFACELEVELPROC)(IDirect3DTexture8* self,
UINT level, IDirect3DSurface8** ppSurfaceLevel);

void kservAfsReplace(GETFILEINFO* gfi);
void kservUniDecode(DWORD decBuf);
void kservUnpack(GETFILEINFO* gfi, DWORD part, DWORD decBuf, DWORD size);
void  JuceSet2Dkits();
void  JuceClear2Dkits();
DWORD JuceGetTeamInfo(DWORD id, DWORD p1);
DWORD JuceReadTeamInfoOnReplaySave(DWORD id, DWORD p1);
DWORD JuceReadTeamInfoOnReplaySave2(DWORD id);
DWORD JuceReadTeamInfoOnMLSave();
DWORD JuceOnReplayLoad();
DWORD kservWriteKitInfo(DWORD teamId, DWORD kitOrdinal);
DWORD kservSetFlag();
DWORD kservResetFlag();
DWORD kservResetFlag2();
DWORD kservProcessKit(DWORD dest, DWORD src);
DWORD kservSetRadarColor();
void ResetTeamInfo(KITPACKINFO* kitPackInfo, TEAMKITINFO* savedTeamInfo);
void resetLicensedOrdinals();
void clearTeamKitInfo();
void setTeamKitInfo();
void ClearTextureMaps();

HRESULT STDMETHODCALLTYPE JuceCreateTexture(IDirect3DDevice8* self, UINT width, UINT height,
UINT levels, DWORD usage, D3DFORMAT format, D3DPOOL pool, IDirect3DTexture8** ppTexture, 
DWORD src, bool* IsProcessed);
void kservUnlockRect(IDirect3DTexture8* self,UINT Level);
HRESULT STDMETHODCALLTYPE JuceSetTexture(IDirect3DDevice8* self, DWORD stage, 
IDirect3DBaseTexture8* pTexture);
HRESULT STDMETHODCALLTYPE JuceSetTextureStageState(IDirect3DDevice8* self, DWORD Stage,
D3DTEXTURESTAGESTATETYPE Type, DWORD Value);
HRESULT STDMETHODCALLTYPE JuceUpdateTexture(IDirect3DDevice8* self,
IDirect3DBaseTexture8* pSrc, IDirect3DBaseTexture8* pDest);
void JucePresent(IDirect3DDevice8* self, CONST RECT* src, CONST RECT* dest, HWND hWnd, LPVOID unused);
HRESULT STDMETHODCALLTYPE JuceCopyRects(IDirect3DDevice8* self,
IDirect3DSurface8* pSourceSurface, CONST RECT* pSourceRectsArray, UINT cRects,
IDirect3DSurface8* pDestinationSurface, CONST POINT* pDestPointsArray);
HRESULT STDMETHODCALLTYPE JuceApplyStateBlock(IDirect3DDevice8* self, DWORD token);
HRESULT STDMETHODCALLTYPE JuceBeginScene(IDirect3DDevice8* self);
HRESULT STDMETHODCALLTYPE JuceEndScene(IDirect3DDevice8* self);
HRESULT STDMETHODCALLTYPE JuceGetSurfaceLevel(IDirect3DTexture8* self,
UINT level, IDirect3DSurface8** ppSurfaceLevel);

DWORD LoadTexture(BITMAPINFO** tex, char* filename);
DWORD LoadPNGTexture(BITMAPINFO** tex, char* filename);
void CreateGDBTextureFromFile(char* filename, IDirect3DTexture8** ppTex, PALETTEENTRY* pPal);
void CreateGDBTextureFromFolder(char* foldername, char* filename, IDirect3DTexture8** ppTex, PALETTEENTRY* pPal);
void FreeTexture(BITMAPINFO* tex);
void FreePNGTexture(BITMAPINFO* tex);

void ApplyGlovesDIBTexture(TEXIMGPACKHEADER* texPack, BITMAPINFO* bitmapInfo);

DWORD FindImageFileForId(DWORD id, char* suffix, char* filename);
DWORD FindImageFileForId(DWORD id, char* suffix, char* filename, BOOL* needsMask);
BOOL FindImageFileForIdEx(DWORD id, char* suffix, char* filename, char* ext, BOOL* needsMask);
DWORD FindShortsPalImageFileForId(DWORD id, string& kitFolderKey, char* filename);
DWORD FindMaskFileForId(DWORD id, char* filename);
DWORD FindOverlayFileForId(DWORD id, char* filename);

char* getKitFoldername(string& kitFolderKey, WORD teamId, int kitType, int kitPart);

/* function pointers */
PFNCREATETEXTUREPROC g_orgCreateTexture = NULL;
PFNSETTEXTUREPROC g_orgSetTexture = NULL;
PFNSETTEXTURESTAGESTATEPROC g_orgSetTextureStageState = NULL;
PFNCOPYRECTSPROC g_orgCopyRects = NULL;
PFNAPPLYSTATEBLOCKPROC g_orgApplyStateBlock = NULL;
PFNBEGINSCENEPROC g_orgBeginScene = NULL;
PFNENDSCENEPROC g_orgEndScene = NULL;
PFNGETSURFACELEVELPROC g_orgGetSurfaceLevel = NULL;
PFNUPDATETEXTUREPROC g_orgUpdateTexture = NULL;

HINSTANCE hInst;

char buf[BUFLEN];

// keyboard hook handle
HHOOK g_hKeyboardHook = NULL;
BOOL bIndicate = false;

UINT g_bbWidth = 0;
UINT g_bbHeight = 0;
UINT g_labelsYShift = 0;

BOOL GoodTeamId(WORD id);
void VerifyTeams(void);
void ResetCyclesAndBuffers2(void);
HRESULT SaveAsBitmap(char*, TEXIMGHEADER*);
HRESULT SaveAs8bitBMP(char*, BYTE*);
HRESULT SaveAs8bitBMP(char*, BYTE*, BYTE*, LONG, LONG);
HRESULT SaveAs4bitBMP(char*, BYTE*, BYTE*, LONG, LONG);
HRESULT SaveAsBMP(char*, BYTE*, SIZE_T, LONG, LONG, int);

//Kit* utilGetKit(WORD kitId);
TeamAttr* utilGetTeamAttr(WORD teamId);
BOOL IsEditedKit(int slot);

BOOL TeamDirExists(DWORD teamId);
BOOL IsNumOrFontTexture(DWORD id);
BOOL IsNumbersTexture(DWORD id);
BOOL IsGKTexture(DWORD id);
string GetKitFolderKeyByTeamId(WORD teamId, int fileType);
string GetKitFolderKey(DWORD id);
string GetNextHomeShirtKey(WORD teamId, string key);
string GetNextHomeShortsKey(WORD teamId, string key);
string GetNextHomeSocksKey(WORD teamId, string key);
string GetNextAwayShirtKey(WORD teamId, string key);
string GetNextAwayShortsKey(WORD teamId, string key);
string GetNextAwaySocksKey(WORD teamId, string key);
void ResetKitKeys();
void ResetKitKeys(int which);
void ResetKitKeys(bool all);
void ClearKitKeys();
BOOL BackToStandardStripSelection(BYTE homeStrip, BYTE awayStrip);
BOOL FileExists(char* filename);
BYTE GetTeamStrips(int which);
void GetTeamStrips(BYTE* strips);
WORD GetTeamId(int which);
WORD GetTeamIdByOrdinalAFS(WORD teamId);
KITPACKINFO* GetKitPackInfo(int which);
KITPACKINFO* GetKitPackInfoById(WORD id, int which);

typedef string (*CYCLEPROC)(WORD teamId, string key);

void Load2DkitTexture(WORD teamId, const char* kitFolder, char* filename, IDirect3DTexture8** ppTex, PALETTEENTRY* pPal, int kitType, int kitPart);
void TryLoad2DkitTexture(WORD teamId, int strip, string currKey, string* key, CYCLEPROC proc, char* filename, IDirect3DTexture8** ppTex, PALETTEENTRY* pPal, int kitType, int kitPart);
void TryLoadSamePal2DkitTexture(WORD teamId, int strip, string currKey, string* key, CYCLEPROC proc, char* filename, IDirect3DTexture8** ppTex, PALETTEENTRY* pPal, PALETTEENTRY* cmpPal, int kitType, int kitPart);
BOOL IsSamePalette(PALETTEENTRY* a, PALETTEENTRY* b);
PALETTEENTRY* MakePaletteCopy(PALETTEENTRY* src);

// palette buffers: for shared-palette kit checks
PALETTEENTRY g_home_shirt_pal[0x100];
PALETTEENTRY g_home_shorts_pal[0x100];
PALETTEENTRY g_home_socks_pal[0x100];
PALETTEENTRY g_away_shirt_pal[0x100];
PALETTEENTRY g_away_shorts_pal[0x100];
PALETTEENTRY g_away_socks_pal[0x100];

char nullString[8] = "(null)\0";
enum {
	KITPART_SHIRT, KITPART_SHORTS, KITPART_SOCKS, KITPART_NUMBERS, KITPART_FONT,
};


//////////////////////////////////////////////////////////////
// FUNCTIONS /////////////////////////////////////////////////
//////////////////////////////////////////////////////////////

// Calls IUnknown::Release() on an instance
void SafeRelease(LPVOID ppObj)
{
    try {
        IUnknown** ppIUnknown = (IUnknown**)ppObj;
        if (ppIUnknown == NULL)
        {
            Log(&k_mydll,"Address of IUnknown reference is 0");
            return;
        }
        if (*ppIUnknown != NULL)
        {
            (*ppIUnknown)->Release();
            *ppIUnknown = NULL;
        }
    } catch (...) {
        // problem with a safe-release
        TRACE(&k_mydll,"Problem with safe-release");
    }
}

/* remove keyboard hook */
void UninstallKeyboardHook(void)
{
	if (g_hKeyboardHook != NULL)
	{
		UnhookWindowsHookEx( g_hKeyboardHook );
		Log(&k_mydll,"Keyboard hook uninstalled.");
		g_hKeyboardHook = NULL;
	}
}

void SetPosition(CUSTOMVERTEX2* dest, CUSTOMVERTEX2* src, int n, int x, int y) 
{
    FLOAT xratio = g_bbWidth / 1024.0;
    FLOAT yratio = g_bbHeight / 768.0;
    for (int i=0; i<n; i++) {
        dest[i].x = (FLOAT)(int)((src[i].x + x) * xratio);
        dest[i].y = (FLOAT)(int)((src[i].y + y) * yratio);
    }
}

void SetPosition(CUSTOMVERTEX* dest, CUSTOMVERTEX* src, int n, int x, int y) 
{
    FLOAT xratio = g_bbWidth / 1024.0;
    FLOAT yratio = g_bbHeight / 768.0;
    for (int i=0; i<n; i++) {
        dest[i].x = (FLOAT)(int)((src[i].x + x) * xratio);
        dest[i].y = (FLOAT)(int)((src[i].y + y) * yratio);
    }
}

void NarrowBackCorrection(CUSTOMVERTEX2* dest, CUSTOMVERTEX2* src, int n)
{
    for (int i=0; i<n; i++) {
        dest[i].tu = src[i].tu;
        dest[i].tv = src[i].tv;
    }
}

HRESULT MakeVertexBuffer(IDirect3DDevice8* dev, CUSTOMVERTEX* data, 
        size_t dataSize, float x, IDirect3DVertexBuffer8** ppVB)
{
	VOID* pVertices;
	if (FAILED(dev->CreateVertexBuffer(dataSize*2, D3DUSAGE_WRITEONLY, 
					D3DFVF_CUSTOMVERTEX, D3DPOOL_MANAGED, ppVB)))
	{
		Log(&k_mydll,"CreateVertexBuffer() failed.");
		return E_FAIL;
	}
	Log(&k_mydll,"CreateVertexBuffer() done.");

	if (FAILED((*ppVB)->Lock(0, dataSize*2, (BYTE**)&pVertices, 0)))
	{
		Log(&k_mydll,"(*ppVB)->Lock() failed.");
		return E_FAIL;
	}
	memcpy(pVertices, data, dataSize);
	memcpy((BYTE*)pVertices + dataSize, data, dataSize);
	SetPosition((CUSTOMVERTEX*)pVertices, data, dataSize/sizeof(CUSTOMVERTEX), x, Y_2DKIT_LEFT);
	SetPosition((CUSTOMVERTEX*)pVertices + dataSize/sizeof(CUSTOMVERTEX), data, dataSize/sizeof(CUSTOMVERTEX), x, Y_2DKIT_LEFT_ONLINE);
	g_pVB_gloves_left->Unlock();
    return S_OK;
}

HRESULT MakeVertexBuffer2(IDirect3DDevice8* dev, CUSTOMVERTEX2* data, size_t dataSize, 
        float x, IDirect3DVertexBuffer8** ppVB)
{
	VOID* pVertices;
	if (FAILED(dev->CreateVertexBuffer(dataSize*2, D3DUSAGE_WRITEONLY, 
					D3DFVF_CUSTOMVERTEX2, D3DPOOL_MANAGED, ppVB)))
	{
		Log(&k_mydll,"CreateVertexBuffer() failed.");
		return E_FAIL;
	}
	Log(&k_mydll,"CreateVertexBuffer() done.");

	if (FAILED((*ppVB)->Lock(0, dataSize*2, (BYTE**)&pVertices, 0)))
	{
		Log(&k_mydll,"(*ppVB)->Lock() failed.");
		return E_FAIL;
	}
	memcpy(pVertices, data, dataSize);
	memcpy((BYTE*)pVertices + dataSize, data, dataSize);
	SetPosition((CUSTOMVERTEX2*)pVertices, data, dataSize/sizeof(CUSTOMVERTEX2), x, Y_2DKIT_LEFT);
	SetPosition((CUSTOMVERTEX2*)pVertices + dataSize/sizeof(CUSTOMVERTEX2), data, dataSize/sizeof(CUSTOMVERTEX2), x, Y_2DKIT_LEFT_ONLINE);
	g_pVB_gloves_left->Unlock();
    return S_OK;
}

/* creates vertex buffers */
HRESULT InitVB(IDirect3DDevice8* dev)
{
	// create vertex buffers

	// gloves-left
    if (FAILED(MakeVertexBuffer2(dev,
                g_gloves_left,sizeof(g_gloves_left),
                481, &g_pVB_gloves_left))) 
    {
        Log(&k_mydll, "MakeVertexBuffer2 failed for gloves");
        return E_FAIL;
    }

    ///// HOME /////////////////////////////////

    for (int i=0; i<4; i++) {
        if (FAILED(MakeVertexBuffer2(dev,
                        g_2Dkit_shirt_left[i],sizeof(g_2Dkit_shirt_left[i]),
                        X_2DKIT_LEFT, &g_pVB_home_shirt_left[i])) 
            || FAILED(MakeVertexBuffer2(dev,
                        g_2Dkit_shirt_right[i],sizeof(g_2Dkit_shirt_right[i]),
                        X_2DKIT_LEFT, &g_pVB_home_shirt_right[i])) 
            || FAILED(MakeVertexBuffer2(dev,
                        g_2Dkit_sleeve_left[i],sizeof(g_2Dkit_sleeve_left[i]),
                        X_2DKIT_LEFT, &g_pVB_home_sleeve_left[i])) 
            || FAILED(MakeVertexBuffer2(dev,
                        g_2Dkit_sleeve_right[i],sizeof(g_2Dkit_sleeve_right[i]),
                        X_2DKIT_LEFT, &g_pVB_home_sleeve_right[i])) 
            || FAILED(MakeVertexBuffer(dev,
                        g_2Dkit_shirt_outline[i],sizeof(g_2Dkit_shirt_outline[i]),
                        X_2DKIT_LEFT, &g_pVB_home_shirt_outline[i])) 
            || FAILED(MakeVertexBuffer(dev,
                        g_2Dkit_sleeve_left_outline[i],sizeof(g_2Dkit_sleeve_left_outline[i]),
                        X_2DKIT_LEFT, &g_pVB_home_sleeve_left_outline[i])) 
            || FAILED(MakeVertexBuffer(dev,
                        g_2Dkit_sleeve_right_outline[i],sizeof(g_2Dkit_sleeve_right_outline[i]),
                        X_2DKIT_LEFT, &g_pVB_home_sleeve_right_outline[i])) 
            || FAILED(MakeVertexBuffer2(dev,
                        g_2Dkit_shorts[i],sizeof(g_2Dkit_shorts[i]),
                        X_2DKIT_LEFT, &g_pVB_home_shorts[i])) 
            || FAILED(MakeVertexBuffer2(dev,
                        g_2Dkit_shorts_2[i],sizeof(g_2Dkit_shorts_2[i]),
                        X_2DKIT_LEFT, &g_pVB_home_shorts_2[i])) 
            || FAILED(MakeVertexBuffer(dev,
                        g_2Dkit_shorts_outline[i],sizeof(g_2Dkit_shorts_outline[i]),
                        X_2DKIT_LEFT, &g_pVB_home_shorts_outline[i])) 
            || FAILED(MakeVertexBuffer(dev,
                        g_2Dkit_shorts_2_outline[i],sizeof(g_2Dkit_shorts_2_outline[i]),
                        X_2DKIT_LEFT, &g_pVB_home_shorts_2_outline[i])) 
            || FAILED(MakeVertexBuffer2(dev,
                        g_2Dkit_socks_shin_left[i],sizeof(g_2Dkit_socks_shin_left[i]),
                        X_2DKIT_LEFT, &g_pVB_home_socks_shin_left[i])) 
            || FAILED(MakeVertexBuffer2(dev,
                        g_2Dkit_socks_shin_right[i],sizeof(g_2Dkit_socks_shin_right[i]),
                        X_2DKIT_LEFT + SOCK_SHIFT, &g_pVB_home_socks_shin_right[i])) 
            || FAILED(MakeVertexBuffer2(dev,
                        g_2Dkit_socks_foot_left[i],sizeof(g_2Dkit_socks_foot_left[i]),
                        X_2DKIT_LEFT, &g_pVB_home_socks_foot_left[i])) 
            || FAILED(MakeVertexBuffer2(dev,
                        g_2Dkit_socks_foot_right[i],sizeof(g_2Dkit_socks_foot_right[i]),
                        X_2DKIT_LEFT + SOCK_SHIFT, &g_pVB_home_socks_foot_right[i])) 
            || FAILED(MakeVertexBuffer(dev,
                        g_2Dkit_socks_left_outline[i],sizeof(g_2Dkit_socks_left_outline[i]),
                        X_2DKIT_LEFT, &g_pVB_home_socks_left_outline[i])) 
            || FAILED(MakeVertexBuffer(dev,
                        g_2Dkit_socks_right_outline[i],sizeof(g_2Dkit_socks_right_outline[i]),
                        X_2DKIT_LEFT + SOCK_SHIFT, &g_pVB_home_socks_right_outline[i])) 
           ) {
            Log(&k_mydll,"CreateVertexBuffer() for HOME failed.");
            return E_FAIL;
        }
    }

    ///// AWAY /////////////////////////////////
    
    for (i=0; i<4; i++) {
        if (FAILED(MakeVertexBuffer2(dev,
                        g_2Dkit_shirt_left[i],sizeof(g_2Dkit_shirt_left[i]),
                        X_2DKIT_LEFT + AWAY_KIT_SHIFT, &g_pVB_away_shirt_left[i])) 
            || FAILED(MakeVertexBuffer2(dev,
                        g_2Dkit_shirt_right[i],sizeof(g_2Dkit_shirt_right[i]),
                        X_2DKIT_LEFT + AWAY_KIT_SHIFT, &g_pVB_away_shirt_right[i])) 
            || FAILED(MakeVertexBuffer2(dev,
                        g_2Dkit_sleeve_left[i],sizeof(g_2Dkit_sleeve_left[i]),
                        X_2DKIT_LEFT + AWAY_KIT_SHIFT, &g_pVB_away_sleeve_left[i])) 
            || FAILED(MakeVertexBuffer2(dev,
                        g_2Dkit_sleeve_right[i],sizeof(g_2Dkit_sleeve_right[i]),
                        X_2DKIT_LEFT + AWAY_KIT_SHIFT, &g_pVB_away_sleeve_right[i])) 
            || FAILED(MakeVertexBuffer(dev,
                        g_2Dkit_shirt_outline[i],sizeof(g_2Dkit_shirt_outline[i]),
                        X_2DKIT_LEFT + AWAY_KIT_SHIFT, &g_pVB_away_shirt_outline[i])) 
            || FAILED(MakeVertexBuffer(dev,
                        g_2Dkit_sleeve_left_outline[i],sizeof(g_2Dkit_sleeve_left_outline[i]),
                        X_2DKIT_LEFT + AWAY_KIT_SHIFT, &g_pVB_away_sleeve_left_outline[i])) 
            || FAILED(MakeVertexBuffer(dev,
                        g_2Dkit_sleeve_right_outline[i],sizeof(g_2Dkit_sleeve_right_outline[i]),
                        X_2DKIT_LEFT + AWAY_KIT_SHIFT, &g_pVB_away_sleeve_right_outline[i])) 
            || FAILED(MakeVertexBuffer2(dev,
                        g_2Dkit_shorts[i],sizeof(g_2Dkit_shorts[i]),
                        X_2DKIT_LEFT + AWAY_KIT_SHIFT, &g_pVB_away_shorts[i])) 
            || FAILED(MakeVertexBuffer2(dev,
                        g_2Dkit_shorts_2[i],sizeof(g_2Dkit_shorts_2[i]),
                        X_2DKIT_LEFT + AWAY_KIT_SHIFT, &g_pVB_away_shorts_2[i])) 
            || FAILED(MakeVertexBuffer(dev,
                        g_2Dkit_shorts_outline[i],sizeof(g_2Dkit_shorts_outline[i]),
                        X_2DKIT_LEFT + AWAY_KIT_SHIFT, &g_pVB_away_shorts_outline[i])) 
            || FAILED(MakeVertexBuffer(dev,
                        g_2Dkit_shorts_2_outline[i],sizeof(g_2Dkit_shorts_2_outline[i]),
                        X_2DKIT_LEFT + AWAY_KIT_SHIFT, &g_pVB_away_shorts_2_outline[i])) 
            || FAILED(MakeVertexBuffer2(dev,
                        g_2Dkit_socks_shin_left[i],sizeof(g_2Dkit_socks_shin_left[i]),
                        X_2DKIT_LEFT + AWAY_KIT_SHIFT, &g_pVB_away_socks_shin_left[i])) 
            || FAILED(MakeVertexBuffer2(dev,
                        g_2Dkit_socks_shin_right[i],sizeof(g_2Dkit_socks_shin_right[i]),
                        X_2DKIT_LEFT + AWAY_KIT_SHIFT + SOCK_SHIFT, &g_pVB_away_socks_shin_right[i])) 
            || FAILED(MakeVertexBuffer2(dev,
                        g_2Dkit_socks_foot_left[i],sizeof(g_2Dkit_socks_foot_left[i]),
                        X_2DKIT_LEFT + AWAY_KIT_SHIFT, &g_pVB_away_socks_foot_left[i])) 
            || FAILED(MakeVertexBuffer2(dev,
                        g_2Dkit_socks_foot_right[i],sizeof(g_2Dkit_socks_foot_right[i]),
                        X_2DKIT_LEFT + AWAY_KIT_SHIFT + SOCK_SHIFT, &g_pVB_away_socks_foot_right[i])) 
            || FAILED(MakeVertexBuffer(dev,
                        g_2Dkit_socks_left_outline[i],sizeof(g_2Dkit_socks_left_outline[i]),
                        X_2DKIT_LEFT + AWAY_KIT_SHIFT, &g_pVB_away_socks_left_outline[i])) 
            || FAILED(MakeVertexBuffer(dev,
                        g_2Dkit_socks_right_outline[i],sizeof(g_2Dkit_socks_right_outline[i]),
                        X_2DKIT_LEFT + AWAY_KIT_SHIFT + SOCK_SHIFT, &g_pVB_away_socks_right_outline[i])) 
           ) {
            Log(&k_mydll,"CreateVertexBuffer() for AWAY failed.");
            return E_FAIL;
        }
    }

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
		Log(&k_mydll,"InitVB() failed.");
        return hr;
    }
	Log(&k_mydll,"InitVB() done.");

	// Create the state blocks for rendering overlay graphics
	for( UINT which=0; which<2; which++ )
	{
		dev->BeginStateBlock();

        dev->SetTextureStageState( 0, D3DTSS_COLOROP, D3DTOP_SELECTARG1 );
        dev->SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_TEXTURE );
        dev->SetTextureStageState( 0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1 );
        dev->SetTextureStageState( 0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE );
        dev->SetTextureStageState( 1, D3DTSS_COLOROP, D3DTOP_DISABLE );
        dev->SetTextureStageState( 1, D3DTSS_ALPHAOP, D3DTOP_DISABLE );
        dev->SetVertexShader( D3DFVF_CUSTOMVERTEX2 );
        dev->SetTexture( 0, g_home_shirt_tex );

        // Set diffuse blending for alpha set in vertices.
        dev->SetRenderState( D3DRS_ALPHABLENDENABLE,   TRUE );
        
		if( which==0 )
			dev->EndStateBlock( &g_dwSavedStateBlock );
		else
			dev->EndStateBlock( &g_dwDrawOverlayStateBlock );
	}

    return S_OK;
}

void DeleteStateBlocks(IDirect3DDevice8* dev)
{
	// Delete the state blocks
	try
	{
        //LogWithNumber(&k_mydll,"dev = %08x", (DWORD)dev);
        DWORD* vtab = (DWORD*)(*(DWORD*)dev);
        //LogWithNumber(&k_mydll,"vtab = %08x", (DWORD)vtab);
        if (vtab && vtab[VTAB_DELETESTATEBLOCK]) {
            //LogWithNumber(&k_mydll,"vtab[VTAB_DELETESTATEBLOCK] = %08x", (DWORD)vtab[VTAB_DELETESTATEBLOCK]);
            if (g_dwSavedStateBlock) {
                dev->DeleteStateBlock( g_dwSavedStateBlock );
                Log(&k_mydll,"g_dwSavedStateBlock deleted.");
            }
            if (g_dwDrawOverlayStateBlock) {
                dev->DeleteStateBlock( g_dwDrawOverlayStateBlock );
                Log(&k_mydll,"g_dwDrawOverlayStateBlock deleted.");
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
	TRACE(&k_mydll,"InvalidateDeviceObjects called.");
	if (dev == NULL)
	{
		TRACE(&k_mydll,"InvalidateDeviceObjects: nothing to invalidate.");
		return S_OK;
	}

    // home
    for (int i=0; i<4; i++) {
        SafeRelease( &g_pVB_home_shirt_left[i] );
        SafeRelease( &g_pVB_home_sleeve_left[i] );
        SafeRelease( &g_pVB_home_shirt_right[i] );
        SafeRelease( &g_pVB_home_sleeve_right[i] );
        SafeRelease( &g_pVB_home_shirt_outline[i] );
        SafeRelease( &g_pVB_home_sleeve_left_outline[i] );
        SafeRelease( &g_pVB_home_sleeve_right_outline[i] );
        SafeRelease( &g_pVB_home_shorts[i] );
        SafeRelease( &g_pVB_home_shorts_2[i] );
        SafeRelease( &g_pVB_home_shorts_outline[i] );
        SafeRelease( &g_pVB_home_shorts_2_outline[i] );
        SafeRelease( &g_pVB_home_socks_shin_left[i] );
        SafeRelease( &g_pVB_home_socks_foot_left[i] );
        SafeRelease( &g_pVB_home_socks_left_outline[i] );
        SafeRelease( &g_pVB_home_socks_shin_right[i] );
        SafeRelease( &g_pVB_home_socks_foot_right[i] );
        SafeRelease( &g_pVB_home_socks_right_outline[i] );
    }

    // away
    for (i=0; i<4; i++) {
        SafeRelease( &g_pVB_away_shirt_left[i] );
        SafeRelease( &g_pVB_away_sleeve_left[i] );
        SafeRelease( &g_pVB_away_shirt_right[i] );
        SafeRelease( &g_pVB_away_sleeve_right[i] );
        SafeRelease( &g_pVB_away_shirt_outline[i] );
        SafeRelease( &g_pVB_away_sleeve_left_outline[i] );
        SafeRelease( &g_pVB_away_sleeve_right_outline[i] );
        SafeRelease( &g_pVB_away_shorts[i] );
        SafeRelease( &g_pVB_away_shorts_2[i] );
        SafeRelease( &g_pVB_away_shorts_outline[i] );
        SafeRelease( &g_pVB_away_shorts_2_outline[i] );
        SafeRelease( &g_pVB_away_socks_shin_left[i] );
        SafeRelease( &g_pVB_away_socks_foot_left[i] );
        SafeRelease( &g_pVB_away_socks_left_outline[i] );
        SafeRelease( &g_pVB_away_socks_shin_right[i] );
        SafeRelease( &g_pVB_away_socks_foot_right[i] );
        SafeRelease( &g_pVB_away_socks_right_outline[i] );
    }

    // gloves indicator
	SafeRelease( &g_pVB_gloves_left );

	Log(&k_mydll,"InvalidateDeviceObjects: SafeRelease(s) done.");

    DeleteStateBlocks(dev);
    Log(&k_mydll,"InvalidateDeviceObjects: DeleteStateBlock(s) done.");

    //if (g_font) g_font->InvalidateDeviceObjects();
    return S_OK;
}

//-----------------------------------------------------------------------------
// Name: DeleteDeviceObjects()
// Desc: Destroys all device-dependent objects
//-----------------------------------------------------------------------------
HRESULT DeleteDeviceObjects(IDirect3DDevice8* dev)
{
	//if (g_font) g_font->DeleteDeviceObjects();
	//g_fontInitialized = false;

    return S_OK;
}

/**
 * Reset kit keys back to original position, and reset iterators.
 */
void ResetKitKeys()
{
    Log(&k_mydll,"ResetKitKeys: CALLED.");

    BYTE homeStrip = GetTeamStrips(HOME);
    BYTE awayStrip = GetTeamStrips(AWAY);

    switch (typ) {
        case PL_TYPE:
            g_homeShirtKeyPL = (homeStrip & STRIP_PL_SHIRT)?"pb":"pa";
            g_homeShortsKeyPL = (homeStrip & STRIP_PL_SHORTS)?"pb":"pa";
            g_homeSocksKeyPL = (homeStrip & STRIP_PL_SOCKS)?"pb":"pa";
            g_awayShirtKeyPL = (awayStrip & STRIP_PL_SHIRT)?"pb":"pa";
            g_awayShortsKeyPL = (awayStrip & STRIP_PL_SHORTS)?"pb":"pa";
            g_awaySocksKeyPL = (awayStrip & STRIP_PL_SOCKS)?"pb":"pa";
            break;

        case GK_TYPE:
            g_homeShirtKeyGK = (homeStrip & STRIP_GK_SHIRT)?"gb":"ga";
            g_homeShortsKeyGK = (homeStrip & STRIP_GK_SHORTS)?"gb":"ga";
            g_homeSocksKeyGK = (homeStrip & STRIP_GK_SOCKS)?"gb":"ga";
            g_awayShirtKeyGK = (awayStrip & STRIP_GK_SHIRT)?"gb":"ga";
            g_awayShortsKeyGK = (awayStrip & STRIP_GK_SHORTS)?"gb":"ga";
            g_awaySocksKeyGK = (awayStrip & STRIP_GK_SOCKS)?"gb":"ga";
            break;
    }
}

/**
 * Reset kit keys for a team back to original position, and reset iterators.
 */
void ResetKitKeys(int which)
{
    Log(&k_mydll,"ResetKitKeys(which): CALLED.");

    BYTE homeStrip = GetTeamStrips(HOME);
    BYTE awayStrip = GetTeamStrips(AWAY);

    switch (which) {
        case HOME:
            switch (typ) {
                case PL_TYPE:
                    g_homeShirtKeyPL = (homeStrip & STRIP_PL_SHIRT)?"pb":"pa";
                    g_homeShortsKeyPL = (homeStrip & STRIP_PL_SHORTS)?"pb":"pa";
                    g_homeSocksKeyPL = (homeStrip & STRIP_PL_SOCKS)?"pb":"pa";
                    break;

                case GK_TYPE:
                    g_homeShirtKeyGK = (homeStrip & STRIP_GK_SHIRT)?"gb":"ga";
                    g_homeShortsKeyGK = (homeStrip & STRIP_GK_SHORTS)?"gb":"ga";
                    g_homeSocksKeyGK = (homeStrip & STRIP_GK_SOCKS)?"gb":"ga";
                    break;
            }
            break;
        case AWAY:
            switch (typ) {
                case PL_TYPE:
                    g_awayShirtKeyPL = (awayStrip & STRIP_PL_SHIRT)?"pb":"pa";
                    g_awayShortsKeyPL = (awayStrip & STRIP_PL_SHORTS)?"pb":"pa";
                    g_awaySocksKeyPL = (awayStrip & STRIP_PL_SOCKS)?"pb":"pa";
                    break;

                case GK_TYPE:
                    g_awayShirtKeyGK = (awayStrip & STRIP_GK_SHIRT)?"gb":"ga";
                    g_awayShortsKeyGK = (awayStrip & STRIP_GK_SHORTS)?"gb":"ga";
                    g_awaySocksKeyGK = (awayStrip & STRIP_GK_SOCKS)?"gb":"ga";
                    break;
            }
            break;
    }
}

/**
 * Reset all kit keys back to original position, and reset iterators.
 */
void ResetKitKeys(bool all)
{
    Log(&k_mydll,"ResetKitKeys(bool): CALLED.");

    BYTE homeStrip = GetTeamStrips(HOME);
    BYTE awayStrip = GetTeamStrips(AWAY);

    if (typ == PL_TYPE || all) {
        g_homeShirtKeyPL = (homeStrip & STRIP_PL_SHIRT)?"pb":"pa";
        g_homeShortsKeyPL = (homeStrip & STRIP_PL_SHORTS)?"pb":"pa";
        g_homeSocksKeyPL = (homeStrip & STRIP_PL_SOCKS)?"pb":"pa";
        g_awayShirtKeyPL = (awayStrip & STRIP_PL_SHIRT)?"pb":"pa";
        g_awayShortsKeyPL = (awayStrip & STRIP_PL_SHORTS)?"pb":"pa";
        g_awaySocksKeyPL = (awayStrip & STRIP_PL_SOCKS)?"pb":"pa";
    } 
    if (typ == GK_TYPE || all) {
        g_homeShirtKeyGK = (homeStrip & STRIP_GK_SHIRT)?"gb":"ga";
        g_homeShortsKeyGK = (homeStrip & STRIP_GK_SHORTS)?"gb":"ga";
        g_homeSocksKeyGK = (homeStrip & STRIP_GK_SOCKS)?"gb":"ga";
        g_awayShirtKeyGK = (awayStrip & STRIP_GK_SHIRT)?"gb":"ga";
        g_awayShortsKeyGK = (awayStrip & STRIP_GK_SHORTS)?"gb":"ga";
        g_awaySocksKeyGK = (awayStrip & STRIP_GK_SOCKS)?"gb":"ga";
    }
}

/**
 * Clear kit keys back to original position.
 */
void ClearKitKeys()
{
    Log(&k_mydll,"ClearKitKeys: CALLED.");

    g_homeShirtKeyPL = "";
    g_homeShortsKeyPL = "";
    g_homeSocksKeyPL = "";
    g_awayShirtKeyPL = "";
    g_awayShortsKeyPL = "";
    g_awaySocksKeyPL = "";

    g_homeShirtKeyGK = "";
    g_homeShortsKeyGK = "";
    g_homeSocksKeyGK = "";
    g_awayShirtKeyGK = "";
    g_awayShortsKeyGK = "";
    g_awaySocksKeyGK = "";
}

/**
 * Checks if we got back to changing the kits via 1st/2nd strip selection.
 * (as opposed to find-tuning the kits)
 */
BOOL BackToStandardStripSelection(BYTE homeStrip, BYTE awayStrip)
{
    //LogWithTwoNumbers(&k_mydll,"BackToStandard: home=%02x, g_home=%02x", homeStrip, g_homeStrip);
    //LogWithTwoNumbers(&k_mydll,"BackToStandard: away=%02x, g_away=%02x", awayStrip, g_awayStrip);
    return 
        ((homeStrip & STRIP_PL_SHIRT)!=(g_homeStrip & STRIP_PL_SHIRT) &&
        (homeStrip & STRIP_PL_SHORTS)!=(g_homeStrip & STRIP_PL_SHORTS) &&
        (homeStrip & STRIP_PL_SOCKS)!=(g_homeStrip & STRIP_PL_SOCKS)) 
        ||
        ((awayStrip & STRIP_PL_SHIRT)!=(g_awayStrip & STRIP_PL_SHIRT) &&
        (awayStrip & STRIP_PL_SHORTS)!=(g_awayStrip & STRIP_PL_SHORTS) &&
        (awayStrip & STRIP_PL_SOCKS)!=(g_awayStrip & STRIP_PL_SOCKS));
}

/**
 * Checks if there was a change in kits
 */
BOOL StripChange(int which, BYTE strip)
{
    switch (which) {
        case HOME:
            return 
                (strip & STRIP_PL_SHIRT)!=(g_homeStrip & STRIP_PL_SHIRT) &&
                (strip & STRIP_PL_SHORTS)!=(g_homeStrip & STRIP_PL_SHORTS) &&
                (strip & STRIP_PL_SOCKS)!=(g_homeStrip & STRIP_PL_SOCKS);
        case AWAY:
            return
                (strip & STRIP_PL_SHIRT)!=(g_awayStrip & STRIP_PL_SHIRT) &&
                (strip & STRIP_PL_SHORTS)!=(g_awayStrip & STRIP_PL_SHORTS) &&
                (strip & STRIP_PL_SOCKS)!=(g_awayStrip & STRIP_PL_SOCKS);
    }
    return FALSE;
}

KITPACKINFO* GetKitPackInfo(int which)
{
    WORD id = ((WORD*)data[TEAM_IDS])[which];
    return GetKitPackInfoById(id, which);
}

KITPACKINFO* GetKitPackInfoById(WORD id, int which)
{
    //LogWithTwoNumbers(&k_mydll,"GetKitPackInfoById: id=%08x, which=%d", id, which);
    if (id < 0x40) {
        NATIONALKITINFO* nki = *(NATIONALKITINFO**)data[NATIONAL_TEAMS_ADDR];
        return &nki[id].kits;

    } else if (id < 204) {
        CLUBKITINFO* cki = *(CLUBKITINFO**)data[CLUB_TEAMS_ADDR];
        return &cki[id - 0x40].kits;

    } else if (id == 0x126) {
        return (KITPACKINFO*)(*(BYTE**)data[SAVED_TEAM_HOME] + 0x58);

    } else if (id == 0x127) {
        return (KITPACKINFO*)(*(BYTE**)data[SAVED_TEAM_AWAY] + 0x58);

    } else {
        switch (which) {
            case HOME:
                return (KITPACKINFO*)(*(BYTE**)data[SAVED_TEAM_HOME] + 0x58);
            case AWAY:
                return (KITPACKINFO*)(*(BYTE**)data[SAVED_TEAM_AWAY] + 0x58);
        }
    }
    return NULL;
}

/**
 * Return currently selected (home or away) team ID.
 */
WORD GetTeamId(int which)
{
    BYTE* mlData;
    if (data[TEAM_IDS]==0) return 0xffff;
    WORD id = ((WORD*)data[TEAM_IDS])[which];
    if (id==0x126 || id==0x127) {
        WORD id1,id2;
        switch (id) {
            case 0x126:
                // saved team (home)
                id1 = *(WORD*)(*(BYTE**)data[SAVED_TEAM_HOME] + 0x36);
                id2 = *(WORD*)(*(BYTE**)data[SAVED_TEAM_HOME] + 0x40);
                if (id1 != 0) {
                    id = id1;
                } else {
                    id = id2;
                }
                break;
            case 0x127:
                // saved team (away)
                id1 = *(WORD*)(*(BYTE**)data[SAVED_TEAM_AWAY] + 0x36);
                id2 = *(WORD*)(*(BYTE**)data[SAVED_TEAM_AWAY] + 0x40);
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

/**
 * Return currently selected (home or away) team strips configuration.
 */
BYTE GetTeamStrips(int which)
{
//LogWithTwoNumbers(&k_mydll, "GetTeamStrips(%d) = %d", which, ((BYTE*)data[TEAM_STRIPS])[which]);
//    if (data[TEAM_STRIPS]==0) return 0xff;
//    return ((BYTE*)data[TEAM_STRIPS])[which];
    switch (which) {
        case HOME: return (data[TEAM_STRIPS_HOME]!=0) ? *(BYTE*)data[TEAM_STRIPS_HOME] : 0;
        case AWAY: return (data[TEAM_STRIPS_AWAY]!=0) ? *(BYTE*)data[TEAM_STRIPS_AWAY] : 0;
    }
    return 0;
}

void GetTeamStrips(BYTE* strips)
{
    if (data[TEAM_STRIPS_HOME]==0 || data[TEAM_STRIPS_AWAY]==0) return;
    strips[0] = *(BYTE*)data[TEAM_STRIPS_HOME];
    strips[1] = *(BYTE*)data[TEAM_STRIPS_AWAY];
}

/**
 * Load a texture from a specific kit folder.
 */
void Load2DkitTexture(WORD teamId, const char* kitFolder, char* filename, IDirect3DTexture8** ppTex, PALETTEENTRY* pPal, int kitType, int kitPart) 
{
    MAKE_BUFFER(keyBuf);
    sprintf(keyBuf, "%d|%s|%s", teamId, kitFolder, filename);
    string key = keyBuf;
    //LogWithString(&k_mydll,"keyBuf = %s", keyBuf);
    //LogWithNumber(&k_mydll,"g_kitTextureMap[key] = %08x", (DWORD)g_kitTextureMap[key]);
    if (!g_kitTextureMap[key]) {
        MAKE_BUFFER(folder);
        //gdbFindKitsForTeam(gdb, teamId);
        KitCollection* col = MAP_FIND(gdb->uni,teamId);
		string kitFolder1 = kitFolder;
    	char* kitFoldername = getKitFoldername(kitFolder1, teamId, kitType, kitPart);
        sprintf(folder, "uni\\%s\\%s", (col)?col->foldername:"(null)", kitFoldername);
        CreateGDBTextureFromFolder(folder, filename, ppTex, pPal);
        if (*ppTex) {
            // store texture in the texture cache
            g_kitTextureMap[key] = *ppTex;
        }
    } else {
        // texture already loaded: get from cache
        *ppTex = g_kitTextureMap[key];
    }
}

void PrintHomeKitCollection()
{
	gdbFindKitsForTeam(gdb, GetTeamId(HOME));
    KitCollection* col = MAP_FIND(gdb->uni,GetTeamId(HOME));
    if (col) {
        for (StringKitMap::iterator it = (*(col->players)).begin(); 
                it != (*(col->players)).end(); it++) {
            string key = it->first;
            LogWithString(&k_mydll,"home kit collection key: {%s}", (char*)key.c_str());
        }
    }
}

void PrintAwayKitCollection()
{
	gdbFindKitsForTeam(gdb, GetTeamId(AWAY));
    KitCollection* col = MAP_FIND(gdb->uni,GetTeamId(AWAY));
    if (col) {
        for (StringKitMap::iterator it = (*(col->players)).begin(); 
                it != (*(col->players)).end(); it++) {
            string key = it->first;
            LogWithString(&k_mydll,"away kit collection key: {%s}", (char*)key.c_str());
        }
    }
}

void TryLoad2DkitTexture(WORD teamId, string oldKey, string& key, CYCLEPROC proc, char* filename, IDirect3DTexture8** ppTex, PALETTEENTRY* pPal, int kitType, int kitPart)
{
    // check for "(null)"
    if (key == "(null)") {
        return; // no textures available
    }
    // make sure we try all existing textures
    Load2DkitTexture(teamId, key.c_str(), filename, ppTex, pPal, kitType, kitPart);
    string seenKey = key;
    while (!*ppTex) {
        key = proc(teamId, oldKey);
        Load2DkitTexture(teamId, key.c_str(), filename, ppTex, pPal, kitType, kitPart);
        if (key == seenKey) break; // full cycle

    }
}

void TryLoadSamePal2DkitTexture(WORD teamId, string oldKey, string& key, CYCLEPROC proc, char* filename, IDirect3DTexture8** ppTex, PALETTEENTRY* pPal, PALETTEENTRY* cmpPal, int kitType, int kitPart)
{
    // check for "(null)"
    if (key == "(null)") {
        return; // no textures available
    }
    // make sure we try all existing textures
    key = proc(teamId, oldKey);
    Load2DkitTexture(teamId, key.c_str(), filename, ppTex, pPal, kitType, kitPart);
    string seenKey = key;
    while (!*ppTex || !IsSamePalette(pPal, cmpPal)) {
        key = proc(teamId, oldKey);
        Load2DkitTexture(teamId, key.c_str(), filename, ppTex, pPal, kitType, kitPart);
        if (key == seenKey) break; // full cycle

    }
}

BOOL IsSamePalette(PALETTEENTRY* a, PALETTEENTRY* b)
{
    return memcmp(a, b, 0x100*sizeof(PALETTEENTRY))==0;
}

PALETTEENTRY* MakePaletteCopy(PALETTEENTRY* src)
{
    PALETTEENTRY* dest = (PALETTEENTRY*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, 0x100*sizeof(PALETTEENTRY));
    if (dest) {
        memcpy(dest, src, 0x100*sizeof(PALETTEENTRY));
        return dest;
    }
    return NULL;
}

void PrintPal(PALETTEENTRY* pal)
{
    FILE* f = fopen("c:\\pal.log","ab");
    for (int i=0; i<0x100; i++) {
        fprintf(f,"%c%c%c%c",pal[i].peRed,pal[i].peGreen,pal[i].peBlue,pal[i].peFlags);
    }
    fclose(f);
}

#define CHANGED_SHIRT(oldS, newS) ((oldS & STRIP_PL_SHIRT)!=(newS & STRIP_PL_SHIRT))
#define CHANGED_SHORTS(oldS, newS) ((oldS & STRIP_PL_SHORTS)!=(newS & STRIP_PL_SHORTS))
#define CHANGED_SOCKS(oldS, newS) ((oldS & STRIP_PL_SOCKS)!=(newS & STRIP_PL_SOCKS))
#define UNDEFINED(s1, s2) ((s1 == 0xff && s2 == 0xff))

/**
 * Load the needed textures from GDB
 */
void Load2Dkits()
{
    // Get current strip selection
    BYTE homeStrip = GetTeamStrips(HOME);
    BYTE awayStrip = GetTeamStrips(AWAY);

    // safety check for early exit
    if (UNDEFINED(homeStrip, awayStrip)) {
        return;
    }

    // initialize g_homeStrip and g_awayStrip, if this is a new session
    if (UNDEFINED(g_homeStrip, g_awayStrip)) { 
        g_homeStrip = homeStrip;
        g_awayStrip = awayStrip;
        ResetKitKeys();
    }

    // check if we got back to "1st/2nd" selection mode
    //BOOL needsReset = BackToStandardStripSelection(homeStrip, awayStrip);
    BOOL needsReset = StripChange(HOME, homeStrip);

    // Compare with previously saved last strip selection
    if (g_lastDisplay2Dkits != g_display2Dkits || homeStrip != g_homeStrip || needsReset || g_modeSwitch || g_shirtChange[HOME]) {
        // home strip selection changed
        WORD id = GetTeamId(HOME);

        // reset kit keys, if needed
        if (needsReset || g_lastDisplay2Dkits != g_display2Dkits) {
            ResetKitKeys(HOME);
        } else if (TeamDirExists(id)) {
            //if (CHANGED_SHIRT(g_homeStrip, homeStrip) &&
            //        !(CHANGED_SHORTS(g_homeStrip, homeStrip)) &&
            //        !(CHANGED_SOCKS(g_homeStrip, homeStrip))) {
            if (g_shirtChange[HOME]) {
                // cycle home shirt
                string oldKey = GET_HOME_SHIRT_KEY(typ);
                GET_HOME_SHIRT_KEY(typ) = GetNextHomeShirtKey(id, oldKey);
                LogWithString(&k_mydll,"New home shirt: {%s}", (char*)GET_HOME_SHIRT_KEY(typ).c_str());

                TryLoad2DkitTexture(id, oldKey, GET_HOME_SHIRT_KEY(typ), (CYCLEPROC)GetNextHomeShirtKey, "shirt.png", &g_home_shirt_tex, g_home_shirt_pal, typ, KITPART_SHIRT);
                //TryLoad2DkitTexture(id, oldKey, GET_HOME_SHIRT_KEY(typ), (CYCLEPROC)GetNextHomeShirtKey, "shorts.png", &g_home_shorts_tex, g_home_shorts_pal);
                //TryLoad2DkitTexture(id, oldKey, GET_HOME_SHIRT_KEY(typ), (CYCLEPROC)GetNextHomeShirtKey, "socks.png", &g_home_socks_tex, g_home_socks_pal);

                /*
                //make sure shorts and socks are of the same palette
                if (!IsSamePalette(g_home_shirt_pal, g_home_shorts_pal)) {
                    oldKey = GET_HOME_SHORTS_KEY(typ);
                    TryLoadSamePal2DkitTexture(id, oldKey, GET_HOME_SHORTS_KEY(typ), (CYCLEPROC)GetNextHomeShortsKey, "shorts.png", &g_home_shorts_tex, g_home_shorts_pal, g_home_shirt_pal);
                }
                if (!IsSamePalette(g_home_shirt_pal, g_home_socks_pal)) {
                    oldKey = GET_HOME_SOCKS_KEY(typ);
                    TryLoadSamePal2DkitTexture(id, oldKey, GET_HOME_SOCKS_KEY(typ), (CYCLEPROC)GetNextHomeSocksKey, "socks.png", &g_home_socks_tex, g_home_socks_pal, g_home_shirt_pal);
                }
                */
            }
            /*
            //else if (CHANGED_SHORTS(g_homeStrip, homeStrip) &&
            //        !(CHANGED_SHIRT(g_homeStrip, homeStrip)) &&
            //        !(CHANGED_SOCKS(g_homeStrip, homeStrip))) {
            if (g_shirtChange[HOME]) {
                // cycle home shorts
                string oldKey = GET_HOME_SHORTS_KEY(typ);
                GET_HOME_SHORTS_KEY(typ) = GetNextHomeShortsKey(id, oldKey);
                LogWithString(&k_mydll,"New home shorts: {%s}", (char*)GET_HOME_SHORTS_KEY(typ).c_str());

                TryLoad2DkitTexture(id, oldKey, GET_HOME_SHORTS_KEY(typ), (CYCLEPROC)GetNextHomeShortsKey, "shorts.png", &g_home_shorts_tex, g_home_shorts_pal);

                //make sure shirt and socks are of the same palette
                if (!IsSamePalette(g_home_shorts_pal, g_home_shirt_pal)) {
                    oldKey = GET_HOME_SHIRT_KEY(typ);
                    TryLoadSamePal2DkitTexture(id, oldKey, GET_HOME_SHIRT_KEY(typ), (CYCLEPROC)GetNextHomeShirtKey, "shirt.png", &g_home_shirt_tex, g_home_shirt_pal, g_home_shorts_pal);
                }
                if (!IsSamePalette(g_home_shorts_pal, g_home_socks_pal)) {
                    oldKey = GET_HOME_SOCKS_KEY(typ);
                    TryLoadSamePal2DkitTexture(id, oldKey, GET_HOME_SOCKS_KEY(typ), (CYCLEPROC)GetNextHomeSocksKey, "socks.png", &g_home_socks_tex, g_home_socks_pal, g_home_shorts_pal);
                }
            }
            //else if (CHANGED_SOCKS(g_homeStrip, homeStrip) &&
            //        !(CHANGED_SHIRT(g_homeStrip, homeStrip)) &&
            //        !(CHANGED_SHORTS(g_homeStrip, homeStrip))) {
            if (g_shirtChange[HOME]) {
                // cycle home socks
                string oldKey = GET_HOME_SOCKS_KEY(typ);
                GET_HOME_SOCKS_KEY(typ) = GetNextHomeSocksKey(id, oldKey);
                LogWithString(&k_mydll,"New home socks: {%s}", (char*)GET_HOME_SOCKS_KEY(typ).c_str());

                TryLoad2DkitTexture(id, oldKey, GET_HOME_SOCKS_KEY(typ), (CYCLEPROC)GetNextHomeSocksKey, "socks.png", &g_home_socks_tex, g_home_socks_pal);

                //make sure shirt and shorts are of the same palette
                if (!IsSamePalette(g_home_socks_pal, g_home_shirt_pal)) {
                    oldKey = GET_HOME_SHIRT_KEY(typ);
                    TryLoadSamePal2DkitTexture(id, oldKey, GET_HOME_SHIRT_KEY(typ), (CYCLEPROC)GetNextHomeShirtKey, "shirt.png", &g_home_shirt_tex, g_home_shirt_pal, g_home_socks_pal);
                }
                if (!IsSamePalette(g_home_socks_pal, g_home_shorts_pal)) {
                    oldKey = GET_HOME_SHORTS_KEY(typ);
                    TryLoadSamePal2DkitTexture(id, oldKey, GET_HOME_SHORTS_KEY(typ), (CYCLEPROC)GetNextHomeShortsKey, "shorts.png", &g_home_shorts_tex, g_home_shorts_pal, g_home_socks_pal);
                }
            }
            */
        }

        Load2DkitTexture(id, GET_HOME_SHIRT_KEY(typ).c_str(), "shirt.png", &g_home_shirt_tex, g_home_shirt_pal, typ, KITPART_SHIRT);
        Load2DkitTexture(id, GET_HOME_SHIRT_KEY(typ).c_str(), "shorts.png", &g_home_shorts_tex, g_home_shorts_pal, typ, KITPART_SHORTS);
        Load2DkitTexture(id, GET_HOME_SHIRT_KEY(typ).c_str(), "socks.png", &g_home_socks_tex, g_home_socks_pal, typ, KITPART_SOCKS);
        //Load2DkitTexture(id, GET_HOME_SHORTS_KEY(typ).c_str(), "shorts.png", &g_home_shorts_tex, g_home_shorts_pal);
        //Load2DkitTexture(id, GET_HOME_SOCKS_KEY(typ).c_str(), "socks.png", &g_home_socks_tex, g_home_socks_pal);

        g_shirtChange[HOME] = false;

        // update home strip
        g_homeStrip = homeStrip;
    }

    needsReset = StripChange(AWAY, awayStrip);

    if (g_lastDisplay2Dkits != g_display2Dkits || awayStrip != g_awayStrip || needsReset || g_modeSwitch || g_shirtChange[AWAY]) {
        // away strip selection chagned
        WORD id = GetTeamId(AWAY);

        // reset kit keys, if needed
        if (needsReset || g_lastDisplay2Dkits != g_display2Dkits) {
            ResetKitKeys(AWAY);
        } else if (TeamDirExists(id)) {
            //if (CHANGED_SHIRT(g_awayStrip, awayStrip) &&
            //        !(CHANGED_SHORTS(g_awayStrip, awayStrip)) &&
            //        !(CHANGED_SOCKS(g_awayStrip, awayStrip))) {
            if (g_shirtChange[AWAY]) {
                // cycle away shirt
                string oldKey = GET_AWAY_SHIRT_KEY(typ);
                GET_AWAY_SHIRT_KEY(typ) = GetNextAwayShirtKey(id, oldKey);
                LogWithString(&k_mydll,"New away shirt: {%s}", (char*)GET_AWAY_SHIRT_KEY(typ).c_str());

                TryLoad2DkitTexture(id, oldKey, GET_AWAY_SHIRT_KEY(typ), (CYCLEPROC)GetNextAwayShirtKey, "shirt.png", &g_away_shirt_tex, g_away_shirt_pal, typ, KITPART_SHIRT);

                /*
                //make sure shorts and socks are of the same palette
                if (!IsSamePalette(g_away_shirt_pal, g_away_shorts_pal)) {
                    oldKey = GET_AWAY_SHORTS_KEY(typ);
                    TryLoadSamePal2DkitTexture(id, oldKey, GET_AWAY_SHORTS_KEY(typ), (CYCLEPROC)GetNextAwayShortsKey, "shorts.png", &g_away_shorts_tex, g_away_shorts_pal, g_away_shirt_pal);
                }
                if (!IsSamePalette(g_away_shirt_pal, g_away_socks_pal)) {
                    oldKey = GET_AWAY_SOCKS_KEY(typ);
                    TryLoadSamePal2DkitTexture(id, oldKey, GET_AWAY_SOCKS_KEY(typ), (CYCLEPROC)GetNextAwaySocksKey, "socks.png", &g_away_socks_tex, g_away_socks_pal, g_away_shirt_pal);
                }
                */
            }
            /*
            //else if (CHANGED_SHORTS(g_awayStrip, awayStrip) &&
            //        !(CHANGED_SHIRT(g_awayStrip, awayStrip)) &&
            //        !(CHANGED_SOCKS(g_awayStrip, awayStrip))) {
            if (g_shirtChange[AWAY]) {
                // cycle away shorts
                string oldKey = GET_AWAY_SHORTS_KEY(typ);
                GET_AWAY_SHORTS_KEY(typ) = GetNextAwayShortsKey(id, oldKey);
                LogWithString(&k_mydll,"New away shorts: {%s}", (char*)GET_AWAY_SHORTS_KEY(typ).c_str());

                TryLoad2DkitTexture(id, oldKey, GET_AWAY_SHORTS_KEY(typ), (CYCLEPROC)GetNextAwayShortsKey, "shorts.png", &g_away_shorts_tex, g_away_shorts_pal);

                //make sure shirt and socks are of the same palette
                if (!IsSamePalette(g_away_shorts_pal, g_away_shirt_pal)) {
                    oldKey = GET_AWAY_SHIRT_KEY(typ);
                    TryLoadSamePal2DkitTexture(id, oldKey, GET_AWAY_SHIRT_KEY(typ), (CYCLEPROC)GetNextAwayShirtKey, "shirt.png", &g_away_shirt_tex, g_away_shirt_pal, g_away_shorts_pal);
                }
                if (!IsSamePalette(g_away_shorts_pal, g_away_socks_pal)) {
                    oldKey = GET_AWAY_SOCKS_KEY(typ);
                    TryLoadSamePal2DkitTexture(id, oldKey, GET_AWAY_SOCKS_KEY(typ), (CYCLEPROC)GetNextAwaySocksKey, "socks.png", &g_away_socks_tex, g_away_socks_pal, g_away_shorts_pal);
                }
            }
            //else if (CHANGED_SOCKS(g_awayStrip, awayStrip) &&
            //        !(CHANGED_SHIRT(g_awayStrip, awayStrip)) &&
            //        !(CHANGED_SHORTS(g_awayStrip, awayStrip))) {
            if (g_shirtChange[AWAY]) {
                // cycle away socks
                string oldKey = GET_AWAY_SOCKS_KEY(typ);
                GET_AWAY_SOCKS_KEY(typ) = GetNextAwaySocksKey(id, oldKey);
                LogWithString(&k_mydll,"New away socks: {%s}", (char*)GET_AWAY_SOCKS_KEY(typ).c_str());

                TryLoad2DkitTexture(id, oldKey, GET_AWAY_SOCKS_KEY(typ), (CYCLEPROC)GetNextAwaySocksKey, "socks.png", &g_away_socks_tex, g_away_socks_pal);

                //make sure shirt and shorts are of the same palette
                if (!IsSamePalette(g_away_socks_pal, g_away_shirt_pal)) {
                    oldKey = GET_AWAY_SHIRT_KEY(typ);
                    TryLoadSamePal2DkitTexture(id, oldKey, GET_AWAY_SHIRT_KEY(typ), (CYCLEPROC)GetNextAwayShirtKey, "shirt.png", &g_away_shirt_tex, g_away_shirt_pal, g_away_socks_pal);
                }
                if (!IsSamePalette(g_away_socks_pal, g_away_shorts_pal)) {
                    oldKey = GET_AWAY_SHORTS_KEY(typ);
                    TryLoadSamePal2DkitTexture(id, oldKey, GET_AWAY_SHORTS_KEY(typ), (CYCLEPROC)GetNextAwayShortsKey, "shorts.png", &g_away_shorts_tex, g_away_shorts_pal, g_away_socks_pal);
                }
            }
            */
        }

        Load2DkitTexture(id, GET_AWAY_SHIRT_KEY(typ).c_str(), "shirt.png", &g_away_shirt_tex, g_away_shirt_pal, typ, KITPART_SHIRT);
        Load2DkitTexture(id, GET_AWAY_SHIRT_KEY(typ).c_str(), "shorts.png", &g_away_shorts_tex, g_away_shorts_pal, typ, KITPART_SHORTS);
        Load2DkitTexture(id, GET_AWAY_SHIRT_KEY(typ).c_str(), "socks.png", &g_away_socks_tex, g_away_socks_pal, typ, KITPART_SOCKS);
        //Load2DkitTexture(id, GET_AWAY_SHORTS_KEY(typ).c_str(), "shorts.png", &g_away_shorts_tex, g_away_shorts_pal);
        //Load2DkitTexture(id, GET_AWAY_SOCKS_KEY(typ).c_str(), "socks.png", &g_away_socks_tex, g_away_socks_pal);

        g_shirtChange[AWAY] = false;

        // update away strip
        g_awayStrip = awayStrip;
        g_modeSwitch = FALSE;
    }
}

/*
BOOL IsNarrowBackModel(BYTE model)
{
    if (model>=0 && model<sizeof(g_config.narrowBackModels)) {
        return g_config.narrowBackModels[model]==1;
    }
    return FALSE;
}

BOOL IsNarrowBack(int which )
{
    string currKey;
    Kit* kit = NULL;
    WORD teamId = GetTeamId(which);
    if (teamId != 0xffff) {
        KitCollection* col = MAP_FIND(gdb->uni,teamId);
        if (!col) return FALSE;
        switch (which) {
            case HOME:
                currKey = GET_HOME_SHIRT_KEY(typ);
                kit = MAP_FIND(col->players,currKey);
                break;
            case AWAY:
                currKey = GET_AWAY_SHIRT_KEY(typ);
                kit = MAP_FIND(col->players,currKey);
                break;
        }
        // check the model
        if (kit && (kit->attDefined & MODEL)) {
            return IsNarrowBackModel(kit->model);
        }
    }
    return FALSE;
}
*/

int GetTemplateTypeForModel(BYTE model)
{
    hash_map<int,bool>::iterator it0 = g_config.wideBackModels.find(model);
    if (it0 != g_config.wideBackModels.end()) return 0;
    hash_map<int,bool>::iterator it1 = g_config.narrowBackModels.find(model);
    if (it1 != g_config.narrowBackModels.end()) return 1;
    hash_map<int,bool>::iterator it2 = g_config.squashedWithLogoModels.find(model);
    if (it2 != g_config.squashedWithLogoModels.end()) return 2;
    return 3;
}

int GetTemplateType(int which)
{
    string currKey;
    Kit* kit = NULL;
    WORD teamId = GetTeamId(which);
    if (teamId != 0xffff) {
    	gdbFindKitsForTeam(gdb, teamId);
        KitCollection* col = MAP_FIND(gdb->uni,teamId);
        if (!col) return FALSE;
        switch (which) {
            case HOME:
                currKey = GET_HOME_SHIRT_KEY(typ);
                if (currKey[0]=='g') kit = MAP_FIND(col->goalkeepers,currKey);
                else kit = MAP_FIND(col->players,currKey);
                break;
            case AWAY:
                currKey = GET_AWAY_SHIRT_KEY(typ);
                if (currKey[0]=='g') kit = MAP_FIND(col->goalkeepers,currKey);
                else kit = MAP_FIND(col->players,currKey);
                break;
        }
        // check the model
        if (kit && (kit->attDefined & MODEL)) {
            return GetTemplateTypeForModel(kit->model);
        }
    }
    return 0;
}

void RenderHomeShirt(IDirect3DDevice8* dev, int tt, BYTE onlineFlag)
{
    if (g_home_shirt_tex) {
        IDirect3DVertexBuffer8* home_sleeve_left = g_pVB_home_sleeve_left[tt];
        IDirect3DVertexBuffer8* home_sleeve_right = g_pVB_home_sleeve_right[tt];
        IDirect3DVertexBuffer8* home_shirt_left = g_pVB_home_shirt_left[tt];
        IDirect3DVertexBuffer8* home_shirt_right = g_pVB_home_shirt_right[tt];

        // shirt texture
        dev->SetVertexShader( D3DFVF_CUSTOMVERTEX2 );
        // shirt
        dev->SetTexture(0, g_home_shirt_tex);
        // left sleeve
        dev->SetStreamSource( 0, home_sleeve_left, sizeof(CUSTOMVERTEX2));
        dev->DrawPrimitive( D3DPT_TRIANGLESTRIP, 4*onlineFlag, 2);
        // right sleeve
        dev->SetStreamSource( 0, home_sleeve_right, sizeof(CUSTOMVERTEX2));
        dev->DrawPrimitive( D3DPT_TRIANGLESTRIP, 4*onlineFlag, 2);

        // sleeve outlines
        dev->SetVertexShader( D3DFVF_CUSTOMVERTEX );
        dev->SetTexture(0, NULL);
        dev->SetStreamSource( 0, g_pVB_home_sleeve_left_outline[tt], sizeof(CUSTOMVERTEX));
        dev->DrawPrimitive( D3DPT_LINESTRIP, 5*onlineFlag, 4);
        dev->SetStreamSource( 0, g_pVB_home_sleeve_right_outline[tt], sizeof(CUSTOMVERTEX));
        dev->DrawPrimitive( D3DPT_LINESTRIP, 5*onlineFlag, 4);

        // shirt texture
        dev->SetVertexShader( D3DFVF_CUSTOMVERTEX2 );
        // shirt-left
        dev->SetTexture(0, g_home_shirt_tex);
        dev->SetStreamSource( 0, home_shirt_left, sizeof(CUSTOMVERTEX2));
        dev->DrawPrimitive( D3DPT_TRIANGLESTRIP, 6*onlineFlag, 4);
        // shirt-right
        dev->SetStreamSource( 0, home_shirt_right, sizeof(CUSTOMVERTEX2));
        dev->DrawPrimitive( D3DPT_TRIANGLESTRIP, 6*onlineFlag, 4);

        // shirt outline
        dev->SetVertexShader( D3DFVF_CUSTOMVERTEX );
        dev->SetTexture(0, NULL);
        dev->SetStreamSource( 0, g_pVB_home_shirt_outline[tt], sizeof(CUSTOMVERTEX));
        dev->DrawPrimitive( D3DPT_LINESTRIP, 8*onlineFlag, 7);
    }
}

void RenderAwayShirt(IDirect3DDevice8* dev, int tt, BYTE onlineFlag)
{
    if (g_away_shirt_tex) {
        IDirect3DVertexBuffer8* away_sleeve_left = g_pVB_away_sleeve_left[tt];
        IDirect3DVertexBuffer8* away_sleeve_right = g_pVB_away_sleeve_right[tt];
        IDirect3DVertexBuffer8* away_shirt_left = g_pVB_away_shirt_left[tt];
        IDirect3DVertexBuffer8* away_shirt_right = g_pVB_away_shirt_right[tt];

        // shirt texture
        dev->SetVertexShader( D3DFVF_CUSTOMVERTEX2 );
        // shirt
        dev->SetTexture(0, g_away_shirt_tex);
        // left sleeve
        dev->SetStreamSource( 0, away_sleeve_left, sizeof(CUSTOMVERTEX2));
        dev->DrawPrimitive( D3DPT_TRIANGLESTRIP, 4*onlineFlag, 2);
        // right sleeve
        dev->SetStreamSource( 0, away_sleeve_right, sizeof(CUSTOMVERTEX2));
        dev->DrawPrimitive( D3DPT_TRIANGLESTRIP, 4*onlineFlag, 2);

        // sleeve outlines
        dev->SetVertexShader( D3DFVF_CUSTOMVERTEX );
        dev->SetTexture(0, NULL);
        dev->SetStreamSource( 0, g_pVB_away_sleeve_left_outline[tt], sizeof(CUSTOMVERTEX));
        dev->DrawPrimitive( D3DPT_LINESTRIP, 5*onlineFlag, 4);
        dev->SetStreamSource( 0, g_pVB_away_sleeve_right_outline[tt], sizeof(CUSTOMVERTEX));
        dev->DrawPrimitive( D3DPT_LINESTRIP, 5*onlineFlag, 4);

        // shirt texture
        dev->SetVertexShader( D3DFVF_CUSTOMVERTEX2 );
        // shirt-left
        dev->SetTexture(0, g_away_shirt_tex);
        dev->SetStreamSource( 0, away_shirt_left, sizeof(CUSTOMVERTEX2));
        dev->DrawPrimitive( D3DPT_TRIANGLESTRIP, 6*onlineFlag, 4);
        // shirt-right
        dev->SetStreamSource( 0, away_shirt_right, sizeof(CUSTOMVERTEX2));
        dev->DrawPrimitive( D3DPT_TRIANGLESTRIP, 6*onlineFlag, 4);
        
        // shirt outline
        dev->SetVertexShader( D3DFVF_CUSTOMVERTEX );
        dev->SetTexture(0, NULL);
        dev->SetStreamSource( 0, g_pVB_away_shirt_outline[tt], sizeof(CUSTOMVERTEX));
        dev->DrawPrimitive( D3DPT_LINESTRIP, 8*onlineFlag, 7);
    }
}

void CheckInput()
{
    DWORD* inputs = GetInputTable();
    KEYCFG* keyCfg = GetInputCfg();
    /*
    for (int i=0; i<24; i++) {
        if (inputs[i]!=0) {
            LogWithTwoNumbers(&k_mydll, "inputs[%02d] = %08x", i, inputs[i]);
        }
    }
    */

    //  simple logic for now: read input from all controllers
    //  and handle the hot keys, that kitserver has bindings for.
    for (int n=0; n<8; n++) {
        if (INPUT_EVENT(inputs, n, FUNCTIONAL, keyCfg->gamepad.keySwitchLeft)) {
            // cycle home kit
            g_shirtChange[HOME] = true;
        }
        if (INPUT_EVENT(inputs, n, FUNCTIONAL, keyCfg->gamepad.keySwitchRight)) {
            // cycle away kit
            g_shirtChange[AWAY] = true;
        }
        if (INPUT_EVENT(inputs, n, FUNCTIONAL, keyCfg->gamepad.keyAction2)) {
            // switch kit selection mode: Players/Goalkeepers
            typ = (typ + 1) % 2;
            g_modeSwitch = TRUE;

            // switch iterators
            g_homeShirtIterator = (typ==PL_TYPE) ?
                g_homeShirtIteratorPL : g_homeShirtIteratorGK;
            g_homeShortsIterator = (typ==PL_TYPE) ?
                g_homeShortsIteratorPL : g_homeShortsIteratorGK;
            g_homeSocksIterator = (typ==PL_TYPE) ?
                g_homeSocksIteratorPL : g_homeSocksIteratorGK;
            g_awayShirtIterator = (typ==PL_TYPE) ?
                g_awayShirtIteratorPL : g_awayShirtIteratorGK;
            g_awayShortsIterator = (typ==PL_TYPE) ?
                g_awayShortsIteratorPL : g_awayShortsIteratorGK;
            g_awaySocksIterator = (typ==PL_TYPE) ?
                g_awaySocksIteratorPL : g_awaySocksIteratorGK;
        }
    }
}

void Draw2Dkits(IDirect3DDevice8* dev)
{
	if (g_needsRestore) 
	{
		if (FAILED(RestoreDeviceObjects(dev)))
		{
			Log(&k_mydll,"Draw2Dkits: RestoreDeviceObjects() failed.");
            return;
		}
		Log(&k_mydll,"Draw2Dkits: RestoreDeviceObjects() done.");
        g_needsRestore = FALSE;
	}

    // load textures from GDB (if needed)
    Load2Dkits();

    BYTE onlineFlag = 1 - *(BYTE*)data[OFFLINE_FLAG];

	// render
	dev->BeginScene();

	// setup renderstate
	dev->CaptureStateBlock( g_dwSavedStateBlock );
	dev->ApplyStateBlock( g_dwDrawOverlayStateBlock );

    ////////// HOME 2DKIT /////////////////////////////
    
    dev->SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE ); 

    int tt = GetTemplateType(HOME);
    RenderHomeShirt(dev, tt, onlineFlag);

    if (g_home_shorts_tex) {
        // shorts
        dev->SetVertexShader( D3DFVF_CUSTOMVERTEX2 );
        dev->SetTexture(0, g_home_shorts_tex);
        dev->SetStreamSource( 0, g_pVB_home_shorts[tt], sizeof(CUSTOMVERTEX2));
        dev->DrawPrimitive( D3DPT_TRIANGLESTRIP, 4*onlineFlag, 2);
        dev->SetStreamSource( 0, g_pVB_home_shorts_2[tt], sizeof(CUSTOMVERTEX2));
        dev->DrawPrimitive( D3DPT_TRIANGLESTRIP, 4*onlineFlag, 2);

        // outline
        dev->SetVertexShader( D3DFVF_CUSTOMVERTEX );
        dev->SetTexture(0, NULL);
        dev->SetStreamSource( 0, g_pVB_home_shorts_outline[tt], sizeof(CUSTOMVERTEX));
        dev->DrawPrimitive( D3DPT_LINESTRIP, 5*onlineFlag, 4);
        dev->SetStreamSource( 0, g_pVB_home_shorts_2_outline[tt], sizeof(CUSTOMVERTEX));
        dev->DrawPrimitive( D3DPT_LINESTRIP, 5*onlineFlag, 4);
    }

    if (g_home_socks_tex) {
        // socks
        dev->SetVertexShader( D3DFVF_CUSTOMVERTEX2 );
        dev->SetTexture(0, g_home_socks_tex);
        dev->SetStreamSource( 0, g_pVB_home_socks_shin_left[tt], sizeof(CUSTOMVERTEX2));
        dev->DrawPrimitive( D3DPT_TRIANGLESTRIP, 4*onlineFlag, 2);
        dev->SetStreamSource( 0, g_pVB_home_socks_foot_left[tt], sizeof(CUSTOMVERTEX2));
        dev->DrawPrimitive( D3DPT_TRIANGLEFAN, 7*onlineFlag, 5);
        dev->SetStreamSource( 0, g_pVB_home_socks_shin_right[tt], sizeof(CUSTOMVERTEX2));
        dev->DrawPrimitive( D3DPT_TRIANGLESTRIP, 4*onlineFlag, 2);
        dev->SetStreamSource( 0, g_pVB_home_socks_foot_right[tt], sizeof(CUSTOMVERTEX2));
        dev->DrawPrimitive( D3DPT_TRIANGLEFAN, 7*onlineFlag, 5);

        // outline
        dev->SetVertexShader( D3DFVF_CUSTOMVERTEX );
        dev->SetTexture(0, NULL);
        dev->SetStreamSource( 0, g_pVB_home_socks_left_outline[tt], sizeof(CUSTOMVERTEX));
        dev->DrawPrimitive( D3DPT_LINESTRIP, 10*onlineFlag, 9);
        dev->SetStreamSource( 0, g_pVB_home_socks_right_outline[tt], sizeof(CUSTOMVERTEX));
        dev->DrawPrimitive( D3DPT_LINESTRIP, 10*onlineFlag, 9);
    }

    ////////// AWAY 2DKIT /////////////////////////////
    
    tt = GetTemplateType(AWAY);
    RenderAwayShirt(dev, tt, onlineFlag);

    if (g_away_shorts_tex) {
        // shorts
        dev->SetVertexShader( D3DFVF_CUSTOMVERTEX2 );
        dev->SetTexture(0, g_away_shorts_tex);
        dev->SetStreamSource( 0, g_pVB_away_shorts[tt], sizeof(CUSTOMVERTEX2));
        dev->DrawPrimitive( D3DPT_TRIANGLESTRIP, 4*onlineFlag, 2);
        dev->SetStreamSource( 0, g_pVB_away_shorts_2[tt], sizeof(CUSTOMVERTEX2));
        dev->DrawPrimitive( D3DPT_TRIANGLESTRIP, 4*onlineFlag, 2);

        // outline
        dev->SetVertexShader( D3DFVF_CUSTOMVERTEX );
        dev->SetTexture(0, NULL);
        dev->SetStreamSource( 0, g_pVB_away_shorts_outline[tt], sizeof(CUSTOMVERTEX));
        dev->DrawPrimitive( D3DPT_LINESTRIP, 5*onlineFlag, 4);
        dev->SetStreamSource( 0, g_pVB_away_shorts_2_outline[tt], sizeof(CUSTOMVERTEX));
        dev->DrawPrimitive( D3DPT_LINESTRIP, 5*onlineFlag, 4);
    }

    if (g_away_socks_tex) {
        // socks
        dev->SetVertexShader( D3DFVF_CUSTOMVERTEX2 );
        dev->SetTexture(0, g_away_socks_tex);
        dev->SetStreamSource( 0, g_pVB_away_socks_shin_left[tt], sizeof(CUSTOMVERTEX2));
        dev->DrawPrimitive( D3DPT_TRIANGLESTRIP, 4*onlineFlag, 2);
        dev->SetStreamSource( 0, g_pVB_away_socks_foot_left[tt], sizeof(CUSTOMVERTEX2));
        dev->DrawPrimitive( D3DPT_TRIANGLEFAN, 7*onlineFlag, 5);
        dev->SetStreamSource( 0, g_pVB_away_socks_shin_right[tt], sizeof(CUSTOMVERTEX2));
        dev->DrawPrimitive( D3DPT_TRIANGLESTRIP, 4*onlineFlag, 2);
        dev->SetStreamSource( 0, g_pVB_away_socks_foot_right[tt], sizeof(CUSTOMVERTEX2));
        dev->DrawPrimitive( D3DPT_TRIANGLEFAN, 7*onlineFlag, 5);

        // outline
        dev->SetVertexShader( D3DFVF_CUSTOMVERTEX );
        dev->SetTexture(0, NULL);
        dev->SetStreamSource( 0, g_pVB_away_socks_left_outline[tt], sizeof(CUSTOMVERTEX));
        dev->DrawPrimitive( D3DPT_LINESTRIP, 10*onlineFlag, 9);
        dev->SetStreamSource( 0, g_pVB_away_socks_right_outline[tt], sizeof(CUSTOMVERTEX));
        dev->DrawPrimitive( D3DPT_LINESTRIP, 10*onlineFlag, 9);
    }

    // gloves indicators
    if (typ == GK_TYPE) {
        if (!g_gloves_left_tex) {
        	char tmp[512];
        	sprintf(tmp,"%sigloves.png",GetPESInfo()->mydir);
            if (FAILED(D3DXCreateTextureFromFileEx(dev, tmp, 
                        0, 0, 1, 0, D3DFMT_UNKNOWN, D3DPOOL_MANAGED,
                        D3DX_FILTER_NONE, D3DX_FILTER_NONE,
                        0xffffffff, NULL, NULL, &g_gloves_left_tex))) {
                Log(&k_mydll,"FAILED to load image for gloves indicator.");
            }
        }
        if (g_gloves_left_tex) {
            // Set diffuse blending for alpha set in vertices.
            dev->SetRenderState( D3DRS_ALPHABLENDENABLE,   TRUE );

            // left
            dev->SetVertexShader( D3DFVF_CUSTOMVERTEX2 );
            dev->SetTexture(0, g_gloves_left_tex);
            dev->SetStreamSource( 0, g_pVB_gloves_left, sizeof(CUSTOMVERTEX2));
            dev->DrawPrimitive( D3DPT_TRIANGLESTRIP, 4*onlineFlag, 2);
        }
    }

	// restore the modified renderstates
	dev->ApplyStateBlock( g_dwSavedStateBlock );

	dev->EndScene();
}

// sanity-check for team id
BOOL GoodTeamId(WORD id)
{
	//TRACE2(&k_mydll,"GoodTeamId: checking id = %04x", id);
	return (id>=0 && id<256);
}

/*
void DrawKitLabels(IDirect3DDevice8* dev)
{
	if (g_needsRestore)
	{
		if (FAILED(RestoreDeviceObjects(dev)))
		{
			Log(&k_mydll,"DrawKitLabels: RestoreDeviceObjects() failed.");
            return;
		}
		Log(&k_mydll,"DrawKitLabels: RestoreDeviceObjects() done.");
        g_needsRestore = FALSE;
	}

	//TRACE(&k_mydll,"DrawKitLabels: About to draw text.");
	char buf[255];
	SIZE size;
	DWORD color;

	WORD id = 0xffff;
	WORD kitId = 0xffff;
	BYTE* strips = (BYTE*)data[TEAM_STRIPS];

	// HOME PLAYER
	ZeroMemory(buf, 255);
	lstrcpy(buf, "PL: ");
	id = g_currTeams[0];
	// only show the label, if extra kits are available, and the kit is not edited
	if (GoodTeamId(id) && !IsEditedKit(1) && kDB->players[id].extra != NULL)
	{
		kitId = id * 5 + 2 + (strips[0] & 0x01);
		if (g_kitExtras[kitId] == NULL)
		{
			lstrcat(buf, DEFAULT_LABEL[strips[0] & 0x01]);
			color = 0xffa0a0a0; // light grey - for default
		}
		else 
		{
			lstrcat(buf, g_kitExtras[kitId]->kit->fileName);
			color = 0xff4488ff; // blue;
		}
		//g_font->GetTextExtent(buf, &size);
		//g_font->DrawText( g_bbWidth/2 - 15 - size.cx,  POSY + IHEIGHT + 35, color, buf);
	}

	// AWAY PLAYER
	ZeroMemory(buf, 255);
	lstrcpy(buf, "PL: ");
	id = g_currTeams[1];
	// only show the label, if extra kits are available, and the kit is not edited
	if (GoodTeamId(id) && !IsEditedKit(3) && kDB->players[id].extra != NULL)
	{
		kitId = id * 5 + 2 + (strips[1] & 0x01);
		if (g_kitExtras[kitId] == NULL)
		{
			lstrcat(buf, DEFAULT_LABEL[strips[1] & 0x01]);
			color = 0xffa0a0a0; // light grey - for default
		}
		else 
		{
			lstrcat(buf, g_kitExtras[kitId]->kit->fileName);
			color = 0xff4488ff; // blue;
		}
		//g_font->GetTextExtent(buf, &size);
		//g_font->DrawText( g_bbWidth/2 + 15,  POSY + IHEIGHT + 35, color, buf);
	}

	// HOME GOALKEEPER
	ZeroMemory(buf, 255);
	lstrcpy(buf, "GK: ");
	id = g_currTeams[0];
	kitId = id * 5;
	// only show the label
	if (GoodTeamId(id))
	{
		if (g_kitExtras[kitId] == NULL)
		{
			lstrcat(buf, DEFAULT_LABEL[2]);
			color = 0xffa0a0a0; // light grey - for default
		}
		else 
		{
			lstrcat(buf, g_kitExtras[kitId]->kit->fileName);
			color = 0xff4488ff; // blue;
		}
		//g_font->GetTextExtent(buf, &size);
		//g_font->DrawText( g_bbWidth/2 - 15 - size.cx,  POSY + IHEIGHT + 50, color, buf);
	}

	// AWAY GOALKEEPER
	ZeroMemory(buf, 255);
	lstrcpy(buf, "GK: ");
	id = g_currTeams[1];
	kitId = id * 5;
	// only show the label
	if (GoodTeamId(id))
	{
		if (g_kitExtras[kitId] == NULL)
		{
			lstrcat(buf, DEFAULT_LABEL[2]);
			color = 0xffa0a0a0; // light grey - for default
		}
		else 
		{
			lstrcat(buf, g_kitExtras[kitId]->kit->fileName);
			color = 0xff4488ff; // blue;
		}
		//g_font->GetTextExtent(buf, &size);
		//g_font->DrawText( g_bbWidth/2 + 15,  POSY + IHEIGHT + 50, color, buf);
	}

	//Log(&k_mydll,"DrawKitLabels: Text drawn.");
}
*/

/* New Reset function */
void JuceReset(IDirect3DDevice8* self, LPVOID params)
{
	Log(&k_mydll,"JuceReset: cleaning-up.");

	InvalidateDeviceObjects(self);
	DeleteDeviceObjects(self);

	g_bGotFormat = false;
    g_needsRestore = TRUE;

	return;
}

/**
 * Determine format of back buffer and its dimensions.
 */
void GetBackBufferInfo(IDirect3DDevice8* d3dDevice)
{
	TRACE(&k_mydll,"GetBackBufferInfo: called.");

	// get the 0th backbuffer.
	if (SUCCEEDED(d3dDevice->GetBackBuffer(0, D3DBACKBUFFER_TYPE_MONO, &g_backBuffer)))
	{
		D3DSURFACE_DESC desc;
		g_backBuffer->GetDesc(&desc);
		g_bbFormat = desc.Format;
		g_bbWidth = desc.Width;
		g_bbHeight = desc.Height;

		// vertical offset from bottom for uniform labels
		g_labelsYShift = (UINT)(g_bbHeight * 0.185);

		// adjust indicator coords
		float OLDPOSX = POSX;
		float OLDPOSY = POSY;
		POSX = g_bbWidth/2 - INDX - IWIDTH/2;
		POSY = g_bbHeight/2 - INDY - IHEIGHT/2 - 20;
		for (int k=0; k<9; k++)
		{
			g_Vert0[k].x += POSX - OLDPOSX;
			g_Vert0[k].y += POSY - OLDPOSY;
		}

		// check dest.surface format
		bpp = 0;
		if (g_bbFormat == D3DFMT_R8G8B8) bpp = 3;
		else if (g_bbFormat == D3DFMT_R5G6B5 || g_bbFormat == D3DFMT_X1R5G5B5) bpp = 2;
		else if (g_bbFormat == D3DFMT_A8R8G8B8 || g_bbFormat == D3DFMT_X8R8G8B8) bpp = 4;

		// release backbuffer
		g_backBuffer->Release();

		Log(&k_mydll,"GetBackBufferInfo: got new back buffer format and info.");
		g_bGotFormat = true;
	}
}

/** 
 * Finds slot for given kitId.
 * Returns -1, if slot not found.
 */
int GetKitSlot(int kitId)
{
	int slot = -1;
	for (int i=0; i<4; i++) 
	{
		WORD id = *((WORD*)(data[KIT_SLOTS] + i*0x38 + 0x0a));
		if (kitId == id) { slot = i; break; }
	}
	return slot;
}

/* New Present function */
void JucePresent(IDirect3DDevice8* self, CONST RECT* src, CONST RECT* dest, HWND hWnd, LPVOID unused)
{
	// determine backbuffer's format and dimensions, if not done yet.
	if (!g_bGotFormat) {
		GetBackBufferInfo(self);
	}

	// install keyboard hook, if not done yet.
	if (g_hKeyboardHook == NULL)
	{
		g_hKeyboardHook = SetWindowsHookEx(WH_KEYBOARD, KeyboardProc, hInst, GetCurrentThreadId());
		LogWithNumber(&k_mydll,"Installed keyboard hook: g_hKeyboardHook = %d", (DWORD)g_hKeyboardHook);
	}

    ////// 2D-kits //////////////////////////

    if (g_display2Dkits) Draw2Dkits(self);
    g_lastDisplay2Dkits = g_display2Dkits;

	/*
	// draw the uniform labels
	if (bIndicate) DrawKitLabels(self);

	*/

	/*
	if (g_triggerLoad3rdKit > 0)
	{
		int kitId = 0, slot = -1;
		BYTE* strips = (BYTE*)data[TEAM_STRIPS];

		switch (g_triggerLoad3rdKit)
		{
			case 1:
				g_triggerLoad3rdKit = 0;
				// clear kit slot to force uni-file reload
				kitId = (strips[0] & 0x01) ? g_currTeams[0] * 5 + 3 : g_currTeams[0] * 5 + 2;
				slot = GetKitSlot(kitId);
				if (slot != -1)
				{
					ZeroMemory((BYTE*)data[KIT_SLOTS] + slot * 0x38, 0x38);
					// set uni-reload flag
					*((DWORD*)data[KIT_CHECK_TRIGGER]) = 1;
				}
				break;

			case 2:
				g_triggerLoad3rdKit = 0;
				// clear kit info to force uni-file reload
				kitId = (strips[1] & 0x01) ? g_currTeams[1] * 5 + 3 : g_currTeams[1] * 5 + 2;
				slot = GetKitSlot(kitId);
				if (slot != -1)
				{
					ZeroMemory((BYTE*)data[KIT_SLOTS] + slot * 0x38, 0x38);
					// set uni-reload flag
					*((DWORD*)data[KIT_CHECK_TRIGGER]) = 1;
				}
				break;
		}
	}
	*/

	// reset texture count
	g_frame_tex_count = 0;
	g_frame_count++;

	/*
	if (g_dumpTexturesMode && g_frame_count >= 2) {
		g_dumpTexturesMode = FALSE;
		g_frame_count = 0;
	}
	*/

	//Log(&k_mydll,"=========================frame============================");
	return;
}

/* CopyRects tracker. */
HRESULT STDMETHODCALLTYPE JuceCopyRects(IDirect3DDevice8* self,
IDirect3DSurface8* pSourceSurface, CONST RECT* pSourceRectsArray, UINT cRects,
IDirect3DSurface8* pDestinationSurface, CONST POINT* pDestPointsArray)
{
	TRACEX(&k_mydll,"JuceCopyRects: rect(%dx%d), numRects = %d", pSourceRectsArray[0].right,
			pSourceRectsArray[0].bottom, cRects);
	return g_orgCopyRects(self, pSourceSurface, pSourceRectsArray, cRects,
			pDestinationSurface, pDestPointsArray);
}

/* ApplyStateBlock tracker. */
HRESULT STDMETHODCALLTYPE JuceApplyStateBlock(IDirect3DDevice8* self, DWORD token)
{
	TRACE2(&k_mydll,"JuceApplyStateBlock: token = %d", token);
	return g_orgApplyStateBlock(self, token);
}

/* BeginScene tracker. */
HRESULT STDMETHODCALLTYPE JuceBeginScene(IDirect3DDevice8* self)
{
	TRACE(&k_mydll,"JuceBeginScene: CALLED.");
	return g_orgBeginScene(self);
}

/* EndScene tracker. */
HRESULT STDMETHODCALLTYPE JuceEndScene(IDirect3DDevice8* self)
{
	TRACE(&k_mydll,"JuceEndScene: CALLED.");
	return g_orgEndScene(self);
}

/* Restore original Reset() and Present() */
EXTERN_C _declspec(dllexport) void RestoreDeviceMethods()
{
	if (!GetActiveDevice()) return;
	try
	{
        DWORD* vtable = (DWORD*)(*((DWORD*)GetActiveDevice()));
        TRACE2(&k_mydll,"RestoreDeviceMethods: vtable = %08x", (DWORD)vtable);

		UnhookFunction(hk_D3D_Reset,(DWORD)JuceReset);
		TRACE(&k_mydll,"RestoreDeviceMethods: Reset restored.");
		
		UnhookFunction(hk_D3D_Present,(DWORD)JucePresent);
		TRACE(&k_mydll,"RestoreDeviceMethods: Present restored.");
		
		if (g_orgCreateTexture != NULL) vtable[20] = (DWORD)g_orgCreateTexture;
		TRACE(&k_mydll,"RestoreDeviceMethods: CreateTexture restored.");
	}
	catch (...) {} // ignore exceptions at this point

	Log(&k_mydll,"RestoreDeviceMethods: done.");
}

void LoadKitMasks()
{
	char filename[BUFLEN];
	ZeroMemory(filename, BUFLEN);
	lstrcpy(filename, GetPESInfo()->gdbDir);
	lstrcat(filename, "GDB\\uni\\masks\\shirt.png");
	if (LoadPNGTexture(&g_shirtMaskTex, filename) > 0) {
		BITMAPINFOHEADER* bih = (BITMAPINFOHEADER*)g_shirtMaskTex;
		g_shirtMask = (DWORD*)((BYTE*)g_shirtMaskTex + bih->biSize + 0x400);
	}
	ZeroMemory(filename, BUFLEN);
	lstrcpy(filename, GetPESInfo()->gdbDir);
	lstrcat(filename, "GDB\\uni\\masks\\shirt-mip1.png");
	if (LoadPNGTexture(&g_shirtMaskTexMip1, filename) > 0) {
		BITMAPINFOHEADER* bih = (BITMAPINFOHEADER*)g_shirtMaskTexMip1;
		g_shirtMaskMip1 = (DWORD*)((BYTE*)g_shirtMaskTexMip1 + bih->biSize + 0x400);
	}
	ZeroMemory(filename, BUFLEN);
	lstrcpy(filename, GetPESInfo()->gdbDir);
	lstrcat(filename, "GDB\\uni\\masks\\shirt-mip2.png");
	if (LoadPNGTexture(&g_shirtMaskTexMip2, filename) > 0) {
		BITMAPINFOHEADER* bih = (BITMAPINFOHEADER*)g_shirtMaskTexMip2;
		g_shirtMaskMip2 = (DWORD*)((BYTE*)g_shirtMaskTexMip2 + bih->biSize + 0x400);
	}


	ZeroMemory(filename, BUFLEN);
	lstrcpy(filename, GetPESInfo()->gdbDir);
	lstrcat(filename, "GDB\\uni\\masks\\shorts.png");
	if (LoadPNGTexture(&g_shortsMaskTex, filename) > 0) {
		BITMAPINFOHEADER* bih = (BITMAPINFOHEADER*)g_shortsMaskTex;
		g_shortsMask = (DWORD*)((BYTE*)g_shortsMaskTex + bih->biSize + 0x400);
	}
	ZeroMemory(filename, BUFLEN);
	lstrcpy(filename, GetPESInfo()->gdbDir);
	lstrcat(filename, "GDB\\uni\\masks\\shorts-mip1.png");
	if (LoadPNGTexture(&g_shortsMaskTexMip1, filename) > 0) {
		BITMAPINFOHEADER* bih = (BITMAPINFOHEADER*)g_shortsMaskTexMip1;
		g_shortsMaskMip1 = (DWORD*)((BYTE*)g_shortsMaskTexMip1 + bih->biSize + 0x400);
	}
	ZeroMemory(filename, BUFLEN);
	lstrcpy(filename, GetPESInfo()->gdbDir);
	lstrcat(filename, "GDB\\uni\\masks\\shorts-mip2.png");
	if (LoadPNGTexture(&g_shortsMaskTexMip2, filename) > 0) {
		BITMAPINFOHEADER* bih = (BITMAPINFOHEADER*)g_shortsMaskTexMip2;
		g_shortsMaskMip2 = (DWORD*)((BYTE*)g_shortsMaskTexMip2 + bih->biSize + 0x400);
	}

	ZeroMemory(filename, BUFLEN);
	lstrcpy(filename, GetPESInfo()->gdbDir);
	lstrcat(filename, "GDB\\uni\\masks\\socks.png");
	if (LoadPNGTexture(&g_socksMaskTex, filename) > 0) {
		BITMAPINFOHEADER* bih = (BITMAPINFOHEADER*)g_socksMaskTex;
		g_socksMask = (DWORD*)((BYTE*)g_socksMaskTex + bih->biSize + 0x400);
	}
	ZeroMemory(filename, BUFLEN);
	lstrcpy(filename, GetPESInfo()->gdbDir);
	lstrcat(filename, "GDB\\uni\\masks\\socks-mip1.png");
	if (LoadPNGTexture(&g_socksMaskTexMip1, filename) > 0) {
		BITMAPINFOHEADER* bih = (BITMAPINFOHEADER*)g_socksMaskTexMip1;
		g_socksMaskMip1 = (DWORD*)((BYTE*)g_socksMaskTexMip1 + bih->biSize + 0x400);
	}
	ZeroMemory(filename, BUFLEN);
	lstrcpy(filename, GetPESInfo()->gdbDir);
	lstrcat(filename, "GDB\\uni\\masks\\socks-mip2.png");
	if (LoadPNGTexture(&g_socksMaskTexMip2, filename) > 0) {
		BITMAPINFOHEADER* bih = (BITMAPINFOHEADER*)g_socksMaskTexMip2;
		g_socksMaskMip2 = (DWORD*)((BYTE*)g_socksMaskTexMip2 + bih->biSize + 0x400);
	}

	ZeroMemory(filename, BUFLEN);
	lstrcpy(filename, GetPESInfo()->gdbDir);
	lstrcat(filename, "GDB\\uni\\masks\\alltest.png");
	if (LoadPNGTexture(&g_testMaskTex, filename) > 0) {
		BITMAPINFOHEADER* bih = (BITMAPINFOHEADER*)g_testMaskTex;
		g_testMask = (DWORD*)((BYTE*)g_testMaskTex + bih->biSize + 0x400);
	}
}

void UnloadKitMasks()
{
	if (g_shirtMaskTex != NULL) {
        pngdib_p2d_free_dib(NULL, (BITMAPINFOHEADER*)g_shirtMaskTex);
	}
	if (g_shirtMaskTexMip1 != NULL) {
        pngdib_p2d_free_dib(NULL, (BITMAPINFOHEADER*)g_shirtMaskTexMip1);
	}
	if (g_shirtMaskTexMip2 != NULL) {
        pngdib_p2d_free_dib(NULL, (BITMAPINFOHEADER*)g_shirtMaskTexMip2);
	}
	if (g_shortsMaskTex != NULL) {
        pngdib_p2d_free_dib(NULL, (BITMAPINFOHEADER*)g_shortsMaskTex);
	}
	if (g_shortsMaskTexMip1 != NULL) {
        pngdib_p2d_free_dib(NULL, (BITMAPINFOHEADER*)g_shortsMaskTexMip1);
	}
	if (g_shortsMaskTexMip2 != NULL) {
        pngdib_p2d_free_dib(NULL, (BITMAPINFOHEADER*)g_shortsMaskTexMip2);
	}
	if (g_socksMaskTex != NULL) {
        pngdib_p2d_free_dib(NULL, (BITMAPINFOHEADER*)g_socksMaskTex);
	}
	if (g_socksMaskTexMip1 != NULL) {
        pngdib_p2d_free_dib(NULL, (BITMAPINFOHEADER*)g_socksMaskTexMip1);
	}
	if (g_socksMaskTexMip2 != NULL) {
        pngdib_p2d_free_dib(NULL, (BITMAPINFOHEADER*)g_socksMaskTexMip2);
	}
	if (g_testMaskTex != NULL) {
        pngdib_p2d_free_dib(NULL, (BITMAPINFOHEADER*)g_testMaskTex);
	}
}

void BeginDrawKitLabel()
{
	dksiSetMenuTitle("Kit selection");
	return;
};

void EndDrawKitLabel()
{
	return;
};

void DrawKitLabel()
{
    // draw labels
	SIZE size;
	DWORD color = 0xffffffff; // white
    char display[30] = {0};
    if (typ == PL_TYPE) {
        strcpy(display, "Kits: players");
    } else {
        strcpy(display, "Kits: goalkeepers");
    }
	
	UINT g_bbWidth=GetPESInfo()->bbWidth;
	UINT g_bbHeight=GetPESInfo()->bbHeight;
	double stretchX=GetPESInfo()->stretchX;
	double stretchY=GetPESInfo()->stretchY;
	
	KGetTextExtent(display,12,&size);
	//draw shadow
    KDrawText((g_bbWidth-size.cx)/2+3*stretchX,g_bbHeight*0.77+3*stretchY,0xff000000,12,display,true);
	//print label
	KDrawText((g_bbWidth-size.cx)/2,g_bbHeight*0.77,color,12,display,true);

    // print kit keys
    char buf[256] = {0};
    if (g_home_shirt_tex) {
        string& key1 = GET_HOME_SHIRT_KEY(typ);
        string key2 = (key1.length()>0) ? key1 : (typ==PL_TYPE)?"pa":"ga";
        // check for description attribute
        gdbFindKitsForTeam(gdb, GetTeamId(HOME));
        KitCollection* col = MAP_FIND(gdb->uni,GetTeamId(HOME)); 
        if (col) {
            Kit* kit = (typ==PL_TYPE)?MAP_FIND(col->players,key2):MAP_FIND(col->goalkeepers,key2);
            if (kit && (kit->attDefined & KITDESCRIPTION)) {
                key2 = string(kit->description);
                sprintf(buf, "%s", key2.c_str());
                KDrawText(26,638,0xff000000,12,buf);
                KDrawText(24,636,color,12,buf);
            }
        }
    }
    if (g_away_shirt_tex) {
        string& key3 = GET_AWAY_SHIRT_KEY(typ);
        string key4 = (key3.length()>0) ? key3 : (typ==PL_TYPE)?"pb":"gb";
        // check for description attribute
        gdbFindKitsForTeam(gdb, GetTeamId(AWAY));
        KitCollection* col = MAP_FIND(gdb->uni,GetTeamId(AWAY)); 
        if (col) {
            Kit* kit = (typ==PL_TYPE)?MAP_FIND(col->players,key4):MAP_FIND(col->goalkeepers,key4);
            if (kit && (kit->attDefined & KITDESCRIPTION)) {
                key4 = string(kit->description);
                sprintf(buf, "%s", key4.c_str());
                KGetTextExtent(buf,12,&size);
                KDrawText((g_bbWidth-size.cx)/stretchX-24,638,0xff000000,12,buf);
                KDrawText((g_bbWidth-size.cx)/stretchX-26,636,color,12,buf);
            }
        }
    }

    // check input
    CheckInput();

	return;
};

/************
 * This function initializes kitserver.
 ************/
void InitKserv()
{
	ZeroMemory(g_teamDir, sizeof(g_teamDir)*sizeof(BYTE));

    // Determine the game version
    int v = GetPESInfo()->GameVersion;
    if (v != -1)
    {
        memcpy(code, codeArray[v], sizeof(code));
        memcpy(data, dataArray[v], sizeof(data));

		// mark the files we need to replace
		for (int i=data[FIRST_ID]; i <= data[LAST_ID]; i++) {
    		g_AFS_idMap[i] = true;
		}

        // Load GDB database
        gdb = gdbLoad(GetPESInfo()->gdbDir);
        if (!gdb) {
            Log(&k_mydll,"Unable to load GDB.");
            return;
        }
        LogWithTwoNumbers(&k_mydll,"GDB loaded: gdb = %08x, teams: %d", (DWORD)gdb, (*(gdb->uni)).size());
        //LogWithString(&k_mydll,"77: %s", MAP_FIND(gdb->uni,77)->foldername);
        //LogWithString(&k_mydll,"88: %s", MAP_FIND(gdb->uni,88)->foldername);
        //LogWithNumber(&k_mydll,"99: %08x", (DWORD)MAP_FIND(gdb->uni,99));
        
        // load kit masks
        LoadKitMasks();

        // hook BeginUniSelect
		HookFunction(hk_BeginUniSelect,(DWORD)JuceSet2Dkits);
		HookFunction(hk_D3D_CreateTexture,(DWORD)JuceCreateTexture);
		HookFunction(hk_D3D_UnlockRect,(DWORD)kservUnlockRect);

        //LogWithNumber(&k_mydll,"JuceCreateTexture: func=%08x", (DWORD)JuceCreateTexture);

        // hook EndUniSelect
		HookFunction(hk_EndUniSelect,(DWORD)JuceClear2Dkits);

        // hook UniDecode
		RegisterUniDecodeCallback(kservUniDecode);
		
		// hook replacing functions
		RegisterAfsReplaceCallback(kservAfsReplace, kservUnpack, NULL);

        /*
		// hook GetNationalTeamInfo
		HookFunction(hk_GetNationalTeamInfo,(DWORD)JuceGetNationalTeamInfo);

		// hook GetClubTeamInfo
		HookFunction(hk_GetClubTeamInfo,(DWORD)JuceGetClubTeamInfo);
		HookFunction(hk_GetClubTeamInfoML1,(DWORD)JuceGetClubTeamInfoML1);
		HookFunction(hk_GetClubTeamInfoML2,(DWORD)JuceGetClubTeamInfoML2);
		
		// hook GetNationalTeamInfoExitEdit
		HookFunction(hk_GetNationalTeamInfoExitEdit,(DWORD)JuceGetNationalTeamInfoExitEdit);
        */
		
        // hook edit mode entrance
        MasterHookFunction(code[C_GETTEAMINFO_CS], 2, JuceGetTeamInfo);

        MasterHookFunction(code[C_READTEAMINFO_CS], 2, JuceReadTeamInfoOnReplaySave);
        MasterHookFunction(code[C_READTEAMINFO2_CS], 1, JuceReadTeamInfoOnReplaySave2);
        MasterHookFunction(code[C_READTEAMINFO_MLSAVE_CS], 0, JuceReadTeamInfoOnMLSave);
        MasterHookFunction(code[C_ONREPLAYLOAD_CS], 0, JuceOnReplayLoad);

        MasterHookFunction(code[C_SETFLAG_CS], 0, kservSetFlag);
        MasterHookFunction(code[C_RESETFLAG_CS], 0, kservResetFlag);
        MasterHookFunction(code[C_RESETFLAG2_CS], 0, kservResetFlag2);
        MasterHookFunction(code[C_WRITEKITINFO_CS], 2, kservWriteKitInfo);
        MasterHookFunction(code[C_WRITEKITINFO_CS2], 2, kservWriteKitInfo);
        MasterHookFunction(code[C_WRITEKITINFO_CS3], 2, kservWriteKitInfo);
        MasterHookFunction(code[C_WRITEKITINFO_CS4], 2, kservWriteKitInfo);

        MasterHookFunction(code[C_PROCESSKIT_CS], 2, kservProcessKit);
        MasterHookFunction(code[C_SETRADARCOLOR_CS], 0, kservSetRadarColor);

        // create font instance
        //g_font = new CD3DFont( _T("Arial"), 10, D3DFONT_BOLD);
        //TRACE2(&k_mydll,"g_font = %08x" , (DWORD)g_font);
    }
    else
    {
        Log(&k_mydll,"Game version not recognized. Nothing is hooked");
    }
}

/*******************/
/* DLL Entry Point */
/*******************/
EXTERN_C BOOL WINAPI DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID lpReserved)
{
	DWORD wbytes, procId; 

	if (dwReason == DLL_PROCESS_ATTACH)
	{
		hInst = hInstance;
        InitializeCriticalSection(&g_cs);

        //check game version
		switch (GetPESInfo()->GameVersion) {
            case gvPES6PC: //support for PES6 PC
            case gvPES6PC110: //support for PES6 PC 1.10
            case gvWE2007PC: //support for WE:PES 2007 PC
				break;
            default:
                Log(&k_mydll,"Your game version is currently not supported!");
                return false;
		}
 
        RegisterKModule(&k_mydll);

		//texs.clear();

		// read configuration
		char cfgFile[BUFLEN];
		ZeroMemory(cfgFile, BUFLEN);
		lstrcpy(cfgFile, GetPESInfo()->mydir); 
		lstrcat(cfgFile, CONFIG_FILE);

		ReadConfig(&g_config, cfgFile);

		LogWithNumber(&k_mydll,"k_mydll.debug = %d", k_mydll.debug);
		
		HookFunction(hk_D3D_Create,(DWORD)InitKserv);
		HookFunction(hk_D3D_Reset,(DWORD)JuceReset);

	    HookFunction(hk_DrawKitSelectInfo,(DWORD)DrawKitLabel);
	    HookFunction(hk_OnShowMenu,(DWORD)BeginDrawKitLabel);
	    HookFunction(hk_OnHideMenu,(DWORD)EndDrawKitLabel);
	}

	else if (dwReason == DLL_PROCESS_DETACH)
	{
		Log(&k_mydll,"DLL detaching...");

        // unhook BeginUniSelect
		UnhookFunction(hk_BeginUniSelect,(DWORD)JuceSet2Dkits);

        // unhook EndUniSelect
		UnhookFunction(hk_EndUniSelect,(DWORD)JuceClear2Dkits);

        /*
		// unhook GetNationalTeamInfo
		UnhookFunction(hk_GetNationalTeamInfo,(DWORD)JuceGetNationalTeamInfo);

		// unhook GetClubTeamInfo
		UnhookFunction(hk_GetClubTeamInfo,(DWORD)JuceGetClubTeamInfo);
		UnhookFunction(hk_GetClubTeamInfoML1,(DWORD)JuceGetClubTeamInfoML1);
		UnhookFunction(hk_GetClubTeamInfoML2,(DWORD)JuceGetClubTeamInfoML2);
		
		// unhook GetNationalTeamInfoExitEdit
		UnhookFunction(hk_GetNationalTeamInfoExitEdit,(DWORD)JuceGetNationalTeamInfoExitEdit);
        */

		/* uninstall keyboard hook */
		UninstallKeyboardHook();

        UnhookFunction(hk_D3D_CreateTexture, (DWORD)JuceCreateTexture);
        UnhookFunction(hk_D3D_UnlockRect, (DWORD)kservUnlockRect);

		/* restore original pointers */
		RestoreDeviceMethods();

		Log(&k_mydll,"Device methods restored.");

		InvalidateDeviceObjects(GetActiveDevice());
		Log(&k_mydll,"InvalideDeviceObjects done.");
		DeleteDeviceObjects(GetActiveDevice());
		Log(&k_mydll,"DeleteDeviceObjects done.");

		// unload kit masks
		UnloadKitMasks();
		Log(&k_mydll,"Kit masks unloaded.");

		// unload GDB
		TRACE(&k_mydll,"Unloading GDB...");
		gdbUnload(gdb);
		Log(&k_mydll,"GDB unloaded.");

		//SAFE_DELETE( g_font );
		//TRACE(&k_mydll,"g_font SAFE_DELETEd.");

        DeleteCriticalSection(&g_cs);
	}

	return TRUE;    /* ok */
}

// helper function
// returns TRUE if specified slot contains edited kit.
BOOL IsEditedKit(int slot) 
{
	KitSlot* kitSlot = (KitSlot*)(data[ANOTHER_KIT_SLOTS] + slot*sizeof(KitSlot));
	return kitSlot->isEdited;
}

void AdjustTextureDimensions(UINT& texWidth, UINT& texHeight)
{
    texWidth = (texWidth>1024)?1024:texWidth;
    texHeight = (texHeight>1024)?1024:texHeight;
}

HRESULT JuceCreateTextureFromFile(IDirect3DDevice8* dev, char* name, IDirect3DTexture8** ppTex, PALETTEENTRY* pPal)
{
    // initialize the return value to NULL
    if (ppTex) {
        *ppTex = NULL;
    }

    return D3DXCreateTextureFromFile(dev, name, ppTex);
 
    /*
    D3DXIMAGE_INFO imageInfo;
    D3DXGetImageInfoFromFile(name, &imageInfo);
    if (imageInfo.Width == 0 && imageInfo.Height == 0) {
        // no image
        return E_FAIL;
    }

    UINT texWidth = imageInfo.Width;
    UINT texHeight = imageInfo.Height;
    UINT texLevels = 1;
    D3DFORMAT texFormat = imageInfo.Format;

    // make sure textures are not too large
    AdjustTextureDimensions(texWidth, texHeight);
    // check if the video card supports it
    LogWithTwoNumbers(&k_mydll, 
            "JuceCreateTextureFromFile: checking texture dimenstions (%d,%d)",
            texWidth, texHeight);
    D3DXCheckTextureRequirements(
            dev, &texWidth, &texHeight,
            &texLevels, 0, 
            &texFormat, D3DPOOL_MANAGED
    );
    LogWithTwoNumbers(&k_mydll, 
            "JuceCreateTextureFromFile: check complete. Trying to use (%d,%d)",
            texWidth, texHeight);

    // make very unassuming call - to avoid problems
    // that users with older GeForce cards are having
    return D3DXCreateTextureFromFileEx(
            dev, name, texWidth, texHeight, 1, 0, D3DFMT_UNKNOWN, 
            D3DPOOL_MANAGED, D3DX_DEFAULT, D3DX_DEFAULT, 0, NULL, NULL, ppTex);
    */
}

void CreateGDBTextureFromFile(char* filename, IDirect3DTexture8** ppTex, PALETTEENTRY* pPal)
{
	char name[BUFLEN];
	ZeroMemory(name, BUFLEN);
	sprintf(name, "%sGDB\\%s", GetPESInfo()->gdbDir, filename);
	LogWithString(&k_mydll,"CreateGDBTextureFromFile: Loading texture: %s", filename);
	if (SUCCEEDED(JuceCreateTextureFromFile(GetActiveDevice(), name, ppTex, pPal))) {
		Log(&k_mydll,"CreateGDBTextureFromFile: JuceCreateTextureFromFile succeeded.");
		//D3DXSaveTextureToFile("G:\\texs\\test.bmp", D3DXIFF_BMP, g_home_kit_tex, NULL);
	} else {
		Log(&k_mydll,"CreateGDBTextureFromFile: JuceCreateTextureFromFile failed.");
        *ppTex = NULL;
	}
}

void CreateGDBTextureFromFolder(char* foldername, char* filename, IDirect3DTexture8** ppTex, PALETTEENTRY* pPal)
{
    MAKE_BUFFER(name);
    string altFilename(filename);
    altFilename.replace(altFilename.size()-4,4,".bmp");

    vector<string> namesToTry;
    namesToTry.push_back(string(filename));
    namesToTry.push_back("all.png");
    namesToTry.push_back(altFilename);
    namesToTry.push_back("all.bmp");
    
    for (vector<string>::iterator vit=namesToTry.begin(); vit!=namesToTry.end(); vit++) {
        sprintf(name, "%sGDB\\%s\\%s", GetPESInfo()->gdbDir, foldername, vit->c_str());
        LogWithString(&k_mydll,"CreateGDBTextureFromFolder: Loading texture: %s", name);
        if (SUCCEEDED(JuceCreateTextureFromFile(GetActiveDevice(), name, ppTex, pPal))) {
            Log(&k_mydll,"CreateGDBTextureFromFolder: SUCCEEDED.");
            return;
        }
    }
    Log(&k_mydll,"CreateGDBTextureFromFolder: FAILED.");
    *ppTex = NULL;
}

string GetNextHomeShirtKey(WORD teamId, string currKey)
{
	gdbFindKitsForTeam(gdb, teamId);
    KitCollection* col = MAP_FIND(gdb->uni,teamId);
    if (col) {
        switch (typ) {
            case GK_TYPE:
                if ((*(col->goalkeepers)).end() == (*(col->goalkeepers)).begin()) {
                    return "(null)";
                } else {
                    g_homeShirtIterator++;
                    if (g_homeShirtIterator == (*(col->goalkeepers)).end()) {
                        g_homeShirtIterator = (*(col->goalkeepers)).begin();
                    }
                    string nextKey = g_homeShirtIterator->first;
                    if (currKey == nextKey) {
                        // same key -> skip to next
                        g_homeShirtIterator++;
                        if (g_homeShirtIterator == (*(col->goalkeepers)).end()) {
                            g_homeShirtIterator = (*(col->goalkeepers)).begin();
                        }
                        nextKey = g_homeShirtIterator->first;
                    }
                    return nextKey;
                }
            case PL_TYPE:
                if ((*(col->players)).end() == (*(col->players)).begin()) {
                    return "(null)";
                } else {
                    g_homeShirtIterator++;
                    if (g_homeShirtIterator == (*(col->players)).end()) {
                        g_homeShirtIterator = (*(col->players)).begin();
                    }
                    string nextKey = g_homeShirtIterator->first;
                    if (currKey == nextKey) {
                        // same key -> skip to next
                        g_homeShirtIterator++;
                        if (g_homeShirtIterator == (*(col->players)).end()) {
                            g_homeShirtIterator = (*(col->players)).begin();
                        }
                        nextKey = g_homeShirtIterator->first;
                    }
                    return nextKey;
                }
        }
    }
    return "(null)";
}

string GetNextHomeShortsKey(WORD teamId, string currKey)
{
	gdbFindKitsForTeam(gdb, teamId);
    KitCollection* col = MAP_FIND(gdb->uni,teamId);
    if (col) {
        switch (typ) {
            case GK_TYPE:
                if ((*(col->goalkeepers)).end() == (*(col->goalkeepers)).begin()) {
                    return "(null)";
                } else {
                    g_homeShortsIterator++;
                    if (g_homeShortsIterator == (*(col->goalkeepers)).end()) {
                        g_homeShortsIterator = (*(col->goalkeepers)).begin();
                    }
                    string nextKey = g_homeShortsIterator->first;
                    if (currKey == nextKey) {
                        // same key -> skip to next
                        g_homeShortsIterator++;
                        if (g_homeShortsIterator == (*(col->goalkeepers)).end()) {
                            g_homeShortsIterator = (*(col->goalkeepers)).begin();
                        }
                        nextKey = g_homeShortsIterator->first;
                    }
                    return nextKey;
                }
            case PL_TYPE:
                if ((*(col->players)).end() == (*(col->players)).begin()) {
                    return "(null)";
                } else {
                    g_homeShortsIterator++;
                    if (g_homeShortsIterator == (*(col->players)).end()) {
                        g_homeShortsIterator = (*(col->players)).begin();
                    }
                    string nextKey = g_homeShortsIterator->first;
                    if (currKey == nextKey) {
                        // same key -> skip to next
                        g_homeShortsIterator++;
                        if (g_homeShortsIterator == (*(col->players)).end()) {
                            g_homeShortsIterator = (*(col->players)).begin();
                        }
                        nextKey = g_homeShortsIterator->first;
                    }
                    return nextKey;
                }
        }
    }
    return "(null)";
}

string GetNextHomeSocksKey(WORD teamId, string currKey)
{
	gdbFindKitsForTeam(gdb, teamId);
    KitCollection* col = MAP_FIND(gdb->uni,teamId);
    if (col) {
        switch (typ) {
            case GK_TYPE:
                if ((*(col->goalkeepers)).end() == (*(col->goalkeepers)).begin()) {
                    return "(null)";
                } else {
                    g_homeSocksIterator++;
                    if (g_homeSocksIterator == (*(col->goalkeepers)).end()) {
                        g_homeSocksIterator = (*(col->goalkeepers)).begin();
                    }
                    string nextKey = g_homeSocksIterator->first;
                    if (currKey == nextKey) {
                        // same key -> skip to next
                        g_homeSocksIterator++;
                        if (g_homeSocksIterator == (*(col->goalkeepers)).end()) {
                            g_homeSocksIterator = (*(col->goalkeepers)).begin();
                        }
                        nextKey = g_homeSocksIterator->first;
                    }
                    return nextKey;
                }
            case PL_TYPE:
                if ((*(col->players)).end() == (*(col->players)).begin()) {
                    return "(null)";
                } else {
                    g_homeSocksIterator++;
                    if (g_homeSocksIterator == (*(col->players)).end()) {
                        g_homeSocksIterator = (*(col->players)).begin();
                    }
                    string nextKey = g_homeSocksIterator->first;
                    if (currKey == nextKey) {
                        // same key -> skip to next
                        g_homeSocksIterator++;
                        if (g_homeSocksIterator == (*(col->players)).end()) {
                            g_homeSocksIterator = (*(col->players)).begin();
                        }
                        nextKey = g_homeSocksIterator->first;
                    }
                    return nextKey;
                }
        }
    }
    return "(null)";
}

string GetNextAwayShirtKey(WORD teamId, string currKey)
{
	gdbFindKitsForTeam(gdb, teamId);
    KitCollection* col = MAP_FIND(gdb->uni,teamId);
    if (col) {
        switch (typ) {
            case GK_TYPE:
                if ((*(col->goalkeepers)).end() == (*(col->goalkeepers)).begin()) {
                    return "(null)";
                } else {
                    g_awayShirtIterator++;
                    if (g_awayShirtIterator == (*(col->goalkeepers)).end()) {
                        g_awayShirtIterator = (*(col->goalkeepers)).begin();
                    }
                    string nextKey = g_awayShirtIterator->first;
                    if (currKey == nextKey) {
                        // same key -> skip to next
                        g_awayShirtIterator++;
                        if (g_awayShirtIterator == (*(col->goalkeepers)).end()) {
                            g_awayShirtIterator = (*(col->goalkeepers)).begin();
                        }
                        nextKey = g_awayShirtIterator->first;
                    }
                    return nextKey;
                }
            case PL_TYPE:
                if ((*(col->players)).end() == (*(col->players)).begin()) {
                    return "(null)";
                } else {
                    g_awayShirtIterator++;
                    if (g_awayShirtIterator == (*(col->players)).end()) {
                        g_awayShirtIterator = (*(col->players)).begin();
                    }
                    string nextKey = g_awayShirtIterator->first;
                    if (currKey == nextKey) {
                        // same key -> skip to next
                        g_awayShirtIterator++;
                        if (g_awayShirtIterator == (*(col->players)).end()) {
                            g_awayShirtIterator = (*(col->players)).begin();
                        }
                        nextKey = g_awayShirtIterator->first;
                    }
                    return nextKey;
                }
        }
    }
    return "(null)";
}

string GetNextAwayShortsKey(WORD teamId, string currKey)
{
	gdbFindKitsForTeam(gdb, teamId);
    KitCollection* col = MAP_FIND(gdb->uni,teamId);
    if (col) {
        switch (typ) {
            case GK_TYPE:
                if ((*(col->goalkeepers)).end() == (*(col->goalkeepers)).begin()) {
                    return "(null)";
                } else {
                    g_awayShortsIterator++;
                    if (g_awayShortsIterator == (*(col->goalkeepers)).end()) {
                        g_awayShortsIterator = (*(col->goalkeepers)).begin();
                    }
                    string nextKey = g_awayShortsIterator->first;
                    if (currKey == nextKey) {
                        // same key -> skip to next
                        g_awayShortsIterator++;
                        if (g_awayShortsIterator == (*(col->goalkeepers)).end()) {
                            g_awayShortsIterator = (*(col->goalkeepers)).begin();
                        }
                        nextKey = g_awayShortsIterator->first;
                    }
                    return nextKey;
                }
            case PL_TYPE:
                if ((*(col->players)).end() == (*(col->players)).begin()) {
                    return "(null)";
                } else {
                    g_awayShortsIterator++;
                    if (g_awayShortsIterator == (*(col->players)).end()) {
                        g_awayShortsIterator = (*(col->players)).begin();
                    }
                    string nextKey = g_awayShortsIterator->first;
                    if (currKey == nextKey) {
                        // same key -> skip to next
                        g_awayShortsIterator++;
                        if (g_awayShortsIterator == (*(col->players)).end()) {
                            g_awayShortsIterator = (*(col->players)).begin();
                        }
                        nextKey = g_awayShortsIterator->first;
                    }
                    return nextKey;
                }
        }
    }
    return "(null)";
}

string GetNextAwaySocksKey(WORD teamId, string currKey)
{
	gdbFindKitsForTeam(gdb, teamId);
    KitCollection* col = MAP_FIND(gdb->uni,teamId);
    if (col) {
        switch (typ) {
            case GK_TYPE:
                if ((*(col->goalkeepers)).end() == (*(col->goalkeepers)).begin()) {
                    return "(null)";
                } else {
                    g_awaySocksIterator++;
                    if (g_awaySocksIterator == (*(col->goalkeepers)).end()) {
                        g_awaySocksIterator = (*(col->goalkeepers)).begin();
                    }
                    string nextKey = g_awaySocksIterator->first;
                    if (currKey == nextKey) {
                        // same key -> skip to next
                        g_awaySocksIterator++;
                        if (g_awaySocksIterator == (*(col->goalkeepers)).end()) {
                            g_awaySocksIterator = (*(col->goalkeepers)).begin();
                        }
                        nextKey = g_awaySocksIterator->first;
                    }
                    return nextKey;
                }
            case PL_TYPE:
                if ((*(col->players)).end() == (*(col->players)).begin()) {
                    return "(null)";
                } else {
                    g_awaySocksIterator++;
                    if (g_awaySocksIterator == (*(col->players)).end()) {
                        g_awaySocksIterator = (*(col->players)).begin();
                    }
                    string nextKey = g_awaySocksIterator->first;
                    if (currKey == nextKey) {
                        // same key -> skip to next
                        g_awaySocksIterator++;
                        if (g_awaySocksIterator == (*(col->players)).end()) {
                            g_awaySocksIterator = (*(col->players)).begin();
                        }
                        nextKey = g_awaySocksIterator->first;
                    }
                    return nextKey;
                }
        }
    }
    return "(null)";
}

/**************************************************************** 
 * WH_KEYBOARD hook procedure                                   *
 ****************************************************************/ 

EXTERN_C _declspec(dllexport) LRESULT CALLBACK KeyboardProc(int code, WPARAM wParam, LPARAM lParam)
{
	//TRACE2(&k_mydll,"KeyboardProc CALLED. code = %d", code);

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
                    // switch kit selection mode: Players/Goalkeepers
                    typ = (typ + 1) % 2;
                    g_modeSwitch = TRUE;

                    // switch iterators
                    g_homeShirtIterator = (typ==PL_TYPE) ?
                        g_homeShirtIteratorPL : g_homeShirtIteratorGK;
                    g_homeShortsIterator = (typ==PL_TYPE) ?
                        g_homeShortsIteratorPL : g_homeShortsIteratorGK;
                    g_homeSocksIterator = (typ==PL_TYPE) ?
                        g_homeSocksIteratorPL : g_homeSocksIteratorGK;
                    g_awayShirtIterator = (typ==PL_TYPE) ?
                        g_awayShirtIteratorPL : g_awayShirtIteratorGK;
                    g_awayShortsIterator = (typ==PL_TYPE) ?
                        g_awayShortsIteratorPL : g_awayShortsIteratorGK;
                    g_awaySocksIterator = (typ==PL_TYPE) ?
                        g_awaySocksIteratorPL : g_awaySocksIteratorGK;
                }
                /*
				if (wParam == g_config.vKeyHomeKit) {
					//g_display2Dkits = (g_display2Dkits + 1) % 2;
					//LogWithNumber(&k_mydll,"KeyboardProc: g_display2Dkits = %d", g_display2Dkits);

					// hook SetTexture method, if not yet done so
					if (GetActiveDevice() && !g_orgSetTexture) {
						DWORD* vtable = (DWORD*)(*((DWORD*)GetActiveDevice()));

						g_orgSetTexture = (PFNSETTEXTUREPROC)vtable[VTAB_SETTEXTURE];
						DWORD protection = 0;
						DWORD newProtection = PAGE_EXECUTE_READWRITE;
						if (VirtualProtect(vtable+VTAB_SETTEXTURE, 4, newProtection, &protection))
						{
							vtable[VTAB_SETTEXTURE] = (DWORD)JuceSetTexture;
							Log(&k_mydll,"SetTexture hooked.");
						}
					} else if (GetActiveDevice() && g_orgSetTexture) {
						DWORD* vtable = (DWORD*)(*((DWORD*)GetActiveDevice()));

						DWORD protection = 0;
						DWORD newProtection = PAGE_EXECUTE_READWRITE;
						if (VirtualProtect(vtable+VTAB_SETTEXTURE, 4, newProtection, &protection))
						{
							vtable[VTAB_SETTEXTURE] = (DWORD)g_orgSetTexture;
							Log(&k_mydll,"SetTexture unhooked.");
						}
                        g_orgSetTexture = NULL;
                    }

                    if (!g_display2Dkits) {
                        // clear the kit texture map
                        Log(&k_mydll,"Clearing 2Dkit textures and g_kitTextureMap.");
                        g_kitTextureMap.clear();
                        g_home_shirt_tex = NULL;
                        g_home_shorts_tex = NULL;
                        g_home_socks_tex = NULL;
                        g_away_shirt_tex = NULL;
                        g_away_shorts_tex = NULL;
                        g_away_socks_tex = NULL;
                    }

                    if (g_display2Dkits) {
                        // initialize home iterators
                        WORD teamId = GetTeamId(HOME);
                        if (teamId != 0xffff) {
                            KitCollection* col = MAP_FIND(gdb->uni,teamId);
                            if (col) {
                                g_homeShirtIterator = (*(col->players)).begin();
                                g_homeShortsIterator = (*(col->players)).begin();
                                g_homeSocksIterator = (*(col->players)).begin();
                            }
                        }
                        // initialize away iterators
                        teamId = GetTeamId(AWAY);
                        if (teamId != 0xffff) {
                            KitCollection* col = MAP_FIND(gdb->uni,teamId);
                            if (col) {
                                g_awayShirtIterator = (*(col->players)).begin();
                                g_awayShortsIterator = (*(col->players)).begin();
                                g_awaySocksIterator = (*(col->players)).begin();
                            }
                        }
                    }

					//bIndicate = g_display2Dkits;
				}
				else if (wParam == g_config.vKeyAwayKit)
				{
				}
				else if (wParam == g_config.vKeyGKHomeKit)
				{
				}
				else if (wParam == g_config.vKeyGKAwayKit)
				{
				}
				*/
				if (wParam == keyCfg->keyboard.keySwitchLeft) {
                    // cycle home kit
                    g_shirtChange[HOME] = true;
                    //Log(&k_mydll, "vKeyHomeKit PRESSED.");
                }
				else if (wParam == keyCfg->keyboard.keySwitchRight)
				{
                    // cycle home kit
                    g_shirtChange[AWAY] = true;
                    //Log(&k_mydll, "vKeyAwayKit PRESSED.");
				}
                /*
                else if (wParam == g_config.vKeyGKHomeKit)
                {
                    // dump team models
                    FILE* f = fopen("kitserver\\models.txt","wt");
                    for (WORD id = 0; id < 255; id++) {
                        KITPACKINFO* ki = GetKitPackInfoById(id);
                        fprintf(f, "team %03d (%02x): model %03d (%02x)\n",
                                id, id, ki->plHome.model, ki->plHome.model);
                    }
                    fclose(f);
                }
                */
			}
			break;
	}

	// We must pass the all messages on to CallNextHookEx.
	return CallNextHookEx(g_hKeyboardHook, code, wParam, lParam);
}

/* Set debug flag */
EXTERN_C LIBSPEC void SetDebug(DWORD flag)
{
	//LogWithNumber(&k_mydll,"Setting g_config.debug flag to %d", flag);
	k_mydll.debug = flag;
}

EXTERN_C LIBSPEC DWORD GetDebug(void)
{
	return k_mydll.debug;
}

/* Set key */
EXTERN_C LIBSPEC void SetKey(int whichKey, int code)
{
	switch (whichKey)
	{
		case VKEY_HOMEKIT: g_config.vKeyHomeKit = code; break;
		case VKEY_AWAYKIT: g_config.vKeyAwayKit = code; break;
		case VKEY_GKHOMEKIT: g_config.vKeyGKHomeKit = code; break;
		case VKEY_GKAWAYKIT: g_config.vKeyGKAwayKit = code; break;
	}
}

EXTERN_C LIBSPEC int GetKey(int whichKey)
{
	switch (whichKey)
	{
		case VKEY_HOMEKIT: return g_config.vKeyHomeKit;
		case VKEY_AWAYKIT: return g_config.vKeyAwayKit;
		case VKEY_GKHOMEKIT: return g_config.vKeyGKHomeKit;
		case VKEY_GKAWAYKIT: return g_config.vKeyGKAwayKit;
	}
	return -1;
}

/* Set gdb dir */
EXTERN_C LIBSPEC void SetKdbDir(char* gdbDir)
{
	if (gdbDir == NULL) return;
	ZeroMemory(GetPESInfo()->gdbDir, BUFLEN);
	lstrcpy(GetPESInfo()->gdbDir, gdbDir);
}

EXTERN_C LIBSPEC void GetKdbDir(char* gdbDir)
{
	if (gdbDir == NULL) return;
	ZeroMemory(gdbDir, BUFLEN);
	lstrcpy(gdbDir, GetPESInfo()->gdbDir);
}

/**
 * Utility function to dump any data to file.
 */
void DumpData(BYTE* addr, DWORD size)
{
	static int count = 0;

	// prepare filename
	char name[BUFLEN];
	ZeroMemory(name, BUFLEN);
	sprintf(name, "%s%s%02d.dat", GetPESInfo()->mydir, "dd", count++);

	FILE* f = fopen(name, "wb");
	if (f != NULL)
	{
		fwrite(addr, size, 1, f);
		fclose(f);
	}
}

/**
 * Utility function to dump any data to a named file.
 */
void DumpData(BYTE* addr, DWORD size, char* filename)
{
	// prepare filename
	char name[BUFLEN];
	ZeroMemory(name, BUFLEN);
	sprintf(name, "%s%s", GetPESInfo()->mydir, filename);

	FILE* f = fopen(name, "wb");
	if (f != NULL)
	{
		fwrite(addr, size, 1, f);
		fclose(f);
	}
}

/**
 * This function saves the textures from decoded
 * uniform buffer into two BMP files: 512x256x8 and 256x128x8
 * (with palette).
 */
void DumpUniforms(TEXIMGPACKHEADER* kitPackHeader)
{
	static int count = 0;
	DWORD* kitPackToc = (DWORD*)((BYTE*)kitPackHeader + kitPackHeader->tocOffset);
	BYTE* buffer = (BYTE*)kitPackHeader + kitPackToc[0];

	// prepare 1st filename
	char name[BUFLEN];
	ZeroMemory(name, BUFLEN);
	sprintf(name, "%s%s%02d.bmp", GetPESInfo()->mydir, "du", count++);

	SaveAs8bitBMP(name, buffer + 0x480, buffer + 0x80, 512, 256);

	/*
	// prepare 2nd filename
	ZeroMemory(name, BUFLEN);
	sprintf(name, "%s%s%02d.bmp", GetPESInfo()->mydir, "du", count++);

	SaveAs8bitBMP(name, buffer + 0x20480, buffer + 0x80, 256, 128);

	// prepare 3rd filename
	ZeroMemory(name, BUFLEN);
	sprintf(name, "%s%s%02d.bmp", GetPESInfo()->mydir, "du", count++);

	SaveAs8bitBMP(name, buffer + 0x28480, buffer + 0x80, 128, 64);
	*/
}

/**
 * This function saves the textures from decoded
 * image-pack into BMP files.
 */
void DumpImagePack(TEXIMGPACKHEADER* imagePackHeader)
{
	static int count = 0;
	int i;
	for (i=0; i<imagePackHeader->numFiles; i++) {
		TEXIMGHEADER* imageHeader = (TEXIMGHEADER*)((BYTE*)imagePackHeader + imagePackHeader->toc[i]);

		// prepare 1st filename
		char name[BUFLEN];
		ZeroMemory(name, BUFLEN);
		sprintf(name, "%s%s%02d.bmp", GetPESInfo()->mydir, "du", count++);

		SaveAsBitmap(name, imageHeader);
	}
}

// Read a file into a memory block.
static int read_file_to_mem(char *fn,unsigned char **ppfiledata, int *pfilesize)
{
	HANDLE hfile;
	DWORD fsize;
	//unsigned char *fbuf;
	BYTE* fbuf;
	DWORD bytesread;

	hfile=CreateFile(fn,GENERIC_READ,FILE_SHARE_READ,NULL,
		OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
	if(hfile==INVALID_HANDLE_VALUE) return 1;

	fsize=GetFileSize(hfile,NULL);
	if(fsize>0) {
		//fbuf=(unsigned char*)GlobalAlloc(GPTR,fsize);
		//fbuf=(unsigned char*)calloc(fsize,1);
        fbuf = (BYTE*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, fsize);
		if(fbuf) {
			if(ReadFile(hfile,(void*)fbuf,fsize,&bytesread,NULL)) {
				if(bytesread==fsize) { 
					(*ppfiledata)  = fbuf;
					(*pfilesize) = (int)fsize;
					CloseHandle(hfile);
					return 0;   // success
				}
			}
			free((void*)fbuf);
		}
	}
	CloseHandle(hfile);
	return 1;  // error
}

/**
 * Extracts alpha values from tRNS chunk and applies stores
 * them in the RGBQUADs of the DIB
 */
void ApplyAlphaChunk(RGBQUAD* palette, BYTE* memblk, DWORD size)
{
    bool got_alpha = false;

    // find the tRNS chunk
    DWORD offset = 8;
    while (offset < size) {
        PNG_CHUNK_HEADER* chunk = (PNG_CHUNK_HEADER*)(memblk + offset);
        if (chunk->dwName == MAKEFOURCC('t','R','N','S')) {
            int numColors = SWAPBYTES(chunk->dwSize);
            BYTE* alphaValues = memblk + offset + sizeof(chunk->dwSize) + sizeof(chunk->dwName);
            for (int i=0; i<numColors; i++) {
                palette[i].rgbReserved = (alphaValues[i]==0xff) ? 0x80 : alphaValues[i]/2;
            }
            got_alpha = true;
        }
        // move on to next chunk
        offset += sizeof(chunk->dwSize) + sizeof(chunk->dwName) + 
            SWAPBYTES(chunk->dwSize) + sizeof(DWORD); // last one is CRC
    }

    // initialize alpha to all-opaque, if haven't gotten it
    if (!got_alpha) {
        for (int i=0; i<256; i++) {
            palette[i].rgbReserved = 0x80;
        }
    }
}

// Load texture from PNG file. Returns the size of loaded texture
DWORD LoadPNGTexture(BITMAPINFO** tex, char* filename)
{
	TRACE4(&k_mydll,"LoadPNGTexture: loading %s", filename);
    DWORD size = 0;

    PNGDIB *pngdib;
    LPBITMAPINFOHEADER* ppDIB = (LPBITMAPINFOHEADER*)tex;

    pngdib = pngdib_p2d_init();
	//TRACE(&k_mydll,"LoadPNGTexture: structure initialized");

    BYTE* memblk;
    int memblksize;
    if(read_file_to_mem(filename,&memblk, &memblksize)) {
        TRACE(&k_mydll,"LoadPNGTexture: unable to read PNG file");
        return 0;
    }
    //TRACE(&k_mydll,"LoadPNGTexture: file read into memory");

    pngdib_p2d_set_png_memblk(pngdib,memblk,memblksize);
	pngdib_p2d_set_use_file_bg(pngdib,1);
	pngdib_p2d_run(pngdib);

	//TRACE(&k_mydll,"LoadPNGTexture: run done");
    pngdib_p2d_get_dib(pngdib, ppDIB, (int*)&size);
	//TRACE(&k_mydll,"LoadPNGTexture: get_dib done");

    pngdib_done(pngdib);
	TRACE(&k_mydll,"LoadPNGTexture: done done");

	TRACE2(&k_mydll,"LoadPNGTexture: *ppDIB = %08x", (DWORD)*ppDIB);
    if (*ppDIB == NULL) {
		TRACE(&k_mydll,"LoadPNGTexture: ERROR - unable to load PNG image.");
        return 0;
    }

    // read transparency values from tRNS chunk
    // and put them into DIB's RGBQUAD.rgbReserved fields
    ApplyAlphaChunk((RGBQUAD*)&((BITMAPINFO*)*ppDIB)->bmiColors, memblk, memblksize);

    HeapFree(GetProcessHeap(), 0, memblk);

	TRACE(&k_mydll,"LoadPNGTexture: done");
	return size;
}

// Load texture. Returns the size of loaded texture
DWORD LoadTexture(BITMAPINFO** tex, char* filename)
{
	TRACE4(&k_mydll,"LoadTexture: loading %s", filename);

	FILE* f = fopen(filename, "rb");
	if (f == NULL)
	{
		TRACE(&k_mydll,"LoadTexture: ERROR - unable to open file.");
		return 0;
	}
	BITMAPFILEHEADER bfh;
	fread(&bfh, sizeof(BITMAPFILEHEADER), 1, f);
	DWORD size = bfh.bfSize - sizeof(BITMAPFILEHEADER);

	TRACE2(&k_mydll,"LoadTexture: filesize = %08x", bfh.bfSize);

	*tex = (BITMAPINFO*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, size);
	fread(*tex, size, 1, f);
	fclose(f);

	TRACE(&k_mydll,"LoadTexture: done");
	return size;
}

// Substiture kit textures with data from DIB
void ApplyDIBTexture(TEXIMGPACKHEADER* orgKit, BITMAPINFO* bitmap)
{
    TRACE(&k_mydll,"Applying DIB texture");

	TEXIMGHEADER* tex = (TEXIMGHEADER*)((BYTE*)orgKit + orgKit->toc[0]);
	BYTE* buf = (BYTE*)tex + tex->dataOffset;
	BYTE* pal = (BYTE*)tex + tex->paletteOffset;

	BYTE* srcTex = (BYTE*)bitmap;
	BITMAPINFOHEADER* bih = &bitmap->bmiHeader;
	DWORD palOff = bih->biSize;
    DWORD numColors = 1 << bih->biBitCount;
    DWORD palSize = numColors*4;
	DWORD bitsOff = palOff + palSize;

    BYTE* largeKit = (BYTE*)orgKit + orgKit->toc[0];

	// copy palette from tex0
	TRACE2(&k_mydll,"bitsOff = %08x", bitsOff);
	TRACE2(&k_mydll,"palOff  = %08x", palOff);

	for (int bank=0; bank<8; bank++)
	{
		memcpy(largeKit + 0x80 + bank*32*4,        srcTex + palOff + bank*32*4,        8*4);
		memcpy(largeKit + 0x80 + bank*32*4 + 16*4, srcTex + palOff + bank*32*4 + 8*4,  8*4);
		memcpy(largeKit + 0x80 + bank*32*4 + 8*4,  srcTex + palOff + bank*32*4 + 16*4, 8*4);
		memcpy(largeKit + 0x80 + bank*32*4 + 24*4, srcTex + palOff + bank*32*4 + 24*4, 8*4);
	}
	// swap R and B
	for (int i=0; i<0x100; i++) 
	{
		BYTE blue = largeKit[0x80 + i*4];
		BYTE red = largeKit[0x80 + i*4 + 2];
		largeKit[0x80 + i*4] = red;
		largeKit[0x80 + i*4 + 2] = blue;
	}
	TRACE(&k_mydll,"Palette copied.");

	int k, m, j, w;
	int height, width;
	int imageWidth;

	// copy tex0
	width = imageWidth = 512; height = 256;
	for (k=0, m=bih->biHeight-1; k<height, m>=bih->biHeight - height; k++, m--)
	{
		memcpy(largeKit + 0x480 + k*width, srcTex + bitsOff + m*imageWidth, width);
	}
	TRACE(&k_mydll,"Texture 512x256 replaced.");

	if (bih->biHeight == 384) // MipMap kit 
	{
		BYTE* mediumAndSmallKits = (BYTE*)orgKit + orgKit->toc[1];

		// copy palette from largeKit
		memcpy(mediumAndSmallKits + 0x80, largeKit + 0x80, 0x400);

		// copy tex1
		width = 256; height = 128;
		for (k=0, m=127; k<height, m>=0; k++, m--)
		{
			memcpy(mediumAndSmallKits + 0x480 + k*width, srcTex + bitsOff + m*imageWidth, width);
		}
		TRACE(&k_mydll,"Texture 256x128 replaced.");

		// copy tex2
		width = 128; height = 64;
		for (k=0, m=127; k<height, m>=64; k++, m--)
		{
			memcpy(mediumAndSmallKits + 0x8480 + k*width, srcTex + bitsOff + 256 + m*imageWidth, width);
		}
		TRACE(&k_mydll,"Texture 128x64 replaced.");
	}
	else // Single-texture kit
	{
		BYTE* mediumAndSmallKits = (BYTE*)orgKit + orgKit->toc[1];

		// copy palette from largeKit
		memcpy(mediumAndSmallKits + 0x80, largeKit + 0x80, 0x400);

		// resample the texture at half-size in each dimension
		int W = width/2, H = height/2;
		for (k=0, m=height-2; k<H, m>=0; k++, m-=2)
		{
			for (w=0, j=0; w<W, j<width-1; w++, j+=2)
			{
				mediumAndSmallKits[0x480 + k*W + w] = srcTex[bitsOff + m*width + j];
			}
		}
		TRACE(&k_mydll,"Texture 512x256 resampled to 256x128.");
		TRACE(&k_mydll,"Texture 256x128 replaced.");

		// resample the texture at 1/4-size in each dimension
		W = width/4, H = height/4;
		for (k=0, m=height-4; k<H, m>=0; k++, m-=4)
		{
			for (w=0, j=0; w<W, j<width-3; w++, j+=4)
			{
				mediumAndSmallKits[0x8480 + k*W + w] = srcTex[bitsOff + m*width + j];
			}
		}
		TRACE(&k_mydll,"Texture 256x128 resampled to 128x64.");
		TRACE(&k_mydll,"Texture 128x64 replaced.");
	}
}

// OR kit textures with data from DIB
void OrDIBTexture(TEXIMGPACKHEADER* orgKit, BITMAPINFO* bitmap)
{
    TRACE(&k_mydll,"ORing DIB texture");

	TEXIMGHEADER* tex = (TEXIMGHEADER*)((BYTE*)orgKit + orgKit->toc[0]);
	BYTE* buf = (BYTE*)tex + tex->dataOffset;
	BYTE* pal = (BYTE*)tex + tex->paletteOffset;

	BYTE* srcTex = (BYTE*)bitmap;
	BITMAPINFOHEADER* bih = &bitmap->bmiHeader;
	DWORD palOff = bih->biSize;
    DWORD numColors = 1 << bih->biBitCount;
    DWORD palSize = numColors*4;
	DWORD bitsOff = palOff + palSize;

    BYTE* largeKit = (BYTE*)orgKit + orgKit->toc[0];

	int k, m, j, w;
	int height, width;
	int imageWidth;

	// OR tex0
	width = imageWidth = 512; height = 256;
	for (k=0, m=bih->biHeight-1; k<height, m>=bih->biHeight - height; k++, m--)
	{
		//memcpy(largeKit + 0x480 + k*width, srcTex + bitsOff + m*imageWidth, width);
        for (w=0; w<width; w++) {
            largeKit[0x480 + k*width + w] |= srcTex[bitsOff + m*imageWidth + w];
        }
	}
	TRACE(&k_mydll,"Texture 512x256 OR-ed.");

	if (bih->biHeight == 384) // MipMap kit 
	{
		BYTE* mediumAndSmallKits = (BYTE*)orgKit + orgKit->toc[1];

		// copy tex1
		width = 256; height = 128;
		for (k=0, m=127; k<height, m>=0; k++, m--)
		{
			//memcpy(mediumAndSmallKits + 0x480 + k*width, srcTex + bitsOff + m*imageWidth, width);
            for (w=0; w<width; w++) {
                mediumAndSmallKits[0x480 + k*width + w] |= srcTex[bitsOff + m*imageWidth + w];
            }
		}
		TRACE(&k_mydll,"Texture 256x128 OR-ed.");

		// copy tex2
		width = 128; height = 64;
		for (k=0, m=127; k<height, m>=64; k++, m--)
		{
			//memcpy(mediumAndSmallKits + 0x8480 + k*width, srcTex + bitsOff + 256 + m*imageWidth, width);
            for (w=0; w<width; w++) {
                mediumAndSmallKits[0x8480 + k*width + w] |= srcTex[bitsOff + 256 + m*imageWidth + w];
            }
		}
		TRACE(&k_mydll,"Texture 128x64 OR-ed.");
	}
	else // Single-texture kit
	{
		BYTE* mediumAndSmallKits = (BYTE*)orgKit + orgKit->toc[1];

		// resample the texture at half-size in each dimension
		int W = width/2, H = height/2;
		for (k=0, m=height-2; k<H, m>=0; k++, m-=2)
		{
			for (w=0, j=0; w<W, j<width-1; w++, j+=2)
			{
				mediumAndSmallKits[0x480 + k*W + w] |= srcTex[bitsOff + m*width + j];
			}
		}
		TRACE(&k_mydll,"Texture 512x256 resampled to 256x128.");
		TRACE(&k_mydll,"Texture 256x128 OR-ed.");

		// resample the texture at 1/4-size in each dimension
		W = width/4, H = height/4;
		for (k=0, m=height-4; k<H, m>=0; k++, m-=4)
		{
			for (w=0, j=0; w<W, j<width-3; w++, j+=4)
			{
				mediumAndSmallKits[0x8480 + k*W + w] |= srcTex[bitsOff + m*width + j];
			}
		}
		TRACE(&k_mydll,"Texture 256x128 resampled to 128x64.");
		TRACE(&k_mydll,"Texture 128x64 OR-ed.");
	}
}

// Substiture kit textures with data from DIB
void ApplyDIBTextureMipMap(TEXIMGPACKHEADER* orgKit, int ordinal, BITMAPINFO* bitmap)
{
    TRACE(&k_mydll,"Applying DIB texture (mipmap)");

	TEXIMGHEADER* tex = (TEXIMGHEADER*)((BYTE*)orgKit + orgKit->toc[0]);
	BYTE* buf = (BYTE*)tex + tex->dataOffset;
	BYTE* pal = (BYTE*)tex + tex->paletteOffset;

	BYTE* srcTex = (BYTE*)bitmap;
	BITMAPINFOHEADER* bih = &bitmap->bmiHeader;
	DWORD palOff = bih->biSize;
    DWORD numColors = 1 << bih->biBitCount;
    DWORD palSize = numColors*4;
	DWORD bitsOff = palOff + palSize;

    BYTE* destData = (BYTE*)orgKit + orgKit->toc[1];

	TRACE2(&k_mydll,"bitsOff = %08x", bitsOff);
	TRACE2(&k_mydll,"palOff  = %08x", palOff);

	for (int bank=0; bank<8; bank++)
	{
		memcpy(destData + 0x80 + bank*32*4,        srcTex + palOff + bank*32*4,        8*4);
		memcpy(destData + 0x80 + bank*32*4 + 16*4, srcTex + palOff + bank*32*4 + 8*4,  8*4);
		memcpy(destData + 0x80 + bank*32*4 + 8*4,  srcTex + palOff + bank*32*4 + 16*4, 8*4);
		memcpy(destData + 0x80 + bank*32*4 + 24*4, srcTex + palOff + bank*32*4 + 24*4, 8*4);
	}
	// swap R and B
	for (int i=0; i<0x100; i++) 
	{
		BYTE blue = destData[0x80 + i*4];
		BYTE red = destData[0x80 + i*4 + 2];
		destData[0x80 + i*4] = red;
		destData[0x80 + i*4 + 2] = blue;
	}
	TRACE(&k_mydll,"Palette copied.");

	int k, m, j, w;
    int width = 128*(2-ordinal);
	int height = 64*(2-ordinal);

    DWORD pixelsOff = 0x480 + 0x8000*ordinal;

    for (k=0, m=height-1; k<height, m>=0; k++, m--)
    {
        memcpy(destData + pixelsOff + k*width, srcTex + bitsOff + m*width, width);
    }
    TRACE2(&k_mydll,"Mipmap (%d) texture replaced.", ordinal);
}

// Substiture palette with palette from DIB.
void Apply4BitDIBPalette(TEXIMGHEADER* tex, BITMAPINFO* bitmap)
{
    TRACE(&k_mydll,"Applying palette from DIB");

	BYTE* buf = (BYTE*)tex + tex->dataOffset;
	BYTE* pal = (BYTE*)tex + tex->paletteOffset;

	BYTE* srcTex = (BYTE*)bitmap;
	BITMAPINFOHEADER* bih = &bitmap->bmiHeader;
	DWORD palOff = bih->biSize;
    DWORD numColors = 1 << bih->biBitCount;
    DWORD palSize = numColors*4;
	DWORD bitsOff = palOff + palSize;

    // copy palette from image
    TRACE2(&k_mydll,"bitsOff = %08x", bitsOff);
    TRACE2(&k_mydll,"palOff  = %08x", palOff);
    memcpy(pal, srcTex + palOff, 16*4); // take only 1st 16 colors

    // swap R and B
    for (int i=0; i<16; i++) 
    {
        BYTE blue = pal[i*4];
        BYTE red = pal[i*4 + 2];
        pal[i*4] = red;
        pal[i*4 + 2] = blue;
    }
    TRACE(&k_mydll,"Palette copied.");
}

// Substiture kit textures with data from DIB.
void Apply4BitDIBTexture(TEXIMGPACKHEADER* pack, BITMAPINFO* bitmap, BOOL usePalette)
{
    TRACE(&k_mydll,"Applying DIB image to 4-bit texture");

	TEXIMGHEADER* tex = (TEXIMGHEADER*)((BYTE*)pack + pack->toc[0]);
	BYTE* buf = (BYTE*)tex + tex->dataOffset;
	BYTE* pal = (BYTE*)tex + tex->paletteOffset;

	BYTE* srcTex = (BYTE*)bitmap;
	BITMAPINFOHEADER* bih = &bitmap->bmiHeader;
	DWORD palOff = bih->biSize;
    DWORD numColors = 1 << bih->biBitCount;
    DWORD palSize = numColors*4;
	DWORD bitsOff = palOff + palSize;

	if (usePalette) {
		// copy palette from image
		TRACE2(&k_mydll,"bitsOff = %08x", bitsOff);
		TRACE2(&k_mydll,"palOff  = %08x", palOff);
		memcpy(pal, srcTex + palOff, 16*4); // take only 1st 16 colors

		// swap R and B
		for (int i=0; i<16; i++) 
		{
			BYTE blue = pal[i*4];
			BYTE red = pal[i*4 + 2];
			pal[i*4] = red;
			pal[i*4 + 2] = blue;
		}
		TRACE(&k_mydll,"Palette copied.");
	}

    if (bih->biBitCount == 4) {
        // copy data as is
        int k, m, j;
        for (k=0, m=bih->biHeight-1; k<tex->height, m>=0; k++, m--)
        {
            memcpy(buf + k*(tex->width/2), srcTex + bitsOff + m*(bih->biWidth/2), tex->width/2);
            // shuffle pixel data
            for (j=0; j<tex->width/2; j++) {
                BYTE src = buf[k*(tex->width/2)+j];
                buf[k*(tex->width/2)+j] = (src << 4 & 0xf0) | (src >> 4 & 0x0f);
            }
        }

    } else if (bih->biBitCount == 8) {
        // need to convert 8-bit data to 4-bit
        int k, m, j;
        for (k=0, m=bih->biHeight-1; k<tex->height, m>=0; k++, m--)
        {
            for (j=0; j<bih->biWidth; j+=2) {
                // combine two bytes of data into one
                // (shuffling it as well)
                buf[k*(tex->width/2)+j/2] = 
                    (srcTex[bitsOff + m*(bih->biWidth) + j] & 0x0f) |
                    (srcTex[bitsOff + m*(bih->biWidth) + j + 1] << 4 & 0xf0);
            }
        }
    }

	TRACE(&k_mydll,"4-bit DIB Texture replaced.");
}

void FreeTexture(BITMAPINFO* bitmap) 
{
	if (bitmap != NULL) {
		HeapFree(GetProcessHeap(), 0, bitmap);
	}
}

void FreePNGTexture(BITMAPINFO* bitmap) 
{
	if (bitmap != NULL) {
        pngdib_p2d_free_dib(NULL, (BITMAPINFOHEADER*)bitmap);
	}
}

void ApplyGlovesDIBTexture(TEXIMGPACKHEADER* orgKit, BITMAPINFO* bitmap)
{
    TRACE(&k_mydll,"Applying PNG glove texture");

	TEXIMGHEADER* tex = (TEXIMGHEADER*)((BYTE*)orgKit + orgKit->toc[2]);
	BYTE* buf = (BYTE*)tex + tex->dataOffset;
	BYTE* pal = (BYTE*)tex + tex->paletteOffset;

	BYTE* srcTex = (BYTE*)bitmap;
	BITMAPINFOHEADER* bih = &bitmap->bmiHeader;
	DWORD palOff = bih->biSize;
    DWORD numColors = 1 << bih->biBitCount;
    DWORD palSize = numColors*4;
	DWORD bitsOff = palOff + palSize;

    BYTE* largeGloves = (BYTE*)orgKit + orgKit->toc[2];

	TRACE2(&k_mydll,"bitsOff = %08x", bitsOff);
	TRACE2(&k_mydll,"palOff  = %08x", palOff);

	for (int bank=0; bank<8; bank++)
	{
		memcpy(largeGloves + 0x80 + bank*32*4,        srcTex + palOff + bank*32*4,        8*4);
		memcpy(largeGloves + 0x80 + bank*32*4 + 16*4, srcTex + palOff + bank*32*4 + 8*4,  8*4);
		memcpy(largeGloves + 0x80 + bank*32*4 + 8*4,  srcTex + palOff + bank*32*4 + 16*4, 8*4);
		memcpy(largeGloves + 0x80 + bank*32*4 + 24*4, srcTex + palOff + bank*32*4 + 24*4, 8*4);
	}
	// swap R and B
	for (int i=0; i<0x100; i++) 
	{
		BYTE blue = largeGloves[0x80 + i*4];
		BYTE red = largeGloves[0x80 + i*4 + 2];
		largeGloves[0x80 + i*4] = red;
		largeGloves[0x80 + i*4 + 2] = blue;
		largeGloves[0x80 + i*4 + 3] = 0x80; // always 100% opacity
	}
	TRACE(&k_mydll,"Palette copied.");

	int k, m, j, w;
	int height, width;
	int imageWidth;

	// copy tex0
	width = imageWidth = 64; height = 128;
	for (k=0, m=bih->biHeight-1; k<height, m>=bih->biHeight - height; k++, m--)
	{
		memcpy(largeGloves + 0x480 + k*width, srcTex + bitsOff + m*imageWidth, width);
	}
	TRACE(&k_mydll,"Texture 64x128 replaced.");

	if (bih->biHeight == 192) // MipMap kit 
	{
		// copy tex1
		width = 32; height = 64;
		for (k=0, m=63; k<height, m>=0; k++, m--)
		{
			memcpy(largeGloves + 0x2480 + k*width, srcTex + bitsOff + m*imageWidth, width);
		}
		TRACE(&k_mydll,"Texture 32x64 replaced.");
	}
	else // Single-texture kit
	{
		// resample the texture at half-size in each dimension
		int W = width/2, H = height/2;
		for (k=0, m=height-2; k<H, m>=0; k++, m-=2)
		{
			for (w=0, j=0; w<W, j<width-1; w++, j+=2)
			{
				largeGloves[0x2480 + k*W + w] = srcTex[bitsOff + m*width + j];
			}
		}
		TRACE(&k_mydll,"Texture 64x128 resampled to 32x64.");
		TRACE(&k_mydll,"Texture 32x64 replaced.");
	}
}

/**
 * Tracker for IDirect3DTexture8::GetSurfaceLevel method.
 */
HRESULT STDMETHODCALLTYPE JuceGetSurfaceLevel(IDirect3DTexture8* self, UINT level,
IDirect3DSurface8** ppSurfaceLevel)
{
	TRACE(&k_mydll,"JuceGetSurfaceLevel: CALLED.");
	TRACE2(&k_mydll,"JuceGetSurfaceLevel: texture(self) = %08x", (DWORD)self);
	TRACE2(&k_mydll,"JuceGetSurfaceLevel: level = %d", level);

	return g_orgGetSurfaceLevel(self, level, ppSurfaceLevel);
}

/**
 * Saves surface into a auto-named file.
 */
void DumpSurface(int count, IDirect3DSurface8* surf)
{
	char filename[BUFLEN];
	ZeroMemory(filename, BUFLEN);
	sprintf(filename, "%ssurf-%03d.bmp", GetPESInfo()->mydir, count);
	D3DXSaveSurfaceToFile(filename, D3DXIFF_BMP, surf, NULL, NULL);
}

void DumpTexture(IDirect3DTexture8* const ptexture) 
{
    static int count = 0;
    char buf[BUFLEN];
    sprintf(buf,"%stex%03d.bmp",GetPESInfo()->mydir,count++);
    //sprintf(buf,"kitserver\\tex-%08x.bmp",(DWORD)ptexture);
    if (FAILED(D3DXSaveTextureToFile(buf, D3DXIFF_BMP, ptexture, NULL))) {
        LogWithString(&k_mydll, "DumpTexture: failed to save texture to %s", buf);
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
                LogWithNumber(&k_mydll,"ReplaceTextureLevel: replacing level %d COMPLETE", level);

            } else {
                LogWithNumber(&k_mydll,"ReplaceTextureLevel: replacing level %d FAILED", level);
            }
            src->Release();
        }
        dest->Release();
    }
}

void ReplaceTexture(IDirect3DTexture8* srcTexture, IDirect3DTexture8* repTexture, UINT levels)
{
    LogWithNumber(&k_mydll, "ReplaceTexture: replacing texture with levels count = %d", levels);

    // DEBUG: dump texture before replacement
    //DumpTexture(srcTexture);

    for (int i=0; i<levels; i++) {
        ReplaceTextureLevel(srcTexture, repTexture, i);
    }

    // DEBUG: dump texture after replacement
    //DumpTexture(srcTexture);
}


DWORD VtableSet(void* self, int index, DWORD value)
{
    DWORD* vtab = (DWORD*)(*(DWORD*)self);
    DWORD currValue = vtab[index];
    vtab[index] = value;
    return currValue;
}

void kservUnlockRect(IDirect3DTexture8* self,UINT Level) 
{
    vector<TextureBinding>::iterator texit = NULL;
    if (_textureBindings.size()>0) {
        for (vector<TextureBinding>::iterator it = _textureBindings.begin();
                it != _textureBindings.end();
                it++) {
            if (it->srcTexture == self) {
                ReplaceTextureLevel(it->srcTexture, it->repTexture, Level);

                if (Level == it->levels - 1) {
                    // release replacement texture, so that we don't leak resources
                    it->repTexture->Release();
                    // also mark it for deletion from the bindings vector
                    texit = it;
                }
                break;
            }
        }
        // remove that binding from the vector
        if (texit) _textureBindings.erase(texit);
    }
}

// copy all the pixels where the color c is in the mask file from src to dest
void applyKitMask(D3DLOCKED_RECT* dest, D3DLOCKED_RECT* src, char* maskfilename, D3DSURFACE_DESC* desc, DWORD c)
{
	UINT width = desc->Width;
	UINT height = desc->Height;
	
	BITMAPINFO* maskTex = NULL;
	LoadPNGTexture(&maskTex, maskfilename);
	if (!maskTex) return;
	
	BITMAPINFOHEADER* bih = (BITMAPINFOHEADER*)maskTex;
	UINT maskW = bih->biWidth;
	UINT maskH = bih->biHeight;
	BYTE* mask = (BYTE*)maskTex + bih->biSize + 0x400;
	DWORD index = -1;
	for (int i=0; i<256; i++) {
		if ((((DWORD*)((BYTE*)maskTex + bih->biSize))[i] & 0xffffff) == c) {
			index = i;
			break;
		}
	}

	BYTE* pDestRow = (BYTE*)dest->pBits;
	BYTE* pSrcRow = (BYTE*)src->pBits;
	for (int y=0; y<height; y++) {
		DWORD* pDestPixel = (DWORD*)pDestRow;
		DWORD* pSrcPixel = (DWORD*)pSrcRow;
		for (int x=0; x<width; x++) {
			if (mask[(height-y-1)*maskH/height*maskW + x*maskW/width] == index) {
				*pDestPixel = *pSrcPixel;
			}
			pDestPixel++;
			pSrcPixel++;
		}
		pDestRow += dest->Pitch;
		pSrcRow += src->Pitch;
	}
	
	pngdib_p2d_free_dib(NULL,(BITMAPINFOHEADER*)maskTex);
	return;
}

void applyOverlay(D3DLOCKED_RECT* dest, char* overlayfilename, D3DSURFACE_DESC* desc)
{
	int i, x, y;
	
	D3DXIMAGE_INFO ovImageInfo;
	if (D3DXGetImageInfoFromFile(overlayfilename, &ovImageInfo) != D3D_OK) return;
	
	UINT ovWidth = ovImageInfo.Width;
	UINT ovHeight = ovImageInfo.Height;

	BITMAPINFO* ovTex = NULL;
	if (ovImageInfo.ImageFileFormat == D3DXIFF_PNG) {
		LogWithNumber(&k_mydll, "%d bytes loaded", LoadPNGTexture(&ovTex, overlayfilename));
	} else if (ovImageInfo.ImageFileFormat == D3DXIFF_BMP) {
		LogWithNumber(&k_mydll, "%d bytes loaded", LoadTexture(&ovTex, overlayfilename));
	} else {
		return;
	}
	if (!ovTex) return;
		
	BITMAPINFOHEADER* bih = (BITMAPINFOHEADER*)ovTex;
	BYTE* byOverlay = (BYTE*)ovTex + bih->biSize;
	DWORD* dwOverlay = (DWORD*)ovTex + bih->biSize;
	DWORD index = -1;
	BYTE* pDestRow = NULL;
	BYTE* pSrcRow = NULL;
	DWORD pixel = 0;

	UINT width = desc->Width;
	UINT height = desc->Height;
	IDirect3DTexture8* pOvTexture = NULL;
	D3DLOCKED_RECT rectOv;
	
	WORD bitCount = bih->biBitCount;
	if (bitCount != 8 && bitCount != 24 && bitCount != 32) goto freeOv3;
	
	// create overlay texture
	if (!SUCCEEDED(D3DXCreateTextureFromFileEx(
            GetActiveDevice(), overlayfilename, 
            width, height,
            1, desc->Usage, desc->Format, desc->Pool,
            D3DX_DEFAULT, D3DX_DEFAULT,
            0, NULL, NULL, &pOvTexture))) {
		goto freeOv3;
	}
	if (!SUCCEEDED(pOvTexture->LockRect(0, &rectOv, NULL, 0))) goto freeOv2;

	if (bitCount == 8) {
		for (i=0; i<256; i++) {
			pixel = ((DWORD*)((BYTE*)ovTex + bih->biSize))[i];
			// allow a difference of 3 for each byte to 0xff00ff (good if color reduction
			// to 256 colors doesn't keep this value)
			if (((pixel&0xff0000)>=0xfc0000) && ((pixel&0x00ff00)<=0x000400) && ((pixel&0x0000ff)>=0x0000fc)) {
				index = i;
				break;
			}
		}
		LogWithNumber(&k_mydll, "Index: %d", index);		
	}

	pDestRow = (BYTE*)dest->pBits;
	pSrcRow = (BYTE*)rectOv.pBits;
	for (y=0; y<height; y++) {
		DWORD* pDestPixel = (DWORD*)pDestRow;
		DWORD* pSrcPixel = (DWORD*)pSrcRow;
		for (x=0; x<width; x++) {
			pixel = (height-y-1)*ovHeight/height*ovWidth + x*ovWidth/width;
			switch (bitCount) {
			case 8:
				if (byOverlay[pixel + 0x400] != index) {
					*pDestPixel = *pSrcPixel;
				}
				break;
			case 24:
				//3 bits only
				if (byOverlay[3*pixel]!=0xff || byOverlay[3*pixel+1]!=0 || byOverlay[3*pixel+2]!=0xff) {
					*pDestPixel = *pSrcPixel;
				}
				break;
			case 32:
				if (*dwOverlay & 0xffffff != 0xff00ff) {
					*pDestPixel = *pSrcPixel;
				}
				break;
			}
			pDestPixel++;
			pSrcPixel++;
		}
		pDestRow += dest->Pitch;
		pSrcRow += rectOv.Pitch;
	}
		
	freeOv1:
	pOvTexture->UnlockRect(0);
	freeOv2:
	pOvTexture->Release();
	freeOv3:
	if (ovImageInfo.ImageFileFormat == D3DXIFF_PNG) {
		FreePNGTexture(ovTex);
	} else {
		FreeTexture(ovTex);
	}
		
	return;
}
	

void mixKits(IDirect3DTexture8* pShirtTexture, IDirect3DTexture8* pShortsTexture,
	IDirect3DTexture8* pSocksTexture, char* overlayfilename, char* maskfilename, int level)
{
	if (!pShirtTexture) return;
	if (!pShortsTexture && !pSocksTexture) return;
	
	D3DLOCKED_RECT rectShirt;
	D3DLOCKED_RECT rectShorts;
	D3DLOCKED_RECT rectSocks;
	D3DSURFACE_DESC desc;
	
	if (!SUCCEEDED(pShirtTexture->LockRect(level, &rectShirt, NULL, 0))) return;
	pShirtTexture->GetLevelDesc(level, &desc);

	if (pShortsTexture && SUCCEEDED(pShortsTexture->LockRect(level, &rectShorts, NULL, 0))) {
		Log(&k_mydll, "Replacing shorts now...");
		//blue
		applyKitMask(&rectShirt, &rectShorts, maskfilename, &desc, 0x0000ff);
		pShortsTexture->UnlockRect(level);
	}
	
	if (pSocksTexture && SUCCEEDED(pSocksTexture->LockRect(level, &rectSocks, NULL, 0))) {
		Log(&k_mydll, "Replacing socks now...");
		//yellow
		applyKitMask(&rectShirt, &rectSocks, maskfilename, &desc, 0xffff00);
		pSocksTexture->UnlockRect(level);
	}
	
	if (overlayfilename != NULL) {
		Log(&k_mydll, "Applying overlay...");
		applyOverlay(&rectShirt, overlayfilename, &desc);
	}
	
	pShirtTexture->UnlockRect(level);
	return;
}

/**
 * Tracker for IDirect3DDevice8::CreateTexture method.
 */
HRESULT STDMETHODCALLTYPE JuceCreateTexture(IDirect3DDevice8* self, UINT width, UINT height,
UINT levels, DWORD usage, D3DFORMAT format, D3DPOOL pool, IDirect3DTexture8** ppTexture, 
DWORD src, bool* IsProcessed)
{
	HRESULT res = D3D_OK;
    if (*IsProcessed) return res; // already handled by some other module

    if (g_edit_mode) return res; // safety check

    // process 256x128 2-level texture
    if (width == 256 && height == 128) {
        if (levels == 2 && g_unidecode_flag) {
            LogWithThreeNumbers(&k_mydll,"JuceCreateTexture: (%dx%d), levels = %d", width, height, levels);
            LogWithNumber(&k_mydll,"JuceCreateTexture: src = %08x", src);

            BOOL needsMask;
            char filename1[BUFLEN] = {0};
            char filename2[BUFLEN] = {0};
            char filename3[BUFLEN] = {0};
            char maskfilename[BUFLEN] = {0};
            char overlayfilename[BUFLEN] = {0};
            int fileType = (g_currentAfsId - data[FIRST_ID]) % FILES_PER_TEAM;
            int fileTypeBase = fileType / 3;
            // shirt
            DWORD texType1 = FindImageFileForId(g_currentAfsId, "", filename1, &needsMask);
            // shorts
            DWORD id = g_currentAfsId - fileType + fileTypeBase * 2;
            DWORD texType2 = FindImageFileForId(id | 0x80000000, "", filename2, &needsMask);
			// socks
			id = g_currentAfsId - fileType + fileTypeBase * 2 + 1;
            DWORD texType3 = FindImageFileForId(id | 0x80000000, "", filename3, &needsMask);
            // mask
            FindMaskFileForId(g_currentAfsId, maskfilename);
            // overlay
            DWORD overlayTexType = FindOverlayFileForId(g_currentAfsId, overlayfilename);
            
            if (texType1 != TEXTYPE_NONE) {
                // replacement texture file exists
                // step 1: get some information about it
                D3DXIMAGE_INFO imageInfo1, imageInfo2, imageInfo3;
                D3DXGetImageInfoFromFile(filename1, &imageInfo1);

                // step 2: check whether the system supports such texture dimensions
                UINT texWidth = imageInfo1.Width/2;
                UINT texHeight = imageInfo1.Height/2;
                UINT texLevels = levels;
                D3DFORMAT texFormat = format;
                if (g_config.enable_HD_kits) {
                    LogWithTwoNumbers(&k_mydll, 
                            "JuceCreateTexture: checking texture dimensions: (%d,%d)",
                            texWidth, texHeight);
                    if (SUCCEEDED(D3DXCheckTextureRequirements(
                                    self, &texWidth, &texHeight,
                                    &texLevels, usage, 
                                    &texFormat, pool))) {
                        Log(&k_mydll, "JuceCreateTexture: texture parameters checked");
                    }
                    
                    // eventually enlarge the replacing texure for shorts or socks
					if (texType2 != TEXTYPE_NONE) {
                		D3DXGetImageInfoFromFile(filename2, &imageInfo2);
                		texWidth = max(imageInfo2.Width/2, texWidth);
                		texHeight = max(imageInfo2.Height/2, texHeight);
                		
                		LogWithTwoNumbers(&k_mydll, 
                            "JuceCreateTexture: checking texture dimensions: (%d,%d)",
                            texWidth, texHeight);
	                    if (SUCCEEDED(D3DXCheckTextureRequirements(
	                                    self, &texWidth, &texHeight,
	                                    &texLevels, usage, 
	                                    &texFormat, pool))) {
	                        Log(&k_mydll, "JuceCreateTexture: texture parameters checked");
	                    }
					}
					if (texType3 != TEXTYPE_NONE) {
                		D3DXGetImageInfoFromFile(filename3, &imageInfo3);
                		texWidth = max(imageInfo3.Width/2, texWidth);
                		texHeight = max(imageInfo3.Height/2, texHeight);
                		
                		LogWithTwoNumbers(&k_mydll, 
                            "JuceCreateTexture: checking texture dimensions: (%d,%d)",
                            texWidth, texHeight);
	                    if (SUCCEEDED(D3DXCheckTextureRequirements(
	                                    self, &texWidth, &texHeight,
	                                    &texLevels, usage, 
	                                    &texFormat, pool))) {
	                        Log(&k_mydll, "JuceCreateTexture: texture parameters checked");
	                    }
					}
 
                    // ensure minimum size: 256x128
                    texWidth = (texWidth<256)?256:texWidth;
                    texHeight = (texHeight<128)?128:texHeight;
                    LogWithTwoNumbers(&k_mydll, 
                            "JuceCreateTexture: check complete. Using (%d,%d)",
                            texWidth, texHeight);
                } else {
                    texWidth = 256;
                    texHeight = 128;
                    Log(&k_mydll, "JuceCreateTexture: using standard-size (256,128) texture");
                }

                // step 3: create the texture that was requested, but change
                // the dimensions of it according to the replacement texture.
                // Also create the replacement texture for later usage.
                IDirect3DTexture8* pRepTexture = NULL;
                IDirect3DTexture8* pShortsTexture = NULL;
                IDirect3DTexture8* pSocksTexture = NULL;
                
                DWORD prevValue = VtableSet(self, VTAB_CREATETEXTURE, (DWORD)OrgCreateTexture);
                if (SUCCEEDED(D3DXCreateTextureFromFileEx(
                                self, filename1, 
                                texWidth, texHeight,
                                levels, usage, texFormat, pool,
                                D3DX_DEFAULT, D3DX_DEFAULT,
                                0, NULL, NULL, &pRepTexture))) {
                                	
					// shorts
					if (texType2 != TEXTYPE_NONE && stricmp(filename1, filename2)!=0) {
						if (!SUCCEEDED(D3DXCreateTextureFromFileEx(
                                self, filename2, 
                                texWidth, texHeight,
                                levels, usage, texFormat, pool,
                                D3DX_DEFAULT, D3DX_DEFAULT,
                                0, NULL, NULL, &pShortsTexture))) {
							pShortsTexture = NULL;
						}
					}
					// socks
					if (texType3 != TEXTYPE_NONE && stricmp(filename1, filename3)!=0) {
						if (!SUCCEEDED(D3DXCreateTextureFromFileEx(
                                self, filename3, 
                                texWidth, texHeight,
                                levels, usage, texFormat, pool,
                                D3DX_DEFAULT, D3DX_DEFAULT,
                                0, NULL, NULL, &pSocksTexture))) {
							pSocksTexture = NULL;
						}
					}
					
					// now mix the kits
					for (int i=0; i<2; i++) {
						if (overlayTexType == TEXTYPE_NONE) {
							mixKits(pRepTexture, pShortsTexture, pSocksTexture, NULL, maskfilename, i);
						} else {
							mixKits(pRepTexture, pShortsTexture, pSocksTexture, overlayfilename, maskfilename, i);
						}
					}
					
					// free shorts and socks textures
					if (pShortsTexture) pShortsTexture->Release();
					if (pSocksTexture) pSocksTexture->Release();

                    VtableSet(self, VTAB_CREATETEXTURE, prevValue);
                    res = OrgCreateTexture(self, texWidth, texHeight,
                            levels,usage,format,pool,ppTexture);

                    TextureBinding textureBinding;
                    textureBinding.srcTexture = *ppTexture;
                    textureBinding.repTexture = pRepTexture;
                    textureBinding.levels = 2;
                    _textureBindings.push_back(textureBinding);

                    *IsProcessed = true;
                    return res;

                } else {
                    VtableSet(self, VTAB_CREATETEXTURE, prevValue);
                    LogWithString(&k_mydll, "JuceCreateTexture: FAILED to create a texture from %s", filename1);
                }
            } else {
                LogWithNumber(&k_mydll, "JuceCreateTexture: Image file not found for id = %d", g_currentAfsId);
            }
        }
    }

    // process 512x256 1-level texture
    if (width == 512 && height == 256) {
        g_unidecode_flag = false;

        hash_map<DWORD,DWORD>::iterator mit = _source_to_id.find(src);
        if (mit != _source_to_id.end()) {
            LogWithThreeNumbers(&k_mydll,"JuceCreateTexture: (%dx%d), levels = %d", width, height, levels);
            LogWithNumber(&k_mydll,"JuceCreateTexture: (found entry): src = %08x", mit->first);
            LogWithNumber(&k_mydll,"JuceCreateTexture: AFS id = %d", mit->second);

            BOOL needsMask;
            char filename1[BUFLEN] = {0};
            char filename2[BUFLEN] = {0};
            char filename3[BUFLEN] = {0};
            char maskfilename[BUFLEN] = {0};
            char overlayfilename[BUFLEN] = {0};
            int fileType = (mit->second - data[FIRST_ID]) % FILES_PER_TEAM;
            int fileTypeBase = fileType / 3;
            // shirt
            DWORD texType1 = FindImageFileForId(mit->second, "", filename1, &needsMask);
            // shorts
            DWORD id = mit->second - fileType + fileTypeBase * 2;
            DWORD texType2 = FindImageFileForId(id | 0x80000000, "", filename2, &needsMask);
			// socks
			id = mit->second - fileType + fileTypeBase * 2 + 1;
            DWORD texType3 = FindImageFileForId(id | 0x80000000, "", filename3, &needsMask);
            // mask
            FindMaskFileForId(mit->second, maskfilename);
            // overlay
            DWORD overlayTexType = FindOverlayFileForId(mit->second, overlayfilename);
            
            if (texType1 != TEXTYPE_NONE) {
                // replacement texture file exists
                // step 1: get some information about it
                D3DXIMAGE_INFO imageInfo1, imageInfo2, imageInfo3;
                D3DXGetImageInfoFromFile(filename1, &imageInfo1);

                // step 2: check whether the system supports such texture dimensions
                UINT texWidth = imageInfo1.Width;
                UINT texHeight = imageInfo1.Height;
                UINT texLevels = levels;
                D3DFORMAT texFormat = format;
                if (g_config.enable_HD_kits) {
                    LogWithTwoNumbers(&k_mydll, 
                            "JuceCreateTexture: checking texture dimensions: (%d,%d)",
                            texWidth, texHeight);
                    if (SUCCEEDED(D3DXCheckTextureRequirements(
                                    self, &texWidth, &texHeight,
                                    &texLevels, usage, 
                                    &texFormat, pool))) {
                        Log(&k_mydll, "JuceCreateTexture: texture parameters checked");
                    }
                    
                    // eventually enlarge the replacing texure for shorts or socks
					if (texType2 != TEXTYPE_NONE) {
                		D3DXGetImageInfoFromFile(filename2, &imageInfo2);
                		texWidth = max(imageInfo2.Width, texWidth);
                		texHeight = max(imageInfo2.Height, texHeight);
                		
                		LogWithTwoNumbers(&k_mydll, 
                            "JuceCreateTexture: checking texture dimensions: (%d,%d)",
                            texWidth, texHeight);
	                    if (SUCCEEDED(D3DXCheckTextureRequirements(
	                                    self, &texWidth, &texHeight,
	                                    &texLevels, usage, 
	                                    &texFormat, pool))) {
	                        Log(&k_mydll, "JuceCreateTexture: texture parameters checked");
	                    }
					}
					if (texType3 != TEXTYPE_NONE) {
                		D3DXGetImageInfoFromFile(filename3, &imageInfo3);
                		texWidth = max(imageInfo3.Width, texWidth);
                		texHeight = max(imageInfo3.Height, texHeight);
                		
                		LogWithTwoNumbers(&k_mydll, 
                            "JuceCreateTexture: checking texture dimensions: (%d,%d)",
                            texWidth, texHeight);
	                    if (SUCCEEDED(D3DXCheckTextureRequirements(
	                                    self, &texWidth, &texHeight,
	                                    &texLevels, usage, 
	                                    &texFormat, pool))) {
	                        Log(&k_mydll, "JuceCreateTexture: texture parameters checked");
	                    }
					}
 
                    // ensure minimum size: 512x256
                    texWidth = (texWidth<512)?512:texWidth;
                    texHeight = (texHeight<256)?256:texHeight;
                    LogWithTwoNumbers(&k_mydll, 
                            "JuceCreateTexture: check complete. Using (%d,%d)",
                            texWidth, texHeight);
                } else {
                    texWidth = 512;
                    texHeight = 256;
                    Log(&k_mydll, "JuceCreateTexture: using standard-size (512,256) texture");
                }

                // step 3: create the texture that was requested, but change
                // the dimensions of it according to the replacement texture.
                // Also create the replacement texture for later usage.
                IDirect3DTexture8* pRepTexture = NULL;
                IDirect3DTexture8* pShortsTexture = NULL;
                IDirect3DTexture8* pSocksTexture = NULL;
                
                DWORD prevValue = VtableSet(self, VTAB_CREATETEXTURE, (DWORD)OrgCreateTexture);
                if (SUCCEEDED(D3DXCreateTextureFromFileEx(
                                self, filename1, 
                                texWidth, texHeight,
                                levels, usage, texFormat, pool,
                                D3DX_DEFAULT, D3DX_DEFAULT,
                                0, NULL, NULL, &pRepTexture))) {
                                	
					// shorts
					if (texType2 != TEXTYPE_NONE && stricmp(filename1, filename2)!=0) {
						if (!SUCCEEDED(D3DXCreateTextureFromFileEx(
                                self, filename2, 
                                texWidth, texHeight,
                                levels, usage, texFormat, pool,
                                D3DX_DEFAULT, D3DX_DEFAULT,
                                0, NULL, NULL, &pShortsTexture))) {
							pShortsTexture = NULL;
						}
					}
					// socks
					if (texType3 != TEXTYPE_NONE && stricmp(filename1, filename3)!=0) {
						if (!SUCCEEDED(D3DXCreateTextureFromFileEx(
                                self, filename3, 
                                texWidth, texHeight,
                                levels, usage, texFormat, pool,
                                D3DX_DEFAULT, D3DX_DEFAULT,
                                0, NULL, NULL, &pSocksTexture))) {
							pSocksTexture = NULL;
						}
					}
					
					// now mix the kits
					if (overlayTexType == TEXTYPE_NONE) {
						mixKits(pRepTexture, pShortsTexture, pSocksTexture, NULL, maskfilename, 0);
					} else {
						mixKits(pRepTexture, pShortsTexture, pSocksTexture, overlayfilename, maskfilename, 0);
					}
					
					// free shorts and socks textures
					if (pShortsTexture) pShortsTexture->Release();
					if (pSocksTexture) pSocksTexture->Release();

                    VtableSet(self, VTAB_CREATETEXTURE, prevValue);
                    res = OrgCreateTexture(self, texWidth, texHeight,
                            levels,usage,format,pool,ppTexture);

                    TextureBinding textureBinding;
                    textureBinding.srcTexture = *ppTexture;
                    textureBinding.repTexture = pRepTexture;
                    textureBinding.levels = 1;
                    _textureBindings.push_back(textureBinding);

                    *IsProcessed = true;
                    return res;

                } else {
                    LogWithString(&k_mydll, "JuceCreateTexture: FAILED to create a texture from %s", filename1);
                }
            } else {
                LogWithNumber(&k_mydll, "JuceCreateTexture: Image file not found for id = %d", mit->second);
            }
        }
    }

	return res;
}

/***
 * Tracker for IDirect3DDevice8::SetTextureStageState method
 */
HRESULT STDMETHODCALLTYPE JuceSetTextureStageState(IDirect3DDevice8* self, DWORD Stage,
D3DTEXTURESTAGESTATETYPE Type, DWORD Value)
{
	TRACE(&k_mydll,"JuceSetTextureStageState: CALLED.");
	HRESULT res = g_orgSetTextureStageState(self, Stage, Type, Value);
	return res;
}

/***
 * Tracker for IDirect3DDevice8::UpdateTexture method
 */
HRESULT STDMETHODCALLTYPE JuceUpdateTexture(IDirect3DDevice8* self,
IDirect3DBaseTexture8* pSrc, IDirect3DBaseTexture8* pDest)
{
	TRACE(&k_mydll,"JuceUpdateTexture: CALLED.");
	HRESULT res = g_orgUpdateTexture(self, pSrc, pDest);
	return res;
}

/**
 * Tracker for IDirect3DDevice8::SetTexture method.
 */
HRESULT STDMETHODCALLTYPE JuceSetTexture(IDirect3DDevice8* self, DWORD stage,
IDirect3DBaseTexture8* pTexture)
{
	HRESULT res = S_OK;
	TRACE2X(&k_mydll,"JuceSetTexture: stage = %d, pTexture = %08x", stage, (DWORD)pTexture);

	res = g_orgSetTexture(self, stage, pTexture);

    /*
	//if (g_dumpTexturesMode) {
    {
		// dump texture to BMP
		char name[BUFLEN];
		ZeroMemory(name, BUFLEN);
		sprintf(name, "C:\\texdump\\%s%05d.bmp", "tex-", g_frame_tex_count++);
		D3DXSaveTextureToFile(name, D3DXIFF_BMP, pTexture, NULL);
	}
    */

	return res;
}

/**
 * Simple file-check routine.
 */
BOOL FileExists(char* filename)
{
    TRACE4(&k_mydll,"FileExists: Checking file: %s", filename);
    HANDLE hFile;
    hFile = CreateFile(TEXT(filename),        // file to open
                       GENERIC_READ,          // open for reading
                       FILE_SHARE_READ,       // share for reading
                       NULL,                  // default security
                       OPEN_EXISTING,         // existing file only
                       FILE_ATTRIBUTE_NORMAL, // normal file
                       NULL);                 // no attr. template
     
    if (hFile == INVALID_HANDLE_VALUE) 
    { 
        return FALSE;
    }
    CloseHandle(hFile);
    return TRUE;
}

string GetKitFolderKey(DWORD id)
{
    //LogWithNumber(&k_mydll,"GetKitFolderKey: fileType = %d", fileType);
    
    WORD teamId = GetTeamIdByOrdinalAFS(((id & 0x7fffffff) - data[FIRST_ID]) / FILES_PER_TEAM);
    int fileType = ((id & 0x7fffffff) - data[FIRST_ID]) % FILES_PER_TEAM;
    if (id & 0x80000000) fileType += FILES_PER_TEAM;
    return GetKitFolderKeyByTeamId(teamId, fileType);
}

string GetKitFolderKeyByTeamId(WORD teamId, int fileType)
{
    WORD home = GetTeamId(HOME);
    WORD away = GetTeamId(AWAY);

    // Important check: if teams changed, then we need to clear kit keys
    if (home != g_last_home || away != g_last_away) {
        ClearKitKeys();
        g_last_home = home;
        g_last_away = away;
    }

    BOOL isHome = TRUE;
    if (home == away) {
        //BYTE* strips = (BYTE*)data[TEAM_STRIPS];
        BYTE strips[2];
        GetTeamStrips(strips);
        // special case. We need to find out then
        // which team this file is being loaded for: away or home
        switch (fileType) {
            case GA_KIT:
            case GA_UNKNOWN1:
            case GA_UNKNOWN2:
            case GA_NUMBERS:
            case GA_FONT:
                isHome = (strips[0] & STRIP_GK_SHIRT) == 0;
                break;
            case GB_KIT:
            case GB_UNKNOWN1:
            case GB_UNKNOWN2:
            case GB_NUMBERS:
            case GB_FONT:
                isHome = (strips[1] & STRIP_GK_SHIRT) == 0;
                break;
            case PA_SHORTS:
                isHome = (strips[0] & STRIP_PL_SHORTS) == 0;
                break;
            case PA_SOCKS:
                isHome = (strips[0] & STRIP_PL_SOCKS) == 0;
                break;
            case PA_SHIRT:
            case PA_NUMBERS:
            case PA_FONT:
                isHome = (strips[0] & STRIP_PL_SHIRT) == 0;
                break;
            case PB_SHORTS:
                isHome = (strips[1] & STRIP_PL_SHORTS) == 0;
                break;
            case PB_SOCKS:
                isHome = (strips[1] & STRIP_PL_SOCKS) == 0;
                break;
            case PB_SHIRT:
            case PB_NUMBERS:
            case PB_FONT:
            default:
                isHome = (strips[1] & STRIP_PL_SHIRT) == 0;
        }
    } else {
        // normal case: two different teams
        isHome = (home == teamId);
    }

//LogWithTwoNumbers(&k_mydll, "GetFolderKey(%d); fileType = %d", id, fileType); 
    switch (fileType) {
        case GA_KIT:


        case GA_NUMBERS:
        case GA_FONT: {
            string& key = (isHome) ? GET_HOME_SHIRT_KEY(GK_TYPE) : GET_AWAY_SHIRT_KEY(GK_TYPE);
            return (key.length()>0) ? key : "ga";
                      }
        case GA_UNKNOWN1: {
            string& key = (isHome) ? GET_HOME_SHORTS_KEY(GK_TYPE) : GET_AWAY_SHORTS_KEY(GK_TYPE);
            return (key.length()>0) ? key : "ga";
                      }
        case GA_UNKNOWN2: {
            string& key = (isHome) ? GET_HOME_SOCKS_KEY(GK_TYPE) : GET_AWAY_SOCKS_KEY(GK_TYPE);
            return (key.length()>0) ? key : "ga";
                      }
        case GB_KIT:


        case GB_NUMBERS:
        case GB_FONT: {
            string& key = (isHome) ? GET_HOME_SHIRT_KEY(GK_TYPE) : GET_AWAY_SHIRT_KEY(GK_TYPE);
            return (key.length()>0) ? key : "gb";
                      }
        case GB_UNKNOWN1: {
            string& key = (isHome) ? GET_HOME_SHORTS_KEY(GK_TYPE) : GET_AWAY_SHORTS_KEY(GK_TYPE);
            return (key.length()>0) ? key : "gb";
                      }
        case GB_UNKNOWN2: {
            string& key = (isHome) ? GET_HOME_SOCKS_KEY(GK_TYPE) : GET_AWAY_SOCKS_KEY(GK_TYPE);
            return (key.length()>0) ? key : "gb";
                      }
        case PA_SHIRT:
        case PA_NUMBERS:
        case PA_FONT: {
                          //Log(&k_mydll, "PA_SHIRT");
            string& key = (isHome) ? GET_HOME_SHIRT_KEY(PL_TYPE) : GET_AWAY_SHIRT_KEY(PL_TYPE);
                          //LogWithString(&k_mydll, "PA_SHIRT: key = {%s}", (char*)key.c_str());
            return (key.length()>0) ? key : "pa";
                      }
        case PA_SHORTS: {
            string& key = (isHome) ? GET_HOME_SHORTS_KEY(PL_TYPE) : GET_AWAY_SHORTS_KEY(PL_TYPE);
            return (key.length()>0) ? key : "pa";
                        }
        case PA_SOCKS: {
            string& key = (isHome) ? GET_HOME_SOCKS_KEY(PL_TYPE) : GET_AWAY_SOCKS_KEY(PL_TYPE);
            return (key.length()>0) ? key : "pa";
                       }
        case PB_SHIRT:
        case PB_NUMBERS:
        case PB_FONT: {
            string& key = (isHome) ? GET_HOME_SHIRT_KEY(PL_TYPE) : GET_AWAY_SHIRT_KEY(PL_TYPE);
            return (key.length()>0) ? key : "pb";
                      }
        case PB_SHORTS: {
            string& key = (isHome) ? GET_HOME_SHORTS_KEY(PL_TYPE) : GET_AWAY_SHORTS_KEY(PL_TYPE);
            return (key.length()>0) ? key : "pb";
                        }
        case PB_SOCKS: {
            string& key = (isHome) ? GET_HOME_SOCKS_KEY(PL_TYPE) : GET_AWAY_SOCKS_KEY(PL_TYPE);
            return (key.length()>0) ? key : "pb";
                       }
    }

    return "(null)";
}

/*
 * Only licensed team are stored in AFS file. Non-licensed, don't even have the
 * slots.
 */
WORD GetTeamIdByOrdinalAFS(WORD ord)
{
    //return g_licensed_ordinals[ord];
    WORD* list = (WORD*)data[LICENSED_LIST];
    return list[ord];
}

BOOL FindImageFileForIdEx(DWORD id, char* suffix, char* filename, char* ext, BOOL* pNeedsMask)
{
    // sanity check
    if (id < data[FIRST_ID]) {
        return FALSE;
    }

	WORD teamId = GetTeamIdByOrdinalAFS(((id & 0x7fffffff) - data[FIRST_ID]) / FILES_PER_TEAM);
	int fileType = ((id & 0x7fffffff) - data[FIRST_ID]) % FILES_PER_TEAM;
	if (id & 0x80000000) {
		fileType += FILES_PER_TEAM;
    }

    gdbFindKitsForTeam(gdb, teamId);
    KitCollection* col = MAP_FIND(gdb->uni,teamId);
    if (!col) {
        // no kit collection for this team 
        return FALSE;
    }
    Kit* kit = NULL;

    char fileClue1[BUFLEN];
    ZeroMemory(fileClue1, BUFLEN);

    string kitKey = GetKitFolderKey(id);
//LogWithString(&k_mydll, "kitKey = {%s}", (char*)kitKey.c_str());
	int kitType = GK_TYPE;
	int kitPart = KITPART_SHIRT;

    switch (fileType) {
        case GA_KIT:
            strcpy(fileClue1,"\\shirt");
            break;
        case GA_UNKNOWN1:
            strcpy(fileClue1,"\\shorts");
            kitPart = KITPART_SHORTS;
            break;
        case GA_UNKNOWN2:
            strcpy(fileClue1,"\\socks");
            kitPart = KITPART_SOCKS;
            break;
        case GA_NUMBERS:
            kit = MAP_FIND(col->goalkeepers,kitKey);
            sprintf(fileClue1,"\\%s",(kit)?kit->numbersFile:"");
			kitPart = KITPART_NUMBERS;
            break;
        case GA_FONT:
            strcpy(fileClue1,"\\font");
            kitPart = KITPART_FONT;
            break;
        case GB_KIT:
            strcpy(fileClue1,"\\shirt");
            break;
        case GB_UNKNOWN1:
            strcpy(fileClue1,"\\shorts");
            kitPart = KITPART_SHORTS;
            break;
        case GB_UNKNOWN2:
            strcpy(fileClue1,"\\socks");
            kitPart = KITPART_SOCKS;
            break;
        case GB_NUMBERS:
            kit = MAP_FIND(col->goalkeepers,kitKey);
            sprintf(fileClue1,"\\%s", (kit)?kit->numbersFile:"");
            kitPart = KITPART_NUMBERS;
            break;
        case GB_FONT:
            strcpy(fileClue1,"\\font");
            kitPart = KITPART_FONT;
            break;
        case PA_SHIRT:
            strcpy(fileClue1,"\\shirt");
            kitType = PL_TYPE;
            break;
        case PA_SHORTS:
            strcpy(fileClue1,"\\shorts");
            kitType = PL_TYPE;
            kitPart = KITPART_SHORTS;
            break;
        case PA_SOCKS:
            strcpy(fileClue1,"\\socks");
            kitType = PL_TYPE;
            kitPart = KITPART_SOCKS;
            break;
        case PA_NUMBERS:
            kit = MAP_FIND(col->players,kitKey);
            sprintf(fileClue1,"\\%s", (kit)?kit->numbersFile:"");
            kitType = PL_TYPE;
            kitPart = KITPART_NUMBERS;
            break;
        case PA_FONT:
            strcpy(fileClue1,"\\font");
            kitType = PL_TYPE;
            kitPart = KITPART_FONT;
            break;
        case PB_SHIRT:
            strcpy(fileClue1,"\\shirt");
            kitType = PL_TYPE;
            break;
        case PB_SHORTS:
            strcpy(fileClue1,"\\shorts");
            kitType = PL_TYPE;
            kitPart = KITPART_SHORTS;
            break;
        case PB_SOCKS:
            strcpy(fileClue1,"\\socks");
            kitType = PL_TYPE;
            kitPart = KITPART_SOCKS;
            break;
        case PB_NUMBERS:
            kit = MAP_FIND(col->players,kitKey);
            sprintf(fileClue1,"\\%s", (kit)?kit->numbersFile:"");
            kitType = PL_TYPE;
            kitPart = KITPART_NUMBERS;
            break;
        case PB_FONT:
            strcpy(fileClue1,"\\font");
            kitType = PL_TYPE;
            kitPart = KITPART_FONT;
            break;
    }
    char* kitFoldername = getKitFoldername(kitKey, teamId, kitType, kitPart);

    char filename1[BUFLEN];
    ZeroMemory(filename1, BUFLEN);
    sprintf(filename1, "%sGDB\\uni\\%s\\%s%s%s%s", GetPESInfo()->gdbDir, col->foldername,
    					kitFoldername, fileClue1, suffix, ext);

    if (FileExists(filename1)) {
        lstrcpy(filename, filename1);
        return TRUE;
    } else if (pNeedsMask != NULL) {
        // fallback: in case of shirt/shorts/socks - check for "all" filename
        char fileClue2[BUFLEN];
        ZeroMemory(fileClue2, BUFLEN);

        switch (fileType) {
            case GA_KIT:
            case GA_UNKNOWN1:
            case GA_UNKNOWN2:
                strcpy(fileClue2,"\\all");
                break;
            case GB_KIT:
            case GB_UNKNOWN1:
            case GB_UNKNOWN2:
                strcpy(fileClue2,"\\all");
                break;
            case PA_SHIRT:
            case PA_SHORTS:
            case PA_SOCKS:
                strcpy(fileClue2,"\\all");
                break;
            case PB_SHIRT:
            case PB_SHORTS:
            case PB_SOCKS:
                strcpy(fileClue2,"\\all");
                break;
        }

        char filename2[BUFLEN];
        ZeroMemory(filename2, BUFLEN);
        sprintf(filename2, "%sGDB\\uni\\%s\\%s%s%s%s", GetPESInfo()->gdbDir, col->foldername,
        						kitFoldername, fileClue2, suffix, ext);

        if (FileExists(filename2)) {
            lstrcpy(filename, filename2);
            *pNeedsMask = TRUE;
            return TRUE;
        }
    }
    
    filename[0] = '\0';
    return FALSE;
}

DWORD FindImageFileForId(DWORD id, char* suffix, char* filename, BOOL* pNeedsMask)
{
    if (FindImageFileForIdEx(id, suffix, filename, ".png", pNeedsMask)) {
        return TEXTYPE_PNG;
    } else if (FindImageFileForIdEx(id, suffix, filename, ".bmp", pNeedsMask)) {
        return TEXTYPE_BMP;
    }
    return TEXTYPE_NONE;
}

DWORD FindImageFileForId(DWORD id, char* suffix, char* filename)
{
    if (FindImageFileForIdEx(id, suffix, filename, "", NULL)) {
        int n = lstrlen(filename);
        char* ext = (n>3)?filename+n-4:"";
        if (lstrcmpi(ext, ".png")==0) {
            return TEXTYPE_PNG;
        } else if (lstrcmpi(ext, ".bmp")==0) {
            return TEXTYPE_BMP;
        }
    }
    return TEXTYPE_NONE;
}

DWORD FindShortsPalImageFileForId(DWORD id, string& kitFolderKey, char* filename)
{
    WORD teamId = GetTeamIdByOrdinalAFS((id - data[FIRST_ID]) / FILES_PER_TEAM);
    int fileType = (id - data[FIRST_ID]) % FILES_PER_TEAM;

    gdbFindKitsForTeam(gdb, teamId);
    KitCollection* col = MAP_FIND(gdb->uni,teamId);
    if (col) {
    	string idKitFolderKey = GetKitFolderKey(id);
        Kit* kit = (fileType < PA_SHIRT) ?
            MAP_FIND(col->goalkeepers, idKitFolderKey) :
            MAP_FIND(col->players, idKitFolderKey);
        if (!kit) {
            return TEXTYPE_NONE;
        }
        LogWithString(&k_mydll,"FindShortsPal: kitFolderKey = {%s}", (char*)kitFolderKey.c_str());
        char* palFile = MAP_FIND(kit->shortsPaletteFiles,kitFolderKey);
        char filename1[BUFLEN];
        ZeroMemory(filename1, BUFLEN);
        char* kitFoldername = getKitFoldername(idKitFolderKey, teamId, (fileType < PA_SHIRT)?GK_TYPE:PL_TYPE, KITPART_NUMBERS);
        sprintf(filename1, "%sGDB\\uni\\%s\\%s\\%s", GetPESInfo()->gdbDir, col->foldername, kitFoldername, palFile);
        LogWithString(&k_mydll,"FindShortsPal: filename1 = {%s}", filename1);

        if (FileExists(filename1)) {
            lstrcpy(filename, filename1);
            int n = lstrlen(filename);
            char* ext = (n>3)?filename+n-4:"";
            if (lstrcmpi(ext, ".png")==0) {
                return TEXTYPE_PNG;
            } else if (lstrcmpi(ext, ".bmp")==0) {
                return TEXTYPE_BMP;
            }
        }
    }
    return TEXTYPE_NONE;
}

DWORD FindMaskFileForId(DWORD id, char* filename)
{
    WORD teamId = GetTeamIdByOrdinalAFS((id - data[FIRST_ID]) / FILES_PER_TEAM);
    int fileType = (id - data[FIRST_ID]) % FILES_PER_TEAM;

    gdbFindKitsForTeam(gdb, teamId);
    KitCollection* col = MAP_FIND(gdb->uni,teamId);
    if (col) {
    	string idKitFolderKey = GetKitFolderKey(id);
        Kit* kit = (fileType < PA_SHIRT) ?
            MAP_FIND(col->goalkeepers, idKitFolderKey) :
            MAP_FIND(col->players, idKitFolderKey);
        if (!kit) {
            goto useStandard;
        }
        if (!(kit->attDefined & MASK_FILE)) goto useStandard;
        int n = lstrlen(kit->maskFile);
		char* ext = (n>3)?(kit->maskFile)+n-4:"";
        // only png
        if (lstrcmpi(ext, ".png") != 0) goto useStandard;	
        
        char filename1[BUFLEN];
        ZeroMemory(filename1, BUFLEN);
        sprintf(filename1, "%s%s\\%s", GetPESInfo()->gdbDir, kit->foldername, kit->maskFile);
        
        // see as relative path first
        if (FileExists(filename1)) {
        	lstrcpy(filename, filename1);
        	return TEXTYPE_PNG;
        }
        
        // then, look in mask directory
        sprintf(filename1, "%sGDB\\uni\\masks\\%s", GetPESInfo()->gdbDir, kit->maskFile);
		if (FileExists(filename1)) {
        	lstrcpy(filename, filename1);
        	return TEXTYPE_PNG;
        }
    }
    
    useStandard:
    // use standard mask 
    sprintf(filename, "%sGDB\\uni\\masks\\mask.png", GetPESInfo()->gdbDir);
    return TEXTYPE_PNG;
}

DWORD FindOverlayFileForId(DWORD id, char* filename)
{
    WORD teamId = GetTeamIdByOrdinalAFS((id - data[FIRST_ID]) / FILES_PER_TEAM);
    int fileType = (id - data[FIRST_ID]) % FILES_PER_TEAM;

    gdbFindKitsForTeam(gdb, teamId);
    KitCollection* col = MAP_FIND(gdb->uni,teamId);
    if (col) {
    	string idKitFolderKey = GetKitFolderKey(id);
        Kit* kit = (fileType < PA_SHIRT) ?
            MAP_FIND(col->goalkeepers, idKitFolderKey) :
            MAP_FIND(col->players, idKitFolderKey);
        if (!kit) {
			return TEXTYPE_NONE;
        }
        if (!(kit->attDefined & OVERLAY_FILE)) return TEXTYPE_NONE;
        char filename1[BUFLEN];
        ZeroMemory(filename1, BUFLEN);
        sprintf(filename1, "%s%s\\%s", GetPESInfo()->gdbDir, kit->foldername, kit->overlayFile);
        
        // see as relative path first
        if (FileExists(filename1)) {
            lstrcpy(filename, filename1);
            int n = lstrlen(filename);
            char* ext = (n>3)?filename+n-4:"";
            if (lstrcmpi(ext, ".png")==0) {
                return TEXTYPE_PNG;
            } else if (lstrcmpi(ext, ".bmp")==0) {
                return TEXTYPE_BMP;
            }
        }
        
        // then, look in overlay directory
        sprintf(filename1, "%sGDB\\uni\\overlay\\%s", GetPESInfo()->gdbDir, kit->overlayFile);
		if (FileExists(filename1)) {
            lstrcpy(filename, filename1);
            int n = lstrlen(filename);
            char* ext = (n>3)?filename+n-4:"";
            if (lstrcmpi(ext, ".png")==0) {
                return TEXTYPE_PNG;
            } else if (lstrcmpi(ext, ".bmp")==0) {
                return TEXTYPE_BMP;
            }
        }
    }

    return TEXTYPE_NONE;
}


BOOL IsNumOrFontTexture(DWORD id)
{
	DWORD ordinal = (id - data[FIRST_ID]) % FILES_PER_TEAM;
	switch (ordinal) {
		case GA_NUMBERS:
		case GA_FONT:
		case GB_NUMBERS:
		case GB_FONT:
		case PA_NUMBERS:
		case PA_FONT:
		case PB_NUMBERS:
		case PB_FONT:
			return TRUE;
	}
	return FALSE;
}

BOOL IsNumbersTexture(DWORD id)
{
	DWORD ordinal = (id - data[FIRST_ID]) % FILES_PER_TEAM;
	switch (ordinal) {
		case GA_NUMBERS:
		case GB_NUMBERS:
		case PA_NUMBERS:
		case PB_NUMBERS:
			return TRUE;
	}
	return FALSE;
}

BOOL IsGK(DWORD id)
{
	DWORD ordinal = (id - data[FIRST_ID]) % FILES_PER_TEAM;
	switch (ordinal) {
		case GA_KIT:
		case GB_KIT:
			return TRUE;
	}
	return FALSE;
}

BOOL IsGKTexture(DWORD id)
{
	DWORD ordinal = (id - data[FIRST_ID]) % FILES_PER_TEAM;
	switch (ordinal) {
		case GA_KIT:
		case GA_UNKNOWN1:
		case GA_UNKNOWN2:
		case GA_NUMBERS:
		case GA_FONT:
		case GB_KIT:
		case GB_UNKNOWN1:
		case GB_UNKNOWN2:
		case GB_NUMBERS:
		case GB_FONT:
			return TRUE;
	}
	return FALSE;
}

BOOL IsShirt(DWORD id)
{
	DWORD ordinal = (id - data[FIRST_ID]) % FILES_PER_TEAM;
	switch (ordinal) {
		case GA_KIT:
		case GB_KIT:
		case PA_SHIRT:
		case PB_SHIRT:
			return TRUE;
	}
	return FALSE;
}

BOOL IsShortsOrSocks(DWORD id)
{
	DWORD ordinal = (id - data[FIRST_ID]) % FILES_PER_TEAM;
	switch (ordinal) {
		case GA_UNKNOWN1:
		case GA_UNKNOWN2:
		case GB_UNKNOWN1:
		case GB_UNKNOWN2:
		case PA_SHORTS:
		case PA_SOCKS:
		case PB_SHORTS:
		case PB_SOCKS:
			return TRUE;
	}
	return FALSE;
}

BOOL IsKitTexture(DWORD id)
{
	DWORD ordinal = (id - data[FIRST_ID]) % FILES_PER_TEAM;
	switch (ordinal) {
		case GA_KIT:
		case GB_KIT:
		case PA_SHIRT:
		case PA_SHORTS:
		case PA_SOCKS:
		case PB_SHIRT:
		case PB_SHORTS:
		case PB_SOCKS:
			return TRUE;
	}
	return FALSE;
}

DWORD GetSplitKitOffset(DWORD id)
{
	DWORD ordinal = (id - data[FIRST_ID]) % FILES_PER_TEAM;
	switch (ordinal) {
		case PA_SHIRT:
		case PB_SHIRT:
            return 0;
		case PA_SHORTS:
		case PB_SHORTS:
			return 1;
		case PA_SOCKS:
		case PB_SOCKS:
			return 2;
	}
	return 0xffffffff;
}

BOOL IsAllInOneTexture(BITMAPINFO* tex)
{
    int i;
	BITMAPINFOHEADER* bih = (BITMAPINFOHEADER*)tex;
	DWORD* pixels = (DWORD*)((BYTE*)tex + bih->biSize + 0x400);
    if (g_testMask == NULL) return FALSE;
    for (i=0; i<bih->biWidth*bih->biHeight/sizeof(DWORD); i++) {
        if (pixels[i] & g_testMask[i]) return TRUE;
    }
    return FALSE;
}

void MaskKitTexture(BITMAPINFO* tex, DWORD id)
{
	int i;
	BITMAPINFOHEADER* bih = (BITMAPINFOHEADER*)tex;
	DWORD* pixels = (DWORD*)((BYTE*)tex + bih->biSize + 0x400);

	DWORD ordinal = (id - data[FIRST_ID]) % FILES_PER_TEAM;
	switch (ordinal) {
		case 0:
		case 5:
		case 10:
		case 15:
			// shirt
			if (g_shirtMask == NULL) break;
			for (i=0; i<bih->biWidth*bih->biHeight/sizeof(DWORD); i++) {
				pixels[i] &= g_shirtMask[i];
			}
			break;
        case 1:
        case 6:
		case 11:
		case 16:
			// shorts
			if (g_shortsMask == NULL) break;
			for (i=0; i<bih->biWidth*bih->biHeight/sizeof(DWORD); i++) {
				pixels[i] &= g_shortsMask[i];
			}
			break;
        case 2:
        case 7:
		case 12:
		case 17:
			// socks
			if (g_socksMask == NULL) break;
			for (i=0; i<bih->biWidth*bih->biHeight/sizeof(DWORD); i++) {
				pixels[i] &= g_socksMask[i];
			}
			break;
	}
}

void MaskKitTextureMip1(BITMAPINFO* tex, DWORD id)
{
	int i;
	BITMAPINFOHEADER* bih = (BITMAPINFOHEADER*)tex;
	DWORD* pixels = (DWORD*)((BYTE*)tex + bih->biSize + 0x400);

	DWORD ordinal = (id - data[FIRST_ID]) % FILES_PER_TEAM;
	switch (ordinal) {
		case 0:
		case 5:
		case 10:
		case 15:
			// shirt
			if (g_shirtMaskMip1 == NULL) break;
			for (i=0; i<bih->biWidth*bih->biHeight/sizeof(DWORD); i++) {
				pixels[i] &= g_shirtMaskMip1[i];
			}
			break;
		case 1:
		case 6:
		case 11:
		case 16:
			// shorts
			if (g_shortsMaskMip1 == NULL) break;
			for (i=0; i<bih->biWidth*bih->biHeight/sizeof(DWORD); i++) {
				pixels[i] &= g_shortsMaskMip1[i];
			}
			break;
		case 2:
		case 7:
		case 12:
		case 17:
			// socks
			if (g_socksMaskMip1 == NULL) break;
			for (i=0; i<bih->biWidth*bih->biHeight/sizeof(DWORD); i++) {
				pixels[i] &= g_socksMaskMip1[i];
			}
			break;
	}
}

void MaskKitTextureMip2(BITMAPINFO* tex, DWORD id)
{
	int i;
	BITMAPINFOHEADER* bih = (BITMAPINFOHEADER*)tex;
	DWORD* pixels = (DWORD*)((BYTE*)tex + bih->biSize + 0x400);

	DWORD ordinal = (id - data[FIRST_ID]) % FILES_PER_TEAM;
	switch (ordinal) {
		case 0:
		case 5:
		case 10:
		case 15:
			// shirt
			if (g_shirtMaskMip2 == NULL) break;
			for (i=0; i<bih->biWidth*bih->biHeight/sizeof(DWORD); i++) {
				pixels[i] &= g_shirtMaskMip2[i];
			}
			break;
		case 1:
		case 6:
		case 11:
		case 16:
			// shorts
			if (g_shortsMaskMip2 == NULL) break;
			for (i=0; i<bih->biWidth*bih->biHeight/sizeof(DWORD); i++) {
				pixels[i] &= g_shortsMaskMip2[i];
			}
			break;
		case 2:
		case 7:
		case 12:
		case 17:
			// socks
			if (g_socksMaskMip2 == NULL) break;
			for (i=0; i<bih->biWidth*bih->biHeight/sizeof(DWORD); i++) {
				pixels[i] &= g_socksMaskMip2[i];
			}
			break;
	}
}

BOOL TeamDirExists(DWORD id) 
{
    KitCollection* col = MAP_FIND(gdb->uni,id);
    return (col)?TRUE:FALSE;
}

void SetKitInfo(Kit* kit, KITINFO* kitInfo, BOOL editable)
{
    //LogWithNumber(&k_mydll, "SetKitInfo: kitType is at %08x", (DWORD)&(kitInfo->kitType));
	// set kit type
    kitInfo->kitType = 1;

	// set collar
	if (kit && (kit->attDefined & COLLAR)) {
		kitInfo->collar = kit->collar;
	}
	// set model
	if (kit && (kit->attDefined & MODEL)) {
		kitInfo->model = kit->model;
	}
	// set name location
	if (kit && (kit->attDefined & NAME_LOCATION)) {
		kitInfo->nameLocation = kit->nameLocation;
	}
	// set logo location
	if (kit && (kit->attDefined & LOGO_LOCATION)) {
		kitInfo->logoLocation = kit->logoLocation;
	}
   	// set name shape
	if (kit && (kit->attDefined & NAME_SHAPE)) {
		kitInfo->nameShape = kit->nameShape;
	}
    // set shirt number position
	if (kit && (kit->attDefined & SHIRT_NUMBER_LOCATION)) {
		kitInfo->shirtNumberLocation = kit->shirtNumberLocation;
	}
	
	// set radar color
	if (kit && (kit->attDefined & RADAR_COLOR)) {
		kitInfo->shirtColors[0] = 0x8000
			+((kit->radarColor.r>>3) & 31)
			+0x20*((kit->radarColor.g>>3) & 31)
			+0x400*((kit->radarColor.b>>3) & 31);
	}
	
	// set shorts main color (for under-shorts)
	if (kit && (kit->attDefined & SHORTS_MAIN_COLOR)) {
		kitInfo->shortsColors[0] = 0x8000
			+((kit->shortsMainColor.r>>3) & 31)
			+0x20*((kit->shortsMainColor.g>>3) & 31)
			+0x400*((kit->shortsMainColor.b>>3) & 31);
	}
	
    /*
	// set name type
	if (kit && (kit->attDefined & NAME_TYPE)) {
		kitInfo->nameType = kit->nameType;
	}
	// set number type
	if (kit && (kit->attDefined & NUMBER_TYPE)) {
		kitInfo->numberType = kit->numberType;
	}

    if (kit && strstr(kit->foldername,"France")) {
        FILE* f = fopen("kitserver\\kitinfo.bin","wb");
        for (int i=0; i<sizeof(KITINFO); i++) fprintf(f, "%c", ((BYTE*)kitInfo)[i]);
        fclose(f);
    }
    */
}

DWORD RGBAColor2DWORD(RGBAColor color)
{
	DWORD res = color.r;
	res += (color.g <<  8);
	res += (color.b << 16);
	res += (color.a << 24);
	return res;
};

DWORD kservSetRadarColor()
{
	DWORD res=MasterCallNext();

	for (int i=0; i<2; i++) {
		if (savedRadarColor[i]) {
			DWORD dwColor=RGBAColor2DWORD(*savedRadarColor[i]);
			((DWORD*)data[RADARCOLORS])[i]=dwColor;
			//LogWithTwoNumbers(&k_mydll, "Set radar color %d to %.8x", i, dwColor);
		};
	};

	return res;
};

void SetShortsInfo(Kit* kit, KITINFO* kitInfo, BOOL editable)
{
    // set shorts number position
	if (kit && (kit->attDefined & SHORTS_NUMBER_LOCATION)) {
		kitInfo->shortsNumberLocation = kit->shortsNumberLocation;
	}
}

void ResetKitPackInfo(KITPACKINFO* kitPackInfo, KITPACKINFO* saved)
{
    /*
	kitPackInfo->gkHome.kitType = saved->gkHome.kitType;
	kitPackInfo->gkHome.collar = saved->gkHome.collar;
	kitPackInfo->gkHome.model = saved->gkHome.model;
	kitPackInfo->gkHome.shirtNumberLocation = saved->gkHome.shirtNumberLocation;
	kitPackInfo->gkHome.shortsNumberLocation = saved->gkHome.shortsNumberLocation;
	kitPackInfo->gkHome.nameLocation = saved->gkHome.nameLocation;
	kitPackInfo->gkHome.logoLocation = saved->gkHome.logoLocation;
	kitPackInfo->gkHome.nameShape = saved->gkHome.nameShape;
	kitPackInfo->gkHome.shirtColors[0] = saved->gkHome.shirtColors[0];
	//kitPackInfo->gkHome.nameType = saved->gkHome.nameType;
	//kitPackInfo->gkHome.numberType = saved->gkHome.numberType;

	kitPackInfo->gkAway.kitType = saved->gkAway.kitType;
	kitPackInfo->gkAway.collar = saved->gkAway.collar;
	kitPackInfo->gkAway.model = saved->gkAway.model;
	kitPackInfo->gkAway.shirtNumberLocation = saved->gkAway.shirtNumberLocation;
	kitPackInfo->gkAway.shortsNumberLocation = saved->gkAway.shortsNumberLocation;
	kitPackInfo->gkAway.nameLocation = saved->gkAway.nameLocation;
	kitPackInfo->gkAway.logoLocation = saved->gkAway.logoLocation;
	kitPackInfo->gkAway.nameShape = saved->gkAway.nameShape;
	kitPackInfo->gkAway.shirtColors[0] = saved->gkAway.shirtColors[0];
	//kitPackInfo->gkAway.nameType = saved->gkAway.nameType;
	//kitPackInfo->gkAway.numberType = saved->gkAway.numberType;

	kitPackInfo->plHome.kitType = saved->plHome.kitType;
	kitPackInfo->plHome.collar = saved->plHome.collar;
	kitPackInfo->plHome.model = saved->plHome.model;
	kitPackInfo->plHome.shirtNumberLocation = saved->plHome.shirtNumberLocation;
	kitPackInfo->plHome.shortsNumberLocation = saved->plHome.shortsNumberLocation;
	kitPackInfo->plHome.nameLocation = saved->plHome.nameLocation;
	kitPackInfo->plHome.logoLocation = saved->plHome.logoLocation;
	kitPackInfo->plHome.nameShape = saved->plHome.nameShape;
	kitPackInfo->plHome.shirtColors[0] = saved->plHome.shirtColors[0];
	//kitPackInfo->plHome.nameType = saved->plHome.nameType;
	//kitPackInfo->plHome.numberType = saved->plHome.numberType;

	kitPackInfo->plAway.kitType = saved->plAway.kitType;
	kitPackInfo->plAway.collar = saved->plAway.collar;
	kitPackInfo->plAway.model = saved->plAway.model;
	kitPackInfo->plAway.shirtNumberLocation = saved->plAway.shirtNumberLocation;
	kitPackInfo->plAway.shortsNumberLocation = saved->plAway.shortsNumberLocation;
	kitPackInfo->plAway.nameLocation = saved->plAway.nameLocation;
	kitPackInfo->plAway.logoLocation = saved->plAway.logoLocation;
	kitPackInfo->plAway.nameShape = saved->plAway.nameShape;
	kitPackInfo->plAway.shirtColors[0] = saved->plAway.shirtColors[0];
	//kitPackInfo->plAway.nameType = saved->plAway.nameType;
	//kitPackInfo->plAway.numberType = saved->plAway.numberType;
    */
    memcpy(kitPackInfo, saved, sizeof(KITPACKINFO));
}

void ResetTeamInfo(KITPACKINFO* kitPackInfo, TEAMKITINFO* saved)
{
    memcpy(kitPackInfo, &saved->kits, sizeof(KITPACKINFO));
    size_t len = (saved->teamId < 0x40) ? 0x68 : 0x128;
    memcpy(kitPackInfo + 1, &saved->editInfo, len);
}


/**
 * This function calls GetClubTeamInfo() function
 * Parameter:
 *   id   - team id
 * Return value:
 *   address of the KITPACKINFO structure
 *//*
void JuceGetClubTeamInfo(DWORD id,DWORD result)
{
	TRACE2(&k_mydll,"JuceGetClubTeamInfo: CALLED for id = %003d.", id);

	if (id == 0xf2) {
		// master league team (home)
		BYTE* mlData = *((BYTE**)data[ML_HOME_AREA]);
		id = *((DWORD*)(mlData + 0x6c)) & 0xffff; // 3rd byte is a flag of "edited" kit
	} else if (id == 0xf3) {
		// master league team (away)
		BYTE* mlData = *((BYTE**)data[ML_AWAY_AREA]);
		id = *((DWORD*)(mlData + 0x6c)) & 0xffff; // 3rd byte is a flag of "edited" kit
	}

	if (id >= 64 && id < 255 && TeamDirExists(id)) {
		// check if we need to store it in the hash-map
		if (g_teamKitInfo[result] == NULL) {
			// not yet saved: save it
			TEAMKITINFO* teamKitInfo = (TEAMKITINFO*)HeapAlloc(GetProcessHeap(),
					0, sizeof(TEAMKITINFO));
			if (teamKitInfo) {
				memcpy(&teamKitInfo->kits, (KITPACKINFO*)result, sizeof(KITPACKINFO));
				teamKitInfo->teamId = id;
				g_teamKitInfo[result] = teamKitInfo;
				//DumpData((BYTE*)&teamKitInfo->kits, sizeof(KITPACKINFO));
			} else {
				Log(&k_mydll,"JuceGetClubTeamInfo: SEVERE PROBLEM: unable to allocate memory for kitPackInfo");
			}
            LogWithNumber(&k_mydll,"JuceGetClubTeamInfo: stored id = %003d.", id);
		}

		KITPACKINFO* kitPackInfo = (KITPACKINFO*)result;

        KitCollection* col = MAP_FIND(gdb->uni,id);
        if (col) {
            Kit* ga = MAP_FIND(col->goalkeepers,GetKitFolderKey(data[FIRST_ID]+id*FILES_PER_TEAM+GA_KIT));
            Kit* gb = MAP_FIND(col->goalkeepers,GetKitFolderKey(data[FIRST_ID]+id*FILES_PER_TEAM+GB_KIT));
            Kit* gaShorts = MAP_FIND(col->goalkeepers,GetKitFolderKey(data[FIRST_ID]+id*FILES_PER_TEAM+GA_UNKNOWN1));
            Kit* gbShorts = MAP_FIND(col->goalkeepers,GetKitFolderKey(data[FIRST_ID]+id*FILES_PER_TEAM+GB_UNKNOWN1));
            Kit* pa = MAP_FIND(col->players,GetKitFolderKey(data[FIRST_ID]+id*FILES_PER_TEAM+PA_SHIRT));
            Kit* pb = MAP_FIND(col->players,GetKitFolderKey(data[FIRST_ID]+id*FILES_PER_TEAM+PB_SHIRT));
            Kit* paShorts = MAP_FIND(col->players,GetKitFolderKey(data[FIRST_ID]+id*FILES_PER_TEAM+PA_SHORTS));
            Kit* pbShorts = MAP_FIND(col->players,GetKitFolderKey(data[FIRST_ID]+id*FILES_PER_TEAM+PB_SHORTS));
            
            //BOOL editable = kDB->editable[id];
            BOOL editable = TRUE;

            // set attributes
            SetKitInfo(ga, &kitPackInfo->gkHome, editable);
            SetKitInfo(gb, &kitPackInfo->gkAway, editable);
            SetShortsInfo(gaShorts, &kitPackInfo->gkHome, editable);
            SetShortsInfo(gbShorts, &kitPackInfo->gkAway, editable);
            SetKitInfo(pa, &kitPackInfo->plHome, editable);
            SetKitInfo(pb, &kitPackInfo->plAway, editable);
            SetShortsInfo(paShorts, &kitPackInfo->plHome, editable);
            SetShortsInfo(pbShorts, &kitPackInfo->plAway, editable);
        }
	}
	return;
}
*/
void restoreLicensedOrdinals()
{
	Log(&k_mydll,"restoreLicensedOrdinals: putting back 4 and 6");
    WORD* licensed_ids = (WORD*)data[LICENSED_LIST];
    licensed_ids[0] = g_licensed_ordinals[0];
    licensed_ids[1] = g_licensed_ordinals[1];
}

void clearTeamKitInfo()
{
	savedRadarColor[0]=NULL;
    savedRadarColor[1]=NULL;
	
	if (g_teamKitInfo.size() == 0) {
		return;
	}

	Log(&k_mydll,"clearTeamKitInfo: restoring kitinfo edits");

	HANDLE pHeap = GetProcessHeap();
	for(g_teamKitInfoIterator = g_teamKitInfo.begin(); 
			g_teamKitInfoIterator != g_teamKitInfo.end();
			g_teamKitInfoIterator++)
	{
		KITPACKINFO* kitPackInfo = (KITPACKINFO*)g_teamKitInfoIterator->first;
		TEAMKITINFO* savedTeamInfo = g_teamKitInfoIterator->second;

		LogWithTwoNumbers(&k_mydll,"clearTeamKitInfo: restoring info for %03d at %08x", 
				savedTeamInfo->teamId, (DWORD)kitPackInfo);

		// reset licensed flags
        //ResetKitPackInfo(kitPackInfo, &savedTeamInfo->kits);
        ResetTeamInfo(kitPackInfo, savedTeamInfo);

		// free the memory for saved pack
		HeapFree(pHeap, 0, savedTeamInfo);
	}

	// clear the hash-map
	g_teamKitInfo.clear();

    // disable kit loading
    g_kit_loading_enabled = false;
}
/*
void DoMipMap(DWORD id, TEXIMGPACKHEADER* pack, int ordinal, char* fileSuffix, MASKFUNCPROC maskFunc)
{
    // STEP1: check if either BMP or PNG file exists
    char filename[BUFLEN];
    // flag to apply mask
    BOOL needsMask = FALSE;
    DWORD texType = FindImageFileForId(id, fileSuffix, filename, &needsMask);

    BITMAPINFO* tex = NULL;
    DWORD texSize = 0;

    switch (texType) {
        case TEXTYPE_PNG:
            LogWithString(&k_mydll,"JuceUniDecode: Image file = %s", filename);
            texSize = LoadPNGTexture(&tex, filename);
            if (texSize > 0) {
                if (IsShortsOrSocks(id)) {
                    if (needsMask) {
                        maskFunc(tex, id);
                    } 
                } else if (IsShirt(id)) {
                    if (IsAllInOneTexture(tex)) {
                        // this is all-in-one texture loaded in place
                        // of shirt texture. apply the mask.
                        maskFunc(tex, id);
                    }
                }
                ApplyDIBTextureMipMap(pack, ordinal, tex);
                FreePNGTexture(tex);
            }
            break;
        case TEXTYPE_BMP:
            LogWithString(&k_mydll,"JuceUniDecode: Image file = %s", filename);
            texSize = LoadTexture(&tex, filename);
            if (texSize > 0) {
                if (IsShortsOrSocks(id)) {
                    if (needsMask) {
                        maskFunc(tex, id);
                    } 
                } else if (IsShirt(id)) { 
                    if (IsAllInOneTexture(tex)) {
                        // this is all-in-one texture loaded in place
                        // of shirt texture. apply the mask.
                        maskFunc(tex, id);
                    }
                }
                ApplyDIBTextureMipMap(pack, ordinal, tex);
                FreeTexture(tex);
            }
            break;

    } // end switch
}
*/
void JuceSet2Dkits()
{
    g_display2Dkits = TRUE;

    // initialize home iterators
    WORD teamId = GetTeamId(HOME);
    if (teamId != 0xffff) {
    	gdbFindKitsForTeam(gdb, teamId);
        KitCollection* col = MAP_FIND(gdb->uni,teamId);
        if (col) {
            g_homeShirtIteratorPL = (*(col->players)).begin();
            g_homeShirtIteratorGK = (*(col->goalkeepers)).begin();
            g_homeShortsIteratorPL = (*(col->players)).begin();
            g_homeShortsIteratorGK = (*(col->goalkeepers)).begin();
            g_homeSocksIteratorPL = (*(col->players)).begin();
            g_homeSocksIteratorGK = (*(col->goalkeepers)).begin();

            g_homeShirtIterator = (typ == PL_TYPE) ? 
                g_homeShirtIteratorPL : g_homeShirtIteratorGK;
            g_homeShortsIterator = (typ == PL_TYPE) ? 
                g_homeShirtIteratorPL : g_homeShirtIteratorGK;
            g_homeSocksIterator = (typ == PL_TYPE) ? 
                g_homeShirtIteratorPL : g_homeShirtIteratorGK;
        }
    }
    // initialize away iterators
    teamId = GetTeamId(AWAY);
    if (teamId != 0xffff) {
    	gdbFindKitsForTeam(gdb, teamId);
        KitCollection* col = MAP_FIND(gdb->uni,teamId);
        if (col) {
            g_awayShirtIteratorPL = (*(col->players)).begin();
            g_awayShirtIteratorGK = (*(col->goalkeepers)).begin();
            g_awayShortsIteratorPL = (*(col->players)).begin();
            g_awayShortsIteratorGK = (*(col->goalkeepers)).begin();
            g_awaySocksIteratorPL = (*(col->players)).begin();
            g_awaySocksIteratorGK = (*(col->goalkeepers)).begin();

            g_awayShirtIterator = (typ == PL_TYPE) ? 
                g_awayShirtIteratorPL : g_awayShirtIteratorGK;
            g_awayShortsIterator = (typ == PL_TYPE) ? 
                g_awayShirtIteratorPL : g_awayShirtIteratorGK;
            g_awaySocksIterator = (typ == PL_TYPE) ? 
                g_awayShirtIteratorPL : g_awayShirtIteratorGK;
        }
    }

    // hook Present method
    HookFunction(hk_D3D_Present,(DWORD)JucePresent);
    Log(&k_mydll,"Present hooked.");
	
    // reset the keys to defaults
    ResetKitKeys(true);

    g_last_home = GetTeamId(HOME);
    g_last_away = GetTeamId(AWAY);

    Log(&k_mydll,"Show 2D kits");
    return;
}

bool IsLicensed(WORD teamId)
{
    for (int i=0; i<sizeof(g_licensed_ordinals)/sizeof(WORD); i++) {
        if (teamId == g_licensed_ordinals[i]) {
            return true;
        }
    }
    return false;
}

DWORD JuceReadTeamInfoOnReplaySave(DWORD id, DWORD p1)
{
	Log(&k_mydll,"JuceReadTeamInfoOnReplaySave: restoring kitinfo edits");

    clearTeamKitInfo();

    // call original
    DWORD result = MasterCallNext(id, p1);
    return result;
}

DWORD JuceReadTeamInfoOnReplaySave2(DWORD id)
{
	Log(&k_mydll,"JuceReadTeamInfoOnReplaySave2: restoring kitinfo edits");

    clearTeamKitInfo();

    // call original
    DWORD result = MasterCallNext(id);
    return result;
}

DWORD JuceReadTeamInfoOnMLSave()
{
	Log(&k_mydll,"JuceReadTeamInfoOnMLSave: restoring kitinfo edits");

    clearTeamKitInfo();

    // call original
    DWORD result = MasterCallNext();
    return result;
}

DWORD kservSetFlag()
{
	Log(&k_mydll,"kservSetFlag: restoring kitinfo edits");

    restoreLicensedOrdinals();
    clearTeamKitInfo();
    g_kit_loading_enabled = false;

    // clear texture replacement maps
    ClearTextureMaps();

    // call original
    DWORD result = MasterCallNext();
    return result;
}

DWORD kservResetFlag()
{
	Log(&k_mydll,"kservResetFlag: restoring kitinfo edits");

    restoreLicensedOrdinals();
    clearTeamKitInfo();
    g_kit_loading_enabled = false;
    g_edit_mode = false;

    // clear texture replacement maps
    ClearTextureMaps();

    // call original
    DWORD result = MasterCallNext();
    return result;
}

DWORD kservResetFlag2()
{
	Log(&k_mydll,"kservResetFlag2: restoring kitinfo edits");

    restoreLicensedOrdinals();
    clearTeamKitInfo();
    g_kit_loading_enabled = false;
    g_edit_mode = false;

    // clear texture replacement maps
    ClearTextureMaps();

    // call original
    DWORD result = MasterCallNext();
    return result;
}

void ClearTextureMaps()
{
    Log(&k_mydll, "ClearTextureMaps() CALLED");
    _texture_to_id.clear();
    _source_to_id.clear();
}

DWORD kservProcessKit(DWORD dest, DWORD src)
{
	Log(&k_mydll,"kservProcessKit: processing kit");
    LogWithNumber(&k_mydll, "kservProcessKit: destination buffer: %08x", dest);
    LogWithNumber(&k_mydll, "kservProcessKit: source buffer: %08x", src);

    // call original
    DWORD result = MasterCallNext(dest,src);
    LogWithNumber(&k_mydll, "kservProcessKit: kit buffer addresses stored here: %08x", result);

    hash_map<DWORD,DWORD>::iterator it = _texture_to_id.find(src - 0x10);
    if (it != _texture_to_id.end()) {
        _source_to_id[dest] = it->second;
        _texture_to_id.erase(it);
        LogWithTwoNumbers(&k_mydll, "kservProcessKit: made mapping: %08x -> %d", dest, it->second);

    } else {
        hash_map<DWORD,DWORD>::iterator sit = _texture_to_id.find(src - 0x20);
        if (sit != _texture_to_id.end()) {
            _source_to_id[dest] = sit->second;
            _texture_to_id.erase(sit);
            LogWithTwoNumbers(&k_mydll, "kservProcessKit: made mapping: %08x -> %d", dest, sit->second);

        } else {
            // nothing found. Make sure that we remove this dest key from _source_to_id, 
            // if it exists in there
            /*
            hash_map<DWORD,DWORD>::iterator dit = _source_to_id.find(dest);
            if (dit != _source_to_id.end()) {
                _source_to_id.erase(dit);
            }
            */
        }
    } 
    return result;
}

void setTeamKitInfo()
{
    // overwrite kit information
    /////////////////////////////////////
    /////////////////////////////////////
    // Make the game think the teams are licensed

    WORD home = GetTeamId(HOME);
    WORD away = GetTeamId(AWAY);
    
    savedRadarColor[0]=NULL;
    savedRadarColor[1]=NULL;

    if (TeamDirExists(home))
    {
        KITPACKINFO* ki = GetKitPackInfo(HOME);

		// check if we need to store it in the hash-map
		if (g_teamKitInfo[(DWORD)ki] == NULL) {
			// not yet saved: save it
			TEAMKITINFO* teamKitInfo = (TEAMKITINFO*)HeapAlloc(GetProcessHeap(),
					0, sizeof(TEAMKITINFO));
			if (teamKitInfo) {
				memcpy(&teamKitInfo->kits, ki, sizeof(KITPACKINFO));
				teamKitInfo->teamId = home;
				g_teamKitInfo[(DWORD)ki] = teamKitInfo;
                size_t len = (home < 0x40) ? 0x68 : 0x128;
                memcpy(&teamKitInfo->editInfo, ki+1, len);
			} else {
				Log(&k_mydll,"setTeamKitInfo: SEVERE PROBLEM: unable to allocate memory for kitPackInfo");
			}
            LogWithNumber(&k_mydll,"setTeamKitInfo: stored id = %003d.", home);
		}

        gdbFindKitsForTeam(gdb, home);
        KitCollection* col = MAP_FIND(gdb->uni,home);
        if (col) {
            Kit* ga = MAP_FIND(col->goalkeepers,GetKitFolderKeyByTeamId(home, GA_KIT));
            Kit* gb = MAP_FIND(col->goalkeepers,GetKitFolderKeyByTeamId(home, GB_KIT));
            Kit* gaShorts = MAP_FIND(col->goalkeepers,GetKitFolderKeyByTeamId(home, GA_KIT));
            Kit* gbShorts = MAP_FIND(col->goalkeepers,GetKitFolderKeyByTeamId(home, GB_KIT));
            Kit* pa = MAP_FIND(col->players,GetKitFolderKeyByTeamId(home, PA_SHIRT));
            Kit* pb = MAP_FIND(col->players,GetKitFolderKeyByTeamId(home, PB_SHIRT));
            Kit* paShorts = MAP_FIND(col->players,GetKitFolderKeyByTeamId(home, PA_SHIRT));
            Kit* pbShorts = MAP_FIND(col->players,GetKitFolderKeyByTeamId(home, PB_SHIRT));

            //BOOL editable = kDB->editable[id];
            BOOL editable = TRUE;

            // set attributes
            SetKitInfo(ga, &ki->gkHome, editable);
            SetKitInfo(gb, &ki->gkAway, editable);
            SetShortsInfo(gaShorts, &ki->gkHome, editable);
            SetShortsInfo(gbShorts, &ki->gkAway, editable);
            SetKitInfo(pa, &ki->plHome, editable);
            SetKitInfo(pb, &ki->plAway, editable);
            SetShortsInfo(paShorts, &ki->plHome, editable);
            SetShortsInfo(pbShorts, &ki->plAway, editable);
            
            Kit* selPlKit=(GetTeamStrips(HOME) & STRIP_PL_SHIRT == 0)?pa:pb;
            if (home == away) selPlKit=pa;
            if (selPlKit && (selPlKit->attDefined & RADAR_COLOR)) {
				savedRadarColor[0]=&selPlKit->radarColor;
			};

            // clear out the padding
            size_t len = (home < 0x40) ? 0x68 : 0x128;
            BYTE* padding = (BYTE*)(ki + 1);
            memset(padding,0,len);

            if (!IsLicensed(home)) {
                WORD* list = (WORD*)data[LICENSED_LIST];
                list[(away!=4)?0:1] = home;
            }
        }
    }
    if (TeamDirExists(away))
    {
        KITPACKINFO* ki = GetKitPackInfo(AWAY);

		// check if we need to store it in the hash-map
		if (g_teamKitInfo[(DWORD)ki] == NULL) {
			// not yet saved: save it
			TEAMKITINFO* teamKitInfo = (TEAMKITINFO*)HeapAlloc(GetProcessHeap(),
					0, sizeof(TEAMKITINFO));
			if (teamKitInfo) {
				memcpy(&teamKitInfo->kits, ki, sizeof(KITPACKINFO));
				teamKitInfo->teamId = away;
				g_teamKitInfo[(DWORD)ki] = teamKitInfo;
                size_t len = (away < 0x40) ? 0x68 : 0x128;
                memcpy(&teamKitInfo->editInfo, ki+1, len);
			} else {
				Log(&k_mydll,"setTeamKitInfo: SEVERE PROBLEM: unable to allocate memory for kitPackInfo");
			}
            LogWithNumber(&k_mydll,"setTeamKitInfo: stored id = %003d.", away);
		}

        gdbFindKitsForTeam(gdb, away);
        KitCollection* col = MAP_FIND(gdb->uni,away);
        if (col) {
            Kit* ga = MAP_FIND(col->goalkeepers,GetKitFolderKeyByTeamId(away, GA_KIT));
            Kit* gb = MAP_FIND(col->goalkeepers,GetKitFolderKeyByTeamId(away, GB_KIT));
            Kit* gaShorts = MAP_FIND(col->goalkeepers,GetKitFolderKeyByTeamId(away, GA_KIT));
            Kit* gbShorts = MAP_FIND(col->goalkeepers,GetKitFolderKeyByTeamId(away, GB_KIT));
            Kit* pa = MAP_FIND(col->players,GetKitFolderKeyByTeamId(away, PA_SHIRT));
            Kit* pb = MAP_FIND(col->players,GetKitFolderKeyByTeamId(away, PB_SHIRT));
            Kit* paShorts = MAP_FIND(col->players,GetKitFolderKeyByTeamId(away, PA_SHIRT));
            Kit* pbShorts = MAP_FIND(col->players,GetKitFolderKeyByTeamId(away, PB_SHIRT));
            
            //BOOL editable = kDB->editable[id];
            BOOL editable = TRUE;

            // set attributes
            SetKitInfo(ga, &ki->gkHome, editable);
            SetKitInfo(gb, &ki->gkAway, editable);
            SetShortsInfo(gaShorts, &ki->gkHome, editable);
            SetShortsInfo(gbShorts, &ki->gkAway, editable);
            SetKitInfo(pa, &ki->plHome, editable);
            SetKitInfo(pb, &ki->plAway, editable);
            SetShortsInfo(paShorts, &ki->plHome, editable);
            SetShortsInfo(pbShorts, &ki->plAway, editable);
            
			Kit* selPlKit=(GetTeamStrips(AWAY) & STRIP_PL_SHIRT == 0)?pa:pb;
			if (home == away) selPlKit=pb;
            if (selPlKit && (selPlKit->attDefined & RADAR_COLOR)) {
				savedRadarColor[1]=&selPlKit->radarColor;
			};

            // clear out the padding
            size_t len = (away < 0x40) ? 0x68 : 0x128;
            BYTE* padding = (BYTE*)(ki + 1);
            memset(padding,0,len);

            if (!IsLicensed(away)) {
                WORD* list = (WORD*)data[LICENSED_LIST];
                list[(home!=6)?1:0] = away;
            }
        }
    }

    // enable kit loading
    g_kit_loading_enabled = true;
}

DWORD kservWriteKitInfo(DWORD teamId, DWORD kitOrdinal)
{
	//LogWithTwoNumbers(&k_mydll,"kservWriteKitInfo: teamId=%04x, kitOrdinal=%d", teamId & 0xffff, kitOrdinal);

    // call original
    DWORD result = MasterCallNext(teamId, kitOrdinal);

    if (g_edit_mode) return result;

    /*
    teamId &= 0xffff;
    if (teamId == 0x126) teamId = GetTeamId(HOME);
    if (teamId == 0x127) teamId = GetTeamId(AWAY);

    if (kitOrdinal==0 && TeamDirExists(teamId))
    {
        KITPACKINFO* ki = (KITPACKINFO*)result - 1;

		// check if we need to store it in the hash-map
		if (g_teamKitInfo[(DWORD)ki] == NULL) {
			// not yet saved: save it
			TEAMKITINFO* teamKitInfo = (TEAMKITINFO*)HeapAlloc(GetProcessHeap(),
					0, sizeof(TEAMKITINFO));
			if (teamKitInfo) {
				memcpy(&teamKitInfo->kits, ki, sizeof(KITPACKINFO));
				teamKitInfo->teamId = teamId;
				g_teamKitInfo[(DWORD)ki] = teamKitInfo;
                size_t len = (teamId < 0x40) ? 0x68 : 0x128;
                memcpy(&teamKitInfo->editInfo, ki+1, len);
			} else {
				Log(&k_mydll,"setTeamKitInfo: SEVERE PROBLEM: unable to allocate memory for kitPackInfo");
			}
            LogWithNumber(&k_mydll,"setTeamKitInfo: stored id = %003d.", teamId);
		}

        KitCollection* col = MAP_FIND(gdb->uni,teamId);
        if (col) {
            Kit* ga = MAP_FIND(col->goalkeepers,GetKitFolderKeyByTeamId(teamId, GA_KIT));
            Kit* gb = MAP_FIND(col->goalkeepers,GetKitFolderKeyByTeamId(teamId, GB_KIT));
            Kit* gaShorts = MAP_FIND(col->goalkeepers,GetKitFolderKeyByTeamId(teamId, GA_KIT));
            Kit* gbShorts = MAP_FIND(col->goalkeepers,GetKitFolderKeyByTeamId(teamId, GB_KIT));
            Kit* pa = MAP_FIND(col->players,GetKitFolderKeyByTeamId(teamId, PA_SHIRT));
            Kit* pb = MAP_FIND(col->players,GetKitFolderKeyByTeamId(teamId, PB_SHIRT));
            Kit* paShorts = MAP_FIND(col->players,GetKitFolderKeyByTeamId(teamId, PA_SHIRT));
            Kit* pbShorts = MAP_FIND(col->players,GetKitFolderKeyByTeamId(teamId, PB_SHIRT));

            //BOOL editable = kDB->editable[id];
            BOOL editable = TRUE;

            // set attributes
            SetKitInfo(ga, &ki->gkHome, editable);
            SetKitInfo(gb, &ki->gkAway, editable);
            SetShortsInfo(gaShorts, &ki->gkHome, editable);
            SetShortsInfo(gbShorts, &ki->gkAway, editable);
            SetKitInfo(pa, &ki->plHome, editable);
            SetKitInfo(pb, &ki->plAway, editable);
            SetShortsInfo(paShorts, &ki->plHome, editable);
            SetShortsInfo(pbShorts, &ki->plAway, editable);

            // clear out the padding
            size_t len = (teamId < 0x40) ? 0x68 : 0x128;
            BYTE* padding = (BYTE*)(ki + 1);
            memset(padding,0,len);

            if (teamId == GetTeamId(HOME)) {
                if (!IsLicensed(teamId)) {
                    WORD* list = (WORD*)data[LICENSED_LIST];
                    list[(teamId!=4)?0:1] = teamId;
                }
            }
            if (teamId == GetTeamId(AWAY)) {
                if (!IsLicensed(teamId)) {
                    WORD* list = (WORD*)data[LICENSED_LIST];
                    list[(teamId!=6)?1:0] = teamId;
                }
            }
        }
    }
    */
    setTeamKitInfo();

    return result;
}

DWORD JuceOnReplayLoad()
{
	Log(&k_mydll,"JuceOnReplayLoad: CALLED.");

    restoreLicensedOrdinals();
    clearTeamKitInfo();
    g_kit_loading_enabled = true;

    // call original
    DWORD result = MasterCallNext();

    // set kit info
    setTeamKitInfo();

    return result;
}

DWORD JuceGetTeamInfo(DWORD id, DWORD p1)
{
	//Log(&k_mydll,"JuceGetTeamInfo: restoring kitinfo edits");

    restoreLicensedOrdinals();
    clearTeamKitInfo();
    g_kit_loading_enabled = false;
    g_edit_mode = true;

    // call original
    DWORD result = MasterCallNext(id, p1);
    return result;
}

void JuceClear2Dkits()
{
    g_display2Dkits = FALSE;
    g_lastDisplay2Dkits = FALSE;

    // clear the kit texture map
    Log(&k_mydll,"Clearing 2Dkit textures and g_kitTextureMap.");
    g_kitTextureMap.clear();
    SafeRelease(&g_home_shirt_tex);
    SafeRelease(&g_home_shorts_tex);
    SafeRelease(&g_home_socks_tex);
    SafeRelease(&g_away_shirt_tex);
    SafeRelease(&g_away_shorts_tex);
    SafeRelease(&g_away_socks_tex);

    // clear texture replacement maps
    ClearTextureMaps();

    // unhook Present method
	UnhookFunction(hk_D3D_Present,(DWORD)JucePresent);
    Log(&k_mydll,"Present unhooked.");

    // Special logic for a case when team is playing against itself
    WORD id = GetTeamId(HOME); 
    if (id == GetTeamId(AWAY) && id != 0xffff & TeamDirExists(id)) {
        BYTE* strips = (BYTE*)data[TEAM_STRIPS];

        // home: set bits to 0,0,0,0,0,0
        strips[0] = (((strips[0] & STRIP_PL_SHIRT_C) & STRIP_PL_SHORTS_C) & STRIP_PL_SOCKS_C);
        strips[0] = (((strips[0] & STRIP_GK_SHIRT_C) & STRIP_GK_SHORTS_C) & STRIP_GK_SOCKS_C);

        // away: set bits to 1,1,1,1,1,1
        strips[1] = (((strips[1] | STRIP_PL_SHIRT) | STRIP_PL_SHORTS) | STRIP_PL_SOCKS);
        strips[1] = (((strips[1] | STRIP_GK_SHIRT) | STRIP_GK_SHORTS) | STRIP_GK_SOCKS);

        //LogWithTwoNumbers(&k_mydll,"strip = %02x %02x", strips[0], strips[1]);
        *(BYTE*)data[TEAM_STRIPS_HOME] = strips[0];
        *(BYTE*)data[TEAM_STRIPS_AWAY] = strips[1];
    }

    g_homeStrip = 0xff;
    g_awayStrip = 0xff;

    // reset kit selection mode
    typ = PL_TYPE;

    restoreLicensedOrdinals();
    clearTeamKitInfo();
    g_kit_loading_enabled = true;

    // set kit info
    setTeamKitInfo();

    Log(&k_mydll,"Hide 2D kits");
    return;
}

void kservAfsReplace(GETFILEINFO* gfi)
{
	if (gfi->isProcessed) return;
	
	DWORD afsId = 0, fileId = 0;
	fileId = splitFileId(gfi->fileId, &afsId);
	
	if (afsId == 1 && MAP_CONTAINS(g_AFS_idMap, fileId)) {
		// we want to get control over the unpacked file
		gfi->needsUnpack = true;
		gfi->isProcessed = true;
	}
	return;
}
	
// this function gets called on each call to UniDecode
void kservUniDecode(DWORD decBuf)
{
	Log(&k_mydll,"JuceUniDecode: CALLED.");

    g_currentAfsId = 0;
    g_unidecode_flag = true;

    // if new set of teams, clear the texture replacement maps
    // so that we're for sure not going to use wrong kits.
    WORD homeId = GetTeamId(HOME);
    WORD awayId = GetTeamId(AWAY);
    if (homeId != _last_homeId || awayId != _last_awayId) {
        ClearTextureMaps();
    }
    _last_homeId = homeId;
    _last_awayId = awayId;
    
	return;
}

// this function is only called for files we marked above
void kservUnpack(GETFILEINFO* gfi, DWORD part, DWORD decBuf, DWORD size)
{
	if (gfi->isProcessed) return;
		
	if (!g_kit_loading_enabled) return;
	
	DWORD afsId = 0, fileId = 0;
	fileId = splitFileId(gfi->fileId, &afsId);
	
	// the former UniDecode
	if (IsKitTexture(fileId)) {
		// the former UniDecode -> kits
		
		if (g_edit_mode) return;
			
		LogWithNumber(&k_mydll,"JuceUniDecode: decBuf = %08x", decBuf);

		// remember current id
        g_currentAfsId = fileId;

        LogWithNumber(&k_mydll,"JuceUniDecode: Loading id = %d", fileId);
        if (fileId == data[TRAINING_KIT]) {
            return;
        }

		g_kitID = fileId;

        // STEP1: check if either BMP or PNG file exists
        char filename[BUFLEN];
        // flag to apply mask
        BOOL needsMask = FALSE;
        DWORD texType = FindImageFileForId(fileId, "", filename, &needsMask);
        if (texType != TEXTYPE_NONE) {
            LogWithString(&k_mydll,"JuceUniDecode: Image file = %s", filename);
            _texture_to_id[decBuf] = fileId;
        }

        BITMAPINFO* tex = NULL;
        DWORD texSize = 0;

        /*
        switch (texType) {
            case TEXTYPE_PNG:
                LogWithString(&k_mydll,"JuceUniDecode: Image file = %s", filename);
                texSize = LoadPNGTexture(&tex, filename);
                if (texSize > 0) {
                    if (IsShortsOrSocks(fileId)) {
                        if (needsMask) {
                            //MaskKitTexture(tex, fileId);
                        } 
                    } else if (IsShirt(fileId)) {
                        if (IsAllInOneTexture(tex)) {
                            // this is all-in-one texture loaded in place
                            // of shirt texture. apply the mask.
                            //MaskKitTexture(tex, fileId);
                        }
                    }
                    ApplyDIBTexture((TEXIMGPACKHEADER*)decBuf, tex);
                    FreePNGTexture(tex);
                }
                break;
            case TEXTYPE_BMP:
                LogWithString(&k_mydll,"JuceUniDecode: Image file = %s", filename);
                texSize = LoadTexture(&tex, filename);
                if (texSize > 0) {
                    if (IsShortsOrSocks(fileId)) {
                        if (needsMask) {
                            //MaskKitTexture(tex, fileId);
                        } 
                    } else if (IsShirt(fileId)) { 
                        if (IsAllInOneTexture(tex)) {
                            // this is all-in-one texture loaded in place
                            // of shirt texture. apply the mask.
                            //MaskKitTexture(tex, fileId);
                        }
                    }
                    ApplyDIBTexture((TEXIMGPACKHEADER*)decBuf, tex);
                    FreeTexture(tex);
                }
                break;

        } // end switch

        DoMipMap(fileId, (TEXIMGPACKHEADER*)decBuf, 0, "-mip1", MaskKitTextureMip1);
        DoMipMap(fileId, (TEXIMGPACKHEADER*)decBuf, 1, "-mip2", MaskKitTextureMip2);
        */

        // if goalkeeper, extra steps needed:
        if (IsGK(fileId)) {
            // check for gloves
            texType = FindImageFileForId(fileId, "-gloves", filename, &needsMask);
            switch (texType) {
                case TEXTYPE_PNG:
                    LogWithString(&k_mydll,"JuceUniDecode: Gloves file = %s", filename);
                    texSize = LoadPNGTexture(&tex, filename);
                    if (texSize > 0) {
                        ApplyGlovesDIBTexture((TEXIMGPACKHEADER*)decBuf, tex);
                        FreePNGTexture(tex);
                    }
                    break;
                case TEXTYPE_BMP:
                    LogWithString(&k_mydll,"JuceUniDecode: Gloves file = %s", filename);
                    texSize = LoadTexture(&tex, filename);
                    if (texSize > 0) {
                        ApplyGlovesDIBTexture((TEXIMGPACKHEADER*)decBuf, tex);
                        FreeTexture(tex);
                    }
                    break;
            }

            /*
            // load GK shorts
            texType = FindImageFileForId(fileId + 1, "", filename, &needsMask);
            switch (texType) {
                case TEXTYPE_PNG:
                    LogWithString(&k_mydll,"JuceUniDecode: GK shorts file = %s", filename);
                    texSize = LoadPNGTexture(&tex, filename);
                    if (texSize > 0) {
                        if (needsMask) {
                            MaskKitTexture(tex, fileId + 1);
                        }
                        OrDIBTexture((TEXIMGPACKHEADER*)decBuf, tex);
                        FreePNGTexture(tex);
                    }
                    break;
                case TEXTYPE_BMP:
                    LogWithString(&k_mydll,"JuceUniDecode: GK shorts file = %s", filename);
                    texSize = LoadTexture(&tex, filename);
                    if (texSize > 0) {
                        if (needsMask) {
                            MaskKitTexture(tex, fileId + 1);
                        }
                        OrDIBTexture((TEXIMGPACKHEADER*)decBuf, tex);
                        FreeTexture(tex);
                    }
                    break;
            }

            //DoMipMap(fileId + 1, (TEXIMGPACKHEADER*)decBuf, 0, "-mip1", MaskKitTextureMip1);
            //DoMipMap(fileId + 1, (TEXIMGPACKHEADER*)decBuf, 1, "-mip2", MaskKitTextureMip2);

            // load GK socks
            texType = FindImageFileForId(fileId + 2, "", filename, &needsMask);
            switch (texType) {
                case TEXTYPE_PNG:
                    LogWithString(&k_mydll,"JuceUniDecode: GK socks file = %s", filename);
                    texSize = LoadPNGTexture(&tex, filename);
                    if (texSize > 0) {
                        if (needsMask) {
                            MaskKitTexture(tex, fileId + 2);
                        }
                        OrDIBTexture((TEXIMGPACKHEADER*)decBuf, tex);
                        FreePNGTexture(tex);
                    }
                    break;
                case TEXTYPE_BMP:
                    LogWithString(&k_mydll,"JuceUniDecode: GK socks file = %s", filename);
                    texSize = LoadTexture(&tex, filename);
                    if (texSize > 0) {
                        if (needsMask) {
                            MaskKitTexture(tex, fileId + 2);
                        }
                        OrDIBTexture((TEXIMGPACKHEADER*)decBuf, tex);
                        FreeTexture(tex);
                    }
                    break;
            }
            */

            //DoMipMap(fileId + 2, (TEXIMGPACKHEADER*)decBuf, 0, "-mip1", MaskKitTextureMip1);
            //DoMipMap(fileId + 2, (TEXIMGPACKHEADER*)decBuf, 1, "-mip2", MaskKitTextureMip2);
        }
        
        TRACE(&k_mydll,"JuceUniDecode done");
	}
	
	else if (part == 0) {
		// UniDecode, but not a kit texture
		Log(&k_mydll,"JuceUniDecode: SEVERE WARNING!: this is NOT a kit texture.");
        clearTeamKitInfo();
        return;
	}
	
	else if (IsNumOrFontTexture(fileId)) {
		// the former Unpack -> numbers and fonts
		TRACE(&k_mydll,"JuceUnpack: CALLED.");

	    bool replaced = FALSE;
	
	    //BYTE* strips = (BYTE*)data[TEAM_STRIPS];
	    //LogWithTwoNumbers(&k_mydll,"strip = %02x %02x", strips[0], strips[1]);
		
        LogWithNumber(&k_mydll,"JuceUnpack: Loading id = %d", fileId);

		// find corresponding file
		BITMAPINFO* tex = NULL;
		DWORD texSize = 0;

		char filename[BUFLEN];
		ZeroMemory(filename, BUFLEN);
        DWORD texType = IsNumbersTexture(fileId) ?
            FindImageFileForId(fileId, "", filename) :
            FindImageFileForId(fileId, "", filename, NULL);

        switch (texType) {
            case TEXTYPE_PNG:
                LogWithString(&k_mydll,"JuceUnpack: PNG Image file = %s", filename);
                texSize = LoadPNGTexture(&tex, filename);
                if (texSize > 0) {
                    Apply4BitDIBTexture((TEXIMGPACKHEADER*)decBuf, tex, TRUE);
                    FreePNGTexture(tex);
                    replaced = TRUE;
                }
                break;
            case TEXTYPE_BMP:
                LogWithString(&k_mydll,"JuceUnpack: BMP Image file = %s", filename);
                texSize = LoadTexture(&tex, filename);
                if (texSize > 0) {
                    Apply4BitDIBTexture((TEXIMGPACKHEADER*)decBuf, tex, TRUE);
                    FreeTexture(tex);
                    replaced = TRUE;
                }
                break;
        }

        TEXIMGPACKHEADER* pack = (TEXIMGPACKHEADER*)decBuf;
        TEXIMGHEADER* top = (TEXIMGHEADER*)((BYTE*)pack + pack->toc[0]);
        if (replaced && pack->numFiles == 3 && top->width == 256 && top->height == 128) {
            // looks like an numbers image.
            // In this case we need to load 2 more images
            // to substitute palettes for shorts

            // for home shorts
            TEXIMGHEADER* palAtex = (TEXIMGHEADER*)((BYTE*)pack + pack->toc[1]);

            int id1 = fileId;
            int fileType = (fileId - data[FIRST_ID]) % FILES_PER_TEAM;
            switch (fileType) {
                //case GA_NUMBERS: id1 = fileId - GA_NUMBERS + GA_UNKNOWN1; break;
                //case GB_NUMBERS: id1 = fileId - GB_NUMBERS + GA_UNKNOWN1; break;
                //case PA_NUMBERS: id1 = fileId - PA_NUMBERS + PA_SHORTS; break;
                //case PB_NUMBERS: id1 = fileId - PB_NUMBERS + PA_SHORTS; break;
                case GA_NUMBERS: id1 = fileId - GA_NUMBERS + GA_KIT; break;
                case GB_NUMBERS: id1 = fileId - GB_NUMBERS + GA_KIT; break;
                case PA_NUMBERS: id1 = fileId - PA_NUMBERS + PA_SHIRT; break;
                case PB_NUMBERS: id1 = fileId - PB_NUMBERS + PA_SHIRT; break;
            }
            string kitFolderKey1 = GetKitFolderKey(id1);

            ZeroMemory(filename, BUFLEN);
            texType = FindShortsPalImageFileForId(fileId, kitFolderKey1, filename);
            switch (texType) {
                case TEXTYPE_PNG:
                    LogWithString(&k_mydll,"JuceUnpack: PNG Image file = %s", filename);
                    texSize = LoadPNGTexture(&tex, filename);
                    if (texSize > 0) {
                        Apply4BitDIBPalette(palAtex, tex);
                        FreePNGTexture(tex);
                    }
                    break;
                case TEXTYPE_BMP:
                    LogWithString(&k_mydll,"JuceUnpack: BMP Image file = %s", filename);
                    texSize = LoadTexture(&tex, filename);
                    if (texSize > 0) {
                        Apply4BitDIBPalette(palAtex, tex);
                        FreeTexture(tex);
                    }
                    break;
            }

            // for away shorts
            TEXIMGHEADER* palBtex = (TEXIMGHEADER*)((BYTE*)pack + pack->toc[2]);

            int id2 = fileId;
            switch (fileType) {
                //case GA_NUMBERS: id2 = fileId - GA_NUMBERS + GB_UNKNOWN1; break;
                //case GB_NUMBERS: id2 = fileId - GB_NUMBERS + GB_UNKNOWN1; break;
                //case PA_NUMBERS: id2 = fileId - PA_NUMBERS + PB_SHORTS; break;
                //case PB_NUMBERS: id2 = fileId - PB_NUMBERS + PB_SHORTS; break;
                case GA_NUMBERS: id2 = fileId - GA_NUMBERS + GB_KIT; break;
                case GB_NUMBERS: id2 = fileId - GB_NUMBERS + GB_KIT; break;
                case PA_NUMBERS: id2 = fileId - PA_NUMBERS + PB_SHIRT; break;
                case PB_NUMBERS: id2 = fileId - PB_NUMBERS + PB_SHIRT; break;
            }
            string kitFolderKey2 = GetKitFolderKey(id2);

            ZeroMemory(filename, BUFLEN);
            texType = FindShortsPalImageFileForId(fileId, kitFolderKey2, filename);
            switch (texType) {
                case TEXTYPE_PNG:
                    LogWithString(&k_mydll,"JuceUnpack: PNG Image file = %s", filename);
                    texSize = LoadPNGTexture(&tex, filename);
                    if (texSize > 0) {
                        Apply4BitDIBPalette(palBtex, tex);
                        FreePNGTexture(tex);
                    }
                    break;
                case TEXTYPE_BMP:
                    LogWithString(&k_mydll,"JuceUnpack: BMP Image file = %s", filename);
                    texSize = LoadTexture(&tex, filename);
                    if (texSize > 0) {
                        Apply4BitDIBPalette(palBtex, tex);
                        FreeTexture(tex);
                    }
                    break;
            }
        }

		//ENCBUFFERHEADER* header = (ENCBUFFERHEADER*)(addr1 - 0x20);
		//DumpData((BYTE*)decBuf, header->dwDecSize);
		//DumpImagePack((TEXIMGPACKHEADER*)decBuf);
	
	    TRACE(&k_mydll,"JuceUnpack: done.");
	}
	
	return;	
}

/* Writes an indexed BMP file (with palette) */
HRESULT SaveAsBitmap(char* filename, TEXIMGHEADER* header)
{
	BYTE* buf = (BYTE*)header + header->dataOffset;
	if (header->dataOffset == header->dwSize) {
		buf = NULL; // palette-only file
	}
	BYTE* pal = (BYTE*)header + header->paletteOffset;
	LONG width = header->width;
	LONG height = header->height;

	if (header->bps == 4) {
		return SaveAs4bitBMP(filename, buf, pal, width, height);
	} else if (header->bps == 8) {
		return SaveAs8bitBMP(filename, buf, pal, width, height);
	}

	Log(&k_mydll,"SaveAsBitmap: unsupported bits-per-pixel format.");
	return E_FAIL;
}

/* Writes an indexed 4-bit BMP file (with palette) */
HRESULT SaveAs4bitBMP(char* filename, BYTE* buf, BYTE* pal, LONG width, LONG height)
{
	BITMAPFILEHEADER fhdr;
	BITMAPINFOHEADER infoheader;
	SIZE_T size = (buf)? width * height : 8; // size of data in bytes

	// fill in the headers
	fhdr.bfType = 0x4D42; // "BM"
	fhdr.bfSize = sizeof(fhdr) + sizeof(infoheader) + 0x40 + size;
	fhdr.bfReserved1 = 0;
	fhdr.bfReserved2 = 0;
	fhdr.bfOffBits = sizeof(fhdr) + sizeof(infoheader) + 0x40;

	infoheader.biSize = sizeof(infoheader);
	infoheader.biWidth = (buf)? width : 16;
	infoheader.biHeight = (buf) ? height : 1;
	infoheader.biPlanes = 1;
	infoheader.biBitCount = 4;
	infoheader.biCompression = BI_RGB;
	infoheader.biSizeImage = 0;
	infoheader.biXPelsPerMeter = 0;
	infoheader.biYPelsPerMeter = 0;
	infoheader.biClrUsed = 16;
	infoheader.biClrImportant = 16;

	// prepare filename
	char name[BUFLEN];
	if (filename == NULL)
	{
		SYSTEMTIME time;
		GetLocalTime(&time);
		ZeroMemory(name, BUFLEN);
		sprintf(name, "%s%s-%d%02d%02d-%02d%02d%02d%03d.bmp", GetPESInfo()->mydir, "kserv", 
				time.wYear, time.wMonth, time.wDay,
				time.wHour, time.wMinute, time.wSecond, time.wMilliseconds); 
		filename = name;
	}

	// save to file
	DWORD wbytes;
	HANDLE hFile = CreateFile(filename,            // file to create 
					 GENERIC_WRITE,                // open for writing 
					 0,                            // do not share 
					 NULL,                         // default security 
					 OPEN_ALWAYS,                  // overwrite existing 
					 FILE_ATTRIBUTE_NORMAL,        // normal file 
					 NULL);                        // no attr. template 

	if (hFile != INVALID_HANDLE_VALUE) 
	{
		WriteFile(hFile, &fhdr, sizeof(fhdr), (LPDWORD)&wbytes, NULL);
		WriteFile(hFile, &infoheader, sizeof(infoheader), (LPDWORD)&wbytes, NULL);

		// write palette
		for (int i=0; i<16; i++)
		{
			WriteFile(hFile, pal + i*4 + 2, 1, (LPDWORD)&wbytes, NULL);
			WriteFile(hFile, pal + i*4 + 1, 1, (LPDWORD)&wbytes, NULL);
			WriteFile(hFile, pal + i*4, 1, (LPDWORD)&wbytes, NULL);
			WriteFile(hFile, pal + i*4 + 3, 1, (LPDWORD)&wbytes, NULL);
		}

		// write pixel data
		if (buf) {
			for (int k=height-1; k>=0; k--)
			{
				int j;
				// unshuffle pixel data
				for (j=0;j<width/2;j++) {
					BYTE src = buf[k*(width/2)+j];
					buf[k*(width/2)+j] = (src << 4 & 0xf0) | (src >> 4 & 0x0f);
				}
				WriteFile(hFile, buf + k*(width/2), width/2, (LPDWORD)&wbytes, NULL);
				// reshuffle pixel data
				for (j=0;j<width/2;j++) {
					BYTE src = buf[k*(width/2)+j];
					buf[k*(width/2)+j] = (src << 4 & 0xf0) | (src >> 4 & 0x0f);
				}
			}
		} else {
			// palette-only data: write sample stuff
			BYTE sample[] = {0x01,0x23,0x45,0x67,0x89,0xab,0xcd,0xef};
			WriteFile(hFile, sample, sizeof(sample), (LPDWORD)&wbytes, NULL);
		}
		CloseHandle(hFile);
	}
	else 
	{
		Log(&k_mydll,"SaveAs4bitBMP: failed to save to file.");
		return E_FAIL;
	}
	return S_OK;
}

/* Writes an indexed 8-bit BMP file (with palette) */
HRESULT SaveAs8bitBMP(char* filename, BYTE* buffer)
{
	DECBUFFERHEADER* header = (DECBUFFERHEADER*)buffer;
	BYTE* buf = (BYTE*)header + header->bitsOffset;
	BYTE* pal = (BYTE*)header + header->paletteOffset;
	LONG width = header->texWidth;
	LONG height = header->texHeight;

	return SaveAs8bitBMP(filename, buf, pal, width, height);
}

/* Writes an indexed 8-bit BMP file (with palette) */
HRESULT SaveAs8bitBMP(char* filename, BYTE* buf, BYTE* pal, LONG width, LONG height)
{
	BITMAPFILEHEADER fhdr;
	BITMAPINFOHEADER infoheader;
	SIZE_T size = width * height; // size of data in bytes

	// fill in the headers
	fhdr.bfType = 0x4D42; // "BM"
	fhdr.bfSize = sizeof(fhdr) + sizeof(infoheader) + 0x400 + size;
	fhdr.bfReserved1 = 0;
	fhdr.bfReserved2 = 0;
	fhdr.bfOffBits = sizeof(fhdr) + sizeof(infoheader) + 0x400;

	infoheader.biSize = sizeof(infoheader);
	infoheader.biWidth = width;
	infoheader.biHeight = height;
	infoheader.biPlanes = 1;
	infoheader.biBitCount = 8;
	infoheader.biCompression = BI_RGB;
	infoheader.biSizeImage = 0;
	infoheader.biXPelsPerMeter = 0;
	infoheader.biYPelsPerMeter = 0;
	infoheader.biClrUsed = 256;
	infoheader.biClrImportant = 0;

	// prepare filename
	char name[BUFLEN];
	if (filename == NULL)
	{
		SYSTEMTIME time;
		GetLocalTime(&time);
		ZeroMemory(name, BUFLEN);
		sprintf(name, "%s%s-%d%02d%02d-%02d%02d%02d%03d.bmp", GetPESInfo()->mydir, "kserv", 
				time.wYear, time.wMonth, time.wDay,
				time.wHour, time.wMinute, time.wSecond, time.wMilliseconds); 
		filename = name;
	}

	// save to file
	DWORD wbytes;
	HANDLE hFile = CreateFile(filename,            // file to create 
					 GENERIC_WRITE,                // open for writing 
					 0,                            // do not share 
					 NULL,                         // default security 
					 OPEN_ALWAYS,                  // overwrite existing 
					 FILE_ATTRIBUTE_NORMAL,        // normal file 
					 NULL);                        // no attr. template 

	if (hFile != INVALID_HANDLE_VALUE) 
	{
		WriteFile(hFile, &fhdr, sizeof(fhdr), (LPDWORD)&wbytes, NULL);
		WriteFile(hFile, &infoheader, sizeof(infoheader), (LPDWORD)&wbytes, NULL);

		// write palette
		for (int bank=0; bank<8; bank++)
		{
            int i;
			for (i=0; i<8; i++)
			{
				WriteFile(hFile, pal + bank*32*4 + i*4 + 2, 1, (LPDWORD)&wbytes, NULL);
				WriteFile(hFile, pal + bank*32*4 + i*4 + 1, 1, (LPDWORD)&wbytes, NULL);
				WriteFile(hFile, pal + bank*32*4 + i*4, 1, (LPDWORD)&wbytes, NULL);
				WriteFile(hFile, pal + bank*32*4 + i*4 + 3, 1, (LPDWORD)&wbytes, NULL);
			}
			for (i=16; i<24; i++)
			{
				WriteFile(hFile, pal + bank*32*4 + i*4 + 2, 1, (LPDWORD)&wbytes, NULL);
				WriteFile(hFile, pal + bank*32*4 + i*4 + 1, 1, (LPDWORD)&wbytes, NULL);
				WriteFile(hFile, pal + bank*32*4 + i*4, 1, (LPDWORD)&wbytes, NULL);
				WriteFile(hFile, pal + bank*32*4 + i*4 + 3, 1, (LPDWORD)&wbytes, NULL);
			}
			for (i=8; i<16; i++)
			{
				WriteFile(hFile, pal + bank*32*4 + i*4 + 2, 1, (LPDWORD)&wbytes, NULL);
				WriteFile(hFile, pal + bank*32*4 + i*4 + 1, 1, (LPDWORD)&wbytes, NULL);
				WriteFile(hFile, pal + bank*32*4 + i*4, 1, (LPDWORD)&wbytes, NULL);
				WriteFile(hFile, pal + bank*32*4 + i*4 + 3, 1, (LPDWORD)&wbytes, NULL);
			}
			for (i=24; i<32; i++)
			{
				WriteFile(hFile, pal + bank*32*4 + i*4 + 2, 1, (LPDWORD)&wbytes, NULL);
				WriteFile(hFile, pal + bank*32*4 + i*4 + 1, 1, (LPDWORD)&wbytes, NULL);
				WriteFile(hFile, pal + bank*32*4 + i*4, 1, (LPDWORD)&wbytes, NULL);
				WriteFile(hFile, pal + bank*32*4 + i*4 + 3, 1, (LPDWORD)&wbytes, NULL);
			}
		}
		// write pixel data
		for (int k=height-1; k>=0; k--)
		{
			WriteFile(hFile, buf + k*width, width, (LPDWORD)&wbytes, NULL);
		}
		CloseHandle(hFile);
	}
	else 
	{
		Log(&k_mydll,"SaveAs8bitBMP: failed to save to file.");
		return E_FAIL;
	}
	return S_OK;
}

/* Writes a BMP file */
HRESULT SaveAsBMP(char* filename, BYTE* rgbBuf, SIZE_T size, LONG width, LONG height, int bpp)
{
	BITMAPFILEHEADER fhdr;
	BITMAPINFOHEADER infoheader;

	// fill in the headers
	fhdr.bfType = 0x4D42; // "BM"
	fhdr.bfSize = sizeof(fhdr) + sizeof(infoheader) + size;
	fhdr.bfReserved1 = 0;
	fhdr.bfReserved2 = 0;
	fhdr.bfOffBits = sizeof(fhdr) + sizeof(infoheader);

	infoheader.biSize = sizeof(infoheader);
	infoheader.biWidth = width;
	infoheader.biHeight = height;
	infoheader.biPlanes = 1;
	infoheader.biBitCount = bpp*8;
	infoheader.biCompression = BI_RGB;
	infoheader.biSizeImage = 0;
	infoheader.biXPelsPerMeter = 0;
	infoheader.biYPelsPerMeter = 0;
	infoheader.biClrUsed = 0;
	infoheader.biClrImportant = 0;

	// prepare filename
	char name[BUFLEN];
	if (filename == NULL)
	{
		SYSTEMTIME time;
		GetLocalTime(&time);
		ZeroMemory(name, BUFLEN);
		sprintf(name, "%s%s-%d%02d%02d-%02d%02d%02d%03d.bmp", GetPESInfo()->mydir, "kserv", 
				time.wYear, time.wMonth, time.wDay,
				time.wHour, time.wMinute, time.wSecond, time.wMilliseconds); 
		filename = name;
	}

	// save to file
	DWORD wbytes;
	HANDLE hFile = CreateFile(filename,            // file to create 
					 GENERIC_WRITE,                // open for writing 
					 0,                            // do not share 
					 NULL,                         // default security 
					 OPEN_ALWAYS,                  // overwrite existing 
					 FILE_ATTRIBUTE_NORMAL,        // normal file 
					 NULL);                        // no attr. template 

	if (hFile != INVALID_HANDLE_VALUE) 
	{
		WriteFile(hFile, &fhdr, sizeof(fhdr), (LPDWORD)&wbytes, NULL);
		WriteFile(hFile, &infoheader, sizeof(infoheader), (LPDWORD)&wbytes, NULL);
		WriteFile(hFile, rgbBuf, size, (LPDWORD)&wbytes, NULL);
		CloseHandle(hFile);
	}
	else 
	{
		Log(&k_mydll,"SaveAsBMP: failed to save to file.");
		return E_FAIL;
	}
	return S_OK;
}

/*
// helper function
Kit* utilGetKit(WORD kitId)
{
	int id = kitId / 5; int idRem = kitId % 5;

	// sanity check
	if (id < 0 || id > 255)
		return NULL;

	// select corresponding kit
	Kit* kit = NULL;
	if (g_kitExtras[kitId] != NULL) 
		kit = g_kitExtras[kitId]->kit;
	if (kit == NULL)
	{
		switch (idRem)
		{
			case 0: kit = kDB->goalkeepers[id].a; break;
			case 1: kit = kDB->goalkeepers[id].b; break;
			case 2: kit = kDB->players[id].a; break;
			case 3: kit = kDB->players[id].b; break;
		}
	}

	return kit;
}
*/

// helper function
TeamAttr* utilGetTeamAttr(WORD id)
{
	// sanity check
	if (id < 0 || id > 255)
		return NULL;

	DWORD teamAttrBase = *((DWORD*)data[TEAM_COLLARS_PTR]);
	return (TeamAttr*)(teamAttrBase + id*sizeof(TeamAttr));
}

// this function makes sure we have the most up-to-date information
// about what teams are currently chosen.
void VerifyTeams()
{
	g_currTeams[0] = g_currTeams[1] = 0xffff;
	WORD* teams = (WORD*)data[TEAM_IDS];

	// try to define both teams.
	if (teams[0] >=0 && teams[0] <= g_numTeams) g_currTeams[0] = teams[0];
	if (teams[1] >=0 && teams[1] <= g_numTeams) g_currTeams[1] = teams[1];

	// check if home team is still undefined.
	if (g_currTeams[0] == 0xffff)
	{
		int which = (teams[0] == 0x0122) ? 1 : 0;

		// looks like it's an ML team.
		DWORD* mlPtrs = (DWORD*)data[MLDATA_PTRS];
		if (mlPtrs[which] != NULL)
		{
			WORD* mlData = (WORD*)(mlPtrs[which] + 0x278);
			WORD teamId = mlData[0];
			BOOL originalTeam = mlData[1];
			if (!originalTeam) g_currTeams[0] = teamId;
		}
	}

	// check if away team is still undefined.
	if (g_currTeams[1] == 0xffff)
	{
		int which = (teams[1] == 0x0122) ? 1 : 0;

		// looks like it's an ML team.
		DWORD* mlPtrs = (DWORD*)data[MLDATA_PTRS];
		if (mlPtrs[which] != NULL)
		{
			WORD* mlData = (WORD*)(mlPtrs[which] + 0x278);
			WORD teamId = mlData[0];
			BOOL originalTeam = mlData[1];
			if (!originalTeam) g_currTeams[1] = teamId;
		}
	}
}

char* getKitFoldername(string& kitFolderKey, WORD teamId, int kitType, int kitPart)
{
	if (kitType < GK_TYPE || kitType > PL_TYPE) return nullString;
	if (kitPart < KITPART_SHIRT || kitPart > KITPART_FONT) return nullString;
	if (kitFolderKey == nullString) return nullString;
	
	gdbFindKitsForTeam(gdb, teamId);
	KitCollection* col = MAP_FIND(gdb->uni, teamId);
	if (!col) return nullString;

	Kit* kit = (kitType==GK_TYPE) ?
		MAP_FIND(col->goalkeepers, kitFolderKey) :
		MAP_FIND(col->players, kitFolderKey);
	if (!kit) return nullString;

	switch (kitPart) {
	case KITPART_SHIRT:
	//if this was enabled, numbers and font were in the directory of the shirt
	//case KITPART_NUMBERS:
	//case KITPART_FONT:
		return (kit->attDefined & SHIRT_FOLDER)?kit->shirtFolder : (char*)kitFolderKey.c_str();
	case KITPART_SHORTS:
		return (kit->attDefined & SHORTS_FOLDER)?kit->shortsFolder : (char*)kitFolderKey.c_str();
	case KITPART_SOCKS:
		return (kit->attDefined & SOCKS_FOLDER)?kit->socksFolder : (char*)kitFolderKey.c_str();
	case KITPART_NUMBERS:
	case KITPART_FONT:
		// numbers and font are always in the same directory
		return (char*)kitFolderKey.c_str();
	}
			
	return nullString;
}
