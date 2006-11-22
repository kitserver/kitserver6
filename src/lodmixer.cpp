// lodmixer.cpp
#include <windows.h>
#include <stdio.h>
#include "lodcfg.h"
#include "lodmixer.h"
#include "kload_exp.h"
#include "input.h"

KMOD k_lodmixer={MODID,NAMELONG,NAMESHORT,DEFAULT_DEBUG};

HINSTANCE hInst;

LCM g_lcm;
BYTE g_crowdCheck = 0;
BYTE g_lodLevels[] = {0,1,2,3,4};
BYTE g_JapanCheck = 0;
BYTE g_aspectCheck = 0;
bool inited=false;
bool isSelectMode=false;
int setting=0;
enum {SET_TIMEOFDAY, SET_WEATHER, SET_SEASON, SET_NUMSUBS, SET_EFFECTS,
		SET_CROWDSTANCE, SET_HOMECROWD, SET_AWAYCROWD,	
		SET_LAST, //equal to number of settings
};
int numValues[]={4,7,4,6,3,4,5,5};
int values[]={0,0,0,0,0,0,0,0};
char* timeOfDay_Values[]={
	"Game choice",
	"Day",
	"Night",
	"Random",
};
char* weather_Values[]={
	"Game choice",
	"Fine",
	"Rain",
	"Snow",
	"Konami Random",
	"Warm Random",
	"Cold Random",
};
char* season_Values[]={
	"Game choice",
	"Summer",
	"Winter",
	"Random",
};
char* crowdStance_Values[]={
	"Game choice",
	"Home/Away",
	"Neutral",
	"Player",
};
char* crowd_Values[]={
	"Game choice",
	"(0) ",
	"(1) +",
	"(2) + +",
	"(3) + + +",
};

#define DATALEN 25
enum {C_CAMERACHECK_CS,
	C_LOD1, C_LOD2, C_LOD3, C_LOD4, C_LOD5,
	LOD_TABLE, TEAM_IDS, C_KONAMI_WEATHER_HACK_CS,
    C_JAP0, C_JAP1, C_JAP2, C_JAP3, C_JAP4, C_JAP5, C_JAP6,
    C_JAP7, C_JAP8, C_JAP9, C_JAP10, C_JAP11, C_JAP12, C_JAP13,
    CULL_W, PROJ_W,
};
    
DWORD dataArray[][DATALEN] = {
    // PES6
	{0x964130, 
     0x8afb90, 0x8afc50, 0x8afd10, 0x8afdd0, 0x8afe70, 
     0xd3bc88, 0x3be0940, 0,
     0, 0, 0, 0, 0, 0, 0,
     0, 0, 0, 0, 0, 0, 0,
     0xf84064, 0xf84090},
};

DWORD data[DATALEN];

EXTERN_C BOOL WINAPI DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID lpReserved);
void InitLodmixer();
void JuceSetLodMixerData();
void InitLODMixerData();
void SaveLODMixerData();
BYTE GetRandom(BYTE val, BYTE randBit);
BYTE GetLCMWeather(BYTE season, BYTE weather);
void lodBeginUniSelect();
void lodEndUniSelect();
void ReadMenuData();
void lodShowMenu();
void lodInput(int code1, WPARAM wParam, LPARAM lParam);
void lodReset(IDirect3DDevice8* self, LPVOID params);
void lodCreateDevice(IDirect3D8* self, UINT Adapter,
    D3DDEVTYPE DeviceType, HWND hFocusWindow, DWORD BehaviorFlags,
    D3DPRESENT_PARAMETERS *p, IDirect3DDevice8** ppReturnedDeviceInterface);
void correctAspectRatio(UINT width, UINT height);
void CheckInput();


