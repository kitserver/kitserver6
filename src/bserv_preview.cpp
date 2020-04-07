#include <windows.h>
#include <mmsystem.h>
#include <stdio.h>
#include "d3dfont.h"
#include "bserv.h"
#include "crc32.h"
#include "kload_exp.h"
#include "soft\zlib123-dll\include\zlib.h"
#include "numpages.h"
#include "input.h"
#include "keycfg.h"

#include <pngdib.h>
#include <map>
#include <string>

KMOD k_bserv={MODID,NAMELONG,NAMESHORT,DEFAULT_DEBUG};

BSERV_CFG bserv_cfg;
int selectedBall=-1;
DWORD numBalls=0;
BALLS *balls;

bool isSelectMode=false;
char display[BUFLEN];
char model[BUFLEN];
char texture[BUFLEN];
static std::map<WORD,int> g_HomeBallMap;
static std::map<string,int> g_BallIdMap;
static bool g_homeTeamChoice = false;
static bool g_userChoice = true;
bool autoRandomMode = false;
bool noHomeBall = true;

DWORD gdbBallAddr=0;
DWORD gdbBallSize=0;
DWORD gdbBallCRC=0;
BYTE* ballTexture=NULL;
int ballTextureSize=0;
RECT ballTextureRect;
bool isPNGtexture=false;
char currTextureName[BUFLEN];
IDirect3DTexture8* g_lastBallTex;
IDirect3DTexture8* g_gdbBallTexture;

#define MAP_FIND(map,key) map[key]
#define MAP_CONTAINS(map,key) (map.find(key)!=map.end())
	
	
//preview
#define D3DFVF_BALLVERTEX (D3DFVF_XYZ | D3DFVF_NORMAL | D3DFVF_TEX1)
DWORD BVTX=0x100;
struct BALLVERTEX { 
    float x,y,z;
    DWORD unknown[3];
    float tu, tv;
};

//preview lighting
#define D3DFVF_LIGHTVERTEX (D3DFVF_XYZ | D3DFVF_NORMAL | D3DFVF_TEX1)
struct LIGHTVERTEX { 
    float x,y,z;
    float nx,ny,nz;
    float tu, tv;
};

LIGHTVERTEX g_lighting[] = {
	{-5.95f, -5.95f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f}, //1
	{-5.95f,  5.95f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f}, //2
	{ 5.95f, -5.95f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f}, //3
	{ 5.95f,  5.95f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f}, //4
};

#define FACTOR 5.0f

static IDirect3DVertexBuffer8* g_pVB_preview = NULL;
static IDirect3DVertexBuffer8* g_pVB_preview_lighting = NULL;
static IDirect3DTexture8* g_preview_tex = NULL;
static IDirect3DTexture8* g_lighting_tex = NULL;
static IDirect3DIndexBuffer8* g_pIB_preview = NULL;

BYTE* g_previewData=0;
BYTE* g_previewIBData=NULL;
DWORD g_previewStride=0;
DWORD g_previewNumVertices=0;
DWORD g_previewSize=0;
DWORD g_previewNumPrimitives=0;

static bool g_needsRestore = TRUE;
static bool g_newBall = false;
static DWORD g_dwSavedStateBlock = 0L;
static DWORD g_dwDrawOverlayStateBlock = 0L;

void DumpBuffer(char* filename, LPVOID buf, DWORD len);
void SafeRelease(LPVOID ppObj);
BOOL FileExists(char* filename);
void DrawBallPreview(IDirect3DDevice8* dev);
void bservReset(IDirect3DDevice8* self, LPVOID params);
HRESULT InitVB(IDirect3DDevice8* dev);
void DeleteStateBlocks(IDirect3DDevice8* dev);
HRESULT InvalidateDeviceObjects(IDirect3DDevice8* dev);
HRESULT DeleteDeviceObjects(IDirect3DDevice8* dev);
HRESULT RestoreDeviceObjects(IDirect3DDevice8* dev);
void ReadBallModel();
void ResizeLightingRect(LIGHTVERTEX* dta, int n);
BOOL ReadConfig(BSERV_CFG* config, char* cfgFile);
void CheckInput();

EXTERN_C BOOL WINAPI DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID lpReserved);
void InitBserv();
void ReadBalls();
void AddBall(LPTSTR sdisplay,LPTSTR smodel,LPTSTR stexture);
void FreeBalls();
void SetBall(DWORD id);
void bservKeyboardProc(int code1, WPARAM wParam, LPARAM lParam);
void bservBeginUniSelect();
void BeginDrawBallLabel();
void EndDrawBallLabel();
void DrawBallLabel(IDirect3DDevice8* self);
void bservAfsReplace(GETFILEINFO* gfi);
void bservUnpack(GETFILEINFO* gfi, DWORD part, DWORD decBuf, DWORD size);

DWORD LoadPNGTexture(BITMAPINFO** tex, char* filename);
static int read_file_to_mem(char *fn,unsigned char **ppfiledta, int *pfilesize);
void ApplyAlphaChunk(RGBQUAD* palette, BYTE* memblk, DWORD size);
void FreePNGTexture(BITMAPINFO* bitmap);

HRESULT CreateBallTexture(IDirect3DDevice8* dev, UINT width, UINT height, UINT levels, DWORD usage,
        D3DFORMAT format, IDirect3DTexture8** ppTexture);
void FreeBallTexture();
HRESULT STDMETHODCALLTYPE bservCreateTexture(IDirect3DDevice8* self, UINT width, UINT height,UINT levels,
	DWORD usage, D3DFORMAT format, D3DPOOL pool, IDirect3DTexture8** ppTexture, DWORD src, bool* IsProcessed);
void bservUnlockRect(IDirect3DTexture8* self,UINT Level);

DWORD SetBallName(char** names, DWORD numNames, DWORD p3, DWORD p4, DWORD p5, DWORD p6, DWORD p7);
void updateHomeBall();

void DumpBuffer(char* filename, LPVOID buf, DWORD len)
{
    FILE* f = fopen(filename,"wb");
    if (f) {
        fwrite(buf, len, 1, f);
        fclose(f);
    }
}


// Calls IUnknown::Release() on an instance
void SafeRelease(LPVOID ppObj)
{
    try {
        IUnknown** ppIUnknown = (IUnknown**)ppObj;
        if (ppIUnknown == NULL)
        {
            Log(&k_bserv,"Address of IUnknown reference is 0");
            return;
        }
        if (*ppIUnknown != NULL)
        {
            (*ppIUnknown)->Release();
            *ppIUnknown = NULL;
        }
    } catch (...) {
        // problem with a safe-release
        TRACE(&k_bserv,"Problem with safe-release");
    }
}

