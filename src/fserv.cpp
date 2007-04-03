// fserv.cpp
#include <windows.h>
#include <stdio.h>
#include "fserv.h"
#include "kload_exp.h"
#include "numpages.h"

#include <hash_map>

KMOD k_fserv={MODID,NAMELONG,NAMESHORT,DEFAULT_DEBUG};
#define DEFAULT_USESPECIALHAIR 0

HINSTANCE hInst;
bool Inited=false;
bool savegameModPresent=false;
BYTE g_savedDWORDFaceIDfix[2];
BYTE g_savedCalcHairFile[18];
BYTE g_savedCalcSpHairFile[15];
BYTE g_savedCopyPlayerData[5];
BYTE g_savedReplCopyPlayerData[11];

bool bGetHairTranspHooked=false;
bool bEditCopyPlayerDataHooked=false;

COPYPLAYERDATA orgCopyPlayerData=NULL;
GETHAIRTRANSP orgGetHairTransp=NULL;
EDITCOPYPLAYERDATA orgEditCopyPlayerData=NULL;
RANDSEL_PLAYERS randSelPlayersAddr=NULL;

DWORD StartsStdFaces[4];
WORD* randSelIDs=NULL;

std::hash_map<DWORD,LPVOID> g_Buffers;
std::hash_map<DWORD,LPVOID>::iterator g_BuffersIterator;
	
//Stores the filenames to a face id
DWORD numFaces=0;
std::hash_map<DWORD,char*> g_Faces;
std::hash_map<DWORD,char*>::iterator g_FacesIterator;

//Same for hair
DWORD numHair=0;
std::hash_map<DWORD,char*> g_Hair;
std::hash_map<DWORD,char*>::iterator g_HairIterator;
std::hash_map<DWORD,BYTE> g_HairTransp;

//Stores larger face id for players
std::hash_map<DWORD,DWORD> g_Players;
std::hash_map<DWORD,DWORD> g_PlayersHair;
std::hash_map<DWORD,DWORD> g_PlayersAddr;
std::hash_map<DWORD,DWORD> g_PlayersAddr2;
std::hash_map<DWORD,DWORD> g_SpecialFaceHair;
std::hash_map<DWORD,DWORD> g_EditorAddresses;
std::hash_map<DWORD,bool>  g_FaceExists;
std::hash_map<DWORD,bool>  g_HairExists;
std::hash_map<DWORD,DWORD>::iterator g_PlayersIterator;

BYTE isInEditPlayerMode=0, isInEditPlayerList=0;
DWORD lastPlayerNumber=0, lastFaceID=0;
DWORD lastHairID=0xffffffff, lastGDBHairID=0xffffffff, lastGDBHairRes=0;
bool lastWasFromGDB=false, lastHairWasFromGDB=false, hasChanged=true;
char lastPlayerNumberString[BUFLEN],lastFaceFileString[BUFLEN];
char lastHairFileString[BUFLEN];
char tmpFilename[BUFLEN]; //crashes if this is defined locally in the functions


EXTERN_C BOOL WINAPI DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID lpReserved);
void InitFserv();

bool FileExists(char* filename);

void GetGDBFaces();
void AddPlayerFace(DWORD PlayerNumber, char* sfile, DWORD useSpecialHair);
DWORD GetIDForFaceName(char* sfile);

void GetGDBHair();
void AddPlayerHair(DWORD PlayerNumber,char* sfile,BYTE transp);
DWORD GetIDForHairName(char* sfile);

DWORD CalcHairFile(BYTE Caller);

DWORD GetNextSpecialAfsFileIdForFace(DWORD FaceID, BYTE Skincolor);

void fservAfsReplace(GETFILEINFO* gfi);
void fservProcessPlayerData(DWORD ESI, DWORD* PlayerNumber);
void PrintPlayerInfo(IDirect3DDevice8* self, CONST RECT* src, CONST RECT* dest, HWND hWnd, LPVOID unused);
void fservCopyPlayerData(DWORD p1, DWORD p2, DWORD p3);
void fservReplCopyPlayerData();
DWORD fservEditCopyPlayerData(DWORD playerNumber, DWORD p2);
DWORD fservEditCopyPlayerData2(DWORD playerNumber);
DWORD fservEditCopyPlayerData3(DWORD p1);
DWORD fservEditCopyPlayerData4(DWORD slot, DWORD editorAddress, DWORD p3);
DWORD fservEditUniCopyPlayerData(DWORD playerNumber);
DWORD fservMyTeamCPD(DWORD playerNumber);
BYTE fservGetHairTransp(DWORD addr);
DWORD ResolvePlayerID(DWORD playerID);

void fservConnectAddrToId(DWORD addr, DWORD id);

bool HookProcAtAddr(DWORD proc, DWORD proc_cs, DWORD newproc, char* sproc, char* sproc_cs)
{
    if (proc != 0 && proc_cs != 0)
    {
        BYTE* bptr = (BYTE*)proc_cs;
        DWORD protection = 0;
        DWORD newProtection = PAGE_EXECUTE_READWRITE;
        if (VirtualProtect(bptr, 8, newProtection, &protection)) {
            DWORD* ptr = (DWORD*)(proc_cs + 1);
            ptr[0] = newproc - (DWORD)(proc_cs + 5);
            LogWithTwoStrings(&k_fserv,"%s HOOKED at %s", sproc, sproc_cs);
            return true;
        }
    }
    return false;
}

