// bserv.h

#define MODID 101
#define NAMELONG "Ballserver 6.4.0"
#define NAMESHORT "BSERV"

#define DEFAULT_DEBUG 1

#define BUFLEN 4096

#define CODELEN 1
enum {
    C_SETBALLNAME_CS,
};

static DWORD codeArray[][CODELEN] = {
	// PES6
	{
        0x7834b8,
    },
	// PES6 1.10
	{
        0x783688,
    },
};

#define DATALEN 6
enum {
    AFS_PAGELEN_TABLE, NUM_BALL_FILES, NOT_A_BALL_FILE,
    TEAM_IDS, SAVED_TEAM_HOME, SAVED_TEAM_AWAY,
};

static DWORD dataArray[][DATALEN] = {
	// PES6
	{
	 0x3b5cbc0, 49, 38,
	 0x3be0940, 0x1131fd4, 0x1131fd8,
    },
	// PES6 1.10
	{
	 0x3b5dbc0, 49, 38,
	 0x3be1940, 0x1132fd4, 0x1132fd8,
    },
};


static DWORD code[CODELEN];
static DWORD data[DATALEN];
	

#define SWAPBYTES(dw) \
    (dw<<24 & 0xff000000) | (dw<<8  & 0x00ff0000) | \
    (dw>>8  & 0x0000ff00) | (dw>>24 & 0x000000ff)

#define MAX_ITERATIONS 1000

//#define KeyNextBall 0x42
//#define KeyPrevBall 0x56
//#define KeyResetBall 0x43
//#define KeyRandomBall 0x52
//#define KeyResetBall 0x37
//#define KeyRandomBall 0x38
//#define KeyPrevBall 0x39
//#define KeyNextBall 0x30

typedef struct _AFSENTRY {
	DWORD FileNumber;
	DWORD AFSAddr;
	DWORD FileSize;
	DWORD Buffer;
} AFSENTRY;

typedef struct _BALLS {
	LPTSTR display;
	LPTSTR model;
	LPTSTR texture;
} BALLS;

typedef struct _ENCBUFFERHEADER {
	DWORD dwSig;
	DWORD dwEncSize;
	DWORD dwDecSize;
	BYTE other[20];
} ENCBUFFERHEADER;

typedef struct _DECBUFFERHEADER {
	DWORD dwSig;
	DWORD numTexs;
	DWORD dwDecSize;
	BYTE reserved1[4];
	WORD bitsOffset;
	WORD paletteOffset;
	WORD texWidth;
	WORD texHeight;
	BYTE reserved2[2];
	BYTE bitsPerPixel;
	BYTE reserved3[13];
	WORD blockWidth;
	WORD blockHeight;
	BYTE other[84];
} DECBUFFERHEADER;

typedef struct _PNG_CHUNK_HEADER {
    DWORD dwSize;
    DWORD dwName;
} PNG_CHUNK_HEADER;

typedef struct _BSERV_CFG {
    int selectedBall;
    BOOL previewEnabled;
} BSERV_CFG;

#define BALL_GAME_CHOICE 0
#define BALL_SELECT 1
#define BALL_HOME_TEAM 2

#define HOME 0
#define AWAY 1

