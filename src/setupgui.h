// setupgui.h

#ifndef _JUCE_SETUPGUI
#define _JUCE_SETUPGUI

#include <windows.h>

#define WIN_WIDTH 470 
#define WIN_HEIGHT 135

extern HWND g_exeListControl;              // displays list of executable files
//extern HWND g_osListControl;               // displays list of OS choices
extern HWND g_exeInfoControl;              // displays info about current executable file

extern HWND g_installButtonControl;        // restore settings button
extern HWND g_removeButtonControl;         // save settings button

extern HWND g_statusTextControl;           // displays status messages

// functions
bool BuildControls(HWND parent);

#endif
