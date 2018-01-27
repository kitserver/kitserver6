// log.cpp
#include <windows.h>
#include <stdio.h>
#include <stdarg.h>
#include "log.h"

#define MLOG_SIZE 256
char **mlog=NULL;
DWORD maxMLog=200;
DWORD actMLog=0;
bool MLogFull=false;

#define BUFLEN 4096
static HANDLE mylog = INVALID_HANDLE_VALUE;

//////////////////////////////////////////////////
// FUNCTIONS
//////////////////////////////////////////////////


KEXPORT void TestFunc()
{
	MessageBox(0,"TestFunc","@kload.dll",0);
	return;
};

// Creates log file
KEXPORT void OpenLog(char* logName)
{
	//if (caller->debug > 0)
		mylog = CreateFile(logName,                    // file to create 
					 GENERIC_WRITE,                    // open for writing 
					 FILE_SHARE_READ | FILE_SHARE_DELETE,   // do not share 
					 NULL,                             // default security 
					 CREATE_ALWAYS,                    // overwrite existing 
					 FILE_ATTRIBUTE_NORMAL,            // normal file 
					 NULL);                            // no attr. template 
}

// Closes log file
KEXPORT void CloseLog()
{
	if (mylog != INVALID_HANDLE_VALUE) CloseHandle(mylog);
}

// Simple logger
KEXPORT void Log(KMOD *caller,char *msg)
{
	MLog(caller,msg);
	
	if (caller->debug < 1) return;
	DWORD wbytes;
	if (mylog != INVALID_HANDLE_VALUE) 
	{
		WriteFile(mylog,(LPVOID)"{",1,(LPDWORD)&wbytes,NULL);
		WriteFile(mylog,(LPVOID)(caller->NameShort),lstrlen(caller->NameShort),(LPDWORD)&wbytes, NULL);
		WriteFile(mylog,(LPVOID)"} ",2,(LPDWORD)&wbytes,NULL);
		WriteFile(mylog,(LPVOID)msg, lstrlen(msg),(LPDWORD)&wbytes,NULL);
		WriteFile(mylog,(LPVOID)"\r\n",2,(LPDWORD)&wbytes,NULL);
	}
}

// Simple logger 2
KEXPORT void LogWithNumber(KMOD *caller,char *msg, DWORD number)
{
	if (caller->debug < 1) return;
	char buf[BUFLEN];
	DWORD wbytes;
	if (mylog != INVALID_HANDLE_VALUE) 
	{
		ZeroMemory(buf, BUFLEN);
		sprintf(buf, msg, number);
		Log(caller,buf);
	}
}

// Simple logger 2
KEXPORT void LogWithFloat(KMOD *caller,char *msg, float number)
{
	if (caller->debug < 1) return;
	char buf[BUFLEN];
	DWORD wbytes;
	if (mylog != INVALID_HANDLE_VALUE) 
	{
		ZeroMemory(buf, BUFLEN);
		sprintf(buf, msg, number);
		Log(caller,buf);
	}
}

// Simple logger
KEXPORT void LogWithTwoNumbers(KMOD *caller,char *msg, DWORD a, DWORD b)
{
	if (caller->debug < 1) return;
	char buf[BUFLEN];
	DWORD wbytes;
	if (mylog != INVALID_HANDLE_VALUE) 
	{
		ZeroMemory(buf, BUFLEN);
		sprintf(buf, msg, a, b);
		Log(caller,buf);
	}
}

// Simple debugging logger
KEXPORT void LogWithThreeNumbers(KMOD *caller,char *msg, DWORD a, DWORD b, DWORD c)
{
	if (caller->debug < 1) return;
	char buf[BUFLEN];
	DWORD wbytes;
	if (mylog != INVALID_HANDLE_VALUE) 
	{
		ZeroMemory(buf, BUFLEN);
		sprintf(buf, msg, a, b, c);
		Log(caller,buf);
	}
}

/*
// Simple debugging logger
KEXPORT void LogWithFourFloats(KMOD *caller,char *msg, float a, float b, float c, float d)
{
	if (caller->debug < 1) return;
	char buf[BUFLEN];
	DWORD wbytes;
	if (mylog != INVALID_HANDLE_VALUE) 
	{
		ZeroMemory(buf, BUFLEN);
		sprintf(buf, msg, a, b, c, d);
		Log(caller,buf);
	}
}
*/

