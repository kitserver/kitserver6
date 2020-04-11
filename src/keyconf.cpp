/* KitServer Key Binder */
/* Version 1.0 (with Win32 GUI) by Juce. */

#include <windows.h>
#include <windef.h>
#include <string.h>
#include <stdio.h>

#include "keyconfui.h"
#include "keyconf.h"
#include "keycfg.h"

HWND hWnd = NULL;

// configuration
KEYCFG g_keyCfg = {
    {
        KEYCFG_KEYBOARD_DEFAULT_SWITCH_LEFT,
        KEYCFG_KEYBOARD_DEFAULT_SWITCH_RIGHT,
        KEYCFG_KEYBOARD_DEFAULT_RESET,
        KEYCFG_KEYBOARD_DEFAULT_RANDOM,
        KEYCFG_KEYBOARD_DEFAULT_PREV,
        KEYCFG_KEYBOARD_DEFAULT_NEXT,
        KEYCFG_KEYBOARD_DEFAULT_PREVVAL,
        KEYCFG_KEYBOARD_DEFAULT_NEXTVAL,
        KEYCFG_KEYBOARD_DEFAULT_INFOPAGEPREV,
        KEYCFG_KEYBOARD_DEFAULT_INFOPAGENEXT,
        KEYCFG_KEYBOARD_DEFAULT_ACTION1,
        KEYCFG_KEYBOARD_DEFAULT_ACTION2,
        KEYCFG_KEYBOARD_DEFAULT_SWITCH1,
        KEYCFG_KEYBOARD_DEFAULT_SWITCH2,
    },
    {
        KEYCFG_GAMEPAD_DEFAULT_SWITCH_LEFT,
        KEYCFG_GAMEPAD_DEFAULT_SWITCH_RIGHT,
        KEYCFG_GAMEPAD_DEFAULT_RESET,
        KEYCFG_GAMEPAD_DEFAULT_RANDOM,
        KEYCFG_GAMEPAD_DEFAULT_PREV,
        KEYCFG_GAMEPAD_DEFAULT_NEXT,
        KEYCFG_GAMEPAD_DEFAULT_PREVVAL,
        KEYCFG_GAMEPAD_DEFAULT_NEXTVAL,
        KEYCFG_GAMEPAD_DEFAULT_INFOPAGEPREV,
        KEYCFG_GAMEPAD_DEFAULT_INFOPAGENEXT,
        KEYCFG_GAMEPAD_DEFAULT_ACTION1,
        KEYCFG_GAMEPAD_DEFAULT_ACTION2,
        KEYCFG_GAMEPAD_DEFAULT_SWITCH1,
        KEYCFG_GAMEPAD_DEFAULT_SWITCH2,
    },
};

void ApplySettings(void)
{
	char buf[BUFLEN];

	VKEY_TEXT(g_keyCfg.keyboard.keySwitchLeft, buf, BUFLEN); 
	SendMessage(g_keySwitchLeftControl, WM_SETTEXT, 0, (LPARAM)buf);
	VKEY_TEXT(g_keyCfg.keyboard.keySwitchRight, buf, BUFLEN); 
	SendMessage(g_keySwitchRightControl, WM_SETTEXT, 0, (LPARAM)buf);
	VKEY_TEXT(g_keyCfg.keyboard.keyReset, buf, BUFLEN); 
	SendMessage(g_keyResetControl, WM_SETTEXT, 0, (LPARAM)buf);
	VKEY_TEXT(g_keyCfg.keyboard.keyRandom, buf, BUFLEN); 
	SendMessage(g_keyRandomControl, WM_SETTEXT, 0, (LPARAM)buf);
	VKEY_TEXT(g_keyCfg.keyboard.keyPrev, buf, BUFLEN); 
	SendMessage(g_keyPrevControl, WM_SETTEXT, 0, (LPARAM)buf);
	VKEY_TEXT(g_keyCfg.keyboard.keyNext, buf, BUFLEN); 
	SendMessage(g_keyNextControl, WM_SETTEXT, 0, (LPARAM)buf);
	VKEY_TEXT(g_keyCfg.keyboard.keyPrevVal, buf, BUFLEN); 
	SendMessage(g_keyPrevValControl, WM_SETTEXT, 0, (LPARAM)buf);
	VKEY_TEXT(g_keyCfg.keyboard.keyNextVal, buf, BUFLEN); 
	SendMessage(g_keyNextValControl, WM_SETTEXT, 0, (LPARAM)buf);
	VKEY_TEXT(g_keyCfg.keyboard.keyInfoPagePrev, buf, BUFLEN); 
	SendMessage(g_keyInfoPagePrevControl, WM_SETTEXT, 0, (LPARAM)buf);
	VKEY_TEXT(g_keyCfg.keyboard.keyInfoPageNext, buf, BUFLEN); 
	SendMessage(g_keyInfoPageNextControl, WM_SETTEXT, 0, (LPARAM)buf);
	VKEY_TEXT(g_keyCfg.keyboard.keyAction1, buf, BUFLEN); 
	SendMessage(g_keyAction1Control, WM_SETTEXT, 0, (LPARAM)buf);
	VKEY_TEXT(g_keyCfg.keyboard.keyAction2, buf, BUFLEN); 
	SendMessage(g_keyAction2Control, WM_SETTEXT, 0, (LPARAM)buf);
	VKEY_TEXT(g_keyCfg.keyboard.keySwitch1, buf, BUFLEN); 
	SendMessage(g_keySwitch1Control, WM_SETTEXT, 0, (LPARAM)buf);
	VKEY_TEXT(g_keyCfg.keyboard.keySwitch2, buf, BUFLEN); 
	SendMessage(g_keySwitch2Control, WM_SETTEXT, 0, (LPARAM)buf);
}