EXTERN_C BOOL WINAPI DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID lpReserved)
{
	if (dwReason == DLL_PROCESS_ATTACH)
	{
		Log(&k_lodmixer,"Attaching dll...");
		
		hInst=hInstance;
			
		RegisterKModule(&k_lodmixer);
		
		HookFunction(hk_D3D_Create,(DWORD)InitLodmixer);
        HookFunction(hk_D3D_CreateDevice,(DWORD)lodCreateDevice);
        HookFunction(hk_D3D_Reset,(DWORD)lodReset);
		
		HookFunction(hk_DrawKitSelectInfo,(DWORD)lodShowMenu);
        HookFunction(hk_OnShowMenu,(DWORD)lodBeginUniSelect);
        HookFunction(hk_OnHideMenu,(DWORD)lodEndUniSelect);
        HookFunction(hk_EndUniSelect,(DWORD)lodEndUniSelect);
        
        HookFunction(hk_Input,(DWORD)lodInput);
		
	}
	else if (dwReason == DLL_PROCESS_DETACH)
	{
		Log(&k_lodmixer,"Detaching dll...");
		
		SaveLODMixerData();
		
		UnhookFunction(hk_SetLodMixerData,(DWORD)JuceSetLodMixerData);
		UnhookFunction(hk_DrawKitSelectInfo,(DWORD)lodShowMenu);
        UnhookFunction(hk_OnShowMenu,(DWORD)lodBeginUniSelect);
        UnhookFunction(hk_OnHideMenu,(DWORD)lodEndUniSelect);
        UnhookFunction(hk_EndUniSelect,(DWORD)lodEndUniSelect);
        
        UnhookFunction(hk_Input,(DWORD)lodInput);
		
		Log(&k_lodmixer,"Detaching done.");
	};

	return true;
};

void InitLodmixer()
{
    Log(&k_lodmixer, "InitLodMixer called.");
	memcpy(data, dataArray[GetPESInfo()->GameVersion], sizeof(data));
	
	// configure LOD-mixer data
	InitLODMixerData();

	HookFunction(hk_SetLodMixerData,(DWORD)JuceSetLodMixerData);
	
	// disable camera-check (a.k.a. "crowd unblock")
    if (data[C_CAMERACHECK_CS] && g_crowdCheck) {
        DWORD protection = 0;
        DWORD newProtection = PAGE_EXECUTE_READWRITE;
        BYTE* addr = (BYTE*)data[C_CAMERACHECK_CS];
        if (VirtualProtect(addr, 4, newProtection, &protection))
        {
            *addr = 0xc3; // retn
            Log(&k_lodmixer,"Crowd unblock activated.");
        };
    };

   	// disable Japan-check
    if (data[C_JAP0] && g_JapanCheck) {
        DWORD protection = 0;
        DWORD newProtection = PAGE_EXECUTE_READWRITE;
        for (int i=0; i<7; i++) {
            WORD* addr = (WORD*)(data[C_JAP0+i]+2);
            if (VirtualProtect(addr, 4, newProtection, &protection))
            {
                *addr = 0xffff;
            }
        }
        Log(&k_lodmixer,"Japan check disabled.");
    }
    
	// set LOD levels
    if (data[LOD_TABLE]) {
        DWORD protection = 0;
        DWORD newProtection = PAGE_EXECUTE_READWRITE;
        DWORD* addr = (DWORD*)data[LOD_TABLE];
        if (VirtualProtect(addr, 4*5, newProtection, &protection))
        {
            addr[0] = data[C_LOD1 + g_lodLevels[0]];
            addr[1] = data[C_LOD1 + g_lodLevels[1]];
            addr[2] = data[C_LOD1 + g_lodLevels[2]];
            addr[3] = data[C_LOD1 + g_lodLevels[3]];
            addr[4] = data[C_LOD1 + g_lodLevels[4]];
            
            char buf[80]; ZeroMemory(buf, 80);
            sprintf(buf, "Lod levels set to %d,%d,%d,%d,%d.",
                    g_lodLevels[0]+1, g_lodLevels[1]+1, g_lodLevels[2]+1,
                    g_lodLevels[3]+1, g_lodLevels[4]+1);
            Log(&k_lodmixer,buf);
        };
    };

    // Konami-weather hack
    if (data[C_KONAMI_WEATHER_HACK_CS]) {
        DWORD protection = 0;
        DWORD newProtection = PAGE_EXECUTE_READWRITE;
        BYTE* addr = (BYTE*)data[C_KONAMI_WEATHER_HACK_CS];
        if (VirtualProtect(addr, 4, newProtection, &protection))
        {
            addr[0] = 0x90; // nop
            addr[1] = 0x90; // nop
        }
    }	
	return;
};

