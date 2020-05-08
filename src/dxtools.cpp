// dxtools.cpp
#include <windows.h>
#include <stdio.h>
#include "dxtools.h"
#include "kload_exp.h"

KMOD k_dxtools={MODID,NAMELONG,NAMESHORT,DEFAULT_DEBUG};

HWND hFocusWin;
HINSTANCE hInst;
DXCONFIG dxconfig = {
    { DEFAULT_WINDOW_WIDTH, DEFAULT_WINDOW_HEIGHT},
    DEFAULT_WINDOW_X, DEFAULT_WINDOW_Y,
    0xFFFFFFFF, 0xFFFFFFFF,
    { DEFAULT_FULLSCREEN_WIDTH, DEFAULT_FULLSCREEN_HEIGHT},
    { DEFAULT_INTERNAL_WIDTH, DEFAULT_INTERNAL_HEIGHT }
};

EXTERN_C BOOL WINAPI DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID lpReserved);
void dxtoolsReset(IDirect3DDevice8* self, D3DPRESENT_PARAMETERS* params);
void dxtoolsPresent(IDirect3DDevice8* self, CONST RECT* src, CONST RECT* dest, HWND hWnd, LPVOID unused);
void dxtoolsCreateDevice(IDirect3D8* self, UINT Adapter,
    D3DDEVTYPE DeviceType, HWND hFocusWindow, DWORD BehaviorFlags,
    D3DPRESENT_PARAMETERS *pPresentationParameters, 
    IDirect3DDevice8** ppReturnedDeviceInterface);
BOOL ReadConfig(DXCONFIG* config, char* cfgFile);

#define DATALEN 2

// dta array names
enum {
    INTRES_WIDTH, INTRES_HEIGHT,
};

// Data addresses.
DWORD dtaArray[][DATALEN] = {
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

DWORD dta[DATALEN];

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
		HookFunction(hk_D3D_BeforeCreateDevice,(DWORD)dxtoolsCreateDevice);
		HookFunction(hk_D3D_Reset,(DWORD)dxtoolsReset);
		
	}
	else if (dwReason == DLL_PROCESS_DETACH)
	{
		Log(&k_dxtools,"Detaching dll...");
		UnhookFunction(hk_D3D_BeforeCreateDevice,(DWORD)dxtoolsCreateDevice);
		UnhookFunction(hk_D3D_Reset,(DWORD)dxtoolsReset);
	}

	return true;
}

