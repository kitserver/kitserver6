// kload_config.cpp

#include <stdio.h>
#include <string.h>

#include "kload_config.h"
#include "log.h"
#include "manage.h"

extern KMOD k_kload;
extern PESINFO g_pesinfo;

/**
 * Returns true if successful.
 */
BOOL ReadConfig(KLOAD_CONFIG* config, char* cfgFile)
{
	if (config == NULL) return false;

	bool hasGdbDir = false;
	FILE* cfg = fopen(cfgFile, "rt");
	if (cfg == NULL) return false;

	char str[BUFLEN];
	char name[BUFLEN];
	int value = 0;
	float dvalue = 0.0f;
	bool DebugSet=false;

	char *pName = NULL, *pValue = NULL, *comment = NULL;
	while (true)
	{
		ZeroMemory(str, BUFLEN);
		fgets(str, BUFLEN-1, cfg);
		if (feof(cfg)) break;

		// skip comments
		comment = strstr(str, "#");
		if (comment != NULL) comment[0] = '\0';

		// parse the line
		pName = pValue = NULL;
		ZeroMemory(name, BUFLEN); value = 0;
		char* eq = strstr(str, "=");
		if (eq == NULL || eq[1] == '\0') continue;

		eq[0] = '\0';
		pName = str; pValue = eq + 1;

		ZeroMemory(name, NULL); 
		sscanf(pName, "%s", name);

		if (lstrcmp(name, "debug")==0)
		{
			if (sscanf(pValue, "%d", &value)!=1) continue;
			k_kload.debug = value;
			DebugSet=true;
			LogWithNumber(&k_kload,"ReadConfig: debug = (%d)", value);
		}
		else if (lstrcmp(name, "gdb.dir")==0)
		{
			char* startQuote = strstr(pValue, "\"");
			if (startQuote == NULL) continue;
			char* endQuote = strstr(startQuote + 1, "\"");
			if (endQuote == NULL) continue;

			char buf[BUFLEN];
			ZeroMemory(buf, BUFLEN);
			memcpy(buf, startQuote + 1, endQuote - startQuote - 1);

			LogWithString(&k_kload,"ReadConfig: gdbDir = \"%s\"", buf);
			
			g_pesinfo.gdbDir=new char[strlen(buf)+1];
			
			strcpy(g_pesinfo.gdbDir, buf);

			hasGdbDir = true;
		}
		else if (stricmp(name, "dx.force-SW-TnL")==0)
		{
			if (sscanf(pValue, "%d", &value)!=1) continue;
			LogWithNumber(&k_kload,"ReadConfig: dx.force-SW-TnL = (%d)", value);
			config->forceSW_TnL = (value > 0);
		}
		else if (stricmp(name, "dx.emulate-HW-TnL")==0)
		{
			if (sscanf(pValue, "%d", &value)!=1) continue;
			LogWithNumber(&k_kload,"ReadConfig: dx.emulate-HW-TnL = (%d)", value);
			config->emulateHW_TnL = (value > 0);
		}
		else if (stricmp(name, "ReservedMemory")==0)
		{
			if (sscanf(pValue, "%d", &value)!=1) continue;
			LogWithNumber(&k_kload,"ReadConfig: ReservedMemory = (%d)", value);
			config->newResMem = value;
		}
		else if (stricmp(name, "font-size.factor")==0)
		{
			float fvalue;
			if (sscanf(pValue, "%f", &fvalue)!=1) continue;
			LogWithNumber(&k_kload,"ReadConfig: font-size.factor = (%0.3f)", fvalue);
			config->fontSizeFactor = fvalue;
		}
		else if (lstrcmp(name, "DLL.num")==0)
		{
			if (sscanf(pValue, "%d", &value)!=1) continue;
			LogWithNumber(&k_kload,"ReadConfig: DLL.num = (%d)", value);
            int i;
			for (i=0;i< config->numDLLs ;i++) delete config->dllnames[i];
			delete config->dllnames;
			config->numDLLs=value;
			config->dllnames=new LPTSTR[value];
			for (i=0;i< config->numDLLs ;i++) config->dllnames[i]=NULL;
		}
		else if (strncmp(name, "DLL.",4)==0)
		{
			if (sscanf(name, "DLL.%d",&value)!=1) continue;
			if (value>=config->numDLLs) continue;
			
			char* startQuote = strstr(pValue, "\"");
			if (startQuote == NULL) continue;
			char* endQuote = strstr(startQuote + 1, "\"");
			if (endQuote == NULL) continue;
			
			char buf[BUFLEN];
			ZeroMemory(buf, BUFLEN);
			memcpy(buf, startQuote + 1, endQuote - startQuote - 1);
			
			LogWithNumberAndString(&k_kload,"ReadConfig: new dll (id=%d) is named \"%s\"",value,buf);
			
			config->dllnames[value]=new char[strlen(buf)+1];
			strcpy(config->dllnames[value],buf);
		}
	}
	fclose(cfg);

	// if GDB directory is not specified, assume default
	if (!hasGdbDir)
	{
		g_pesinfo.gdbDir=new char[strlen(DEFAULT_GDB_DIR)+1];
		strcpy(g_pesinfo.gdbDir, DEFAULT_GDB_DIR);
	}

	if (!DebugSet)
		k_kload.debug=DEFAULT_DEBUG;
	
	return true;
}

/**
 * Returns true if successful.
 */
BOOL WriteConfig(KLOAD_CONFIG* config, char* cfgFile)
{
	char* buf = NULL;
	DWORD size = 0;

	// first read all lines
	HANDLE oldCfg = CreateFile(cfgFile, GENERIC_READ, 0, NULL,
			OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (oldCfg != INVALID_HANDLE_VALUE)
	{
		size = GetFileSize(oldCfg, NULL);
		buf = (char*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, size);
		if (buf == NULL) return false;

		DWORD dwBytesRead = 0;
		ReadFile(oldCfg, buf, size, &dwBytesRead, NULL);
		if (dwBytesRead != size) 
		{
			HeapFree(GetProcessHeap(), 0, buf);
			return false;
		}
		CloseHandle(oldCfg);
	}

	// create new file
	FILE* cfg = fopen(cfgFile, "wt");

	// loop over every line from the old file, and overwrite it in the new file
	// if necessary. Otherwise - copy the old line.
	
	BOOL bWrittenGdbDir = false; 

	char* line = buf; BOOL done = false;
	char* comment = NULL;
	if (buf != NULL) while (!done && line < buf + size)
	{
		char* endline = strstr(line, "\r\n");
		if (endline != NULL) endline[0] = '\0'; else done = true;
		char* comment = strstr(line, "#");
		char* setting = NULL;

		if ((setting = strstr(line, "gdb.dir")) && 
			(comment == NULL || setting < comment))
		{
			fprintf(cfg, "gdb.dir = \"%s\"", g_pesinfo.gdbDir);
			bWrittenGdbDir = true;
		}

		else
		{
			// take the old line
			fprintf(cfg, "%s\n", line);
		}

		if (endline != NULL) { endline[0] = '\r'; line = endline + 2; }
	}

	// if something wasn't written, make sure we write it.
	if (!bWrittenGdbDir) 
		fprintf(cfg, "gdb.dir = \"%s\"\n", g_pesinfo.gdbDir);

	// release buffer
	HeapFree(GetProcessHeap(), 0, buf);

	fclose(cfg);

	return true;
}