bool UnhookProcAtAddr(bool flag, DWORD proc, DWORD proc_cs, char* sproc, char* sproc_cs)
{
    if (flag && proc !=0 && proc_cs != 0)
    {
        BYTE* bptr = (BYTE*)proc_cs;
        DWORD protection = 0;
        DWORD newProtection = PAGE_EXECUTE_READWRITE;
        if (VirtualProtect(bptr, 8, newProtection, &protection)) {
            DWORD* ptr = (DWORD*)(proc_cs + 1);
            ptr[0] = (DWORD)proc - (DWORD)(proc_cs + 5);
            LogWithTwoStrings(&k_fserv,"%s UNHOOKED at %s", sproc, sproc_cs);
            return true;
        }
    }
    return false;
}


EXTERN_C BOOL WINAPI DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID lpReserved)
{
	int i,j;
	
	if (dwReason == DLL_PROCESS_ATTACH)
	{
		Log(&k_fserv,"Attaching dll...");
		
		hInst=hInstance;
		
		int v=GetPESInfo()->GameVersion;
		switch (v) {
			case gvPES6PC:
			case gvPES6PC110:
				goto GameVersIsOK;
				break;
		};
		//Will land here if game version is not supported
		Log(&k_fserv,"Your game version is currently not supported!");
		return false;
		
		//Everything is OK!
		GameVersIsOK:

		RegisterKModule(&k_fserv);
		
		//copy the FaceIDs for the right game version
		memcpy(fIDs,fIDsArray[v],sizeof(fIDs));
		orgCopyPlayerData=(COPYPLAYERDATA)fIDs[C_COPYPLAYERDATA];
		orgEditCopyPlayerData=(EDITCOPYPLAYERDATA)fIDs[C_EDITCOPYPLAYERDATA];
		orgGetHairTransp=(GETHAIRTRANSP)fIDs[C_GETHAIRTRANSP];
		randSelPlayersAddr=(RANDSEL_PLAYERS)fIDs[C_RANDSEL_PLAYERS];
		randSelIDs=(WORD*)fIDs[RANDSEL_IDS];
		
		HookFunction(hk_D3D_Create,(DWORD)InitFserv);
		
	}
	else if (dwReason == DLL_PROCESS_DETACH)
	{
		Log(&k_fserv,"Detaching dll...");
		
		UnhookFunction(hk_ProcessPlayerData,(DWORD)fservProcessPlayerData);
		UnhookFunction(hk_D3D_Present,(DWORD)PrintPlayerInfo);
		
		DWORD protection=0, newProtection=PAGE_EXECUTE_READWRITE;
		if (fIDs[FIX_DWORDFACEID] != 0) {
            if (VirtualProtect((BYTE*)fIDs[FIX_DWORDFACEID], 2, newProtection, &protection)) {
                memcpy((BYTE*)fIDs[FIX_DWORDFACEID], g_savedDWORDFaceIDfix, 2);
            }
        }
		if (fIDs[CALCHAIRID] != 0) {
            if (VirtualProtect((BYTE*)fIDs[CALCHAIRID], 18, newProtection, &protection)) {
                memcpy((BYTE*)fIDs[CALCHAIRID], g_savedCalcHairFile, 18);
            }
        }
		if (fIDs[CALCSPHAIRID] != 0) {
			if (VirtualProtect((BYTE*)fIDs[CALCSPHAIRID], 15, newProtection, &protection)) {
                memcpy((BYTE*)fIDs[CALCSPHAIRID], g_savedCalcSpHairFile, 15);
            }
        }
		if (fIDs[C_COPYPLAYERDATA_CS] != 0)
			if (VirtualProtect((BYTE*)fIDs[C_COPYPLAYERDATA_CS], 5, newProtection, &protection))
				memcpy((BYTE*)fIDs[C_COPYPLAYERDATA_CS], g_savedCopyPlayerData, 5);
			
		if (fIDs[C_REPL_COPYPLAYERDATA_CS] != 0)
			if (VirtualProtect((BYTE*)fIDs[C_REPL_COPYPLAYERDATA_CS], 11, newProtection, &protection))
				memcpy((BYTE*)fIDs[C_REPL_COPYPLAYERDATA_CS], g_savedReplCopyPlayerData, 11);
			
		bGetHairTranspHooked=UnhookProcAtAddr(bGetHairTranspHooked,fIDs[C_GETHAIRTRANSP],
			fIDs[C_GETHAIRTRANSP_CS],"C_GETHAIRTRANSP","C_GETHAIRTRANSP_CS");
			
		bEditCopyPlayerDataHooked=UnhookProcAtAddr(bEditCopyPlayerDataHooked,fIDs[C_EDITCOPYPLAYERDATA],
			fIDs[C_EDITCOPYPLAYERDATA_CS],"C_EDITCOPYPLAYERDATA","C_EDITCOPYPLAYERDATA_CS");
			
		MasterUnhookFunction(fIDs[C_EDITCOPYPLAYERDATA2],fservEditCopyPlayerData2);
		MasterUnhookFunction(fIDs[C_EDITCOPYPLAYERDATA3],fservEditCopyPlayerData3);
		MasterUnhookFunction(fIDs[C_EDITCOPYPLAYERDATA4],fservEditCopyPlayerData4);
		MasterUnhookFunction(fIDs[C_EDITUNI_CPD_CS],fservEditUniCopyPlayerData);
		MasterUnhookFunction(fIDs[C_MYTEAM_CPD_CS],fservMyTeamCPD);
			
		for (j=0;j<numFaces;j++)
			if (g_Faces[j] != NULL)
				delete g_Faces[j];
		
		g_Faces.clear();
		
		for (j=0;j<numHair;j++)
			if (g_Hair[j] != NULL)
					delete g_Hair[j];
		
		g_Hair.clear();
		g_HairTransp.clear();
		
		g_Players.clear();
		g_PlayersHair.clear();
		g_PlayersAddr.clear();
		g_PlayersAddr2.clear();
		g_SpecialFaceHair.clear();
		g_EditorAddresses.clear();
		g_FaceExists.clear();
		g_HairExists.clear();
		
		Log(&k_fserv,"Detaching done.");
	};
	
	return true;
};

