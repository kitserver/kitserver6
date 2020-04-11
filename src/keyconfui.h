// keybindui.h

#ifndef _KEYBINDUI_H
#define _KEYBINDUI_H

#include <windows.h>

#define WIN_WIDTH 475 
#define WIN_HEIGHT 275
#define BUFLEN 2048

extern HWND g_keySwitchLeftControl;
extern HWND g_keySwitchRightControl;
extern HWND g_keyResetControl;
extern HWND g_keyRandomControl;
extern HWND g_keyPrevControl;
extern HWND g_keyNextControl;
extern HWND g_keyPrevValControl;
extern HWND g_keyNextValControl;
extern HWND g_keyInfoPagePrevControl;
extern HWND g_keyInfoPageNextControl;
extern HWND g_keyAction1Control;
extern HWND g_keyAction2Control;
extern HWND g_keySwitch1Control;
extern HWND g_keySwitch2Control;

extern HWND g_statusTextControl;           // displays status messages
extern HWND g_restoreButtonControl;        // restore settings button
extern HWND g_saveButtonControl;           // save settings button

// macros
#define VKEY_TEXT(key, buffer, buflen) \
	ZeroMemory(buffer, buflen); \
	GetKeyNameText(MapVirtualKey(key, 0) << 16, buffer, buflen)

// functions
bool BuildControls(HWND parent);

#endif
