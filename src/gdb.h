#ifndef __JUCE_GDB__
#define __JUCE_GDB__

#include <windows.h>
#include <map>
#include <hash_map>
#include <string>

using namespace std;

#define MAXFILENAME 2048

// attribute definition flags (bits)
#define SHIRT_NUMBER  0x01 
#define SHIRT_NAME    0x02 
#define SHORTS_NUMBER 0x04 
#define COLLAR        0x08 
#define MODEL         0x10 
#define CUFF          0x20 
#define SHORTS_NUMBER_LOCATION 0x40 
#define NUMBER_TYPE   0x80
#define NAME_TYPE     0x100
#define NAME_SHAPE    0x200
#define NUMBERS_FILE  0x400
#define NUMBERS_PALETTE_FILE 0x800
#define NAME_LOCATION 0x1000
#define LOGO_LOCATION 0x2000
#define RADAR_COLOR   0x4000
#define MASK_FILE     0x8000
#define KITDESCRIPTION   0x10000
#define SHIRT_NUMBER_LOCATION 0x20000
#define SHORTS_MAIN_COLOR 0x40000

// GDB data structures
///////////////////////////////

typedef struct _RGBAColor {
    BYTE r;
    BYTE g;
    BYTE b;
    BYTE a;
} RGBAColor;

typedef map<string,char*> StringCStrMap;

typedef struct _Kit {
    char foldername[MAXFILENAME];
    char description[MAXFILENAME];
    RGBAColor shirtNumber;
    RGBAColor shirtName;
    RGBAColor shortsNumber;
    BYTE collar;
    BYTE model;
    BYTE cuff;
    BYTE shirtNumberLocation;
    BYTE shortsNumberLocation;
    BYTE numberType;
    BYTE nameLocation;
    BYTE nameType;
    BYTE nameShape;
    BYTE logoLocation;
    RGBAColor radarColor;
    RGBAColor shortsMainColor;
    char maskFile[MAXFILENAME];
    char numbersFile[MAXFILENAME];
    char shirtPaletteFile[MAXFILENAME];
    StringCStrMap* shortsPaletteFiles;
    DWORD attDefined;
} Kit;

typedef map<string,Kit*> StringKitMap;

typedef struct _KitCollection {
    char foldername[MAXFILENAME];
    StringKitMap* players;
    StringKitMap* goalkeepers;
    BOOL editable;
    BOOL loaded;
} KitCollection;

typedef hash_map<WORD,KitCollection*> WordKitCollectionMap;

typedef struct _GDB {
    char dir[MAXFILENAME];
    WordKitCollectionMap* uni;
} GDB;

// GDB functions
//////////////////////////////

GDB* gdbLoad(char* dir);
void gdbUnload(GDB* gdb);
void gdbFindKitsForTeam(GDB* gdb, WORD teamId);
void gdbLoadConfig(GDB* gdb, Kit* kit);

#endif