BOOL FileExists(char* filename)
{
    TRACE4(&k_bserv,"FileExists: Checking file: %s", filename);
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

EXTERN_C BOOL WINAPI DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID lpReserved)
{
	char ballCfg[BUFLEN];
	
	if (dwReason == DLL_PROCESS_ATTACH)
	{	
		Log(&k_bserv,"Attaching dll...");
		
		switch (GetPESInfo()->GameVersion) {
            case gvPES6PC: //support for PES6 PC
            case gvPES6PC110: //support for PES6 PC 1.10
            case gvWE2007PC: //support for WE:PES2007 PC
				goto GameVersIsOK;
				break;
		};
		//Will land here if game version is not supported
		Log(&k_bserv,"Your game version is currently not supported!");
		return false;
		
		//Everything is OK!
		GameVersIsOK:
		
		RegisterKModule(&k_bserv);
		
		memcpy(code, codeArray[GetPESInfo()->GameVersion], sizeof(code));
		memcpy(dta, dtaArray[GetPESInfo()->GameVersion], sizeof(dta));
		
		strcpy(currTextureName,"\0");
		
		ReadBalls();
		
		//load settings
	    ZeroMemory(ballCfg, BUFLEN);
	    sprintf(ballCfg, "%s\\bserv.dat", GetPESInfo()->mydir);
	    FILE* f = fopen(ballCfg, "rb");
	    if (f) {
	        fread(&bserv_cfg, sizeof(BSERV_CFG), 1, f);
	        fread(&g_homeTeamChoice, sizeof(bool), 1, f);
	        fread(&autoRandomMode, sizeof(bool), 1, f);
	        fclose(f);
	    } else {
	    	bserv_cfg.selectedBall=-1;
	    	g_userChoice = false;
	    };

        //read preview setting
        bserv_cfg.previewEnabled = TRUE;
	    ZeroMemory(ballCfg, BUFLEN);
	    sprintf(ballCfg, "%s\\bserv.cfg", GetPESInfo()->mydir);
        ReadConfig(&bserv_cfg, ballCfg);

		SetBall(bserv_cfg.selectedBall);
		g_userChoice = (selectedBall >= 0);

		HookFunction(hk_D3D_Create,(DWORD)InitBserv);
		HookFunction(hk_D3D_CreateTexture,(DWORD)bservCreateTexture);
		HookFunction(hk_D3D_UnlockRect,(DWORD)bservUnlockRect);
		
	    HookFunction(hk_Input,(DWORD)bservKeyboardProc);
	    		
		HookFunction(hk_BeginUniSelect, (DWORD)bservBeginUniSelect);
	    HookFunction(hk_DrawKitSelectInfo,(DWORD)DrawBallLabel);
	    HookFunction(hk_OnShowMenu,(DWORD)BeginDrawBallLabel);
	    HookFunction(hk_OnHideMenu,(DWORD)EndDrawBallLabel);
	    HookFunction(hk_D3D_Reset,(DWORD)bservReset);
	}
	else if (dwReason == DLL_PROCESS_DETACH)
	{
		Log(&k_bserv,"Detaching dll...");
		
		//save settings
		bserv_cfg.selectedBall=selectedBall;
	    ZeroMemory(ballCfg, BUFLEN);
	    sprintf(ballCfg, "%s\\bserv.dat", GetPESInfo()->mydir);
	    FILE* f = fopen(ballCfg, "wb");
	    if (f) {
	        fwrite(&bserv_cfg, sizeof(BSERV_CFG), 1, f);
	        fwrite(&g_homeTeamChoice, sizeof(bool), 1, f);
	        fwrite(&autoRandomMode, sizeof(bool), 1, f);
	        fclose(f);
	    }
		
		UnhookFunction(hk_D3D_CreateTexture,(DWORD)bservCreateTexture);
		UnhookFunction(hk_D3D_UnlockRect,(DWORD)bservUnlockRect);
		
		UnhookFunction(hk_Input,(DWORD)bservKeyboardProc);
		UnhookFunction(hk_BeginUniSelect, (DWORD)bservBeginUniSelect);
		UnhookFunction(hk_DrawKitSelectInfo,(DWORD)DrawBallLabel);
		UnhookFunction(hk_D3D_Reset,(DWORD)bservReset);
		
		MasterUnhookFunction(code[C_SETBALLNAME_CS],SetBallName);
		
		if (g_previewData!=NULL)
			HeapFree(GetProcessHeap(), 0, g_previewData);
			
		if (g_previewIBData!=NULL)
			HeapFree(GetProcessHeap(), 0, g_previewIBData);
			
		SafeRelease( &g_preview_tex );
		SafeRelease( &g_lighting_tex );
		
		FreeBallTexture();
		
		FreeBalls();
		
		g_BallIdMap.clear();
		g_HomeBallMap.clear();
		
		Log(&k_bserv,"Detaching done.");
	};

	return true;
};

void InitBserv()
{
    Log(&k_bserv, "InitBserv called.");
    
    RegisterAfsReplaceCallback(bservAfsReplace, bservUnpack, NULL);
    
	MasterHookFunction(code[C_SETBALLNAME_CS], 7, SetBallName);
	
	UnhookFunction(hk_D3D_Create,(DWORD)InitBserv);

	return;
};

bool IsBallModel(int id)
{
    return 
        (id < dta[NOT_A_BALL_FILE] && id%2==0) ||
        (id > dta[NOT_A_BALL_FILE] && id%2==1);
}

bool IsBallTexture(int id)
{
    return 
        (id < dta[NOT_A_BALL_FILE] && id%2==1) ||
        (id > dta[NOT_A_BALL_FILE] && id%2==0);
}