void InitFserv()
{
	Log(&k_fserv,"InitFserv");
	HookFunction(hk_ProcessPlayerData,(DWORD)fservProcessPlayerData);
	HookFunction(hk_D3D_Present,(DWORD)PrintPlayerInfo);
	
	RegisterAfsReplaceCallback(fservAfsReplace);
	
	for (int i=0;i<4;i++) {
		StartsStdFaces[i]=fIDs[STARTW+i];
	};
	
	//No need to do this later, and kits are loaded at this time as well
	if (!Inited) {
		GetGDBFaces();
		GetGDBHair();
		
		Inited=true;
	};

	Log(&k_fserv, "hooking various functions");

	DWORD protection=0, newProtection=PAGE_EXECUTE_READWRITE;
	BYTE* bptr=NULL;
	DWORD* ptr=NULL;

	// make PES read the face id as DWORD value rather than as WORD value
	if (fIDs[FIX_DWORDFACEID] != 0)
	{
		bptr = (BYTE*)fIDs[FIX_DWORDFACEID];
	    memcpy(g_savedDWORDFaceIDfix, bptr, 2);
	    if (VirtualProtect(bptr, 2, newProtection, &protection)) {
	        bptr[0]=0x90;
	        bptr[1]=0x8B;
	    };
	};

	//Hook hair id calculation
	if (fIDs[CALCHAIRID] != 0)
	{
		bptr = (BYTE*)fIDs[CALCHAIRID];
		ptr = (DWORD*)(bptr+1);
	    memcpy(g_savedCalcHairFile, bptr, 18);
	    if (VirtualProtect(bptr, 18, newProtection, &protection)) {
	        bptr[0]=0xe8;
	        ptr[0] = (DWORD)CalcHairFile - (DWORD)(fIDs[CALCHAIRID] + 5);
			memset(bptr+5,0x90,13);
	    };
	};
	
	if (fIDs[CALCSPHAIRID] != 0)
	{
		bptr = (BYTE*)fIDs[CALCSPHAIRID];
		ptr = (DWORD*)(bptr+1);
	    memcpy(g_savedCalcSpHairFile, bptr, 15);
	    if (VirtualProtect(bptr, 15, newProtection, &protection)) {
	        bptr[0]=0xe8;
	        ptr[0] = (DWORD)CalcHairFile - (DWORD)(fIDs[CALCSPHAIRID] + 5);
	        memset(bptr+5,0x90,10);
	    };
	};

	if (fIDs[C_COPYPLAYERDATA_CS] != 0)
	{
		bptr = (BYTE*)fIDs[C_COPYPLAYERDATA_CS];
		ptr = (DWORD*)(bptr+1);
	    memcpy(g_savedCopyPlayerData, bptr, 5);
	    if (VirtualProtect(bptr, 5, newProtection, &protection)) {
	        bptr[0]=0xe8;
	        ptr[0] = (DWORD)fservCopyPlayerData - (DWORD)(fIDs[C_COPYPLAYERDATA_CS] + 5);
	    };
	};
	
	if (fIDs[C_REPL_COPYPLAYERDATA_CS] != 0)
	{
		bptr = (BYTE*)fIDs[C_REPL_COPYPLAYERDATA_CS];
		ptr = (DWORD*)(bptr+1);
	    memcpy(g_savedReplCopyPlayerData, bptr, 11);
	    if (VirtualProtect(bptr, 11, newProtection, &protection)) {
	        bptr[0]=0xe8;
	        ptr[0] = (DWORD)fservReplCopyPlayerData - (DWORD)(fIDs[C_REPL_COPYPLAYERDATA_CS] + 5);
	        memset(bptr+5,0x90,6);
	    };
	};
	
	bGetHairTranspHooked=HookProcAtAddr(fIDs[C_GETHAIRTRANSP],fIDs[C_GETHAIRTRANSP_CS],
		(DWORD)fservGetHairTransp,"C_GETHAIRTRANSP","C_GETHAIRTRANSP_CS");
		
	bEditCopyPlayerDataHooked=HookProcAtAddr(fIDs[C_EDITCOPYPLAYERDATA],fIDs[C_EDITCOPYPLAYERDATA_CS],
		(DWORD)fservEditCopyPlayerData,"C_EDITCOPYPLAYERDATA","C_EDITCOPYPLAYERDATA_CS");
			
	MasterHookFunction(fIDs[C_EDITCOPYPLAYERDATA2],1,fservEditCopyPlayerData2);
	MasterHookFunction(fIDs[C_EDITCOPYPLAYERDATA3],1,fservEditCopyPlayerData3);
	MasterHookFunction(fIDs[C_EDITCOPYPLAYERDATA4],3,fservEditCopyPlayerData4);
	MasterHookFunction(fIDs[C_EDITUNI_CPD_CS],1,fservEditUniCopyPlayerData);
	MasterHookFunction(fIDs[C_MYTEAM_CPD_CS],1,fservMyTeamCPD);
	
	Log(&k_fserv, "hooking done");
	
	UnhookFunction(hk_D3D_Create,(DWORD)InitFserv);

	return;
};

