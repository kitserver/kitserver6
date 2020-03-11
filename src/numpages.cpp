/* numpages.cpp */

#include "numpages.h"
#include "manage.h"
#include "log.h"
#include "kload.h"
#include <vector>

using namespace std;

extern KMOD k_kload;
extern PESINFO g_pesinfo;
//extern KLOAD_CONFIG g_config;

/////////////////////////////////////////////////////////////

#define CODELEN 6
enum {
    C_GETNUMPAGES_HOOK, C_GETNUMPAGES_JMPBACK, GETNUMPAGES_CMDLEN,
    C_GETNUMPAGES2_HOOK, C_GETNUMPAGES2_JMPBACK, GETNUMPAGES2_CMDLEN,
};
static DWORD codeArray[][CODELEN] = {
    // PES6
    {
        0x661b24, 0x661b2b, 7,
        0x661b4f, 0x661b57, 8,
    },
    // PES6 1.10
    {
        0x460fdb9, 0x460fdc0, 7,
        0x460fde4, 0x460fdec, 8,
    },
    // WE2007
    {
        0x71, 0x78, 7,
        0x9c, 0xa4, 8,
    },
};

#define DATALEN 2
enum {
    AFS_NUMPAGES_TABLES, DECRYPTED_CODE_ADDR,
};
static DWORD dataArray[][DATALEN] = {
    // PES6
    {
        0x3b5cbc0, 0,
    },
    // PES6 1.10
    {
        0x3b5dbc0, 0,
    },
    // WE2007
    {
        0x3b57640, 0x44adc18,
    },
};

static DWORD code[CODELEN];
static DWORD data[DATALEN];

typedef DWORD (*getnumpages_callback_t)(DWORD fileId, DWORD afsId, DWORD* retval);
vector<getnumpages_callback_t> _getnumpages_vec;
DWORD _getnumpages_jmpback = 0;
DWORD _getnumpages2_jmpback = 0;

CRITICAL_SECTION _gnp_cs;
BYTE _saved_code[10];
BYTE _saved_code2[10];

/////////////////////////////////////////////////////////////

/**
 * Manager of GetNumPages callbacks.
 * It may call a sequence of registered callbacks, but
 * the return value must be returned by only one of them. 
 * If more than one returns a value, then
 * the first one in the chain is used.
 */
DWORD GetNumPagesForFileInAFS(DWORD fileId, DWORD afsId)
{
    DWORD* numPagesTables = (DWORD*)data[AFS_NUMPAGES_TABLES];
    DWORD* numPagesTable = (DWORD*)(numPagesTables[afsId] + 0x11c);
    DWORD orgNumPages = numPagesTable[fileId];

    DWORD numPages = 0;
    for (vector<getnumpages_callback_t>::iterator it = _getnumpages_vec.begin(); it != _getnumpages_vec.end(); it++) {
        bool processed = (*it)(fileId, afsId, &numPages);
        //LogWithNumber(&k_kload, "processed = %d", processed);
        //LogWithNumber(&k_kload, "numPages = %08x", numPages);
    }

    return max(orgNumPages, numPages);
}

/**
 * Manager of GetNumPages callbacks.
 * It may call a sequence of registered callbacks, but
 * the return value must be returned by only one of them. 
 * If more than one returns a value, then
 * the first one in the chain is used.
 */
DWORD GetNumPagesForFileInAFS2(DWORD fileId, DWORD afsId)
{
    DWORD* numPagesTables = (DWORD*)data[AFS_NUMPAGES_TABLES];
    WORD* numPagesTable = (WORD*)(numPagesTables[afsId] + 0x11a);
    DWORD orgNumPages = (DWORD)numPagesTable[fileId];

    DWORD numPages = 0;
    for (vector<getnumpages_callback_t>::iterator it = _getnumpages_vec.begin(); it != _getnumpages_vec.end(); it++) {
        bool processed = (*it)(fileId, afsId, &numPages);
        //LogWithNumber(&k_kload, "processed = %d", processed);
        //LogWithNumber(&k_kload, "numPages = %08x", numPages);
    }

    return max(orgNumPages, numPages);
}

void GetNumPagesForFileInAFSCaller()
{
    __asm {
        push ecx
        push ebx
        mov ecx, [esp+0x1c]
        mov ebx, [esp+0x20]
        push ecx
        push ebx
        call GetNumPagesForFileInAFS
        pop ebx
        pop ecx
        pop ebx
        pop ecx
        push _getnumpages_jmpback
        retn
    }
}

void GetNumPagesForFileInAFSCaller2()
{
    __asm {
        push ecx
        push ebx
        mov ecx, [esp+0x1c]
        mov ebx, [esp+0x20]
        push ecx
        push ebx
        call GetNumPagesForFileInAFS2
        pop ebx
        pop ecx
        pop ebx
        pop ecx
        push _getnumpages2_jmpback
        retn
    }
}

KEXPORT void InitGetNumPages()
{
    InitializeCriticalSection(&_gnp_cs);
    _getnumpages_vec.clear();

    // Determine the game version
    int v = GetPESInfo()->GameVersion;
    if (v != -1)
    {
        memcpy(code, codeArray[v], sizeof(code));
        memcpy(data, dataArray[v], sizeof(data));
    }
}

