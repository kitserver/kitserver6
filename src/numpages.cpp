/* numpages.cpp */

#include "numpages.h"
#include "manage.h"
#include "log.h"
#include <vector>

using namespace std;

extern KMOD k_kload;
extern PESINFO g_pesinfo;
//extern KLOAD_CONFIG g_config;

/////////////////////////////////////////////////////////////

enum {
    C_GETNUMPAGES_HOOK, C_GETNUMPAGES_JMPBACK, GETNUMPAGES_CMDLEN,
    C_GETNUMPAGES2_HOOK, C_GETNUMPAGES2_JMPBACK, GETNUMPAGES2_CMDLEN,
};
static DWORD code[] = {
    0x661b24, 0x661b2b, 7,
    0x661b4f, 0x661b57, 8,
};

enum {
    AFS_NUMPAGES_TABLES
};
static DWORD data[] = {
    0x3b5cbc0,
};

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
}

KEXPORT void HookGetNumPages()
{
    if (_getnumpages_vec.size() == 0) {
        Log(&k_kload, "HookGetNumPages: no callbacks registered.");
        Log(&k_kload, "GetNumPagesForFileInAFS command not hooked.");
        Log(&k_kload, "GetNumPagesForFileInAFS2 command not hooked.");
        return;
    }

    _getnumpages_jmpback = code[C_GETNUMPAGES_JMPBACK];
    _getnumpages2_jmpback = code[C_GETNUMPAGES2_JMPBACK];

    DWORD addr = code[C_GETNUMPAGES_HOOK];
    DWORD target = (DWORD)GetNumPagesForFileInAFSCaller + 6;

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
    target = (DWORD)GetNumPagesForFileInAFSCaller2 + 6;

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

