// roster.cpp
#include <windows.h>
#include <winreg.h>
#include <stdio.h>
#include "roster.h"
#include "kload_exp.h"
#include "md5.h"

KMOD k_roster={MODID,NAMELONG,NAMESHORT,DEFAULT_DEBUG};

HINSTANCE hInst;
md5_state_t state;
char _versionString[17] = "1.66-FakeVersion";
int _login_credentials = 0;
char _cred_key[31] = "djkj93rajf8123bvdfg9475hpok43k";

//static DWORD dta[DATALEN];

class roster_config_t
{
public:
    roster_config_t() : 
        debug(false),
        doRosterHash(true)
        //rememberLogin(true)
    {}

    bool debug;
    bool doRosterHash;
    //bool rememberLogin;
};

EXTERN_C BOOL WINAPI DllMain(
    HINSTANCE hInstance, DWORD dwReason, LPVOID lpReserved);
void initModule();
bool readConfig(roster_config_t& config);
void rosterVersionReadCallPoint();
void rosterVersionRead(char* version);
void copyPlayerDataCallPoint();
void beforeCopyPlayerData(BYTE* dta, DWORD numDwords);
void afterComputeHashCallPoint();
void afterComputeHash(BYTE* pHash);
//void loginReadCallPoint();
//void loginRead();
//void loginWriteCallPoint();
//void loginWrite();

// global config
roster_config_t _config;


static void string_strip(string& s)
{
    static const char* empties = " \t\n\r\"";
    int e = s.find_last_not_of(empties);
    s.erase(e + 1);
    int b = s.find_first_not_of(empties);
    s.erase(0,b);
}

KEXPORT void HookCallPoint(DWORD addr, 
        void* func, int codeShift, int numNops, bool addRetn)
{
    DWORD target = (DWORD)func + codeShift;
	if (addr && target)
	{
	    BYTE* bptr = (BYTE*)addr;
	    DWORD protection = 0;
	    DWORD newProtection = PAGE_EXECUTE_READWRITE;
	    if (VirtualProtect(bptr, 16, newProtection, &protection)) {
	        bptr[0] = 0xe8;
	        DWORD* ptr = (DWORD*)(addr + 1);
	        ptr[0] = target - (DWORD)(addr + 5);
            // padding with NOPs
            for (int i=0; i<numNops; i++) bptr[5+i] = 0x90;
            if (addRetn)
                bptr[5+numNops]=0xc3;
	        TRACE2X(&k_roster, "Function (%08x) HOOKED at address (%08x)", 
                    target, addr);
	    }
	}
}

EXTERN_C BOOL WINAPI DllMain(
    HINSTANCE hInstance, DWORD dwReason, LPVOID lpReserved)
{
	int i,j;
	
	if (dwReason == DLL_PROCESS_ATTACH)
	{
		Log(&k_roster,"Attaching dll...");
		
		hInst=hInstance;
		
		int v=GetPESInfo()->GameVersion;
		switch (v) {
			case gvPES6PC:
			case gvPES6PC110:
			case gvWE2007PC:
				goto GameVersIsOK;
				break;
		};
		return false;
		
		//Everything is OK!
		GameVersIsOK:

		RegisterKModule(&k_roster);

        // initialize addresses
        memcpy(dta, dtaArray[v], sizeof(dta));
		
		HookFunction(hk_D3D_Create,(DWORD)initModule);
	}
	else if (dwReason == DLL_PROCESS_DETACH)
	{
		Log(&k_roster,"Detaching dll...");
		Log(&k_roster,"Detaching done.");
	}
	
	return true;
}

/**
 * Returns true if successful.
 */
