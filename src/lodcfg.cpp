/* main */
/* Version 1.0 */

#include <windows.h>
#include <windef.h>
#include <string.h>
#include <stdio.h>
#include <tchar.h>

#define BUFLEN 4096

#include "lodcfgui.h"
#include "lodcfg.h"
#include "detect.h"
#include "imageutil.h"

#define UNDEFINED -1
#define WM_APP_EXECHANGE WM_APP+1
#define CFG_NAME "lodmixer.dat"

LCM g_lcm;
BYTE g_crowdCheck;
BYTE g_JapanCheck;
BYTE g_aspectCheck;
BYTE g_lodLevels[] = {0,1,2,3,4};

HWND hWnd = NULL;
bool g_buttonClick = true;
BOOL g_isBeingEdited = FALSE;

// function prototypes
void ReadConfig();
void EnableControls(BOOL flag);
void PopulateLodLists(BYTE* indices);
void PopulateCrowdCheckBox(BYTE check);
void PopulateJapanCheckBox(BYTE check);
void PopulateAspectCheckBox(BYTE check);
void SetLCM();
void GetLCM();
void GetLCMValueFromList(HWND control, BYTE* b);
void GetCrowdCheckBox(BYTE* b);
void GetLodFromList(HWND control, BYTE* b);
void SaveConfig();

void SetLCMDefaults()
{
    SendMessage(g_weatherListControl, CB_SETCURSEL, 0, 0);
    SendMessage(g_timeListControl, CB_SETCURSEL, 0, 0);
    SendMessage(g_seasonListControl, CB_SETCURSEL, 0, 0);
    SendMessage(g_stadiumEffectsListControl, CB_SETCURSEL, 0, 0);
    SendMessage(g_numberOfSubs, CB_SETCURSEL, 0, 0);
    SendMessage(g_homeCrowdListControl, CB_SETCURSEL, 0, 0);
    SendMessage(g_awayCrowdListControl, CB_SETCURSEL, 0, 0);
    SendMessage(g_crowdStanceListControl, CB_SETCURSEL, 0, 0);
}

void EnableLCM(BOOL value)
{
    EnableWindow(g_weatherListControl, value);
    EnableWindow(g_timeListControl, value);
    EnableWindow(g_seasonListControl, value);
    EnableWindow(g_stadiumEffectsListControl, value);
    EnableWindow(g_numberOfSubs, value);
    EnableWindow(g_homeCrowdListControl, value);
    EnableWindow(g_awayCrowdListControl, value);
    EnableWindow(g_crowdStanceListControl, value);
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
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
					// save LOD
					g_buttonClick = true;

                    char exeName[BUFLEN];
                    ZeroMemory(exeName, BUFLEN);
					SaveConfig();
				}
				else if ((HWND)lParam == g_defButtonControl)
				{
					// reset defaults 
					g_buttonClick = true;

                    BYTE idx[] = {0,1,2,3,4};
                    PopulateLodLists(idx);
                    PopulateCrowdCheckBox(0);
                    PopulateJapanCheckBox(0);
                    PopulateAspectCheckBox(0);
                    SetLCMDefaults();
				}
			}
			else if (HIWORD(wParam) == CBN_EDITUPDATE)
			{
				g_isBeingEdited = TRUE;
			}
			else if (HIWORD(wParam) == CBN_KILLFOCUS && g_isBeingEdited)
			{
				g_isBeingEdited = FALSE;
				HWND control = (HWND)lParam;
			}
			else if (HIWORD(wParam) == CBN_SELCHANGE)
			{
				HWND control = (HWND)lParam;
			}
			break;

		case WM_APP_EXECHANGE:
			if (wParam == VK_RETURN) {
				g_isBeingEdited = FALSE;
				MessageBox(hWnd, "WM_APP_EXECHANGE", "Installer Message", 0);
			}
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
	wcx.lpszClassName = "LODCFGCLS";
	wcx.hIconSm = NULL;

	// Register the class with Windows
	if(!RegisterClassEx(&wcx))
		return false;

	return true;
}