/**
 * Copies LOD-Mixer settings to proper place in memory.
 */
void JuceSetLodMixerData()
{
	ReadMenuData();
	g_lcm.stadium=GetLCMStadium();
	
    // apply settings
    LCM* inmem = (LCM*)data[TEAM_IDS];
    DWORD protection = 0;
    DWORD newProtection = PAGE_EXECUTE_READWRITE;
    if (VirtualProtect(inmem, sizeof(LCM), newProtection, &protection)) {
    	if (g_lcm.stadium != 0xff)     inmem->stadium = g_lcm.stadium;
        if (g_lcm.timeOfDay != 0xff)   inmem->timeOfDay = GetRandom(g_lcm.timeOfDay, 1);
        if (g_lcm.season != 0xff)      inmem->season = GetRandom(g_lcm.season, 2);
        if (g_lcm.weather != 0xff) {
            BYTE weather = GetLCMWeather(inmem->season, g_lcm.weather);
            if (weather != 3) inmem->weather = weather;
        }
        if (g_lcm.effects != 0xff)     inmem->effects = g_lcm.effects;
        if (g_lcm.numSubs != 0xff)     inmem->numSubs = g_lcm.numSubs;
        if (g_lcm.crowdStance != 0xff) inmem->crowdStance = g_lcm.crowdStance;
        if (g_lcm.homeCrowd != 0xff)   inmem->homeCrowd = g_lcm.homeCrowd;
        if (g_lcm.awayCrowd != 0xff)   inmem->awayCrowd = g_lcm.awayCrowd;
	}

    LogWithNumber(&k_lodmixer,"JuceSetLodMixerData: season = %d", inmem->season);
    LogWithNumber(&k_lodmixer,"JuceSetLodMixerData: weather = %d", inmem->weather);
    LogWithNumber(&k_lodmixer,"JuceSetLodMixerData: homeCrowd = %d", inmem->homeCrowd);
    LogWithNumber(&k_lodmixer,"JuceSetLodMixerData: awayCrowd = %d", inmem->awayCrowd);
    Log(&k_lodmixer,"JuceSetLodMixerData done.");
    return;
};

/**
 * Load LOD-mixer settings from config.
 */
void InitLODMixerData()
{
	LCM lcm;
    memset(&lcm,0xff,sizeof(LCM));
	
    // load LOD-mixer data
    char lodmixerCfg[BUFLEN];
    ZeroMemory(lodmixerCfg, BUFLEN);
    sprintf(lodmixerCfg, "%s\\lodmixer.dat", GetPESInfo()->mydir);
    FILE* f = fopen(lodmixerCfg, "rb");
    if (f) {
        fread(&lcm, sizeof(LCM), 1, f);
        fread(&g_crowdCheck, 1, 1, f);
        fread(&g_lodLevels, sizeof(g_lodLevels), 1, f);
        fread(&g_JapanCheck, 1, 1, f);
        fread(&g_aspectCheck, 1, 1, f);
        fclose(f);
    }
    
	int val=lcm.timeOfDay;
	int val2=0;
	if (val==0xff)
		val2=0;
	else
		val2=val+1;
	values[SET_TIMEOFDAY]=val2;
		
	
	val=lcm.weather;
	if (val==0xff)
		val2=0;
	else
		val2=val+1;
	values[SET_WEATHER]=val2;
		
	
	val=lcm.season;
	if (val==0xff)
		val2=0;
	else
		val2=val+1;
	values[SET_SEASON]=val2;
		
	
	val=lcm.numSubs;
	if (val==0xff)
		val2=0;
	else
		val2=val-2;
	values[SET_NUMSUBS]=val2;

	
	val=lcm.effects;
	if (val==0xff)
		val2=0;
	else
		val2=2-val;
	values[SET_EFFECTS]=val2;
		
		
	val=lcm.crowdStance;
	if (val==0xff)
		val2=0;
	else
		val2=val;
	values[SET_CROWDSTANCE]=val2;
	
	
	val=lcm.homeCrowd;
	if (val==0xff)
		val2=0;
	else
		val2=val+1;
	values[SET_HOMECROWD]=val2;
	
	
	val=lcm.awayCrowd;
	if (val==0xff)
		val2=0;
	else
		val2=val+1;
	values[SET_AWAYCROWD]=val2;
	
	
    //will be filled later
    memset(&g_lcm,0xff,sizeof(LCM));
    
	inited=true;
    
    return;
};

