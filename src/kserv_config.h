#ifndef _JUCE_CONFIG
#define _JUCE_CONFIG

#include <windows.h>
#include <map>

#define BUFLEN 4096

#define MODID 100
#define NAMELONG "KitServer 6.3.2"
#define NAMESHORT "KSERV"
#define CONFIG_FILE "kserv.cfg"

#define DEFAULT_DEBUG 0
#define DEFAULT_VKEY_HOMEKIT 0x31
#define DEFAULT_VKEY_AWAYKIT 0x32
#define DEFAULT_VKEY_GKHOMEKIT 0x33
#define DEFAULT_VKEY_GKAWAYKIT 0x34

class KSERV_CONFIG {
public:
    KSERV_CONFIG() :
        vKeyHomeKit(DEFAULT_VKEY_HOMEKIT),
        vKeyAwayKit(DEFAULT_VKEY_AWAYKIT),
        vKeyGKHomeKit(DEFAULT_VKEY_GKHOMEKIT),
        vKeyGKAwayKit(DEFAULT_VKEY_GKAWAYKIT),
        ShowKitInfo(true)
    {}

	WORD   vKeyHomeKit;
	WORD   vKeyAwayKit;
	WORD   vKeyGKHomeKit;
	WORD   vKeyGKAwayKit;
	bool   ShowKitInfo;
    std::map<int,bool> wideBackModels;
    std::map<int,bool> narrowBackModels;
    std::map<int,bool> squashedWithLogoModels;
};

BOOL ReadConfig(KSERV_CONFIG* config, char* cfgFile);
BOOL WriteConfig(KSERV_CONFIG* config, char* cfgFile);

#endif