HWND BuildWindow(int nCmdShow)
{
	DWORD style, xstyle;
	HWND retval;

	style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
	xstyle = WS_EX_LEFT;

	retval = CreateWindowEx(xstyle,
        "LODCFGCLS",      // class name
        LODCFG_WINDOW_TITLE, // title for our window (appears in the titlebar)
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

void PopulateLodLists(BYTE* indices)
{
    if (indices == NULL) return;
	for (int i=0; i<5; i++) {
        for (int j=0; j<5; j++) {
            SendMessage(g_lodListControl[i], CB_SETCURSEL, (WPARAM)indices[i], (LPARAM)0);
            if (indices[i] == -1) {
                EnableWindow(g_lodListControl[i], FALSE);
            }
        }
	}
}

void PopulateCrowdCheckBox(BYTE check)
{
    if (check) {
        SendMessage(g_crowdCheckBox, BM_SETCHECK, BST_CHECKED, 0);
    } else {
        SendMessage(g_crowdCheckBox, BM_SETCHECK, BST_UNCHECKED, 0);
    }
}

void PopulateJapanCheckBox(BYTE check)
{
    if (check) {
        SendMessage(g_JapanCheckBox, BM_SETCHECK, BST_CHECKED, 0);
    } else {
        SendMessage(g_JapanCheckBox, BM_SETCHECK, BST_UNCHECKED, 0);
    }
}

void PopulateAspectCheckBox(BYTE check)
{
    if (check) {
        SendMessage(g_aspectCheckBox, BM_SETCHECK, BST_CHECKED, 0);
    } else {
        SendMessage(g_aspectCheckBox, BM_SETCHECK, BST_UNCHECKED, 0);
    }
}

void SetLCM()
{
    BYTE idx;
    idx = (g_lcm.weather == 0xff)?0:g_lcm.weather+1;
    SendMessage(g_weatherListControl, CB_SETCURSEL, (WPARAM)idx, (LPARAM)0);
    idx = (g_lcm.timeOfDay == 0xff)?0:g_lcm.timeOfDay+1;
    SendMessage(g_timeListControl, CB_SETCURSEL, (WPARAM)idx, (LPARAM)0);
    idx = (g_lcm.season == 0xff)?0:g_lcm.season+1;
    SendMessage(g_seasonListControl, CB_SETCURSEL, (WPARAM)idx, (LPARAM)0);
    idx = (g_lcm.effects == 0xff)?0:g_lcm.effects+1;
    SendMessage(g_stadiumEffectsListControl, CB_SETCURSEL, (WPARAM)idx, (LPARAM)0);
    idx = (g_lcm.numSubs == 0xff)?0:((g_lcm.numSubs == 0)?1:g_lcm.numSubs-1);
    SendMessage(g_numberOfSubs, CB_SETCURSEL, (WPARAM)idx, (LPARAM)0);
    idx = (g_lcm.homeCrowd == 0xff)?0:g_lcm.homeCrowd+1;
    SendMessage(g_homeCrowdListControl, CB_SETCURSEL, (WPARAM)idx, (LPARAM)0);
    idx = (g_lcm.awayCrowd == 0xff)?0:g_lcm.awayCrowd+1;
    SendMessage(g_awayCrowdListControl, CB_SETCURSEL, (WPARAM)idx, (LPARAM)0);
    idx = (g_lcm.crowdStance == 0xff)?0:g_lcm.crowdStance;
    SendMessage(g_crowdStanceListControl, CB_SETCURSEL, (WPARAM)idx, (LPARAM)0);
}

void GetLCMValueFromList(HWND control, BYTE* b)
{
    int idx = (int)SendMessage(control, CB_GETCURSEL, 0, 0);
    idx = (idx == -1) ? 0: idx;
    if (idx == 0) *b = 0xff;
    else *b = (BYTE)idx-1;
}

void GetLodFromList(HWND control, BYTE* b)
{
    int idx = (int)SendMessage(control, CB_GETCURSEL, 0, 0);
    idx = (idx == -1) ? 0: idx;
    *b = (BYTE)idx;
}

void GetCrowdCheckBox(BYTE* b)
{
    LRESULT lResult = SendMessage(g_crowdCheckBox, BM_GETCHECK, 0, 0);
    *b = (lResult == BST_CHECKED)?1:0;
}

void GetJapanCheckBox(BYTE* b)
{
    LRESULT lResult = SendMessage(g_JapanCheckBox, BM_GETCHECK, 0, 0);
    *b = (lResult == BST_CHECKED)?1:0;
}

void GetAspectCheckBox(BYTE* b)
{
    LRESULT lResult = SendMessage(g_aspectCheckBox, BM_GETCHECK, 0, 0);
    *b = (lResult == BST_CHECKED)?1:0;
}

void GetLCM()
{
    GetLCMValueFromList(g_weatherListControl, &g_lcm.weather);
    GetLCMValueFromList(g_timeListControl, &g_lcm.timeOfDay);
    GetLCMValueFromList(g_seasonListControl, &g_lcm.season);
    GetLCMValueFromList(g_stadiumEffectsListControl, &g_lcm.effects);
    GetLCMValueFromList(g_numberOfSubs, &g_lcm.numSubs);
    if (g_lcm.numSubs != 0xff && g_lcm.numSubs != 0) {
        g_lcm.numSubs += 2;
    }
    GetLCMValueFromList(g_homeCrowdListControl, &g_lcm.homeCrowd);
    GetLCMValueFromList(g_awayCrowdListControl, &g_lcm.awayCrowd);
    GetLCMValueFromList(g_crowdStanceListControl, &g_lcm.crowdStance);
    // crowd stance is 1-based
    g_lcm.crowdStance = (g_lcm.crowdStance == 0xff)?0xff:g_lcm.crowdStance+1;
}

void GetLodLevels()
{
    for (int i=0; i<5; i++) {
        GetLodFromList(g_lodListControl[i], g_lodLevels+i);
    }
}

void ReadConfig()
{
    memset(&g_lcm, 0xff, sizeof(LCM));
    FILE* f = fopen(CFG_NAME,"rb");
    if (f) {
        fread(&g_lcm, sizeof(LCM), 1, f);
        fread(&g_crowdCheck, 1, 1, f);
        fread(&g_lodLevels, sizeof(g_lodLevels), 1, f);
        fread(&g_JapanCheck, 1, 1, f);
        fread(&g_aspectCheck, 1, 1, f);
        fclose(f);
    }
}

void SaveConfig()
{
    GetLodLevels();
    GetCrowdCheckBox(&g_crowdCheck);
    GetJapanCheckBox(&g_JapanCheck);
    GetAspectCheckBox(&g_aspectCheck);
    GetLCM();
    FILE* f = fopen(CFG_NAME, "wb");
    if (f) {
        fwrite(&g_lcm, sizeof(LCM), 1, f);
        fwrite(&g_crowdCheck, 1, 1, f);
        fwrite(&g_lodLevels, sizeof(g_lodLevels), 1, f);
        fwrite(&g_JapanCheck, 1, 1, f);
        fwrite(&g_aspectCheck, 1, 1, f);
        fclose(f);

		// show message box with success msg
		char buf[BUFLEN];
		ZeroMemory(buf, BUFLEN);
		sprintf(buf, "\
======= SUCCESS! ========\n\
LOD Mixer configuration saved.\n\
\n");
		MessageBox(hWnd, buf, "LOD Mixer Message", 0);

    } else {
		// show message box with error msg
		char buf[BUFLEN];
		ZeroMemory(buf, BUFLEN);
		sprintf(buf, "\
========== ERROR! ===========\n\
Problem saving LOD Mixer configuration info this file:\n\
%s.\n\
\n", CFG_NAME);

		MessageBox(hWnd, buf, "LOD Mixer Message", 0);
		return;
	}
}

void EnableControls(BOOL flag)
{
    for (int i=0; i<5; i++) {
        EnableWindow(g_lodListControl[i], flag);
    }
    EnableWindow(g_crowdCheckBox, flag);
//    EnableWindow(g_JapanCheckBox, flag);
    EnableWindow(g_aspectCheckBox, flag);
    EnableWindow(g_saveButtonControl, flag);
    EnableWindow(g_defButtonControl, flag);
    EnableLCM(flag);
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

    // read configuration
    ReadConfig();
    PopulateLodLists(g_lodLevels);
    PopulateCrowdCheckBox(g_crowdCheck);
//    PopulateJapanCheckBox(g_JapanCheck);
    PopulateAspectCheckBox(g_aspectCheck);
    SetLCM();
    EnableControls(TRUE);

	// show credits
	char buf[BUFLEN];
	ZeroMemory(buf, BUFLEN);
	strncpy(buf, CREDITS, BUFLEN-1);
	SendMessage(g_statusTextControl, WM_SETTEXT, 0, (LPARAM)buf);

	while((retval = GetMessage(&msg,NULL,0,0)) != 0)
	{
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

