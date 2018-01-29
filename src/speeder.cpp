/* <speeder>
 *
 */

#include <windows.h>
#include <stdio.h>
#include <sys/stat.h>
#include "kload_exp.h"
#include "speeder.h"
#include "apihijack.h"

#define lang(s) getTransl("speeder",s)

#define SWAPBYTES(dw) \
    ((dw<<24 & 0xff000000) | (dw<<8  & 0x00ff0000) | \
    (dw>>8  & 0x0000ff00) | (dw>>24 & 0x000000ff))

#define round(x) ((abs((x)-(int)(x))<0.5)?(int)(x):(int)((x)+1))

#define FLOAT_ZERO 0.0001f
#define DBG(x) {if (k_speed.debug) x;}
#define BUFLEN 1024

// VARIABLES
HINSTANCE hInst = NULL;
KMOD k_speed = {MODID, NAMELONG, NAMESHORT, DEFAULT_DEBUG};

// GLOBALS

class config_t 
{
public:
    config_t() : count_factor(0.9), debug(false) {}
    float count_factor;
    bool debug;
};

config_t _speeder_config;

// FUNCTIONS
void initModule();
bool readConfig(config_t& config);

KEXPORT BOOL WINAPI Override_QueryPerformanceFrequency(
        LARGE_INTEGER *lpPerformanceFrequency);


/*******************/
/* DLL Entry Point */
/*******************/
EXTERN_C BOOL WINAPI DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID lpReserved)
{
	if (dwReason == DLL_PROCESS_ATTACH)
	{
		hInst = hInstance;
		RegisterKModule(&k_speed);
		//HookFunction(hk_D3D_CreateDevice, (DWORD)initModule);
		HookFunction(hk_D3D_Create, (DWORD)initModule);
	}
	
	else if (dwReason == DLL_PROCESS_DETACH)
	{
	}
	
	return true;
}

void initModule()
{
	UnhookFunction(hk_D3D_Create, (DWORD)initModule);

    // read configuration
    readConfig(_speeder_config);

    if (_speeder_config.count_factor >= 0.0001)
    {
       SDLLHook Kernel32Hook = 
       {
          "KERNEL32.DLL",
          false, NULL,		// Default hook disabled, NULL function pointer.
          {
              { "QueryPerformanceFrequency", 
                  Override_QueryPerformanceFrequency },
              { NULL, NULL }
          }
       };
       HookAPICalls( &Kernel32Hook );
    }
    LogWithDouble(&k_speed, "count.factor = %0.2f", 
            (double)_speeder_config.count_factor);
    Log(&k_speed, "module initialized.");
}

/**
 * Returns true if successful.
 */
bool readConfig(config_t& config)
{
    string cfgFile(GetPESInfo()->mydir);
    cfgFile += "\\speeder.cfg";

	FILE* cfg = fopen(cfgFile.c_str(), "rt");
	if (cfg == NULL) return false;

	char str[BUFLEN];
	char name[BUFLEN];
	int value = 0;
	float dvalue = 0.0f;

	char *pName = NULL, *pValue = NULL, *comment = NULL;
	while (true)
	{
		ZeroMemory(str, BUFLEN);
		fgets(str, BUFLEN-1, cfg);
		if (feof(cfg)) break;

		// skip comments
		comment = strstr(str, "#");
		if (comment != NULL) comment[0] = '\0';

		// parse the line
		pName = pValue = NULL;
		ZeroMemory(name, BUFLEN); value = 0;
		char* eq = strstr(str, "=");
		if (eq == NULL || eq[1] == '\0') continue;

		eq[0] = '\0';
		pName = str; pValue = eq + 1;

		ZeroMemory(name, NULL); 
		sscanf(pName, "%s", name);

		if (strcmp(name, "debug")==0)
		{
			if (sscanf(pValue, "%d", &value)!=1) continue;
			config.debug = value;
		}
        else if (strcmp(name, "count.factor")==0)
		{
            float value = 1.0;
			if (sscanf(pValue, "%f", &value)!=1) continue;
			config.count_factor = min(max(value,0.1),2.5);
		}
	}
	fclose(cfg);
	return true;
}

KEXPORT BOOL WINAPI Override_QueryPerformanceFrequency(LARGE_INTEGER *lpPerformanceFrequency)
{
    LARGE_INTEGER metric;
    BOOL result = QueryPerformanceFrequency(&metric);
    LOG(&k_speed, 
            "(old) hi=%08x, lo=%08x", metric.HighPart, metric.LowPart);
    if (fabs(_speeder_config.count_factor-1.0)>FLOAT_ZERO)
    {
        LOG(&k_speed, "Changing frequency");
        metric.HighPart /= _speeder_config.count_factor;
        metric.LowPart /= _speeder_config.count_factor;
    }
    LOG(&k_speed, 
            "(new) hi=%08x, lo=%08x", metric.HighPart, metric.LowPart);
    *lpPerformanceFrequency = metric;
    return result;
}

