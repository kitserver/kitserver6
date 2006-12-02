/* KitServer Loader */

#include <windows.h>
#define _COMPILING_KLOAD
#include <stdio.h>

#include "manage.h"
#include "kload_config.h"
#include "log.h"
#include "hook.h"
#include "imageutil.h"
#include "detect.h"
#include "kload.h"
#include "keycfg.h"

#include <map>

KMOD k_kload={MODID,NAMELONG,NAMESHORT,DEFAULT_DEBUG};

HINSTANCE hInst;
KLOAD_CONFIG g_config={0,NULL,FALSE,FALSE};
PESINFO g_pesinfo={NULL,NULL,NULL,NULL,NULL,NULL,DEFAULT_GDB_DIR,-1,NULL,0,0,0.0,0.0};
KEYCFG g_keyCfg = {
    {
        KEYCFG_KEYBOARD_DEFAULT_SWITCH_LEFT,
        KEYCFG_KEYBOARD_DEFAULT_SWITCH_RIGHT,
        KEYCFG_KEYBOARD_DEFAULT_RESET,
        KEYCFG_KEYBOARD_DEFAULT_RANDOM,
        KEYCFG_KEYBOARD_DEFAULT_PREV,
        KEYCFG_KEYBOARD_DEFAULT_NEXT,
        KEYCFG_KEYBOARD_DEFAULT_PREVVAL,
        KEYCFG_KEYBOARD_DEFAULT_NEXTVAL,
        KEYCFG_KEYBOARD_DEFAULT_INFOPAGEPREV,
        KEYCFG_KEYBOARD_DEFAULT_INFOPAGENEXT,
        KEYCFG_KEYBOARD_DEFAULT_ACTION1,
        KEYCFG_KEYBOARD_DEFAULT_ACTION2,
        KEYCFG_KEYBOARD_DEFAULT_SWITCH1,
        KEYCFG_KEYBOARD_DEFAULT_SWITCH2,
    },
    {
        KEYCFG_GAMEPAD_DEFAULT_SWITCH_LEFT,
        KEYCFG_GAMEPAD_DEFAULT_SWITCH_RIGHT,
        KEYCFG_GAMEPAD_DEFAULT_RESET,
        KEYCFG_GAMEPAD_DEFAULT_RANDOM,
        KEYCFG_GAMEPAD_DEFAULT_PREV,
        KEYCFG_GAMEPAD_DEFAULT_NEXT,
        KEYCFG_GAMEPAD_DEFAULT_PREVVAL,
        KEYCFG_GAMEPAD_DEFAULT_NEXTVAL,
        KEYCFG_GAMEPAD_DEFAULT_INFOPAGEPREV,
        KEYCFG_GAMEPAD_DEFAULT_INFOPAGENEXT,
        KEYCFG_GAMEPAD_DEFAULT_ACTION1,
        KEYCFG_GAMEPAD_DEFAULT_ACTION2,
        KEYCFG_GAMEPAD_DEFAULT_SWITCH1,
        KEYCFG_GAMEPAD_DEFAULT_SWITCH2,
    },
};

SAVEGAMEFUNCS* SavegameFuncs;
SAVEGAMEEXPFUNCS SavegameExpFuncs[numSaveGameExpModules];

std::map<DWORD,THREEDWORDS> g_SpecialAfsIds;
std::map<DWORD,THREEDWORDS>::iterator g_SpecialAfsIdsIterator;
DWORD currSpecialAfsId=0;

// global hook manager
static hook_manager _hook_manager;
DWORD lastCallSite=0;

extern char* GAME[];

#ifndef MYDLL_RELEASE_BUILD
#define KLOG(mydir,lib) { char klogFile[BUFLEN];\
    ZeroMemory(klogFile, BUFLEN);\
    sprintf(klogFile, "%s\\kload.log", mydir);\
    FILE* klog = fopen(klogFile, "at");\
    if (klog) { fprintf(klog, "Loading library: %s\n", lib); }\
    fclose(klog); }
#else
#define KLOG(mydir,lib)
#endif


EXTERN_C BOOL WINAPI DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID lpReserved);
void SetPESInfo();

KEXPORT KEYCFG* GetInputCfg()
{
    return &g_keyCfg;
}

