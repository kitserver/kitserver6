#ifndef _SGF_H_
#define _SGF_H_

typedef BYTE*	(*REQUESTREPLAYPLAYERDATA)(BYTE,DWORD);
typedef BYTE*	(*REQUESTMLPLAYERDATA)(BYTE,DWORD);
typedef void    (*SGFREEBUFFER)(BYTE*);

typedef struct _SAVEGAMEFUNCS {
	REQUESTREPLAYPLAYERDATA RequestReplayPlayerData;
	REQUESTMLPLAYERDATA RequestMLPlayerData;
	SGFREEBUFFER FreeBuffer;
} SAVEGAMEFUNCS;


typedef BYTE*	(*GIVEIDTOMODULE)(DWORD,DWORD);

typedef struct _SAVEGAMEEXPFUNCS {
	GIVEIDTOMODULE giveIdToModule;
} SAVEGAMEEXPFUNCS;

enum {
	SGEFM_FSERV,
	SGEFM_LAST,
};

enum {
	SGEF_GIVEIDTOMODULE,
	SGEF_LAST,
};

#define numSaveGameExpModules SGEFM_LAST


#endif