bool FileExists(char* filename)
{
    TRACE4(&k_fserv,"FileExists: Checking file: %s", filename);
    HANDLE hFile;
    hFile = CreateFile(TEXT(filename),        // file to open
                       GENERIC_READ,          // open for reading
                       FILE_SHARE_READ,       // share for reading
                       NULL,                  // default security
                       OPEN_EXISTING,         // existing file only
                       FILE_ATTRIBUTE_NORMAL, // normal file
                       NULL);                 // no attr. template
     
    if (hFile == INVALID_HANDLE_VALUE) 
    { 
        return FALSE;
    }
    CloseHandle(hFile);
    return TRUE;
};


void GetGDBFaces()
{
	char tmp[BUFLEN];
	char str[BUFLEN];
	char *comment=NULL;
	char sfile[BUFLEN];
	DWORD number=0, useSpecialHair, scanRes;
	
	strcpy(tmp,GetPESInfo()->gdbDir);
	strcat(tmp,"GDB\\faces\\map.txt");
	
	FILE* cfg=fopen(tmp, "rt");
	if (cfg==NULL) {
		Log(&k_fserv,"Couldn't find faces map!");
		return;
	};
	
	while (true) {
		ZeroMemory(str, BUFLEN);
		fgets(str, BUFLEN-1, cfg);
		if (feof(cfg)) break;
		
		// skip comments
		comment=NULL;
		comment = strstr(str, "#");
		if (comment != NULL) comment[0] = '\0';
		
		// parse line
		ZeroMemory(sfile,BUFLEN);
		useSpecialHair=DEFAULT_USESPECIALHAIR;
		scanRes=sscanf(str,"%d,\"%[^\"]\",%d",&number,sfile,&useSpecialHair);
		switch (scanRes) {
		case 3:
			AddPlayerFace(number, sfile, useSpecialHair);
			break;
		case 2:
			AddPlayerFace(number, sfile, DEFAULT_USESPECIALHAIR);
			break;
		};
	};
	fclose(cfg);
	
	LogWithNumber(&k_fserv,"Number of GDB faces is %d",numFaces);
	return;
};

void AddPlayerFace(DWORD PlayerNumber, char* sfile, DWORD useSpecialHair)
{
	LogWithNumberAndString(&k_fserv,"Player # %d gets face %s",PlayerNumber,sfile);
	/* Leave out this check, this is done later
	char tmpFilename[BUFLEN];
	strcpy(tmpFilename,GetPESInfo()->gdbDir);
	strcat(tmpFilename,"GDB\\faces\\");
	strcat(tmpFilename,sfile);
	if (!FileExists(tmpFilename)) {
		Log(&k_fserv,"File doesn't exist, line is ignored!");
		return;
	};*/
	
	DWORD newId=GetIDForFaceName(sfile);
	if (newId==0xFFFFFFFF) {
		newId=numFaces+1000;
		g_Faces[newId]=new char[strlen(sfile)+1];
		ZeroMemory(g_Faces[newId],strlen(sfile));
		strcpy(g_Faces[newId],sfile);
		numFaces++;
		g_FaceExists[newId]=false;
	};
	LogWithNumber(&k_fserv,"Assigned face id is %d",newId);
	g_Players[PlayerNumber]=newId;
	
	//don't use the hair belonging to a special face
	if (useSpecialHair==0)
		g_SpecialFaceHair[PlayerNumber]=0xffffffff;
	return;
};

DWORD GetIDForFaceName(char* sfile)
{
	for (g_FacesIterator=g_Faces.begin();g_FacesIterator!=g_Faces.end();g_FacesIterator++) {
		if (g_FacesIterator->second != NULL)
			if (strcmp(g_FacesIterator->second,sfile)==0)
				return g_FacesIterator->first;
	};
	
	return 0xFFFFFFFF;
};

