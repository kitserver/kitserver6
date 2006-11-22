// alog.cpp --> for applications
#include <windows.h>
#include <stdio.h>

#include "alog.h"
#include "aconfig.h"

extern KSERV_CONFIG g_config;
static HANDLE mylog = INVALID_HANDLE_VALUE;

//////////////////////////////////////////////////
// FUNCTIONS
//////////////////////////////////////////////////

// Creates log file
void OpenLog(char* logName)
{
	if (g_config.debug > 0)
		mylog = CreateFile(logName,                    // file to create 
					 GENERIC_WRITE,                    // open for writing 
					 FILE_SHARE_READ | FILE_SHARE_DELETE,   // do not share 
					 NULL,                             // default security 
					 CREATE_ALWAYS,                    // overwrite existing 
					 FILE_ATTRIBUTE_NORMAL,            // normal file 
					 NULL);                            // no attr. template 
}

// Closes log file
void CloseLog()
{
	if (mylog != INVALID_HANDLE_VALUE) CloseHandle(mylog);
}

// Simple logger
void Log(char *msg)
{
	if (g_config.debug < 1) return;
	DWORD wbytes;
	if (mylog != INVALID_HANDLE_VALUE) 
	{
		WriteFile(mylog, (LPVOID)msg, lstrlen(msg), (LPDWORD)&wbytes, NULL);
		WriteFile(mylog, (LPVOID)"\r\n", 2, (LPDWORD)&wbytes, NULL);
	}
}

// Simple logger 2
void LogWithNumber(char *msg, DWORD number)
{
	if (g_config.debug < 1) return;
	char buf[BUFLEN];
	DWORD wbytes;
	if (mylog != INVALID_HANDLE_VALUE) 
	{
		ZeroMemory(buf, BUFLEN);
		sprintf(buf, msg, number);
		WriteFile(mylog, (LPVOID)buf, lstrlen(buf), (LPDWORD)&wbytes, NULL);
		WriteFile(mylog, (LPVOID)"\r\n", 2, (LPDWORD)&wbytes, NULL);
	}
}

// Simple logger
void LogWithTwoNumbers(char *msg, DWORD a, DWORD b)
{
	if (g_config.debug < 1) return;
	char buf[BUFLEN];
	DWORD wbytes;
	if (mylog != INVALID_HANDLE_VALUE) 
	{
		ZeroMemory(buf, BUFLEN);
		sprintf(buf, msg, a, b);
		WriteFile(mylog, (LPVOID)buf, lstrlen(buf), (LPDWORD)&wbytes, NULL);
		WriteFile(mylog, (LPVOID)"\r\n", 2, (LPDWORD)&wbytes, NULL);
	}
}

// Simple debugging logger
void LogWithThreeNumbers(char *msg, DWORD a, DWORD b, DWORD c)
{
	if (g_config.debug < 1) return;
	char buf[BUFLEN];
	DWORD wbytes;
	if (mylog != INVALID_HANDLE_VALUE) 
	{
		ZeroMemory(buf, BUFLEN);
		sprintf(buf, msg, a, b, c);
		WriteFile(mylog, (LPVOID)buf, lstrlen(buf), (LPDWORD)&wbytes, NULL);
		WriteFile(mylog, (LPVOID)"\r\n", 2, (LPDWORD)&wbytes, NULL);
	}
}

// Simple debugging logger
void LogWithFourNumbers(char *msg, DWORD a, DWORD b, DWORD c, DWORD d)
{
	if (g_config.debug < 1) return;
	char buf[BUFLEN];
	DWORD wbytes;
	if (mylog != INVALID_HANDLE_VALUE) 
	{
		ZeroMemory(buf, BUFLEN);
		sprintf(buf, msg, a, b, c, d);
		WriteFile(mylog, (LPVOID)buf, lstrlen(buf), (LPDWORD)&wbytes, NULL);
		WriteFile(mylog, (LPVOID)"\r\n", 2, (LPDWORD)&wbytes, NULL);
	}
}

// Simple logger 3
void LogWithDouble(char *msg, double number)
{
	if (g_config.debug < 1) return;
	char buf[BUFLEN];
	DWORD wbytes;
	if (mylog != INVALID_HANDLE_VALUE) 
	{
		ZeroMemory(buf, BUFLEN);
		sprintf(buf, msg, number);
		WriteFile(mylog, (LPVOID)buf, lstrlen(buf), (LPDWORD)&wbytes, NULL);
		WriteFile(mylog, (LPVOID)"\r\n", 2, (LPDWORD)&wbytes, NULL);
	}
}

// Simple logger 4
void LogWithString(char *msg, char* str)
{
	if (g_config.debug < 1) return;
	char buf[BUFLEN];
	DWORD wbytes;
	if (mylog != INVALID_HANDLE_VALUE) 
	{
		ZeroMemory(buf, BUFLEN);
		sprintf(buf, msg, str);
		WriteFile(mylog, (LPVOID)buf, lstrlen(buf), (LPDWORD)&wbytes, NULL);
		WriteFile(mylog, (LPVOID)"\r\n", 2, (LPDWORD)&wbytes, NULL);
	}
}