/*******************/
/* DLL Entry Point */
/*******************/
EXTERN_C BOOL WINAPI DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID lpReserved)
{
	DWORD wbytes, procId; 

    char tmp[BUFLEN];

	if (dwReason == DLL_PROCESS_ATTACH)
	{
		hInst=hInstance;
		
		SetPESInfo();
		
		OpenLog(g_pesinfo.logName);
		
		// read configuration
		char cfgFile[BUFLEN];
		ZeroMemory(cfgFile, BUFLEN);
		lstrcpy(cfgFile, g_pesinfo.mydir); 
		lstrcat(cfgFile, CONFIG_FILE);

		//Enable logging at the beginning
		k_kload.debug=1;
		Log(&k_kload,"Log started.");
		RegisterKModule(&k_kload);
		
		ReadConfig(&g_config, cfgFile);

        //read key bindings
        char keyCfgFile[BUFLEN];
        sprintf(keyCfgFile,"%skeybind.dat",g_pesinfo.mydir);
        LogWithString(&k_kload,"Reading key bindings from {%s}", keyCfgFile);
		ReadKeyCfg(&g_keyCfg, keyCfgFile);
		
		// adjust gdbDir, if it is specified as relative path
		if (g_pesinfo.gdbDir[0] == '.')
		{
			// it's a relative path. Therefore do it relative to mydir
			char temp[BUFLEN];
			ZeroMemory(temp, BUFLEN);
			lstrcpy(temp,g_pesinfo.mydir); 
			lstrcat(temp, g_pesinfo.gdbDir);
			delete g_pesinfo.gdbDir;
			g_pesinfo.gdbDir=new char[strlen(temp)+1];
			ZeroMemory(g_pesinfo.gdbDir, strlen(temp)+1);
			lstrcpy(g_pesinfo.gdbDir, temp);
		}
		
		//if debugging should'nt be enabled, delete log file
		if (k_kload.debug<1) {
			CloseLog();
			DeleteFile(g_pesinfo.logName);
		} else {
			strcpy(tmp,GetPESInfo()->mydir);
			strcat(tmp,"mlog.tmp");
			OpenMLog(200,tmp);
			TRACE(&k_kload,"Opened memory log.");
		};		
		
		LogWithString(&k_kload,"Game version: %s", GAME[g_pesinfo.GameVersion]);
		InitAddresses(g_pesinfo.GameVersion);
		
		ZeroMemory(SavegameExpFuncs,sizeof(SavegameExpFuncs));
		
		_hook_manager.SetCallHandler(MasterCallFirst);
		
        // load libs
        for (int i=0;i<g_config.numDLLs;i++) {
        	if (g_config.dllnames[i]==NULL) continue;
        	LogWithString(&k_kload,"Loading module \"%s\" ...",g_config.dllnames[i]);
        	if (LoadLibrary(g_config.dllnames[i])==NULL)
        		strcpy(tmp," NOT");
        	else
        		strcpy(tmp,"\0");
			LogWithString(&k_kload,"... was%s successful!",tmp);
        };
		
		HookDirect3DCreate8();

        // The hooking of game routines should happen later:
        // when the game calls Direct3DCreate8. This is a requirement
        // for SECUROM-encrypted executables (such as WE9.exe, WE9LEK.exe)        
	}

	else if (dwReason == DLL_PROCESS_DETACH)
	{
		// Release the device
        if (GetActiveDevice()) {
            GetActiveDevice()->Release();
        }
        
        UnhookReadFile();
        UnhookOthers();
        UnhookKeyb();
		
		TRACE(&k_kload,"Closing memory log.");
		strcpy(tmp,GetPESInfo()->mydir);
		strcat(tmp,"mlog.tmp");
		CloseMLog(tmp);
		
		g_SpecialAfsIds.clear();
		
		/* close specific log file */
		Log(&k_kload,"Closing log.");
		CloseLog();
	}

	return TRUE;    /* ok */
}

