// psc.cpp
#include <windows.h>
#include <stdio.h>
#include "psc.h"
#include "kload_exp.h"

KMOD k_psc={MODID,NAMELONG,NAMESHORT,DEFAULT_DEBUG};

HINSTANCE hInst;

// PSC only supports PES6 anyway, so no need for filling in the addresses
#define CODELEN 2
enum {
    C_MAKESTATISTICS_CS, C_ISMATCHOVER,
};

DWORD codeArray[][CODELEN] = {
	// PES6
	{
		0x77bd45, 0x773ed0,
    },
	// PES6 1.10
	{
        0, 0,
    },
    // WE2007
	{
        0, 0,
    },
};

#define DATALEN 2
enum {
    ISMATCHOVER_FLAG1, DIFFICULTY,
};

DWORD dataArray[][DATALEN] = {
	// PES6
	{
	 0x10c9947, 0x3be094c,
    },
	// PES6 1.10
	{
	 0, 0,
    },
    // WE2007
	{
	 0, 0,
    },
};

DWORD code[CODELEN];
DWORD data[DATALEN];

typedef DWORD (*ISMATCHOVER)();

char pscExe[BUFLEN];

DWORD showCaptureMessage = 0;

EXTERN_C BOOL WINAPI DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID lpReserved);
void InitPSC();
BOOL ReadConfig(char* cfgFile);
DWORD pscMakeStatistics();
void pscPresent(IDirect3DDevice8* self, CONST RECT* src, CONST RECT* dest, HWND hWnd, LPVOID unused);


EXTERN_C BOOL WINAPI DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID lpReserved)
{
	int i,j;
	
	if (dwReason == DLL_PROCESS_ATTACH)
	{
		Log(&k_psc,"Attaching dll...");
		
		hInst=hInstance;
		
		int v=GetPESInfo()->GameVersion;
		switch (v) {
			case gvPES6PC:
			//case gvPES6PC110:
			//case gvWE2007PC:
				goto GameVersIsOK;
				break;
		};
		//Will land here if game version is not supported
		Log(&k_psc,"Your game version is currently not supported!");
		return false;
		
		//Everything is OK!
		GameVersIsOK:

		RegisterKModule(&k_psc);
		
		// the config file is written by PSC when selecting the pes6.exe there
		char pscCfg[BUFLEN];
		ZeroMemory(pscCfg, BUFLEN);
	    sprintf(pscCfg, "%s\\psc.cfg", GetPESInfo()->mydir);
	    
	    ZeroMemory(pscExe, BUFLEN);
        ReadConfig(pscCfg);
        
        if (strlen(pscExe) == 0) {
        	Log(&k_psc, "No exe file was set!");
        	return false;
        } else {
        	strcat(pscExe, " -capture");
        }
		
		memcpy(code, codeArray[v], sizeof(code));
		memcpy(data, dataArray[v], sizeof(data));
		
		HookFunction(hk_D3D_Create,(DWORD)InitPSC);
		
	}
	else if (dwReason == DLL_PROCESS_DETACH)
	{
		Log(&k_psc,"Detaching dll...");
		
		MasterUnhookFunction(code[C_MAKESTATISTICS_CS], pscMakeStatistics);
		UnhookFunction(hk_D3D_Present,(DWORD)pscPresent);
		
		Log(&k_psc,"Detaching done.");
	};
	
	return true;
};

void InitPSC()
{
	Log(&k_psc,"Init PSC...");

	MasterHookFunction(code[C_MAKESTATISTICS_CS], 0, pscMakeStatistics);

	Log(&k_psc, "hooking done");

	return;
};

BOOL ReadConfig(char* cfgFile)
{
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

		if (lstrcmp(name, "exe")==0)
		{
			char* startQuote = strstr(pValue, "\"");
			if (startQuote == NULL) continue;
			char* endQuote = strstr(startQuote + 1, "\"");
			if (endQuote == NULL) continue;

			ZeroMemory(pscExe, BUFLEN);
			memcpy(pscExe, startQuote + 1, endQuote - startQuote - 1);
			
			LogWithString(&k_psc,"ReadConfig: exe = {%s}", pscExe);	
		}
	}
	fclose(cfg);
	return true;
}

DWORD pscMakeStatistics()
{
	DWORD res = MasterCallNext();
	
	if (*(BYTE*)data[ISMATCHOVER_FLAG1] != 0) return res;
	DWORD isMatchOver = ((ISMATCHOVER)(code[C_ISMATCHOVER]))();
	if (isMatchOver == 0) return res;

	if (strlen(pscExe) > 0) {
		WinExec(pscExe, SW_HIDE);
	}
	
	//show a message for 4 seconds
	showCaptureMessage = GetTickCount() + 4000;
	HookFunction(hk_D3D_Present,(DWORD)pscPresent);

	return res;
}

void pscPresent(IDirect3DDevice8* self, CONST RECT* src, CONST RECT* dest, HWND hWnd, LPVOID unused)
{
	if (GetTickCount() > showCaptureMessage) {
		UnhookFunction(hk_D3D_Present,(DWORD)pscPresent);
		return;
	}
		
	KDrawText(10, 746, 0xffffffc0, 12, "Statistics were saved.");
		
	return;
}