void bservAfsReplace(GETFILEINFO* gfi)
{
	if (gfi->isProcessed) return;
	
	DWORD afsId = 0, fileId = 0;
	char filename[BUFLEN];
	ZeroMemory(filename, BUFLEN);
	fileId = splitFileId(gfi->fileId, &afsId);
	
	if (afsId == 1 && fileId < dta[NUM_BALL_FILES] && fileId != dta[NOT_A_BALL_FILE]) {
        LogWithTwoNumbers(&k_bserv,"BALL FILE!: 0x%x, 0x%x", afsId, fileId);
        
        updateHomeBall();
        if (selectedBall >= 0) {
        	if (IsBallModel(fileId)) {
	        	// replace the model
				strcpy(filename, GetPESInfo()->gdbDir);
				strcat(filename, "GDB\\balls\\mdl\\");
				strcat(filename, model);
		
				loadReplaceFile(filename);
		        gfi->isProcessed = true;
			} else if (IsBallTexture(fileId)) {
				// texture will be replaced in CreateTexture, but some things from
				// the Unpack function are needed
				gfi->needsUnpack = true;
			}
    	}
    }
	return;
}

void bservUnpack(GETFILEINFO* gfi, DWORD part, DWORD decBuf, DWORD size)
{
	//texture
	gdbBallAddr = decBuf;
	gdbBallSize = size;
	gdbBallCRC = GetCRC((BYTE*)gdbBallAddr, gdbBallSize);
	
	return;
}

void ReadBalls()
{
	char tmp[BUFLEN];
	char str[BUFLEN];
	char *comment=NULL;
	char sdisplay[BUFLEN], smodel[BUFLEN], stexture[BUFLEN];
		
	strcpy(tmp,GetPESInfo()->gdbDir);
	strcat(tmp,"GDB\\balls\\map.txt");
	
	FILE* cfg=fopen(tmp, "rt");
	if (cfg==NULL) return;
	while (true) {
		ZeroMemory(str, BUFLEN);
		fgets(str, BUFLEN-1, cfg);
		if (feof(cfg)) break;
		
		// skip comments
		comment=NULL;
		comment = strstr(str, "#");
		if (comment != NULL) comment[0] = '\0';
		
		// parse line
		ZeroMemory(sdisplay,BUFLEN);
		ZeroMemory(smodel,BUFLEN);
		ZeroMemory(stexture,BUFLEN);
		if (sscanf(str,"\"%[^\"]\",\"%[^\"]\",\"%[^\"]\"",sdisplay,smodel,stexture)==3)
			AddBall(sdisplay,smodel,stexture);
	};
	fclose(cfg);
	
	//home map
	
    char mapFile[4096];
    ZeroMemory(mapFile,4096);

    sprintf(mapFile, "%sGDB\\balls\\home_map.txt", GetPESInfo()->gdbDir);
    FILE* map = fopen(mapFile, "rt");
    if (map == NULL) {
        LogWithString(&k_bserv, "Unable to find home-ball-map: %s", mapFile);
        return;
    }

	// go line by line
    char buf[4096];
	while (!feof(map))
	{
		ZeroMemory(buf, 4096);
		fgets(buf, 4096, map);
		if (lstrlen(buf) == 0) break;

		// strip off comments
		char* comm = strstr(buf, "#");
		if (comm != NULL) comm[0] = '\0';

        // find team id
        WORD teamId = 0xffff;
        if (sscanf(buf, "%d", &teamId)==1) {
            LogWithNumber(&k_bserv, "teamId = %d", teamId);
            char* ballname = NULL;
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
                    ballname = start + 1;
                } else {
                    // just take the rest of the line
                    ballname = pComma + 1;
                }

                LogWithString(&k_bserv, "ballname = {%s}", ballname);

                // store in the home-ball map
                if (MAP_CONTAINS(g_BallIdMap, ballname)) {
                	g_HomeBallMap[teamId] = g_BallIdMap[ballname];
                }
            }
        }
    }
    fclose(map);
	
	return;
};

void AddBall(LPTSTR sdisplay,LPTSTR smodel,LPTSTR stexture)
{
	BALLS *tmp=new BALLS[numBalls+1];
	memcpy(tmp,balls,numBalls*sizeof(BALLS));
	delete balls;
	balls=tmp;
	
	balls[numBalls].display=new char [strlen(sdisplay)+1];
	strcpy(balls[numBalls].display,sdisplay);
	
	balls[numBalls].model=new char [strlen(smodel)+1];
	strcpy(balls[numBalls].model,smodel);
	
	balls[numBalls].texture=new char [strlen(stexture)+1];
	strcpy(balls[numBalls].texture,stexture);
	
	g_BallIdMap[sdisplay]=numBalls;

	numBalls++;
	return;
};

void FreeBalls()
{
	for (int i=0;i<numBalls;i++) {
		delete balls[i].display;
		delete balls[i].model;
		delete balls[i].texture;
	};
	delete balls;
	numBalls=0;
	selectedBall=-1;
	return;
};

void SetBall(DWORD id)
{
	char tmp[BUFLEN];
	
	if (id<numBalls)
		selectedBall=id;
	else if (id<0)
		selectedBall=-1;
	else
		selectedBall=-1;
		
	if (selectedBall<0) {
		strcpy(tmp,"game choice");
		strcpy(model,"\0");
		strcpy(texture,"\0");
	} else {
		strcpy(tmp,balls[selectedBall].display);
		strcpy(model,balls[selectedBall].model);
		strcpy(texture,balls[selectedBall].texture);
		
		SafeRelease( &g_preview_tex );
		g_newBall=true;
	};
	
	strcpy(display,"Ball: ");
	strcat(display,tmp);
	
	return;
};

void bservKeyboardProc(int code1, WPARAM wParam, LPARAM lParam)
{
	if ((!isSelectMode) || (code1 < 0))
		return; 

	if ((code1==HC_ACTION) && (lParam & 0x80000000)) {
        KEYCFG* keyCfg = GetInputCfg();
		if (wParam == keyCfg->keyboard.keyNext) {
			SetBall(selectedBall+1);
			g_homeTeamChoice = false;
			g_userChoice = true;
		} else if (wParam == keyCfg->keyboard.keyPrev) {
			if (selectedBall<0)
				SetBall(numBalls-1);
			else
				SetBall(selectedBall-1);
				
			g_homeTeamChoice = false;
			g_userChoice = true;
			
		} else if (wParam == keyCfg->keyboard.keyReset) {
			if (!autoRandomMode && g_homeTeamChoice) {
        		autoRandomMode = true;
        		g_homeTeamChoice = false;
        		g_userChoice = true;
	            
        	} else {
	        	if (g_userChoice) g_homeTeamChoice = true;
				SetBall(-1);
				g_homeTeamChoice = !g_homeTeamChoice;
				updateHomeBall();
				g_userChoice = false;
				autoRandomMode = false;
			}
			
			if (autoRandomMode || noHomeBall) {
				LARGE_INTEGER num;
				QueryPerformanceCounter(&num);
				int iterations = num.LowPart % MAX_ITERATIONS;
				for (int j=0;j<iterations;j++) {
					SetBall(selectedBall+1);
		        }
			}

		} else if (wParam == keyCfg->keyboard.keyRandom) {
			LARGE_INTEGER num;
			QueryPerformanceCounter(&num);
			int iterations = num.LowPart % MAX_ITERATIONS;
			for (int j=0;j<iterations;j++)
				SetBall(selectedBall+1);
			
			g_homeTeamChoice = false;
			g_userChoice = true;
		};
	};
	
	return;
};