void GetGDBHair()
{
	char tmp[BUFLEN];
	char str[BUFLEN];
	char *comment=NULL;
	char sfile[BUFLEN];
	DWORD number=0, scanRes=0, transp=255;
	
	strcpy(tmp,GetPESInfo()->gdbDir);
	strcat(tmp,"GDB\\hair\\map.txt");
	
	FILE* cfg=fopen(tmp, "rt");
	if (cfg==NULL) {
		Log(&k_fserv,"Couldn't find hair map!");
		return;
	};
	
	while (true) {
		ZeroMemory(str, BUFLEN);
		fgets(str, BUFLEN-1, cfg);
		if (feof(cfg)) break;
		
		// skip comments
		comment=NULL;
		comment = strstr(str, "#");
		if (comment != NULL) comment[0] = '\0';
		
		// parse line
		number=0;
		ZeroMemory(sfile,BUFLEN);
		transp=255;
		scanRes=sscanf(str,"%d,\"%[^\"]\",%d",&number,sfile,&transp);
		if (transp>255) transp=255;
		switch (scanRes) {
		case 3:
			AddPlayerHair(number,sfile,transp);
			break;
		case 2:
			AddPlayerHair(number,sfile,255);
			break;
		};
	};
	fclose(cfg);
	
	LogWithNumber(&k_fserv,"Number of GDB hair is %d",numHair);
	return;
};

void AddPlayerHair(DWORD PlayerNumber,char* sfile,BYTE transp)
{
	LogWithNumberAndString(&k_fserv,"Player # %d gets hair %s",PlayerNumber,sfile);
	
	/*char tmpFilename[BUFLEN];
	strcpy(tmpFilename,GetPESInfo()->gdbDir);
	strcat(tmpFilename,"GDB\\hair\\");
	strcat(tmpFilename,sfile);
	if (!FileExists(tmpFilename)) {
		Log(&k_fserv,"File doesn't exist, line is ignored!");
		return;
	};*/
	
	DWORD newId=GetIDForHairName(sfile);
	if (newId==0xFFFFFFFF) {
		newId=numHair+1000;
		g_Hair[newId]=new char[strlen(sfile)+1];
		strcpy(g_Hair[newId],sfile);
		numHair++;
		g_HairExists[newId]=false;
	};
	LogWithNumber(&k_fserv,"Assigned hair id is %d",newId);
	g_PlayersHair[PlayerNumber]=newId;
	g_HairTransp[PlayerNumber]=transp;
	return;
};

DWORD GetIDForHairName(char* sfile)
{
	for (g_HairIterator=g_Hair.begin();g_HairIterator!=g_Hair.end();g_HairIterator++) {
		if (g_HairIterator->second != NULL)
			if (strcmp(g_HairIterator->second,sfile)==0)
				return g_HairIterator->first;
	};
	
	return 0xFFFFFFFF;
};

void fservAfsReplace(GETFILEINFO* gfi)
{
	DWORD FaceID = 0, HairID = 0;
	char filename[BUFLEN];
	ZeroMemory(filename, BUFLEN);
	
	THREEDWORDS* threeDWORDs = GetSpecialAfsFileInfo(gfi->fileId);
	if (threeDWORDs == NULL) return;
 	
 	switch (threeDWORDs->dw1) {
 	case AFSSIG_FACEDATA:
 	case AFSSIG_HAIRDATA:
 		//get data from the cache
 		return;
 		break;
 		
 	case AFSSIG_FACE:
		//replace the data for face
		LogWithNumber(&k_fserv,"Processing file id %x", gfi->fileId);
		FaceID = threeDWORDs->dw2;
		LogWithNumber(&k_fserv, "FaceID is %d",FaceID);
		if (g_Faces[FaceID] != NULL) {
			sprintf(filename, "%sGDB\\faces\\%s", GetPESInfo()->gdbDir, g_Faces[FaceID]);
		} else {
			Log(&k_fserv,"FAILED! No file is assigned to this parameter combination!");
			return;
		}
		break;
		
	case AFSSIG_HAIR:
 		//replace the data for hair
		HairID=threeDWORDs->dw2;
		LogWithNumber(&k_fserv,"Processing hair id %d",HairID);
		if (g_Hair[HairID] != NULL) {
			sprintf(filename, "%sGDB\\hair\\%s", GetPESInfo()->gdbDir, g_Hair[HairID]);
		} else {
			Log(&k_fserv,"FAILED! No file is assigned to this parameter combination!");
			return;
		};
		break;
		
	default:
		return;
		break;
	}
	
	//let PES load the first face for skin color 1
	gfi->fileId = fIDs[STARTW] + 0x10000;
	loadReplaceFile(filename);

	return;
}