void SetInternalResolution(DXCONFIG *cfg)
{
    DWORD *w = (DWORD*)dta[INTRES_WIDTH];
    DWORD *h = (DWORD*)dta[INTRES_HEIGHT];
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

void dxtoolsSetDimensions(D3DPRESENT_PARAMETERS* params)
{
    LOG(&k_dxtools, "dxtoolsSetDimensions: params->Windowed = %d", params->Windowed);
    LOG(&k_dxtools, "dxtoolsSetDimensions: backbuffer now: %dx%d", params->BackBufferWidth, params->BackBufferHeight);

    if (params->Windowed) {
        if (dxconfig.window.width>0 && dxconfig.window.height>0) {
            // window
            params->BackBufferWidth = dxconfig.window.width;
            params->BackBufferHeight = dxconfig.window.height;
            LOG(&k_dxtools, "dxtoolsSetDimensions: setting backbuffer for window: %dx%d",
                    dxconfig.window.width, dxconfig.window.height);
        }
    }
    else {
        if (dxconfig.window.width==0 && dxconfig.window.height==0) {
            // save window info for restoring backbuffer
            dxconfig.window.width = params->BackBufferWidth;
            dxconfig.window.height = params->BackBufferHeight;
        }
        if (dxconfig.fullscreen.width>0 && dxconfig.fullscreen.height>0) {
            // fullscreen
            params->BackBufferWidth = dxconfig.fullscreen.width;
            params->BackBufferHeight = dxconfig.fullscreen.height;
            LOG(&k_dxtools, "dxtoolsSetDimensions: setting fullscreen resolution: %dx%d",
                    dxconfig.fullscreen.width, dxconfig.fullscreen.height);

        }
    }

    if (params->Windowed) {
        if (dxconfig.window.width>0 && dxconfig.window.height>0) {
            // to resize window
            HookFunction(hk_D3D_Present,(DWORD)dxtoolsPresent);
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

    hFocusWin = hFocusWindow;

    HookFunction(hk_D3D_Present,(DWORD)dxtoolsPresent);

    // Determine the game version
    int v = GetPESInfo()->GameVersion;
    if (v != -1)
    {
        memcpy(dta, dtaArray[v], sizeof(dta));
    }

    dxtoolsSetDimensions(pPresentationParameters);
}

void dxtoolsReset(IDirect3DDevice8* self, D3DPRESENT_PARAMETERS* params)
{
    LOG(&k_dxtools, "dxtoolsReset:: CALLED");
    dxtoolsSetDimensions(params);

    // enforce internal resolution
    if (dxconfig.internal.width>0 && dxconfig.internal.height>0) {
        SetInternalResolution(&dxconfig);
    }
}

void dxtoolsPresent(IDirect3DDevice8* self, CONST RECT* src, CONST RECT* dest, HWND hWnd, LPVOID unused)
{
    LOG(&k_dxtools, "dxtoolsPresent:: CALLED");
    // resize window
    if (dxconfig.window.width>0 && dxconfig.window.height>0) {
        LOG(&k_dxtools, "dxtoolsPresent: resizing window");
        UINT style = GetWindowLong(hFocusWin, GWL_STYLE);
        LOG(&k_dxtools, "dxtoolsPresent: window style: 0x%x", style);
        if (dxconfig.window_style != 0xFFFFFFFF) {
            style = dxconfig.window_style;
            SetWindowLong(hFocusWin, GWL_STYLE, style);
            LOG(&k_dxtools, "dxtoolsPresent: window new style: 0x%x", style);
        }
        if (dxconfig.window_exstyle != 0xFFFFFFFF) {
            SetWindowLong(hFocusWin, GWL_EXSTYLE, dxconfig.window_exstyle);
            LOG(&k_dxtools, "dxtoolsPresent: window new exstyle: 0x%x", dxconfig.window_exstyle);
        }
        RECT rect;
        rect.left = dxconfig.window_x;
        rect.top = dxconfig.window_y;
        rect.right = rect.left + dxconfig.window.width;
        rect.bottom = rect.top + dxconfig.window.height;
        LOG(&k_dxtools, "dxtoolsPresent: window client area: {%d,%d,%d,%d}", rect.left, rect.top, rect.right, rect.bottom);
        AdjustWindowRect(&rect, style, FALSE);
        LOG(&k_dxtools, "dxtoolsPresent: window rect adjusted: {%d,%d,%d,%d}", rect.left, rect.top, rect.right, rect.bottom);
        SetWindowPos(hFocusWin, HWND_NOTOPMOST, rect.left, rect.top, rect.right, rect.bottom, SWP_FRAMECHANGED); 
    }

    // enforce internal resolution
    if (dxconfig.internal.width>0 && dxconfig.internal.height>0) {
        SetInternalResolution(&dxconfig);
    }

    UnhookFunction(hk_D3D_Present,(DWORD)dxtoolsPresent);
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
        else if (lstrcmp(name, "dx.window.x")==0)
		{
			if (sscanf(pValue, "%d", &value)!=1) continue;
			LogWithNumber(&k_dxtools,"ReadConfig: dx.window.x = (%d)", value);
            config->window_x = value;
		}
        else if (lstrcmp(name, "dx.window.y")==0)
		{
			if (sscanf(pValue, "%d", &value)!=1) continue;
			LogWithNumber(&k_dxtools,"ReadConfig: dx.window.y = (%d)", value);
            config->window_y = value;
		}
        else if (lstrcmp(name, "dx.window.style")==0)
		{
			if (sscanf(pValue, "%x", &value)!=1) continue;
			LogWithNumber(&k_dxtools,"ReadConfig: dx.window.style = (0x%x)", value);
            config->window_style = value;
		}
        else if (lstrcmp(name, "dx.window.exstyle")==0)
		{
			if (sscanf(pValue, "%x", &value)!=1) continue;
			LogWithNumber(&k_dxtools,"ReadConfig: dx.window.exstyle = (0x%x)", value);
            config->window_exstyle = value;
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

