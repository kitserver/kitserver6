#ifndef _FSERV_CONFIG
#define _FSEV_CONFIG

#include <windows.h>

#define BUFLEN 4096

#define CONFIG_FILE "fserv.cfg"

#define DEFAULT_DUMP_TEXTURES false
#define DEFAULT_NPOT_TEXTURES false
#define DEFAULT_HD_FACE_MAX_WIDTH 512
#define DEFAULT_HD_FACE_MAX_HEIGHT 1024
#define DEFAULT_HD_HAIR_MAX_WIDTH 1024
#define DEFAULT_HD_HAIR_MAX_HEIGHT 512
#define HD_FACE_MIN_WIDTH 64
#define HD_FACE_MIN_HEIGHT 128
#define HD_HAIR_MIN_WIDTH 128
#define HD_HAIR_MIN_HEIGHT 64

typedef struct _FSERV_CONFIG_STRUCT {
    bool   dump_textures;
    bool   npot_textures;
    UINT   hd_face_max_width;
    UINT   hd_face_max_height;
    UINT   hd_hair_max_width;
    UINT   hd_hair_max_height;
} FSERV_CONFIG;

BOOL ReadConfig(FSERV_CONFIG* config, char* cfgFile);

#endif
