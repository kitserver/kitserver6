// dxtools.h

#define MODID 107
#define NAMELONG "DXTools 6.7.0"
#define NAMESHORT "DXTOOLS"

#define DEFAULT_DEBUG 1

#define BUFLEN 4096

typedef struct _DIMENSIONS {
    int width;
    int height;
} DIMENSIONS;

typedef struct _DXCONFIG {
    DIMENSIONS window;
    DIMENSIONS fullscreen;
    DIMENSIONS internal;
} DXCONFIG;

#define DEFAULT_WINDOW_WIDTH 0
#define DEFAULT_WINDOW_HEIGHT 0
#define DEFAULT_FULLSCREEN_WIDTH 0
#define DEFAULT_FULLSCREEN_HEIGHT 0
#define DEFAULT_INTERNAL_WIDTH 0
#define DEFAULT_INTERNAL_HEIGHT 0

