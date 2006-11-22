#include "input.h"
#include "log.h"

extern KMOD k_kload;

static DWORD* _pInputTable = 0;
static DWORD _input_inputs[24];
static BYTE _input_saved_code[10];

void NewCleanInputTable()
{
    // copy input table to our structure
    memcpy(_input_inputs, (BYTE*)_pInputTable, sizeof(_input_inputs));

    // perform original game logic: clean the table
    memset((BYTE*)_pInputTable, 0, sizeof(_input_inputs));
}

void HookGameInput(DWORD hook_cs, DWORD inputTableAddr)
{
    BYTE* cptr = (BYTE*)hook_cs;
    DWORD protection = 0;
    DWORD newProtection = PAGE_EXECUTE_READWRITE;
    if (VirtualProtect(cptr, 32, newProtection, &protection))
    {
        memcpy(_input_saved_code, cptr, 10);
        cptr[0] = 0xe8;
        DWORD* pAddr = (DWORD*)(cptr + 1);
        *pAddr = (DWORD)NewCleanInputTable - (DWORD)(hook_cs + 5);
        cptr[5] = 0xc3;
        cptr[6] = 0x90;
        cptr[7] = 0x90;
        cptr[8] = 0x90;
        cptr[9] = 0x90;
    }
    _pInputTable = (DWORD*)inputTableAddr;
    Log(&k_kload, "CleanInputTable hooked.");
}

void UnhookGameInput(DWORD hook_cs)
{
    BYTE* cptr = (BYTE*)hook_cs;
    DWORD protection = 0;
    DWORD newProtection = PAGE_EXECUTE_READWRITE;
    if (VirtualProtect(cptr, 32, newProtection, &protection))
    {
        memcpy(cptr, _input_saved_code, 10);
    }
    Log(&k_kload, "CleanInputTable unhooked.");
}

KEXPORT DWORD* GetInputTable()
{
    return _input_inputs;
}

