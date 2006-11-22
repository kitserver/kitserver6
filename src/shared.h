#ifndef _JUCE_SHAREDMEM
#define _JUCE_SHAREDMEM

#if _MSC_VER > 1000
	#pragma once
#endif

#define LIBSPEC __declspec(dllexport)

EXTERN_C LIBSPEC void  SetDebug(DWORD);
EXTERN_C LIBSPEC DWORD GetDebug(void);
EXTERN_C LIBSPEC void  SetKey(int, int);
EXTERN_C LIBSPEC int   GetKey(int);
EXTERN_C LIBSPEC void  SetKdbDir(char*);
EXTERN_C LIBSPEC void  GetKdbDir(char*);

#endif /* _JUCE_SHAREDMEM */

