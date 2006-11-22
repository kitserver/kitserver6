#ifndef _JUCE_LOG_
#define _JUCE_LOG_

#ifndef MYDLL_RELEASE_BUILD
#define TRACE(x) Debug(x)
#define TRACE2(x,y) DebugWithNumber(x,y)
#define TRACE3(x,y) DebugWithDouble(x,y)
#define TRACE4(x,y) DebugWithString(x,y)
#define TRACE2X(x,a,b) DebugWithTwoNumbers(x,a,b)
#define TRACEX(x,a,b,c) DebugWithThreeNumbers(x,a,b,c)
#else
#define TRACE(x) 
#define TRACE2(x,y) 
#define TRACE3(x,y) 
#define TRACE4(x,y) 
#define TRACE2X(x,a,b)
#define TRACEX(x,a,b,c)
#endif

void OpenLog(char* logName);
void CloseLog();

void MasterLog(char* logfile, char* procfile, char* msg);
void Log(char* msg);
void LogWithNumber(char* msg, DWORD number);
void LogWithTwoNumbers(char* msg, DWORD a, DWORD b);
void LogWithThreeNumbers(char* msg, DWORD a, DWORD b, DWORD c);
void LogWithFourNumbers(char* msg, DWORD a, DWORD b, DWORD c, DWORD d);
void LogWithDouble(char* msg, double number);
void LogWithString(char* msg, char* str);
void LogWithTwoStrings(char* msg, char* a, char* b);

void Debug(char* msg);
void DebugWithNumber(char* msg, DWORD number);
void DebugWithDouble(char* msg, double number);
void DebugWithString(char* msg, char* str);
void DebugWithTwoNumbers(char* msg, DWORD a, DWORD b);
void DebugWithThreeNumbers(char* msg, DWORD a, DWORD b, DWORD c);
void DebugWithFourNumbers(char* msg, DWORD a, DWORD b, DWORD c, DWORD d);
void DebugWithTwoStrings(char* msg, char* a, char* b);

#endif

