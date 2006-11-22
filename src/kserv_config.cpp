// kserv_config.cpp

#include <stdio.h>
#include <string.h>

#include "kserv_config.h"
#include "kload_exp.h"

extern KMOD k_mydll;

/**
 * Returns true if successful.
 */
BOOL ReadConfig(KSERV_CONFIG* config, char* cfgFile)
{
	if (config == NULL) return false;

    config->wideBackModels.clear();
    config->narrowBackModels.clear();
    config->squashedWithLogoModels.clear();

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
			k_mydll.debug = SetDebugLevel(value);
			DebugSet=true;
			LogWithNumber(&k_mydll,"ReadConfig: debug = (%d)", value);
		}
        else if (lstrcmp(name, "mini-kits.narrow-backs")==0)
        {
			char* startBracket = strstr(pValue, "[");
			if (startBracket == NULL) continue;
			char* endBracket = strstr(startBracket + 1, "]");
			if (endBracket == NULL) continue;

            endBracket[0]='\0';
			LogWithString(&k_mydll,"ReadConfig: mini-kits.narrow-backs = [%s]", startBracket+1);

            // read the numbers, separated by comma
            int num = -1;
            char* p = startBracket+1;
            while (p && sscanf(p,"%d",&num)==1) {
                config->narrowBackModels.insert(pair<int,bool>(num,true));
                p = strstr(p,",");
                if (p) p++;
            }
        }
        else if (lstrcmp(name, "mini-kits.wide-backs")==0)
        {
			char* startBracket = strstr(pValue, "[");
			if (startBracket == NULL) continue;
			char* endBracket = strstr(startBracket + 1, "]");
			if (endBracket == NULL) continue;

            endBracket[0]='\0';
			LogWithString(&k_mydll,"ReadConfig: mini-kits.wide-backs = [%s]", startBracket+1);

            // read the numbers, separated by comma
            int num = -1;
            char* p = startBracket+1;
            while (p && sscanf(p,"%d",&num)==1) {
                config->wideBackModels.insert(pair<int,bool>(num,true));
                p = strstr(p,",");
                if (p) p++;
            }
        }
        else if (lstrcmp(name, "mini-kits.squashed-with-logo")==0)
        {
			char* startBracket = strstr(pValue, "[");
			if (startBracket == NULL) continue;
			char* endBracket = strstr(startBracket + 1, "]");
			if (endBracket == NULL) continue;

            endBracket[0]='\0';
			LogWithString(&k_mydll,"ReadConfig: mini-kits.squashed-with-logo = [%s]", startBracket+1);

            // read the numbers, separated by comma
            int num = -1;
            char* p = startBracket+1;
            while (p && sscanf(p,"%d",&num)==1) {
                config->squashedWithLogoModels.insert(pair<int,bool>(num,true));
                p = strstr(p,",");
                if (p) p++;
            }
        }
		else if (lstrcmp(name, "vKey.homeKit")==0)
		{
			if (sscanf(pValue, "%x", &value)!=1) continue;
			LogWithNumber(&k_mydll,"ReadConfig: vKeyHomeKit = 0x%02x", value);
			config->vKeyHomeKit = value;
		}
		else if (lstrcmp(name, "vKey.awayKit")==0)
		{
			if (sscanf(pValue, "%x", &value)!=1) continue;
			LogWithNumber(&k_mydll,"ReadConfig: vKeyAwayKit = 0x%02x", value);
			config->vKeyAwayKit = value;
		}
		else if (lstrcmp(name, "vKey.gkHomeKit")==0)
		{
			if (sscanf(pValue, "%x", &value)!=1) continue;
			LogWithNumber(&k_mydll,"ReadConfig: vKeyGKHomeKit = 0x%02x", value);
			config->vKeyGKHomeKit = value;
		}
		else if (lstrcmp(name, "vKey.gkAwayKit")==0)
		{
			if (sscanf(pValue, "%x", &value)!=1) continue;
			LogWithNumber(&k_mydll,"ReadConfig: vKeyGKAwayKit = 0x%02x", value);
			config->vKeyGKAwayKit = value;
		}
		else if (lstrcmp(name, "ShowKitInfo")==0)
		{
			if (sscanf(pValue, "%d", &value)!=1) continue;
			LogWithNumber(&k_mydll,"ReadConfig: ShowKitInfo = (%d)", value);
			config->ShowKitInfo = (value > 0);
		}
	}
	fclose(cfg);

	if (!DebugSet)
		k_mydll.debug = SetDebugLevel(DEFAULT_DEBUG);

	return true;
}

/**
 * Returns true if successful.
 */
BOOL WriteConfig(KSERV_CONFIG* config, char* cfgFile)
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
	
	bool bWrittenVKeyHomeKit = false; BOOL bWrittenVKeyAwayKit = false;
	BOOL bWrittenVKeyGKHomeKit = false; BOOL bWrittenVKeyGKAwayKit = false;

	char* line = buf; BOOL done = false;
	char* comment = NULL;
	if (buf != NULL) while (!done && line < buf + size)
	{
		char* endline = strstr(line, "\r\n");
		if (endline != NULL) endline[0] = '\0'; else done = true;
		char* comment = strstr(line, "#");
		char* setting = NULL;

		if ((setting = strstr(line, "vKey.homeKit")) && 
				 (comment == NULL || setting < comment))
		{
			fprintf(cfg, "vKey.homeKit = 0x%02x", config->vKeyHomeKit);
			bWrittenVKeyHomeKit = true;
		}
		else if ((setting = strstr(line, "vKey.awayKit")) && 
				 (comment == NULL || setting < comment))
		{
			fprintf(cfg, "vKey.awayKit = 0x%02x", config->vKeyAwayKit);
			bWrittenVKeyAwayKit = true;
		}
		else if ((setting = strstr(line, "vKey.gkHomeKit")) && 
				 (comment == NULL || setting < comment))
		{
			fprintf(cfg, "vKey.gkHomeKit = 0x%02x", config->vKeyGKHomeKit);
			bWrittenVKeyGKHomeKit = true;
		}
		else if ((setting = strstr(line, "vKey.gkAwayKit")) && 
				 (comment == NULL || setting < comment))
		{
			fprintf(cfg, "vKey.gkAwayKit = 0x%02x", config->vKeyGKAwayKit);
			bWrittenVKeyGKAwayKit = true;
		}

		else
		{
			// take the old line
			fprintf(cfg, "%s\n", line);
		}

		if (endline != NULL) { endline[0] = '\r'; line = endline + 2; }
	}

	// if something wasn't written, make sure we write it.
	if (!bWrittenVKeyHomeKit)
		fprintf(cfg, "vKey.homeKit = 0x%02x\n", config->vKeyHomeKit);
	if (!bWrittenVKeyAwayKit)
		fprintf(cfg, "vKey.awayKit = 0x%02x\n", config->vKeyAwayKit);
	if (!bWrittenVKeyGKHomeKit)
		fprintf(cfg, "vKey.gkHomeKit = 0x%02x\n", config->vKeyGKHomeKit);
	if (!bWrittenVKeyGKAwayKit)
		fprintf(cfg, "vKey.gkAwayKit = 0x%02x\n", config->vKeyGKAwayKit);

	// release buffer
	HeapFree(GetProcessHeap(), 0, buf);

	fclose(cfg);

	return true;
}