// Simple logger 5
void LogWithTwoStrings(char *msg, char* a, char* b)
{
	if (g_config.debug < 1) return;
	char buf[BUFLEN];
	DWORD wbytes;
	if (mylog != INVALID_HANDLE_VALUE) 
	{
		ZeroMemory(buf, BUFLEN);
		sprintf(buf, msg, a, b);
		WriteFile(mylog, (LPVOID)buf, lstrlen(buf), (LPDWORD)&wbytes, NULL);
		WriteFile(mylog, (LPVOID)"\r\n", 2, (LPDWORD)&wbytes, NULL);
	}
}

// Simple debugging logger
void Debug(char *msg)
{
	if (g_config.debug < 2) return;
	DWORD wbytes;
	if (mylog != INVALID_HANDLE_VALUE) 
	{
		WriteFile(mylog, (LPVOID)msg, lstrlen(msg), (LPDWORD)&wbytes, NULL);
		WriteFile(mylog, (LPVOID)"\r\n", 2, (LPDWORD)&wbytes, NULL);
	}
}

// Simple debugging logger 2
void DebugWithNumber(char *msg, DWORD number)
{
	if (g_config.debug < 2) return;
	char buf[BUFLEN];
	DWORD wbytes;
	if (mylog != INVALID_HANDLE_VALUE) 
	{
		ZeroMemory(buf, BUFLEN);
		sprintf(buf, msg, number);
		WriteFile(mylog, (LPVOID)buf, lstrlen(buf), (LPDWORD)&wbytes, NULL);
		WriteFile(mylog, (LPVOID)"\r\n", 2, (LPDWORD)&wbytes, NULL);
	}
}

// Simple debugging logger 3
void DebugWithDouble(char *msg, double number)
{
	if (g_config.debug < 2) return;
	char buf[BUFLEN];
	DWORD wbytes;
	if (mylog != INVALID_HANDLE_VALUE) 
	{
		ZeroMemory(buf, BUFLEN);
		sprintf(buf, msg, number);
		WriteFile(mylog, (LPVOID)buf, lstrlen(buf), (LPDWORD)&wbytes, NULL);
		WriteFile(mylog, (LPVOID)"\r\n", 2, (LPDWORD)&wbytes, NULL);
	}
}

// Simple debugging logger 4
void DebugWithString(char *msg, char* str)
{
	if (g_config.debug < 2) return;
	char buf[BUFLEN];
	DWORD wbytes;
	if (mylog != INVALID_HANDLE_VALUE) 
	{
		ZeroMemory(buf, BUFLEN);
		sprintf(buf, msg, str);
		WriteFile(mylog, (LPVOID)buf, lstrlen(buf), (LPDWORD)&wbytes, NULL);
		WriteFile(mylog, (LPVOID)"\r\n", 2, (LPDWORD)&wbytes, NULL);
	}
}

// Simple debugging logger 5
void DebugWithTwoStrings(char *msg, char* a, char* b)
{
	if (g_config.debug < 2) return;
	char buf[BUFLEN];
	DWORD wbytes;
	if (mylog != INVALID_HANDLE_VALUE) 
	{
		ZeroMemory(buf, BUFLEN);
		sprintf(buf, msg, a, b);
		WriteFile(mylog, (LPVOID)buf, lstrlen(buf), (LPDWORD)&wbytes, NULL);
		WriteFile(mylog, (LPVOID)"\r\n", 2, (LPDWORD)&wbytes, NULL);
	}
}

// Simple debugging logger
void DebugWithTwoNumbers(char *msg, DWORD a, DWORD b)
{
	if (g_config.debug < 2) return;
	char buf[BUFLEN];
	DWORD wbytes;
	if (mylog != INVALID_HANDLE_VALUE) 
	{
		ZeroMemory(buf, BUFLEN);
		sprintf(buf, msg, a, b);
		WriteFile(mylog, (LPVOID)buf, lstrlen(buf), (LPDWORD)&wbytes, NULL);
		WriteFile(mylog, (LPVOID)"\r\n", 2, (LPDWORD)&wbytes, NULL);
	}
}

// Simple debugging logger
void DebugWithThreeNumbers(char *msg, DWORD a, DWORD b, DWORD c)
{
	if (g_config.debug < 2) return;
	char buf[BUFLEN];
	DWORD wbytes;
	if (mylog != INVALID_HANDLE_VALUE) 
	{
		ZeroMemory(buf, BUFLEN);
		sprintf(buf, msg, a, b, c);
		WriteFile(mylog, (LPVOID)buf, lstrlen(buf), (LPDWORD)&wbytes, NULL);
		WriteFile(mylog, (LPVOID)"\r\n", 2, (LPDWORD)&wbytes, NULL);
	}
}

// Simple debugging logger
void DebugWithFourNumbers(char *msg, DWORD a, DWORD b, DWORD c, DWORD d)
{
	if (g_config.debug < 2) return;
	char buf[BUFLEN];
	DWORD wbytes;
	if (mylog != INVALID_HANDLE_VALUE) 
	{
		ZeroMemory(buf, BUFLEN);
		sprintf(buf, msg, a, b, c, d);
		WriteFile(mylog, (LPVOID)buf, lstrlen(buf), (LPDWORD)&wbytes, NULL);
		WriteFile(mylog, (LPVOID)"\r\n", 2, (LPDWORD)&wbytes, NULL);
	}
}

