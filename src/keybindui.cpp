#include <windows.h>
#include <stdio.h>
#include "keybindui.h"
#include "keycfg.h"

HWND g_keySwitchLeftControl;
HWND g_keySwitchRightControl;
HWND g_keyResetControl;
HWND g_keyRandomControl;
HWND g_keyPrevControl;
HWND g_keyNextControl;
HWND g_keyPrevValControl;
HWND g_keyNextValControl;
HWND g_keyInfoPagePrevControl;
HWND g_keyInfoPageNextControl;
HWND g_keyAction1Control;
HWND g_keyAction2Control;
HWND g_keySwitch1Control;
HWND g_keySwitch2Control;

HWND g_statusTextControl;           // displays status messages
HWND g_restoreButtonControl;        // restore settings button
HWND g_saveButtonControl;           // save settings button


/**
 * build all controls
 */
bool BuildControls(HWND parent)
{
	HGDIOBJ hObj;
	DWORD style, xstyle;
	int x, y, spacer;
	int boxW, boxH, statW, statH, borW, borH, butW, butH;

	spacer = 6; 
	x = spacer, y = spacer;
	butH = 20;

	// use default extended style
	xstyle = WS_EX_LEFT;
	style = WS_CHILD | WS_VISIBLE;

	char keyName[BUFLEN];

	// TOP SECTION: key bindings

	statW = 130; boxW = 70;
    statH = boxH = butH;

    borW = statW + spacer*3 + boxW;
    borH = statH*7 + spacer*8;

	HWND staticBorderTopControl = CreateWindowEx(
			xstyle, "Static", "", WS_CHILD | WS_VISIBLE | SS_ETCHEDFRAME,
			x, y, borW, borH,
			parent, NULL, NULL, NULL);

    x = spacer*2 + borW;
	HWND staticBorderTopControl2 = CreateWindowEx(
			xstyle, "Static", "", WS_CHILD | WS_VISIBLE | SS_ETCHEDFRAME,
			x, y, borW + spacer, borH,
			parent, NULL, NULL, NULL);

    y += spacer;

    /////////////////////////////////////////////////////////////////

	x = spacer*2; 

	HWND keySwitchLeftLabel = CreateWindowEx(
			xstyle, "Static", "Switch Home Kit", style,
			x, y+4, statW, statH,
			parent, NULL, NULL, NULL);

	x += statW + spacer;

	VKEY_TEXT(KEYCFG_KEYBOARD_DEFAULT_SWITCH_LEFT, keyName, BUFLEN);

	g_keySwitchLeftControl = CreateWindowEx(
			xstyle | WS_EX_CLIENTEDGE, "Edit", keyName, style,
			x, y, boxW, boxH,
			parent, NULL, NULL, NULL);

    /////////////////////////////////////////////////////////////////

	x = spacer*3 + borW;

	VKEY_TEXT(KEYCFG_KEYBOARD_DEFAULT_SWITCH_RIGHT, keyName, BUFLEN);

	g_keySwitchRightControl = CreateWindowEx(
			xstyle | WS_EX_CLIENTEDGE, "Edit", keyName, style,
			x, y, boxW, boxH,
			parent, NULL, NULL, NULL);

	x += boxW + spacer*2;

	HWND keySwitchRightLabel = CreateWindowEx(
			xstyle, "Static", "Switch Away Kit", style,
			x, y+4, statW, statH,
			parent, NULL, NULL, NULL);

    y += statH + spacer;

    /////////////////////////////////////////////////////////////////
    
   	x = spacer*2;

	HWND keyResetLabel = CreateWindowEx(
			xstyle, "Static", "Reset", style,
			x, y+4, statW, statH,
			parent, NULL, NULL, NULL);

	x += statW + spacer;

	VKEY_TEXT(KEYCFG_KEYBOARD_DEFAULT_RESET, keyName, BUFLEN);

	g_keyResetControl = CreateWindowEx(
			xstyle | WS_EX_CLIENTEDGE, "Edit", keyName, style,
			x, y, boxW, boxH,
			parent, NULL, NULL, NULL);

    /////////////////////////////////////////////////////////////////
    
	x = spacer*3 + borW;

	VKEY_TEXT(KEYCFG_KEYBOARD_DEFAULT_RANDOM, keyName, BUFLEN);

	g_keyRandomControl = CreateWindowEx(
			xstyle | WS_EX_CLIENTEDGE, "Edit", keyName, style,
			x, y, boxW, boxH,
			parent, NULL, NULL, NULL);

	x += boxW + spacer*2;

	HWND keyRandomLabel = CreateWindowEx(
			xstyle, "Static", "Random", style,
			x, y+4, statW, statH,
			parent, NULL, NULL, NULL);

    y += statH + spacer;

    /////////////////////////////////////////////////////////////////
     
   	x = spacer*2;

	HWND keyPrevLabel = CreateWindowEx(
			xstyle, "Static", "Previous", style,
			x, y+4, statW, statH,
			parent, NULL, NULL, NULL);

	x += statW + spacer;

	VKEY_TEXT(KEYCFG_KEYBOARD_DEFAULT_PREV, keyName, BUFLEN);

	g_keyPrevControl = CreateWindowEx(
			xstyle | WS_EX_CLIENTEDGE, "Edit", keyName, style,
			x, y, boxW, boxH,
			parent, NULL, NULL, NULL);

    /////////////////////////////////////////////////////////////////
      
	x = spacer*3 + borW;

	VKEY_TEXT(KEYCFG_KEYBOARD_DEFAULT_NEXT, keyName, BUFLEN);

	g_keyNextControl = CreateWindowEx(
			xstyle | WS_EX_CLIENTEDGE, "Edit", keyName, style,
			x, y, boxW, boxH,
			parent, NULL, NULL, NULL);

	x += boxW + spacer*2;

	HWND keyNextLabel = CreateWindowEx(
			xstyle, "Static", "Next", style,
			x, y+4, statW, statH,
			parent, NULL, NULL, NULL);

    y += statH + spacer;

    /////////////////////////////////////////////////////////////////
       
   	x = spacer*2;

	HWND keyPrevValLabel = CreateWindowEx(
			xstyle, "Static", "Previous value", style,
			x, y+4, statW, statH,
			parent, NULL, NULL, NULL);

	x += statW + spacer;

	VKEY_TEXT(KEYCFG_KEYBOARD_DEFAULT_PREVVAL, keyName, BUFLEN);

	g_keyPrevValControl = CreateWindowEx(
			xstyle | WS_EX_CLIENTEDGE, "Edit", keyName, style,
			x, y, boxW, boxH,
			parent, NULL, NULL, NULL);

    /////////////////////////////////////////////////////////////////
       
	x = spacer*3 + borW;

	VKEY_TEXT(KEYCFG_KEYBOARD_DEFAULT_NEXTVAL, keyName, BUFLEN);

	g_keyNextValControl = CreateWindowEx(
			xstyle | WS_EX_CLIENTEDGE, "Edit", keyName, style,
			x, y, boxW, boxH,
			parent, NULL, NULL, NULL);

	x += boxW + spacer*2;

	HWND keyNextValLabel = CreateWindowEx(
			xstyle, "Static", "Next value", style,
			x, y+4, statW, statH,
			parent, NULL, NULL, NULL);

    y += statH + spacer;

    /////////////////////////////////////////////////////////////////
 
   	x = spacer*2;

	HWND keyInfoPagePrevLabel = CreateWindowEx(
			xstyle, "Static", "Info-page previous", style,
			x, y+4, statW, statH,
			parent, NULL, NULL, NULL);

	x += statW + spacer;

	VKEY_TEXT(KEYCFG_KEYBOARD_DEFAULT_INFOPAGEPREV, keyName, BUFLEN);

	g_keyInfoPagePrevControl = CreateWindowEx(
			xstyle | WS_EX_CLIENTEDGE, "Edit", keyName, style,
			x, y, boxW, boxH,
			parent, NULL, NULL, NULL);

    /////////////////////////////////////////////////////////////////
  
	x = spacer*3 + borW;

	VKEY_TEXT(KEYCFG_KEYBOARD_DEFAULT_INFOPAGENEXT, keyName, BUFLEN);

	g_keyInfoPageNextControl = CreateWindowEx(
			xstyle | WS_EX_CLIENTEDGE, "Edit", keyName, style,
			x, y, boxW, boxH,
			parent, NULL, NULL, NULL);

	x += boxW + spacer*2;

	HWND keyInfoPageNextLabel = CreateWindowEx(
			xstyle, "Static", "Info-page next", style,
			x, y+4, statW, statH,
			parent, NULL, NULL, NULL);

    y += statH + spacer;

    /////////////////////////////////////////////////////////////////
 
   	x = spacer*2;

	HWND keyAction1Label = CreateWindowEx(
			xstyle, "Static", "Action 1", style,
			x, y+4, statW, statH,
			parent, NULL, NULL, NULL);

	x += statW + spacer;

	VKEY_TEXT(KEYCFG_KEYBOARD_DEFAULT_ACTION1, keyName, BUFLEN);

	g_keyAction1Control = CreateWindowEx(
			xstyle | WS_EX_CLIENTEDGE, "Edit", keyName, style,
			x, y, boxW, boxH,
			parent, NULL, NULL, NULL);

    /////////////////////////////////////////////////////////////////
  
	x = spacer*3 + borW;

	VKEY_TEXT(KEYCFG_KEYBOARD_DEFAULT_ACTION2, keyName, BUFLEN);

	g_keyAction2Control = CreateWindowEx(
			xstyle | WS_EX_CLIENTEDGE, "Edit", keyName, style,
			x, y, boxW, boxH,
			parent, NULL, NULL, NULL);

	x += boxW + spacer*2;

	HWND keyAction2Label = CreateWindowEx(
			xstyle, "Static", "Action 2", style,
			x, y+4, statW, statH,
			parent, NULL, NULL, NULL);

    y += statH + spacer;

    /////////////////////////////////////////////////////////////////
 
   	x = spacer*2;

	HWND keySwitch1Label = CreateWindowEx(
			xstyle, "Static", "Switch 1", style,
			x, y+4, statW, statH,
			parent, NULL, NULL, NULL);

	x += statW + spacer;

	VKEY_TEXT(KEYCFG_KEYBOARD_DEFAULT_SWITCH1, keyName, BUFLEN);

	g_keySwitch1Control = CreateWindowEx(
			xstyle | WS_EX_CLIENTEDGE, "Edit", keyName, style,
			x, y, boxW, boxH,
			parent, NULL, NULL, NULL);

    /////////////////////////////////////////////////////////////////
  
	x = spacer*3 + borW;

	VKEY_TEXT(KEYCFG_KEYBOARD_DEFAULT_SWITCH2, keyName, BUFLEN);

	g_keySwitch2Control = CreateWindowEx(
			xstyle | WS_EX_CLIENTEDGE, "Edit", keyName, style,
			x, y, boxW, boxH,
			parent, NULL, NULL, NULL);

	x += boxW + spacer*2;

	HWND keySwitch2Label = CreateWindowEx(
			xstyle, "Static", "Switch 2", style,
			x, y+4, statW, statH,
			parent, NULL, NULL, NULL);

    y += statH + spacer;

    /////////////////////////////////////////////////////////////////

	y += spacer*2;
	x = spacer;

	// BOTTOM SECTION - buttons

	butW = 60; butH = 24;
	x = WIN_WIDTH - spacer*2 - butW;

	g_saveButtonControl = CreateWindowEx(
			xstyle, "Button", "Save", style | WS_TABSTOP,
			x, y, butW, butH,
			parent, NULL, NULL, NULL);

	butW = 60;
	x -= butW + spacer;

	g_restoreButtonControl = CreateWindowEx(
			xstyle, "Button", "Restore", style | WS_TABSTOP,
			x, y, butW, butH,
			parent, NULL, NULL, NULL);

	x = spacer;
	statW = WIN_WIDTH - spacer*4 - 160;

	g_statusTextControl = CreateWindowEx(
			xstyle, "Static", "", style,
			x, y+6, statW, statH,
			parent, NULL, NULL, NULL);

	// Get a handle to the stock font object
	hObj = GetStockObject(DEFAULT_GUI_FONT);

	SendMessage(keySwitchLeftLabel, WM_SETFONT, (WPARAM)hObj, true);
	SendMessage(g_keySwitchLeftControl, WM_SETFONT, (WPARAM)hObj, true);
	SendMessage(keySwitchRightLabel, WM_SETFONT, (WPARAM)hObj, true);
	SendMessage(g_keySwitchRightControl, WM_SETFONT, (WPARAM)hObj, true);
	SendMessage(keyResetLabel, WM_SETFONT, (WPARAM)hObj, true);
	SendMessage(g_keyResetControl, WM_SETFONT, (WPARAM)hObj, true);
	SendMessage(keyRandomLabel, WM_SETFONT, (WPARAM)hObj, true);
	SendMessage(g_keyRandomControl, WM_SETFONT, (WPARAM)hObj, true);
	SendMessage(keyPrevLabel, WM_SETFONT, (WPARAM)hObj, true);
	SendMessage(g_keyPrevControl, WM_SETFONT, (WPARAM)hObj, true);
	SendMessage(keyNextLabel, WM_SETFONT, (WPARAM)hObj, true);
	SendMessage(g_keyNextControl, WM_SETFONT, (WPARAM)hObj, true);
	SendMessage(keyPrevValLabel, WM_SETFONT, (WPARAM)hObj, true);
	SendMessage(g_keyPrevValControl, WM_SETFONT, (WPARAM)hObj, true);
	SendMessage(keyNextValLabel, WM_SETFONT, (WPARAM)hObj, true);
	SendMessage(g_keyNextValControl, WM_SETFONT, (WPARAM)hObj, true);
	SendMessage(keyInfoPagePrevLabel, WM_SETFONT, (WPARAM)hObj, true);
	SendMessage(g_keyInfoPagePrevControl, WM_SETFONT, (WPARAM)hObj, true);
	SendMessage(keyInfoPageNextLabel, WM_SETFONT, (WPARAM)hObj, true);
	SendMessage(g_keyInfoPageNextControl, WM_SETFONT, (WPARAM)hObj, true);
	SendMessage(keyAction1Label, WM_SETFONT, (WPARAM)hObj, true);
	SendMessage(g_keyAction1Control, WM_SETFONT, (WPARAM)hObj, true);
	SendMessage(keyAction2Label, WM_SETFONT, (WPARAM)hObj, true);
	SendMessage(g_keyAction2Control, WM_SETFONT, (WPARAM)hObj, true);
	SendMessage(keySwitch1Label, WM_SETFONT, (WPARAM)hObj, true);
	SendMessage(g_keySwitch1Control, WM_SETFONT, (WPARAM)hObj, true);
	SendMessage(keySwitch2Label, WM_SETFONT, (WPARAM)hObj, true);
	SendMessage(g_keySwitch2Control, WM_SETFONT, (WPARAM)hObj, true);

	SendMessage(g_statusTextControl, WM_SETFONT, (WPARAM)hObj, true);
	SendMessage(g_restoreButtonControl, WM_SETFONT, (WPARAM)hObj, true);
	SendMessage(g_saveButtonControl, WM_SETFONT, (WPARAM)hObj, true);

	return true;
}