/**
 * Restores last saved settings.
 */
void RestoreSettings(void)
{
	// read optional configuration file
	ReadKeyCfg(&g_keyCfg, "keybind.dat");
	ApplySettings();
}

/**
 * Saves settings.
 */
void SaveSettings(void)
{
	// write configuration file
	WriteKeyCfg(&g_keyCfg, "keybind.dat");
	ApplySettings();
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	int home, away, timecode;
	char buf[BUFLEN];

	switch(uMsg)
	{
		case WM_DESTROY:
			// Exit the application when the window closes
			PostQuitMessage(1);
			return true;

		case WM_COMMAND:
			if (HIWORD(wParam) == BN_CLICKED)
			{
				if ((HWND)lParam == g_saveButtonControl)
				{
					SaveSettings();
					// modify status text
					SendMessage(g_statusTextControl, WM_SETTEXT, 0, (LPARAM)"SAVED");
				}
				else if ((HWND)lParam == g_restoreButtonControl)
				{
					RestoreSettings();
					// modify status text
					SendMessage(g_statusTextControl, WM_SETTEXT, 0, (LPARAM)"RESTORED");
				}
			}
			else if (HIWORD(wParam) == EN_CHANGE)
			{
				HWND control = (HWND)lParam;
				// modify status text
				SendMessage(g_statusTextControl, WM_SETTEXT, 0, (LPARAM)"CHANGES MADE");
			}
			else if (HIWORD(wParam) == CBN_EDITCHANGE)
			{
				HWND control = (HWND)lParam;
				// modify status text
				SendMessage(g_statusTextControl, WM_SETTEXT, 0, (LPARAM)"CHANGES MADE");
			}
			break;

		case WM_APP_KEYDEF:
			HWND target = (HWND)lParam;
			ZeroMemory(buf, BUFLEN);
			GetKeyNameText(MapVirtualKey(wParam, 0) << 16, buf, BUFLEN);
			SendMessage(target, WM_SETTEXT, 0, (LPARAM)buf);
			SendMessage(g_statusTextControl, WM_SETTEXT, 0, (LPARAM)"CHANGES MADE");
			// update config
			if (target == g_keySwitchLeftControl) g_keyCfg.keyboard.keySwitchLeft = (WORD)wParam;
			else if (target == g_keySwitchRightControl) g_keyCfg.keyboard.keySwitchRight = (WORD)wParam;
			else if (target == g_keyResetControl) g_keyCfg.keyboard.keyReset = (WORD)wParam;
			else if (target == g_keyRandomControl) g_keyCfg.keyboard.keyRandom = (WORD)wParam;
			else if (target == g_keyPrevControl) g_keyCfg.keyboard.keyPrev = (WORD)wParam;
			else if (target == g_keyNextControl) g_keyCfg.keyboard.keyNext = (WORD)wParam;
			else if (target == g_keyPrevValControl) g_keyCfg.keyboard.keyPrevVal = (WORD)wParam;
			else if (target == g_keyNextValControl) g_keyCfg.keyboard.keyNextVal = (WORD)wParam;
			else if (target == g_keyInfoPagePrevControl) g_keyCfg.keyboard.keyInfoPagePrev = (WORD)wParam;
			else if (target == g_keyInfoPageNextControl) g_keyCfg.keyboard.keyInfoPageNext = (WORD)wParam;
			else if (target == g_keyAction1Control) g_keyCfg.keyboard.keyAction1 = (WORD)wParam;
			else if (target == g_keyAction1Control) g_keyCfg.keyboard.keyAction2 = (WORD)wParam;
			else if (target == g_keySwitch1Control) g_keyCfg.keyboard.keySwitch1 = (WORD)wParam;
			else if (target == g_keySwitch1Control) g_keyCfg.keyboard.keySwitch2 = (WORD)wParam;
			break;
	}
	return DefWindowProc(hwnd,uMsg,wParam,lParam);
}

