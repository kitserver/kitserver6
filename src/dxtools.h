// dxtools.h

#define MODID 107
#define NAMELONG "DXTools 1.0.2"
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
} DXCONFIG;

#define DEFAULT_WINDOW_WIDTH 0
#define DEFAULT_WINDOW_HEIGHT 0
#define DEFAULT_FULLSCREEN_WIDTH 0
#define DEFAULT_FULLSCREEN_HEIGHT 0

