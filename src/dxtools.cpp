// dxtools.cpp
#include <windows.h>
#include <stdio.h>
#include "dxtools.h"
#include "kload_exp.h"

KMOD k_dxtools={MODID,NAMELONG,NAMESHORT,DEFAULT_DEBUG};

HINSTANCE hInst;
DXCONFIG dxconfig = {
    { DEFAULT_WINDOW_WIDTH, DEFAULT_WINDOW_HEIGHT},
    { DEFAULT_FULLSCREEN_WIDTH, DEFAULT_FULLSCREEN_HEIGHT},
    { DEFAULT_INTERNAL_WIDTH, DEFAULT_INTERNAL_HEIGHT }
};

EXTERN_C BOOL WINAPI DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID lpReserved);
EXTERN_C HRESULT _declspec(dllexport) STDMETHODCALLTYPE dxtoolsReset(
    IDirect3DDevice8* self, D3DPRESENT_PARAMETERS* params);
HRESULT STDMETHODCALLTYPE dxtoolsPresent(IDirect3DDevice8* self, CONST RECT* src, CONST RECT* dest,
	HWND hWnd, LPVOID unused);
void dxtoolsCreateDevice(IDirect3D8* self, UINT Adapter,
    D3DDEVTYPE DeviceType, HWND hFocusWindow, DWORD BehaviorFlags,
    D3DPRESENT_PARAMETERS *pPresentationParameters, 
    IDirect3DDevice8** ppReturnedDeviceInterface);
BOOL ReadConfig(DXCONFIG* config, char* cfgFile);

typedef HRESULT (STDMETHODCALLTYPE *PFNRESETFUNC)(IDirect3DDevice8* self, D3DPRESENT_PARAMETERS*);
PFNRESETFUNC g_reset = NULL;
PFNPRESENTPROC g_present = NULL;

#define DATALEN 2

// data array names
enum {
    INTRES_WIDTH, INTRES_HEIGHT,
};

// Data addresses.
DWORD dataArray[][DATALEN] = {
    // PES6
    {
        0x11631f8, 0x11631fc,
    },
    // PES6 1.10
    {
        0x11641f8, 0x11641fc,
    },
    // WE2007
    {
        0x115dc78, 0x115dc7c,
    },
};

DWORD data[DATALEN];

EXTERN_C BOOL WINAPI DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID lpReserved)
{
	if (dwReason == DLL_PROCESS_ATTACH)
	{
		Log(&k_dxtools,"Attaching dll...");
		hInst=hInstance;
		RegisterKModule(&k_dxtools);
		char tmp[512];
		sprintf(tmp,"%skload.cfg",GetPESInfo()->mydir);
        ReadConfig(&dxconfig, tmp);
		HookFunction(hk_D3D_CreateDevice,(DWORD)dxtoolsCreateDevice);
		
	}
	else if (dwReason == DLL_PROCESS_DETACH)
	{
		Log(&k_dxtools,"Detaching dll...");
		UnhookFunction(hk_D3D_CreateDevice,(DWORD)dxtoolsCreateDevice);
	}

	return true;
}

void SetInternalResolution(DXCONFIG *cfg)
{
    DWORD *w = (DWORD*)data[INTRES_WIDTH];
    DWORD *h = (DWORD*)data[INTRES_HEIGHT];
    if (w != NULL && h != NULL) {
        LogWithTwoNumbers(&k_dxtools,"Internal resolution was: %d x %d", *w, *h);
        DWORD protection;
        DWORD newProtection = PAGE_READWRITE;
        if (VirtualProtect(w, 4, newProtection, &protection))
        {
            *w = cfg->internal.width;
            if (VirtualProtect(h, 4, newProtection, &protection)) {
                *h = cfg->internal.height;
                LogWithTwoNumbers(&k_dxtools,"Internal resolution now: %d x %d", *w, *h);
            }
        }
        else {
            Log(&k_dxtools,"Problem changing internal resolution.");
        }
    }
}

void dxtoolsCreateDevice(IDirect3D8* self, UINT Adapter,
    D3DDEVTYPE DeviceType, HWND hFocusWindow, DWORD BehaviorFlags,
    D3DPRESENT_PARAMETERS *pPresentationParameters, 
    IDirect3DDevice8** ppReturnedDeviceInterface) 
{
	DWORD* vtable;
	DWORD protection;
	DWORD newProtection;
	
    vtable = (DWORD*)(*((DWORD*)*ppReturnedDeviceInterface));
	protection = 0;
	newProtection = PAGE_EXECUTE_READWRITE;

    // hook Reset method
    if (!g_reset) {
        g_reset = (PFNRESETFUNC)vtable[VTAB_RESET];
		if (VirtualProtect(vtable+VTAB_RESET, 4, newProtection, &protection))
		{
            vtable[VTAB_RESET] = (DWORD)dxtoolsReset;
			Log(&k_dxtools,"Reset hooked.");
		}
    }

    /*
    // hook Present method
    if (!g_present) {
        g_present = (PFNPRESENTPROC)vtable[VTAB_PRESENT];
		if (VirtualProtect(vtable+VTAB_PRESENT, 4, newProtection, &protection))
		{
            vtable[VTAB_PRESENT] = (DWORD)dxtoolsPresent;
			Log(&k_dxtools,"Present hooked.");
		}
    }
    */

    // Determine the game version
    int v = GetPESInfo()->GameVersion;
    if (v != -1)
    {
        memcpy(data, dataArray[v], sizeof(data));
    }
}