bool InitApp(HINSTANCE hInstance, LPSTR lpCmdLine)
{
	WNDCLASSEX wcx;

	// cbSize - the size of the structure.
	wcx.cbSize = sizeof(WNDCLASSEX);
	wcx.style = CS_HREDRAW | CS_VREDRAW;
	wcx.lpfnWndProc = (WNDPROC)WindowProc;
	wcx.cbClsExtra = 0;
	wcx.cbWndExtra = 0;
	wcx.hInstance = hInstance;
	wcx.hIcon = LoadIcon(NULL,IDI_APPLICATION);
	wcx.hCursor = LoadCursor(NULL,IDC_ARROW);
	wcx.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
	wcx.lpszMenuName = NULL;
	wcx.lpszClassName = "KSERVCLS";
	wcx.hIconSm = NULL;

	// Register the class with Windows
	if(!RegisterClassEx(&wcx))
		return false;

	// read optional configuration file
	ReadKeyCfg(&g_keyCfg, "keybind.dat");

	return true;
}

HWND BuildWindow(int nCmdShow)
{
	DWORD style, xstyle;
	HWND retval;

	style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
	xstyle = WS_EX_LEFT;

	retval = CreateWindowEx(xstyle,
        "KSERVCLS",      // class name
        KSERV_WINDOW_TITLE, // title for our window (appears in the titlebar)
        style,
        CW_USEDEFAULT,  // initial x coordinate
        CW_USEDEFAULT,  // initial y coordinate
        WIN_WIDTH, WIN_HEIGHT,   // width and height of the window
        NULL,           // no parent window.
        NULL,           // no menu
        NULL,           // no creator
        NULL);          // no extra data

	if (retval == NULL) return NULL;  // BAD.

	ShowWindow(retval,nCmdShow);  // Show the window
	return retval; // return its handle for future use.
}

int APIENTRY WinMain(HINSTANCE hInstance,
                     HINSTANCE hPrevInstance,
                     LPSTR     lpCmdLine,
                     int       nCmdShow)
{
	MSG msg; int retval;

 	if(InitApp(hInstance, lpCmdLine) == false)
		return 0;

	hWnd = BuildWindow(nCmdShow);
	if(hWnd == NULL)
		return 0;

	// build GUI
	if (!BuildControls(hWnd))
		return 0;

	// Initialize all controls
	ApplySettings();

	// show credits
	char buf[BUFLEN];
	ZeroMemory(buf, BUFLEN);
	strncpy(buf, CREDITS, BUFLEN-1);
	SendMessage(g_statusTextControl, WM_SETTEXT, 0, (LPARAM)buf);

	while((retval = GetMessage(&msg,NULL,0,0)) != 0)
	{
		// capture key-def events
		if ((msg.hwnd == g_keySwitchLeftControl 
                    || msg.hwnd == g_keySwitchRightControl 
                    || msg.hwnd == g_keyResetControl 
                    || msg.hwnd == g_keyRandomControl 
                    || msg.hwnd == g_keyPrevControl 
                    || msg.hwnd == g_keyNextControl 
                    || msg.hwnd == g_keyPrevValControl 
                    || msg.hwnd == g_keyNextValControl 
                    || msg.hwnd == g_keyInfoPagePrevControl 
                    || msg.hwnd == g_keyInfoPageNextControl 
                    || msg.hwnd == g_keyAction1Control 
                    || msg.hwnd == g_keyAction2Control 
                    || msg.hwnd == g_keySwitch1Control 
                    || msg.hwnd == g_keySwitch2Control 
            ) && (
                msg.message == WM_KEYDOWN 
                    || msg.message == WM_SYSKEYDOWN
            ))
		{
			PostMessage(hWnd, WM_APP_KEYDEF, msg.wParam, (LPARAM)msg.hwnd);
			continue;
		}

		if(retval == -1)
			return 0;	// an error occured while getting a message

		if (!IsDialogMessage(hWnd, &msg)) // need to call this to make WS_TABSTOP work
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	return 0;
}