void SaveLODMixerData()
{
	if (!inited) return;
	
	ReadMenuData();
	g_lcm.stadium=0xff;
	
	// save LOD-mixer data
    char lodmixerCfg[BUFLEN];
    ZeroMemory(lodmixerCfg, BUFLEN);
    sprintf(lodmixerCfg, "%s\\lodmixer.dat", GetPESInfo()->mydir);
	FILE* f = fopen(lodmixerCfg, "wb");
    if (f) {
        fwrite(&g_lcm, sizeof(LCM), 1, f);
        fwrite(&g_crowdCheck, 1, 1, f);
        fwrite(&g_lodLevels, sizeof(g_lodLevels), 1, f);
        fwrite(&g_JapanCheck, 1, 1, f);
        fwrite(&g_aspectCheck, 1, 1, f);
        fclose(f);
      };
        
	return;
};

/**
 * Weather generator function.
 */
BYTE GetLCMWeather(BYTE season, BYTE weather)
{
    DWORD numTicks = GetTickCount();
    LogWithNumber(&k_lodmixer,"GetLCMWeather: tick-count % 27 = %d", numTicks % 27);
    if (weather > 3) switch (weather) {
        case 4: // Warm
            if (season == 0) { // summer
                return (numTicks % 27) / 20;
            } else { // winter
                return ((numTicks % 27) < 15)?0:(numTicks % 2)+1;
            }
        case 5: // Cold
            if (season == 0) { // summer
                return (numTicks % 27) / 15;
            } else { // winter
                return ((numTicks % 27) < 11)?0:(numTicks % 2)+1;
            }
            break;
    }
    return weather; // return as is
}


//529d74
//52c741

void lodBeginUniSelect()
{
	dksiSetMenuTitle("Settings control");
	isSelectMode=true;
	
	setting=0;
};

void lodEndUniSelect()
{
	isSelectMode=false;
	
	JuceSetLodMixerData();
};

void ReadMenuData()
{
	int val=values[SET_TIMEOFDAY];
	if (val==0)
		g_lcm.timeOfDay=0xff;
	else
		g_lcm.timeOfDay=val-1;
		
	
	val=values[SET_WEATHER];
	if (val==0)
		g_lcm.weather=0xff;
	else
		g_lcm.weather=val-1;
		
	
	val=values[SET_SEASON];
	if (val==0)
		g_lcm.season=0xff;
	else
		g_lcm.season=val-1;
		
	
	val=values[SET_NUMSUBS];
	if (val==0)
		g_lcm.numSubs=0xff;
	else
		g_lcm.numSubs=val+2;
		
	
	val=values[SET_EFFECTS];
	if (val==0)
		g_lcm.effects=0xff;
	else
		g_lcm.effects=2-val;
		
		
	val=values[SET_CROWDSTANCE];
	if (val==0)
		g_lcm.crowdStance=0xff;
	else
		g_lcm.crowdStance=val;
	
	
	val=values[SET_HOMECROWD];
	if (val==0)
		g_lcm.homeCrowd=0xff;
	else
		g_lcm.homeCrowd=val-1;
	
	
	val=values[SET_AWAYCROWD];
	if (val==0)
		g_lcm.awayCrowd=0xff;
	else
		g_lcm.awayCrowd=val-1;
	
	
	return;
};

