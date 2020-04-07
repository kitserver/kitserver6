// ADDRESSES for afs2fs.cpp

#define CODELEN 1
enum { DUMMY };
static DWORD codeArray[][CODELEN] = {
	// PES6
    {0},
    // PES6 1.10
    {0},
    // WE2007
    {0},
};

#define DATALEN 1
enum {
    BIN_SIZES_TABLE,
};
static DWORD dtaArray[][DATALEN] = {
    // PES6
    {
        0x3b5cbc0,
    },
    // PES6 1.10
    {
        0x3b5dbc0,
    },
	// WE2007
    {
        0x3b57640,
    },
};

DWORD code[CODELEN];
DWORD dta[DATALEN];
