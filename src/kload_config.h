#ifndef _JUCE_CONFIG
#define _JUCE_CONFIG

#include <windows.h>
#include "manage.h"

#define BUFLEN 4096

#define MODID 0
#define NAMELONG "Module Loader 6.3.5"
#define NAMESHORT "KLOAD"
#define CONFIG_FILE "kload.cfg"

#define DEFAULT_DEBUG 0
#define DEFAULT_GDB_DIR ".\\"

typedef struct _KLOAD_CONFIG_STRUCT {
	DWORD numDLLs;
	LPTSTR* dllnames;
	BOOL forceSW_TnL;
	BOOL emulateHW_TnL;
	DWORD newResMem;
} KLOAD_CONFIG;

BOOL ReadConfig(KLOAD_CONFIG* config, char* cfgFile);
BOOL WriteConfig(KLOAD_CONFIG* config, char* cfgFile);

#endif
