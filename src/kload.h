// kload.h

#include "SGF.h"
#include "hooklib.h"
#include "keycfg.h"

#define KEXPORT EXTERN_C __declspec(dllexport)

KEXPORT PESINFO* GetPESInfo();
KEXPORT DWORD SetDebugLevel(DWORD ModDebugLevel);
KEXPORT void RegisterKModule(KMOD *k);

KEXPORT void SetSavegameExpFuncs(BYTE module, BYTE type, DWORD addr);
KEXPORT SAVEGAMEEXPFUNCS* GetSavegameExpFuncs(DWORD* num);
KEXPORT void SetSavegameFuncs(SAVEGAMEFUNCS* sgf);
KEXPORT SAVEGAMEFUNCS* GetSavegameFuncs();

typedef struct _THREEDWORDS {
	DWORD dw1;
	DWORD dw2;
	DWORD dw3;
} THREEDWORDS;

//these identifiers will be saved in dw1
enum {
	AFSSIG_FACE, AFSSIG_HAIR, AFSSIG_FACEDATA, AFSSIG_HAIRDATA,
};

KEXPORT THREEDWORDS* GetSpecialAfsFileInfo(DWORD id);
KEXPORT DWORD GetNextSpecialAfsFileId(DWORD dw1, DWORD dw2, DWORD dw3);

KEXPORT bool MasterHookFunction(DWORD call_site, DWORD numArgs, void* target);
KEXPORT bool MasterUnhookFunction(DWORD call_site, void* target);
DWORD MasterCallFirst(...);
KEXPORT DWORD MasterCallNext(...);

