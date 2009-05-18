// afs2fs.h

#define MODID 301
#define NAMELONG "Afs2fs Module 6.6.3"
#define NAMESHORT "AFS2FS"
#define DEFAULT_DEBUG 1

typedef struct _BIN_SIZE_INFO
{
    DWORD unknown1;
    DWORD structSize;
    DWORD numItems;
    WORD numItems2;
    WORD unknown2;
    char relativePathName[0x108];
    DWORD unknown3;
    DWORD sizes[1];
} BIN_SIZE_INFO;