// Simple logger 3
KEXPORT void LogWithDouble(KMOD *caller,char *msg, double number)
{
	if (caller->debug < 1) return;
	char buf[BUFLEN];
	DWORD wbytes;
	if (mylog != INVALID_HANDLE_VALUE) 
	{
		ZeroMemory(buf, BUFLEN);
		sprintf(buf, msg, number);
		Log(caller,buf);
	}
}

// Simple logger 4
KEXPORT void LogWithString(KMOD *caller,char *msg, char* str)
{
	if (caller->debug < 1) return;
	char buf[BUFLEN];
	DWORD wbytes;
	if (mylog != INVALID_HANDLE_VALUE) 
	{
		ZeroMemory(buf, BUFLEN);
		sprintf(buf, msg, str);
		Log(caller,buf);
	}
}

// Simple logger 5
KEXPORT void LogWithTwoStrings(KMOD *caller,char *msg, char* a, char* b)
{
	if (caller->debug < 1) return;
	char buf[BUFLEN];
	DWORD wbytes;
	if (mylog != INVALID_HANDLE_VALUE) 
	{
		ZeroMemory(buf, BUFLEN);
		sprintf(buf, msg, a, b);
		Log(caller,buf);
	}
}

// Simple logger 6
KEXPORT void LogWithNumberAndString(KMOD *caller,char *msg, DWORD a, char* b)
{
	if (caller->debug < 1) return;
	char buf[BUFLEN];
	DWORD wbytes;
	if (mylog != INVALID_HANDLE_VALUE) 
	{
		ZeroMemory(buf, BUFLEN);
		sprintf(buf, msg, a, b);
		Log(caller,buf);
	}
}

// Simple logger 7
KEXPORT void LogWithStringAndNumber(KMOD *caller,char *msg, char* a, DWORD b)
{
	if (caller->debug < 1) return;
	char buf[BUFLEN];
	DWORD wbytes;
	if (mylog != INVALID_HANDLE_VALUE) 
	{
		ZeroMemory(buf, BUFLEN);
		sprintf(buf, msg, a, b);
		Log(caller,buf);
	}
}

// Simple debugging logger
KEXPORT void Debug(KMOD *caller,char *msg)
{
	MLog(caller,msg);
}

// Simple debugging logger 2
KEXPORT void DebugWithNumber(KMOD *caller,char *msg, DWORD number)
{
	char buf[BUFLEN];
	ZeroMemory(buf, BUFLEN);
	sprintf(buf, msg, number);
	MLog(caller,buf);
}

// Simple debugging logger 2
KEXPORT void DebugWithFloat(KMOD *caller,char *msg, float number)
{
	char buf[BUFLEN];
	ZeroMemory(buf, BUFLEN);
	sprintf(buf, msg, number);
	MLog(caller,buf);
}

// Simple debugging logger 3
KEXPORT void DebugWithDouble(KMOD *caller,char *msg, double number)
{
	char buf[BUFLEN];
	ZeroMemory(buf, BUFLEN);
	sprintf(buf, msg, number);
	MLog(caller,buf);
}

// Simple debugging logger 4
KEXPORT void DebugWithString(KMOD *caller,char *msg, char* str)
{
	char buf[BUFLEN];
	DWORD wbytes;
	ZeroMemory(buf, BUFLEN);
	sprintf(buf, msg, str);
	MLog(caller,buf);
}

// Simple debugging logger 5
KEXPORT void DebugWithTwoStrings(KMOD *caller,char *msg, char* a, char* b)
{
	char buf[BUFLEN];
	ZeroMemory(buf, BUFLEN);
	sprintf(buf, msg, a, b);
	MLog(caller,buf);
}

// Simple debugging logger
KEXPORT void DebugWithTwoNumbers(KMOD *caller,char *msg, DWORD a, DWORD b)
{
	char buf[BUFLEN];
	ZeroMemory(buf, BUFLEN);
	sprintf(buf, msg, a, b);
	MLog(caller,buf);
}