void bservBeginUniSelect()
{
	updateHomeBall();
	
	if (autoRandomMode || noHomeBall) {
		LARGE_INTEGER num;
		QueryPerformanceCounter(&num);
		int iterations = num.LowPart % MAX_ITERATIONS;
		for (int j=0;j<iterations;j++) {
			SetBall(selectedBall+1);
        }
	}
	return;
}

void BeginDrawBallLabel()
{
	isSelectMode=true;
	dksiSetMenuTitle("Ball selection");
	
	SafeRelease( &g_preview_tex );
    g_newBall = true;
	return;
};

void EndDrawBallLabel()
{
	isSelectMode=false;
	return;
};

void DrawBallLabel(IDirect3DDevice8* self)
{
	char display1[256];
	SIZE size;
	DWORD color = 0xffffffff; // white
	
	if (selectedBall<0)
		color = 0xffc0c0c0; // gray if ball is game choice
		
	if (autoRandomMode)
		color = 0xffff3300; // light red for randomly selected ball
		
	if (g_homeTeamChoice) {
		if (noHomeBall)
			color = 0xffff6600; // orange for home-choice, but team has no ball
		else
			color = 0xffffffc0; // pale yellow if ball is home-team choice
	}
	
	UINT g_bbWidth=GetPESInfo()->bbWidth;
	UINT g_bbHeight=GetPESInfo()->bbHeight;
	double stretchX=GetPESInfo()->stretchX;
	double stretchY=GetPESInfo()->stretchY;
	
	KGetTextExtent(display,12,&size);
	//draw shadow
	if (selectedBall>=0)
		KDrawText((g_bbWidth-size.cx)/2+3*stretchX,g_bbHeight*0.77+3*stretchY,0xff000000,12,display,true);
	//print ball label
	KDrawText((g_bbWidth-size.cx)/2,g_bbHeight*0.77,color,12,display,true);

    //draw ball preview
    if (bserv_cfg.previewEnabled) {
        DrawBallPreview(self);
    }

    // check input
    CheckInput();

	return;
};

// Load texture from PNG file. Returns the size of loaded texture
DWORD LoadPNGTexture(BITMAPINFO** tex, char* filename)
{
	TRACE4(&k_bserv,"LoadPNGTexture: loading %s", filename);
    DWORD size = 0;

    PNGDIB *pngdib;
    LPBITMAPINFOHEADER* ppDIB = (LPBITMAPINFOHEADER*)tex;

    pngdib = pngdib_p2d_init();
	//TRACE(&k_bserv,"LoadPNGTexture: structure initialized");

    BYTE* memblk;
    int memblksize;
    if (read_file_to_mem(filename,&memblk, &memblksize)) {
        TRACE(&k_bserv,"LoadPNGTexture: unable to read PNG file");
        return 0;
    }
    //TRACE(&k_bserv,"LoadPNGTexture: file read into memory");

    pngdib_p2d_set_png_memblk(pngdib,memblk,memblksize);
	pngdib_p2d_set_use_file_bg(pngdib,1);
	pngdib_p2d_run(pngdib);

	//TRACE(&k_bserv,"LoadPNGTexture: run done");
    pngdib_p2d_get_dib(pngdib, ppDIB, (int*)&size);
	//TRACE(&k_bserv,"LoadPNGTexture: get_dib done");

    pngdib_done(pngdib);
	TRACE(&k_bserv,"LoadPNGTexture: done done");

	TRACE2(&k_bserv,"LoadPNGTexture: *ppDIB = %08x", (DWORD)*ppDIB);
    if (*ppDIB == NULL) {
		TRACE(&k_bserv,"LoadPNGTexture: ERROR - unable to load PNG image.");
        return 0;
    }

    // read transparency values from tRNS chunk
    // and put them into DIB's RGBQUAD.rgbReserved fields
    ApplyAlphaChunk((RGBQUAD*)&((BITMAPINFO*)*ppDIB)->bmiColors, memblk, memblksize);

    HeapFree(GetProcessHeap(), 0, memblk);

	TRACE(&k_bserv,"LoadPNGTexture: done");
	return size;
};

// Read a file into a memory block.
static int read_file_to_mem(char *fn,unsigned char **ppfiledta, int *pfilesize)
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
					(*ppfiledta)  = fbuf;
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
};

/**
 * Extracts alpha values from tRNS chunk and applies stores
 * them in the RGBQUADs of the DIB
 */
void ApplyAlphaChunk(RGBQUAD* palette, BYTE* memblk, DWORD size)
{
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
        }
        // move on to next chunk
        offset += sizeof(chunk->dwSize) + sizeof(chunk->dwName) + 
            SWAPBYTES(chunk->dwSize) + sizeof(DWORD); // last one is CRC
    }
};

void FreePNGTexture(BITMAPINFO* bitmap)
{
	if (bitmap != NULL) {
        pngdib_p2d_free_dib(NULL, (BITMAPINFOHEADER*)bitmap);
	}
};

