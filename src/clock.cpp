// clock.cpp
#include <windows.h>
#include <stdio.h>
#include "clock.h"
#include "kload_exp.h"

KMOD k_clock={MODID,NAMELONG,NAMESHORT,DEFAULT_DEBUG};

HINSTANCE hInst;

BYTE patch_data_2[] = {
    0x56,                            // 78e8d3: push esi
    0x0f,0xb6,0xf0,                  //         movzx esi,al
    0x81,0xc6,0x6a,0xe9,0x78,0,      //         add esi,78e96a
    0xeb,0x22,                       //         jmp short 78e901
    0x0f,0xb6,0x36,                  // 78e901: movzx esi,byte ptr ds:[esi]
    0x66,0x03,0xce,                  //         add cx,si
    0x5e,                            //         pop esi
    0xe9,0x88,0xfb,0xff,0xff,        //         jmp 78e495
    0,0x2d,0,0x0f,0,0,               // 78e96a: 0,45,0,15,0,0
};

BYTE patch_data_1[] = {
    0xe9,0xc1,0,0,0,                 // 78e48f: jmp 78e555
    0x90,                            //         nop
    0x66,0x0f,0xb6,0x4c,0x24,0x0a,   // 78e555: movzx cx,byte ptr ss:[esp+0x0a]
    0xeb,0x08,                       //         jmp short 78e565
    0xe9,0x69,0x03,0,0,              // 78e565: jmp 78e8d3 
};

typedef struct _patch_t {
    DWORD _addr;
    BYTE* _data;
    int _length;
} patch_t;

patch_t patch_parts[] = {
    {0x78e48f,patch_data_1+0,6},
    {0x78e555,patch_data_1+6,8},
    {0x78e565,patch_data_1+14,5},
    {0x78e8d3,patch_data_2+0,12},
    {0x78e901,patch_data_2+12,12},
    {0x78e96a,patch_data_2+24,6},
};

EXTERN_C BOOL WINAPI DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID lpReserved);
bool apply_patch(patch_t& part);
void InitClock();

EXTERN_C BOOL WINAPI DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID lpReserved)
{
	int i,j;
	
	if (dwReason == DLL_PROCESS_ATTACH)
	{
		Log(&k_clock,"Attaching dll...");
		
		hInst=hInstance;
		
		int v=GetPESInfo()->GameVersion;
		switch (v) {
			//case gvPES6PC:
			//case gvPES6PC110:
			case gvWE2007PC:
				goto GameVersIsOK;
				break;
		};
		//Will land here if game version is not supported
		Log(&k_clock,"Your game version is currently not supported!");
		return false;
		
		//Everything is OK!
		GameVersIsOK:

		RegisterKModule(&k_clock);
		
		HookFunction(hk_D3D_Create,(DWORD)InitClock);
	}
	else if (dwReason == DLL_PROCESS_DETACH)
	{
		Log(&k_clock,"Detaching dll...");
		Log(&k_clock,"Detaching done.");
	}
	
	return true;
}

void InitClock()
{
	Log(&k_clock,"Init clock...");
    bool success = true;
    for (int i=0; i<sizeof(patch_parts)/sizeof(patch_t); i++)
    {
        success &= apply_patch(patch_parts[i]);
    }
    if (success) Log(&k_clock, "Clock patch successfully applied.");
    else Log(&k_clock, "PROBLEM: Unable to apply clock patch.");
}

bool apply_patch(patch_t& part)
{
    DWORD protection = 0;
    DWORD newProtection = PAGE_EXECUTE_READWRITE;
    if (VirtualProtect((BYTE*)part._addr, part._length, newProtection, &protection))
    {
        memcpy((BYTE*)part._addr, part._data, part._length);
        return true;
    }
    return false;
}