void SetPESInfo()
{
	char mydir[BUFLEN];
	char processfile[BUFLEN];
	char *shortProcessfile;
	char shortProcessfileNoExt[BUFLEN];
	char logName[BUFLEN];
	
	/* determine my directory */
	ZeroMemory(mydir, BUFLEN);
	GetModuleFileName(hInst, mydir, BUFLEN);
	char *q = mydir + lstrlen(mydir);
	while ((q != mydir) && (*q != '\\')) { *q = '\0'; q--; }

	g_pesinfo.mydir=new char[strlen(mydir)+1];
	strcpy(g_pesinfo.mydir,mydir);
	
	g_pesinfo.hProc = GetModuleHandle(NULL);
	
	ZeroMemory(processfile, BUFLEN);
	GetModuleFileName(NULL, processfile, BUFLEN);
	
	g_pesinfo.processfile=new char[strlen(processfile)+1];
	strcpy(g_pesinfo.processfile,processfile);
	
	char *q1 = processfile + lstrlen(processfile);
	while ((q1 != processfile) && (*q1 != '\\')) { *q1 = '\0'; q1--; }
	
	g_pesinfo.pesdir=new char[strlen(processfile)+1];
	strcpy(g_pesinfo.pesdir,processfile);
	
	GetModuleFileName(NULL, processfile, BUFLEN);

	char* zero = processfile + lstrlen(processfile);
	char* p = zero; while ((p != processfile) && (*p != '\\')) p--;
	if (*p == '\\') p++;
	shortProcessfile = p;
	
	g_pesinfo.shortProcessfile=new char[strlen(shortProcessfile)+1];
	strcpy(g_pesinfo.shortProcessfile,shortProcessfile);
		
	// save short filename without ".exe" extension.
	ZeroMemory(shortProcessfileNoExt, BUFLEN);
	char* ext = shortProcessfile + lstrlen(shortProcessfile) - 4;
	if (lstrcmpi(ext, ".exe")==0) {
		memcpy(shortProcessfileNoExt, shortProcessfile, ext - shortProcessfile); 
	}
	else {
		lstrcpy(shortProcessfileNoExt, shortProcessfile);
	}
	
	g_pesinfo.shortProcessfileNoExt=new char[strlen(shortProcessfileNoExt)+1];
	strcpy(g_pesinfo.shortProcessfileNoExt,shortProcessfileNoExt);
	
	/* open log file, specific for this process */
	ZeroMemory(logName, BUFLEN);
	lstrcpy(logName, mydir);
	lstrcat(logName, shortProcessfileNoExt); 
	lstrcat(logName, ".log");
	
	g_pesinfo.logName=new char[strlen(logName)+1];
	strcpy(g_pesinfo.logName,logName);
	
	g_pesinfo.GameVersion=GetGameVersion();
	
	return;
};

KEXPORT PESINFO* GetPESInfo()
{
	return &g_pesinfo;
};

KEXPORT void RegisterKModule(KMOD *k)
{
	LogWithTwoStrings(&k_kload,"Registering module %s (\"%s\")",k->NameShort,k->NameLong);
	return;
};

KEXPORT DWORD SetDebugLevel(DWORD ModDebugLevel)
{
	if (k_kload.debug<1)
		return 0;
	else if (k_kload.debug==255)
		return 255;
	else
		return ModDebugLevel;
};

//for savegame manager module
KEXPORT void SetSavegameExpFuncs(BYTE module, BYTE type, DWORD addr)
{
	if (module>=numSaveGameExpModules) return;
	if (type>=SGEF_LAST) return;
	
	switch (type) {
	case SGEF_GIVEIDTOMODULE:
		SavegameExpFuncs[module].giveIdToModule=(GIVEIDTOMODULE)addr;
		break;
	};
	
	return;
};

KEXPORT SAVEGAMEEXPFUNCS* GetSavegameExpFuncs(DWORD* num)
{
	if (num!=NULL)
		*num=numSaveGameExpModules;
	
	return &(SavegameExpFuncs[0]);
};


KEXPORT void SetSavegameFuncs(SAVEGAMEFUNCS* sgf)
{
	SavegameFuncs=sgf;
	return;
};

KEXPORT SAVEGAMEFUNCS* GetSavegameFuncs()
{
	return SavegameFuncs;
};


KEXPORT THREEDWORDS* GetSpecialAfsFileInfo(DWORD id)
{
	if (g_SpecialAfsIds.find(id) == g_SpecialAfsIds.end())
		return NULL;
	
	return &(g_SpecialAfsIds[id]);
};

KEXPORT DWORD GetNextSpecialAfsFileId(DWORD dw1, DWORD dw2, DWORD dw3)
{
	DWORD result=0;
	
	//search for any existing set with the same values
	for (g_SpecialAfsIdsIterator=g_SpecialAfsIds.begin();
			g_SpecialAfsIdsIterator != g_SpecialAfsIds.end(); g_SpecialAfsIdsIterator++) {
		if (g_SpecialAfsIdsIterator->second.dw1==dw1 &&
			g_SpecialAfsIdsIterator->second.dw2==dw2 &&
			g_SpecialAfsIdsIterator->second.dw3==dw3) {
				return g_SpecialAfsIdsIterator->first;
		};
	};
	
	THREEDWORDS threeDWORDs;
	threeDWORDs.dw1=dw1;
	threeDWORDs.dw2=dw2;
	threeDWORDs.dw3=dw3;
	
	//not using the fifth half byte makes the GetFileFromAFS() function fail
	result=currSpecialAfsId;
	result=((result & 0xff0000)<<4) | (result & 0xffff);
	result|=0x80070000;
		
	g_SpecialAfsIds[result]=threeDWORDs;
	
	currSpecialAfsId++;
	currSpecialAfsId%=0x1000000;
	
	return result;
};


KEXPORT bool MasterHookFunction(DWORD call_site, DWORD numArgs, void* target)
{
    hook_point hp(call_site, numArgs, (DWORD)target);
    return _hook_manager.hook(hp);
}

