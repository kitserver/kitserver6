// fserv.h

#define MODID 103
#define NAMELONG "Faceserver 6.3.0"
#define NAMESHORT "FSERV"

#define DEFAULT_DEBUG 1

#define BUFLEN 4096

//AFS IDs of the faces
#define FIDSLEN 25
enum {STARTW, STARTY, STARTR, STARTK,
	ISEDITPLAYERLIST, ISEDITPLAYERMODE,EDITEDPLAYER, FIX_DWORDFACEID,
	CALCHAIRID, CALCSPHAIRID, HAIRSTARTARRAY, C_GETHAIRTRANSP, C_GETHAIRTRANSP_CS,
	PLAYERDATA_BASE, EDITPLAYERDATA_BASE, C_COPYPLAYERDATA, C_COPYPLAYERDATA_CS,
	C_REPL_COPYPLAYERDATA_CS, C_EDITCOPYPLAYERDATA, C_EDITCOPYPLAYERDATA_CS,
	C_NEWFILEFROMAFS_CS,AFS_PAGELEN_TABLE,C_EDITCOPYPLAYERDATA2,C_EDITCOPYPLAYERDATA3,
	C_EDITCOPYPLAYERDATA4,
	};

DWORD fIDsArray[][FIDSLEN] = {
	//PES6
	{
		2402,2764,2331,2284,
		0x1108534,0x11084e8,0x112e24a,0x8b3573,
		0x8b400e,0x8b3fd4,0xd441a4,0x8b3f50,0xb63f69,
		0x3bcf55c,0x3bdc980,0xa4b040,0xa4b4b2,
		0xb3ba5e,0x861d20,0x803c2f,
		0x65b63f,0x3b5cbc0,0x80cf6f,0x804803,
		0xb0ec2d,
	},
};

DWORD fIDs[FIDSLEN];

typedef struct _ENCBUFFERHEADER {
	DWORD dwSig;
	DWORD dwEncSize;
	DWORD dwDecSize;
	BYTE other[20];
} ENCBUFFERHEADER;

typedef struct _INFOBLOCK {
	BYTE reserved1[0x54];
	DWORD FileID; //0x54
	BYTE reserved2[8];
	DWORD src; //0x60
	DWORD dest; //0x64
} INFOBLOCK;

typedef struct _DATAOFID {
	BYTE type;
	DWORD id;
} DATAOFID;

typedef struct _DATAOFMEMORY {
	BYTE type;
	DWORD size;
	BYTE data[];
} DATAOFMEMORY;
	
#define FACESET_NONE 0
#define FACESET_SPECIAL 1
#define FACESET_NORMAL 2

#define HIGHEST_PLAYER_ID 0xFFFF
#define FIRSTREPLPLAYERID 0x7500

typedef void   (*COPYPLAYERDATA)(DWORD,DWORD,DWORD);
typedef BYTE   (*GETHAIRTRANSP)(DWORD);
typedef DWORD  (*EDITCOPYPLAYERDATA)(DWORD,DWORD);
