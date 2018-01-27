#ifndef _LOG_
#define _LOG_

#include "manage.h"

#define KEXPORT EXTERN_C __declspec(dllexport)

KEXPORT void TestFunc();

#ifndef MYDLL_RELEASE_BUILD
//if not release, also log to file, not just to memory
#define TRACE(i,x) Log(i,x)
#define TRACE2(i,x,y) LogWithNumber(i,x,y)
#define TRACE3(i,x,y) LogWithDouble(i,x,y)
#define TRACE4(i,x,y) LogWithString(i,x,y)
#define TRACE2X(i,x,a,b) LogWithTwoNumbers(i,x,a,b)
#define TRACEX(i,x,a,b,c) LogWithThreeNumbers(i,x,a,b,c)
#else
#define TRACE(i,x) Debug(i,x)
#define TRACE2(i,x,y) DebugWithNumber(i,x,y)
#define TRACE3(i,x,y) DebugWithDouble(i,x,y)
#define TRACE4(i,x,y) DebugWithString(i,x,y)
#define TRACE2X(i,x,a,b) DebugWithTwoNumbers(i,x,a,b)
#define TRACEX(i,x,a,b,c) DebugWithThreeNumbers(i,x,a,b,c)
#endif

#define LOG _Log
#define DEBUGLOG _DebugLog
#define DEEPDEBUGLOG _DeepDebugLog
#define TRACELOG _TraceLog

KEXPORT void OpenLog(char* logName);
KEXPORT void CloseLog();

KEXPORT void OpenMLog(DWORD size, char* filename);
KEXPORT void CloseMLog(char* filename);

KEXPORT void _Log(KMOD *caller, const char* format, ...);
KEXPORT void _DebugLog(KMOD *caller, const char* format, ...);
KEXPORT void _DeepDebugLog(KMOD *caller, const char* format, ...);
KEXPORT void _TraceLog(KMOD *caller, const char* format, ...);

KEXPORT void MasterLog(KMOD *caller,char* logfile, char* procfile, char* msg);
KEXPORT void Log(KMOD *caller,char* msg);
KEXPORT void LogWithNumber(KMOD *caller,char* msg, DWORD number);
KEXPORT void LogWithFloat(KMOD *caller,char* msg, float number);
KEXPORT void LogWithTwoNumbers(KMOD *caller,char* msg, DWORD a, DWORD b);
KEXPORT void LogWithThreeNumbers(KMOD *caller,char* msg, DWORD a, DWORD b, DWORD c);
KEXPORT void LogWithFourFloats(KMOD *caller,char* msg, float a, float b, float c, float d);
KEXPORT void LogWithDouble(KMOD *caller,char* msg, double number);
KEXPORT void LogWithString(KMOD *caller,char* msg, char* str);
KEXPORT void LogWithTwoStrings(KMOD *caller,char* msg, char* a, char* b);
KEXPORT void LogWithNumberAndString(KMOD *caller,char *msg, DWORD a, char* b);
KEXPORT void LogWithStringAndNumber(KMOD *caller,char *msg, char* a, DWORD b);

KEXPORT void Debug(KMOD *caller,char* msg);
KEXPORT void DebugWithNumber(KMOD *caller,char* msg, DWORD number);
//KEXPORT void DebugWithFloat(KMOD *caller,char* msg, float number);
KEXPORT void DebugWithDouble(KMOD *caller,char* msg, double number);
KEXPORT void DebugWithString(KMOD *caller,char* msg, char* str);
KEXPORT void DebugWithTwoNumbers(KMOD *caller,char* msg, DWORD a, DWORD b);
KEXPORT void DebugWithThreeNumbers(KMOD *caller,char* msg, DWORD a, DWORD b, DWORD c);
//KEXPORT void DebugWithFourFloats(KMOD *caller,char* msg, float a, float b, float c, float d);
KEXPORT void DebugWithTwoStrings(KMOD *caller,char* msg, char* a, char* b);

KEXPORT void MLog(KMOD *caller,char *msg);

typedef struct _MLOGTMP {
	DWORD ProcessId;
	char** Addr;
	DWORD Size;
	DWORD Max;
	DWORD* Act;
	bool* Full;
} MLOGTMP;

#endif