void CheckInput()
{
    DWORD* inputs = GetInputTable();
    KEYCFG* keyCfg = GetInputCfg();
    for (int n=0; n<8; n++) {
        if (INPUT_EVENT(inputs, n, FUNCTIONAL, keyCfg->gamepad.keyNext)) {
			SetBall(selectedBall+1);
			g_homeTeamChoice = false;
			g_userChoice = true;

        } else if (INPUT_EVENT(inputs, n, FUNCTIONAL, keyCfg->gamepad.keyPrev)) {
			if (selectedBall<0) {
				SetBall(numBalls-1);
            } else {
				SetBall(selectedBall-1);
            }
            g_homeTeamChoice = false;
            g_userChoice = true;

        } else if (INPUT_EVENT(inputs, n, FUNCTIONAL, keyCfg->gamepad.keyReset)) {
        	if (!autoRandomMode && g_homeTeamChoice) {
        		autoRandomMode = true;
        		g_homeTeamChoice = false;
        		g_userChoice = true;
	            
        	} else {
	        	if (g_userChoice) g_homeTeamChoice = true;
				SetBall(-1);
				g_homeTeamChoice = !g_homeTeamChoice;
				updateHomeBall();
				g_userChoice = false;
				autoRandomMode = false;
			}
			
			if (autoRandomMode || noHomeBall) {
				LARGE_INTEGER num;
				QueryPerformanceCounter(&num);
				int iterations = num.LowPart % MAX_ITERATIONS;
				for (int j=0;j<iterations;j++) {
					SetBall(selectedBall+1);
		        }
			}

        } else if (INPUT_EVENT(inputs, n, FUNCTIONAL, keyCfg->gamepad.keyRandom)) {
			LARGE_INTEGER num;
			QueryPerformanceCounter(&num);
			int iterations = num.LowPart % MAX_ITERATIONS;
			for (int j=0;j<iterations;j++) {
				SetBall(selectedBall+1);
            }
            g_homeTeamChoice = false;
            g_userChoice = true;
		}
    }
}

void DrawBallPreview(IDirect3DDevice8* dev)
{
	if (strlen(model)==0 || strlen(texture)==0)
		return;
	
	if (g_needsRestore || g_newBall) 
	{
		if (g_newBall) {
			InvalidateDeviceObjects(dev);
			DeleteDeviceObjects(dev);
			g_needsRestore = TRUE;
		};
		if (FAILED(RestoreDeviceObjects(dev)))
		{
			Log(&k_bserv,"DrawBallPreview: RestoreDeviceObjects() failed.");
            return;
		}
		Log(&k_bserv,"DrawBallPreview: RestoreDeviceObjects() done.");
        g_needsRestore = FALSE;
	}

	// render
	dev->BeginScene();

	// setup renderstate
	dev->CaptureStateBlock( g_dwSavedStateBlock );
	dev->ApplyStateBlock( g_dwDrawOverlayStateBlock );
    
    if (!g_preview_tex && g_newBall) {
        char buf[2048];
        sprintf(buf, "%s\\GDB\\balls\\%s", GetPESInfo()->gdbDir, texture);
        if (FAILED(D3DXCreateTextureFromFileEx(dev, buf, 
                    0, 0, 4, 0, D3DFMT_UNKNOWN, D3DPOOL_MANAGED,
                    D3DX_FILTER_LINEAR, D3DX_FILTER_LINEAR,
                    0, NULL, NULL, &g_preview_tex))) {
            Log(&k_bserv,"FAILED to load image for ball preview.");
		}
        g_newBall = false;
    }
    
    if (g_preview_tex && g_pVB_preview && g_pIB_preview) {

        // For our world matrix, we will just rotate the object about the y-axis.
        D3DXMATRIXA16 matWorldRot;

        // Set up the rotation matrix to generate 1 full rotation (2*PI radians) 
        // every 5000 ms. To avoid the loss of precision inherent in very high 
        // floating point numbers, the system time is modulated by the rotation 
        // period before conversion to a radian angle.
        UINT  iTime  = timeGetTime() % 5000;
        FLOAT fAngle = iTime * (2.0f * D3DX_PI) / 5000.0f;
        D3DXMatrixRotationY( &matWorldRot, fAngle );

        // set up translation matrix to move the ball down
        D3DXMATRIX matWorldTrans;
        D3DXMatrixTranslation(&matWorldTrans, 0.0, -282.0, 0.0);
        dev->SetTransform( D3DTS_WORLD, &(matWorldRot * matWorldTrans));

        // Set up our view matrix. A view matrix can be defined given an eye point,
        // a point to lookat, and a direction for which way is up. Here, we set the
        // eye 100 units back along the z-axis, look at the
        // origin, and define "up" to be in the y-direction.
        D3DXVECTOR3 vEyePt( 0.0f, 0.0f, -FACTOR*4 );
        D3DXVECTOR3 vLookatPt( 0.0f, 0.0f, 0.0f );
        D3DXVECTOR3 vUpVec( 0.0f, 1.0f, 0.0f );
        D3DXMATRIXA16 matView;
        D3DXMatrixLookAtLH( &matView, &vEyePt, &vLookatPt, &vUpVec );
        dev->SetTransform( D3DTS_VIEW, &matView );

        // For the projection matrix, we set up a orthogonal transform.
        float ar_correction = GetPESInfo()->bbWidth / (float)GetPESInfo()->bbHeight * 0.75;
        D3DXMATRIXA16 matProj;
        
        D3DXMatrixOrthoLH( &matProj, 1024*ar_correction, 768, -FACTOR*4, FACTOR*4 );
        dev->SetTransform( D3DTS_PROJECTION, &matProj );

        // texture
        dev->Clear(0, NULL, D3DCLEAR_ZBUFFER, 0, 1.0, 0);
        dev->SetIndices(g_pIB_preview,0);
        dev->SetVertexShader(D3DFVF_BALLVERTEX);
		dev->SetStreamSource( 0, g_pVB_preview, g_previewStride);
		dev->SetTexture(0, g_preview_tex);
		//dev->SetRenderState(D3DRS_FILLMODE, D3DFILL_WIREFRAME);
		dev->SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);
        dev->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
        dev->SetRenderState(D3DRS_ZENABLE, D3DZB_TRUE);
        dev->SetRenderState(D3DRS_ZWRITEENABLE, TRUE);
        dev->SetRenderState(D3DRS_ZFUNC, D3DCMP_LESSEQUAL);
		
        dev->DrawIndexedPrimitive( D3DPT_TRIANGLESTRIP, 0, g_previewNumVertices,
        							0, g_previewNumPrimitives-2);
        							
        // draw lighting billboard
        if (g_lighting_tex && g_pVB_preview_lighting) {
            D3DXMATRIXA16 matWorldRot;
            D3DXMatrixRotationY( &matWorldRot, 0.0f );
            D3DXMATRIX matWorldTrans;
            D3DXMatrixTranslation(&matWorldTrans, 0.0, -282.0, 0.0);
            dev->SetTransform( D3DTS_WORLD, &(matWorldRot * matWorldTrans));

            dev->SetIndices(NULL,0);
            dev->SetVertexShader(D3DFVF_LIGHTVERTEX);
            dev->SetStreamSource( 0, g_pVB_preview_lighting, sizeof(LIGHTVERTEX));
            dev->SetTexture(0, g_lighting_tex);
            dev->SetTextureStageState( 0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1 );
            dev->SetTextureStageState( 0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE );
            dev->SetRenderState( D3DRS_ALPHABLENDENABLE,   TRUE );
            dev->SetRenderState(D3DRS_ZFUNC, D3DCMP_ALWAYS);
            dev->DrawPrimitive( D3DPT_TRIANGLESTRIP, 0, 2);
        }
    }

	// restore the modified renderstates
	dev->ApplyStateBlock( g_dwSavedStateBlock );

	dev->EndScene();
	
	return;
};