bool readConfig(roster_config_t& config)
{
    string cfgFile(GetPESInfo()->mydir);
    cfgFile += "\\roster.cfg";

	FILE* cfg = fopen(cfgFile.c_str(), "rt");
	if (cfg == NULL) return false;

	char str[BUFLEN];
	char name[BUFLEN];
	int value = 0;
	float dvalue = 0.0f;

	char *pName = NULL, *pValue = NULL, *comment = NULL;
	while (true)
	{
        if (feof(cfg)) 
            break;
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

		if (strcmp(name, "debug")==0)
		{
			if (sscanf(pValue, "%d", &value)!=1) continue;
			config.debug = value;
		}
        else if (strcmp(name, "roster.hash")==0)
		{
			if (sscanf(pValue, "%d", &value)!=1) continue;
			config.doRosterHash = (value > 0);
		}
        /*
        else if (strcmp(name, "roster.remember.login")==0)
		{
			if (sscanf(pValue, "%d", &value)!=1) continue;
			config.rememberLogin = (value > 0);
		} 
        */
	}
	fclose(cfg);
	return true;
}

void initModule()
{
    //HookCallPoint(dta[CODE_READ_VERSION_STRING], 
    //        rosterVersionReadCallPoint, 6, 3, false);
    //HookCallPoint(dta[CODE_READ_CRED_FLAG], 
    //        loginReadCallPoint, 6, 2, false);
    //HookCallPoint(dta[CODE_WRITE_CRED_FLAG], 
    //        loginWriteCallPoint, 6, 2, false);
    
    HookCallPoint(dta[C_COMPUTE_HASH]+5, 
            afterComputeHashCallPoint, 6, 2, false);
    HookCallPoint(dta[C_COPY_PLAYER_DATA], 
            copyPlayerDataCallPoint, 6, 0, false);

    Log(&k_roster, "Network module initialized.");

    // read configuration
    readConfig(_config);

    LogWithNumber(&k_roster, "_config.debug = %d", _config.debug);
    LogWithNumber(&k_roster, "_config.doRosterHash = %d", 
            _config.doRosterHash);
    //LogWithNumber(&k_roster, "_config.rememberLogin = %d", 
    //        _config.rememberLogin);

    //_login_credentials = dta[CREDENTIALS];
}

void computeHash(void* lpBuffer, size_t len)
{
    md5_init(&state);
    md5_append(&state, 
            (const md5_byte_t *)lpBuffer, 
            len);
    /*
    // also get db.cfg
    size_t size;
    char* cfgBuffer = read_db_cfg(size);
    if (cfgBuffer && size>0) {
        md5_append(&state,
                (const md5_byte_t *)cfgBuffer,
                size);
        free(cfgBuffer);
    }
    */
    md5_finish(&state, 
            (md5_byte_t*)_versionString);
    Log(&k_roster, "Roster-HASH CALCULATED.");
}

void rosterVersionReadCallPoint()
{
    __asm {
        pushfd 
        push ebp
        push eax
        push ebx
        push ecx
        push edx
        push esi
        push edi
        mov ecx, esp
        add ecx, 8
        add ecx, 0x24  // adjust stack
        push ecx
        call rosterVersionRead
        add esp, 4  // pop parameters
        pop edi
        pop esi
        pop edx
        pop ecx
        pop ebx
        pop eax
        pop ebp
        popfd
        retn
    }
}

void rosterVersionRead(char* version)
{
    if (_config.doRosterHash) {
        memcpy(version, _versionString, 16);
    }
}

