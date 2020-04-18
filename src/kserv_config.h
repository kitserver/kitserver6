#ifndef _JUCE_CONFIG
#define _JUCE_CONFIG

#include <windows.h>
#include <unordered_map>

#define BUFLEN 4096

#define MODID 107
#define NAMELONG "KitServer 6.8.0"
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
        ShowKitInfo(true),
        enable_HD_kits(true),
        enable_in_edit_mode(false)
    {}

	WORD   vKeyHomeKit;
	WORD   vKeyAwayKit;
	WORD   vKeyGKHomeKit;
	WORD   vKeyGKAwayKit;
	bool   ShowKitInfo;
    bool   enable_HD_kits;
    bool   enable_in_edit_mode;
    std::unordered_map<int,bool> wideBackModels;
    std::unordered_map<int,bool> narrowBackModels;
    std::unordered_map<int,bool> squashedWithLogoModels;
};

BOOL ReadConfig(KSERV_CONFIG* config, char* cfgFile);
BOOL WriteConfig(KSERV_CONFIG* config, char* cfgFile);

#endif
