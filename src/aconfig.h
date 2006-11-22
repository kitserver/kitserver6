#ifndef _JUCE_CONFIG
#define _JUCE_CONFIG

#include <windows.h>

#define BUFLEN 4096

#define CONFIG_FILE "kserv.cfg"

#define DEFAULT_DEBUG 0
#define DEFAULT_GDB_DIR ".\\"
#define DEFAULT_VKEY_HOMEKIT 0x31
#define DEFAULT_VKEY_AWAYKIT 0x32
#define DEFAULT_VKEY_GKHOMEKIT 0x33
#define DEFAULT_VKEY_GKAWAYKIT 0x34
#define DEFAULT_VKEY_NEXT_BALL 0x42
#define DEFAULT_VKEY_PREV_BALL 0x44
#define DEFAULT_VKEY_RANDOM_BALL 0x52
#define DEFAULT_VKEY_RESET_BALL 0x43
#define DEFAULT_USE_LARGE_TEXTURE TRUE
#define DEFAULT_ENABLE_KIT_ENGINE TRUE

typedef struct _KSERV_CONFIG_STRUCT {
	DWORD  debug;
	char   gdbDir[BUFLEN];
    BOOL   enableKitEngine;
    BYTE   narrowBackModels[256];
	WORD   vKeyHomeKit;
	WORD   vKeyAwayKit;
	WORD   vKeyGKHomeKit;
	WORD   vKeyGKAwayKit;
	WORD   vKeyPrevBall;
	WORD   vKeyNextBall;
	WORD   vKeyRandomBall;
	WORD   vKeyResetBall;
	BOOL   useLargeTexture;

} KSERV_CONFIG;

BOOL ReadConfig(KSERV_CONFIG* config, char* cfgFile);
BOOL WriteConfig(KSERV_CONFIG* config, char* cfgFile);

#endif