void fservProcessPlayerData(DWORD ESI, DWORD* PlayerNumber)
{
	DWORD addr=**(DWORD**)(ESI+4);
	BYTE *Skincolor=(BYTE*)(addr+0x31);
	BYTE *Faceset=(BYTE*)(addr+0x33);
	DWORD *FaceID=(DWORD*)(addr+0x40);
	DWORD srcData=((*(BYTE*)(ESI+0x12))*32+*(BYTE*)(ESI+0x11))*0x348+fIDs[PLAYERDATA_BASE];
	
	isInEditPlayerMode=*(BYTE*)fIDs[ISEDITPLAYERMODE];
	isInEditPlayerList=*(BYTE*)fIDs[ISEDITPLAYERLIST];
	WORD editedPlayerNumber=*(WORD*)fIDs[EDITEDPLAYER];
	
	*PlayerNumber=g_PlayersAddr[srcData];
	DWORD usedPlayerNumber=*PlayerNumber;
	
	DWORD newFaceID=0;

	if (isInEditPlayerMode!=0)
		usedPlayerNumber=editedPlayerNumber;
		
	usedPlayerNumber=ResolvePlayerID(usedPlayerNumber);
	
	lastPlayerNumber=usedPlayerNumber;
	
	lastWasFromGDB=false;
	lastFaceID=0xffffffff;
	
	
	g_PlayersIterator=g_Players.find(usedPlayerNumber);

	//If no face is assigned to this player, nothing wrong can happen later
	if (g_PlayersIterator == g_Players.end()) goto ValidFile;
	//check if file exists now
	if (g_FaceExists[g_PlayersIterator->second]) goto ValidFile;
	if (g_Faces[g_PlayersIterator->second]==NULL) goto NoProcessing;

	strcpy(tmpFilename,GetPESInfo()->gdbDir);
	strcat(tmpFilename,"GDB\\faces\\");
	strcat(tmpFilename,g_Faces[g_PlayersIterator->second]);
	if (!FileExists(tmpFilename)) {
		g_Faces[g_PlayersIterator->second]=NULL;
		goto NoProcessing;
	};
	
	//No, it was no good idea to execute this command when the file was NOT found!!!
	g_FaceExists[g_PlayersIterator->second]=true;

	ValidFile:

	//Different behaviour causes crashes in edit mode when trying to edit settings
	//if (isInEditPlayerMode==0) {
		TRACE2X(&k_fserv,"addr for player # %d is %.8x",usedPlayerNumber,addr);
		if (g_PlayersIterator != g_Players.end()) {
			TRACE2(&k_fserv,"Found player in map, assigning face id %d",g_PlayersIterator->second);
			lastFaceID=g_PlayersIterator->second;
			if (*Faceset==FACESET_SPECIAL && g_SpecialFaceHair[usedPlayerNumber]!=0xffffffff)
				g_SpecialFaceHair[usedPlayerNumber]=*FaceID;
			*FaceID=GetNextSpecialAfsFileIdForFace(g_PlayersIterator->second,*Skincolor);
			*Faceset=FACESET_NORMAL;
			lastWasFromGDB=true;
		} else if (*Faceset==FACESET_NORMAL && *FaceID>=1000) {
			TRACE2(&k_fserv,"Assigning face id %d",*FaceID);
			lastFaceID=*FaceID;
			*FaceID=GetNextSpecialAfsFileIdForFace(*FaceID,*Skincolor);
			lastWasFromGDB=true;
		};
	/*	
	} else {
		TRACE2X(&k_fserv,"addr for player # %d (EDIT MODE) is %.8x",usedPlayerNumber,addr);
		if (*Faceset==FACESET_NORMAL && *FaceID>=1000) {
			TRACE2(&k_fserv,"Assigning face id %d",*FaceID);
			lastFaceID=*FaceID;
			*FaceID=GetNextSpecialAfsFileIdForFace(*FaceID,*Skincolor);
			lastWasFromGDB=true;
		};
		
		if (g_PlayersIterator != g_Players.end()) {
			lastFaceID=g_PlayersIterator->second;
			lastWasFromGDB=true;
		};
	};
	*/
	NoProcessing:

	hasChanged=true;
	
	g_PlayersAddr2[addr]=usedPlayerNumber;
	
	TRACEX(&k_fserv,"FaceID is 0x%x, faceset %d, skincolor %d",*FaceID,*Faceset,*Skincolor);
	return;
};