// Simple debugging logger
KEXPORT void DebugWithThreeNumbers(KMOD *caller,char *msg, DWORD a, DWORD b, DWORD c)
{
	char buf[BUFLEN];
	ZeroMemory(buf, BUFLEN);
	sprintf(buf, msg, a, b, c);
	MLog(caller,buf);
}

/*
// Simple debugging logger
KEXPORT void DebugWithFourFloats(KMOD *caller,char *msg, float a, float b, float c, float d)
{
	char buf[BUFLEN];
	ZeroMemory(buf, BUFLEN);
	sprintf(buf, msg, a, b, c, d);
	MLog(caller,buf);
}
*/

KEXPORT void OpenMLog(DWORD size, char* filename)
{
	DWORD NBW=0;
	
	maxMLog=size;
	if (size<1) return;
	if (mlog != NULL)
		CloseMLog(filename);
	mlog=new char*[size];
	for (int i=0;i<size;i++) {
		mlog[i]=new char[MLOG_SIZE];
		ZeroMemory(mlog[i],MLOG_SIZE);
	};
	actMLog=0;
	MLogFull=false;
	
	HANDLE mlog_tmp = CreateFile(filename,GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_DELETE, NULL,
					 CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
					 
	MLOGTMP tmp;
	tmp.ProcessId=GetCurrentProcessId();
	tmp.Addr=mlog;
	tmp.Size=MLOG_SIZE;
	tmp.Max=maxMLog;
	tmp.Act=&actMLog;
	tmp.Full=&MLogFull;
	WriteFile(mlog_tmp,&tmp,sizeof(MLOGTMP),&NBW,NULL);
	
	CloseHandle(mlog_tmp);
	return;
};

KEXPORT void CloseMLog(char* filename)
{
	if (mlog == NULL) return;
	
	for (int i=0;i<maxMLog;i++)
		delete mlog[i];
	delete mlog;
	mlog=NULL;
	actMLog=0;
	MLogFull=false;
	
	DeleteFile(filename);
	return;
};
	
KEXPORT void MLog(KMOD *caller,char *msg)
{
	if (maxMLog < 1 || mlog == NULL) return;
	
	char buf[BUFLEN];
	sprintf(buf,"{%s} %s\r\n",caller->NameShort,msg);
	
	strncpy(mlog[actMLog],buf,MLOG_SIZE-1);
	actMLog++;
	if (actMLog>=maxMLog) {
		actMLog=0;
		MLogFull=true;
	};
	
	return;
}

KEXPORT void _Log(KMOD *caller, const char *format, ...)
{
    if (mylog == INVALID_HANDLE_VALUE)
        return;

    char buffer[512];
    memset(buffer,0,sizeof(buffer));

    va_list params;
    va_start(params, format);
    _vsnprintf(buffer, 512, format, params);
    va_end(params);

    Log(caller,buffer);
}

KEXPORT void _TraceLog(KMOD *caller, const char *format, ...)
{
#ifdef MYDLL_RELEASE_BUILD
    return;
#endif
    
    if (mylog == INVALID_HANDLE_VALUE)
        return;

    char buffer[512];
    memset(buffer,0,sizeof(buffer));

    va_list params;
    va_start(params, format);
    _vsnprintf(buffer, 512, format, params);
    va_end(params);

    Log(caller,buffer);
}

KEXPORT void _DebugLog(KMOD *caller, const char *format, ...)
{
    if (!caller->debug)
        return;
    
    if (mylog == INVALID_HANDLE_VALUE)
        return;

    char buffer[512];
    memset(buffer,0,sizeof(buffer));

    va_list params;
    va_start(params, format);
    _vsnprintf(buffer, 512, format, params);
    va_end(params);

    Log(caller,buffer);
}

KEXPORT void _DeepDebugLog(KMOD *caller, const char *format, ...)
{
    if (caller->debug < 2)
        return;
    
    if (mylog == INVALID_HANDLE_VALUE)
        return;

    char buffer[512];
    memset(buffer,0,sizeof(buffer));

    va_list params;
    va_start(params, format);
    _vsnprintf(buffer, 512, format, params);
    va_end(params);

    Log(caller,buffer);
}
