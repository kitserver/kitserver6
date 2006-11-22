#include <windows.h>
#include <stdio.h>
#include "kserv.h"
#include "shared.h"
#include "setupgui.h"

HWND g_exeListControl;              // displays list of executable files
//HWND g_osListControl;               // displays list of OS choices
HWND g_exeInfoControl;              // displays info about current executable file

HWND g_installButtonControl;        // restore settings button
HWND g_removeButtonControl;         // save settings button

HWND g_statusTextControl;           // displays status messages

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

	// TOP SECTION

	borW = WIN_WIDTH - spacer*3;
	statW = 120;
	boxW = borW - statW - spacer*3; boxH = 20; statH = 16;
	borH = spacer*3 + boxH*2;

	HWND staticBorderTopControl = CreateWindowEx(
			xstyle, "Static", "", WS_CHILD | WS_VISIBLE | SS_ETCHEDFRAME,
			x, y, borW, borH,
			parent, NULL, NULL, NULL);

	x += spacer; y += spacer;

	HWND topLabel = CreateWindowEx(
			xstyle, "Static", "Game executable:", style,
			x, y+4, statW, statH, 
			parent, NULL, NULL, NULL);

	x += statW + spacer;
	boxW = borW - spacer*3 - statW;

	style = WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL;

	g_exeListControl = CreateWindowEx(
			xstyle, "ComboBox", "", style | WS_TABSTOP,
			x, y, boxW, boxH * 6,
			parent, NULL, NULL, NULL);

	x = spacer*2;
	y += boxH + spacer;

	style = WS_CHILD | WS_VISIBLE;

    /*
	HWND osLabel = CreateWindowEx(
			xstyle, "Static", "Operating system:", style,
			x, y+4, statW, statH, 
			parent, NULL, NULL, NULL);

	x += statW + spacer;
	boxW = borW - spacer*3 - statW;

	style = WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL;

	g_osListControl = CreateWindowEx(
			xstyle, "ComboBox", "", style | WS_TABSTOP,
			x, y, boxW, boxH * 6,
			parent, NULL, NULL, NULL);

	x = spacer*2;
	y += boxH + spacer;
    */

	style = WS_CHILD | WS_VISIBLE;

	HWND infoLabel = CreateWindowEx(
			xstyle, "Static", "Current state:", style,
			x, y+4, statW, statH, 
			parent, NULL, NULL, NULL);

	x += statW + spacer;

	g_exeInfoControl = CreateWindowEx(
			xstyle, "Static", "Information unavailable", style,
			x, y+4, boxW, boxH,
			parent, NULL, NULL, NULL);

	x = spacer*2;
	y += boxH + spacer*2;

	// BOTTOM SECTION - buttons

	butW = 60; butH = 24;
	x = WIN_WIDTH - spacer*2 - butW;

	g_removeButtonControl = CreateWindowEx(
			xstyle, "Button", "Remove", style | WS_TABSTOP,
			x, y, butW, butH,
			parent, NULL, NULL, NULL);

	butW = 60;
	x -= butW + spacer;

	g_installButtonControl = CreateWindowEx(
			xstyle, "Button", "Install", style | WS_TABSTOP,
			x, y, butW, butH,
			parent, NULL, NULL, NULL);

	x = spacer;
	statW = WIN_WIDTH - spacer*4 - 160;

	g_statusTextControl = CreateWindowEx(
			xstyle, "Static", "", style,
			x, y+6, statW, statH,
			parent, NULL, NULL, NULL);

	// If any control wasn't created, return false
	if (topLabel == NULL ||
		g_exeListControl == NULL ||
		//osLabel == NULL ||
		//g_osListControl == NULL ||
		infoLabel == NULL ||
		g_exeInfoControl == NULL ||
		g_statusTextControl == NULL ||
		g_installButtonControl == NULL ||
		g_removeButtonControl == NULL)
		return false;

	// Get a handle to the stock font object
	hObj = GetStockObject(DEFAULT_GUI_FONT);

	SendMessage(topLabel, WM_SETFONT, (WPARAM)hObj, true);
	SendMessage(g_exeListControl, WM_SETFONT, (WPARAM)hObj, true);
	//SendMessage(osLabel, WM_SETFONT, (WPARAM)hObj, true);
	//SendMessage(g_osListControl, WM_SETFONT, (WPARAM)hObj, true);
	SendMessage(infoLabel, WM_SETFONT, (WPARAM)hObj, true);
	SendMessage(g_exeInfoControl, WM_SETFONT, (WPARAM)hObj, true);

	SendMessage(g_statusTextControl, WM_SETFONT, (WPARAM)hObj, true);
	SendMessage(g_installButtonControl, WM_SETFONT, (WPARAM)hObj, true);
	SendMessage(g_removeButtonControl, WM_SETFONT, (WPARAM)hObj, true);

	// disable the dropdown list and the buttons by default
	EnableWindow(g_installButtonControl, FALSE);
	EnableWindow(g_removeButtonControl, FALSE);
	EnableWindow(g_exeListControl, FALSE);
	//EnableWindow(g_osListControl, FALSE);

	return true;
}

