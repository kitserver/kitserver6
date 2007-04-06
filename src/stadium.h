// stadium.h

#include "afsreader.h"

#define MODID 106
#define NAMELONG "Stadium Server 6.5.0"
#define NAMESHORT "STADIUM"

#define DEFAULT_DEBUG 1

#define BUFLEN 4096

#define MAX_ITERATIONS 1000

#define KeyResetStadium 0x37
#define KeyRandomStadium 0x38
#define KeyPrevStadium 0x39
#define KeyNextStadium 0x30
#define KeySwitchGdbStadiums 0x31
#define KeySwitchWeather 0x32

typedef struct _LCM {
    WORD homeTeamId;
    WORD awayTeamId;
    BYTE stadium;
    BYTE unknown1;
    BYTE timeOfDay;
    BYTE weather;
    BYTE season;
    BYTE effects;
    BYTE unknown2[6];
    BYTE unknown3[2];
    BYTE crowdStance;
    BYTE numSubs;
    BYTE homeCrowd;
    BYTE awayCrowd;
    BYTE unknown4[10];
} LCM;

typedef struct _STADINFO {
    DWORD built;
    DWORD capacity;
    char city[255];
} STADINFO;

#define STAD_GAME_CHOICE 0
#define STAD_SELECT 1
#define STAD_HOME_TEAM 2

typedef struct _STAD_CFG {
    BYTE mode;
    char stadName[512];
} STAD_CFG;