void bservReset(IDirect3DDevice8* self, LPVOID params)
{
	Log(&k_bserv,"bservReset: cleaning-up.");
	
	InvalidateDeviceObjects(self);
	DeleteDeviceObjects(self);
	
	g_needsRestore = TRUE;
	
	return;
}

/* creates vertex buffers */
HRESULT InitVB(IDirect3DDevice8* dev)
{
	VOID* pVertices;

	ReadBallModel();

	// create vertex buffers
	if (!g_pVB_preview && g_previewData) {
        if (FAILED(dev->CreateVertexBuffer(g_previewSize, D3DUSAGE_WRITEONLY, D3DFVF_BALLVERTEX,
                                            D3DPOOL_MANAGED, &g_pVB_preview)))
        {
            Log(&k_bserv,"CreateVertexBuffer() failed.");
            return E_FAIL;
        }
        Log(&k_bserv,"CreateVertexBuffer() done.");
        if (FAILED(g_pVB_preview->Lock(0, g_previewSize, (BYTE**)&pVertices, 0)))
		{
			Log(&k_bserv,"g_pVB_preview->Lock() failed.");
			return E_FAIL;
		}
		memcpy(pVertices, g_previewData, g_previewSize);
		g_pVB_preview->Unlock();
	};

	if (!g_pVB_preview_lighting) {
        if (FAILED(dev->CreateVertexBuffer(sizeof(g_lighting), D3DUSAGE_WRITEONLY, D3DFVF_LIGHTVERTEX,
                                            D3DPOOL_MANAGED, &g_pVB_preview_lighting)))
        {
            Log(&k_bserv,"CreateVertexBuffer() failed.");
            return E_FAIL;
        }
        Log(&k_bserv,"CreateVertexBuffer() done.");
        if (FAILED(g_pVB_preview_lighting->Lock(0, sizeof(g_lighting), (BYTE**)&pVertices, 0)))
        {
            Log(&k_bserv,"g_pVB_preview_lighting->Lock() failed.");
            return E_FAIL;
        }
        ResizeLightingRect(g_lighting, sizeof(g_lighting)/sizeof(LIGHTVERTEX));
        memcpy(pVertices, g_lighting, sizeof(g_lighting));
        g_pVB_preview_lighting->Unlock();
	}

    // create index buffers
	if (!g_pIB_preview && g_previewIBData) {
        if (FAILED(dev->CreateIndexBuffer(g_previewNumPrimitives*2, D3DUSAGE_WRITEONLY, D3DFMT_INDEX16,
                                            D3DPOOL_MANAGED, &g_pIB_preview)))
        {
            Log(&k_bserv,"CreateIndexBuffer() failed.");
            return E_FAIL;
        }
        Log(&k_bserv,"CreateIndexBuffer() done.");
        if (FAILED(g_pIB_preview->Lock(0, g_previewNumPrimitives*2, (BYTE**)&pVertices, 0)))
		{
			Log(&k_bserv,"g_pIB_preview->Lock() failed.");
			return E_FAIL;
		}
		memcpy(pVertices, g_previewIBData, g_previewNumPrimitives*2);
		g_pIB_preview->Unlock();
	};

    return S_OK;
}