DWORD CalcHairFile(BYTE Caller)
{
	DWORD addr;
	__asm mov addr, eax
	
	bool fromGDB=false;
	BYTE Skincolor=*(BYTE*)(addr+0x31);
	BYTE Faceset=*(BYTE*)(addr+0x33);
	DWORD HairID=*(WORD*)(addr+0x34);
	WORD FaceID=*(WORD*)(addr+0x40);
	
	DWORD res=0;
	DWORD usedPlayerNumber=g_PlayersAddr2[addr];
	bool hadSpecialFace=false;
	lastHairWasFromGDB=false;
	lastHairID=0xffffffff;
	
	if (usedPlayerNumber != 0) {
		TRACE2(&k_fserv,"CalcHairFile for %d",usedPlayerNumber);
		g_PlayersIterator=g_PlayersHair.find(usedPlayerNumber);

		if (g_PlayersIterator == g_PlayersHair.end()) goto ValidFile;
		//check if file exists now
		if (g_HairExists[g_PlayersIterator->second]) goto ValidFile;
		if (g_Hair[g_PlayersIterator->second]==NULL) goto NoProcessing;
	
		strcpy(tmpFilename,GetPESInfo()->gdbDir);
		strcat(tmpFilename,"GDB\\hair\\");
		strcat(tmpFilename,g_Hair[g_PlayersIterator->second]);
		if (!FileExists(tmpFilename)) {
			g_Hair[g_PlayersIterator->second]=NULL;
			goto NoProcessing;
		};
		
		g_HairExists[g_PlayersIterator->second]=true;
	
		ValidFile:

		if (isInEditPlayerMode==0) {
			if (g_PlayersIterator != g_PlayersHair.end()) {
				TRACE2(&k_fserv,"Found player in map, assigning hair id %d",g_PlayersIterator->second);
				HairID=g_PlayersIterator->second;
				fromGDB=true;
			};
			
			if (fromGDB && g_Hair[HairID]==NULL) {
				LogWithTwoNumbers(&k_fserv,"WRONG HAIR ID: %x for player %d",HairID,usedPlayerNumber);
				HairID=0;
			};
			
			lastHairID=HairID;
			lastHairWasFromGDB=fromGDB;
		} else {
			lastHairID;
			if (g_PlayersIterator != g_PlayersHair.end()) {
				lastHairID=g_PlayersIterator->second;
				lastHairWasFromGDB=true;
			};
		};
		
		g_PlayersIterator=g_SpecialFaceHair.find(usedPlayerNumber);
		if (g_PlayersIterator != g_SpecialFaceHair.end()) {
			FaceID=g_PlayersIterator->second;
			hadSpecialFace=true;
		};
		
		NoProcessing:
		//dummy for compiling
		true;
	};

	if (fromGDB) {
		if (HairID==lastGDBHairID)
			res=lastGDBHairRes;
		else {
			res=GetNextSpecialAfsFileId(AFSSIG_HAIR,HairID,0);
			lastGDBHairID=HairID;
			lastGDBHairRes=res;
		};
	} else if (*(DWORD*)(&Caller-4)==fIDs[CALCHAIRID]+5 && !(hadSpecialFace && FaceID==0xffffffff))
		res=((DWORD*)fIDs[HAIRSTARTARRAY])[Skincolor+4*Faceset]+HairID;
	else
		res=((DWORD*)fIDs[HAIRSTARTARRAY])[Skincolor+4]+FaceID;
		
	return res;
};

void PrintPlayerInfo(IDirect3DDevice8* self, CONST RECT* src, CONST RECT* dest, HWND hWnd, LPVOID unused)
{
	isInEditPlayerMode=*(BYTE*)fIDs[ISEDITPLAYERMODE];
	isInEditPlayerList=*(BYTE*)fIDs[ISEDITPLAYERLIST];
	if (isInEditPlayerMode==0 && isInEditPlayerList==0) {
		//KDrawText(450,4,0xffff0000,16,"NOT IN EDIT MODE!");
		return;
	};
	
	char tmp[BUFLEN];
	DWORD color=0xffffffff;

	if (hasChanged) {
		sprintf(tmp,"Player ID: %d",lastPlayerNumber);
		strcpy(lastPlayerNumberString,tmp);
	};
	KDrawText(450,2,color,12,lastPlayerNumberString);
	
	if (hasChanged) {
		if (lastFaceID==0xffffffff || g_Faces[lastFaceID]==NULL) {
			lastWasFromGDB=false;
		} else {
			sprintf(tmp,"Face file: %s",g_Faces[lastFaceID]);
			strcpy(lastFaceFileString,tmp);
		};
		
		if (lastHairID==0xffffffff || g_Hair[lastHairID]==NULL) {
			lastHairWasFromGDB=false;
		} else {
			sprintf(tmp,"Hair file: %s",g_Hair[lastHairID]);
			strcpy(lastHairFileString,tmp);
		};
	};
	hasChanged=false;
	if (lastWasFromGDB)
		KDrawText(450,22,color,12,lastFaceFileString);
		
	if (lastHairWasFromGDB)
		KDrawText(450,42,color,12,lastHairFileString);

	return;
};

void fservCopyPlayerData(DWORD p1, DWORD p2, DWORD p3)
{
	DWORD playerNumber, addr;
	__asm mov playerNumber, ebx
	__asm mov addr, edi
	
	orgCopyPlayerData(p1,p2,p3);
	
	g_PlayersAddr[addr-36]=playerNumber;
	LogWithTwoNumbers(&k_fserv,"addr 0x%x -> player %d",addr-36,playerNumber);
	
	return;
};

void fservReplCopyPlayerData()
{
	DWORD oldEAX, oldESI, oldEDX, oldEBP, team;
	__asm mov oldEAX, eax
	__asm mov oldESI, esi
	__asm mov oldEDX, edx
	__asm mov eax, [ebp]
	__asm mov oldEBP, eax
	LogWithNumber(&k_fserv,"fservReplCopyPlayerData: 0x%x",oldEDX-0x12c);
	
	//replace the functionality of the code we have overwritten
	ZeroMemory((BYTE*)oldESI,16);
	
	team=FIRSTREPLPLAYERID+(((oldEDX-0x12c-fIDs[PLAYERDATA_BASE])<32*0x348)?0:32);
	
	g_PlayersAddr[oldEDX-0x12c]=0; //team+oldEBP;
	
	__asm mov eax, oldEAX
	__asm mov edx, oldEDX
	
	return;
};

