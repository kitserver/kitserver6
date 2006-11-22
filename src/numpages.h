#ifndef _NUMPAGES_H
#define _NUMPAGES_H

#include <windows.h>

#ifndef KEXPORT
#define KEXPORT EXTERN_C __declspec(dllexport)
#endif

KEXPORT void InitGetNumPages();
KEXPORT void HookGetNumPages();
KEXPORT void RegisterGetNumPagesCallback(void* callback);
KEXPORT void UnhookGetNumPages();

#endif