void DeleteStateBlocks(IDirect3DDevice8* dev)
{
	// Delete the state blocks
	try
	{
        DWORD* vtab = (DWORD*)(*(DWORD*)dev);
        if (vtab && vtab[VTAB_DELETESTATEBLOCK]) {
            if (g_dwSavedStateBlock) {
                dev->DeleteStateBlock( g_dwSavedStateBlock );
                Log(&k_bserv,"g_dwSavedStateBlock deleted.");
            }
            if (g_dwDrawOverlayStateBlock) {
                dev->DeleteStateBlock( g_dwDrawOverlayStateBlock );
                Log(&k_bserv,"g_dwDrawOverlayStateBlock deleted.");
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
	TRACE(&k_bserv,"InvalidateDeviceObjects called.");
	if (dev == NULL)
	{
		TRACE(&k_bserv,"InvalidateDeviceObjects: nothing to invalidate.");
		return S_OK;
	}

    // ball preview
	SafeRelease( &g_pVB_preview );
	SafeRelease( &g_pIB_preview );
	SafeRelease( &g_pVB_preview_lighting );

	Log(&k_bserv,"InvalidateDeviceObjects: SafeRelease(s) done.");

    DeleteStateBlocks(dev);
    Log(&k_bserv,"InvalidateDeviceObjects: DeleteStateBlock(s) done.");
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
		Log(&k_bserv,"InitVB() failed.");
        return hr;
    }
	Log(&k_bserv,"InitVB() done.");

    // create reflections/shading texture
    if (!g_lighting_tex) {
        char buf[2048];
        sprintf(buf, "%s\\blight.png", GetPESInfo()->mydir);
        if (FAILED(D3DXCreateTextureFromFileEx(dev, buf, 
                    0, 0, 4, 0, D3DFMT_UNKNOWN, D3DPOOL_MANAGED,
                    D3DX_FILTER_NONE, D3DX_FILTER_LINEAR,
                    0xffffffff, NULL, NULL, &g_lighting_tex))) {
            Log(&k_bserv,"FAILED to load image for ball lighting.");
		}
    }
    
	// Create the state blocks for rendering overlay graphics
	for( UINT which=0; which<2; which++ )
	{
		dev->BeginStateBlock();
		/*
		dev->SetTextureStageState(0,D3DTSS_MAGFILTER,D3DTEXF_LINEAR);
		dev->SetTextureStageState(0,D3DTSS_MINFILTER,D3DTEXF_POINT);
		dev->SetTextureStageState(0,D3DTSS_ADDRESSU,D3DTADDRESS_WRAP);
		dev->SetTextureStageState(0,D3DTSS_ADDRESSV,D3DTADDRESS_WRAP);
        dev->SetVertexShader( D3DFVF_BALLVERTEX );
        dev->SetRenderState(D3DRS_SPECULARENABLE,1);
		dev->SetTexture(0, g_preview_tex);
		dev->SetTextureStageState(1,D3DTSS_COLOROP,D3DTOP_DISABLE);
		dev->SetTextureStageState(1,D3DTSS_ALPHAOP,D3DTOP_DISABLE);
        */

        D3DXMATRIXA16 matWorld;
        dev->SetTransform( D3DTS_WORLD, &matWorld );
        D3DXMATRIXA16 matView;
        dev->SetTransform( D3DTS_VIEW, &matView );
        D3DXMATRIXA16 matProj;
        dev->SetTransform( D3DTS_PROJECTION, &matProj );

		//dev->SetTexture(1, NULL);
        dev->SetIndices(g_pIB_preview,0);
        dev->SetVertexShader(D3DFVF_BALLVERTEX);
		dev->SetStreamSource( 0, g_pVB_preview, g_previewStride);
		dev->SetTexture(0, g_preview_tex);
		//dev->SetRenderState(D3DRS_FILLMODE, D3DFILL_WIREFRAME);
		dev->SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);
        dev->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
        dev->SetRenderState(D3DRS_ZENABLE, D3DZB_TRUE);
        dev->SetRenderState(D3DRS_ZWRITEENABLE, TRUE);
        dev->SetRenderState(D3DRS_ZFUNC, D3DCMP_LESSEQUAL);
        dev->SetRenderState( D3DRS_ALPHABLENDENABLE,   TRUE );
        dev->SetTextureStageState( 0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1 );
        dev->SetTextureStageState( 0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE );
		
		if( which==0 )
			dev->EndStateBlock( &g_dwSavedStateBlock );
		else
			dev->EndStateBlock( &g_dwDrawOverlayStateBlock );
	}
    return S_OK;
}

void ReadBallModel()
{
	char buf[2048];
	BYTE* mdlFileCompr;
	int mdlFileSizeCompr=0;
	BYTE* mdlFile;
	DWORD mdlFileSize=0;
	
	// clear dta buffers
	if (g_previewData!=NULL) {
		HeapFree(GetProcessHeap(), 0, g_previewData);
        g_previewData = NULL;
    }
	if (g_previewIBData!=NULL) {
		HeapFree(GetProcessHeap(), 0, g_previewIBData);
        g_previewIBData = NULL;
    }
	
    // try to read the model file
    sprintf(buf, "%sGDB\\balls\\mdl\\%s", GetPESInfo()->gdbDir, model);
	if (read_file_to_mem(buf,&mdlFileCompr,&mdlFileSizeCompr) != 0) {
        LogWithString(&k_bserv, "Unable to read ball model: %s", model);
        return;
    }

	mdlFileSizeCompr=*(DWORD*)(mdlFileCompr+4);
	mdlFileSize=*(DWORD*)(mdlFileCompr+8);
	mdlFile=(BYTE*)HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,mdlFileSize);
	
	int retval = uncompress((UCHAR*)mdlFile,&mdlFileSize,(UCHAR*)(mdlFileCompr+32),mdlFileSizeCompr);
    if (retval != Z_OK) {
        LogWithString(&k_bserv, "Unable to uncompress ball model: %s", model);
        HeapFree(GetProcessHeap(), 0, mdlFileCompr);
        HeapFree(GetProcessHeap(), 0, mdlFile);
        return;
    }
	HeapFree(GetProcessHeap(), 0, mdlFileCompr);

	DWORD start1=*(DWORD*)(mdlFile+16);
	BYTE* indexStart=mdlFile+*(DWORD*)(mdlFile+20);

	DWORD vertHeaderStart=*(DWORD*)(mdlFile+start1+4);
	
	g_previewNumVertices=*(WORD*)(mdlFile+start1+vertHeaderStart);
	g_previewStride=*(WORD*)(mdlFile+start1+vertHeaderStart+2);
	g_previewSize=g_previewNumVertices*g_previewStride;
	
	g_previewData=(BYTE*)HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,g_previewSize);
	memcpy(g_previewData,mdlFile+start1+vertHeaderStart+8,g_previewSize);
	
	g_previewNumPrimitives=*(WORD*)(indexStart);
	
	g_previewIBData=(BYTE*)HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,g_previewNumPrimitives*2);
	memcpy(g_previewIBData,indexStart+2,g_previewNumPrimitives*2);
	
	HeapFree(GetProcessHeap(), 0, mdlFile);
	
	
	BALLVERTEX* bv=(BALLVERTEX*)g_previewData;
	for (int i=0;i<g_previewNumVertices;i++) {
		bv[i].x=bv[i].x*FACTOR;
		bv[i].y=bv[i].y*FACTOR;
		bv[i].z=bv[i].z*-FACTOR;
	};
	
	return;
};

void ResizeLightingRect(LIGHTVERTEX* dta, int n)
{
    static bool resized = false;
	if (!resized) { 
        for (int i=0;i<n;i++) {
            dta[i].x=dta[i].x*FACTOR;
            dta[i].y=dta[i].y*FACTOR;
        }
        resized = true;
    }
}

/**
 * Returns true if successful.
 */
BOOL ReadConfig(BSERV_CFG* config, char* cfgFile)
{
	if (config == NULL) return false;

	FILE* cfg = fopen(cfgFile, "rt");
	if (cfg == NULL) return false;

	char str[BUFLEN];
	char name[BUFLEN];
	int value = 0;

	char *pName = NULL, *pValue = NULL, *comment = NULL;
	while (!feof(cfg))
	{
		ZeroMemory(str, BUFLEN);
		fgets(str, BUFLEN-1, cfg);

		// skip comments
		comment = strstr(str, "#");
		if (comment != NULL) comment[0] = '\0';

		// parse the line
		pName = pValue = NULL;
		ZeroMemory(name, BUFLEN); value = 0;
		char* eq = strstr(str, "=");
		if (eq == NULL || eq[1] == '\0') continue;

		eq[0] = '\0';
		pName = str; pValue = eq + 1;

		ZeroMemory(name, NULL); 
		sscanf(pName, "%s", name);

		if (lstrcmp(name, "preview")==0)
		{
			if (sscanf(pValue, "%d", &value)!=1) continue;
			LogWithNumber(&k_bserv,"ReadConfig: preview = (%d)", value);
            config->previewEnabled = (value == 1);
		}
	}
	fclose(cfg);
	return true;
}