DWORD fservEditCopyPlayerData(DWORD playerNumber, DWORD p2)
{
	DWORD result=orgEditCopyPlayerData(playerNumber,p2);

	DWORD srcData=fIDs[EDITPLAYERDATA_BASE]+((*(BYTE*)(result+0x35) & 0xf0)?0x520:0x290);
	DWORD addr=fIDs[PLAYERDATA_BASE]+((*(BYTE*)(srcData+0x12))*32+*(BYTE*)(srcData+0x11))*0x348;
	
	g_PlayersAddr[addr]=playerNumber & 0xffff;
	
	return result;
};

DWORD fservEditCopyPlayerData2(DWORD playerNumber)
{
	DWORD oldEDI;
	__asm mov oldEDI, edi
	g_EditorAddresses[oldEDI-0x1f*4]=playerNumber;
	
	return MasterCallNext(playerNumber);
};

DWORD fservEditCopyPlayerData3(DWORD p1)
{
	DWORD oldEBX, oldESI;
	__asm mov oldEBX, ebx
	__asm mov oldESI, esi
	
	g_EditorAddresses[oldEBX+0x40]=g_EditorAddresses[oldESI];
	
	__asm mov esi, oldESI
	__asm mov ebx, oldEBX
	return MasterCallNext(p1);
};

DWORD fservEditCopyPlayerData4(DWORD slot, DWORD editorAddress, DWORD p3)
{
	DWORD slot1=slot%0x17;
	if (slot==0xff) slot1=1;
	
	DWORD srcData=fIDs[EDITPLAYERDATA_BASE]+slot*0x240;
	DWORD addr=fIDs[PLAYERDATA_BASE]+((*(BYTE*)(srcData+0x12))*32+*(BYTE*)(srcData+0x11))*0x348;
	
	g_PlayersAddr[addr]=g_EditorAddresses[editorAddress] & 0xffff;
	
	return MasterCallNext(slot, editorAddress, p3);
};

DWORD fservEditUniCopyPlayerData(DWORD playerNumber)
{
	DWORD oldEDI;
	__asm mov oldEDI, edi
	g_EditorAddresses[oldEDI-0x1f*4]=0;
	
	return MasterCallNext(playerNumber);
};

DWORD fservMyTeamCPD(DWORD playerNumber)
{
	DWORD res=MasterCallNext(playerNumber);
	g_PlayersAddr[fIDs[PLAYERDATA_BASE]]=playerNumber & 0xffff;
	return res;
};

BYTE fservGetHairTransp(DWORD addr)
{
	DWORD usedPlayerNumber=g_PlayersAddr2[addr];
	BYTE *Faceset=(BYTE*)(addr+0x33);
	DWORD *FaceID=(DWORD*)(addr+0x40);
	
	bool hadSpecialFace=false;
	BYTE ourFaceset=*Faceset;
	DWORD ourFaceID=*FaceID;
	
	
	g_PlayersIterator=g_SpecialFaceHair.find(usedPlayerNumber);
	if (g_PlayersIterator != g_SpecialFaceHair.end() && g_PlayersIterator->second!=0xffffffff) {
		//set back to special face to get the right transparency for it
		*FaceID = g_PlayersIterator->second;
		*Faceset = FACESET_SPECIAL;
		hadSpecialFace=true;
	};
	
	
	BYTE result=orgGetHairTransp(addr);
	
	
	if (hadSpecialFace) {
		//and set back to our settings
		*Faceset=ourFaceset;
		*FaceID=ourFaceID;
	};
	
	if (usedPlayerNumber != 0) // && isInEditPlayerMode==0)
		if (g_PlayersHair.find(usedPlayerNumber) != g_PlayersHair.end())
			result=g_HairTransp[usedPlayerNumber];
	
	return result;
};

DWORD GetNextSpecialAfsFileIdForFace(DWORD FaceID, BYTE Skincolor)
{
	DWORD result=GetNextSpecialAfsFileId(AFSSIG_FACE,FaceID,0);
	result-=StartsStdFaces[Skincolor];
	result-=0x10000;
	
	return result;
};

void fservConnectAddrToId(DWORD addr, DWORD id)
{
	Log(&k_fserv,"fservConnectAddrToId");
	g_PlayersAddr[addr]=id;
	return;
};
                                                                      
DWORD ResolvePlayerID(DWORD playerID)
{
	DWORD team, teamID, teamAddr;
	if (playerID>=randSelIDs[RANDSEL_PLID1] && playerID<randSelIDs[RANDSEL_PLID1]+0x40) {
		//Random Selection Mode
		team=(playerID<randSelIDs[RANDSEL_PLID2])?0:1;
		teamID=randSelIDs[RANDSEL_TEAM1+team];
		teamAddr=randSelPlayersAddr(teamID)+0x10f4;
		return ((WORD*)teamAddr)[playerID-randSelIDs[RANDSEL_PLID1+team]];
	};
	return playerID;
};