/*
void loginReadCallPoint()
{
    __asm {
        pushfd 
        push ebp
        push eax
        push ebx
        push ecx
        push edx
        push esi
        push edi
        call loginRead
        pop edi
        pop esi
        pop edx
        pop ecx
        pop ebx
        pop eax
        pop ebp
        popfd
        push eax
        mov eax, _login_credentials
        cmp byte ptr ds:[eax],1  // original code
        pop eax
        retn
    }
}

void loginWriteCallPoint()
{
    __asm {
        pushfd 
        push ebp
        push eax
        push ebx
        push ecx
        push edx
        push esi
        push edi
        call loginWrite
        pop edi
        pop esi
        pop edx
        pop ecx
        pop ebx
        pop eax
        pop ebp
        popfd
        push eax
        mov eax, _login_credentials
        mov byte ptr ds:[eax],1  // original code
        pop eax
        retn
    }
}

/*
void loginRead()
{
    LOGIN_CREDENTIALS* lc = (LOGIN_CREDENTIALS*)dta[CREDENTIALS];
    if (_config.rememberLogin && lc) {
        // read registry key
        HKEY handle;
        if (RegCreateKeyEx(HKEY_CURRENT_USER, 
                "Software\\Kitserver", 0, NULL,
                REG_OPTION_NON_VOLATILE, KEY_READ|KEY_WRITE, 
                NULL, &handle, NULL) != ERROR_SUCCESS) {
            LOG(&k_roster,"ERROR: Unable to open/create registry key.");
            return;
        }
        DWORD lcSize = sizeof(LOGIN_CREDENTIALS);
        if (RegQueryValueEx(handle,_config.server.c_str(),
                NULL, NULL, (unsigned char *)lc, 
                &lcSize) == ERROR_SUCCESS) {
            // decode password
            for (int i=0; i<sizeof((*lc).password); i++) {
                lc->password[i] = lc->password[i] ^ _cred_key[i];
            }
        }
        RegCloseKey(handle);
    }
}

void loginWrite()
{
    LOGIN_CREDENTIALS* lc = (LOGIN_CREDENTIALS*)dta[CREDENTIALS];
    if (_config.rememberLogin && lc) {
        lc->initialized = 1;

        // open/create registry key
        HKEY handle;
        if (RegCreateKeyEx(HKEY_CURRENT_USER, 
                "Software\\Kitserver", 0, NULL,
                REG_OPTION_NON_VOLATILE, KEY_READ|KEY_WRITE, 
                NULL, &handle, NULL) != ERROR_SUCCESS) {
            LOG(&k_roster,"ERROR: Unable to open/create registry key.");
            LOG(&k_roster,"ERROR: Network login cannot be stored.");
            return;
        }
        // encode password for storing in registry
        int i;
        for (i=0; i<sizeof((*lc).password); i++) {
            lc->password[i] = lc->password[i] ^ _cred_key[i];
        }
        DWORD lcSize = sizeof(LOGIN_CREDENTIALS);
        if (RegSetValueEx(handle,_config.server.c_str(),
                NULL, REG_BINARY,
                (const unsigned char*)lc, lcSize) != ERROR_SUCCESS) {
            LOG(&k_roster,"ERROR: Unable to write 'login' value");
        }
        // decode password
        for (i=0; i<sizeof((*lc).password); i++) {
            lc->password[i] = lc->password[i] ^ _cred_key[i];
        }
        RegCloseKey(handle);
    }
}
*/

void copyPlayerDataCallPoint()
{
    __asm {
        pushfd 
        push ebp
        push eax
        push ebx
        push ecx
        push edx
        push esi
        push edi
        mov ecx, 0x25d78
        sub esi,ecx
        sub esi,ecx
        sub esi,ecx
        sub esi,ecx
        push ecx // count
        push esi // src
        call beforeCopyPlayerData
        add esp,8 // pop params
        pop edi
        pop esi
        pop edx
        pop ecx
        pop ebx
        pop eax
        pop ebp
        popfd
        mov ecx, 0x1a3  // original code
        retn
    }
}

void beforeCopyPlayerData(BYTE* dta, DWORD numDwords)
{
    // compute roster hash
    computeHash(dta, numDwords*4);
}

void afterComputeHashCallPoint()
{
    __asm {
        pushfd 
        push ebp
        push eax
        push ebx
        push ecx
        push edx
        push esi
        push edi
        mov eax, ss:[esp+0x24]
        push eax  // arg: address of hash
        call afterComputeHash
        add esp, 4 // pop parameter
        pop edi
        pop esi
        pop edx
        pop ecx
        pop ebx
        pop eax
        pop ebp
        popfd
        lea ecx, dword ptr ss:[esp+0x12c]  // adj. original code
        retn
    }
}

void afterComputeHash(BYTE* pHash)
{
    // overwrite with our hash
    memcpy(pHash, _versionString, 16);
}

