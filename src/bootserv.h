// bootserv.h

#define MODID 107
#define NAMELONG "Bootserver 1.0.12-BETA"
#define NAMESHORT "BOOTSERV"

#define DEFAULT_DEBUG 1
#define BUFLEN 4096

typedef struct _LINEUP_RECORD {
    DWORD ordinal;
    DWORD playerInfoAddr;
    DWORD unknown1;
    WORD ordinalAgain;
    WORD playerId;
    BYTE unknown2;
    BYTE playerOrdinal; // in the team
    BYTE isRight; // 0-left, 1-right
    BYTE isAway;
    WORD unknown3[0x22c];
} LINEUP_RECORD;

