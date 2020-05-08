// dxtools.h

#define MODID 105
#define NAMELONG "DXTools 6.8.2.1"
#define NAMESHORT "DXTOOLS"

#define DEFAULT_DEBUG 1

#define BUFLEN 4096

typedef struct _DIMENSIONS {
    int width;
    int height;
} DIMENSIONS;

typedef struct _DXCONFIG {
    DIMENSIONS window;
    UINT window_x;
    UINT window_y;
    UINT window_style;
    UINT window_exstyle;
    DIMENSIONS fullscreen;
    DIMENSIONS internal;
} DXCONFIG;

#define DEFAULT_WINDOW_WIDTH 0
#define DEFAULT_WINDOW_HEIGHT 0
#define DEFAULT_WINDOW_X 0xFFFFFFFF
#define DEFAULT_WINDOW_Y 0xFFFFFFFF
#define DEFAULT_FULLSCREEN_WIDTH 0
#define DEFAULT_FULLSCREEN_HEIGHT 0
#define DEFAULT_INTERNAL_WIDTH 0
#define DEFAULT_INTERNAL_HEIGHT 0