HRESULT CreateBallTexture(IDirect3DDevice8* dev, UINT width, UINT height, UINT levels, DWORD usage,
        D3DFORMAT format, IDirect3DTexture8** ppTexture) 
{
	char tmp[BUFLEN];
	ZeroMemory(tmp,BUFLEN);
	
	updateHomeBall();
	if (selectedBall<0) return false;
		
	if (ballTexture!=NULL && lstrcmpi(currTextureName,texture)==0)
		return true;
		
	FreeBallTexture();
	sprintf(tmp,"%sGDB\\balls\\%s",GetPESInfo()->gdbDir,texture);
	
    D3DXIMAGE_INFO imageInfo;
    if (SUCCEEDED(D3DXGetImageInfoFromFile(tmp, &imageInfo))) {
        // it's IMPORTANT not to downsize the texture, because it will
        // lead to crashes, as the game still thinks the texture at least 
        // width*height size.
        ballTextureRect.right = max(imageInfo.Width, width);
        ballTextureRect.bottom = max(imageInfo.Height, height);

        if (FAILED(D3DXCreateTextureFromFileEx(
                    dev, tmp, ballTextureRect.right, ballTextureRect.bottom, 
                    levels, usage, format, D3DPOOL_MANAGED, 
                    D3DX_DEFAULT, D3DX_DEFAULT, 0, NULL, NULL, ppTexture
                ))) {
            LogWithString(&k_bserv, "D3DXCreateTextureFromFileEx FAILED for %s", tmp);
            return false;
        }

        strcpy(currTextureName,texture);
    } else {
        LogWithString(&k_bserv, "D3DXGetImageInfoFromFile FAILED for %s", tmp);
        return false;
    }

	return true;
};

void FreeBallTexture()
{
    SafeRelease(&g_gdbBallTexture);
	strcpy(currTextureName,"\0");
	return;
};

DWORD VtableSet(void* self, int index, DWORD value)
{
    DWORD* vtab = (DWORD*)(*(DWORD*)self);
    DWORD currValue = vtab[index];
    vtab[index] = value;
    return currValue;
}

HRESULT STDMETHODCALLTYPE bservCreateTexture(IDirect3DDevice8* self, UINT width, UINT height,UINT levels,
DWORD usage, D3DFORMAT format, D3DPOOL pool, IDirect3DTexture8** ppTexture, DWORD src, bool* IsProcessed)
{
	HRESULT res=D3D_OK;
	RECT texSize;
//LogWithNumber(&k_bserv, "bservCreateTexture: gdbBallAddr = %08x", gdbBallAddr);
	
	if (*IsProcessed==true)
		return res;
//Log(&k_bserv, "bservCreateTexture: IsProcessed = false");
//LogWithNumber(&k_bserv, "bservCreateTexture: src = %08x", src);
	
	g_lastBallTex=NULL;
	
	if (IsBadReadPtr((BYTE*)src,gdbBallSize) || gdbBallCRC!=GetCRC((BYTE*)src,gdbBallSize)) {
		//wrong CRC -> dta changed
		gdbBallAddr=0;
		gdbBallSize=0;
		gdbBallCRC=0;
	};
//LogWithTwoNumbers(&k_bserv, "bservCreateTexture: %08x vs. %08x", src, (DWORD)gdbBallAddr);
	
	if (src!=0 && src==gdbBallAddr) {
		Log(&k_bserv,"bservCreateTexture called for ball texture.");
		
        DWORD prevValue = VtableSet(self, VTAB_CREATETEXTURE, (DWORD)OrgCreateTexture);
        if (FAILED(CreateBallTexture(self,width,height,levels,usage,format,&g_gdbBallTexture))) {
            Log(&k_bserv,"bservCreateTexture: CreateBallTexture FAILED.");
            *IsProcessed = false;
            return res;
        }
        VtableSet(self, VTAB_CREATETEXTURE, prevValue);

		res = OrgCreateTexture(self, ballTextureRect.right, ballTextureRect.bottom,
				levels,usage,format,pool,ppTexture);

        g_lastBallTex = *ppTexture;
        *IsProcessed = true;
		TRACE2(&k_bserv,"tex = %08x", (DWORD)g_lastBallTex);
	};

	return res;
}

void bservUnlockRect(IDirect3DTexture8* self,UINT Level)
{
	if (g_gdbBallTexture==NULL || g_lastBallTex==NULL)
		return;
		
    IDirect3DSurface8* src = NULL;
    IDirect3DSurface8* dest = NULL;

	//LogWithTwoNumbers(&k_bserv,"bservUnlockRect: Processing texture %x, level %d",(DWORD)self,Level);
    if (SUCCEEDED(g_lastBallTex->GetSurfaceLevel(0, &dest))) {
        if (SUCCEEDED(g_gdbBallTexture->GetSurfaceLevel(0, &src))) {
            if (SUCCEEDED(D3DXLoadSurfaceFromSurface(
                            dest, NULL, NULL,
                            src, NULL, NULL,
                            D3DX_FILTER_NONE, 0))) {
                Log(&k_bserv,"Replacing ball texture COMPLETE");

            } else {
                Log(&k_bserv,"Replacing ball texture FAILED");
            }
            src->Release();
        }
        dest->Release();
    }

	g_lastBallTex=NULL;
	return;
}

DWORD SetBallName(char** names, DWORD numNames, DWORD p3, DWORD p4, DWORD p5, DWORD p6, DWORD p7)
{
	updateHomeBall();
	if (selectedBall>=0 && numNames==3) {
		//strcpy(names[1],balls[selectedBall].display);
		names[1]=balls[selectedBall].display;
	};
	
	return MasterCallNext(names,numNames,p3,p4,p5,p6,p7);
};

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

void updateHomeBall()
{
	if (g_homeTeamChoice) {
        WORD teamId = GetTeamId(HOME);
        std::map<WORD,int>::iterator hit = g_HomeBallMap.find(teamId);
        if (hit !=  g_HomeBallMap.end()) {
            SetBall(hit->second);
            noHomeBall = false;

        } else {
        	noHomeBall = true;
            return;
        }
    } else {
    	noHomeBall = false;
    }
    return;
}