/* New Reset function */
EXTERN_C HRESULT _declspec(dllexport) STDMETHODCALLTYPE dxtoolsReset(
IDirect3DDevice8* self, D3DPRESENT_PARAMETERS* params)
{
    if (dxconfig.fullscreen.width>0 && dxconfig.fullscreen.height>0) {
        if (!params->Windowed) {
            // fullscreen
            dxconfig.window.width = params->BackBufferWidth;
            dxconfig.window.height = params->BackBufferHeight;
            params->BackBufferWidth = dxconfig.fullscreen.width;
            params->BackBufferHeight = dxconfig.fullscreen.height;
            LogWithTwoNumbers(&k_dxtools, "dxtoolsReset: forcing fullscreen resolution: %dx%d",
                    dxconfig.fullscreen.width, dxconfig.fullscreen.height);

        } else if (dxconfig.window.width>0 && dxconfig.window.height>0) {
            // restore original resolution
            params->BackBufferWidth = dxconfig.window.width;
            params->BackBufferHeight = dxconfig.window.height;
            LogWithTwoNumbers(&k_dxtools, "dxtoolsReset: restoring original resolution: %dx%d",
                    dxconfig.window.width, dxconfig.window.height);
            dxconfig.window.width = 0;
            dxconfig.window.height = 0;
        }
    }

    // force anti-aliasing
    //params->MultiSampleType = D3DMULTISAMPLE_4_SAMPLES;
    //params->SwapEffect = D3DSWAPEFFECT_DISCARD;

	// CALL ORIGINAL FUNCTION
    LogWithNumber(&k_dxtools, "dxtoolsReset: calling original = %08x", (DWORD)g_reset);
	HRESULT res = g_reset(self, params);

    // enforce internal resolution
    if (dxconfig.internal.width>0 && dxconfig.internal.height>0) {
        SetInternalResolution(&dxconfig);
    }

	return res;
}

/* New Present function */
HRESULT STDMETHODCALLTYPE dxtoolsPresent(IDirect3DDevice8* self, CONST RECT* src, CONST RECT* dest,
	HWND hWnd, LPVOID unused)
{
    // force anisotropic filtering
    for (int i=0; i<8; i++) {
        self->SetTextureStageState(i, D3DTSS_MAGFILTER, D3DTEXF_ANISOTROPIC);
        self->SetTextureStageState(i, D3DTSS_MINFILTER, D3DTEXF_ANISOTROPIC);
        self->SetTextureStageState(i, D3DTSS_MIPFILTER, D3DTEXF_ANISOTROPIC);
    }

	// CALL ORIGINAL FUNCTION
	HRESULT res = g_present(self, src, dest, hWnd, unused);
	return res;
}

/**
 * Returns true if successful.
 */
BOOL ReadConfig(DXCONFIG* config, char* cfgFile)
{
	if (config == NULL) return false;

	FILE* cfg = fopen(cfgFile, "rt");
	if (cfg == NULL) return false;

	char str[BUFLEN];
	char name[BUFLEN];
	int value = 0;

	char *pName = NULL, *pValue = NULL, *comment = NULL;
	while (true)
	{
		ZeroMemory(str, BUFLEN);
		fgets(str, BUFLEN-1, cfg);
		if (feof(cfg)) break;

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

		if (lstrcmp(name, "dx.fullscreen.width")==0)
		{
			if (sscanf(pValue, "%d", &value)!=1) continue;
			LogWithNumber(&k_dxtools,"ReadConfig: dx.fullscreen.width = (%d)", value);
            config->fullscreen.width = value;
		}
        else if (lstrcmp(name, "dx.fullscreen.height")==0)
		{
			if (sscanf(pValue, "%d", &value)!=1) continue;
			LogWithNumber(&k_dxtools,"ReadConfig: dx.fullscreen.height = (%d)", value);
            config->fullscreen.height = value;
		}
        else if (lstrcmp(name, "dx.window.width")==0)
		{
			if (sscanf(pValue, "%d", &value)!=1) continue;
			LogWithNumber(&k_dxtools,"ReadConfig: dx.window.width = (%d)", value);
            config->window.width = value;
		}
        else if (lstrcmp(name, "dx.window.height")==0)
		{
			if (sscanf(pValue, "%d", &value)!=1) continue;
			LogWithNumber(&k_dxtools,"ReadConfig: dx.window.height = (%d)", value);
            config->window.height = value;
		}
        else if (lstrcmp(name, "internal.resolution.width")==0)
        {
            if (sscanf(pValue, "%d", &value)!=1) continue;
            LogWithNumber(&k_dxtools,"ReadConfig: internal.resolution.width = (%d)", value);
            config->internal.width = value;
        }
        else if (lstrcmp(name, "internal.resolution.height")==0)
        {
            if (sscanf(pValue, "%d", &value)!=1) continue;
            LogWithNumber(&k_dxtools,"ReadConfig: internal.resolution.height = (%d)", value);
            config->internal.height = value;
        }
	}
	fclose(cfg);
	return true;
}