void lodShowMenu()
{
	DWORD color;
	char buf[255];
	
	//weather
	sprintf(buf,"Time of day: %s",timeOfDay_Values[values[SET_TIMEOFDAY]]);
	color=(values[SET_TIMEOFDAY]==0)?0xffc0c0c0:0xffffffff;
	if (setting==SET_TIMEOFDAY) color=0xffffff00;
	KDrawText(26,638,0xff000000,12,buf);
	KDrawText(24,636,color,12,buf);
	
	sprintf(buf,"Weather: %s",weather_Values[values[SET_WEATHER]]);
	color=(values[SET_WEATHER]==0)?0xffc0c0c0:0xffffffff;
	if (setting==SET_WEATHER) color=0xffffff00;
	KDrawText(26,658,0xff000000,12,buf);
	KDrawText(24,656,color,12,buf);
	
	sprintf(buf,"Season: %s",season_Values[values[SET_SEASON]]);
	color=(values[SET_SEASON]==0)?0xffc0c0c0:0xffffffff;
	if (setting==SET_SEASON) color=0xffffff00;
	KDrawText(26,678,0xff000000,12,buf);
	KDrawText(24,676,color,12,buf);
	
	//other
	if (values[SET_NUMSUBS]==0)
		strcpy(buf,"Substitutes: Game choice");
	else
		sprintf(buf,"Substitutes: %d",values[SET_NUMSUBS]+2);
	
	color=(values[SET_NUMSUBS]==0)?0xffc0c0c0:0xffffffff;
	if (setting==SET_NUMSUBS) color=0xffffff00;
	KDrawText(358,638,0xff000000,12,buf);
	KDrawText(356,636,color,12,buf);
	
	if (values[SET_EFFECTS]==0)
		strcpy(buf,"Effects: Game choice");
	else if (values[SET_EFFECTS]==1)
		strcpy(buf,"Effects: On");
	else
		strcpy(buf,"Effects: Off");
	
	color=(values[SET_EFFECTS]==0)?0xffc0c0c0:0xffffffff;
	if (setting==SET_EFFECTS) color=0xffffff00;
	KDrawText(358,658,0xff000000,12,buf);
	KDrawText(356,656,color,12,buf);
	
	//crowd
	sprintf(buf,"Crowd stance: %s",crowdStance_Values[values[SET_CROWDSTANCE]]);
	color=(values[SET_CROWDSTANCE]==0)?0xffc0c0c0:0xffffffff;
	if (setting==SET_CROWDSTANCE) color=0xffffff00;
	KDrawText(708,638,0xff000000,12,buf);
	KDrawText(706,636,color,12,buf);
	
	sprintf(buf,"Home crowd: %s",crowd_Values[values[SET_HOMECROWD]]);
	color=(values[SET_HOMECROWD]==0)?0xffc0c0c0:0xffffffff;
	if (setting==SET_HOMECROWD) color=0xffffff00;
	KDrawText(708,658,0xff000000,12,buf);
	KDrawText(706,656,color,12,buf);
	
	sprintf(buf,"Away crowd: %s",crowd_Values[values[SET_AWAYCROWD]]);
	color=(values[SET_AWAYCROWD]==0)?0xffc0c0c0:0xffffffff;
	if (setting==SET_AWAYCROWD) color=0xffffff00;
	KDrawText(708,678,0xff000000,12,buf);
	KDrawText(706,676,color,12,buf);

    // check input
    CheckInput();
};

