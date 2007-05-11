// fserv.h

#define MODID 103
#define NAMELONG "Faceserver 6.5.2"
#define NAMESHORT "FSERV"

#define DEFAULT_DEBUG 1

#define BUFLEN 4096

//AFS IDs of the faces
#define FIDSLEN 29
enum {STARTW, STARTY, STARTR, STARTK,
	ISEDITPLAYERLIST, ISEDITPLAYERMODE,EDITEDPLAYER, FIX_DWORDFACEID,
	CALCHAIRID, CALCSPHAIRID, HAIRSTARTARRAY, C_GETHAIRTRANSP, C_GETHAIRTRANSP_CS,
	PLAYERDATA_BASE, EDITPLAYERDATA_BASE, C_COPYPLAYERDATA, C_COPYPLAYERDATA_CS,
	C_REPL_COPYPLAYERDATA_CS, C_EDITCOPYPLAYERDATA, C_EDITCOPYPLAYERDATA_CS,
	C_NEWFILEFROMAFS_CS,AFS_PAGELEN_TABLE,C_EDITCOPYPLAYERDATA2,C_EDITCOPYPLAYERDATA3,
	C_EDITCOPYPLAYERDATA4,RANDSEL_IDS,C_RANDSEL_PLAYERS,C_MYTEAM_CPD_CS,
	C_EDITUNI_CPD_CS,
	
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
		0xb0ec2d,0xd5d088,0x866d50,0x6d21e8,
		0x80133f,
		
	},
	//PES6 1.10
	{
		2402,2764,2331,2284,
		0x1109534,0x11094e8,0x112f24a,0x8b3653,
		0x8b40ee,0x8b40b4,0xd45284,0x8b4030,0xb640b9,
		0x3bd055c,0x3bdd980,0xa4b1a0,0xa4b612,
		0xb3bc0e,0x861e50,0x803daf,
		0x65b831,0x3b5dbc0,0x80d0df,0x804983,
		0xb0edad,0xd5e168,0x866ea0,0x6d2348,
		0x8014df,
	},
	//WE2007
	{
		2547,2929,2473,2419,
		0x1102f9c,0x1102f50,0x1128cb2,0x8b3d73,
		0x8b480e,0x8b47d4,0xd3e4ac,0x8b4750,0xb64969,
		0x3bc9fdc,0x3bd7400,0xa4b8f0,0xa4bd62,
		0xb3c56e,0x862430,0x803bff,
		0x65b67f,0x3b57640,0x80cf2f,0x8047d3,
		0xb0f72d,0xd57448,0x867460,0x6d21b8,
		0x80132f,
	},
};

DWORD fIDs[FIDSLEN];

#ifndef _ENCBUFFERHEADER_
#define _ENCBUFFERHEADER_
typedef struct _ENCBUFFERHEADER {
	DWORD dwSig;
	DWORD dwEncSize;
	DWORD dwDecSize;
	BYTE other[20];
} ENCBUFFERHEADER;
#endif

#ifndef _INFOBLOCK_
#define _INFOBLOCK_
typedef struct _INFOBLOCK {
	BYTE reserved1[0x54];
	DWORD FileID; //0x54
	BYTE reserved2[8];
	DWORD src; //0x60
	DWORD dest; //0x64
} INFOBLOCK;
#endif

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

enum {RANDSEL_TEAM1,RANDSEL_TEAM2,RANDSEL_PLID1,RANDSEL_PLID2};

typedef void   (*COPYPLAYERDATA)(DWORD,DWORD,DWORD);
typedef BYTE   (*GETHAIRTRANSP)(DWORD);
typedef DWORD  (*EDITCOPYPLAYERDATA)(DWORD,DWORD);
typedef DWORD  (*RANDSEL_PLAYERS)(DWORD);