KEXPORT bool MasterUnhookFunction(DWORD call_site, void* target)
{
    hook_point hp(call_site, 0, (DWORD)target);
    return _hook_manager.unhook(hp);
}

DWORD oldEBP1;

DWORD MasterCallFirst(...)
{
	DWORD oldEAX, oldEBX, oldECX, oldEDX;
	DWORD oldEBP, oldEDI, oldESI;
	int i;
	__asm {
		mov oldEAX, eax
		mov oldEBX, ebx
		mov oldECX, ecx
		mov oldEDX, edx
		mov oldEBP, ebp
		mov oldEBP1, ebp
		mov oldEDI, edi
		mov oldESI, esi
	};
	
	DWORD arg;
	DWORD result=0;
	bool lastAddress=false;
	DWORD jmpDest=0;

	DWORD call_site=*(DWORD*)(oldEBP+4)-5;
	DWORD addr=_hook_manager.getFirstTarget(call_site, &lastAddress);
	
	if (addr==0) return;
	
	DWORD before=lastCallSite;
	lastCallSite=call_site;
	
	DWORD numArgs=_hook_manager.getNumArgs(call_site);
	bool wasJump=_hook_manager.getType(call_site)==HOOKTYPE_JMP;
	
	if (wasJump && lastAddress) {
		result=oldEAX;
		goto EndOfCallF;
	};
	
	//writing this as inline assembler allows to
	//give as much parameters as we want and more
	//important, we can restore all registers
	__asm {
		//push ebp
	};
	
	for (i=numArgs-1;i>=0;i--) {
		if (wasJump)
			arg=*((DWORD*)oldEBP+3+i);
		else
			arg=*((DWORD*)oldEBP+2+i);
		__asm mov eax, arg
		__asm push eax
	};
	
	__asm {
		//restore registers
		mov eax, oldEAX
		mov ebx, oldEBX
		mov ecx, oldECX
		mov edx, oldEDX
		mov edi, oldEDI
		mov esi, oldESI
		//mov ebp, oldEBP
		//mov ebp, [ebp]
		
		call ds:[addr]
		
		mov result, eax
		
	};
	
	for (i=0;i<numArgs;i++)
		__asm pop eax
	
	__asm {
		//pop ebp
		mov eax, result
	};
	
	EndOfCallF:
	
	lastCallSite=before;
	
	if (wasJump) {
		if (lastAddress)
			jmpDest=addr;
		else
			jmpDest=_hook_manager.getOriginalTarget(call_site);
		
		//change the return address to the destination of our jump
		*(DWORD*)(oldEBP+4)=jmpDest;
	};
	
	return result;
};

KEXPORT DWORD MasterCallNext(...)
{
	if (lastCallSite==0) return 0;
	
	DWORD oldEAX, oldEBX, oldECX, oldEDX;
	DWORD oldEBP, oldEDI, oldESI, numArgs;
	int i;
	__asm {
		mov oldEAX, eax
		mov oldEBX, ebx
		mov oldECX, ecx
		mov oldEDX, edx
		mov oldEBP, ebp
		mov oldEDI, edi
		mov oldESI, esi
	};
	
	DWORD result=0;
	DWORD arg;
	bool lastAddress=false;
	
	DWORD addr=_hook_manager.getNextTarget(lastCallSite, &lastAddress);
	bool wasJump=_hook_manager.getType(lastCallSite)==HOOKTYPE_JMP;
	if (addr==0) return 0;
	
	//Don't call a jump's last address (its original destination)
	if (wasJump && lastAddress) {
		result=oldEAX;
		//restore registers
		__asm {
			mov eax, oldEAX
			mov ebx, oldEBX
			mov ecx, oldECX
			mov edx, oldEDX
			mov edi, oldEDI
			mov esi, oldESI
		};
		goto EndOfCallN;
	};
	
	numArgs=_hook_manager.getNumArgs(lastCallSite);

	__asm {
		//push ebp
	};
	
	for (i=numArgs-1;i>=0;i--) {
		arg=*((DWORD*)oldEBP+2+i);
		__asm mov eax, arg
		__asm push eax
	};
	
	__asm {
		//restore registers
		mov eax, oldEAX
		mov ebx, oldEBX
		mov ecx, oldECX
		mov edx, oldEDX
		mov edi, oldEDI
		mov esi, oldESI
		//mov ebp, oldEBP
		//mov ebp, [ebp]
		
		call ds:[addr]
		
		mov result, eax
	};
	
	for (i=0;i<numArgs;i++)
		__asm pop eax
	
	__asm mov eax, result
	
	EndOfCallN:
	
	return result;
}