KEXPORT bool HookGetNumPages()
{
    if (_getnumpages_vec.size() == 0) {
        Log(&k_kload, "HookGetNumPages: no callbacks registered.");
        Log(&k_kload, "GetNumPagesForFileInAFS command not hooked.");
        Log(&k_kload, "GetNumPagesForFileInAFS2 command not hooked.");
    }

    if (data[DECRYPTED_CODE_ADDR]!=0) {
        if ((*(DWORD*)data[DECRYPTED_CODE_ADDR] & 0xffff)!=0) {
            return false; // delayed hooking
        }
    }

    _getnumpages_jmpback = code[C_GETNUMPAGES_JMPBACK];
    if (data[DECRYPTED_CODE_ADDR]!=0) _getnumpages_jmpback += *(DWORD*)data[DECRYPTED_CODE_ADDR];
    _getnumpages2_jmpback = code[C_GETNUMPAGES2_JMPBACK];
    if (data[DECRYPTED_CODE_ADDR]!=0) _getnumpages2_jmpback += *(DWORD*)data[DECRYPTED_CODE_ADDR];

    DWORD addr = code[C_GETNUMPAGES_HOOK];
    if (data[DECRYPTED_CODE_ADDR]!=0) addr += *(DWORD*)data[DECRYPTED_CODE_ADDR];
    DWORD target = (DWORD)GetNumPagesForFileInAFSCaller + KS_JMP_SHIFT; //6;

    DWORD protection = 0;
    DWORD newProtection = PAGE_EXECUTE_READWRITE;
    if (VirtualProtect((void*)addr, 16, newProtection, &protection))
    {
        BYTE* bptr = (BYTE*)addr;

        // save existing code
        memcpy(_saved_code, bptr, code[GETNUMPAGES_CMDLEN]);

        // hook by setting new target
        memset(bptr,0x90,code[GETNUMPAGES_CMDLEN]);
        bptr[0] = 0xe9;
        DWORD* ptr = (DWORD*)(addr + 1);
        ptr[0] = target - (DWORD)(addr + 5);

        Log(&k_kload, "GetNumPagesForFileInAFS command hooked");
    }

    addr = code[C_GETNUMPAGES2_HOOK];
    if (data[DECRYPTED_CODE_ADDR]!=0) addr += *(DWORD*)data[DECRYPTED_CODE_ADDR];
    target = (DWORD)GetNumPagesForFileInAFSCaller2 + KS_JMP_SHIFT; //6;

    protection = 0;
    newProtection = PAGE_EXECUTE_READWRITE;
    if (VirtualProtect((void*)addr, 16, newProtection, &protection))
    {
        BYTE* bptr = (BYTE*)addr;

        // save existing code
        memcpy(_saved_code2, bptr, code[GETNUMPAGES2_CMDLEN]);

        // hook by setting new target
        memset(bptr,0x90,code[GETNUMPAGES2_CMDLEN]);
        bptr[0] = 0xe9;
        DWORD* ptr = (DWORD*)(addr + 1);
        ptr[0] = target - (DWORD)(addr + 5);

        Log(&k_kload, "GetNumPagesForFileInAFS2 command hooked");
    }

    return true;
}

KEXPORT void RegisterGetNumPagesCallback(void* callback)
{
    EnterCriticalSection(&_gnp_cs);
    _getnumpages_vec.push_back((getnumpages_callback_t)callback);
    LogWithNumber(&k_kload, "RegisterGetNumPagesCallback(%08x)", (DWORD)callback);
    LeaveCriticalSection(&_gnp_cs);
}

KEXPORT void UnhookGetNumPages()
{
    if (_getnumpages_vec.size() == 0) {
        Log(&k_kload, "GetNumPagesForFileInAFS was not hooked.");
        Log(&k_kload, "GetNumPagesForFileInAFS2 was not hooked.");
        return;
    }

    DWORD addr = code[C_GETNUMPAGES_HOOK];
    if (data[DECRYPTED_CODE_ADDR]!=0) addr += *(DWORD*)data[DECRYPTED_CODE_ADDR];
    DWORD protection = 0;
    DWORD newProtection = PAGE_EXECUTE_READWRITE;
    if (VirtualProtect((void*)addr, 16, newProtection, &protection))
    {
        BYTE* bptr = (BYTE*)addr;

        // unhook
        memcpy(bptr,_saved_code,code[GETNUMPAGES_CMDLEN]);
        Log(&k_kload, "GetNumPagesForFileInAFS command unhooked");
    }

    addr = code[C_GETNUMPAGES2_HOOK];
    if (data[DECRYPTED_CODE_ADDR]!=0) addr += *(DWORD*)data[DECRYPTED_CODE_ADDR];
    protection = 0;
    newProtection = PAGE_EXECUTE_READWRITE;
    if (VirtualProtect((void*)addr, 16, newProtection, &protection))
    {
        BYTE* bptr = (BYTE*)addr;

        // unhook
        memcpy(bptr,_saved_code2,code[GETNUMPAGES2_CMDLEN]);
        Log(&k_kload, "GetNumPagesForFileInAFS2 command unhooked");
    }

    DeleteCriticalSection(&_gnp_cs);
}