void CheckInput()
{
	if (isSelectMode) {
        DWORD* inputs = GetInputTable();
        KEYCFG* keyCfg = GetInputCfg();
        for (int n=0; n<8; n++) {
            if (INPUT_EVENT(inputs, n, FUNCTIONAL, keyCfg->gamepad.keyNext)) {
                setting++;
                if (setting>=SET_LAST)
                    setting=0;
            } else if (INPUT_EVENT(inputs, n, FUNCTIONAL, keyCfg->gamepad.keyPrev)) {
                if (setting==0)
                    setting=SET_LAST;
                setting--;
            } else if (INPUT_EVENT(inputs, n, FUNCTIONAL, keyCfg->gamepad.keyNextVal)) {
                values[setting]++;
                if (values[setting]>=numValues[setting])
                    values[setting]=0;
            } else if (INPUT_EVENT(inputs, n, FUNCTIONAL, keyCfg->gamepad.keyPrevVal)) {
                if (values[setting]==0)
                    values[setting]=numValues[setting];
                values[setting]--;
            }
        }
	}
}

void lodInput(int code1, WPARAM wParam, LPARAM lParam)
{
	if (code1 < 0)
		return;
		
	if ((code1!=HC_ACTION) || !(lParam & 0x80000000))
		return;

    KEYCFG* keyCfg = GetInputCfg();
	if (isSelectMode) {
		if (wParam == keyCfg->keyboard.keyNext) {
			setting++;
			if (setting>=SET_LAST)
				setting=0;
		} else if (wParam == keyCfg->keyboard.keyPrev) {
			if (setting==0)
				setting=SET_LAST;
			setting--;
		} else if (wParam == keyCfg->keyboard.keyNextVal) {
			values[setting]++;
			if (values[setting]>=numValues[setting])
				values[setting]=0;
		} else if (wParam == keyCfg->keyboard.keyPrevVal) {
			if (values[setting]==0)
				values[setting]=numValues[setting];
			values[setting]--;
		};
		
		return;
	};
	
	return;	
};

BYTE GetRandom(BYTE val, BYTE randBit)
{
	if (val != 2)
		return val;
	
	DWORD num=GetTickCount();
	if (num & (1<<randBit) > 0)
		return 1;
	else
		return 0;
};

void lodReset(IDirect3DDevice8* self, LPVOID params)
{
    D3DPRESENT_PARAMETERS *p = (D3DPRESENT_PARAMETERS*)params;
    correctAspectRatio(p->BackBufferWidth, p->BackBufferHeight);
}

void lodCreateDevice(IDirect3D8* self, UINT Adapter,
    D3DDEVTYPE DeviceType, HWND hFocusWindow, DWORD BehaviorFlags,
    D3DPRESENT_PARAMETERS *p, IDirect3DDevice8** ppReturnedDeviceInterface) 
{
    correctAspectRatio(p->BackBufferWidth, p->BackBufferHeight);
}
	
void correctAspectRatio(UINT width, UINT height)
{
    if (g_aspectCheck) {
        Log(&k_lodmixer,"adjusting aspect ratio corrector.");

        DWORD protection = 0;
        DWORD newProtection = PAGE_EXECUTE_READWRITE;
        float* projW = (float*)data[PROJ_W];
        WORD* cullW = (WORD*)data[CULL_W];
        LogWithNumber(&k_lodmixer, "projW = %08x", (DWORD)projW);
        LogWithNumber(&k_lodmixer, "cullW = %08x", (DWORD)cullW);
        if (projW && cullW && VirtualProtect(cullW, 0x80, newProtection, &protection)) {
            float reverse_aspect_ratio = height / (float)width;
            float ar_correction = 0.75 / reverse_aspect_ratio;
            *projW = 512.0 * ar_correction;
            *cullW = (WORD)(512 * ar_correction);
            LogWithFloat(&k_lodmixer, "projWidth = %0.5f", *projW);
            LogWithNumber(&k_lodmixer, "cullWidth = %d", (DWORD)*cullW);
        }
    }
}

